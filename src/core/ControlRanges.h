#pragma once
// no-port-check: NereusSDR-original. The values below were moved here from
// the desktop widgets that draw them; each keeps the upstream cite it had
// there (AetherSDR, GPLv3 like NereusSDR; Thetis, GPLv2 or later).
// =================================================================
// src/core/ControlRanges.h  (NereusSDR)
// =================================================================
//
// The ranges and scales the operator's controls and gauges use, in one
// place the Core can read (iPhone app plan Task 19, R-IOS-06). The desktop
// widgets read them from here, and the Core's catalogue
// (src/core/session/StationCatalog) sends the same numbers to an app, so a
// phone draws the AGC-T slider, the S-meter and the transmit gauges with
// exactly the desktop's values.
//
// Only what a widget used to hold as a literal lives here. Values a board
// decides (the attenuator range, the preamp items, the PA rating) stay in
// BoardCapabilities and HpsdrModel.h, where the widgets already read them.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: Slice colours E to H from AetherSDR's theme seed, so a
//               fifth slice no longer repeats slice A's colour. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: Match NR2/NR4 controls and defaults to Thetis v2.10.3.15.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//               (R-IOS-06, R-IOS-27).
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code. Moved from VfoWidget, RxApplet, SMeterWidget,
//               PhoneCwApplet and TxApplet.
//   2026-09-24: AGC-T range re-cited against Thetis v2.10.3.15 and its top
//               raised from 0 to +2 dB to match Thetis's clamp. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: The receive ranges (AF gain, SSQL, AM and FM squelch)
//               moved here from VfoWidget, RxApplet, SliceModel and the
//               DSP setup pages for the catalogue's `receive` key. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: The Setup > Display controls (FFT size, window, Hz/bin
//               target, FPS, the spectrum and waterfall detector,
//               averaging and averaging time, decimation) moved here from
//               DisplaySetupPages for the catalogue's `display` key, their
//               Thetis cites restamped against v2.10.3.15 (R-IOS-18,
//               R-IOS-27, R-IOS-06, R-R3-08). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: The noise-reduction controls start here with NR1, its
//               ranges and defaults corrected to Thetis's NR spinboxes and
//               their SetRXAANRVals conversion (R-IOS-06, R-IOS-27). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: MNR's quick controls, their Reset made to restore
//               MacNRFilter's own DEF_* values (Aggressiveness 4, Bias
//               1.2) as a new slice does. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: NR2, NR3, NR4, DFNR and NNR's controls moved here from
//               VfoWidget, NnrControls, NnrSettings and SliceModel (values
//               unchanged; Thetis cites restamped against v2.10.3.15, and
//               NereusSDR's own values marked where they differ), with the
//               slot table the catalogue's `noiseReduction` key reads
//               (R-IOS-06, R-IOS-27). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-10: planAutoZoomFft and largestPlannedFftSize, the one FFT
//               sizing rule a local and a remote pan share (R-DSP-30,
//               R-DSP-31, R-DSP-59). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-11: The FFT sizing mode (Auto, Hz/bin, Fixed): its key and
//               tokens, the rule that reads it, and planDisplayFft, the
//               one planner every reader uses (R-DSP-62, R-DSP-63).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace NereusSDR::ControlRanges {

// ── AGC ───────────────────────────────────────────────────────────────────

/// One AGC mode the operator can pick: `id` is its AGCMode value.
struct AgcModeItem {
    int id;
    const char* label;
};

// The five modes the VFO flag's AGC row and the RX applet's AGC combo offer,
// in their order (AGCMode::Off..Fast; Custom is not offered).
inline constexpr std::array<AgcModeItem, 5> kAgcModes{{
    {0, "Off"},
    {1, "Long"},
    {2, "Slow"},
    {3, "Med"},
    {4, "Fast"},
}};

// AGC-T, the VFO flag's and the RX applet's AGC threshold slider, in dB.
// Thetis clamps the threshold it hands WDSP to -160..+2 in setAGCThresholdPoint:
// From Thetis Project Files/Source/Console/console.cs:46048-46049 [v2.10.3.15]
//   // MW0LGE_21k9d values are already offset as part of Display
//   if (agc_thresh_point > 2) agc_thresh_point = 2;
//   if (agc_thresh_point < -160.0) agc_thresh_point = -160.0;
inline constexpr int kAgcThresholdMinDb = -160;
inline constexpr int kAgcThresholdMaxDb = 2;
inline constexpr int kAgcThresholdStepDb = 1;

// ── Receive ───────────────────────────────────────────────────────────────

// AF gain, the VFO flag's AF slider and SliceModel::setAfGain's clamp, in
// the slider's own units (0 to 100). Thetis's AF slider spans the same:
// From Thetis Project Files/Source/Console/console.Designer.cs:3729-3730 [v2.10.3.15]
//   this.ptbAF.Maximum = 100;
//   this.ptbAF.Minimum = 0;
inline constexpr int kAfGainMin = 0;
inline constexpr int kAfGainMax = 100;
inline constexpr int kAfGainStep = 1;

// SSB squelch (SSQL), the VFO flag's and the RX applet's SQL slider, in
// slider units (0 to 100). SliceModel's ssqlThresh holds the slider value;
// RadioModel divides it by 100 for WDSP's 0.0..1.0.
inline constexpr int kSsqlThreshMin = 0;
inline constexpr int kSsqlThreshMax = 100;
inline constexpr int kSsqlThreshStep = 1;

// AM squelch, Setup > DSP > AM/SAM's threshold slider, in dB (SliceModel
// amsqThresh). Thetis's squelch slider spans the same:
// From Thetis Project Files/Source/Console/console.Designer.cs:7572-7573 [v2.10.3.15]
//   this.ptbSquelch.Maximum = 0;
//   this.ptbSquelch.Minimum = -160;
inline constexpr int kAmsqThreshMinDb = -160;
inline constexpr int kAmsqThreshMaxDb = 0;
inline constexpr int kAmsqThreshStepDb = 1;

// FM squelch, Setup > DSP > FM's threshold slider (SliceModel fmsqThresh):
// the slider position, -160 to 0, which RxChannel::fmsqThresholdForWdsp
// maps to the WDSP threshold. The page uses the AM slider's range.
inline constexpr int kFmsqThreshMinDb = -160;
inline constexpr int kFmsqThreshMaxDb = 0;
inline constexpr int kFmsqThreshStepDb = 1;

// ── S-meter ───────────────────────────────────────────────────────────────

// S-unit reference: S0 = -127 dBm, each S-unit = 6 dB
// From AetherSDR src/gui/SMeterWidget.h:114-117 [@0cd4559]
inline constexpr float kSMeterS0Dbm = -127.0f;
inline constexpr float kSMeterS9Dbm = -73.0f;
inline constexpr float kSMeterMaxDbm = -13.0f;  // S9+60
inline constexpr float kSMeterDbPerSUnit = 6.0f;
// The analog S-meter's steps above S9: a tick every 10 dB up to S9+60
// (SMeterWidget's vintage faces; the classic face labels +20 and +40).
inline constexpr int kSMeterOverS9StepDb = 10;

// ── Transmit gauges ───────────────────────────────────────────────────────

// Mic level, the Phone/CW applet's gauge, in dB:
// HGauge(-40, +10, redStart=0, yellowStart=-10)
inline constexpr double kMicLevelMinDb = -40.0;
inline constexpr double kMicLevelMaxDb = 10.0;
inline constexpr double kMicLevelYellowFromDb = -10.0;
inline constexpr double kMicLevelRedFromDb = 0.0;

