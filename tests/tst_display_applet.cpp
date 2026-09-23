// tests/tst_display_applet.cpp
//
// no-port-check: NereusSDR-original test infrastructure for the 3D
// Stacked-Trace Spectrum Plan Task 22 DisplayApplet. Neither Thetis nor
// AetherSDR has a left-panel display-controls applet bound to a
// consolidating settings model (see DisplaySettingsModel.h's own header
// comment), so there is no upstream source to cite.
//
// Pins:
//   - RadioModel::spectrumWidgetChanged fires exactly once when the same
//     widget pointer is set twice (Task 22 Step 1).
//   - appletId()/appletTitle() return "Display"; every control exists.
//   - Three sections in order (Waterfall, Spectrum, 3D VIEW), titled
//     exactly as the popup's own section headers, with two dividers.
//   - Model -> control: each of the fifteen fields, isolated one field
//     per scope (fresh QSignalSpy per block) so a missing single connect
//     cannot hide behind another field's assertion -- the same batching
//     trap Task 20's review caught (see tst_display_setup_binding.cpp).
//     Each block also proves the reflect does not echo back through the
//     control (spy count stays at 1, not 2).
//   - Control -> model: each of the fifteen controls drives the bound
//     model's setter.
//   - Rebind: the applet follows RadioModel::setSpectrumWidget(); after
//     rebinding to a second widget, driving a control changes the NEW
//     widget's model and leaves the old one untouched.
//   - Reset 3D writes the seven ship defaults onto the model.
//
// Design: docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md
// Plan:   docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>

#include "core/AppSettings.h"
#include "gui/applets/DisplayApplet.h"
#include "gui/SpectrumWidget.h"
#include "models/DisplaySettingsModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestDisplayApplet : public QObject {
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void spectrumWidgetChanged_sameWidgetTwice_emitsOnce();

    void constructsWithRadioModel_boundWidget();
    void constructsWithRadioModel_noWidgetYet();

    void appletIdAndTitleAreDisplay();
    void structureHasThreeSectionsInOrderWithDividers();

    void modelChange_reflectsOnControl_waterfall();
    void modelChange_outOfRangeColorScheme_clampsDisplayWithoutCorruptingModel();
    void modelChange_reflectsOnControl_spectrum();
    void modelChange_reflectsOnControl_threeDView();

    void controlChange_reachesModel_allFifteen();

    void rebind_followsNewWidgetAndLeavesOldOneAlone();

    void resetButton_writesSevenDefaultsOntoModel();
};

// ---------------------------------------------------------------------
// Step 1: RadioModel::spectrumWidgetChanged
// ---------------------------------------------------------------------
void TestDisplayApplet::spectrumWidgetChanged_sameWidgetTwice_emitsOnce()
{
    RadioModel m;
    SpectrumWidget w;
    QSignalSpy spy(&m, &RadioModel::spectrumWidgetChanged);
    m.setSpectrumWidget(&w);
    m.setSpectrumWidget(&w);
    QCOMPARE(spy.count(), 1);
}

// ---------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------
void TestDisplayApplet::constructsWithRadioModel_boundWidget()
{
    RadioModel m;
    SpectrumWidget w;
    m.setSpectrumWidget(&w);

    DisplayApplet applet(&m);
    QVERIFY(applet.colorSchemeComboForTest()  != nullptr);
    QVERIFY(applet.colorGainSliderForTest()   != nullptr);
    QVERIFY(applet.blackLevelSliderForTest()  != nullptr);
    QVERIFY(applet.refLevelSliderForTest()    != nullptr);
    QVERIFY(applet.dynRangeSliderForTest()    != nullptr);
    QVERIFY(applet.fillAlphaSliderForTest()   != nullptr);
    QVERIFY(applet.fillTraceCheckForTest()    != nullptr);
    QVERIFY(applet.splitSliderForTest()       != nullptr);
    QVERIFY(applet.spectrumModeComboForTest() != nullptr);
    QVERIFY(applet.floorSliderForTest()       != nullptr);
    QVERIFY(applet.gainSliderForTest()        != nullptr);
    QVERIFY(applet.spanSliderForTest()        != nullptr);
    QVERIFY(applet.angleSliderForTest()       != nullptr);
    QVERIFY(applet.speedSliderForTest()       != nullptr);
    QVERIFY(applet.speedValueLabelForTest()   != nullptr);
    QVERIFY(applet.sliceShadowCheckForTest()  != nullptr);
    QVERIFY(applet.reset3dButtonForTest()     != nullptr);

    // Bound at construction: the slider reflects w's model immediately.
    QCOMPARE(applet.gainSliderForTest()->value(), w.displaySettings()->dssGain());
}

