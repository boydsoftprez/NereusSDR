// =================================================================
// src/core/MacMicMode.mm  (NereusSDR-native)
// =================================================================
// no-port-check: NereusSDR-original.
// See MacMicMode.h for what this is for.
//
// Modification history (NereusSDR):
//   2026-10-15  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The read, for the window's voice isolation opt-out.
// =================================================================
#import <AVFoundation/AVFoundation.h>

#include "core/MacMicMode.h"

namespace NereusSDR {

std::optional<int> preferredMicrophoneMode()
{
    if (@available(macOS 12.0, *)) {
        return static_cast<int>(AVCaptureDevice.preferredMicrophoneMode);
    }
    return std::nullopt;
}

} // namespace NereusSDR
