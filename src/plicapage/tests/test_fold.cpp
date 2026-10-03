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


#include "testplicapage.h"

#include <QTest>

// getPageStream() is private; see test_trim.cpp.
#define protected public
#define private public
#include "../plicapagetypes.h"
#include "../kernel/layout.h"
#include "../kernel/project.h"
#include "../kernel/projectpage.h"
#include "../kernel/sheet.h"
#include "../kernel/tmppdffile.h"
#include "../kernel/duplex.h"
#include "../gui/widgets/previewwidget.h"
#include "../settings.h"
#include "../kernel/papersizes.h"
#include <QFile>
#undef private
#undef protected


// Letter, portrait, in points: the booklet sheet before it is turned.
static const QRectF letter(0, 0, 612, 792);

static const QColor gray(153, 153, 153);


/************************************************
 * Only the lines drawn: "x y m" moves, one per stroke segment start.
 ************************************************/
static int moveCount(const QString &stream)
{
    return stream.count(" m\n");
}


/************************************************
 * The fold sits in the middle of the gap between the two pages, which is the
 * center of the margins-adjusted page rect, and runs the full paper width.
 ************************************************/
void TestPlicaPage::test_FoldLineGeometry()
{
    // 1/8 in all round.
    QRectF pageRect = letter.adjusted(9, 9, -9, -9);
    QString s = TmpPdfFile::foldLineStream(FoldLineSolid, false, 144, gray, letter, pageRect);
    QVERIFY2(s.contains("0.000 396.000 m\n612.000 396.000 l\nS\n"), qPrintable(s));
    QVERIFY2(s.contains("[] 0 d"), "a solid line must reset the dash pattern");
    QCOMPARE(moveCount(s), 1);

    // Unequal top and bottom margins move the fold off the paper's center.
    // Layout top 9, bottom 39: the page rect spans 9..753, center 381, which
    // is 792 - 381 = 411 from the bottom.
    pageRect = QRectF(9, 9, 594, 744);
    s = TmpPdfFile::foldLineStream(FoldLineSolid, false, 144, gray, letter, pageRect);
    QVERIFY2(s.contains("0.000 411.000 m\n612.000 411.000 l"), qPrintable(s));

    QVERIFY(TmpPdfFile::foldLineStream(FoldLineNone, false, 144, gray, letter, pageRect).isEmpty());
}


/************************************************

 ************************************************/
void TestPlicaPage::test_FoldLineDotted()
{
    const QString s = TmpPdfFile::foldLineStream(FoldLineDotted, false, 144, gray, letter,
                                                 letter.adjusted(9, 9, -9, -9));
    QVERIFY2(s.contains("1 J\n[0 3] 0 d"), qPrintable(s));
    QCOMPARE(moveCount(s), 1);
}


/************************************************
 * Marks are spaced from the center of the fold out, as far as the pages
 * reach along it - never past them.
 ************************************************/
void TestPlicaPage::test_FoldLineStitches_data()
{
    QTest::addColumn<qreal>("spacing");
    QTest::addColumn<QList<qreal> >("centers");

    // Page rect 9..603 along the fold, center 306, reach 297.
    QTest::newRow("2 in")   << qreal(144) << (QList<qreal>() << 18 << 162 << 306 << 450 << 594);
    QTest::newRow("3 in")   << qreal(216) << (QList<qreal>() << 90 << 306 << 522);
    // Exactly the reach: the outermost marks land on the page edges.
    QTest::newRow("reach")  << qreal(297) << (QList<qreal>() << 9 << 306 << 603);
    QTest::newRow("longer") << qreal(400) << (QList<qreal>() << 306);
}


void TestPlicaPage::test_FoldLineStitches()
{
    QFETCH(qreal, spacing);
    QFETCH(QList<qreal>, centers);

    const QString s = TmpPdfFile::foldLineStream(FoldLineDotted, true, spacing, gray, letter,
                                                 letter.adjusted(9, 9, -9, -9));

    // The fold is dotted under the marks.
    QVERIFY2(s.contains("[0 3] 0 d"), qPrintable(s));

    // The line, then two strokes per cross.
    QCOMPARE(moveCount(s), 1 + 2 * centers.count());

    foreach (qreal x, centers)
    {
        const QString cross = QString("%1 393.000 m\n%2 399.000 l\n")
                .arg(x - 3, 0, 'f', 3)
                .arg(x + 3, 0, 'f', 3);
        QVERIFY2(s.contains(cross), qPrintable(QString("no cross at %1:\n%2").arg(x).arg(s)));
    }

    // The marks are a setting of their own: a solid line takes them as well,
    // in the same places.
    const QString solid = TmpPdfFile::foldLineStream(FoldLineSolid, true, spacing, gray, letter,
                                                     letter.adjusted(9, 9, -9, -9));
    QVERIFY2(!solid.contains("[0 3] 0 d"), qPrintable(solid));
    QCOMPARE(moveCount(solid), 1 + 2 * centers.count());

    // And without them, just the line.
    QCOMPARE(moveCount(TmpPdfFile::foldLineStream(FoldLineDotted, false, spacing, gray, letter,
                                                  letter.adjusted(9, 9, -9, -9))), 1);
}