// model->spectrumWidget() is commonly still null when MainWindow
// constructs this applet (populateDefaultMeter() runs before the later
// RadioModel::setSpectrumWidget() call in the MainWindow constructor --
// see the class-header comment in DisplayApplet.h). Must not crash, and
// every control must still exist (unbound, showing nothing live yet).
void TestDisplayApplet::constructsWithRadioModel_noWidgetYet()
{
    RadioModel m;
    DisplayApplet applet(&m);
    QVERIFY(applet.colorSchemeComboForTest() != nullptr);
    QVERIFY(applet.reset3dButtonForTest()    != nullptr);

    // Clicking Reset 3D with nothing bound must be a harmless no-op, not
    // a null-pointer crash.
    applet.reset3dButtonForTest()->click();
}

void TestDisplayApplet::appletIdAndTitleAreDisplay()
{
    RadioModel m;
    DisplayApplet applet(&m);
    QCOMPARE(applet.appletId(), QStringLiteral("Display"));
    QCOMPARE(applet.appletTitle(), QStringLiteral("Display"));
}

void TestDisplayApplet::structureHasThreeSectionsInOrderWithDividers()
{
    RadioModel m;
    DisplayApplet applet(&m);

    QStringList labelTexts;
    for (QLabel* lbl : applet.findChildren<QLabel*>()) {
        labelTexts << lbl->text();
    }
    const int waterfallIdx = labelTexts.indexOf(QStringLiteral("Waterfall"));
    const int spectrumIdx  = labelTexts.indexOf(QStringLiteral("Spectrum"));
    const int viewIdx      = labelTexts.indexOf(QStringLiteral("3D VIEW"));
    QVERIFY2(waterfallIdx >= 0, "Waterfall section title missing");
    QVERIFY2(spectrumIdx  >= 0, "Spectrum section title missing");
    QVERIFY2(viewIdx      >= 0, "3D VIEW section title missing");
    QVERIFY2(waterfallIdx < spectrumIdx, "Waterfall must precede Spectrum");
    QVERIFY2(spectrumIdx  < viewIdx,     "Spectrum must precede 3D VIEW");

    // divider() returns an HLine/Sunken QFrame (AppletWidget.cpp); filter
    // on that shape rather than counting every QFrame in the subtree --
    // QComboBox's internal popup container is itself a QFrame, so a bare
    // findChildren<QFrame*>() count is not specific to the dividers this
    // applet added.
    int hLineCount = 0;
    for (QFrame* f : applet.findChildren<QFrame*>()) {
        if (f->frameShape() == QFrame::HLine) {
            ++hLineCount;
        }
    }
    QCOMPARE(hLineCount, 2);
}

// ---------------------------------------------------------------------
// Model -> control, one field per scope (Ambiguity 1 resolution).
// ---------------------------------------------------------------------
void TestDisplayApplet::modelChange_reflectsOnControl_waterfall()
{
    RadioModel m;
    SpectrumWidget w;
    m.setSpectrumWidget(&w);
    DisplayApplet applet(&m);
    DisplaySettingsModel* settings = w.displaySettings();

    {
        QSignalSpy spy(settings, &DisplaySettingsModel::wfColorSchemeChanged);
        settings->setWfColorScheme(2);  // "Spectran" -- within the combo's 4 items
        QCOMPARE(applet.colorSchemeComboForTest()->currentIndex(), 2);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::wfColorGainChanged);
        settings->setWfColorGain(81);
        QCOMPARE(applet.colorGainSliderForTest()->value(), 81);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::wfBlackLevelChanged);
        settings->setWfBlackLevel(37);
        QCOMPARE(applet.blackLevelSliderForTest()->value(), 37);
        QCOMPARE(spy.count(), 1);
    }
}

// wfColorScheme's model range is 0..7 (WfColorScheme::Count == 8), but the
// combo -- copied verbatim from the popup, which only ever offered four
// items -- can only DISPLAY 0..3 (SpectrumOverlayMenu::setValues() clamps
// the same way). Without QSignalBlocker on the reflect, that clamped
// DISPLAY value would bounce back out through the combo's
// currentIndexChanged and corrupt the model down to 3. This is the one
// field in the fifteen where the blocker is load-bearing rather than
// belt-and-suspenders over the model's own equality guard (every other
// field's reflect sets the control to the SAME value the model just
// stored, so an unblocked round trip is a same-value no-op there).
void TestDisplayApplet::modelChange_outOfRangeColorScheme_clampsDisplayWithoutCorruptingModel()
{
    RadioModel m;
    SpectrumWidget w;
    m.setSpectrumWidget(&w);
    DisplayApplet applet(&m);
    DisplaySettingsModel* settings = w.displaySettings();

    settings->setWfColorScheme(6);  // LinRad -- outside the combo's 4 items
    QCOMPARE(applet.colorSchemeComboForTest()->currentIndex(), 3);
    QCOMPARE(settings->wfColorScheme(), 6);
}

