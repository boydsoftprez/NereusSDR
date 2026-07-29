#include <QtTest/QtTest>
#include "core/Rf2ksConnection.h"

using namespace NereusSDR;

class Rf2ksConnectionReconnectTest : public QObject {
    Q_OBJECT
private slots:
    void backoffSequenceFollowsSchedule();
    void successResetsBackoff();
    void disconnectCancelsPendingReconnect();
    void autoReconnectOffSuppressesRetry();
    void failedReconnectProbeKeepsBackingOff();
};

void Rf2ksConnectionReconnectTest::backoffSequenceFollowsSchedule() {
    Rf2ksConnection conn;
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 1000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 2000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 4000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 8000);
    for (int i = 0; i < 5; ++i) { conn.testForceBackoffSequence(); }
    QVERIFY(conn.testCurrentBackoffMs() <= 60000);
}

void Rf2ksConnectionReconnectTest::successResetsBackoff() {
    Rf2ksConnection conn;
    conn.testForceBackoffSequence();
    conn.testForceBackoffSequence();
    conn.testForceBackoffSequence();
    QVERIFY(conn.testCurrentBackoffMs() > 1000);
    conn.testForceBackoffReset();
    QCOMPARE(conn.testCurrentBackoffMs(), 1000);
}

// Review blocker [P1] on PR #291: scheduleReconnect() used
// m_reconnectTimer.singleShot(...), which is the STATIC QTimer::singleShot
// invoked through an instance.  It compiles, but it does not arm
// m_reconnectTimer, so disconnect()'s m_reconnectTimer.stop() could never
// cancel it.  A pending reconnect could therefore fire after the user
// disabled RF-Kit or after RadioModel::teardownPeripherals(), re-issue
// /info and restart polling against a device the operator had turned off.
void Rf2ksConnectionReconnectTest::disconnectCancelsPendingReconnect() {
    Rf2ksConnection conn;

    // Arming a reconnect must arm the owned timer, not a detached one.
    conn.testScheduleReconnect();
    QVERIFY2(conn.testReconnectPending(),
             "scheduleReconnect() did not arm the owned m_reconnectTimer, "
             "so disconnect() cannot cancel it");

    // ...and disconnect() must cancel it.
    conn.disconnect();
    QVERIFY2(!conn.testReconnectPending(),
             "disconnect() left a reconnect pending");
}

// Review blocker [P2] on PR #291: RfKitPage saved an "Auto-reconnect"
// checkbox to AppSettings that nothing ever read back, so scheduleReconnect()
// retried unconditionally whatever the operator chose.
void Rf2ksConnectionReconnectTest::autoReconnectOffSuppressesRetry() {
    Rf2ksConnection conn;
    QVERIFY2(conn.autoReconnect(), "default must stay on (prior behaviour)");

    conn.setAutoReconnect(false);
    conn.testScheduleReconnect();
    QVERIFY2(!conn.testReconnectPending(),
             "auto-reconnect is off but a retry was still armed");

    // And back on again, so the gate is not a one-way latch.
    conn.setAutoReconnect(true);
    conn.testScheduleReconnect();
    QVERIFY(conn.testReconnectPending());
    conn.disconnect();
}

// Codex review [P2] on PR #291: onReconnectTimeout() restarted the poll
// timer as soon as it issued the /info probe, without waiting to see whether
// the probe answered.  m_connected was still false, so every later failure
// hit markPollFailure()'s `>= 3 && m_connected` guard and fell straight
// through: the poller was never stopped again and no further reconnect was
// ever armed.  The backoff froze wherever it had reached while a dead amp
// was hammered at the full poll cadence indefinitely.
//
// Walks the whole path: connected -> 3 failures -> down, then two failed
// probes, each of which must re-arm a retry at a strictly longer delay and
// must NOT resurrect the poller.
void Rf2ksConnectionReconnectTest::failedReconnectProbeKeepsBackingOff() {
    Rf2ksConnection conn;
    conn.testForceBackoffReset();
    conn.testForceConnectedForTesting();

    // Down transition: three consecutive failures while connected.
    conn.testMarkPollFailure();
    conn.testMarkPollFailure();
    conn.testMarkPollFailure();
    QVERIFY2(!conn.isConnected(), "three failures should declare the amp down");
    QVERIFY2(!conn.testPollActive(),
             "poller must stop once the amp is declared down");
    QVERIFY2(conn.testReconnectPending(), "down transition must arm a retry");
    const int afterDown = conn.testCurrentBackoffMs();

    // First reconnect probe fails.  This is the case that used to dead-end.
    conn.testMarkPollFailure();
    QVERIFY2(conn.testReconnectPending(),
             "a failed reconnect probe must arm another retry, not give up");
    const int afterProbe1 = conn.testCurrentBackoffMs();
    QVERIFY2(afterProbe1 > afterDown,
             "backoff must keep growing across failed probes");
    QVERIFY2(!conn.testPollActive(),
             "poller must stay stopped while the amp is still unreachable");

    // Second failed probe: still backing off, still not polling.
    conn.testMarkPollFailure();
    QVERIFY(conn.testReconnectPending());
    QVERIFY2(conn.testCurrentBackoffMs() > afterProbe1,
             "backoff must keep growing across repeated failed probes");
    QVERIFY(!conn.testPollActive());

    conn.disconnect();
}

QTEST_MAIN(Rf2ksConnectionReconnectTest)
#include "tst_rf2ks_connection_reconnect.moc"
