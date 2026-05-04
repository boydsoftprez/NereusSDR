// tst_pa_gain_profile.cpp
//
// no-port-check: Test file references Thetis behaviour in commentary only;
// no Thetis source is translated here. Thetis cites are in PaGainProfile.cpp.
//
// Covers (Phase 1 Agent 1A of issue #167 — PA-calibration safety hotfix):
//   - defaultPaGainsForBand(model, band) returns the byte-for-byte values
//     from Thetis clsHardwareSpecific.cs:459-751 [v2.10.3.13]
//     DefaultPAGainsForBands(HPSDRModel) for every per-model case.
//   - HL2 row from mi0bot clsHardwareSpecific.cs:769-797 [v2.10.3.13-beta2].
//   - bypassPaGainsForBand(band) is the all-100.0f sentinel profile used
//     by the "Bypass" factory entry in PaProfileManager.
//   - NereusSDR-specific Band slots (GEN, WWV, XVTR, SWL bands) fall through
//     to the 100.0f sentinel — Thetis has no equivalent for those bands in
//     the gain table, and the HL2 sentinel short-circuit handles them
//     downstream in TransmitModel::computeAudioVolume.

#include <QtTest/QtTest>

#include "core/HpsdrModel.h"
#include "core/PaGainProfile.h"
#include "models/Band.h"

using namespace NereusSDR;

