// tst_pa_profile.cpp
//
// no-port-check: Test file references Thetis behaviour in commentary only;
// no Thetis source is translated here. Thetis cites are in PaProfile.cpp.
//
// Covers:
//   - PaProfile construction + ResetGainDefaultsForModel populates the
//     14-band gain table from PaGainProfile factory rows
//     (Thetis setup.cs:23786-23810 [v2.10.3.13]).
//   - GetGainForBand(driveValue == -1) returns base gain unaltered.
//   - calcDriveAdjust 9-step lerp boundaries
//     (Thetis setup.cs:23923-23959 [v2.10.3.13]):
//       * driveValue == 0  -> 0.0
//       * driveValue == 100 -> 0.0
//       * exact step (driveValue % 10 == 0, idx in 1..9) -> _gainAdjust[idx-1]
//       * mid-lerp idx=0 -> half of step 0
//       * mid-lerp idx=5 -> 0.5 * (step[4] + step[5])
//       * mid-lerp idx=9 -> half of step 8
//   - GetGainForBand(driveValue >= 0) applies calcDriveAdjust subtraction.
//   - Setters silently no-op on out-of-range Band / stepIndex.
//   - copySettings deep-copies gainValues / gainAdjust / maxPower / useMaxPower.
//   - DataToString -> DataFromString round-trip; field count is exactly 171.
//   - Default-constructed PaProfile is in valid empty state.
//   - DataFromString rejects malformed input (wrong field count) -> false.

#include <QtTest/QtTest>

#include <QString>
#include <QStringList>

#include "core/HpsdrModel.h"
#include "core/PaProfile.h"
#include "models/Band.h"

using namespace NereusSDR;

class TestPaProfile : public QObject {
    Q_OBJECT

private slots:
    // ── 1. Construct + ResetGainDefaultsForModel ─────────────────────────
    // Thetis PAProfile constructor (setup.cs:23786-23810 [v2.10.3.13])
    // calls ResetGainDefaultsForModel(model) which loads from
    // HardwareSpecific.DefaultPAGainsForBands. NereusSDR ports this through
    // defaultPaGainsForBand. ANAN-8000DLE 80m factory value is 50.5f
    // (clsHardwareSpecific.cs:656-666 [v2.10.3.13]).
    void construct_resets_gain_defaults_for_model() {
        PaProfile p(QStringLiteral("Test"), HPSDRModel::ANAN8000D, true);
        QCOMPARE(p.name(), QStringLiteral("Test"));
        QCOMPARE(p.model(), HPSDRModel::ANAN8000D);
        QCOMPARE(p.isFactoryDefault(), true);
        QCOMPARE(p.getGainForBand(Band::Band80m), 50.5f);
    }

    // ── 2. getGainForBand(-1) returns base ───────────────────────────────
    // Thetis setup.cs:23914-23922 [v2.10.3.13]:
    //   if (nDriveValue == -1) return _gainValues[(int)b];
    void get_gain_for_band_minus_one_returns_base() {
        PaProfile p(QStringLiteral("ANAN-8000DLE base"),
                    HPSDRModel::ANAN8000D, true);
        // 80m factory = 50.5f for ANAN-8000DLE.
        QCOMPARE(p.getGainForBand(Band::Band80m, -1), 50.5f);
        // Default driveValue arg is -1 → same path.
        QCOMPARE(p.getGainForBand(Band::Band80m), 50.5f);
    }

