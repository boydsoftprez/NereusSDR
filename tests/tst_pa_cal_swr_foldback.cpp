// no-port-check: NereusSDR-original unit-test file.  The mi0bot
// NetworkIO.cs reference below is a cite comment documenting which
// upstream behaviour the assertions verify; no C# is translated here.
// =================================================================
// tests/tst_pa_cal_swr_foldback.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::swrProtectFactor + the foldback-aware
// percent-to-wire-byte scaling applied at every RadioModel setTxDrive
// site.
//
// Plan: docs/architecture/2026-05-02-p1-full-parity-plan.md §3.5.
//
// Source — mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1 @c26a8a4]:
//   public static void SetOutputPowerFactor(int level) {
//       int i = (int)(255 * f * _swr_protect);   // f normalised 0..1,
//                                                // _swr_protect ≤ 1.0
//       NetworkIO.SetOutputPowerFactor(i);
//   }
//
// Coverage:
//   - Property: default value, clamp negative, clamp above-one,
//     change-signal emission + de-dup-on-no-op.
//   - Wire byte: identity at factor=1.0 (50% → 127), half-foldback
//     at factor=0.5 (100% → 127), zero-foldback (any power → 0).
//
// Strategy:
//   The wire-byte computation is exposed as a static helper
//   `RadioModel::computeWireDriveForTest(int powerPct, float swrFactor)`
//   under NEREUS_BUILD_TESTS — same test-seam pattern Task 3.4 used
//   (handlePaTelemetryForTest).  The helper is a pure function that
//   reproduces the exact `clamp(int(255.0f * f * swrProtect), 0, 255)`
//   formula inlined at all three setTxDrive sites in RadioModel.cpp,
//   so a regression in the helper is a regression in production.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TstPaCalSwrFoldback : public QObject {
    Q_OBJECT

private slots:

    // ── TransmitModel::swrProtectFactor property ──────────────────────────

    void default_factor_is_one() {
        TransmitModel t;
        QCOMPARE(t.swrProtectFactor(), 1.0f);
    }

    void negative_factor_clamped_to_zero() {
        TransmitModel t;
        t.setSwrProtectFactor(-0.5f);
        QCOMPARE(t.swrProtectFactor(), 0.0f);
    }

    void above_one_factor_clamped_to_one() {
        TransmitModel t;
        t.setSwrProtectFactor(1.5f);
        QCOMPARE(t.swrProtectFactor(), 1.0f);
    }

    void mid_factor_round_trips_unchanged() {
        TransmitModel t;
        t.setSwrProtectFactor(0.7f);
        QCOMPARE(t.swrProtectFactor(), 0.7f);
    }

    void property_changed_signal_emits_once_on_change() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::swrProtectFactorChanged);
        t.setSwrProtectFactor(0.7f);
        // Setting the same clamped value again should be a no-op.
        t.setSwrProtectFactor(0.7f);
        QCOMPARE(spy.count(), 1);
    }

    void property_changed_signal_silent_on_noop() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::swrProtectFactorChanged);
        // Default is 1.0; setting 1.0 must be silent.
        t.setSwrProtectFactor(1.0f);
        QCOMPARE(spy.count(), 0);
    }

    void clamped_value_dedups_against_already_clamped_value() {
        TransmitModel t;
        t.setSwrProtectFactor(2.0f);  // clamps to 1.0
        QSignalSpy spy(&t, &TransmitModel::swrProtectFactorChanged);
        // 1.5 also clamps to 1.0 — should be a silent no-op.
        t.setSwrProtectFactor(1.5f);
        QCOMPARE(spy.count(), 0);
    }

    // ── Wire-byte formula (identity at factor=1.0) ────────────────────────

    void no_foldback_at_factor_1_0_zero_power() {
        // 0% → 0
        QCOMPARE(RadioModel::computeWireDriveForTest(0, 1.0f), 0);
    }

    void no_foldback_at_factor_1_0_full_power() {
        // 100% → 255
        QCOMPARE(RadioModel::computeWireDriveForTest(100, 1.0f), 255);
    }

    void no_foldback_at_factor_1_0_half_power() {
        // 50% → 127  (= int(0.5 * 255 * 1.0))
        QCOMPARE(RadioModel::computeWireDriveForTest(50, 1.0f), 127);
    }

    // ── Wire-byte formula (foldback active) ───────────────────────────────

    void half_foldback_at_factor_0_5() {
        // 100% × 0.5 → 127  (= int(1.0 * 255 * 0.5))
        QCOMPARE(RadioModel::computeWireDriveForTest(100, 0.5f), 127);
    }

    void zero_foldback_emits_zero_drive_at_full_power() {
        // 100% × 0.0 → 0
        QCOMPARE(RadioModel::computeWireDriveForTest(100, 0.0f), 0);
    }

    void zero_foldback_emits_zero_drive_at_half_power() {
        // 50% × 0.0 → 0
        QCOMPARE(RadioModel::computeWireDriveForTest(50, 0.0f), 0);
    }

    // ── Wire-byte formula (clamp behaviour) ───────────────────────────────

    void negative_factor_inside_helper_clamps_to_zero() {
        // The helper applies the same clamp as the property setter so a
        // direct out-of-range call (e.g. from a stale binding) cannot
        // exceed wire-byte limits.
        QCOMPARE(RadioModel::computeWireDriveForTest(100, -1.0f), 0);
    }

    void above_one_factor_inside_helper_clamps_to_one() {
        // factor > 1.0 must not amplify the wire byte beyond the
        // identity-path maximum (= power / 100 * 255).
        QCOMPARE(RadioModel::computeWireDriveForTest(100, 2.0f), 255);
    }

    void out_of_range_power_clamps_to_zero_and_max() {
        // power below 0 → wire 0; power above 100 → wire 255.
        QCOMPARE(RadioModel::computeWireDriveForTest(-10, 1.0f), 0);
        QCOMPARE(RadioModel::computeWireDriveForTest(150, 1.0f), 255);
    }
};

QTEST_GUILESS_MAIN(TstPaCalSwrFoldback)
#include "tst_pa_cal_swr_foldback.moc"
