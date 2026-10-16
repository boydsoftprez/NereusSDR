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
// Only the window does this. The mic helper and nereusd have no bundle
// identifier, and macOS offers voice isolation only to a process that has
// one. The call itself can take seconds (it is the process's first Core
// Audio call, and pays for Core Audio starting up), so it runs on a thread
// of its own: startSystemVoiceIsolationOptOut() first thing in the window's
// main(), and waitForSystemVoiceIsolationOptOut() before the first device
// walk, so no walk runs ahead of it.
//
// Modification history (NereusSDR):
//   2026-10-10  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The opt-out, made first thing in main() of every program.
//   2026-10-15  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The window only, on a thread of its own, with the device
//               walk waiting for it; nothing in a test run.
// =================================================================

#include <QtGlobal>

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
    Applied,       // Core Audio accepted the opt-out
    Refused        // Core Audio has the switch and returned an error
};

// What the started opt-out did, for the log.
struct VoiceIsolationOptOutReport {
    VoiceIsolationOptOut outcome = VoiceIsolationOptOut::NotApplicable;
    std::int32_t status = 0;  // the OSStatus Core Audio answered with
    qint64 callMs = 0;        // from the thread's start to the call's return
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

// The window's opt-out, on a thread of its own. A second call does nothing.
void startSystemVoiceIsolationOptOut();

// Returns once the opt-out started above has returned, with what it did;
// the first caller to see it done also logs it. Returns at once, with
// nothing, in a process that never started one (nereusd, the mic helper,
// tests).
std::optional<VoiceIsolationOptOutReport> waitForSystemVoiceIsolationOptOut();

// Test seams: start with `optOut` in place of the Core Audio call, and
// forget a finished run so the next test starts clean.
void startSystemVoiceIsolationOptOutWith(std::function<VoiceIsolationOptOut(std::int32_t*)> optOut);
void resetSystemVoiceIsolationOptOutForTest();
#else
inline void startSystemVoiceIsolationOptOut() {}
inline std::optional<VoiceIsolationOptOutReport> waitForSystemVoiceIsolationOptOut()
{
    return std::nullopt;
}
#endif

} // namespace NereusSDR
