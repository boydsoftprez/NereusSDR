// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// references below are cite comments documenting which Thetis lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_compute_audio_volume.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::computeAudioVolume (Phase 3 Agent 3B
// of issue #167 — the math kernel of the PA-cal safety hotfix).
//
// Covers:
//   - K2GX regression (CRITICAL, ship-blocking): ANAN-8000DLE 80m at
//     100 W and 200 W slider must produce the hand-computed audio_volume.
//   - sliderWatts == 0 short-circuit returns 0.0 exactly.
//   - HL2 sentinel (gbb >= 99.5): linear fallback at 80m (gbb=100), and
//     the dBm math at 6m (gbb=38.8 — clamps to 1.0 at 100W).
//   - NereusSDR Bypass profile (all-100.0f sentinel) hits the same
//     linear fallback as the HL2 short-circuit.
//   - Safety ceiling matrix (CRITICAL): every (HPSDRModel, Band) at the
//     radio's spec'd max watts produces audio_volume <= 1.0 (no
//     overshoot of the rail).  ANAN-8000D B10M @ 200W is the tightest
//     non-clamped cell at ~0.992.
//   - Out-of-range Band hits the HL2 sentinel via the
//     PaProfile::getGainForBand 1000-sentinel.
//   - No NaN / Inf for pathological inputs (sliderWatts = INT_MAX,
//     sliderWatts = -1).
//   - Drive-step lerp adjust takes effect at exact-step values.
//
// Source references (cite comments only — no Thetis logic in tests):
//   console.cs:46720-46751 [v2.10.3.13] — SetPowerUsingTargetDBM math.
//   mi0bot clsHardwareSpecific.cs:484 [v2.10.3.13-beta2] — "100 is no
//                                  output power" sentinel.
// =================================================================

#include <QtTest/QtTest>

#include <cmath>
#include <climits>

#include "core/HpsdrModel.h"
#include "core/PaProfile.h"
#include "models/Band.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// Per-HPSDRModel spec'd max watts.  Used by the safety-ceiling matrix.
// Matches the brief's per-model mapping verbatim.
int specMaxWattsFor(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:
        case HPSDRModel::HERMES:
        case HPSDRModel::ANAN10:
        case HPSDRModel::ANAN10E:
            return 10;
        case HPSDRModel::ANAN100:
        case HPSDRModel::ANAN100B:
            return 100;
        case HPSDRModel::ANAN100D:
        case HPSDRModel::ANAN200D:
            return 200;
        case HPSDRModel::ORIONMKII:
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ANVELINAPRO3:
            return 200;
        case HPSDRModel::ANAN_G2:
            return 100;
        case HPSDRModel::ANAN_G2_1K:
            return 1000;
        case HPSDRModel::REDPITAYA:
            return 10;
        case HPSDRModel::HERMESLITE:
            return 5;
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:
            return 0;  // sentinel — caller skips
    }
    return 0;
}

const char* hpsdrModelName(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:        return "HPSDR";
        case HPSDRModel::HERMES:       return "HERMES";
        case HPSDRModel::ANAN10:       return "ANAN10";
        case HPSDRModel::ANAN10E:      return "ANAN10E";
        case HPSDRModel::ANAN100:      return "ANAN100";
        case HPSDRModel::ANAN100B:     return "ANAN100B";
        case HPSDRModel::ANAN100D:     return "ANAN100D";
        case HPSDRModel::ANAN200D:     return "ANAN200D";
        case HPSDRModel::ORIONMKII:    return "ORIONMKII";
        case HPSDRModel::ANAN7000D:    return "ANAN7000D";
        case HPSDRModel::ANAN8000D:    return "ANAN8000D";
        case HPSDRModel::ANAN_G2:      return "ANAN_G2";
        case HPSDRModel::ANAN_G2_1K:   return "ANAN_G2_1K";
        case HPSDRModel::ANVELINAPRO3: return "ANVELINAPRO3";
        case HPSDRModel::HERMESLITE:   return "HERMESLITE";
        case HPSDRModel::REDPITAYA:    return "REDPITAYA";
        case HPSDRModel::FIRST:        return "FIRST";
        case HPSDRModel::LAST:         return "LAST";
    }
    return "?";
}

