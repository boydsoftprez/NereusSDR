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
// src/core/codec/P2CodecHermes.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources (multi-source) [v2.10.3.15 / 3759d09]:
//   Project Files/Source/ChannelMaster/network.c:821-1248
//     (P2 packet shape — inherited from P2CodecOrionMkII)
//   Project Files/Source/Console/console.cs:8387-8459
//     (UpdateDDCs() Hermes-class branch — nddc=4, rx1 on DDC0)
//   Project Files/Source/Console/console.cs:8610-8642
//     (GetDDC() P2 Hermes-class branch — rx1=0, rx2=1)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-25 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Phase 3F Sub-Epic I Task 7c. Extends
//                P2CodecOrionMkII by overriding applyDdcAssignment() with
//                the Hermes-class stream-to-DDC table (stream 0 -> DDC0)
//                so the 1-ADC boards running community P2 firmware stop
//                inheriting the 2-ADC DDC2 layout.
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

#include "P2CodecHermes.h"

#include "core/DdcAssignment.h"

namespace NereusSDR {

// =================================================================
// P2CodecHermes::applyDdcAssignment — Hermes-class (1 ADC, 4 DDCs)
// =================================================================
//
// Porting from Thetis console.cs:8387-8459 [v2.10.3.15] UpdateDDCs()
// Hermes-class branch:
//
//   case HPSDRModel.HERMES:
//   case HPSDRModel.ANAN_G2E: //N1GP G2E added
//   case HPSDRModel.ANAN10:
//   case HPSDRModel.ANAN100:
//       P1_rxcount = 4;                     // RX4 used for puresignal feedback
//       nddc = 4;
//       if (!_mox) {
//           if (!diversity_enabled) {
//               P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
//               Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
//               if (rx2_enabled) { DDCEnable += DDC1; Rate[1] = rx2_rate; }
//           } else {
//               P1_DDCConfig = 5; DDCEnable = DDC0; SyncEnable = DDC1;
//               Rate[0] = rx1_rate; Rate[1] = rx1_rate;
//               cntrl1 = 0; cntrl2 = 0;
//           }
//       } else {
//           if (!diversity_enabled && !puresignal_enabled) {
//               P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
//               Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
//               if (rx2_enabled) { DDCEnable += DDC1; Rate[1] = rx2_rate; }
//           } else if (diversity_enabled && !puresignal_enabled) {
//               P1_DDCConfig = 5; DDCEnable = DDC0; SyncEnable = DDC1;
//               Rate[0] = rx1_rate; Rate[1] = rx1_rate;
//               cntrl1 = 0; cntrl2 = 0;
//           } else { // transmitting and PS is ON
//               P1_DDCConfig = 6; DDCEnable = DDC0; SyncEnable = DDC1;
//               Rate[0] = ps_rate; Rate[1] = ps_rate; cntrl1 = 4; cntrl2 = 0;
//           }
//       }
//
// Cross-checked against console.cs:8610-8642 [v2.10.3.15] GetDDC() P2
// Hermes-class branch, which resolves the same placement from the other
// direction:
//
//   case HPSDRHW.Hermes: // ANAN-10 ANAN-100 Heremes
//   case HPSDRHW.HermesII: // ANAN-10E ANAN-100B HeremesII
//   case HPSDRHW.HermesC10: // ANAN-G2E //N1GP G2E added (HermesC10)
//       switch (tot) {
//           case 0: // off off off
//               rx1 = 0;
//               rx2 = 1;
//               break;
//           case 1: // off off on
//               rx1 = 0;   //MW0LGE_22b missed out
//               rx2 = 1;
//               break;
//
// DDC0=1, DDC1=2 bitmask values from Thetis console.cs:8199 [v2.10.3.15]:
//   int DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
// ps_rate = cmaster.PSrate = 192000 from cmaster.cs:425 [v2.10.3.15].
//
// The structural difference from the 2-ADC codec, and the whole point of this
// class: DDCEnable = DDC0 is UNCONDITIONAL here. Stream 0 sits on DDC0 in
// every (mox x diversity x PS) state; only DDC1's role changes, between
// carrying rx2 and acting as the sync partner for the diversity or PureSignal
// pair. The 2-ADC branch instead migrates stream 0 off DDC2 onto the DDC0/DDC1
// pair, which is why P2CodecOrionMkII::applyDdcAssignment has to clear DDC2.
//
// Phase 3F multi-stream extension: Thetis's Hermes branch places rx1 on DDC0
// and rx2 on DDC1 and caps the family at nddc=4 (console.cs:8391-8392
// [v2.10.3.15]), so streams 2 and 3 extend additively into DDC2 and DDC3 —
// Thetis's two idle slots for this family — in the plain-RX path only. Streams
// 2 and 3 are suppressed while PS or diversity is active because those states
// reclaim DDC0+DDC1 as a synced pair, matching the P1 sibling
// (P1CodecStandard::applyDdcAssignment, same upstream branch). Stream 4 is
// never assigned on Hermes-class: nddc=4 leaves no fifth DDC, and inventing
// one would be a guess Thetis does not support.
//
// //N1GP G2E added  [original inline tag from console.cs:8388 — ANAN_G2E case label]
// //MW0LGE_22b missed out  [original inline tag from console.cs:8620 — rx1 = 0 on tot==1]
DdcAssignment P2CodecHermes::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    // From Thetis console.cs:8199 [v2.10.3.15]: int DDC0 = 1, DDC1 = 2, ...
    static constexpr int kDDC0 = 1;
    static constexpr int kDDC1 = 2;

