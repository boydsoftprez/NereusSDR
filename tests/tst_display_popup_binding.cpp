// =================================================================
// tests/tst_display_popup_binding.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure for the
// NereusSDR-original SpectrumOverlayMenu <-> DisplaySettingsModel
// binding (3D Stacked-Trace Spectrum Plan Task 20). Fixture shape
// (right-click discovery of the popup, sendMouse()) follows
// tests/tst_dss_overlay_menu.cpp; the model-side assertions follow
// tests/tst_display_settings_binding.cpp (Task 18).
//
// Task 20 re-points the right-click popup's thirteen change signals at
// DisplaySettingsModel instead of SpectrumWidget's own named setters, and
// adds a live-refresh path so the popup follows a model-driven change
// made by some OTHER surface while it stays open. Only thirteen of the
// model's fourteen values have a popup control; spectrumFrac has no
// popup surface (see the plan's scout report) and is out of scope here.
//
// A NOTE ON WHAT "CONNECTED TO THE MODEL, NOT THE WIDGET" CAN ACTUALLY
// PROVE HERE: Task 18 built m_displaySettings <-> SpectrumWidget as a
// fully bidirectional, equality-guarded bridge. For the eight fields
// with no per-field widget signal (wfColorScheme/wfColorGain/
// wfBlackLevel/refLevel/dynamicRange/fillAlpha/panFill/spectrumFrac),
// every one of SpectrumWidget's own named setters ends its body with an
// UNCONDITIONAL call to syncDisplaySettingsFromWidget(), which pushes the
// new value into the model regardless of which side originally received
// it. That means, at steady state, a popup signal wired to the WIDGET's
// setter (the pre-Task-20 shape) reaches the model exactly as reliably
// as one wired straight to the model's setter (confirmed empirically:
// popupSignalsConnectToTheModel_notTheWidget below passed even before
// Step 2's rewire was applied). A pure black-box test of "does the model
// end up right" therefore cannot, by itself, tell the two wirings apart
// for those eight fields; it still catches the far more likely defect
// class in a mechanical rewiring task (a dropped connect, or one paired
// with the wrong setter/signal). seedingReadsFromModel_notAStaleWidget
// below uses QObject::disconnect() to sever one plain-pointer leg of the
// bridge and force a real disagreement, which DOES distinguish "reads
// from the model" from "reads from the widget" for the seeding path, and
// popupSignalsConnectToTheModel_notTheWidget's closing sub-case does the
// same for a 3D-group connect (those six route through their own
// dedicated widget-level signal, a real Qt connection that CAN be
// severed, unlike the eight's hardcoded sync() call). See the task
// report for the full trace.
//
// Isolation: AppSettings::instance() is a process-wide in-memory
// singleton with no automatic load()/save() in this test process
// (TestSandboxInit.cpp also sandboxes QStandardPaths so nothing here can
// reach the developer's real ~/.config/NereusSDR/NereusSDR.settings).
// init()/cleanup() clear it before and after every slot, matching
// tst_display_settings_binding.cpp, even though none of these tests
// calls loadSettings()/saveSettings() directly -- a deferred
// scheduleSettingsSave() firing mid-run would otherwise be able to leak
// state between slots.
// =================================================================

#include <QtTest/QtTest>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <cmath>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "gui/SpectrumOverlayMenu.h"
#include "gui/SpectrumWidget.h"
#include "models/DisplaySettingsModel.h"

using namespace NereusSDR;

namespace {

// Mirrors tst_dss_overlay_menu.cpp's sendMouse(): a directly synthesized
// QMouseEvent via QApplication::sendEvent is this project's established
// way to drive SpectrumWidget's mousePressEvent headlessly.
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos,
               Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   button, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &me);
}

// Common fixture (Global Constraints: resize/show/expose before any
// synthetic mouse event). setConnectionState(Connected) clears
// mousePressEvent's Phase 3Q-8 disconnected-click guard.
// setFrequencyRange() matches tst_dss_overlay_menu.cpp's own end-to-end
// test: mousePressEvent's right-click branch computes xToHz() from
// m_bandwidthHz, which defaults to 0 on a freshly-constructed widget.
void setUpWidget(SpectrumWidget& w)
{
    w.resize(1000, 400);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.setConnectionState(ConnectionState::Connected);
    w.setFrequencyRange(14'200'000.0, 128'000.0);
}

// Right-clicks the empty pan (no notches/spots pushed, so the click
// falls through to the overlay menu -- same idiom as
// tst_dss_overlay_menu.cpp's sixControls_wireThroughToSpectrumWidgetGetters)
// and returns the popup SpectrumWidget::mousePressEvent constructs. That
// is the only way to prove the connects committed in SpectrumWidget.cpp
// are the REAL wiring rather than a parallel set of connects duplicated
// inside the test.
SpectrumOverlayMenu* openPopup(SpectrumWidget& w)
{
    sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, 50),
              Qt::RightButton, Qt::RightButton);
    return w.findChild<SpectrumOverlayMenu*>();
}

} // namespace