// SWR, the TX applet's gauge: 1.0 to 3.0, red from 2.5.
// Ticks: 1 / 1.5 / 2.5 / 3  (AetherSDR TxApplet.cpp:77)
inline constexpr double kSwrGaugeMin = 1.0;
inline constexpr double kSwrGaugeMax = 3.0;
inline constexpr double kSwrGaugeRedFrom = 2.5;

// RF power, the TX applet's gauge: red from the board's PA rating
// (paMaxWattsFor), full scale 20% past it.
inline constexpr double kRfPowerGaugeHeadroom = 1.2;

// ── Setup > Display ───────────────────────────────────────────────────────
//
// The Spectrum Defaults and Waterfall Defaults controls an app offers,
// moved here from src/gui/setup/DisplaySetupPages.cpp so the desktop page
// and the Core's catalogue (its `display` key) read one table. Each
// control's settings key, the desktop's label (without the colon), its
// range or items, and the default the desktop applies when the key is unset.

// The pages and groups the controls sit in, by their desktop titles.
inline constexpr const char* kDisplaySpectrumPageTitle = "Spectrum Defaults";
inline constexpr const char* kDisplayWaterfallPageTitle = "Waterfall Defaults";
inline constexpr const char* kDisplayFftGroupTitle = "Fast Fourier Transform";
inline constexpr const char* kDisplayRenderingGroupTitle = "Rendering";
inline constexpr const char* kDisplayWaterfallGroupTitle = "Display";

/// One item of a Setup > Display combo: `value` is what the control stores
/// (the combo index), `label` the desktop's text.
struct DisplayChoiceItem {
    int value;
    const char* label;
};

// FFT size: the Fast Fourier Transform group's "Size" slider. Its positions
// 0..6 give 4096 x 2^position points, 4096 to 262144:
// From Thetis Project Files/Source/Console/setup.designer.cs:35165 [v2.10.3.15]
//   this.tbDisplayFFTSize.Maximum = 6;
// From Thetis Project Files/Source/Console/setup.cs:16189 [v2.10.3.15]
//   FFTSize = (int)(4096 * Math.Pow(2, Math.Floor((double)(tbDisplayFFTSize.Value))));
inline constexpr const char* kDisplayFftSizeKey = "DisplayFftSize";
inline constexpr const char* kDisplayFftSizeLabel = "Size";
inline constexpr int kDisplayFftSizeBase = 4096;
inline constexpr int kDisplayFftSizePositionMax = 6;
// NereusSDR-native default: the size every reader of DisplayFftSize applies
// when it is unset (the FFT engine pool, a remote window's request, this
// page), slider position 0. Thetis's slider starts at position 5
// (setup.designer.cs:35169 [v2.10.3.15], `tbDisplayFFTSize.Value = 5`); the
// desktop's slider holds 5 only until its first load replaces it.
inline constexpr int kDisplayFftSizeDefault = 4096;
inline constexpr int kDisplayFftSizeSliderInitial = 5;

/// The FFT size at slider `position` (clamped to 0..6).
constexpr int displayFftSizeAt(int position) noexcept
{
    if (position < 0) { position = 0; }
    if (position > kDisplayFftSizePositionMax) { position = kDisplayFftSizePositionMax; }
    return kDisplayFftSizeBase << position;
}

/// The slider position for an FFT size: log2(size / 4096), 0..6. A size
/// that is not one of the seven takes the position below it.
constexpr int displayFftSizePosition(int fftSize) noexcept
{
    int position = 0;
    for (int n = fftSize / kDisplayFftSizeBase; n > 1 && position < kDisplayFftSizePositionMax;
         n >>= 1) {
        ++position;
    }
    return position;
}

/// Whether `fftSize` is one of the slider's seven sizes. A stored
/// DisplayFftSize that is not is replaced by the default when it is read
/// (MainWindow::refreshFftPoolConfig), so the key and the engines agree.
constexpr bool isDisplayFftSize(int fftSize) noexcept
{
    return displayFftSizeAt(displayFftSizePosition(fftSize)) == fftSize;
}

// The Bin Width (Hz) readout beside the slider: sample rate / FFT size, to
// three places:
// From Thetis Project Files/Source/Console/setup.cs:16192-16193 [v2.10.3.15]
//   double bin_width = (double)Display.SampleRateRX1 / (double)console.specRX.GetSpecRX(0).FFTSize;
//   lblDisplayBinWidth.Text = bin_width.ToString("N3");
inline constexpr const char* kDisplayBinWidthLabel = "Bin Width (Hz)";
inline constexpr int kDisplayBinWidthDecimals = 3;

// The FFT sizes a pan's plan rounds to (planAutoZoomFft below, and a remote
// window's stored size, src/gui/RemoteMediaController.cpp): the smallest power of two from 1024 up
// that reaches the wanted size, at most FFTEngine::maximumFftSize() (262144).
// NereusSDR-native.
inline constexpr int kDisplayFftPlanMinSize = 1024;
inline constexpr int kDisplayFftPlanMaxSize = 262144;

// How far a zoom (or the Hz/bin target) grows a pan's FFT, on a local pan
// (MainWindow's bandwidthChangeRequested auto-zoom) and a remote pan
// (RemoteMediaController's plannedFftSize) alike: a time per transform, the
// same at every sample rate. NereusSDR-native (JJ's ruling, 2026-10-09;
// supersedes the fixed 65536 cap of 2026-05-08).
//
// Auto-zoom keeps about `baseline` points on screen at every zoom
// (size = baseline * rate / span), so the time per transform at a given zoom
// (size / rate = baseline / span) does not depend on the rate. A fixed point
// count therefore cut in at a different zoom on every rate: 65536 is 0.68 s
// at 96 kHz but 43 ms at 1.536 MHz, where a deep zoom went blocky from a
// 96 kHz-wide view. A time limit gives the same deep-zoom look at every rate,
// with the engine maximum (kDisplayFftPlanMaxSize) on top. The longer buffer
// fill on a replan at deep zoom on high rates is accepted. Users who want
// larger FFTs pick them on the slider; auto-zoom won't push above the cap
// unless the slider already did.
//
//   48 kHz 32768, 96 kHz 65536, 192 kHz 131072 (0.68 s each),
//   384 kHz 262144 (0.68 s), 768 kHz 262144 (0.34 s), 1.536 MHz 262144 (0.17 s)
inline constexpr double kAutoZoomMaxTransformSeconds = 0.7;

// The largest FFT size a zoom grows to at `sampleRateHz`: the largest power
// of two <= sampleRateHz * kAutoZoomMaxTransformSeconds, clamped to
// [kDisplayFftPlanMinSize, kDisplayFftPlanMaxSize]. A rate that is not
// finite or not positive gives kDisplayFftPlanMinSize; both callers return
// early on such rates before asking.
constexpr int autoZoomMaxFftSize(double sampleRateHz) noexcept
{
    // !(x > 0) is also true for NaN; the upper test rejects infinity
    // (std::isfinite is not constexpr in C++20).
    if (!(sampleRateHz > 0.0)
        || !(sampleRateHz <= std::numeric_limits<double>::max())) {
        return kDisplayFftPlanMinSize;
    }
    const double limit = sampleRateHz * kAutoZoomMaxTransformSeconds;
    int size = kDisplayFftPlanMinSize;
    while (size < kDisplayFftPlanMaxSize && double(size) * 2.0 <= limit) {
        size *= 2;
    }
    return size;
}

