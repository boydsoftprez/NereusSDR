// =================================================================
// src/core/codec/P1CodecHl2.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c:869-1201
//   (WriteMainLoop_HL2)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. HL2-only codec; mirrors mi0bot's
//                literal WriteMainLoop_HL2 vs WriteMainLoop split.
//                Fixes reported HL2 S-ATT bug at the wire layer.
// =================================================================
//
// === Verbatim mi0bot networkproto1.c header (lines 1-19) ===
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

#include "P1CodecHl2.h"
#include "core/IoBoardHl2.h"

namespace NereusSDR {

void P1CodecHl2::composeCcForBank(int bank, const CodecContext& ctx,
                                  quint8 out[5]) const
{
    // C0 low bit = XmitBit (MOX)
    // Source: mi0bot networkproto1.c:899 [@c26a8a4]
    const quint8 C0base = ctx.mox ? 0x01 : 0x00;
    for (int i = 0; i < 5; ++i) { out[i] = 0; }

    switch (bank) {
        // Bank 0 — General settings
        // Source: mi0bot networkproto1.c:938-978 [@c26a8a4 / matches @501e3f5]
        // C3 bits 5-6: RX-only mux — From Thetis netInterface.c:479-481 [v2.10.3.13 @501e3f5]
        // C3 bit 7: _Rx_1_Out relay — networkproto1.c:455 [v2.10.3.13 @501e3f5]
        case 0: {
            out[0] = C0base | 0x00;
            out[1] = quint8(ctx.sampleRateCode & 0x03);
            out[2] = quint8((ctx.ocByte << 1) & 0xFE);
            // C3: rxPreamp[0] + dither + random + RX-only mux + RX-bypass-out.
            // Source: networkproto1.c:453-468 [v2.10.3.13 @501e3f5]
            quint8 c3 = quint8((ctx.rxPreamp[0] ? 0x04 : 0)
                             | (ctx.dither[0]   ? 0x08 : 0)
                             | (ctx.random[0]   ? 0x10 : 0));
            switch (ctx.rxOnlyAnt) {
                case 1: c3 |= 0b0010'0000; break;  // _Rx_1_In
                case 2: c3 |= 0b0100'0000; break;  // _Rx_2_In
                case 3: c3 |= 0b0110'0000; break;  // _XVTR_Rx_In
                default: break;                     // 0 = no RX-only path selected
            }
            if (ctx.rxOut) { c3 |= 0b1000'0000; }  // _Rx_1_Out relay
            out[3] = c3;
            out[4] = quint8((ctx.antennaIdx & 0x03)
                          | (ctx.duplex ? 0x04 : 0)
                          | (((ctx.activeRxCount - 1) & 0x0F) << 3)
                          | (ctx.diversity ? 0x80 : 0));
            return;
        }

        // Bank 1 — TX VFO
        // Source: mi0bot networkproto1.c:980-984 [@c26a8a4 / matches @501e3f5]
        case 1:
            out[0] = C0base | 0x02;
            out[1] = quint8((ctx.txFreqHz >> 24) & 0xFF);
            out[2] = quint8((ctx.txFreqHz >> 16) & 0xFF);
            out[3] = quint8((ctx.txFreqHz >>  8) & 0xFF);
            out[4] = quint8( ctx.txFreqHz        & 0xFF);
            return;

        // Banks 2-9 — RX VFOs
        // Source: mi0bot networkproto1.c:985-1058 [@c26a8a4 / matches @501e3f5]
        case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: {
            out[0] = C0base | quint8(0x04 + (bank - 2) * 2);
            const int rxIdx = bank - 2;
            const quint64 freq = (rxIdx < ctx.activeRxCount)
                                  ? ctx.rxFreqHz[rxIdx]
                                  : ctx.txFreqHz;
            out[1] = quint8((freq >> 24) & 0xFF);
            out[2] = quint8((freq >> 16) & 0xFF);
            out[3] = quint8((freq >>  8) & 0xFF);
            out[4] = quint8( freq        & 0xFF);
            return;
        }

        // Bank 10 — TX drive, mic boost, Alex HPF/LPF, T/R relay
        // Source: mi0bot networkproto1.c:1060-1090 [@c26a8a4 / matches @501e3f5]
        // HL2 note: deskhpsdr clears C2/C3/C4 entirely for HL2 PA-enable
        // (old_protocol.c:2964-2966 [@120188f]) — HL2 firmware (control.v:211-214)
        // does NOT decode C3 bit 7 (Alex T/R relay).  We still write trxRelay
        // here for correctness; HL2 FW ignores the bit.
        // T/R relay bit (C3 bit 7) is INVERTED: 0 = engaged, 1 = disabled.
        // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
        case 10:
            out[0] = C0base | 0x12;
            out[1] = quint8(ctx.txDrive & 0xFF);
            // C2: mic_boost → bit 0 (0x01); line_in → bit 1 (0x02); bit 6 always set per upstream default.
            // From Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
            //   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ... | 0b01000000) & 0x7f;
            out[2] = quint8((ctx.p1MicBoost ? 0x01 : 0x00) | (ctx.p1LineIn ? 0x02 : 0x00) | 0x40);
            out[3] = quint8(ctx.alexHpfBits | (ctx.trxRelay ? 0x00 : 0x80));  // T/R relay engaged (INVERTED: 1 = disabled)
            out[4] = quint8(ctx.alexLpfBits);
            return;

        // Bank 11 — Preamp + RX/TX step ATT ADC0 (HL2 6-bit encoding + MOX branch)
        // **THIS IS THE BUG FIX** — HL2 needs 6-bit mask (0x3F) + 0x40 enable,
        // and MOX branch selects txStepAttn[0] instead of rxStepAttn[0].
        // Standard codec keeps ramdor's 5-bit (0x1F) + 0x20 encoding.
        // Source: mi0bot networkproto1.c:1091-1104 [@c26a8a4]
        case 11: {
            out[0] = C0base | 0x14;
            // C1: preamp bits 0-3 (bit 3 = rx0 again, Thetis quirk) + mic_trs bit 4.
            // HL2 has no mic jack but the bit is written for correctness (FW ignores).
            // mic_trs polarity inversion: wire bit set when tip is BIAS/PTT (!tipHot).
            // From Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
            //   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ... — 3M-1b G.3
            out[1] = quint8((ctx.rxPreamp[0] ? 0x01 : 0)
                          | (ctx.rxPreamp[1] ? 0x02 : 0)
                          | (ctx.rxPreamp[2] ? 0x04 : 0)
                          | (ctx.rxPreamp[0] ? 0x08 : 0)         // bit3 = rx0 again (Thetis quirk)
                          | (!ctx.p1MicTipRing ? 0x10 : 0x00));  // mic_trs (inverted) — 3M-1b G.3
            out[2] = 0;
            out[3] = 0;
            // MI0BOT: Different read loop for HL2 — Larger range for the HL2 attenuator
            // [original inline comment from networkproto1.c:1100,1102]
            if (ctx.mox) {
                out[4] = quint8((ctx.txStepAttn[0] & 0b00111111) | 0b01000000);  // Larger range for the HL2 attenuator
            } else {
                out[4] = quint8((ctx.rxStepAttn[0] & 0b00111111) | 0b01000000);  // Larger range for the HL2 attenuator
            }
            return;
        }

        // Bank 12 — Step ATT ADC1/2 + CW keyer
        // HL2 behavior: identical to Standard (forces ADC1=0x1F under MOX).
        // No RedPitaya-special-case needed — HL2 is known hardware.
        // Source: mi0bot networkproto1.c:1106-1125 [@c26a8a4]
        case 12: {
            out[0] = C0base | 0x16;
            if (ctx.mox) {
                out[1] = 0x1F | 0x20;  // forced under MOX (same as Standard)
            } else {
                out[1] = quint8(ctx.rxStepAttn[1]) | 0x20;
            }
            out[2] = quint8((ctx.rxStepAttn[2] & 0x1F) | 0x20);
            out[3] = 0;
            out[4] = 0;
            return;
        }

        // Banks 13-16 — CW / EER / BPF2 (identical to Standard)
        // Source: mi0bot networkproto1.c:1126-1159 [@c26a8a4 / matches @501e3f5]
        case 13: out[0] = C0base | 0x1E; return;
        case 14: out[0] = C0base | 0x20; return;
        case 15: out[0] = C0base | 0x22; return;
        case 16: out[0] = C0base | 0x24; return;

        // Bank 17 — TX latency + PTT hang (HL2-only, NOT AnvelinaPro3 extra OC)
        // Source: mi0bot networkproto1.c:1162-1168 [@c26a8a4]
        case 17:
            out[0] = C0base | 0x2E;
            out[1] = 0;
            out[2] = 0;
            out[3] = quint8(ctx.hl2PttHang & 0b00011111);
            out[4] = quint8(ctx.hl2TxLatency & 0b01111111);
            return;

        // Bank 18 — Reset on disconnect (HL2-only firmware feature)
        // Source: mi0bot networkproto1.c:1170-1176 [@c26a8a4]
        case 18:
            out[0] = C0base | 0x74;
            out[1] = 0;
            out[2] = 0;
            out[3] = 0;
            out[4] = quint8(ctx.hl2ResetOnDisconnect ? 0x01 : 0x00);
            return;

        default:
            out[0] = C0base;
            return;
    }
}

// ---------------------------------------------------------------------------
// tryComposeI2cFrame
//
// Overrides the normal C&C compose path when the IoBoardHl2 queue has pending
// I2C transactions. Pops the next txn from the front of the queue and encodes
// it into the 5-byte C&C frame per mi0bot WriteMainLoop_HL2.
//
// Wire layout (ported from mi0bot networkproto1.c:895-943 [@c26a8a4]):
//   C0 = XmitBit | (I2C chip addr << 1) | (ctrl_request << 7)
//        I2C chip addr: 0x3c for bus 0 (I2C1), 0x3d for bus 1 (I2C2)
//        ctrl_request: bit 2 (0x04) of txn.control
//   C1 = 0x07 (read) or 0x06 (write)   — ctrl_read: bit 0 (0x01) of txn.control
//   C2 = 0x80 | address                — device 7-bit address with stop bit
//        If address > 0x7F, right-shift by 1 to strip the r/w bit.
//   C3 = txn.control                   — I2C control byte (verbatim)
//   C4 = txn.writeData                 — write data byte
//
// Returns true if a frame was composed and the txn dequeued; false if queue
// was empty or m_io is null.
// ---------------------------------------------------------------------------
bool P1CodecHl2::tryComposeI2cFrame(quint8 out[5], bool mox) const
{
    if (!m_io || m_io->i2cQueueIsEmpty()) { return false; }
    IoBoardHl2::I2cTxn txn;
    if (!m_io->dequeueI2c(txn)) { return false; }

    for (int i = 0; i < 5; ++i) { out[i] = 0; }

    // C0: XmitBit in bit 0, I2C chip select in bits 1-7, ctrl_request in bit 7.
    // Source: mi0bot networkproto1.c:912-919 [@c26a8a4]
    const quint8 xmitBit   = mox ? quint8(0x01) : quint8(0x00);
    // ctrl_request is bit 2 of txn.control (IoBoardHl2::CtrlRequest = 0x04)
    const quint8 ctrlReq   = (txn.control & IoBoardHl2::CtrlRequest) ? quint8(0x01) : quint8(0x00);
    if (txn.bus == 0) {
        out[0] = xmitBit | quint8(0x3c << 1) | quint8(ctrlReq << 7);  // I2C1 0x3c
    } else {
        out[0] = xmitBit | quint8(0x3d << 1) | quint8(ctrlReq << 7);  // I2C2 0x3d
    }

    // C1: 0x07 = read, 0x06 = write.
    // Source: mi0bot networkproto1.c:930-935 [@c26a8a4]
    // ctrl_read is bit 0 of txn.control (IoBoardHl2::CtrlRead = 0x01)
    const bool ctrlRead = (txn.control & IoBoardHl2::CtrlRead) != 0;
    out[1] = ctrlRead ? quint8(0x07) : quint8(0x06);

    // C2: 0x80 | device address (stop bit). Address > 0x7F → shift right to strip r/w bit.
    // Source: mi0bot networkproto1.c:921-928 [@c26a8a4]
    quint8 address = txn.address;
    if (address > 0x7F) { address = address >> 1; }
    out[2] = quint8(0x80) | address;  // Stop request

    // C3: I2C register / control byte verbatim.
    // Source: mi0bot networkproto1.c:939 [@c26a8a4]
    out[3] = txn.control;

    // C4: write data byte.
    // Source: mi0bot networkproto1.c:940 [@c26a8a4]
    out[4] = txn.writeData;

    return true;
}

} // namespace NereusSDR
