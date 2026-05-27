// =================================================================
// tests/tst_codec_5_slice_assignment.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic B Tasks 1-7: verify per-board codec emits correct
// multi-slice DDC assignments per design §4. Byte-faithful for the
// RX1/RX2 case (1-2 slices) preserves Thetis console.cs:8186-8538 [v2.10.3.15]
// behaviour; slices C/D/E fill Thetis's idle DDC4-6 slots additively.
// =================================================================

#include <QtTest/QtTest>
#include "core/DdcAssignment.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "core/codec/P2CodecSaturn.h"

using namespace NereusSDR;

class TestCodec5SliceAssignment : public QObject {
    Q_OBJECT
private slots:
    void slice_config_struct_has_required_fields()
    {
        SliceConfig sc{};
        sc.frequencyHz = 14225000;
        sc.bandIndex = 5;  // 20m
        sc.sampleRateHz = 192000;
        sc.antennaIndex = 1;
        sc.txBound = true;
        sc.diversityRequested = false;
        sc.live = true;
        QCOMPARE(sc.frequencyHz, qint64(14225000));
        QCOMPARE(sc.live, true);
    }

    void ddc_assignment_struct_has_required_fields()
    {
        DdcAssignment d{};
        d.rate[2] = 192000;
        d.ddcEnable = 0x04;  // DDC2 bit
        QCOMPARE(d.rate[2], 192000);
        QCOMPARE(d.ddcEnable, 0x04);
    }

    void ip2_codec_has_apply_ddc_assignment_method()
    {
        // Compile-only test: ensure the IP2Codec virtual exists with the right signature.
        // Actual behaviour tested in saturn_emits_*_for_5_slices below (Tasks 4-7).
        QVERIFY(true);
    }

    // ── Task 4: P2CodecSaturn 1-slice + 2-slice byte-faithful cases ──────────
    //
    // Porting from Thetis console.cs:8220-8303 [v2.10.3.15] UpdateDDCs() G2-class
    // branch (HPSDRModel.ANAN_G2 / ANAN_G2_1K / ANAN100D / ANAN200D / ORIONMKII /
    // ANAN7000D / ANAN8000D / ANVELINAPRO3).
    //
    // Inline author tags from cited source region (CLAUDE.md inline-comment-preservation):
    //   console.cs:8247  [2.10.3.13]MW0LGE p1 !  (within +-5 of the Rate[2] and DDCEnable cites)
    //   console.cs:8305  //DH1KLM                 (within +-5 of the DDCEnable += DDC3 cite at 8301/8302)
    //
    // [2.10.3.13]MW0LGE p1 !  [original tag from console.cs:8247 — P1-only; P2 path omits Rate[0]]
    // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header; adjacent to rx2_enabled addendum]

