// =================================================================
// src/gui/instruments/SMeterDialContent.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original container-content registration for
// added copies of the SIGNAL STRENGTH dial (SMeterWidget, itself ported
// from AetherSDR and attributed in its own files). Workspace preview only.
//
// Independently implemented from the Thetis MeterManager.cs interface;
// the ported constants live in SMeterDialContent.cpp, which carries the
// headers.
//
// Registers one per-instance widget type with ContainerContentRegistry:
//
//   sMeter.dial   SMeterWidget, SettingsStore::Instance
//
// Any number of copies, in any container. Each copy follows its entry's
// effective source context (the container's source chip: Active or RX A
// to D) exactly as a Longpath needle does: its readings come from
// MeterPoller::addReadingConsumer / readingUpdated for that context, its
// transmit state from RadioModel::transmittingChanged, and the external
// amplifier's power scale and watts from the RadioModel signals that feed
// the Main Panel's dial. Each copy keeps its own TX mode, RX mode, peak
// hold, decay and face in its entry config; the right-click menu commits
// them through ContentConfigNotifier. A copy with no face key draws Setup's
// Meter Styles face and follows it until it is given a face of its own. The
// Main Panel's dial (applet:s_meter) and its AppSettings keys are not
// touched.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The external amplifier and Setup's face reach copies.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - TX modes ALC Compression, ALC Group, Volts and Amps; a
//                 power limit per copy (kPowerLimitKey); the VHF S9 rule
//                 of the copy's slice. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Appearance and dynamics per copy: peak hold, needle,
//                 transmit needle, power scale and history colours,
//                 shadow, dark mode, update interval, attack and decay
//                 ratios, history, and Fade on RX / TX; history and peak
//                 hold clear on each transmit change. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "gui/instruments/LongpathContent.h"

#include <QJsonObject>
#include <QList>

namespace NereusSDR {

class ContainerContentRegistry;

namespace SMeterDialContent {

inline constexpr auto kDialType = "sMeter.dial";

// Entry config keys. Absent keys are today's defaults (the Main Panel's
// defaults with no saved key): Power, Signal, peak hold on, Medium; an
// absent kFaceKey is Setup's face (SMeter_FaceStyle), followed. Values are the right-click menu's names: kTxModeKey "Power",
// "SWR", "Level", "Compression", "ALC Compression", "ALC Group", "Volts",
// "Amps"; kRxModeKey "Signal", "Sig Avg",
// "Signal Peak", "Max Bin"; kPeakHoldKey true/false; kPeakDecayKey "Fast",
// "Medium", "Slow"; kFaceKey an SMeterWidget::faceStyleKey.
// kPowerLimitKey, whole watts: the copy's own power scale, red from the
// limit and full scale 20% above it (top = limit *
// ControlRanges::kRfPowerGaugeHeadroom, as the bars). Absent or 0: the radio's scale, followed as the Main
// Panel's dial follows it (the external amplifier's included).
inline constexpr auto kTxModeKey    = "txMode";
inline constexpr auto kRxModeKey    = "rxMode";
inline constexpr auto kPeakHoldKey  = "peakHold";
inline constexpr auto kPeakDecayKey = "peakDecay";
inline constexpr auto kFaceKey      = "face";
inline constexpr auto kPowerLimitKey = "powerLimitW";

// Appearance and dynamics. An absent key is the dial as it was drawn
// before these keys existed. Colours are "#rrggbb" (absent: the face's
// own); kAttackRatioKey / kDecayRatioKey are numbers 0..1 (absent: the
// built-in time constants); kUpdateIntervalKey, kHistoryDurationKey and
// kIgnoreHistoryKey are whole milliseconds; kHistoryAlphaKey 0..255 is the
// history colour's opacity; kShowHistoryKey, kShadowKey and kDarkModeKey
// are true/false (absent: off, on, off). Fade on RX / TX are
// ContentFade::kFadeOnRxKey / kFadeOnTxKey.
inline constexpr auto kPeakHoldColorKey   = "peakHoldColor";
inline constexpr auto kNeedleColorKey     = "needleColor";
inline constexpr auto kSubNeedleColorKey  = "subNeedleColor";
inline constexpr auto kShadowKey          = "shadow";
inline constexpr auto kPowerScaleColorKey = "powerScaleColor";
inline constexpr auto kDarkModeKey        = "darkMode";
inline constexpr auto kUpdateIntervalKey  = "updateIntervalMs";
inline constexpr auto kAttackRatioKey     = "attackRatio";
inline constexpr auto kDecayRatioKey      = "decayRatio";
inline constexpr auto kShowHistoryKey     = "showHistory";
inline constexpr auto kHistoryColorKey    = "historyColor";
inline constexpr auto kHistoryAlphaKey    = "historyAlpha";
inline constexpr auto kHistoryDurationKey = "historyDurationMs";
inline constexpr auto kIgnoreHistoryKey   = "ignoreHistoryMs";

// Registers the type. Call only with the workspace preview on, after
// LongpathContent::registerAll (the picker lists it after the Longpath
// instruments).
void registerDial(ContainerContentRegistry& registry, const LongpathContent::Sources& sources);

// The MeterBinding a copy's RX mode reads: SignalPeak for Signal and
// Signal Peak, SignalAvg for Sig Avg, SignalMaxBin for Max Bin (the same
// choice MeterPoller::pollAdaptedSMeter makes for the Main Panel's dial).
int rxBindingForMode(const QString& rxMode);

// Every binding a copy registers with the poller: the three RX bindings
// and TxPower, TxSwr, TxMic, TxComp, TxAlcGain, TxAlcGroup, HwVolts,
// HwAmps.
QList<int> registeredBindings();

} // namespace SMeterDialContent
} // namespace NereusSDR
