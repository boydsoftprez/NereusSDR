// =================================================================
// tests/tst_tgxl_connection_reconnect.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test.  No AetherSDR equivalent (auto-reconnect
// with exponential backoff is a NereusSDR Tier 2 addition per
// design doc §2 and §6.4).
// Backoff sequence: 1/2/5/10/30/60 s, saturates at 60 s.
// =================================================================
// Modification history (NereusSDR):
//   2026-07-27  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 TGXL counterpart to tst_pgxl_connection_reconnect.
//                 Covers the reconnect-cancellation contract that
//                 PgxlConnection and Rf2ksConnection also carry.
// =================================================================

#include <QtTest/QtTest>
#include "core/TgxlConnection.h"
#include "core/AppSettings.h"

class TgxlConnectionReconnectTest : public QObject {
    Q_OBJECT
private slots:
    void backoffSequence();
    void scheduleReconnectArmsTheOwnedTimer();
    void disconnectCancelsPendingReconnect();
};

void TgxlConnectionReconnectTest::backoffSequence() {
    NereusSDR::TgxlConnection conn;
    QSignalSpy spy(&conn, &NereusSDR::TgxlConnection::reconnectAttempt);
    NereusSDR::AppSettings::instance().setValue("TGXL_AutoReconnect", "True");
    // Seed a last host so scheduleReconnect does not bail on an empty
    // one.  TEST-NET-1 (RFC 5737) never routes, so no real connect fires
    // and the singleShot lambdas never execute in this event-loop-less
    // test -- we are only asserting the emitted delay schedule.
    conn.connectToTgxl("192.0.2.1", 9010);

    for (int i = 0; i < 8; ++i) {
        conn.testForceDisconnect();
    }

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

// Same defect as PgxlConnection, and as Rf2ksConnection before it
// (PR #291, fixed there by 7202d393): scheduleReconnect() arms the
// retry with the STATIC QTimer::singleShot(delayMs, this, lambda),
// which creates a detached one-shot with no handle.  m_reconnectTimer
// is declared in the header but never referenced in the .cpp, so
// nothing can cancel a pending retry.
//
// The consequence: the link drops, onDisconnected() schedules a retry
// up to 60 s out, and the operator then hits Disconnect (or turns off
// 4O3A).  disconnect() sets m_userInitiatedDisconnect so no NEW retry
// is scheduled, but the one already in flight still fires and calls
// connectToHost() against the peripheral they just turned off.
void TgxlConnectionReconnectTest::scheduleReconnectArmsTheOwnedTimer() {
    NereusSDR::TgxlConnection conn;
    NereusSDR::AppSettings::instance().setValue("TGXL_AutoReconnect", "True");
    conn.connectToTgxl("192.0.2.1", 9010);

    conn.testForceDisconnect();

    QVERIFY2(conn.testReconnectPending(),
             "scheduleReconnect() did not arm the owned m_reconnectTimer, "
             "so disconnect() cannot cancel the pending retry");
}

void TgxlConnectionReconnectTest::disconnectCancelsPendingReconnect() {
    NereusSDR::TgxlConnection conn;
    NereusSDR::AppSettings::instance().setValue("TGXL_AutoReconnect", "True");
    conn.connectToTgxl("192.0.2.1", 9010);

    conn.testForceDisconnect();
    QVERIFY2(conn.testReconnectPending(),
             "precondition: a retry should be armed before we cancel it");

    conn.disconnect();

    QVERIFY2(!conn.testReconnectPending(),
             "disconnect() left a reconnect armed; it will fire against a "
             "peripheral the operator just disconnected");
}

QTEST_GUILESS_MAIN(TgxlConnectionReconnectTest)
#include "tst_tgxl_connection_reconnect.moc"