    // PS feedback DDC rate from Thetis cmaster.cs:425 [v2.10.3.15]:
    //   private static int ps_rate = 192000;
    // From Thetis console.cs:8205 [v2.10.3.15]: int ps_rate = cmaster.PSrate;
    static constexpr int kPsRate = 192000;

    DdcAssignment a{};

    // From Thetis console.cs:8391-8392 [v2.10.3.15]:
    //   P1_rxcount = 4;                     // RX4 used for puresignal feedback
    //   nddc = 4;
    // p1RxCount is P1-only and ignored by the P2 wire path (it feeds
    // NetworkIO.Protocol1DDCConfig at console.cs:8534); carried here so the
    // struct reports the same family shape the upstream branch does.
    // Read through familyP1RxCount() so P2CodecHermesII reports 2 per
    // console.cs:8463 [v2.10.3.15] without restating this branch.
    //N1GP G2E added  [original inline tag from console.cs:8388 — ANAN_G2E case label]
    a.p1RxCount = familyP1RxCount();

    // From Thetis console.cs:8396 / 8409 [v2.10.3.15]: Rate[0] = rx1_rate;
    // From Thetis console.cs:8400 / 8413 [v2.10.3.15]: Rate[1] = rx2_rate;
    // Phase 3F Sub-Epic I Task 7b: `slices` is indexed by DDC STREAM, not by
    // slice, so co-hosted slices share one entry and therefore one DDC.
    const int rx1Rate = slices[0].live ? slices[0].sampleRateHz : 0;
    const int rx2Rate = slices[1].live ? slices[1].sampleRateHz : 0;

    // Stream 0 -> DDC0 in every branch below (all three set DDCEnable = DDC0);
    // set once here rather than duplicated per branch. This is the assertion
    // that has to agree with P2RadioConnection::primaryRxDdcForBoard, which
    // returns 0 for Hermes / HermesII / HermesC10 citing
    // console.cs:8610-8642 [v2.10.3.15] plus a wire-byte capture of a working
    // Thetis-on-G2E session (CmdRx byte 7 = 0x01, the DDC0 enable bit).
    //MW0LGE_22b missed out  [original inline tag from console.cs:8620 — rx1 = 0 on tot==1]
    //
    // Set per branch below rather than once here. Two of the three branches do
    // put stream 0 on DDC0, but the PureSignal branch reclaims DDC0 as the
    // feedback leg and leaves no user receiver at all, so a mapping made
    // before the branch runs survives into a state where it is false.
    // publishDdcAssignment would then route stream 0 to DDC0, report it
    // available instead of suspended, and push PA-feedback samples through the
    // ordinary RX DSP path. (Codex review, PR #293.)

