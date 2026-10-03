// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationCatalog.cpp  (NereusSDR)
// =================================================================
//
// See StationCatalog.h. The values come from where the desktop reads them;
// this file only arranges them as the link document's Catalogue section
// describes.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: the `receive` key (AF gain, SSQL, AM and FM squelch
//               ranges). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: each band-plan segment's `lowestClass`, from the band-plan
//               strip's own rule (lowestLicenceClass, models/BandPlan.h).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25: the `bands` key, the desktop's per-pan BAND grid from its
//               own table (kBandGrid, models/BandGrid.h), for an app's band
//               buttons (R-IOS-27, R-IOS-06). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: each band plan's `active` (the Core's own plan, from
//               BandPlanManager::activePlanName()) and `spots` (D79;
//               R-IOS-11, R-R3-49). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: the `display` key, Setup > Display's FFT, Rendering and
//               waterfall controls from ControlRanges.h, the table the
//               desktop page reads (R-IOS-18, R-IOS-27, R-IOS-06,
//               R-R3-08). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: board.transmit (the transmit controls' ranges and what
//               they show, from HpsdrModel.h and the board's mic range),
//               board.rx1Preamp, board.relays (rxOutOnTxPresent and
//               SkuUiProfile) and the top-level `noiseReduction` (the NR
//               quick controls from ControlRanges.h; R-IOS-06, R-IOS-27).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: board.transmit's `shown.rounding` (RF Power halfEven, the
//               HL2 drive snap). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: `shown.endSnap`, and the tune keys' rounding from mi0bot's
//               HL2 tune readouts (PowerShownRule). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25 (D41): each tool's and Radio
//               item's `offered` from the unbuilt features list and the
//               radio and Core it runs (PureSignal present, a diversity
//               receiver, the station TCI server, VAX devices, antenna
//               control). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: Level Cal 2: board.rx2Attenuator, rx2PreampItems and
//               rx2AttenuatorReason, RX2's own input control per model
//               (rx2AttenuatorVersion 1). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: Radio codec lane: board.radioMic and radioMicNote
//               (radioMicVersion 1). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/StationCatalog.h"

#include "core/AudioEngine.h"
#include "core/ControlRanges.h"
#include "core/HardwareProfile.h"
#include "core/SampleRateCatalog.h"
#include "core/SkuUiProfile.h"
#include "core/UnbuiltFeatureList.h"
#include "core/spectrum/WaterfallPalettes.h"
#include "models/BandGrid.h"
#include "models/BandPlanManager.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/StationTciModel.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

Q_LOGGING_CATEGORY(lcCatalog, "nereus.station.catalog")

// The 14 modes, DSPMode 0 to 13.
constexpr int kModeCount = static_cast<int>(DSPMode::RADE_L) + 1;

QString colourText(std::uint32_t rgb)
{
    return QColor(static_cast<QRgb>(rgb)).name(QColor::HexRgb).toUpper();
}

QString colourText(const QColor& colour)
{
    return colour.name(QColor::HexRgb).toUpper();
}

// Which side of the carrier a mode's passband sits on: the sign of its
// filters (SliceModel::presetsForMode). The lower three are the ones
// Thetis turns the two-tone round for (setup.cs:11058 [v2.10.3.13], as
// TwoToneController::isLowerSidebandMode reads it), and RADE-L mirrors
// RADE-U.
QString sidebandOf(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::CWL:
    case DSPMode::DIGL:
    case DSPMode::RADE_L:
        return QStringLiteral("lower");
    case DSPMode::USB:
    case DSPMode::CWU:
    case DSPMode::DIGU:
    case DSPMode::RADE_U:
        return QStringLiteral("upper");
    case DSPMode::DSB:
    case DSPMode::FM:
    case DSPMode::AM:
    case DSPMode::SPEC:
    case DSPMode::SAM:
    case DSPMode::DRM:
        return QStringLiteral("both");
    }
    return QStringLiteral("both");
}

// "1 Hz", "500 Hz", "1 kHz", "2.5 kHz", "1 MHz".
QString tuneStepLabel(int hz)
{
    if (hz >= 1000000) {
        return QStringLiteral("%1 MHz").arg(QString::number(hz / 1.0e6, 'g', 10));
    }
    if (hz >= 1000) {
        return QStringLiteral("%1 kHz").arg(QString::number(hz / 1.0e3, 'g', 10));
    }
    return QStringLiteral("%1 Hz").arg(hz);
}

QJsonObject rangeObject(double min, double max, double step)
{
    return QJsonObject{{QStringLiteral("min"), min},
                       {QStringLiteral("max"), max},
                       {QStringLiteral("step"), step}};
}

QJsonArray modesArray()
{
    QJsonArray modes;
    for (int id = 0; id < kModeCount; ++id) {
        const auto mode = static_cast<DSPMode>(id);
        modes.append(QJsonObject{{QStringLiteral("id"), id},
                                 {QStringLiteral("label"), SliceModel::modeName(mode)},
                                 {QStringLiteral("sideband"), sidebandOf(mode)}});
    }
    return modes;
}

QJsonObject filterPresetsObject(const StationCatalog::Inputs& inputs)
{
    QJsonObject presets;
    for (const auto& [mode, list] : inputs.filterPresets) {
        QJsonArray entries;
        for (int slot = 0; slot < list.size(); ++slot) {
            const FilterPreset& preset = list.at(slot);
            entries.append(QJsonObject{{QStringLiteral("slot"), slot},
                                       {QStringLiteral("label"), preset.name},
                                       {QStringLiteral("lowHz"), preset.low},
                                       {QStringLiteral("highHz"), preset.high}});
        }
        presets.insert(SliceModel::modeName(mode), entries);
    }
    return presets;
}

QJsonArray tuneStepsArray(const StationCatalog::Inputs& inputs)
{
    QJsonArray steps;
    for (int hz : inputs.tuneStepsHz) {
        steps.append(QJsonObject{{QStringLiteral("hz"), hz},
                                 {QStringLiteral("label"), tuneStepLabel(hz)}});
    }
    return steps;
}

