// =================================================================
// tests/tst_display_settings_binding.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure for the
// NereusSDR-original SpectrumWidget <-> DisplaySettingsModel binding
// (3D Stacked-Trace Spectrum Plan Task 18). Fixture shape (AppSettings
// seeding, widget construction, show and expose, sendMouse()/
// dragDbmStrip()) follows tests/tst_dss_floor_drag.cpp.
//
// Isolation: AppSettings::instance() is a process-wide in-memory
// singleton with no automatic load()/save() in this test process
// (TestSandboxInit.cpp also sandboxes QStandardPaths so nothing here
// can reach the developer's real ~/.config/NereusSDR/NereusSDR.settings).
// init()/cleanup() clear it before and after every slot, matching
// tst_display_settings_model.cpp, so no test's writes can leak into
// another's.
// =================================================================

#include <QtTest/QtTest>
#include <QMouseEvent>
#include <QSignalSpy>
#include <cmath>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"
#include "models/DisplaySettingsModel.h"

using namespace NereusSDR;

namespace {

// Same idiom as tst_dss_floor_drag.cpp's sendMouse(): a directly
// synthesized QMouseEvent via QApplication::sendEvent is this project's
// established way to drive SpectrumWidget's mouse handlers headlessly.
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos,
               Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   button, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &me);
}

// Press, move by dy pixels, release -- at the dBm strip's drag-pan zone
// (below the kDbmArrowH-tall up/down arrow row). Identical to
// tst_dss_floor_drag.cpp's helper of the same name.
void dragDbmStrip(SpectrumWidget& w, int dy)
{
    const int mx = w.width() - 18;   // centre of the 36px-wide strip
    const int y0 = 60;               // clear of the 14px arrow row
    sendMouse(&w, QEvent::MouseButtonPress, QPoint(mx, y0),
              Qt::LeftButton, Qt::LeftButton);
    sendMouse(&w, QEvent::MouseMove, QPoint(mx, y0 + dy),
              Qt::NoButton, Qt::LeftButton);
    sendMouse(&w, QEvent::MouseButtonRelease, QPoint(mx, y0 + dy),
              Qt::LeftButton, Qt::NoButton);
}

// Press at the divider row, move to absolute Y newY, release. The
// divider block in mouseMoveEvent reads the CURRENT event's absolute Y
// (not a press-relative delta), so newY alone determines the resulting
// split fraction: frac = newY / height().
void dragDivider(SpectrumWidget& w, int newY)
{
    const int mx = w.width() / 2;
    const int dividerY = w.notchSpecRectForTest().height();
    sendMouse(&w, QEvent::MouseButtonPress, QPoint(mx, dividerY),
              Qt::LeftButton, Qt::LeftButton);
    sendMouse(&w, QEvent::MouseMove, QPoint(mx, newY),
              Qt::NoButton, Qt::LeftButton);
    sendMouse(&w, QEvent::MouseButtonRelease, QPoint(mx, newY),
              Qt::LeftButton, Qt::NoButton);
}

// Common fixture every test needs: resize() before anything else
// (mousePressEvent's hit tests read width()/height(), both 0 on an
// unresized widget), show() + qWaitForWindowExposed() (Global
// Constraints), and setConnectionState(Connected) (mousePressEvent's
// Phase 3Q-8 guard swallows every left-click while not Connected).
// 1000x1000 keeps specH large under either of specHFromHeight's two
// layout formulas (GPU vs CPU-only build).
void setUpWidget(SpectrumWidget& w)
{
    w.resize(1000, 1000);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.setConnectionState(ConnectionState::Connected);
}

} // namespace

class TestDisplaySettingsBinding : public QObject
{
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // Acceptance: "Model to widget, all fourteen: setting a non-default
    // in-range value on the model makes the matching widget getter
    // return it."
    //
    // Catches: any of the fourteen model-to-widget connections in
    // bindDisplaySettings() missing, mis-typed (e.g. no static_cast on
    // Colour Scheme, wiring the wrong signal to the wrong applier).
    void modelToWidget_allFourteen()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();
        QVERIFY(m != nullptr);

