// =================================================================
// tests/tst_connection_diagnostics.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent; ConnectionDiagnostics
// is a NereusSDR-native class per design doc §4.6.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: tracksFrameCount, countsReconnectsFromSignal,
//                 rebindToTgxlClearsPgxlCounts.
// =================================================================

#include <QtTest/QtTest>
#include "core/ConnectionDiagnostics.h"
#include "core/PgxlConnection.h"
#include "core/TgxlConnection.h"
#include "core/AppSettings.h"

class ConnectionDiagnosticsTest : public QObject {
    Q_OBJECT
private slots:
    void tracksFrameCount();
    void countsReconnectsFromSignal();
    void rebindToTgxlClearsPgxlCounts();
};

// Inject two parsed lines into a PgxlConnection and verify that
// ConnectionDiagnostics::framesIn() returns 2 after the coalesce flush.
// "V3.8.9"       -> emits connected()      -> +1 framesIn
// "R1|0|state=OPERATE" -> emits statusUpdated() -> +1 framesIn
void ConnectionDiagnosticsTest::tracksFrameCount()
{
    NereusSDR::PgxlConnection conn;
    NereusSDR::ConnectionDiagnostics diag;
    diag.bindTo(&conn);

    conn.injectLineForTesting("V3.8.9");
    conn.injectLineForTesting("R1|0|state=OPERATE");

    diag.testFlushCoalesceTimer();

    QCOMPARE(diag.framesIn(), quint64(2));
}

// Verify that reconnectCount increments once per reconnectAttempt signal.
// Uses testForceDisconnect() (already exists on PgxlConnection, Task 59).
void ConnectionDiagnosticsTest::countsReconnectsFromSignal()
{
    NereusSDR::AppSettings::instance().setValue("PGXL_AutoReconnect", "True");

    NereusSDR::PgxlConnection conn;
    NereusSDR::ConnectionDiagnostics diag;
    diag.bindTo(&conn);

    // Seed a non-routable host so scheduleReconnect does not bail on empty host.
    conn.connectToPgxl("192.0.2.1", 9008);

    // Force 3 disconnects; each triggers one reconnectAttempt signal.
    conn.testForceDisconnect();
    conn.testForceDisconnect();
    conn.testForceDisconnect();

    diag.testFlushCoalesceTimer();

    QCOMPARE(diag.reconnectCount(), 3);
}

// Rebinding to a TgxlConnection must disconnect PGXL signals and reset
// accumulated frame counts. After rebind, injecting frames into the old
// PGXL must not alter the diagnostics anymore.
void ConnectionDiagnosticsTest::rebindToTgxlClearsPgxlCounts()
{
    NereusSDR::PgxlConnection pgxl;
    NereusSDR::TgxlConnection tgxl;
    NereusSDR::ConnectionDiagnostics diag;

    // Bind to PGXL and inject two frames.
    diag.bindTo(&pgxl);
    pgxl.injectLineForTesting("V3.8.9");
    pgxl.injectLineForTesting("R1|0|state=OPERATE");
    diag.testFlushCoalesceTimer();
    QCOMPARE(diag.framesIn(), quint64(2));

    // Now rebind to TGXL -- counters must reset to 0.
    diag.bindTo(&tgxl);
    diag.testFlushCoalesceTimer();
    QCOMPARE(diag.framesIn(), quint64(0));

    // Injecting more lines into the old PGXL must not change diag.
    pgxl.injectLineForTesting("S0|state=OPERATE");
    diag.testFlushCoalesceTimer();
    QCOMPARE(diag.framesIn(), quint64(0));
}

QTEST_GUILESS_MAIN(ConnectionDiagnosticsTest)
#include "tst_connection_diagnostics.moc"