QJsonObject agcObject()
{
    QJsonArray modes;
    for (const ControlRanges::AgcModeItem& item : ControlRanges::kAgcModes) {
        modes.append(QJsonObject{{QStringLiteral("id"), item.id},
                                 {QStringLiteral("label"), QString::fromLatin1(item.label)}});
    }
    // The Modes tab's AGC section shows the five modes, AGC-T and AUTO (an
    // on/off switch), so AGC-T is the one range it needs.
    return QJsonObject{
        {QStringLiteral("modes"), modes},
        {QStringLiteral("thresholdDb"),
         rangeObject(ControlRanges::kAgcThresholdMinDb, ControlRanges::kAgcThresholdMaxDb,
                     ControlRanges::kAgcThresholdStepDb)},
    };
}

// The receive controls' ranges, each as the desktop's own control holds it:
// the AF slider and the SQL slider in slider units, the AM and FM squelch
// thresholds in dB (ControlRanges.h).
QJsonObject receiveObject()
{
    using namespace ControlRanges;
    return QJsonObject{
        {QStringLiteral("afGain"), rangeObject(kAfGainMin, kAfGainMax, kAfGainStep)},
        {QStringLiteral("ssqlThresh"),
         rangeObject(kSsqlThreshMin, kSsqlThreshMax, kSsqlThreshStep)},
        {QStringLiteral("amsqThresh"),
         rangeObject(kAmsqThreshMinDb, kAmsqThreshMaxDb, kAmsqThreshStepDb)},
        {QStringLiteral("fmsqThresh"),
         rangeObject(kFmsqThreshMinDb, kFmsqThreshMaxDb, kFmsqThreshStepDb)},
    };
}

// ── display ─────────────────────────────────────────────────────────────
//
// Setup > Display's Spectrum Defaults and Waterfall Defaults controls, from
// the table the desktop page reads (ControlRanges.h). Each entry names the
// desktop's settings key, where its value lives (`station`: a Core setting
// a device writes with settings.write; `device`: each device's own, sent
// in its spectrum subscription), the subscription field it reaches, and
// the control as the desktop draws it. The same on every radio: the bin
// width the FFT size gives is the pan's sample rate over the size, and the
// rates are the board's (`board.sampleRates`).

QJsonArray displayChoices(const ControlRanges::DisplayChoiceItem* items, std::size_t count)
{
    QJsonArray options;
    for (std::size_t i = 0; i < count; ++i) {
        options.append(QJsonObject{{QStringLiteral("value"), items[i].value},
                                   {QStringLiteral("label"), QString::fromLatin1(items[i].label)}});
    }
    return options;
}

template <std::size_t N>
QJsonArray displayChoices(const std::array<ControlRanges::DisplayChoiceItem, N>& items)
{
    return displayChoices(items.data(), items.size());
}

QJsonValue keyOrNull(const char* key)
{
    return key != nullptr ? QJsonValue(QString::fromLatin1(key)) : QJsonValue(QJsonValue::Null);
}

QJsonObject displayControl(const char* settingsKey, const char* scope, const char* subscribe,
                           const char* page, const char* group, const char* label,
                           const char* kind)
{
    return QJsonObject{
        {QStringLiteral("settingsKey"), keyOrNull(settingsKey)},
        {QStringLiteral("scope"), QString::fromLatin1(scope)},
        {QStringLiteral("subscribe"), QString::fromLatin1(subscribe)},
        {QStringLiteral("page"), QString::fromLatin1(page)},
        {QStringLiteral("group"), QString::fromLatin1(group)},
        {QStringLiteral("label"), QString::fromLatin1(label)},
        {QStringLiteral("kind"), QString::fromLatin1(kind)},
    };
}

QJsonObject displayChoice(QJsonObject control, const QJsonArray& options, int defaultValue)
{
    control.insert(QStringLiteral("options"), options);
    control.insert(QStringLiteral("default"), defaultValue);
    return control;
}

QJsonObject displaySlider(QJsonObject control, double min, double max, double step,
                          const char* unit, int decimals, double defaultValue)
{
    control.insert(QStringLiteral("min"), min);
    control.insert(QStringLiteral("max"), max);
    control.insert(QStringLiteral("step"), step);
    control.insert(QStringLiteral("unit"), QString::fromLatin1(unit));
    control.insert(QStringLiteral("decimals"), decimals);
    control.insert(QStringLiteral("default"), defaultValue);
    return control;
}

