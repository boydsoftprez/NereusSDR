// =================================================================
// src/core/SkuUiProfile.cpp  (NereusSDR)
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

// Per-SKU dispatch — one switch case per HPSDRModel value. Every
// case cites the Thetis setup.cs line range it ports from.
//
// Source: Thetis setup.cs:19832-20375 [v2.10.3.13 @501e3f5]

#include "SkuUiProfile.h"

namespace NereusSDR {

SkuUiProfile skuUiProfileFor(HPSDRModel sku)
{
    SkuUiProfile p;

    switch (sku) {
    case HPSDRModel::HPSDR:
        // HPSDR (pre-ANAN Atlas/Penelope) has no explicit case in the Thetis
        // setup.cs:19832-20405 antenna-overlay switch — it falls through and
        // the panel renders with default-initialized fields. NereusSDR-native
        // fallback: treat as classic Alex with all TX-bypass options visible,
        // matching HERMES (setup.cs:19832-19861) except hasRxBypassUi=true
        // (hypothetical hardware — user can disable in UI if not present).
        p.hasExt1OutOnTx = true;
        p.hasExt2OutOnTx = true;
        p.hasRxOutOnTx   = true;
        p.hasRxBypassUi  = true;
        p.antennaTabLabel = QStringLiteral("Alex");
        break;

    case HPSDRModel::HERMES:
        // Thetis setup.cs:19832-19861
        p.hasExt1OutOnTx = true;
        p.hasExt2OutOnTx = true;
        p.hasRxOutOnTx   = true;
        p.hasRxBypassUi  = false;  // setup.cs:19855
        p.antennaTabLabel = QStringLiteral("Alex");
        break;

    case HPSDRModel::ANAN10:
    case HPSDRModel::ANAN10E:
        // Thetis setup.cs:19863-19931 — ANAN10 hides all three TX-bypass options
        p.hasExt1OutOnTx = false;  // setup.cs:19883 / 19920
        p.hasExt2OutOnTx = false;
        p.hasRxOutOnTx   = false;  // setup.cs:6205-6208
        p.hasRxBypassUi  = false;  // setup.cs:19886 / 19923
        p.antennaTabLabel = QStringLiteral("Ant/Filters");  // setup.cs:19876
        break;

    case HPSDRModel::ANAN100:
    case HPSDRModel::ANAN100B:
    case HPSDRModel::ANAN100D:
    case HPSDRModel::ANAN200D:
        // Thetis setup.cs:19933-20095 — classic Alex with all options visible
        // (ORIONMKII shares this branch by analogy below.)
        p.hasExt1OutOnTx = true;   // setup.cs:6270-6271
        p.hasExt2OutOnTx = true;   // setup.cs:6272-6273
        p.hasRxOutOnTx   = true;   // setup.cs:6268-6269
        p.hasRxBypassUi  = true;   // setup.cs:6195
        p.rxOnlyLabels   = {QStringLiteral("EXT2"),
                            QStringLiteral("EXT1"),
                            QStringLiteral("XVTR")};
        p.antennaTabLabel = QStringLiteral("Ant/Filters");
        break;

    case HPSDRModel::ORIONMKII:
        // ORIONMKII has no explicit case in the Thetis setup.cs:19832-20405
        // antenna-overlay switch — it falls through with panel defaults.
        // NereusSDR-native grouping: mirrors the ANAN100-family EXT2/EXT1/XVTR
        // labels and classic-Alex checkbox set because ORIONMKII predates the
        // BPS-port generation (7000D+) and shares the EXT-prefixed wiring.
        p.hasExt1OutOnTx = true;   // setup.cs:6270-6271 (default else branch)
        p.hasExt2OutOnTx = true;   // setup.cs:6272-6273
        p.hasRxOutOnTx   = true;   // setup.cs:6268-6269
        p.hasRxBypassUi  = true;   // setup.cs:6195
        p.rxOnlyLabels   = {QStringLiteral("EXT2"),
                            QStringLiteral("EXT1"),
                            QStringLiteral("XVTR")};
        p.antennaTabLabel = QStringLiteral("Ant/Filters");
        break;

    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANVELINAPRO3:
        // Thetis setup.cs:20097-20147, 20304-20354 — 7000D has Ext1/Ext2 on TX
        p.hasExt1OutOnTx = true;   // setup.cs:6231
        p.hasExt2OutOnTx = true;   // setup.cs:6232
        p.hasRxOutOnTx   = false;  // setup.cs:6230
        p.hasRxBypassUi  = false;  // setup.cs:6174
        p.rxOnlyLabels   = {QStringLiteral("BYPS"),
                            QStringLiteral("EXT1"),
                            QStringLiteral("XVTR")};
        p.antennaTabLabel = QStringLiteral("Ant/Filters");
        break;

    case HPSDRModel::ANAN_G2:
        // Thetis setup.cs:20202-20252 — G2 shares the 7000D checkbox set
        p.hasExt1OutOnTx = true;   // setup.cs:6231
        p.hasExt2OutOnTx = true;
        p.hasRxOutOnTx   = false;
        p.hasRxBypassUi  = false;
        p.rxOnlyLabels   = {QStringLiteral("BYPS"),
                            QStringLiteral("EXT1"),
                            QStringLiteral("XVTR")};
        p.antennaTabLabel = QStringLiteral("Ant/Filters");
        break;

    case HPSDRModel::ANAN_G2E:
        // From Thetis setup.cs:19904-19929 [v2.10.3.15] //N1GP G2E added —
        // G2E shares the G2 / 7000D checkbox set with one override:
        // chkEXT2OutOnTx.Text re-labeled to "Rx BYPASS on Tx" at setup.cs:19929.
        // Both EXT1/EXT2 checkboxes are visible (G2E falls into the else branch
        // at setup.cs:6299 — not the 7000D/G2/G2_1K block — so both are shown).
        p.hasExt1OutOnTx  = true;
        p.hasExt2OutOnTx  = true;
        p.hasRxOutOnTx    = false;
        p.hasRxBypassUi   = false;
        p.rxOnlyLabels    = {QStringLiteral("BYPS"),   // setup.cs:19923
                             QStringLiteral("EXT1"),   // setup.cs:19924
                             QStringLiteral("XVTR")};  // setup.cs:19925
        p.antennaTabLabel = QStringLiteral("Ant/Filters");
        p.ext1OutOnTxLabel = QStringLiteral("Ext 1 on Tx");       // setup.cs:19928
        p.ext2OutOnTxLabel = QStringLiteral("Rx BYPASS on Tx");   // setup.cs:19929 //N1GP G2E added
        break;

    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ANAN_G2_1K:
        // Thetis setup.cs:20148-20200, 20253-20303 — 8000D / G2 1K hide all
        p.hasExt1OutOnTx = false;  // setup.cs:6239 / 6247
        p.hasExt2OutOnTx = false;  // setup.cs:6240 / 6248
        p.hasRxOutOnTx   = false;  // setup.cs:6238 / 6246
        p.hasRxBypassUi  = false;
        p.rxOnlyLabels   = {QStringLiteral("BYPS"),
                            QStringLiteral("EXT1"),
                            QStringLiteral("XVTR")};
        p.antennaTabLabel = QStringLiteral("Ant/Filters");
        break;

    case HPSDRModel::REDPITAYA:
        // Thetis setup.cs:20355-20405 [v2.10.3.13 @501e3f5]
        //DH1KLM
        p.hasExt1OutOnTx = true;
        p.hasExt2OutOnTx = true;
        p.hasRxOutOnTx   = false;
        p.hasRxBypassUi  = false;
        p.rxOnlyLabels   = {QStringLiteral("BYPS"),
                            QStringLiteral("EXT1"),
                            QStringLiteral("XVTR")};
        p.antennaTabLabel = QStringLiteral("Ant/Filters");
        break;

    case HPSDRModel::HERMESLITE:
        // HERMESLITE does not appear in Thetis setup.cs:19832-20405 (HL2
        // support lives in the mi0bot fork, not upstream Thetis). Bare HL2
        // has no Alex board — tab is hidden outright so the per-band grid
        // never renders.
        p.hasExt1OutOnTx = false;
        p.hasExt2OutOnTx = false;
        p.hasRxOutOnTx   = false;
        p.hasRxBypassUi  = false;
        p.hasAntennaTab  = false;
        break;

    case HPSDRModel::FIRST:
    case HPSDRModel::LAST:
        // Sentinel values — same pattern as HardwareProfile.cpp:197-199.
        break;
    }

    return p;
}

}  // namespace NereusSDR
