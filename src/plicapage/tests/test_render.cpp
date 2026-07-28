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
#include <QElapsedTimer>
#include <QFile>
#include <QImage>

#include "../render.h"
#include "../popplergate.h"


/************************************************
 * Deliberately high. The document under test is small, and at preview
 * resolution a page renders too quickly for a reload to land on top of a
 * worker that is still inside poppler - which is precisely the collision these
 * tests exist to provoke. Raising the resolution stretches each render out
 * until the window is wide enough to hit reliably.
 ************************************************/
#define STRESS_RESOLUTION 400

/************************************************
 * Both preview widgets run eight, and the count matters: the old
 * setFileName() destroyed each worker in turn, so a worker further down the
 * list was still rendering when an earlier one freed its document.
 ************************************************/
#define STRESS_THREADS 8

/************************************************
 * How long to let the workers get into poppler before reloading on top of
 * them. Long enough that renders are genuinely in flight, short enough that
 * they have not all finished.
 ************************************************/
#define STRESS_SETTLE_MS 60


/************************************************
 * The suite has to stay quick, but "it did not crash" from a handful of
 * rounds is worth very little against a fault that used to appear about one
 * launch in five. The default is enough to have failed consistently before the
 * fix; PLICAPAGE_STRESS_ITERATIONS raises it for a long soak.
 ************************************************/
static int stressIterations()
{
    bool ok = false;
    const int n = qgetenv("PLICAPAGE_STRESS_ITERATIONS").toInt(&ok);
    return (ok && n > 0) ? n : 25;
}


/************************************************

 ************************************************/
static QString stressFile()
{
    // Overridable so the same stress can be pointed at a large scanned
    // document, which exercises far more of poppler than the small synthetic
    // one committed here.
    const QByteArray override = qgetenv("PLICAPAGE_STRESS_PDF");
    if (!override.isEmpty())
        return QString::fromLocal8Bit(override);

    return QString(TEST_DATA_DIR) + "testInFiles/01-16pages.pdf";
}


/************************************************
 * Reloading a document while its own workers are still rendering.
 *
 * This is what happens every time the project changes: tmpFileRenamed reaches
 * Render::setFileName() while the preview is still catching up on the sheets
 * it asked for a moment ago, so a pool of workers is torn down and rebuilt
 * around renders that are already running.
 *
 * Before the fix this aborted 3 runs in 10; it is not a test that has never
 * failed. What it caught is not quite what it was written for - see
 * test_RenderNoReloadStress for the cause it turned out to be - but it covers
 * the lifecycle handling in setFileName(), which was independently wrong.
 ************************************************/
void TestPlicaPage::test_RenderReloadRace()
{
    const QString fileName = stressFile();
    QVERIFY2(QFile::exists(fileName), qPrintable(fileName));

    Render render(STRESS_RESOLUTION, STRESS_THREADS);

    int ready = 0;
    int blank = 0;
    QObject::connect(&render, &Render::sheetReady,
                     [&ready, &blank](const QImage &img, int) {
                         ++ready;
                         if (img.isNull())
                             ++blank;
                     });

    const int iterations = stressIterations();
    for (int i = 0; i < iterations; ++i)
    {
        // The reload for round i lands on the renders round i-1 started.
        render.setFileName(fileName);

        for (int sheet = 0; sheet < STRESS_THREADS * 2; ++sheet)
            render.renderSheet(sheet % 16);

        QTest::qWait(STRESS_SETTLE_MS);
    }

    // Let the last round drain, so the run also covers a teardown that is not
    // racing anything and the counts below mean something.
    QTest::qWait(1000);

    // Surviving is most of the point, but a fix that quietly stopped rendering
    // would survive too. Pages must still come out, and come out with pixels.
    QVERIFY2(ready > 0, "no sheets rendered at all");
    QCOMPARE(blank, 0);
}


/************************************************
 * The same collision across two Render instances.
 *
 * PlicaPage runs two - the preview and the page list - each with its own pool
 * of workers and its own copy of the document. A gate owned by one of them
 * would leave this wide open: poppler's global state does not care which
 * instance a thread belongs to, so the gate has to be process-wide.
 ************************************************/
