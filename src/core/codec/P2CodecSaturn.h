/*
 * network.c
 * Copyright (C) 2015-2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

// =================================================================
// src/core/codec/P2CodecSaturn.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources (multi-source) [@501e3f5]:
//   Project Files/Source/ChannelMaster/network.c:821-1248
//     (P2 packet shape — inherited from P2CodecOrionMkII)
//   Project Files/Source/Console/console.cs:6944-7040
//     (G8NJJ setBPF1ForOrionIISaturn — Saturn-specific HPF override)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Extends P2CodecOrionMkII by overriding
//                buildAlex1() to optionally substitute Saturn BPF1 bits
//                (from CodecContext.p2SaturnBpfHpfBits) when the
//                user has configured Saturn-specific band edges via
//                Phase B Task 8's Alex-1 Filters Setup page.
// =================================================================
//
// === Verbatim Thetis ChannelMaster/network.c header (lines 1-19) ===
//
// /*
//  * network.c
//  * Copyright (C) 2015-2020 Doug Wigley (W5WC)
//  *
//  * This library is free software; you can redistribute it and/or
//  * modify it under the terms of the GNU Lesser General Public
//  * License as published by the Free Software Foundation; either
//  * version 2 of the License, or (at your option) any later version.
//  *
//  * This library is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  * Lesser General Public License for more details.
//  *
//  * You should have received a copy of the GNU Lesser General Public
//  * License along with this library; if not, write to the Free Software
//  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//  *
//  */
//
// =================================================================
// --- From console.cs ---
// === Verbatim Thetis Console/console.cs header (lines 1-50) ===
//
// //=================================================================
// // console.cs
// //=================================================================
// // Thetis is a C# implementation of a Software Defined Radio.
// // Copyright (C) 2004-2009  FlexRadio Systems
// // Copyright (C) 2010-2020  Doug Wigley
// // Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
// //
// // This program is free software; you can redistribute it and/or
// // modify it under the terms of the GNU General Public License
// // as published by the Free Software Foundation; either version 2
// // of the License, or (at your option) any later version.
// //
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //
// // You should have received a copy of the GNU General Public License
// // along with this program; if not, write to the Free Software
// // Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// //
// // You may contact us via email at: sales@flex-radio.com.
// // Paper mail may be sent to:
// //    FlexRadio Systems
// //    8900 Marybank Dr.
// //    Austin, TX 78750
// //    USA
// //
// //=================================================================
// // Modifications to support the Behringer Midi controllers
// // by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// // Modifications for using the new database import function.  W2PA, 29 May 2017
// // Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// // Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// //
// //============================================================================================//
// // Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// // ------------------------------------------------------------------------------------------ //
// // For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// // made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// // right to use, license, and distribute such code under different terms, including           //
// // closed-source and proprietary licences, in addition to the GNU General Public License      //
// // granted above. Nothing in this statement restricts any rights granted to recipients under  //
// // the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// // its original terms and is not affected by this dual-licensing statement in any way.        //
// // Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
// //============================================================================================//
// //
// // Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12
//
// =================================================================

#pragma once

#include "P2CodecOrionMkII.h"

namespace NereusSDR {

// Saturn (ANAN-G2 / G2-1K, G8NJJ contribution) — extends P2CodecOrionMkII
// with the BPF1 band-edge override. When CodecContext.p2SaturnBpfHpfBits
// is non-zero, that value substitutes for the standard Alex HPF bits in
// the Alex1 (RX) byte region of CmdHighPriority. Falls through to the
// parent's behavior otherwise.
//
// Source: console.cs:6944-7040 [@501e3f5] (G8NJJ setBPF1ForOrionIISaturn)
//
// Phase 3M-4 Task 5: applyPureSignalDdcConfig is INHERITED unchanged from
// P2CodecOrionMkII.  Thetis console.cs:8211-8295 [v2.10.3.13] groups
// HPSDRModel.ANAN_G2 and HPSDRModel.ANAN_G2_1K into the same G2-class
// switch case as the OrionMkII family, so no Saturn-specific PS DDC
// override is needed.
//
// Phase 3F Sub-Epic B Task 4 gave this class its own applyDdcAssignment,
// hand-copied from P2CodecOrionMkII. That was a mistake, and it cost us: when
// antenna-driven ADC routing landed in the parent on 2026-07-26 the copy
// never received it, so on the ANAN-G2 -- the exact radio the feature was
// written for -- selecting EXT1 or EXT2 did nothing to the DDC-to-ADC map.
// Every test for the new routing constructed P2CodecOrionMkII, so all four
// passed while the feature was absent on Saturn.
//
// The override is gone. applyDdcAssignment is INHERITED, for the same reason
// applyPureSignalDdcConfig already was (see the note above): Thetis groups
// HPSDRModel.ANAN_G2 and ANAN_G2_1K into the SAME UpdateDDCs switch case as
// the OrionMkII family (console.cs:8220-8303 [v2.10.3.15]), so a
// Saturn-specific DDC assignment does not exist upstream to port. The two
// bodies were byte-equivalent apart from the missing routing block and the
// position of the adcCtrl seed.
//
// buildAlex1 stays overridden because that one IS Saturn-specific upstream
// (G8NJJ setBPF1ForOrionIISaturn, console.cs:6944-7040) -- which is the test
// for whether a future override belongs here: point at the Thetis branch that
// makes Saturn different, or inherit. tst_p2_codec_saturn.cpp's
// ddcAssignment_inherits_orionmkii_behavior sweeps the state matrix and fails
// on any silent divergence.
class P2CodecSaturn : public P2CodecOrionMkII {
protected:
    // Override the Alex1 (RX) byte builder to optionally substitute
    // Saturn BPF1 bits for the standard Alex HPF bits.
    //
    // Bit layout (from P2CodecOrionMkII::buildAlex1): HPF bits are
    // scattered across bit positions 1, 2, 3, 4, 5, 6, and 12 of the
    // 32-bit Alex1 register (mask 0x0000107E). The override clears those
    // seven bits and re-inserts ctx.p2SaturnBpfHpfBits using the same
    // scatter table.
    //
    // Source: console.cs:6944-7040 [@501e3f5] (G8NJJ setBPF1ForOrionIISaturn)
    quint32 buildAlex1(const CodecContext& ctx) const override;
};

} // namespace NereusSDR
