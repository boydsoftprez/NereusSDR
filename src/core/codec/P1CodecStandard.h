// =================================================================
// src/core/codec/P1CodecStandard.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c:419-698 (WriteMainLoop)
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Lifted from P1RadioConnection::composeCcForBank
//                which previously held the inline ramdor port; now
//                delegates here.
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

#include "IP1Codec.h"

namespace NereusSDR {

// ramdor WriteMainLoop port. Banks 0-16. Used for every non-HL2 P1
// board by default; AnvelinaPro3 and RedPitaya extend it with bank-17
// or bank-12 overrides (see those subclasses).
class P1CodecStandard : public IP1Codec {
public:
    void composeCcForBank(int bank, const CodecContext& ctx, quint8 out[5]) const override;
    int  maxBank() const override { return 16; }

    // Phase 3M-4 Task 5: PureSignal DDC config emission.  P1CodecStandard
    // dispatches on `model` because it covers multiple HpsdrModel values
    // (HERMES, ANAN10/10E, ANAN100/100B, ANAN100D, ANAN200D, ORIONMKII,
    // ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2_1K, HPSDR), each with a
    // distinct branch in Thetis console.cs UpdateDDCs.  AnvelinaPro3
    // shares the G2-class branch via override; RedPitaya has its own
    // branch via override.
    //
    // Source: Thetis console.cs:8186-8525 [v2.10.3.13]
    PsDdcConfig applyPureSignalDdcConfig(
        HPSDRModel model,
        bool psEnabled,
        bool diversityEnabled,
        bool moxState,
        int rx1Rate,
        int rx2Rate,
        bool rx2Enabled,
        quint8 adcCtrl1,
        quint8 adcCtrl2
    ) const override;

    DdcAssignment applyDdcAssignment(
        const CodecContext& ctx,
        const std::array<SliceConfig, 5>& slices) const override;

protected:
    // Helpers exposed so subclasses can call into specific banks they
    // don't override.
    void bank0(const CodecContext& ctx, quint8 out[5]) const;
    void bank10(const CodecContext& ctx, quint8 out[5]) const;
    void bank11(const CodecContext& ctx, quint8 out[5]) const;
    virtual void bank12(const CodecContext& ctx, quint8 out[5]) const;  // RedPitaya overrides

    // Phase 3M-4 Task 5: per-branch helpers reusable from subclasses
    // (AnvelinaPro3 = G2-class, RedPitaya overrides directly).  Each
    // helper transcribes one Thetis switch-case verbatim.
    PsDdcConfig psDdcConfigG2Class(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled,
        quint8 adcCtrl1, quint8 adcCtrl2) const;

    PsDdcConfig psDdcConfigHermesClass(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled) const;

    PsDdcConfig psDdcConfigHermesIIClass(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled) const;
};

} // namespace NereusSDR
