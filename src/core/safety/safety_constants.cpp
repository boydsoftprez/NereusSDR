// =================================================================
// src/core/safety/safety_constants.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.13 @501e3f5]:
//   Project Files/Source/Console/console.cs
//
// Original licence from the Thetis source file is included below,
// verbatim, with // --- From [filename] --- marker per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — Ported to C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
//                Task: Phase 3M-0 Task 7A — per-board PA scaling table.
//                Ports computeAlexFwdPower per-board switch
//                (console.cs:25008-25072 [v2.10.3.13]).
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

#include "core/safety/safety_constants.h"

namespace NereusSDR::safety {

// Ports computeAlexFwdPower per-board switch (console.cs:25008-25017 [v2.10.3.13]).
// Per-group cites below reference each case block's upstream lines.
PaScalingTriplet paScalingFor(HPSDRModel model) noexcept
{
    switch (model) {
    // From Thetis console.cs:25018-25028 [v2.10.3.13]
    case HPSDRModel::ANAN100:
    case HPSDRModel::ANAN100B:
    case HPSDRModel::ANAN100D:
        return { 0.095f, 3.3f, 6 };
    // From Thetis console.cs:25029-25033 [v2.10.3.13]
    case HPSDRModel::ANAN200D:
        return { 0.108f, 5.0f, 4 };
    // From Thetis console.cs:25034-25042 [v2.10.3.13]
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:             // !K will need different scaling
    case HPSDRModel::REDPITAYA: //DH1KLM
        return { 0.12f, 5.0f, 32 };
    // From Thetis console.cs:25043-25048 [v2.10.3.13]
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN8000D:
        return { 0.08f, 5.0f, 18 };
    // From Thetis console.cs:25049-25053 [v2.10.3.13]
    default:
        return { 0.09f, 3.3f, 6 };
    }
}

// From Thetis console.cs:25056-25071 [v2.10.3.13] (computeAlexFwdPower
// post-switch computation). Clamps volts and watts to zero (matches Thetis
// `if (volts < 0) volts = 0; ... if (watts < 0) watts = 0;`).
// Note: this function takes pre-fetched ADC counts rather than calling
// NetworkIO.getFwdPower() directly — the caller (RadioStatus / TxChannel)
// owns the hardware interface.
float computeAlexFwdPower(HPSDRModel model, int adcCounts) noexcept
{
    const PaScalingTriplet t = paScalingFor(model);
    float volts = static_cast<float>(adcCounts - t.adcCalOffset) /
                  4095.0f * t.refVoltage;
    if (volts < 0.0f) { volts = 0.0f; }
    float watts = (volts * volts) / t.bridgeVolt;
    if (watts < 0.0f) { watts = 0.0f; }
    return watts;
}

} // namespace NereusSDR::safety
