// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setMicBias() (3M-1b Task G.4).
//
// mic_bias bit position: bank 11 (C0=0x14) C1 byte, bit 5 (mask 0x20).
// Polarity: 1 = bias on (no inversion — parameter maps directly to wire bit).
//
// Source cite:
//   Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
//     case 11: C1 = (prn->rx[0].preamp & 1) | ((prn->rx[1].preamp & 1) << 1) |
//                   ((prn->rx[2].preamp & 1) << 2) | ((prn->rx[0].preamp & 1) << 3) |
//                   ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5) |
//                   ((prn->mic.mic_ptt & 1) << 6);
//     → mic_bias is bit 5 (mask 0x20) of C1 in bank 11.
//     → mic_bias = 1 means bias on (no inversion).
//
// Bank 11 byte layout for C0=0x14:
//   out[0] = 0x14 (C0 — address ORed with MOX bit 0)
//   out[1] = C1: preamp bits 0-3 | mic_trs bit 4 (INVERTED) | mic_bias bit 5 | mic_ptt bit 6
//   out[2] = line_in_gain + puresignal
//   out[3] = user digital outputs
//   out[4] = ADC0 RX step ATT (5-bit + 0x20 enable)
//
// Test seam: captureBank11ForTest() calls composeCcForBankForTest(11, ...)
// which routes through composeCcForBank(). Both the codec path (when
// setBoardForTest() is called) and the legacy path are exercised.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1MicBiasWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: mic_bias bit is CLEAR (bias off by default) ──────────
    // m_micBias defaults false (bias off), so C1 bit 5 must be 0.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    //   ((prn->mic.mic_bias & 1) << 5) → bit is 0 when mic_bias = 0.
    void defaultState_micBiasBitIsClear() {
        P1RadioConnection conn;
        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // C1 = bank11[1], bit 5 (0x20) must be 0 when bias off (default).
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0);
    }

    // ── 2. setMicBias(true) [bias on] → C1 bit 5 SET ─────────────────────────
    // on = true → bias on → mic_bias = 1 → wire bit 5 = 1.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    //   ((prn->mic.mic_bias & 1) << 5) → 0x20 when mic_bias = 1.
    void setMicBiasTrue_c1Bit5IsSet() {
        P1RadioConnection conn;
        conn.setMicBias(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0x20);
    }

    // ── 3. setMicBias(false) [bias off] → C1 bit 5 CLEAR ─────────────────────
    // on = false → bias off → mic_bias = 0 → wire bit 5 = 0.
    void setMicBiasFalse_c1Bit5IsClear() {
        P1RadioConnection conn;
        conn.setMicBias(true);   // set first
        conn.setMicBias(false);  // then clear

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0);
    }

    // ── 4. Round-trip: setMicBias(true) then setMicBias(false) ───────────────
    void roundTrip_trueFalse_bitClearsAfterSet() {
        P1RadioConnection conn;

        conn.setMicBias(true);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x20), 0x20);

        conn.setMicBias(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x20), 0);
    }

    // ── 5. Bank 11 C0 address must be 0x14 ───────────────────────────────────
    // Source: Thetis networkproto1.c:594 [v2.10.3.13] — C0 |= 0x14
    void c0AddressIs0x14() {
        P1RadioConnection conn;
        conn.setMicBias(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C0 = bank11[0]. Bits 7..1 = address 0x14; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x14.
        QCOMPARE(int(quint8(bank11[0]) & 0xFE), 0x14);
    }

    // ── 6. setMicBias(true) does NOT clobber C1 bits 0-3 (preamp bits) ───────
    // Preamp bits 0-3 must be unaffected when setMicBias(true) sets bit 5.
    // Source: Thetis networkproto1.c:595-598 [v2.10.3.13]
    void setMicBiasTrue_doesNotClobberPreampBits() {
        P1RadioConnection conn;
        conn.setMicBias(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Only bit 5 (0x20) should be set; bits 0-3 must be 0 (no preamp set).
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
    }

    // ── 7. setMicBias(true) does NOT touch C1 bit 4 (G.3 mic_trs bit) ───────
    // Cross-bit guard: mic_bias (bit 5 = 0x20) must not collide with
    // mic_trs (bit 4 = 0x10, G.3). Both are independent OR'd bits.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    void setMicBiasTrue_doesNotTouchMicTrsBit() {
        P1RadioConnection conn;
        // Only set mic bias — mic_trs (bit 4) must remain 0 (default tipHot=true → bit=0).
        conn.setMicBias(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 5 (mic_bias) must be 1.
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0x20);
        // Bit 4 (mic_trs, G.3) must be 0 (default tipHot=true → inverted wire = 0).
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0);
    }

    // ── 8. setMicBias(true) produces C1 = 0x20 in default state (post issue #182) ──
    // With no preamp set, no mic_trs override: bit 5 (mic_bias) is set.
    // mic_ptt bit 6 stays clear because m_micPTTDisabled defaults false (PTT
    // enabled), matching Thetis console.cs:19757 [v2.10.3.13+501e3f51]:
    //   private bool mic_ptt_disabled = false;
    // Pre-fix the inverted-polarity legacy path emitted bit 6 = 1 by default;
    // the issue #182 follow-up flipped the legacy path to direct, so the bit
    // is now 0 by default.
    // Source: Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13+501e3f51]
    void setMicBiasTrue_c1IsExactly0x20InDefaultState() {
        P1RadioConnection conn;
        conn.setMicBias(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // 0x20 (mic_bias bit 5) | 0x00 (mic_ptt direct, default disabled=false) = 0x20.
        QCOMPARE(int(quint8(bank11[1])), 0x20);
    }

    // ── 9. Flush flag: setMicBias sets m_forceBank11Next (Codex P2) ──────────
    // setMicBias() reuses m_forceBank11Next from G.3 (same bank 11 byte).
    // Flush flag must be set before idempotent guard (Codex P2 pattern).
    void setMicBias_setsForceBank11Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank11NextForTest(), false);

        conn.setMicBias(true);
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 10. Flush flag set even when value doesn't change (Codex P2) ─────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // (flush flag) must still fire.
    void setMicBiasRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setMicBias(false);  // no state change (default is false)
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 11. Idempotent: setMicBias(true) twice doesn't crash ─────────────────
    void setMicBiasTrueTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setMicBias(true);
        conn.setMicBias(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0x20);
    }

    // ── 12. Codec path (Standard): setMicBias(true) → C1 bit 5 set ──────────
    // setBoardForTest(HermesII) installs P1CodecStandard (m_codec != null).
    // The codec path must read ctx.p1MicBias and set bit 5 of C1.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    void setMicBiasTrue_codecPath_c1Bit5Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicBias(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0x20);
    }

    // ── 13. Codec path (Standard): setMicBias(false) → C1 bit 5 clear ────────
    void setMicBiasFalse_codecPath_c1Bit5Clear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicBias(true);
        conn.setMicBias(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0);
    }

    // ── 14. Codec path (HL2): setMicBias(true) → C1 bit 5 set ───────────────
    // HL2 firmware does not have a mic jack — but P1CodecHl2::bank11 still
    // writes ctx.p1MicBias for correctness; the HL2 FW ignores the bit.
    void setMicBiasTrue_hl2CodecPath_c1Bit5Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2
        conn.setMicBias(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0x20);
    }
};

QTEST_APPLESS_MAIN(TestP1MicBiasWire)
#include "tst_p1_mic_bias_wire.moc"
