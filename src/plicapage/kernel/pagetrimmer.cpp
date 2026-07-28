/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 *
 * Copyright: 2026 Matthew Daines <spuddermax@gmail.com>
 * Authors:
 *   Matthew Daines <spuddermax@gmail.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */


#include "pagetrimmer.h"

#include <poppler-document.h>
#include <poppler-image.h>
#include <poppler-page.h>
#include <poppler-page-renderer.h>

#include "../popplergate.h"


/************************************************
 * Rasterization resolution. 72dpi makes one pixel exactly one PDF point,
 * which keeps the pixel-to-point mapping trivial and bounds the error on a
 * detected edge to a single point.
 ************************************************/
#define TRIM_RESOLUTION 72.0

/************************************************
 * A pixel counts as ink when it is darker than this. Just below pure white,
 * so that a faintly tinted background is still treated as blank.
 ************************************************/
#define INK_THRESHOLD 250

/************************************************
 * Connected blobs smaller than this are dust, not content. At 72dpi a stroke
 * of 10pt text covers well over a dozen pixels, while scanner speckle is a
 * handful, so this separates the two without eating punctuation that matters.
 ************************************************/
#define MIN_INK_AREA 4

/************************************************
 * How close to the paper edge a blob must reach to be considered part of a
 * scanner lid shadow, and how thick such a shadow may be. Six points is about
 * 2mm - wide enough for a real shadow, narrow enough that a page border rule
 * is never mistaken for one.
 ************************************************/
#define EDGE_BAND 3

/************************************************
 * A shadow runs along nearly the whole of one side. A genuine frame or table
 * rule spans one axis but is large on both, so it is never matched here.
 ************************************************/
#define EDGE_SPAN 0.9


/************************************************

 ************************************************/
PageTrimmer::PageTrimmer(QObject *parent):
    QObject(parent),
    mResolution(TRIM_RESOLUTION)
{
}


/************************************************
 * True for a blob that hugs one edge of the paper and runs almost its whole
 * length while staying thin - the signature of a scanner lid shadow.
 ************************************************/
static bool isEdgeShadow(int minX, int minY, int maxX, int maxY, int width, int height)
{
    bool touchesEdge = minX <= EDGE_BAND         || minY <= EDGE_BAND ||
                       maxX >= width - 1 - EDGE_BAND || maxY >= height - 1 - EDGE_BAND;
    if (!touchesEdge)
        return false;

    int blobWidth  = maxX - minX + 1;
    int blobHeight = maxY - minY + 1;

    bool horizontalStrip = blobWidth  >= width  * EDGE_SPAN && blobHeight <= EDGE_BAND * 2;
    bool verticalStrip   = blobHeight >= height * EDGE_SPAN && blobWidth  <= EDGE_BAND * 2;

    return horizontalStrip || verticalStrip;
}


/************************************************

 ************************************************/
