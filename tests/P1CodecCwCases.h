// no-port-check: NereusSDR-original test helper. Upstream file names appear
// only in cites for the expected byte values.
// =================================================================
// tests/P1CodecCwCases.h  (NereusSDR)
// =================================================================
//
// The Protocol 1 CW bank cases every P1 codec test runs (R-CW-11): banks 12
// (C2 bit 6, C3, C4), 13 and 14 from CodecContext::cw, the attenuator bits
// of bank 12 left as they were, and each C0 keeping its MOX bit and address.
// Layout: Thetis networkproto1.c:618-646 [v2.10.3.15], identical in mi0bot
// networkproto1.c:1112-1140 [v2.10.3.13-beta2].
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the shared P1 CW bank cases. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#pragma once

#include <QtTest/QtTest>

#include "core/codec/CodecContext.h"
#include "core/codec/IP1Codec.h"

namespace NereusSDR::P1CwCases {

// Speed 30, iambic A, weight 55, strict on, reverse on, enable on, level 90,
// RF delay 12, hang 310, frequency 600.
inline CodecContext cwContext(bool mox)
{
    CodecContext ctx;
    ctx.mox = mox;
    ctx.rxStepAttn[1] = 7;
    ctx.rxStepAttn[2] = 5;
    ctx.cw.speedWpm = 30;
    ctx.cw.keyerMode = int(CwKeyerMode::IambicA);
    ctx.cw.weight = 55;
    ctx.cw.strictSpacing = true;
    ctx.cw.reversePaddles = true;
    ctx.cw.enable = true;
    ctx.cw.sidetoneLevel = 90;
    ctx.cw.rfDelayMs = 12;
    ctx.cw.hangMs = 310;
    ctx.cw.sidetoneFreqHz = 600;
    return ctx;
}

inline void compose(const IP1Codec& codec, int bank, const CodecContext& ctx, quint8 out[5])
{
    for (int i = 0; i < 5; ++i) {
        out[i] = 0xAA;
    }
    codec.composeCcForBank(bank, ctx, out);
}

// Runs every case against one codec; QCOMPARE failures return from here and
// the caller's slot reports them.
inline void runAll(const IP1Codec& codec)
{
    for (const bool mox : {false, true}) {
        const quint8 moxBit = mox ? 0x01 : 0x00;
        const CodecContext ctx = cwContext(mox);
        quint8 out[5] = {};

        // Bank 12: the attenuator bits are what the same context gives with
        // reverse paddles off; reverse adds C2 bit 6 only.
        CodecContext noReverse = ctx;
        noReverse.cw.reversePaddles = false;
        quint8 base[5] = {};
        compose(codec, 12, noReverse, base);
        QCOMPARE(int(base[2] & 0x40), 0);
        compose(codec, 12, ctx, out);
        QCOMPARE(int(out[0]), 0x16 | moxBit);
        QCOMPARE(int(out[1]), int(base[1]));
        QCOMPARE(int(out[2]), int(base[2] | 0x40));
        QCOMPARE(int(out[2] & 0x1F), 5);
        QCOMPARE(int(out[3]), 0x5E);  // 30 | 0x40
        QCOMPARE(int(out[4]), 0xB7);  // 55 | 0x80

        CodecContext iambicB = ctx;
        iambicB.cw.keyerMode = int(CwKeyerMode::IambicB);
        compose(codec, 12, iambicB, out);
        QCOMPARE(int(out[3]), 30 | 0x80);

        CodecContext straight = ctx;
        straight.cw.keyerMode = int(CwKeyerMode::Straight);
        compose(codec, 12, straight, out);
        QCOMPARE(int(out[3]), 30);

        // Bank 13.
        compose(codec, 13, ctx, out);
        QCOMPARE(int(out[0]), 0x1E | moxBit);
        QCOMPARE(int(out[1]), 1);
        QCOMPARE(int(out[2]), 90);
        QCOMPARE(int(out[3]), 12);
        QCOMPARE(int(out[4]), 0);

        // Enable off: C1 0, the other fields still sent.
        CodecContext off = ctx;
        off.cw.enable = false;
        compose(codec, 13, off, out);
        QCOMPARE(int(out[0]), 0x1E | moxBit);
        QCOMPARE(int(out[1]), 0);
        QCOMPARE(int(out[2]), 90);
        QCOMPARE(int(out[3]), 12);
        QCOMPARE(int(out[4]), 0);
        compose(codec, 12, off, out);
        QCOMPARE(int(out[3]), 0x5E);
        QCOMPARE(int(out[4]), 0xB7);

        // Bank 14.
        compose(codec, 14, ctx, out);
        QCOMPARE(int(out[0]), 0x20 | moxBit);
        QCOMPARE(int(out[1]), 310 >> 2);    // 77
        QCOMPARE(int(out[2]), 310 & 3);     // 2
        QCOMPARE(int(out[3]), 600 >> 4);    // 37
        QCOMPARE(int(out[4]), 600 & 0x0F);  // 8
        compose(codec, 14, off, out);
        QCOMPARE(int(out[1]), 77);
        QCOMPARE(int(out[3]), 37);
    }
}

} // namespace NereusSDR::P1CwCases
