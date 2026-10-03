// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setLineIn() (3M-1b Task G.2).
//
// line_in bit position: bank 10 (C0=0x12) C2 byte, bit 1 (mask 0x02).
// Semantic: 1 = line in active (no polarity inversion).
//
// Source cite:
//   Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
//     case 10: C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1)
//                   | ... | 0b01000000) & 0x7f;
//     → line_in is bit 1 of C2.
//
// Bank 10 byte layout for C0=0x12:
//   out[0] = 0x12 (C0 — address ORed with MOX bit 0)
//   out[1] = TX drive level (0-255)
//   out[2] = mic flags C2 (mic_boost = bit 0, line_in = bit 1, bit 6 set by default)
//   out[3] = Alex HPF bits | T/R relay disable bit (C3 bit 7, INVERTED)
//   out[4] = Alex LPF bits
//
// Test seam: captureBank10ForTest() calls composeCcForBankForTest(10, ...)
// which routes through composeCcForBank(). Both the codec path (when
// setBoardForTest() is called) and the legacy path are exercised.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1LineInWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: line_in bit is clear ────────────────────────────
    // A freshly constructed P1RadioConnection has m_lineIn = false,
    // so bank-10 C2 byte bit 1 must be 0.
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    //   C2 = (... | ((prn->mic.line_in & 1) << 1) | ...) → bit is 0 when line_in = 0.
    void defaultState_lineInBitIsClear() {
        P1RadioConnection conn;
        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // C2 = bank10[2], bit 1 (0x02) must be 0.
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0);
    }

    // ── 2. setLineIn(true) → C2 bit 1 set ────────────────────────────────
    // After setLineIn(true), bank-10 C2 bit 1 must be 1.
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    //   C2 = (... | ((prn->mic.line_in & 1) << 1) | ...) → bit 1 = 1
    void setLineInTrue_c2Bit1IsSet() {
        P1RadioConnection conn;
        conn.setLineIn(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // Bit 1 (0x02) must be 1 (line in active).
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0x02);
    }

    // ── 3. setLineIn(false) → C2 bit 1 cleared ───────────────────────────
    void setLineInFalse_c2Bit1IsCleared() {
        P1RadioConnection conn;
        conn.setLineIn(true);
        conn.setLineIn(false);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0);
    }

    // ── 4. Bit 6 (0x40) is always set in C2 regardless of line_in ────────
    // Thetis always sets bit 6 of C2 (the Apollo filter select bit at the
    // low end of the byte, ORed as 0b01000000).
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    //   C2 = (... | 0b01000000) & 0x7f
    void c2Bit6_alwaysSetRegardlessOfLineIn() {
        P1RadioConnection conn;

        // Without line in
        conn.setLineIn(false);
        const QByteArray bank10_off = conn.captureBank10ForTest();
        QCOMPARE(int(quint8(bank10_off[2]) & 0x40), 0x40);

        // With line in
        conn.setLineIn(true);
        const QByteArray bank10_on = conn.captureBank10ForTest();
        QCOMPARE(int(quint8(bank10_on[2]) & 0x40), 0x40);
    }

    // ── 5. No spillover into other C2 bits (bits 7, 5..2 stay 0) ─────────
    // Only bit 1 (line_in), bit 6 (Apollo default) and bit 0 (mic_boost,
    // default on as Thetis console.cs:13259 mic_boost = true) may be set.
    // Bits 7, 5, 4, 3 and 2 are all 0 in default state.
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    void setLineInTrue_otherC2BitsUnchanged() {
        P1RadioConnection conn;
        conn.setLineIn(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        // Bits 7, 5..2 must all be 0.  Only bits 0, 1 and 6 may be set.
        // Mask = 0xFF & ~0x43 = 0xBC → all other bits must be zero.
        QCOMPARE(int(quint8(bank10[2]) & 0xBC), 0);
    }

    // ── 6. Bank 10 C0 address must be 0x12 ───────────────────────────────
    // Bank 10's C0 byte carries address 0x12 (bits 7..1).  After setLineIn,
    // C2 changes but C0 must still have address 0x12.
    // Source: Thetis networkproto1.c:578 [v2.10.3.13] — C0 |= 0x12
    void setLineInTrue_c0AddressIs0x12() {
        P1RadioConnection conn;
        conn.setLineIn(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        // C0 = bank10[0].  Bits 7..1 = address 0x12; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x12.
        QCOMPARE(int(quint8(bank10[0]) & 0xFE), 0x12);
    }

    // ── 7. Flush flag: setLineIn sets m_forceBank10Next ──────────────────
    // setLineIn() must set m_forceBank10Next (mirrors setMicBoost Codex P2
    // pattern: safety effect fires before idempotent guard).
    void setLineIn_setsForceBank10Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank10NextForTest(), false);

        conn.setLineIn(true);
        QCOMPARE(conn.forceBank10NextForTest(), true);
    }

    // ── 8. Repeated call with same value still sets the flush flag ────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // must still fire.
    void setLineInRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setLineIn(true);
        conn.setLineIn(true);
        QCOMPARE(conn.forceBank10NextForTest(), true);
    }

    // ── 9. Idempotent: setLineIn(true) twice doesn't crash ───────────────
    void setLineInTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setLineIn(true);
        conn.setLineIn(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0x02);
    }

    // ── 10. Round-trip: setLineIn(true) then setLineIn(false) ────────────
    void roundTrip_trueToFalse_bitClears() {
        P1RadioConnection conn;

        conn.setLineIn(true);
        QCOMPARE(int(quint8(conn.captureBank10ForTest()[2]) & 0x02), 0x02);

        conn.setLineIn(false);
        QCOMPARE(int(quint8(conn.captureBank10ForTest()[2]) & 0x02), 0);
    }

    // ── 11. Codec path (Standard): setLineIn(true) → C2 bit 1 set ────────
    // setBoardForTest(HermesII) installs P1CodecStandard (m_codec != null).
    // The codec path must read ctx.p1LineIn and set bit 1 of C2.
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    void setLineInTrue_codecPath_c2Bit1Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setLineIn(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0x02);
    }

    // ── 12. Codec path (Standard): setLineIn(false) → C2 bit 1 clear ─────
    void setLineInFalse_codecPath_c2Bit1Clear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setLineIn(true);
        conn.setLineIn(false);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0);
    }

    // ── 13. Codec path (HL2): setLineIn(true) → C2 bit 1 set ────────────
    // HL2 firmware does not have a mic jack — but P1CodecHl2::bank10 still
    // writes ctx.p1LineIn for correctness; the HL2 FW ignores the bit.
    void setLineInTrue_hl2CodecPath_c2Bit1Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2
        conn.setLineIn(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0x02);
    }

    // ── 14. setLineIn(true) does NOT touch C2 bit 0 (mic_boost) ──────────
    // Cross-bit guard: line_in (bit 1 = 0x02) must not collide with
    // mic_boost (bit 0 = 0x01). Both bits are independent.
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    //   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ...)
    void setLineInTrue_doesNotTouchMicBoostBit() {
        P1RadioConnection conn;
        // Only set line_in — mic_boost must keep its default.
        conn.setLineIn(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        // Bit 1 (line_in) must be 1.
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0x02);
        // Bit 0 (mic_boost) must keep its default — not changed by setLineIn.
        // mic_boost stays at its default, on (Thetis console.cs:13259 mic_boost = true).
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 0x01);
    }
};

QTEST_APPLESS_MAIN(TestP1LineInWire)
#include "tst_p1_line_in_wire.moc"
