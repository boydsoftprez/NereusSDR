// no-port-check: test fixture asserting sendTxIq() and setTrxRelay()
// interface-level behaviour on P1RadioConnection and P2RadioConnection.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Verifies:
//   - Both subclasses override all RadioConnection pure-virtual TX methods
//     declared in 3M-1a Task E.1 (sendTxIq, setTrxRelay).
//   - sendTxIq() is callable without a crash (stub path; bodies are
//     filled by Tasks E.2 and E.6).
//   - setTrxRelay() stores state in the base-class m_trxRelay field,
//     readable via isTrxRelayEngaged().
//   - setMox / setWatchdogEnabled remain UNTOUCHED (regression guards
//     confirm E.1 did not disturb prior API).
//   - setTxDrive(N) is callable without crash (regression guard;
//     p1_setTxDrive_callableNoCrash + p2_setTxDrive_callableNoCrash).
//
// Cite: Phase 3M-1a plan Task E.1, Master design §5.1.4.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestRadioConnectionTxIface : public QObject {
    Q_OBJECT
private slots:
    // ── sendTxIq ───────────────────────────────────────────────────────

    // P1: sendTxIq is callable with a non-null buffer and n > 0.
    // Stub body does not crash; it logs a warning and drops samples.
    void p1_sendTxIq_callableNocrash();

    // P2: same smoke test.
    void p2_sendTxIq_callableNocrash();

    // P1: sendTxIq with n=0 is safe (boundary case).
    void p1_sendTxIq_zeroSamples();

    // P2: same zero-sample boundary case.
    void p2_sendTxIq_zeroSamples();

    // ── setTrxRelay ────────────────────────────────────────────────────

    // P1: initial T/R relay state is false (relay not engaged).
    void p1_trxRelay_initialIsFalse();

    // P2: initial T/R relay state is false.
    void p2_trxRelay_initialIsFalse();

    // P1: setTrxRelay(true) → isTrxRelayEngaged() == true.
    void p1_setTrxRelay_storesTrue();

    // P2: setTrxRelay(true) → isTrxRelayEngaged() == true.
    void p2_setTrxRelay_storesTrue();

    // P1: setTrxRelay(true) then setTrxRelay(false) → false (round-trip).
    void p1_setTrxRelay_roundTrip();

    // P2: same round-trip.
    void p2_setTrxRelay_roundTrip();

    // P1: calling setTrxRelay(true) twice is idempotent.
    void p1_setTrxRelay_idempotent();

    // P2: same idempotency check.
    void p2_setTrxRelay_idempotent();

    // ── Regression guards (E.1 must not disturb prior API) ─────────────

    // P1: setMox(true) is still callable and stores state (unchanged from
    // pre-E.1 behaviour — m_mox in P1RadioConnection).
    void p1_setMox_unchanged();

    // P2: setMox does not crash after E.1 additions.
    void p2_setMox_unchanged();

    // P1: setWatchdogEnabled(true) still stores state in m_watchdogEnabled
    // (unchanged from Phase 3M-0 Task 5).
    void p1_setWatchdogEnabled_unchanged();

    // P2: same.
    void p2_setWatchdogEnabled_unchanged();

    // P1: setTxDrive(N) is callable without crash (regression guard —
    // E.1 header comment claims this is covered; this is the actual test).
    void p1_setTxDrive_callableNoCrash();

    // P2: same.
    void p2_setTxDrive_callableNoCrash();
};

// ── sendTxIq ──────────────────────────────────────────────────────────────────

void TestRadioConnectionTxIface::p1_sendTxIq_callableNocrash()
{
    P1RadioConnection conn;
    float buf[128]{};
    // Stub logs a warning; the call must not crash or abort.
    conn.sendTxIq(buf, 64);
    QVERIFY(true);  // reaching here means no crash
}

void TestRadioConnectionTxIface::p2_sendTxIq_callableNocrash()
{
    P2RadioConnection conn;
    float buf[128]{};
    conn.sendTxIq(buf, 64);
    QVERIFY(true);
}

