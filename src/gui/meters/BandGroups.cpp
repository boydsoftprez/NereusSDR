// =================================================================
// src/gui/meters/BandGroups.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// The band groups of Thetis clsBandButtonBox (GEN, HF, VHF): which band or
// jump each of the 15 buttons holds in each group, and the group a band
// belongs to. One table, shared by BandButtonItem and NereusBandButtonItem.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Band groups and their layouts ported from Thetis
//                 clsBandButtonBox.setupButtons and GetBandGroupFromBand.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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


#include "BandGroups.h"

namespace NereusSDR {
namespace {

using Slot = BandGroupSlot;

constexpr Slot bandSlot(Band band) { return {band, Slot::Band}; }
constexpr Slot blankSlot() { return {Band::GEN, Slot::Blank}; }
constexpr Slot jumpSlot(Slot::Kind kind) { return {Band::GEN, kind}; }

// From Thetis MeterManager.cs:12967-12977 [v2.10.3.15] (case BandGroups.GEN):
//   start_band = (int)Band.BLMF; end_band = (int)Band.B11M;
//   SetText(1, 14, "HF");
// Thetis's first GEN button is L/MW (Band.BLMF), which NereusSDR has no band
// for. NereusSDR's general coverage band, GEN, takes that place, so 120m to
// 11m keep Thetis's positions 1 to 13 and GEN keeps a button.
QVector<Slot> makeGenLayout()
{
    return {
        bandSlot(Band::GEN),
        bandSlot(Band::Band120m), bandSlot(Band::Band90m), bandSlot(Band::Band61m),
        bandSlot(Band::Band49m),  bandSlot(Band::Band41m), bandSlot(Band::Band31m),
        bandSlot(Band::Band25m),  bandSlot(Band::Band22m), bandSlot(Band::Band19m),
        bandSlot(Band::Band16m),  bandSlot(Band::Band14m), bandSlot(Band::Band13m),
        bandSlot(Band::Band11m),
        jumpSlot(Slot::JumpHf),
    };
}

// From Thetis MeterManager.cs:12978-13008 [v2.10.3.15] (case BandGroups.HF):
//   start_band = (int)Band.B160M; end_band = (int)Band.B6M;
//   SetText(1, 11, ""); SetText(1, 12, "VHF"); SetText(1, 13, "WWV");
//   SetText(1, 14, "SWL");
QVector<Slot> makeHfLayout()
{
    return {
        bandSlot(Band::Band160m), bandSlot(Band::Band80m), bandSlot(Band::Band60m),
        bandSlot(Band::Band40m),  bandSlot(Band::Band30m), bandSlot(Band::Band20m),
        bandSlot(Band::Band17m),  bandSlot(Band::Band15m), bandSlot(Band::Band12m),
        bandSlot(Band::Band10m),  bandSlot(Band::Band6m),
        // The blank at 11. Thetis styles it as an empty button:
        //SetBorderColour(1, 11, System.Drawing.Color.Transparent);
        //SetFillColour(1, 11, System.Drawing.Color.Transparent);
        // [original inline comments from MeterManager.cs:12988-12989]
        blankSlot(),
        jumpSlot(Slot::JumpVhf),
        bandSlot(Band::WWV),
        // "SWL" (MeterManager.cs:13003) opens the GEN group (:13187-13190).
        jumpSlot(Slot::JumpGen),
    };
}

// From Thetis MeterManager.cs:13009-13019 [v2.10.3.15] (case BandGroups.VHF):
//   start_band = (int)Band.VHF0; end_band = (int)Band.VHF13;
//   SetText(1, 14, "HF");
// Thetis's VHF0 to VHF13 are its transverter bands. NereusSDR has 2 m and the
// one transverter band, XVTR; the rest of the group's buttons are blank.
QVector<Slot> makeVhfLayout()
{
    QVector<Slot> layout{bandSlot(Band::Band2m), bandSlot(Band::XVTR)};
    while (layout.size() < kBandGroupButtons - 1) {
        layout.append(blankSlot());
    }
    layout.append(jumpSlot(Slot::JumpHf));
    return layout;
}

} // namespace

const QVector<BandGroupSlot>& bandGroupLayout(BandGroup group)
{
    static const QVector<Slot> gen = makeGenLayout();
    static const QVector<Slot> hf = makeHfLayout();
    static const QVector<Slot> vhf = makeVhfLayout();
    switch (group) {
    case BandGroup::Gen: return gen;
    case BandGroup::Vhf: return vhf;
    case BandGroup::Hf: break;
    }
    return hf;
}

// From Thetis MeterManager.cs:25864-25916 [v2.10.3.15] (GetBandGroupFromBand).
// Thetis puts B2M in HF, where its layout has no 2 m button; NereusSDR's 2 m
// button is in VHF, so 2 m belongs to VHF here. XVTR (Thetis's VHF0-VHF13)
// is VHF. NereusSDR's GEN band takes BLMF's place in GEN, and anything else
// falls to GEN as in Thetis (default: return BandGroups.GEN).
BandGroup bandGroupForBand(Band band)
{
    switch (band) {
    case Band::Band160m:
    case Band::Band80m:
    case Band::Band60m:
    case Band::Band40m:
    case Band::Band30m:
    case Band::Band20m:
    case Band::Band17m:
    case Band::Band15m:
    case Band::Band12m:
    case Band::Band10m:
    case Band::Band6m:
    case Band::WWV:
        return BandGroup::Hf;
    case Band::Band2m:
    case Band::XVTR:
        return BandGroup::Vhf;
    case Band::GEN:
    case Band::Band120m:
    case Band::Band90m:
    case Band::Band61m:
    case Band::Band49m:
    case Band::Band41m:
    case Band::Band31m:
    case Band::Band25m:
    case Band::Band22m:
    case Band::Band19m:
    case Band::Band16m:
    case Band::Band14m:
    case Band::Band13m:
    case Band::Band11m:
    case Band::Count:
        break;
    }
    return BandGroup::Gen;
}

int bandGroupPosition(BandGroup group, Band band)
{
    const QVector<BandGroupSlot>& layout = bandGroupLayout(group);
    for (int i = 0; i < layout.size(); ++i) {
        if (layout[i].kind == BandGroupSlot::Band && layout[i].band == band) {
            return i;
        }
    }
    return -1;
}

} // namespace NereusSDR
