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
// Ported from Thetis sources (multi-source) [v2.10.3.13]:
//   Project Files/Source/ChannelMaster/network.c:821-1248
//     (P2 CmdGeneral / CmdHighPriority / CmdRx / CmdTx packet builders)
//   Project Files/Source/Console/console.cs:8211-8521
//     (UpdateDDCs() per-board PureSignal DDC config: G2-class
//     8211-8295, Hermes-class 8378-8449, HermesII-class 8451-8521)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Lifted from P2RadioConnection inline compose
//                helpers (extracted in Phase 3P-B Task 1); now delegates
//                here once Task 7 cutover lands.
//   2026-05-17 — applyPureSignalDdcConfig dispatches on HPSDRModel and
//                ports the HermesII (ANAN-10E / ANAN-100B) and Hermes
//                (ANAN-10 / ANAN-100) P2 branches of Thetis console.cs
//                UpdateDDCs() (lines 8378-8521 [v2.10.3.13]). Closes
//                issue #263: ANAN-10E running community P2 firmware
//                disconnected after the 2 s connect watchdog because the
//                G2-class branch placed RX1 on DDC2 instead of DDC0.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
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
    // From Thetis network.c:879 [v2.10.3.15] - wb_enable mask, bit N = ADCN.
    // Phase 3F Sub-Epic F Task 1 wired this from a hardcoded 0 placeholder to
    // ctx.p2WbEnableMask. Threaded via CodecContext from
    // P2RadioConnection::wbEnableMask() in buildCodecContext.
    buf[23] = ctx.p2WbEnableMask;
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
    //
    // Phase 3M-4 Task 17 fix: RX0/RX1 have a PureSignal override.  When
    // (ptt_out && puresignal_run) both true, DDC0 and DDC1 frequencies
    // are overridden to TX freq so both DDCs down-convert at the TX
    // signal's centre — that's what feeds calcc the post-PA loopback
    // (DDC0) and TX-monitor (DDC1) at the right baseband.  Mirrors
    // Thetis network.c:936-945 [v2.10.3.13]:
    //   packetbuf[9..12]  = (ptt_out && puresignal_run)
    //                       ? tx_freq_phase : rx0_freq_phase;
    //   packetbuf[13..16] = (ptt_out && puresignal_run)
    //                       ? tx_freq_phase : rx1_freq_phase;
    // RX2..RX6 are not overridden — they always use their own freq.
    const bool psFreqOverride = (ctx.p2PttOut != 0) && ctx.puresignalRun;
    const quint32 txPhaseWord =
        hzToPhaseWord(ctx.txFreqHz, ctx.freqCorrectionFactor);
    for (int i = 0; i < kMaxDdc; ++i) {
        int offset = 9 + (i * 4);
        if (offset + 3 < kBufLen) {
            quint32 phaseWord;
            if (psFreqOverride && (i == 0 || i == 1)) {
                phaseWord = txPhaseWord;
            } else {
                phaseWord = hzToPhaseWord(ctx.rxFreqHz[i],
                                          ctx.freqCorrectionFactor);
            }
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

        // RX bypass / RX MASTER IN SEL relay encoding — issue #257.
        //
        // From Thetis ChannelMaster/netInterface.c:461-477 [v2.10.3.13 @501e3f51]:
        //   if (mkiibpf)
        //   {
        //       if (rx_only_ant == 1 || tx) // set rx bypass only if Ext2 enabled
        //       {
        //           prbpfilter->_Rx_1_Out = (rx_out & 0x01) != 0; // RX BYPASS OUT RL17
        //           prbpfilter->_10_dB_Atten = 0; // RX MASTER IN SEL RL22
        //       }
        //       else
        //       {
        //           prbpfilter->_Rx_1_Out = 0; // RX BYPASS OUT RL17
        //           prbpfilter->_10_dB_Atten = rx_out & 0x1; // RX MASTER IN SEL RL22
        //       }
        //   }
        //   else
        //   {
        //       prbpfilter->_Rx_1_Out = (rx_out & 0x01) != 0; // RX BYPASS OUT RL17
        //   }
        //
        // Non-Mk II boards (HERMES / ANAN10/100/200D / REDPITAYA) keep the
        // legacy single-relay encoding: bit 11 (_Rx_1_Out, RL17) carries
        // rx_out unconditionally.
        //
        // Mk II BPF boards (ORIONMKII / ANAN-7000D / ANAN-8000D / ANAN_G2 /
        // ANAN_G2_1K / ANVELINAPRO3) split it:
        //   - EXT2 (rx_only_ant==1) OR transmitting → bit 11 (_Rx_1_Out, RL17)
        //   - Everything else (EXT1 / BYPS / XVTR while receiving) →
        //       bit 14 (_10_dB_Atten, the "RX MASTER IN SEL" RL22 line on
        //       Mk II BPF — same wire bit, different physical relay).
        //
        // ctx.mox carries the MOX (transmit) bit; matches Thetis SetAntBits()
        // `tx` parameter (Alex.cs:401 `NetworkIO.SetAntBits(rx_only_ant,
        // trx_ant, tx_ant, rx_out, tx);`).
        //
        // Bit-14 reference: network.h:285 `_10_dB_Atten : 1, // bit 14
        // (RX MASTER IN SEL RL22)`.
        if (ctx.mkiiBpf) {
            const bool ext2OrTx = (rxOnlyBits == 0x01) || ctx.mox;
            if (ext2OrTx) {
                if (ctx.rxOut) {
                    reg |= (1u << 11);  // _Rx_1_Out RL17 — EXT2 or TX path
                }
            } else {
                if (ctx.rxOut) {
                    reg |= (1u << 14);  // _10_dB_Atten / RX MASTER IN SEL RL22
                }
            }
        } else {
            if (ctx.rxOut) {
                reg |= (1u << 11);  // _Rx_1_Out [network.h:282 @501e3f5]
            }
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

    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): SOURCE-FIRST CORRECTION
    // (strict diff against Thetis-locked G2E pcap).
    //
    // The prior body unconditionally set bit 27 (_TR_Relay) AND bit 18
    // (_trx_status) on Alex1, citing deskhpsdr behavior.  Wire-byte diff
    // against Thetis 2.10.3.15 on the SAME radio (G2E) during PS+MOX
    // showed Alex1 = 0x01440100 (bits 8, 18, 22, 24 — NO bit 27).  Our
    // code emitted Alex1 = 0x09241020 (bits 5, 12, 18, 21, 24, 27).
    //
    // The deskhpsdr citation does NOT match Thetis behavior on the G2E,
    // and the project rule is Thetis-faithful porting.  Per Thetis
    // ChannelMaster/netInterface.c:381 [v2.10.3.13]:
    //   prbpfilter2->_trx_status = prbpfilter->_TR_Relay; // TXRX_STATUS for Alex1
    // i.e. Alex1's bit 18 (_trx_status) MIRRORS Alex0's bit 27 (_TR_Relay)
    // value, but Alex1 itself NEVER sets bit 27.  prbpfilter2->_TR_Relay
    // does not exist in the Thetis setter chain.
    //
    // So Alex1 bit 18 follows the MOX state (because Alex0 bit 27 = MOX),
    // and Alex1 bit 27 stays clear.  This was a critical latent bug — the
    // extra bit 27 on Alex1 was driving the radio's TX antenna relay path
    // in a way Thetis doesn't, which may have been causing the PS
    // feedback DDC to receive garbage data on the G2E (calcc never
    // reached LSTAYON regardless of every other fix we tried).
    if (ctx.mox) {
        reg |= (1u << 18);  // _trx_status mirrors Alex0's _TR_Relay

        // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): Alex1 bit 8 (_rx2_gnd)
        // on MOX-on per Thetis console.cs:29091 HdwMOXChanged [v2.10.3.13]:
        //   if (tx) { ...
        //       if (bpf2_gnd) NetworkIO.SetBPF2Gnd(1);
        //   }
        // Default bpf2_gnd = true at console.cs:10903 [v2.10.3.13], so
        // Alex1 bit 8 fires on every MOX engagement unless the user has
        // explicitly disabled it.  Bench-confirmed in Thetis-locked G2E
        // pcap (Alex1=0x01440100 during MOX has bit 8 set).  Without
        // this, the Alex2 BPF chain doesn't ground the rx2 port during
        // TX, which can affect the PS feedback path on Mk II BPF boards
        // (G2E is mkiiBpf=true).
        // NereusSDR-divergence: we don't yet expose a "bpf2_gnd" user
        // checkbox.  Hard-coded to true (Thetis default).
        reg |= (1u << 8);   // _rx2_gnd (BPF2 ground during TX)
    }

    // TX antenna selection — same encoding as RX but in Alex1 [@501e3f5]
    int antBits = ctx.p2AlexTxAnt & 0x03;
    if (antBits == 0x01) {
        reg |= (1u << 24);  // _TXANT_1
    } else if (antBits == 0x02) {
        reg |= (1u << 25);  // _TXANT_2
    } else if (antBits == 0x03) {
        reg |= (1u << 26);  // _TXANT_3
    }

    // LPF bits — the TRANSMIT low-pass, from the transmit frequency.
    //
    // This used to read ctx.alexLpfBits ("Same LPF bits as Alex0 (TX uses
    // same LPF selection)"), which is wrong: Alex0's low-pass is a RECEIVE
    // selection whenever the radio is not keyed. Mirroring it here meant a
    // receive retune onto a low band silently moved the transmit low-pass
    // below the carrier, so keying a high band drove the PA into a low-pass
    // several octaves down.
    //
    // Thetis keeps the two words on separate masks and only ever feeds this
    // one from the transmit frequency:
    //   From Thetis ChannelMaster/netInterface.c:688-704 [v2.10.3.15]
    //     if (isMox || isTX) { ... Alex1LPFMask = bits; }
    //   From Thetis console.cs:15464-15468 UpdateTXDDSFreq [v2.10.3.15]
    //     setAlexLPF(tx_dds_freq_mhz, true);
    // Upstream inline attribution preserved verbatim (console.cs:15471):
    //   if (MOX)//[2.10.3.13]MW0LGE
    if (ctx.alexLpfBitsTx & 0x01) { reg |= (1u << 20); }
    if (ctx.alexLpfBitsTx & 0x02) { reg |= (1u << 21); }
    if (ctx.alexLpfBitsTx & 0x04) { reg |= (1u << 22); }
    if (ctx.alexLpfBitsTx & 0x08) { reg |= (1u << 23); }
    if (ctx.alexLpfBitsTx & 0x10) { reg |= (1u << 29); }
    if (ctx.alexLpfBitsTx & 0x20) { reg |= (1u << 30); }
    if (ctx.alexLpfBitsTx & 0x40) { reg |= (1u << 31); }

    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): the HPF Bypass-on-PureSignal
    // override that this commit landed on Alex0 (bit 12) must NOT leak
    // into Alex1.  Per Thetis ChannelMaster/netInterface.c [v2.10.3.13]:
    //   SetAlexHPFBits(0x20)  → prbpfilter->_Bypass  = 1   (Alex0 bit 12)
    //   SetAlex2HPFBits(0x20) → prbpfilter2->_Bypass = 1   (Alex1 bit 12)
    // Two SEPARATE setters write two SEPARATE storage fields; the PS-FB
    // HPF bypass at console.cs:6957 setBPF1ForOrionIISaturn only fires
    // SetAlexHPFBits (Alex0).  Wire-byte diff against Thetis-locked G2E
    // pcap confirms: Thetis Alex1=0x01440100 (bit 12 CLEAR) while we
    // emitted 0x09241020 (bit 12 SET).  Mask off the bypass bit from
    // Alex1's HPF input before encoding.
    //
    // Phase 3F: that mirror is the FALLBACK, used only when nothing is
    // receiving on ADC1. When a slice set is live on ADC1, AlexController has
    // an answer for that chain and it is the authoritative one — Alex1's HPF
    // is a chain of its own, not a copy of Alex0's. Thetis feeds it from a
    // separate source entirely (setAlex2HPF(rx2_dds_freq_mhz) →
    // SetAlex2HPFBits → prbpfilter2, console.cs:15435-15442 [v2.10.3.15]),
    // and in that case bit 12 IS Alex1's own bypass.
    //
    // DO NOT DELETE THE MIRROR because diversity looks after itself now.
    // It reads like the thing that keeps diversity's two legs matched, and
    // until the D1 fix it accidentally was: with nothing ever reaching ADC1,
    // every diversity run took this branch and got Alex0's filter copied
    // across, which is the right answer for the wrong reason. The right
    // reason now lives in RadioModel::republishAlexAdcSlices, which counts
    // every slice on BOTH chains while the DDC0/DDC1 sync pair is engaged, so
    // ctx.alexHpfBitsAdc1 is a real decision there and equals ADC0's by
    // construction -- including when that decision is bypass, which this
    // branch could never express because it masks 0x20 off.
    //
    // The mirror is still load-bearing for its own case: no diversity, and no
    // slice on ADC1. That is the state the G2E pcap was captured in, and
    // Thetis really does leave prbpfilter2's HPF nibble alone there. Removing
    // it would start writing a chain-1 word out of a decision nobody made.
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    //   From Thetis ChannelMaster/netInterface.c:634-651 [v2.10.3.15]
    //     prbpfilter2->_Bypass = (bits & 0x20) != 0;
    const bool haveAdc1Decision = (ctx.alexHpfBitsAdc1 >= 0);
    const quint8 hpfBitsAlex1 =
        haveAdc1Decision ? static_cast<quint8>(ctx.alexHpfBitsAdc1)
                         : static_cast<quint8>(ctx.alexHpfBits & ~0x20u);

    // Same HPF bits [@501e3f5]
    if (hpfBitsAlex1 & 0x01) { reg |= (1u << 1);  }
    if (hpfBitsAlex1 & 0x02) { reg |= (1u << 2);  }
    if (hpfBitsAlex1 & 0x04) { reg |= (1u << 4);  }
    if (hpfBitsAlex1 & 0x08) { reg |= (1u << 5);  }
    if (hpfBitsAlex1 & 0x10) { reg |= (1u << 6);  }
    // bit 12 (Bypass): never mirrored from Alex0 (see above), but emitted when
    // ADC1's own decision is bypass.
    if (haveAdc1Decision && (hpfBitsAlex1 & 0x20)) { reg |= (1u << 12); }
    if (hpfBitsAlex1 & 0x40) { reg |= (1u << 3);  }

    return reg;
}