void TestDisplayApplet::modelChange_reflectsOnControl_spectrum()
{
    RadioModel m;
    SpectrumWidget w;
    m.setSpectrumWidget(&w);
    DisplayApplet applet(&m);
    DisplaySettingsModel* settings = w.displaySettings();

    {
        QSignalSpy spy(settings, &DisplaySettingsModel::refLevelChanged);
        settings->setRefLevel(-12.0f);
        QCOMPARE(applet.refLevelSliderForTest()->value(), -12);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::dynamicRangeChanged);
        settings->setDynamicRange(96.0f);
        QCOMPARE(applet.dynRangeSliderForTest()->value(), 96);
        QCOMPARE(spy.count(), 1);
    }
    {
        // 0.25f is exactly representable in binary float (1/4), so the
        // reflect's static_cast<int>(alpha * 100.0f) cannot round
        // ambiguously -- same precaution tst_display_setup_binding.cpp's
        // spectrumFillAlpha_loadReadsModel takes with the same field.
        QSignalSpy spy(settings, &DisplaySettingsModel::fillAlphaChanged);
        settings->setFillAlpha(0.25f);
        QCOMPARE(applet.fillAlphaSliderForTest()->value(), 25);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::panFillChanged);
        settings->setPanFill(false);  // ship default is true
        QCOMPARE(applet.fillTraceCheckForTest()->isChecked(), false);
        QCOMPARE(spy.count(), 1);
    }
    {
        // 0.75f is exactly representable (3/4); see the Fill Alpha note.
        QSignalSpy spy(settings, &DisplaySettingsModel::spectrumFracChanged);
        settings->setSpectrumFrac(0.75f);
        QCOMPARE(applet.splitSliderForTest()->value(), 75);
        QCOMPARE(spy.count(), 1);
    }
}

void TestDisplayApplet::modelChange_reflectsOnControl_threeDView()
{
    RadioModel m;
    SpectrumWidget w;
    m.setSpectrumWidget(&w);
    DisplayApplet applet(&m);
    DisplaySettingsModel* settings = w.displaySettings();

    {
        QSignalSpy spy(settings, &DisplaySettingsModel::spectrumRenderModeChanged);
        settings->setSpectrumRenderMode(1);
        QCOMPARE(applet.spectrumModeComboForTest()->currentIndex(), 1);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::dssFloorDepthChanged);
        settings->setDssFloorDepth(12);
        QCOMPARE(applet.floorSliderForTest()->value(), 12);
        QCOMPARE(spy.count(), 1);
    }
    {
        // The acceptance's own example field.
        QSignalSpy spy(settings, &DisplaySettingsModel::dssGainChanged);
        settings->setDssGain(33);
        QCOMPARE(applet.gainSliderForTest()->value(), 33);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::dssRowSpanChanged);
        settings->setDssRowSpan(15);
        QCOMPARE(applet.spanSliderForTest()->value(), 15);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::dssAngleChanged);
        settings->setDssAngle(80);
        QCOMPARE(applet.angleSliderForTest()->value(), 80);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::dssRowDividerChanged);
        settings->setDssRowDivider(5);
        QCOMPARE(applet.speedSliderForTest()->value(), 5);
        QCOMPARE(applet.speedValueLabelForTest()->text(), QStringLiteral("1:5"));
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::dssRowDividerChanged);
        settings->setDssRowDivider(0);
        QCOMPARE(applet.speedValueLabelForTest()->text(), QStringLiteral("Match"));
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(settings, &DisplaySettingsModel::threeDSliceDepthChanged);
        settings->setThreeDSliceDepth(true);  // ship default is false
        QCOMPARE(applet.sliceShadowCheckForTest()->isChecked(), true);
        QCOMPARE(spy.count(), 1);
    }
}

