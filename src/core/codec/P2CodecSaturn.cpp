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
// src/core/codec/P2CodecSaturn.cpp  (NereusSDR)
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

#include "P2CodecSaturn.h"

namespace NereusSDR {

// Saturn BPF1 override — substitute p2SaturnBpfHpfBits for the standard
// HPF bits in Alex1 when configured.
// Source: console.cs:6944-7040 [@501e3f5] (G8NJJ setBPF1ForOrionIISaturn)
//
// setBPF1ForOrionIISaturn calls NetworkIO.SetAlexHPFBits() with these values:
//   0x20 = Bypass HPF
//   0x10 = 1.5 MHz HPF
//   0x08 = 6.5 MHz HPF
//   0x04 = 9.5 MHz HPF
//   0x01 = 13 MHz HPF
//   0x02 = 20 MHz HPF
//   0x40 = 6m BPF/LNA
//
// P2CodecOrionMkII::buildAlex1 maps alexHpfBits into Alex1 register bits:
//   alexHpfBits 0x01 → bit 1  (13 MHz)
//   alexHpfBits 0x02 → bit 2  (20 MHz)
//   alexHpfBits 0x04 → bit 4  (9.5 MHz)
//   alexHpfBits 0x08 → bit 5  (6.5 MHz)
//   alexHpfBits 0x10 → bit 6  (1.5 MHz)
//   alexHpfBits 0x20 → bit 12 (Bypass)
//   alexHpfBits 0x40 → bit 3  (6m preamp)
//
// The full HPF bit positions in the 32-bit Alex1 word: bits 1,2,3,4,5,6,12.
// Mask: (1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<12) = 0x0000107E
//
// Strategy: build the parent's Alex1 word (preserves antenna + LPF bits),
// strip the 7 HPF bits, then re-insert using p2SaturnBpfHpfBits through
// the same scatter table so the bit encoding is identical.
quint32 P2CodecSaturn::buildAlex1(const CodecContext& ctx) const
{
    // Build the parent's Alex1 word — it carries the right antenna
    // selection bits and LPF state we want to preserve.
    const quint32 baseAlex1 = P2CodecOrionMkII::buildAlex1(ctx);

    // If user hasn't configured Saturn BPF1 edges, fall through to parent.
    // Source: console.cs:6944 [@501e3f5] — function only runs when
    // alexpresent && !initializing; we model "not configured" as bit==0.
    if (ctx.p2SaturnBpfHpfBits == 0) { return baseAlex1; }

    // HPF bit positions used by buildAlex1 (see P2CodecOrionMkII.cpp):
    //   bits 1, 2, 3, 4, 5, 6, 12
    // Mask covers all seven positions.
    constexpr quint32 kHpfBitMask = (1u << 1) | (1u << 2) | (1u << 3)
                                  | (1u << 4) | (1u << 5) | (1u << 6)
                                  | (1u << 12);  // 0x0000107E

    // Strip the HPF bits from the parent word.
    quint32 reg = baseAlex1 & ~kHpfBitMask;

    // Re-insert using p2SaturnBpfHpfBits through the same scatter table
    // as buildAlex1, so the bit encoding is byte-identical for the radio.
    // Source: console.cs:6944-7040 [@501e3f5] — SetAlexHPFBits() bit values
    // mirrored by P2CodecOrionMkII HPF scatter (netInterface.c:605-621).
    const quint8 satBits = ctx.p2SaturnBpfHpfBits;
    if (satBits & 0x01) { reg |= (1u << 1);  }  // 13 MHz HPF
    if (satBits & 0x02) { reg |= (1u << 2);  }  // 20 MHz HPF
    if (satBits & 0x04) { reg |= (1u << 4);  }  // 9.5 MHz HPF
    if (satBits & 0x08) { reg |= (1u << 5);  }  // 6.5 MHz HPF
    if (satBits & 0x10) { reg |= (1u << 6);  }  // 1.5 MHz HPF
    if (satBits & 0x20) { reg |= (1u << 12); }  // Bypass HPF
    if (satBits & 0x40) { reg |= (1u << 3);  }  // 6m BPF/LNA

    return reg;
}

} // namespace NereusSDR
