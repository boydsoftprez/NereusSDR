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

DdcAssignment P2CodecSaturn::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    // Saturn / ANAN-G2 / G2-1K (HPSDRModel.ANAN_G2 / ANAN_G2_1K) DDC assignment.
    // Mirrors Thetis console.cs:8220-8304 [v2.10.3.15] UpdateDDCs() G2-class branch
    // for the 1-2 slice case (byte-faithful for RX1/RX2 + PS + diversity).
    //
    // Slice-to-DDC mapping for Saturn-class (2-ADC, 7 DDCs):
    //   Slice A (index 0) -> DDC2    [Thetis: DDCEnable = DDC2 at line 8245]
    //   Slice B (index 1) -> DDC3    [Thetis: DDCEnable += DDC3 at line 8301]
    //   Slice C (index 2) -> DDC4    [NereusSDR extension: idle Thetis DDC4 slot]
    //   Slice D (index 3) -> DDC5    [NereusSDR extension: idle Thetis DDC5 slot]
    //   Slice E (index 4) -> DDC6    [NereusSDR extension: idle Thetis DDC6 slot]
    // DDC0/DDC1 reserved for PS feedback pair or Diversity sync pair.
    //
    // From Thetis console.cs:8199 [v2.10.3.15]:
    //   int DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
    // [2.10.3.13]MW0LGE p1 !  [original inline comment from console.cs:8247, p1 path only]

    DdcAssignment a{};

    // PS feedback DDC rate from Thetis cmaster.cs:425 [v2.10.3.15]:
    //   private static int ps_rate = 192000;
    // From Thetis console.cs:8205 [v2.10.3.15]: int ps_rate = cmaster.PSrate;
    static constexpr int kPsRate = 192000;

    // Slice-to-DDC index table. DDC0 and DDC1 are reserved.
    // From Thetis console.cs:8244-8245 [v2.10.3.15] (DDC2 = Slice A) and
    // console.cs:8301 [v2.10.3.15] (DDC3 = Slice B / rx2_enabled).
    static constexpr int kSliceToDdc[5] = {2, 3, 4, 5, 6};

    // Populate DDC assignments for live slices.
    // For Slice A (index 0) -> DDC2: matches Thetis's rx1 on DDC2.
    // For Slice B (index 1) -> DDC3: matches Thetis's rx2_enabled DDC3 addendum.
    // For Slices C-E -> DDC4-6: NereusSDR extension into Thetis's idle slots.
    for (int i = 0; i < 5; ++i) {
        if (!slices[i].live) { continue; }
        const int ddc = kSliceToDdc[i];
        // Phase 3F Sub-Epic I Task 7: publish the mapping explicitly.
        a.sliceDdc[i] = ddc;
        a.ddcEnable |= (1 << ddc);
        // From Thetis console.cs:8248 [v2.10.3.15]: Rate[2] = rx1_rate;
        // [2.10.3.13]MW0LGE p1 !  [verbatim from console.cs:8247 — P1-only branch on
        // the same RX state; P2 codec does not set Rate[0] here, but tag preserved per
        // CLAUDE.md inline-comment-preservation rule (author tag within +-5 of cite)]
        // From Thetis console.cs:8302 [v2.10.3.15]: Rate[3] = rx2_rate;
        // //DH1KLM  [verbatim from console.cs:8305 — tag on REDPITAYA case header
        // adjacent to the rx2_enabled addendum at 8302; preserved per CLAUDE.md rule]
        a.rate[ddc] = slices[i].sampleRateHz;
        ++a.nDdc;
    }

    // ADC control from Thetis console.cs:8249 [v2.10.3.15]:
    //   cntrl1 = rx_adc_ctrl1 & 0xff;  (default rx_adc_ctrl1=4, console.cs:15099)
    //   cntrl2 = rx_adc_ctrl2 & 0x3f;  (default rx_adc_ctrl2=0, console.cs:15135)
    // ctx.adcCtrl carries rx_adc_ctrl1 in low byte, rx_adc_ctrl2 in high byte.
    a.adcCtrl1 = static_cast<int>(ctx.adcCtrl & 0xff);
    a.adcCtrl2 = static_cast<int>((ctx.adcCtrl >> 8) & 0x3f);

    // PureSignal override. Thetis console.cs:8265-8274 [v2.10.3.15]:
    //   if (!diversity_enabled && puresignal_enabled)  {  // mox path
    //       DDCEnable = DDC0 + DDC2;
    //       SyncEnable = DDC1;
    //       Rate[0] = ps_rate;
    //       Rate[1] = ps_rate;
    //       Rate[2] = rx1_rate;   (Slice A rate preserved)
    //       cntrl1 = (rx_adc_ctrl1 & 0xf3) | 0x08;  // DDC1 -> ADC2 (PA-feedback)
    //   }
    //   Also: console.cs:8276-8285 (diversity + PS): same cntrl1 formula, PS wins.
    if (ctx.puresignalRun && ctx.mox) {
        // PS pair occupies DDC0 (fwd/TX monitor) + DDC1 (rev/PA-feedback).
        a.ddcEnable |= 0x03;                        // set DDC0 + DDC1
        a.syncEnable |= 0x02;                       // DDC1 syncs to DDC0
        a.rate[0] = kPsRate;
        a.rate[1] = kPsRate;
        // From Thetis console.cs:8273 [v2.10.3.15]:
        //   cntrl1 = (rx_adc_ctrl1 & 0xf3) | 0x08;
        //   Clears DDC1 ADC bits (bits 3:2 = 0xf3 mask) and sets DDC1 -> ADC2 (0x08)
        a.adcCtrl1 = (a.adcCtrl1 & 0xf3) | 0x08;
        a.psFwdDdc = 0;
        a.psRevDdc = 1;
        a.nDdc += 2;
    }
    // Diversity migration (PS wins over diversity if both engaged).
    // Thetis console.cs:8232-8240 [v2.10.3.15] (no-mox, diversity path):
    //   DDCEnable = DDC0;
    //   SyncEnable = DDC1;
    //   Rate[0] = rx1_rate;
    //   Rate[1] = rx1_rate;
    //   cntrl1 = rx_adc_ctrl1 & 0xff;
    // Thetis console.cs:8287-8295 [v2.10.3.15] (mox, diversity && !PS):
    //   DDCEnable = DDC0;
    //   SyncEnable = DDC1;
    //   Rate[0] = rx1_rate;
    //   Rate[1] = rx1_rate;
    //   cntrl1 = rx_adc_ctrl1 & 0xff;  // same as no-mox: no PS active
    else if (ctx.diversity) {
        // Slice A migrates: DDC2 is disabled, DDC0+DDC1 sync pair takes over.
        a.ddcEnable &= ~0x04;                       // clear DDC2
        a.ddcEnable |= 0x03;                        // set DDC0 + DDC1
        a.syncEnable |= 0x02;                       // DDC1 syncs to DDC0
        if (slices[0].live) {
            // From Thetis console.cs:8237-8238 [v2.10.3.15]: Rate[0]=Rate[1]=rx1_rate
            a.rate[0] = slices[0].sampleRateHz;
            a.rate[1] = slices[0].sampleRateHz;
            a.rate[2] = 0;
            // Phase 3F Sub-Epic I Task 7: Slice A's DDC moved from DDC2 to
            // the DDC0/DDC1 diversity sync pair set above; republish DDC0
            // as the pair's primary so sliceDdc stays consistent with
            // ddcEnable (same convention as psFwdDdc for the PS pair).
            a.sliceDdc[0] = 0;
        }
        // adcCtrl1 stays as rx_adc_ctrl1 & 0xff (no PS override here)
        // nDdc: was incremented for DDC2 above; swap to DDC0+DDC1 (net delta = +1)
        // Remove DDC2 count, add DDC0+DDC1 count.
        if (slices[0].live) {
            --a.nDdc;    // remove the DDC2 slot counted for Slice A
            a.nDdc += 2; // add DDC0 + DDC1
        } else {
            a.nDdc += 2;
        }
    }

    return a;
}

} // namespace NereusSDR
