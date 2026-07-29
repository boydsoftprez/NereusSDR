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
// Porting from Thetis console.cs:8218-8303 [v2.10.3.15] UpdateDDCs()
// OrionMkII-class branch. ANVELINAPRO3 falls in the same case block as
// ANAN100D / ANAN200D / ORIONMKII / ANAN7000D / ANAN8000D / ANAN_G2 /
// ANAN_G2_1K. NOT the Hermes-class branch (Thetis console.cs:8387-8455).
//
// Original C# (relevant fragment):
//
//   case HPSDRModel.ANAN100D:
//   case HPSDRModel.ANAN200D:
//   case HPSDRModel.ORIONMKII:
//   case HPSDRModel.ANAN7000D:
//   case HPSDRModel.ANAN8000D:
//   case HPSDRModel.ANAN_G2:
//   case HPSDRModel.ANAN_G2_1K:
//   case HPSDRModel.ANVELINAPRO3:
//       P1_rxcount = 5;                     // RX5 used for puresignal feedback
//       nddc = 5;
//       if (!_mox)
//       {
//           if (diversity_enabled)
//           {
//               P1_DDCConfig = DDCEnable = DDC0;
//               SyncEnable = DDC1;
//               Rate[0] = rx1_rate; Rate[1] = rx1_rate;
//               cntrl1 = rx_adc_ctrl1 & 0xff;
//               cntrl2 = rx_adc_ctrl2 & 0x3f;
//           }
//           else
//           {
//               P1_DDCConfig = 1; DDCEnable = DDC2; SyncEnable = 0;
//               if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE p1 !
//               Rate[2] = rx1_rate;
//               cntrl1 = rx_adc_ctrl1 & 0xff;
//               cntrl2 = rx_adc_ctrl2 & 0x3f;
//           }
//       }
//       else
//       {
//           if (!diversity_enabled && !puresignal_enabled) { ... P1_DDCConfig=1, DDC2, Rate[2] }
//           else if (!diversity_enabled && puresignal_enabled) { ... DDC0+DDC2, PS pair }
//           else if (diversity_enabled && puresignal_enabled) { ... PS wins }
//           else { // diversity_enabled && !puresignal_enabled: DDC0 SyncDDC1 }
//       }
//       if (rx2_enabled) { DDCEnable += DDC3; Rate[3] = rx2_rate; }
//       break;
//
// Phase 3F multi-slice extension:
//   Slice A (index 0) = DDC2 (rx1); Slice B (index 1) = DDC3 (rx2_enabled addendum).
//   Slices C/D/E (indices 2-4) extend to DDC4/5/6 in the plain-RX path only
//   (same as P2CodecSaturn/P2CodecOrionMkII extension). Suppressed during PS
//   and diversity because those modes reclaim DDC0+DDC1 and firmware has no
//   room for extra user-slice receivers.
//
// [2.10.3.13]MW0LGE p1 !  [original inline tag from console.cs:8247, P1-only Rate[0] set]
DdcAssignment P1CodecAnvelinaPro3::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    // DDC bitmask constants from Thetis console.cs:8199-8202 [v2.10.3.15]
    static constexpr int kDDC0 = 1;
    static constexpr int kDDC1 = 2;
    static constexpr int kDDC2 = 4;
    static constexpr int kDDC3 = 8;

    // ps_rate = cmaster.PSrate from Thetis cmaster.cs:425 [v2.10.3.15]
    static constexpr int kPsRate = 192000;

    DdcAssignment a{};

    // From Thetis console.cs:8228-8229 [v2.10.3.15]:
    //   P1_rxcount = 5;  // RX5 used for puresignal feedback
    //   nddc = 5;
    a.p1RxCount = 5;
    a.nDdc      = 5;

    // Slice A (index 0) = DDC2 (rx1_rate); Slice B (index 1) = DDC3 (rx2_rate).
    const int rx1Rate = slices[0].live ? slices[0].sampleRateHz : 0;
    const int rx2Rate = slices[1].live ? slices[1].sampleRateHz : 0;
    const bool rx2Live = slices[1].live;

    if (ctx.puresignalRun && ctx.mox) {
        // From Thetis console.cs:8265-8274 [v2.10.3.15] (!diversity && puresignal):
        //   P1_DDCConfig = 3; DDCEnable = DDC0 + DDC2; SyncEnable = DDC1;
        //   Rate[0] = ps_rate; Rate[1] = ps_rate; Rate[2] = rx1_rate;
        //   cntrl1 = (rx_adc_ctrl1 & 0xf3) | 0x08;
        // (diversity+PS also uses P1_DDCConfig=3, same assignment per console.cs:8276-8285)
        // Adjacent inline tag from console.cs:8260 (within +-5 of cite start):
        //   if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE p1 !
        // [2.10.3.13]MW0LGE p1 !  [original inline tag from console.cs:8260, verbatim]
        a.p1DdcConfig = 3;
        a.ddcEnable   = kDDC0 | kDDC2;
        a.syncEnable  = kDDC1;
        a.rate[0]     = kPsRate;
        a.rate[1]     = kPsRate;
        a.rate[2]     = rx1Rate;
        a.adcCtrl1    = (ctx.p1AdcCntrl & 0xf3) | 0x08;
        a.adcCtrl2    = ctx.p1AdcCntrl & 0x3f;
        a.psFwdDdc    = 0;
        a.psRevDdc    = 1;
        // PS reclaims DDC0+1; no room for extra user slices C/D/E.
        // rx2 (DDC3) is still added if live.
        // Phase 3F Sub-Epic I Task 7b: stream 0 stays on DDC2 (rate[2] above
        // is preserved, not reclaimed by the PS pair).
        if (slices[0].live) { a.streamDdc[0] = 2; }
    } else if (ctx.diversity) {
        // From Thetis console.cs:8232-8240 [v2.10.3.15] (no-mox, diversity):
        // [2.10.3.13]MW0LGE p1 !
        //   P1_DDCConfig = DDCEnable = DDC0;  (P1_DDCConfig = 1 via DDC0=1 bitmask)
        //   SyncEnable = DDC1;
        //   Rate[0] = rx1_rate; Rate[1] = rx1_rate;
        //   cntrl1 = rx_adc_ctrl1 & 0xff; cntrl2 = rx_adc_ctrl2 & 0x3f;
        // Also for mox+diversity+!PS (console.cs:8287-8296):
        //   P1_DDCConfig = 2; DDCEnable = DDC0; SyncEnable = DDC1; same rates.
        a.p1DdcConfig = (ctx.mox ? 2 : kDDC0);  // kDDC0=1 from Thetis no-mox path
        a.ddcEnable   = kDDC0;
        a.syncEnable  = kDDC1;
        a.rate[0]     = rx1Rate;
        a.rate[1]     = rx1Rate;
        a.adcCtrl1    = ctx.p1AdcCntrl & 0xff;
        a.adcCtrl2    = ctx.p1AdcCntrl & 0x3f;
        a.p1Diversity = 1;
        // No room for extra user slices during diversity.
        // Phase 3F Sub-Epic I Task 7b: stream 0 migrates to the DDC0/DDC1
        // sync pair set above (no separate DDC2 rate in this branch).
        if (slices[0].live) { a.streamDdc[0] = 0; }
    } else {
        // From Thetis console.cs:8243-8250 [v2.10.3.15] (no-diversity plain RX):
        //   P1_DDCConfig = 1; DDCEnable = DDC2; SyncEnable = 0;
        //   if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE p1 !
        //   Rate[2] = rx1_rate;
        //   cntrl1 = rx_adc_ctrl1 & 0xff; cntrl2 = rx_adc_ctrl2 & 0x3f;
        // [2.10.3.13]MW0LGE p1 !  [original inline tag from console.cs:8247, P1-only Rate[0] set]
        a.p1DdcConfig = 1;
        a.ddcEnable   = kDDC2;
        a.syncEnable  = 0;
        a.rate[0]     = rx1Rate;  // P1 path: Rate[0] = rx1_rate  [2.10.3.13]MW0LGE p1 !
        a.rate[2]     = rx1Rate;
        a.adcCtrl1    = ctx.p1AdcCntrl & 0xff;
        a.adcCtrl2    = ctx.p1AdcCntrl & 0x3f;
        a.nDdc        = 1;
        // Phase 3F Sub-Epic I Task 7b: stream 0 -> DDC2, plain-RX path.
        if (slices[0].live) { a.streamDdc[0] = 2; }

        // Phase 3F extension: streams 2/3/4 -> DDC4/5/6 in plain-RX path.
        // Thetis nddc=5 cap applies; DDC4/5/6 are idle in Thetis's 2-slice max.
        for (int i = 2; i <= 4; ++i) {
            if (slices[i].live) {
                const int ddc = i + 2;  // stream 2->DDC4, 3->DDC5, 4->DDC6
                // Phase 3F Sub-Epic I Task 7b: publish the mapping explicitly.
                a.streamDdc[i] = ddc;
                a.ddcEnable |= (1 << ddc);
                a.rate[ddc]  = slices[i].sampleRateHz;
                ++a.nDdc;
            }
        }
    }

    // From Thetis console.cs:8299-8303 [v2.10.3.15] (rx2_enabled addendum):
    //   if (rx2_enabled) { DDCEnable += DDC3; Rate[3] = rx2_rate; }
    // Note: adjacent case header at console.cs:8305: case HPSDRModel.REDPITAYA: //DH1KLM
    // //DH1KLM  [original inline tag from console.cs:8305, adjacent to rx2 addendum]
    if (rx2Live) {
        a.ddcEnable |= kDDC3;
        a.rate[3]    = rx2Rate;
        // Phase 3F Sub-Epic I Task 7b: stream 1 -> DDC3, in every branch (the
        // rx2Live addendum runs unconditionally, same as ddcEnable/rate[3]
        // above).
        a.streamDdc[1] = 3;
        if (!(ctx.puresignalRun && ctx.mox) && !ctx.diversity) {
            ++a.nDdc;  // already 1 in plain path, now 2
        }
    }

    return a;
}

} // namespace NereusSDR
