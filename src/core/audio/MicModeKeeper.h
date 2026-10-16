// =================================================================
// src/core/audio/MicModeKeeper.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Keeps the window's stored macOS
// Mic Mode at Standard; no upstream logic.
//
// macOS keeps a Mic Mode for each app (Standard, Wide Spectrum, Voice
// Isolation; Control Center's Mic Mode panel).  On macOS 27, when an app
// whose stored mode is Voice Isolation makes its first audio call, the
// system's voice processing settings go round every audio process for a
// minute or two, and device lists and mic opens wait on it.  NereusSDR
// never wants voice processing on its mic, so the window's stored mode is
// set to Standard before the window's first audio call, from the mic
// helper, which has no bundle id and so takes no part in that.
//
// This file is the rule alone, over two calls a test can stand in for:
// read our stored mode; if it is anything but Standard, set Standard.
// MacMicMode.mm supplies the system's calls.
//
// Modification history (NereusSDR):
//   2026-10-16: new. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"

#include <QString>

#include <functional>

namespace NereusSDR {

// The stored Mic Mode, numbered as AVCaptureMicrophoneMode numbers it
// (AVFoundation AVCaptureDevice.h, macOS 12 and later: Standard 0,
// WideSpectrum 1, VoiceIsolation 2).
constexpr long kMicModeStandard = 0;
constexpr long kMicModeWideSpectrum = 1;
constexpr long kMicModeVoiceIsolation = 2;

enum class MicModeOutcome {
    AlreadyStandard,   // nothing to do
    MadeStandard,      // it was another mode; it is Standard now
    Refused,           // the system did not take the change
    Unavailable,       // this system has no such setting
    NoBundle,          // no bundle id to keep (never affected)
};

struct MicModeCalls {
    // The stored mode for the bundle.
    std::function<long(const QString& bundleId)> read;
    // Sets the stored mode; true when the system took it.
    std::function<bool(long mode, const QString& bundleId)> write;
};

NEREUS_CORE_EXPORT MicModeOutcome keepMicModeStandard(const QString& bundleId,
                                                      const MicModeCalls& calls);

// One plain line for the log, and the helper's exit code each way.
NEREUS_CORE_EXPORT QString micModeOutcomeText(MicModeOutcome outcome);
NEREUS_CORE_EXPORT int micModeExitCode(MicModeOutcome outcome);
NEREUS_CORE_EXPORT bool micModeOutcomeFromExitCode(int code, MicModeOutcome* outcome);

// The helper's argument: nereus-audio-capture --keep-mic-mode-standard <bundle id>
inline constexpr char kKeepMicModeStandardArg[] = "--keep-mic-mode-standard";

} // namespace NereusSDR
