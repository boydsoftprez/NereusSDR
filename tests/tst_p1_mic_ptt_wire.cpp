// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setMicPTT() (3M-1b Task G.5).
//
// mic_ptt bit position: bank 11 (C0=0x14) C1 byte, bit 6 (mask 0x40).
// POLARITY INVERSION: parameter enabled = true means "PTT is enabled" (intuitive).
// Wire bit is INVERTED: 0 = PTT-enabled (enabled=true), 1 = PTT-disabled (enabled=false).
//
// Source cite:
//   Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13]
//     case 11: C1 = (prn->rx[0].preamp & 1) | ((prn->rx[1].preamp & 1) << 1) |
//                   ((prn->rx[2].preamp & 1) << 2) | ((prn->rx[0].preamp & 1) << 3) |
//                   ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5) |
//                   ((prn->mic.mic_ptt & 1) << 6);
//     → mic_ptt is bit 6 (mask 0x40) of C1 in bank 11.
//     → mic_ptt = 1 means PTT is DISABLED (inverted from 'enabled').
//
//   deskhpsdr/src/old_protocol.c:3000-3002 [@120188f]:
//     if (mic_ptt_enabled == 0) { output_buffer[C1] |= 0x40; }  — same inversion
//
//   Thetis console.cs:19758-19764 [v2.10.3.13]:
//     MicPTTDisabled property → NetworkIO.SetMicPTT(Convert.ToInt32(value))
//     confirming the wire convention (1 = PTT disabled).
//
// Bank 11 byte layout for C0=0x14:
//   out[0] = 0x14 (C0 — address ORed with MOX bit 0)
//   out[1] = C1: preamp bits 0-3 | mic_trs bit 4 (INVERTED) | mic_bias bit 5
//              | mic_ptt bit 6 (INVERTED)
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

