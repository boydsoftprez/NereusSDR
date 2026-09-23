// From AetherSDR SpectrumWidget.cpp:11710-11733 [@1872028c] (dssStrengthToRgb
// / dssPaletteToken; NereusSDR's own dbmToRgb predates this plan and is
// Thetis-derived, not ported from here) plus NereusSDR-original test
// infrastructure. Task 8 of the 3D stacked-trace spectrum plan (design doc
// docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md, plan
// docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md).
//
// Pins the 3DSS palette LUT's decoupling from the 2D waterfall's gain /
// black-level knobs (waterfallKnobs_doNotMove3DColours is the point of the
// task: the 3D surface must map a stable colour aperture across the full
// colormap, gamma-shaped only by "3D Gain", so a high Ref level cannot
// compress every real signal into blue), the gamma identities (gain 50
// linear, 100 -> 0.25, 0 -> 4), that all schemes still work in 3D because
// the gradient stops are shared, and that the LUT re-bake token folds
// scheme + gain only.
//
// Deviation from the task brief, noted here for the coordinator: NereusSDR's
// WfColorScheme (SpectrumWidget.h:206-217) is NOT AetherSDR's own
// Default/Grayscale/BlueGreen/Fire/Plasma/Purple set -- it diverged during
// the Thetis-parity work of Phase 3G-8/3G-9b and is now
// Default/Enhanced/Spectran/BlackWhite/LinLog/LinRad/Custom/ClarityBlue
// (grep-confirmed the only WfColorScheme in the tree, stable since its
// 2026-04-19 introduction). BlackWhite stands in for the brief's Grayscale
// below -- it is a literal 2-stop black-to-white gradient (kBlackWhiteStops,
// SpectrumWidget.cpp:242-245), the same role Grayscale plays upstream --
// and Enhanced/LinRad stand in for the brief's Fire/Plasma in the token
// test, which only needs two distinct real schemes. setWfColorScheme() also
// takes WfColorScheme by value, not int, so the enum-to-int casts the brief
// used when calling it are dropped (setDssGain/setWfColorGain/
// setWfBlackLevel do take int, matching the brief).

#include <QTest>
#include <cmath>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestDssPalette : public QObject {
    Q_OBJECT

