// =================================================================
// tests/tst_rxchannel_squelch.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/radio.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
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
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

// tst_rxchannel_squelch.cpp
//
// Verifies RxChannel squelch (SSB/AM/FM) setter/accessor contracts without
// requiring a live WDSP channel.
//
// Strategy: only test paths where no WDSP API call is made:
//   1. Default-value assertions — accessor returns Thetis-sourced default.
//      The setter is never called, so WDSP is never invoked.
//   2. Same-value idempotency — setter early-returns when value equals the
//      currently stored atomic. WDSP call skipped by the early return guard.
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:1185 — _bSSqlOn = false
//   From Thetis Project Files/Source/Console/radio.cs:1293 — rx_am_squelch_on = false
//   From Thetis Project Files/Source/Console/radio.cs:1312 — rx_fm_squelch_on = false
//   WDSP: third_party/wdsp/src/ssql.c:331, amsq.c, fmsq.c:236

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelSquelch : public QObject {
    Q_OBJECT

private slots:
    // ── setSsqlEnabled ───────────────────────────────────────────────────────

    void ssqlEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1185 — _bSSqlOn default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.ssqlEnabled(), false);
    }

    void setSsqlEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setSsqlEnabled(false);   // same as default → early return
        QCOMPARE(ch.ssqlEnabled(), false);
    }

    // ── setAmsqEnabled ───────────────────────────────────────────────────────

    void amsqEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1293 — rx_am_squelch_on default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.amsqEnabled(), false);
    }

    void setAmsqEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAmsqEnabled(false);   // same as default → early return
        QCOMPARE(ch.amsqEnabled(), false);
    }

    // ── setFmsqEnabled ───────────────────────────────────────────────────────

    void fmsqEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1312 — rx_fm_squelch_on default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.fmsqEnabled(), false);
    }

    void setFmsqEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setFmsqEnabled(false);   // same as default → early return
        QCOMPARE(ch.fmsqEnabled(), false);
    }
};

QTEST_MAIN(TestRxChannelSquelch)
#include "tst_rxchannel_squelch.moc"