class TestP1MicPttWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: mic_ptt bit is SET (PTT disabled by default) ────────
    // m_micPTT defaults false (PTT not enabled), so wire bit must be 1
    // (mic_ptt = 1 = disabled) because we write !m_micPTT.
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13]
    //   ((prn->mic.mic_ptt & 1) << 6) → bit is 1 when mic_ptt = 1 (PTT disabled).
    void defaultState_micPttBitIsSet() {
        P1RadioConnection conn;
        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // C1 = bank11[1], bit 6 (0x40) must be 1 when m_micPTT=false (default).
        // Because polarity-inverted: !false = 1 on wire.
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 2. setMicPTT(true) [enabled] → C1 bit 6 CLEAR ───────────────────────
    // enabled=true → PTT enabled → wire bit 6 = 0 (PTT-disabled-flag cleared).
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13]
    void setMicPTTTrue_c1Bit6IsClear() {
        P1RadioConnection conn;
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // Bit 6 (0x40) must be 0 (PTT enabled → disable-flag cleared).
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 3. setMicPTT(false) [disabled] → C1 bit 6 SET ────────────────────────
    // enabled=false → PTT disabled → wire bit 6 = 1 (PTT-disabled-flag set).
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13]
    //   ((prn->mic.mic_ptt & 1) << 6) → 0x40 when mic_ptt = 1.
    void setMicPTTFalse_c1Bit6IsSet() {
        P1RadioConnection conn;
        conn.setMicPTT(true);   // first enable...
        conn.setMicPTT(false);  // ...then disable

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // Bit 6 (0x40) must be 1 (PTT disabled → disable-flag set).
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 4. Round-trip: setMicPTT(true) then setMicPTT(false) ─────────────────
    void roundTrip_trueToFalse_bitSetsAgain() {
        P1RadioConnection conn;

        conn.setMicPTT(true);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x40), 0);

        conn.setMicPTT(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x40), 0x40);
    }

    // ── 5. Bank 11 C0 address must be 0x14 ────────────────────────────────────
    // Bank 11's C0 byte carries address 0x14 (bits 7..1). After setMicPTT,
    // C1 changes but C0 must still have address 0x14.
    // Source: Thetis networkproto1.c:594 [v2.10.3.13] — C0 |= 0x14
    void c0AddressIs0x14() {
        P1RadioConnection conn;
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C0 = bank11[0]. Bits 7..1 = address 0x14; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x14.
        QCOMPARE(int(quint8(bank11[0]) & 0xFE), 0x14);
    }

    // ── 6. setMicPTT does NOT clobber C1 bits 0-3 (preamp bits) ──────────────
    // Preamp bits 0-3 must be unaffected when setMicPTT(true) clears bit 6.
    // Source: Thetis networkproto1.c:595-598 [v2.10.3.13]
    void setMicPTTTrue_doesNotClobberPreampBits() {
        P1RadioConnection conn;
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 6 (mic_ptt) must be 0 (cleared). Bits 0-3 (preamp) must be 0.
        // Bit 4 (mic_trs, default inverted): m_micTipRing defaults true → wire bit 0.
        // Bit 5 (mic_bias, default false): wire bit 0.
        // Bit 6: cleared by setMicPTT(true).
        // C1 should be 0x00 in default state (no preamp, no mic_trs/bias flags).
        QCOMPARE(int(quint8(bank11[1])), 0x00);
    }

    // ── 7. setMicPTT(true) does NOT touch C1 bits 4 or 5 ────────────────────
    // Cross-bit guard: mic_ptt (bit 6 = 0x40) must not collide with mic_trs
    // (bit 4 = 0x10, G.3) or mic_bias (bit 5 = 0x20, G.4).
    void setMicPTTTrue_doesNotTouchBit4OrBit5() {
        P1RadioConnection conn;
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bits 4 (mic_trs) and 5 (mic_bias) must be 0 in default state.
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0);
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0);
    }

    // ── 8. Flush flag: setMicPTT sets m_forceBank11Next ──────────────────────
    // setMicPTT() must set m_forceBank11Next (mirrors setMicTipRing / setMicBias
    // Codex P2 pattern: safety effect fires before idempotent guard).
    void setMicPTT_setsForceBank11Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank11NextForTest(), false);

        conn.setMicPTT(true);
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 9. Flush flag set even when value doesn't change (Codex P2) ──────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // (flush flag) must still fire.
    void setMicPTTRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setMicPTT(false);  // no state change (default is false)
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 10. Idempotent: setMicPTT(true) twice → bit stays 0 ─────────────────
    void setMicPTTTrueTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setMicPTT(true);
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 6 should still be 0 (PTT enabled, disable-flag cleared).
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 11. Codec path (Standard): setMicPTT(true) → C1 bit 6 clear ─────────
    // setBoardForTest(HermesII) installs P1CodecStandard (m_codec != null).
    // The codec path must read ctx.p1MicPTT and clear bit 6 of C1.
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13]
    void setMicPTTTrue_codecPath_c1Bit6Clear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 12. Codec path (Standard): setMicPTT(false) → C1 bit 6 set ──────────
    void setMicPTTFalse_codecPath_c1Bit6Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);   // enable first
        conn.setMicPTT(false);  // then disable

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 13. Codec path (HL2): setMicPTT(true) → C1 bit 6 SET (direct polarity)
    // HL2 firmware does not have a mic PTT — but P1CodecHl2::bank11 still
    // writes ctx.p1MicPTT for correctness; the HL2 FW ignores the bit.
    //
    // Polarity is DIRECT on HL2 (matches mi0bot networkproto1.c:1101
    // [v2.10.3.14-beta1]) — fixed by main commit ca8cd73 to stop T/R relay
    // flutter caused by the previous inverted emission making HL2 read
    // mic_ptt=1 by default.  The standard (non-HL2) codec keeps Thetis's
    // inverted polarity per networkproto1.c:597-598 [v2.10.3.13] — verified
    // by setMicPTTFalse_codecPath_c1Bit6Set() above.
    void setMicPTTTrue_hl2CodecPath_c1Bit6Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 14. setMicPTT(true) does NOT touch C1 bits 0-3 — cross-bit guard ─────
    // mic_ptt (bit 6 = 0x40) must not collide with the per-ADC preamp bits (0-3).
    // Source: Thetis networkproto1.c:595-598 [v2.10.3.13]
    void setMicPTTTrue_doesNotTouchPreampBits() {
        P1RadioConnection conn;
        // Only set mic PTT — preamp bits must remain 0.
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 6 (mic_ptt enabled) must be 0.
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
        // Bits 0-3 (preamp) must all be 0 — not set by setMicPTT.
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
    }

    // ── 15. G.3+G.4+G.5 composition: all three C1 bits compose correctly ─────
    // setMicTipRing(false) → bit 4 = 1 (mic_trs, inverted)
    // setMicBias(true)     → bit 5 = 1 (mic_bias, no inversion)
    // setMicPTT(false)     → bit 6 = 1 (mic_ptt, inverted: PTT disabled)
    // C1 & 0x70 must equal 0x70 (bits 4, 5, and 6 all set).
    // Preamp bits (0-3) must remain 0.
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13]
    //   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5)
    //           | ((prn->mic.mic_ptt & 1) << 6);
    void setMicTrsAndBiasAndPTT_composeCorrectlyOnC1() {
        P1RadioConnection conn;
        conn.setMicTipRing(false);  // bit 4 = 1 (tip is BIAS → mic_trs = 1)
        conn.setMicBias(true);      // bit 5 = 1 (bias on)
        conn.setMicPTT(false);      // bit 6 = 1 (PTT disabled → mic_ptt = 1 on wire)

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bits 4+5+6 (0x70) must all be set.
        QCOMPARE(int(quint8(bank11[1]) & 0x70), 0x70);
        // Preamp bits (0-3 = 0x0F) must all be 0 — no preamp set.
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
        // Bit 7 (reserved) must be 0.
        QCOMPARE(int(quint8(bank11[1]) & 0x80), 0);
    }

    // ── 16. G.3+G.4 composition (simpler sub-case): bits 4+5 compose ─────────
    // setMicTipRing(false) + setMicBias(true) → C1 & 0x30 == 0x30.
    // This is the G.3+G.4 composition case from the nit-2 requirement.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    void setMicTrsAndBias_composeCorrectlyOnC1() {
        P1RadioConnection conn;
        conn.setMicTipRing(false);  // bit 4 = 1
        conn.setMicBias(true);      // bit 5 = 1

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bits 4+5 (0x30) must both be set.
        QCOMPARE(int(quint8(bank11[1]) & 0x30), 0x30);
        // Bits 0-3 (preamp) must be 0.
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
    }
};

QTEST_APPLESS_MAIN(TestP1MicPttWire)
#include "tst_p1_mic_ptt_wire.moc"
