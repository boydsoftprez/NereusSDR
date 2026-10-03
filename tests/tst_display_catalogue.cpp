// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_display_catalogue.cpp  (NereusSDR)
// =================================================================
//
// The Core's catalogue `display` key against the desktop's Setup > Display
// pages (R-IOS-18, R-IOS-27, R-IOS-06, R-R3-08):
//
//   - Each entry equals what Spectrum Defaults or Waterfall Defaults offers:
//     its label, items, range, step, decimals and unit, and the default the
//     page shows with nothing stored, for an ANAN-G2 and a Hermes Lite 2.
//   - The bin width the FFT size gives, sample rate over size to three
//     places, is the page's readout at each rate the board offers.
//   - The page behaves as it did before its values moved to
//     ControlRanges.h: the same ranges, items and labels, and the same
//     settings written when the FFT size, window, Hz/bin target and FPS
//     move.
//   - The FFT sizes a pan asks for follow the catalogue's `fftPlan`.
//
//   cmake --build build --target tst_display_catalogue
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_display_catalogue$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QScopeGuard>
#include <QSlider>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/ControlRanges.h"
#include "core/FFTEngine.h"
#include "core/HpsdrModel.h"
#include "core/SampleRateCatalog.h"
#include "core/session/StationCatalog.h"
#include "models/RadioModel.h"
#define private public
#include "gui/SpectrumWidget.h"
#include "gui/setup/DisplaySetupPages.h"
#undef private

using namespace NereusSDR;

namespace {

StationCatalog::Inputs inputsFor(HPSDRModel model, ProtocolVersion protocol)
{
    StationCatalog::Inputs inputs;
    inputs.model = model;
    inputs.board = BoardCapsTable::forModel(model);
    inputs.protocol = protocol;
    return inputs;
}

QJsonObject displayOf(HPSDRModel model, ProtocolVersion protocol)
{
    return StationCatalog::build(inputsFor(model, protocol))
        .value(QStringLiteral("display"))
        .toObject();
}

// The entry for `label` on `page`.
QJsonObject control(const QJsonObject& display, const QString& page, const QString& label)
{
    for (const QJsonValue& value : display.value(QStringLiteral("controls")).toArray()) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("page")).toString() == page
            && entry.value(QStringLiteral("label")).toString() == label) {
            return entry;
        }
    }
    return {};
}

// The form label a row's field sits beside, without its colon.
QString formLabel(QWidget* field)
{
    auto* group = qobject_cast<QGroupBox*>(field->parentWidget());
    auto* form = group ? qobject_cast<QFormLayout*>(group->layout()) : nullptr;
    if (!form) {
        // makeSliderRow puts the slider in a container row.
        QWidget* container = field->parentWidget();
        group = container ? qobject_cast<QGroupBox*>(container->parentWidget()) : nullptr;
        form = group ? qobject_cast<QFormLayout*>(group->layout()) : nullptr;
        field = container;
    }
    if (!form) { return {}; }
    auto* label = qobject_cast<QLabel*>(form->labelForField(field));
    QString text = label ? label->text() : QString();
    if (text.endsWith(QLatin1Char(':'))) { text.chop(1); }
    return text;
}

QString groupTitle(QWidget* field)
{
    for (QWidget* w = field->parentWidget(); w; w = w->parentWidget()) {
        if (auto* group = qobject_cast<QGroupBox*>(w)) { return group->title(); }
    }
    return {};
}

QStringList comboItems(const QComboBox* combo)
{
    QStringList items;
    for (int i = 0; i < combo->count(); ++i) { items.append(combo->itemText(i)); }
    return items;
}

QStringList optionLabels(const QJsonObject& entry)
{
    QStringList labels;
    int expected = 0;
    for (const QJsonValue& value : entry.value(QStringLiteral("options")).toArray()) {
        const QJsonObject option = value.toObject();
        // Each option's value is its combo index.
        if (option.value(QStringLiteral("value")).toInt() != expected++) { return {}; }
        labels.append(option.value(QStringLiteral("label")).toString());
    }
    return labels;
}

} // namespace

