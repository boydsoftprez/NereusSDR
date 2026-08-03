// tests/tst_daemon_app.cpp
//
// R1 Task 10 -- DaemonApp connects a headless nereusd process to a radio
// and creates min(cfg.sliceCount, connected-board-maxSlices) slices,
// using RadioModel's own addSlice() rather than the GUI-only
// addSliceOnPan(). Before this class existed, a headless daemon that
// called RadioModel::connectToRadio() got Slice A and nothing else --
// every other slice-creation call site is wired from MainWindow.
//
// This test primes the RadioModel via DaemonApp::primeBoardForTest(),
// NOT a real RadioModel::connectToRadio() round trip (whether against
// real hardware or a P1FakeRadio loopback fake). Found while writing
// this test: RadioModel::connectToRadio() contains a synchronous nested
// QEventLoop that blocks the calling thread until WdspEngine finishes
// generating FFTW wisdom (RadioModel.cpp: "Block here while the wisdom
// worker finishes, pumping the Qt event loop") -- deliberate, existing
// behaviour (it is how the GUI shows a wisdom progress dialog on a cold
// first connect; see CLAUDE.md's "First run generates FFTW wisdom
// (~15 min)"), not something this task introduces. Measured directly
// while writing this test: over 5 minutes on a cold cache, at which
// point QtTest's own per-function watchdog aborted the process rather
// than connectToRadio() ever returning within a usable test budget. No
// test anywhere in this suite calls RadioModel::connectToRadio() for
// exactly this reason (grep tests/*.cpp for ".connectToRadio(" --
// every hit goes through P1RadioConnection/P2RadioConnection directly,
// never through RadioModel). Every RadioModel-level test instead primes
// board state via RadioModel::setBoardForTest() + configureStreamPool(),
// e.g. tst_p1_hl2_rx2_wiring.cpp's second_live_stream_enables_rx2_end_to_end();
// primeBoardForTest() puts DaemonApp through the identical two calls.
//
// The "connects to the radio" half of DaemonApp::start() (production
// discovery + RadioModel::connectToRadio()) is exercised by manual
// verification instead (see task-10-report.md), not by an automated
// test, for the same reason.
//
// Uses QTEST_MAIN (not APPLESS_MAIN): RadioModel's construction touches
// Qt machinery (timers, WdspEngine, AudioEngine) that wants a
// QCoreApplication, matching every other RadioModel-constructing test.

#include <QtTest/QtTest>

#include "core/HpsdrModel.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

class TstDaemonApp : public QObject {
    Q_OBJECT
private slots:
    // The whole point of the task: a headless start must create the
    // configured number of slices, not just Slice A. HermesLite's
    // BoardCapabilities row (BoardCapabilities.cpp kHermesLite) sets
    // maxSlices = 5, so 3 is well within the SKU's real capacity and
    // must come back exactly, not clamped.
    void createsConfiguredSliceCount()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.sliceCount = 3;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        QCOMPARE(app.sliceCount(), 3);

        app.stop();
    }

    // Fix round 1, Finding 1: mintFftEndpoints() populating m_topology
    // is not enough by itself -- it has to reach RadioModel's own live
    // FFTRouter (RadioModel::fftRouter()) via FftTopology::applyTo(),
    // the same way MainWindow::rebuildFftRouting() ends with
    // m_topology.applyTo(*router). Before the fix, m_topology was
    // subscribed to but never pushed anywhere, so the router never knew
    // about any of it.
    //
    // A fresh DaemonApp's endpoint-id counter starts at 0, so with
    // sliceCount = 1 the single slice created gets "daemon-ep-0" and
    // binds to stream 0 (the allocator's first placement for a slice
    // with no existing occupant to share with).
    void fftRouterReflectsSubscriptions()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.sliceCount = 1;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        QCOMPARE(app.sliceCount(), 1);

        const QList<int> mapped =
            app.fftRouterMappingsForTest(QStringLiteral("daemon-ep-0"));
        QCOMPARE(mapped.size(), 1);
        QCOMPARE(mapped.first(), 0);

        app.stop();

        // The RadioModel (and the FFTRouter it owned) is gone after
        // stop(); the observable contract is that DaemonApp reports no
        // mapping for anything, rather than a test reaching into a
        // dangling router pointer.
        QVERIFY(app.fftRouterMappingsForTest(QStringLiteral("daemon-ep-0")).isEmpty());
    }

    // HermesLite's maxSlices is 5 -- the request must clamp DOWN to the
    // board's real capability, not silently create 99 slices.
    void clampsSliceCountToBoardCapability()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.sliceCount = 99;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        QVERIFY(app.sliceCount() >= 1);
        QVERIFY(app.sliceCount() <= 5);   // no supported SKU exceeds 5

        app.stop();
    }

    void stopIsSafeWithoutStart()
    {
        DaemonApp app;
        app.stop();          // must not crash
        QCOMPARE(app.sliceCount(), 0);
    }

    void restartIsClean()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.sliceCount = 2;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        app.stop();
        // Carry-forward from the coordinator's dispatch: stop() must
        // leave sliceCount() == 0 BEFORE the next start(), not just
        // "eventually" after it. Checked explicitly rather than only
        // inferred from the post-restart count below.
        QCOMPARE(app.sliceCount(), 0);

        QVERIFY(app.start(cfg));
        QCOMPARE(app.sliceCount(), 2);   // not 4

        app.stop();
    }
};

QTEST_MAIN(TstDaemonApp)
#include "tst_daemon_app.moc"
