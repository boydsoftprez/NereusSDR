// =================================================================
// src/core/spectrum/WaterfallPalettes.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// The waterfall palettes' gradient stops, moved unchanged from
// gui/SpectrumWidget.cpp (iPhone app plan Task 19, R-IOS-06) so the Core's
// catalogue can send them to an app; SpectrumWidget draws from them here.
// The Default stops come from AetherSDR (ten9876/AetherSDR, GPLv3, like
// NereusSDR); Enhanced, Spectran and BlackWhite follow Thetis's display.cs
// colour schemes; LinLog, LinRad, the Custom fallback and Clarity Blue are
// NereusSDR's. Each table keeps the comment it had in SpectrumWidget.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 19 (R-IOS-06): moved
//                                    from gui/SpectrumWidget.cpp unchanged,
//                                    with wfSchemeName(). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "core/spectrum/WaterfallPalettes.h"

namespace NereusSDR {

// ---- Default waterfall gradient stops (AetherSDR style) ----
// From AetherSDR SpectrumWidget.cpp:43-51
static const WfGradientStop kDefaultStops[] = {
    {0.00f,   0,   0,   0},    // black
    {0.15f,   0,   0, 128},    // dark blue
    {0.30f,   0,  64, 255},    // blue
    {0.45f,   0, 200, 255},    // cyan
    {0.60f,   0, 220,   0},    // green
    {0.80f, 255, 255,   0},    // yellow
    {1.00f, 255,   0,   0},    // red
};

// Enhanced scheme — from Thetis display.cs:6864-6954 (9-band progression)
static const WfGradientStop kEnhancedStops[] = {
    {0.000f,   0,   0,   0},   // black
    {0.111f,   0,   0, 255},   // blue
    {0.222f,   0, 255, 255},   // cyan
    {0.333f,   0, 255,   0},   // green
    {0.444f, 128, 255,   0},   // yellow-green
    {0.556f, 255, 255,   0},   // yellow
    {0.667f, 255, 128,   0},   // orange
    {0.778f, 255,   0,   0},   // red
    {0.889f, 255,   0, 128},   // red-magenta
    {1.000f, 192,   0, 255},   // purple
};

// Spectran scheme — from Thetis display.cs:6956-7036
static const WfGradientStop kSpectranStops[] = {
    {0.00f,   0,   0,   0},    // black
    {0.10f,  32,   0,  64},    // dark purple
    {0.25f,   0,   0, 255},    // blue
    {0.40f,   0, 192,   0},    // green
    {0.55f, 255, 255,   0},    // yellow
    {0.70f, 255, 128,   0},    // orange
    {0.85f, 255,   0,   0},    // red
    {1.00f, 255, 255, 255},    // white
};

// Black-white scheme — from Thetis display.cs:7038-7075
static const WfGradientStop kBlackWhiteStops[] = {
    {0.00f,   0,   0,   0},    // black
    {1.00f, 255, 255, 255},    // white
};

// LinLog scheme — linear ramp in low region, log-shaped in high region.
// Approximation of Thetis "LinLog" combo entry (setup.cs:11904-11939).
// Phase 3G-8 commit 5 addition.
static const WfGradientStop kLinLogStops[] = {
    {0.00f,   0,   0,   0},
    {0.10f,   0,   0,  96},
    {0.25f,   0,  64, 192},
    {0.50f,   0, 192, 192},
    {0.65f,   0, 224,  64},
    {0.80f, 255, 192,   0},
    {1.00f, 255,   0,   0},
};

// LinRad scheme — LinRadiance-style cool → hot gradient. Phase 3G-8.
static const WfGradientStop kLinRadStops[] = {
    {0.00f,   0,   0,   0},
    {0.15f,  16,  16, 120},
    {0.30f,  32,  80, 200},
    {0.50f,   0, 200, 255},
    {0.70f, 200, 255, 120},
    {0.85f, 255, 200,   0},
    {1.00f, 255,  32,   0},
};

// Custom scheme — loaded from AppSettings "DisplayWfCustomStops" if set,
// otherwise falls back to Default. Phase 3G-8 commit 5. Runtime state
// is parsed lazily from the settings key when the scheme is selected.
// For now the static fallback keeps the same stops as Default.
static const WfGradientStop kCustomFallbackStops[] = {
    {0.00f,   0,   0,   0},
    {0.15f,   0,   0, 128},
    {0.30f,   0,  64, 255},
    {0.45f,   0, 200, 255},
    {0.60f,   0, 220,   0},
    {0.80f, 255, 255,   0},
    {1.00f, 255,   0,   0},
};

// Phase 3G-9b: Clarity Blue palette — full-spectrum rainbow with a
// deep-black noise floor. The "blue look" a user sees most of the time
// comes from the combination of AGC + tight thresholds compressing most
// signals into the blue/cyan range of the palette; strong signals still
// cleanly progress through green → yellow → red so peak energy remains
// distinguishable. Compare to Default/Enhanced which start at dark blue
// and spread the bright colours across the full range (producing a noisy
// noise floor). Visual target: 2026-04-14/2026-04-15 AetherSDR reference.
static const WfGradientStop kClarityBlueStops[] = {
    {0.00f,  0x00, 0x00, 0x00},  // pure black — noise floor bottom
    {0.18f,  0x02, 0x08, 0x20},  // very dark blue — noise floor top
    {0.32f,  0x08, 0x20, 0x58},  // dark blue — weak signal edge
    {0.46f,  0x10, 0x50, 0xb0},  // medium blue — weak signals
    {0.58f,  0x10, 0xa0, 0xe0},  // cyan — medium signals
    {0.70f,  0x10, 0xd0, 0x60},  // green — strong signals
    {0.80f,  0xf0, 0xe0, 0x10},  // yellow — very strong
    {0.90f,  0xff, 0x80, 0x00},  // orange — extreme
    {0.96f,  0xff, 0x20, 0x20},  // red — peak
    {1.00f,  0xff, 0x40, 0xc0},  // magenta — absolute peak
};

const WfGradientStop* wfSchemeStops(WfColorScheme scheme, int& count)
{
    switch (scheme) {
    case WfColorScheme::Enhanced:
        count = 10;
        return kEnhancedStops;
    case WfColorScheme::Spectran:
        count = 8;
        return kSpectranStops;
    case WfColorScheme::BlackWhite:
        count = 2;
        return kBlackWhiteStops;
    case WfColorScheme::LinLog:
        count = 7;
        return kLinLogStops;
    case WfColorScheme::LinRad:
        count = 7;
        return kLinRadStops;
    case WfColorScheme::Custom:
        count = 7;
        return kCustomFallbackStops;
    case WfColorScheme::ClarityBlue:
        count = 10;
        return kClarityBlueStops;
    case WfColorScheme::Default:
    default:
        count = 7;
        return kDefaultStops;
    }
}

const char* wfSchemeName(WfColorScheme scheme)
{
    // The labels Setup > Display > Waterfall's colour scheme list shows, in
    // WfColorScheme order.
    switch (scheme) {
    case WfColorScheme::Default:     return "Default";
    case WfColorScheme::Enhanced:    return "Enhanced";
    case WfColorScheme::Spectran:    return "Spectran";
    case WfColorScheme::BlackWhite:  return "BlackWhite";
    case WfColorScheme::LinLog:      return "LinLog";
    case WfColorScheme::LinRad:      return "LinRad";
    case WfColorScheme::Custom:      return "Custom";
    case WfColorScheme::ClarityBlue: return "Clarity Blue";   // Phase 3G-9b
    case WfColorScheme::Count:       break;
    }
    return "";
}

} // namespace NereusSDR