// The FFT size a zoom asks for: `desiredPow2` (the power of two the zoom or
// Hz/bin target wants) floored at the user's `baseline` (the slider's choice
// is always honoured) and capped at autoZoomMaxFftSize(sampleRateHz). When
// the baseline is above the cap (the user picked a larger size), the
// baseline wins and a zoom does not grow past it.
constexpr int autoZoomFftSize(int desiredPow2, int baseline, double sampleRateHz) noexcept
{
    return std::min(std::max(desiredPow2, baseline),
                    std::max(baseline, autoZoomMaxFftSize(sampleRateHz)));
}

/// The FFT size one pan plans, and whether the Hz/bin target set it.
struct AutoZoomPlan {
    int size;
    bool hzPerBinInCharge;
};

// The one sizing rule, for a local pan and a remote pan alike
// (NereusSDR-native). The points wanted are `baseline` x rate / span, so a
// zoom keeps about `baseline` points across the pan; when the Hz/bin target
// is on (above 0) they are rate / target instead. That is rounded up to a
// power of two (kDisplayFftPlanMinSize to kDisplayFftPlanMaxSize) and passed
// through autoZoomFftSize: floored at `baseline`, capped at about 0.7 s per
// transform. The pan's width in pixels is not an input.
//
// `hzPerBinInCharge` is true when the target is on and the size it gave is
// above the baseline. A rate or span that is not finite or not positive
// gives {baseline, false}. The local pan's hysteresis is the caller's.
constexpr AutoZoomPlan planAutoZoomFft(int baseline, double sampleRateHz, double spanHz,
                                       double hzPerBinTarget) noexcept
{
    // !(x > 0) is also true for NaN; the upper test rejects infinity
    // (std::isfinite is not constexpr in C++20).
    constexpr double kLargest = std::numeric_limits<double>::max();
    if (!(sampleRateHz > 0.0) || !(sampleRateHz <= kLargest)
        || !(spanHz > 0.0) || !(spanHz <= kLargest)) {
        return {baseline, false};
    }
    const bool targetOn = hzPerBinTarget > 0.0;
    const double desired = targetOn ? sampleRateHz / hzPerBinTarget
                                    : double(baseline) * sampleRateHz / spanHz;
    int desiredPow2 = kDisplayFftPlanMinSize;
    while (desiredPow2 < kDisplayFftPlanMaxSize && double(desiredPow2) < desired) {
        desiredPow2 *= 2;
    }
    const int size = autoZoomFftSize(desiredPow2, baseline, sampleRateHz);
    return {size, targetOn && size > baseline};
}

/// The largest size among `plans` (pans that share a stream run one FFT),
/// `baseline` when there are none.
constexpr int largestPlannedFftSize(std::span<const AutoZoomPlan> plans, int baseline) noexcept
{
    if (plans.empty()) {
        return baseline;
    }
    int largest = plans.front().size;
    for (const AutoZoomPlan& plan : plans) {
        largest = std::max(largest, plan.size);
    }
    return largest;
}

// ── FFT sizing mode (design D38 to D43; R-DSP-62, R-DSP-63) ─────────────────
//
// NereusSDR, based on Thetis: Fixed is Thetis's own behavior, where the FFT
// Size slider is the only writer of the size and a zoom changes only the
// analyzer's span clip, never the size:
// From Thetis Project Files/Source/Console/setup.cs:16189 [v2.10.3.15]
//   (the slider writes FFTSize = 4096 * 2^position)
// From Thetis Project Files/Source/Console/HPSDR/specHPSDR.cs:420-429 [v2.10.3.15]
//   (ZoomSlider's setter calls initAnalyzer(); the FFT size is not touched)
// Auto and Hz/bin are NereusSDR's own (Thetis has no automatic sizing and
// no Hz/bin target), kept because a zoom on a wide DDC goes blocky without
// them. Exactly one control sets the running size in each mode.
enum class DisplayFftSizingMode {
    Auto,      ///< the zoom plan, from kDisplayFftSizeDefault; the stored size plays no part
    HzPerBin,  ///< the Hz/bin target plan, floored at kDisplayFftSizeDefault
    Fixed,     ///< exactly the stored DisplayFftSize; zoom and span never change it
};
// Station scope (SettingsScope.cpp), like DisplayFftSize. The stored value is
// one of the three tokens below.
inline constexpr const char* kDisplayFftSizingModeKey = "DisplayFftSizingMode";
inline constexpr const char* kDisplayFftSizingModeAutoToken = "Auto";
inline constexpr const char* kDisplayFftSizingModeHzPerBinToken = "HzPerBin";
inline constexpr const char* kDisplayFftSizingModeFixedToken = "Fixed";
inline constexpr DisplayFftSizingMode kDisplayFftSizingModeDefault = DisplayFftSizingMode::Auto;
// The last Hz/bin target chosen, so leaving and re-entering Hz/bin restores
// it (station scope). DisplayHzPerBinTarget itself holds the target only
// while the mode is HzPerBin and 0 otherwise, which is what it means to a
// window or phone that does not know the mode (design D39).
inline constexpr const char* kDisplayHzPerBinLastTargetKey = "DisplayHzPerBinLastTarget";

// The control's label and its three choices in the order they are shown,
// as the station catalog lists them (the Display pill and Setup draw the
// same words, gui/DisplayFftReadout.h; tst_display_catalogue holds the two
// together).
inline constexpr const char* kDisplayFftSizingLabel = "Sizing";
struct DisplayFftSizingItem {
    DisplayFftSizingMode mode;
    const char* label;
};
inline constexpr std::array<DisplayFftSizingItem, 3> kDisplayFftSizingItems{{
    {DisplayFftSizingMode::Auto, "Auto"},
    {DisplayFftSizingMode::HzPerBin, "Hz/bin"},
    {DisplayFftSizingMode::Fixed, "Fixed"},
}};

/// The stored token of `mode`.
constexpr const char* displayFftSizingModeToken(DisplayFftSizingMode mode) noexcept
{
    switch (mode) {
    case DisplayFftSizingMode::HzPerBin: return kDisplayFftSizingModeHzPerBinToken;
    case DisplayFftSizingMode::Fixed:    return kDisplayFftSizingModeFixedToken;
    case DisplayFftSizingMode::Auto:     break;
    }
    return kDisplayFftSizingModeAutoToken;
}

// The mode in force, from the stored DisplayFftSizingMode token and the
// stored DisplayHzPerBinTarget. Every reader uses this one rule, on read; no
// settings migration writes it back (design D40):
//  - "Fixed" is Fixed;
//  - "Auto" is Auto;
//  - "HzPerBin" is HzPerBin while the target is above 0, and Auto without
//    one, so the mode read is always the one that runs;
//  - a key that is absent (an empty token) or holds anything else is the
//    upgrade case: a target above 0 gives HzPerBin, anything else Auto.
constexpr DisplayFftSizingMode displayFftSizingModeOnRead(std::string_view storedToken,
                                                          double hzPerBinTarget) noexcept
{
    // !(x > 0) is also true for NaN.
    const bool targetOn = hzPerBinTarget > 0.0
        && hzPerBinTarget <= std::numeric_limits<double>::max();
    if (storedToken == kDisplayFftSizingModeFixedToken) {
        return DisplayFftSizingMode::Fixed;
    }
    if (storedToken == kDisplayFftSizingModeAutoToken) {
        return DisplayFftSizingMode::Auto;
    }
    return targetOn ? DisplayFftSizingMode::HzPerBin : DisplayFftSizingMode::Auto;
}

