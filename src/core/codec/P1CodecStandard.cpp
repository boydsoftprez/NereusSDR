// =================================================================
// src/core/codec/P1CodecStandard.cpp  (NereusSDR)
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

#include "P1CodecStandard.h"

namespace NereusSDR {

void P1CodecStandard::composeCcForBank(int bank, const CodecContext& ctx,
                                       quint8 out[5]) const
{
    // C0 low bit = MOX
    const quint8 C0base = ctx.mox ? 0x01 : 0x00;
    // Zero-init out[1..4] before the per-bank switch fills them.
    for (int i = 1; i < 5; ++i) { out[i] = 0; }

    switch (bank) {
        case 0:  bank0(ctx, out);  return;
        case 10: bank10(ctx, out); return;
        case 11: bank11(ctx, out); return;
        case 12: bank12(ctx, out); return;

        // Bank 1 — TX VFO
        // Source: Thetis ChannelMaster/networkproto1.c:476-481 [v2.10.3.13]
        //   C0 = XmitBit; before the switch (line 447).
        //   case 1: C0 |= 2;  → final C0 = XmitBit | 0x02.
        //
        // Frequency banks DO carry the MOX bit (XmitBit) — Thetis sets it on
        // every bank's C0 base.  Prior NereusSDR comment claimed otherwise; that
        // claim was wrong.  Matches HL2 codec (P1CodecHl2.cpp:85) which already
        // emits C0base | 0x02 here.
        case 1: {
            const quint32 hz = quint32(ctx.txFreqHz);
            out[0] = quint8(C0base | 0x02);
            out[1] = quint8((hz >> 24) & 0xFF);
            out[2] = quint8((hz >> 16) & 0xFF);
            out[3] = quint8((hz >>  8) & 0xFF);
            out[4] = quint8( hz        & 0xFF);
            return;
        }

        // Banks 2-3 — RX1/RX2 VFOs (DDC0/DDC1)
        // Source: Thetis ChannelMaster/networkproto1.c:485,498 [v2.10.3.13]
        //   C0 = XmitBit; before the switch (line 447).
        //   case 2: C0 |= 4;  → final C0 = XmitBit | 0x04.
        //   case 3: C0 |= 6;  → final C0 = XmitBit | 0x06.
        //
        // Frequency banks DO carry the MOX bit (XmitBit) — Thetis sets it on
        // every bank's C0 base.  Prior NereusSDR comment claimed otherwise; that
        // claim was wrong.  Matches HL2 codec (P1CodecHl2.cpp:107,119) which
        // already emits C0base | <addr> here.
        case 2: case 3: {
            // bank 2 → rxIdx 0 (C0 = 0x04), bank 3 → rxIdx 1 (C0 = 0x06)
            static const quint8 kRx01C0[] = { 0x04, 0x06 };
            const int rxIdx = bank - 2;
            out[0] = quint8(C0base | kRx01C0[rxIdx]);

            // Phase 3M-4 Task 17 P1 follow-up: PureSignal DDC0/DDC1 freq override.
            //
            // From mi0bot ChannelMaster/networkproto1.c:982-1009 [v2.10.3.13-beta2]
            // (byte-for-byte identical to ramdor :484-511 [v2.10.3.13]):
            //   case 2: //RX1 VFO (DDC0)
            //       if ((nddc == 2) && (XmitBit == 1) && (prn->puresignal_run))
            //           ddc_freq = prn->tx[0].frequency;
            //       else
            //           ddc_freq = prn->rx[0].frequency;
            //   case 3: //RX2 VFO (DDC1)
            //       if ((nddc == 2) && (XmitBit == 1) && (prn->puresignal_run))
            //           ddc_freq = prn->tx[0].frequency;
            //       else if (nddc == 5)
            //           ddc_freq = prn->rx[0].frequency;
            //       else
            //           ddc_freq = prn->rx[1].frequency; //Hermes RX2 freq
            //
            // Mapping (CodecContext fields populated by P1RadioConnection
            // ::buildCodecContext from m_psNDdc / m_mox / m_puresignalRun):
            //   nddc                  ≡  ctx.p1PsNDdc
            //   XmitBit == 1          ≡  ctx.mox
            //   prn->puresignal_run   ≡  ctx.p1PuresignalRun
            //
            // For nddc==4 boards (HL2 / Hermes / ANAN10 / ANAN100), the
            // override is NOT applied — the firmware handles freq routing
            // internally via cntrl1=4 ADC-to-DDC steering (mi0bot
            // console.cs:8486 [v2.10.3.13-beta2]).
            quint64 freq;
            if (rxIdx >= ctx.activeRxCount) {
                freq = ctx.txFreqHz;  // unused DDCs default to TX freq
            } else if (ctx.p1PsNDdc == 2 && ctx.mox && ctx.p1PuresignalRun) {
                freq = ctx.txFreqHz;  // HermesII PS-MOX override (both DDC0 + DDC1)
            } else if (rxIdx == 1 && ctx.p1PsNDdc == 5) {
                freq = ctx.rxFreqHz[0];  // Orion: DDC1 = RX1 freq (case 3 nddc==5 branch)
            } else {
                freq = ctx.rxFreqHz[rxIdx];  // standard: DDC{rxIdx} = RX{rxIdx} VFO
            }
            const quint32 hz = quint32(freq);
            out[1] = quint8((hz >> 24) & 0xFF);
            out[2] = quint8((hz >> 16) & 0xFF);
            out[3] = quint8((hz >>  8) & 0xFF);
            out[4] = quint8( hz        & 0xFF);
            return;
        }

        // Bank 4 — ADC-to-DDC routing + TX step attenuator
        // Source: networkproto1.c:517-523 [@501e3f5]
        case 4:
            out[0] = C0base | 0x1C;
            out[1] = quint8(ctx.adcCtrl & 0xFF);
            out[2] = quint8((ctx.adcCtrl >> 8) & 0x3F);
            out[3] = quint8(ctx.txStepAttn[0] & 0x1F);
            out[4] = 0;
            return;

        // Banks 5-9 — RX3-RX7 VFOs (DDC2-DDC6)
        // Source: Thetis ChannelMaster/networkproto1.c:526,539,549,560,569 [v2.10.3.13]
        //   C0 = XmitBit; before the switch (line 447).
        //   case 5: C0 |= 8;     → final C0 = XmitBit | 0x08.
        //   case 6: C0 |= 0x0a;  → final C0 = XmitBit | 0x0A.
        //   case 7: C0 |= 0x0c;  → final C0 = XmitBit | 0x0C.
        //   case 8: C0 |= 0x0e;  → final C0 = XmitBit | 0x0E.
        //   case 9: C0 |= 0x10;  → final C0 = XmitBit | 0x10.
        // Unused DDCs get TX freq as a safe default.
        //
        // Frequency banks DO carry the MOX bit (XmitBit) — Thetis sets it on
        // every bank's C0 base.  Prior NereusSDR comment claimed otherwise; that
        // claim was wrong.  Matches HL2 codec (P1CodecHl2.cpp:155) which already
        // emits C0base | kRxC0Addr[bank - 5] here.
        case 5: case 6: case 7: case 8: case 9: {
            // bank 5 → rxIdx 2 (C0 = 0x08), ..., bank 9 → rxIdx 6 (C0 = 0x10)
            // Address table: rxIdx 2=0x08, 3=0x0A, 4=0x0C, 5=0x0E, 6=0x10
            static const quint8 kRxC0Addr[] = { 0x08, 0x0A, 0x0C, 0x0E, 0x10 };
            const int rxIdx = bank - 3;  // bank 5 → rxIdx 2, bank 9 → rxIdx 6
            out[0] = quint8(C0base | kRxC0Addr[bank - 5]);
            const quint64 freq = (rxIdx < ctx.activeRxCount)
                                  ? ctx.rxFreqHz[rxIdx]
                                  : ctx.txFreqHz;
            const quint32 hz = quint32(freq);
            out[1] = quint8((hz >> 24) & 0xFF);
            out[2] = quint8((hz >> 16) & 0xFF);
            out[3] = quint8((hz >>  8) & 0xFF);
            out[4] = quint8( hz        & 0xFF);
            return;
        }

        // Bank 13 — CW enable / sidetone level
        // Source: networkproto1.c:634-638 [@501e3f5]
        case 13: out[0] = C0base | 0x1E; return;

        // Bank 14 — CW hang / sidetone freq
        // Source: networkproto1.c:642-646 [@501e3f5]
        case 14: out[0] = C0base | 0x20; return;

        // Bank 15 — EER PWM
        // Source: networkproto1.c:650-654 [@501e3f5]
        case 15: out[0] = C0base | 0x22; return;

        // Bank 16 — BPF2 + xvtr_enable + puresignal_run
        // Source: Thetis ChannelMaster/networkproto1.c:657-666 [v2.10.3.13]:
        //   case 16: // BPF2
        //       C0 |= 0x24;
        //       C1 = (BPF2 HPF/LPF/preamp bits + rx2_gnd<<7);
        //       C2 = (xvtr_enable & 1) | ((prn->puresignal_run & 1) << 6);
        //       C3 = 0;
        //       C4 = 0;
        //
        // v0.4.1-rc2 bench-fix (J.J. KG4VCF, 2026-05-09): pre-fix this was a
        // stub (C0 only, C1-C4 = 0).  Bench evidence from a friend's ANAN-10E
        // running v0.4.1-rc1 showed PsccPump pumping 1361 blocks straight
        // with TX envelope 0.20 (-14 dBFS) but feedback envelope stuck at
        // 0.0003 (-70 dBFS, ADC noise floor); calcc reached LCOLLECT
        // (state=4) but never converged.  Thetis emits ps_run on BOTH bank
        // 11 C2 bit 6 AND bank 16 C2 bit 6 — without the bank-16 bit, the
        // FPGA fires the cmaster.cs bookkeeping but the PA-loopback
        // physical path stays disengaged on Hermes-class boards.  Same fix
        // landed in P1CodecHl2 on 2026-05-07 (PR #212 follow-up bench-fix);
        // this is the parity propagation to P1CodecStandard.
        //
        // BPF2 (HL2's secondary BPF board / Hermes-class HPF2) state isn't
        // tracked in NereusSDR yet — emit C1 = 0 (no BPF2 routing) until
        // BPF2 support lands.  xvtr_enable is also not yet plumbed —
        // emit 0.  Only the ps_run bit is wired here; that's what the
        // FPGA needs for PS feedback to engage.
        case 16:
            out[0] = C0base | 0x24;
            out[1] = 0;     // BPF2 filter bits — not plumbed in NereusSDR yet
            out[2] = quint8(ctx.p1PuresignalRun ? 0x40 : 0x00);  // ps_run only
            out[3] = 0;
            out[4] = 0;
            return;

        // Bank 17 — AnvelinaPro3 extra OC pins
        // Source: networkproto1.c:668-669 [@501e3f5] — "HPSDRModel_ANVELINAPRO3 only"
        //
        // Bug-parity note: the pre-refactor legacy composeCcForBankLegacy sent
        // C0base | 0x26 here for ALL boards, not just AnvelinaPro3.  The baseline
        // JSON (Task 1) captures that behavior, so the codec must replicate it
        // for byte-identical output.  Phase B can reclaim this to AP3-only.
        case 17: out[0] = C0base | 0x26; return;

        default:
            out[0] = C0base;
            return;
    }
}

// Bank 0 — General settings: sample rate, OC, preamp/dither/random,
// antenna, duplex, NDDC, diversity.
// Source: networkproto1.c:446-471 [@501e3f5/@c26a8a4 — identical]
void P1CodecStandard::bank0(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x00;
    out[1] = quint8(ctx.sampleRateCode & 0x03);
    out[2] = quint8((ctx.ocByte << 1) & 0xFE);
    // C3: rxPreamp + dither + random + RX-only mux + RX-bypass-out.
    // Source: networkproto1.c:453-468 [v2.10.3.13 @501e3f5]
    // Bits 5-6: RX-only mux — From Thetis netInterface.c:479-481 [v2.10.3.13 @501e3f5]
    //   prbpfilter->_Rx_1_In    = (rx_only_ant & (0x01 | 0x02)) == 0x01;  // 1 → bit5
    //   prbpfilter->_Rx_2_In    = (rx_only_ant & (0x01 | 0x02)) == 0x02;  // 2 → bit6
    //   prbpfilter->_XVTR_Rx_In = (rx_only_ant & (0x01 | 0x02)) == (0x01 | 0x02); // 3 → bits5+6
    // Bit 7: _Rx_1_Out (RX-Bypass-Out relay) — networkproto1.c:455 [v2.10.3.13 @501e3f5]
    quint8 c3 = quint8((ctx.rxPreamp[0] ? 0x04 : 0)
                     | (ctx.dither[0]   ? 0x08 : 0)
                     | (ctx.random[0]   ? 0x10 : 0));
    switch (ctx.rxOnlyAnt) {
        case 1: c3 |= 0b0010'0000; break;  // _Rx_1_In
        case 2: c3 |= 0b0100'0000; break;  // _Rx_2_In
        case 3: c3 |= 0b0110'0000; break;  // _XVTR_Rx_In
        default: break;                     // 0 = no RX-only path selected
    }
    if (ctx.rxOut) {
        c3 |= 0b1000'0000;  // _Rx_1_Out relay
    }
    out[3] = c3;
    // C4: antenna, duplex, NDDC-1, diversity (networkproto1.c:463-471)
    out[4] = quint8((ctx.antennaIdx & 0x03)
                  | (ctx.duplex ? 0x04 : 0)
                  | (((ctx.activeRxCount - 1) & 0x0F) << 3)
                  | (ctx.diversity ? 0x80 : 0));
}

// Bank 10 — TX drive, mic, Alex HPF/LPF, T/R relay
// Source: networkproto1.c:579-590 [@501e3f5]
// T/R relay bit (C3 bit 7) is INVERTED: 0 = relay engaged, 1 = relay disabled.
// Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
//   if (txband->disablePA || !pa_enabled)
//       output_buffer[C3] |= 0x80; // disable Alex T/R relay
void P1CodecStandard::bank10(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x12;
    out[1] = quint8(ctx.txDrive & 0xFF);
    // C2: mic_boost → bit 0 (0x01); line_in → bit 1 (0x02); bit 6 always set per upstream default.
    // From Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
    //   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ... | 0b01000000) & 0x7f;
    out[2] = quint8((ctx.p1MicBoost ? 0x01 : 0x00) | (ctx.p1LineIn ? 0x02 : 0x00) | 0x40);
    out[3] = quint8(ctx.alexHpfBits | (ctx.trxRelay ? 0x00 : 0x80));  // T/R relay engaged (INVERTED: 1 = disabled)
    out[4] = quint8(ctx.alexLpfBits);
}

// Bank 11 — Preamp + RX step ATT ADC0 (5-bit mask + 0x20 enable)
// Source: networkproto1.c:594-601 [@501e3f5]
void P1CodecStandard::bank11(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x14;
    // C1: preamp bits 0-3 (bit 3 = rx0 again, Thetis quirk) + mic_trs bit 4
    //     + mic_bias bit 5 + mic_ptt bit 6.
    // mic_trs polarity inversion: wire bit set when tip is BIAS/PTT (!tipHot).
    // mic_bias polarity: 1 = bias on (no inversion).
    // mic_ptt polarity: direct — bit set when PTT is disabled at firmware,
    // matching Thetis console.cs:19764 [v2.10.3.13+501e3f51]:
    //   NetworkIO.SetMicPTT(Convert.ToInt32(mic_ptt_disabled));
    // From Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13+501e3f51]
    //   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5)
    //           | ((prn->mic.mic_ptt & 1) << 6);
    out[1] = quint8((ctx.rxPreamp[0] ? 0x01 : 0)
                  | (ctx.rxPreamp[1] ? 0x02 : 0)
                  | (ctx.rxPreamp[2] ? 0x04 : 0)
                  | (ctx.rxPreamp[0] ? 0x08 : 0)             // bit3 = rx0 again (Thetis quirk)
                  | (!ctx.p1MicTipRing      ? 0x10 : 0x00)   // mic_trs (inverted) — 3M-1b G.3
                  | (ctx.p1MicBias          ? 0x20 : 0x00)   // mic_bias (no inversion) — 3M-1b G.4
                  | (ctx.p1MicPTTDisabled   ? 0x40 : 0x00)); // mic_ptt (direct, issue #182)
    // C2: line_in_gain (low 5 bits) | puresignal_run (bit 6).
    // From Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    out[2] = quint8((ctx.p1LineInGain & 0x1F)
                  | (ctx.p1PuresignalRun ? 0x40 : 0x00));
    // C3: user digital outputs, low 4 bits.
    // From Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13]
    //   C3 = prn->user_dig_out & 0b00001111;
    out[3] = quint8(ctx.p1UserDigOut & 0x0F);
    // canonical 5-bit ramdor encoding
    out[4] = quint8((ctx.rxStepAttn[0] & 0x1F) | 0x20);
}