// ---------------------------------------------------------------------
// Control -> model, all fifteen (matches
// tst_display_setup_binding.cpp's pageChange_reachesModelAndWidget_perField
// precedent: not isolated per field -- that batching risk is specific to
// the model -> control reflect direction, see Ambiguity 1).
// ---------------------------------------------------------------------
void TestDisplayApplet::controlChange_reachesModel_allFifteen()
{
    RadioModel m;
    SpectrumWidget w;
    m.setSpectrumWidget(&w);
    DisplayApplet applet(&m);
    DisplaySettingsModel* settings = w.displaySettings();

    applet.colorSchemeComboForTest()->setCurrentIndex(3);
    QCOMPARE(settings->wfColorScheme(), 3);

    applet.colorGainSliderForTest()->setValue(60);
    QCOMPARE(settings->wfColorGain(), 60);

    applet.blackLevelSliderForTest()->setValue(50);
    QCOMPARE(settings->wfBlackLevel(), 50);

    applet.refLevelSliderForTest()->setValue(-5);
    QVERIFY(qFuzzyCompare(settings->refLevel(), -5.0f));

    applet.dynRangeSliderForTest()->setValue(120);
    QVERIFY(qFuzzyCompare(settings->dynamicRange(), 120.0f));

    applet.fillAlphaSliderForTest()->setValue(40);
    QVERIFY(qFuzzyCompare(settings->fillAlpha(), 40.0f / 100.0f));

    applet.fillTraceCheckForTest()->setChecked(false);  // default true
    QCOMPARE(settings->panFill(), false);

    applet.splitSliderForTest()->setValue(30);
    QVERIFY(qFuzzyCompare(settings->spectrumFrac(), 30.0f / 100.0f));

    applet.spectrumModeComboForTest()->setCurrentIndex(1);
    QCOMPARE(settings->spectrumRenderMode(), 1);

    applet.floorSliderForTest()->setValue(20);
    QCOMPARE(settings->dssFloorDepth(), 20);

    applet.gainSliderForTest()->setValue(44);
    QCOMPARE(settings->dssGain(), 44);

    applet.spanSliderForTest()->setValue(10);
    QCOMPARE(settings->dssRowSpan(), 10);

    applet.angleSliderForTest()->setValue(90);
    QCOMPARE(settings->dssAngle(), 90);

    applet.speedSliderForTest()->setValue(7);
    QCOMPARE(settings->dssRowDivider(), 7);

    applet.sliceShadowCheckForTest()->setChecked(true);  // default false
    QCOMPARE(settings->threeDSliceDepth(), true);
}

// ---------------------------------------------------------------------
// Rebind
// ---------------------------------------------------------------------
void TestDisplayApplet::rebind_followsNewWidgetAndLeavesOldOneAlone()
{
    RadioModel m;
    SpectrumWidget w1;
    SpectrumWidget w2;

    m.setSpectrumWidget(&w1);
    w1.displaySettings()->setDssGain(11);  // distinctive w1 value

    DisplayApplet applet(&m);
    QCOMPARE(applet.gainSliderForTest()->value(), 11);

    w2.displaySettings()->setDssGain(99);  // distinctive w2 value, set
                                            // before the applet ever sees w2
    m.setSpectrumWidget(&w2);

    // Applet now shows w2's value, not w1's.
    QCOMPARE(applet.gainSliderForTest()->value(), 99);

    // Driving the control changes w2's model...
    applet.gainSliderForTest()->setValue(55);
    QCOMPARE(w2.displaySettings()->dssGain(), 55);

    // ...and must NOT reach w1's model, which stays at the value it had
    // before the rebind.
    QCOMPARE(w1.displaySettings()->dssGain(), 11);
}

// ---------------------------------------------------------------------
// Reset 3D
// ---------------------------------------------------------------------
void TestDisplayApplet::resetButton_writesSevenDefaultsOntoModel()
{
    RadioModel m;
    SpectrumWidget w;
    m.setSpectrumWidget(&w);
    DisplayApplet applet(&m);
    DisplaySettingsModel* settings = w.displaySettings();

    settings->setSpectrumRenderMode(1);
    settings->setDssFloorDepth(20);
    settings->setDssGain(10);
    settings->setDssRowSpan(5);
    settings->setDssAngle(90);
    settings->setDssRowDivider(8);
    settings->setThreeDSliceDepth(true);

    applet.reset3dButtonForTest()->click();

    QCOMPARE(settings->spectrumRenderMode(), 0);
    QCOMPARE(settings->dssFloorDepth(), 6);
    QCOMPARE(settings->dssGain(), 70);
    QCOMPARE(settings->dssRowSpan(), 100);
    QCOMPARE(settings->dssAngle(), 50);
    QCOMPARE(settings->dssRowDivider(), 0);
    QCOMPARE(settings->threeDSliceDepth(), false);
}

QTEST_MAIN(TestDisplayApplet)
#include "tst_display_applet.moc"
