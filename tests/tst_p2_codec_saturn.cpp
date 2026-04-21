// no-port-check: test fixture cites Thetis source for expected values, not a port itself
#include <QtTest/QtTest>
#include "core/codec/P2CodecSaturn.h"
#include "core/codec/P2CodecOrionMkII.h"

using namespace NereusSDR;

class TestP2CodecSaturn : public QObject {
    Q_OBJECT
private slots:
    // When CodecContext.p2SaturnBpfHpfBits is 0 (no Saturn override),
    // Saturn falls through to OrionMkII's Alex bits behavior.
    void cmdHighPriority_no_saturn_override_falls_to_alex_defaults() {
        P2CodecSaturn codec;
        CodecContext ctx;
        ctx.alexHpfBits = 0x01;        // 13 MHz HPF (Alex default)
        ctx.p2SaturnBpfHpfBits = 0;    // no Saturn override
        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);
        // Alex1 (RX) bytes 1428-1431 should reflect ctx.alexHpfBits.
        // buildAlex1 maps alexHpfBits 0x01 → bit 1 of the 32-bit register.
        // The register is written big-endian at offset 1428:
        //   buf[1428] = (reg >> 24) & 0xff
        //   buf[1429] = (reg >> 16) & 0xff
        //   buf[1430] = (reg >>  8) & 0xff
        //   buf[1431] = (reg      ) & 0xff
        // Bit 1 of the word → value 2 → lands in buf[1431].
        QVERIFY(buf[1428] != 0 || buf[1429] != 0 || buf[1430] != 0 || buf[1431] != 0);
    }

    // When CodecContext.p2SaturnBpfHpfBits is non-zero, Saturn uses it
    // for Alex1 (RX) instead of alexHpfBits.
    // Source: console.cs:6944-7040 [@501e3f5] (G8NJJ setBPF1ForOrionIISaturn)
    void cmdHighPriority_saturn_override_uses_bpf1_bits() {
        P2CodecSaturn codec;
        CodecContext ctx;
        ctx.alexHpfBits = 0x01;            // 13 MHz HPF (would be sent without override)
        ctx.p2SaturnBpfHpfBits = 0x10;     // 1.5 MHz HPF (Saturn override)
        quint8 buf1444Saturn[1444] = {};
        codec.composeCmdHighPriority(ctx, buf1444Saturn);

        // Compare to output without override — bytes should differ
        // in the Alex1 region (1428-1431).
        ctx.p2SaturnBpfHpfBits = 0;
        quint8 buf1444Default[1444] = {};
        codec.composeCmdHighPriority(ctx, buf1444Default);

        // At least one byte in the Alex1 region must differ between the
        // two — the Saturn override changed the HPF bits.
        bool differ = false;
        for (int i = 1428; i < 1432; ++i) {
            if (buf1444Saturn[i] != buf1444Default[i]) { differ = true; break; }
        }
        QVERIFY2(differ, "Saturn override should change Alex1 HPF bits");
    }

    // Saturn output with no override must match OrionMkII output byte-for-byte.
    void cmdHighPriority_saturn_no_override_matches_orionmkii() {
        P2CodecSaturn satCodec;
        P2CodecOrionMkII baseCodec;
        CodecContext ctx;
        ctx.alexHpfBits = 0x01;
        ctx.p2SaturnBpfHpfBits = 0;   // no override → must match parent
        quint8 satBuf[1444] = {};
        quint8 baseBuf[1444] = {};
        satCodec.composeCmdHighPriority(ctx, satBuf);
        baseCodec.composeCmdHighPriority(ctx, baseBuf);
        for (int i = 0; i < 1444; ++i) {
            QCOMPARE(int(satBuf[i]), int(baseBuf[i]));
        }
    }

    // Banks unchanged from OrionMkII (CmdGeneral, CmdRx, CmdTx)
    void cmdGeneral_inherits_orionmkii_behavior() {
        P2CodecSaturn satCodec;
        P2CodecOrionMkII baseCodec;
        CodecContext ctx;
        ctx.mox = true;
        quint8 satBuf[60] = {};
        quint8 baseBuf[60] = {};
        satCodec.composeCmdGeneral(ctx, satBuf);
        baseCodec.composeCmdGeneral(ctx, baseBuf);
        for (int i = 0; i < 60; ++i) { QCOMPARE(int(satBuf[i]), int(baseBuf[i])); }
    }

    void cmdTx_inherits_orionmkii_behavior() {
        P2CodecSaturn satCodec;
        P2CodecOrionMkII baseCodec;
        CodecContext ctx;
        ctx.txStepAttn[0] = 5;
        quint8 satBuf[60] = {};
        quint8 baseBuf[60] = {};
        satCodec.composeCmdTx(ctx, satBuf);
        baseCodec.composeCmdTx(ctx, baseBuf);
        for (int i = 0; i < 60; ++i) { QCOMPARE(int(satBuf[i]), int(baseBuf[i])); }
    }
};

QTEST_APPLESS_MAIN(TestP2CodecSaturn)
#include "tst_p2_codec_saturn.moc"