// Bank 12 — Step ATT ADC1/2 + CW keyer
// Source: networkproto1.c:606-628 [@501e3f5]
//
// ADC1 carve-out: during MOX, force 0x1F UNLESS RedPitaya. Standard
// codec is non-RedPitaya; RedPitaya subclass overrides this method.
// Upstream inline attribution (networkproto1.c:612, preserved verbatim):
//   if (HPSDRModel == HPSDRModel_REDPITAYA) //[2.10.3.9]DH1KLM  //model needed as board type (prn->discovery.BoardType) is an OrionII
void P1CodecStandard::bank12(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x16;
    if (ctx.mox) {
        out[1] = 0x1F | 0x20;  // forced max
    } else {
        out[1] = quint8(ctx.rxStepAttn[1] & 0xFF) | 0x20;
    }
    out[2] = quint8((ctx.rxStepAttn[2] & 0x1F) | 0x20);
    // C3 / C4 = CW keyer defaults — zero for Phase A; Phase 3M-2 wires CW.
    out[3] = 0;
    out[4] = 0;
}

// =================================================================
// Phase 3M-4 Task 5: PureSignal DDC config — per-board branches
// =================================================================
//
// Verbatim port of the per-HpsdrModel branches in Thetis console.cs
// UpdateDDCs().  P1CodecStandard handles every model the codec selector
// routes through it (P1RadioConnection.cpp:1626 default branch):
//   - HPSDR (Atlas)                      -> no PS, return zeros
//   - HERMES, ANAN10, ANAN100            -> psDdcConfigHermesClass
//   - ANAN10E, ANAN100B                  -> psDdcConfigHermesIIClass
//   - ANAN100D, ANAN200D, ORIONMKII,
//     ANAN7000D, ANAN8000D, ANAN_G2,
//     ANAN_G2_1K                         -> psDdcConfigG2Class
//
// AnvelinaPro3 overrides this method to delegate to psDdcConfigG2Class
// (Thetis groups it with G2-class at console.cs:8218).  RedPitaya
// overrides this method with its own branch (Thetis console.cs:8296).
//
// Source: Thetis console.cs:8186-8525 [v2.10.3.13]

