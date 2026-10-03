#pragma once
// no-port-check: NereusSDR-original. Declarations only; the palette tables
// and their upstream header are in WaterfallPalettes.cpp.
// Independently implemented from WaterfallPalettes.cpp interface.
// =================================================================
// src/core/spectrum/WaterfallPalettes.h  (NereusSDR)
// =================================================================
//
// The waterfall palettes' gradient stops and names, moved from
// gui/SpectrumWidget (iPhone app plan Task 19, R-IOS-06) so the Core's
// catalogue can send them to an app. SpectrumWidget includes this header and
// draws from the same tables, so a phone's waterfall and the desktop's use
// one set of colours.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code. WfGradientStop and wfSchemeStops() moved
//               unchanged from gui/SpectrumWidget.h; wfSchemeName() holds
//               the names Setup > Display's colour scheme list shows.
// =================================================================

#include "core/spectrum/ISpectrumSink.h"  // WfColorScheme

namespace NereusSDR {

// Gradient stop for waterfall color mapping.
struct WfGradientStop { float pos; int r, g, b; };

// Returns gradient stops for a given color scheme.
const WfGradientStop* wfSchemeStops(WfColorScheme scheme, int& count);

// The name Setup > Display's colour scheme list shows for `scheme`, in
// operator words ("Clarity Blue"); empty for WfColorScheme::Count.
const char* wfSchemeName(WfColorScheme scheme);

} // namespace NereusSDR
