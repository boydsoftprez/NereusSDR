// =================================================================
// src/core/accessories/N2adrPreset.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/setup.cs:chkHERCULES_CheckedChanged
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4, lines 14311-14424 — HERMESLITE
//   branch lines 14324-14368)
//
// See N2adrPreset.h for the full design + scope rationale.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Extracted from Hl2IoBoardTab.cpp:950-995 + RadioModel.cpp
//                :1077-1111 to centralise the per-band write table.
//                Adds 13 SWL bands × pin-7 RX entries previously missing.
//                J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-46: applyN2adrPresetReceiveOnly, the preset's
//                receive half for a station that may not change transmit
//                settings. NereusSDR-original. J.J. Boyd (KG4VCF), AI-
//                assisted via Anthropic Claude Code.
// =================================================================
//
//=================================================================
// setup.cs (mi0bot fork)
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "N2adrPreset.h"

#include "core/OcMatrix.h"
#include "models/Band.h"

#include <initializer_list>

namespace NereusSDR {

void applyN2adrPreset(OcMatrix& oc, bool enabled)
{
    // Step 1 — wipe every cell on every band/pin/{rx,tx}.  mi0bot does this
    // unconditionally before populating (setup.cs:14315-14322 case true and
    // :14414-14422 case false [v2.10.3.13-beta2]).
    for (int b = 0; b < int(Band::Count); ++b) {
        for (int pin = 0; pin < 7; ++pin) {
            oc.setPin(static_cast<Band>(b), pin, /*tx=*/false, false);
            oc.setPin(static_cast<Band>(b), pin, /*tx=*/true,  false);
        }
    }

    if (!enabled) { return; }

    // Step 2a — populate the 10 ham band pattern.
    // Per-band OC bit table decoded from mi0bot setup.cs:14326-14344 (RX) +
    // :14345-14354 (TX) [v2.10.3.13-beta2], with the
    // `chkPenOC<rcv|xmit><band><pin>` naming convention where pin 1 → bit 0,
    // pin 7 → bit 6 (per setup.cs:12908).
    //
    //   Band  RX pins (bits)        TX pins (bits)
    //   160m  pin 1 (bit 0)         pin 1 (bit 0)
    //    80m  pin 2 + pin 7 (1,6)   pin 2 (bit 1)
    //    60m  pin 3 + pin 7 (2,6)   pin 3 (bit 2)
    //    40m  pin 3 + pin 7 (2,6)   pin 3 (bit 2)
    //    30m  pin 4 + pin 7 (3,6)   pin 4 (bit 3)
    //    20m  pin 4 + pin 7 (3,6)   pin 4 (bit 3)
    //    17m  pin 5 + pin 7 (4,6)   pin 5 (bit 4)
    //    15m  pin 5 + pin 7 (4,6)   pin 5 (bit 4)
    //    12m  pin 6 + pin 7 (5,6)   pin 6 (bit 5)
    //    10m  pin 6 + pin 7 (5,6)   pin 6 (bit 5)
    //
    // Pin 7 (bit 6) is the "RX active" relay (asserted on RX above 160m,
    // never on TX).  Pin pairs 60/40, 30/20, 17/15, 12/10 share LPFs
    // (standard pairing).
    auto setBand = [&](Band band, std::initializer_list<int> rxBits,
                       std::initializer_list<int> txBits) {
        for (int b : rxBits) { oc.setPin(band, b, /*tx=*/false, true); }
        for (int b : txBits) { oc.setPin(band, b, /*tx=*/true,  true); }
    };
    setBand(Band::Band160m, {0},     {0});
    setBand(Band::Band80m,  {1, 6},  {1});
    setBand(Band::Band60m,  {2, 6},  {2});
    setBand(Band::Band40m,  {2, 6},  {2});
    setBand(Band::Band30m,  {3, 6},  {3});
    setBand(Band::Band20m,  {3, 6},  {3});
    setBand(Band::Band17m,  {4, 6},  {4});
    setBand(Band::Band15m,  {4, 6},  {4});
    setBand(Band::Band12m,  {5, 6},  {5});
    setBand(Band::Band10m,  {5, 6},  {5});

    // Step 2b — populate the 13 SWL bands × pin-7 RX entries.
    // From mi0bot setup.cs:14346-14358 [v2.10.3.13-beta2]:
    //   chkOCrcv1207..chkOCrcv117 (120m, 90m, 61m, 49m, 41m, 31m, 25m,
    //                              22m, 19m, 16m, 14m, 13m, 11m), pin 7 RX.
    //
    // No TX entries — SWL bands are receive-only on the N2ADR Filter board.
    // This is what closes the 30% N2ADR-on visibility gap that motivated
    // Phase 3L (without it, OcOutputsSwlTab would always render all-blank
    // even after the user enables the N2ADR master toggle).
    for (int idx = static_cast<int>(Band::SwlFirst);
         idx <= static_cast<int>(Band::SwlLast); ++idx) {
        oc.setPin(static_cast<Band>(idx), /*pin=*/6, /*tx=*/false, true);
    }
}

// R-R3-46: NereusSDR-original. Keep every transmit pin, apply the rest.
void applyN2adrPresetReceiveOnly(OcMatrix& oc, bool enabled)
{
    bool tx[int(Band::Count)][7] = {};
    for (int b = 0; b < int(Band::Count); ++b) {
        for (int pin = 0; pin < 7; ++pin) {
            tx[b][pin] = oc.pinEnabled(static_cast<Band>(b), pin, /*tx=*/true);
        }
    }
    applyN2adrPreset(oc, enabled);
    for (int b = 0; b < int(Band::Count); ++b) {
        for (int pin = 0; pin < 7; ++pin) {
            oc.setPin(static_cast<Band>(b), pin, /*tx=*/true, tx[b][pin]);
        }
    }
}

} // namespace NereusSDR
