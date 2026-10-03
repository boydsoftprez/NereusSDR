#pragma once

// Request microphone permission on macOS at app startup.
//
// Hardened-Runtime + Developer-ID signed apps with NSMicrophoneUsageDescription
// in Info.plist will normally surface the system permission dialog the first
// time something opens an input audio unit. In practice that's unreliable for
// us: AudioEngine::ensureTxInputOpen() drives Pa_OpenStream through PortAudio's
// CoreAudio backend, and the device resolver silently returns paNoDevice on
// machines that lack a built-in mic (Mac mini / Mac Studio) or whose only
// inputs are audio interfaces with names that don't match the hardware-mic
// regex. When that happens, AVCaptureDevice never sees an access attempt and
// macOS has no reason to show the prompt.
//
// Calling AVCaptureDevice's requestAccess(forMediaType:AVMediaTypeAudio) at
// app launch deterministically engages TCC regardless of what audio hardware
// is attached, so the user gets the prompt on first launch and the answer is
// cached for every subsequent mic open. No-op on non-macOS platforms.
//
// microphonePermissionStatus / requestMicrophonePermissionAndWait (added
// 2026-09-22 for the nereus-audio-capture helper, R-R3-36): the helper
// reports a pending OS consent prompt as its own phase, so it needs the
// current answer without prompting and a request that blocks the calling
// thread until the user answers. Restricted maps to Denied. On other
// platforms both return Granted without doing anything.
#include <QtGlobal>

namespace NereusSDR {
enum class MicPermission { Granted, Denied, Undetermined };
}

#ifdef Q_OS_MAC
namespace NereusSDR {
void requestMicrophonePermission();
MicPermission microphonePermissionStatus();
MicPermission requestMicrophonePermissionAndWait(); // blocks the calling thread until the user answers
}
#else
namespace NereusSDR {
inline void requestMicrophonePermission() {}
inline MicPermission microphonePermissionStatus() { return MicPermission::Granted; }
inline MicPermission requestMicrophonePermissionAndWait() { return MicPermission::Granted; }
}
#endif
