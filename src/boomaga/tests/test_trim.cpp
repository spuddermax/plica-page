/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 *
 * Copyright: 2026 Spuddermax <spuddermax@gmail.com>
 * Authors:
 *   Spuddermax <spuddermax@gmail.com>
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

#include "testboomaga.h"

#include <QTest>
#include <QFile>
#include <QVector>

// Same trick testboomaga.cpp uses to reach the kernel internals. getPageStream()
// is private, and it is the only place the clipping path can be observed.
#define protected public
#define private public
#include "../boomagatypes.h"
#include "../kernel/layout.h"
#include "../kernel/pagetrimmer.h"
#include "../kernel/project.h"
#include "../kernel/projectpage.h"
#include "../kernel/sheet.h"
#include "../kernel/tmppdffile.h"
#undef private
#undef protected


/************************************************
 * A white 8-bit grayscale page that black boxes can be painted onto, standing
 * in for what poppler hands PageTrimmer.
 ************************************************/
class TestRaster
{
public:
    TestRaster(int width, int height):
        mWidth(width),
        mHeight(height),
        mData(width * height, 255)
    {
    }

    void paint(int x, int y, int width, int height)
    {
        for (int row = y; row < y + height; ++row)
            for (int col = x; col < x + width; ++col)
                mData[row * mWidth + col] = 0;
    }

    QRectF inkBox(const QRectF &pageRect) const
    {
        return PageTrimmer::inkBox(mData.constData(), mWidth, mHeight, mWidth, pageRect);
    }

private:
    int mWidth;
    int mHeight;
    QVector<uchar> mData;
};


/************************************************
 * A page whose ink box is known up front.
 ************************************************/
static ProjectPage *createTrimPage(const QRectF &cropBox, const QRectF &inkBox)
{
    PdfPageInfo pdfInfo;
    pdfInfo.cropBox  = cropBox;
    pdfInfo.mediaBox = cropBox;
    pdfInfo.xObjNums << 1;

    ProjectPage *page = new ProjectPage();
    page->setPdfInfo(pdfInfo);
    page->setInkBox(inkBox);
    return page;
}


/************************************************
 * Trimming reads its settings off the project singleton, so the tests have to
 * put it into a known state and hand it a layout - Project::update() walks
 * into calcRotation(), which dereferences the layout.
 ************************************************/
static Layout *trimTestLayout()
{
    static LayoutNUp *layout = new LayoutNUp(1, 1);
    project->setLayout(layout);
    return layout;
}


static void setTrim(bool enabled, bool uniform = false, qreal padding = 0)
{
    trimTestLayout();
    project->setTrimPadding(padding);
    project->setTrimUniform(uniform);
    project->setTrimWhitespace(enabled);
}


/************************************************
 * Points are the storage unit; millimeters and inches are only ever display
 * forms of it. They have to agree exactly, or a value shifts every time the
 * user flips the preference.
 ************************************************/
void TestBoomaga::test_Units()
{
    // The two definitions that everything else follows from.
    QCOMPARE(fromUnit(1.0, UnitInch), 72.0);
    QCOMPARE(fromUnit(25.4, UnitMillimeter), 72.0);

    QCOMPARE(toUnit(72.0, UnitInch), 1.0);
    QCOMPARE(toUnit(72.0, UnitMillimeter), 25.4);

    // Points pass straight through.
    QCOMPARE(fromUnit(42.0, UnitPoint), 42.0);
    QCOMPARE(toUnit(42.0, UnitPoint), 42.0);

    // An inch is 25.4mm however you get there. This is what breaks if the
    // millimeter ratio is derived from rounded A4 dimensions instead of the
    // exact one.
    QCOMPARE(toUnit(fromUnit(1.0, UnitInch), UnitMillimeter), 25.4);
    QCOMPARE(toUnit(fromUnit(25.4, UnitMillimeter), UnitInch), 1.0);

    // Round trips must not drift.
    const QVector<Unit> units = QVector<Unit>() << UnitMillimeter << UnitPoint << UnitInch;
    foreach (Unit unit, units)
    {
        for (double v = 0.5; v < 20; v += 0.25)
            QVERIFY(qAbs(toUnit(fromUnit(v, unit), unit) - v) < 1e-9);
    }

    // The three maxima are meant to be the same physical length, so switching
    // units never silently truncates a value.
    QVERIFY(qAbs(fromUnit(unitMax(UnitInch), UnitInch)
                 - fromUnit(unitMax(UnitPoint), UnitPoint)) < 1.0);
    QVERIFY(qAbs(fromUnit(unitMax(UnitInch), UnitInch)
                 - fromUnit(unitMax(UnitMillimeter), UnitMillimeter)) < 5.0);

    // Names survive a trip through the settings file.
    QCOMPARE(strToUnit(unitToStr(UnitMillimeter)), UnitMillimeter);
    QCOMPARE(strToUnit(unitToStr(UnitPoint)), UnitPoint);
    QCOMPARE(strToUnit(unitToStr(UnitInch)), UnitInch);
}


