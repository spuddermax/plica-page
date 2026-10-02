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
#undef private
#undef protected


// Letter, portrait, in points: the booklet sheet before it is turned.
static const QRectF letter(0, 0, 612, 792);


/************************************************
 * Only the lines drawn: "x y m" moves, one per stroke segment start.
 ************************************************/
static int moveCount(const QString &stream)
{
    return stream.count(" m\n");
}


/************************************************
 * The fold sits in the middle of the gap between the two pages, which is the
 * centre of the margins-adjusted page rect, and runs the full paper width.
 ************************************************/
void TestPlicaPage::test_FoldLineGeometry()
{
    // 1/8 in all round.
    QRectF pageRect = letter.adjusted(9, 9, -9, -9);
    QString s = TmpPdfFile::foldLineStream(FoldLineSolid, 144, letter, pageRect);
    QVERIFY2(s.contains("0.000 396.000 m\n612.000 396.000 l\nS\n"), qPrintable(s));
    QVERIFY2(s.contains("[] 0 d"), "a solid line must reset the dash pattern");
    QCOMPARE(moveCount(s), 1);

    // Unequal top and bottom margins move the fold off the paper's centre.
    // Layout top 9, bottom 39: the page rect spans 9..753, centre 381, which
    // is 792 - 381 = 411 from the bottom.
    pageRect = QRectF(9, 9, 594, 744);
    s = TmpPdfFile::foldLineStream(FoldLineSolid, 144, letter, pageRect);
    QVERIFY2(s.contains("0.000 411.000 m\n612.000 411.000 l"), qPrintable(s));

    QVERIFY(TmpPdfFile::foldLineStream(FoldLineNone, 144, letter, pageRect).isEmpty());
}


/************************************************

 ************************************************/
void TestPlicaPage::test_FoldLineDotted()
{
    const QString s = TmpPdfFile::foldLineStream(FoldLineDotted, 144, letter,
                                                 letter.adjusted(9, 9, -9, -9));
    QVERIFY2(s.contains("1 J\n[0 3] 0 d"), qPrintable(s));
    QCOMPARE(moveCount(s), 1);
}


/************************************************
 * Marks are spaced from the centre of the fold out, as far as the pages
 * reach along it - never past them.
 ************************************************/
void TestPlicaPage::test_FoldLineStitches_data()
{
    QTest::addColumn<qreal>("spacing");
    QTest::addColumn<QList<qreal> >("centres");

    // Page rect 9..603 along the fold, centre 306, reach 297.
    QTest::newRow("2 in")   << qreal(144) << (QList<qreal>() << 18 << 162 << 306 << 450 << 594);
    QTest::newRow("3 in")   << qreal(216) << (QList<qreal>() << 90 << 306 << 522);
    // Exactly the reach: the outermost marks land on the page edges.
    QTest::newRow("reach")  << qreal(297) << (QList<qreal>() << 9 << 306 << 603);
    QTest::newRow("longer") << qreal(400) << (QList<qreal>() << 306);
}


void TestPlicaPage::test_FoldLineStitches()
{
    QFETCH(qreal, spacing);
    QFETCH(QList<qreal>, centres);

    const QString s = TmpPdfFile::foldLineStream(FoldLineStitched, spacing, letter,
                                                 letter.adjusted(9, 9, -9, -9));

    // The fold is dotted under the marks.
    QVERIFY2(s.contains("[0 3] 0 d"), qPrintable(s));

    // The line, then two strokes per cross.
    QCOMPARE(moveCount(s), 1 + 2 * centres.count());

    foreach (qreal x, centres)
    {
        const QString cross = QString("%1 393.000 m\n%2 399.000 l\n")
                .arg(x - 3, 0, 'f', 3)
                .arg(x + 3, 0, 'f', 3);
        QVERIFY2(s.contains(cross), qPrintable(QString("no cross at %1:\n%2").arg(x).arg(s)));
    }
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
    foreach (FoldLine f, QList<FoldLine>() << FoldLineNone << FoldLineSolid
                                           << FoldLineDotted << FoldLineStitched)
        QCOMPARE(strToFoldLine(foldLineToStr(f)), f);

    // An unknown value, e.g. from a newer version's settings, means no line.
    QCOMPARE(strToFoldLine("Wavy"), FoldLineNone);
}