PsDdcConfig P1CodecStandard::applyPureSignalDdcConfig(
    HPSDRModel model,
    bool psEnabled,
    bool diversityEnabled,
    bool moxState,
    int rx1Rate,
    int rx2Rate,
    bool rx2Enabled,
    quint8 adcCtrl1,
    quint8 adcCtrl2) const
{
    switch (model) {
        // From Thetis console.cs:8211-8218 [v2.10.3.13]
        case HPSDRModel::ANAN100D:
        case HPSDRModel::ANAN200D:
        case HPSDRModel::ORIONMKII:
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2_1K:
        case HPSDRModel::ANVELINAPRO3:  // AnvelinaPro3 also G2-class in Thetis switch
            return psDdcConfigG2Class(psEnabled, diversityEnabled, moxState,
                                      rx1Rate, rx2Rate, rx2Enabled,
                                      adcCtrl1, adcCtrl2);

        // From Thetis console.cs:8378-8380 [v2.10.3.13]
        case HPSDRModel::HERMES:
        case HPSDRModel::ANAN10:
        case HPSDRModel::ANAN100:
            return psDdcConfigHermesClass(psEnabled, diversityEnabled, moxState,
                                          rx1Rate, rx2Rate, rx2Enabled);

        // From Thetis console.cs:8451-8452 [v2.10.3.13]
        case HPSDRModel::ANAN10E:
        case HPSDRModel::ANAN100B:
            return psDdcConfigHermesIIClass(psEnabled, diversityEnabled, moxState,
                                            rx1Rate, rx2Rate, rx2Enabled);

        // From Thetis console.cs:8523-8524 [v2.10.3.13]
        //   case HPSDRModel.HPSDR:
        //       break;
        // (no DDC config emitted for Atlas — no PS hardware)
        case HPSDRModel::HPSDR:
        // HERMESLITE handled by P1CodecHl2 override, not this codec.
        case HPSDRModel::HERMESLITE:
        // REDPITAYA handled by P1CodecRedPitaya override.
        case HPSDRModel::REDPITAYA:
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:
        default:
            return PsDdcConfig{};
    }
}

