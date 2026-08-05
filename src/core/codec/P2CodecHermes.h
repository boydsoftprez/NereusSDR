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
// src/core/codec/P2CodecHermes.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources (multi-source) [v2.10.3.15 / 3759d09]:
//   Project Files/Source/ChannelMaster/network.c:821-1248
//     (P2 packet shape — inherited from P2CodecOrionMkII)
//   Project Files/Source/Console/console.cs:8387-8459
//     (UpdateDDCs() Hermes-class branch — nddc=4, rx1 on DDC0)
//   Project Files/Source/Console/console.cs:8610-8642
//     (GetDDC() P2 Hermes-class branch — rx1=0, rx2=1)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-25 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Phase 3F Sub-Epic I Task 7c. Extends
//                P2CodecOrionMkII by overriding applyDdcAssignment() with
//                the Hermes-class stream-to-DDC table (stream 0 -> DDC0)
//                so the 1-ADC boards running community P2 firmware stop
//                inheriting the 2-ADC DDC2 layout.
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

// Hermes-class P2 codec — ANAN-10 / ANAN-100 (Hermes), ANAN-10E / ANAN-100B
// (HermesII) and ANAN-G2E (HermesC10) running community Protocol 2 firmware.
//
// These are 1-ADC boards. Thetis keeps them in a switch case entirely separate
// from the 2-ADC family, in both functions that decide DDC placement:
//
//   console.cs:8556-8608 [v2.10.3.15]  GetDDC() 2-ADC:      rx1 = 2, rx2 = 3
//   console.cs:8610-8642 [v2.10.3.15]  GetDDC() Hermes:     rx1 = 0, rx2 = 1
//   console.cs:8220-8303 [v2.10.3.15]  UpdateDDCs() 2-ADC:  DDCEnable = DDC2
//   console.cs:8387-8459 [v2.10.3.15]  UpdateDDCs() Hermes: DDCEnable = DDC0
//
// NereusSDR mirrors that split with a separate codec rather than a conditional
// inside the 2-ADC codec. Everything else about the P2 wire dialect is shared,
// so only applyDdcAssignment() is overridden:
//
//   - compose* are INHERITED unchanged. The community P2 firmware on these
//     boards speaks standard P2; the discovery reply and command packet shapes
//     are identical (network.c:821-1248).
//   - applyPureSignalDdcConfig is INHERITED unchanged. It already dispatches on
//     HPSDRModel into psDdcConfigHermesClass (console.cs:8378-8449) and
//     psDdcConfigHermesIIClass (console.cs:8451-8521), added 2026-05-17 to
//     close issue #263 — the same "1-ADC board inherited the G2-class DDC2
//     layout and never streamed I/Q" defect this codec fixes one layer up, in
//     the 5-stream assignment path.
//
// //N1GP G2E added  [original inline tag from console.cs:8388 — ANAN_G2E case label]
// //N1GP G2E added (HermesC10)  [original inline tag from console.cs:8612 — HermesC10 case label]
class P2CodecHermes : public P2CodecOrionMkII {
public:
    // Phase 3F Sub-Epic I Task 7c: Hermes-class stream-to-DDC table.
    //
    // Source: Thetis console.cs:8387-8459 [v2.10.3.15] UpdateDDCs()
    // Hermes-class branch, cross-checked against console.cs:8610-8642
    // [v2.10.3.15] GetDDC() P2 Hermes-class branch.
    DdcAssignment applyDdcAssignment(
        const CodecContext& ctx,
        const std::array<SliceConfig, 5>& slices) const override;

protected:
    // Family shape. Thetis carries the 1-ADC boards in two adjacent switch
    // branches that agree on DDC placement (rx1 -> DDC0, rx2 -> DDC1) and
    // differ only in these scalars, so P2CodecHermesII overrides them rather
    // than restating the whole branch:
    //
    //   Hermes-class  console.cs:8391-8392 [v2.10.3.15]  P1_rxcount=4, nddc=4
    //                 console.cs:8451                    PS P1_DDCConfig=6
    //   HermesII      console.cs:8463-8464 [v2.10.3.15]  P1_rxcount=2, nddc=2
    //                 console.cs:8522                    PS P1_DDCConfig=5
    //
    // familyDdcCount() bounds the Phase 3F extension of streams 2+ into
    // Thetis's idle slots, so a 2-DDC board is never handed DDC2 or DDC3.
    //
    //N1GP G2E added  [original inline tag from console.cs:8388 — ANAN_G2E case label]

    // From Thetis console.cs:8392 [v2.10.3.15]: nddc = 4;
    virtual int familyDdcCount() const noexcept { return 4; }

    // From Thetis console.cs:8391 [v2.10.3.15]: P1_rxcount = 4;
    virtual int familyP1RxCount() const noexcept { return 4; }

    // From Thetis console.cs:8451 [v2.10.3.15]: P1_DDCConfig = 6; (PS branch)
    virtual int familyPsP1DdcConfig() const noexcept { return 6; }
};

// HermesII P2 codec — ANAN-10E / ANAN-100B on community Protocol 2 firmware.
//
// Thetis's third 1-ADC branch, at console.cs:8461-8531 [v2.10.3.15]. Identical
// to the Hermes-class branch on DDC placement — GetDDC groups all three boards
// together at console.cs:8610-8642 (rx1 = 0, rx2 = 1) — and differs only in
// nddc / P1_rxcount (2 rather than 4) and the PureSignal P1_DDCConfig (5 rather
// than 6). Both differences are expressed as overrides of the family-shape
// accessors above; applyDdcAssignment itself is inherited.
class P2CodecHermesII : public P2CodecHermes {
protected:
    // From Thetis console.cs:8464 [v2.10.3.15]: nddc = 2;
    int familyDdcCount() const noexcept override { return 2; }

    // From Thetis console.cs:8463 [v2.10.3.15]: P1_rxcount = 2;
    int familyP1RxCount() const noexcept override { return 2; }

    // From Thetis console.cs:8522 [v2.10.3.15]: P1_DDCConfig = 5; (PS branch)
    int familyPsP1DdcConfig() const noexcept override { return 5; }
};

} // namespace NereusSDR