QJsonObject displayObject()
{
    using namespace ControlRanges;
    const char* const spectrum = kDisplaySpectrumPageTitle;
    const char* const waterfall = kDisplayWaterfallPageTitle;
    const char* const fft = kDisplayFftGroupTitle;
    const char* const rendering = kDisplayRenderingGroupTitle;
    const char* const wfGroup = kDisplayWaterfallGroupTitle;

    // The FFT size slider steps through its seven sizes in order.
    QJsonArray fftSizes;
    for (int position = 0; position <= kDisplayFftSizePositionMax; ++position) {
        const int size = displayFftSizeAt(position);
        fftSizes.append(QJsonObject{{QStringLiteral("value"), size},
                                    {QStringLiteral("label"), QString::number(size)}});
    }
    QJsonObject fftSize = displayControl(kDisplayFftSizeKey, "station", "fftSize", spectrum, fft,
                                         kDisplayFftSizeLabel, "slider");
    fftSize.insert(QStringLiteral("options"), fftSizes);
    fftSize.insert(QStringLiteral("default"), kDisplayFftSizeDefault);

    QJsonObject hzPerBin = displaySlider(
        displayControl(kDisplayHzPerBinTargetKey, "station", "fftSize", spectrum, fft,
                       kDisplayHzPerBinTargetLabel, "slider"),
        kDisplayHzPerBinTargetMin, kDisplayHzPerBinTargetMax, kDisplayHzPerBinTargetStep,
        kDisplayHzPerBinTargetUnit, kDisplayHzPerBinTargetDecimals,
        kDisplayHzPerBinTargetDefault);
    hzPerBin.insert(QStringLiteral("offValue"), kDisplayHzPerBinTargetMin);
    hzPerBin.insert(QStringLiteral("offLabel"),
                    QString::fromLatin1(kDisplayHzPerBinTargetOffLabel));

    const QJsonArray controls{
        fftSize,
        displayChoice(displayControl(kDisplayFftWindowKey, "station", "windowType", spectrum,
                                     fft, kDisplayFftWindowLabel, "choice"),
                      displayChoices(kDisplayFftWindows), kDisplayFftWindowDefault),
        hzPerBin,
        displaySlider(displayControl(kDisplaySpectrumFpsKey, "station", "fps", spectrum,
                                     rendering, kDisplaySpectrumFpsLabel, "slider"),
                      kDisplaySpectrumFpsMin, kDisplaySpectrumFpsMax, kDisplaySpectrumFpsStep,
                      kDisplaySpectrumFpsUnit, 0, kDisplaySpectrumFpsDefault),
        displayChoice(displayControl(kDisplaySpectrumDetectorKey, "device", "trace.detector",
                                     spectrum, rendering, kDisplaySpectrumDetectorLabel,
                                     "choice"),
                      displayChoices(kDisplaySpectrumDetectors), kDisplaySpectrumDetectorDefault),
        displayChoice(displayControl(kDisplaySpectrumAveragingKey, "device",
                                     "trace.averageMode", spectrum, rendering,
                                     kDisplaySpectrumAveragingLabel, "choice"),
                      displayChoices(kDisplayAveragingModes), kDisplaySpectrumAveragingDefault),
        displaySlider(displayControl(kDisplaySpectrumAvgTimeKey, "device", "averageTimeMs",
                                     spectrum, rendering, kDisplaySpectrumAvgTimeLabel,
                                     "slider"),
                      kDisplayAvgTimeMinMs, kDisplayAvgTimeMaxMs, kDisplayAvgTimeStepMs, "ms", 0,
                      kDisplaySpectrumAvgTimeDefaultMs),
        displaySlider(displayControl(nullptr, "device", "decimation", spectrum, rendering,
                                     kDisplayDecimationLabel, "slider"),
                      kDisplayDecimationMin, kDisplayDecimationMax, kDisplayDecimationStep, "", 0,
                      kDisplayDecimationDefault),
        displayChoice(displayControl(kDisplayWaterfallDetectorKey, "device",
                                     "waterfall.detector", waterfall, wfGroup,
                                     kDisplayWaterfallDetectorLabel, "choice"),
                      displayChoices(kDisplayWaterfallDetectors),
                      kDisplayWaterfallDetectorDefault),
        displayChoice(displayControl(kDisplayWaterfallAveragingKey, "device",
                                     "waterfall.averageMode", waterfall, wfGroup,
                                     kDisplayWaterfallAveragingLabel, "choice"),
                      displayChoices(kDisplayAveragingModes), kDisplayWaterfallAveragingDefault),
        displaySlider(displayControl(kDisplayWaterfallAvgTimeKey, "device",
                                     "waterfallAverageTimeMs", waterfall, wfGroup,
                                     kDisplayWaterfallAvgTimeLabel, "slider"),
                      kDisplayAvgTimeMinMs, kDisplayAvgTimeMaxMs, kDisplayAvgTimeStepMs, "ms", 0,
                      kDisplayWaterfallAvgTimeDefaultMs),
    };

    // The readout beside the FFT size, and the sizes a pan asks for
    // (RemoteMediaController's plannedFftSize): see the link document's
    // section 7.4 for the rule.
    return QJsonObject{
        {QStringLiteral("controls"), controls},
        {QStringLiteral("binWidth"),
         QJsonObject{{QStringLiteral("label"), QString::fromLatin1(kDisplayBinWidthLabel)},
                     {QStringLiteral("decimals"), kDisplayBinWidthDecimals}}},
        {QStringLiteral("fftPlan"),
         QJsonObject{{QStringLiteral("minFftSize"), kDisplayFftPlanMinSize},
                     {QStringLiteral("maxFftSize"), kDisplayFftPlanMaxSize}}},
    };
}

QJsonObject metersObject(const StationCatalog::Inputs& inputs)
{
    using namespace ControlRanges;

    QJsonArray sUnits;
    for (int s = 0; s <= 9; ++s) {
        sUnits.append(QJsonObject{
            {QStringLiteral("label"), QStringLiteral("S%1").arg(s)},
            {QStringLiteral("dbm"), double(kSMeterS0Dbm + float(s) * kSMeterDbPerSUnit)}});
    }
    QJsonArray overS9;
    const int overMax = static_cast<int>(kSMeterMaxDbm - kSMeterS9Dbm);
    for (int over = kSMeterOverS9StepDb; over <= overMax; over += kSMeterOverS9StepDb) {
        overS9.append(QJsonObject{{QStringLiteral("label"), QStringLiteral("+%1").arg(over)},
                                  {QStringLiteral("dbm"), double(kSMeterS9Dbm) + over}});
    }
    const QJsonObject sMeter{
        {QStringLiteral("minDbm"), double(kSMeterS0Dbm)},
        {QStringLiteral("s9Dbm"), double(kSMeterS9Dbm)},
        {QStringLiteral("maxDbm"), double(kSMeterMaxDbm)},
        {QStringLiteral("dbPerSUnit"), double(kSMeterDbPerSUnit)},
        {QStringLiteral("redFromDbm"), double(kSMeterS9Dbm)},
        {QStringLiteral("sUnits"), sUnits},
        {QStringLiteral("overS9"), overS9},
    };
    const QJsonObject micLevel{
        {QStringLiteral("minDb"), kMicLevelMinDb},
        {QStringLiteral("maxDb"), kMicLevelMaxDb},
        {QStringLiteral("yellowFromDb"), kMicLevelYellowFromDb},
        {QStringLiteral("redFromDb"), kMicLevelRedFromDb},
    };
    // As the TX applet scales its gauge (rescaleFwdGaugeForModel): red from
    // the PA rating, full scale past it by the headroom.
    const double ratedW = paMaxWattsFor(inputs.model);
    const QJsonObject rfPower{
        {QStringLiteral("minW"), 0.0},
        {QStringLiteral("maxW"), ratedW * kRfPowerGaugeHeadroom},
        {QStringLiteral("ratedW"), ratedW},
        {QStringLiteral("redFromW"), ratedW},
    };
    const QJsonObject swr{
        {QStringLiteral("min"), kSwrGaugeMin},
        {QStringLiteral("max"), kSwrGaugeMax},
        {QStringLiteral("redFrom"), kSwrGaugeRedFrom},
    };
    return QJsonObject{{QStringLiteral("sMeter"), sMeter},
                       {QStringLiteral("micLevel"), micLevel},
                       {QStringLiteral("rfPower"), rfPower},
                       {QStringLiteral("swr"), swr}};
}