// G2-class branch — from Thetis console.cs:8211-8295 [v2.10.3.13]
//
// case HPSDRModel.ANAN100D:
// case HPSDRModel.ANAN200D:
// case HPSDRModel.ORIONMKII:
// case HPSDRModel.ANAN7000D:
// case HPSDRModel.ANAN8000D:
// case HPSDRModel.ANAN_G2:
// case HPSDRModel.ANAN_G2_1K:
// case HPSDRModel.ANVELINAPRO3:
//     P1_rxcount = 5;                     // RX5 used for puresignal feedback
//     nddc = 5;
//     ...
//
// `ps_rate` from cmaster.cs:424 [v2.10.3.13]: private static int ps_rate = 192000;
PsDdcConfig P1CodecStandard::psDdcConfigG2Class(
    bool psEnabled, bool diversityEnabled, bool moxState,
    int rx1Rate, int rx2Rate, bool rx2Enabled,
    quint8 adcCtrl1, quint8 adcCtrl2) const
{
    PsDdcConfig cfg;
    constexpr uint8_t DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
    constexpr int ps_rate = 192000;

    // From console.cs:8219-8220 [v2.10.3.13]
    cfg.p1RxCount = 5;
    cfg.nDdc      = 5;

    if (!moxState) {
        if (diversityEnabled) {
            // From console.cs:8223-8232 [v2.10.3.13]
            // P1_DDCConfig =       (Thetis fall-through assignment; defaults to 0)
            cfg.p1DdcConfig = 0;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>(adcCtrl1 & 0xff);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);
        } else {
            // From console.cs:8233-8242 [v2.10.3.13]
            cfg.p1DdcConfig = 1;
            cfg.ddcEnable   = DDC2;
            cfg.syncEnable  = 0;
            // From console.cs:8238 [v2.10.3.13]: if (p1) Rate[0] = rx1_rate;  // [2.10.3.13]MW0LGE p1 !
            // P1CodecStandard only runs in Protocol 1 path → p1 == true → set Rate[0]
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>(adcCtrl1 & 0xff);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);
        }
    } else {
        if (!diversityEnabled && !psEnabled) {
            // From console.cs:8246-8255 [v2.10.3.13]
            cfg.p1DdcConfig = 1;
            cfg.ddcEnable   = DDC2;
            cfg.syncEnable  = 0;
            // [2.10.3.13]MW0LGE p1 !
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>(adcCtrl1 & 0xff);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);
        } else if (!diversityEnabled && psEnabled) {
            // From console.cs:8256-8266 [v2.10.3.13]
            cfg.p1DdcConfig = 3;
            cfg.ddcEnable   = static_cast<uint8_t>(DDC0 + DDC2);
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = ps_rate;
            cfg.rate[1]     = ps_rate;
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>((adcCtrl1 & 0xf3) | 0x08);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);

            // Phase 3M-4 mi0bot audit: PS DDC pair indices for nddc=5
            // (G2-class P1 — Orion/MKII/ANAN-7000D/8000D/G2/G2_1K/AnvelinaPro3
            // running in P1 mode, rare).
            //
            // From mi0bot networkproto1.c:388-392 [v2.10.3.13-beta2]
            // MetisReadThreadMainLoop case 5:
            //   case 5:
            //       twist(spr, 0, 1, 0);           // DDC0+DDC1 → source 0 (synchronous, NOT PS)
            //       twist(spr, 3, 4, 1);           // DDC3+DDC4 → source 1 (PS pair!)
            //       xrouter(0, 0, 2, spr, prn->RxBuff[2]);  // DDC2 → source 2 (main RX)
            //
            // Plus mi0bot console.cs:8710-8714 [v2.10.3.13-beta2] GetDDC()
            // Orion-class P1 PS-MOX (tot=5):
            //   rx1 = 0; rx2 = 2; psrx = 3; pstx = 4;
            cfg.psFbDdc  = 3;
            cfg.txMonDdc = 4;
        } else if (diversityEnabled && psEnabled) {
            // From console.cs:8267-8277 [v2.10.3.13]
            cfg.p1DdcConfig = 3;
            cfg.ddcEnable   = static_cast<uint8_t>(DDC0 + DDC2);
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = ps_rate;
            cfg.rate[1]     = ps_rate;
            cfg.rate[2]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>((adcCtrl1 & 0xf3) | 0x08);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);

            // Same PS DDC layout as the !diversity && PS branch above —
            // diversity affects only the routing of RX1+RX2 audio paths,
            // not the PS feedback DDC indices.
            cfg.psFbDdc  = 3;
            cfg.txMonDdc = 4;
        } else {
            // diversity_enabled && !puresignal_enabled
            // From console.cs:8278-8287 [v2.10.3.13]
            cfg.p1DdcConfig = 2;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = static_cast<uint8_t>(adcCtrl1 & 0xff);
            cfg.cntrl2      = static_cast<uint8_t>(adcCtrl2 & 0x3f);
        }
    }

    // From console.cs:8290-8294 [v2.10.3.13]
    if (rx2Enabled) {
        cfg.ddcEnable = static_cast<uint8_t>(cfg.ddcEnable + DDC3);
        cfg.rate[3]   = static_cast<uint32_t>(rx2Rate);
    }

    return cfg;
}