class TestPaGainProfile : public QObject {
    Q_OBJECT

private slots:
    // ── K2GX regression spotlight ────────────────────────────────────────
    // 80m on ANAN-8000DLE is the highest-gain entry on a 200 W radio
    // (50.5 dB). This single value is the linchpin of the K2GX field report
    // — drive-slider math without per-band PA gain compensation produces
    // >300 W output here. Verify it pins to the upstream 50.5f exactly.
    // From Thetis clsHardwareSpecific.cs:657 [v2.10.3.13]:
    //   gains[(int)Band.B80M] = 50.5f;
    void factoryParity_Anan8000D_80m_50p5dB() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band80m),
                 50.5f);
    }

    // ── Per-model 11-band parity ──────────────────────────────────────────
    // For every HPSDRModel value, the 11 ham-band gains (160m..6m) match the
    // Thetis upstream byte-for-byte.

    // FIRST/HERMES/HPSDR/ORIONMKII share a row.
    // From Thetis clsHardwareSpecific.cs:471-502 [v2.10.3.13]
    void hermes_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band160m), 41.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band80m),  41.2f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band60m),  41.3f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band40m),  41.3f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band30m),  41.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band20m),  40.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band17m),  39.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band15m),  38.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band12m),  38.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band10m),  38.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::Band6m),   38.8f);
    }
    void hpsdr_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HPSDR,     Band::Band160m), 41.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HPSDR,     Band::Band80m),  41.2f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HPSDR,     Band::Band6m),   38.8f);
    }
    void orionmkii_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ORIONMKII, Band::Band160m), 41.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ORIONMKII, Band::Band80m),  41.2f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ORIONMKII, Band::Band6m),   38.8f);
    }

    // ANAN10 / ANAN10E share a row identical to FIRST/HERMES/HPSDR/ORIONMKII.
    // From Thetis clsHardwareSpecific.cs:504-533 [v2.10.3.13]
    void anan10_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band160m), 41.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band80m),  41.2f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band60m),  41.3f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band40m),  41.3f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band30m),  41.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band20m),  40.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band17m),  39.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band15m),  38.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band12m),  38.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band10m),  38.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10,    Band::Band6m),   38.8f);
    }
    void anan10e_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10E,   Band::Band160m), 41.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10E,   Band::Band80m),  41.2f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN10E,   Band::Band6m),   38.8f);
    }

    // ANAN100 — separate case (not shared with ANAN100B even though values match)
    // From Thetis clsHardwareSpecific.cs:535-563 [v2.10.3.13]
    void anan100_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band160m), 50.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band60m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band40m),  50.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band30m),  49.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band20m),  48.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band17m),  48.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band15m),  47.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band12m),  46.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band10m),  42.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100,   Band::Band6m),   43.0f);
    }

    // ANAN100B — separate case (Thetis has it as its own switch arm).
    // From Thetis clsHardwareSpecific.cs:565-593 [v2.10.3.13]
    void anan100b_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band160m), 50.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band60m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band40m),  50.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band30m),  49.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band20m),  48.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band17m),  48.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band15m),  47.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band12m),  46.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band10m),  42.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100B,  Band::Band6m),   43.0f);
    }

    // ANAN100D — separate case.
    // From Thetis clsHardwareSpecific.cs:595-623 [v2.10.3.13]
    void anan100d_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band160m), 49.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band60m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band40m),  50.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band30m),  49.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band20m),  48.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band17m),  47.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band15m),  46.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band12m),  46.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band10m),  43.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN100D,  Band::Band6m),   43.0f);
    }

    // ANAN200D — separate case (values match ANAN100D).
    // From Thetis clsHardwareSpecific.cs:625-653 [v2.10.3.13]
    void anan200d_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band160m), 49.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band60m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band40m),  50.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band30m),  49.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band20m),  48.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band17m),  47.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band15m),  46.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band12m),  46.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band10m),  43.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN200D,  Band::Band6m),   43.0f);
    }

    // ANAN8000D — separate case. K2GX flagship.
    // From Thetis clsHardwareSpecific.cs:655-683 [v2.10.3.13]
    void anan8000d_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band160m), 50.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band60m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band40m),  50.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band30m),  49.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band20m),  48.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band17m),  48.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band15m),  47.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band12m),  46.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band10m),  42.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band6m),   43.0f);
    }

    // ANAN7000D / ANAN_G2 / ANVELINAPRO3 / REDPITAYA share a row.
    // From Thetis clsHardwareSpecific.cs:685-716 [v2.10.3.13]
    void anan7000d_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band160m), 47.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band60m),  50.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band40m),  50.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band30m),  50.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band20m),  50.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band17m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band15m),  47.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band12m),  47.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band10m),  46.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN7000D,    Band::Band6m),   44.6f);
    }
    void anan_g2_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2,      Band::Band160m), 47.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2,      Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2,      Band::Band6m),   44.6f);
    }
    void anvelinapro3_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANVELINAPRO3, Band::Band160m), 47.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANVELINAPRO3, Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANVELINAPRO3, Band::Band6m),   44.6f);
    }
    void redpitaya_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::REDPITAYA,    Band::Band160m), 47.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::REDPITAYA,    Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::REDPITAYA,    Band::Band6m),   44.6f);
    }

    // ANAN_G2_1K — separate case (values match ANAN7000D group).
    // From Thetis clsHardwareSpecific.cs:718-746 [v2.10.3.13]
    void anan_g2_1k_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band160m), 47.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band80m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band60m),  50.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band40m),  50.8f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band30m),  50.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band20m),  50.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band17m),  50.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band15m),  47.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band12m),  47.9f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band10m),  46.5f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2_1K,   Band::Band6m),   44.6f);
    }

    // ── HL2 sentinel from mi0bot ──────────────────────────────────────────
    // From mi0bot clsHardwareSpecific.cs:769-797 [v2.10.3.13-beta2 @c26a8a4].
    // HF (160m..10m) all 100.0f (sentinel — "no output power" per upstream
    // line 484 comment); 6m and VHF rows all 38.8f.
    void hermeslite_80m_is_100p0_sentinel() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band80m),  100.0f);
    }
    void hermeslite_6m_is_38p8() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band6m),   38.8f);
    }
    void hermeslite_full_hf_byteForByte() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band160m), 100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band80m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band60m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band40m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band30m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band20m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band17m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band15m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band12m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band10m),  100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMESLITE,   Band::Band6m),   38.8f);
    }

    // ── Bypass profile is all 100.0f sentinel ─────────────────────────────
    void bypass_returns_100_for_every_ham_band() {
        QCOMPARE(bypassPaGainsForBand(Band::Band160m), 100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band80m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band60m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band40m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band30m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band20m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band17m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band15m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band12m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band10m),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::Band6m),   100.0f);
    }
    void bypass_returns_100_for_nereus_specific_slots() {
        QCOMPARE(bypassPaGainsForBand(Band::GEN),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::WWV),  100.0f);
        QCOMPARE(bypassPaGainsForBand(Band::XVTR), 100.0f);
    }

    // ── NereusSDR-specific Band slots (GEN/WWV/XVTR) ──────────────────────
    // No equivalent exists in the Thetis gain table; return the 100.0f
    // sentinel. Documented divergence — the HL2 sentinel short-circuit
    // (gbb >= 99.5) in TransmitModel::computeAudioVolume routes these to
    // the linear-fallback branch downstream.
    void anan8000d_gen_returns_sentinel() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::GEN),  100.0f);
    }
    void anan8000d_wwv_returns_sentinel() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::WWV),  100.0f);
    }
    void anan8000d_xvtr_returns_sentinel() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::XVTR), 100.0f);
    }
    void anan_g2_gen_returns_sentinel() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN_G2,   Band::GEN),  100.0f);
    }
    void hermes_xvtr_returns_sentinel() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::HERMES,    Band::XVTR), 100.0f);
    }
    // SWL bands also have no Thetis gain-table equivalent.
    void anan8000d_swl_returns_sentinel() {
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band120m), 100.0f);
        QCOMPARE(defaultPaGainsForBand(HPSDRModel::ANAN8000D, Band::Band11m),  100.0f);
    }
};

QTEST_MAIN(TestPaGainProfile)
#include "tst_pa_gain_profile.moc"