// ── noiseReduction ──────────────────────────────────────────────────────
//
// The VFO flag's noise-reduction quick controls, slot by slot in the
// flag's order, from the table the popups, NnrControls and SliceModel's
// defaults read (ControlRanges.h). The same on every radio.

QJsonObject noiseReductionControl(const ControlRanges::NrControl& control)
{
    using ControlRanges::NrControlKind;
    QJsonObject entry{
        {QStringLiteral("property"), QString::fromLatin1(control.property)},
        {QStringLiteral("label"), QString::fromUtf8(control.label)},
    };
    switch (control.kind) {
    case NrControlKind::Slider:
        entry.insert(QStringLiteral("kind"), QStringLiteral("slider"));
        entry.insert(QStringLiteral("min"), control.min);
        entry.insert(QStringLiteral("max"), control.max);
        entry.insert(QStringLiteral("step"), control.step);
        entry.insert(QStringLiteral("scale"), control.scale);
        entry.insert(QStringLiteral("divide"), control.divide);
        entry.insert(QStringLiteral("decimals"), control.decimals);
        entry.insert(QStringLiteral("suffix"), QString::fromUtf8(control.suffix));
        entry.insert(QStringLiteral("default"), control.defaultValue);
        entry.insert(QStringLiteral("reset"),
                     control.hasReset ? QJsonValue(control.reset) : QJsonValue(QJsonValue::Null));
        break;
    case NrControlKind::Switch:
        entry.insert(QStringLiteral("kind"), QStringLiteral("switch"));
        entry.insert(QStringLiteral("default"), control.defaultValue != 0.0);
        break;
    case NrControlKind::Choice: {
        entry.insert(QStringLiteral("kind"), QStringLiteral("choice"));
        QJsonArray options;
        for (std::size_t i = 0; i < control.optionCount; ++i) {
            options.append(QJsonObject{
                {QStringLiteral("id"), control.options[i].id},
                {QStringLiteral("label"), QString::fromUtf8(control.options[i].label)}});
        }
        entry.insert(QStringLiteral("options"), options);
        entry.insert(QStringLiteral("default"), static_cast<int>(control.defaultValue));
        entry.insert(QStringLiteral("reset"),
                     control.hasReset ? QJsonValue(static_cast<int>(control.reset))
                                      : QJsonValue(QJsonValue::Null));
        break;
    }
    }
    return entry;
}

QJsonObject noiseReductionObject()
{
    QJsonObject sections;
    for (const ControlRanges::NrSlotControls& slot : ControlRanges::kNoiseReductionSlots) {
        QJsonArray controls;
        for (std::size_t i = 0; i < slot.count; ++i) {
            controls.append(noiseReductionControl(slot.controls[i]));
        }
        sections.insert(QString::fromLatin1(slot.key), controls);
    }
    return sections;
}

// ── board.transmit ──────────────────────────────────────────────────────
//
// The transmit controls' ranges on this board, each in its property's
// units, as the TX applet, the Phone/CW applet and Setup > Transmit >
// Power range them (HpsdrModel.h, caps.micGainMinDb/MaxDb). The power
// controls add `shown`: what the control shows at its two ends, linear in
// between, to `decimals` places in `unit`, after `endSnap` and `rounding`
// take a value between steps to a step as mi0bot's HL2 readouts do.

// `rounding` and `endSnap` say how the control takes a value between its
// steps before showing it (HpsdrModel.h's PowerShownRule).
QJsonObject shownObject(double min, double max, int decimals, const QString& unit,
                        const PowerShownRule& rule)
{
    return QJsonObject{
        {QStringLiteral("min"), min},
        {QStringLiteral("max"), max},
        {QStringLiteral("decimals"), decimals},
        {QStringLiteral("unit"), unit},
        {QStringLiteral("rounding"), QString::fromLatin1(rule.rounding)},
        {QStringLiteral("endSnap"),
         rule.hasEndSnap ? QJsonValue(QJsonObject{{QStringLiteral("below"), rule.below},
                                                  {QStringLiteral("above"), rule.above}})
                         : QJsonValue(QJsonValue::Null)},
    };
}

QJsonObject transmitObject(const StationCatalog::Inputs& inputs)
{
    const HPSDRModel m = inputs.model;
    const int decimals = powerSliderShownDecimalsFor(m);
    const QString sliderUnit = QString::fromLatin1(powerSliderShownUnitFor(m));

    QJsonObject power = rangeObject(0, rfPowerSliderMaxFor(m), rfPowerSliderStepFor(m));
    power.insert(QStringLiteral("shown"),
                 shownObject(rfPowerShownFor(m, 0), rfPowerShownFor(m, rfPowerSliderMaxFor(m)),
                             decimals, sliderUnit, rfPowerShownRuleFor(m)));

    QJsonObject tune = rangeObject(0, tuneSliderMaxFor(m), tuneSliderStepFor(m));
    tune.insert(QStringLiteral("shown"),
                shownObject(tuneSliderShownFor(m, 0), tuneSliderShownFor(m, tuneSliderMaxFor(m)),
                            decimals, sliderUnit, tuneSliderShownRuleFor(m)));

    // Setup's fixed tune spinbox: its shown range and step, and the stored
    // values they write.
    const double shownMin = fixedTuneSpinboxMinFor(m);
    const double shownMax = fixedTuneSpinboxMaxFor(m);
    const int storedMin = tunePowerStoredFromShown(m, shownMin);
    const int storedMax = tunePowerStoredFromShown(m, shownMax);
    const int storedStep =
        tunePowerStoredFromShown(m, shownMin + fixedTuneSpinboxStepFor(m)) - storedMin;
    QJsonObject fixedTune = rangeObject(storedMin, storedMax, storedStep);
    fixedTune.insert(QStringLiteral("shown"),
                     shownObject(shownMin, shownMax, fixedTuneSpinboxDecimalsFor(m),
                                 QString::fromLatin1(fixedTuneSpinboxSuffixFor(m)).trimmed(),
                                 tunePowerShownRuleFor(m)));

    return QJsonObject{
        {QStringLiteral("power"), power},
        {QStringLiteral("tunePowerForTxBand"), tune},
        {QStringLiteral("tunePower"), fixedTune},
        {QStringLiteral("micGainDb"),
         rangeObject(inputs.board.micGainMinDb, inputs.board.micGainMaxDb, 1)},
    };
}

