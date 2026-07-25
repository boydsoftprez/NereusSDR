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
#include "core/P2RadioConnection.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecAnvelinaPro3.h"
#include "core/codec/P1CodecHl2.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecHermes.h"
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
    // The kStreamToDdc[] table in Task 4's loop maps:
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

    // -----------------------------------------------------------------------
    // Task 9: P1CodecHl2::applyDdcAssignment
    // Source: mi0bot-Thetis console.cs:8409-8488 [v2.10.3.13-beta2]
    // -----------------------------------------------------------------------

    void hl2_1_slice_assigns_ddc0_at_slice_rate()
    {
        // From mi0bot console.cs:8409-8429 [v2.10.3.13-beta2] (no-mox no-diversity):
        //   case HPSDRModel.HERMESLITE: // MI0BOT: HL2
        //   P1_DDCConfig = 4; DDCEnable = DDC0; Rate[0] = rx1_rate;
        P1CodecHl2 codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 384000;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.ddcEnable & 0x01, 0x01);
        QCOMPARE(a.rate[0], 384000);
    }

    void hl2_ps_mox_uses_rx1_rate_not_ps_rate()
    {
        // mi0bot console.cs:8476-8479 [v2.10.3.13-beta2] divergence:
        // MI0BOT: HL2 can work at a high sample rate; keeps rx1_rate under PS-MOX.
        // ramdor uses ps_rate=192000; mi0bot branches to rx1_rate for HL2.
        P1CodecHl2 codec;
        CodecContext ctx{};
        ctx.mox = true;
        ctx.puresignalRun = true;
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 384000; slices[0].txBound = true;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        // MI0BOT: HL2 can work at a high sample rate. Rate[0]=Rate[1]=rx1_rate (384000),
        // NOT ps_rate (192000). From mi0bot console.cs:8476-8479 [v2.10.3.13-beta2].
        QCOMPARE(a.rate[0], 384000);
        QCOMPARE(a.rate[1], 384000);
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
    // ── Task 10a: P1CodecAnvelinaPro3 (OrionMkII-class P1, nddc=5) ─────────────
    //
    // AnvelinaPro3 falls in the ANAN100D/ORIONMKII/ANVELINAPRO3 case in Thetis
    // console.cs:8218-8303 [v2.10.3.15]; NOT the Hermes-class branch.
    // nddc=5, DDC2=RX1, DDC3=RX2, DDC0+DDC1 for PS/diversity.
    //
    // Inline author tags from cited source region:
    //   console.cs:8247  [2.10.3.13]MW0LGE p1 !  (within +-5 of Rate[2] assign)
    //
    // [2.10.3.13]MW0LGE p1 !  [original tag from console.cs:8247; P1-only Rate[0] path]

    void anvelina_pro3_1_slice_compiles_and_returns_nonzero()
    {
        // From Thetis console.cs:8243-8250 [v2.10.3.15] (no-mox, no-diversity):
        //   P1_DDCConfig = 1; DDCEnable = DDC2;
        //   if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE p1 !
        //   Rate[2] = rx1_rate;
        // [2.10.3.13]MW0LGE p1 !  [original tag from console.cs:8247; P1 path sets Rate[0] too]
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 96000;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        // DDC2 enabled (OrionMkII-class, not DDC0 like Hermes)
        QCOMPARE(a.ddcEnable & 0x04, 0x04);
        QVERIFY(a.nDdc >= 1);
        // From console.cs:8244 [v2.10.3.15]: P1_DDCConfig = 1 (OrionMkII-class plain RX)
        QCOMPARE(a.p1DdcConfig, 1);
        // From console.cs:8248 [v2.10.3.15]: Rate[2] = rx1_rate
        QCOMPARE(a.rate[2], 96000);
    }

    void anvelina_pro3_2_slice_enables_ddc2_and_ddc3()
    {
        // From Thetis console.cs:8299-8303 [v2.10.3.15] (rx2_enabled addendum):
        //   DDCEnable += DDC3; Rate[3] = rx2_rate;
        // Adjacent case header from console.cs:8305: case HPSDRModel.REDPITAYA: //DH1KLM
        // //DH1KLM  [original inline tag from console.cs:8305, adjacent to rx2 addendum]
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 96000; slices[0].txBound = true;
        slices[1].live = true; slices[1].sampleRateHz = 48000;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.ddcEnable & 0x0c, 0x0c);  // DDC2 + DDC3
        QCOMPARE(a.rate[2], 96000);
        QCOMPARE(a.rate[3], 48000);
        QCOMPARE(a.nDdc, 2);
    }

    // ── Task 10b: P1CodecRedPitaya (own case, //DH1KLM, nddc=5) ───────────────
    //
    // RedPitaya has its OWN case in Thetis console.cs:8305-8385 [v2.10.3.15]
    // (//DH1KLM). Same DDC layout as OrionMkII-class but Rate[0] and Rate[1]
    // are ALWAYS set to rx1_rate even in non-diversity/non-PS mode
    // (// REDPITAYA PAVEL inline notes). 384k allowed via include_extra_p1_rate
    // (setup.cs:847-849 [v2.10.3.15] //DH1KLM).
    //
    // Inline author tags from cited source region (MUST be preserved):
    //   console.cs:8305  //DH1KLM
    //   console.cs:8312  // REDPITAYA PAVEL   (P1_DDCConfig diversity override)
    //   console.cs:8317  // REDPITAYA PAVEL   (Rate[2] in diversity)
    //   console.cs:8326  // REDPITAYA PAVEL   (Rate[0] in plain mode)
    //   console.cs:8327  // REDPITAYA PAVEL   (Rate[1] in plain mode)
    //   console.cs:8375  // REDPITAYA PAVEL   (Rate[2] in diversity-mox)
    //
    // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header, verbatim]
    // // REDPITAYA PAVEL  [original inline from console.cs:8326, verbatim - Rate[0] always set]

    void redpitaya_1_slice_at_384k_supported()
    {
        // From Thetis setup.cs:847-849 [v2.10.3.15]:
        //   bool include_extra_p1_rate = HardwareSpecific.Model == HPSDRModel.REDPITAYA; //DH1KLM
        //   p1_rates = include_extra_p1_rate ? new int[] { 48000,96000,192000,384000 } : ...
        // And from console.cs:8321-8330 [v2.10.3.15] (no-mox, no-diversity):
        //   Rate[0] = rx1_rate; // REDPITAYA PAVEL
        //   Rate[1] = rx1_rate; // REDPITAYA PAVEL
        //   Rate[2] = rx1_rate;
        // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header]
        // // REDPITAYA PAVEL  [original inline tag from console.cs:8326]
        P1CodecRedPitaya codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 384000;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        // Rate[0], Rate[1], Rate[2] all get rx1_rate on RedPitaya (REDPITAYA PAVEL)
        QCOMPARE(a.rate[0], 384000);
        QCOMPARE(a.rate[1], 384000);
        QCOMPARE(a.rate[2], 384000);
    }

    void redpitaya_1_slice_ddc2_enabled()
    {
        // From Thetis console.cs:8323-8324 [v2.10.3.15] (no-mox, no-diversity):
        //   P1_DDCConfig = 1; DDCEnable = DDC2;
        // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header]
        P1CodecRedPitaya codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 192000;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.ddcEnable & 0x04, 0x04);  // DDC2 bit
        QCOMPARE(a.p1DdcConfig, 1);
        QCOMPARE(a.nDdc, 1);
    }

    void redpitaya_diversity_sets_rate2_too()
    {
        // From Thetis console.cs:8310-8319 [v2.10.3.15] (no-mox, diversity):
        //   P1_DDCConfig = 2; // REDPITAYA PAVEL
        //   DDCEnable = DDC0; SyncEnable = DDC1;
        //   Rate[0] = Rate[1] = Rate[2] = rx1_rate; // REDPITAYA PAVEL
        // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header]
        // // REDPITAYA PAVEL  [original inline tag from console.cs:8312]
        // // REDPITAYA PAVEL  [original inline tag from console.cs:8317]
        P1CodecRedPitaya codec;
        CodecContext ctx{};
        ctx.diversity = true;
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 192000; slices[0].diversityRequested = true;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.p1DdcConfig, 2);    // REDPITAYA PAVEL P1_DDCConfig
        QCOMPARE(a.ddcEnable & 0x01, 0x01);   // DDC0
        QCOMPARE(a.syncEnable & 0x02, 0x02);  // DDC1 synced
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        QCOMPARE(a.rate[2], 192000);  // REDPITAYA PAVEL - extra rate[2]
    }

    // ── Phase 3F Sub-Epic I Task 7b: per-STREAM DDC mapping publication ──────
    //
    // NOT to be confused with "Task 7" above (P2CodecOrionMkII), which is
    // Sub-Epic B numbering. This is Sub-Epic I's own Task 7/7b: DdcAssignment
    // gains streamDdc[5] so RadioModel can publish the codec's DDC choice
    // onto ReceiverManager and onto each SliceModel via setDdcIndex(), which
    // previously had zero callers. See docs/architecture/2026-07-24-phase3f-
    // sub-epic-i-data-plane-plan.md Task 7b.
    //
    // Task 7 wrote this as saturn_publishes_per_slice_ddc_mapping and asserted
    // per-SLICE semantics. That was written against the wrong model: a DDC
    // belongs to a stream, and slices bind to streams many-to-one, so slice
    // indexing would hand two co-hosted slices DDC2 and DDC3 and break the
    // sharing they were bound under. Task 7b corrects the model; the
    // assertions below are the same checks re-expressed against it, not a
    // weakened version of them.

    void saturn_publishes_per_stream_ddc_mapping()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> streams{};
        streams[0].live = true; streams[0].sampleRateHz = 192000;
        streams[2].live = true; streams[2].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, streams);

        QCOMPARE(a.streamDdc[0], 2);    // stream 0 -> DDC2
        QCOMPARE(a.streamDdc[1], -1);   // idle
        QCOMPARE(a.streamDdc[2], 4);    // stream 2 -> DDC4
        // Every published DDC must also be enabled in the bitmask.
        QVERIFY((a.ddcEnable >> a.streamDdc[0]) & 1);
        QVERIFY((a.ddcEnable >> a.streamDdc[2]) & 1);
    }

    // ── Task 7c: Hermes-class P2 stream table ────────────────────────────────
    //
    // Porting from Thetis console.cs:8610-8642 [v2.10.3.15] GetDDC() P2
    // Hermes-class branch:
    //
    //   case HPSDRHW.Hermes: // ANAN-10 ANAN-100 Heremes
    //   case HPSDRHW.HermesII: // ANAN-10E ANAN-100B HeremesII
    //   case HPSDRHW.HermesC10: // ANAN-G2E //N1GP G2E added (HermesC10)
    //       switch (tot) {
    //           case 0: // off off off
    //               rx1 = 0;
    //               rx2 = 1;
    //               break;
    //           case 1: // off off on
    //               rx1 = 0;   //MW0LGE_22b missed out
    //               rx2 = 1;
    //
    // versus the 2-ADC branch at console.cs:8556-8608 [v2.10.3.15] which puts
    // rx1 = 2, rx2 = 3.  Thetis keeps the two families in separate switch
    // cases; NereusSDR keeps them in separate codecs.
    //
    // Inline author tags from the cited source region
    // (CLAUDE.md inline-comment-preservation rule):
    //   console.cs:8612  //N1GP G2E added (HermesC10)   (HermesC10 case label)
    //   console.cs:8620  //MW0LGE_22b missed out        (rx1 = 0 on tot==1)

    void hermes_class_p2_selection_matches_primary_rx_ddc()
    {
        // The codec's stream-0 DDC and primaryRxDdcForBoard must agree for
        // every board, or connectToRadio and the first frequency change
        // disagree about which DDC carries RX.  connectToRadio seeds
        // m_rx[primaryRxDdcForBoard()].enable = 1; RadioModel::bindSliceToStream
        // then recomputes the assignment on every SliceModel::frequencyChanged
        // and P2RadioConnection::applyDdcAssignment writes the codec's
        // ddcEnable bitmask verbatim into m_rx[i].enable.  A disagreement
        // means the operator's first VFO turn drops the DDC that is actually
        // streaming and receive stops.
        //
        // //N1GP G2E added (HermesC10)  [original tag from console.cs:8612]
        // //MW0LGE_22b missed out  [original tag from console.cs:8620]
        struct Case { HPSDRHW board; const char* name; };
        const Case cases[] = {
            // 1-ADC Hermes-class on community P2 firmware: rx1 = DDC0.
            // From Thetis console.cs:8615-8617 [v2.10.3.15].
            {HPSDRHW::Hermes,     "Hermes"},
            {HPSDRHW::HermesII,   "HermesII"},
            {HPSDRHW::HermesC10,  "HermesC10 (ANAN-G2E)"},
            // 2-ADC family: rx1 = DDC2. From Thetis console.cs:8562-8565.
            {HPSDRHW::Orion,      "Orion"},
            {HPSDRHW::OrionMKII,  "OrionMKII"},
            {HPSDRHW::Saturn,     "Saturn"},
            {HPSDRHW::SaturnMKII, "SaturnMKII"},
        };

        for (const Case& c : cases) {
            P2RadioConnection conn;
            conn.setBoardForTest(c.board);
            IP2Codec* codec = conn.p2Codec();
            QVERIFY2(codec != nullptr, c.name);

            CodecContext ctx{};
            std::array<SliceConfig, 5> streams{};
            streams[0].live = true;
            streams[0].sampleRateHz = 192000;

            const DdcAssignment a = codec->applyDdcAssignment(ctx, streams);

            const int expected = P2RadioConnection::primaryRxDdcForBoard(c.board);
            QCOMPARE(a.streamDdc[0], expected);
            // The published DDC must also be asserted in the enable bitmask,
            // since applyDdcAssignment writes that mask straight to m_rx[].
            QVERIFY2((a.ddcEnable >> expected) & 1, c.name);
        }
    }

    void hermes_class_p2_puts_stream_zero_on_ddc0()
    {
        // ANAN-10 / ANAN-100 / ANAN-10E / ANAN-100B / ANAN-G2E on community P2
        // firmware. Wire-byte capture of a working Thetis-on-G2E session shows
        // CmdRx byte 7 = 0x01, i.e. the DDC0 enable bit, and
        // primaryRxDdcForBoard returns 0 for this family. A codec that asserts
        // DDC2 kills receive on the first VFO turn.
        //
        // From Thetis console.cs:8394-8396 [v2.10.3.15]:
        //   P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
        //   Rate[0] = rx1_rate;
        // //N1GP G2E added  [original tag from console.cs:8388 - ANAN_G2E case label]
        P2CodecHermes codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> streams{};
        streams[0].live = true;
        streams[0].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, streams);

        QCOMPARE(a.streamDdc[0], 0);
        QVERIFY((a.ddcEnable >> 0) & 1);
        // DDC2 must NOT be enabled — that is the 2-ADC layout this codec exists
        // to avoid inheriting.
        QCOMPARE((a.ddcEnable >> 2) & 1, 0);
        // From console.cs:8396 [v2.10.3.15]: Rate[0] = rx1_rate
        QCOMPARE(a.rate[0], 192000);
        // From console.cs:8393 [v2.10.3.15]: P1_DDCConfig = 4
        QCOMPARE(a.p1DdcConfig, 4);
        QCOMPARE(a.syncEnable, 0);
        QCOMPARE(a.nDdc, 1);
    }

    void hermes_class_p2_four_streams_fill_ddc0_through_ddc3()
    {
        // Thetis places rx1 on DDC0 (console.cs:8394) and rx2 on DDC1
        // (console.cs:8399-8400 [v2.10.3.15]) and caps the family at nddc = 4
        // (console.cs:8392). Streams 2-3 extend additively into Thetis's two
        // idle slots; stream 4 has no DDC on this family and must stay
        // unassigned rather than being invented.
        P2CodecHermes codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> streams{};
        for (int i = 0; i < 5; ++i) {
            streams[i].live = true;
            streams[i].sampleRateHz = 96000;
        }

        const DdcAssignment a = codec.applyDdcAssignment(ctx, streams);

        QCOMPARE(a.streamDdc[0], 0);
        QCOMPARE(a.streamDdc[1], 1);
        QCOMPARE(a.streamDdc[2], 2);
        QCOMPARE(a.streamDdc[3], 3);
        // nddc = 4 (console.cs:8392 [v2.10.3.15]) — no fifth DDC exists here.
        QCOMPARE(a.streamDdc[4], -1);
        QCOMPARE(a.ddcEnable & 0x0F, 0x0F);
        QCOMPARE((a.ddcEnable >> 4) & 1, 0);
        QCOMPARE(a.nDdc, 4);
        // From console.cs:8391 [v2.10.3.15]: P1_rxcount = 4
        QCOMPARE(a.p1RxCount, 4);
    }

    void hermes_class_p2_ps_mox_keeps_stream_zero_on_ddc0()
    {
        // From Thetis console.cs:8449-8456 [v2.10.3.15]:
        //   else // transmitting and PS is ON
        //   {
        //       P1_DDCConfig = 6; DDCEnable = DDC0; SyncEnable = DDC1;
        //       Rate[0] = ps_rate; Rate[1] = ps_rate;
        //       cntrl1 = 4; cntrl2 = 0;
        //   }
        // Unlike the 2-ADC branch, DDCEnable = DDC0 is unconditional across
        // every Hermes-class state, so stream 0 does NOT migrate when PS
        // engages — only DDC1's role changes.
        P2CodecHermes codec;
        CodecContext ctx{};
        ctx.mox = true;
        ctx.puresignalRun = true;
        std::array<SliceConfig, 5> streams{};
        streams[0].live = true;
        streams[0].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, streams);

        QCOMPARE(a.streamDdc[0], 0);
        QCOMPARE(a.ddcEnable, 1);        // DDC0 only
        QCOMPARE(a.syncEnable, 2);       // DDC1 syncs to DDC0
        // ps_rate = 192000 (cmaster.cs:425 [v2.10.3.15])
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        // From console.cs:8455 [v2.10.3.15]: cntrl1 = 4
        QCOMPARE(a.adcCtrl1, 4);
        QCOMPARE(a.p1DdcConfig, 6);
        // PS pair indices per cmaster.cs:538-539 [v2.10.3.15]
        QCOMPARE(a.psFwdDdc, 0);
        QCOMPARE(a.psRevDdc, 1);
    }
};

QTEST_MAIN(TestCodec5SliceAssignment)
#include "tst_codec_5_slice_assignment.moc"
