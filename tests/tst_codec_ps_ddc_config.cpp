// no-port-check: test fixture cites Thetis source for expected values, not a port itself
//
// Phase 3M-4 Task 5: PsDdcConfig per-board codec test.
//
// Verifies that each codec subclass returns the wire-byte map matching
// its per-HpsdrModel branch in Thetis console.cs:8186-8538 [v2.10.3.13]
// (and mi0bot console.cs:8409-8488 [v2.10.3.13-beta2] for HL2).
//
// Test taxonomy (one row per branch in Thetis switch):
//
//   G2-class (P2CodecOrionMkII, P2CodecSaturn, P1CodecAnvelinaPro3,
//             P1CodecStandard for ANAN100D/200D/ORIONMKII/...)
//      - PS-on,  no-divers, MOX     -> p1DdcConfig=3, DDCEnable=DDC0+DDC2
//      - PS-on,  with-divers, MOX   -> same as above
//      - PS-off, MOX                -> p1DdcConfig=1, DDCEnable=DDC2
//      - PS-on,  no MOX             -> p1DdcConfig=1 (RX-only)
//      - RX2 enabled adds DDC3
//
//   HL2 (P1CodecHl2)
//      - PS-on,  MOX                -> p1DdcConfig=6, cntrl1=4, rate=rx1Rate
//      - PS-off, MOX                -> p1DdcConfig=4
//      - RX2 enabled adds DDC1
//
//   HermesII-class (P1CodecStandard for ANAN10E/100B)
//      - PS-on, MOX                 -> p1DdcConfig=5, cntrl1=4
//
//   RedPitaya (P1CodecRedPitaya)
//      - PS-on, MOX                 -> p1DdcConfig=3, includes Rate[1]
//
//   No-PS sentinels
//      - HPSDR (Atlas) → P1CodecStandard returns all zeros

#include <QtTest/QtTest>

#include "core/codec/CodecContext.h"
#include "core/codec/IP1Codec.h"
#include "core/codec/IP2Codec.h"
#include "core/codec/P1CodecAnvelinaPro3.h"
#include "core/codec/P1CodecHl2.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "core/codec/P2CodecSaturn.h"

using namespace NereusSDR;

namespace {

// Bit constants from Thetis console.cs:8190 [v2.10.3.13]:
//   int DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
constexpr uint8_t DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;

// From Thetis cmaster.cs:424 [v2.10.3.13]: private static int ps_rate = 192000;
constexpr int kPsRate = 192000;

} // namespace

class TestCodecPsDdcConfig : public QObject {
    Q_OBJECT
private slots:
    // ====================================================================
    // P2CodecOrionMkII — G2-class branch (covers ANAN-100D / 200D / ORIONMKII /
    // 7000D / 8000D / G2 / G2-1K / ANVELINAPRO3 in P2)
    //
    // Thetis source: console.cs:8211-8295 [v2.10.3.13]
    // ====================================================================