/************************************************

 ************************************************/
void TestBoomaga::test_InkBox()
{
    const QRectF page(0, 0, 100, 100);

    // A plain block of ink ........................
    // Rows 10..29 of a 100 tall raster. Raster row 0 is the top of the page
    // while PDF y grows upward, so those rows sit at PDF y 70..90.
    {
        TestRaster r(100, 100);
        r.paint(20, 10, 20, 20);
        QCOMPARE(r.inkBox(page), QRectF(20, 70, 20, 20));
    }

    // A blank page yields nothing to trim to ......
    {
        TestRaster r(100, 100);
        QVERIFY(r.inkBox(page).isNull());
    }

    // The box is expressed in the page's own coordinates, not the raster's ..
    {
        TestRaster r(100, 100);
        r.paint(20, 10, 20, 20);
        QCOMPARE(r.inkBox(QRectF(50, 200, 100, 100)), QRectF(70, 270, 20, 20));
    }

    // ...and scales when the raster is not 1px per point ..
    {
        TestRaster r(200, 200);
        r.paint(40, 20, 40, 40);
        QCOMPARE(r.inkBox(page), QRectF(20, 70, 20, 20));
    }

    // A speck of scanner dust must not pin the box to the whole page ..
    {
        TestRaster r(100, 100);
        r.paint(20, 10, 20, 20);
        r.paint(95, 95, 1, 1);
        QCOMPARE(r.inkBox(page), QRectF(20, 70, 20, 20));
    }

    // ...but a blob big enough to be real content counts ..
    {
        TestRaster r(100, 100);
        r.paint(20, 10, 20, 20);
        r.paint(90, 90, 4, 4);
        QCOMPARE(r.inkBox(page), QRectF(20, 6, 74, 84));
    }

    // A scanner lid shadow down one edge is ignored ..
    {
        TestRaster r(100, 100);
        r.paint(20, 10, 20, 20);
        r.paint(0, 0, 100, 2);
        QCOMPARE(r.inkBox(page), QRectF(20, 70, 20, 20));
    }

    // ...and so is one down the side ..
    {
        TestRaster r(100, 100);
        r.paint(20, 10, 20, 20);
        r.paint(98, 0, 2, 100);
        QCOMPARE(r.inkBox(page), QRectF(20, 70, 20, 20));
    }

    // A hairline rule is real content, even though it is only 1px thick.
    // This is why blobs are filtered by area rather than by row density.
    {
        TestRaster r(100, 100);
        r.paint(10, 50, 80, 1);
        QCOMPARE(r.inkBox(page), QRectF(10, 49, 80, 1));
    }

    // A full page border is not mistaken for a lid shadow: it spans both axes.
    {
        TestRaster r(100, 100);
        r.paint(5, 5, 90, 1);
        r.paint(5, 94, 90, 1);
        r.paint(5, 5, 1, 90);
        r.paint(94, 5, 1, 90);
        QCOMPARE(r.inkBox(page), QRectF(5, 5, 90, 90));
    }
}


/************************************************
 * The same detection, but driven through poppler on a real PDF. This is what
 * catches a mistake in the render setup or in the pixel-to-point mapping that
 * a hand-built raster would never show.
 *
 * data/trim/00-block.pdf is three 400x600 pages: a block at (100,150)-(300,450),
 * a block at (50,50)-(150,150), and a blank one.
 ************************************************/
void TestBoomaga::test_InkBoxPdf()
{
    QFile file(QString(TEST_DATA_DIR) + "trim/00-block.pdf");
    QVERIFY2(file.open(QFile::ReadOnly), qPrintable(file.fileName()));
    const QByteArray pdf = file.readAll();
    file.close();

    QVector<QRectF> pageRects;
    pageRects << QRectF(0, 0, 400, 600)
              << QRectF(0, 0, 400, 600)
              << QRectF(0, 0, 400, 600);

    PageTrimmer trimmer;
    const QVector<QRectF> boxes = trimmer.scan(pdf, pageRects);

    QCOMPARE(boxes.count(), 3);

    // One point of slack: the rasterizer can only place an edge to the pixel.
    const QRectF first = boxes.at(0);
    QVERIFY2(qAbs(first.left()   - 100) <= 1 &&
             qAbs(first.top()    - 150) <= 1 &&
             qAbs(first.width()  - 200) <= 1 &&
             qAbs(first.height() - 300) <= 1,
             qPrintable(QString("page 1 ink box was %1,%2 %3x%4")
                        .arg(first.left()).arg(first.top())
                        .arg(first.width()).arg(first.height())));

    const QRectF second = boxes.at(1);
    QVERIFY2(qAbs(second.left()   - 50)  <= 1 &&
             qAbs(second.top()    - 50)  <= 1 &&
             qAbs(second.width()  - 100) <= 1 &&
             qAbs(second.height() - 100) <= 1,
             qPrintable(QString("page 2 ink box was %1,%2 %3x%4")
                        .arg(second.left()).arg(second.top())
                        .arg(second.width()).arg(second.height())));

    QVERIFY2(boxes.at(2).isNull(), "a blank page should have no ink box");
}