/************************************************
 * Only booklet sheets carry the fold hint, and the preview sheet that joins
 * two sub-booklets has no fold to draw.
 ************************************************/
void TestPlicaPage::test_FoldLineSheets()
{
    static LayoutNUp *layout = new LayoutNUp(1, 1);
    project->setLayout(layout);
    project->setFoldLine(FoldLineSolid);

    TmpPdfFile tmp;
    QString stream;

    Sheet plain(2, 0);
    tmp.getPageStream(&stream, &plain);
    QVERIFY2(!stream.contains(" l\nS\n"), qPrintable("fold on a sheet without the hint:\n" + stream));

    Sheet booklet(2, 0);
    booklet.setHints(Sheet::HintDrawFold);
    stream.clear();
    tmp.getPageStream(&stream, &booklet);
    QVERIFY2(stream.contains(" l\nS\n"), qPrintable("no fold on a booklet sheet:\n" + stream));

    // Drawn inside the print offset, so it moves with the pages.
    QVERIFY(stream.indexOf(" l\nS\n") < stream.lastIndexOf("Q\n"));

    Sheet joint(2, 0);
    joint.setHints(Sheet::HintDrawFold | Sheet::HintSubBooklet);
    stream.clear();
    tmp.getPageStream(&stream, &joint);
    QVERIFY2(!stream.contains(" l\nS\n"), qPrintable("fold between two sub-booklets:\n" + stream));

    project->setFoldLine(FoldLineNone);
    stream.clear();
    tmp.getPageStream(&stream, &booklet);
    QVERIFY2(!stream.contains(" l\nS\n"), qPrintable("fold drawn with the option off:\n" + stream));
}


/************************************************

 ************************************************/
void TestPlicaPage::test_FoldLineStrings()
{
    foreach (FoldLine f, QList<FoldLine>() << FoldLineNone << FoldLineSolid << FoldLineDotted)
        QCOMPARE(strToFoldLine(foldLineToStr(f)), f);

    // Settings from before the marks had a switch: the line was dotted.
    QCOMPARE(strToFoldLine("Stitched"), FoldLineDotted);

    // An unknown value, e.g. from a newer version's settings, means no line.
    QCOMPARE(strToFoldLine("Wavy"), FoldLineNone);
}


/************************************************
 * A project holding `count` blank pages, enough for the booklet layout to
 * lay out. Pages are owned by the caller.
 ************************************************/
static QList<ProjectPage*> setBookletPages(int count)
{
    QList<ProjectPage*> pages;
    for (int i = 0; i < count; ++i)
        pages << new ProjectPage();

    project->mPages = pages;
    return pages;
}


/************************************************
 * Printed sheets come in pairs, outside then inside. The inside of the last
 * pair written is the innermost sheet's: the center spread.
 ************************************************/
void TestPlicaPage::test_StitchMarksPrintSheets_data()
{
    QTest::addColumn<int>("pageCount");
    QTest::addColumn<QString>("expected");   // per sheet: o/i, C for the center

    QTest::newRow("4 pages")  << 4  << "o iC";
    QTest::newRow("8 pages")  << 8  << "o i o iC";
    QTest::newRow("12 pages") << 12 << "o i o i o iC";
    // Padded with blanks to 12; the center is still the innermost sheet.
    QTest::newRow("10 pages") << 10 << "o i o i o iC";
}


void TestPlicaPage::test_StitchMarksPrintSheets()
{
    QFETCH(int, pageCount);
    QFETCH(QString, expected);

    QList<ProjectPage*> pages = setBookletPages(pageCount);
    LayoutBooklet layout;
    QList<Sheet*> sheets;
    layout.fillSheetsForBook(0, pageCount, &sheets);

    QStringList result;
    foreach (const Sheet *sheet, sheets)
    {
        QString s = sheet->hints().testFlag(Sheet::HintInsideFace) ? "i" : "o";
        if (sheet->hints().testFlag(Sheet::HintCenterSpread))
            s += "C";
        QVERIFY(sheet->hints().testFlag(Sheet::HintDrawFold));
        result << s;
    }
    QCOMPARE(result.join(" "), expected);

    project->mPages.clear();
    qDeleteAll(sheets);
    qDeleteAll(pages);
}


/************************************************
 * In the preview only the spread holding the two middle pages is the center.
 ************************************************/
