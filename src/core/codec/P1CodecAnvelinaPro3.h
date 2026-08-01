// =================================================================
// src/core/codec/P1CodecAnvelinaPro3.h  (NereusSDR)
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

#pragma once

#include "P1CodecStandard.h"

namespace NereusSDR {

// AnvelinaPro3 (ANAN-8000DLE / G8NJJ contribution) — extends Standard
// with a 17th bank carrying extra OC pin outputs (4 bits in C1).
//
// Source: networkproto1.c:668-674 + 682 [@501e3f5]
//
// Phase 3M-4 Task 5: applyPureSignalDdcConfig is INHERITED unchanged from
// P1CodecStandard.  Its switch dispatches HpsdrModel::ANVELINAPRO3 into
// the G2-class branch, matching Thetis console.cs:8218 [v2.10.3.13]
// which lists ANVELINAPRO3 in the same case as ANAN_G2 / 7000D / 8000D.
//
// Phase 3F: applyDdcAssignment overridden with a stub; real implementation
// queued for Sub-Epic B Task 10 per Thetis UpdateDDCs Hermes-class branch.
class P1CodecAnvelinaPro3 : public P1CodecStandard {
public:
    void composeCcForBank(int bank, const CodecContext& ctx, quint8 out[5]) const override;
    int  maxBank() const override { return 17; }

    DdcAssignment applyDdcAssignment(
        const CodecContext& ctx,
        const std::array<SliceConfig, 5>& slices) const override;
};

} // namespace NereusSDR
