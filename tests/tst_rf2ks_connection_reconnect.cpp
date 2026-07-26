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

QTEST_MAIN(Rf2ksConnectionReconnectTest)
#include "tst_rf2ks_connection_reconnect.moc"
