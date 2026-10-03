// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_nr_quick_controls.cpp  (NereusSDR)
// =================================================================
//
// The VFO flag's noise-reduction quick controls (right-click on NR1 and
// MNR) and the Setup > DSP > NR/ANF NR1 tab against their sources.
//
//   - NR1: the ranges and defaults are Thetis's NR spinboxes
//     (setup.designer.cs:43418-43557 [v2.10.3.15]: taps 1-1024, delay
//     1-1023, gain and leak 1-1000, defaults 64 / 16 / 100 / 100), and the
//     values written are Thetis's conversion (setup.cs:8573-8586
//     [v2.10.3.15]: gain x 1e-6, leak x 1e-3). A new slice starts where
//     the sliders do, so the Gain slider is not pinned above its top and
//     Leak does not show 0.
//   - MNR: the Reset button, a new slice and Setup's MNR tab agree on
//     MacNRFilter's own DEF_* values (Aggressiveness 4, Bias 1.2).
//
// No radio is connected, nothing keys and no audio device opens.
//
//   cmake --build build --target tst_nr_quick_controls
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_nr_quick_controls$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: Match NR2/NR4 controls and defaults to Thetis v2.10.3.15.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//               (R-IOS-06, R-IOS-27).
//   2026-09-27: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code
//               (R-IOS-06, R-IOS-27).
// =================================================================

#include <QtTest>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMap>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTabWidget>

#include <cmath>

