// tests/tst_cross_thread_teardown.cpp
//
// Regression test for the Windows shutdown/disconnect crash where
// RadioModel destroys a RadioConnection (and its worker-thread-affined
// QTimers) on the main thread after the worker QThread has already
// joined. Qt's "Timers cannot be stopped from another thread" check
// fires on destruction because the child timers' thread affinity is
// still the (exited) worker thread. On Windows this has terminated
// the process abnormally after disconnect-button clicks and after
// close-window shutdowns.
//
// The test reproduces the exact teardown pattern with P1RadioConnection:
// move the connection onto a worker thread, let init() create the
// timers on the worker, queue a disconnect() back on the worker, quit
// and join the thread, then destroy the connection on the main (test)
// thread. A Qt message handler counts any warning that contains the
// cross-thread timer string; the test passes only when zero such
// warnings are emitted.

#include <QtTest/QtTest>
#include <QThread>
#include <atomic>

#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioConnectionTeardown.h"
#include "core/HpsdrModel.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

namespace {

static std::atomic<int> g_crossThreadTimerWarnings{0};
static QtMessageHandler g_previousHandler = nullptr;

static void captureHandler(QtMsgType type,
                           const QMessageLogContext& ctx,
                           const QString& msg)
{
    if (msg.contains(QStringLiteral("Timers cannot be stopped from another thread"))) {
        g_crossThreadTimerWarnings.fetch_add(1);
    }
    if (g_previousHandler) {
        g_previousHandler(type, ctx, msg);
    }
}

} // namespace

class TestCrossThreadTeardown : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        g_previousHandler = qInstallMessageHandler(captureHandler);
    }

    void cleanupTestCase() {
        qInstallMessageHandler(g_previousHandler);
    }

    void init() {
        g_crossThreadTimerWarnings.store(0);
    }

    void workerThreadedConnectionTeardownEmitsNoCrossThreadWarning() {
        // Loopback fake radio on an ephemeral port. Runs on the test
        // (main) thread — same as every other P1 test.
        P1FakeRadio fake;
        fake.start();

        // Production setup: a worker QThread, a RadioConnection moved
        // onto it, init() running on the worker so its timers are
        // affined to the worker thread.
        auto* worker = new QThread();
        worker->setObjectName(QStringLiteral("TestConnWorker"));

        auto* conn = new P1RadioConnection();
        conn->moveToThread(worker);
        connect(worker, &QThread::started,
                conn,   &P1RadioConnection::init);
        worker->start();

        // Connect to the fake radio via a queued call on the worker.
        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.firmwareVersion = 72;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");

        QMetaObject::invokeMethod(conn, [conn, info]() {
            conn->connectToRadio(info);
        });

        // Wait until the state machine reports Connected. At that point
        // the watchdog timer is running on the worker thread.
        QTRY_VERIFY_WITH_TIMEOUT(
            conn->state() == ConnectionState::Connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        // Teardown via the shared helper that production code uses.
        // This is deliberate — the test exercises the same code path
        // that RadioModel::teardownConnection does, so any regression
        // in the helper fails this test.
        RadioConnection* connBase = conn;
        QThread*         workerP  = worker;
        teardownWorkerThreadedConnection(connBase, workerP);
        QVERIFY(connBase == nullptr);
        QVERIFY(workerP == nullptr);

        fake.stop();

        // The assertion that drives the fix: no cross-thread timer
        // warnings may be emitted at any point during the sequence.
        QCOMPARE(g_crossThreadTimerWarnings.load(), 0);
    }
};

// Sandbox is enabled by tests/TestSandboxInit.cpp's global static
// constructor (linked in by nereus_add_test), so we can use QTEST_MAIN
// like the other tests and trust that AppSettings::instance() resolves
// to a sandbox path.
QTEST_MAIN(TestCrossThreadTeardown)
#include "tst_cross_thread_teardown.moc"