// HERMES-class branch — from Thetis console.cs:8378-8449 [v2.10.3.13]
//
// case HPSDRModel.HERMES:
// case HPSDRModel.ANAN10:
// case HPSDRModel.ANAN100:
//     P1_rxcount = 4;                     // RX4 used for puresignal feedback
//     nddc = 4;
//     ...
//
// `ps_rate` from cmaster.cs:424 [v2.10.3.13]: private static int ps_rate = 192000;
PsDdcConfig P1CodecStandard::psDdcConfigHermesClass(
    bool psEnabled, bool diversityEnabled, bool moxState,
    int rx1Rate, int rx2Rate, bool rx2Enabled) const
{
    PsDdcConfig cfg;
    constexpr uint8_t DDC0 = 1, DDC1 = 2;
    constexpr int ps_rate = 192000;

    // From console.cs:8381-8382 [v2.10.3.13]
    cfg.p1RxCount = 4;
    cfg.nDdc      = 4;

    if (!moxState) {
        if (!diversityEnabled) {
            // From console.cs:8385-8398 [v2.10.3.13]
            cfg.p1DdcConfig = 4;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = 0;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;

            if (rx2Enabled) {
                cfg.ddcEnable = static_cast<uint8_t>(cfg.ddcEnable + DDC1);
                cfg.rate[1]   = static_cast<uint32_t>(rx2Rate);
            }
        } else {
            // From console.cs:8400-8409 [v2.10.3.13]
            cfg.p1DdcConfig = 5;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;
        }
    } else {
        if (!diversityEnabled && !psEnabled) {
            // From console.cs:8413-8426 [v2.10.3.13]
            cfg.p1DdcConfig = 4;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = 0;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;

            if (rx2Enabled) {
                cfg.ddcEnable = static_cast<uint8_t>(cfg.ddcEnable + DDC1);
                cfg.rate[1]   = static_cast<uint32_t>(rx2Rate);
            }
        } else if (diversityEnabled && !psEnabled) {
            // From console.cs:8428-8437 [v2.10.3.13]
            cfg.p1DdcConfig = 5;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;
        } else { // transmitting and PS is ON
            // From console.cs:8438-8447 [v2.10.3.13]
            //   else // transmitting and PS is ON
            //   {
            //       P1_DDCConfig = 6;
            //       DDCEnable = DDC0;
            //       SyncEnable = DDC1;
            //       Rate[0] = ps_rate;
            //       Rate[1] = ps_rate;
            //       cntrl1 = 4;
            //       cntrl2 = 0;
            //   }
            cfg.p1DdcConfig = 6;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = ps_rate;
            cfg.rate[1]     = ps_rate;
            cfg.cntrl1      = 4;
            cfg.cntrl2      = 0;

            // Phase 3M-4 mi0bot audit: PS DDC pair indices for HERMES /
            // ANAN-10 / ANAN-100 (nddc=4 family — same as HL2).
            //
            // From mi0bot ChannelMaster/networkproto1.c:383-387
            // [v2.10.3.13-beta2] MetisReadThreadMainLoop case 4 (also
            // appears at 549-553 in the HL2 read loop, byte-identical):
            //   xrouter(0, 0, 0, spr, prn->RxBuff[0]);   // DDC0 → main RX
            //   twist(spr, 2, 3, 1);                      // DDC2+DDC3 → PS pair
            //   xrouter(0, 0, 2, spr, prn->RxBuff[1]);    // DDC1 → secondary RX
            //
            // Plus mi0bot console.cs:8757-8762 [v2.10.3.13-beta2] GetDDC()
            // Hermes P1 PS-MOX (tot=5: MOX=1, Diversity=0, PS=1):
            //   rx1 = 0; rx2 = 1; psrx = 2; pstx = 3;
            //
            // Same DDC layout as HL2 — the cntrl1=4 ADC routing puts the
            // PS feedback on slots 2 (loopback) + 3 (TX monitor).
            cfg.psFbDdc  = 2;
            cfg.txMonDdc = 3;
        }
    }

    return cfg;
}

