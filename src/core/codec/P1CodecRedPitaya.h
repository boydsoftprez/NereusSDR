// =================================================================
// src/core/codec/P1CodecRedPitaya.h  (NereusSDR)
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

#pragma once

#include "P1CodecStandard.h"

namespace NereusSDR {

// RedPitaya (DH1KLM port) — extends Standard with bank 12 ADC1 carve-out.
// During MOX, RedPitaya does NOT force ADC1 to 0x1F like other boards;
// it respects the user-set rxStepAttn[1] masked to 5 bits.
//
// Source: networkproto1.c:611-613 [@501e3f5] (DH1KLM contribution)
class P1CodecRedPitaya : public P1CodecStandard {
protected:
    void bank12(const CodecContext& ctx, quint8 out[5]) const override;
};

} // namespace NereusSDR