void TestPlicaPage::test_StitchMarksPreviewSheets()
{
    QList<ProjectPage*> pages = setBookletPages(8);
    LayoutBooklet layout;
    QList<Sheet*> sheets;
    layout.fillPreviewSheetsForBook(0, 8, &sheets);

    // Spreads: [0], [1 2], [3 4], [5 6], [7]
    QCOMPARE(sheets.count(), 5);
    for (int i = 0; i < sheets.count(); ++i)
    {
        const bool center = sheets.at(i)->hints().testFlag(Sheet::HintCenterSpread);
        QCOMPARE(center, i == 2);
    }
    QCOMPARE(sheets.at(2)->page(0), pages.at(3));
    QCOMPARE(sheets.at(2)->page(1), pages.at(4));

    project->mPages.clear();
    qDeleteAll(sheets);
    qDeleteAll(pages);
}


/************************************************
 * The sides setting decides which faces get the line at all; the marks, when
 * on, go wherever the line goes. Columns: the sides, then whether the outside
 * face, an inside face and the center spread get a line.
 ************************************************/
void TestPlicaPage::test_FoldSidesDrawn_data()
{
    QTest::addColumn<int>("sides");
    QTest::addColumn<bool>("outside");
    QTest::addColumn<bool>("inside");
    QTest::addColumn<bool>("center");

    QTest::newRow("all sides")   << int(FoldSidesAll)    << true  << true  << true;
    QTest::newRow("stitch side") << int(FoldSidesInside) << false << true  << true;
    QTest::newRow("center only") << int(FoldSidesCenter) << false << false << true;
}


void TestPlicaPage::test_FoldSidesDrawn()
{
    QFETCH(int, sides);
    QFETCH(bool, outside);
    QFETCH(bool, inside);
    QFETCH(bool, center);

    static LayoutNUp *layout = new LayoutNUp(1, 1);
    project->setLayout(layout);
    project->setFoldLine(FoldLineSolid);
    project->setFoldSides(FoldSides(sides));

    Sheet outsideSheet(2, 0);
    outsideSheet.setHints(Sheet::HintDrawFold);
    Sheet insideSheet(2, 1);
    insideSheet.setHints(Sheet::HintDrawFold | Sheet::HintInsideFace);
    Sheet centerSheet(2, 2);
    centerSheet.setHints(Sheet::HintDrawFold | Sheet::HintInsideFace | Sheet::HintCenterSpread);

    TmpPdfFile tmp;
    for (int marks = 0; marks < 2; ++marks)
    {
        project->setStitchMarks(marks);
        auto check = [&](const Sheet &sheet, bool line, const char *name)
        {
            QString stream;
            tmp.getPageStream(&stream, &sheet);
            // The fold is one move; each cross adds two.
            const int moves = stream.count(" m\n");
            QVERIFY2((moves > 0) == line,
                     qPrintable(QString("%1: line %2 expected").arg(name).arg(line ? "was" : "was not")));
            QVERIFY2((moves > 1) == (line && marks),
                     qPrintable(QString("%1: marks %2 expected").arg(name).arg(line && marks ? "were" : "were not")));
        };

        check(outsideSheet, outside, "outside face");
        check(insideSheet,  inside,  "inside face");
        check(centerSheet,  center,  "center spread");
    }

    project->setFoldLine(FoldLineNone);
    project->setFoldSides(FoldSidesAll);
    project->setStitchMarks(false);
}


/************************************************

 ************************************************/
void TestPlicaPage::test_FoldSidesStrings()
{
    foreach (FoldSides m, QList<FoldSides>() << FoldSidesAll << FoldSidesInside << FoldSidesCenter)
        QCOMPARE(strToFoldSides(foldSidesToStr(m)), m);

    // The strings the stitch-mark sides were stored under, which carry over.
    QCOMPARE(strToFoldSides("Center"), FoldSidesCenter);
    QCOMPARE(strToFoldSides(""), FoldSidesAll);
}


/************************************************
 * Restoring several deleted pages at once - the "All deleted pages" menu
 * items - shows every hidden one and makes the first of them current.
 ************************************************/
void TestPlicaPage::test_UndoDeletePages()
{
    static LayoutNUp *layout = new LayoutNUp(1, 1);
    project->setLayout(layout);

    Job job;
    for (int i = 0; i < 5; ++i)
        job.addPage(new ProjectPage());

    project->mJobs.clear();
    project->mJobs << job;

    job.page(1)->hide();
    job.page(3)->hide();
    job.page(4)->hide();
    project->update();
    QCOMPARE(project->pageCount(), 2);

    QList<ProjectPage*> pages;
    for (int i = 0; i < job.pageCount(); ++i)
        pages << job.page(i);

    project->undoDeletePages(pages);
    QCOMPARE(project->pageCount(), 5);
    QCOMPARE(project->currentPage(), job.page(1));

    // Nothing deleted: nothing changes, current page included.
    project->setCurrentPage(job.page(4));
    project->undoDeletePages(pages);
    QCOMPARE(project->currentPage(), job.page(4));

    project->mJobs.clear();
    project->update();
}


/************************************************
 * The chosen color strokes both the line and the marks: it is set once, and
 * nothing after it sets another.
 ************************************************/
