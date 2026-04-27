// no-port-check: test fixture asserting setWatchdogEnabled() state-tracking
// on P1RadioConnection and P2RadioConnection. No Thetis logic is ported here;
// this file is NereusSDR-original. The API mirrors NetworkIOImports.cs:197-198
// [v2.10.3.13] (DllImport SetWatchdogTimer) at the concept level only.
//
// Wire-format assertions (RUNSTOP byte pkt[3] bit 7) are in
// tst_p1_watchdog_wire.cpp (3M-1a Task E.5, NEREUS_BUILD_TESTS gated).
//
// Default state changed from false to true in 3M-1a Task E.5 — see
// RadioConnection.h m_watchdogEnabled comment for rationale (HL2 firmware
// dsopenhpsdr1.v:399-400: bit 7 = 0 means watchdog ENABLED; deskhpsdr
// implicitly keeps bit 7 = 0 by never ORing 0x80 into buffer[3]).
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestRadioConnectionWatchdog : public QObject {
    Q_OBJECT
private slots:
    // P1: initial state is true.
    // RadioConnection base class m_watchdogEnabled defaults to true (3M-1a E.5):
    // HL2 firmware bit 7 = 0 means watchdog ENABLED; deskhpsdr never sets bit 7,
    // so watchdog is always on. Default true matches that "on by default" behavior.
    void initial_isTrue_p1();

    // P2: initial state is true.
    void initial_isTrue_p2();

    // P1: setWatchdogEnabled(true) → isWatchdogEnabled() == true.
    void p1_setEnabled_storesTrue();

    // P2: setWatchdogEnabled(true) → isWatchdogEnabled() == true.
    void p2_setEnabled_storesTrue();

    // P1: setWatchdogEnabled(false) then setWatchdogEnabled(true) → true.
    void toggle_returnsToTrue_p1();

    // P2: same round-trip.
    void toggle_returnsToTrue_p2();

    // P1: calling setWatchdogEnabled(true) twice is idempotent —
    // no observable difference in stored state.
    void idempotent_p1_secondCallNoEffect();

    // P2: same idempotency check.
    void idempotent_p2_secondCallNoEffect();
};

// ── P1 ─────────────────────────────────────────────────────────────────────

void TestRadioConnectionWatchdog::initial_isTrue_p1()
{
    P1RadioConnection conn;
    // Default: watchdog is on (3M-1a E.5: m_watchdogEnabled defaults to true).
    // HL2 firmware dsopenhpsdr1.v:399-400 -- bit 7 = 0 means enabled;
    // deskhpsdr never sets bit 7, keeping watchdog always on by default.
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::p1_setEnabled_storesTrue()
{
    P1RadioConnection conn;
    conn.setWatchdogEnabled(false);  // change from default true to false
    conn.setWatchdogEnabled(true);   // back to true
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::toggle_returnsToTrue_p1()
{
    P1RadioConnection conn;
    // Default is true; disable then re-enable.
    conn.setWatchdogEnabled(false);
    QVERIFY(!conn.isWatchdogEnabled());
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::idempotent_p1_secondCallNoEffect()
{
    P1RadioConnection conn;
    // Default is already true; calling true again must not crash or corrupt state.
    conn.setWatchdogEnabled(true);
    conn.setWatchdogEnabled(true);  // second call -- idempotent guard
    QVERIFY(conn.isWatchdogEnabled());
}

// ── P2 ─────────────────────────────────────────────────────────────────────

void TestRadioConnectionWatchdog::initial_isTrue_p2()
{
    P2RadioConnection conn;
    // Same default as P1: m_watchdogEnabled{true} from RadioConnection base.
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::p2_setEnabled_storesTrue()
{
    P2RadioConnection conn;
    conn.setWatchdogEnabled(false);  // change from default true
    conn.setWatchdogEnabled(true);   // back to true
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::toggle_returnsToTrue_p2()
{
    P2RadioConnection conn;
    // Default is true; disable then re-enable.
    conn.setWatchdogEnabled(false);
    QVERIFY(!conn.isWatchdogEnabled());
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::idempotent_p2_secondCallNoEffect()
{
    P2RadioConnection conn;
    // Default is already true; calling true again must not crash or corrupt state.
    conn.setWatchdogEnabled(true);
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
}

QTEST_APPLESS_MAIN(TestRadioConnectionWatchdog)
#include "tst_radio_connection_watchdog.moc"
