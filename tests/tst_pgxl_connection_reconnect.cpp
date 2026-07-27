// =================================================================
// tests/tst_pgxl_connection_reconnect.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (auto-reconnect
// with exponential backoff is a NereusSDR Tier 2 addition per
// design doc §2 and §6.4).
// Backoff sequence: 1/2/5/10/30/60 s, saturates at 60 s.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Test: backoffSequence verifies 8 calls to
//                 testForceDisconnect() produce the expected
//                 1/2/5/10/30/60/60/60 s delay emissions.
// =================================================================

#include <QtTest/QtTest>
#include "core/PgxlConnection.h"
#include "core/AppSettings.h"

class PgxlConnectionReconnectTest : public QObject {
    Q_OBJECT
private slots:
    void backoffSequence();
    void scheduleReconnectArmsTheOwnedTimer();
    void disconnectCancelsPendingReconnect();
};

void PgxlConnectionReconnectTest::backoffSequence() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy spy(&conn, &NereusSDR::PgxlConnection::reconnectAttempt);
    NereusSDR::AppSettings::instance().setValue("PGXL_AutoReconnect", "True");
    // Seed a last host so scheduleReconnect does not bail on empty host.
    // testForceDisconnect() calls scheduleReconnect() directly, which
    // checks m_lastHost. We set it via the public connectToPgxl path;
    // use a non-routable address so no real connect fires.
    // NOTE: connectToPgxl() clears m_reconnectAttempts - set after the
    // call so the counter starts at 0.
    // The testForceDisconnect() call does NOT reset m_reconnectAttempts,
    // so 8 consecutive calls accumulate attempts 0..7 producing the
    // expected delay table.

    // Directly prime the private m_lastHost via connectToPgxl (which
    // will not block since the socket is not event-loop-driven here).
    conn.connectToPgxl("192.0.2.1", 9008);  // non-routable TEST-NET-1 per RFC 5737

    // m_reconnectAttempts was reset to 0 by onConnected in connectToPgxl.
    // onConnected only fires on real TCP connect which doesn't happen here,
    // so m_reconnectAttempts stays at whatever value it was (0 after ctor).
    // The QTimer::singleShot calls in scheduleReconnect() do NOT fire here
    // because the test runs without a running event loop (QTEST_GUILESS_MAIN).
    // We just verify the reconnectAttempt signal emissions (the increments to
    // m_reconnectAttempts inside the singleShot lambdas never execute).
    for (int i = 0; i < 8; ++i) {
        conn.testForceDisconnect();
    }

    // Collect all emitted delays.
    QVector<int> delays;
    while (spy.count()) {
        delays << spy.takeFirst().at(1).toInt();
    }
    QCOMPARE(delays.count(), 8);
    QCOMPARE(delays.value(0), 1000);
    QCOMPARE(delays.value(1), 2000);
    QCOMPARE(delays.value(2), 5000);
    QCOMPARE(delays.value(3), 10000);
    QCOMPARE(delays.value(4), 30000);
    QCOMPARE(delays.value(5), 60000);
    QCOMPARE(delays.value(6), 60000);
    QCOMPARE(delays.value(7), 60000);
}

// The same defect Codex/review found in Rf2ksConnection (PR #291,
// fixed there by 7202d393) is present here and in TgxlConnection.
//
// scheduleReconnect() arms the retry with the STATIC
// QTimer::singleShot(delayMs, this, lambda).  That creates a detached
// one-shot with no handle, so nothing can cancel it -- m_reconnectTimer
// is declared in the header but never referenced in the .cpp at all.
//
// The consequence: the link drops, onDisconnected() schedules a retry
// up to 60 s out, and the operator then hits Disconnect (or turns off
// 4O3A).  disconnect() sets m_userInitiatedDisconnect so no NEW retry
// gets scheduled, but the one already in flight still fires and calls
// connectToHost() against the peripheral they just turned off.
void PgxlConnectionReconnectTest::scheduleReconnectArmsTheOwnedTimer() {
    NereusSDR::PgxlConnection conn;
    NereusSDR::AppSettings::instance().setValue("PGXL_AutoReconnect", "True");
    conn.connectToPgxl("192.0.2.1", 9008);  // TEST-NET-1, never routes

    conn.testForceDisconnect();

    QVERIFY2(conn.testReconnectPending(),
             "scheduleReconnect() did not arm the owned m_reconnectTimer, "
             "so disconnect() cannot cancel the pending retry");
}

void PgxlConnectionReconnectTest::disconnectCancelsPendingReconnect() {
    NereusSDR::PgxlConnection conn;
    NereusSDR::AppSettings::instance().setValue("PGXL_AutoReconnect", "True");
    conn.connectToPgxl("192.0.2.1", 9008);

    conn.testForceDisconnect();
    QVERIFY2(conn.testReconnectPending(),
             "precondition: a retry should be armed before we cancel it");

    conn.disconnect();

    QVERIFY2(!conn.testReconnectPending(),
             "disconnect() left a reconnect armed; it will fire against a "
             "peripheral the operator just disconnected");
}

QTEST_GUILESS_MAIN(PgxlConnectionReconnectTest)
#include "tst_pgxl_connection_reconnect.moc"
