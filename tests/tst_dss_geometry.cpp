// =================================================================
// tests/tst_dss_geometry.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Golden test for the perspective geometry ported from
//                 AetherSDR `src/gui/DssRenderer.h`.
// =================================================================

#include <QTest>
#include <cmath>
#include <limits>

#include "gui/DssGeometry.h"

using namespace NereusSDR;

class TestDssGeometry : public QObject {
    Q_OBJECT

private slots:
    // The crux test. If these drift, the surface stops looking like
    // AetherSDR's and nothing else in the suite will notice.
    void upstreamConstants_areExact() {
        QCOMPARE(kDssUpstreamShape.backWidthFrac,     0.60f);
        QCOMPARE(kDssUpstreamShape.depthSpanFrac,     0.58f);
        QCOMPARE(kDssUpstreamShape.frontMaxRidgeFrac, 0.46f);
        QCOMPARE(kDssHaze,          0.16f);
        QCOMPARE(kDssColorSpanDb,  45.0f);
        QCOMPARE(kDssCols,          768);
        QCOMPARE(kDssVisibleRows,    96);
        QCOMPARE(kDssRows,          104);
    }

    void depthScale_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        QCOMPARE(dssDepthScale(0.0f, s), 1.0f);
        QCOMPARE(dssDepthScale(1.0f, s), 0.60f);
        // depth 0.5 -> 1 - 0.5 * (1 - 0.60) = 0.80
        QVERIFY(std::abs(dssDepthScale(0.5f, s) - 0.80f) < 1e-6f);
        // clamps outside [0,1]
        QCOMPARE(dssDepthScale(-1.0f, s), 1.0f);
        QCOMPARE(dssDepthScale(2.0f, s),  0.60f);
    }

    void projectPerspective_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        // Centre frequency never moves horizontally at any depth.
        for (float d : {0.0f, 0.25f, 0.5f, 1.0f}) {
            const QPointF p = dssProjectPerspective(0.5f, d, s);
            QVERIFY(std::abs(p.x() - 0.5) < 1e-6);
        }
        // Front edge spans the full width; back edge narrows to 0.60.
        QVERIFY(std::abs(dssProjectPerspective(0.0f, 0.0f, s).x() - 0.0) < 1e-6);
        QVERIFY(std::abs(dssProjectPerspective(1.0f, 0.0f, s).x() - 1.0) < 1e-6);
        QVERIFY(std::abs(dssProjectPerspective(0.0f, 1.0f, s).x() - 0.20) < 1e-6);
        QVERIFY(std::abs(dssProjectPerspective(1.0f, 1.0f, s).x() - 0.80) < 1e-6);
        // Baseline rises with depth by depthSpanFrac.
        QVERIFY(std::abs(dssProjectPerspective(0.5f, 0.0f, s).y() - 1.0)  < 1e-6);
        QVERIFY(std::abs(dssProjectPerspective(0.5f, 1.0f, s).y() - 0.42) < 1e-6);
    }

    void projectSurface_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        // At the floor the surface sits exactly on the baseline.
        const QPointF atFloor =
            dssProjectSurface(0.5f, 0.0f, -120.0f, -120.0f, 60.0f, 1.0f, s);
        QVERIFY(std::abs(atFloor.y() - 1.0) < 1e-6);
        // Full scale at the front rises by frontMaxRidgeFrac * width(=1).
        const QPointF atPeak =
            dssProjectSurface(0.5f, 0.0f, -60.0f, -120.0f, 60.0f, 1.0f, s);
        QVERIFY(std::abs(atPeak.y() - (1.0 - 0.46)) < 1e-6);
        // Far ridges are shorter by the depth width factor.
        const QPointF atBack =
            dssProjectSurface(0.5f, 1.0f, -60.0f, -120.0f, 60.0f, 1.0f, s);
        QVERIFY(std::abs(atBack.y() - (0.42 - 0.46 * 0.60)) < 1e-6);
        // A non-finite dBm is pinned to the floor, not propagated as NaN.
        const QPointF nan = dssProjectSurface(
            0.5f, 0.0f, std::nanf(""), -120.0f, 60.0f, 1.0f, s);
        QVERIFY(std::abs(nan.y() - 1.0) < 1e-6);
    }

    void rowSpanFactor_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        QVERIFY(std::abs(dssMaxRowSpanFactor(s) - (1.0f / 0.60f)) < 1e-6f);
        // Not wider than the viewport -> classic clipped trapezoid.
        QCOMPARE(dssRowSpanFactorFor(1.0, 1.0, 100, s), 1.0f);
        QCOMPARE(dssRowSpanFactorFor(0.5, 1.0, 100, s), 1.0f);
        // Non-finite or non-positive inputs are refused, not extrapolated.
        QCOMPARE(dssRowSpanFactorFor(2.0, 0.0, 100, s), 1.0f);
        QCOMPARE(dssRowSpanFactorFor(
            std::numeric_limits<double>::quiet_NaN(), 1.0, 100, s), 1.0f);
        // The percentage scales the AVAILABLE span, not the absolute maximum.
        // 1.5x available at 100% -> 1.5; at 50% -> 1.25.
        QVERIFY(std::abs(dssRowSpanFactorFor(1.5, 1.0, 100, s) - 1.5f)  < 1e-6f);
        QVERIFY(std::abs(dssRowSpanFactorFor(1.5, 1.0,  50, s) - 1.25f) < 1e-6f);
        QCOMPARE(dssRowSpanFactorFor(1.5, 1.0, 0, s), 1.0f);
        // Clamped at the max: past it the extra data is off-plot anyway.
        QVERIFY(std::abs(dssRowSpanFactorFor(10.0, 1.0, 100, s)
                         - dssMaxRowSpanFactor(s)) < 1e-6f);
    }

    void wedgeFreeDepth_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        QCOMPARE(dssWedgeFreeDepth(1.0f, s), 0.0f);
        QCOMPARE(dssWedgeFreeDepth(dssMaxRowSpanFactor(s), s), 1.0f);
    }

    void rowFrequencyUnit_isDepthIndependent() {
        // Widening a row extends the frequency range it covers, never where
        // a frequency lands on screen.
        QCOMPARE(dssRowFrequencyUnit(0.5f, 1.0f), 0.5f);
        QCOMPARE(dssRowFrequencyUnit(0.5f, 2.0f), 0.5f);
        QCOMPARE(dssRowFrequencyUnit(1.0f, 2.0f), 1.5f);
        QCOMPARE(dssRowFrequencyUnit(0.0f, 2.0f), -0.5f);
    }
};

QTEST_APPLESS_MAIN(TestDssGeometry)
#include "tst_dss_geometry.moc"
