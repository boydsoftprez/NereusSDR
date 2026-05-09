// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Cross-thread safety tests for StepAttenuatorController → RadioConnection
// pushes (v0.4.1 hotfix).
//
// Background: StepAttenuatorController lives on the main thread, and its
// m_connection is a QPointer<RadioConnection> referencing an object that
// (in production) lives on a worker thread spun up by RadioModel.  Three
// call sites touch m_connection directly without QMetaObject::invokeMethod:
//   - setAttenuation()  → m_connection->setAttenuator(dB)   [line 225]
//   - setPreampMode()   → m_connection->setPreamp(...)      [line 261]
//   - applyAttToHardware() → m_connection->setAttenuator(dB) [line 824]
// These are all `public slots:` on RadioConnection.  Calling a slot
// directly across threads bypasses Qt's queued dispatch and writes
// connection-thread-owned state from the wrong thread.  On x86/x64
// aligned-int writes happen to be atomic so the value lands eventually,
// but on ARM64 the weaker memory model can delay the write or pair it
// with stale reads.  Other StepAttenuatorController call sites (lines
// 364, 469, 535, 557 — all setTxStepAttenuation) already use
// QMetaObject::invokeMethod with a lambda that captures `conn` and the
// new value, so this test pins parity for the three RX-side sites.
//
// Test approach: spin a worker thread, move a MockConnection onto it,
// trigger the controller's setter from the main thread, and verify the
// mock's slot ran on the worker thread (not main).  Direct calls would
// run synchronously on main; QMetaObject::invokeMethod with default
// AutoConnection routes to the worker thread.

#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>
#include <QThread>
#include <QPointer>
#include <QCoreApplication>

#include <atomic>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"

using namespace NereusSDR;

// ── MockConnection ─────────────────────────────────────────────────────────
// Records QThread::currentThread() at the moment each setter slot fires.
// std::atomic<void*> is used because the recorded value is read from the
// main test thread after a thread quit() / wait().
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    std::atomic<QThread*> setAttenuatorRanOnThread{nullptr};
    std::atomic<QThread*> setPreampRanOnThread{nullptr};
    std::atomic<int>       lastAttenuatorDb{-999};

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int dB) override {
        setAttenuatorRanOnThread.store(QThread::currentThread(),
                                       std::memory_order_release);
        lastAttenuatorDb.store(dB, std::memory_order_release);
    }
    void setPreamp(bool) override {
        setPreampRanOnThread.store(QThread::currentThread(),
                                   std::memory_order_release);
    }
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setWatchdogEnabled(bool) override {}
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

// ── Test class ────────────────────────────────────────────────────────────
class TestStepAttCrossThread : public QObject {
    Q_OBJECT

private:
    void clearAppSettings() { AppSettings::instance().clear(); }

    // Drains the Qt event loop on the main test thread so any queued slot
    // invocations targeting the worker get dispatched.  The worker has its
    // own event loop (QThread::exec) that processes its inbound queue
    // automatically; we just need to ensure main has nothing pending.
    static void pumpMain() {
        for (int i = 0; i < 4; ++i) {
            QCoreApplication::processEvents();
        }
    }

    // Wait up to `timeoutMs` for the recorded thread pointer to become
    // non-null (i.e. for the slot to fire on the worker).  Returns the
    // recorded thread, or nullptr on timeout.
    static QThread* waitForSlotFire(std::atomic<QThread*>& recorded,
                                    int timeoutMs = 2000) {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeoutMs) {
            QThread* p = recorded.load(std::memory_order_acquire);
            if (p != nullptr) { return p; }
            pumpMain();
            QThread::msleep(10);
        }
        return recorded.load(std::memory_order_acquire);
    }

private slots:
    void initTestCase() { clearAppSettings(); }
    void init()          { clearAppSettings(); }
    void cleanup()       { clearAppSettings(); }

    // ── §1 setAttenuation marshals to the connection thread ────────────────
    // When MockConnection lives on a worker thread, controller.setAttenuation
    // from main must NOT execute mockConn->setAttenuator() synchronously on
    // main.  The slot must run on the worker thread.
    void setAttenuation_marshalsToConnectionThread() {
        // Spin worker
        QThread worker;
        worker.setObjectName(QStringLiteral("MockConnWorker"));

        // Construct mock, move to worker
        auto* mockConn = new MockConnection();
        mockConn->moveToThread(&worker);
        worker.start();

        // Construct controller on main, wire mockConn
        StepAttenuatorController controller;
        controller.setTickTimerEnabled(false);
        controller.setRadioConnection(mockConn);

        // Pre-condition: no slot fire recorded yet
        const bool preNull = mockConn->setAttenuatorRanOnThread.load(
                                 std::memory_order_acquire) == nullptr;

        // Trigger from main thread
        controller.setAttenuation(5, 0);

        // Wait for the queued slot to fire on the worker
        QThread* ranOn = waitForSlotFire(mockConn->setAttenuatorRanOnThread);
        const int   recordedDb = mockConn->lastAttenuatorDb.load(
                                     std::memory_order_acquire);

        // Tear down worker cleanly BEFORE any assertions so a failure
        // doesn't leave the worker running when QThread's destructor fires.
        worker.quit();
        worker.wait();
        delete mockConn;

        QVERIFY(preNull);
        QVERIFY2(ranOn != nullptr, "setAttenuator slot never fired");
        QVERIFY2(ranOn == &worker,
                 "setAttenuator ran on the wrong thread — direct cross-thread "
                 "method call instead of QMetaObject::invokeMethod queued");
        QCOMPARE(recordedDb, 5);
    }

    // ── §2 setPreampMode marshals to the connection thread ─────────────────
    void setPreampMode_marshalsToConnectionThread() {
        QThread worker;
        worker.setObjectName(QStringLiteral("MockConnWorker"));

        auto* mockConn = new MockConnection();
        mockConn->moveToThread(&worker);
        worker.start();

        StepAttenuatorController controller;
        controller.setTickTimerEnabled(false);
        controller.setRadioConnection(mockConn);

        const bool preNull = mockConn->setPreampRanOnThread.load(
                                 std::memory_order_acquire) == nullptr;

        controller.setPreampMode(PreampMode::On);

        QThread* ranOn = waitForSlotFire(mockConn->setPreampRanOnThread);

        worker.quit();
        worker.wait();
        delete mockConn;

        QVERIFY(preNull);
        QVERIFY2(ranOn != nullptr, "setPreamp slot never fired");
        QVERIFY2(ranOn == &worker,
                 "setPreamp ran on the wrong thread — direct cross-thread "
                 "method call instead of QMetaObject::invokeMethod queued");
    }

    // Note: the third call site (applyAttToHardware in StepAttenuator
    // Controller.cpp:824) is private — it's invoked by the auto-attenuate
    // tick and classic / band-restore paths, not directly from public API.
    // Code review parity covers it: the GREEN fix uses the same
    // QMetaObject::invokeMethod(conn, [conn, dBcopy]() { conn->
    // setAttenuator(dBcopy); }) lambda pattern as setAttenuation above.
    // The other StepAttenuatorController invokeMethod sites (lines 364,
    // 469, 535, 557 — all setTxStepAttenuation paths) already follow this
    // pattern and have been bench-stable since v0.3.x.
};

QTEST_MAIN(TestStepAttCrossThread)
#include "tst_step_att_cross_thread.moc"