void TestPlicaPage::test_FoldLineColor()
{
    const QString s = TmpPdfFile::foldLineStream(FoldLineDotted, true, 144, QColor(255, 0, 51),
                                                 letter, letter.adjusted(9, 9, -9, -9));
    QVERIFY2(s.startsWith("q\n1.000 0.000 0.200 RG\n"), qPrintable(s));
    QCOMPARE(s.count(" RG\n"), 1);
    QVERIFY2(!s.contains(" G\n"), qPrintable("a gray stroke color overrides the choice:\n" + s));

    // An invalid color, e.g. a mangled setting, leaves the current one.
    project->setFoldLineColor(QColor(10, 20, 30));
    project->setFoldLineColor(QColor("not a color"));
    QCOMPARE(project->foldLineColor(), QColor(10, 20, 30));
    project->setFoldLineColor(gray);
}


/************************************************
 * A project of `count` visible pages in one job, laid out 1-up.
 ************************************************/
static Job setSelectionPages(int count)
{
    static LayoutNUp *layout = new LayoutNUp(1, 1);
    project->setLayout(layout);

    // A page number makes it a real page: one without is an inserted blank,
    // which deleting removes rather than hides.
    Job job;
    for (int i = 0; i < count; ++i)
        job.addPage(new ProjectPage(i));

    project->mJobs.clear();
    project->mJobs << job;
    project->clearSelection();
    project->mSelectionAnchor = nullptr;
    project->update();
    return job;
}


static QList<int> selectedNums(const Job &job)
{
    QList<int> res;
    foreach (ProjectPage *page, project->selectedPages())
        res << job.indexOfPage(page) + 1;
    return res;
}


/************************************************
 * Click, Ctrl+click and Shift+click, numbered from 1 as the user sees them.
 ************************************************/
void TestPlicaPage::test_PageSelection()
{
    Job job = setSelectionPages(8);

    // Click 4, Ctrl+click 6: both.
    project->selectPage(job.page(3));
    project->togglePageSelection(job.page(5));
    QCOMPARE(selectedNums(job), QList<int>() << 4 << 6);

    // Ctrl+click 4 again: just 6.
    project->togglePageSelection(job.page(3));
    QCOMPARE(selectedNums(job), QList<int>() << 6);

    // Click 2, Shift+click 4: 2 to 4. Shift+click 1 from the same anchor: 1 to 2.
    project->selectPage(job.page(1));
    project->selectPageRange(job.page(3));
    QCOMPARE(selectedNums(job), QList<int>() << 2 << 3 << 4);
    project->selectPageRange(job.page(0));
    QCOMPARE(selectedNums(job), QList<int>() << 1 << 2);

    // Ctrl+click moves the anchor, and Ctrl+Shift adds a range from it:
    // click 2, Ctrl+click 7, Ctrl+Shift+click 8.
    project->selectPage(job.page(1));
    project->togglePageSelection(job.page(6));
    project->selectPageRange(job.page(7), true);
    QCOMPARE(selectedNums(job), QList<int>() << 2 << 7 << 8);

    project->clearSelection();
    QVERIFY(project->selectedPages().isEmpty());

    project->mJobs.clear();
    project->update();
}


/************************************************
 * Deleting the selection removes every selected page in one go, empties the
 * selection, and lands on the next page left.
 ************************************************/
void TestPlicaPage::test_DeleteSelectedPages()
{
    Job job = setSelectionPages(8);

    project->selectPage(job.page(3));
    project->togglePageSelection(job.page(5));
    project->deleteSelectedPages();

    QCOMPARE(project->pageCount(), 6);
    QVERIFY(!job.page(3)->visible());
    QVERIFY(!job.page(5)->visible());
    QVERIFY(project->selectedPages().isEmpty());
    QCOMPARE(project->currentPage(), job.page(6));

    // The last pages: the current page falls back to the one before them.
    project->selectPage(job.page(6));
    project->selectPageRange(job.page(7));
    project->deleteSelectedPages();
    QCOMPARE(project->pageCount(), 4);
    QCOMPARE(project->currentPage(), job.page(4));

    // An inserted blank page is removed outright, as by "Delete this page".
    ProjectPage *blank = new ProjectPage();
    project->mJobs[0].insertPage(1, blank);
    project->update();
    QCOMPARE(project->pageCount(), 5);
    project->selectPage(blank);
    project->deleteSelectedPages();
    QCOMPARE(project->pageCount(), 4);
    QCOMPARE(project->mJobs.at(0).indexOfPage(blank), -1);

    // Undo delete all brings back the hidden ones.
    project->undoDeletePages(QList<ProjectPage*>() << job.page(3) << job.page(5)
                                                   << job.page(6) << job.page(7));
    QCOMPARE(project->pageCount(), 8);

    // Nothing selected: nothing happens.
    project->deleteSelectedPages();
    QCOMPARE(project->pageCount(), 8);

    project->mJobs.clear();
    project->update();
}