// HermesII-class branch — from Thetis console.cs:8451-8521 [v2.10.3.13]
//
// case HPSDRModel.ANAN10E:
// case HPSDRModel.ANAN100B:
//     P1_rxcount = 2;                     // RX2 used for puresignal feedback
//     nddc = 2;
//     ...
//
// `ps_rate` from cmaster.cs:424 [v2.10.3.13]: private static int ps_rate = 192000;
PsDdcConfig P1CodecStandard::psDdcConfigHermesIIClass(
    bool psEnabled, bool diversityEnabled, bool moxState,
    int rx1Rate, int rx2Rate, bool rx2Enabled) const
{
    PsDdcConfig cfg;
    constexpr uint8_t DDC0 = 1, DDC1 = 2;
    constexpr int ps_rate = 192000;

    // From console.cs:8453-8454 [v2.10.3.13]
    cfg.p1RxCount = 2;
    cfg.nDdc      = 2;

    if (!moxState) {
        if (!diversityEnabled) {
            // From console.cs:8457-8470 [v2.10.3.13]
            cfg.p1DdcConfig = 4;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = 0;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;

            if (rx2Enabled) {
                cfg.ddcEnable = static_cast<uint8_t>(cfg.ddcEnable + DDC1);
                cfg.rate[1]   = static_cast<uint32_t>(rx2Rate);
            }
        } else {
            // From console.cs:8472-8481 [v2.10.3.13]
            cfg.p1DdcConfig = 5;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;
        }
    } else {
        if (!diversityEnabled && !psEnabled) {
            // From console.cs:8485-8498 [v2.10.3.13]
            cfg.p1DdcConfig = 4;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = 0;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;

            if (rx2Enabled) {
                cfg.ddcEnable = static_cast<uint8_t>(cfg.ddcEnable + DDC1);
                cfg.rate[1]   = static_cast<uint32_t>(rx2Rate);
            }
        } else if (diversityEnabled && !psEnabled) {
            // From console.cs:8500-8509 [v2.10.3.13]
            cfg.p1DdcConfig = 5;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;
        } else { // transmitting and PS is ON
            // From console.cs:8510-8519 [v2.10.3.13]
            //   else // transmitting and PS is ON
            //   {
            //       P1_DDCConfig = 5;
            //       DDCEnable = DDC0;
            //       SyncEnable = DDC1;
            //       Rate[0] = ps_rate;
            //       Rate[1] = ps_rate;
            //       cntrl1 = 4;
            //       cntrl2 = 0;
            //   }
            cfg.p1DdcConfig = 5;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = ps_rate;
            cfg.rate[1]     = ps_rate;
            cfg.cntrl1      = 4;
            cfg.cntrl2      = 0;

            // Phase 3M-4 mi0bot audit: PS DDC pair indices for HermesII /
            // ANAN-10E / ANAN-100B (nddc=2 family).
            //
            // From mi0bot ChannelMaster/networkproto1.c:380-381
            // [v2.10.3.13-beta2] MetisReadThreadMainLoop case 2:
            //   case 2:
            //       twist(spr, 0, 1, 0);     // DDC0+DDC1 → PS pair (source 0)
            //       break;
            //
            // Plus mi0bot console.cs:8798-8800 [v2.10.3.13-beta2] GetDDC()
            // HermesII P1 PS-MOX (tot=5: MOX=1, Diversity=0, PS=1):
            //   psrx = 0;  pstx = 1;
            //
            // The bank-2/3 freq override (commit `9bde052`) forces DDC0+DDC1
            // to TX freq during PS-MOX, so DDC0 = PS feedback, DDC1 = TX
            // monitor.  These match the global cmaster.cs:533-534 defaults.
            cfg.psFbDdc  = 0;     // explicit for documentation; matches default
            cfg.txMonDdc = 1;     // explicit for documentation; matches default
        }
    }

    return cfg;
}

} // namespace NereusSDR
