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

    // ── Anti-drift guard for the DDC assignment (defect D3) ───────────────
    //
    // Thetis groups HPSDRModel.ANAN_G2 and ANAN_G2_1K into the SAME
    // UpdateDDCs switch case as the OrionMkII family (console.cs:8220-8303
    // [v2.10.3.15]), so there is no Saturn-specific DDC assignment to have.
    // NereusSDR nonetheless carried a hand-copied Saturn override for three
    // months; when antenna-driven ADC routing was added to the parent on
    // 2026-07-26 the copy silently kept the old behaviour, and the feature
    // was dead on the one radio it was written for.
    //
    // The fix deleted the override. This test is what stops it coming back:
    // it sweeps the state matrix the two codecs branch on and demands the
    // two produce identical assignments. Add a Saturn-only DDC rule and this
    // fails, which is the point -- a genuine divergence should have to be
    // argued for here, not arrive by omission.
    void ddcAssignment_inherits_orionmkii_behavior() {
        P2CodecSaturn satCodec;
        P2CodecOrionMkII baseCodec;

        // Antennas worth sweeping: ANT1 (main chain), EXT1 and EXT2 (the
        // RX-only jacks that select chain 1), BYPS (deliberately chain 0).
        const int antennas[] = {1, 4, 5, 6};

        for (int mox = 0; mox < 2; ++mox) {
        for (int ps = 0; ps < 2; ++ps) {
        for (int div = 0; div < 2; ++div) {
        for (int liveCount = 1; liveCount <= 5; ++liveCount) {
        for (int ant : antennas) {
            CodecContext ctx{};
            ctx.mox           = (mox != 0);
            ctx.puresignalRun = (ps != 0);
            ctx.diversity     = (div != 0);
            // Seed the Thetis default so the diversity branch is exercised
            // with a non-zero incoming word rather than an all-zero one.
            ctx.adcCtrl = NereusSDR::defaultRxAdcCtrl(2);

            std::array<SliceConfig, 5> slices{};
            for (int i = 0; i < liveCount; ++i) {
                slices[i].live         = true;
                slices[i].frequencyHz  = 7000000 + i * 3000000;
                slices[i].sampleRateHz = 192000;
                slices[i].antennaIndex = ant;
            }
            slices[0].txBound = true;

            const DdcAssignment s = satCodec.applyDdcAssignment(ctx, slices);
            const DdcAssignment b = baseCodec.applyDdcAssignment(ctx, slices);

            QCOMPARE(s.ddcEnable,  b.ddcEnable);
            QCOMPARE(s.syncEnable, b.syncEnable);
            QCOMPARE(s.adcCtrl1,   b.adcCtrl1);
            QCOMPARE(s.adcCtrl2,   b.adcCtrl2);
            QCOMPARE(s.nDdc,       b.nDdc);
            QCOMPARE(s.psFwdDdc,   b.psFwdDdc);
            QCOMPARE(s.psRevDdc,   b.psRevDdc);
            for (int i = 0; i < 8; ++i) { QCOMPARE(s.rate[i], b.rate[i]); }
            for (int i = 0; i < 5; ++i) { QCOMPARE(s.streamDdc[i], b.streamDdc[i]); }
        }}}}}
    }

    // The defect in one assertion: on Saturn, a slice on an RX-only jack
    // must land on the second chain. Stated separately from the parity
    // sweep so a reader sees the behaviour, not just the equality.
    void ddcAssignment_saturn_routes_ext1_to_adc1() {
        P2CodecSaturn codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].sampleRateHz = 192000;
        slices[0].antennaIndex = 4;   // EXT1

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        // Stream 0 -> DDC2, whose ADC selector is adcCtrl1 bits 5&4.
        QCOMPARE(a.adcCtrl1 & 0x30, 1 << 4);
    }
};

QTEST_APPLESS_MAIN(TestP2CodecSaturn)
#include "tst_p2_codec_saturn.moc"