/************************************************
 * The printer's own limits come from the PPD entry for the profile's paper
 * size - the queue default when the profile names none - and are zero when
 * the PPD has nothing to say.
 ************************************************/
void TestPlicaPage::test_HardwareMargins()
{
    Printer printer("Fake");

    PpdPaperSize letter;
    letter.keyword = "Letter";
    letter.size    = QSizeF(612, 792);
    letter.left = 8.39; letter.top = 8.39; letter.right = 8.39; letter.bottom = 36;

    PpdPaperSize a4;
    a4.keyword = "A4";
    a4.size    = QSizeF(595, 842);
    a4.left = 10; a4.top = 12; a4.right = 14; a4.bottom = 16;

    printer.mPaperSizes = QList<PpdPaperSize>() << letter << a4;
    printer.mDefaultPaperSizeName = "Letter";

    PrinterProfile profile;
    QCOMPARE(printer.hardwareMargins(profile), QMarginsF(8.39, 8.39, 8.39, 36));

    profile.setPaperSizeName("A4");
    QCOMPARE(printer.hardwareMargins(profile), QMarginsF(10, 12, 14, 16));

    profile.setPaperSizeName("Tabloid");
    QVERIFY(printer.hardwareMargins(profile).isNull());

    // The current profile's paper, less those margins, in the layout's frame:
    // y runs down from the top edge.
    PrinterProfile current;
    current.setPaperSize(QSizeF(612, 792), UnitPoint);
    printer.mProfiles = QVector<PrinterProfile>() << current;
    printer.setCurrentProfile(0);
    QCOMPARE(printer.printableRect(), QRectF(8.39, 8.39, 612 - 2 * 8.39, 792 - 8.39 - 36));
}


/************************************************
 * How CUPS lands each /Rotate on portrait paper, by the PPD's
 * *LandscapeOrientation - measured through the real filter chain for the
 * HP DeskJet 2700 (Minus90) and Brother HL-2270DW (Plus90) queues.
 ************************************************/
void TestPlicaPage::test_PageTurnedOnPaper()
{
    Printer printer("Fake");

    printer.mLandscape = -90;   // Minus90, and the default
    QCOMPARE(printer.pageTurnedOnPaper(NoRotate),  false);
    QCOMPARE(printer.pageTurnedOnPaper(Rotate90),  true);
    QCOMPARE(printer.pageTurnedOnPaper(Rotate180), true);
    QCOMPARE(printer.pageTurnedOnPaper(Rotate270), false);

    printer.mLandscape = 90;    // Plus90
    QCOMPARE(printer.pageTurnedOnPaper(NoRotate),  false);
    QCOMPARE(printer.pageTurnedOnPaper(Rotate90),  false);
    QCOMPARE(printer.pageTurnedOnPaper(Rotate180), true);
    QCOMPARE(printer.pageTurnedOnPaper(Rotate270), true);
}


/************************************************
 * Which printed sides go through the printer turned round. Columns: the
 * printing set-up, then whether sheet 0 (a front) and sheet 1 (a back) are
 * turned.
 ************************************************/
void TestPlicaPage::test_SheetGoesThroughTurned_data()
{
    QTest::addColumn<int>("landscape");
    QTest::addColumn<int>("rotation");
    QTest::addColumn<bool>("doubleSided");
    QTest::addColumn<int>("duplex");
    QTest::addColumn<int>("autoFlip");
    QTest::addColumn<int>("manualFlip");
    QTest::addColumn<bool>("reverseOrder");
    QTest::addColumn<bool>("front");
    QTest::addColumn<bool>("back");

    const int A = DuplexAuto, M = DuplexManual;
    const int L = int(FlipType::LongEdge), S = int(FlipType::ShortEdge);
    const int P = 90, N = -90;
    const int R0 = NoRotate, R90 = Rotate90;

    QTest::newRow("portrait one-sided")        << P << R0  << false << A << L << L << false << false << false;
    QTest::newRow("landscape one-sided, P90")  << P << R90 << false << A << L << L << false << false << false;
    QTest::newRow("landscape one-sided, M90")  << N << R90 << false << A << L << L << false << true  << true;
    QTest::newRow("portrait auto, long edge")  << P << R0  << true  << A << L << L << false << false << true;
    QTest::newRow("portrait auto, short edge") << P << R0  << true  << A << S << L << false << false << false;

    // The printout this was checked against: an 8-page booklet on the HP,
    // manual duplex turned on the long edge. The pre-rotated outsides
    // (/Rotate 270) lost the left pages, 8 and 6, to the printer's bottom
    // band - the frame's bottom; the insides (/Rotate 90) lost the right
    // pages, 7 and 5 - the frame's top.
    QTest::newRow("HP booklet, manual long")   << N << R90 << true  << M << S << L << false << false << true;
    // The same on a Plus90 printer comes out the other way about.
    QTest::newRow("P90 booklet, manual long")  << P << R90 << true  << M << S << L << false << true  << false;
    // No pre-rotation: both sides as the sheet's /Rotate lands.
    QTest::newRow("HP booklet, manual short")  << N << R90 << true  << M << S << S << false << true  << true;
    // In reverse order the first pass, the pre-rotated one, is the backs.
    QTest::newRow("HP booklet, manual long, reversed") << N << R90 << true << M << S << L << true << true << false;
}