/// The size `mode`'s own control chose, before any zoom: the stored
/// DisplayFftSize in Fixed (the default when it is not one of the seven),
/// kDisplayFftSizeDefault in Auto and HzPerBin. It is the plan's floor, the
/// size a plain "wide" request asks a Core for, and the size the FFT size
/// offset belongs to (FFTEngine::setFftSizeBaseline).
constexpr int displayFftSizingBaseline(DisplayFftSizingMode mode, int storedSize) noexcept
{
    if (mode == DisplayFftSizingMode::Fixed && isDisplayFftSize(storedSize)) {
        return storedSize;
    }
    return kDisplayFftSizeDefault;
}

// The one planner (design D42): the FFT size a pan plans in `mode`, for a
// local pan, a remote pan, Setup's readouts and the pill's readout alike,
// with a Swift twin on the phone.
//  - Auto: planAutoZoomFft from kDisplayFftSizeDefault with no target.
//  - HzPerBin: planAutoZoomFft from kDisplayFftSizeDefault with the target;
//    a target that is not above 0 plans as Auto does.
//  - Fixed: exactly `storedSize` (displayFftSizingBaseline), whatever the
//    rate and span, with no cap. The caller applies no hysteresis to it.
// `storedSize` (the stored DisplayFftSize) is read in Fixed only.
// `hzPerBinInCharge` is true only in HzPerBin, when the target's size is
// above the default.
constexpr AutoZoomPlan planDisplayFft(DisplayFftSizingMode mode, int storedSize,
                                      double sampleRateHz, double spanHz,
                                      double hzPerBinTarget) noexcept
{
    const int baseline = displayFftSizingBaseline(mode, storedSize);
    switch (mode) {
    case DisplayFftSizingMode::Fixed:
        return {baseline, false};
    case DisplayFftSizingMode::HzPerBin:
        return planAutoZoomFft(baseline, sampleRateHz, spanHz, hzPerBinTarget);
    case DisplayFftSizingMode::Auto:
        break;
    }
    return planAutoZoomFft(baseline, sampleRateHz, spanHz, 0.0);
}

// FFT window: the "Window" combo, in Thetis's order, each item's value its
// WindowFunction (WDSP analyzer.c's case order):
// From Thetis Project Files/Source/Console/setup.designer.cs:35082-35089 [v2.10.3.15]
//   this.comboDispWinType.Items.AddRange(new object[] {
//   "Rectangular", "Blackman-Harris 4T", "Hann", "Flat-Top", "Hamming",
//   "Kaiser", "Blackman-Harris 7T"});
inline constexpr const char* kDisplayFftWindowKey = "DisplayFftWindow";
inline constexpr const char* kDisplayFftWindowLabel = "Window";
inline constexpr std::array<DisplayChoiceItem, 7> kDisplayFftWindows{{
    {0, "Rectangular"},
    {1, "Blackman-Harris 4T"},
    {2, "Hann"},
    {3, "Flat-Top"},
    {4, "Hamming"},
    {5, "Kaiser"},
    {6, "Blackman-Harris 7T"},
}};
// NereusSDR-native default: FFTEngine's own (WindowFunction::BlackmanHarris4).
inline constexpr int kDisplayFftWindowDefault = 1;

// Hz/bin Target: NereusSDR-native (the 2026-05-08 auto-zoom override, no
// Thetis equivalent). 0 is "Off"; above 0 the pan's FFT is at least
// sample rate / target points, the FFT size slider still its floor.
inline constexpr const char* kDisplayHzPerBinTargetKey = "DisplayHzPerBinTarget";
inline constexpr const char* kDisplayHzPerBinTargetLabel = "Hz/bin Target";
inline constexpr double kDisplayHzPerBinTargetMin = 0.0;
inline constexpr double kDisplayHzPerBinTargetMax = 200.0;
inline constexpr double kDisplayHzPerBinTargetStep = 0.5;
inline constexpr int kDisplayHzPerBinTargetDecimals = 2;
inline constexpr double kDisplayHzPerBinTargetDefault = 0.0;
inline constexpr const char* kDisplayHzPerBinTargetOffLabel = "Off";
inline constexpr const char* kDisplayHzPerBinTargetUnit = "Hz/bin";

/// The Hz/bin target that entering Hz/bin starts from when none is
/// remembered (design D39): the bin width running now, up to the
/// control's step and inside its range, so entering changes nothing until
/// the control moves. Up, not down: a target below the bin width running
/// asks for more points than run now, and the next power of two is twice
/// the size (23.4375 Hz at 192 kHz and 8192 points: 23.5 plans 8192 again,
/// 23.0 would plan 16384). A bin width that is not above 0 gives the
/// control's maximum, the coarsest. `binWidthHz` is sample rate /
/// (decimation x FFT size).
inline double hzPerBinTargetForBinWidth(double binWidthHz) noexcept
{
    // !(x > 0) is also true for NaN.
    if (!(binWidthHz > 0.0) || !std::isfinite(binWidthHz)) {
        return kDisplayHzPerBinTargetMax;
    }
    return std::clamp(std::ceil(binWidthHz / kDisplayHzPerBinTargetStep)
                          * kDisplayHzPerBinTargetStep,
                      kDisplayHzPerBinTargetStep, kDisplayHzPerBinTargetMax);
}

// Trace line width, in pixels, set in tenths of a pixel by the Display
// pill's Line row and Setup's Line Width row.
// NereusSDR, based on Thetis: the maximum and step are udDisplayLineWidth's
// (Minimum 0.0, Maximum 5.0, Increment 0.1). The minimum here is 0.1, not
// Thetis's 0.0, because a zero-width trace is invisible.
// From Thetis setup.designer.cs:53016-53043 [v2.10.3.15]
inline constexpr int kDisplayLineWidthTenthsPerPx = 10;
inline constexpr int kDisplayLineWidthMinTenths = 1;
inline constexpr int kDisplayLineWidthMaxTenths = 50;
// The width a pan and Setup start at: udDisplayLineWidth's Value, 1.0.
// It was 1.5 while the GPU path drew every trace one device pixel wide;
// that path now draws the width it is given, and 1.5 looked heavier than
// the line it replaced. A stored width is read as stored.
// From Thetis setup.designer.cs:53039-53043 [v2.10.3.15]
inline constexpr double kDisplayLineWidthDefaultPx = 1.0;

// A line width in pixels as the tenths the controls count, held to the
// range above. Anything that is not a number at or above the minimum
// (a stored 0, a negative, a NaN) is the minimum.
inline int displayLineWidthTenths(double px)
{
    const double tenths = px * kDisplayLineWidthTenthsPerPx;
    if (!(tenths >= kDisplayLineWidthMinTenths)) {
        return kDisplayLineWidthMinTenths;
    }
    if (tenths >= kDisplayLineWidthMaxTenths) {
        return kDisplayLineWidthMaxTenths;
    }
    return static_cast<int>(tenths + 0.5);
}

inline double displayLineWidthPx(int tenths)
{
    return static_cast<double>(tenths) / kDisplayLineWidthTenthsPerPx;
}

