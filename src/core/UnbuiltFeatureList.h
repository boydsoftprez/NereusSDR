// no-port-check: NereusSDR-original. The one list of features not built yet.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/UnbuiltFeatureList.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// The list itself (R-R3-49): the features not built yet and the one
// query, isBuilt(), that every surface asks. It lives in the Core library
// so the Core's own catalogue offers an app the same tools the desktop
// shows (iPhone app plan Task 25, D41); src/gui/UnbuiltFeatures.h adds the
// helpers that hide a desktop control and includes this file, so every
// desktop caller is unchanged. See that header for the list's rules.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28 - iPhone app plan Task 25 (D41): the enum, all(), key(),
//                 isBuilt() and the test seams moved here from
//                 src/gui/UnbuiltFeatures.{h,cpp} unchanged. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - ExportRadio leaves the list: Export Connected Radio is
//                 built. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: Hl2TxTiming built (the TX buffer
//                latency and PTT hang reach bank 17). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - AlexTxFilterOptions leaves the list: the Alex-1 low-pass
//                band edges and 6m/ByPass on RX select the low-pass
//                (radioHardwareVersion 10). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - TciExtras leaves the list: the three RX2 VFO options
//                reach the TCI wire as Thetis sends them. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QList>
#include <QString>

namespace NereusSDR {

// One entry per unbuilt feature. The comment on each names its surfaces.
enum class UnbuiltFeature {
    DisplayMode,      // View > Display Mode; the eleven container display-mode buttons
    UiScale,          // View > UI Scale; Setup > General > UI Scale & Theme
    MinimalMode,      // View > Minimal Mode; Setup > Appearance > Collapsible Display
    Keyboard,         // View > Keyboard Shortcuts; Setup > Keyboard > Shortcuts
    Equalizer,        // DSP > Equalizer (built after R4)
    Transverters,     // Radio > Transverters; Band > VHF; Hardware > XVTR; OC Outputs VHF tab;
                      // the container band and antenna XVTR buttons
    BandStack,        // Band > Band Stacking; the band stack dots on the status bar
    Cwx,              // Tools > CWX; Phone/CW applet CW page; CW keyer settings; CWX on the status bar
    Memories,         // Tools > Memory Manager; the Spot Hub Memories option
    Cat,              // Tools > CAT Control; Setup > Serial Ports, TCP/IP CAT; CAT on the status bar
    Midi,             // Tools > MIDI Mapping; Setup > MIDI Control
    Help,             // Help > Getting Started, NereusSDR Help, Understanding Data Modes
    Acc,              // Phone/CW applet +ACC and the microphone source ACC item
    PhoneMon,         // Phone/CW applet MON and its level
    FmPage,           // Phone/CW applet FM page
    RfkitTune,        // RF-Kit applet TUNE and BYPASS
    MacroButtons,     // The container macro buttons (built after R4)
    DisplayAveraging, // Display averaging from a container: the container AVG button
    TwoReceiverLayout,// The two-receiver layout (RX1 and RX2, never built for slices A to D):
                      // the container RX2, SUB RX and Pan Swap buttons
    VariableFilters,  // The variable filter slots: the container filter Var1 and Var2 buttons
    AntennaRxTxSplit, // The antenna box's receive/transmit split: the container antenna Rx/Tx button
    Voice,            // Voice Rec/Play container control; VFO flag record and play; DVK on the status bar;
                      // the container Play and Rec buttons
    Fdx,              // FDX on the status bar (full duplex; the DUP button is display duplex, built)
    Navigation,       // Setup > General > Navigation
    Sam,              // Setup > DSP > AM/SAM synchronous AM options (built after R4)
    Skins,            // Setup > Appearance > Skins
    TxProfilesLeaf,   // Setup > Transmit > TX Profiles (a page that only says it moved)
    BandwidthMonitor, // Setup > Hardware > Bandwidth Monitor
    Hl2SecondI2cBus,  // Setup > Hardware > HL2 Options second I2C bus
    ConnectionHistory,// Setup > Diagnostics > Connection Quality 60 s history (built after R4)
    Logging,          // Setup > Diagnostics > Logging: log level, open and clear, categories (built after R4)
    SignalGenerator,  // Setup > Diagnostics > Signal Generator and Hardware Tests
    DspRate,          // Setup > Audio > Advanced DSP sample rate and block size
    IqToVax,          // Setup > Audio > Advanced Send IQ to VAX, TX Monitor to VAX
    MuteVaxDuringTx,  // Setup > Audio > Advanced Mute VAX during TX on other slice (built after R4)
    AntennaConflict,  // Setup > Hardware > Antenna Control conflict policy
    OcExtras,         // Setup > Hardware > OC Outputs hot switching, USB BCD, external PA;
                      // the container xPA button
    MultimeterHolds,  // Setup > Display > Multimeter peak hold, text hold, digital delay, history (built after R4)
    WsjtxFilters,     // Spot Hub WSJT-X filters (three) (built after R4)
    RbnRateLimit,     // Spot Hub RBN rate limit (built after R4)
    FreeDvToPsk,      // Spot Hub report FreeDV decodes to PSK Reporter (built after R4)
    SmallFilter,      // Setup > Appearance small filter display on the VFO flag
    ApfParams,        // Setup > DSP > CW peak filter bandwidth and gain (built after R4)
    AmSquelchTail,    // Setup > DSP > AM/SAM maximum squelch tail (built after R4)
    FmDeviation,      // Setup > DSP > FM deviation and de-emphasis (built after R4)
    FmTransmit,       // Setup > DSP > FM transmit group; the VFO flag's FM repeater minus,
                      // simplex and plus buttons, the Offset box and Rev (table rows fm-tx,
                      // fm-repeater and fm-flag)
    DdcRouting,       // Setup > Hardware > DDC Routing (multi-panadapter receiver routing)
    HpfBroadcastReject,   // The filter policy dialog's "HPF (broadcast band reject) enabled"
    FrequencyCalibration, // Setup > Hardware > Calibration: the frequency calibration Start button
    FmTones,          // CTCSS tone encode and tone squelch: the VFO flag's FM tone mode and tone
                      // choices (plan row fm-flag)
    GanymedeTrip,     // Status PA badge: Andromeda/Ganymede CAT trip input is not ported
    PbSnr,            // Multimeter PBSNR binding has no producer
    ContainerFilterDisplay, // Feed implemented; gate awaits loaded FFT startup acceptance
    ContainerClickBox, // Container click box has no host action routing
    AudioBitDepth,    // Audio Devices bit-depth hint is not consumed by an audio backend
    AudioAutoMatch,   // Audio Devices default sample-rate lookup is not ported
    AudioMonitorTxInput, // Audio Devices microphone monitor routing is not ported
    AudioToneCheck,   // Audio Devices first-PTT tone generator is not ported
    WaterfallLowColor, // Colors & Theme low-level color has no gradient setter
    MultimeterAveraging, // MeterPoller stores window size but does not average readings
    TxGridScale,      // TX Display grid controls are an inert placeholder
};

namespace UnbuiltFeatures {

struct Entry {
    UnbuiltFeature feature;
    QString key;          // the decisions table's item name, e.g. "band-stack"
    QString description;  // what the feature is, for logs and tests
};

// Every entry of the list, in the decisions table's order.
const QList<Entry>& all();

// The decisions table's item name for a feature.
QString key(UnbuiltFeature feature);

// The single query every surface calls. False for every entry until its
// feature is built.
bool isBuilt(UnbuiltFeature feature);

// Tests only: mark a feature built (or not) to prove its surfaces appear.
// The surfaces read the list when they are built, so build them after.
void setBuiltForTest(UnbuiltFeature feature, bool built);
void resetForTest();

} // namespace UnbuiltFeatures
} // namespace NereusSDR