#include "core/WdspTypes.h"
#include "core/AppSettings.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/widgets/DspParamPopup.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// The flag's button whose text is `text`.
QPushButton* buttonNamed(const QWidget& widget, const QString& text)
{
    for (QPushButton* button : widget.findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

// The popup's sliders by their row label: each addSlider row is a label,
// the slider and its value readout.
QMap<QString, QSlider*> popupSliders(const DspParamPopup& popup)
{
    QMap<QString, QSlider*> sliders;
    QLayout* layout = popup.layout();
    for (int i = 0; layout && i < layout->count(); ++i) {
        QLayout* row = layout->itemAt(i)->layout();
        if (!row || row->count() != 3) {
            continue;
        }
        auto* label = qobject_cast<QLabel*>(row->itemAt(0)->widget());
        auto* slider = qobject_cast<QSlider*>(row->itemAt(1)->widget());
        if (label && slider) {
            sliders.insert(label->text(), slider);
        }
    }
    return sliders;
}

// The readout beside `slider` in its popup row.
QString readoutOf(const DspParamPopup& popup, const QSlider* slider)
{
    QLayout* layout = popup.layout();
    for (int i = 0; layout && i < layout->count(); ++i) {
        QLayout* row = layout->itemAt(i)->layout();
        if (row && row->count() == 3 && row->itemAt(1)->widget() == slider) {
            if (auto* value = qobject_cast<QLabel*>(row->itemAt(2)->widget())) {
                return value->text();
            }
        }
    }
    return {};
}

DspParamPopup* openPopup(VfoWidget& vfo, const QString& buttonText)
{
    QPushButton* button = buttonNamed(vfo, buttonText);
    if (!button) {
        return nullptr;
    }
    emit button->customContextMenuRequested(QPoint(1, 1));
    const QList<DspParamPopup*> popups = vfo.findChildren<DspParamPopup*>();
    return popups.isEmpty() ? nullptr : popups.last();
}

void checkRange(const QSlider* slider, int min, int max, int value)
{
    QVERIFY(slider);
    QCOMPARE(slider->minimum(), min);
    QCOMPARE(slider->maximum(), max);
    QCOMPARE(slider->value(), value);
}

} // namespace

class TstNrQuickControls : public QObject {
    Q_OBJECT

private slots:
    // A new slice starts at Thetis's NR spinbox defaults, converted as
    // Thetis converts them for SetRXAANRVals.
    void nr1SliceDefaultsAreThetis()
    {
        SliceModel slice;
        QCOMPARE(slice.nr1Taps(), 64);
        QCOMPARE(slice.nr1Delay(), 16);
        // 1e-6 x udLMSNRgain.Value (100), 1e-3 x udLMSNRLeak.Value (100).
        QVERIFY(std::abs(slice.nr1Gain() - 100 * 1e-6) < 1e-12);
        QVERIFY(std::abs(slice.nr1Leakage() - 100 * 1e-3) < 1e-12);
    }

    // The NR1 quick controls span Thetis's spinbox ranges and start at a
    // new slice's values.
    void nr1PopupRangesAreThetis()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
        DspParamPopup* popup = openPopup(vfo, QStringLiteral("NR1"));
        QVERIFY(popup);
        const QMap<QString, QSlider*> sliders = popupSliders(*popup);
        QCOMPARE(sliders.keys(), (QStringList{QStringLiteral("Delay"), QStringLiteral("Gain"),
                                              QStringLiteral("Leak"), QStringLiteral("Taps")}));
        // The readouts show a new slice's gain and leak in slider units.
        const QString gainShown = readoutOf(*popup, sliders.value(QStringLiteral("Gain")));
        const QString leakShown = readoutOf(*popup, sliders.value(QStringLiteral("Leak")));
        QCOMPARE(QStringLiteral("Gain %1, Leak %2").arg(gainShown, leakShown),
                 QStringLiteral("Gain 100, Leak 100"));
        checkRange(sliders.value(QStringLiteral("Taps")), 1, 1024, 64);
        checkRange(sliders.value(QStringLiteral("Delay")), 1, 1023, 16);
        checkRange(sliders.value(QStringLiteral("Gain")), 1, 1000, 100);
        checkRange(sliders.value(QStringLiteral("Leak")), 1, 1000, 100);
    }

    // The popup still writes the properties in their own units: taps and
    // delay as they are, gain x 1e-6, leak x 1e-3.
    void nr1PopupWritesThePropertyUnits()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
        DspParamPopup* popup = openPopup(vfo, QStringLiteral("NR1"));
        QVERIFY(popup);
        const QMap<QString, QSlider*> sliders = popupSliders(*popup);
        sliders.value(QStringLiteral("Taps"))->setValue(512);
        sliders.value(QStringLiteral("Delay"))->setValue(700);
        sliders.value(QStringLiteral("Gain"))->setValue(250);
        sliders.value(QStringLiteral("Leak"))->setValue(1000);
        QCOMPARE(slice->nr1Taps(), 512);
        QCOMPARE(slice->nr1Delay(), 700);
        QVERIFY(std::abs(slice->nr1Gain() - 250e-6) < 1e-12);
        QVERIFY(std::abs(slice->nr1Leakage() - 1.0) < 1e-12);

        // Reopened, each slider shows what the slice holds.
        popup->close();
        delete popup;
        popup = openPopup(vfo, QStringLiteral("NR1"));
        QVERIFY(popup);
        const QMap<QString, QSlider*> again = popupSliders(*popup);
        QCOMPARE(again.value(QStringLiteral("Taps"))->value(), 512);
        QCOMPARE(again.value(QStringLiteral("Delay"))->value(), 700);
        QCOMPARE(again.value(QStringLiteral("Gain"))->value(), 250);
        QCOMPARE(again.value(QStringLiteral("Leak"))->value(), 1000);
    }

    // Setup > DSP > NR/ANF's NR1 tab spans the same ranges and starts at
    // the slice's values.
    void nr1SetupTabRangesAreThetis()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        NrAnfSetupPage page(&model);
        auto* tabs = page.findChild<QTabWidget*>();
        QVERIFY(tabs);
        QWidget* nr1 = nullptr;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == QStringLiteral("NR1")) {
                nr1 = tabs->widget(i);
            }
        }
        QVERIFY(nr1);
        // The tab's sliders, in its order: Taps, Delay, Gain, Leak.
        const QList<QSlider*> sliders = nr1->findChildren<QSlider*>();
        QCOMPARE(sliders.size(), 4);
        checkRange(sliders.at(0), 1, 1024, 64);
        checkRange(sliders.at(1), 1, 1023, 16);
        checkRange(sliders.at(2), 1, 1000, 100);
        checkRange(sliders.at(3), 1, 1000, 100);
        sliders.at(2)->setValue(300);
        sliders.at(3)->setValue(20);
        QVERIFY(std::abs(slice->nr1Gain() - 300e-6) < 1e-12);
        QVERIFY(std::abs(slice->nr1Leakage() - 20e-3) < 1e-12);
    }

    // Catch a popup clipping Thetis's valid fractional values above 30.
    void nr2PopupWritesTheFullThetisRange()
    {
        RadioModel model;
        model.addSlice();
        auto* slice = model.activeSlice();
        QVERIFY(slice);
        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
        auto* popup = openPopup(vfo, QStringLiteral("NR2"));
        QVERIFY(popup);
        const auto sliders = popupSliders(*popup);
        for (const auto& label : {QStringLiteral("Factor"), QStringLiteral("Rate")}) {
            auto* slider = sliders.value(label);
            QVERIFY(slider);
            QCOMPARE(slider->minimum(), 0);
            QCOMPARE(slider->maximum(), 1000);
            slider->setValue(753);
            QCOMPARE(readoutOf(*popup, slider), QStringLiteral("75.3"));
        }
        QVERIFY(std::abs(slice->nr2Post2Factor() - 75.3) < 1e-9);
        QVERIFY(std::abs(slice->nr2Post2Rate() - 75.3) < 1e-9);
    }

    // The new defaults affect only missing settings, not a saved choice.
    void nr4DefaultsAndSavedChoices()
    {
        auto& settings = AppSettings::instance();
        settings.clear();
        SliceModel fresh;
        QCOMPARE(fresh.nr4Smoothing(), 0.0);
        QCOMPARE(fresh.nr4Whitening(), 0.0);
        QCOMPARE(static_cast<int>(fresh.nr4Algo()), 0);
        fresh.setNr4Smoothing(65.0);
        fresh.setNr4Whitening(2.0);
        fresh.setNr4Algo(static_cast<SbnrAlgo>(1));
        fresh.setNr2Post2Factor(75.3);
        fresh.saveToSettings(Band::Band20m);
        SliceModel restored;
        restored.restoreFromSettings(Band::Band20m);
        QCOMPARE(restored.nr4Smoothing(), 65.0);
        QCOMPARE(restored.nr4Whitening(), 2.0);
        QCOMPARE(static_cast<int>(restored.nr4Algo()), 1);
        QCOMPARE(restored.nr2Post2Factor(), 75.3);
        settings.clear();
    }

    // Positive SNR thresholds must be usable, and Rescale stops at 12.
    void nr4PopupUsesThetisLimits()
    {
        RadioModel model;
        model.addSlice();
        auto* slice = model.activeSlice();
        QVERIFY(slice);
        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
        auto* popup = openPopup(vfo, QStringLiteral("NR4"));
        QVERIFY(popup);
        const auto sliders = popupSliders(*popup);
        auto* rescale = sliders.value(QStringLiteral("Rescale"));
        auto* threshold = sliders.value(QStringLiteral("SNRthresh"));
        QVERIFY(rescale);
        QVERIFY(threshold);
        rescale->setValue(20);
        threshold->setValue(10);
        QCOMPARE(slice->nr4Rescale(), 12.0);
        QCOMPARE(slice->nr4PostThresh(), 10.0);
        threshold->setValue(-30);
        QCOMPARE(slice->nr4PostThresh(), -10.0);
    }

    void nr4SetupWithoutASliceUsesThetisDefaults()
    {
        RadioModel model;
        NrAnfSetupPage page(&model);
        auto* tabs = page.findChild<QTabWidget*>();
        QVERIFY(tabs);
        QWidget* nr4 = nullptr;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == QStringLiteral("NR4")) {
                nr4 = tabs->widget(i);
            }
        }
        QVERIFY(nr4);
        const auto sliders = nr4->findChildren<QSlider*>();
        QCOMPARE(sliders.size(), 5);
        QCOMPARE(sliders.at(1)->value(), 0);
        QCOMPARE(sliders.at(2)->value(), 0);
        bool firstSelected = false;
        for (auto* button : nr4->findChildren<QRadioButton*>()) {
            if (button->text() == QStringLiteral("Algo 1")) {
                firstSelected = button->isChecked();
            }
        }
        QVERIFY(firstSelected);
    }

    // MNR's Reset restores a new slice's values: MacNRFilter's DEF_*
    // (Strength 1.0, Aggressiveness 4, Floor 0.05, Alpha 0.92, Bias 1.2,
    // Gsmooth 0.70), and the popup writes them in the properties' units.
    void mnrResetRestoresANewSlicesValues()
    {
#ifndef HAVE_MNR
        QSKIP("MNR's quick controls open only where MNR runs (a Mac Core).");
#else
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        const SliceModel fresh;
        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
        DspParamPopup* popup = openPopup(vfo, QStringLiteral("MNR"));
        QVERIFY(popup);
        const QMap<QString, QSlider*> sliders = popupSliders(*popup);
        QCOMPARE(sliders.size(), 6);
        // Opened on a new slice, the sliders sit at its values.
        QCOMPARE(sliders.value(QStringLiteral("Aggressiveness"))->value(), 4);
        QCOMPARE(sliders.value(QStringLiteral("Bias"))->value(), 12);
        // Moved away, then Reset.
        for (QSlider* slider : sliders) {
            slider->setValue(slider->minimum() + 1);
        }
        QPushButton* reset = buttonNamed(*popup, QStringLiteral("Reset"));
        QVERIFY(reset);
        reset->click();
        QCOMPARE(QStringLiteral("Aggressiveness %1, Bias %2")
                     .arg(slice->mnrOversub()).arg(slice->mnrBias()),
                 QStringLiteral("Aggressiveness %1, Bias %2")
                     .arg(fresh.mnrOversub()).arg(fresh.mnrBias()));
        QVERIFY(std::abs(slice->mnrStrength() - fresh.mnrStrength()) < 1e-9);
        QVERIFY(std::abs(slice->mnrOversub() - fresh.mnrOversub()) < 1e-9);
        QVERIFY(std::abs(slice->mnrFloor() - fresh.mnrFloor()) < 1e-9);
        QVERIFY(std::abs(slice->mnrAlpha() - fresh.mnrAlpha()) < 1e-9);
        QVERIFY(std::abs(slice->mnrBias() - fresh.mnrBias()) < 1e-9);
        QVERIFY(std::abs(slice->mnrGsmooth() - fresh.mnrGsmooth()) < 1e-9);
#endif
    }

    // A new slice's MNR values are MacNRFilter's DEF_* values.
    void mnrSliceDefaultsAreTheFiltersOwn()
    {
        const SliceModel slice;
        QVERIFY(std::abs(slice.mnrStrength() - 1.0) < 1e-9);
        QVERIFY(std::abs(slice.mnrOversub() - 4.0) < 1e-9);
        QVERIFY(std::abs(slice.mnrFloor() - 0.05) < 1e-9);
        QVERIFY(std::abs(slice.mnrAlpha() - 0.92) < 1e-9);
        QVERIFY(std::abs(slice.mnrBias() - 1.2) < 1e-9);
        QVERIFY(std::abs(slice.mnrGsmooth() - 0.70) < 1e-9);
    }

    // Setup's MNR tab, with no slice to read, starts where a new slice does.
    void mnrSetupTabWithoutASliceStartsAtTheDefaults()
    {
        RadioModel model;
        NrAnfSetupPage page(&model);
        auto* tabs = page.findChild<QTabWidget*>();
        QVERIFY(tabs);
        QWidget* mnr = nullptr;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == QStringLiteral("MNR")) {
                mnr = tabs->widget(i);
            }
        }
        QVERIFY(mnr);
        // Strength, Aggressiveness, Floor, Alpha, Bias, Gsmooth.
        const QList<QSlider*> sliders = mnr->findChildren<QSlider*>();
        QCOMPARE(sliders.size(), 6);
        QList<int> values;
        for (const QSlider* slider : sliders) {
            values.append(slider->value());
        }
        QCOMPARE(values, (QList<int>{100, 4, 50, 92, 12, 70}));
    }
};

QTEST_MAIN(TstNrQuickControls)
#include "tst_nr_quick_controls.moc"
