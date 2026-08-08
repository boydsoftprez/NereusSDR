// =================================================================
// tests/tst_dss_angle.cpp  (NereusSDR)
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
//                 Safety properties test for the 3D Angle mapping of
//                 DssGeometry, verifying ridge-height derivation,
//                 monotonic convergence, and edge-case handling.
// =================================================================

#include <QTest>
#include <cmath>

#include "gui/DssGeometry.h"

using namespace NereusSDR;

class TestDssAngle : public QObject {
    Q_OBJECT

private slots:
    // Slider 50 must reproduce AetherSDR's fixed look bit for bit, or the
    // default view is not the view the design promised.
    void angle50_reproducesUpstreamExactly() {
        const DssShape s = dssShapeForAngle(50);
        QVERIFY(std::abs(s.backWidthFrac
                         - kDssUpstreamShape.backWidthFrac)     < 1e-6f);
        QVERIFY(std::abs(s.depthSpanFrac
                         - kDssUpstreamShape.depthSpanFrac)     < 1e-6f);
        QVERIFY(std::abs(s.frontMaxRidgeFrac
                         - kDssUpstreamShape.frontMaxRidgeFrac) < 1e-6f);
    }

    // The whole point of deriving ridge height instead of exposing it: no
    // angle may push a back-row peak off the top of the plot.
    void everyAngle_keepsRidgesOnScreen() {
        for (int a = 0; a <= 100; ++a) {
            const DssShape s = dssShapeForAngle(a);
            // Worst case: full-scale signal on the deepest row.
            const QPointF back = dssProjectSurface(
                0.5f, 1.0f, 0.0f, -100.0f, 100.0f, 1.0f, s);
            QVERIFY2(back.y() >= 0.0,
                     qPrintable(QStringLiteral("back ridge off top at angle %1"
                                               " (y=%2)").arg(a).arg(back.y())));
            const QPointF front = dssProjectSurface(
                0.5f, 0.0f, 0.0f, -100.0f, 100.0f, 1.0f, s);
            QVERIFY2(front.y() >= 0.0,
                     qPrintable(QStringLiteral("front ridge off top at angle %1")
                                    .arg(a)));
            QVERIFY(front.y() <= 1.0);
        }
    }

    // Monotonic travel: raising the angle spreads the stack further up the
    // plot and weakens the convergence, together, like a real camera.
    void angle_movesSpreadAndConvergenceTogether() {
        for (int a = 0; a < 100; ++a) {
            const DssShape lo = dssShapeForAngle(a);
            const DssShape hi = dssShapeForAngle(a + 1);
            QVERIFY(hi.backWidthFrac > lo.backWidthFrac);
            QVERIFY(hi.depthSpanFrac > lo.depthSpanFrac);
        }
    }

    // Ridge height falls as the view tips toward top-down, once past the
    // clamp that bounds the edge-on end.
    void ridgeHeight_shrinksAsAngleRises() {
        const DssShape edgeOn = dssShapeForAngle(0);
        const DssShape mid    = dssShapeForAngle(50);
        const DssShape topDown = dssShapeForAngle(100);
        QCOMPARE(edgeOn.frontMaxRidgeFrac, kDssMaxRidgeFrac);  // clamped
        QVERIFY(mid.frontMaxRidgeFrac    < edgeOn.frontMaxRidgeFrac);
        QVERIFY(topDown.frontMaxRidgeFrac < mid.frontMaxRidgeFrac);
    }

    void anglePercent_isClamped() {
        const DssShape lo = dssShapeForAngle(-50);
        const DssShape hi = dssShapeForAngle(500);
        QCOMPARE(lo.backWidthFrac, dssShapeForAngle(0).backWidthFrac);
        QCOMPARE(hi.backWidthFrac, dssShapeForAngle(100).backWidthFrac);
    }

    // Widest span occurs at the most dramatic angle. Task 3's mesh sizing
    // depends on this being the low end of the travel.
    void maxRowSpan_isWidestAtAngleZero() {
        const float atZero = dssMaxRowSpanFactor(dssShapeForAngle(0));
        const float atFifty = dssMaxRowSpanFactor(dssShapeForAngle(50));
        const float atHundred = dssMaxRowSpanFactor(dssShapeForAngle(100));
        QVERIFY(atZero > atFifty);
        QVERIFY(atFifty > atHundred);
        QVERIFY(std::abs(atFifty - 1.0f / 0.60f) < 1e-6f);
    }
};

QTEST_APPLESS_MAIN(TestDssAngle)
#include "tst_dss_angle.moc"
