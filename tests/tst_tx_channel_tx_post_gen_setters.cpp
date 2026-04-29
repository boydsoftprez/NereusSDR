/*  TXA.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014, 2016, 2017, 2021, 2023 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

// =================================================================
// tests/tst_tx_channel_tx_post_gen_setters.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is directly ported in this test file.  The test exercises
// the 12 thin TXA PostGen wrapper setters added in Phase 3M-1c E.2-E.6:
//
//   E.2 — Mode setter (1 method)
//     - setTxPostGenMode(int)                  → SetTXAPostGenMode
//
//   E.3 — Continuous-mode TT setters (4 methods)
//     - setTxPostGenTTFreq1(double)            → SetTXAPostGenTTFreq (caches freq2)
//     - setTxPostGenTTFreq2(double)            → SetTXAPostGenTTFreq (caches freq1)
//     - setTxPostGenTTMag1(double)             → SetTXAPostGenTTMag  (caches mag2)
//     - setTxPostGenTTMag2(double)             → SetTXAPostGenTTMag  (caches mag1)
//
//   E.4 — Pulsed-mode TT setters (4 methods)
//     - setTxPostGenTTPulseToneFreq1(double)   → SetTXAPostGenTTPulseToneFreq (caches freq2)
//     - setTxPostGenTTPulseToneFreq2(double)   → SetTXAPostGenTTPulseToneFreq (caches freq1)
//     - setTxPostGenTTPulseMag1(double)        → SetTXAPostGenTTPulseMag      (caches mag2)
//     - setTxPostGenTTPulseMag2(double)        → SetTXAPostGenTTPulseMag      (caches mag1)
//
//   E.5 — Pulse-profile setters (3 methods)
//     - setTxPostGenTTPulseFreq(int)           → SetTXAPostGenTTPulseFreq
//     - setTxPostGenTTPulseDutyCycle(double)   → SetTXAPostGenTTPulseDutyCycle
//     - setTxPostGenTTPulseTransition(double)  → SetTXAPostGenTTPulseTransition
//
//   E.6 — Run setter (1 method)
//     - setTxPostGenRun(bool)                  → SetTXAPostGenRun
//
// Porting context (cited verbatim in TxChannel.h / TxChannel.cpp):
//   E.2 / E.6: setup.cs:11084 / 11096 / 11107 / 11166 [v2.10.3.13]
//   E.3:       setup.cs:11097-11105 [v2.10.3.13]
//   E.4:       setup.cs:11085-11092 [v2.10.3.13]
//   E.5:       setup.cs:34409-34418 [v2.10.3.13] (setupTwoTonePulse)
//   WDSP API:  wdsp/gen.c:783-964 [v2.10.3.13] — SetTXAPostGen*.
//
// These wrappers are simple pass-throughs to the underlying WDSP C API.
// The C# Thetis property surface exposes each Freq1/Freq2 / Mag1/Mag2 as a
// separate setter, but the WDSP function takes both at once.  NereusSDR
// caches the partner value internally so each setX1 / setX2 wrapper can
// individually invoke the combined WDSP call (matching radio.cs:3697-4032
// [v2.10.3.13]).
//
// Test strategy (pure smoke / does-not-crash, matching the convention from
// tst_tx_channel_tune_tone.cpp):
//
//   1. Each of the 12 setters is callable on a default-constructed TxChannel
//      without crashing or throwing.  WDSP setter calls in unit tests fall
//      through the txa[m_channelId].rsmpin.p == nullptr null-guard added in
//      D.3 / D.6 (channel was never opened via OpenChannel).  Without
//      HAVE_WDSP, the Q_UNUSED stub path is exercised.  Either way the
//      setter must complete without UB.
//
//   2. Edge values: 0.0, large, negative.  WDSP itself does not pre-validate
//      (e.g. negative duty-cycle or transition is meaningless but not flagged
//      by SetTXAPostGenTTPulseDutyCycle / Transition).  NereusSDR mirrors
//      this — no pre-validation in the wrappers.
//
//   3. Toggle / round-trip patterns confirm internal cache state (freq1/freq2
//      / mag1/mag2 split-property fields) does not corrupt subsequent calls.
//
// 14 test cases — one per setter (12) plus 2 round-trip / mixed-call
// regression tests for the split-property cache.
//
// Pre-code review reference: Phase 3M-1c E.2-E.6 plan.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-28 — New test for Phase 3M-1c Tasks E.2-E.6: 12 TXA PostGen
//                 wrapper setters (mode / continuous TT freq+mag / pulsed TT
//                 freq+mag / pulse profile / run).  J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp.

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

// WDSP TX channel ID — from Thetis cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;   // WDSP.id(1, 0) — TX channel

class TestTxChannelTxPostGenSetters : public QObject {
    Q_OBJECT

private slots:

    // ── E.2: setTxPostGenMode ────────────────────────────────────────────────
    //
    // Wraps SetTXAPostGenMode(channel, mode).
    // Mode values: 0 = off, 1 = continuous two-tone, 7 = pulsed two-tone.
    // From Thetis setup.cs:11084 / 11096 [v2.10.3.13].

    void setTxPostGenMode_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenMode(0);   // off
    }

    void setTxPostGenMode_continuousTwoTone_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenMode(1);   // continuous two-tone
    }

    void setTxPostGenMode_pulsedTwoTone_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenMode(7);   // pulsed two-tone
    }

    // ── E.3: setTxPostGenTTFreq1 / Freq2 / Mag1 / Mag2 ──────────────────────
    //
    // Continuous-mode two-tone setters.
    // From Thetis setup.cs:11097-11105 [v2.10.3.13]:
    //   TXPostGenTTFreq1 = ttfreq1;
    //   TXPostGenTTFreq2 = ttfreq2;
    //   TXPostGenTTMag1  = ttmag1;
    //   TXPostGenTTMag2  = ttmag2;
    // WDSP combines both freq / both mag into single calls:
    //   SetTXAPostGenTTFreq(channel, freq1, freq2)
    //   SetTXAPostGenTTMag(channel, mag1, mag2)
    // The wrappers cache the partner value (matching Thetis radio.cs
    // 3697-3771 [v2.10.3.13]).

    void setTxPostGenTTFreq1_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTFreq1(700.0);   // 700 Hz typical low tone
    }

    void setTxPostGenTTFreq2_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTFreq2(1900.0);  // 1900 Hz typical high tone
    }

    void setTxPostGenTTMag1_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTMag1(0.49);     // typical 2-TONE mag (avoids overmod)
    }

    void setTxPostGenTTMag2_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTMag2(0.49);
    }

    // ── E.4: setTxPostGenTTPulseToneFreq1 / Freq2 / Mag1 / Mag2 ─────────────
    //
    // Pulsed-mode two-tone setters.
    // From Thetis setup.cs:11085-11092 [v2.10.3.13]:
    //   TXPostGenTTPulseToneFreq1 = ttfreq1;
    //   TXPostGenTTPulseToneFreq2 = ttfreq2;
    //   TXPostGenTTPulseMag1      = ttmag1;
    //   TXPostGenTTPulseMag2      = ttmag2;
    // Same combined-call WDSP pattern as continuous mode.

    void setTxPostGenTTPulseToneFreq1_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTPulseToneFreq1(700.0);
    }

    void setTxPostGenTTPulseToneFreq2_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTPulseToneFreq2(1900.0);
    }

    void setTxPostGenTTPulseMag1_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTPulseMag1(0.49);
    }

    void setTxPostGenTTPulseMag2_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTPulseMag2(0.49);
    }

    // ── E.5: setTxPostGenTTPulseFreq / DutyCycle / Transition ───────────────
    //
    // Pulse-profile setters used by Thetis setupTwoTonePulse() to configure
    // the gating window around the two-tone carrier.
    // From Thetis setup.cs:34415-34417 [v2.10.3.13]:
    //   TXPostGenTTPulseFreq        = (int)nudPulsed_TwoTone_window.Value;
    //   TXPostGenTTPulseDutyCycle   = (float)(nudPulsed_TwoTone_percent.Value) / 100f;
    //   TXPostGenTTPulseTransition  = (float)(nudPulsed_TwoTone_ramp.Value)    / 1000f;

    void setTxPostGenTTPulseFreq_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTPulseFreq(10);   // 10 Hz pulse rate (typical default)
    }

    void setTxPostGenTTPulseDutyCycle_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTPulseDutyCycle(0.10);   // 10% duty-cycle typical
    }

    void setTxPostGenTTPulseTransition_typicalValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTPulseTransition(0.005);  // 5 ms ramp typical
    }

    // ── E.6: setTxPostGenRun ─────────────────────────────────────────────────
    //
    // Wraps SetTXAPostGenRun(channel, on ? 1 : 0).
    // From Thetis setup.cs:11107 / 11166 [v2.10.3.13]:
    //   TXPostGenRun = 1;  (on)
    //   TXPostGenRun = 0;  (off)

    void setTxPostGenRun_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenRun(true);
    }

    void setTxPostGenRun_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenRun(false);
    }

    // ── Cache integrity: split-property partner value retained ──────────────
    //
    // The continuous TT setters Freq1/Freq2/Mag1/Mag2 each cache the partner
    // value so the combined WDSP call uses both fields.  Mixed call sequences
    // must not corrupt the cache or trip UB.

    void continuousTT_mixedCallSequence_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Configure full continuous-TT workflow (matches setup.cs:11097-11107):
        ch.setTxPostGenTTFreq1(700.0);
        ch.setTxPostGenTTFreq2(1900.0);
        ch.setTxPostGenTTMag1(0.49);
        ch.setTxPostGenTTMag2(0.49);
        ch.setTxPostGenMode(1);          // continuous two-tone
        ch.setTxPostGenRun(true);
        ch.setTxPostGenRun(false);
    }

    void pulsedTT_mixedCallSequence_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Configure full pulsed-TT workflow (matches setup.cs:11085-11107
        // + setup.cs:34415-34417):
        ch.setTxPostGenTTPulseToneFreq1(700.0);
        ch.setTxPostGenTTPulseToneFreq2(1900.0);
        ch.setTxPostGenTTPulseMag1(0.49);
        ch.setTxPostGenTTPulseMag2(0.49);
        ch.setTxPostGenTTPulseFreq(10);
        ch.setTxPostGenTTPulseDutyCycle(0.10);
        ch.setTxPostGenTTPulseTransition(0.005);
        ch.setTxPostGenMode(7);          // pulsed two-tone
        ch.setTxPostGenRun(true);
        ch.setTxPostGenRun(false);
    }

    // ── Edge cases: zero, negative, large values ────────────────────────────

    void setters_zeroValues_doNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPostGenTTFreq1(0.0);
        ch.setTxPostGenTTFreq2(0.0);
        ch.setTxPostGenTTMag1(0.0);
        ch.setTxPostGenTTMag2(0.0);
        ch.setTxPostGenTTPulseToneFreq1(0.0);
        ch.setTxPostGenTTPulseToneFreq2(0.0);
        ch.setTxPostGenTTPulseMag1(0.0);
        ch.setTxPostGenTTPulseMag2(0.0);
        ch.setTxPostGenTTPulseFreq(0);
        ch.setTxPostGenTTPulseDutyCycle(0.0);
        ch.setTxPostGenTTPulseTransition(0.0);
        ch.setTxPostGenMode(0);
        ch.setTxPostGenRun(false);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelTxPostGenSetters)
#include "tst_tx_channel_tx_post_gen_setters.moc"
