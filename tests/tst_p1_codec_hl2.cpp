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

    // Bank 11 C4 — RX path: 6-bit mask + 0x40 enable
    // Source: mi0bot networkproto1.c:1102 [@c26a8a4]
    void bank11_rx_att_20dB_hl2_encoding() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = false;
        ctx.rxStepAttn[0] = 20;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[0]), 0x14);
        QCOMPARE(int(out[4]), (20 & 0x3F) | 0x40);  // 0x54
    }

    // Bank 11 C4 — TX path: uses txStepAttn[0] not rxStepAttn[0]
    // Source: mi0bot networkproto1.c:1099-1100 [@c26a8a4]
    void bank11_tx_att_uses_tx_value() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = true;
        ctx.rxStepAttn[0] = 0;     // RX-side value ignored
        ctx.txStepAttn[0] = 20;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[0]), 0x15);  // C0=0x14 | mox
        QCOMPARE(int(out[4]), (20 & 0x3F) | 0x40);  // 0x54 from TX side
    }

    // Bank 11 C4 — full 6-bit range (HL2 supports 0-63)
    void bank11_rx_att_63dB_full_hl2_range() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.rxStepAttn[0] = 63;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[4]), 0x3F | 0x40);  // 0x7F
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
};

QTEST_APPLESS_MAIN(TestP1CodecHl2)
#include "tst_p1_codec_hl2.moc"