// =================================================================
// Phase 3M-4 Task 5 + 2026-05-17: PureSignal DDC config — per-board dispatch
// =================================================================
//
// Verbatim port of the per-HpsdrModel branches in Thetis console.cs
// UpdateDDCs(). The P2 path uses the same switch as P1 (single switch on
// HardwareSpecific.Model at console.cs:8209 [v2.10.3.13]); only the
// `if (p1) Rate[0] = rx1_rate;` quirks at console.cs:8238 / 8251 differ
// between protocols (P1 mirrors Rate[0] for G2-class boards even though
// DDCEnable selects DDC2 — a P1-wire-format quirk that does not apply to
// P2).  The 1-ADC branches (Hermes / HermesII) emit identical cfg on P1
// and P2 because their primary DDC is DDC0 either way.
//
// Dispatch table:
//   ANAN100D / ANAN200D / ORIONMKII / ANAN7000D / ANAN8000D / ANAN_G2 /
//   ANAN_G2_1K / ANVELINAPRO3   → psDdcConfigG2Class       (console.cs:8211-8295)
//   HERMES / ANAN10 / ANAN100   → psDdcConfigHermesClass   (console.cs:8378-8449)
//   ANAN10E / ANAN100B          → psDdcConfigHermesIIClass (console.cs:8451-8521)
//   HPSDR / HERMESLITE / REDPITAYA / FIRST / LAST / unknown → empty cfg
//
// HERMESLITE and REDPITAYA fall through to the empty cfg here because
// neither board ships P2 firmware (Thetis treats them as P1-only at
// clsHardwareSpecific.cs).  P2CodecOrionMkII is the dispatcher; per-
// model overrides are protected static helpers defined below.
PsDdcConfig P2CodecOrionMkII::applyPureSignalDdcConfig(
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
    case HPSDRModel::ANVELINAPRO3:
        return psDdcConfigG2Class(psEnabled, diversityEnabled, moxState,
                                  rx1Rate, rx2Rate, rx2Enabled,
                                  adcCtrl1, adcCtrl2);

    // From Thetis console.cs:8378-8380 [v2.10.3.13]
    // Updated for ANAN_G2E (HermesC10) per console.cs:8387-8390 [v2.10.3.15]
    // //N1GP G2E added — Thetis groups ANAN_G2E with the 4-DDC Hermes-class
    // arm in UpdateDDCs (single-ADC, 4-DDC, primary RX on DDC0).
    // Wire-byte capture 2026-05-22 from working Thetis-on-G2E confirms
    // CmdRx byte 7 = 0x01 (DDC0 enable) when active.
    case HPSDRModel::HERMES:
    case HPSDRModel::ANAN_G2E:  //N1GP G2E added
    case HPSDRModel::ANAN10:
    case HPSDRModel::ANAN100:
        return psDdcConfigHermesClass(psEnabled, diversityEnabled, moxState,
                                      rx1Rate, rx2Rate, rx2Enabled);

    // From Thetis console.cs:8451-8452 [v2.10.3.13]
    case HPSDRModel::ANAN10E:
    case HPSDRModel::ANAN100B:
        return psDdcConfigHermesIIClass(psEnabled, diversityEnabled, moxState,
                                        rx1Rate, rx2Rate, rx2Enabled);

    // From Thetis console.cs:8523-8524 [v2.10.3.13]:
    //   case HPSDRModel.HPSDR:
    //       break;
    // No PS hardware on Atlas / HL2 / RedPitaya P2 firmware paths.
    case HPSDRModel::HPSDR:
    case HPSDRModel::HERMESLITE:
    case HPSDRModel::REDPITAYA:
    case HPSDRModel::FIRST:
    case HPSDRModel::LAST:
    default:
        return PsDdcConfig{};
    }
}

