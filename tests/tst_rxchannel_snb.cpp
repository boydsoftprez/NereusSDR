// =================================================================
// tests/tst_rxchannel_snb.cpp  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
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

// tst_rxchannel_snb.cpp
//
// Verifies RxChannel SNB setter/accessor contracts without requiring a live
// WDSP channel.
//
// Strategy: only test paths where no WDSP API call is made:
//   1. Default-value assertions — accessor returns correct default.
//      The setter is never called, so WDSP is never invoked.
//   2. Same-value idempotency — setter early-returns when value equals the
//      currently stored atomic. WDSP call skipped by the early return guard.
//
// Source citations:
//   From Thetis Project Files/Source/Console/console.cs:36347 — SNB run flag
//   From Thetis Project Files/Source/Console/dsp.cs:692-693 — P/Invoke decl
//   WDSP: third_party/wdsp/src/snb.c:579

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelSnb : public QObject {
    Q_OBJECT

private slots:
    // ── setSnbEnabled ────────────────────────────────────────────────────────

    void snbEnabledDefaultIsFalse() {
        // SNB off at startup — Thetis chkDSPNB2.Checked default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.snbEnabled(), false);
    }

    void setSnbEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setSnbEnabled(false);   // same as default → early return
        QCOMPARE(ch.snbEnabled(), false);
    }
};

QTEST_MAIN(TestRxChannelSnb)
#include "tst_rxchannel_snb.moc"