void TestRadioConnectionTxIface::p1_sendTxIq_zeroSamples()
{
    P1RadioConnection conn;
    conn.sendTxIq(nullptr, 0);  // n=0: stub must not dereference iq
    QVERIFY(true);
}

void TestRadioConnectionTxIface::p2_sendTxIq_zeroSamples()
{
    P2RadioConnection conn;
    conn.sendTxIq(nullptr, 0);
    QVERIFY(true);
}

// ── setTrxRelay ───────────────────────────────────────────────────────────────

void TestRadioConnectionTxIface::p1_trxRelay_initialIsFalse()
{
    P1RadioConnection conn;
    QVERIFY(!conn.isTrxRelayEngaged());
}

void TestRadioConnectionTxIface::p2_trxRelay_initialIsFalse()
{
    P2RadioConnection conn;
    QVERIFY(!conn.isTrxRelayEngaged());
}

void TestRadioConnectionTxIface::p1_setTrxRelay_storesTrue()
{
    P1RadioConnection conn;
    conn.setTrxRelay(true);
    QVERIFY(conn.isTrxRelayEngaged());
}

void TestRadioConnectionTxIface::p2_setTrxRelay_storesTrue()
{
    P2RadioConnection conn;
    conn.setTrxRelay(true);
    QVERIFY(conn.isTrxRelayEngaged());
}

void TestRadioConnectionTxIface::p1_setTrxRelay_roundTrip()
{
    P1RadioConnection conn;
    conn.setTrxRelay(true);
    QVERIFY(conn.isTrxRelayEngaged());
    conn.setTrxRelay(false);
    QVERIFY(!conn.isTrxRelayEngaged());
}

void TestRadioConnectionTxIface::p2_setTrxRelay_roundTrip()
{
    P2RadioConnection conn;
    conn.setTrxRelay(true);
    QVERIFY(conn.isTrxRelayEngaged());
    conn.setTrxRelay(false);
    QVERIFY(!conn.isTrxRelayEngaged());
}

void TestRadioConnectionTxIface::p1_setTrxRelay_idempotent()
{
    P1RadioConnection conn;
    conn.setTrxRelay(true);
    conn.setTrxRelay(true);  // second call — must not corrupt state
    QVERIFY(conn.isTrxRelayEngaged());
}

void TestRadioConnectionTxIface::p2_setTrxRelay_idempotent()
{
    P2RadioConnection conn;
    conn.setTrxRelay(true);
    conn.setTrxRelay(true);
    QVERIFY(conn.isTrxRelayEngaged());
}

// ── Regression guards ─────────────────────────────────────────────────────────

void TestRadioConnectionTxIface::p1_setMox_unchanged()
{
    P1RadioConnection conn;
    // setMox must still compile and not crash. State is private in P1;
    // we can only confirm the call is safe.
    conn.setMox(true);
    conn.setMox(false);
    QVERIFY(true);
}

void TestRadioConnectionTxIface::p2_setMox_unchanged()
{
    P2RadioConnection conn;
    conn.setMox(true);
    conn.setMox(false);
    QVERIFY(true);
}

void TestRadioConnectionTxIface::p1_setWatchdogEnabled_unchanged()
{
    P1RadioConnection conn;
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionTxIface::p2_setWatchdogEnabled_unchanged()
{
    P2RadioConnection conn;
    conn.setWatchdogEnabled(true);
    QVERIFY(conn.isWatchdogEnabled());
}

void TestRadioConnectionTxIface::p1_setTxDrive_callableNoCrash()
{
    P1RadioConnection conn;
    // setTxDrive is a stub in 3M-1a; must not crash at any legal drive value.
    conn.setTxDrive(0);
    conn.setTxDrive(50);
    conn.setTxDrive(100);
    QVERIFY(true);  // reaching here means no crash
}

void TestRadioConnectionTxIface::p2_setTxDrive_callableNoCrash()
{
    P2RadioConnection conn;
    conn.setTxDrive(0);
    conn.setTxDrive(50);
    conn.setTxDrive(100);
    QVERIFY(true);
}

QTEST_APPLESS_MAIN(TestRadioConnectionTxIface)
#include "tst_radio_connection_tx_iface.moc"