void TestPlicaPage::test_SheetGoesThroughTurned()
{
    QFETCH(int, landscape);
    QFETCH(int, rotation);
    QFETCH(bool, doubleSided);
    QFETCH(int, duplex);
    QFETCH(int, autoFlip);
    QFETCH(int, manualFlip);
    QFETCH(bool, reverseOrder);
    QFETCH(bool, front);
    QFETCH(bool, back);

    Printer printer("Fake");
    printer.mLandscape = landscape;

    PrinterProfile profile;
    profile.setDuplexType(DuplexType(duplex));
    profile.setManualFlipType(FlipType(manualFlip));
    profile.setReverseOrder(reverseOrder);

    // The pattern repeats every two sheets.
    for (int sheet = 0; sheet < 4; ++sheet)
    {
        const bool expected = (sheet % 2) ? back : front;
        QCOMPARE(sheetGoesThroughTurned(sheet, doubleSided, profile, printer,
                                        FlipType(autoFlip), Rotation(rotation)), expected);
    }
}


/************************************************

 ************************************************/
void TestPlicaPage::test_TurnMargins()
{
    QCOMPARE(turnMargins(QMarginsF(1, 2, 3, 4)), QMarginsF(3, 4, 1, 2));
    QCOMPARE(turnMargins(turnMargins(QMarginsF(1, 2, 3, 4))), QMarginsF(1, 2, 3, 4));
}


/************************************************
 * The preview shows reading spreads; the project records which printed sheet
 * each page is on, which for a booklet is not the spread it is shown in.
 ************************************************/
void TestPlicaPage::test_PrintSheetIndex()
{
    static LayoutBooklet *layout = new LayoutBooklet();
    project->setLayout(layout);

    Job job;
    for (int i = 0; i < 8; ++i)
        job.addPage(new ProjectPage(i));
    project->mJobs.clear();
    project->mJobs << job;
    project->update();

    // Sheet 0: 8 and 1, sheet 1: 2 and 7, sheet 2: 6 and 3, sheet 3: 4 and 5.
    const int expected[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };
    for (int i = 0; i < 8; ++i)
        QCOMPARE(project->printSheetIndex(job.page(i)), expected[i]);

    project->mJobs.clear();
    project->update();
    QCOMPARE(project->printSheetIndex(job.page(0)), -1);

    static LayoutNUp *oneUp = new LayoutNUp(1, 1);
    project->setLayout(oneUp);
}


/************************************************
 * The status bar's figures. Columns: layout, double-sided, page (from 1),
 * then the expected "Sheet X of Y" and whether it is the second side.
 ************************************************/
void TestPlicaPage::test_PaperPosition_data()
{
    QTest::addColumn<QString>("layout");
    QTest::addColumn<bool>("doubleSided");
    QTest::addColumn<int>("page");
    QTest::addColumn<int>("paper");
    QTest::addColumn<int>("papers");
    QTest::addColumn<bool>("second");

    // The case reported: 8 pages 1-up double-sided is 4 sheets, page 4 on
    // the back of the second - not "Sheet 4 of 8".
    QTest::newRow("1up duplex p1")  << "1up" << true  << 1 << 1 << 4 << false;
    QTest::newRow("1up duplex p4")  << "1up" << true  << 4 << 2 << 4 << true;
    QTest::newRow("1up duplex p8")  << "1up" << true  << 8 << 4 << 4 << true;
    QTest::newRow("1up simplex p4") << "1up" << false << 4 << 4 << 8 << false;
    QTest::newRow("2up duplex p5")  << "2up" << true  << 5 << 2 << 2 << false;
    // Booklet: sheet 1 holds 8|1 outside and 2|7 inside, sheet 2 6|3 and 4|5.
    QTest::newRow("booklet p1")     << "Booklet" << true << 1 << 1 << 2 << false;
    QTest::newRow("booklet p7")     << "Booklet" << true << 7 << 1 << 2 << true;
    QTest::newRow("booklet p3")     << "Booklet" << true << 3 << 2 << 2 << false;
    QTest::newRow("booklet p5")     << "Booklet" << true << 5 << 2 << 2 << true;
}


