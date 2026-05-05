// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setMicBoost() (3M-1b Task G.1).
//
// mic_boost bit position: bank 10 (C0=0x12) C2 byte, bit 0 (mask 0x01).
// Semantic: 1 = boost on (no polarity inversion).
//
// Source cite:
//   Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
//     case 10: C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1)
//                   | ... | 0b01000000) & 0x7f;
//     → mic_boost is bit 0 of C2.
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

class TestP1MicBoostWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: mic_boost bit is set ────────────────────────────
    // A freshly constructed P1RadioConnection has m_micBoost = true (Thetis
    // parity), so bank-10 C2 byte bit 0 must be 1.
    // Source: Thetis console.cs:13237 [v2.10.3.13+501e3f51]
    //   private bool mic_boost = true;
    // Wire encoding: networkproto1.c:581 [v2.10.3.13+501e3f51]
    //   C2 = ((prn->mic.mic_boost & 1) | ...) → bit is 1 when mic_boost = 1.
    void defaultState_micBoostBitIsSet() {
        P1RadioConnection conn;
        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // C2 = bank10[2], bit 0 (0x01) must be 1 (Thetis-default boost ON).
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 1);
    }

    // ── 2. setMicBoost(true) → C2 bit 0 set ──────────────────────────────
    // After setMicBoost(true), bank-10 C2 bit 0 must be 1.
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    //   C2 = ((prn->mic.mic_boost & 1) | ...) → bit 0 = 1
    void setMicBoostTrue_c2Bit0IsSet() {
        P1RadioConnection conn;
        conn.setMicBoost(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // Bit 0 (0x01) must be 1 (boost on).
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 1);
    }

    // ── 3. setMicBoost(false) → C2 bit 0 cleared ─────────────────────────
    void setMicBoostFalse_c2Bit0IsCleared() {
        P1RadioConnection conn;
        conn.setMicBoost(true);
        conn.setMicBoost(false);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 0);
    }

    // ── 4. Bit 6 (0x40) is always set in C2 regardless of mic_boost ──────
    // Thetis always sets bit 6 of C2 (the Apollo filter select bit at the
    // low end of the byte, ORed as 0b01000000).
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    //   C2 = (... | 0b01000000) & 0x7f
    void c2Bit6_alwaysSetRegardlessOfBoost() {
        P1RadioConnection conn;

        // Without boost
        conn.setMicBoost(false);
        const QByteArray bank10_off = conn.captureBank10ForTest();
        QCOMPARE(int(quint8(bank10_off[2]) & 0x40), 0x40);

        // With boost
        conn.setMicBoost(true);
        const QByteArray bank10_on = conn.captureBank10ForTest();
        QCOMPARE(int(quint8(bank10_on[2]) & 0x40), 0x40);
    }

    // ── 5. No spillover into other C2 bits (bits 7, 5..2 stay 0) ─────────
    // Only bit 0 (mic_boost) and bit 6 (Apollo default) should be set.
    // Bits 7, 5, 4, 3, 2 and 1 (line_in) are all 0 in default state.
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    void setMicBoostTrue_otherC2BitsUnchanged() {
        P1RadioConnection conn;
        conn.setMicBoost(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        // Bits 7, 5..2, 1 must all be 0.  Only bits 0 and 6 may be set.
        // Mask = 0xFF & ~0x41 = 0xBE → all other bits must be zero.
        QCOMPARE(int(quint8(bank10[2]) & 0xBE), 0);
    }

    // ── 6. Bank 10 C0 address must be 0x12 ───────────────────────────────
    // Bank 10's C0 byte carries address 0x12 (bits 7..1).  After setMicBoost,
    // C2 changes but C0 must still have address 0x12.
    // Source: Thetis networkproto1.c:578 [v2.10.3.13] — C0 |= 0x12
    void setMicBoostTrue_c0AddressIs0x12() {
        P1RadioConnection conn;
        conn.setMicBoost(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        // C0 = bank10[0].  Bits 7..1 = address 0x12; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x12.
        QCOMPARE(int(quint8(bank10[0]) & 0xFE), 0x12);
    }

    // ── 7. Flush flag: setMicBoost sets m_forceBank10Next ────────────────
    // setMicBoost() must set m_forceBank10Next (mirrors setTrxRelay Codex P2
    // pattern: safety effect fires before idempotent guard).
    void setMicBoost_setsForceBank10Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank10NextForTest(), false);

        conn.setMicBoost(true);
        QCOMPARE(conn.forceBank10NextForTest(), true);
    }

    // ── 8. Repeated call with same value still sets the flush flag ────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // must still fire.
    void setMicBoostRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setMicBoost(true);
        conn.setMicBoost(true);
        QCOMPARE(conn.forceBank10NextForTest(), true);
    }

    // ── 9. Idempotent: setMicBoost(true) twice doesn't crash ─────────────
    void setMicBoostTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setMicBoost(true);
        conn.setMicBoost(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 1);
    }

    // ── 10. Round-trip: setMicBoost(true) then setMicBoost(false) ────────
    void roundTrip_trueToFalse_bitClears() {
        P1RadioConnection conn;

        conn.setMicBoost(true);
        QCOMPARE(int(quint8(conn.captureBank10ForTest()[2]) & 0x01), 1);

        conn.setMicBoost(false);
        QCOMPARE(int(quint8(conn.captureBank10ForTest()[2]) & 0x01), 0);
    }

    // ── 11. Codec path (Standard): setMicBoost(true) → C2 bit 0 set ──────
    // setBoardForTest(HermesII) installs P1CodecStandard (m_codec != null).
    // The codec path must read ctx.p1MicBoost and set bit 0 of C2.
    // Source: Thetis networkproto1.c:581 [v2.10.3.13]
    void setMicBoostTrue_codecPath_c2Bit0Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicBoost(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 1);
    }

    // ── 12. Codec path (Standard): setMicBoost(false) → C2 bit 0 clear ───
    void setMicBoostFalse_codecPath_c2Bit0Clear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicBoost(true);
        conn.setMicBoost(false);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 0);
    }

    // ── 13. Codec path (HL2): setMicBoost(true) → C2 bit 0 set ──────────
    // HL2 firmware does not have a mic jack — but P1CodecHl2::bank10 still
    // writes ctx.p1MicBoost for correctness; the HL2 FW ignores the bit.
    void setMicBoostTrue_hl2CodecPath_c2Bit0Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2
        conn.setMicBoost(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 1);
    }

    // ── 14. T/R relay (C3 bit 7) unaffected by mic_boost changes ─────────
    // Setting mic_boost must not disturb C3 (the T/R relay byte).
    // Default: m_trxRelay = false → C3 bit 7 = 1 (relay disengaged).
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
    void trxRelayByteUnaffectedByMicBoost() {
        P1RadioConnection conn;

        conn.setMicBoost(true);
        const QByteArray bank10 = conn.captureBank10ForTest();
        // Relay default (disengaged) → bit 7 = 1.
        QCOMPARE(int(quint8(bank10[3]) & 0x80), 0x80);
        // mic_boost → bit 0 = 1 in C2.
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 1);
    }
};

QTEST_APPLESS_MAIN(TestP1MicBoostWire)
#include "tst_p1_mic_boost_wire.moc"
