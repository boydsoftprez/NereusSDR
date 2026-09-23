// tests/tst_display_setup_binding.cpp
//
// no-port-check: NereusSDR-original test infrastructure for the 3D
// Stacked-Trace Spectrum Plan Task 21 Setup migration. Neither Thetis nor
// AetherSDR routes its Setup-equivalent dialog through a consolidating
// display-settings model (see DisplaySettingsModel.h's own header comment),
// so there is no upstream source to cite.
//
// 3D stacked-trace spectrum plan Task 21: re-points Setup -> Display's nine
// DisplaySettingsModel-backed controls (Display3DSetupPage's six, plus
// WaterfallDefaultsPage's Colour Scheme and SpectrumDefaultsPage's Fill
// Alpha and Fill trace) at the model Task 18 introduced, instead of each
// page talking straight to SpectrumWidget. Pins:
//
//   - Display3DSetupPage: a model-driven change lands on the page's own
//     widget without echoing the widget's own change signal back out
//     (QSignalBlocker on reflect), the model's own signal fires exactly
//     once per real change; a page-driven change reaches both the model
//     and (through Task 18's existing binding) the widget; the Reset
//     button writes the six ship defaults onto the model.
//   - WaterfallDefaultsPage / SpectrumDefaultsPage: Colour Scheme, Fill
//     Alpha and Fill trace round-trip the same way through
//     model()->spectrumWidget()->displaySettings().
//
// Design: docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md
// Plan:   docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>

#include "core/FFTEngine.h"
#include "gui/SpectrumWidget.h"
#include "gui/setup/DisplaySetupPages.h"
#include "models/DisplaySettingsModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestDisplaySetupBinding : public QObject {
    Q_OBJECT

private slots:
    // ---- Display3DSetupPage: model -> page, one field at a time ----
    void modelChange_reflectsOnPage_perField();

    // ---- Display3DSetupPage: page -> model (and, through Task 18, the
    //      widget), one field at a time ----
    void pageChange_reachesModelAndWidget_perField();

    // ---- Display3DSetupPage: Reset button writes the model ----
    void resetButton_writesSixDefaultsOntoModel();

    // ---- WaterfallDefaultsPage: Colour Scheme ----
    void waterfallColorScheme_pushReachesModelAndWidget();
    void waterfallColorScheme_loadReadsModel();

    // ---- SpectrumDefaultsPage: Fill Alpha ----
    void spectrumFillAlpha_pushReachesModelAndWidget();
    void spectrumFillAlpha_loadReadsModel();

    // ---- SpectrumDefaultsPage: Fill trace ----
    void spectrumFillTrace_pushReachesModelAndWidget();
    void spectrumFillTrace_loadReadsModel();
};

// A model-driven change on each of the six 3D fields must land on the
// matching page widget, and must do so WITHOUT the reflected widget
// re-emitting its own change signal back out (that is what the
// QSignalBlocker on the reflect connect is for) -- proven directly for 3D
// Gain, the literal example in the task brief's acceptance text. Each field
// runs in its own scope: Task 20's review found a batched
// "change all five, then assert all five" test could pass with one
// reflect connect missing, because a DIFFERENT field's connect still firing
// triggered a full reseed that incidentally covered for it. There is no
// such reseed-everything path here (each reflect lambda only touches its
// own widget), but the isolated-scope shape is kept for the same reason:
// it is the only shape that cannot silently paper over a missing connect.
void TestDisplaySetupBinding::modelChange_reflectsOnPage_perField()
{
    SpectrumWidget w;
    Display3DSetupPage page(&w);
    DisplaySettingsModel* settings = w.displaySettings();

    // Spectrum render mode: 2D (0) -> 3D (1).
    {
        QSignalSpy modelSpy(settings, &DisplaySettingsModel::spectrumRenderModeChanged);
        settings->setSpectrumRenderMode(1);
        auto* combo = page.findChild<QComboBox*>(QStringLiteral("setup3DModeCombo"));
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->currentIndex(), 1);
        QCOMPARE(modelSpy.count(), 1);
    }
    // 3D Floor.
    {
        QSignalSpy modelSpy(settings, &DisplaySettingsModel::dssFloorDepthChanged);
        settings->setDssFloorDepth(12);
        auto* slider = page.findChild<QSlider*>(QStringLiteral("setup3DFloorSlider"));
        QVERIFY(slider != nullptr);
        QCOMPARE(slider->value(), 12);
        QCOMPARE(modelSpy.count(), 1);
    }
    // 3D Gain -- the acceptance's own example: setDssGain(33) must land on
    // the page's gain slider without emitting the SLIDER's own
    // valueChanged (proof the reflect is QSignalBlocker-guarded), and the
    // model itself must have emitted exactly once total.
    {
        auto* slider = page.findChild<QSlider*>(QStringLiteral("setup3DGainSlider"));
        QVERIFY(slider != nullptr);
        QSignalSpy sliderSpy(slider, &QSlider::valueChanged);
        QSignalSpy modelSpy(settings, &DisplaySettingsModel::dssGainChanged);
        settings->setDssGain(33);
        QCOMPARE(slider->value(), 33);
        QCOMPARE(sliderSpy.count(), 0);
        QCOMPARE(modelSpy.count(), 1);
    }
    // 3D Span.
    {
        QSignalSpy modelSpy(settings, &DisplaySettingsModel::dssRowSpanChanged);
        settings->setDssRowSpan(15);
        auto* slider = page.findChild<QSlider*>(QStringLiteral("setup3DSpanSlider"));
        QVERIFY(slider != nullptr);
        QCOMPARE(slider->value(), 15);
        QCOMPARE(modelSpy.count(), 1);
    }
    // 3D Angle.
    {
        QSignalSpy modelSpy(settings, &DisplaySettingsModel::dssAngleChanged);
        settings->setDssAngle(80);
        auto* slider = page.findChild<QSlider*>(QStringLiteral("setup3DAngleSlider"));
        QVERIFY(slider != nullptr);
        QCOMPARE(slider->value(), 80);
        QCOMPARE(modelSpy.count(), 1);
    }
    // 3D Slice Shadow.
    {
        QSignalSpy modelSpy(settings, &DisplaySettingsModel::threeDSliceDepthChanged);
        settings->setThreeDSliceDepth(true);
        auto* check = page.findChild<QCheckBox*>(QStringLiteral("setup3DSliceShadowCheck"));
        QVERIFY(check != nullptr);
        QVERIFY(check->isChecked());
        QCOMPARE(modelSpy.count(), 1);
    }
}