        m->setWfColorScheme(6);       // non-default in-range index
        m->setWfColorGain(81);
        m->setWfBlackLevel(37);
        m->setRefLevel(-12.5f);
        m->setDynamicRange(96.0f);
        m->setFillAlpha(0.23f);
        m->setPanFill(false);
        m->setSpectrumFrac(0.61f);
        m->setSpectrumRenderMode(1);
        m->setDssFloorDepth(12);
        m->setDssGain(19);
        m->setDssRowSpan(64);
        m->setDssAngle(88);
        m->setThreeDSliceDepth(true);

        QCOMPARE(static_cast<int>(w.wfColorScheme()), 6);
        QCOMPARE(w.wfColorGain(), 81);
        QCOMPARE(w.wfBlackLevel(), 37);
        QVERIFY(std::abs(w.refLevel() - (-12.5f)) < 1e-6f);
        QVERIFY(std::abs(w.dynamicRange() - 96.0f) < 1e-6f);
        QVERIFY(std::abs(w.fillAlpha() - 0.23f) < 1e-6f);
        QCOMPARE(w.panFillEnabled(), false);
        QVERIFY(std::abs(w.spectrumFrac() - 0.61f) < 1e-6f);
        QCOMPARE(w.spectrumRenderMode(), 1);
        QCOMPARE(w.dssFloorDepth(), 12);
        QCOMPARE(w.dssGain(), 19);
        QCOMPARE(w.dssRowSpan(), 64);
        QCOMPARE(w.dssAngle(), 88);
        QCOMPARE(w.threeDSliceDepth(), true);
    }

    // Acceptance: "Widget to model, every write path: each of the eight
    // named setters (five existing plus the three new) ..."
    //
    // Catches: a named setter's push call missing or reaching the wrong
    // model field.
    void widgetToModel_eightNamedSetters()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        w.setWfColorScheme(static_cast<WfColorScheme>(6));
        w.setWfColorGain(33);
        w.setWfBlackLevel(22);
        w.setPanFillEnabled(false);
        w.setFillAlpha(0.15f);
        w.setRefLevel(-55.0f);
        w.setDynamicRange(120.0f);
        w.setSpectrumFrac(0.75f);

        QCOMPARE(m->wfColorScheme(), 6);
        QCOMPARE(m->wfColorGain(), 33);
        QCOMPARE(m->wfBlackLevel(), 22);
        QCOMPARE(m->panFill(), false);
        QVERIFY(std::abs(m->fillAlpha() - 0.15f) < 1e-6f);
        QVERIFY(std::abs(m->refLevel() - (-55.0f)) < 1e-6f);
        QVERIFY(std::abs(m->dynamicRange() - 120.0f) < 1e-6f);
        QVERIFY(std::abs(m->spectrumFrac() - 0.75f) < 1e-6f);
    }

    // Acceptance: "... and the six 3D setters ..."
    //
    // Catches: one of the six widget-to-model connections in
    // bindDisplaySettings() missing or mis-wired.
    void widgetToModel_sixDssSetters()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        w.setSpectrumRenderMode(1);
        w.setDssFloorDepth(20);
        w.setDssGain(33);
        w.setDssRowSpan(40);
        w.setDssAngle(70);
        w.setThreeDSliceDepth(true);

        QCOMPARE(m->spectrumRenderMode(), 1);
        QCOMPARE(m->dssFloorDepth(), 20);
        QCOMPARE(m->dssGain(), 33);
        QCOMPARE(m->dssRowSpan(), 40);
        QCOMPARE(m->dssAngle(), 70);
        QCOMPARE(m->threeDSliceDepth(), true);
    }

    // Acceptance: "setDbmRange(-120.0f, -40.0f) gives model Ref Level
    // -40 and Dyn Range 80."
    //
    // Catches: the push at the end of setDbmRange() missing.
    void setDbmRange_pushesRefLevelAndDynamicRangeToModel()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        w.setDbmRange(-120.0f, -40.0f);

        QVERIFY(std::abs(m->refLevel() - (-40.0f)) < 1e-6f);
        QVERIFY(std::abs(m->dynamicRange() - 80.0f) < 1e-6f);
    }

    // Acceptance: "loadSettings() with seeded AppSettings keys (pan 0,
    // the exact key strings in the scout table) leaves the model holding
    // the seeded values."
    //
    // Scoped to the eight fields syncDisplaySettingsFromWidget() actually
    // pushes (Step 5 of the brief names exactly eight calls); the other
    // five 3D fields' persisted values are not covered by that method
    // (loadSettings() assigns them directly, with no signal to push
    // through) -- see the report's "where the brief was imprecise"
    // section.
    //
    // Catches: loadSettings()'s single push call at the end missing, or
    // seeded values not actually reaching the model.
    void loadSettings_seedsTheModelsEightFields()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("DisplayWfColorScheme"), QStringLiteral("3"));
        s.setValue(QStringLiteral("DisplayWfColorGain"), QStringLiteral("77"));
        s.setValue(QStringLiteral("DisplayWfBlackLevel"), QStringLiteral("50"));
        s.setValue(QStringLiteral("DisplayGridMax"), QStringLiteral("-30.0"));
        s.setValue(QStringLiteral("DisplayGridMin"), QStringLiteral("-150.0"));
        s.setValue(QStringLiteral("DisplayFftFillAlpha"), QStringLiteral("0.33"));
        s.setValue(QStringLiteral("DisplayPanFill"), QStringLiteral("False"));
        s.setValue(QStringLiteral("DisplaySpectrumFrac"), QStringLiteral("0.55"));

        SpectrumWidget w;
        setUpWidget(w);
        w.loadSettings();
        DisplaySettingsModel* m = w.displaySettings();

        QCOMPARE(m->wfColorScheme(), 3);
        QCOMPARE(m->wfColorGain(), 77);
        QCOMPARE(m->wfBlackLevel(), 50);
        QVERIFY(std::abs(m->refLevel() - (-30.0f)) < 1e-6f);
        QVERIFY(std::abs(m->dynamicRange() - 120.0f) < 1e-6f); // -30 - (-150)
        QVERIFY(std::abs(m->fillAlpha() - 0.33f) < 1e-6f);
        QCOMPARE(m->panFill(), false);
        QVERIFY(std::abs(m->spectrumFrac() - 0.55f) < 1e-6f);
    }

    // Acceptance: "a plain dBm-strip drag ... driven the way
    // tests/tst_dss_floor_drag.cpp ... drive[s] the mouse, move[s] the
    // model's Ref Level ... with the widget's."
    //
    // Catches: the push in mouseMoveEvent's plain-drag 2D branch missing.
    void dbmStripDrag_movesModelRefLevelWithWidget()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        DisplaySettingsModel* m = w.displaySettings();

        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);
        dragDbmStrip(w, specH / 6);

        // Sanity: the drag actually moved refLevel (otherwise the sync
        // check below would trivially pass by both sides staying put).
        QVERIFY(std::abs(w.refLevel() - (-48.0f)) > 0.5f);
        QVERIFY(std::abs(m->refLevel() - w.refLevel()) < 1e-6f);
    }

    // Acceptance: "... and a divider drag, driven the way
    // tests/tst_dbm_range_drag.cpp ... drive[s] the mouse, move[s] ...
    // split fraction with the widget's."
    //
    // Catches: the push in mouseMoveEvent's divider branch missing.
    void dividerDrag_movesModelSplitFractionWithWidget()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        dragDivider(w, 600); // h=1000 -> frac 0.6, clear of the 0.40 default

        QVERIFY(std::abs(w.spectrumFrac() - 0.40f) > 0.01f);
        QVERIFY(std::abs(m->spectrumFrac() - w.spectrumFrac()) < 1e-6f);
    }

    // Acceptance: "Echo termination: with QSignalSpy on the model's
    // refLevelChanged, one model-driven change emits exactly once, one
    // widget-driven change (through setRefLevel) emits exactly once, and
    // re-applying an equal value emits zero times and leaves
    // displaySettingsApplyCountForTest() unchanged."
    //
    // Catches: a missing equality guard anywhere in the round trip
    // (model or widget), which would either fail to converge (endless
    // bounce, caught by QSignalSpy count growing past 1) or over-count
    // on the no-op re-apply.
    void echoTermination_refLevelChanged()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        QSignalSpy spy(m, &DisplaySettingsModel::refLevelChanged);

        // Model-driven change: exactly one emission, and it reaches the
        // widget.
        m->setRefLevel(-77.0f);
        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(w.refLevel() - (-77.0f)) < 1e-6f);

        // Widget-driven change, through the new named setter: exactly
        // one MORE emission (the widget's own applier settles at the
        // value the model just echoed back, so its guard absorbs the
        // bounce before it can round-trip a second time).
        spy.clear();
        w.setRefLevel(-33.0f);
        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(m->refLevel() - (-33.0f)) < 1e-6f);

        // Re-applying the SAME value from either side: zero emissions,
        // and the widget's own apply counter does not move (its guard
        // catches the value before the counter increments).
        spy.clear();
        const int countBefore = w.displaySettingsApplyCountForTest();
        m->setRefLevel(-33.0f);
        w.setRefLevel(-33.0f);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(w.displaySettingsApplyCountForTest(), countBefore);
    }

    // Acceptance: "No apply during load: after constructing a widget and
    // running loadSettings(), displaySettingsApplyCountForTest() is 0."
    //
    // Catches: loadSettings()'s push routed through the widget's OWN
    // setters (which would increment the counter) instead of straight
    // into the model.
    void noApplyDuringLoad()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.loadSettings();
        QCOMPARE(w.displaySettingsApplyCountForTest(), 0);
    }

    // Acceptance: "Round trip never narrows: setDbmRange(-180.0f, 20.0f)
    // (Dyn Range 200) and setDbmRange(-50.0f, -40.0f) (Dyn Range 10)
    // leave dynamicRange() and the model's dynamicRange() at 200 and 10
    // respectively."
    //
    // Catches: the model's Dyn Range clamp left at the old 20..160 (this
    // case is the one the brief calls out as required to go red under
    // that old clamp -- confirmed by mutation in the task report).
    void roundTripNeverNarrows_dynamicRange()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        w.setDbmRange(-180.0f, 20.0f);   // Dyn Range 200
        QVERIFY(std::abs(w.dynamicRange() - 200.0f) < 1e-6f);
        QVERIFY(std::abs(m->dynamicRange() - 200.0f) < 1e-6f);

        w.setDbmRange(-50.0f, -40.0f);   // Dyn Range 10
        QVERIFY(std::abs(w.dynamicRange() - 10.0f) < 1e-6f);
        QVERIFY(std::abs(m->dynamicRange() - 10.0f) < 1e-6f);
    }

    // Acceptance: "The 3D Floor bridge still fires:
    // displaySettings()->setDssFloorDepth(12) makes dssFloorDepth() 12
    // and emits the widget's dssFloorDepthChanged exactly once (that
    // signal is what MainWindow's per-band save listens to)."
    //
    // Catches: the model-to-widget connection for dssFloorDepthChanged
    // missing, or the widget-to-model connect-back firing a second,
    // unwanted emission instead of being absorbed by the widget's own
    // equality guard.
    void dssFloorBridgeStillFires()
    {
        SpectrumWidget w;
        setUpWidget(w);
        QSignalSpy spy(&w, &SpectrumWidget::dssFloorDepthChanged);

        w.displaySettings()->setDssFloorDepth(12);

        QCOMPARE(w.dssFloorDepth(), 12);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestDisplaySettingsBinding)
#include "tst_display_settings_binding.moc"