    // ── 3. calcDriveAdjust boundaries ────────────────────────────────────
    // Thetis setup.cs:23923-23959 [v2.10.3.13]. We can't call calcDriveAdjust
    // directly (it's private), but getGainForBand(b, drive) =
    // _gainValues[b] - calcDriveAdjust(b, drive), so we can solve for it.
    void calc_drive_adjust_zero_returns_zero() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        // Set known step values so we can detect non-zero output.
        for (int i = 0; i < 9; ++i) {
            p.setAdjust(Band::Band80m, i, 1.0f * (i + 1));
        }
        // driveValue == 0 -> calcDriveAdjust returns 0
        // (nLIndex=0, %10==0, idx==0 short-circuit to 0).
        QCOMPARE(p.getGainForBand(Band::Band80m, 0), 50.5f);
    }

    void calc_drive_adjust_hundred_returns_zero() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        for (int i = 0; i < 9; ++i) {
            p.setAdjust(Band::Band80m, i, 1.0f * (i + 1));
        }
        // driveValue == 100 -> nLIndex=10, %10==0, idx==10 short-circuit to 0.
        QCOMPARE(p.getGainForBand(Band::Band80m, 100), 50.5f);
    }

    void calc_drive_adjust_exact_step_one() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        p.setAdjust(Band::Band80m, 0, 0.5f);
        // driveValue == 10 -> nLIndex=1, %10==0, idx in 1..9 -> _gainAdjust[0]
        // = 0.5 → getGainForBand = 50.5 - 0.5 = 50.0.
        QCOMPARE(p.getGainForBand(Band::Band80m, 10), 50.0f);
    }

    void calc_drive_adjust_exact_step_five() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        p.setAdjust(Band::Band80m, 4, 1.5f);
        // driveValue == 50 -> nLIndex=5, %10==0, idx in 1..9 -> _gainAdjust[4]
        // = 1.5 → getGainForBand = 50.5 - 1.5 = 49.0.
        QCOMPARE(p.getGainForBand(Band::Band80m, 50), 49.0f);
    }

    void calc_drive_adjust_exact_step_nine() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        p.setAdjust(Band::Band80m, 8, 2.0f);
        // driveValue == 90 -> nLIndex=9, %10==0, idx in 1..9 -> _gainAdjust[8]
        // = 2.0 → getGainForBand = 50.5 - 2.0 = 48.5.
        QCOMPARE(p.getGainForBand(Band::Band80m, 90), 48.5f);
    }

    void calc_drive_adjust_mid_lerp_idx_zero() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        p.setAdjust(Band::Band80m, 0, 1.0f);
        // driveValue == 5 -> nLIndex=0, frac=0.5. low=0, high=_gainAdjust[0]=1.0.
        // lerp(0, 1.0, 0.5) = 0.5 → getGainForBand = 50.5 - 0.5 = 50.0.
        QCOMPARE(p.getGainForBand(Band::Band80m, 5), 50.0f);
    }

    void calc_drive_adjust_mid_lerp_idx_five() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        p.setAdjust(Band::Band80m, 4, 1.0f);
        p.setAdjust(Band::Band80m, 5, 3.0f);
        // driveValue == 55 -> nLIndex=5, frac=0.5. low=_gainAdjust[4]=1.0,
        // high=_gainAdjust[5]=3.0. lerp(1.0, 3.0, 0.5) = 2.0
        // → getGainForBand = 50.5 - 2.0 = 48.5.
        QCOMPARE(p.getGainForBand(Band::Band80m, 55), 48.5f);
    }

    void calc_drive_adjust_mid_lerp_idx_nine() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        p.setAdjust(Band::Band80m, 8, 2.0f);
        // driveValue == 95 -> nLIndex=9, frac=0.5. low=_gainAdjust[8]=2.0,
        // high=0. lerp(2.0, 0, 0.5) = 1.0 → getGainForBand = 50.5 - 1.0 = 49.5.
        QCOMPARE(p.getGainForBand(Band::Band80m, 95), 49.5f);
    }

    // ── 4. getGainForBand with drive applies adjust ──────────────────────
    void get_gain_with_drive_subtracts_adjust() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        p.setAdjust(Band::Band80m, 0, 0.5f);
        // 80m factory = 50.5; adjust[0] = 0.5; drive=10 → 50.5 - 0.5 = 50.0.
        QCOMPARE(p.getGainForBand(Band::Band80m, 10), 50.0f);
    }

    // ── 5. Setters silently no-op on out-of-range Band / stepIndex ───────
    // Thetis setup.cs:23964-24003 [v2.10.3.13] — every setter early-returns
    // on `(int)b <= (int)Band.FIRST || (int)b >= (int)Band.LAST` and
    // SetAdjust additionally on `stepIndex < 0 || stepIndex > 8`.
    // NereusSDR's analog: any Band index >= kBandCount (i.e. SWL bands 14..26)
    // is silently ignored.
    void set_gain_for_band_out_of_range_no_ops() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        const float before = p.getGainForBand(Band::Band80m);
        // Band120m has index 14 — outside the PaProfile 14-band window.
        p.setGainForBand(Band::Band120m, 99.0f);
        // Band80m unaffected.
        QCOMPARE(p.getGainForBand(Band::Band80m), before);
        // Out-of-range getter returns the 1000.0f Thetis sentinel
        // (setup.cs:23866 [v2.10.3.13]) — feeds the gbb >= 99.5 safety
        // short-circuit in TransmitModel::computeAudioVolume.
        QCOMPARE(p.getGainForBand(Band::Band120m), 1000.0f);
    }

    // ── 5b. XVTR (band index 13) is IN-range — no sentinel ───────────────
    // XVTR is the last in-range slot (index 13 of [0, 14)).  After
    // construction with HPSDRModel::ANAN8000D, `defaultPaGainsForBand`
    // returns 100.0f for XVTR (no Thetis equivalent in the gain table —
    // PaGainProfile's NereusSDR-spin sentinel for non-HF slots).  The
    // getter must NOT trip the 1000-sentinel branch.
    void get_gain_for_band_xvtr_in_range_no_sentinel() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        // XVTR slot's factory value comes from PaGainProfile sentinel (100.0f),
        // not the out-of-range 1000.0f sentinel.
        QCOMPARE(p.getGainForBand(Band::XVTR), 100.0f);
        QCOMPARE(p.getGainForBand(Band::XVTR, 50), 100.0f);  // no adjust set
    }

    void set_adjust_out_of_range_no_ops() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        // Negative stepIndex is silently ignored.
        p.setAdjust(Band::Band80m, -1, 99.0f);
        // stepIndex > 8 is silently ignored.
        p.setAdjust(Band::Band80m, 9, 99.0f);
        // Out-of-range Band silently ignored.
        p.setAdjust(Band::Band120m, 0, 99.0f);
        // Direct read of in-range stepIndex still 0.
        for (int i = 0; i < PaProfile::kDriveSteps; ++i) {
            QCOMPARE(p.getAdjust(Band::Band80m, i), 0.0f);
        }
    }

    void set_max_power_out_of_range_no_ops() {
        PaProfile p(QStringLiteral("p"), HPSDRModel::ANAN8000D, true);
        p.setMaxPower(Band::Band120m, 200.0f);
        p.setMaxPowerUse(Band::Band120m, true);
        // Out-of-range getter returns 0 / false.
        QCOMPARE(p.getMaxPower(Band::Band120m), 0.0f);
        QCOMPARE(p.getMaxPowerUse(Band::Band120m), false);
    }

    // ── 6. copySettings deep-copies gainValues + gainAdjust + maxPower ───
    // Thetis setup.cs:24004-24026 [v2.10.3.13].
    void copy_settings_deep_copies_all_fields() {
        PaProfile src(QStringLiteral("src"), HPSDRModel::ANAN8000D, true);
        // Mutate every field on src.
        src.setGainForBand(Band::Band80m, 99.0f);
        src.setGainForBand(Band::Band20m, 88.0f);
        src.setAdjust(Band::Band80m, 3, 1.5f);
        src.setMaxPower(Band::Band80m, 200.0f);
        src.setMaxPowerUse(Band::Band80m, true);

        // Empty target with different model — name/model/isFactoryDefault
        // should NOT change (Thetis CopySettings only copies arrays).
        PaProfile dst(QStringLiteral("dst"), HPSDRModel::ANAN10, false);
        dst.copySettings(src);

        QCOMPARE(dst.name(), QStringLiteral("dst"));
        QCOMPARE(dst.model(), HPSDRModel::ANAN10);
        QCOMPARE(dst.isFactoryDefault(), false);

        QCOMPARE(dst.getGainForBand(Band::Band80m), 99.0f);
        QCOMPARE(dst.getGainForBand(Band::Band20m), 88.0f);
        QCOMPARE(dst.getAdjust(Band::Band80m, 3), 1.5f);
        QCOMPARE(dst.getMaxPower(Band::Band80m), 200.0f);
        QCOMPARE(dst.getMaxPowerUse(Band::Band80m), true);
    }

    // ── 7. Round-trip serialization ──────────────────────────────────────
    // NereusSDR-canonical 14-band layout: 1 (base64 name) + 1 (model int) +
    // 1 (default bool) + 14 (gains) + 14*9 (adjusts) + 14*2 (max-power
    // pairs) = 3 + 14 + 126 + 28 = 171 fields.
    void data_to_string_field_count_is_171() {
        PaProfile p(QStringLiteral("count-check"),
                    HPSDRModel::ANAN8000D, true);
        const QStringList parts = p.dataToString().split(QLatin1Char('|'));
        QCOMPARE(parts.size(), 171);
    }

    void round_trip_serialization_random_fill() {
        PaProfile src(QStringLiteral("Round Trip"),
                      HPSDRModel::ANAN_G2, false);
        // Fill every band slot with deterministic-but-varied values.
        for (int b = 0; b < PaProfile::kBandCount; ++b) {
            const Band band = static_cast<Band>(b);
            src.setGainForBand(band, 40.0f + 0.5f * b);
            for (int s = 0; s < PaProfile::kDriveSteps; ++s) {
                src.setAdjust(band, s, 0.1f * (b + 1) * (s + 1));
            }
            src.setMaxPower(band, 100.0f + 5.0f * b);
            src.setMaxPowerUse(band, (b % 2) == 0);
        }

        const QString blob = src.dataToString();
        PaProfile dst(QStringLiteral("placeholder"),
                      HPSDRModel::ANAN10, true);
        QVERIFY(dst.dataFromString(blob));

        QCOMPARE(dst.name(), QStringLiteral("Round Trip"));
        QCOMPARE(dst.model(), HPSDRModel::ANAN_G2);
        QCOMPARE(dst.isFactoryDefault(), false);
        for (int b = 0; b < PaProfile::kBandCount; ++b) {
            const Band band = static_cast<Band>(b);
            QCOMPARE(dst.getGainForBand(band), src.getGainForBand(band));
            for (int s = 0; s < PaProfile::kDriveSteps; ++s) {
                QCOMPARE(dst.getAdjust(band, s), src.getAdjust(band, s));
            }
            QCOMPARE(dst.getMaxPower(band), src.getMaxPower(band));
            QCOMPARE(dst.getMaxPowerUse(band), src.getMaxPowerUse(band));
        }
    }

    // ── 8. Empty default constructor produces valid empty state ──────────
    void empty_default_constructor() {
        PaProfile p;
        QCOMPARE(p.name(), QString{});
        QCOMPARE(p.isFactoryDefault(), false);
        // No gains populated -> all 100.0f sentinel (Thetis init loop
        // setup.cs:23797-23807).
        for (int b = 0; b < PaProfile::kBandCount; ++b) {
            QCOMPARE(p.getGainForBand(static_cast<Band>(b)), 100.0f);
        }
        // No adjusts.
        for (int b = 0; b < PaProfile::kBandCount; ++b) {
            for (int s = 0; s < PaProfile::kDriveSteps; ++s) {
                QCOMPARE(p.getAdjust(static_cast<Band>(b), s), 0.0f);
            }
        }
    }

    // ── 9. dataFromString rejects malformed ──────────────────────────────
    void data_from_string_rejects_wrong_field_count() {
        PaProfile p;
        // Wrong field count → reject.
        QVERIFY(!p.dataFromString(QStringLiteral("only|three|fields")));
        // Empty string → reject.
        QVERIFY(!p.dataFromString(QString{}));
        // Too many fields → reject.
        QString tooMany;
        for (int i = 0; i < 200; ++i) {
            tooMany += QStringLiteral("0|");
        }
        tooMany.chop(1);
        QVERIFY(!p.dataFromString(tooMany));
    }
};

QTEST_MAIN(TestPaProfile)
#include "tst_pa_profile.moc"