// A page-driven change on each of the six 3D fields must reach both the
// model directly and, through Task 18's existing SpectrumWidget <->
// DisplaySettingsModel binding, the live widget getter too.
void TestDisplaySetupBinding::pageChange_reachesModelAndWidget_perField()
{
    SpectrumWidget w;
    Display3DSetupPage page(&w);
    DisplaySettingsModel* settings = w.displaySettings();

    page.findChild<QComboBox*>(QStringLiteral("setup3DModeCombo"))->setCurrentIndex(1);
    QCOMPARE(settings->spectrumRenderMode(), 1);
    QCOMPARE(static_cast<int>(w.spectrumRenderMode()), 1);

    page.findChild<QSlider*>(QStringLiteral("setup3DFloorSlider"))->setValue(18);
    QCOMPARE(settings->dssFloorDepth(), 18);
    QCOMPARE(w.dssFloorDepth(), 18);

    // The acceptance's own example: moving the page's slider to 44 must
    // make both the model and the widget read 44.
    page.findChild<QSlider*>(QStringLiteral("setup3DGainSlider"))->setValue(44);
    QCOMPARE(settings->dssGain(), 44);
    QCOMPARE(w.dssGain(), 44);

    page.findChild<QSlider*>(QStringLiteral("setup3DSpanSlider"))->setValue(60);
    QCOMPARE(settings->dssRowSpan(), 60);
    QCOMPARE(w.dssRowSpan(), 60);

    page.findChild<QSlider*>(QStringLiteral("setup3DAngleSlider"))->setValue(15);
    QCOMPARE(settings->dssAngle(), 15);
    QCOMPARE(w.dssAngle(), 15);

    page.findChild<QCheckBox*>(QStringLiteral("setup3DSliceShadowCheck"))->setChecked(true);
    QVERIFY(settings->threeDSliceDepth());
    QVERIFY(w.threeDSliceDepth());
}

void TestDisplaySetupBinding::resetButton_writesSixDefaultsOntoModel()
{
    SpectrumWidget w;
    Display3DSetupPage page(&w);
    DisplaySettingsModel* settings = w.displaySettings();

    settings->setSpectrumRenderMode(1);
    settings->setDssFloorDepth(20);
    settings->setDssGain(10);
    settings->setDssRowSpan(5);
    settings->setDssAngle(90);
    settings->setThreeDSliceDepth(true);

    page.resetToDefaultsForTest();

    QCOMPARE(settings->spectrumRenderMode(), 0);
    QCOMPARE(settings->dssFloorDepth(), 6);
    QCOMPARE(settings->dssGain(), 70);
    QCOMPARE(settings->dssRowSpan(), 100);
    QCOMPARE(settings->dssAngle(), 50);
    QCOMPARE(settings->threeDSliceDepth(), false);
}

