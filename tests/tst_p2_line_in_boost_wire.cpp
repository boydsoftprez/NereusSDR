// no-port-check: test-only — Thetis and deskhpsdr file names appear only in
// source-cite comments that document which upstream line each assertion
// verifies.  No upstream logic is ported here; this file is NereusSDR-original.
//
// Wire-byte regression for P2RadioConnection::setLineInBoost(double dB)
// (radio-mic-input Thetis-parity Task 3, sister of Task 2's P1 coverage).
//
// The base-class default of setLineInBoost(double) (RadioConnection.h:316-325)
// maps a continuous dB value to the discrete 5-bit `line_in_gain` field via
// the 32-entry table from Thetis console.cs MakeLineInList.  P2 emits this
// field at CmdTx buffer byte 51 directly (no formula — the value stored in
// m_mic.lineInGain is the index, not a dB scale).  This file pins the
// dB-to-index translation AND the resulting wire byte without needing a
// live socket.
//
// Source cite (table — index 0..31, value -34.5..+12.0 in 1.5 dB steps):
//   Thetis Console/console.cs:40827-40859 [v2.10.3.13+501e3f51]
//     for (double i = -34.5; i <= 12; i += 1.5)
//         lineinboost[k++] = i.ToString();
//     ...
//     var lineboost = Array.IndexOf(lineinboost, line_in_boost.ToString());
//     NetworkIO.SetLineBoost(lineboost);
//
// Source cite (wire field — byte 51 holds the line_in gain index, 0..31):
//   deskhpsdr/src/new_protocol.c:1502 [@120188f]
//     transmit_specific_buffer[51] = (int)((linein_gain + 34.0) * 0.6739 + 0.5);
//   NereusSDR's P2RadioConnection stores the index directly in
//   m_mic.lineInGain and emits it at byte 51 unmodified
//   (P2RadioConnection.cpp:1960):
//     buf[51] = m_mic.lineInGain;
//
// CmdTx buffer layout (60 bytes, relevant bytes only):
//   byte 50: mic control byte (m_mic.micControl) — bits unrelated to boost
//   byte 51: line_in gain (low 5 bits = index 0..31)
//
// Test seam: composeCmdTxForTest() in P2RadioConnection.h (NEREUS_BUILD_TESTS)
// exposes the CmdTx buffer composition without needing a live socket.
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2LineInBoostWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 51 low 5 bits clear ───────────────────────────
    // m_mic.lineInGain defaults to 0, so CmdTx byte 51 bits 0..4 must be 0
    // before any setter call.
    // Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51] (index 0
    //   is the lowest valid value in the 32-entry table).
    void defaultState_byte51LowFiveBitsClear() {
        P2RadioConnection conn;
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[51] & 0x1F), 0);
    }

    // ── 2. Minimum dB maps to 0 on byte 51 ───────────────────────────────────
    // setLineInBoost(-34.5) -> setLineInGain(0) -> m_mic.lineInGain = 0
    //                       -> buf[51] & 0x1F = 0.
    // Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   lineinboost[0] = "-34.5" -> Array.IndexOf returns 0.
    void minDbMapsToZeroOnByte51() {
        P2RadioConnection conn;
        conn.setLineInBoost(-34.5);

        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[51] & 0x1F), 0);
    }

    // ── 3. Below-range dB clamps to 0 on byte 51 ─────────────────────────────
    // RadioConnection::setLineInBoost(dB) clamps dB <= kMinDb to setLineInGain(0).
    // Source: src/core/RadioConnection.h:316-325 (default impl)
    //         Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   Index 0 is the lowest valid value in the 32-entry table.
    void belowMinClampsToZeroOnByte51() {
        P2RadioConnection conn;
        conn.setLineInBoost(-50.0);

        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[51] & 0x1F), 0);
    }

    // ── 4. 0 dB maps to 23 on byte 51 ────────────────────────────────────────
    // -34.5 + 23 * 1.5 = 0.0, so 0 dB sits exactly at index 23.
    // setLineInBoost(0.0) -> setLineInGain(23) -> m_mic.lineInGain = 23
    //                     -> buf[51] & 0x1F = 23.
    // Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    void zeroDbMapsToTwentyThreeOnByte51() {
        P2RadioConnection conn;
        conn.setLineInBoost(0.0);

        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[51] & 0x1F), 23);
    }

    // ── 5. Maximum dB maps to 31 on byte 51 ──────────────────────────────────
    // setLineInBoost(+12.0) -> setLineInGain(31) -> m_mic.lineInGain = 31
    //                       -> buf[51] & 0x1F = 31 (= 0b11111).
    // Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   lineinboost[31] = "12" -> Array.IndexOf returns 31.
    void maxDbMapsToThirtyOneOnByte51() {
        P2RadioConnection conn;
        conn.setLineInBoost(12.0);

        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[51] & 0x1F), 31);
    }

    // ── 6. Above-range dB clamps to 31 on byte 51 ────────────────────────────
    // RadioConnection::setLineInBoost(dB) clamps dB >= kMaxDb to setLineInGain(31).
    // Source: src/core/RadioConnection.h:316-325 (default impl)
    //         Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   Index 31 is the highest valid value in the 32-entry table.
    void aboveMaxClampsToThirtyOneOnByte51() {
        P2RadioConnection conn;
        conn.setLineInBoost(20.0);

        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[51] & 0x1F), 31);
    }

    // ── 7. Byte 50 (mic control) unaffected by setLineInBoost ────────────────
    // Cross-byte guard: setLineInBoost only writes byte 51 (line_in gain).
    // Byte 50 (m_mic.micControl) holds line_in / mic_boost / mic_ptt_disabled /
    // mic_tip_ring / mic_bias / mic_xlr bits — none of which the line-in-boost
    // path should touch.  Compare byte 50 with a fresh default-state connection.
    // Source: deskhpsdr/src/new_protocol.c:1502 [@120188f] (byte 51 is the
    //   sole target of the line_in gain field; byte 50 holds independent
    //   mic-control bits).
    void byte50Unaffected() {
        P2RadioConnection conn;
        conn.setLineInBoost(12.0);  // index 31 — exercises full byte 51 range.
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);

        P2RadioConnection conn_default;
        quint8 buf_default[60] = {};
        conn_default.composeCmdTxForTest(buf_default);

        QCOMPARE(int(buf[50]), int(buf_default[50]));
    }
};

QTEST_APPLESS_MAIN(TestP2LineInBoostWire)
#include "tst_p2_line_in_boost_wire.moc"