// FPS: the Rendering group's frame-rate slider. NereusSDR-native range and
// default (10 to 60, 30); Thetis's udDisplayFPS spans 1 to 144 and starts
// at 60 (setup.designer.cs:33958-33977 [v2.10.3.15]).
inline constexpr const char* kDisplaySpectrumFpsKey = "DisplaySpectrumFps";
inline constexpr const char* kDisplaySpectrumFpsLabel = "FPS";
inline constexpr int kDisplaySpectrumFpsMin = 10;
inline constexpr int kDisplaySpectrumFpsMax = 60;
inline constexpr int kDisplaySpectrumFpsStep = 1;
inline constexpr int kDisplaySpectrumFpsDefault = 30;
inline constexpr const char* kDisplaySpectrumFpsUnit = "fps";

// Spectrum Detector, in Thetis's order (SpectrumDetectorMode's values):
// From Thetis Project Files/Source/Console/setup.designer.cs:34984-34989 [v2.10.3.15]
//   this.comboDispPanDetector.Items.AddRange(new object[] {
//   "Peak", "Rosenfell", "Average", "Sample", "RMS"});
inline constexpr const char* kDisplaySpectrumDetectorKey = "DisplaySpectrumDetector";
inline constexpr const char* kDisplaySpectrumDetectorLabel = "Spectrum Detector";
inline constexpr std::array<DisplayChoiceItem, 5> kDisplaySpectrumDetectors{{
    {0, "Peak"},
    {1, "Rosenfell"},
    {2, "Average"},
    {3, "Sample"},
    {4, "RMS"},
}};
// NereusSDR-native default (SpectrumWidget: Peak).
inline constexpr int kDisplaySpectrumDetectorDefault = 0;

// Spectrum Averaging, in Thetis's order (SpectrumAveraging's values):
// From Thetis Project Files/Source/Console/setup.designer.cs:34959-34963 [v2.10.3.15]
//   this.comboDispPanAveraging.Items.AddRange(new object[] {
//   "None", "Recursive", "Time Window", "Log Recursive"});
inline constexpr const char* kDisplaySpectrumAveragingKey = "DisplaySpectrumAveraging";
inline constexpr const char* kDisplaySpectrumAveragingLabel = "Spectrum Averaging";
inline constexpr std::array<DisplayChoiceItem, 4> kDisplayAveragingModes{{
    {0, "None"},
    {1, "Recursive"},
    {2, "Time Window"},
    {3, "Log Recursive"},
}};
// NereusSDR-native default (SpectrumWidget: Log Recursive).
inline constexpr int kDisplaySpectrumAveragingDefault = 3;

// Spectrum Avg Time, in ms. Thetis's udDisplayAVGTime starts at 30:
// From Thetis Project Files/Source/Console/setup.designer.cs:35019-35023 [v2.10.3.15]
//   this.udDisplayAVGTime.Value = new decimal(new int[] { 30, 0, 0, 0});
// Its range is 1 to 9999 in steps of 1, and so is the waterfall's
// udDisplayAVTimeWF:
// From Thetis Project Files/Source/Console/setup.designer.cs:34996-35023 [v2.10.3.15]
//   this.udDisplayAVGTime.Increment = new decimal(new int[] { 1, 0, 0, 0});
//   this.udDisplayAVGTime.Maximum = new decimal(new int[] { 9999, 0, 0, 0});
//   this.udDisplayAVGTime.Minimum = new decimal(new int[] { 1, 0, 0, 0});
// From Thetis Project Files/Source/Console/setup.designer.cs:34588-34616 [v2.10.3.15]
//   this.udDisplayAVTimeWF.Increment = new decimal(new int[] { 1, 0, 0, 0});
//   this.udDisplayAVTimeWF.Maximum = new decimal(new int[] { 9999, 0, 0, 0});
//   this.udDisplayAVTimeWF.Minimum = new decimal(new int[] { 1, 0, 0, 0});
// The desktop's two Setup boxes and the Display pill's two number boxes
// take that range and step.
inline constexpr const char* kDisplaySpectrumAvgTimeKey = "DisplaySpectrumAverageTimeMs";
inline constexpr const char* kDisplaySpectrumAvgTimeLabel = "Spectrum Avg Time";
inline constexpr int kDisplayAvgTimeMinMs = 1;
inline constexpr int kDisplayAvgTimeMaxMs = 9999;
inline constexpr int kDisplayAvgTimeStepMs = 1;
inline constexpr int kDisplaySpectrumAvgTimeDefaultMs = 30;

// The Display pill's two averaging time sliders stop at half a second, one
// position per ms. NereusSDR choice: the values in use are about 30 to 120
// ms, and past half a second the trace visibly lags. A longer time is
// typed into the number box beside the slider, or into Setup.
inline constexpr int kDisplayAvgTimePillMaxMs = 500;
// What Page Up and Page Down move those sliders by. NereusSDR choice.
inline constexpr int kDisplayAvgTimePillPageMs = 25;

// Decimation matches Thetis by JJ's 2026-09-27 ruling: 1 to 16, step 1,
// starting at 1 (setup.designer.cs:33828-33853 [v2.10.3.15]). The desktop
// saves it beside the other FFT knobs; a window's engines hold what it says.
inline constexpr const char* kDisplayDecimationKey = "DisplayDecimation";
inline constexpr const char* kDisplayDecimationLabel = "Decimation";
inline constexpr int kDisplayDecimationMin = 1;
inline constexpr int kDisplayDecimationMax = 16;
inline constexpr int kDisplayDecimationStep = 1;
inline constexpr int kDisplayDecimationDefault = 1;

// WF Detector: Thetis's waterfall detector has no RMS:
// From Thetis Project Files/Source/Console/setup.designer.cs:34577-34581 [v2.10.3.15]
//   this.comboDispWFDetector.Items.AddRange(new object[] {
//   "Peak", "Rosenfell", "Average", "Sample"});
inline constexpr const char* kDisplayWaterfallDetectorKey = "DisplayWaterfallDetector";
inline constexpr const char* kDisplayWaterfallDetectorLabel = "WF Detector";
inline constexpr std::array<DisplayChoiceItem, 4> kDisplayWaterfallDetectors{{
    {0, "Peak"},
    {1, "Rosenfell"},
    {2, "Average"},
    {3, "Sample"},
}};
// NereusSDR-native default (SpectrumWidget: Peak).
inline constexpr int kDisplayWaterfallDetectorDefault = 0;

// WF Averaging: the same four modes as the spectrum's:
// From Thetis Project Files/Source/Console/setup.designer.cs:34552-34556 [v2.10.3.15]
//   this.comboDispWFAveraging.Items.AddRange(new object[] {
//   "None", "Recursive", "Time Window", "Log Recursive"});
inline constexpr const char* kDisplayWaterfallAveragingKey = "DisplayWaterfallAveraging";
inline constexpr const char* kDisplayWaterfallAveragingLabel = "WF Averaging";
// NereusSDR-native default (SpectrumWidget: None).
inline constexpr int kDisplayWaterfallAveragingDefault = 0;

// WF Avg Time, in ms, in the spectrum's range. Thetis's udDisplayAVTimeWF
// starts at 120:
// From Thetis Project Files/Source/Console/setup.designer.cs:34611-34615 [v2.10.3.15]
//   this.udDisplayAVTimeWF.Value = new decimal(new int[] { 120, 0, 0, 0});
inline constexpr const char* kDisplayWaterfallAvgTimeKey = "DisplayWaterfallAverageTimeMs";
inline constexpr const char* kDisplayWaterfallAvgTimeLabel = "WF Avg Time";
inline constexpr int kDisplayWaterfallAvgTimeDefaultMs = 120;