// WaterfallDefaultsPage / SpectrumDefaultsPage are RadioModel-backed pages.
// No existing test constructs either directly (tests/tst_waterfall_defaults_
// changes.cpp exercises SpectrumWidget setters only, despite its name), so
// the fixture below is built per the task brief's fallback: RadioModel,
// SpectrumWidget, radioModel.setSpectrumWidget(&w), then the page.

void TestDisplaySetupBinding::waterfallColorScheme_pushReachesModelAndWidget()
{
    RadioModel radioModel;
    SpectrumWidget w;
    radioModel.setSpectrumWidget(&w);
    WaterfallDefaultsPage page(&radioModel);

    auto* combo = page.findChild<QComboBox*>(QStringLiteral("wfColorSchemeCombo"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(3);  // BlackWhite

    QCOMPARE(w.displaySettings()->wfColorScheme(), 3);
    QCOMPARE(static_cast<int>(w.wfColorScheme()), 3);
}

void TestDisplaySetupBinding::waterfallColorScheme_loadReadsModel()
{
    RadioModel radioModel;
    SpectrumWidget w;
    radioModel.setSpectrumWidget(&w);
    w.displaySettings()->setWfColorScheme(5);  // LinRad, set before page construction

    WaterfallDefaultsPage page(&radioModel);
    auto* combo = page.findChild<QComboBox*>(QStringLiteral("wfColorSchemeCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->currentIndex(), 5);
}

// SpectrumDefaultsPage::loadFromRenderer() early-returns unless BOTH
// model()->spectrumWidget() and model()->fftEngine() are non-null (it
// reads FFT size/window/fps alongside the fill controls in one pass). A
// bare RadioModel has no FFTEngine wired up, so a stack-local one is
// constructed here purely to satisfy that guard; its constructor is a
// two-line no-op (see src/core/FFTEngine.cpp) and every getter
// loadFromRenderer() calls is a trivial std::atomic::load() with a sane
// default (fftSize 4096, sampleRate 48000.0, outputFps 30, decimation 1),
// so this cannot crash or hang.
void TestDisplaySetupBinding::spectrumFillAlpha_pushReachesModelAndWidget()
{
    RadioModel radioModel;
    SpectrumWidget w;
    FFTEngine fftEngine(0);
    radioModel.setSpectrumWidget(&w);
    radioModel.setFftEngine(&fftEngine);
    SpectrumDefaultsPage page(&radioModel);

    auto* slider = page.findChild<QSlider*>(QStringLiteral("specFillAlphaSlider"));
    QVERIFY(slider != nullptr);
    slider->setValue(42);

    QCOMPARE(w.displaySettings()->fillAlpha(), 42 / 100.0f);
    QCOMPARE(w.fillAlpha(), 42 / 100.0f);
}

void TestDisplaySetupBinding::spectrumFillAlpha_loadReadsModel()
{
    RadioModel radioModel;
    SpectrumWidget w;
    FFTEngine fftEngine(0);
    radioModel.setSpectrumWidget(&w);
    radioModel.setFftEngine(&fftEngine);
    w.displaySettings()->setFillAlpha(0.25f);  // set before page construction

    SpectrumDefaultsPage page(&radioModel);
    auto* slider = page.findChild<QSlider*>(QStringLiteral("specFillAlphaSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), 25);
}

void TestDisplaySetupBinding::spectrumFillTrace_pushReachesModelAndWidget()
{
    RadioModel radioModel;
    SpectrumWidget w;
    FFTEngine fftEngine(0);
    radioModel.setSpectrumWidget(&w);
    radioModel.setFftEngine(&fftEngine);
    SpectrumDefaultsPage page(&radioModel);

    auto* toggle = page.findChild<QCheckBox*>(QStringLiteral("specFillToggle"));
    QVERIFY(toggle != nullptr);
    QVERIFY(toggle->isChecked());  // ship default: on
    toggle->setChecked(false);

    QVERIFY(!w.displaySettings()->panFill());
    QVERIFY(!w.panFillEnabled());
}

void TestDisplaySetupBinding::spectrumFillTrace_loadReadsModel()
{
    RadioModel radioModel;
    SpectrumWidget w;
    FFTEngine fftEngine(0);
    radioModel.setSpectrumWidget(&w);
    radioModel.setFftEngine(&fftEngine);
    w.displaySettings()->setPanFill(false);  // set before page construction

    SpectrumDefaultsPage page(&radioModel);
    auto* toggle = page.findChild<QCheckBox*>(QStringLiteral("specFillToggle"));
    QVERIFY(toggle != nullptr);
    QVERIFY(!toggle->isChecked());
}

QTEST_MAIN(TestDisplaySetupBinding)
#include "tst_display_setup_binding.moc"