// The antenna relays this radio has, as the VFO flag and Setup > Antenna
// Control show them (hidden where absent): RX out on TX on the BYPS gate,
// the Ext-on-TX switches by their labels (null where absent), and the RX
// out override.
QJsonObject relaysObject(const StationCatalog::Inputs& inputs, const SkuUiProfile& sku)
{
    const auto labelOrNull = [](bool present, const QString& label) {
        return present ? QJsonValue(label) : QJsonValue(QJsonValue::Null);
    };
    return QJsonObject{
        {QStringLiteral("rxOutOnTx"), rxOutOnTxPresent(inputs.board, sku)},
        {QStringLiteral("ext1OutOnTx"), labelOrNull(sku.hasExt1OutOnTx, sku.ext1OutOnTxLabel)},
        {QStringLiteral("ext2OutOnTx"), labelOrNull(sku.hasExt2OutOnTx, sku.ext2OutOnTxLabel)},
        {QStringLiteral("rxOutOverride"), sku.hasRxBypassUi},
    };
}

// Level Cal 2 (JJ's ruling of 2026-09-30): RX2's own input control, every
// step the hardware has. On the two-ADC radios that is the second ADC's
// 0-31 dB step attenuator in 1 dB steps, which the Core already sets
// through stepAtt's rx2AttenuationDb with rx2StepAttEnabled on.
//   ANAN-100D (Angelia): TAPR-OpenHPSDR-Firmware @e7c6584, "Protocol 1/
//     ANAN-100D/Metis_Angelia_v6.0.qar" Angelia.v:2318-2321 (C0 0001_011x,
//     C1[4:0] input attenuator 2 for ADC2, C1[5] its enable), 2442, 2447.
//   ANAN-200D (Orion): same repo, "Protocol 1/ANAN-200D/
//     Metis_Orion_v5.2.qar" Orion.v:2295 ("0-31 dB"), 2417-2421, 2564, 2571.
//   OrionMKII family: n1gp-Anvelina_PROIII @8e86a61 High_Priority_CC.v:69-70,
//     272, 276 (P2 bytes 1442/1443) and Orion.v:738-744 (second attenuator).
//   ANAN-G2 and G2 1K (Saturn): P2 byte 1442, the same protocol field.
constexpr int kRx2AttenuatorMinDb = 0;
constexpr int kRx2AttenuatorMaxDb = 31;
constexpr int kRx2AttenuatorStepDb = 1;

enum class Rx2Input { StepAttenuator, MercuryPreamp, SharesRx1, Unknown };

Rx2Input rx2InputFor(HPSDRModel model)
{
    switch (model) {
    case HPSDRModel::ANAN100D:
    case HPSDRModel::ANAN200D:
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
        return Rx2Input::StepAttenuator;
    case HPSDRModel::HPSDR:
        // A second Mercury board's front end: C0 0001_010x C1 bit 1 is
        // Mercury 2's preamp (TAPR-OpenHPSDR-Firmware @e7c6584, "Protocol 1/
        // Mercury/Source/Mercury_V3.4/Mercury.v":765, 803-807), two states,
        // no attenuator (preamp ON) or 20 dB inserted (OFF) (Mercury.v:192,
        // 200). The Core sends it as RX2's preamp mode.
        return Rx2Input::MercuryPreamp;
    case HPSDRModel::REDPITAYA:
        // No Red Pitaya gateware in the local sources: nothing says what
        // its second input has, so no control is offered.
        return Rx2Input::Unknown;
    default:
        // One ADC shared with RX1: Hermes and HermesII (Thetis
        // clsHardwareSpecific.cs:87-116 SetRxADC(1) [v2.10.3.15]), the G2E
        // (clsHardwareSpecific.cs:129-130; its gateware has one ADC input,
        // TAPR-OpenHPSDR-Firmware "Protocol 1/ANAN-G2E/
        // Hermes_3.3_C10_P1_Mk2PA.qar" Hermes.v:236), the HL2 (mi0bot
        // clsHardwareSpecific.cs:94-95, console.cs:14846-14849 [@c26a8a4]).
        return Rx2Input::SharesRx1;
    }
}

int boardMaxSlices(const BoardCapabilities& caps)
{
    return caps.maxSlices > 0 ? caps.maxSlices : 1;
}

