// no-port-check: NereusSDR-original. See header.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/UnbuiltFeatureList.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for full
// Modification history (NereusSDR).
//   2026-09-29 - HL2 port part 1: Hl2TxTiming removed (built). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - TciExtras removed (built). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/UnbuiltFeatureList.h"

#include <QSet>

namespace NereusSDR::UnbuiltFeatures {

namespace {

using F = UnbuiltFeature;

// Features a test has marked built. Empty in the app: nothing on the list
// is built.
QSet<int>& builtForTest()
{
    static QSet<int> built;
    return built;
}

} // namespace

const QList<Entry>& all()
{
    static const QList<Entry> entries = {
        {F::DisplayMode, QStringLiteral("display-mode"),
         QStringLiteral("View > Display Mode")},
        {F::UiScale, QStringLiteral("ui-scale"),
         QStringLiteral("View > UI Scale; Setup > General > UI Scale & Theme")},
        {F::MinimalMode, QStringLiteral("minimal-mode"),
         QStringLiteral("View > Minimal Mode; Setup > Appearance > Collapsible Display")},
        {F::Keyboard, QStringLiteral("keyboard"),
         QStringLiteral("View > Keyboard Shortcuts; Setup > Keyboard > Shortcuts")},
        {F::Equalizer, QStringLiteral("equalizer"),
         QStringLiteral("DSP > Equalizer")},
        {F::Transverters, QStringLiteral("transverters"),
         QStringLiteral("Radio > Transverters; Band > VHF; Hardware > XVTR; OC Outputs VHF tab")},
        {F::BandStack, QStringLiteral("band-stack"),
         QStringLiteral("Band > Band Stacking; the band stack dots on the status bar")},
        {F::Cwx, QStringLiteral("cwx"),
         QStringLiteral("Tools > CWX; Phone/CW applet CW page; the CW keyer settings; "
                        "CWX on the status bar")},
        {F::Memories, QStringLiteral("memories"),
         QStringLiteral("Tools > Memory Manager; the Memories spot option")},
        {F::Cat, QStringLiteral("cat"),
         QStringLiteral("Tools > CAT Control; Setup > Serial Ports and TCP/IP CAT; "
                        "CAT on the status bar")},
        {F::Midi, QStringLiteral("midi"),
         QStringLiteral("Tools > MIDI Mapping; Setup > MIDI Control")},
        {F::Help, QStringLiteral("help"),
         QStringLiteral("Help > Getting Started, Help, Data Modes")},
        {F::Acc, QStringLiteral("acc"),
         QStringLiteral("Phone/CW applet +ACC and the microphone source ACC item")},
        {F::PhoneMon, QStringLiteral("phone-mon"),
         QStringLiteral("Phone/CW applet MON and its level")},
        {F::FmPage, QStringLiteral("fm-page"),
         QStringLiteral("Phone/CW applet FM page")},
        {F::RfkitTune, QStringLiteral("rfkit-tune"),
         QStringLiteral("RF-Kit applet TUNE and BYPASS")},
        {F::MacroButtons, QStringLiteral("macro-buttons"),
         QStringLiteral("The container macro buttons")},
        {F::DisplayAveraging, QStringLiteral("display-averaging"),
         QStringLiteral("Display averaging from a container: the container AVG button")},
        {F::TwoReceiverLayout, QStringLiteral("two-receiver-layout"),
         QStringLiteral("The two-receiver layout, never built for slices: the container RX2, "
                        "SUB RX and Pan Swap buttons")},
        {F::VariableFilters, QStringLiteral("variable-filters"),
         QStringLiteral("The variable filter slots: the container filter Var1 and Var2 buttons")},
        {F::AntennaRxTxSplit, QStringLiteral("antenna-rx-tx"),
         QStringLiteral("The antenna box's receive/transmit split: the container antenna Rx/Tx "
                        "button")},
        {F::Voice, QStringLiteral("voice"),
         QStringLiteral("Voice record and play container control; VFO flag record and play; "
                        "DVK on the status bar")},
        {F::Fdx, QStringLiteral("fdx"),
         QStringLiteral("Status bar FDX")},
        {F::Navigation, QStringLiteral("navigation"),
         QStringLiteral("Setup > General > Navigation")},
        {F::Sam, QStringLiteral("sam"),
         QStringLiteral("Setup > DSP > AM/SAM synchronous AM options")},
        {F::Skins, QStringLiteral("skins"),
         QStringLiteral("Setup > Appearance > Skins")},
        {F::TxProfilesLeaf, QStringLiteral("tx-profiles-leaf"),
         QStringLiteral("Setup > Transmit > TX Profiles")},
        {F::BandwidthMonitor, QStringLiteral("bw-monitor"),
         QStringLiteral("Setup > Hardware > Bandwidth Monitor")},
        {F::Hl2SecondI2cBus, QStringLiteral("hl2-i2c"),
         QStringLiteral("Setup > Hardware > HL2 Options second I2C bus")},
        {F::ConnectionHistory, QStringLiteral("conn-history"),
         QStringLiteral("Setup > Diagnostics > Connection Quality 60 second history")},
        {F::Logging, QStringLiteral("logging"),
         QStringLiteral("Setup > Diagnostics > Logging: log level, open and clear, categories")},
        {F::SignalGenerator, QStringLiteral("siggen"),
         QStringLiteral("Setup > Diagnostics > Signal Generator and Hardware Tests")},
        {F::DspRate, QStringLiteral("dsp-rate"),
         QStringLiteral("Setup > Audio > Advanced DSP rate and block size")},
        {F::IqToVax, QStringLiteral("iq-to-vax"),
         QStringLiteral("Setup > Audio > Advanced Send IQ to VAX, TX Monitor to VAX")},
        {F::MuteVaxDuringTx, QStringLiteral("mute-vax-tx"),
         QStringLiteral("Setup > Audio > Advanced Mute VAX during transmit on another slice")},
        {F::AntennaConflict, QStringLiteral("ant-conflict"),
         QStringLiteral("Setup > Hardware > Antenna conflict policy")},
        {F::OcExtras, QStringLiteral("oc-extras"),
         QStringLiteral("Setup > Hardware > OC Outputs hot switching, USB BCD, external PA")},
        {F::MultimeterHolds, QStringLiteral("multimeter"),
         QStringLiteral("Setup > Multimeter peak hold, text hold, digital delay, history")},
        {F::WsjtxFilters, QStringLiteral("wsjtx-filters"),
         QStringLiteral("Spot Hub WSJT-X filters (three)")},
        {F::RbnRateLimit, QStringLiteral("rbn-rate"),
         QStringLiteral("Spot Hub RBN rate limit")},
        {F::FreeDvToPsk, QStringLiteral("freedv-psk"),
         QStringLiteral("Spot Hub report FreeDV decodes to PSK Reporter")},
        {F::SmallFilter, QStringLiteral("small-filter"),
         QStringLiteral("Setup > Appearance small filter display on the VFO flag")},
        {F::ApfParams, QStringLiteral("apf-params"),
         QStringLiteral("Setup > DSP > CW peak filter bandwidth and gain")},
        {F::AmSquelchTail, QStringLiteral("am-tail"),
         QStringLiteral("Setup > DSP > AM/SAM maximum squelch tail")},
        {F::FmDeviation, QStringLiteral("fm-dev"),
         QStringLiteral("Setup > DSP > FM deviation and de-emphasis")},
        // The table's fm-tx and fm-repeater rows both wait for FM transmit:
        // one feature, one entry.
        {F::FmTransmit, QStringLiteral("fm-tx"),
         QStringLiteral("FM transmit: Setup > DSP > FM transmit group; the VFO flag's FM "
                        "repeater minus, simplex and plus buttons, Offset and Rev")},
        {F::DdcRouting, QStringLiteral("ddc-routing"),
         QStringLiteral("Setup > Hardware > DDC Routing (multi-panadapter receiver routing)")},
        {F::HpfBroadcastReject, QStringLiteral("hpf-bcast"),
         QStringLiteral("Filter policy dialog: HPF (broadcast band reject) enabled")},
        {F::FrequencyCalibration, QStringLiteral("freq-cal"),
         QStringLiteral("Setup > Hardware > Calibration: frequency calibration Start")},
        // Plan row fm-flag: the tone choices serve both a tone encoder and a
        // tone squelch, neither built.
        {F::FmTones, QStringLiteral("fm-tone"),
         QStringLiteral("CTCSS tone encode and tone squelch: the VFO flag's FM tone mode and "
                        "tone choices")},
        {F::GanymedeTrip, QStringLiteral("ganymede-trip"),
         QStringLiteral("Status PA badge: Andromeda/Ganymede CAT trip input is not ported")},
        {F::PbSnr, QStringLiteral("pb-snr"),
         QStringLiteral("Multimeter PBSNR binding has no producer")},
        {F::ContainerClickBox, QStringLiteral("container-click-box"),
         QStringLiteral("Container click box has no host action routing")},
        {F::AudioBitDepth, QStringLiteral("audio-bit-depth"),
         QStringLiteral("Audio Devices bit-depth hint has no audio backend")},
        {F::AudioAutoMatch, QStringLiteral("audio-auto-match"),
         QStringLiteral("Audio Devices default sample-rate lookup is not ported")},
        {F::AudioMonitorTxInput, QStringLiteral("audio-monitor-tx-input"),
         QStringLiteral("Audio Devices microphone monitor routing is not ported")},
        {F::AudioToneCheck, QStringLiteral("audio-tone-check"),
         QStringLiteral("Audio Devices first-PTT tone generator is not ported")},
        {F::WaterfallLowColor, QStringLiteral("waterfall-low-color"),
         QStringLiteral("Colors & Theme low-level color has no gradient setter")},
        {F::MultimeterAveraging, QStringLiteral("multimeter-averaging"),
         QStringLiteral("MeterPoller stores a window size but does not average readings")},
        {F::TxGridScale, QStringLiteral("tx-grid-scale"),
         QStringLiteral("TX Display grid controls are not ported")},
    };
    return entries;
}

QString key(UnbuiltFeature feature)
{
    for (const Entry& entry : all()) {
        if (entry.feature == feature) { return entry.key; }
    }
    return {};
}

bool isBuilt(UnbuiltFeature feature)
{
    if (feature == UnbuiltFeature::ContainerFilterDisplay) { return true; }
    return builtForTest().contains(static_cast<int>(feature));
}

void setBuiltForTest(UnbuiltFeature feature, bool built)
{
    if (built) {
        builtForTest().insert(static_cast<int>(feature));
    } else {
        builtForTest().remove(static_cast<int>(feature));
    }
}

void resetForTest()
{
    builtForTest().clear();
}

} // namespace NereusSDR::UnbuiltFeatures
