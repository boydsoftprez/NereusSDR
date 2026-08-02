// --- From radio.cs ---
//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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
//
// =================================================================
// src/core/dsp/Notch.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis Project Files/Source/Console/radio.cs [v2.10.3.15]
// (commit 3759d096), class MNotch at radio.cs:4328-4360.
//
// Modification history (NereusSDR):
//   2026-07-29  J.J. Boyd / KG4VCF  TNF Task 2. The three-field shape
//                 (FCenter / FWidth / Active) is carried over from
//                 MNotch as centerHz / widthHz / active. MNotch's logic
//                 is deliberately NOT ported: Parse / ToString exist to
//                 fit Thetis's key-value database and NereusSDR persists
//                 flat AppSettings keys instead (design section 5.5),
//                 and CompareTo has no NereusSDR caller. The `id` field
//                 has no Thetis counterpart; Thetis identifies a notch
//                 by its position in MNotchDB, which is why every Thetis
//                 mutation loses the operator's selection and recovers
//                 it by searching for matching field values (design
//                 section 5.2). Default width 200 Hz is the panadapter
//                 width from console.cs:40268 [v2.10.3.15]; the
//                 authoritative named constant lives on NotchModel.
//                 AI-assisted transformation via Anthropic Claude Code.
// =================================================================

#pragma once

namespace NereusSDR {

/// One manual notch, in absolute RF Hz.
///
/// Handed between NotchModel (the owner), RxChannel (the WDSP fan-out) and
/// SpectrumWidget (the marker layer). It lives in core/ rather than models/ so
/// RxChannel can take it by const reference without core acquiring a dependency
/// on models; same placement rationale as dsp/ChannelConfig.h.
///
/// **Field-set provenance.** centerHz / widthHz / active correspond to the three
/// fields of Thetis's MNotch class in radio.cs (FCenter / FWidth / Active). None
/// of MNotch's logic is carried over: its Parse / ToString round-trip exists to
/// fit Thetis's key-value database and NereusSDR persists flat AppSettings keys
/// instead, and its CompareTo has no NereusSDR caller. `id` has no Thetis
/// counterpart at all; Thetis identifies a notch by its position in MNotchDB,
/// which is why every Thetis mutation loses the operator's selection and has to
/// recover it by searching for matching field values.
///
/// **Default width.** 200 Hz is the width Thetis gives a notch created from the
/// panadapter. The authoritative named constant lives on NotchModel; the
/// initialiser here mirrors it so a default-constructed Notch is usable.
///
/// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
/// section 5.1.
struct Notch {
    int    id{0};           ///< stable, monotonic; UI hit-test and drag key
    double centerHz{0.0};   ///< absolute RF Hz
    double widthHz{200.0};  ///< Hz
    bool   active{true};    ///< per-notch bypass
};

}  // namespace NereusSDR