/************************************************

 ************************************************/
void TestBoomaga::test_TrimRect()
{
    const QRectF crop(0, 0, 600, 800);
    const QRectF ink(100, 200, 200, 300);

    // Trimming off: the page is placed exactly as before ..
    {
        setTrim(false);
        ProjectPage *page = createTrimPage(crop, ink);
        QCOMPARE(page->trimRect(), crop);
        delete page;
    }

    // Trimming on, no padding: the ink box is what gets placed ..
    {
        setTrim(true);
        ProjectPage *page = createTrimPage(crop, ink);
        QCOMPARE(page->trimRect(), ink);
        delete page;
    }

    // Padding grows the box on every side ..
    {
        setTrim(true, false, 10);
        ProjectPage *page = createTrimPage(crop, ink);
        QCOMPARE(page->trimRect(), QRectF(90, 190, 220, 320));
        delete page;
    }

    // ...but never past the edge of the page, where there is nothing to show.
    {
        setTrim(true, false, 500);
        ProjectPage *page = createTrimPage(crop, ink);
        QCOMPARE(page->trimRect(), crop);
        delete page;
    }

    // A page that has no ink box - blank, or not yet scanned - is left alone.
    {
        setTrim(true);
        ProjectPage *page = createTrimPage(crop, QRectF());
        QCOMPARE(page->trimRect(), crop);
        delete page;
    }

    setTrim(false);
}


/************************************************
 * Halving the placed box in both directions has to double the scale, whatever
 * the paper and margins happen to be.
 ************************************************/
void TestBoomaga::test_TrimScale()
{
    const QRectF crop(0, 0, 400, 600);
    const QRectF halfInk(100, 150, 200, 300);

    LayoutNUp *layout = static_cast<LayoutNUp*>(trimTestLayout());

    // Baseline, trimming off ......................
    setTrim(false);
    Sheet *sheet = new Sheet(1, 0);
    sheet->setPage(0, createTrimPage(crop, halfInk));
    const double plainScale = layout->transformSpec(sheet, 0, NoRotate).scale;
    QVERIFY(plainScale > 0);

    // An ink box the size of the whole page changes nothing ..
    {
        setTrim(true);
        Sheet *s = new Sheet(1, 0);
        s->setPage(0, createTrimPage(crop, crop));
        QCOMPARE(layout->transformSpec(s, 0, NoRotate).scale, plainScale);
        delete s->page(0);
        delete s;
    }

    // Half the width and half the height, so twice the scale ..
    {
        setTrim(true);
        TransformSpec spec = layout->transformSpec(sheet, 0, NoRotate);
        QVERIFY(qAbs(spec.scale - plainScale * 2.0) < 1e-9);

        // The placed rectangle keeps the trimmed box's aspect ratio.
        QVERIFY(qAbs(spec.rect.width() / spec.rect.height()
                     - halfInk.width() / halfInk.height()) < 1e-9);
    }

    // Trimming must not change how the sheet is oriented, even when it makes a
    // portrait page's content landscape.
    {
        setTrim(true);
        QList<ProjectPage*> landscapeInk;
        landscapeInk << createTrimPage(crop, QRectF(50, 250, 300, 100));

        QList<ProjectPage*> noInk;
        noInk << createTrimPage(crop, QRectF());

        QCOMPARE((int)project->calcRotation(landscapeInk, layout),
                 (int)project->calcRotation(noInk, layout));

        qDeleteAll(landscapeInk);
        qDeleteAll(noInk);
    }

    setTrim(false);
    delete sheet->page(0);
    delete sheet;
}


/************************************************
 * Scaling a trimmed page up would drag whatever sits outside the box across
 * its neighbours on the sheet, so the content stream has to clip.
 ************************************************/
void TestBoomaga::test_TrimClip()
{
    const QRectF crop(0, 0, 400, 600);

    setTrim(true, false, 0);

    Sheet *sheet = new Sheet(1, 0);
    sheet->setPage(0, createTrimPage(crop, QRectF(100, 150, 200, 300)));

    QString stream;
    TmpPdfFile tmp;
    tmp.getPageStream(&stream, sheet);

    // The clip has to be established before the page is painted, or it does
    // nothing at all.
    const int clipPos = stream.indexOf(" re\nW\nn\n");
    const int drawPos = stream.indexOf(" Do\n");
    QVERIFY2(clipPos > -1, "no clipping path in the page stream");
    QVERIFY2(drawPos > -1, "page was never drawn");
    QVERIFY2(clipPos < drawPos, "clip is set after the page is drawn");

    QVERIFY2(stream.contains("100.000 150.000 200.000 300.000 re"),
             qPrintable("clip is not the trimmed box:\n" + stream));

    setTrim(false);
    delete sheet->page(0);
    delete sheet;
}
