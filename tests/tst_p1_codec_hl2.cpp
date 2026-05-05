// no-port-check: test fixture cites Thetis source for expected values, not a port itself
#include <QtTest/QtTest>
#include <array>
#include "core/codec/P1CodecHl2.h"

using namespace NereusSDR;

class TestP1CodecHl2 : public QObject {
    Q_OBJECT
private slots:
    void maxBank_is_18() {
        P1CodecHl2 codec;
        QCOMPARE(codec.maxBank(), 18);
    }

    void usesI2cIntercept_is_true() {
        P1CodecHl2 codec;
        QVERIFY(codec.usesI2cIntercept());
    }

    // Bank 11 C4 — RX path: 6-bit mask + 0x40 enable WITH (31 - userDb)
    // inversion. HL2 firmware treats higher values as MORE attenuation, so
    // mi0bot inverts at 3 callsites (console.cs:11075/11251/19380); without
    // the inversion, slider value 31 reaches HL2 as zero attenuation.
    // Source: mi0bot networkproto1.c:1102 + console.cs:11075/11251/19380 [@c26a8a4]
    void bank11_rx_att_20dB_hl2_encoding() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = false;
        ctx.rxStepAttn[0] = 20;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[0]), 0x14);
        // userDb=20 → wire = (31-20) | 0x40 = 11 | 0x40 = 0x4B
        QCOMPARE(int(out[4]), ((31 - 20) & 0x3F) | 0x40);
    }

    // Bank 11 C4 — TX path: HL2 emits the (31 - userDb) inversion in the
    // 6-bit field, same as the RX path.  Issue #175 follow-up: pre-fix the
    // codec hardcoded wire = 0x40 (= 0 dB applied), silently discarding
    // m_txStepAttn — so "ATT on TX" with force-31 emitted no actual
    // attenuation on the wire.
    //
    // From mi0bot networkproto1.c:1099-1100 [v2.10.3.13-beta2]:
    //   if (XmitBit) C4 = (prn->adc[0].tx_step_attn & 0b00111111) | 0b01000000;
    // The (31 - userDb) inversion is applied upstream in mi0bot at
    // console.cs:10658 [v2.10.3.13-beta2]; NereusSDR applies it at the codec
    // (matching the RX-path branch below).
    //
    // ATT-on-TX-disabled default: StepAttenuatorController emits
    // setTxStepAttenuation(0) on RX→TX → m_txStepAttn=0 → user 0 dB →
    // wire = (31 - 0) & 0x3F | 0x40 = 0x5F (= "no attenuation" on HL2's
    // inverted-polarity wire field).
    void bank11_tx_att_inverts_userDb_under_mox() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = true;

        // user 0 dB → wire = (31 - 0) & 0x3F | 0x40 = 0x5F (no ATT)
        ctx.txStepAttn[0] = 0;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[0]), 0x15);  // C0 = 0x14 | mox
        QCOMPARE(int(out[4]), 0x5F);

        // user 15 dB → wire = (31 - 15) & 0x3F | 0x40 = 0x50
        ctx.txStepAttn[0] = 15;
        std::fill_n(out, 5, quint8{0});
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[4]), 0x50);

        // user 31 dB (max ATT — the JJ bench scenario) → wire = (31 - 31) & 0x3F | 0x40 = 0x40
        ctx.txStepAttn[0] = 31;
        std::fill_n(out, 5, quint8{0});
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[4]), 0x40);

        // user -28 dB (HL2 lower corner) → wire = (31 - (-28)) & 0x3F | 0x40 = 0x7B
        ctx.txStepAttn[0] = -28;
        std::fill_n(out, 5, quint8{0});
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[4]), 0x7B);
    }

    // Bank 11 C4 — RX path range. HL2 user-facing slider is signed
    // −28..+31 dB (issue #175 follow-up — capped at +31 per maintainer
    // approval; mi0bot widens to +32 at console.cs:11043 [v2.10.3.13-beta2]
    // but that is an off-by-one upstream bug since wire encoding
    // `31 - userDb` produces wire = -1 at userDb=32 which 6-bit-masks to
    // the LNA-gain wraparound region).  Inputs outside the range clamp
    // to the corner; the (31 - userDb) inversion folds the clamped value
    // into the 6-bit wire field.
    void bank11_rx_att_signed_range_corners() {
        P1CodecHl2 codec;

        // Negative corner: userDb=-28 → wire = (31-(-28)) & 0x3F | 0x40
        //                = 59 & 0x3F | 0x40 = 0x3B | 0x40 = 0x7B.
        {
            CodecContext ctx;
            ctx.rxStepAttn[0] = -28;
            quint8 out[5] = {};
            codec.composeCcForBank(11, ctx, out);
            QCOMPARE(int(out[4]), 0x7B);
        }

        // Zero (mi0bot default): userDb=0 → wire = 31 | 0x40 = 0x5F.
        {
            CodecContext ctx;
            ctx.rxStepAttn[0] = 0;
            quint8 out[5] = {};
            codec.composeCcForBank(11, ctx, out);
            QCOMPARE(int(out[4]), 0x5F);
        }

        // Positive corner: userDb=+31 → wire = (31-31) & 0x3F | 0x40
        //                = 0 | 0x40 = 0x40 (max chip ATT).
        {
            CodecContext ctx;
            ctx.rxStepAttn[0] = 31;
            quint8 out[5] = {};
            codec.composeCcForBank(11, ctx, out);
            QCOMPARE(int(out[4]), 0x40);
        }

        // Out-of-range low: userDb=-100 → clamped to -28 → wire 0x7B.
        {
            CodecContext ctx;
            ctx.rxStepAttn[0] = -100;
            quint8 out[5] = {};
            codec.composeCcForBank(11, ctx, out);
            QCOMPARE(int(out[4]), 0x7B);
        }

        // Out-of-range high: userDb=+100 → clamped to +31 → wire 0x40.
        {
            CodecContext ctx;
            ctx.rxStepAttn[0] = 100;
            quint8 out[5] = {};
            codec.composeCcForBank(11, ctx, out);
            QCOMPARE(int(out[4]), 0x40);
        }
    }

    // Bank 12 — HL2 has same MOX behavior as Standard (forces 0x1F under MOX),
    // no RedPitaya-special-case needed since HL2 is never confused with RedPitaya.
    // Source: mi0bot networkproto1.c:1107-1111 [@c26a8a4]
    void bank12_mox_forces_0x1F_standard_behavior() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = true;
        ctx.rxStepAttn[1] = 7;  // ignored under MOX — HL2 forces 0x1F like Standard
        quint8 out[5] = {};
        codec.composeCcForBank(12, ctx, out);
        // Under MOX: C1 = 0x1F | 0x20 = 0x3F (mi0bot networkproto1.c:1107-1109)
        QCOMPARE(int(out[0]), 0x17);  // 0x16 | mox=1
        QCOMPARE(int(out[1]), 0x3F);  // 0x1F | 0x20 enable bit
    }

    // Bank 12 — RX path: uses rxStepAttn[1] unmasked (same as Standard)
    void bank12_rx_uses_user_attn() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = false;
        ctx.rxStepAttn[1] = 7;
        quint8 out[5] = {};
        codec.composeCcForBank(12, ctx, out);
        // RX: C1 = rxStepAttn[1] | 0x20
        QCOMPARE(int(out[1]), 7 | 0x20);
    }

    // Bank 17 — HL2 TX latency / PTT hang (NOT AnvelinaPro3 extra OC)
    // Source: mi0bot networkproto1.c:1162-1168 [@c26a8a4]
    void bank17_hl2_tx_latency_and_ptt_hang() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.hl2PttHang = 0x0A;
        ctx.hl2TxLatency = 0x55;
        quint8 out[5] = {};
        codec.composeCcForBank(17, ctx, out);
        QCOMPARE(int(out[0]), 0x2E);
        QCOMPARE(int(out[1]), 0x00);
        QCOMPARE(int(out[2]), 0x00);
        QCOMPARE(int(out[3]), 0x0A & 0x1F);
        QCOMPARE(int(out[4]), 0x55 & 0x7F);
    }

    // Bank 18 — HL2 reset on disconnect
    // Source: mi0bot networkproto1.c:1170-1176 [@c26a8a4]
    void bank18_hl2_reset_on_disconnect_set() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.hl2ResetOnDisconnect = true;
        quint8 out[5] = {};
        codec.composeCcForBank(18, ctx, out);
        QCOMPARE(int(out[0]), 0x74);
        QCOMPARE(int(out[4]), 0x01);
    }

    void bank18_hl2_reset_on_disconnect_clear() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.hl2ResetOnDisconnect = false;
        quint8 out[5] = {};
        codec.composeCcForBank(18, ctx, out);
        QCOMPARE(int(out[4]), 0x00);
    }

    // HL2 inherits bank0 from Standard — verify RX-only + rxOut encoding
    // matches Standard's byte-lock across all 8 {rxOnlyAnt × rxOut} combinations.
    // Source: Thetis networkproto1.c:453-468 + netInterface.c:479-481
    // [v2.10.3.13 @501e3f5]
    void bank0_c3_rxOnly_and_rxOut_byteLock_hl2_parity() {
        struct Case { int rxOnly; bool rxOut; quint8 expectedMask; };
        const std::array<Case, 8> cases = {{
            {0, false, 0b0000'0000},  // no RX-only path, no bypass
            {1, false, 0b0010'0000},  // _Rx_1_In (bit5)
            {2, false, 0b0100'0000},  // _Rx_2_In (bit6)
            {3, false, 0b0110'0000},  // _XVTR_Rx_In (bits5+6)
            {0, true,  0b1000'0000},  // bypass only (bit7)
            {1, true,  0b1010'0000},  // RX1_In + bypass
            {2, true,  0b1100'0000},  // RX2_In + bypass
            {3, true,  0b1110'0000},  // XVTR + bypass
        }};

        for (const auto& tc : cases) {
            CodecContext ctx{};
            ctx.rxOnlyAnt = tc.rxOnly;
            ctx.rxOut     = tc.rxOut;
            quint8 out[5] = {};
            P1CodecHl2 codec;
            codec.composeCcForBank(0, ctx, out);

            // Mask off bits 0-4 so preamp/dither/random defaults don't contaminate.
            const quint8 rxOnlyMask = out[3] & 0b1110'0000;
            QCOMPARE(int(rxOnlyMask), int(tc.expectedMask));
        }
    }

    // Banks 0-10 unchanged from Standard — spot-check bank 10 (Alex filters)
    void bank10_alex_filter_passthrough() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.alexHpfBits = 0x01;
        ctx.alexLpfBits = 0x01;
        quint8 out[5] = {};
        codec.composeCcForBank(10, ctx, out);
        QCOMPARE(int(out[0]), 0x12);
        QCOMPARE(int(out[3]) & 0x7F, 0x01);
        QCOMPARE(int(out[4]), 0x01);
    }

    // Bank 10 C2 must include bit 3 (0x08) on HL2 — mi0bot repurposes the
    // ApolloTuner bit as the HL2 PA-enable signal.  Without this bit set,
    // the HL2 firmware treats MOX assertions as "want to TX but PA not
    // enabled" and oscillates the T/R relay.
    //
    // Source:
    //   mi0bot ChannelMaster/networkproto1.c:1084-1085 [v2.10.3.14-beta1]
    //     C2 = ((mic_boost & 1) | ((line_in & 1) << 1) | ApolloFilt |
    //           ApolloTuner | ApolloATU | ApolloFiltSelect | 0b01000000) & 0x7f;
    //   mi0bot ChannelMaster/netInterface.c:582-588 [v2.10.3.14-beta1]
    //     EnableApolloTuner(int bits): bits != 0 → ApolloTuner = 0x8 else 0
    //   mi0bot ChannelMaster/netInterface.c:629-635 [v2.10.3.14-beta1]
    //     DisablePA on HL2 routes through EnableApolloTuner(!bit).
    void bank10_c2_pa_enable_bit_set_for_hl2() {
        P1CodecHl2 codec;
        CodecContext ctx;
        // Default ctx — no mic boost, no line in, no caller PA toggle.
        // mi0bot's tx[0].pa defaults to 0 (PA enabled), so EnableApolloTuner(1)
        // sets ApolloTuner = 0x8 by default — bit 3 of bank 10 C2 is set.
        quint8 out[5] = {};
        codec.composeCcForBank(10, ctx, out);
        QVERIFY2((out[2] & 0x08) != 0,
                 "bank 10 C2 bit 3 (HL2 PA enable / mi0bot ApolloTuner) must be set");
        // Bit 6 (always-on default mask 0x40) must remain set.
        QVERIFY2((out[2] & 0x40) != 0,
                 "bank 10 C2 bit 6 (always-on default mask) must remain set");
    }

    // Confirm the bit is also present when other bank-10 C2 flags are on
    // (mic_boost, line_in) — bit 3 must persist regardless of those toggles.
    void bank10_c2_pa_enable_bit_set_with_micBoost_and_lineIn() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.p1MicBoost = true;
        ctx.p1LineIn   = true;
        quint8 out[5] = {};
        codec.composeCcForBank(10, ctx, out);
        QCOMPARE(int(out[2] & 0x01), 0x01);  // mic_boost preserved
        QCOMPARE(int(out[2] & 0x02), 0x02);  // line_in preserved
        QVERIFY2((out[2] & 0x08) != 0,
                 "bank 10 C2 bit 3 (HL2 PA enable) must coexist with mic_boost / line_in");
        QVERIFY2((out[2] & 0x40) != 0,
                 "bank 10 C2 bit 6 (always-on) must remain set");
    }
};

QTEST_APPLESS_MAIN(TestP1CodecHl2)
#include "tst_p1_codec_hl2.moc"
