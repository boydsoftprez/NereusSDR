// no-port-check: test fixture cites Thetis source for expected values, not a port itself
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW bank 12-14 cases (R-CW-11). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
#include <QtTest/QtTest>
#include "core/codec/P1CodecRedPitaya.h"
#include "P1CodecCwCases.h"

using namespace NereusSDR;

class TestP1CodecRedPitaya : public QObject {
    Q_OBJECT
private slots:
    // CW transmit (R-CW-11): banks 12-14 carry the radio keyer's settings.
    // From Thetis networkproto1.c:618-646 [v2.10.3.15]
    void cwBanks_carryTheKeyerSettings() {
        P1CodecRedPitaya codec;
        P1CwCases::runAll(codec);
    }

    void maxBank_is_16() {
        P1CodecRedPitaya codec;
        QCOMPARE(codec.maxBank(), 16);
    }

    // Bank 12 — RedPitaya does NOT force 0x1F during MOX. It uses the
    // user-set rxStepAttn[1] masked to 5 bits.
    // Source: networkproto1.c:611-613 [@501e3f5] (DH1KLM contribution)
    void bank12_mox_uses_user_attn_not_forced_max() {
        P1CodecRedPitaya codec;
        CodecContext ctx;
        ctx.mox = true;
        ctx.rxStepAttn[1] = 12;  // user-set ADC1 ATT
        quint8 out[5] = {};
        codec.composeCcForBank(12, ctx, out);
        QCOMPARE(int(out[0]), 0x17);  // C0=0x16 | mox
        // RedPitaya: respects user attn masked to 5 bits + 0x20 enable
        QCOMPARE(int(out[1]), (12 & 0x1F) | 0x20);
        QCOMPARE(int(out[2]), 0x20);  // ADC2 = 0 + enable
    }

    // Bank 12 RX path identical to Standard (still 5-bit masked + 0x20)
    void bank12_rx_path_uses_user_attn() {
        P1CodecRedPitaya codec;
        CodecContext ctx;
        ctx.rxStepAttn[1] = 7;
        quint8 out[5] = {};
        codec.composeCcForBank(12, ctx, out);
        QCOMPARE(int(out[1] & 0x1F), 7);
    }
};

QTEST_APPLESS_MAIN(TestP1CodecRedPitaya)
#include "tst_p1_codec_redpitaya.moc"