class TestDisplayPopupBinding : public QObject
{
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // Acceptance: "Every one of the thirteen popup signals is connected
    // to the matching DisplaySettingsModel setter ... no popup signal is
    // connected to a SpectrumWidget member function or to a lambda that
    // touches a widget member."
    //
    // Drives all fourteen real popup controls -- the seven 3D ones by
    // their object name, same as tst_dss_overlay_menu.cpp; the other
    // seven by findChildren<QSlider*/QComboBox*/QCheckBox*>()
    // construction order, since only the seven 3D controls carry an
    // object name (SpectrumOverlayMenu.cpp's buildUI(): Color Gain,
    // Black Level, Fill Alpha, Ref Level, Dyn Range sliders in that
    // order; Color Scheme combo; Fill-spectrum-trace and CTUN checks) --
    // and checks the MODEL's getters. Catches: a missing connect, or one
    // paired with the wrong signal/setter/argument.
    //
    // The closing sub-case is the genuine "connected to the model, not
    // the widget" mutation proof described in the file header: it severs
    // bindDisplaySettings()'s widget-to-model dssGainChanged connect (a
    // real Qt connection for the 3D group, unlike the eight's hardcoded
    // sync() call) so a popup-driven change can only still reach the
    // model if the popup's own dssGainChanged signal targets the model
    // DIRECTLY, as Step 2 requires. Mutation-proven: re-pointing that one
    // connect back at SpectrumWidget::setDssGain fails this assertion
    // (see report).
    void popupSignalsConnectToTheModel_notTheWidget()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        SpectrumOverlayMenu* menu = openPopup(w);
        QVERIFY2(menu != nullptr, "right-click did not open the overlay menu");

        const auto sliders = menu->findChildren<QSlider*>();
        const auto combos  = menu->findChildren<QComboBox*>();
        const auto checks  = menu->findChildren<QCheckBox*>();
        QCOMPARE(sliders.size(), 10);
        QCOMPARE(combos.size(), 2);
        QCOMPARE(checks.size(), 3);

        sliders.at(0)->setValue(81);                  // Color Gain
        QCOMPARE(m->wfColorGain(), 81);
        sliders.at(1)->setValue(37);                  // Black Level
        QCOMPARE(m->wfBlackLevel(), 37);
        sliders.at(2)->setValue(25);                   // Fill Alpha (25%)
        QVERIFY(std::abs(m->fillAlpha() - 0.25f) < 1e-6f);
        sliders.at(3)->setValue(-12);                  // Ref Level
        QVERIFY(std::abs(m->refLevel() - (-12.0f)) < 1e-6f);
        sliders.at(4)->setValue(96);                   // Dyn Range
        QVERIFY(std::abs(m->dynamicRange() - 96.0f) < 1e-6f);
        combos.at(0)->setCurrentIndex(2);               // Color Scheme
        QCOMPARE(m->wfColorScheme(), 2);
        checks.at(0)->setChecked(false);                // Fill spectrum trace
        QCOMPARE(m->panFill(), false);

        auto* mode   = menu->findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"));
        auto* floor  = menu->findChild<QSlider*>(QStringLiteral("dssFloorDepthSlider"));
        auto* gain   = menu->findChild<QSlider*>(QStringLiteral("dssGainSlider"));
        auto* span   = menu->findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"));
        auto* angle  = menu->findChild<QSlider*>(QStringLiteral("dssAngleSlider"));
        auto* speed  = menu->findChild<QSlider*>(QStringLiteral("dssSpeedSlider"));
        auto* shadow = menu->findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"));
        QVERIFY(mode);
        QVERIFY(floor);
        QVERIFY(gain);
        QVERIFY(span);
        QVERIFY(angle);
        QVERIFY(speed);
        QVERIFY(shadow);

        mode->setCurrentIndex(1);
        QCOMPARE(m->spectrumRenderMode(), 1);
        floor->setValue(15);
        QCOMPARE(m->dssFloorDepth(), 15);
        gain->setValue(22);
        QCOMPARE(m->dssGain(), 22);
        span->setValue(33);
        QCOMPARE(m->dssRowSpan(), 33);
        angle->setValue(77);
        QCOMPARE(m->dssAngle(), 77);
        speed->setValue(6);                            // 3D Speed
        QCOMPARE(m->dssRowDivider(), 6);
        shadow->setChecked(true);
        QCOMPARE(m->threeDSliceDepth(), true);

