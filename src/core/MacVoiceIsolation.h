#pragma once

// =================================================================
// src/core/MacVoiceIsolation.h  (NereusSDR-native)
// =================================================================
// no-port-check: NereusSDR-original.
//
// Opt the NereusSDR window out of macOS's system voice isolation.
//
// macOS 27's Core Audio client library runs "HAL Voice Isolation support"
// inside every process that uses audio. When the app's Mic Mode in Control
// Center is Voice Isolation, it sets a voice-processing type on every audio
// device, each device answers with a configuration change, and each change
// makes it set them all again. From the window's first Core Audio call that
// loop runs 18 to 21 rounds over 71 to 92 s, and while it runs every new
// Core Audio client takes seconds to start and no device list finishes, so
// the mic helper's open misses its 10 s deadline.
//
// NereusSDR does its own transmit audio processing, and a speech isolator
// tuned for calls is the wrong tool in front of an SSB transmit chain, so
// the window declines the system processing through Core Audio's own
// per-client switch.
//
// Only when the app's Mic Mode is Voice Isolation (MacMicMode.h). Every
// loop seen came from a Voice Isolation window, and the call can take
// seconds, so a window in Standard or Wide Spectrum neither makes it nor
// waits for it. The mode is read once, at launch; a change made while the
// window runs is read at the next launch.
//
// Only the window does this. The mic helper and nereusd have no bundle
// identifier, and macOS offers voice isolation only to a process that has
// one. The call itself can take seconds (it is the process's first Core
// Audio call, and pays for Core Audio starting up), so it runs on a thread
// of its own: startSystemVoiceIsolationOptOut() in the window's main() once
// the command line is read, and waitForSystemVoiceIsolationOptOut() at the
// top of every Core Audio device list (MacCoreAudioSystem::devices()).
//
// Only that device list waits, and only for as long as the Mic Mode read
// when the mode is not Voice Isolation. These can still reach Core Audio while the
// call runs, and start the loop ahead of it: the VAX channel's device lookup
// (CoreAudioHalBus, findDeviceByUid, on the thread opening the channel),
// the native output and input buses' AudioComponentInstanceNew, and
// PortAudio. Gating them would hold the GUI thread for the call. They
// belong to native audio, which measures the VAX lookup on the launch log
// and owns the fix if it starts the loop.
//
// The wait gives up after kVoiceIsolationOptOutWaitLimit: a call that never
// returns must not leave the device list empty for the whole session.
//
// Modification history (NereusSDR):
//   2026-10-10  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The opt-out, made first thing in main() of every program.
//   2026-10-15  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The window only, on a thread of its own, with the device
//               walk waiting for it up to a limit; only when the app's Mic
//               Mode is Voice Isolation; nothing in a test run.
// =================================================================

#include <QtGlobal>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>

#ifdef Q_OS_MAC
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace NereusSDR {

enum class VoiceIsolationOptOut {
    NotApplicable, // not macOS, a test run, or this macOS has no such switch
                   // (no Mic Mode, the property unknown, or the operation
                   // unsupported)
    NotNeeded,     // the app's Mic Mode is not Voice Isolation; no call made
    Applied,       // Core Audio accepted the opt-out
    Refused        // Core Audio has the switch and returned an error
};

// How long the device list waits for the opt-out before it lists without
// it. Above the slowest call seen at launch (7 to 13 s on test builds with
// the audio service idle), and well short of the 71 to 92 s loop.
constexpr std::chrono::milliseconds kVoiceIsolationOptOutWaitLimit{30000};

// What the started opt-out did, for the log.
struct VoiceIsolationOptOutReport {
    VoiceIsolationOptOut outcome = VoiceIsolationOptOut::NotApplicable;
    std::int32_t status = 0;  // the OSStatus Core Audio answered with
    qint64 callMs = 0;        // from the thread's start to its answer
    std::optional<int> micMode; // what the Mic Mode read returned
};

#ifdef Q_OS_MAC
VoiceIsolationOptOut optOutOfSystemVoiceIsolation(std::int32_t* status = nullptr);

// Test seam: the same request, sent through `setProperty` instead of
// AudioObjectSetPropertyData, for `bundleId` (null when the process has no
// bundle identifier).
using AudioObjectSetPropertyFn = OSStatus (*)(AudioObjectID objectId,
                                              const AudioObjectPropertyAddress* address,
                                              UInt32 qualifierSize,
                                              const void* qualifier,
                                              UInt32 dataSize,
                                              const void* data);
VoiceIsolationOptOut optOutOfSystemVoiceIsolationWith(AudioObjectSetPropertyFn setProperty,
                                                      CFStringRef bundleId,
                                                      std::int32_t* status = nullptr);

// The window's opt-out, on a thread of its own: reads the app's Mic Mode,
// and opts out only when it is Voice Isolation. A second call does nothing.
void startSystemVoiceIsolationOptOut();

// Returns once the opt-out started above has returned, with what it did;
// the first caller to see it done also logs it. Gives up after `limit`,
// with one warning and nothing; from then on returns at once, a waiter
// already blocked included, with the answer once it has come and nothing
// before (the opt-out's thread logs a
// late answer itself). Returns at once, with nothing, in a process that
// never started one (nereusd, the mic helper, tests).
std::optional<VoiceIsolationOptOutReport> waitForSystemVoiceIsolationOptOut(
    std::chrono::milliseconds limit = kVoiceIsolationOptOutWaitLimit);

// Test seams: start with `readMicMode` in place of the Mic Mode read and
// `optOut` in place of the Core Audio call, and forget a finished run so
// the next test starts clean.
void startSystemVoiceIsolationOptOutWith(std::function<std::optional<int>()> readMicMode,
                                         std::function<VoiceIsolationOptOut(std::int32_t*)> optOut);
void resetSystemVoiceIsolationOptOutForTest();
#else
inline void startSystemVoiceIsolationOptOut() {}
inline std::optional<VoiceIsolationOptOutReport> waitForSystemVoiceIsolationOptOut(
    std::chrono::milliseconds = kVoiceIsolationOptOutWaitLimit)
{
    return std::nullopt;
}
#endif

} // namespace NereusSDR