// ── Noise reduction ───────────────────────────────────────────────────────
//
// The VFO flag's noise-reduction quick controls (right-click on an NR
// button) and the Setup > DSP > NR/ANF tabs that share their ranges. Each
// control names the SliceModel property it writes, the popup's label and
// how the control maps to the property:
//
//   slider  min, max, step are slider units; the property's value is
//           slider x scale; the readout beside it is slider / divide to
//           `decimals` places, then `suffix`. `defaultValue` is a new
//           slice's value in the property's own units; `reset` (when
//           `hasReset`) is the slider value the control's Reset restores.
//           Only DFNR's and MNR's popups and NNR's "Reset tuning" have a
//           Reset of their own.
//   switch  on or off; `defaultValue` is 1 for on.
//   choice  one of `options` (each id the property's enum value);
//           `defaultValue` is the default id, `reset` the id Reset
//           restores when `hasReset`.

enum class NrControlKind { Slider, Switch, Choice };

/// One item of a noise-reduction choice: `id` is the property's value.
struct NrChoiceItem {
    int id;
    const char* label;
};

/// One noise-reduction control, as the quick controls draw it.
struct NrControl {
    NrControlKind kind;
    const char* property;
    const char* label;
    double min;
    double max;
    double step;
    double scale;
    int divide;
    int decimals;
    const char* suffix;
    double defaultValue;
    bool hasReset;
    double reset;
    const NrChoiceItem* options;
    std::size_t optionCount;
};

/// A slider with no Reset of its own.
constexpr NrControl nrSlider(const char* property, const char* label, double min, double max,
                             double step, double scale, int divide, int decimals,
                             const char* suffix, double defaultValue) noexcept
{
    return NrControl{NrControlKind::Slider, property, label, min, max, step, scale, divide,
                     decimals, suffix, defaultValue, false, 0, nullptr, 0};
}

/// A slider whose Reset restores slider position `reset`.
constexpr NrControl nrSliderWithReset(const char* property, const char* label, double min,
                                      double max, double step, double scale, int divide,
                                      int decimals, const char* suffix, double defaultValue,
                                      double reset) noexcept
{
    return NrControl{NrControlKind::Slider, property, label, min, max, step, scale, divide,
                     decimals, suffix, defaultValue, true, reset, nullptr, 0};
}

constexpr NrControl nrSwitch(const char* property, const char* label, bool defaultOn) noexcept
{
    return NrControl{NrControlKind::Switch, property, label, 0, 1, 1, 1, 1, 0, "",
                     defaultOn ? 1.0 : 0.0, false, 0, nullptr, 0};
}

/// A choice; `resetsToDefault` when its Reset restores the default.
template <std::size_t N>
constexpr NrControl nrChoice(const char* property, const char* label,
                             const std::array<NrChoiceItem, N>& options, int defaultId,
                             bool resetsToDefault = false) noexcept
{
    return NrControl{NrControlKind::Choice, property, label, 0, 0, 1, 1, 1, 0, "",
                     double(defaultId), resetsToDefault, double(defaultId), options.data(), N};
}

/// The slider position of `defaultValue` (property units), for a Reset
/// that restores a new slice's value.
constexpr int nrResetAtDefault(double defaultValue, double scale) noexcept
{
    const double position = defaultValue / scale;
    return static_cast<int>(position < 0 ? position - 0.5 : position + 0.5);
}

/// The slider position that shows `value` (property units).
inline int nrSliderFromValue(const NrControl& control, double value) noexcept
{
    return static_cast<int>(std::lround(value / control.scale));
}

/// The property value slider position `position` writes.
inline double nrValueFromSlider(const NrControl& control, int position) noexcept
{
    return double(position) * control.scale;
}

// Pre-AGC or Post-AGC (NrPosition's values), the NR1, NR3 and NNR position
// choice, Post-AGC by default. Thetis's NR starts Post-AGC:
// From Thetis Project Files/Source/Console/radio.cs:1626 [v2.10.3.15]
//   private int rx_anr_position = 1;
inline constexpr std::array<NrChoiceItem, 2> kNrPositions{{
    {0, "Pre-AGC"},
    {1, "Post-AGC"},
}};
inline constexpr int kNrPositionDefault = 1;

// NR1 (WDSP ANR). Thetis's NR spinboxes, in their own units:
// From Thetis Project Files/Source/Console/setup.designer.cs:43418-43557 [v2.10.3.15]
//   this.udLMSNRLeak.Maximum = 1000;   this.udLMSNRLeak.Minimum = 1;   Value = 100
//   this.udLMSNRgain.Maximum = 1000;   this.udLMSNRgain.Minimum = 1;   Value = 100
//   this.udLMSNRdelay.Maximum = 1023;  this.udLMSNRdelay.Minimum = 1;  Value = 16
//   this.udLMSNRtaps.Maximum = 1024;   this.udLMSNRtaps.Minimum = 1;   Value = 64
//   (each Increment = 1)
// and what Thetis hands WDSP for them (SetRXAANRVals takes the gain and
// leak as they are, third_party/wdsp/src/anr.c: two_mu = gain, gamma =
// leakage):
// From Thetis Project Files/Source/Console/setup.cs:8573-8586 [v2.10.3.15]
//   console.radio.GetDSPRX(0, 0).SetNRVals(
//       (int)udLMSNRtaps.Value,
//       (int)udLMSNRdelay.Value,
//       1e-6 * (double)udLMSNRgain.Value,
//       1e-3 * (double)udLMSNRLeak.Value);
// Thetis runs that handler from ForceAllEvents when Setup loads
// (setup.cs:2419 [v2.10.3.15]), so the spinbox defaults replace radio.cs's
// field initialisers (nr_gain = 16e-4, nr_leak = 10e-7, radio.cs:679-681
// [v2.10.3.15]) before anything runs.
inline constexpr double kNr1GainScale = 1e-6;
inline constexpr double kNr1LeakScale = 1e-3;
inline constexpr NrControl kNr1Taps =
    nrSlider("nr1Taps", "Taps", 1, 1024, 1, 1, 1, 0, "", 64);
inline constexpr NrControl kNr1Delay =
    nrSlider("nr1Delay", "Delay", 1, 1023, 1, 1, 1, 0, "", 16);
inline constexpr NrControl kNr1Gain =
    nrSlider("nr1Gain", "Gain", 1, 1000, 1, kNr1GainScale, 1, 0, "", 100 * kNr1GainScale);
inline constexpr NrControl kNr1Leak =
    nrSlider("nr1Leakage", "Leak", 1, 1000, 1, kNr1LeakScale, 1, 0, "", 100 * kNr1LeakScale);
inline constexpr NrControl kNr1Position =
    nrChoice("nr1Position", "Position", kNrPositions, kNrPositionDefault);
inline constexpr std::array<NrControl, 5> kNr1Controls{
    kNr1Taps, kNr1Delay, kNr1Gain, kNr1Leak, kNr1Position,
};