        // Genuine "targets the model directly" proof (see file header
        // and method doc comment): sever the widget-to-model leg so the
        // OLD ("popup -> widget setter") shape could no longer reach the
        // model at all, then move the slider again with a fresh value.
        // QVERIFY on the disconnect's own return value: a silently
        // failed disconnect (e.g. a future signal/slot rename) would
        // otherwise leave the bridge intact and this sub-case would pass
        // for the wrong reason regardless of how mousePressEvent wires
        // the popup.
        QVERIFY(QObject::disconnect(&w, &SpectrumWidget::dssGainChanged,
                                     m, &DisplaySettingsModel::setDssGain));
        gain->setValue(55);
        QCOMPARE(m->dssGain(), 55);
    }

    // Acceptance: "The seeding calls read the model's getters ..."
    //
    // Seeds many of the model's fields to non-default values BEFORE the
    // popup has ever been constructed, then right-clicks (first
    // construction, followed by the unconditional setValues()/
    // setDssValues() reseed that always runs right after) and reads
    // every popup control back. Catches: setValues()/setDssValues()
    // mapping an argument to the wrong control, or in the wrong order.
    //
    // Cannot, by itself, prove the source is the MODEL rather than the
    // (always-already-matching) widget -- see
    // seedingReadsFromModel_notAStaleWidget for that proof, and the file
    // header for why this one cannot carry it.
    void seedingMapsEveryFieldToTheCorrectControl()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        m->setWfColorGain(81);
        m->setWfBlackLevel(37);
        m->setWfColorScheme(2);
        m->setFillAlpha(0.25f);
        m->setPanFill(false);
        m->setRefLevel(-12.0f);
        m->setDynamicRange(96.0f);
        m->setSpectrumRenderMode(1);
        m->setDssFloorDepth(15);
        m->setDssGain(22);
        m->setDssRowSpan(33);
        m->setDssAngle(77);
        m->setDssRowDivider(6);
        m->setThreeDSliceDepth(true);

        SpectrumOverlayMenu* menu = openPopup(w);
        QVERIFY2(menu != nullptr, "right-click did not open the overlay menu");

        const auto sliders = menu->findChildren<QSlider*>();
        const auto combos  = menu->findChildren<QComboBox*>();
        const auto checks  = menu->findChildren<QCheckBox*>();
        QCOMPARE(sliders.at(0)->value(), 81);
        QCOMPARE(sliders.at(1)->value(), 37);
        QCOMPARE(sliders.at(2)->value(), 25);   // Fill Alpha, 0.25f * 100
        QCOMPARE(sliders.at(3)->value(), -12);
        QCOMPARE(sliders.at(4)->value(), 96);
        QCOMPARE(combos.at(0)->currentIndex(), 2);
        QCOMPARE(checks.at(0)->isChecked(), false);

        QCOMPARE(menu->findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"))->currentIndex(), 1);
        QCOMPARE(menu->findChild<QSlider*>(QStringLiteral("dssFloorDepthSlider"))->value(), 15);
        QCOMPARE(menu->findChild<QSlider*>(QStringLiteral("dssGainSlider"))->value(), 22);
        QCOMPARE(menu->findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"))->value(), 33);
        QCOMPARE(menu->findChild<QSlider*>(QStringLiteral("dssAngleSlider"))->value(), 77);
        QCOMPARE(menu->findChild<QSlider*>(QStringLiteral("dssSpeedSlider"))->value(), 6);
        QCOMPARE(menu->findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"))->isChecked(), true);
    }

    // Acceptance: "The seeding calls read the model's getters, and a
    // test proves the seeded popup values equal the model's after a
    // model-driven change made before the right-click" -- specifically
    // the MODEL, not a widget member that merely happens to already
    // agree with it.
    //
    // Task 18's bridge keeps SpectrumWidget's own members in lockstep
    // with the model at steady state, so setting the model and then
    // reading the popup back cannot alone tell "setValues() reads the
    // model" apart from "setValues() reads the widget" -- both would
    // show the right number. This test forces a real disagreement first:
    // it severs one plain-pointer leg of bindDisplaySettings()'s
    // model-to-widget bridge for wfColorGain (the "eight" group, pushed
    // by a plain connect straight to SpectrumWidget::setWfColorGain) and
    // for dssGain (one of the "six" 3D group, same shape), so the
    // widget's own member is provably stale by the time the popup opens.
    //
    // Catches: setValues()/setDssValues() still reading SpectrumWidget's
    // own members instead of the model's getters, for either binding
    // flavour Task 18 established.
    void seedingReadsFromModel_notAStaleWidget()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        QVERIFY(QObject::disconnect(m, &DisplaySettingsModel::wfColorGainChanged,
                                     &w, &SpectrumWidget::setWfColorGain));
        QVERIFY(QObject::disconnect(m, &DisplaySettingsModel::dssGainChanged,
                                     &w, &SpectrumWidget::setDssGain));

        m->setWfColorGain(81);
        m->setDssGain(22);
        QVERIFY2(w.wfColorGain() != 81,
                 "test invariant: the severed connect must leave the widget stale");
        QVERIFY2(w.dssGain() != 22,
                 "test invariant: the severed connect must leave the widget stale");

        SpectrumOverlayMenu* menu = openPopup(w);
        QVERIFY2(menu != nullptr, "right-click did not open the overlay menu");

        const auto sliders = menu->findChildren<QSlider*>();
        QCOMPARE(sliders.at(0)->value(), 81);  // Color Gain: the model's value
        QCOMPARE(menu->findChild<QSlider*>(QStringLiteral("dssGainSlider"))->value(), 22);
    }

    // Acceptance: "Live refresh: while the popup is visible, a
    // model-driven change updates the matching popup control, and that
    // refresh emits none of the popup's signals (QSignalSpy count 0), so
    // the model sees exactly one change."
    //
    // A representative sample across both binding flavours Task 18
    // established (the "eight" group with no per-field widget signal,
    // and the "six" 3D group with its own dedicated widget-level
    // signal): wfColorGain (int), refLevel (float), panFill (bool) from
    // the eight; dssGain (int) and threeDSliceDepth (bool) from the six.
    //
    // Each field is changed and checked in its OWN scope, one at a time,
    // rather than batched. setValues()/setDssValues() are BULK reseeds:
    // batching all five model changes first and checking afterwards was
    // tried and found NOT mutation-provable, because ANY ONE of the
    // other four fields' live-refresh connects firing also happens to
    // re-read the model's already-correct value for a field whose OWN
    // connect was removed, silently covering for it. Isolating each
    // field's change so no OTHER model setter runs before its assertion
    // is the only way to attribute a popup update to THAT field's own
    // connect.
    //
    // Catches: a missing (or removed) live-refresh connect for one of
    // these fields. Mutation-proven: removing the wfColorGainChanged
    // live-refresh connect fails this test's slider-value assertion for
    // that field (see report).
    void liveRefresh_updatesPopupWithoutEmittingPopupSignals()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        SpectrumOverlayMenu* menu = openPopup(w);
        QVERIFY2(menu != nullptr, "right-click did not open the overlay menu");
        QVERIFY(menu->isVisible());

        const auto sliders = menu->findChildren<QSlider*>();
        const auto checks  = menu->findChildren<QCheckBox*>();

        {
            QSignalSpy popupSpy(menu, &SpectrumOverlayMenu::wfColorGainChanged);
            QSignalSpy modelSpy(m, &DisplaySettingsModel::wfColorGainChanged);
            m->setWfColorGain(81);
            QCOMPARE(sliders.at(0)->value(), 81);
            QCOMPARE(popupSpy.count(), 0);
            QCOMPARE(modelSpy.count(), 1);
        }
        {
            QSignalSpy popupSpy(menu, &SpectrumOverlayMenu::refLevelChanged);
            QSignalSpy modelSpy(m, &DisplaySettingsModel::refLevelChanged);
            m->setRefLevel(-12.0f);
            QCOMPARE(sliders.at(3)->value(), -12);
            QCOMPARE(popupSpy.count(), 0);
            QCOMPARE(modelSpy.count(), 1);
        }
        {
            QSignalSpy popupSpy(menu, &SpectrumOverlayMenu::panFillChanged);
            QSignalSpy modelSpy(m, &DisplaySettingsModel::panFillChanged);
            m->setPanFill(false);
            QCOMPARE(checks.at(0)->isChecked(), false);
            QCOMPARE(popupSpy.count(), 0);
            QCOMPARE(modelSpy.count(), 1);
        }
        {
            QSignalSpy popupSpy(menu, &SpectrumOverlayMenu::dssGainChanged);
            QSignalSpy modelSpy(m, &DisplaySettingsModel::dssGainChanged);
            m->setDssGain(22);
            QCOMPARE(menu->findChild<QSlider*>(QStringLiteral("dssGainSlider"))->value(), 22);
            QCOMPARE(popupSpy.count(), 0);
            QCOMPARE(modelSpy.count(), 1);
        }
        {
            QSignalSpy popupSpy(menu, &SpectrumOverlayMenu::dssSliceShadowChanged);
            QSignalSpy modelSpy(m, &DisplaySettingsModel::threeDSliceDepthChanged);
            m->setThreeDSliceDepth(true);
            QCOMPARE(menu->findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"))->isChecked(), true);
            QCOMPARE(popupSpy.count(), 0);
            QCOMPARE(modelSpy.count(), 1);
        }
    }

    // Acceptance: "Popup-driven change reaches both the model and the
    // widget: moving a popup slider ... changes the model getter and,
    // through Task 18's binding, the widget getter."
    //
    // The same representative sample as the live-refresh case above,
    // now driven from the POPUP side. This proves the full round trip
    // ends in the correct final state (both getters), not which internal
    // path it took to get there.
    //
    // Catches: Task 18's binding being bypassed on the popup path
    // specifically (e.g. a stray direct member write that skips the
    // model's setter and its own model-to-widget signal).
    void popupDrivenChangeReachesModelAndWidget()
    {
        SpectrumWidget w;
        setUpWidget(w);
        DisplaySettingsModel* m = w.displaySettings();

        SpectrumOverlayMenu* menu = openPopup(w);
        QVERIFY2(menu != nullptr, "right-click did not open the overlay menu");

        const auto sliders = menu->findChildren<QSlider*>();
        sliders.at(0)->setValue(81);                   // Color Gain
        QCOMPARE(m->wfColorGain(), 81);
        QCOMPARE(w.wfColorGain(), 81);

        sliders.at(3)->setValue(-12);                   // Ref Level
        QVERIFY(std::abs(m->refLevel() - (-12.0f)) < 1e-6f);
        QVERIFY(std::abs(w.refLevel() - (-12.0f)) < 1e-6f);

        auto* gain = menu->findChild<QSlider*>(QStringLiteral("dssGainSlider"));
        QVERIFY(gain);
        gain->setValue(22);
        QCOMPARE(m->dssGain(), 22);
        QCOMPARE(w.dssGain(), 22);

        auto* shadow = menu->findChild<QCheckBox*>(QStringLiteral("dssSliceShadowCheck"));
        QVERIFY(shadow);
        shadow->setChecked(true);
        QCOMPARE(m->threeDSliceDepth(), true);
        QCOMPARE(w.threeDSliceDepth(), true);
    }

    // Acceptance: "The setDssRowSpanSupported() push on open is
    // unchanged (it is derived from the widget's DDC width, not a
    // setting)."
    //
    // dssMeshReady() (private; not reachable from this test) flips true
    // as soon as this widget's first real GPU paint builds the DSS mesh
    // pipeline (SpectrumWidget::initialize(), called unconditionally by
    // QRhiWidget's own paint machinery, not gated on 2D/3D mode) -- on a
    // real windowing system that first paint can land during
    // setUpWidget()'s qWaitForWindowExposed(), before the very first
    // right-click even happens (confirmed empirically: an earlier draft
    // of this test assumed "always disabled pre-paint" and failed on
    // this exact machine because the slider read enabled). So this test
    // cannot assume, or independently recompute, a fixed expected value.
    // Instead it uses the FIRST right-click's own observed state as
    // ground truth: force the slider to the OPPOSITE state directly
    // through the popup's own public API (bypassing SpectrumWidget
    // entirely), right-click a SECOND time on the same already-open
    // popup, and confirm the slider is back to the first click's value
    // -- which can only happen if mousePressEvent's push actually
    // re-ran (nothing else in this test touches rendering state between
    // the two clicks, so the underlying dssMeshReady() cannot itself
    // have changed).
    //
    // Catches: the setDssRowSpanSupported() call being dropped while
    // rewiring the surrounding block.
    void dssRowSpanSupportedStillPushedOnOpen()
    {
        SpectrumWidget w;
        setUpWidget(w);

        SpectrumOverlayMenu* menu = openPopup(w);
        QVERIFY2(menu != nullptr, "right-click did not open the overlay menu");

        auto* span = menu->findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"));
        QVERIFY(span);

        const bool observedOnFirstOpen = span->isEnabled();

        menu->setDssRowSpanSupported(!observedOnFirstOpen);
        QCOMPARE(span->isEnabled(), !observedOnFirstOpen);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, 50),
                  Qt::RightButton, Qt::RightButton);

        QCOMPARE(span->isEnabled(), observedOnFirstOpen);
    }
};

QTEST_MAIN(TestDisplayPopupBinding)
#include "tst_display_popup_binding.moc"