void TestPlicaPage::test_PaperPosition()
{
    QFETCH(QString, layout);
    QFETCH(bool, doubleSided);
    QFETCH(int, page);
    QFETCH(int, paper);
    QFETCH(int, papers);
    QFETCH(bool, second);

    static LayoutNUp     *oneUp   = new LayoutNUp(1, 1);
    static LayoutNUp     *twoUp   = new LayoutNUp(2, 1);
    static LayoutBooklet *booklet = new LayoutBooklet();
    project->setLayout(layout == "1up" ? (Layout*)oneUp : layout == "2up" ? (Layout*)twoUp : (Layout*)booklet);
    project->setDoubleSided(doubleSided);

    Job job;
    for (int i = 0; i < 8; ++i)
        job.addPage(new ProjectPage(i));
    project->mJobs.clear();
    project->mJobs << job;
    project->update();

    const int sheet = project->printSheetIndex(job.page(page - 1));
    QCOMPARE(project->paperOf(sheet) + 1, paper);
    QCOMPARE(project->paperCount(), papers);
    QCOMPARE(project->isSecondSide(sheet), second);

    project->mJobs.clear();
    project->setLayout(oneUp);
    project->setDoubleSided(true);
    project->update();
}


/************************************************
 * The wording the status bar and the page tooltip share, and the tooltip's
 * content for a booklet page.
 ************************************************/
void TestPlicaPage::test_SheetDescription()
{
    static LayoutNUp     *oneUp   = new LayoutNUp(1, 1);
    static LayoutBooklet *booklet = new LayoutBooklet();

    Job job;
    for (int i = 0; i < 8; ++i)
        job.addPage(new ProjectPage(i));
    job.setTitle("Sample Job");
    project->mJobs.clear();
    project->mJobs << job;

    project->setLayout(oneUp);
    project->setDoubleSided(true);
    project->update();
    QCOMPARE(project->sheetDescription(project->printSheetIndex(job.page(3))), QString("Sheet 2 of 4, back"));

    project->setDoubleSided(false);
    QCOMPARE(project->sheetDescription(project->printSheetIndex(job.page(3))), QString("Sheet 4 of 8"));

    project->setLayout(booklet);
    project->update();
    QCOMPARE(project->sheetDescription(project->printSheetIndex(job.page(6))), QString("Sheet 1 of 2, inside"));
    QCOMPARE(project->sheetDescription(project->printSheetIndex(job.page(2))), QString("Sheet 2 of 2, outside"));

    // The spread showing pages 2 and 3: page 2 is printed on the inside of
    // the outer sheet.
    const Sheet *spread = project->previewSheet(1);
    QCOMPARE(spread->page(0), job.page(1));
    const QString tip = PreviewWidget::pageToolTip(spread, 0);
    QVERIFY2(tip.contains("Page 2 of 8"), qPrintable(tip));
    QVERIFY2(tip.contains("Job 1, Sample Job: page 2 of 8"), qPrintable(tip));
    QVERIFY2(tip.contains("Sheet 1 of 2, inside"), qPrintable(tip));
    QVERIFY2(tip.contains("Scale"), qPrintable(tip));

    project->mJobs.clear();
    project->setLayout(oneUp);
    project->setDoubleSided(true);
    project->update();
}


/************************************************
 * The whole-job report, on the set-up whose printout lost pages 5-8: an
 * 8-page booklet of 5 x 8.25 in pages on Letter, 1/8 in margins and a 3/4 in
 * gap at the spine, on the HP DeskJet 2700 (Minus90, 1/2 in bottom limit),
 * manual duplex turned on the long edge.
 ************************************************/