QJsonObject boardObject(const StationCatalog::Inputs& inputs)
{
    const BoardCapabilities& caps = inputs.board;

    // The step attenuator as the RX applet ranges its S-ATT box
    // (rebuildPreampAndAttRangeForBoard): the board's floor, and
    // stepAttMaxDb for its ceiling.
    QJsonValue attenuator = QJsonValue::Null;
    if (caps.attenuator.present) {
        attenuator = rangeObject(caps.attenuator.minDb,
                                 BoardCapsTable::stepAttMaxDb(caps.board, caps.hasAlexFilters),
                                 caps.attenuator.stepDb);
    }

    // The preamp items the RX applet's combo lists for this board.
    QJsonArray preampItems;
    for (const auto& item : BoardCapsTable::preampItemsForBoard(caps.board, caps.hasAlexFilters)) {
        preampItems.append(QJsonObject{{QStringLiteral("id"), item.modeInt},
                                       {QStringLiteral("label"), QString::fromLatin1(item.label)}});
    }

    // RX2's own input control: a slider on RX2's step attenuator, the
    // Mercury preamp's two states (the items stepAtt's rx2PreampMode takes,
    // BoardCapsTable::rx2PreampItemsForBoard), or neither with the reason.
    QJsonValue rx2Attenuator = QJsonValue::Null;
    QJsonArray rx2PreampItems;
    QJsonValue rx2AttenuatorReason = QJsonValue::Null;
    switch (rx2InputFor(inputs.model)) {
    case Rx2Input::StepAttenuator:
        rx2Attenuator = rangeObject(kRx2AttenuatorMinDb, kRx2AttenuatorMaxDb,
                                    kRx2AttenuatorStepDb);
        break;
    case Rx2Input::MercuryPreamp:
        for (const auto& item : BoardCapsTable::rx2PreampItemsForBoard(HPSDRHW::Atlas)) {
            rx2PreampItems.append(QJsonObject{{QStringLiteral("id"), item.modeInt},
                                              {QStringLiteral("label"),
                                               QString::fromLatin1(item.label)}});
        }
        break;
    case Rx2Input::SharesRx1:
        rx2AttenuatorReason =
            QStringLiteral("RX2 uses RX1's input on this radio. Set it with RX1's attenuator.");
        break;
    case Rx2Input::Unknown:
        rx2AttenuatorReason =
            QStringLiteral("NereusSDR cannot set RX2's input on this radio.");
        break;
    }

    // The main antenna ports, ANT1 upwards (AntennaLabels' names), which
    // both receive and transmit, and the product's receive-only inputs
    // (SkuUiProfile's labels, as many as the board has).
    QJsonArray mainAntennas;
    for (int i = 1; i <= caps.antennaInputCount; ++i) {
        mainAntennas.append(QStringLiteral("ANT%1").arg(i));
    }
    QJsonArray rxOnlyInputs;
    const SkuUiProfile sku = skuUiProfileFor(inputs.model);
    const int rxOnlyCount = std::min<int>(caps.rxOnlyAntennaCount,
                                          static_cast<int>(sku.rxOnlyLabels.size()));
    for (int i = 0; i < rxOnlyCount; ++i) {
        rxOnlyInputs.append(sku.rxOnlyLabels.at(static_cast<std::size_t>(i)));
    }

    // The rates the radio offers on the protocol it runs, as Setup >
    // Hardware > Radio Info lists them.
    QJsonArray sampleRates;
    for (int rate : allowedSampleRates(inputs.protocol, caps, inputs.model)) {
        sampleRates.append(rate);
    }

    return QJsonObject{
        {QStringLiteral("model"), static_cast<int>(inputs.model)},
        {QStringLiteral("productLabel"), QString::fromLatin1(displayName(inputs.model))},
        {QStringLiteral("maxSlices"), boardMaxSlices(caps)},
        {QStringLiteral("attenuator"), attenuator},
        {QStringLiteral("preampItems"), preampItems},
        {QStringLiteral("rxAntennas"), mainAntennas},
        {QStringLiteral("rxOnlyInputs"), rxOnlyInputs},
        {QStringLiteral("txAntennas"), mainAntennas},
        {QStringLiteral("sampleRates"), sampleRates},
        {QStringLiteral("pureSignal"), caps.hasPureSignal},
        {QStringLiteral("paRatingW"), paMaxWattsFor(inputs.model)},
        {QStringLiteral("micJack"), caps.hasMicJack},
        {QStringLiteral("transmit"), transmitObject(inputs)},
        // The RX applet's RX1 preamp toggle: dual-ADC boards alone.
        {QStringLiteral("rx1Preamp"), caps.p2PreampPerAdc},
        {QStringLiteral("relays"), relaysObject(inputs, sku)},
        // Level Cal 2 (rx2AttenuatorVersion 1): RX2's own input control.
        {QStringLiteral("rx2Attenuator"), rx2Attenuator},
        {QStringLiteral("rx2PreampItems"), rx2PreampItems},
        {QStringLiteral("rx2AttenuatorReason"), rx2AttenuatorReason},
        // Radio codec lane (radioMicVersion 1): whether the TX mic source
        // can be the radio's own mic, and the note beside it (the Hermes
        // Lite 2's audio add-on board, which its gateware cannot report).
        {QStringLiteral("radioMic"), caps.radioMicSelectable()},
        {QStringLiteral("radioMicNote"), caps.radioMicNeedsAddOn
             ? QJsonValue(RadioModel::radioMicAddOnNote()) : QJsonValue(QJsonValue::Null)},
    };
}

QJsonArray bandPlansArray(const StationCatalog::Inputs& inputs)
{
    QJsonArray plans;
    for (const StationCatalog::BandPlan& plan : inputs.bandPlans) {
        QJsonArray segments;
        for (const BandSegment& segment : plan.segments) {
            segments.append(QJsonObject{
                {QStringLiteral("lowHz"), static_cast<qint64>(std::llround(segment.lowMhz * 1.0e6))},
                {QStringLiteral("highHz"), static_cast<qint64>(std::llround(segment.highMhz * 1.0e6))},
                {QStringLiteral("label"), segment.label},
                {QStringLiteral("licence"), segment.license},
                {QStringLiteral("lowestClass"), lowestLicenceClass(segment.license)},
                {QStringLiteral("colour"), colourText(segment.color)},
            });
        }
        // R-IOS-11 (D79): the plan's spots, as its file lists them, and
        // whether it is the Core's own plan.
        QJsonArray spots;
        for (const BandSpot& spot : plan.spots) {
            spots.append(QJsonObject{
                {QStringLiteral("hz"), static_cast<qint64>(std::llround(spot.freqMhz * 1.0e6))},
                {QStringLiteral("label"), spot.label},
            });
        }
        plans.append(QJsonObject{{QStringLiteral("id"), plan.id},
                                 {QStringLiteral("name"), plan.name},
                                 {QStringLiteral("default"), plan.name == inputs.defaultBandPlanName},
                                 {QStringLiteral("active"), plan.name == inputs.activeBandPlanName},
                                 {QStringLiteral("segments"), segments},
                                 {QStringLiteral("spots"), spots}});
    }
    return plans;
}

QJsonArray palettesArray()
{
    QJsonArray palettes;
    for (int id = 0; id < static_cast<int>(WfColorScheme::Count); ++id) {
        const auto scheme = static_cast<WfColorScheme>(id);
        // Custom is each computer's own gradient (DisplayWfCustomStops, a
        // setting that stays on that computer), so the Core has none to send.
        if (scheme == WfColorScheme::Custom) {
            continue;
        }
        int count = 0;
        const WfGradientStop* stops = wfSchemeStops(scheme, count);
        QJsonArray stopArray;
        for (int i = 0; i < count; ++i) {
            const WfGradientStop& stop = stops[i];
            // Positions to three places: the tables are written that way,
            // and a float's binary tail is not part of the value.
            const double at = std::round(double(stop.pos) * 1000.0) / 1000.0;
            stopArray.append(QJsonObject{
                {QStringLiteral("at"), at},
                {QStringLiteral("colour"), colourText(QColor(stop.r, stop.g, stop.b))}});
        }
        palettes.append(QJsonObject{{QStringLiteral("id"), id},
                                    {QStringLiteral("name"), QString::fromLatin1(wfSchemeName(scheme))},
                                    {QStringLiteral("stops"), stopArray}});
    }
    return palettes;
}

