// =================================================================
// src/core/codec/P1CodecAnvelinaPro3.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c:668-674 (bank 17)
//   Project Files/Source/ChannelMaster/networkproto1.c:682 (end_frame gate)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Extends P1CodecStandard with the bank 17
//                extra OC carve-out for ANAN-8000DLE / G8NJJ AnvelinaPro3.
// =================================================================
//
// === Verbatim Thetis ChannelMaster/networkproto1.c header (lines 1-45) ===
// /*
//  * networkprot1.c
//  * Copyright (C) 2020 Doug Wigley (W5WC)
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
// =================================================================

#include "P1CodecAnvelinaPro3.h"
#include "core/DdcAssignment.h"

namespace NereusSDR {

void P1CodecAnvelinaPro3::composeCcForBank(int bank, const CodecContext& ctx,
                                           quint8 out[5]) const
{
    if (bank == 17) {
        // Bank 17 — AnvelinaPro3 extra OC outputs (4 bits in C1)
        // Source: networkproto1.c:668-674 [@501e3f5]
        out[0] = (ctx.mox ? 0x01 : 0x00) | 0x26;
        out[1] = quint8(ctx.ocByte & 0x0F);
        out[2] = 0;
        out[3] = 0;
        out[4] = 0;
        return;
    }
    P1CodecStandard::composeCcForBank(bank, ctx, out);
}

// =================================================================
// P1CodecAnvelinaPro3::applyDdcAssignment
// =================================================================
//
// ANVELINAPRO3 is in Thetis's UpdateDDCs Orion case, from Thetis
// console.cs:8220-8227 [v2.10.3.15] (case HPSDRModel.ANVELINAPRO3 with
// ANAN100D / ANAN200D / ORIONMKII / ANAN7000D / ANAN8000D / ANAN_G2 /
// ANAN_G2_1K), and GetDDC's Protocol 1 half treats it as OrionMKII. Plan
// Task 11 moved that layout into P1CodecStandard::ddcAssignmentOrionClass,
// which every Orion-class model shares. Called directly rather than through
// the model switch, so the layout does not depend on CodecContext::model.
//
// The previous body here published Protocol 2-style DDC numbers (stream 0 on
// DDC2, stream 1 on DDC3) and the PureSignal pair as 0/1. On Protocol 1 the
// frame carries RX1 in slot 0, RX2 in slot 2 and the pair in slots 3 + 4
// (networkproto1.c:380-384 case 5 [v2.10.3.15]); those slot numbers are what
// the helper publishes.
DdcAssignment P1CodecAnvelinaPro3::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    return ddcAssignmentOrionClass(ctx, slices, /*redPitaya=*/false);
}

} // namespace NereusSDR
