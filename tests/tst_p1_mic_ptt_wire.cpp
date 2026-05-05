// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setMicPTTDisabled() (3M-1b
// Task G.5; renamed from setMicPTT for issue #182 to match Thetis storage
// name MicPTTDisabled / mic_ptt_disabled).
//
// mic_ptt bit position: bank 11 (C0=0x14) C1 byte, bit 6 (mask 0x40).
// Direct polarity (no inversion) — wire bit mirrors the API argument:
//   setMicPTTDisabled(false) → wire bit 6 CLEAR (PTT enabled at firmware)
//   setMicPTTDisabled(true)  → wire bit 6 SET   (PTT disabled at firmware)
//
// Source cite:
//   Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51]
//     private bool mic_ptt_disabled = false;        // default PTT enabled
//     public bool MicPTTDisabled {
//         set {
//             mic_ptt_disabled = value;
//             NetworkIO.SetMicPTT(Convert.ToInt32(value));
//         }
//     }
//   Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13+501e3f51]
//     case 11: C1 = ... | ((prn->mic.mic_ptt & 1) << 6);
//     → mic_ptt is bit 6 (mask 0x40) of C1 in bank 11, direct polarity.
//
// Bank 11 byte layout for C0=0x14:
//   out[0] = 0x14 (C0 — address ORed with MOX bit 0)
//   out[1] = C1: preamp bits 0-3 | mic_trs bit 4 (INVERTED) | mic_bias bit 5
//              | mic_ptt bit 6 (direct, "disabled" semantic)
//   out[2] = line_in_gain + puresignal
//   out[3] = user digital outputs
//   out[4] = ADC0 RX step ATT (5-bit + 0x20 enable)
//
// Test seam: every test calls setBoardForTest(HermesII) (or HermesLite for
// HL2-specific cases) so composeCcForBank routes through the production
// codec path (P1CodecStandard or P1CodecHl2) — not the legacy compose path.
// The legacy path (env-gated by NEREUS_USE_LEGACY_P1_CODEC=1) is exercised
// separately in the regression-freeze suite.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1MicPttWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: mic_ptt bit is CLEAR (PTT enabled at firmware) ─────
    // m_micPTTDisabled defaults false (matches Thetis console.cs:19757) →
    // ctx.p1MicPTTDisabled=false → wire bit 6 = 0 (firmware honors mic-jack PTT).
    void defaultState_micPttBitIsClear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 2. setMicPTTDisabled(true) → C1 bit 6 SET (PTT disabled at firmware) ─
    // disabled=true → wire bit 6 = 1 (firmware ignores mic-jack PTT line).
    // Source: Thetis console.cs:19764 [v2.10.3.13+501e3f51]
    //   NetworkIO.SetMicPTT(Convert.ToInt32(value));   // value=true → 1
    void setMicPTTDisabledTrue_c1Bit6IsSet() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 3. setMicPTTDisabled(false) → C1 bit 6 CLEAR (PTT enabled) ──────────
    // disabled=false → wire bit 6 = 0 (firmware honors mic-jack PTT line).
    void setMicPTTDisabledFalse_c1Bit6IsClear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);   // first disable...
        conn.setMicPTTDisabled(false);  // ...then re-enable

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 4. Round-trip: disable then re-enable → bit clears ──────────────────
    void roundTrip_disableThenEnable_bitClears() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard

        conn.setMicPTTDisabled(true);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x40), 0x40);

        conn.setMicPTTDisabled(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x40), 0);
    }

    // ── 5. Bank 11 C0 address must be 0x14 ────────────────────────────────────
    // Bank 11's C0 byte carries address 0x14 (bits 7..1). After setMicPTTDisabled,
    // C1 changes but C0 must still have address 0x14.
    // Source: Thetis networkproto1.c:594 [v2.10.3.13+501e3f51] — C0 |= 0x14
    void c0AddressIs0x14() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C0 = bank11[0]. Bits 7..1 = address 0x14; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x14.
        QCOMPARE(int(quint8(bank11[0]) & 0xFE), 0x14);
    }

    // ── 6. setMicPTTDisabled does NOT clobber C1 bits 0-3 (preamp bits) ─────
    void setMicPTTDisabledTrue_doesNotClobberPreampBits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 6 (mic_ptt) must be 1. Bits 0-3 (preamp) must be 0.
        // Bit 4 (mic_trs, default inverted): m_micTipRing defaults true → wire bit 0.
        // Bit 5 (mic_bias, default false): wire bit 0.
        // C1 should be 0x40 in default state with PTT disabled at firmware.
        QCOMPARE(int(quint8(bank11[1])), 0x40);
    }

    // ── 7. setMicPTTDisabled(true) does NOT touch C1 bits 4 or 5 ────────────
    // Cross-bit guard: mic_ptt (bit 6 = 0x40) must not collide with mic_trs
    // (bit 4 = 0x10, G.3) or mic_bias (bit 5 = 0x20, G.4).
    void setMicPTTDisabledTrue_doesNotTouchBit4OrBit5() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bits 4 (mic_trs) and 5 (mic_bias) must be 0 in default state.
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0);
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0);
    }

    // ── 8. Flush flag: setMicPTTDisabled sets m_forceBank11Next ─────────────
    // Mirrors setMicTipRing / setMicBias Codex P2 pattern: safety effect fires
    // before idempotent guard.
    void setMicPTTDisabled_setsForceBank11Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank11NextForTest(), false);

        conn.setMicPTTDisabled(true);
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 9. Flush flag set even when value doesn't change (Codex P2) ──────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // (flush flag) must still fire.
    void setMicPTTDisabledRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setMicPTTDisabled(false);  // no state change (default is false)
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 10. Idempotent: setMicPTTDisabled(true) twice → bit stays 1 ─────────
    void setMicPTTDisabledTrueTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);
        conn.setMicPTTDisabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 11. Codec path (Standard): setMicPTTDisabled(true) → C1 bit 6 SET ───
    // setBoardForTest(HermesII) installs P1CodecStandard (m_codec != null).
    // The codec path must read ctx.p1MicPTTDisabled and set bit 6 (direct).
    void setMicPTTDisabledTrue_codecPath_c1Bit6Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 12. Codec path (Standard): setMicPTTDisabled(false) → C1 bit 6 CLEAR ─
    void setMicPTTDisabledFalse_codecPath_c1Bit6Clear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);   // disable first
        conn.setMicPTTDisabled(false);  // then re-enable

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0);
    }

    // ── 13. Codec path (HL2): setMicPTTDisabled(true) → C1 bit 6 SET ────────
    // HL2 firmware does not have a mic PTT — but P1CodecHl2::bank11 still
    // writes ctx.p1MicPTTDisabled (direct, since PR #161 / commit ca8cd73);
    // HL2 FW ignores the bit.
    // Source: mi0bot networkproto1.c:1101 [v2.10.3.14-beta1 @c26a8a4]
    //   C1 = ... | ((mic.mic_ptt & 1) << 6);
    void setMicPTTDisabledTrue_hl2CodecPath_c1Bit6Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2
        conn.setMicPTTDisabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
    }

    // ── 14. setMicPTTDisabled(true) does NOT touch C1 bits 0-3 ──────────────
    // Cross-bit guard.
    void setMicPTTDisabledTrue_doesNotTouchPreampBits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicPTTDisabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[1]) & 0x40), 0x40);
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
    }

    // ── 15. G.3+G.4+G.5 composition: all three C1 bits compose correctly ─────
    // setMicTipRing(false) → bit 4 = 1 (mic_trs, inverted)
    // setMicBias(true)     → bit 5 = 1 (mic_bias, no inversion)
    // setMicPTTDisabled(false) → bit 6 = 0 (mic_ptt, direct: PTT enabled → 0)
    // C1 & 0x70 must equal 0x30 (bits 4 + 5 set, bit 6 clear).
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13+501e3f51]
    void setMicTrsAndBiasAndPTT_composeCorrectlyOnC1() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicTipRing(false);          // bit 4 = 1 (tip is BIAS → mic_trs = 1)
        conn.setMicBias(true);              // bit 5 = 1 (bias on)
        conn.setMicPTTDisabled(false);      // bit 6 = 0 (PTT enabled → direct)

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[1]) & 0x70), 0x30);
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
        QCOMPARE(int(quint8(bank11[1]) & 0x80), 0);
    }

    // ── 16. G.3+G.4 composition (simpler sub-case): bits 4+5 compose ─────────
    void setMicTrsAndBias_composeCorrectlyOnC1() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicTipRing(false);  // bit 4 = 1
        conn.setMicBias(true);      // bit 5 = 1

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[1]) & 0x30), 0x30);
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
    }
};

QTEST_APPLESS_MAIN(TestP1MicPttWire)
#include "tst_p1_mic_ptt_wire.moc"
