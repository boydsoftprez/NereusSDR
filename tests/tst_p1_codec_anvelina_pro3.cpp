// no-port-check: test fixture cites Thetis source for expected values, not a port itself
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW bank 12-14 cases (R-CW-11). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
#include <QtTest/QtTest>
#include "core/codec/P1CodecAnvelinaPro3.h"
#include "P1CodecCwCases.h"

using namespace NereusSDR;

class TestP1CodecAnvelinaPro3 : public QObject {
    Q_OBJECT
private slots:
    // CW transmit (R-CW-11): banks 12-14 carry the radio keyer's settings.
    // From Thetis networkproto1.c:618-646 [v2.10.3.15]
    void cwBanks_carryTheKeyerSettings() {
        P1CodecAnvelinaPro3 codec;
        P1CwCases::runAll(codec);
    }

    void maxBank_is_17() {
        P1CodecAnvelinaPro3 codec;
        QCOMPARE(codec.maxBank(), 17);
    }

    // Bank 17 — extra OC outputs, AnvelinaPro3 only: the DX outputs' own
    // byte (prn->oc_output_extras), masked with kDxWireMask (bits 1-4).
    // Source: networkproto1.c:668-674 [v2.10.3.15]; mask per the Anvelina DX
    // spec page 2 (NereusSDR-divergence from Thetis's 0x0f).
    void bank17_extra_oc() {
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx;
        ctx.ocExtrasByte = 0x0A;  // DX OUT 7 and 9
        quint8 out[5] = {};
        codec.composeCcForBank(17, ctx, out);
        QCOMPARE(int(out[0]), 0x26);
        QCOMPARE(int(out[1]), 0x0A);
        QCOMPARE(int(out[2]), 0x00);
        QCOMPARE(int(out[3]), 0x00);
        QCOMPARE(int(out[4]), 0x00);
    }

    // C1 is ocExtrasByte & 0x1E: bit 0 and bits 5-7 never reach the wire,
    // bit 4 (DX OUT 10) does.
    void bank17_masks_with_dx_wire_mask() {
        P1CodecAnvelinaPro3 codec;
        for (int v : {0x00, 0x01, 0x10, 0x1E, 0x1F, 0xFF, 0xE1}) {
            CodecContext ctx;
            ctx.ocExtrasByte = quint8(v);
            quint8 out[5] = {};
            codec.composeCcForBank(17, ctx, out);
            QCOMPARE(int(out[1]), v & 0x1E);
        }
    }

    // The band OC byte no longer reaches bank 17.
    void bank17_ignores_band_oc_byte() {
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx;
        ctx.ocByte = 0x05;
        ctx.ocExtrasByte = 0x00;
        quint8 out[5] = {};
        codec.composeCcForBank(17, ctx, out);
        QCOMPARE(int(out[1]), 0x00);
    }

    // MOX rides C0 bit 0 as in every bank.
    void bank17_mox_bit() {
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx;
        ctx.mox = true;
        quint8 out[5] = {};
        codec.composeCcForBank(17, ctx, out);
        QCOMPARE(int(out[0]), 0x27);
    }

    // Banks 0-16 still match Standard
    void bank0_still_matches_standard() {
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx;
        ctx.sampleRateCode = 1;
        quint8 out[5] = {};
        codec.composeCcForBank(0, ctx, out);
        QCOMPARE(int(out[1] & 0x03), 1);  // 96 kHz
    }
};

QTEST_APPLESS_MAIN(TestP1CodecAnvelinaPro3)
#include "tst_p1_codec_anvelina_pro3.moc"
