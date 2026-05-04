// =================================================================
// src/core/HpsdrModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.h, original licence from Thetis source is included below
//
// =================================================================
// Additional copyright holders whose code is preserved in this file via
// inline markers (upstream file-header blocks do not name them):
//   Laurence Barker (G8NJJ) — ANAN-G2 / Saturn hardware support (preserved
//     via inline markers on HPSDRHW::Saturn enum and HPSDRModel::ANAN_G2/ANAN_G2_1K)
//   Reid Campbell (MI0BOT) — HermesLite 2 enum mappings (preserved via
//     inline markers on HPSDRModel::HERMESLITE and HPSDRHW::HermesLite)
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

/*  network.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015-2020 Doug Wigley, W5WC

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#pragma once

#include <QMetaType>

namespace NereusSDR {

// Logical radio model — what the user says they have in Setup.
// Source: enums.cs:109
enum class HPSDRModel : int {
    FIRST        = -1,
    HPSDR        =  0,  // Atlas/Metis kit
    HERMES       =  1,
    ANAN10       =  2,
    ANAN10E      =  3,
    ANAN100      =  4,
    ANAN100B     =  5,
    ANAN100D     =  6,
    ANAN200D     =  7,
    ORIONMKII    =  8,
    ANAN7000D    =  9,
    ANAN8000D    = 10,
    ANAN_G2      = 11,  // G8NJJ [Thetis enums.cs:125]
    ANAN_G2_1K   = 12,  // G8NJJ [Thetis enums.cs:126]
    ANVELINAPRO3 = 13,
    HERMESLITE   = 14,  // MI0BOT [Thetis enums.cs:128]
    REDPITAYA    = 15,  // DH1KLM contribution — enum slot preserved, impl deferred
    LAST         = 16
};

// Physical board — what's actually on the wire.
// Source: enums.cs:388 + ChannelMaster/network.h:446
enum class HPSDRHW : int {
    Atlas      =   0,  // HPSDR kit (aka Metis in PowerSDR)
    Hermes     =   1,  // ANAN-10 / ANAN-100
    HermesII   =   2,  // ANAN-10E / ANAN-100B
    Angelia    =   3,  // ANAN-100D
    Orion      =   4,  // ANAN-200D
    OrionMKII  =   5,  // ANAN-7000DLE / 8000DLE / AnvelinaPro3
    HermesLite =   6,  // Hermes Lite 2 — MI0BOT: HL2 allocated number [Thetis network.h:422 / enums.cs:396]
    // 7..9 reserved — DO NOT REUSE (Thetis wire format compares these ints)
    Saturn     =  10,  // ANAN-G2: added G8NJJ [Thetis network.h:423 / enums.cs:397]
    SaturnMKII =  11,  // ANAN-G2 MkII board revision
    // NereusSDR-original SKU slots — NOT on the Thetis wire; integers chosen
    // above the Thetis-defined range (0-11) and below Unknown(999).
    HermesLiteRxOnly = 12, // HL2 RX-only kit (no TX driver). Phase 3M-0.
    // 13..19 available for future NereusSDR-original SKU slots.
    Andromeda        = 20, // Andromeda console (Ganymede PA trip). Phase 3M-0.
    Unknown    = 999
};

constexpr HPSDRHW boardForModel(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:        return HPSDRHW::Atlas;
        case HPSDRModel::HERMES:       return HPSDRHW::Hermes;
        case HPSDRModel::ANAN10:       return HPSDRHW::Hermes;
        case HPSDRModel::ANAN10E:      return HPSDRHW::HermesII;
        case HPSDRModel::ANAN100:      return HPSDRHW::Hermes;
        case HPSDRModel::ANAN100B:     return HPSDRHW::HermesII;
        case HPSDRModel::ANAN100D:     return HPSDRHW::Angelia;
        case HPSDRModel::ANAN200D:     return HPSDRHW::Orion;
        case HPSDRModel::ORIONMKII:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN7000D:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN8000D:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN_G2:      return HPSDRHW::Saturn;
        case HPSDRModel::ANAN_G2_1K:   return HPSDRHW::Saturn;
        case HPSDRModel::ANVELINAPRO3: return HPSDRHW::OrionMKII;
        case HPSDRModel::HERMESLITE:   return HPSDRHW::HermesLite;
        case HPSDRModel::REDPITAYA:    return HPSDRHW::OrionMKII;
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return HPSDRHW::Unknown;
    }
    return HPSDRHW::Unknown;
}

constexpr const char* displayName(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:        return "HPSDR (Atlas/Metis)";
        case HPSDRModel::HERMES:       return "Hermes";
        case HPSDRModel::ANAN10:       return "ANAN-10";
        case HPSDRModel::ANAN10E:      return "ANAN-10E";
        case HPSDRModel::ANAN100:      return "ANAN-100";
        case HPSDRModel::ANAN100B:     return "ANAN-100B";
        case HPSDRModel::ANAN100D:     return "ANAN-100D";
        case HPSDRModel::ANAN200D:     return "ANAN-200D";
        case HPSDRModel::ORIONMKII:    return "Orion MkII";
        case HPSDRModel::ANAN7000D:    return "ANAN-7000DLE";
        case HPSDRModel::ANAN8000D:    return "ANAN-8000DLE";
        case HPSDRModel::ANAN_G2:      return "ANAN-G2";
        case HPSDRModel::ANAN_G2_1K:   return "ANAN-G2 1K";
        case HPSDRModel::ANVELINAPRO3: return "Anvelina Pro 3";
        case HPSDRModel::HERMESLITE:   return "Hermes Lite 2";
        case HPSDRModel::REDPITAYA:    return "Red Pitaya";
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return "Unknown";
    }
    return "Unknown";
}

// paMaxWattsFor — per-SKU PA output ceiling, used to scale forward-power
// meter ranges so a 5 W QRP HL2 doesn't share the 0-200 W ANAN-8000DLE
// scale.  Returns the manufacturer's spec'd PA maximum, NOT a software
// clamp (the math kernel never enforces a ceiling).  Meter widgets use
// this to set their bar / gauge ranges and red-zone thresholds.
//
// Sourced from the per-SKU max-watts table in the v0.3.2 PA-cal hotfix
// design doc (docs/architecture/pa-calibration-hotfix.md §9 test matrix
// + plan glowing-snacking-mochi.md Phase 3B safety-ceiling matrix).
constexpr int paMaxWattsFor(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:
        case HPSDRModel::HERMES:
        case HPSDRModel::ANAN10:
        case HPSDRModel::ANAN10E:
        case HPSDRModel::REDPITAYA:    return  10;
        case HPSDRModel::ANAN100:
        case HPSDRModel::ANAN100B:
        case HPSDRModel::ANAN_G2:      return 100;
        case HPSDRModel::ANAN100D:
        case HPSDRModel::ANAN200D:
        case HPSDRModel::ORIONMKII:
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ANVELINAPRO3: return 200;
        case HPSDRModel::ANAN_G2_1K:   return 1000;
        case HPSDRModel::HERMESLITE:   return   5;
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return 100;   // sentinel default
    }
    return 100;
}

// boardCodeName — returns the HPSDRHW enum label as a short model-code string.
// Used in the status-bar board widget to show "Saturn" instead of the full
// marketing name "ANAN-G2 (Saturn)" which truncates at typical status-bar widths.
constexpr const char* boardCodeName(HPSDRHW hw) noexcept {
    switch (hw) {
        case HPSDRHW::Atlas:            return "Atlas";
        case HPSDRHW::Hermes:           return "Hermes";
        case HPSDRHW::HermesII:         return "HermesII";
        case HPSDRHW::Angelia:          return "Angelia";
        case HPSDRHW::Orion:            return "Orion";
        case HPSDRHW::OrionMKII:        return "OrionMKII";
        case HPSDRHW::HermesLite:       return "HL2";
        case HPSDRHW::Saturn:           return "Saturn";
        case HPSDRHW::SaturnMKII:       return "SaturnMKII";
        case HPSDRHW::HermesLiteRxOnly: return "HL2-RX";
        case HPSDRHW::Andromeda:        return "Andromeda";
        case HPSDRHW::Unknown:          return "Unknown";
    }
    return "Unknown";
}

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::HPSDRModel)
Q_DECLARE_METATYPE(NereusSDR::HPSDRHW)
