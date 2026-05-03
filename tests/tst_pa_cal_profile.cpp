// tst_pa_cal_profile.cpp
//
// no-port-check: Test file references Thetis behaviour in commentary only;
// no Thetis source is translated here. Thetis cites are in PaCalProfile.cpp.
//
// Covers:
//   - paCalBoardClassFor(HPSDRModel) maps every HPSDRModel value to the
//     correct PaCalBoardClass per Thetis console.cs:6717-6752 [v2.10.3.13].
//   - PaCalProfile::defaults(class) returns factory tables matching the
//     Thetis spinbox label values (ud10PA{1..10}W, ud100PA{10..100}W,
//     ud200PA{20..200}W).
//   - PaCalProfile::interval() returns the per-class watts-between-labels
//     interval (1.0/10.0/20.0/0.5/0.0).
//   - PaCalProfile::interpolate matches Thetis PowerKernel
//     (console.cs:6756-6768 [v2.10.3.13]) for: zero, identity-default
//     round-trip, calibrated points, between-points linearity, above-max
//     extrapolation, None-class passthrough, and below-zero values.

#include <QtTest/QtTest>

#include "core/HpsdrModel.h"
#include "core/PaCalProfile.h"

using namespace NereusSDR;

class TestPaCalProfile : public QObject {
    Q_OBJECT

private slots:
    // ── paCalBoardClassFor: every HPSDRModel value covered ───────────────
    void board_class_anan10() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN10), PaCalBoardClass::Anan10);
    }
    void board_class_anan10e() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN10E), PaCalBoardClass::Anan10);
    }
    void board_class_anan100() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN100), PaCalBoardClass::Anan100);
    }
    void board_class_anan100b() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN100B), PaCalBoardClass::Anan100);
    }
    void board_class_anan100d() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN100D), PaCalBoardClass::Anan100);
    }
    void board_class_anan200d() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN200D), PaCalBoardClass::Anan100);
    }
    void board_class_anan7000d() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN7000D), PaCalBoardClass::Anan100);
    }
    void board_class_anan8000d() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN8000D), PaCalBoardClass::Anan8000);
    }
    void board_class_anan_g2() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN_G2), PaCalBoardClass::Anan100);
    }
    void board_class_anan_g2_1k() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANAN_G2_1K), PaCalBoardClass::Anan100);
    }
    void board_class_anvelinapro3() {
        // Thetis console.cs:6736 [v2.10.3.13] explicit case → 10 W intervals
        // → Anan100 class. (User-instructions parenthetical: "verify by
        // reading Thetis source — if AnvelinaPro3 has a cal table,
        // classify it accordingly". It does, so it's Anan100.)
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ANVELINAPRO3), PaCalBoardClass::Anan100);
    }
    void board_class_redpitaya() {
        // Thetis console.cs:6739 [v2.10.3.13] explicit case //DH1KLM → 10 W
        // intervals → Anan100 class.
        QCOMPARE(paCalBoardClassFor(HPSDRModel::REDPITAYA), PaCalBoardClass::Anan100);
    }
    void board_class_hermes() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::HERMES), PaCalBoardClass::Anan100);
    }
    void board_class_orionmkii() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::ORIONMKII), PaCalBoardClass::Anan100);
    }
    void board_class_hermeslite() {
        // Source: mi0bot setup.cs:5463-5466 [v2.10.3.13-beta2 @c26a8a4]
        // HL2 explicitly grouped with ANAN10/ANAN10E for PA cal — uses
        // ud10PA1W..ud10PA10W (1 W intervals, 10 W max). Earlier NereusSDR
        // placeholder (PaCalBoardClass::HermesLite, 0.5 W / 5 W max) was
        // dropped 2026-05-02 after upstream verification.
        // (Production code uses [v2.10.3.13-beta2] only; sha is allowed in
        // test files for richer cite form.)
        // Upstream inline tag preserved verbatim from mi0bot setup.cs:5463:
        //   case HPSDRModel.HERMESLITE:     // MI0BOT: HL2
        QCOMPARE(paCalBoardClassFor(HPSDRModel::HERMESLITE), PaCalBoardClass::Anan10);
    }
    void board_class_atlas() {
        // HPSDR/Atlas is a discrete-board kit with no integrated PA.
        // NereusSDR design choice: classify as None so the FWD power UI
        // hides the cal group. Thetis would route this to default 10 W.
        QCOMPARE(paCalBoardClassFor(HPSDRModel::HPSDR), PaCalBoardClass::None);
    }
    void board_class_first_last_sentinels() {
        QCOMPARE(paCalBoardClassFor(HPSDRModel::FIRST), PaCalBoardClass::None);
        QCOMPARE(paCalBoardClassFor(HPSDRModel::LAST), PaCalBoardClass::None);
    }

    // ── defaults(class): factory cal tables ──────────────────────────────
    void defaults_anan10_returns_label_values() {
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan10);
        QCOMPARE(p.boardClass, PaCalBoardClass::Anan10);
        QCOMPARE(p.watts[0], 0.0f);
        QCOMPARE(p.watts[1], 1.0f);
        QCOMPARE(p.watts[2], 2.0f);
        QCOMPARE(p.watts[5], 5.0f);
        QCOMPARE(p.watts[10], 10.0f);
        QCOMPARE(p.interval(), 1.0f);
    }
    void defaults_anan100_returns_label_values() {
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        QCOMPARE(p.boardClass, PaCalBoardClass::Anan100);
        QCOMPARE(p.watts[0], 0.0f);
        QCOMPARE(p.watts[1], 10.0f);
        QCOMPARE(p.watts[5], 50.0f);
        QCOMPARE(p.watts[10], 100.0f);
        QCOMPARE(p.interval(), 10.0f);
    }
    void defaults_anan8000_returns_label_values() {
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan8000);
        QCOMPARE(p.boardClass, PaCalBoardClass::Anan8000);
        QCOMPARE(p.watts[0], 0.0f);
        QCOMPARE(p.watts[1], 20.0f);
        QCOMPARE(p.watts[5], 100.0f);
        QCOMPARE(p.watts[10], 200.0f);
        QCOMPARE(p.interval(), 20.0f);
    }
    void hermesLiteMapsToAnan10Defaults() {
        // mi0bot setup.cs:5463-5466 [v2.10.3.13-beta2 @c26a8a4] — HL2 uses
        // ud10PA1W..ud10PA10W (1 W intervals, 10 W max) — i.e. Anan10.
        // Verifies the full pipeline: paCalBoardClassFor(HERMESLITE) →
        // Anan10, then defaults(Anan10) returns the same {0,1,...,10} table
        // as ANAN-10/10E.
        const PaCalProfile p =
            PaCalProfile::defaults(paCalBoardClassFor(HPSDRModel::HERMESLITE));
        QCOMPARE(p.boardClass, PaCalBoardClass::Anan10);
        QCOMPARE(p.interval(), 1.0f);
        QCOMPARE(p.watts[0], 0.0f);
        QCOMPARE(p.watts[1], 1.0f);
        QCOMPARE(p.watts[10], 10.0f);
    }
    void defaults_none_returns_all_zeros() {
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::None);
        QCOMPARE(p.boardClass, PaCalBoardClass::None);
        for (float w : p.watts) {
            QCOMPARE(w, 0.0f);
        }
        QCOMPARE(p.interval(), 0.0f);
    }

    // ── interpolate: zero passes through ─────────────────────────────────
    void interpolate_zero_returns_zero_for_all_classes() {
        for (PaCalBoardClass cls : {
                 PaCalBoardClass::Anan10,
                 PaCalBoardClass::Anan100,
                 PaCalBoardClass::Anan8000,
             }) {
            const PaCalProfile p = PaCalProfile::defaults(cls);
            QCOMPARE(p.interpolate(0.0f), 0.0f);
        }
    }

    // ── interpolate: identity round-trip with default tables ─────────────
    void interpolate_identity_for_anan100_defaults() {
        // Thetis PowerKernel with PAsets[i] = i*interval is an identity
        // transform: rawWatts → rawWatts. Verify at multiple points.
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        QCOMPARE(p.interpolate(10.0f), 10.0f);
        QCOMPARE(p.interpolate(50.0f), 50.0f);
        QCOMPARE(p.interpolate(100.0f), 100.0f);
        // Halfway between cal points is also identity.
        QCOMPARE(p.interpolate(15.0f), 15.0f);
        QCOMPARE(p.interpolate(75.0f), 75.0f);
    }

    void interpolate_identity_for_anan10_defaults() {
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan10);
        QCOMPARE(p.interpolate(1.0f), 1.0f);
        QCOMPARE(p.interpolate(5.0f), 5.0f);
        QCOMPARE(p.interpolate(10.0f), 10.0f);
    }

    // ── interpolate: at calibrated points (Thetis semantics) ─────────────
    void interpolate_at_calibrated_point_returns_label_value() {
        // Thetis PowerKernel returns `interval * (idx + frac)` — when raw
        // matches a cal-table entry exactly, idx lands on that entry and
        // frac = 0 (or 1 on the previous segment), so the return value is
        // `interval * idx` — the spinbox LABEL value, not the cal value.
        //
        // Concrete example: ANAN-100 with watts[5] = 47.0f means "when
        // external meter showed 50 W, Thetis raw read 47 W". So feeding
        // raw = 47.0 back through the calibration recovers the real-world
        // 50 W (= idx 5 × 10 W interval).
        PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        p.watts[5] = 47.0f;
        QCOMPARE(p.interpolate(47.0f), 50.0f);
    }

    void interpolate_between_calibrated_points_is_piecewise_linear() {
        // ANAN-100 with watts[1] = 12.0f, watts[2] = 22.0f:
        //   "10 W external read 12 W raw, 20 W external read 22 W raw".
        // Feeding raw = 17 W (halfway between 12 and 22) recovers
        // halfway-between-real (10 + 5 = 15 W).
        PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        p.watts[1] = 12.0f;
        p.watts[2] = 22.0f;
        // PowerKernel: idx=1, frac=(17-12)/(22-12)=0.5, return=10*(1+0.5)=15.
        QCOMPARE(p.interpolate(17.0f), 15.0f);
    }

    void interpolate_above_max_extrapolates_with_default_table() {
        // Default ANAN-100 table is identity, so above-max also returns
        // identity (linear extrapolation of identity = identity).
        // PowerKernel: rawWatts=150, table[10]=100, idx=9, frac=(150-90)/(100-90)=6.0,
        //   return = 10 * ((1-6)*9 + 6*10) = 10 * (-45 + 60) = 150.
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        QCOMPARE(p.interpolate(150.0f), 150.0f);
        // Same shape for ANAN-10 above 10 W:
        const PaCalProfile p10 = PaCalProfile::defaults(PaCalBoardClass::Anan10);
        QCOMPARE(p10.interpolate(15.0f), 15.0f);
    }

    void interpolate_above_max_extrapolates_user_calibrated_table() {
        // ANAN-100 with watts[9]=85, watts[10]=110 (user cal):
        //   Above-max raw = 135, segment = [9,10] (entries-2..entries-1),
        //   frac = (135-85)/(110-85) = 50/25 = 2.0,
        //   return = 10 * ((1-2)*9 + 2*10) = 10 * (-9 + 20) = 110 W?
        // Wait, that's only 110. Trace again carefully:
        //   raw=135 > table[10]=110, so idx = entries-2 = 9.
        //   frac = (135-table[9])/(table[10]-table[9]) = (135-85)/(110-85) = 2.0.
        //   return = interval * ((1-frac)*idx + frac*(idx+1))
        //          = 10 * ((1-2)*9 + 2*10)
        //          = 10 * (-9 + 20)
        //          = 110.
        // So the calibrated real-world is 110 W when raw = 135 W on this
        // user-edited table. (Linear extrapolation of the last segment.)
        PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        p.watts[9] = 85.0f;
        p.watts[10] = 110.0f;
        QCOMPARE(p.interpolate(135.0f), 110.0f);
    }

    // ── interpolate: None class passes through ────────────────────────────
    void interpolate_none_class_is_identity() {
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::None);
        QCOMPARE(p.interpolate(0.0f), 0.0f);
        QCOMPARE(p.interpolate(50.0f), 50.0f);
        QCOMPARE(p.interpolate(-3.0f), -3.0f);
        QCOMPARE(p.interpolate(1000.0f), 1000.0f);
    }

    // ── interpolate: below zero ──────────────────────────────────────────
    void interpolate_below_zero_with_default_table_returns_input() {
        // PowerKernel with default ANAN-100 table at raw = -5:
        //   raw > table[10]? No.
        //   while (raw > table[0]=0) — false, loop skipped. idx=0.
        //   if (idx > 0) — false, no decrement. idx=0.
        //   frac = (-5 - 0) / (10 - 0) = -0.5.
        //   return = 10 * ((1.5)*0 + (-0.5)*1) = -5.
        // Negative input → identity passthrough on default tables.
        const PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        QCOMPARE(p.interpolate(-5.0f), -5.0f);
    }

    void interpolate_handles_degenerate_zero_span_gracefully() {
        // Pathological: two adjacent cal points equal. Thetis would yield
        // Inf or NaN; NereusSDR guards and returns rawWatts unchanged.
        // (Thetis itself doesn't see this in practice because the spinbox
        // model prevents non-monotonic entries, but we guard defensively.)
        PaCalProfile p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        p.watts[5] = 50.0f;
        p.watts[6] = 50.0f;  // zero span between idx=5 and idx=6
        // Raw=50 hits the boundary; idx ends at 4 with non-zero span,
        // so this path is fine — just verifying no crash.
        const float result = p.interpolate(50.0f);
        Q_UNUSED(result);
        // Now force a raw that lands directly inside the zero-span segment.
        // (idx=5 is reached when raw > table[5]=50 but raw <= table[6]=50,
        // which is impossible; the guard fires only via the upper-bound
        // path. To exercise it, set table[10]=table[9]:)
        p.watts[9] = 90.0f;
        p.watts[10] = 90.0f;  // zero span at the upper end
        // Raw above the table → idx = entries-2 = 9, span = 90-90 = 0.
        // The guard triggers and returns rawWatts.
        QCOMPARE(p.interpolate(150.0f), 150.0f);
    }
};

QTEST_MAIN(TestPaCalProfile)
#include "tst_pa_cal_profile.moc"
