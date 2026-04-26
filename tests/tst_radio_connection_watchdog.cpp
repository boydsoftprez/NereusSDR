// no-port-check: test fixture asserting setWatchdogEnabled() state-tracking
// on P1RadioConnection and P2RadioConnection. No Thetis logic is ported here;
// this file is NereusSDR-original. The API mirrors NetworkIOImports.cs:197-198
// [v2.10.3.13] (DllImport SetWatchdogTimer) at the concept level only — no
// wire format is asserted (bit position is unknown; see Task 5 stub rationale).
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestRadioConnectionWatchdog : public QObject {
    Q_OBJECT
private slots:
    // P1: initial state is false.
    // Mirrors RadioConnection base class m_watchdogEnabled{false} default.
    void initial_isFalse_p1();

    // P2: initial state is false.
    void initial_isFalse_p2();

    // P1: setWatchdogEnabled(true) → isWatchdogEnabled() == true.
    void p1_setEnabled_storesTrue();

    // P2: setWatchdogEnabled(true) → isWatchdogEnabled() == true.
    void p2_setEnabled_storesTrue();

    // P1: setWatchdogEnabled(true) then setWatchdogEnabled(false) → false.
    void toggle_returnsToFalse_p1();

    // P2: same round-trip.
    void toggle_returnsToFalse_p2();

    // P1: calling setWatchdogEnabled(true) twice is idempotent —
    // no observable difference in stored state.
    void idempotent_p1_secondCallNoEffect();

    // P2: same idempotency check.
    void idempotent_p2_secondCallNoEffect();
};

// ── P1 ─────────────────────────────────────────────────────────────────────

void TestRadioConnectionWatchdog::initial_isFalse_p1()
{
    P1RadioConnection conn;
    // Default: watchdog is off until explicitly enabled.
    QVERIFY(!conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::p1_setEnabled_storesTrue()
{
    P1RadioConnection conn;
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::toggle_returnsToFalse_p1()
{
    P1RadioConnection conn;
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
    conn.setWatchdogEnabled(false);
    QVERIFY(!conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::idempotent_p1_secondCallNoEffect()
{
    P1RadioConnection conn;
    conn.setWatchdogEnabled(true);
    conn.setWatchdogEnabled(true);  // second call — must not crash or corrupt state
    QVERIFY(conn.isWatchdogEnabled());
}

// ── P2 ─────────────────────────────────────────────────────────────────────

void TestRadioConnectionWatchdog::initial_isFalse_p2()
{
    P2RadioConnection conn;
    QVERIFY(!conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::p2_setEnabled_storesTrue()
{
    P2RadioConnection conn;
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::toggle_returnsToFalse_p2()
{
    P2RadioConnection conn;
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
    conn.setWatchdogEnabled(false);
    QVERIFY(!conn.isWatchdogEnabled());
}

void TestRadioConnectionWatchdog::idempotent_p2_secondCallNoEffect()
{
    P2RadioConnection conn;
    conn.setWatchdogEnabled(true);
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
}

QTEST_APPLESS_MAIN(TestRadioConnectionWatchdog)
#include "tst_radio_connection_watchdog.moc"