private slots:
    // Gamma identities from AetherSDR dssStrengthToRgb [@1872028c]:
    // gain 50 -> 1.0 (linear), gain 100 -> 0.25, gain 0 -> 4.
    void gain50_isLinear() {
        SpectrumWidget w;
        w.setDssGain(50);
        w.setWfColorScheme(WfColorScheme::BlackWhite);
        // BlackWhite runs black to white, so a linear ramp puts mid-strength
        // at mid-grey.
        const QRgb mid = w.dssStrengthToRgb(0.5f);
        QVERIFY(std::abs(qRed(mid) - 128) <= 4);
    }

    void gain100_liftsTowardTheFloor() {
        SpectrumWidget w;
        w.setWfColorScheme(WfColorScheme::BlackWhite);
        w.setDssGain(50);
        const int linear = qRed(w.dssStrengthToRgb(0.25f));
        w.setDssGain(100);
        const int lifted = qRed(w.dssStrengthToRgb(0.25f));
        QVERIFY2(lifted > linear,
                 "gain 100 must brighten weak signals, not dim them");
    }

    void gain0_coloursOnlyTheStrongest() {
        SpectrumWidget w;
        w.setWfColorScheme(WfColorScheme::BlackWhite);
        w.setDssGain(50);
        const int linear = qRed(w.dssStrengthToRgb(0.5f));
        w.setDssGain(0);
        const int suppressed = qRed(w.dssStrengthToRgb(0.5f));
        QVERIFY(suppressed < linear);
    }

    void strengthIsClamped() {
        SpectrumWidget w;
        const QRgb lo = w.dssStrengthToRgb(-1.0f);
        const QRgb hi = w.dssStrengthToRgb(2.0f);
        QCOMPARE(lo, w.dssStrengthToRgb(0.0f));
        QCOMPARE(hi, w.dssStrengthToRgb(1.0f));
    }

    // THE point of this task. Upstream bypasses dbmToRgb's waterfall gain
    // and black-level window on purpose (uploadDssPaletteLut
    // SpectrumWidget.cpp:12713-12741 [@1872028c]); otherwise the waterfall
    // sliders would silently reshape the 3D surface.
    //
    // Probes four strength values, not one: a mutation verification run
    // during this task found that a single probe at 0.4 is a blind spot for
    // the most direct "wired through dbmToRgb()" mutation -- dbmToRgb()
    // interprets its argument as a real dBm value (effective thresholds sit
    // around -120..-60), so ANY raw 0..1 strength saturates its `adjusted`
    // clamp to 1.0 regardless of the wfColorGain/wfBlackLevel sliders,
    // making the bug invisible at any single low/mid/high point in
    // isolation. Multiple probes close that hole for mutations that vary
    // with strength; they do NOT close it for the fully saturated case.
    //
    // Coverage boundary, recorded explicitly per review so a later
    // maintainer does not assume this one test guards every recoupling
    // shape: this test catches a mutation that maps strength across a
    // representative dBm span before handing off to dbmToRgb() (e.g.
    // `dbmToRgb(-140.0f + s * 140.0f)`), which is what an actual wrong port
    // of uploadDssPaletteLut would plausibly look like. It CANNOT catch a
    // literal straight-through recoupling, `return dbmToRgb(s);` -- that
    // mutation saturates every probe in [0,1] to the same top-of-gradient
    // colour regardless of scheme/gain, so every QCOMPARE above still
    // passes. The straight-through shape is instead caught by
    // gain50_isLinear / gain100_liftsTowardTheFloor /
    // gain0_coloursOnlyTheStrongest above, which fail because the
    // saturated output stops varying with `s` (and with `m_dssGain`) at
    // all, not because of anything this test asserts. Do not weaken those
    // three on the assumption this test alone covers recoupling; between
    // them they cover both the strength-dependent and the fully-saturated
    // mutation shapes. Both mutation runs are recorded with real output in
    // task-8-report.md "Verification 2".
    void waterfallKnobs_doNotMove3DColours() {
        static const float kProbes[] = {0.1f, 0.4f, 0.7f, 0.9f};
        SpectrumWidget w;
        w.setWfColorScheme(WfColorScheme::Default);
        w.setDssGain(70);
        QVector<QRgb> before;
        for (float s : kProbes) { before.append(w.dssStrengthToRgb(s)); }

        w.setWfColorGain(10);
        w.setWfBlackLevel(10);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(w.dssStrengthToRgb(kProbes[i]), before[i]);
        }

        w.setWfColorGain(120);
        w.setWfBlackLevel(120);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(w.dssStrengthToRgb(kProbes[i]), before[i]);
        }
    }

    // The scheme stops ARE shared, so every palette works in 3D.
    void everyScheme_producesDistinctColours() {
        SpectrumWidget w;
        w.setDssGain(50);
        QVector<QRgb> seen;
        for (int i = 0; i < static_cast<int>(WfColorScheme::Count); ++i) {
            w.setWfColorScheme(static_cast<WfColorScheme>(i));
            seen.append(w.dssStrengthToRgb(0.7f));
        }
        QCOMPARE(seen.size(), static_cast<int>(WfColorScheme::Count));
        // At least four of the rest must differ from the first.
        int distinct = 0;
        for (int i = 1; i < seen.size(); ++i) {
            if (seen[i] != seen[0]) { ++distinct; }
        }
        QVERIFY(distinct >= 4);
    }

    // The LUT re-bakes only on a real change, never on the per-frame floor
    // / range jitter.
    //
    // Tests both halves of "folds scheme and gain ONLY": the FOLDS half
    // (token reacts to scheme/gain) and the ONLY half (token ignores
    // wfColorGain/wfBlackLevel). A review pass on this task found the
    // original version only tested the former -- the exclusion held by
    // construction (dssPaletteToken()'s body is two lines and structurally
    // cannot read those fields), but a future edit that widens it, e.g.
    // copying upstream's differently-scoped 5-field dssPaletteToken() at
    // SpectrumWidget.cpp:11721-11733 [@1872028c], would pass every other
    // test in this file. Mutation-checked: see task-8-report.md
    // "Verification 4".
    void paletteToken_foldsSchemeAndGainOnly() {
        SpectrumWidget w;
        w.setWfColorScheme(WfColorScheme::Enhanced);
        w.setDssGain(70);
        const quint64 base = w.dssPaletteToken();
        QCOMPARE(w.dssPaletteToken(), base);

        // ONLY half: sweeping the waterfall knobs across both ends of their
        // documented ranges (0-100 / 0-125) must never move the token.
        w.setWfColorGain(0);
        QCOMPARE(w.dssPaletteToken(), base);
        w.setWfColorGain(100);
        QCOMPARE(w.dssPaletteToken(), base);
        w.setWfBlackLevel(0);
        QCOMPARE(w.dssPaletteToken(), base);
        w.setWfBlackLevel(125);
        QCOMPARE(w.dssPaletteToken(), base);

        // FOLDS half: scheme and gain are the only things that DO move it.
        w.setDssGain(71);
        QVERIFY(w.dssPaletteToken() != base);
        w.setDssGain(70);
        QCOMPARE(w.dssPaletteToken(), base);
        w.setWfColorScheme(WfColorScheme::LinRad);
        QVERIFY(w.dssPaletteToken() != base);
    }
};

QTEST_MAIN(TestDssPalette)
#include "tst_dss_palette.moc"