    if (ctx.puresignalRun && ctx.mox) {
        // From Thetis console.cs:8449-8456 [v2.10.3.15]:
        //   else // transmitting and PS is ON
        //   {
        //       P1_DDCConfig = 6; DDCEnable = DDC0; SyncEnable = DDC1;
        //       Rate[0] = ps_rate; Rate[1] = ps_rate;
        //       cntrl1 = 4; cntrl2 = 0;
        //   }
        // HermesII's PS branch uses P1_DDCConfig = 5 instead
        // (console.cs:8522 [v2.10.3.15]); hence familyPsP1DdcConfig().
        a.p1DdcConfig = familyPsP1DdcConfig();
        a.ddcEnable   = kDDC0;
        a.syncEnable  = kDDC1;
        a.rate[0]     = kPsRate;
        a.rate[1]     = kPsRate;
        a.adcCtrl1    = 4;
        a.adcCtrl2    = 0;

        // P2 1-ADC PS pair on the wire: DDC0 (PS feedback) + DDC1 (TX monitor).
        // Same indices the inherited psDdcConfigHermesClass emits, per Thetis
        // cmaster.cs:538-539 [v2.10.3.15]:
        //   SetPSRxIdx(0, 0);   // Stream0 for RX feedback
        //   SetPSTxIdx(0, 1);   // Stream1 for TX feedback
        a.psFwdDdc = 0;
        a.psRevDdc = 1;

        // PS reclaims DDC0+DDC1; no room for extra user streams.
        a.nDdc = 2;
    } else if (ctx.diversity) {
        // From Thetis console.cs:8437-8446 [v2.10.3.15]:
        //   else if (diversity_enabled && !puresignal_enabled) {
        //       P1_DDCConfig = 5; DDCEnable = DDC0; SyncEnable = DDC1;
        //       Rate[0] = rx1_rate; Rate[1] = rx1_rate;
        //       cntrl1 = 0; cntrl2 = 0; }
        // Identical to the no-mox diversity path at console.cs:8408-8417.
        a.p1DdcConfig = 5;
        a.ddcEnable   = kDDC0;
        a.syncEnable  = kDDC1;
        a.rate[0]     = rx1Rate;
        a.rate[1]     = rx1Rate;
        a.adcCtrl1    = 0;
        a.adcCtrl2    = 0;
        a.p1Diversity = 1;
        a.nDdc = 2;
        // Stream 0 really is on DDC0 here: the pair's primary carries it, and
        // DDC1 is the synchronized leg rather than a second user stream.
        if (slices[0].live) { a.streamDdc[0] = 0; }
    } else {
        // From Thetis console.cs:8393-8407 [v2.10.3.15] (no-mox, no-diversity)
        // and console.cs:8421-8435 (mox, no-diversity, no-PS) — identical
        // bodies upstream:
        //   P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
        //   Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
        //   if (rx2_enabled) { DDCEnable += DDC1; Rate[1] = rx2_rate; }
        //N1GP G2E added  [original inline tag from console.cs:8388 — ANAN_G2E case label]
        a.p1DdcConfig = 4;
        a.ddcEnable   = kDDC0;
        a.syncEnable  = 0;
        a.rate[0]     = rx1Rate;
        a.adcCtrl1    = 0;
        a.adcCtrl2    = 0;
        a.nDdc        = 1;
        if (slices[0].live) { a.streamDdc[0] = 0; }

        if (slices[1].live) {
            // From Thetis console.cs:8399-8400 [v2.10.3.15]:
            //   DDCEnable += DDC1; Rate[1] = rx2_rate;
            a.ddcEnable += kDDC1;
            a.rate[1]    = rx2Rate;
            a.streamDdc[1] = 1;
            ++a.nDdc;
        }

        // Phase 3F extension: streams 2+ fill Thetis's idle DDC slots
        // additively, bounded by the family's nddc — 4 for Hermes-class
        // (console.cs:8392 [v2.10.3.15]), 2 for HermesII (console.cs:8464), so
        // a 2-DDC board is never handed DDC2 or DDC3. Plain-RX path only, since
        // PS and diversity reclaim DDC0+DDC1 above.
        for (int i = 2; i < familyDdcCount() && i < 5; ++i) {
            if (!slices[i].live) { continue; }
            a.ddcEnable |= (1 << i);
            a.rate[i]    = slices[i].sampleRateHz;
            a.streamDdc[i] = i;
            ++a.nDdc;
        }
        // Stream 4 is never assigned on either 1-ADC family: nddc caps at 4
        // (console.cs:8392 [v2.10.3.15]), so no fifth DDC exists here.
    }

    return a;
}

} // namespace NereusSDR