// =================================================================
// G2-class branch — Thetis console.cs:8211-8295 [v2.10.3.13]
// =================================================================
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
// `ps_rate` is the static cmaster.PSrate = 192000 (cmaster.cs:424
// [v2.10.3.13]).
PsDdcConfig P2CodecOrionMkII::psDdcConfigG2Class(
    bool psEnabled,
    bool diversityEnabled,
    bool moxState,
    int rx1Rate,
    int rx2Rate,
    bool rx2Enabled,
    quint8 adcCtrl1,
    quint8 adcCtrl2) noexcept
{
    PsDdcConfig cfg;
    constexpr uint8_t DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
    // From Thetis cmaster.cs:424 [v2.10.3.13]: private static int ps_rate = 192000;
    constexpr int ps_rate = 192000;

    // From console.cs:8219-8220 [v2.10.3.13]
    cfg.p1RxCount = 5;                     // RX5 used for puresignal feedback
    cfg.nDdc      = 5;

    if (!moxState) {
        if (diversityEnabled) {
            // From console.cs:8223-8232 [v2.10.3.13]
            // P1_DDCConfig =
            // DDCEnable = DDC0;            (note: P1_DDCConfig defaulted to 0 here per Thetis fall-through)
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
            // [2.10.3.13]MW0LGE p1 !
            // (P2 path doesn't set Rate[0] here — Thetis only sets Rate[0] when p1==true)
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
            // [2.10.3.13]MW0LGE p1 !  (Rate[0] only set on P1 path; codec is P2 here)
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

            // Phase 3M-4 mi0bot audit: PS DDC pair indices for P2 G2-class.
            //
            // P2 PS-MOX uses the network.c:936-945 [v2.10.3.13] freq override
            // to force DDC0+DDC1 to TX freq during MOX, so the PS pair on the
            // wire is DDC0 (PS feedback) + DDC1 (TX monitor) regardless of
            // the DDC count.  Different from P1 G2-class which uses DDC3+DDC4.
            //
            // Confirmed by mi0bot console.cs:8623 [v2.10.3.13-beta2] GetDDC()
            // P2 case 5: rx1=2, rx2=3 (no explicit psrx/pstx — implicit DDC0/DDC1).
            //
            // Matches PsccPump default cmaster.cs:533-534 [v2.10.3.13].
            // Explicit assignment for documentation + cross-codec consistency.
            cfg.psFbDdc  = 0;
            cfg.txMonDdc = 1;
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

            // Same PS DDC layout as the !diversity && PS branch above.
            cfg.psFbDdc  = 0;
            cfg.txMonDdc = 1;
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

// =================================================================
// Hermes-class branch — Thetis console.cs:8378-8449 [v2.10.3.13]
// =================================================================
//
// case HPSDRModel.HERMES:
// case HPSDRModel.ANAN10:
// case HPSDRModel.ANAN100:
//     P1_rxcount = 4;                     // RX4 used for puresignal feedback
//     nddc = 4;
//     ...
//
// 1-ADC family — DDC0 carries RX1, DDC1 carries RX2.  No `if (p1)` Rate[0]
// quirk in this branch (DDC0 is the primary on both protocols).  cntrl1 /
// cntrl2 are unused (no ADC selector bits — single ADC).
//
// `ps_rate` from cmaster.cs:424 [v2.10.3.13]: private static int ps_rate = 192000;
PsDdcConfig P2CodecOrionMkII::psDdcConfigHermesClass(
    bool psEnabled,
    bool diversityEnabled,
    bool moxState,
    int rx1Rate,
    int rx2Rate,
    bool rx2Enabled) noexcept
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
        } else {
            // transmitting and PS is ON
            // From console.cs:8438-8447 [v2.10.3.13]
            //
            // cntrl1=4 is the Thetis P2 wire value (see detailed analysis in
            // the HermesII-class psDdcConfigHermesIIClass branch below).  The
            // P1 HermesII empirical override of cntrl1=0 does NOT apply to
            // P2 because P2 wire bytes 17/23/29/35 come from
            // prn->rx[i].rx_adc, which IS the path SetADC_cntrl1(cntrl1)
            // writes (netInterface.c:949-962 [v2.10.3.13]).
            cfg.p1DdcConfig = 6;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = ps_rate;
            cfg.rate[1]     = ps_rate;
            cfg.cntrl1      = 4;
            cfg.cntrl2      = 0;

            // P2 1-ADC PS pair on the wire: DDC0 (PS feedback) + DDC1 (TX
            // monitor).  Per Thetis cmaster.cs:538-539 [v2.10.3.13]:
            //   SetPSRxIdx(0, 0);   // Stream0 for RX feedback
            //   SetPSTxIdx(0, 1);   // Stream1 for TX feedback
            // Both Thetis and our radio actually send these as INTERLEAVED
            // pair on port 1035 (sync byte = DDC1 bit; our deinterleave
            // splits even-indexed samples → ddcIndex=0, odd-indexed →
            // ddcIndex=1).  Verified by tshark conv on /tmp/nereus-g2e-ps
            // .pcap.first (Thetis ref) and current pcap: radio sends only
            // on port 1035 (not 1036/1037).  The earlier speculation
            // (2026-05-23) about DDC2/DDC3 routing was wrong — that's the
            // router CALLID config for a different processing path, not
            // the on-wire DDC numbering.
            cfg.psFbDdc  = 0;
            cfg.txMonDdc = 1;
        }
    }

    return cfg;
}

