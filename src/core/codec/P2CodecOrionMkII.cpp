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
// src/core/codec/P2CodecOrionMkII.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/network.c:821-1248 (CmdGeneral,
//   CmdHighPriority, CmdRx, CmdTx packet builders)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Lifted from P2RadioConnection inline compose
//                helpers (extracted in Phase 3P-B Task 1); now delegates
//                here once Task 7 cutover lands.
// =================================================================

#include "P2CodecOrionMkII.h"
#include "CodecContext.h"

namespace NereusSDR {

// --- Static helpers ---

// From network.c: internal writeBE32 pattern used throughout packet builders
void P2CodecOrionMkII::writeBE32(quint8* buf, int offset, quint32 value)
{
    buf[offset]     = (value >> 24) & 0xff;
    buf[offset + 1] = (value >> 16) & 0xff;
    buf[offset + 2] = (value >> 8)  & 0xff;
    buf[offset + 3] =  value        & 0xff;
}

// From network.c:936-1005 [@501e3f5] — NCO phase word calculation.
// freq_hz * 2^32 / 122880000 (ANAN-G2 / OrionMKII clock rate).
//
// Phase 3P-G: `factor` is Thetis' FreqCorrectionFactor (setup.cs:14036-14050).
// Thetis folds the factor into the frequency argument before calling
// Freq2PhaseWord (HPSDR/NetworkIO.cs:251-254); we fold it in here so every
// compose path — direct or via CodecContext — picks up live calibration.
// factor == 1.0 is byte-identical to the pre-calibration formula.
quint32 P2CodecOrionMkII::hzToPhaseWord(quint64 freqHz, double factor)
{
    const double correctedHz = static_cast<double>(freqHz) * factor;
    return static_cast<quint32>((correctedHz * 4294967296.0) / 122880000.0);
}

// --- CmdGeneral (60 bytes) ---

// Porting from Thetis CmdGeneral() network.c:821-909 [@501e3f5]
void P2CodecOrionMkII::composeCmdGeneral(const CodecContext& ctx, quint8 buf[60]) const
{
    // From Thetis network.c:826 [@501e3f5]
    buf[4] = 0x00;  // Command

    // From Thetis network.c:831-876 — PORT assignments
    int tmp;

    // PC outbound source ports (radio receives FROM these)
    // From Thetis network.c:839-857 [@501e3f5]
    tmp = ctx.p2CustomPortBase + 0;  // Rx Specific #1025
    buf[5] = tmp >> 8; buf[6] = tmp & 0xff;
    tmp = ctx.p2CustomPortBase + 1;  // Tx Specific #1026
    buf[7] = tmp >> 8; buf[8] = tmp & 0xff;
    tmp = ctx.p2CustomPortBase + 2;  // High Priority from PC #1027
    buf[9] = tmp >> 8; buf[10] = tmp & 0xff;
    tmp = ctx.p2CustomPortBase + 3;  // Rx Audio #1028
    buf[13] = tmp >> 8; buf[14] = tmp & 0xff;
    tmp = ctx.p2CustomPortBase + 4;  // Tx0 IQ #1029
    buf[15] = tmp >> 8; buf[16] = tmp & 0xff;

    // Radio outbound source ports (radio sends FROM these)
    // From Thetis network.c:860-875 [@501e3f5]
    tmp = ctx.p2CustomPortBase + 0;  // High Priority to PC #1025
    buf[11] = tmp >> 8; buf[12] = tmp & 0xff;
    tmp = ctx.p2CustomPortBase + 10; // Rx0 DDC IQ #1035
    buf[17] = tmp >> 8; buf[18] = tmp & 0xff;
    tmp = ctx.p2CustomPortBase + 1;  // Mic Samples #1026
    buf[19] = tmp >> 8; buf[20] = tmp & 0xff;
    tmp = ctx.p2CustomPortBase + 2;  // Wideband ADC0 #1027
    buf[21] = tmp >> 8; buf[22] = tmp & 0xff;

    // From Thetis network.c:878-888 [@501e3f5] — Wideband settings
    buf[23] = 0;    // wb_enable
    buf[24] = (ctx.p2WbSamplesPerPacket >> 8) & 0xff;
    buf[25] =  ctx.p2WbSamplesPerPacket        & 0xff;
    buf[26] =  ctx.p2WbSampleSize;      // 16 bits
    buf[27] =  ctx.p2WbUpdateRate;      // 70ms
    buf[28] =  ctx.p2WbPacketsPerFrame; // 32

    // From Thetis network.c:896 [@501e3f5] — 0x08 = bit[3] "freq or phase word"
    // Thetis sends 0x08 but stores frequencies as Hz in prn->rx[].frequency
    // Keep this matching Thetis exactly
    buf[37] = 0x08;

    // From Thetis network.c:898 [@501e3f5]
    buf[38] = static_cast<quint8>(ctx.p2Wdt);  // Watchdog timer (0 = disabled)

    // From Thetis network.c:904 [@501e3f5]
    buf[58] = (!ctx.p2TxPa) & 0x01;  // PA enable

    // From Thetis network.c:906 [@501e3f5] — Alex enable (BPF board)
    // prbpfilter->enable | prbpfilter2->enable
    buf[59] = 0x03;  // Enable both Alex0 and Alex1

    // Note: sequence number NOT written here — caller stamps it just
    // before transmission so composeCmdGeneral(ctx, buf) captures a
    // deterministic zero-sequence snapshot for regression baseline purposes.
}

// --- CmdHighPriority (1444 bytes) ---

// Porting from Thetis CmdHighPriority() network.c:913-1063 [@501e3f5]
void P2CodecOrionMkII::composeCmdHighPriority(const CodecContext& ctx, quint8 buf[1444]) const
{
    // From Thetis network.c:924-925 [@501e3f5]
    // packetbuf[4] = (prn->tx[0].ptt_out << 1 | prn->run) & 0xff;
    buf[4] = static_cast<quint8>((ctx.p2PttOut << 1 | (ctx.p2Running ? 1 : 0)) & 0xff);

    // From Thetis network.c:931-933 [@501e3f5]
    buf[5] = static_cast<quint8>((ctx.p2Dash << 2 | ctx.p2Dot << 1 | ctx.p2Cwx) & 0x7);

    // From Thetis network.c:936-1005 [@501e3f5]
    // RX frequencies — 4 bytes each, big-endian phase words.
    // General cmd byte 37 = 0x08 (bit 3) means frequencies are NCO phase words.
    // From pcap analysis: phase_word = freq_hz * 2^32 / 122880000
    // RX0-RX1 have PureSignal override logic; for now use straight frequency
    for (int i = 0; i < kMaxDdc; ++i) {
        int offset = 9 + (i * 4);
        if (offset + 3 < kBufLen) {
            quint32 phaseWord = hzToPhaseWord(ctx.rxFreqHz[i], ctx.freqCorrectionFactor);
            writeBE32(buf, offset, phaseWord);
        }
    }

    // From Thetis network.c:1008-1011 [@501e3f5] — TX0 frequency (also phase word)
    writeBE32(buf, 329, hzToPhaseWord(ctx.txFreqHz, ctx.freqCorrectionFactor));

    // From Thetis network.c:1014 [@501e3f5]
    buf[345] = static_cast<quint8>(ctx.p2DriveLevel);

    // From Thetis network.c:1037-1038 [@501e3f5] — Mercury Attenuator
    buf[1403] = static_cast<quint8>(ctx.p2Rx1Preamp << 1 | ctx.rxPreamp[0]);

    // From Thetis network.c:1055-1057 [@501e3f5] — Step Attenuators
    buf[1442] = static_cast<quint8>(ctx.rxStepAttn[1]);
    buf[1443] = static_cast<quint8>(ctx.rxStepAttn[0]);

    // Alex filter/antenna registers (bytes 1428-1435)
    // From Thetis ChannelMaster/network.c:1040-1050 [@501e3f5]
    // Alex0 (bytes 1432-1435): RX antenna + HPF + LPF
    // Alex1 (bytes 1428-1431): TX antenna + HPF + LPF
    writeBE32(buf, 1432, buildAlex0(ctx));
    writeBE32(buf, 1428, buildAlex1(ctx));
}

// --- CmdRx (1444 bytes) ---

// Porting from Thetis CmdRx() network.c:1066-1179 [@501e3f5]
void P2CodecOrionMkII::composeCmdRx(const CodecContext& ctx, quint8 buf[1444]) const
{
    // From Thetis network.c:1074 [@501e3f5]
    buf[4] = static_cast<quint8>(ctx.p2NumAdc);

    // From Thetis network.c:1080-1082 [@501e3f5] — Dither
    buf[5] = static_cast<quint8>((ctx.dither[2] << 2 | ctx.dither[1] << 1 | ctx.dither[0]) & 0x7);

    // From Thetis network.c:1088-1090 [@501e3f5] — Random
    buf[6] = static_cast<quint8>((ctx.random[2] << 2 | ctx.random[1] << 1 | ctx.random[0]) & 0x7);

    // From Thetis network.c:1097-1103 [@501e3f5] — Enable bitmask
    buf[7] = static_cast<quint8>(
        (ctx.p2RxEnable[6] << 6 | ctx.p2RxEnable[5] << 5 |
         ctx.p2RxEnable[4] << 4 | ctx.p2RxEnable[3] << 3 |
         ctx.p2RxEnable[2] << 2 | ctx.p2RxEnable[1] << 1 |
         ctx.p2RxEnable[0]) & 0xff);

    // From Thetis network.c:1106-1169 [@501e3f5] — Per-RX config
    // Layout: each RX is 6 bytes apart, starting at byte 17
    // byte+0: ADC, byte+1-2: sampling rate, byte+5: bit depth
    for (int i = 0; i < kMaxDdc; ++i) {
        int base = 17 + (i * 6);
        buf[base]     = static_cast<quint8>(ctx.p2RxAdcAssign[i]);
        buf[base + 1] = static_cast<quint8>((ctx.p2RxSamplingRate[i] >> 8) & 0xff);
        buf[base + 2] = static_cast<quint8>(ctx.p2RxSamplingRate[i] & 0xff);
        buf[base + 5] = static_cast<quint8>(ctx.p2RxBitDepth[i]);
    }

    // From Thetis network.c:1172 [@501e3f5]
    buf[1363] = static_cast<quint8>(ctx.p2RxSync);
}

// --- CmdTx (60 bytes) ---

// Porting from Thetis CmdTx() network.c:1181-1248 [@501e3f5]
void P2CodecOrionMkII::composeCmdTx(const CodecContext& ctx, quint8 buf[60]) const
{
    // From Thetis network.c:1188 [@501e3f5]
    buf[4] = static_cast<quint8>(ctx.p2NumDac);

    // From Thetis network.c:1199 [@501e3f5] — CW mode control
    buf[5] = ctx.p2CwModeControl;

    // From Thetis network.c:1202-1216 [@501e3f5]
    buf[6]  = static_cast<quint8>(ctx.p2CwSidetoneLevel);
    buf[7]  = static_cast<quint8>((ctx.p2CwSidetoneFreq >> 8) & 0xff);
    buf[8]  = static_cast<quint8>(ctx.p2CwSidetoneFreq & 0xff);
    buf[9]  = static_cast<quint8>(ctx.p2CwKeyerSpeed);
    buf[10] = static_cast<quint8>(ctx.p2CwKeyerWeight);
    buf[11] = static_cast<quint8>((ctx.p2CwHangDelay >> 8) & 0xff);
    buf[12] = static_cast<quint8>(ctx.p2CwHangDelay & 0xff);
    buf[13] = static_cast<quint8>(ctx.p2CwRfDelay);

    // From Thetis network.c:1218-1220 [@501e3f5] — TX0 sampling rate
    buf[14] = static_cast<quint8>((ctx.p2TxSamplingRate >> 8) & 0xff);
    buf[15] = static_cast<quint8>(ctx.p2TxSamplingRate & 0xff);

    // From Thetis network.c:1222 [@501e3f5]
    buf[17] = static_cast<quint8>(ctx.p2CwEdgeLength & 0xff);

    // From Thetis network.c:1224-1226 [@501e3f5] — TX0 phase shift
    buf[26] = static_cast<quint8>((ctx.p2TxPhaseShift >> 8) & 0xff);
    buf[27] = static_cast<quint8>(ctx.p2TxPhaseShift & 0xff);

    // From Thetis network.c:1234 [@501e3f5] — Mic control
    buf[50] = ctx.p2MicControl;

    // From Thetis network.c:1236 [@501e3f5]
    buf[51] = static_cast<quint8>(ctx.p2MicLineInGain);

    // From Thetis network.c:1238-1242 [@501e3f5] — Step attenuators on TX
    buf[57] = static_cast<quint8>(ctx.txStepAttn[2]);
    buf[58] = static_cast<quint8>(ctx.txStepAttn[1]);
    buf[59] = static_cast<quint8>(ctx.txStepAttn[0]);
}

// --- Alex register builders ---

// Build Alex0 32-bit register (bytes 1432-1435 in CmdHighPriority).
// Contains: RX antenna (bits 24-26), RX-only mux (bits 8-11),
//           LPF (bits 20-31), HPF (bits 0-6).
// From Thetis ChannelMaster/network.h:263-358 bpfilter struct [@501e3f5]
quint32 P2CodecOrionMkII::buildAlex0(const CodecContext& ctx) const
{
    quint32 reg = 0;

    // RX antenna selection — from Thetis netInterface.c:479-485 [@501e3f5]
    // ANT1=0x01, ANT2=0x02, ANT3=0x03 → bits 24-26
    int antBits = ctx.p2AlexRxAnt & 0x03;
    if (antBits == 0x01) {
        reg |= (1u << 24);  // _ANT_1
    } else if (antBits == 0x02) {
        reg |= (1u << 25);  // _ANT_2
    } else if (antBits == 0x03) {
        reg |= (1u << 26);  // _ANT_3
    }

    // RX-only antenna mux — Phase 3P-I-b T5. From Thetis
    // ChannelMaster/netInterface.c:479-481 + network.h:279-282
    // [v2.10.3.13 @501e3f5]. Bit-pair encoding matches SetAntBits():
    //   (rxOnlyAnt & 0x03) == 0x01 → _Rx_1_In  (bit 10, EXT2)
    //   (rxOnlyAnt & 0x03) == 0x02 → _Rx_2_In  (bit 9,  EXT1)
    //   (rxOnlyAnt & 0x03) == 0x03 → _XVTR_Rx_In (bit 8)
    //   rxOut → _Rx_1_Out (bit 11, K36 RL17 RX-Bypass-Out relay)
    //
    // 3M-1a (2026-04-27): bit 27 _TR_Relay + bit 18 _trx_status — both
    // asserted in the Alex0 word during MOX so the radio physically
    // routes the antenna to the TX path.  Without them MOX engages but
    // the antenna stays on RX and no carrier reaches the SO-239.
    // From Thetis network.h:290,300 [v2.10.3.13] (`_trx_status : 1, // bit 18`,
    //   `_TR_Relay : 1, // bit 27`).
    // From deskhpsdr/src/alex.h:91-96 [@120188f]:
    //   #define ALEX_TX_RELAY 0x08000000   // bit 27
    //   #define ALEX_PS_BIT   0x00040000   // bit 18
    // From deskhpsdr/src/new_protocol.c:996-1004 [@120188f] (Alex0 conditional
    //   on `xmit`, Alex1 unconditional — see buildAlex1 below).
    // We follow `ctx.mox` (≡ deskhpsdr `xmit`) rather than `ctx.trxRelay`
    // because the MOX bit is the unambiguous transmit-keying signal — the
    // host-side relay state may lag (hardware-flip ack) but the radio's
    // antenna routing should track MOX directly.
    if (ctx.mox) {
        reg |= (1u << 27);  // _TR_Relay   (ALEX_TX_RELAY)
        reg |= (1u << 18);  // _trx_status (ALEX_PS_BIT)
    }
    {
        const int rxOnlyBits = ctx.rxOnlyAnt & 0x03;
        if (rxOnlyBits == 0x01) {
            reg |= (1u << 10);  // _Rx_1_In  [network.h:281 @501e3f5]
        } else if (rxOnlyBits == 0x02) {
            reg |= (1u <<  9);  // _Rx_2_In  [network.h:280 @501e3f5]
        } else if (rxOnlyBits == 0x03) {
            reg |= (1u <<  8);  // _XVTR_Rx_In [network.h:279 @501e3f5]
        }
        if (ctx.rxOut) {
            reg |= (1u << 11);  // _Rx_1_Out [network.h:282 @501e3f5]
        }
    }

    // LPF bits — from Thetis netInterface.c:682-726 [@501e3f5]
    // Bits map: 30_20[20], 60_40[21], 80[22], 160[23], 6[29], 12_10[30], 17_15[31]
    if (ctx.alexLpfBits & 0x01) { reg |= (1u << 20); }  // 30/20m
    if (ctx.alexLpfBits & 0x02) { reg |= (1u << 21); }  // 60/40m
    if (ctx.alexLpfBits & 0x04) { reg |= (1u << 22); }  // 80m
    if (ctx.alexLpfBits & 0x08) { reg |= (1u << 23); }  // 160m
    if (ctx.alexLpfBits & 0x10) { reg |= (1u << 29); }  // 6m
    if (ctx.alexLpfBits & 0x20) { reg |= (1u << 30); }  // 12/10m
    if (ctx.alexLpfBits & 0x40) { reg |= (1u << 31); }  // 17/15m

    // HPF bits — from Thetis netInterface.c:605-621 [@501e3f5]
    // Bits map: 13MHz[1], 20MHz[2], 6M_preamp[3], 9.5MHz[4], 6.5MHz[5], 1.5MHz[6]
    if (ctx.alexHpfBits & 0x01) { reg |= (1u << 1);  }  // 13 MHz
    if (ctx.alexHpfBits & 0x02) { reg |= (1u << 2);  }  // 20 MHz
    if (ctx.alexHpfBits & 0x04) { reg |= (1u << 4);  }  // 9.5 MHz
    if (ctx.alexHpfBits & 0x08) { reg |= (1u << 5);  }  // 6.5 MHz
    if (ctx.alexHpfBits & 0x10) { reg |= (1u << 6);  }  // 1.5 MHz
    if (ctx.alexHpfBits & 0x20) { reg |= (1u << 12); }  // Bypass
    if (ctx.alexHpfBits & 0x40) { reg |= (1u << 3);  }  // 6M preamp

    return reg;
}

// Build Alex1 32-bit register (bytes 1428-1431 in CmdHighPriority).
// Contains: TX antenna (bits 24-26), same LPF/HPF layout as Alex0.
// From Thetis ChannelMaster/network.h bpfilter2 struct [@501e3f5]
quint32 P2CodecOrionMkII::buildAlex1(const CodecContext& ctx) const
{
    quint32 reg = 0;

    // 3M-1a (2026-04-27): bit 27 _TR_Relay + bit 18 _trx_status are
    // unconditionally set in the Alex1 (TX-side) word.  deskhpsdr asserts
    // both bits on every CmdHighPriority regardless of MOX state — Alex1
    // is the TX antenna control register and these bits indicate the TX
    // signal-chain readiness, not the per-cycle keying state.
    // From deskhpsdr/src/new_protocol.c:998,1004 [@120188f]:
    //   alex1 |= ALEX_TX_RELAY;
    //   alex1 |= ALEX_PS_BIT;
    reg |= (1u << 27);  // _TR_Relay   (ALEX_TX_RELAY = 0x08000000)
    reg |= (1u << 18);  // _trx_status (ALEX_PS_BIT   = 0x00040000)

    // TX antenna selection — same encoding as RX but in Alex1 [@501e3f5]
    int antBits = ctx.p2AlexTxAnt & 0x03;
    if (antBits == 0x01) {
        reg |= (1u << 24);  // _TXANT_1
    } else if (antBits == 0x02) {
        reg |= (1u << 25);  // _TXANT_2
    } else if (antBits == 0x03) {
        reg |= (1u << 26);  // _TXANT_3
    }

    // Same LPF bits as Alex0 (TX uses same LPF selection) [@501e3f5]
    if (ctx.alexLpfBits & 0x01) { reg |= (1u << 20); }
    if (ctx.alexLpfBits & 0x02) { reg |= (1u << 21); }
    if (ctx.alexLpfBits & 0x04) { reg |= (1u << 22); }
    if (ctx.alexLpfBits & 0x08) { reg |= (1u << 23); }
    if (ctx.alexLpfBits & 0x10) { reg |= (1u << 29); }
    if (ctx.alexLpfBits & 0x20) { reg |= (1u << 30); }
    if (ctx.alexLpfBits & 0x40) { reg |= (1u << 31); }

    // Same HPF bits [@501e3f5]
    if (ctx.alexHpfBits & 0x01) { reg |= (1u << 1);  }
    if (ctx.alexHpfBits & 0x02) { reg |= (1u << 2);  }
    if (ctx.alexHpfBits & 0x04) { reg |= (1u << 4);  }
    if (ctx.alexHpfBits & 0x08) { reg |= (1u << 5);  }
    if (ctx.alexHpfBits & 0x10) { reg |= (1u << 6);  }
    if (ctx.alexHpfBits & 0x20) { reg |= (1u << 12); }
    if (ctx.alexHpfBits & 0x40) { reg |= (1u << 3);  }

    return reg;
}

} // namespace NereusSDR
