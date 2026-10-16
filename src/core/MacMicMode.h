#pragma once

// =================================================================
// src/core/MacMicMode.h  (NereusSDR-native)
// =================================================================
// no-port-check: NereusSDR-original.
//
// The Mic Mode the user chose in Control Center for this app, read through
// AVCaptureDevice's public preferredMicrophoneMode (macOS 12 and later).
// The values are AVCaptureMicrophoneMode's (AVFoundation,
// AVCaptureDevice.h): 0 Standard, 1 Wide Spectrum, 2 Voice Isolation.
// macOS keeps the choice per app; on macOS 27 the system log shows it as
// AUVoiceIOClients/<bundle id>/AUVoiceIOChatFlavor.
//
// Modification history (NereusSDR):
//   2026-10-15  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The read, for the window's voice isolation opt-out.
// =================================================================

#include <QtGlobal>

#include <optional>

namespace NereusSDR {

// AVCaptureMicrophoneMode values (AVCaptureDevice.h, macOS 12 SDK onward).
constexpr int kMicModeStandard = 0;
constexpr int kMicModeWideSpectrum = 1;
constexpr int kMicModeVoiceIsolation = 2;

#ifdef Q_OS_MAC
// Nothing before macOS 12, which has no Mic Mode.
std::optional<int> preferredMicrophoneMode();
#else
inline std::optional<int> preferredMicrophoneMode()
{
    return std::nullopt;
}
#endif

} // namespace NereusSDR