// =================================================================
// HermesII-class branch — Thetis console.cs:8451-8521 [v2.10.3.13]
// =================================================================
//
// case HPSDRModel.ANAN10E:
// case HPSDRModel.ANAN100B:
//     P1_rxcount = 2;                     // RX2 used for puresignal feedback
//     nddc = 2;
//     ...
//
// 1-ADC family with nddc=2 — identical wire layout to Hermes-class but
// fewer hardware DDCs.  DDC0 carries RX1, DDC1 carries RX2.
//
// Closes issue #263: ANAN-10E running community P2 firmware was being
// routed through the G2-class branch (DDC2 primary), so the radio never
// streamed I/Q and the 2 s connect watchdog fired.  This is the verbatim
// Thetis port for the HermesII case.
PsDdcConfig P2CodecOrionMkII::psDdcConfigHermesIIClass(
    bool psEnabled,
    bool diversityEnabled,
    bool moxState,
    int rx1Rate,
    int rx2Rate,
    bool rx2Enabled) noexcept
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
        } else {
            // transmitting and PS is ON
            // From console.cs:8510-8519 [v2.10.3.13]
            //
            // NOTE on cntrl1=4 (vs the P1 HermesII empirical override of 0):
            // The P1 HermesII branch in P1CodecStandard.cpp::psDdcConfigHermesIIClass
            // has a 2026-05-09 bench-fix override that sets cfg.cntrl1=0 here
            // because working Thetis on a friend's ANAN-10E (P1) was observed
            // emitting 0 on bank 4 C1, not the source-computed 4.  That P1
            // override patches a NereusSDR-side conflation: the P1 bank 4 wire
            // byte in NereusSDR (P1CodecStandard.cpp:142) comes from cfg.cntrl1,
            // but in Thetis the same byte comes from a SEPARATE variable
            // (`P1_adc_cntrl`, networkproto1.c:519 [v2.10.3.13]) set only by
            // SetADC_cntrl_P1 from the Setup form's per-DDC radio buttons,
            // independent of UpdateDDCs.  The observed `cntrl1=0` on the P1
            // wire was the value of `P1_adc_cntrl`, not the result of any
            // Thetis runtime override of cntrl1.
            //
            // P2 is structurally different:
            //   - Thetis P2 wire bytes 17/23/29/35 = prn->rx[i].rx_adc
            //     (network.c:1106-1169 [v2.10.3.13]).
            //   - prn->rx[i].rx_adc IS set by SetADC_cntrl1(cntrl1) at
            //     netInterface.c:949-962 [v2.10.3.13], which IS called at the
            //     end of UpdateDDCs (console.cs:8531 [v2.10.3.13]).
            // So on P2, cntrl1=4 from UpdateDDCs becomes rx[1].rx_adc=1 on
            // the wire — Thetis P2 sends exactly the source-computed value.
            // The P1 override does NOT port to P2.  We emit cntrl1=4 to match
            // Thetis P2 source AND Thetis P2 observed wire.  If a future P2
            // bench capture on this code path shows a different value, that
            // would be a real divergence and should be patched as its own
            // bench-fix here.
            cfg.p1DdcConfig = 5;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = ps_rate;
            cfg.rate[1]     = ps_rate;
            cfg.cntrl1      = 4;
            cfg.cntrl2      = 0;

            // P2 1-ADC PS pair on the wire: DDC0 (PS feedback) + DDC1 (TX monitor).
            // Same rationale as Hermes-class above.
            cfg.psFbDdc  = 0;
            cfg.txMonDdc = 1;
        }
    }

    return cfg;
}

