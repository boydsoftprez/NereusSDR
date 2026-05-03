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
            const quint64 freq = (rxIdx < ctx.activeRxCount)
                                  ? ctx.rxFreqHz[rxIdx]
                                  : ctx.txFreqHz;  // unused DDCs default to TX freq
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

        // Bank 16 — BPF2
        // Source: networkproto1.c:658-665 [@501e3f5]
        case 16: out[0] = C0base | 0x24; return;

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
    // mic_ptt polarity: DIRECT — matches Thetis networkproto1.c:597-598
    // [v2.10.3.13] (commit @501e3f51):
    //   C1 = ... | ((prn->mic.mic_ptt & 1) << 6);
    // Pre-fix the codec wrote `!ctx.p1MicPTT`, mirroring the inversion bug
    // that PR #161 (commit ca8cd73) fixed in P1CodecHl2 for HL2.  With
    // p1MicPTT default false, the inverted code emitted bit 6 = 1 every CC
    // frame; Hermes-class firmware reads bit 6 as "track mic-jack tip as
    // PTT source", so the floating mic tip caused phantom PTT signals
    // fighting software MOX → rapid T/R relay flutter on TUNE/TX.  Symptom
    // reported on ANAN-10E (HermesII); same root cause hits every board
    // using P1CodecStandard or its subclasses (AnvelinaPro3, RedPitaya).
    // From Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13]
    //   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5)
    //           | ((prn->mic.mic_ptt & 1) << 6);
    //   3M-1b G.3 (mic_trs) + G.4 (mic_bias) + G.5 (mic_ptt — direct as of P1 full-parity)
    out[1] = quint8((ctx.rxPreamp[0] ? 0x01 : 0)
                  | (ctx.rxPreamp[1] ? 0x02 : 0)
                  | (ctx.rxPreamp[2] ? 0x04 : 0)
                  | (ctx.rxPreamp[0] ? 0x08 : 0)         // bit3 = rx0 again (Thetis quirk)
                  | (!ctx.p1MicTipRing ? 0x10 : 0x00)    // mic_trs (inverted) — 3M-1b G.3
                  | (ctx.p1MicBias    ? 0x20 : 0x00)     // mic_bias (no inversion) — 3M-1b G.4
                  | (ctx.p1MicPTT     ? 0x40 : 0x00));   // mic_ptt (DIRECT — matches Thetis networkproto1.c:597-598 [v2.10.3.13])
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

} // namespace NereusSDR