class TstDisplayCatalogue : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // The desktop page as it was before its values moved to the shared
    // table: every literal it held, and what it writes.
    void theDesktopPageIsUnchanged()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SpectrumWidget widget;
        NereusSDR::FFTEngine idle(-1);
        remote.setSpectrumWidget(&widget);
        remote.setFftEngine(&idle);
        const auto detach = qScopeGuard([&] {
            remote.setSpectrumWidget(nullptr);
            remote.setFftEngine(nullptr);
        });
        SpectrumDefaultsPage page(&remote);
        WaterfallDefaultsPage wf(&remote);
        page.setStationSettingsAvailable(true, QString());

        QCOMPARE(page.m_fftSizeSlider->minimum(), 0);
        QCOMPARE(page.m_fftSizeSlider->maximum(), 6);
        QCOMPARE(groupTitle(page.m_fftSizeSlider), QStringLiteral("Fast Fourier Transform"));
        QCOMPARE(comboItems(page.m_windowCombo),
                 (QStringList{QStringLiteral("Rectangular"), QStringLiteral("Blackman-Harris 4T"),
                              QStringLiteral("Hann"), QStringLiteral("Flat-Top"),
                              QStringLiteral("Hamming"), QStringLiteral("Kaiser"),
                              QStringLiteral("Blackman-Harris 7T")}));
        QCOMPARE(page.m_hzPerBinTargetSpin->minimum(), 0.0);
        QCOMPARE(page.m_hzPerBinTargetSpin->maximum(), 200.0);
        QCOMPARE(page.m_hzPerBinTargetSpin->singleStep(), 0.5);
        QCOMPARE(page.m_hzPerBinTargetSpin->decimals(), 2);
        QCOMPARE(page.m_hzPerBinTargetSpin->specialValueText(), QStringLiteral("Off"));
        QCOMPARE(page.m_hzPerBinTargetSpin->suffix(), QStringLiteral(" Hz/bin"));
        QCOMPARE(page.m_fpSlider->minimum(), 10);
        QCOMPARE(page.m_fpSlider->maximum(), 60);
        QCOMPARE(page.m_fpSpin->suffix(), QStringLiteral(" fps"));
        QCOMPARE(formLabel(page.m_fpSlider), QStringLiteral("FPS"));
        QCOMPARE(comboItems(page.m_spectrumDetectorCombo),
                 (QStringList{QStringLiteral("Peak"), QStringLiteral("Rosenfell"),
                              QStringLiteral("Average"), QStringLiteral("Sample"),
                              QStringLiteral("RMS")}));
        QCOMPARE(formLabel(page.m_spectrumDetectorCombo), QStringLiteral("Spectrum Detector"));
        QCOMPARE(comboItems(page.m_spectrumAveragingCombo),
                 (QStringList{QStringLiteral("None"), QStringLiteral("Recursive"),
                              QStringLiteral("Time Window"), QStringLiteral("Log Recursive")}));
        QCOMPARE(formLabel(page.m_spectrumAveragingCombo), QStringLiteral("Spectrum Averaging"));
        QCOMPARE(page.m_averagingTimeSpin->minimum(), 10);
        QCOMPARE(page.m_averagingTimeSpin->maximum(), 9999);
        QCOMPARE(page.m_averagingTimeSpin->singleStep(), 10);
        QCOMPARE(formLabel(page.m_averagingTimeSpin), QStringLiteral("Spectrum Avg Time"));
        QCOMPARE(page.m_decimationSpin->minimum(), 1);
        QCOMPARE(page.m_decimationSpin->maximum(), 16);
        QCOMPARE(page.m_decimationSpin->value(), 1);
        QCOMPARE(formLabel(page.m_decimationSpin), QStringLiteral("Decimation"));
        QCOMPARE(groupTitle(page.m_decimationSpin), QStringLiteral("Rendering"));
        QCOMPARE(comboItems(wf.m_waterfallDetectorCombo),
                 (QStringList{QStringLiteral("Peak"), QStringLiteral("Rosenfell"),
                              QStringLiteral("Average"), QStringLiteral("Sample")}));
        QCOMPARE(formLabel(wf.m_waterfallDetectorCombo), QStringLiteral("WF Detector"));
        QCOMPARE(comboItems(wf.m_waterfallAveragingCombo),
                 (QStringList{QStringLiteral("None"), QStringLiteral("Recursive"),
                              QStringLiteral("Time Window"), QStringLiteral("Log Recursive")}));
        QCOMPARE(formLabel(wf.m_waterfallAveragingCombo), QStringLiteral("WF Averaging"));
        QCOMPARE(wf.m_waterfallAvgTimeSpin->minimum(), 10);
        QCOMPARE(wf.m_waterfallAvgTimeSpin->maximum(), 9999);
        QCOMPARE(wf.m_waterfallAvgTimeSpin->singleStep(), 10);
        QCOMPARE(formLabel(wf.m_waterfallAvgTimeSpin), QStringLiteral("WF Avg Time"));
        QCOMPARE(groupTitle(wf.m_waterfallAvgTimeSpin), QStringLiteral("Display"));
        QCOMPARE(page.pageTitle(), QStringLiteral("Spectrum Defaults"));
        QCOMPARE(wf.pageTitle(), QStringLiteral("Waterfall Defaults"));

        // Nothing stored: 4096 points, Blackman-Harris 4T, Off, 30 fps.
        QCOMPARE(page.m_fftSizeSlider->value(), 0);
        QCOMPARE(page.m_windowCombo->currentIndex(), 1);
        QCOMPARE(page.m_hzPerBinTargetSpin->value(), 0.0);
        QCOMPARE(page.m_fpSlider->value(), 30);
        QCOMPARE(page.m_fftSizeReadout->text(), QStringLiteral("4096"));

        // What it writes: 4096 x 2^position, the window's index, the target
        // and the rate, as before.
        auto& settings = AppSettings::instance();
        // From the stored 0, every other position, then back to 0.
        for (int position : {1, 2, 3, 4, 5, 6, 0}) {
            page.m_fftSizeSlider->setValue(position);
            QCOMPARE(settings.value(QStringLiteral("DisplayFftSize")).toString(),
                     QString::number(4096 << position));
            QCOMPARE(page.m_fftSizeReadout->text(), QString::number(4096 << position));
        }
        page.m_windowCombo->setCurrentIndex(6);
        QCOMPARE(settings.value(QStringLiteral("DisplayFftWindow")).toString(),
                 QStringLiteral("6"));
        page.m_hzPerBinTargetSpin->setValue(2.5);
        QCOMPARE(settings.value(QStringLiteral("DisplayHzPerBinTarget")).toString(),
                 QStringLiteral("2.5"));
    }

    void theCatalogueIsWhatThePageOffers_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("protocol");
        QTest::newRow("ANAN-G2") << int(HPSDRModel::ANAN_G2) << int(ProtocolVersion::Protocol2);
        QTest::newRow("Hermes Lite 2")
            << int(HPSDRModel::HERMESLITE) << int(ProtocolVersion::Protocol1);
    }

    // Every `display` entry equals the control the desktop page draws, for
    // each radio; the defaults are what the page shows with nothing stored.
    void theCatalogueIsWhatThePageOffers()
    {
        QFETCH(int, model);
        QFETCH(int, protocol);
        const QJsonObject display =
            displayOf(static_cast<HPSDRModel>(model), static_cast<ProtocolVersion>(protocol));
        const QStringList displayKeys = display.keys();
        QCOMPARE(QSet<QString>(displayKeys.cbegin(), displayKeys.cend()),
                 (QSet<QString>{QStringLiteral("controls"), QStringLiteral("binWidth"),
                                QStringLiteral("fftPlan")}));
        QCOMPARE(display.value(QStringLiteral("controls")).toArray().size(), 11);

        RadioModel remote(RadioModel::Role::Remote);
        SpectrumWidget widget;
        NereusSDR::FFTEngine idle(-1);
        remote.setSpectrumWidget(&widget);
        remote.setFftEngine(&idle);
        const auto detach = qScopeGuard([&] {
            remote.setSpectrumWidget(nullptr);
            remote.setFftEngine(nullptr);
        });
        SpectrumDefaultsPage page(&remote);
        WaterfallDefaultsPage wf(&remote);
        page.setStationSettingsAvailable(true, QString());
        const QString spectrum = page.pageTitle();
        const QString waterfall = wf.pageTitle();

        // Size: the slider's seven positions, as the sizes it writes.
        const QJsonObject size = control(display, spectrum, QStringLiteral("Size"));
        QCOMPARE(size.value(QStringLiteral("settingsKey")).toString(),
                 QStringLiteral("DisplayFftSize"));
        QCOMPARE(size.value(QStringLiteral("scope")).toString(), QStringLiteral("station"));
        QCOMPARE(size.value(QStringLiteral("kind")).toString(), QStringLiteral("slider"));
        QCOMPARE(size.value(QStringLiteral("group")).toString(), groupTitle(page.m_fftSizeSlider));
        const QJsonArray sizes = size.value(QStringLiteral("options")).toArray();
        QCOMPARE(sizes.size(), page.m_fftSizeSlider->maximum() - page.m_fftSizeSlider->minimum() + 1);
        // Downwards from the top, so each position moves the slider.
        for (int position = page.m_fftSizeSlider->maximum();
             position >= page.m_fftSizeSlider->minimum(); --position) {
            page.m_fftSizeSlider->setValue(position);
            const QJsonObject option = sizes.at(position).toObject();
            QCOMPARE(QString::number(option.value(QStringLiteral("value")).toInt()),
                     AppSettings::instance().value(QStringLiteral("DisplayFftSize")).toString());
            QCOMPARE(option.value(QStringLiteral("label")).toString(),
                     page.m_fftSizeReadout->text());
        }
        AppSettings::instance().remove(QStringLiteral("DisplayFftSize"));
        page.loadStationSpectrumSettings();
        QCOMPARE(sizes.at(page.m_fftSizeSlider->value()).toObject().value(QStringLiteral("value")),
                 size.value(QStringLiteral("default")));

        // Window.
        const QJsonObject window = control(display, spectrum, QStringLiteral("Window"));
        QCOMPARE(window.value(QStringLiteral("settingsKey")).toString(),
                 QStringLiteral("DisplayFftWindow"));
        QCOMPARE(window.value(QStringLiteral("kind")).toString(), QStringLiteral("choice"));
        QCOMPARE(optionLabels(window), comboItems(page.m_windowCombo));
        QCOMPARE(window.value(QStringLiteral("default")).toInt(), page.m_windowCombo->currentIndex());
        QCOMPARE(window.value(QStringLiteral("subscribe")).toString(), QStringLiteral("windowType"));

        // Hz/bin Target.
        const QJsonObject target = control(display, spectrum, QStringLiteral("Hz/bin Target"));
        QCOMPARE(target.value(QStringLiteral("settingsKey")).toString(),
                 QStringLiteral("DisplayHzPerBinTarget"));
        QCOMPARE(target.value(QStringLiteral("min")).toDouble(), page.m_hzPerBinTargetSpin->minimum());
        QCOMPARE(target.value(QStringLiteral("max")).toDouble(), page.m_hzPerBinTargetSpin->maximum());
        QCOMPARE(target.value(QStringLiteral("step")).toDouble(),
                 page.m_hzPerBinTargetSpin->singleStep());
        QCOMPARE(target.value(QStringLiteral("decimals")).toInt(),
                 page.m_hzPerBinTargetSpin->decimals());
        QCOMPARE(QStringLiteral(" ") + target.value(QStringLiteral("unit")).toString(),
                 page.m_hzPerBinTargetSpin->suffix());
        QCOMPARE(target.value(QStringLiteral("offLabel")).toString(),
                 page.m_hzPerBinTargetSpin->specialValueText());
        QCOMPARE(target.value(QStringLiteral("offValue")).toDouble(),
                 page.m_hzPerBinTargetSpin->minimum());
        QCOMPARE(target.value(QStringLiteral("default")).toDouble(),
                 page.m_hzPerBinTargetSpin->value());

        // FPS.
        const QJsonObject fps = control(display, spectrum, QStringLiteral("FPS"));
        QCOMPARE(fps.value(QStringLiteral("settingsKey")).toString(),
                 QStringLiteral("DisplaySpectrumFps"));
        QCOMPARE(fps.value(QStringLiteral("min")).toInt(), page.m_fpSlider->minimum());
        QCOMPARE(fps.value(QStringLiteral("max")).toInt(), page.m_fpSlider->maximum());
        QCOMPARE(fps.value(QStringLiteral("step")).toInt(), page.m_fpSlider->singleStep());
        QCOMPARE(fps.value(QStringLiteral("default")).toInt(), page.m_fpSlider->value());
        QCOMPARE(QStringLiteral(" ") + fps.value(QStringLiteral("unit")).toString(),
                 page.m_fpSpin->suffix());
        QCOMPARE(fps.value(QStringLiteral("group")).toString(), groupTitle(page.m_fpSlider));

        // The four station keys are the Core's; the rest are each device's,
        // sent in its subscription.
        int station = 0;
        for (const QJsonValue& value : display.value(QStringLiteral("controls")).toArray()) {
            const QJsonObject entry = value.toObject();
            if (entry.value(QStringLiteral("scope")).toString() == QStringLiteral("station")) {
                ++station;
            } else {
                QCOMPARE(entry.value(QStringLiteral("scope")).toString(), QStringLiteral("device"));
            }
            QVERIFY(!entry.value(QStringLiteral("subscribe")).toString().isEmpty());
        }
        QCOMPARE(station, 4);

        // The choices and times each device keeps: as the pages draw them,
        // defaults as a new SpectrumWidget holds them.
        struct Choice {
            QString page;
            QComboBox* combo;
            QString key;
            QString subscribe;
            int widgetDefault;
        };
        const Choice choices[] = {
            {spectrum, page.m_spectrumDetectorCombo, QStringLiteral("DisplaySpectrumDetector"),
             QStringLiteral("trace.detector"), int(widget.spectrumDetector())},
            {spectrum, page.m_spectrumAveragingCombo, QStringLiteral("DisplaySpectrumAveraging"),
             QStringLiteral("trace.averageMode"), int(widget.spectrumAveraging())},
            {waterfall, wf.m_waterfallDetectorCombo, QStringLiteral("DisplayWaterfallDetector"),
             QStringLiteral("waterfall.detector"), int(widget.waterfallDetector())},
            {waterfall, wf.m_waterfallAveragingCombo, QStringLiteral("DisplayWaterfallAveraging"),
             QStringLiteral("waterfall.averageMode"), int(widget.waterfallAveraging())},
        };
        for (const Choice& choice : choices) {
            const QJsonObject entry = control(display, choice.page, formLabel(choice.combo));
            QVERIFY2(!entry.isEmpty(), qPrintable(formLabel(choice.combo)));
            QCOMPARE(entry.value(QStringLiteral("settingsKey")).toString(), choice.key);
            QCOMPARE(entry.value(QStringLiteral("subscribe")).toString(), choice.subscribe);
            QCOMPARE(entry.value(QStringLiteral("group")).toString(), groupTitle(choice.combo));
            QCOMPARE(optionLabels(entry), comboItems(choice.combo));
            QCOMPARE(entry.value(QStringLiteral("default")).toInt(), choice.widgetDefault);
        }
        struct Spin {
            QString page;
            QSpinBox* spin;
            QString subscribe;
            int widgetDefault;
        };
        const Spin spins[] = {
            {spectrum, page.m_averagingTimeSpin, QStringLiteral("averageTimeMs"),
             widget.spectrumAverageTimeMs()},
            {waterfall, wf.m_waterfallAvgTimeSpin, QStringLiteral("waterfallAverageTimeMs"),
             widget.waterfallAverageTimeMs()},
            {spectrum, page.m_decimationSpin, QStringLiteral("decimation"),
             page.m_decimationSpin->value()},
        };
        for (const Spin& spin : spins) {
            const QJsonObject entry = control(display, spin.page, formLabel(spin.spin));
            QVERIFY2(!entry.isEmpty(), qPrintable(formLabel(spin.spin)));
            QCOMPARE(entry.value(QStringLiteral("kind")).toString(), QStringLiteral("slider"));
            QCOMPARE(entry.value(QStringLiteral("subscribe")).toString(), spin.subscribe);
            QCOMPARE(entry.value(QStringLiteral("min")).toInt(), spin.spin->minimum());
            QCOMPARE(entry.value(QStringLiteral("max")).toInt(), spin.spin->maximum());
            QCOMPARE(entry.value(QStringLiteral("step")).toInt(), spin.spin->singleStep());
            QCOMPARE(entry.value(QStringLiteral("decimals")).toInt(), 0);
            QString suffix = spin.spin->suffix().trimmed();
            QCOMPARE(entry.value(QStringLiteral("unit")).toString(), suffix);
            QCOMPARE(entry.value(QStringLiteral("default")).toInt(), spin.widgetDefault);
            QCOMPARE(entry.value(QStringLiteral("group")).toString(), groupTitle(spin.spin));
        }
        // Decimation has no setting on the desktop.
        QVERIFY(control(display, spectrum, QStringLiteral("Decimation"))
                    .value(QStringLiteral("settingsKey")).isNull());

    }

    // The bin width readout at each rate the radio offers is its sample
    // rate over the FFT size, to the catalogue's places.
    void theBinWidthIsTheRateOverTheSize_data() { theCatalogueIsWhatThePageOffers_data(); }
    void theBinWidthIsTheRateOverTheSize()
    {
        QFETCH(int, model);
        QFETCH(int, protocol);
        const auto hpsdr = static_cast<HPSDRModel>(model);
        const auto proto = static_cast<ProtocolVersion>(protocol);
        const QJsonObject display = displayOf(hpsdr, proto);
        const QJsonObject binWidth = display.value(QStringLiteral("binWidth")).toObject();
        const int decimals = binWidth.value(QStringLiteral("decimals")).toInt();
        QCOMPARE(decimals, 3);
        const std::vector<int> rates =
            allowedSampleRates(proto, BoardCapsTable::forModel(hpsdr), hpsdr);
        QVERIFY(!rates.empty());

        RadioModel local;
        SpectrumWidget widget;
        NereusSDR::FFTEngine engine(-1);
        local.setSpectrumWidget(&widget);
        local.setFftEngine(&engine);
        const auto detach = qScopeGuard([&] {
            local.setSpectrumWidget(nullptr);
            local.setFftEngine(nullptr);
        });
        SpectrumDefaultsPage page(&local);
        QCOMPARE(binWidth.value(QStringLiteral("label")).toString(), QStringLiteral("Bin Width (Hz)"));
        const QJsonArray sizes =
            control(display, page.pageTitle(), QStringLiteral("Size")).value(QStringLiteral("options")).toArray();
        for (int rate : rates) {
            engine.setSampleRate(rate);
            for (int position = 0; position < sizes.size(); ++position) {
                // Move off and back so the readout is written at this rate.
                page.m_fftSizeSlider->setValue(position == 0 ? 1 : 0);
                page.m_fftSizeSlider->setValue(position);
                const int size = sizes.at(position).toObject().value(QStringLiteral("value")).toInt();
                QCOMPARE(page.m_binWidthLabel->text(),
                         QString::number(double(rate) / size, 'f', decimals));
            }
        }
    }

    // The FFT sizes a pan asks for: powers of two from fftPlan.minFftSize up
    // to fftPlan.maxFftSize, the engine's own limits.
    void theFftPlanIsTheEnginesSizes()
    {
        const QJsonObject plan =
            displayOf(HPSDRModel::ANAN_G2, ProtocolVersion::Protocol2)
                .value(QStringLiteral("fftPlan")).toObject();
        QCOMPARE(plan.value(QStringLiteral("minFftSize")).toInt(),
                 ControlRanges::kDisplayFftPlanMinSize);
        QCOMPARE(plan.value(QStringLiteral("maxFftSize")).toInt(), NereusSDR::FFTEngine::maximumFftSize());
        QCOMPARE(plan.value(QStringLiteral("minFftSize")).toInt(), 1024);
        // The slider's largest size is the engine's largest.
        QCOMPARE(ControlRanges::displayFftSizeAt(ControlRanges::kDisplayFftSizePositionMax),
                 NereusSDR::FFTEngine::maximumFftSize());
    }
};

QTEST_MAIN(TstDisplayCatalogue)
#include "tst_display_catalogue.moc"
