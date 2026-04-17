// =================================================================
// tests/tst_rxchannel_agc_advanced.cpp  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/radio.cs
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/dsp.cs
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   Copyright (C) 2017-2019  Chris Codella (W2PA) — console.cs / setup.cs / NetworkIO.cs inline mods
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2020-2025  Joe (WD5Y) — console.cs UI tweaks / mute / ForeColor
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Combines logic from the Thetis sources
//                 listed above.
// =================================================================

// tst_rxchannel_agc_advanced.cpp
//
// Verifies RxChannel AGC advanced setter/accessor contracts without
// requiring a live WDSP channel.
//
// Strategy: only test paths where no WDSP API call is made:
//   1. Default-value assertions — accessor returns Thetis-sourced default.
//      The setter is never called, so WDSP is never invoked.
//   2. Same-value idempotency — setter early-returns when value equals the
//      currently stored atomic. WDSP call skipped by the early return guard.
//
// This avoids calling OpenChannel / WDSPwisdom in the test binary, which
// would require the FFTW wisdom file to exist (slow/absent in CI). The
// actual WDSP call path is exercised by the integration tests that spin
// up a WdspEngine (tst_p1_loopback_connection, tst_rx_dsp_worker_thread).
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:1037-1124 — AGC advanced
//   From Thetis Project Files/Source/Console/console.cs:45977  — AGCThresh
//   WDSP: third_party/wdsp/src/wcpAGC.c:418,427,436,504,537

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelAgcAdvanced : public QObject {
    Q_OBJECT

private slots:
    // ── setAgcThreshold ──────────────────────────────────────────────────────

    void agcThresholdDefaultIsMinusTwenty() {
        // From Thetis console.cs:45977 — agc_thresh_point default = -20
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcThreshold(), -20);
    }

    void setAgcThresholdEarlyReturnOnSameValue() {
        // Same value as default — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcThreshold(-20);   // same as default → early return
        QCOMPARE(ch.agcThreshold(), -20);
    }

    // ── setAgcHang ───────────────────────────────────────────────────────────

    void agcHangDefaultIs250() {
        // From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcHang(), 250);
    }

    void setAgcHangEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcHang(250);   // same as default → early return
        QCOMPARE(ch.agcHang(), 250);
    }

    // ── setAgcSlope ──────────────────────────────────────────────────────────

    void agcSlopeDefaultIsZero() {
        // From Thetis radio.cs:1107-1108 — rx_agc_slope = 0
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcSlope(), 0);
    }

    void setAgcSlopeEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcSlope(0);   // same as default → early return
        QCOMPARE(ch.agcSlope(), 0);
    }

    // ── setAgcAttack ─────────────────────────────────────────────────────────

    void agcAttackDefaultIsTwo() {
        // From WDSP wcpAGC.c create_wcpagc — tau_attack default 2 ms
        // From Thetis dsp.cs:116-117 — P/Invoke declared, UI disabled/hidden
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcAttack(), 2);
    }

    void setAgcAttackEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcAttack(2);   // same as default → early return
        QCOMPARE(ch.agcAttack(), 2);
    }

    // ── setAgcDecay ──────────────────────────────────────────────────────────

    void agcDecayDefaultIs250() {
        // From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcDecay(), 250);
    }

    void setAgcDecayEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcDecay(250);   // same as default → early return
        QCOMPARE(ch.agcDecay(), 250);
    }
};

QTEST_MAIN(TestRxChannelAgcAdvanced)
#include "tst_rxchannel_agc_advanced.moc"