void TestPlicaPage::test_RenderCrossInstanceRace()
{
    const QString fileName = stressFile();
    QVERIFY2(QFile::exists(fileName), qPrintable(fileName));

    Render busy(STRESS_RESOLUTION, STRESS_THREADS);
    Render reloading(STRESS_RESOLUTION, STRESS_THREADS);

    int ready = 0;
    QObject::connect(&busy, &Render::sheetReady,
                     [&ready](const QImage &, int) { ++ready; });

    busy.setFileName(fileName);

    const int iterations = stressIterations();
    for (int i = 0; i < iterations; ++i)
    {
        // Keep one instance rendering continuously...
        for (int sheet = 0; sheet < STRESS_THREADS * 2; ++sheet)
            busy.renderSheet(sheet % 16);

        // ...and give its workers a moment to get inside poppler, or the
        // reload below lands while they are all still idle and collides with
        // nothing.
        QTest::qWait(STRESS_SETTLE_MS / 3);

        // ...while the other loads and destroys documents underneath it.
        reloading.setFileName(fileName);

        QTest::qWait(STRESS_SETTLE_MS);
    }

    QTest::qWait(1000);
    QVERIFY2(ready > 0, "no sheets rendered at all");
}


/************************************************
 * One load, then nothing but concurrent rendering - no reload, no destroy.
 *
 * This is the test that pins the actual bug. Document lifetime was the first
 * suspect and it was the wrong one: with the reloads taken away entirely this
 * still aborted 4 runs in 20, every thread sitting innocently inside
 * render_page and the main thread asleep. What collides is poppler's one-time
 * colour-management setup, reached by whichever renders happen to be first.
 *
 * Keep it. It is the only test here that fails for the real reason rather than
 * for a plausible one.
 ************************************************/
void TestPlicaPage::test_RenderNoReloadStress()
{
    const QString fileName = stressFile();
    QVERIFY2(QFile::exists(fileName), qPrintable(fileName));

    Render render(STRESS_RESOLUTION, STRESS_THREADS);
    render.setFileName(fileName);

    int ready = 0;
    QObject::connect(&render, &Render::sheetReady,
                     [&ready](const QImage &, int) { ++ready; });

    const int iterations = stressIterations();
    for (int i = 0; i < iterations; ++i)
    {
        for (int sheet = 0; sheet < STRESS_THREADS * 2; ++sheet)
            render.renderSheet(sheet % 16);

        QTest::qWait(STRESS_SETTLE_MS);
    }

    QTest::qWait(1000);
    QVERIFY(ready > 0);
}


/************************************************
 * The gate has to let renders overlap. Serialising them would fix the crash
 * and cost most of the preview's speed, so the property is worth pinning down
 * rather than trusting.
 *
 * Eight threads holding the shared lock at once is the thing being asserted;
 * a gate that granted it exclusively could never reach a concurrency above 1.
 ************************************************/
void TestPlicaPage::test_PopplerGateSharedConcurrency()
{
    const QString fileName = stressFile();
    QVERIFY2(QFile::exists(fileName), qPrintable(fileName));

    Render render(STRESS_RESOLUTION, STRESS_THREADS);
    render.setFileName(fileName);

    // Earlier tests in the run have already rendered, so the watermark has to
    // be taken from here rather than from the start of the process.
    PopplerGate::resetPeakConcurrentRenders();

    QElapsedTimer timer;
    timer.start();

    int ready = 0;
    QObject::connect(&render, &Render::sheetReady,
                     [&ready](const QImage &, int) { ++ready; });

    const int jobs = STRESS_THREADS;
    for (int sheet = 0; sheet < jobs; ++sheet)
        render.renderSheet(sheet % 16);

    while (ready < jobs && timer.elapsed() < 60000)
        QTest::qWait(10);

    QCOMPARE(ready, jobs);

    // Watermark is sampled inside the render path, so it reflects threads that
    // were actually in poppler together, not merely dispatched together.
    QVERIFY2(PopplerGate::peakConcurrentRenders() > 1,
             qPrintable(QString("renders never overlapped (peak %1)")
                        .arg(PopplerGate::peakConcurrentRenders())));
}
