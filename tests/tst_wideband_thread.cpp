// tests/tst_wideband_thread.cpp
//
// R1 Task 11 -- RadioModel hops the wideband FFT (WidebandFftEngine::
// computeFft, wired inside wireConnectionSignals off P2RadioConnection::
// widebandFrameReady) off the P2 connection thread to stay out of the
// network hot path -- see RadioModel.cpp's own comment beside that
// connect() call. That hop has always implicitly landed on RadioModel's
// own thread (auto-connection with `this` as context): fine for the GUI,
// where RadioModel lives on the main thread and getting off the network
// thread onto the (mostly idle between paint events) main thread is the
// whole point. nereusd has no such spare thread -- its RadioModel lives
// on the daemon's single Qt event-loop thread, which will also carry
// other daemon responsibilities (R1 Task 12+), so parking a 16k-pt FFT
// there instead would defeat the purpose.
//
// This task makes the hop target injectable
// (RadioModel::setWidebandDispatchThread, see its doc comment in
// RadioModel.h) and has DaemonApp supply its own dedicated QThread
// (DaemonApp::widebandThread()), started in start() and quit()+wait()'d
// (joined) in stop() -- see DaemonApp::stop()'s own comment for why that
// join must happen before ~RadioModel() runs (m_widebandDispatchContext,
// the connection's context object, is a RadioModel member).
//
// Board is primed via DaemonApp::primeBoardForTest() rather than a real
// discovery/connectToRadio() round trip, matching every other DaemonApp
// test in tst_daemon_app.cpp (see that file's header comment for why:
// connectToRadio() contains a synchronous nested QEventLoop that blocks
// on cold-cache FFTW wisdom generation, measured over 5 minutes and
// incompatible with an automated test's budget; real network discovery
// alone -- resolveRadioInfo(), which primeBoardForTest bypasses too -- is
// also avoided here so this test's timing does not depend on how many
// NICs the test host has). This test only asserts the wideband thread's
// own start/join lifecycle, not that a real P2 connection ever dispatches
// an actual wideband frame through it -- wideband data flow is explicitly
// out of scope for R1 (design doc section 9.6).
//
// Uses QTEST_MAIN (not APPLESS_MAIN): RadioModel's construction touches
// Qt machinery (timers, WdspEngine, AudioEngine) that wants a
// QCoreApplication, matching every other RadioModel-constructing test.

#include <QtTest/QtTest>
#include <QThread>

#include "core/HpsdrModel.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

class TstWidebandThread : public QObject {
    Q_OBJECT
private slots:
    // The whole point of the task: after start(), the daemon has a
    // running, dedicated thread for the wideband FFT hop -- never the
    // test's own (calling) thread -- and stop() joins it (quit()+wait()),
    // leaving isRunning() false. DaemonApp::stop() deliberately does NOT
    // destroy the QThread (see widebandThread()'s doc comment in
    // DaemonApp.h), so `wb` is still safe to query after stop() below.
    void daemonProvidesADedicatedWidebandThread()
    {
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(DaemonConfig::defaults()));

        QThread* wb = app.widebandThread();
        QVERIFY(wb != nullptr);
        QVERIFY(wb->isRunning());
        QVERIFY(wb != QThread::currentThread());   // never the caller's thread

        app.stop();
        QVERIFY(!wb->isRunning());                 // joined on stop
    }

    // widebandThread() must not report a running (or otherwise
    // surprising) thread before start() has ever been called on this
    // instance, mirroring DaemonApp's other "safe before start()"
    // contracts (see tst_daemon_app.cpp's stopIsSafeWithoutStart()).
    void noThreadBeforeStart()
    {
        DaemonApp app;
        QCOMPARE(app.widebandThread(), nullptr);
        app.stop();   // must not crash
        QCOMPARE(app.widebandThread(), nullptr);
    }

    // A second start()/stop() cycle on the same DaemonApp instance must
    // produce a running thread again, not a stuck-joined one -- Qt
    // supports restarting a QThread that has already finished, and
    // DaemonApp reuses (rather than replaces) its wideband thread across
    // a restart (see widebandThread()'s doc comment).
    void restartGivesARunningThreadAgain()
    {
        DaemonConfig cfg = DaemonConfig::defaults();

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        QVERIFY(app.widebandThread()->isRunning());
        app.stop();
        QVERIFY(!app.widebandThread()->isRunning());

        QVERIFY(app.start(cfg));
        QThread* wb = app.widebandThread();
        QVERIFY(wb != nullptr);
        QVERIFY(wb->isRunning());
        app.stop();
        QVERIFY(!wb->isRunning());
    }
};

QTEST_MAIN(TstWidebandThread)
#include "tst_wideband_thread.moc"