DdcAssignment P2CodecOrionMkII::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    // OrionMkII / ANAN7000D / ANAN8000D / ANAN100D / ANAN200D / ANVELINAPRO3 /
    // ANAN_G2 / ANAN_G2_1K DDC assignment.
    //
    // Mirrors Thetis console.cs:8220-8303 [v2.10.3.15] UpdateDDCs() G2-class
    // branch.  These models all fall through to the same case body as Saturn
    // (HPSDRModel.ANAN_G2 / ANAN_G2_1K) — the logic is byte-for-byte
    // identical; only the dispatch shim differs.
    //
    // P2CodecSaturn INHERITS this. It used to carry a hand-copied duplicate,
    // which is how the antenna-driven routing below reached every 2-ADC board
    // except the ANAN-G2 it was written for; the copy is gone and
    // tst_p2_codec_saturn.cpp asserts the two codecs stay identical.
    //
    // Slice-to-DDC mapping for G2-class (2-ADC, 7 DDCs):
    //   Slice A (index 0) -> DDC2    [Thetis: DDCEnable = DDC2 at line 8244]
    //   Slice B (index 1) -> DDC3    [Thetis: DDCEnable += DDC3 at line 8301]
    //   Slice C (index 2) -> DDC4    [NereusSDR extension: idle Thetis DDC4 slot]
    //   Slice D (index 3) -> DDC5    [NereusSDR extension: idle Thetis DDC5 slot]
    //   Slice E (index 4) -> DDC6    [NereusSDR extension: idle Thetis DDC6 slot]
    // DDC0/DDC1 reserved for PS feedback pair or Diversity sync pair.
    //
    // From Thetis console.cs:8199 [v2.10.3.15]:
    //   int DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
    // [2.10.3.13]MW0LGE p1 !  [original inline comment from console.cs:8247 — P1-only branch on
    // the same RX state; P2 codec does not set Rate[0] here, but tag preserved per
    // CLAUDE.md inline-comment-preservation rule (author tag within +-5 of cite)]
    // //DH1KLM  [original tag from console.cs:8305 REDPITAYA case header — adjacent to the
    // rx2_enabled addendum at console.cs:8301/8302; preserved per CLAUDE.md rule]

    DdcAssignment a{};

    // PS feedback DDC rate from Thetis cmaster.cs:425 [v2.10.3.15]:
    //   private static int ps_rate = 192000;
    // From Thetis console.cs:8205 [v2.10.3.15]: int ps_rate = cmaster.PSrate;
    static constexpr int kPsRate = 192000;

    // Stream-to-DDC index table. DDC0 and DDC1 are reserved. Phase 3F
    // Sub-Epic I Task 7b: indexed by DDC STREAM, not by slice, so co-hosted
    // slices share one entry and therefore one DDC.
    // From Thetis console.cs:8244-8247 [v2.10.3.15] (DDC2 = stream 0) and
    // console.cs:8301 [v2.10.3.15] (DDC3 = stream 1 / rx2_enabled).
    // [2.10.3.13]MW0LGE p1 !  [verbatim from console.cs:8247 — P1-only Rate[0] path]
    static constexpr int kStreamToDdc[5] = {2, 3, 4, 5, 6};

    // ADC control from Thetis console.cs:8249 [v2.10.3.15]:
    //   cntrl1 = rx_adc_ctrl1 & 0xff;  (default rx_adc_ctrl1=4, console.cs:15099)
    //   cntrl2 = rx_adc_ctrl2 & 0x3f;  (default rx_adc_ctrl2=0, console.cs:15135)
    // ctx.adcCtrl carries rx_adc_ctrl1 in low byte, rx_adc_ctrl2 in high byte.
    //
    // Seeded BEFORE the stream loop, because the loop overwrites the 2-bit
    // field of every DDC it assigns (Phase 3F design doc §16, below). Fields
    // belonging to DDCs we do not assign -- notably DDC1's, which the
    // PureSignal override further down rewrites -- keep the incoming value.
    a.adcCtrl1 = static_cast<int>(ctx.adcCtrl & 0xff);
    a.adcCtrl2 = static_cast<int>((ctx.adcCtrl >> 8) & 0x3f);

    // Populate DDC assignments for active streams.
    // For stream 0 -> DDC2: matches Thetis's rx1 on DDC2.
    // For stream 1 -> DDC3: matches Thetis's rx2_enabled DDC3 addendum.
    // For streams 2-4 -> DDC4-6: NereusSDR extension into Thetis's idle slots.
    for (int i = 0; i < 5; ++i) {
        if (!slices[i].live) { continue; }
        const int ddc = kStreamToDdc[i];
        // Phase 3F Sub-Epic I Task 7b: publish the mapping explicitly.
        a.streamDdc[i] = ddc;
        a.ddcEnable |= (1 << ddc);
        // From Thetis console.cs:8248 [v2.10.3.15]: Rate[2] = rx1_rate;
        // [2.10.3.13]MW0LGE p1 !  [original inline comment from console.cs:8247]
        // From Thetis console.cs:8302 [v2.10.3.15]: Rate[3] = rx2_rate;
        // //DH1KLM  [verbatim from console.cs:8305 — tag on REDPITAYA case header
        // adjacent to the rx2_enabled addendum at 8302; preserved per CLAUDE.md rule]
        a.rate[ddc] = slices[i].sampleRateHz;

        // Phase 3F design doc §16: antenna-driven ADC chain assignment.
        //
        // NereusSDR-original policy, NOT a Thetis port. Thetis exposes the
        // DDC->ADC map as a manual Setup control (setup.cs:16935-16944
        // [v2.10.3.15] builds RXADCCtrl1 from radDDCnADCn radio buttons) and
        // never derives it from the antenna. We derive it, because a slice's
        // antenna already determines which physical chain can hear it.
        //
        // The bit layout IS Thetis's, verbatim from that same setup.cs block:
        //   DDC0 bits 1&0, DDC1 bits 3&2, DDC2 bits 5&4, DDC3 bits 7&6
        //   00 = ADC0, 01 = ADC1, 10 = ADC2 (PS feedback)
        // and console.cs:15117-15131 [v2.10.3.15] decodes it the same way
        // (`mask = 3 << (ddc * 2)`), with adcCtrl2 covering DDC4-7 as DDC(n-4).
        //
        // Mapping: the RX-only inputs are the ones wired to the second
        // chain's front end -- on an ANAN-G2 the block diagram shows ADC1 fed
        // from the RX2 ant jack while the Ant/TR switch feeds ADC0 only. So
        // EXT1/EXT2 land a slice on ADC1; ANT1/2/3 stay on ADC0.
        //
        // BYPS deliberately stays on ADC0: it is a bypass relay rather than a
        // distinct feed, and putting it on the second chain is a guess this
        // code should not make.
        //
        // No adcCount gate is needed: this codec serves only the 2-ADC
        // boards (Saturn / OrionMkII / AnvelinaPro3 / RedPitaya), Saturn by
        // inheritance since the D3 fix. P2CodecHermes overrides
        // applyDdcAssignment for the 1-ADC Hermes class, HermesC10 / G2E
        // included, so those stay pinned to ADC0 by construction rather than
        // by a runtime check here.
        //
        // The field is CLEARED and then written, not OR-ed: the antenna is
        // authoritative for a DDC we assign, so a stale ADC1 bit left in the
        // incoming rx_adc_ctrl1 must not survive a move back to ANT1.
        const int ant = slices[i].antennaIndex;
        const int adc = (ant == 4 || ant == 5) ? 1 : 0;  // EXT1/EXT2 -> chain 1
        if (ddc < 4) {
            a.adcCtrl1 = (a.adcCtrl1 & ~(3 << (ddc * 2))) | (adc << (ddc * 2));
        } else {
            const int sh = (ddc - 4) * 2;
            a.adcCtrl2 = (a.adcCtrl2 & ~(3 << sh)) | (adc << sh);
        }

        ++a.nDdc;
    }

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
        // Stream 0 migrates: DDC2 is disabled, DDC0+DDC1 sync pair takes over.
        a.ddcEnable &= ~0x04;                       // clear DDC2
        a.ddcEnable |= 0x03;                        // set DDC0 + DDC1
        a.syncEnable |= 0x02;                       // DDC1 syncs to DDC0
        if (slices[0].live) {
            // From Thetis console.cs:8237-8238 [v2.10.3.15]: Rate[0]=Rate[1]=rx1_rate
            a.rate[0] = slices[0].sampleRateHz;
            a.rate[1] = slices[0].sampleRateHz;
            a.rate[2] = 0;
            // Phase 3F Sub-Epic I Task 7b: stream 0's DDC moved from DDC2 to
            // the DDC0/DDC1 diversity sync pair set above; republish DDC0
            // as the pair's primary so streamDdc stays consistent with
            // ddcEnable (same convention as psFwdDdc for the PS pair).
            a.streamDdc[0] = 0;
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
