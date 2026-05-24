// =================================================================
// src/core/SkuUiProfile.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-22 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// setup.cs
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// UI-only per-SKU overlay — describes the visible labels and which
// Ext/Bypass-on-TX checkboxes to render for a given HPSDRModel. Does
// NOT affect the wire (protocol encoding is identical across SKUs of
// the same chipset). Mirrors Thetis setup.cs:19832-20375 per-SKU
// branches.
//
// Source: Thetis setup.cs:19832-20375 [v2.10.3.13 @501e3f5]
// Phase: 3P-I-b. Spec: docs/architecture/antenna-routing-design.md §4.3.

#pragma once

#include "HpsdrModel.h"

#include <QString>
#include <array>

namespace NereusSDR {

struct SkuUiProfile {
    bool                    hasExt1OutOnTx  {false};
    bool                    hasExt2OutOnTx  {false};
    bool                    hasRxOutOnTx    {false};
    bool                    hasRxBypassUi   {false};  // chkDisableRXOut
    bool                    hasAntennaTab   {true};   // false on HL2
    std::array<QString, 3>  rxOnlyLabels    {QStringLiteral("RX1"),
                                             QStringLiteral("RX2"),
                                             QStringLiteral("XVTR")};
    QString                 antennaTabLabel {QStringLiteral("Alex")};

    // Per-board label overrides for the EXT1/EXT2-on-TX checkboxes in the
    // Antenna control tab. Default to "Ext 1 on Tx" / "Ext 2 on Tx" matching
    // Thetis setup.cs default. Override per SKU when Thetis relabels (e.g.,
    // G2E re-labels chkEXT2OutOnTx to "Rx BYPASS on Tx" at setup.cs:19929
    // [v2.10.3.15] //N1GP G2E added).
    QString                 ext1OutOnTxLabel {QStringLiteral("Ext 1 on Tx")};
    QString                 ext2OutOnTxLabel {QStringLiteral("Ext 2 on Tx")};
};

SkuUiProfile skuUiProfileFor(HPSDRModel sku);

}  // namespace NereusSDR
