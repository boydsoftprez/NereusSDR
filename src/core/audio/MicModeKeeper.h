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
// read our stored mode; if it is anything but Standard, set Standard.  It
// also runs the helper for the window, waiting a bounded time, and words
// the window's one log line.  MacMicMode.mm supplies the system's calls
// and the window's launch step.
//
// Modification history (NereusSDR):
//   2026-10-16: new. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"

#include <QByteArray>
#include <QString>

#include <functional>

namespace NereusSDR {

// The stored Mic Mode, numbered as AVCaptureMicrophoneMode numbers it:
// AVCaptureMicrophoneModeStandard = 0, AVCaptureMicrophoneModeWideSpectrum
// = 1, AVCaptureMicrophoneModeVoiceIsolation = 2, macOS 12 and later
// (MacOSX27.0.sdk, AVFoundation.framework/Headers/AVCaptureDevice.h:2722-2726).
constexpr long kMicModeStandard = 0;
constexpr long kMicModeWideSpectrum = 1;
constexpr long kMicModeVoiceIsolation = 2;
// No mode read (the calls are missing, or there is no bundle id).
constexpr long kMicModeNotRead = -1;

enum class MicModeOutcome {
    AlreadyStandard,   // nothing to do
    MadeStandard,      // it was another mode; it is Standard now
    Refused,           // the system did not take the change
    Unavailable,       // this system has no such setting
    NoBundle,          // no bundle id to keep (never affected)
};

struct MicModeResult {
    MicModeOutcome outcome = MicModeOutcome::Unavailable;
    // The mode read before any change; kMicModeNotRead when none was.
    long before = kMicModeNotRead;
};

struct MicModeCalls {
    // The stored mode for the bundle; kMicModeNotRead when the read failed.
    std::function<long(const QString& bundleId)> read;
    // Sets the stored mode; true when the system took it.
    std::function<bool(long mode, const QString& bundleId)> write;
};

NEREUS_CORE_EXPORT MicModeResult keepMicModeStandard(const QString& bundleId,
                                                     const MicModeCalls& calls);

// "Standard", "Wide Spectrum", "Voice Isolation", else "mode N".
NEREUS_CORE_EXPORT QString micModeName(long mode);
// One plain line for the helper's own log.
NEREUS_CORE_EXPORT QString micModeOutcomeText(MicModeOutcome outcome);

// The helper answers with its exit code (the outcome) and one line on
// standard output (the mode it read before).
NEREUS_CORE_EXPORT int micModeExitCode(MicModeOutcome outcome);
NEREUS_CORE_EXPORT bool micModeOutcomeFromExitCode(int code, MicModeOutcome* outcome);
NEREUS_CORE_EXPORT QByteArray micModeBeforeLine(long before);
NEREUS_CORE_EXPORT long micModeBeforeFromOutput(const QByteArray& output);

// The helper's argument: nereus-audio-capture --keep-mic-mode-standard <bundle id>
inline constexpr char kKeepMicModeStandardArg[] = "--keep-mic-mode-standard";

// The helper's side: keeps `bundleId` at Standard through `calls`, writes
// the mode it read before to standard output, and returns the exit code.
NEREUS_CORE_EXPORT int answerMicModeRequest(const QString& bundleId, const MicModeCalls& calls);

// The window's side: runs `helper` once for `bundleId` and waits at most
// `waitMs` for its answer.  A helper that has not answered by then is
// stopped, and the window goes on.
struct MicModeHelperAnswer {
    enum class Kind {
        Answered,    // result holds the helper's answer
        NoAnswer,    // nothing within waitMs; the helper was stopped
        Ended,       // it ended without an answer (a crash, or no start)
    };
    Kind kind = Kind::Ended;
    MicModeResult result;
    qint64 elapsedMs = 0;
};
NEREUS_CORE_EXPORT MicModeHelperAnswer runMicModeHelper(const QString& helper,
                                                        const QString& bundleId, int waitMs);

// The window's one log line for an answer, and whether it is a warning.
// Empty for a Standard mode found Standard (nothing to say).
struct MicModeLogLine {
    QString text;
    bool warning = false;
};
NEREUS_CORE_EXPORT MicModeLogLine micModeLogLine(const MicModeHelperAnswer& answer, int waitMs);

} // namespace NereusSDR
