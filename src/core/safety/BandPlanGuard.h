// =================================================================
// src/core/safety/BandPlanGuard.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.13 @501e3f5]:
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/clsBandStackManager.cs
//   Project Files/Source/Console/setup.designer.cs
//
// Original licences from each Thetis source file are included below,
// verbatim, separated by // --- From [filename] --- markers per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — Ported to C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
// =================================================================

// --- From console.cs ---
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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
//
// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// --- From clsBandStackManager.cs ---
/*  clsBandStackManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

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

// --- From setup.designer.cs ---
// (Auto-generated Visual Studio designer file; no upstream copyright header present.
//  Cite stamp v2.10.3.13 @501e3f5 traces back to that designer file in the Thetis repo.
//  The comboFRSRegion 24-region list (Region enum below) is sourced from
//  setup.designer.cs:8084-8108 [v2.10.3.13].)

#pragma once

#include <cstdint>
#include "models/Band.h"
#include "core/WdspTypes.h"

namespace NereusSDR::safety {

/// IARU + per-country region selector.
/// 24 entries match Thetis comboFRSRegion
/// (setup.designer.cs:8084-8108 [v2.10.3.13]).
enum class Region : std::uint8_t {
    Australia,
    Europe,
    India,
    Italy,
    Israel,
    Japan,
    Spain,
    UnitedKingdom,
    UnitedStates,
    Norway,
    Denmark,
    Sweden,
    Latvia,
    Slovakia,
    Bulgaria,
    Greece,
    Hungary,
    Netherlands,
    France,
    Russia,
    Region1,
    Region2,
    Region3,
    Germany,
};

/// TX band-plan guard. Compiled C++ constexpr tables. Ports:
///   clsBandStackManager.cs:1063-1083 (IsOKToTX)
///   console.cs:6770-6816 (CheckValidTXFreq + checkValidTXFreq_local)
///   console.cs:2643-2669 (Init60mChannels)
///   console.cs:29401-29432 (_preventTXonDifferentBandToRXband + US 60m mode restriction)
/// All from Thetis [v2.10.3.13].
///
/// Pure data + pure functions. No Qt parent, no signals. Inert until
/// 3M-1a wires the first MOX byte — at which point RadioModel calls
/// these predicates before setting TX.
class BandPlanGuard
{
public:
    /// Returns true iff transmitting at \p freqHz in \p mode is allowed
    /// for \p region. Honors 60m channelization, US 60m mode restriction,
    /// and per-region band edges. \p extended=true bypasses all guards
    /// (matches chkExtended — console.cs:6772 [v2.10.3.13]).
    bool isValidTxFreq(Region region, std::int64_t freqHz,
                       DSPMode mode, bool extended) const noexcept;

    /// Returns true iff TX-band == RX-band, OR \p preventDifferentBand
    /// is false. Mirrors _preventTXonDifferentBandToRXband check at
    /// console.cs:29401-29414 [2.9.0.7]MW0LGE.
    bool isValidTxBand(Band rxBand, Band txBand,
                       bool preventDifferentBand) const noexcept;
};

} // namespace NereusSDR::safety