// NR2 (WDSP EMNR). The popup's labels and choices, and a new slice's
// values, are Thetis's:
// From Thetis Project Files/Source/Console/setup.designer.cs:43175-43387 [v2.10.3.15]
//   this.grpDSPNR2NPEMethod.Text = "NPE Method";   OSMS (Checked), MMSE, NSTAT
//   this.grpDSPGainMethod.Text = "Gain Method";   Linear, Log, Gamma (Checked), Trained
// From Thetis Project Files/Source/Console/setup.designer.cs:43223-43230 [v2.10.3.15]
//   this.chkDSPNR2AE.Checked = true;   this.chkDSPNR2AE.Text = "AE Filter";
// From Thetis Project Files/Source/Console/setup.designer.cs:42992 [v2.10.3.15]
//   this.chkNR2PostProc_enable_rx1.Text = "Noise post proc";
// From Thetis Project Files/Source/Console/setup.designer.cs:43005, 43100 [v2.10.3.15]
//   this.labelTS476.Text = "Factor:";   this.labelTS475.Text = "Rate:";
// From Thetis Project Files/Source/Console/radio.cs:2064-2179 [v2.10.3.15]
//   rx_nr2_gain_method = 2;  rx_nr2_npe_method = 0;  rx_nr2_ae_run = 1;
//   rx_nr2_ae_post2_run = 0;  rx_nr2_ae_post2_factor = 15.0;
//   rx_nr2_ae_post2_rate = 5.0;
// Factor and Rate match Thetis's nudNR2PostProc_factor_rx1 and
// nudNR2PostProc_rate_rx1: 0 to 100 in steps of 0.1. Integer slider
// positions are tenths; property values remain unscaled.
// From Thetis setup.designer.cs:43019-43158 [v2.10.3.15].
inline constexpr std::array<NrChoiceItem, 4> kNr2GainMethods{{
    {0, "Linear"},
    {1, "Log"},
    {2, "Gamma"},
    {3, "Trained"},
}};
inline constexpr std::array<NrChoiceItem, 3> kNr2NpeMethods{{
    {0, "OSMS"},
    {1, "MMSE"},
    {2, "NSTAT"},
}};
inline constexpr NrControl kNr2GainMethod =
    nrChoice("nr2GainMethod", "Gain Method", kNr2GainMethods, 2);
inline constexpr NrControl kNr2NpeMethod =
    nrChoice("nr2NpeMethod", "NPE Method", kNr2NpeMethods, 0);
inline constexpr NrControl kNr2AeFilter = nrSwitch("nr2AeFilter", "AE Filter", true);
inline constexpr NrControl kNr2Post2Run = nrSwitch("nr2Post2Run", "Noise post proc", false);
inline constexpr NrControl kNr2Post2Factor =
    nrSlider("nr2Post2Factor", "Factor", 0, 1000, 1, 0.1, 10, 1, "", 15.0);
inline constexpr NrControl kNr2Post2Rate =
    nrSlider("nr2Post2Rate", "Rate", 0, 1000, 1, 0.1, 10, 1, "", 5.0);
inline constexpr std::array<NrControl, 6> kNr2Controls{
    kNr2GainMethod, kNr2NpeMethod, kNr2AeFilter, kNr2Post2Run, kNr2Post2Factor, kNr2Post2Rate,
};

// NR3 (WDSP RNNR). Its position starts Post-AGC and it starts with the
// fixed input gain:
// From Thetis Project Files/Source/Console/radio.cs:2275 [v2.10.3.15]
//   private int rx_nr3_position = 1;
// From Thetis Project Files/Source/Console/setup.designer.cs:42436-42443 [v2.10.3.15]
//   this.chkNR3_RNNoiseFixedGain.Checked = true;
//   this.chkNR3_RNNoiseFixedGain.Text = "Use fixed gain for input samples";
inline constexpr NrControl kNr3Position =
    nrChoice("nr3Position", "Position", kNrPositions, kNrPositionDefault);
inline constexpr NrControl kNr3UseDefaultGain =
    nrSwitch("nr3UseDefaultGain", "Use fixed gain for input samples", true);
inline constexpr std::array<NrControl, 2> kNr3Controls{
    kNr3Position, kNr3UseDefaultGain,
};

// NR4 (WDSP SBNR). The labels are Thetis's:
// From Thetis Project Files/Source/Console/setup.designer.cs:42131-42386 [v2.10.3.15]
//   "Reduction", "Smoothing", "Whitening", "Rescale", "SNRthresh",
//   "Algo 1", "Algo 2", "Algo 3"
// Ranges, increments and new-slice defaults match the spinboxes:
// From Thetis setup.designer.cs:42186-42412 [v2.10.3.15]
//   Rescale 0..12, SNRthresh -10..10, Smoothing/Whitening default 0,
//   Reduction default 10, Rescale default 2, SNRthresh default -10;
//   all increments 1 with one decimal place. Algo 1 is checked at 42154.
inline constexpr std::array<NrChoiceItem, 3> kNr4Algos{{
    {0, "Algo 1"},
    {1, "Algo 2"},
    {2, "Algo 3"},
}};
inline constexpr NrControl kNr4Reduction =
    nrSlider("nr4Reduction", "Reduction", 0, 20, 1, 1, 1, 1, " dB", 10.0);
inline constexpr NrControl kNr4Smoothing =
    nrSlider("nr4Smoothing", "Smoothing", 0, 100, 1, 1, 1, 1, "%", 0.0);
inline constexpr NrControl kNr4Whitening =
    nrSlider("nr4Whitening", "Whitening", 0, 100, 1, 1, 1, 1, "%", 0.0);
inline constexpr NrControl kNr4Rescale =
    nrSlider("nr4Rescale", "Rescale", 0, 12, 1, 1, 1, 1, " dB", 2.0);
inline constexpr NrControl kNr4PostThresh =
    nrSlider("nr4PostThresh", "SNRthresh", -10, 10, 1, 1, 1, 1, " dB", -10.0);
inline constexpr NrControl kNr4Algo = nrChoice("nr4Algo", "Algo", kNr4Algos, 0);
inline constexpr std::array<NrControl, 6> kNr4Controls{
    kNr4Reduction, kNr4Smoothing, kNr4Whitening, kNr4Rescale, kNr4PostThresh, kNr4Algo,
};

// DFNR (DeepFilterNet3), a post-WDSP filter that is not in Thetis. A new
// slice starts at AetherSDR's DeepFilterFilter defaults [@0cd4559]
// (m_attenLimit{100.0f}, m_postFilterBeta{0.0f}), and the popup's Reset
// restores them (Attenuation Limit 100 dB, Post-Filter Beta 0).
inline constexpr NrControl kDfnrAttenLimit = nrSliderWithReset(
    "dfnrAttenLimit", "Attenuation Limit", 0, 100, 1, 1, 1, 0, " dB", 100.0, 100);
inline constexpr NrControl kDfnrPostFilterBeta = nrSliderWithReset(
    "dfnrPostFilterBeta", "Post-Filter Beta", 0, 100, 1, 0.01, 100, 2, "", 0.0, 0);
inline constexpr std::array<NrControl, 2> kDfnrControls{
    kDfnrAttenLimit, kDfnrPostFilterBeta,
};

// MNR (macOS Accelerate MMSE-Wiener NR), NereusSDR-native quick controls.
// A new slice starts at MacNRFilter's own DEF_* values (src/core/
// MacNRFilter.h, ported from AetherSDR [@0cd4559]; NereusSDR raised OVER
// from AetherSDR's 2 to 4), and Reset restores the same values, so the
// popup, Setup's MNR tab and a new slice agree. MacNRFilter.h checks its
// DEF_* against these on a Mac build.
inline constexpr double kMnrStrengthDefault = 1.0;
inline constexpr double kMnrOversubDefault = 4.0;    // MacNRFilter::DEF_OVER
inline constexpr double kMnrFloorDefault = 0.05;     // MacNRFilter::DEF_FLOOR
inline constexpr double kMnrAlphaDefault = 0.92;     // MacNRFilter::DEF_ALPHA
inline constexpr double kMnrBiasDefault = 1.2;       // MacNRFilter::DEF_BIAS
inline constexpr double kMnrGsmoothDefault = 0.70;   // MacNRFilter::DEF_GSMOOTH
inline constexpr NrControl kMnrStrength =
    nrSliderWithReset("mnrStrength", "Strength", 0, 200, 1, 0.01, 1, 0, "%", kMnrStrengthDefault,
             nrResetAtDefault(kMnrStrengthDefault, 0.01));
