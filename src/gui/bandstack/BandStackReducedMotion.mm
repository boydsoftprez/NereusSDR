// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/bandstack/BandStackReducedMotion.mm  (NereusSDR)
// =================================================================
//
// macOS's reduced motion setting, for BandStackPanel's slide.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "gui/bandstack/BandStackPanel.h"

#import <AppKit/AppKit.h>

namespace NereusSDR {

bool systemPrefersReducedMotion()
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
}

} // namespace NereusSDR
