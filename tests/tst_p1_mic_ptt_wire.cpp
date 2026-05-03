// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setMicPTT() (3M-1b Task G.5,
// updated in P1 full-parity Task 1.1 to direct polarity).
//
// mic_ptt bit position: bank 11 (C0=0x14) C1 byte, bit 6 (mask 0x40).
// DIRECT polarity (no inversion) — wire bit mirrors the API argument:
//   setMicPTT(true)  → wire bit 6 SET   (PTT enabled)
//   setMicPTT(false) → wire bit 6 CLEAR (PTT disabled)
//
// History: pre-P1-full-parity, P1CodecStandard wrote `!ctx.p1MicPTT` to the
// wire (inverted), mirroring the same bug PR #161 (commit ca8cd73) fixed in
// P1CodecHl2 for HL2.  With m_micPTT default false, the inverted code emitted
// bit 6 = 1 every CC frame; Hermes-class firmware reads bit 6 as "track mic-
// jack tip as PTT source", so the floating mic tip caused phantom PTT signals
// fighting software MOX → rapid T/R relay flutter on TUNE/TX (ANAN-10E bench
// symptom).  Standard codec is now direct, matching HL2 codec since PR #161.
//
// Source cite:
//   Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13 @501e3f51]
//     case 11: C1 = (prn->rx[0].preamp & 1) | ((prn->rx[1].preamp & 1) << 1) |
//                   ((prn->rx[2].preamp & 1) << 2) | ((prn->rx[0].preamp & 1) << 3) |
//                   ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5) |
//                   ((prn->mic.mic_ptt & 1) << 6);
//     → mic_ptt is bit 6 (mask 0x40) of C1 in bank 11, DIRECT polarity.
//
// Notes on related upstream APIs (NOT the wire format):
//   deskhpsdr/src/old_protocol.c:3000-3002 [@120188f] uses an inverted
//     `mic_ptt_enabled` higher-level flag, but writes the direct mic_ptt
//     wire field after inversion.
//   Thetis console.cs:19758-19764 [v2.10.3.13] exposes a `MicPTTDisabled`
//     UI property that calls `NetworkIO.SetMicPTT(disabled?1:0)` — same
//     pattern, direct wire field after inversion at the UI layer.
//
// Bank 11 byte layout for C0=0x14:
//   out[0] = 0x14 (C0 — address ORed with MOX bit 0)
//   out[1] = C1: preamp bits 0-3 | mic_trs bit 4 (INVERTED) | mic_bias bit 5
//              | mic_ptt bit 6 (DIRECT)
//   out[2] = line_in_gain + puresignal
//   out[3] = user digital outputs
//   out[4] = ADC0 RX step ATT (5-bit + 0x20 enable)
//
// Test seam: every test calls setBoardForTest(HermesII) (or HermesLite for
// HL2-specific cases) so composeCcForBank routes through the production
// codec path (P1CodecStandard or P1CodecHl2) — not the legacy compose path.
// The legacy path (env-gated by NEREUS_USE_LEGACY_P1_CODEC=1) intentionally
// retains the old inverted polarity and is exercised separately in the
// regression-freeze suite.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1MicPttWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: mic_ptt bit is CLEAR (PTT disabled by default) ─────
    // m_micPTT defaults false → ctx.p1MicPTT=false → wire bit 6 = 0 (direct).
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13 @501e3f51]
    //   ((prn->mic.mic_ptt & 1) << 6) → bit is 0 when mic_ptt = 0 (PTT disabled).
    void defaultState_micPttBitIsClear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // C1 = bank11[1], bit 6 (0x40) must be 0 when m_micPTT=false (default).
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 2. setMicPTT(true) [enabled] → C1 bit 6 SET ──────────────────────────
    // enabled=true → PTT enabled → wire bit 6 = 1 (direct polarity).
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13 @501e3f51]
    void setMicPTTTrue_c1Bit6IsSet() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // Bit 6 (0x40) must be 1 (PTT enabled → direct).
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 3. setMicPTT(false) [disabled] → C1 bit 6 CLEAR ─────────────────────
    // enabled=false → PTT disabled → wire bit 6 = 0 (direct polarity).
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13 @501e3f51]
    void setMicPTTFalse_c1Bit6IsClear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);   // first enable...
        conn.setMicPTT(false);  // ...then disable

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // Bit 6 (0x40) must be 0 (PTT disabled → direct).
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 4. Round-trip: setMicPTT(true) then setMicPTT(false) ─────────────────
    void roundTrip_trueToFalse_bitClears() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard

        conn.setMicPTT(true);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x40), 0x40);

        conn.setMicPTT(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x40), 0);
    }

    // ── 5. Bank 11 C0 address must be 0x14 ────────────────────────────────────
    // Bank 11's C0 byte carries address 0x14 (bits 7..1). After setMicPTT,
    // C1 changes but C0 must still have address 0x14.
    // Source: Thetis networkproto1.c:594 [v2.10.3.13 @501e3f51] — C0 |= 0x14
    void c0AddressIs0x14() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C0 = bank11[0]. Bits 7..1 = address 0x14; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x14.
        QCOMPARE(int(quint8(bank11[0]) & 0xFE), 0x14);
    }

    // ── 6. setMicPTT does NOT clobber C1 bits 0-3 (preamp bits) ──────────────
    // Preamp bits 0-3 must be unaffected when setMicPTT(true) sets bit 6.
    // Source: Thetis networkproto1.c:595-598 [v2.10.3.13 @501e3f51]
    void setMicPTTTrue_doesNotClobberPreampBits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 6 (mic_ptt) must be 1 (set by enabled=true). Bits 0-3 (preamp) must be 0.
        // Bit 4 (mic_trs, default inverted): m_micTipRing defaults true → wire bit 0.
        // Bit 5 (mic_bias, default false): wire bit 0.
        // Bit 6: set by setMicPTT(true).
        // C1 should be 0x40 in default state with PTT enabled.
        QCOMPARE(int(quint8(bank11[1])), 0x40);
    }

    // ── 7. setMicPTT(true) does NOT touch C1 bits 4 or 5 ────────────────────
    // Cross-bit guard: mic_ptt (bit 6 = 0x40) must not collide with mic_trs
    // (bit 4 = 0x10, G.3) or mic_bias (bit 5 = 0x20, G.4).
    void setMicPTTTrue_doesNotTouchBit4OrBit5() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
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

    // ── 10. Idempotent: setMicPTT(true) twice → bit stays 1 ──────────────────
    void setMicPTTTrueTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 6 should still be 1 (PTT enabled, direct).
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 11. Codec path (Standard): setMicPTT(true) → C1 bit 6 SET ───────────
    // setBoardForTest(HermesII) installs P1CodecStandard (m_codec != null).
    // The codec path must read ctx.p1MicPTT and set bit 6 of C1 (direct).
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13 @501e3f51]
    void setMicPTTTrue_codecPath_c1Bit6Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 12. Codec path (Standard): setMicPTT(false) → C1 bit 6 CLEAR ────────
    void setMicPTTFalse_codecPath_c1Bit6Clear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTT(true);   // enable first
        conn.setMicPTT(false);  // then disable

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 13. Codec path (HL2): setMicPTT(true) → C1 bit 6 SET ────────────────
    // HL2 firmware does not have a mic PTT — but P1CodecHl2::bank11 still
    // writes ctx.p1MicPTT (direct, since PR #161 / commit ca8cd73); HL2 FW
    // ignores the bit.  Source: mi0bot networkproto1.c:1101 [v2.10.3.14-beta1]
    //   C1 = ... | ((mic.mic_ptt & 1) << 6);
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
    // Source: Thetis networkproto1.c:595-598 [v2.10.3.13 @501e3f51]
    void setMicPTTTrue_doesNotTouchPreampBits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        // Only set mic PTT — preamp bits must remain 0.
        conn.setMicPTT(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 6 (mic_ptt enabled) must be 1.
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
        // Bits 0-3 (preamp) must all be 0 — not set by setMicPTT.
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
    }

    // ── 15. G.3+G.4+G.5 composition: all three C1 bits compose correctly ─────
    // setMicTipRing(false) → bit 4 = 1 (mic_trs, inverted)
    // setMicBias(true)     → bit 5 = 1 (mic_bias, no inversion)
    // setMicPTT(false)     → bit 6 = 0 (mic_ptt, direct: PTT disabled → 0)
    // C1 & 0x70 must equal 0x30 (bits 4 + 5 set, bit 6 clear).
    // Preamp bits (0-3) must remain 0.
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13 @501e3f51]
    //   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5)
    //           | ((prn->mic.mic_ptt & 1) << 6);
    void setMicTrsAndBiasAndPTT_composeCorrectlyOnC1() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicTipRing(false);  // bit 4 = 1 (tip is BIAS → mic_trs = 1)
        conn.setMicBias(true);      // bit 5 = 1 (bias on)
        conn.setMicPTT(false);      // bit 6 = 0 (PTT disabled → direct)

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bits 4+5 set, bit 6 clear → 0x30 within mask 0x70.
        QCOMPARE(int(quint8(bank11[1]) & 0x70), 0x30);
        // Preamp bits (0-3 = 0x0F) must all be 0 — no preamp set.
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
        // Bit 7 (reserved) must be 0.
        QCOMPARE(int(quint8(bank11[1]) & 0x80), 0);
    }

    // ── 16. G.3+G.4 composition (simpler sub-case): bits 4+5 compose ─────────
    // setMicTipRing(false) + setMicBias(true) → C1 & 0x30 == 0x30.
    // This is the G.3+G.4 composition case from the nit-2 requirement.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13 @501e3f51]
    void setMicTrsAndBias_composeCorrectlyOnC1() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
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