    // PS-on, no diversity, MOX → DDC0+DDC2, sync DDC1, ps_rate
    // Source: console.cs:8256-8266 [v2.10.3.13]
    // Inline tag preservation (CLAUDE.md §"Inline comment preservation"):
    //   console.cs:8251 — // [2.10.3.13]MW0LGE p1 !
    void p2_orionmkii_psOn_noDivers_mox() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN8000D,
            /*psEnabled=*/true, /*diversityEnabled=*/false, /*moxState=*/true,
            /*rx1Rate=*/48000, /*rx2Rate=*/0, /*rx2Enabled=*/false,
            /*adcCtrl1=*/0x00, /*adcCtrl2=*/0x00);

        QCOMPARE(int(cfg.p1DdcConfig), 3);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0 + DDC2));   // 0x05
        QCOMPARE(int(cfg.syncEnable), int(DDC1));          // 0x02
        QCOMPARE(int(cfg.rate[0]), kPsRate);
        QCOMPARE(int(cfg.rate[1]), kPsRate);
        QCOMPARE(int(cfg.rate[2]), 48000);
        // (adcCtrl1=0 & 0xf3) | 0x08 = 0x08
        QCOMPARE(int(cfg.cntrl1),  0x08);
        QCOMPARE(int(cfg.cntrl2),  0x00);
        QCOMPARE(cfg.p1RxCount, 5);
        QCOMPARE(cfg.nDdc,      5);
    }

    // PS-on, no diversity, MOX, with non-zero adcCtrl1 — verify mask preserved
    // Source: console.cs:8264 [v2.10.3.13]: cntrl1 = (rx_adc_ctrl1 & 0xf3) | 0x08
    // Upstream tags preserved: //MW0LGE (from cited console.cs:8260) [v2.10.3.15]
    void p2_orionmkii_psOn_mox_adcCtrl1Mask() {
        P2CodecOrionMkII codec;
        // adcCtrl1 = 0xff → (0xff & 0xf3) | 0x08 = 0xfb
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN_G2,
            true, false, true, 48000, 0, false, 0xff, 0xff);
        QCOMPARE(int(cfg.cntrl1),  0xfb);
        QCOMPARE(int(cfg.cntrl2),  0x3f); // 0xff & 0x3f
    }

    // PS-on, with diversity, MOX → same as no-diversity case
    // Source: console.cs:8267-8277 [v2.10.3.13]
    void p2_orionmkii_psOn_divers_mox() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN_G2,
            true, true, true, 96000, 0, false, 0x00, 0x00);
        QCOMPARE(int(cfg.p1DdcConfig), 3);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0 + DDC2));
        QCOMPARE(int(cfg.syncEnable), int(DDC1));
        QCOMPARE(int(cfg.rate[0]), kPsRate);
        QCOMPARE(int(cfg.rate[1]), kPsRate);
        QCOMPARE(int(cfg.rate[2]), 96000);
    }

    // PS-off, no diversity, MOX → DDC2, no sync, just RX1 rate
    // Source: console.cs:8246-8255 [v2.10.3.13]
    void p2_orionmkii_psOff_mox() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN8000D,
            false, false, true, 48000, 0, false, 0x00, 0x00);
        QCOMPARE(int(cfg.p1DdcConfig), 1);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC2));    // 0x04
        QCOMPARE(int(cfg.syncEnable), 0);
        // P2 path doesn't set Rate[0] (only P1) — see [2.10.3.13]MW0LGE p1 !
        QCOMPARE(int(cfg.rate[0]), 0);
        QCOMPARE(int(cfg.rate[2]), 48000);
        QCOMPARE(int(cfg.cntrl1),  0x00);
    }

    // PS-on, no MOX → DDC2 RX-only branch (cal armed but TX off)
    // Source: console.cs:8233-8242 [v2.10.3.13]
    void p2_orionmkii_psOn_noMox_rxOnly() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ORIONMKII,
            true, false, false, 48000, 0, false, 0x00, 0x00);
        QCOMPARE(int(cfg.p1DdcConfig), 1);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC2));
        QCOMPARE(int(cfg.syncEnable), 0);
    }

    // PS-on, MOX, RX2 enabled → DDC3 added
    // Source: console.cs:8290-8294 [v2.10.3.13]
    // Inline tag preservation (CLAUDE.md §"Inline comment preservation"):
    //   console.cs:8296 — case HPSDRModel.REDPITAYA: //DH1KLM (next case)
    void p2_orionmkii_psOn_mox_rx2Enabled_addsDdc3() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN_G2,
            true, false, true, 48000, 192000, true, 0x00, 0x00);
        // DDC0+DDC2+DDC3 = 1 + 4 + 8 = 0x0d
        QCOMPARE(int(cfg.ddcEnable), int(DDC0 + DDC2 + DDC3));
        QCOMPARE(int(cfg.rate[3]), 192000);
    }

    // ====================================================================
    // P2CodecSaturn — INHERITED from OrionMkII (G2/G2-1K share same case)
    // ====================================================================

    // Saturn delegates to OrionMkII's PS DDC config — verify identical output
    // Source: console.cs:8211-8295 [v2.10.3.13] groups ANAN_G2 with G2-class
    // Inline tag preservation (CLAUDE.md §"Inline comment preservation") for
    // tags within ±5 lines of the cited range:
    //   console.cs:8238 — // [2.10.3.13]MW0LGE p1 !  (Hermes-class branch)
    //   console.cs:8251 — // [2.10.3.13]MW0LGE p1 !  (G2-class branch)
    //   console.cs:8296 — case HPSDRModel.REDPITAYA: //DH1KLM (next case)
    void p2_saturn_inheritsG2Branch() {
        P2CodecSaturn codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN_G2,
            true, false, true, 48000, 0, false, 0x00, 0x00);
        QCOMPARE(int(cfg.p1DdcConfig), 3);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0 + DDC2));
        QCOMPARE(int(cfg.cntrl1),  0x08);
    }

    // ====================================================================
    // P2CodecOrionMkII — Hermes-class branch (HERMES / ANAN10 / ANAN100 on P2)
    //
    // Thetis source: console.cs:8378-8449 [v2.10.3.13]
    //
    // 1-ADC family running community P2 firmware. Primary RX = DDC0 (not
    // DDC2 like G2-class).  Issue #263 fix.
    // ====================================================================

    // Hermes PS-off, no MOX, no diversity → p1DdcConfig=4, DDCEnable=DDC0
    // Source: console.cs:8385-8398 [v2.10.3.13]
    // Upstream tags preserved: //N1GP (from cited console.cs:8388) [v2.10.3.15]
    void p2_hermes_psOff_noMox_noDivers() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMES,
            false, false, false, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 4);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));   // 0x01
        QCOMPARE(int(cfg.syncEnable), 0);
        QCOMPARE(int(cfg.rate[0]), 48000);
        QCOMPARE(int(cfg.cntrl1),  0);
        QCOMPARE(cfg.p1RxCount, 4);
        QCOMPARE(cfg.nDdc,      4);
    }

    // ANAN10 PS-off, no MOX, RX2 enabled → DDC0+DDC1
    // Source: console.cs:8394-8398 [v2.10.3.13]
    void p2_hermes_anan10_rx2Enabled_addsDdc1() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN10,
            false, false, false, 192000, 96000, true, 0, 0);
        QCOMPARE(int(cfg.ddcEnable), int(DDC0 + DDC1));   // 0x03
        QCOMPARE(int(cfg.rate[0]), 192000);
        QCOMPARE(int(cfg.rate[1]), 96000);
    }

    // ANAN100 PS-on MOX → p1DdcConfig=6, DDC0+sync DDC1, cntrl1=4
    // Source: console.cs:8438-8447 [v2.10.3.13]
    void p2_hermes_anan100_psOn_mox() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN100,
            true, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 6);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.syncEnable), int(DDC1));
        QCOMPARE(int(cfg.rate[0]), kPsRate);
        QCOMPARE(int(cfg.rate[1]), kPsRate);
        QCOMPARE(int(cfg.cntrl1),  4);
        QCOMPARE(int(cfg.psFbDdc), 0);
        QCOMPARE(int(cfg.txMonDdc), 1);
    }

    // Hermes diversity, no MOX → DDC0+sync DDC1, same rate
    // Source: console.cs:8400-8409 [v2.10.3.13]
    void p2_hermes_diversity_noMox() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMES,
            false, true, false, 96000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 5);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.syncEnable), int(DDC1));
        QCOMPARE(int(cfg.rate[0]), 96000);
        QCOMPARE(int(cfg.rate[1]), 96000);
    }

    // ====================================================================
    // P2CodecOrionMkII — HermesII-class branch (ANAN10E / ANAN100B on P2)
    //
    // Thetis source: console.cs:8451-8521 [v2.10.3.13]
    //
    // 1-ADC family with nddc=2.  This is the issue #263 regression case:
    // ANAN-10E running community P2 firmware was previously routed through
    // the G2-class branch and never received I/Q because the G2 branch
    // selects DDC2 as primary.
    // ====================================================================

    // ANAN10E PS-off, no MOX, no diversity → p1DdcConfig=4, DDCEnable=DDC0
    // Source: console.cs:8457-8470 [v2.10.3.13]
    void p2_hermesII_anan10e_psOff_noMox_noDivers_DDC0() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN10E,
            false, false, false, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 4);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));   // 0x01 — NOT DDC2 (0x04)
        QCOMPARE(int(cfg.syncEnable), 0);
        QCOMPARE(int(cfg.rate[0]), 48000);
        QCOMPARE(int(cfg.rate[2]), 0);              // G2 branch would set rate[2]
        QCOMPARE(int(cfg.cntrl1),  0);
        QCOMPARE(cfg.p1RxCount, 2);
        QCOMPARE(cfg.nDdc,      2);
    }

    // ANAN100B PS-off, no MOX, RX2 enabled → DDC0+DDC1
    // Source: console.cs:8466-8470 [v2.10.3.13]
    void p2_hermesII_anan100b_rx2Enabled() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN100B,
            false, false, false, 192000, 192000, true, 0, 0);
        QCOMPARE(int(cfg.ddcEnable), int(DDC0 + DDC1));   // 0x03
        QCOMPARE(int(cfg.rate[0]), 192000);
        QCOMPARE(int(cfg.rate[1]), 192000);
    }

    // ANAN10E PS-on MOX → p1DdcConfig=5 (NOT 6 like Hermes), DDC0+sync DDC1
    // Source: console.cs:8510-8519 [v2.10.3.13]
    void p2_hermesII_anan10e_psOn_mox() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN10E,
            true, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 5);   // HermesII uses 5 here; Hermes uses 6
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.syncEnable), int(DDC1));
        QCOMPARE(int(cfg.rate[0]), kPsRate);
        QCOMPARE(int(cfg.rate[1]), kPsRate);
        QCOMPARE(int(cfg.cntrl1),  4);
        QCOMPARE(int(cfg.psFbDdc), 0);
        QCOMPARE(int(cfg.txMonDdc), 1);
    }

    // ANAN10E diversity, no MOX → DDC0+sync DDC1
    // Source: console.cs:8472-8481 [v2.10.3.13]
    void p2_hermesII_anan10e_diversity_noMox() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN10E,
            false, true, false, 96000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 5);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.syncEnable), int(DDC1));
        QCOMPARE(int(cfg.rate[0]), 96000);
        QCOMPARE(int(cfg.rate[1]), 96000);
    }

    // Regression marker for issue #263: PS-off MOX must use DDC0, not DDC2.
    // Before the fix, every P2 board fell through the G2-class branch and
    // emitted DDCEnable=DDC2.  This is the byte-level assertion that the
    // dispatcher routes ANAN10E to the HermesII-class branch.
    void p2_hermesII_psOff_mox_NOT_DDC2_issue263() {
        P2CodecOrionMkII codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN10E,
            false, false, true, 192000, 0, false, 0, 0);
        QVERIFY2((cfg.ddcEnable & DDC2) == 0,
                 "issue #263: HermesII on P2 must NOT enable DDC2");
        QCOMPARE(int(cfg.ddcEnable), int(DDC0));
        QCOMPARE(int(cfg.rate[0]), 192000);
        QCOMPARE(int(cfg.rate[2]), 0);
    }

    // ====================================================================
    // P1CodecHl2 — HL2 branch (mi0bot deltas: rx1_rate not ps_rate)
    //
    // Thetis source: mi0bot console.cs:8408-8490 [v2.10.3.13-beta2]
    // ====================================================================

    // HL2 PS-on MOX → p1DdcConfig=6, cntrl1=4, rate=rx1_rate (not ps_rate)
    // Source: mi0bot console.cs:8469-8488 [v2.10.3.13-beta2]
    void p1_hl2_psOn_mox() {
        P1CodecHl2 codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            true, false, true, 48000, 0, false, 0x00, 0x00);
        QCOMPARE(int(cfg.p1DdcConfig), 6);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));    // 0x01
        QCOMPARE(int(cfg.syncEnable), int(DDC1));    // 0x02
        // MI0BOT: HL2 can work at a high sample rate — rate[0]=rate[1]=rx1_rate
        QCOMPARE(int(cfg.rate[0]), 48000);
        QCOMPARE(int(cfg.rate[1]), 48000);
        QCOMPARE(int(cfg.cntrl1),  4);
        QCOMPARE(int(cfg.cntrl2),  0);
        QCOMPARE(cfg.p1RxCount, 4);
        QCOMPARE(cfg.nDdc,      4);
    }

    // HL2 PS-on MOX at 192k rate → confirm rate[0]=rx1_rate (not ps_rate=192000)
    // Source: mi0bot console.cs:8476-8485 [v2.10.3.13-beta2]
    //   if (hpsdr_model == HPSDRModel.HERMESLITE) // MI0BOT: HL2 can work at a high sample rate
    //   { Rate[0] = rx1_rate; Rate[1] = rx1_rate; }
    void p1_hl2_psOn_mox_192k() {
        P1CodecHl2 codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            true, false, true, 192000, 0, false, 0, 0);
        // For HL2, rate is rx1_rate — coincidentally 192000 here, but the
        // logic is "use rx1Rate" not "use ps_rate".  This case verifies
        // the right path was taken.
        QCOMPARE(int(cfg.rate[0]), 192000);
        QCOMPARE(int(cfg.rate[1]), 192000);
    }

    // HL2 PS-on MOX at 96k rate (different from ps_rate=192000) — definitive
    // proof that HL2 uses rx1_rate, not ps_rate
    void p1_hl2_psOn_mox_96k_provesUsesRx1Rate() {
        P1CodecHl2 codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            true, false, true, 96000, 0, false, 0, 0);
        QCOMPARE(int(cfg.rate[0]), 96000);   // would be 192000 if using ps_rate
        QCOMPARE(int(cfg.rate[1]), 96000);
    }

    // HL2 PS-off MOX → p1DdcConfig=4, cntrl1=0
    // Source: mi0bot console.cs:8444-8457 [v2.10.3.13-beta2]
    void p1_hl2_psOff_mox() {
        P1CodecHl2 codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            false, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 4);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.syncEnable), 0);
        QCOMPARE(int(cfg.rate[0]), 48000);
        QCOMPARE(int(cfg.cntrl1),  0);
    }

    // HL2 PS-off MOX, RX2 enabled → DDC1 added
    // Source: mi0bot console.cs:8453-8457 [v2.10.3.13-beta2]
    void p1_hl2_psOff_mox_rx2Enabled() {
        P1CodecHl2 codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            false, false, true, 48000, 96000, true, 0, 0);
        QCOMPARE(int(cfg.ddcEnable), int(DDC0 + DDC1));
        QCOMPARE(int(cfg.rate[1]), 96000);
    }

    // HL2 no MOX no diversity no PS → p1DdcConfig=4
    // Source: mi0bot console.cs:8416-8429 [v2.10.3.13-beta2]
    void p1_hl2_noMox() {
        P1CodecHl2 codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            false, false, false, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 4);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.cntrl1),  0);
    }

    // ====================================================================
    // P1CodecStandard — HERMES-class branch
    //
    // Thetis source: console.cs:8378-8449 [v2.10.3.13]
    // ====================================================================

    // ANAN100 PS-on MOX → p1DdcConfig=6, cntrl1=4, rate=ps_rate
    // Source: console.cs:8438-8447 [v2.10.3.13]
    void p1_standard_anan100_psOn_mox() {
        P1CodecStandard codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN100,
            true, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 6);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.syncEnable), int(DDC1));
        // Hermes-class uses ps_rate, NOT rx1_rate (only HL2 uses rx1_rate)
        QCOMPARE(int(cfg.rate[0]), kPsRate);
        QCOMPARE(int(cfg.rate[1]), kPsRate);
        QCOMPARE(int(cfg.cntrl1),  4);
        QCOMPARE(cfg.p1RxCount, 4);
        QCOMPARE(cfg.nDdc,      4);
    }

    // HERMES PS-off MOX → p1DdcConfig=4
    // Source: console.cs:8413-8426 [v2.10.3.13]
    void p1_standard_hermes_psOff_mox() {
        P1CodecStandard codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMES,
            false, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 4);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.cntrl1),  0);
    }

    // ANAN10 no MOX → p1DdcConfig=4
    // Source: console.cs:8385-8398 [v2.10.3.13]
    // Upstream tags preserved: //N1GP (from cited console.cs:8388) [v2.10.3.15]
    void p1_standard_anan10_noMox() {
        P1CodecStandard codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN10,
            false, false, false, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 4);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
    }

    // ====================================================================
    // P1CodecStandard — HermesII-class branch
    //
    // Thetis source: console.cs:8451-8521 [v2.10.3.13]
    // ====================================================================

    // ANAN10E PS-on MOX → p1DdcConfig=5 (NOT 6 as in Hermes), cntrl1=0
    // Source: console.cs:8510-8519 [v2.10.3.13]
    void p1_standard_anan10e_psOn_mox() {
        P1CodecStandard codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN10E,
            true, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 5);   // Note: 5 not 6
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
        QCOMPARE(int(cfg.syncEnable), int(DDC1));
        QCOMPARE(int(cfg.rate[0]), kPsRate);
        QCOMPARE(int(cfg.rate[1]), kPsRate);
        // Source-faithful per Thetis console.cs:8517 [v2.10.3.13]: cntrl1=4.
        //
        // Background: from 2026-05-09 until 2026-05-17 this branch carried
        // an empirical override forcing cfg.cntrl1=0 because working
        // Thetis on a friend's ANAN-10E (HermesII) was observed sending
        // 0 on bank-4 C1 during PS-MOX. The override was patching the
        // wrong layer — NereusSDR P1 codecs used to read cfg.cntrl1 as
        // the bank-4 wire byte, which conflated UpdateDDCs's cntrl1 with
        // the separate Thetis P1_adc_cntrl global. After the 2026-05-17
        // port-fidelity refactor the P1 wire byte comes from
        // P1RadioConnection::m_p1AdcCntrl (mirror of Thetis P1_adc_cntrl),
        // which defaults to 0 for Hermes/HermesII via applyBoardQuirks().
        // The cfg.cntrl1 value here is now P2-side state only (maps to
        // prn->rx[i].rx_adc via SetADC_cntrl1) and no longer reaches the
        // P1 wire — so the source value 4 is correct and the previous
        // empirical override is obsolete.
        QCOMPARE(int(cfg.cntrl1),  4);
        QCOMPARE(cfg.p1RxCount, 2);          // 2 not 4
        QCOMPARE(cfg.nDdc,      2);
    }

    // ANAN100B PS-off MOX → p1DdcConfig=4
    // Source: console.cs:8485-8498 [v2.10.3.13]
    void p1_standard_anan100b_psOff_mox() {
        P1CodecStandard codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN100B,
            false, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 4);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0));
    }

    // ====================================================================
    // P1CodecStandard — G2-class via Standard codec
    // (some boards may use Standard codec because P1RadioConnection's codec
    //  selector default-routes to it for any non-HL2/non-AnvelinaPro3/non-RedPitaya)
    //
    // Thetis source: console.cs:8211-8295 [v2.10.3.13]
    // ====================================================================

    // ANAN_G2 routed through Standard → G2-class branch
    // Source: console.cs:8216 [v2.10.3.13]
    void p1_standard_anan_g2_routesToG2Branch() {
        P1CodecStandard codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN_G2,
            true, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 3);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0 + DDC2));
        QCOMPARE(int(cfg.cntrl1),  0x08);
    }

    // ====================================================================
    // P1CodecStandard — sentinel: HPSDR (Atlas) returns all zeros
    //
    // Thetis source: console.cs:8523-8524 [v2.10.3.13]
    //   case HPSDRModel.HPSDR:
    //       break;       // no DDC config emitted
    // ====================================================================

    void p1_standard_hpsdr_returnsZeros() {
        P1CodecStandard codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HPSDR,
            true, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 0);
        QCOMPARE(int(cfg.ddcEnable),  0);
        QCOMPARE(int(cfg.cntrl1),  0);
        QCOMPARE(int(cfg.cntrl2),  0);
        QCOMPARE(cfg.p1RxCount, 0);
        QCOMPARE(cfg.nDdc,      0);
    }

    // ====================================================================
    // P1CodecAnvelinaPro3 — INHERITED from Standard, dispatches to G2-class
    //
    // Thetis source: console.cs:8218 [v2.10.3.13]
    //   case HPSDRModel.ANVELINAPRO3:        (in G2-class case)
    // ====================================================================

    void p1_anvelinaPro3_dispatchesToG2Branch() {
        P1CodecAnvelinaPro3 codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANVELINAPRO3,
            true, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 3);            // G2-class p1DdcConfig
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0 + DDC2));
        QCOMPARE(int(cfg.syncEnable), int(DDC1));
        QCOMPARE(int(cfg.cntrl1),  0x08);             // (0 & 0xf3) | 0x08
        QCOMPARE(cfg.p1RxCount, 5);
    }

    // ====================================================================
    // P1CodecRedPitaya — own branch with Rate[1]=rx1_rate in PS-off cases
    //
    // Thetis source: console.cs:8296-8377 [v2.10.3.13]
    // ====================================================================

    // RedPitaya PS-on MOX → p1DdcConfig=3, ps_rate (same as G2)
    // Source: console.cs:8337-8347 [v2.10.3.13]
    void p1_redPitaya_psOn_mox() {
        P1CodecRedPitaya codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::REDPITAYA,
            true, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 3);
        QCOMPARE(int(cfg.ddcEnable),  int(DDC0 + DDC2));
        QCOMPARE(int(cfg.rate[0]), kPsRate);
        QCOMPARE(int(cfg.rate[1]), kPsRate);
        QCOMPARE(int(cfg.cntrl1),  0x08);
    }

    // RedPitaya PS-off MOX → p1DdcConfig=1, but Rate[0] AND Rate[1] both set
    // (REDPITAYA PAVEL inline notes — distinct from G2-class which only sets Rate[0])
    // Source: console.cs:8331-8332 [v2.10.3.13]
    //   Rate[0] = rx1_rate; // REDPITAYA PAVEL
    //   Rate[1] = rx1_rate; // REDPITAYA PAVEL
    void p1_redPitaya_psOff_mox_setsRate1() {
        P1CodecRedPitaya codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::REDPITAYA,
            false, false, true, 48000, 0, false, 0, 0);
        QCOMPARE(int(cfg.p1DdcConfig), 1);
        QCOMPARE(int(cfg.rate[0]), 48000);   // PAVEL: set in PS-off
        QCOMPARE(int(cfg.rate[1]), 48000);   // PAVEL: set in PS-off
        QCOMPARE(int(cfg.rate[2]), 48000);
    }

    // RedPitaya PS-off MOX, RX2 → DDC3 added
    // Source: console.cs:8372-8375 [v2.10.3.13]
    void p1_redPitaya_mox_rx2Enabled_addsDdc3() {
        P1CodecRedPitaya codec;
        auto cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::REDPITAYA,
            false, false, true, 48000, 192000, true, 0, 0);
        QCOMPARE(int(cfg.ddcEnable), int(DDC2 + DDC3));   // PS-off uses DDC2
        QCOMPARE(int(cfg.rate[3]), 192000);
    }
};

QTEST_APPLESS_MAIN(TestCodecPsDdcConfig)
#include "tst_codec_ps_ddc_config.moc"