void TestPlicaPage::test_PrintableAreaReport()
{
    settings->setValue(Settings::Units, unitToStr(UnitInch));

    Printer printer("HP");
    PpdPaperSize letter;
    letter.keyword = "Letter";
    letter.size    = QSizeF(612, 792);
    letter.left = 8.39; letter.top = 8.39; letter.right = 8.39; letter.bottom = 36;
    printer.mPaperSizes = QList<PpdPaperSize>() << letter;
    printer.mDefaultPaperSizeName = "Letter";
    printer.mLandscape = -90;

    PrinterProfile profile;
    profile.setPaperSize(QSizeF(612, 792), UnitPoint);
    profile.setLeftMargin(9, UnitPoint);
    profile.setRightMargin(9, UnitPoint);
    profile.setTopMargin(9, UnitPoint);
    profile.setBottomMargin(9, UnitPoint);
    profile.setInternalMargin(54, UnitPoint);
    profile.setDuplexType(DuplexManual);
    profile.setManualFlipType(FlipType::LongEdge);
    printer.mProfiles = QVector<PrinterProfile>() << profile;
    printer.setCurrentProfile(0);

    Printer *oldPrinter = project->mPrinter;
    project->mPrinter = &printer;

    static LayoutBooklet *booklet = new LayoutBooklet();
    project->setLayout(booklet);

    Job job;
    for (int i = 0; i < 8; ++i)
    {
        PdfPageInfo info;
        info.cropBox  = QRectF(0, 0, 360, 594);
        info.mediaBox = info.cropBox;
        ProjectPage *page = new ProjectPage(i);
        page->setPdfInfo(info);
        job.addPage(page);
    }
    project->mJobs.clear();
    project->mJobs << job;
    project->update();

    QList<int> clipped;
    foreach (const Project::ClippedPage &c, project->clippedPages())
        clipped << c.page->pageNum() + 1;
    QCOMPARE(clipped, QList<int>() << 5 << 6 << 7 << 8);

    bool fits = true;
    const QStringList report = project->printableAreaReport(&fits);
    QVERIFY(!fits);
    QCOMPARE(report.join("\n"), QString(
                 "4 of 8 pages will be cut off by HP:\n"
                 "Pages 5–8 lose 0.375 in at the outer edge.\n"
                 "To fit every page, set the Top and Bottom margins to at least 0.500 in "
                 "in Printer settings → Margins."));

    // "Fit margins to printer": the crossed edges go to the printer's limit,
    // the rest stay - 1/8 in at the sides is already inside the HP's reach.
    QVERIFY(project->fitMarginsToPrinter());
    QCOMPARE(printer.currentProfile()->topMargin(),    36.0);
    QCOMPARE(printer.currentProfile()->bottomMargin(), 36.0);
    QCOMPARE(printer.currentProfile()->leftMargin(),   9.0);
    QCOMPARE(printer.currentProfile()->rightMargin(),  9.0);
    QCOMPARE(printer.currentProfile()->internalMargin(), 54.0);
    QVERIFY(project->clippedPages().isEmpty());
    QVERIFY(!project->fitMarginsToPrinter());
    QCOMPARE(project->printableAreaReport(&fits), QStringList() << "Every page fits within what HP can print.");
    QVERIFY(fits);

    project->mPrinter = oldPrinter;
    project->mJobs.clear();
    static LayoutNUp *oneUp = new LayoutNUp(1, 1);
    project->setLayout(oneUp);
    project->update();
}


/************************************************
 * The PPD a queue gets: the template plus the user's sizes, each in the four
 * places a size needs, imageable to the edge.
 ************************************************/
void TestPlicaPage::test_PpdWithPaperSizes()
{
    QFile f(QString(TEST_DATA_DIR) + "../../../backend/cups_backend/plicapage.ppd");
    QVERIFY(f.open(QFile::ReadOnly));
    const QString ppd = QString::fromLatin1(f.readAll());

    UserPaperSize a;  a.name = "5 x 8.25 in";   a.size = QSizeF(360, 594);
    UserPaperSize b;  b.name = "Half: \"narrow\""; b.size = QSizeF(324, 594);
    UserPaperSize letter; letter.name = "My Letter"; letter.size = QSizeF(612, 792);

    QCOMPARE(a.keyword(), QString("w360h594"));
    QCOMPARE(b.keyword(), QString("w324h594"));

    // Characters that would end a PPD translation string are dropped.
    QCOMPARE(ppdPaperName(b), QString("Half narrow"));
    UserPaperSize unnamed; unnamed.size = QSizeF(324, 594);
    QCOMPARE(ppdPaperName(unnamed), QString("4.5 x 8.25 in"));

    const QString out = ppdWithPaperSizes(ppd, QList<UserPaperSize>() << a << b << letter);

    foreach (const QString &line, QStringList()
             << "*PageSize w324h594/Half narrow: \"@PJL SET PAPER=CUSTOM<0A>\""
             << "*PageRegion w324h594/Half narrow: \"@PJL SET PAPER=CUSTOM<0A>\""
             << "*ImageableArea w324h594/Half narrow: \"0 0 324.00 594.00\""
             << "*PaperDimension w324h594/Half narrow: \"324.00 594.00\"")
        QVERIFY2(out.contains(line + "\n"), qPrintable(line));

    // Within their option, not after it closes.
    QVERIFY(out.indexOf("*PageSize w360h594/") < out.indexOf("*JCLCloseUI: *PageSize"));
    QVERIFY(out.indexOf("*PageRegion w360h594/") < out.indexOf("*JCLCloseUI: *PageRegion"));
    QVERIFY(out.indexOf("*PageRegion w360h594/") > out.indexOf("*JCLCloseUI: *PageSize"));

    // A size the template already has - 612 x 792 is Letter's keyword only
    // if it were named so; here it is new - and duplicates go in once.
    QCOMPARE(out.count("*PaperDimension w612h792/"), 1);
    const QString twice = ppdWithPaperSizes(ppd, QList<UserPaperSize>() << a << a);
    QCOMPARE(twice.count("*PaperDimension w360h594/"), 1);

    // Nothing to add, nothing changed.
    QCOMPARE(ppdWithPaperSizes(ppd, QList<UserPaperSize>()), ppd);

    // Left for cupstestppd to check by hand.
    QFile o(QString(TEST_OUT_DIR) + "papersizes.ppd");
    QVERIFY(o.open(QFile::WriteOnly));
    o.write(out.toLatin1());
}
