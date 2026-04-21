// =================================================================
// src/core/codec/P1CodecRedPitaya.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c:606-616 (bank 12 ADC1 carve-out)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Extends P1CodecStandard by overriding
//                bank12() to skip the under-MOX 0x1F force that all
//                other boards apply (DH1KLM RedPitaya contribution).
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

#include "P1CodecRedPitaya.h"

namespace NereusSDR {

// Bank 12 ADC1 — RedPitaya carve-out: respect user attn even under MOX,
// mask to 5 bits.
// Source: networkproto1.c:611-613 [@501e3f5]
//
// "unsure why this was forced, but left as-is for all radios other than
//  Red Pitaya" [original inline comment from networkproto1.c:607-608]
void P1CodecRedPitaya::bank12(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x16;
    // RedPitaya: ADC1 always uses user attn, masked to 5 bits.
    out[1] = quint8((ctx.rxStepAttn[1] & 0x1F) | 0x20);
    out[2] = quint8((ctx.rxStepAttn[2] & 0x1F) | 0x20);
    out[3] = 0;
    out[4] = 0;
}

} // namespace NereusSDR