// iPhone app plan Task 23 (R-IOS-09): `audio` {opusProfiles: [{bitrate,
// bandwidthHz}]}, the station's measured table in its order.
QJsonObject audioObject(const StationCatalog::Inputs& inputs)
{
    QJsonArray profiles;
    for (const OpusMeasuredProfile& profile : inputs.opusProfiles) {
        profiles.append(QJsonObject{{QStringLiteral("bitrate"), profile.bitrate},
                                    {QStringLiteral("bandwidthHz"), profile.bandwidthHz}});
    }
    return QJsonObject{{QStringLiteral("opusProfiles"), profiles}};
}

QJsonArray sliceColoursArray(const StationCatalog::Inputs& inputs)
{
    QJsonArray colours;
    for (int i = 0; i < boardMaxSlices(inputs.board); ++i) {
        colours.append(colourText(ControlRanges::sliceColour(i)));
    }
    return colours;
}

// The desktop's Tools menu, in its order, without MIDI Mapping and Macro
// Buttons (D42). `where` is where the tool works: at the Core, or on both
// the Core and the device. iPhone app plan Task 25 (D41): `offered` starts
// from the unbuilt features list the desktop hides by (CWX, the Memory
// Manager and CAT Control stay unoffered until built), then follows the
// radio and the Core: PureSignal when the radio has it, Diversity when it
// has a diversity receiver, TCI Server when the Core runs its own station
// TCI server, VAX Audio when the station computer publishes VAX devices.
// The rest are always offered.
enum class Offer {
    Always,
    PureSignal,
    Diversity,
    StationTci,
    Vax,
    AntennaControl,
    Cwx,
    Memories,
    Cat,
    Transverters,
};

struct ToolEntry {
    const char* id;
    const char* label;
    const char* where;
    Offer offer;
};
constexpr ToolEntry kTools[] = {
    {"spotHub", "Spot Hub", "both", Offer::Always},
    {"freedvReporter", "FreeDV Reporter", "station", Offer::Always},
    {"txEqualizer", "TX Equalizer", "station", Offer::Always},
    {"pureSignal", "PureSignal", "station", Offer::PureSignal},
    {"diversity", "Diversity", "station", Offer::Diversity},
    {"cwx", "CWX", "station", Offer::Cwx},
    {"memoryManager", "Memory Manager", "station", Offer::Memories},
    {"catControl", "CAT Control", "station", Offer::Cat},
    {"tciServer", "TCI Server", "station", Offer::StationTci},
    {"vaxAudio", "VAX Audio", "station", Offer::Vax},
    {"networkDiagnostics", "Network Diagnostics", "both", Offer::Always},
    {"supportBundle", "Support Bundle", "both", Offer::Always},
};

// The desktop's Radio menu items an app lists, in its order. Antenna Setup
// follows the desktop's own rule (antenna controls shown only with Alex and
// at least three antenna inputs); Transverters waits for the unbuilt
// features list (UnbuiltFeature::Transverters).
struct RadioItemEntry {
    const char* id;
    const char* label;
    Offer offer;
};
constexpr RadioItemEntry kRadioItems[] = {
    {"manageRadios", "Manage Radios", Offer::Always},
    {"antennaSetup", "Antenna Setup", Offer::AntennaControl},
    {"transverters", "Transverters", Offer::Transverters},
    {"protocolInfo", "Protocol Info", Offer::Always},
};

bool offered(Offer offer, const StationCatalog::Inputs& inputs)
{
    switch (offer) {
    case Offer::Always:
        return true;
    case Offer::PureSignal:
        return inputs.board.hasPureSignal;
    case Offer::Diversity:
        return inputs.board.hasDiversityReceiver;
    case Offer::StationTci:
        return inputs.stationTciServer;
    case Offer::Vax:
        return inputs.vaxDevices;
    case Offer::AntennaControl:
        return inputs.board.hasAlex && inputs.board.antennaInputCount >= 3;
    case Offer::Cwx:
        return UnbuiltFeatures::isBuilt(UnbuiltFeature::Cwx);
    case Offer::Memories:
        return UnbuiltFeatures::isBuilt(UnbuiltFeature::Memories);
    case Offer::Cat:
        return UnbuiltFeatures::isBuilt(UnbuiltFeature::Cat);
    case Offer::Transverters:
        return UnbuiltFeatures::isBuilt(UnbuiltFeature::Transverters);
    }
    return false;
}

QJsonArray toolsArray(const StationCatalog::Inputs& inputs)
{
    QJsonArray tools;
    for (const ToolEntry& tool : kTools) {
        tools.append(QJsonObject{{QStringLiteral("id"), QString::fromLatin1(tool.id)},
                                 {QStringLiteral("label"), QString::fromLatin1(tool.label)},
                                 {QStringLiteral("where"), QString::fromLatin1(tool.where)},
                                 {QStringLiteral("offered"), offered(tool.offer, inputs)}});
    }
    return tools;
}

QJsonArray radioItemsArray(const StationCatalog::Inputs& inputs)
{
    QJsonArray items;
    for (const RadioItemEntry& item : kRadioItems) {
        items.append(QJsonObject{{QStringLiteral("id"), QString::fromLatin1(item.id)},
                                 {QStringLiteral("label"), QString::fromLatin1(item.label)},
                                 {QStringLiteral("offered"), offered(item.offer, inputs)}});
    }
    return items;
}

// The desktop's per-pan BAND grid, in its order: each button's Band (the
// value slice.selectBand takes) and its text.
QJsonArray bandsArray()
{
    QJsonArray bands;
    for (const BandGridEntry& entry : kBandGrid) {
        bands.append(QJsonObject{{QStringLiteral("id"), static_cast<int>(entry.band)},
                                 {QStringLiteral("label"), QString::fromLatin1(entry.label)}});
    }
    return bands;
}

