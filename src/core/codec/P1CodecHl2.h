// =================================================================
// src/core/codec/P1CodecHl2.h  (NereusSDR)
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

#pragma once

#include "IP1Codec.h"

namespace NereusSDR { class IoBoardHl2; }  // forward decl — full header in .cpp

namespace NereusSDR {

// mi0bot WriteMainLoop_HL2 port. Banks 0-18.
//
// Key divergences from Standard (ramdor WriteMainLoop):
//   - Bank 11 C4: 6-bit ATT mask (0x3F) + 0x40 enable (vs 0x1F + 0x20).
//                 Under MOX, sends txStepAttn[0] instead of rxStepAttn[0].
//                 This is the load-bearing bug fix for the reported S-ATT issue.
//                 Source: mi0bot networkproto1.c:1099-1102 [@c26a8a4]
//   - Bank 12:    Identical to Standard behavior (forces 0x1F under MOX).
//                 No RedPitaya-special-case needed — HL2 is never confused with
//                 RedPitaya, so the bank12() override from P1CodecRedPitaya does
//                 not apply. Uses direct IP1Codec implementation.
//                 Source: mi0bot networkproto1.c:1107-1122 [@c26a8a4]
//   - Bank 17:    TX latency + PTT hang (vs AnvelinaPro3 extra OC byte).
//                 Source: mi0bot networkproto1.c:1162-1168 [@c26a8a4]
//   - Bank 18:    Reset-on-disconnect flag (HL2-only firmware feature).
//                 Source: mi0bot networkproto1.c:1170-1176 [@c26a8a4]
//   - usesI2cIntercept() returns true — Phase E will use this hook to
//     swap in I2C TLV bytes when the IoBoardHl2 queue is non-empty.
//
// Note: banks 0-10, 13-16 are byte-for-byte identical to ramdor WriteMainLoop
// (mi0bot inherits them unchanged).
// Source: mi0bot networkproto1.c:869-1098 [@c26a8a4 / matches @501e3f5]
class P1CodecHl2 : public IP1Codec {
public:
    void composeCcForBank(int bank, const CodecContext& ctx, quint8 out[5]) const override;
    int  maxBank() const override { return 18; }
    bool usesI2cIntercept() const override { return true; }

    // Phase 3P-E Task 2: wire to IoBoardHl2 for I2C intercept.
    // Called by P1RadioConnection::setIoBoard() at connect time.
    void setIoBoard(IoBoardHl2* io) { m_io = io; }

    // Compose the next I2C TLV frame (5 bytes) for the oldest pending
    // I2C transaction in the IoBoardHl2 queue.  Returns true if a frame
    // was emitted (txn dequeued); false if queue is empty / no IoBoard wired.
    // mox: current MOX state (written into C0 bit 0, matching XmitBit).
    //
    // Source: mi0bot networkproto1.c:898-943 [@c26a8a4]
    bool tryComposeI2cFrame(quint8 out[5], bool mox) const;

private:
    IoBoardHl2* m_io{nullptr};
};

} // namespace NereusSDR
