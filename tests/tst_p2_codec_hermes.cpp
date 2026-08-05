// no-port-check: test fixture cites Thetis source for expected values, not a port itself
//
// P2CodecHermes serves the 1-ADC Apache Labs Protocol 2 boards: HW::Hermes and
// HW::HermesC10 (ANAN-G2E), with P2CodecHermesII for ANAN-10E / ANAN-100B
// (P2RadioConnection.cpp:2163-2169). Hermes Lite 2 is NOT one of these; it is
// Protocol 1 and has its own P1CodecHl2, whose authoritative upstream is
// mi0bot-Thetis rather than ramdor/Thetis. The upstream cited here is
// ramdor/Thetis console.cs, which is correct for these Apache Labs boards.

#include <QtTest/QtTest>
#include "core/codec/P2CodecHermes.h"
#include "core/codec/CodecContext.h"

using namespace NereusSDR;

class TestP2CodecHermes : public QObject {
    Q_OBJECT
private:
    static std::array<SliceConfig, 5> oneLiveSlice()
    {
        std::array<SliceConfig, 5> slices{};
        slices[0].live         = true;
        slices[0].frequencyHz  = 7100000;
        slices[0].sampleRateHz = 192000;
        return slices;
    }

private slots:
    // ── Codex PR #293 P1: PureSignal reclaims DDC0, so no user stream owns it ──
    //
    // On a 1-ADC Hermes-class board, transmitting with PureSignal on, upstream
    // hands the whole radio to the PS pair. From Thetis console.cs:8449-8456
    // [v2.10.3.15]:
    //     P1_DDCConfig = 6; DDCEnable = DDC0; SyncEnable = DDC1;
    //     Rate[0] = ps_rate; Rate[1] = ps_rate;
    // There is no user receiver left: DDC0 is the PS feedback leg and DDC1 the
    // TX monitor, which is exactly what the branch records in psFwdDdc /
    // psRevDdc and why it sets nDdc = 2.
    //
    // streamDdc[0] was being set before the branch ran, so logical stream 0
    // stayed pointed at DDC0. publishDdcAssignment would then map stream 0 to
    // DDC0, report it available rather than suspended, and push PA-feedback
    // samples through the ordinary RX DSP path.
    void puresignal_mox_leaves_no_user_stream_on_ddc0() {
        P2CodecHermes codec;
        CodecContext ctx{};
        ctx.puresignalRun = true;
        ctx.mox           = true;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, oneLiveSlice());

        // Preconditions: this really is the PS branch.
        QCOMPARE(a.psFwdDdc, 0);
        QCOMPARE(a.psRevDdc, 1);

        QVERIFY2(a.streamDdc[0] == -1,
            qPrintable(QStringLiteral("PureSignal owns DDC0 while transmitting on a "
                "Hermes-class board, so no user stream may claim it. "
                "streamDdc[0]=%1, expected -1").arg(a.streamDdc[0])));
    }

    // The control cases. Both must keep the mapping, or the fix above would
    // have broken ordinary receive to satisfy the PS case.
    void plain_rx_maps_stream0_to_ddc0() {
        P2CodecHermes codec;
        CodecContext ctx{};

        const DdcAssignment a = codec.applyDdcAssignment(ctx, oneLiveSlice());

        QCOMPARE(a.streamDdc[0], 0);
        QCOMPARE(a.psFwdDdc, -1);
    }

    void diversity_maps_stream0_to_ddc0() {
        P2CodecHermes codec;
        CodecContext ctx{};
        ctx.diversity = true;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, oneLiveSlice());

        // From Thetis console.cs:8437-8446 [v2.10.3.15]: DDCEnable = DDC0,
        // SyncEnable = DDC1. Stream 0 genuinely is on DDC0 here.
        QCOMPARE(a.streamDdc[0], 0);
    }

    // A dormant slice must not claim a DDC either, PS or no PS.
    void no_live_slice_leaves_stream0_idle() {
        P2CodecHermes codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};   // nothing live

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[0], -1);
    }
};

QTEST_APPLESS_MAIN(TestP2CodecHermes)
#include "tst_p2_codec_hermes.moc"