const char* bandName(Band b) noexcept {
    switch (b) {
        case Band::Band160m: return "Band160m";
        case Band::Band80m:  return "Band80m";
        case Band::Band60m:  return "Band60m";
        case Band::Band40m:  return "Band40m";
        case Band::Band30m:  return "Band30m";
        case Band::Band20m:  return "Band20m";
        case Band::Band17m:  return "Band17m";
        case Band::Band15m:  return "Band15m";
        case Band::Band12m:  return "Band12m";
        case Band::Band10m:  return "Band10m";
        case Band::Band6m:   return "Band6m";
        case Band::GEN:      return "GEN";
        case Band::WWV:      return "WWV";
        case Band::XVTR:     return "XVTR";
        default:             return "(SWL/other)";
    }
}

}  // namespace

class TstTransmitModelComputeAudioVolume : public QObject {
    Q_OBJECT

private slots:

    // =========================================================================
    // §1  K2GX regression (CRITICAL, ship-blocking)
    // =========================================================================
    //
    // ANAN-8000DLE 80m gbb = 50.5 dB (kAnan8000dRow B80M).  The dBm-target
    // math (Thetis console.cs:46720-46751 [v2.10.3.13]):
    //
    //   target_dbm   = 10 * log10(100 * 1000)             = 50.0
    //   net          = 50.0 - 50.5                          = -0.5
    //   target_volts = sqrt(10^(-0.5 * 0.1) * 0.05)
    //                = sqrt(10^(-0.05) * 0.05)
    //                = sqrt(0.044566)                       = 0.211099
    //   audio_volume = 0.211099 / 0.8                       = 0.263874
    //
    // K2GX field report scenario.  This regression test is ship-blocking —
    // a NereusSDR build without per-band PA-gain compensation produces
    // audio_volume = 1.0 (full rail) at slider 100 W on this band, which
    // is what made the ANAN-8000DLE deliver >300 W instead of 200 W when
    // K2GX engaged TUNE.
    void k2gx_8000dle_80m_at_100w_returns_0_26389() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - ANAN8000D"),
                          HPSDRModel::ANAN8000D, true);
        const double v = t.computeAudioVolume(p, Band::Band80m, 100);
        QVERIFY2(std::abs(v - 0.26389) < 0.001,
                 qPrintable(QStringLiteral("expected 0.26389 got %1")
                            .arg(v, 0, 'f', 6)));
    }

    // ANAN-8000DLE 80m at 200 W slider.  Same gbb=50.5 — math:
    //   target_dbm = 10 * log10(200 * 1000)              = 53.0103
    //   net        = 53.0103 - 50.5                       = 2.5103
    //   tv         = sqrt(10^0.25103 * 0.05)              = 0.298495
    //   audio_volume = 0.298495 / 0.8                     = 0.373119
    void k2gx_8000dle_80m_at_200w_returns_0_37312() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - ANAN8000D"),
                          HPSDRModel::ANAN8000D, true);
        const double v = t.computeAudioVolume(p, Band::Band80m, 200);
        QVERIFY2(std::abs(v - 0.37312) < 0.001,
                 qPrintable(QStringLiteral("expected 0.37312 got %1")
                            .arg(v, 0, 'f', 6)));
    }

    // =========================================================================
    // §2  sliderWatts == 0 short-circuit
    // =========================================================================
    //
    // Thetis console.cs:46749-46751 [v2.10.3.13]:
    //   if (new_pwr == 0) { Audio.RadioVolume = 0.0; ... }
    void slider_watts_zero_returns_zero_exactly() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - ANAN8000D"),
                          HPSDRModel::ANAN8000D, true);
        const double v = t.computeAudioVolume(p, Band::Band80m, 0);
        QCOMPARE(v, 0.0);
    }

    // =========================================================================
    // §3  HL2 sentinel — linear fallback (gbb >= 99.5)
    // =========================================================================
    //
    // mi0bot clsHardwareSpecific.cs:484 [v2.10.3.13-beta2] sets HL2 HF gain
    // rows to 100.0f ("100 is no output power" sentinel).  NereusSDR-original
    // deviation: gbb >= 99.5 short-circuits to linear fallback
    // clamp(sliderWatts/100, 0, 1) so HL2 users don't regress on HF transmit
    // until they calibrate.
    void hl2_80m_at_100w_linear_fallback_returns_one() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - HERMESLITE"),
                          HPSDRModel::HERMESLITE, true);
        // gbb @ 80m = 100.0f sentinel → short-circuit hits linear fallback.
        // 100/100 = 1.0.
        const double v = t.computeAudioVolume(p, Band::Band80m, 100);
        QCOMPARE(v, 1.0);
    }

    void hl2_80m_at_50w_linear_fallback_returns_0_5() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - HERMESLITE"),
                          HPSDRModel::HERMESLITE, true);
        // 50/100 = 0.5.
        const double v = t.computeAudioVolume(p, Band::Band80m, 50);
        QCOMPARE(v, 0.5);
    }

    void hl2_80m_at_200w_linear_fallback_clamped_to_one() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - HERMESLITE"),
                          HPSDRModel::HERMESLITE, true);
        // 200/100 = 2.0 → clamp to 1.0.
        const double v = t.computeAudioVolume(p, Band::Band80m, 200);
        QCOMPARE(v, 1.0);
    }

    // =========================================================================
    // §4  HL2 6m (real entry, gbb=38.8) — dBm math kicks in
    // =========================================================================
    //
    // HL2 6m has a real gain entry (38.8 dB) — does NOT hit the sentinel
    // short-circuit.  Hand-computed audio_volume at 100 W slider:
    //
    //   target_dbm = 10 * log10(100*1000)                = 50.0
    //   net        = 50.0 - 38.8                         = 11.2
    //   tv         = sqrt(10^1.12 * 0.05)
    //              = sqrt(13.1826 * 0.05)
    //              = sqrt(0.65913)                        = 0.81188
    //   audio_volume_raw = 0.81188 / 0.8                 = 1.01485
    //   audio_volume     = min(1.01485, 1.0)             = 1.0  (clamped)
    //
    // HL2 hits the 1.0 ceiling on 6m at 100 W; the safety constraint is
    // that we don't EXCEED 1.0 (which would be amplitude clipping);
    // reaching the rail is acceptable — this matches Thetis behavior.
    void hl2_6m_at_100w_dbm_math_clamps_to_one() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - HERMESLITE"),
                          HPSDRModel::HERMESLITE, true);
        const double v = t.computeAudioVolume(p, Band::Band6m, 100);
        QCOMPARE(v, 1.0);
    }

    // =========================================================================
    // §5  NereusSDR Bypass profile (all-100.0f) — same linear fallback
    // =========================================================================
    //
    // The Bypass factory profile has every band at 100.0f — Thetis-
    // sentinel "no output power".  Same gbb >= 99.5 short-circuit path
    // as HL2 HF.
    void bypass_profile_any_band_50w_linear_fallback_returns_0_5() {
        TransmitModel t;
        // Bypass profile: name doesn't matter, but model FIRST inherits
        // HERMES values from kHermesRow which would NOT be all-100.
        // We override via setGainForBand to mirror what
        // PaProfileManager does for the Bypass slot.
        PaProfile p(QStringLiteral("Bypass"), HPSDRModel::FIRST, true);
        for (int i = 0; i < PaProfile::kBandCount; ++i) {
            p.setGainForBand(static_cast<Band>(i), 100.0f);
        }
        QCOMPARE(t.computeAudioVolume(p, Band::Band20m, 50), 0.5);
        QCOMPARE(t.computeAudioVolume(p, Band::Band15m, 75), 0.75);
        QCOMPARE(t.computeAudioVolume(p, Band::Band6m, 100), 1.0);
    }

    // =========================================================================
    // §6  Safety ceiling matrix (CRITICAL)
    // =========================================================================
    //
    // For every HPSDRModel × Band at the radio's spec'd max watts, assert
    // audio_volume <= 1.0 — no overshoot of the rail.
    //
    // Note: the tightest unclamped cell is ANAN-8000D × Band10m @ 200 W,
    // which produces ~0.99291 (gbb=42.0; net=11.0103; tv=0.794330;
    // /0.8=0.99291).  This is the highest-gain PA × lowest-gain HF band
    // combination.  Future regression: if a tighter cell appears, it
    // means the math kernel changed or a PaGainProfile factory row got
    // edited.
    //
    // ANAN_G2_1K at 1000 W is a special case: the gain table for
    // G2_1K is identical to ANAN_G2 (47.9..50.9 dB) — i.e., calibrated
    // for ~100 W output, not 1 kW.  At slider=1000 W on G2_1K the math
    // overshoots the rail (~1.65) and is clamped to 1.0 by the kernel.
    // The clamp is the safety net: real Thetis users on G2_1K either
    // run the slider <=100 W or recalibrate their gain table.
    //
    // For HL2, only Band6m goes through the dBm math path (gbb=38.8).
    // All other HF bands take the gbb >= 99.5 sentinel → linear fallback.
    // Since spec_max for HL2 = 5 W, both paths produce 0.05 (linear) or
    // a tiny dBm value, well under 1.0.
    void safety_ceiling_matrix_no_band_exceeds_rail() {
        TransmitModel t;
        // Iterate the 16 production HPSDRModel values (FIRST/LAST are
        // sentinels — skipped via specMaxWattsFor returning 0).
        for (int mi = static_cast<int>(HPSDRModel::HPSDR);
             mi <= static_cast<int>(HPSDRModel::REDPITAYA); ++mi) {
            const auto model = static_cast<HPSDRModel>(mi);
            const int maxW = specMaxWattsFor(model);
            if (maxW <= 0) {
                continue;
            }
            const PaProfile p(QStringLiteral("Default - %1")
                                  .arg(QString::fromLatin1(hpsdrModelName(model))),
                              model, true);
            // Iterate all 14 PA-relevant bands (Band160m..XVTR).
            for (int bi = 0;
                 bi < static_cast<int>(Band::SwlFirst); ++bi) {
                const auto band = static_cast<Band>(bi);
                const double v = t.computeAudioVolume(p, band, maxW);
                QVERIFY2(std::isfinite(v),
                    qPrintable(QStringLiteral("non-finite v for %1 / %2")
                                   .arg(QString::fromLatin1(hpsdrModelName(model)),
                                        QString::fromLatin1(bandName(band)))));
                QVERIFY2(v <= 1.0,
                    qPrintable(QStringLiteral("safety ceiling exceeded: "
                                              "%1 / %2 @ %3W -> %4")
                                   .arg(QString::fromLatin1(hpsdrModelName(model)),
                                        QString::fromLatin1(bandName(band)))
                                   .arg(maxW)
                                   .arg(v, 0, 'f', 6)));
                QVERIFY2(v >= 0.0,
                    qPrintable(QStringLiteral("audio_volume negative: "
                                              "%1 / %2 @ %3W -> %4")
                                   .arg(QString::fromLatin1(hpsdrModelName(model)),
                                        QString::fromLatin1(bandName(band)))
                                   .arg(maxW)
                                   .arg(v, 0, 'f', 6)));
            }
        }
    }

    // Pin the ANAN-8000D B10M @ 200W tightest-cell value.  If this number
    // changes, either the kernel math was edited or the PaGainProfile
    // factory row was edited — both deserve a code review.
    void anan8000d_b10m_at_200w_is_tightest_at_0_99291() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - ANAN8000D"),
                          HPSDRModel::ANAN8000D, true);
        const double v = t.computeAudioVolume(p, Band::Band10m, 200);
        // ANAN-8000D B10M @ 200W is the tightest unclamped cell.  gbb=42.0;
        // net=11.0103; tv=0.79433; audio_volume=0.99291.
        QVERIFY2(std::abs(v - 0.99291) < 0.001,
                 qPrintable(QStringLiteral("expected 0.99291 got %1")
                            .arg(v, 0, 'f', 6)));
        // Always under the 1.0 rail (margin ~0.007).
        QVERIFY(v < 1.0);
    }

    // =========================================================================
    // §7  Out-of-range Band hits sentinel
    // =========================================================================
    //
    // PaProfile::getGainForBand returns 1000.0f sentinel for Band values
    // outside the 14-band PA window (Band::Band120m and later, plus any
    // raw int cast outside [0, 14)).  Math kernel sees gbb >= 99.5 and
    // takes the linear fallback path.
    void out_of_range_band_hits_sentinel_via_linear_fallback() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - ANAN8000D"),
                          HPSDRModel::ANAN8000D, true);
        // Cast raw int to a Band value outside the enum range.  This
        // would be a programming error, but the kernel must fail-safe
        // (not produce audio_volume = 1.0 silently — pre-fix behavior).
        const auto badBand = static_cast<Band>(99);
        // Linear fallback: 50/100 = 0.5.
        QCOMPARE(t.computeAudioVolume(p, badBand, 50), 0.5);
        // 100/100 = 1.0.
        QCOMPARE(t.computeAudioVolume(p, badBand, 100), 1.0);
        // 200/100 clamped to 1.0.
        QCOMPARE(t.computeAudioVolume(p, badBand, 200), 1.0);
    }

    // =========================================================================
    // §8  No NaN / Inf for pathological inputs
    // =========================================================================
    void pathological_inputs_no_nan_or_inf() {
        TransmitModel t;
        const PaProfile p(QStringLiteral("Default - ANAN8000D"),
                          HPSDRModel::ANAN8000D, true);

        // INT_MAX → very large dBm but math is finite (and clamped).
        const double vMax = t.computeAudioVolume(p, Band::Band20m, INT_MAX);
        QVERIFY(std::isfinite(vMax));
        QVERIFY(vMax >= 0.0 && vMax <= 1.0);

        // Negative slider → falls into <=0 branch → 0.0.
        const double vNeg = t.computeAudioVolume(p, Band::Band20m, -1);
        QCOMPARE(vNeg, 0.0);

        // INT_MIN (deeply negative) → also falls into <=0 → 0.0.
        const double vMin = t.computeAudioVolume(p, Band::Band20m, INT_MIN);
        QCOMPARE(vMin, 0.0);

        // Zero (already covered in §2 but include here for completeness).
        QCOMPARE(t.computeAudioVolume(p, Band::Band20m, 0), 0.0);
    }

    // =========================================================================
    // §9  Drive-step adjust applied via lerp
    // =========================================================================
    //
    // PaProfile::calcDriveAdjust on exact-step input (driveValue = N*10,
    // 1 <= N <= 9) returns m_gainAdjust[N-1] directly.  At driveValue=50,
    // that's m_gainAdjust[4].  Math:
    //   gbb = 50.5 - 0.5 = 50.0
    //   target_dbm = 10*log10(50*1000) = 46.9897
    //   net = 46.9897 - 50.0 = -3.0103
    //   tv = sqrt(10^-0.30103 * 0.05) = sqrt(0.5 * 0.05) = sqrt(0.025) = 0.15811
    //   audio_volume = 0.15811 / 0.8 = 0.19764
    //
    // Without the adjust, gbb = 50.5:
    //   net = 46.9897 - 50.5 = -3.5103
    //   tv = sqrt(10^-0.35103 * 0.05) = sqrt(0.4458 * 0.05) = sqrt(0.02229) = 0.14930
    //   audio_volume = 0.14930 / 0.8 = 0.18663
    //
    // Difference ~0.011 — distinct enough to verify the adjust is applied.
    void drive_step_adjust_applied_at_exact_step() {
        TransmitModel t;
        PaProfile p(QStringLiteral("Default - ANAN8000D"),
                    HPSDRModel::ANAN8000D, true);

        // Baseline (no adjust): drive=50, gbb=50.5.
        const double baseline = t.computeAudioVolume(p, Band::Band80m, 50);

        // Set adjust[4] = 0.5 dB.  drive=50 reads m_gainAdjust[4] exactly
        // (Thetis 9-step lerp exact-step rule: nLIndex=5, nLIndex-1=4).
        // Lower gain attenuation → higher net dBm → higher audio_volume.
        p.setAdjust(Band::Band80m, 4, 0.5f);
        const double withAdjust = t.computeAudioVolume(p, Band::Band80m, 50);

        // The adjust subtracts from gbb (PaProfile::getGainForBand:
        // base - adjust).  So withAdjust > baseline.
        QVERIFY2(std::abs(withAdjust - baseline) > 0.005,
                 qPrintable(QStringLiteral("adjust did not change result: "
                                           "baseline=%1 withAdjust=%2")
                                .arg(baseline, 0, 'f', 6)
                                .arg(withAdjust, 0, 'f', 6)));
        QVERIFY(withAdjust > baseline);

        // Pin the actual computed value (gbb=50.0): audio_volume ~ 0.19764.
        QVERIFY2(std::abs(withAdjust - 0.19764) < 0.001,
                 qPrintable(QStringLiteral("expected 0.19764 got %1")
                                .arg(withAdjust, 0, 'f', 6)));
    }
};

QTEST_MAIN(TstTransmitModelComputeAudioVolume)
#include "tst_transmit_model_compute_audio_volume.moc"