inline constexpr NrControl kMnrOversub =
    nrSliderWithReset("mnrOversub", "Aggressiveness", 1, 1000, 1, 1, 1, 0, "", kMnrOversubDefault,
             nrResetAtDefault(kMnrOversubDefault, 1));
inline constexpr NrControl kMnrFloor =
    nrSliderWithReset("mnrFloor", "Floor", 0, 2000, 1, 0.001, 1, 0, "m", kMnrFloorDefault,
             nrResetAtDefault(kMnrFloorDefault, 0.001));
inline constexpr NrControl kMnrAlpha =
    nrSliderWithReset("mnrAlpha", "Alpha", 0, 100, 1, 0.01, 100, 2, "", kMnrAlphaDefault,
             nrResetAtDefault(kMnrAlphaDefault, 0.01));
inline constexpr NrControl kMnrBias =
    nrSliderWithReset("mnrBias", "Bias", 0, 100, 1, 0.1, 10, 1, "", kMnrBiasDefault,
             nrResetAtDefault(kMnrBiasDefault, 0.1));
inline constexpr NrControl kMnrGsmooth =
    nrSliderWithReset("mnrGsmooth", "Gsmooth", 0, 100, 1, 0.01, 100, 2, "", kMnrGsmoothDefault,
             nrResetAtDefault(kMnrGsmoothDefault, 0.01));
inline constexpr std::array<NrControl, 6> kMnrControls{
    kMnrStrength, kMnrOversub, kMnrFloor, kMnrAlpha, kMnrBias, kMnrGsmooth,
};
static_assert(kMnrOversub.reset == 4 && kMnrBias.reset == 12 && kMnrFloor.reset == 50
                  && kMnrAlpha.reset == 92 && kMnrGsmooth.reset == 70
                  && kMnrStrength.reset == 100,
              "MNR's Reset restores a new slice's values");

// NNR (WDSP 2.10's neural noise reduction), NereusSDR-native controls
// (NnrControls, in the VFO flag's NNR popup and Setup's NNR tab). Each is
// in its property's own units; the defaults and domains are WDSP 2.10's
// (RXA.c create_nnr, nnet.c create_dfhead, NNET_TAU_DEFAULT, NNET_GMAX_DB)
// with the operator ranges of 2026-09-21-wdsp210-nnr-ps3-design.md section
// 6. NnrSettings holds a slice's values to these and "Reset tuning"
// restores the defaults, leaving the model as it is.
inline constexpr std::array<NrChoiceItem, 2> kNnrModels{{
    {0, "Standard"},
    {1, "Premium"},
}};
inline constexpr NrControl kNnrModel = nrChoice("nnrModelSlot", "Model", kNnrModels, 0);
inline constexpr NrControl kNnrMaskFloor = nrSliderWithReset(
    "nnrMaskFloorDb", "Suppression", -50.0, -10.0, 0.01, 1, 1, 2, " dB", -25.0, -25.0);
inline constexpr NrControl kNnrPosition =
    nrChoice("nnrPosition", "Position", kNrPositions, kNrPositionDefault, true);
inline constexpr NrControl kNnrAlpha =
    nrSliderWithReset("nnrAlpha", "Alpha", 0.0, 4.0, 0.01, 1, 1, 2, "", 1.0, 1.0);
inline constexpr NrControl kNnrAlphaKnee = nrSliderWithReset(
    "nnrAlphaKneeDb", "Alpha knee", 0.0, 40.0, 0.1, 1, 1, 1, " dB", 10.0, 10.0);
inline constexpr NrControl kNnrTau =
    nrSliderWithReset("nnrTauSeconds", "Noise time", 0.05, 30.0, 0.05, 1, 1, 2, " s", 2.0, 2.0);
inline constexpr NrControl kNnrMaxGain = nrSliderWithReset(
    "nnrMaxGainDb", "Maximum gain", 0.0, 24.0, 0.1, 1, 1, 1, " dB", 12.0, 12.0);
inline constexpr NrControl kNnrAttack =
    nrSliderWithReset("nnrAttackMs", "Attack", 0.0, 500.0, 0.1, 1, 1, 1, " ms", 0.0, 0.0);
inline constexpr NrControl kNnrRelease =
    nrSliderWithReset("nnrReleaseMs", "Release", 0.0, 500.0, 0.1, 1, 1, 1, " ms", 0.0, 0.0);
inline constexpr std::array<NrControl, 9> kNnrControls{
    kNnrModel, kNnrMaskFloor, kNnrPosition, kNnrAlpha, kNnrAlphaKnee,
    kNnrTau, kNnrMaxGain, kNnrAttack, kNnrRelease,
};
// The Suppression slider beside the spinbox moves in tenths of a dB.
inline constexpr int kNnrMaskFloorSliderPerDb = 10;

/// One noise-reduction slot's controls, as the catalogue lists them.
struct NrSlotControls {
    const char* key;
    const NrControl* controls;
    std::size_t count;
};

template <std::size_t N>
constexpr NrSlotControls nrSlot(const char* key, const std::array<NrControl, N>& controls) noexcept
{
    return NrSlotControls{key, controls.data(), N};
}

// The catalogue's `noiseReduction` slots, in the VFO flag's order.
inline constexpr std::array<NrSlotControls, 7> kNoiseReductionSlots{{
    nrSlot("nr1", kNr1Controls),
    nrSlot("nr2", kNr2Controls),
    nrSlot("nr3", kNr3Controls),
    nrSlot("nr4", kNr4Controls),
    nrSlot("dfnr", kDfnrControls),
    nrSlot("mnr", kMnrControls),
    nrSlot("nnr", kNnrControls),
}};

// ── Slice colours ─────────────────────────────────────────────────────────

// The slice badge colours, slice A first, as 0xRRGGBB, one for each of
// slices A to H. A slice past the last one takes the first colour
// (VfoWidget::sliceColor).
// From AetherSDR SliceColors.h (A to D)
// From AetherSDR src/core/ThemeSeedGenerated.cpp:92-95 [@1e0718ad] (E to H:
// color.slice.e .. color.slice.h)
inline constexpr std::array<std::uint32_t, 8> kSliceColours{
    0x00d4ffu,  // cyan
    0xff40ffu,  // magenta
    0x40ff40u,  // green
    0xffff00u,  // yellow
    0xffa000u,  // orange
    0x00e0c0u,  // teal
    0xff6080u,  // pink
    0xb080ffu,  // lavender
};

/// The colour of slice `index` (0 is slice A), as 0xRRGGBB.
constexpr std::uint32_t sliceColour(int index) noexcept
{
    if (index < 0 || index >= static_cast<int>(kSliceColours.size())) {
        return kSliceColours[0];
    }
    return kSliceColours[static_cast<std::size_t>(index)];
}

} // namespace NereusSDR::ControlRanges
