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

// =================================================================
// P1CodecRedPitaya::applyDdcAssignment
// =================================================================
//
// Porting from Thetis console.cs:8305-8385 [v2.10.3.15] UpdateDDCs()
// RedPitaya-specific case. RedPitaya has its OWN separate case (not shared
// with OrionMkII/AnvelinaPro3), attributed //DH1KLM throughout.
//
// Key difference from OrionMkII-class: Rate[0] and Rate[1] are always set
// to rx1_rate even in the plain (non-diversity, non-PS) RX path, per
// // REDPITAYA PAVEL inline comments.
//
// Also: 384k sample rate is supported via include_extra_p1_rate flag:
//   bool include_extra_p1_rate = HardwareSpecific.Model == HPSDRModel.REDPITAYA; //DH1KLM
//   From Thetis setup.cs:847-849 [v2.10.3.15]
//   The 384k rate flows naturally through SliceConfig.sampleRateHz.
//
// Original C# (relevant fragment):
//
//   case HPSDRModel.REDPITAYA: //DH1KLM
//       P1_rxcount = 5;  // RX5 used for puresignal feedback
//       nddc = 5;
//       if (!_mox)
//       {
//           if (diversity_enabled)
//           {
//               P1_DDCConfig = 2; // REDPITAYA PAVEL
//               DDCEnable = DDC0; SyncEnable = DDC1;
//               Rate[0] = rx1_rate; Rate[1] = rx1_rate;
//               Rate[2] = rx1_rate; // REDPITAYA PAVEL
//               cntrl1 = rx_adc_ctrl1 & 0xff; cntrl2 = rx_adc_ctrl2 & 0x3f;
//           }
//           else
//           {
//               P1_DDCConfig = 1; DDCEnable = DDC2; SyncEnable = 0;
//               Rate[0] = rx1_rate; // REDPITAYA PAVEL
//               Rate[1] = rx1_rate; // REDPITAYA PAVEL
//               Rate[2] = rx1_rate;
//               cntrl1 = rx_adc_ctrl1 & 0xff; cntrl2 = rx_adc_ctrl2 & 0x3f;
//           }
//       }
//       else { ... PS / diversity-mox branches, also with REDPITAYA PAVEL Rate extras }
//       if (rx2_enabled) { DDCEnable += DDC3; Rate[3] = rx2_rate; }
//
// //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header, verbatim]
DdcAssignment P1CodecRedPitaya::applyDdcAssignment(
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

    // From Thetis console.cs:8306-8307 [v2.10.3.15]: //DH1KLM
    //   P1_rxcount = 5;  // RX5 used for puresignal feedback
    //   nddc = 5;
    // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header]
    a.p1RxCount = 5;
    a.nDdc      = 5;

    const int rx1Rate = slices[0].live ? slices[0].sampleRateHz : 0;
    const int rx2Rate = slices[1].live ? slices[1].sampleRateHz : 0;
    const bool rx2Live = slices[1].live;

    if (ctx.puresignalRun && ctx.mox) {
        // From Thetis console.cs:8346-8355 [v2.10.3.15] (!diversity && puresignal_enabled):
        //   P1_DDCConfig = 3; DDCEnable = DDC0 + DDC2; SyncEnable = DDC1;
        //   Rate[0] = ps_rate; Rate[1] = ps_rate; Rate[2] = rx1_rate;
        //   cntrl1 = (rx_adc_ctrl1 & 0xf3) | 0x08;
        // (diversity+PS identical per console.cs:8357-8366)
        // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header]
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
        // Phase 3F Sub-Epic I Task 7b: stream 0 stays on DDC2 (rate[2] above
        // is preserved, not reclaimed by the PS pair).
        if (slices[0].live) { a.streamDdc[0] = 2; }
    } else if (ctx.diversity) {
        // From Thetis console.cs:8310-8319 [v2.10.3.15] (no-mox, diversity):
        //   P1_DDCConfig = 2; // REDPITAYA PAVEL
        //   DDCEnable = DDC0; SyncEnable = DDC1;
        //   Rate[0] = rx1_rate; Rate[1] = rx1_rate;
        //   Rate[2] = rx1_rate; // REDPITAYA PAVEL
        //   cntrl1 = rx_adc_ctrl1 & 0xff; cntrl2 = rx_adc_ctrl2 & 0x3f;
        // Also mox+diversity+!PS at console.cs:8368-8378:
        //   P1_DDCConfig = 2; DDCEnable = DDC0; SyncEnable = DDC1;
        //   Rate[0]=Rate[1]=rx1_rate; Rate[2] = rx1_rate; // REDPITAYA PAVEL
        // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header]
        // // REDPITAYA PAVEL  [original inline tag from console.cs:8312, P1_DDCConfig override]
        // // REDPITAYA PAVEL  [original inline tag from console.cs:8317, Rate[2] in diversity]
        a.p1DdcConfig = 2; // REDPITAYA PAVEL
        a.ddcEnable   = kDDC0;
        a.syncEnable  = kDDC1;
        a.rate[0]     = rx1Rate;
        a.rate[1]     = rx1Rate;
        a.rate[2]     = rx1Rate; // REDPITAYA PAVEL
        a.adcCtrl1    = ctx.p1AdcCntrl & 0xff;
        a.adcCtrl2    = ctx.p1AdcCntrl & 0x3f;
        a.p1Diversity = 1;
        // Phase 3F Sub-Epic I Task 7b: stream 0 migrates to the DDC0/DDC1
        // sync pair set above. ddcEnable carries only kDDC0 here (DDC2's
        // rate[2] REDPITAYA PAVEL quirk above does not enable DDC2 itself).
        if (slices[0].live) { a.streamDdc[0] = 0; }
    } else {
        // From Thetis console.cs:8321-8330 [v2.10.3.15] (no-diversity, plain RX):
        //   P1_DDCConfig = 1; DDCEnable = DDC2; SyncEnable = 0;
        //   Rate[0] = rx1_rate; // REDPITAYA PAVEL
        //   Rate[1] = rx1_rate; // REDPITAYA PAVEL
        //   Rate[2] = rx1_rate;
        //   cntrl1 = rx_adc_ctrl1 & 0xff; cntrl2 = rx_adc_ctrl2 & 0x3f;
        // Note: Rate[0] and Rate[1] always set even in non-PS mode (REDPITAYA PAVEL)
        // vs. OrionMkII which only sets Rate[0] for the P1 path ([2.10.3.13]MW0LGE p1!)
        // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header]
        // // REDPITAYA PAVEL  [original inline tag from console.cs:8326, Rate[0] always set]
        // // REDPITAYA PAVEL  [original inline tag from console.cs:8327, Rate[1] always set]
        a.p1DdcConfig = 1;
        a.ddcEnable   = kDDC2;
        a.syncEnable  = 0;
        a.rate[0]     = rx1Rate; // REDPITAYA PAVEL
        a.rate[1]     = rx1Rate; // REDPITAYA PAVEL
        a.rate[2]     = rx1Rate;
        a.adcCtrl1    = ctx.p1AdcCntrl & 0xff;
        a.adcCtrl2    = ctx.p1AdcCntrl & 0x3f;
        a.nDdc        = 1;
        // Phase 3F Sub-Epic I Task 7b: stream 0 -> DDC2, plain-RX path.
        if (slices[0].live) { a.streamDdc[0] = 2; }

        // Phase 3F extension: streams 2/3/4 -> DDC4/5/6 in plain-RX path.
        // 384k flows through naturally via SliceConfig.sampleRateHz
        // (include_extra_p1_rate flag in setup.cs:847 [v2.10.3.15] //DH1KLM
        //  controls the sample rate combo list; the codec just carries through).
        // //DH1KLM  [original tag from console.cs:8305; rate selection is a setup.cs
        //            concern; the codec treats rx1Rate as opaque]
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

    // From Thetis console.cs:8381-8385 [v2.10.3.15]:
    //   if (rx2_enabled) { DDCEnable += DDC3; Rate[3] = rx2_rate; }
    // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header]
    // Note: console.cs:8386 break falls through to the Hermes-class case at 8387-8388:
    //   case HPSDRModel.ANAN_G2E: //N1GP G2E added   [adjacent inline tag from console.cs:8388]
    if (rx2Live) {
        a.ddcEnable |= kDDC3;
        a.rate[3]    = rx2Rate;
        // Phase 3F Sub-Epic I Task 7b: stream 1 -> DDC3, in every branch (the
        // rx2Live addendum runs unconditionally, same as ddcEnable/rate[3]
        // above).
        a.streamDdc[1] = 3;
        if (!(ctx.puresignalRun && ctx.mox) && !ctx.diversity) {
            ++a.nDdc;
        }
    }

    return a;
}

} // namespace NereusSDR
