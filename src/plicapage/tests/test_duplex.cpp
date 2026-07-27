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

#define protected public
#define private public
#include "../plicapagetypes.h"
#include "../kernel/duplex.h"
#include "../kernel/printer.h"
#include "../settings.h"
#undef private
#undef protected


// FlipType already has Q_DECLARE_METATYPE in plicapagetypes.h.
Q_DECLARE_METATYPE(Project::PagesType)
Q_DECLARE_METATYPE(Project::PagesOrder)


/************************************************
 * The behaviour of the two duplex modes that existed before the flip edge and
 * the stacking order were stored separately.
 *
 * These rows are the contract: whatever else changes, a profile carrying a
 * migrated legacy setting has to produce exactly the passes it always did, or
 * everyone's double-sided printing silently breaks.
 *
 *   DuplexManual        == long edge,  order preserved
 *   DuplexManualReverse == short edge, order reversed
 ************************************************/
void TestPlicaPage::test_DuplexPasses_data()
{
    QTest::addColumn<FlipType>("flip");
    QTest::addColumn<bool>("reversesOrder");
    QTest::addColumn<bool>("docReverseOrder");
    QTest::addColumn<bool>("landscape");
    QTest::addColumn<Project::PagesType>("pages1");
    QTest::addColumn<Project::PagesType>("pages2");
    QTest::addColumn<Project::PagesOrder>("order1");
    QTest::addColumn<Project::PagesOrder>("order2");
    QTest::addColumn<bool>("rotate1");

    const FlipType L = FlipType::LongEdge;
    const FlipType S = FlipType::ShortEdge;
    const Project::PagesType  odd = Project::OddPages;
    const Project::PagesType  even = Project::EvenPages;
    const Project::PagesOrder fwd = Project::ForwardOrder;
    const Project::PagesOrder back = Project::BackOrder;

    // ---- legacy DuplexManual: long edge, order preserved -------------------
    QTest::newRow("legacy Manual, portrait")
            << L << false << false << false   << odd << even << fwd << fwd  << false;
    QTest::newRow("legacy Manual, landscape")
            << L << false << false << true    << odd << even << fwd << fwd  << true;
    QTest::newRow("legacy Manual, portrait, reverse order")
            << L << false << true  << false   << even << odd << back << back << false;
    QTest::newRow("legacy Manual, landscape, reverse order")
            << L << false << true  << true    << even << odd << back << back << true;

    // ---- legacy DuplexManualReverse: short edge, order reversed ------------
    QTest::newRow("legacy ManualReverse, portrait")
            << S << true  << false << false   << odd << even << fwd << back << true;
    QTest::newRow("legacy ManualReverse, landscape")
            << S << true  << false << true    << odd << even << fwd << back << false;
    QTest::newRow("legacy ManualReverse, portrait, reverse order")
            << S << true  << true  << false   << even << odd << back << fwd << true;
    QTest::newRow("legacy ManualReverse, landscape, reverse order")
            << S << true  << true  << true    << even << odd << back << fwd << false;

    // ---- the two combinations the old model could not express --------------
    // A printer that stacks face down but whose sheets come back the same way
    // up: order reverses, no rotation.
    QTest::newRow("new: long edge, order reversed, portrait")
            << L << true  << false << false   << odd << even << fwd << back << false;
    QTest::newRow("new: long edge, order reversed, landscape")
            << L << true  << false << true    << odd << even << fwd << back << true;

    // A face-up printer whose sheets come back inverted: order preserved,
    // rotation needed.
    QTest::newRow("new: short edge, order preserved, portrait")
            << S << false << false << false   << odd << even << fwd << fwd  << true;
    QTest::newRow("new: short edge, order preserved, landscape")
            << S << false << false << true    << odd << even << fwd << fwd  << false;
}


/************************************************

 ************************************************/
void TestPlicaPage::test_DuplexPasses()
{
    QFETCH(FlipType, flip);
    QFETCH(bool, reversesOrder);
    QFETCH(bool, docReverseOrder);
    QFETCH(bool, landscape);
    QFETCH(Project::PagesType,  pages1);
    QFETCH(Project::PagesType,  pages2);
    QFETCH(Project::PagesOrder, order1);
    QFETCH(Project::PagesOrder, order2);
    QFETCH(bool, rotate1);

    const DuplexPasses res = calcDuplexPasses(flip, reversesOrder,
                                              docReverseOrder, landscape);

    QCOMPARE((int)res.pages1, (int)pages1);
    QCOMPARE((int)res.pages2, (int)pages2);
    QCOMPARE((int)res.order1, (int)order1);
    QCOMPARE((int)res.order2, (int)order2);
    QCOMPARE(res.rotate1, rotate1);

    // The second pass must cover the sides the first one did not.
    QVERIFY(res.pages1 != res.pages2);
}


/************************************************
 * A profile saved before the split has no key for the stacking order, so it has
 * to be reconstructed from the old DuplexType. Getting this wrong would flip
 * every existing user's duplex settings on upgrade.
 ************************************************/
void TestPlicaPage::test_DuplexLegacyMigration()
{
    struct Case {
        const char *stored;
        bool expectReversed;
        FlipType expectFlip;
    };

    const Case cases[] = {
        { "ManualReverse", true,  FlipType::ShortEdge },
        { "Manual",        false, FlipType::LongEdge  },
    };

    for (const Case &c : cases)
    {
        settings->beginGroup(QString("TestLegacyDuplex_%1").arg(c.stored));

        // Exactly what an old settings file holds: a DuplexType and no more.
        settings->setValue(Settings::PrinterProfile_DuplexType, QString(c.stored));
        settings->remove(QString("ManualDuplexReversesOrder"));
        settings->remove(QString("ManualFlipType"));

        PrinterProfile profile;
        profile.readSettings();

        QCOMPARE(profile.manualDuplexReversesOrder(), c.expectReversed);
        QCOMPARE((int)profile.manualFlipType(), (int)c.expectFlip);
        QVERIFY2(!profile.duplexCalibrated(),
                 "a migrated profile must still be offered the wizard");

        settings->endGroup();
    }

    // Once the new key exists it wins, and the flip edge is whatever was stored
    // rather than something inferred from the legacy enum.
    {
        settings->beginGroup("TestLegacyDuplex_explicit");
        settings->setValue(Settings::PrinterProfile_DuplexType, QString("ManualReverse"));
        settings->setValue(Settings::PrinterProfile_ManualDuplexReversesOrder, false);
        settings->setValue(Settings::PrinterProfile_ManualFlipType, QString("LongEdge"));

        PrinterProfile profile;
        profile.readSettings();

        QCOMPARE(profile.manualDuplexReversesOrder(), false);
        QCOMPARE((int)profile.manualFlipType(), (int)FlipType::LongEdge);

        settings->endGroup();
    }
}
