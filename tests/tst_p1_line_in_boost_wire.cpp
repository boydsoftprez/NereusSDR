// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte regression for P1RadioConnection::setLineInBoost(double dB)
// (radio-mic-input Thetis-parity Task 2 follow-up to Task 1 / PR #193).
//
// The base-class default of setLineInBoost(double) maps a continuous dB
// value to the discrete 5-bit `line_in_gain` field via the 32-entry table
// from Thetis console.cs MakeLineInList.  This file pins the dB-to-index
// translation AND the resulting wire byte (bank 11 (C0=0x14) C2 low 5
// bits) without needing a live socket.
//
// Source cite (table — index 0..31, value -34.5..+12.0 in 1.5 dB steps):
//   Thetis Console/console.cs:40827-40859 [v2.10.3.13+501e3f51]
//     for (double i = -34.5; i <= 12; i += 1.5)
//         lineinboost[k++] = i.ToString();
//     ...
//     var lineboost = Array.IndexOf(lineinboost, line_in_boost.ToString());
//     NetworkIO.SetLineBoost(lineboost);
//
// Source cite (wire field — line_in_gain occupies low 5 bits of C2):
//   Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13+501e3f51]
//     case 11: C2 = (prn->mic.line_in_gain & 0b00011111)
//                 | ((prn->puresignal_run & 1) << 6);
//     -> line_in_gain in bits 0..4, puresignal_run independent at bit 6.
//
// Bank 11 byte layout for C0=0x14 (relevant fields only):
//   out[0] = 0x14 (C0 — address ORed with MOX bit 0)
//   out[2] = C2: line_in_gain (bits 0..4) | puresignal_run (bit 6)
//
// Test seam: captureBank11ForTest() calls composeCcForBankForTest(11, ...)
// which routes through composeCcForBank().  The codec path (when
// setBoardForTest() is called) reads ctx.p1LineInGain and emits the low 5
// bits.  The legacy compose path emits 0 for C2 unconditionally — this is
// pre-existing behaviour predating Task 2.1 / PR #193; tests here therefore
// use setBoardForTest() to exercise the codec path, matching the convention
// already established by tst_p1_line_in_gain_setter.cpp.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1LineInBoostWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: line_in_gain bits are clear ────────────────────────
    // m_lineInGain defaults to 0 (RadioConnection.h:659), so bank-11 C2
    // bits 0..4 must be 0 before any setter call.
    // Source: Thetis networkproto1.c:600 [v2.10.3.13+501e3f51]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ... -> low 5 bits = 0
    //   when line_in_gain = 0.
    void defaultState_lineInGainBitsAreClear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // -> P1CodecStandard
        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[2]) & 0x1F), 0);
    }

    // ── 2. Minimum dB maps to index 0 on the wire ────────────────────────────
    // setLineInBoost(-34.5) -> index 0 -> bits 0..4 = 0.
    // Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   lineinboost[0] = "-34.5" -> Array.IndexOf returns 0.
    void minDbMapsToIndexZeroOnTheWire() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setLineInBoost(-34.5);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[2]) & 0x1F), 0);
    }

    // ── 3. Below-range dB clamps to index 0 ──────────────────────────────────
    // RadioConnection::setLineInBoost(dB) clamps dB <= kMinDb to setLineInGain(0).
    // Source: src/core/RadioConnection.h:316-325 (default impl)
    //         Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   Index 0 is the lowest valid value in the 32-entry table.
    void minimumDbBelowRangeClampsToZero() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setLineInBoost(-50.0);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[2]) & 0x1F), 0);
    }

    // ── 4. 0 dB maps to index 23 on the wire ─────────────────────────────────
    // -34.5 + 23 * 1.5 = 0.0, so 0 dB sits exactly at index 23.
    // Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    void zeroDbMapsToIndexTwentyThree() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setLineInBoost(0.0);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[2]) & 0x1F), 23);
    }

    // ── 5. Maximum dB maps to index 31 on the wire ───────────────────────────
    // setLineInBoost(+12.0) -> index 31 -> bits 0..4 = 0b11111.
    // Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   lineinboost[31] = "12" -> Array.IndexOf returns 31.
    void maxDbMapsToIndexThirtyOne() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setLineInBoost(12.0);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[2]) & 0x1F), 31);
    }

    // ── 6. Above-range dB clamps to index 31 ─────────────────────────────────
    // RadioConnection::setLineInBoost(dB) clamps dB >= kMaxDb to setLineInGain(31).
    // Source: src/core/RadioConnection.h:316-325 (default impl)
    //         Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   Index 31 is the highest valid value in the 32-entry table.
    void maximumDbAboveRangeClampsToThirtyOne() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setLineInBoost(20.0);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[2]) & 0x1F), 31);
    }

    // ── 7. PureSignal-run bit (C2 bit 6) is independent of line_in_gain ──────
    // line_in_gain occupies bits 0..4, puresignal_run is at bit 6 (mask 0x40).
    // Toggling puresignal_run must NOT alter the low-5-bit line_in_gain field.
    // Source: Thetis networkproto1.c:600 [v2.10.3.13+501e3f51]
    //   C2 = (line_in_gain & 0b00011111) | ((puresignal_run & 1) << 6);
    void puresignalRunBitUnaffected() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setLineInBoost(12.0);  // index 31 -> bits 0..4 = 0b11111

        // Step 1: puresignal_run cleared. bit 6 = 0, low 5 bits = 31.
        conn.setPuresignalRun(false);
        const QByteArray off = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(off[2]) & 0x40), 0);
        QCOMPARE(int(quint8(off[2]) & 0x1F), 31);

        // Step 2: puresignal_run set. bit 6 = 1, low 5 bits still 31.
        conn.setPuresignalRun(true);
        const QByteArray on = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(on[2]) & 0x40), 0x40);
        QCOMPARE(int(quint8(on[2]) & 0x1F), 31);

        // Step 3: cycle back. bit 6 cleared, low 5 bits still 31.
        conn.setPuresignalRun(false);
        const QByteArray off2 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(off2[2]) & 0x40), 0);
        QCOMPARE(int(quint8(off2[2]) & 0x1F), 31);
    }
};

QTEST_APPLESS_MAIN(TestP1LineInBoostWire)
#include "tst_p1_line_in_boost_wire.moc"
