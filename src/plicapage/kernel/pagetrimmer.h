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


#ifndef PAGETRIMMER_H
#define PAGETRIMMER_H

#include <QByteArray>
#include <QObject>
#include <QRectF>
#include <QVector>


/************************************************
 * Finds the ink bounding box of each page of a PDF document, so that the
 * whitespace around it can be trimmed away and the remainder scaled up.
 *
 * Pages are rasterized with poppler - already a dependency, see render.cpp -
 * and scanned for non-white pixels. Ink is grouped into connected components
 * so that isolated speckles and scanner lid shadows can be rejected; without
 * that, a single dust speck in the corner of a scan would pin the box to the
 * full page and defeat the trim entirely.
 ************************************************/
class PageTrimmer : public QObject
{
    Q_OBJECT
public:
    explicit PageTrimmer(QObject *parent = nullptr);

    /**
     * Ink boxes for every page of pdfData, in page order.
     *
     * pageRects gives the box each page was rendered from, and the result is
     * expressed in those same coordinates: QRectF(x1, y1, width, height), the
     * convention PdfPageInfo uses, where top() is the PDF *lower* edge.
     *
     * A page holding no ink - or one that cannot be rendered - yields a null
     * rect, which callers should treat as "do not trim".
     */
    QVector<QRectF> scan(const QByteArray &pdfData, const QVector<QRectF> &pageRects);

    /**
     * Ink box of a single 8-bit grayscale raster. Row 0 is the top of the
     * page, so the y axis is flipped relative to pageRect. Exposed for tests.
     */
    static QRectF inkBox(const uchar *gray, int width, int height, int stride,
                         const QRectF &pageRect);

    double resolution() const { return mResolution; }
    void setResolution(double dpi) { mResolution = dpi; }

signals:
    void progress(int done, int all) const;

private:
    double mResolution;
};

#endif // PAGETRIMMER_H
