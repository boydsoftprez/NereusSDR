// no-port-check: test fixture cites Thetis source for expected values, not a port itself
#include <QtTest/QtTest>
#include "core/codec/P1CodecAnvelinaPro3.h"

using namespace NereusSDR;

class TestP1CodecAnvelinaPro3 : public QObject {
    Q_OBJECT
private slots:
    void maxBank_is_17() {
        P1CodecAnvelinaPro3 codec;
        QCOMPARE(codec.maxBank(), 17);
    }

    // Bank 17 — extra OC outputs, AnvelinaPro3 only.
    // Source: networkproto1.c:668-674 [@501e3f5]
    void bank17_extra_oc() {
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx;
        ctx.ocByte = 0x05;  // oc_output_extras nibble in low 4 bits
        quint8 out[5] = {};
        codec.composeCcForBank(17, ctx, out);
        QCOMPARE(int(out[0]), 0x26);
        QCOMPARE(int(out[1]), 0x05);  // & 0x0F
        QCOMPARE(int(out[2]), 0x00);
        QCOMPARE(int(out[3]), 0x00);
        QCOMPARE(int(out[4]), 0x00);
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