QRectF PageTrimmer::inkBox(const uchar *gray, int width, int height, int stride,
                           const QRectF &pageRect)
{
    if (!gray || width < 1 || height < 1 || pageRect.isEmpty())
        return QRectF();

    // Binary ink mask .............................
    // Raw pointers throughout: QVector::operator[] detach-checks on every
    // access, which is felt when this runs over every pixel of every page.
    const int pixels = width * height;
    QVector<quint8> inkBuf(pixels, 0);
    quint8 *ink = inkBuf.data();

    bool anyInk = false;
    for (int y = 0; y < height; ++y)
    {
        const uchar *row = gray + y * stride;
        quint8 *out = ink + y * width;
        for (int x = 0; x < width; ++x)
        {
            if (row[x] < INK_THRESHOLD)
            {
                out[x] = 1;
                anyInk = true;
            }
        }
    }

    if (!anyInk)
        return QRectF();

    // Group the ink into connected blobs, and accumulate the bounds of the
    // ones that look like real content .............
    int pageMinX = width, pageMinY = height, pageMaxX = -1, pageMaxY = -1;

    QVector<qint32> stackBuf(pixels);
    qint32 *stack = stackBuf.data();

    for (int seed = 0; seed < pixels; ++seed)
    {
        if (!ink[seed])
            continue;

        int blobMinX = width, blobMinY = height, blobMaxX = -1, blobMaxY = -1;
        int area = 0;

        // Flood fill, 4-connected. Pixels are cleared as they are claimed, so
        // each one is visited once and no separate "visited" mask is needed.
        // The stack cannot exceed the pixel count for the same reason.
        int top = 0;
        stack[top++] = seed;
        ink[seed] = 0;

        while (top > 0)
        {
            const qint32 idx = stack[--top];
            const int x = idx % width;
            const int y = idx / width;

            ++area;
            blobMinX = qMin(blobMinX, x);
            blobMinY = qMin(blobMinY, y);
            blobMaxX = qMax(blobMaxX, x);
            blobMaxY = qMax(blobMaxY, y);

            if (x > 0          && ink[idx - 1])     { ink[idx - 1]     = 0; stack[top++] = idx - 1;     }
            if (x < width - 1  && ink[idx + 1])     { ink[idx + 1]     = 0; stack[top++] = idx + 1;     }
            if (y > 0          && ink[idx - width]) { ink[idx - width] = 0; stack[top++] = idx - width; }
            if (y < height - 1 && ink[idx + width]) { ink[idx + width] = 0; stack[top++] = idx + width; }
        }

        if (area < MIN_INK_AREA)
            continue;

        if (isEdgeShadow(blobMinX, blobMinY, blobMaxX, blobMaxY, width, height))
            continue;

        pageMinX = qMin(pageMinX, blobMinX);
        pageMinY = qMin(pageMinY, blobMinY);
        pageMaxX = qMax(pageMaxX, blobMaxX);
        pageMaxY = qMax(pageMaxY, blobMaxY);
    }

    if (pageMaxX < 0)
        return QRectF();

    // Pixels back to points ........................
    // Pixel x covers [x, x+1) across the page. Raster row 0 is the top of the
    // page while PDF y grows upward, so row y covers [height-1-y, height-y).
    const double scaleX = pageRect.width()  / width;
    const double scaleY = pageRect.height() / height;

    const double x1 = pageRect.left() + pageMinX * scaleX;
    const double x2 = pageRect.left() + (pageMaxX + 1) * scaleX;
    const double y1 = pageRect.top()  + (height - 1 - pageMaxY) * scaleY;
    const double y2 = pageRect.top()  + (height - pageMinY) * scaleY;

    return QRectF(x1, y1, x2 - x1, y2 - y1);
}


/************************************************

 ************************************************/
QVector<QRectF> PageTrimmer::scan(const QByteArray &pdfData, const QVector<QRectF> &pageRects)
{
    QVector<QRectF> res(pageRects.count());

    if (pdfData.isEmpty() || pageRects.isEmpty())
        return res;

    // load_from_raw_data does not copy, so pdfData has to outlive doc. It is a
    // const reference held for the whole call, so it does.
    //
    // This runs on the main thread while both preview pools may be rendering,
    // so it is a third source of poppler calls and has to go through the same
    // process-wide gate. Exclusive across the open itself only - holding it for
    // the whole scan would stall the preview for every page of the document.
    // See popplergate.h.
    poppler::document *doc = 0;
    {
        PopplerGate::DocumentLock gate;
        doc = poppler::document::load_from_raw_data(pdfData.constData(),
                                                    pdfData.size());
    }

    if (!doc)
        return res;

    const int count = qMin(doc->pages(), pageRects.count());

    poppler::page_renderer renderer;
    // Antialiasing would smear each glyph edge into the surrounding white and
    // inflate the box by a pixel on every side. It is also slower.
    renderer.set_render_hint(poppler::page_renderer::antialiasing, false);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, false);
    renderer.set_image_format(poppler::image::format_gray8);

    for (int i = 0; i < count; ++i)
    {
        {
            // Shared, and taken per page rather than around the whole loop, so
            // a preview reload waiting to open its documents is held up by one
            // page at most instead of by the entire scan.
            //
            // inkBox() stays inside it because it reads the buffer img owns.
            // It touches no poppler state of its own, and a shared holder
            // blocks nothing but a load.
            PopplerGate::RenderLock gate;

            poppler::page *page = doc->create_page(i);
            if (!page)
                continue;

            poppler::image img = renderer.render_page(page, mResolution, mResolution);
            delete page;

            if (img.is_valid() && img.format() == poppler::image::format_gray8)
            {
                res[i] = inkBox(reinterpret_cast<const uchar*>(img.const_data()),
                                img.width(), img.height(), img.bytes_per_row(),
                                pageRects.at(i));
            }
        }

        // Outside the gate deliberately. Whatever is watching progress runs on
        // this thread, and a slot that turned the event loop over could reach
        // Render::setFileName() - which would then wait for a lock this thread
        // is holding.
        emit progress(i + 1, count);
    }

    {
        PopplerGate::DocumentLock gate;
        delete doc;
    }

    return res;
}
