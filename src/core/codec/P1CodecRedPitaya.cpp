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
#include "core/DdcAssignment.h"

namespace NereusSDR {

// Bank 12 ADC1 — RedPitaya carve-out: respect user attn even under MOX,
// mask to 5 bits.
// Source: networkproto1.c:611-613 [@501e3f5]
//
// "unsure why this was forced, but left as-is for all radios other than
//  Red Pitaya" [original inline comment from networkproto1.c:607-608]
// Upstream inline attribution (networkproto1.c:612, preserved verbatim):
//   if (HPSDRModel == HPSDRModel_REDPITAYA) //[2.10.3.9]DH1KLM  //model needed as board type (prn->discovery.BoardType) is an OrionII
void P1CodecRedPitaya::bank12(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x16;
    // RedPitaya: ADC1 always uses user attn, masked to 5 bits.
    out[1] = quint8((ctx.rxStepAttn[1] & 0x1F) | 0x20);
    out[2] = quint8((ctx.rxStepAttn[2] & 0x1F) | 0x20);
    out[3] = 0;
    out[4] = 0;
}

// =================================================================
// Phase 3M-4 Task 5: PureSignal DDC config — RedPitaya branch
// =================================================================
//
// Verbatim port of the RedPitaya branch in Thetis console.cs UpdateDDCs().
// Differs from the G2-class branch only by setting Rate[1] = rx1_rate
// in the PS-off MOX cases (REDPITAYA PAVEL inline notes in upstream).
//
// Source: Thetis console.cs:8296-8377 [v2.10.3.13]
//
// case HPSDRModel.REDPITAYA: //DH1KLM
//     P1_rxcount = 5;                     // RX5 used for puresignal feedback
//     nddc = 5;
//     ...
//
// `ps_rate` from cmaster.cs:424 [v2.10.3.13]: private static int ps_rate = 192000;
PsDdcConfig P1CodecRedPitaya::applyPureSignalDdcConfig(
    HPSDRModel /*model*/,
    bool psEnabled,
    bool diversityEnabled,
    bool moxState,
    int rx1Rate,
    int rx2Rate,
    bool rx2Enabled,
    quint8 adcCtrl1,
    quint8 adcCtrl2) const
{
    PsDdcConfig cfg;
    constexpr uint8_t DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
    constexpr int ps_rate = 192000;

    // From console.cs:8297-8298 [v2.10.3.13]
    cfg.p1RxCount = 5;
    cfg.nDdc      = 5;

    if (!moxState) {
        if (diversityEnabled) {
            // From console.cs:8301-8311 [v2.10.3.13]
            cfg.p1DdcConfig = 2; // REDPITAYA PAVEL
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate); // REDPITAYA PAVEL
            cfg.cntrl1      = static_cast<uint8_t>(adcCtrl1 & 0xff);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);
        } else {
            // From console.cs:8312-8322 [v2.10.3.13]
            cfg.p1DdcConfig = 1;
            cfg.ddcEnable   = DDC2;
            cfg.syncEnable  = 0;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate); // REDPITAYA PAVEL
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate); // REDPITAYA PAVEL
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>(adcCtrl1 & 0xff);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);
        }
    } else {
        if (!diversityEnabled && !psEnabled) {
            // From console.cs:8326-8336 [v2.10.3.13]
            cfg.p1DdcConfig = 1;
            cfg.ddcEnable   = DDC2;
            cfg.syncEnable  = 0;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate); // REDPITAYA PAVEL
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate); // REDPITAYA PAVEL
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>(adcCtrl1 & 0xff);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);
        } else if (!diversityEnabled && psEnabled) {
            // From console.cs:8337-8347 [v2.10.3.13]
            cfg.p1DdcConfig = 3;
            cfg.ddcEnable   = static_cast<uint8_t>(DDC0 + DDC2);
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = ps_rate;
            cfg.rate[1]     = ps_rate;
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>((adcCtrl1 & 0xf3) | 0x08);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);

            // Phase 3M-4 mi0bot audit: PS DDC pair indices for nddc=5
            // (RedPitaya P1 mode).  Same dispatch as the G2-class branch
            // in P1CodecStandard — all nddc=5 P1 boards go through
            // MetisReadThreadMainLoop case 5: twist(spr, 3, 4, 1) which
            // pairs DDC3+DDC4 for PS.
            //
            // Source: mi0bot networkproto1.c:388-392 [v2.10.3.13-beta2]
            // + console.cs:8710-8714 [v2.10.3.13-beta2] GetDDC P1 case 5
            // Orion-class: rx1=0, rx2=2, psrx=3, pstx=4.
            cfg.psFbDdc  = 3;
            cfg.txMonDdc = 4;
        } else if (diversityEnabled && psEnabled) {
            // From console.cs:8348-8358 [v2.10.3.13]
            cfg.p1DdcConfig = 3;
            cfg.ddcEnable   = static_cast<uint8_t>(DDC0 + DDC2);
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = ps_rate;
            cfg.rate[1]     = ps_rate;
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>((adcCtrl1 & 0xf3) | 0x08);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);

            // Same PS DDC layout as the !diversity && PS branch above.
            cfg.psFbDdc  = 3;
            cfg.txMonDdc = 4;
        } else {
            // diversity_enabled && !puresignal_enabled
            // From console.cs:8359-8369 [v2.10.3.13]
            cfg.p1DdcConfig = 2;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate); // REDPITAYA PAVEL
            cfg.cntrl1      = static_cast<uint8_t>(adcCtrl1 & 0xff);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);
        }
    }

    // From console.cs:8372-8376 [v2.10.3.13]
    if (rx2Enabled) {
        cfg.ddcEnable = static_cast<uint8_t>(cfg.ddcEnable + DDC3);
        cfg.rate[3]   = static_cast<uint32_t>(rx2Rate);
    }

    return cfg;
}

DdcAssignment P1CodecRedPitaya::applyDdcAssignment(
    const CodecContext& /*ctx*/,
    const std::array<SliceConfig, 5>& /*slices*/) const
{
    // TODO Sub-Epic B Task 10: real implementation per Thetis UpdateDDCs Hermes-class branch (P1 mode); 384k via include_extra_p1_rate
    return DdcAssignment{};
}

} // namespace NereusSDR
