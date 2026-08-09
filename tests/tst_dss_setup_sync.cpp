// tests/tst_dss_setup_sync.cpp
//
// no-port-check: NereusSDR-original test infrastructure for the 3D
// Stacked-Trace Spectrum Plan Task 15 Setup mirror. Setup > Display gaining
// a second surface for the six 3D controls (alongside the Task 13
// right-click overlay menu) has no AetherSDR Setup-dialog counterpart --
// AetherSDR's own RadioSetupDialog.{h,cpp} (7874 lines at the pinned commit)
// has zero occurrences of "3D VIEW"/"3D Angle"/"dssAngle"/"dssFloor"/
// "dssGain"/"dssRowSpan", confirmed by a direct grep against
// git -C ../AetherSDR show 1872028c:src/gui/RadioSetupDialog.cpp. The
// design doc's own divergence table (docs/architecture/2026-08-08-3d-
// stacked-trace-spectrum-design.md section 7, "Controls in overlay menu
// only" row) already records this as a NereusSDR-original decision:
// "Our operators expect Setup parity."
//
// 3D stacked-trace spectrum plan Task 15: mirrors the six 3D controls the
// Task 13 SpectrumOverlayMenu already exposes into a Display3DSetupPage
// under Setup -> Display. Two surfaces now edit the same six values on the
// same live SpectrumWidget, which CLAUDE.md calls out as exactly the
// feedback-loop shape that needs a guard: an edit on either surface must
// settle in one hop, not echo back and forth.
//
// Design: docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md
// Plan:   docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md

#include <QTest>
#include <QSignalSpy>
#include <QSlider>

#include "gui/SpectrumWidget.h"
#include "gui/setup/DisplaySetupPages.h"

using namespace NereusSDR;

class TestDssSetupSync : public QObject {
    Q_OBJECT

private slots:
    void setupPageEdit_reachesTheWidget() {
        SpectrumWidget w;
        Display3DSetupPage page(&w);
        page.findChild<QSlider*>(QStringLiteral("setup3DAngleSlider"))->setValue(70);
        QCOMPARE(w.dssAngle(), 70);
    }

    void widgetChange_updatesTheSetupPage() {
        SpectrumWidget w;
        Display3DSetupPage page(&w);
        w.setDssGain(25);
        QCOMPARE(page.findChild<QSlider*>(
                     QStringLiteral("setup3DGainSlider"))->value(), 25);
    }

    // The guard that stops the two surfaces echoing each other forever.
    void roundTrip_settlesWithoutAnEchoLoop() {
        SpectrumWidget w;
        Display3DSetupPage page(&w);
        QSignalSpy spy(&w, &SpectrumWidget::dssAngleChanged);
        page.findChild<QSlider*>(QStringLiteral("setup3DAngleSlider"))->setValue(80);
        QCOMPARE(w.dssAngle(), 80);
        QCOMPARE(spy.count(), 1);   // exactly one, not a cascade
    }

    void resetButton_restoresEveryDefault() {
        SpectrumWidget w;
        Display3DSetupPage page(&w);
        w.setDssFloorDepth(20);
        w.setDssGain(10);
        w.setDssRowSpan(5);
        w.setDssAngle(90);
        page.resetToDefaultsForTest();
        QCOMPARE(w.dssFloorDepth(),  6);
        QCOMPARE(w.dssGain(),       70);
        QCOMPARE(w.dssRowSpan(),   100);
        QCOMPARE(w.dssAngle(),      50);
    }
};

QTEST_MAIN(TestDssSetupSync)
#include "tst_dss_setup_sync.moc"
