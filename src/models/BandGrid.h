// =================================================================
// src/models/BandGrid.h  (NereusSDR)
// =================================================================
//
// The desktop's per-pan BAND grid, as one table: the flyout on each
// panadapter's overlay strip (SpectrumOverlayPanel::buildBandFlyout) draws
// its buttons from it, and the Core's catalogue lists the same bands, in
// the same order, as its `bands` key (StationCatalog), so an app's band
// grid and the desktop's cannot drift apart.
//
// Band table from AetherSDR src/gui/SpectrumOverlayMenu.cpp, reduced to
// HF + WWV (moved here verbatim from SpectrumOverlayPanel.cpp).
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - The band table moved here from SpectrumOverlayPanel.cpp
//                (where it was ported from AetherSDR
//                src/gui/SpectrumOverlayMenu.cpp on 2026-04-16), each row
//                given its Band, so the Core's catalogue lists the grid the
//                desktop draws (R-IOS-27, R-IOS-06). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m row, after 6 m as Thetis orders its bands (B6M, B2M,
//                WWV), which gives the grid a fourth row (R-IOS-26,
//                R-R3-49). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

#pragma once

#include "models/Band.h"

#include <cstddef>

namespace NereusSDR {

struct BandGridEntry {
    Band        band;    // what a click on the button selects
    const char* label;   // the button's text
    const char* name;    // bandSelected()'s name argument
    double      freqHz;  // bandSelected()'s legacy frequency argument
    const char* mode;    // bandSelected()'s legacy mode argument
};

// Frequencies in Hz (task spec: 1.8e6, 3.5e6, etc.)
// In the grid's order: four to a row, 160 to 40, 30 to 15, 12 to 2, WWV.
inline constexpr BandGridEntry kBandGrid[] = {
    {Band::Band160m, "160", "160m",  1.8e6,    "LSB"},
    {Band::Band80m,  "80",  "80m",   3.5e6,    "LSB"},
    {Band::Band60m,  "60",  "60m",   5.3e6,    "USB"},
    {Band::Band40m,  "40",  "40m",   7.0e6,    "LSB"},
    {Band::Band30m,  "30",  "30m",  10.1e6,    "DIGU"},
    {Band::Band20m,  "20",  "20m",  14.0e6,    "USB"},
    {Band::Band17m,  "17",  "17m",  18.068e6,  "USB"},
    {Band::Band15m,  "15",  "15m",  21.0e6,    "USB"},
    {Band::Band12m,  "12",  "12m",  24.89e6,   "USB"},
    {Band::Band10m,  "10",  "10m",  28.0e6,    "USB"},
    {Band::Band6m,   "6",   "6m",   50.0e6,    "USB"},
    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:266 [@1e0718ad]:
    //   {"2",    "2m",  144.200,  "USB"},   // 17 — FLEX-6700
    {Band::Band2m,   "2",   "2m",  144.2e6,    "USB"},
    {Band::WWV,      "WWV", "WWV",  10.0e6,    "AM"},
};

inline constexpr int kBandGridCount =
    static_cast<int>(sizeof(kBandGrid) / sizeof(kBandGrid[0]));

/// The grid's row for `band`, or nullptr for a band the grid has no button
/// for (GEN, XVTR, the SWL bands). A Core's catalogue lists the 2 m row
/// only to a peer that knows 2 m (BandLinkFit.h).
inline const BandGridEntry* bandGridEntry(Band band)
{
    for (const BandGridEntry& entry : kBandGrid) {
        if (entry.band == band) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace NereusSDR