// SliceModel's tune-step list; the catalogue follows the STEP controls.
QList<int> sliceTuneSteps()
{
    QList<int> steps;
    for (int i = 0; i < kTuneStepListSize; ++i) {
        steps.append(kTuneStepList[i].stepHz);
    }
    return steps;
}

} // namespace

StationCatalog::StationCatalog(QObject* parent)
    : QObject(parent)
    , m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, &StationCatalog::refresh);
}

StationCatalog::~StationCatalog() = default;

QJsonObject StationCatalog::build(const Inputs& inputs)
{
    return QJsonObject{
        {QStringLiteral("modes"), modesArray()},
        {QStringLiteral("filterPresets"), filterPresetsObject(inputs)},
        {QStringLiteral("tuneSteps"), tuneStepsArray(inputs)},
        {QStringLiteral("agc"), agcObject()},
        {QStringLiteral("receive"), receiveObject()},
        {QStringLiteral("meters"), metersObject(inputs)},
        {QStringLiteral("display"), displayObject()},
        {QStringLiteral("noiseReduction"), noiseReductionObject()},
        {QStringLiteral("board"), boardObject(inputs)},
        {QStringLiteral("bandPlans"), bandPlansArray(inputs)},
        {QStringLiteral("bands"), bandsArray()},
        {QStringLiteral("palettes"), palettesArray()},
        {QStringLiteral("sliceColours"), sliceColoursArray(inputs)},
        {QStringLiteral("tools"), toolsArray(inputs)},
        {QStringLiteral("radioItems"), radioItemsArray(inputs)},
        // iPhone app plan Task 23 (R-IOS-09): the Opus profiles a device
        // may ask for (the audio control's `opusBitrate`).
        {QStringLiteral("audio"), audioObject(inputs)},
    };
}

StationCatalog::Inputs StationCatalog::inputsFrom(const RadioModel& model)
{
    Inputs inputs;
    const HardwareProfile& profile = model.hardwareProfile();
    inputs.model = profile.caps != nullptr ? profile.model : HPSDRModel::FIRST;
    inputs.board = model.boardCapabilities();
    // The protocol the Core's radio runs; the board's own before a radio
    // has been seen.
    const RadioInfo& radio = model.currentRadioInfo();
    inputs.protocol = radio.macAddress.isEmpty() ? inputs.board.protocol : radio.protocol;

    const FilterPresetStore* store = model.filterPresetStore();
    for (int id = 0; id < kModeCount; ++id) {
        const auto mode = static_cast<DSPMode>(id);
        QList<FilterPreset> presets;
        if (store != nullptr) {
            presets = store->presetsForMode(mode);
        } else {
            const auto defaults = SliceModel::presetsForMode(mode);
            for (int slot = 0; slot < defaults.size(); ++slot) {
                presets.append(FilterPresetStore::defaultPreset(mode, slot));
            }
        }
        inputs.filterPresets.append({mode, presets});
    }

    inputs.tuneStepsHz = sliceTuneSteps();

    for (const BandPlanManager::PlanData& plan : model.bandPlanManager().plans()) {
        inputs.bandPlans.append(BandPlan{plan.id, plan.name, plan.segments, plan.spots});
    }
    inputs.defaultBandPlanName = QString::fromLatin1(BandPlanManager::kDefaultPlanName);
    inputs.activeBandPlanName = model.bandPlanManager().activePlanName();
    // iPhone app plan Task 25 (D41): the Core's own station TCI server, and
    // the VAX devices of a Core whose audio engine publishes them (a
    // headless Core's does not, AudioEngine::vaxOutputsAllowed).
    inputs.stationTciServer = model.stationTciController() != nullptr;
    // localAudioDevices() has no const form; it is only read here.
    const AudioEngine* audio = const_cast<RadioModel&>(model).localAudioDevices();
    inputs.vaxDevices = audio != nullptr && audio->vaxOutputsAllowed();
    return inputs;
}

void StationCatalog::bind(RadioModel* model)
{
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
        if (FilterPresetStore* store = m_model->filterPresetStore()) {
            disconnect(store, nullptr, this, nullptr);
        }
        disconnect(&m_model->bandPlanManagerMutable(), nullptr, this, nullptr);
        if (StationTciModel* tci = m_model->stationTciModel()) {
            disconnect(tci, nullptr, this, nullptr);
        }
    }
    m_model = model;
    if (model == nullptr) {
        return;
    }
    // A preset changed on this computer, the band plan data was read again
    // or the Core's plan changed (planChanged, R-IOS-11), or the radio (and with it the board) changed. A preset written from
    // another window reaches StationServer as a settings change, which
    // schedules a refresh the same way.
    if (FilterPresetStore* store = model->filterPresetStore()) {
        connect(store, &FilterPresetStore::presetsChanged, this,
                &StationCatalog::scheduleRefresh);
    }
    connect(&model->bandPlanManagerMutable(), &BandPlanManager::planChanged, this,
            &StationCatalog::scheduleRefresh);
    connect(model, &RadioModel::currentRadioChanged, this, &StationCatalog::scheduleRefresh);
    // iPhone app plan Task 25: the station TCI server can start after the
    // catalogue binds (RadioModel::enableStationTci publishes its state).
    if (StationTciModel* tci = model->stationTciModel()) {
        connect(tci, &StationTciModel::stateChanged, this, &StationCatalog::scheduleRefresh);
    }
    refresh();
}

void StationCatalog::setInputs(const Inputs& inputs)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(build(inputs)).toJson(QJsonDocument::Compact));
    if (json.toUtf8().size() > kMaxJsonBytes) {
        qCWarning(lcCatalog) << "The Core's catalog is" << json.toUtf8().size()
                             << "bytes, over its" << kMaxJsonBytes << "byte limit";
    }
    if (json == m_json) {
        return;
    }
    m_json = json;
    ++m_revision;
    emit catalogChanged();
}

void StationCatalog::refresh()
{
    m_refreshTimer->stop();
    if (!m_model) {
        return;
    }
    setInputs(inputsFrom(*m_model));
}

void StationCatalog::scheduleRefresh()
{
    if (!m_refreshTimer->isActive()) {
        m_refreshTimer->start();
    }
}

} // namespace NereusSDR