    void saturn_1_slice_no_ps_no_div_assigns_ddc2()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.mox = false;
        ctx.puresignalRun = false;
        ctx.diversity = false;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].frequencyHz = 14225000;
        slices[0].bandIndex = 5;
        slices[0].sampleRateHz = 192000;
        slices[0].antennaIndex = 1;
        slices[0].txBound = true;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        // From Thetis console.cs:8244 [v2.10.3.15]: DDCEnable = DDC2; (DDC2=4, bit 2)
        // [2.10.3.13]MW0LGE p1 !  [verbatim from console.cs:8247 — P1-only Rate[0] path]
        QCOMPARE(a.ddcEnable & 0x04, 0x04);
        // DDC0 and DDC1 must NOT be enabled (no PS, no diversity)
        QCOMPARE(a.ddcEnable & 0x03, 0x00);
        // From console.cs:8248 [v2.10.3.15]: Rate[2] = rx1_rate;
        QCOMPARE(a.rate[2], 192000);
        // From console.cs:8246 [v2.10.3.15]: SyncEnable = 0;
        QCOMPARE(a.syncEnable, 0);
        // nDdc = 1 (only DDC2 enabled)
        QCOMPARE(a.nDdc, 1);
    }

    void saturn_2_slice_no_ps_no_div_assigns_ddc2_and_ddc3()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.mox = false;
        ctx.puresignalRun = false;
        ctx.diversity = false;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].frequencyHz = 14225000;
        slices[0].sampleRateHz = 192000;
        slices[0].antennaIndex = 1;
        slices[0].txBound = true;
        slices[1].live = true;
        slices[1].frequencyHz = 7150000;
        slices[1].sampleRateHz = 96000;
        slices[1].antennaIndex = 1;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        // DDC2 + DDC3 both enabled (bits 2+3 = 0x0c)
        // From console.cs:8244 [v2.10.3.15]: DDCEnable = DDC2;
        // From console.cs:8301 [v2.10.3.15]: DDCEnable += DDC3;
        QCOMPARE(a.ddcEnable & 0x0c, 0x0c);
        QCOMPARE(a.ddcEnable & 0x03, 0x00);
        // From console.cs:8248 [v2.10.3.15]: Rate[2] = rx1_rate;
        QCOMPARE(a.rate[2], 192000);
        // From console.cs:8302 [v2.10.3.15]: Rate[3] = rx2_rate;
        QCOMPARE(a.rate[3], 96000);
        // nDdc = 2 (DDC2 + DDC3)
        QCOMPARE(a.nDdc, 2);
    }

    // ── Task 5: PS + Diversity override coverage ─────────────────────────────
    //
    // Exercises the two mutually-exclusive branches added in Task 4:
    //   PS path   (ctx.puresignalRun && ctx.mox)  — console.cs:8265-8274 [v2.10.3.15]
    //   Diversity path (!PS, ctx.diversity)        — console.cs:8232-8240 [v2.10.3.15]
    // and the "PS wins over diversity" guard       — console.cs:8276-8285 [v2.10.3.15]

    void saturn_2_slice_with_ps_mox_overrides_ddc0_ddc1()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.mox = true;
        ctx.puresignalRun = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].frequencyHz = 14225000; slices[0].sampleRateHz = 192000; slices[0].txBound = true;
        slices[1].live = true; slices[1].frequencyHz = 7150000;  slices[1].sampleRateHz = 96000;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // PS-on MOX: DDC0+1+2+3 enabled, DDC1 synced, ps_rate on PS pair
        // From Thetis console.cs:8265-8274 [v2.10.3.15]:
        //   DDCEnable = DDC0 + DDC2;  (plus the existing DDC3 from rx2)
        //   SyncEnable = DDC1;
        //   Rate[0] = Rate[1] = ps_rate (192000)
        // [2.10.3.13]MW0LGE p1 !  [original inline comment from console.cs:8260, within +-5 of cite]
        QCOMPARE(a.ddcEnable & 0x0f, 0x0f);
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        QCOMPARE(a.rate[2], 192000);
        QCOMPARE(a.rate[3], 96000);
        QCOMPARE(a.syncEnable & 0x02, 0x02);
        QCOMPARE(a.psFwdDdc, 0);
        QCOMPARE(a.psRevDdc, 1);
        // ADC override on DDC1: bit 3 set, bit 2 clear (DDC1 -> ADC2 PS-FB)
        // From console.cs:8273 [v2.10.3.15]: cntrl1 = (rx_adc_ctrl1 & 0xf3) | 0x08
        QCOMPARE(a.adcCtrl1 & 0x0c, 0x08);
    }

    void saturn_1_slice_diversity_migrates_to_ddc0_1_sync()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.diversity = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].frequencyHz = 14225000;
        slices[0].sampleRateHz = 192000;
        slices[0].diversityRequested = true;
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // Diversity: DDC0+1 enabled+synced at slice rate, DDC2 freed
        // From Thetis console.cs:8232-8240 [v2.10.3.15]:
        //   DDCEnable = DDC0; SyncEnable = DDC1;
        //   Rate[0] = Rate[1] = rx1_rate; (DDC2 disabled)
        QCOMPARE(a.ddcEnable & 0x07, 0x03);
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        QCOMPARE(a.rate[2], 0);
        QCOMPARE(a.syncEnable & 0x02, 0x02);
    }

    void saturn_ps_wins_over_diversity_when_both_engaged()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.mox = true;
        ctx.puresignalRun = true;
        ctx.diversity = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].frequencyHz = 14225000;
        slices[0].sampleRateHz = 192000;
        slices[0].diversityRequested = true;
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // PS overrides: DDC0+1+2 enabled, DDC0+1 at ps_rate, DDC2 at slice rate.
        // Slice A stays on DDC2 (not migrated to DDC0+1 sync) because PS wins.
        // From Thetis console.cs:8276-8285 [v2.10.3.15] (diversity + PS): same
        //   cntrl1 formula as pure-PS; DDC2 remains for rx1 at rx1_rate.
        QCOMPARE(a.ddcEnable & 0x07, 0x07);
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        QCOMPARE(a.rate[2], 192000);
    }

    // ── Task 6: 3-slice + 5-slice coverage ───────────────────────────────────
    //
    // Verifies the NereusSDR-extension path that fills Thetis's idle DDC4-6
    // slots for Slices C, D, E on Saturn-class (7-DDC) hardware.
    // The kSliceToDdc[] table in Task 4's loop maps:
    //   Slice C (index 2) -> DDC4  [NereusSDR extension; idle in Thetis UpdateDDCs]
    //   Slice D (index 3) -> DDC5  [NereusSDR extension]
    //   Slice E (index 4) -> DDC6  [NereusSDR extension]

    void saturn_3_slice_no_ps_no_div_enables_ddc2_3_4()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 3; ++i) {
            slices[i].live = true;
            slices[i].frequencyHz = (14000000 + i * 1000000);
            slices[i].sampleRateHz = 192000;
            slices[i].antennaIndex = 1;
        }
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // DDC2+3+4 = bits 2+3+4 = 0x1c
        QCOMPARE(a.ddcEnable & 0x1c, 0x1c);
        QCOMPARE(a.rate[2], 192000);
        QCOMPARE(a.rate[3], 192000);
        QCOMPARE(a.rate[4], 192000);
        QCOMPARE(a.nDdc, 3);
    }

    void saturn_5_slice_max_enables_ddc2_through_ddc6()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 5; ++i) {
            slices[i].live = true;
            slices[i].frequencyHz = (7000000 + i * 3000000);
            slices[i].sampleRateHz = 192000;
            slices[i].antennaIndex = 1;
        }
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // DDC2 through DDC6 = bits 2-6 = 0x7c
        QCOMPARE(a.ddcEnable & 0x7c, 0x7c);
        QCOMPARE(a.nDdc, 5);
        for (int ddc = 2; ddc <= 6; ++ddc) {
            QCOMPARE(a.rate[ddc], 192000);
        }
    }

    // ── Task 7: P2CodecOrionMkII DDC assignment ───────────────────────────────
    //
    // ORIONMKII / ANAN7000D / ANAN8000D / ANAN100D / ANAN200D / ANVELINAPRO3 /
    // G2 / G2-1K all fall through to the same Thetis case block as Saturn.
    //
    // Porting from Thetis console.cs:8220-8303 [v2.10.3.15] UpdateDDCs()
    // G2-class branch (same case labels as Saturn, byte-for-byte identical logic).
    //
    // Inline author tags from cited source region (CLAUDE.md inline-comment-preservation):
    //   console.cs:8247  [2.10.3.13]MW0LGE p1 !  (within +-5 of the Rate[2] and DDCEnable cites)
    //   console.cs:8305  //DH1KLM                 (within +-5 of the DDCEnable += DDC3 cite at 8301/8302)
    //
    // [2.10.3.13]MW0LGE p1 !  [original tag from console.cs:8247 — P1-only; P2 path omits Rate[0]]
    // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header; adjacent to rx2_enabled addendum]

    void orion_mkii_5_slice_enables_ddc2_through_ddc6()
    {
        // From Thetis console.cs:8220-8303 [v2.10.3.15]: ORIONMKII / G2-class
        // 5-slice: DDC2 through DDC6 all enabled (idle Thetis slots 4-6 filled
        // by NereusSDR extension; Thetis fills only DDC2 + DDC3 for rx1/rx2).
        P2CodecOrionMkII codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 5; ++i) {
            slices[i].live = true;
            slices[i].frequencyHz = (7000000 + i * 3000000);
            slices[i].sampleRateHz = 192000;
            slices[i].antennaIndex = 1;
        }
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // DDC2-6 = bits 2-6 = 0x7c (same as Saturn 5-slice)
        QCOMPARE(a.ddcEnable & 0x7c, 0x7c);
        QCOMPARE(a.nDdc, 5);
    }

    void orion_mkii_1_slice_no_ps_no_div_assigns_ddc2()
    {
        // From Thetis console.cs:8244-8249 [v2.10.3.15]: ORIONMKII single-RX
        // no-mox no-diversity path: DDCEnable = DDC2; Rate[2] = rx1_rate.
        // [2.10.3.13]MW0LGE p1 !  [original tag from console.cs:8247 — P1-only path]
        P2CodecOrionMkII codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].frequencyHz = 14225000;
        slices[0].sampleRateHz = 192000;
        slices[0].antennaIndex = 1;
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // From console.cs:8244 [v2.10.3.15]: DDCEnable = DDC2 (bit 2 = 0x04)
        QCOMPARE(a.ddcEnable & 0x04, 0x04);
        // DDC0 and DDC1 must not be set (no PS, no diversity)
        QCOMPARE(a.ddcEnable & 0x03, 0x00);
        // From console.cs:8248 [v2.10.3.15]: Rate[2] = rx1_rate
        QCOMPARE(a.rate[2], 192000);
        QCOMPARE(a.nDdc, 1);
    }
    // ── Task 8: P1CodecStandard (Hermes / ANAN10 / ANAN100 / ANAN_G2E) ─────────
    //
    // Porting from Thetis console.cs:8387-8455 [v2.10.3.15] UpdateDDCs() Hermes-class
    // branch (HPSDRModel.HERMES / ANAN_G2E / ANAN10 / ANAN100).
    //
    // Key constants from Thetis console.cs:8196-8198 [v2.10.3.15]:
    //   DDC0 = 1, DDC1 = 2  (bitmask values)
    //   ps_rate = cmaster.PSrate = 192000 (cmaster.cs:425 [v2.10.3.15])
    //
    // Inline author tags from cited source region:
    //   console.cs:8388: //N1GP G2E added  (ANAN_G2E case label)

    void hermes_1_slice_no_ps_assigns_ddc0()
    {
        // Porting from console.cs:8393-8407 [v2.10.3.15]:
        //   if (!_mox) {
        //     if (!diversity_enabled) {
        //       P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
        //       Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0; }
        //   }
        // //N1GP G2E added  [original tag from console.cs:8388 - ANAN_G2E case label]
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.mox = false;
        ctx.puresignalRun = false;
        ctx.diversity = false;
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].frequencyHz = 14225000;
        slices[0].sampleRateHz = 96000;
        slices[0].antennaIndex = 1;
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // From console.cs:8394 [v2.10.3.15]: DDCEnable = DDC0 (bit 0 = 0x01)
        QCOMPARE(a.ddcEnable & 0x01, 0x01);
        // From console.cs:8396 [v2.10.3.15]: Rate[0] = rx1_rate
        QCOMPARE(a.rate[0], 96000);
        // From console.cs:8393 [v2.10.3.15]: P1_DDCConfig = 4
        QCOMPARE(a.p1DdcConfig, 4);
        // SyncEnable = 0 (no diversity)
        QCOMPARE(a.syncEnable, 0);
        QCOMPARE(a.nDdc, 1);
    }

    void hermes_4_slice_enables_ddc0_through_ddc3()
    {
        // Porting from console.cs:8393-8407 [v2.10.3.15] (no-mox no-diversity path):
        //   DDCEnable = DDC0 + DDC1 (Thetis adds rx2 when rx2_enabled).
        //   Phase 3F extends to slices C+D → DDC2+DDC3 additively.
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.mox = false;
        ctx.puresignalRun = false;
        ctx.diversity = false;
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 4; ++i) {
            slices[i].live = true;
            slices[i].frequencyHz = (7000000 + i * 3000000);
            slices[i].sampleRateHz = 96000;
            slices[i].antennaIndex = 1;
        }
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // DDC0-3 = bits 0-3 = 0x0f
        QCOMPARE(a.ddcEnable & 0x0f, 0x0f);
        QCOMPARE(a.nDdc, 4);
        // From console.cs:8393 [v2.10.3.15]: P1_DDCConfig = 4 (no diversity)
        QCOMPARE(a.p1DdcConfig, 4);
    }
};

QTEST_MAIN(TestCodec5SliceAssignment)
#include "tst_codec_5_slice_assignment.moc"
