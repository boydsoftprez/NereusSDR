/*  wcpAGC.c

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
// tests/tst_tx_channel_leveler_alc_setters.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the 5 TX Leveler / ALC wrappers added in Phase 3M-3a-i
// Task B-2:
//
//   setTxLevelerOn(bool)        → SetTXALevelerSt
//   setTxLevelerTopDb(double)   → SetTXALevelerTop
//   setTxLevelerDecayMs(int)    → SetTXALevelerDecay
//   setTxAlcMaxGainDb(double)   → SetTXAALCMaxGain
//   setTxAlcDecayMs(int)        → SetTXAALCDecay
//
// Plus regression coverage for the new Stage::Leveler / Stage::Alc cases
// in setStageRunning that the brief required.
//
// Test strategy: pure smoke / does-not-crash, matching the convention from
// tst_tx_channel_tx_post_gen_setters.cpp.  WDSP setter calls fall through
// the rsmpin.p == nullptr null-guard in WDSP-linked builds; HAVE_WDSP-
// undefined builds exercise the stub path.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-29 — New test for Phase 3M-3a-i Task B-2: 5 TX Leveler/ALC
//                 wrapper setters (Run / Top / Decay for Leveler;
//                 MaxGain / Decay for ALC) + Stage::Leveler/Stage::Alc
//                 setStageRunning regression. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp.

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

// WDSP TX channel ID — from Thetis cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;

class TestTxChannelLevelerAlcSetters : public QObject {
    Q_OBJECT

private slots:

    // ── B-2.1: setTxLevelerOn ────────────────────────────────────────────────
    //
    // Wraps SetTXALevelerSt(channel, on ? 1 : 0).
    // From Thetis wdsp/wcpAGC.c:613-618 [v2.10.3.13].
    // Cited handler: setup.cs:9108-9123 [v2.10.3.13]
    //   chkDSPLevelerEnabled_CheckedChanged.

    void setTxLevelerOn_true_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerOn(true);
    }

    void setTxLevelerOn_false_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerOn(false);
    }

    void setTxLevelerOn_idempotentToggle_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerOn(true);
        ch.setTxLevelerOn(true);   // redundant — still safe
        ch.setTxLevelerOn(false);
        ch.setTxLevelerOn(false);  // redundant — still safe
    }

    // ── B-2.2: setTxLevelerTopDb ────────────────────────────────────────────
    //
    // Wraps SetTXALevelerTop(channel, dB).
    // From Thetis wdsp/wcpAGC.c:647-650 [v2.10.3.13].
    // Cited handler: setup.cs:9095-9099 [v2.10.3.13]
    //   udDSPLevelerThreshold_ValueChanged.
    // Designer range: 0..20 dB (setup.Designer.cs:38718-38738), default 15.

    void setTxLevelerTopDb_default15_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerTopDb(15.0);
    }

    void setTxLevelerTopDb_min0_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerTopDb(0.0);
    }

    void setTxLevelerTopDb_max20_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerTopDb(20.0);
    }

    // ── B-2.3: setTxLevelerDecayMs ──────────────────────────────────────────
    //
    // Wraps SetTXALevelerDecay(channel, ms).
    // From Thetis wdsp/wcpAGC.c:629-635 [v2.10.3.13].
    // Cited handler: setup.cs:9101-9105 [v2.10.3.13]
    //   udDSPLevelerDecay_ValueChanged.
    // Designer range: 1..5000 ms (setup.Designer.cs:38744-38772), default 100.

    void setTxLevelerDecayMs_default100_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerDecayMs(100);
    }

    void setTxLevelerDecayMs_min1_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerDecayMs(1);
    }

    void setTxLevelerDecayMs_max5000_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxLevelerDecayMs(5000);
    }

    // ── B-2.4: setTxAlcMaxGainDb ────────────────────────────────────────────
    //
    // Wraps SetTXAALCMaxGain(channel, dB).
    // From Thetis wdsp/wcpAGC.c:603-610 [v2.10.3.13].
    // Cited handler: setup.cs:9129-9134 [v2.10.3.13]
    //   udDSPALCMaximumGain_ValueChanged.
    // Designer range: 0..120 dB (setup.Designer.cs:38814-38833), default 3.

    void setTxAlcMaxGainDb_default3_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxAlcMaxGainDb(3.0);
    }

    void setTxAlcMaxGainDb_zero_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxAlcMaxGainDb(0.0);
    }

    void setTxAlcMaxGainDb_max120_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxAlcMaxGainDb(120.0);
    }

    // ── B-2.5: setTxAlcDecayMs ──────────────────────────────────────────────
    //
    // Wraps SetTXAALCDecay(channel, ms).
    // From Thetis wdsp/wcpAGC.c:585-592 [v2.10.3.13].
    // Cited handler: setup.cs:9136-9140 [v2.10.3.13]
    //   udDSPALCDecay_ValueChanged.
    // Designer range: 1..50 ms (setup.Designer.cs:38845-38866), default 10.

    void setTxAlcDecayMs_default10_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxAlcDecayMs(10);
    }

    void setTxAlcDecayMs_min1_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxAlcDecayMs(1);
    }

    void setTxAlcDecayMs_max50_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxAlcDecayMs(50);
    }

    // ── Stage::Leveler / Stage::Alc cases in setStageRunning ────────────────
    //
    // 3M-3a-i Batch 1 added explicit case arms for Stage::Leveler (→
    // SetTXALevelerSt) and Stage::Alc (→ SetTXAALCSt) in setStageRunning,
    // routing through the same WDSP entry points as setTxLevelerOn /
    // setTxAlcMaxGainDb's run-companion (SetTXAALCSt).  Verify both paths
    // remain a no-throw smoke.

    void setStageRunning_leveler_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Leveler, true);
    }

    void setStageRunning_leveler_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Leveler, false);
    }

    void setStageRunning_alc_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Alc, true);
    }

    void setStageRunning_alc_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Alc, false);
    }

    // ── Mixed configure-then-engage workflow ────────────────────────────────
    //
    // Mirrors the typical Setup-page flow when a user opens DSP setup,
    // adjusts both panels, then closes the dialog.

    void mixedConfigureSequence_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Leveler config
        ch.setTxLevelerTopDb(15.0);
        ch.setTxLevelerDecayMs(100);
        ch.setTxLevelerOn(true);
        // ALC config
        ch.setTxAlcMaxGainDb(3.0);
        ch.setTxAlcDecayMs(10);
        // Toggle Leveler off then on
        ch.setTxLevelerOn(false);
        ch.setTxLevelerOn(true);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelLevelerAlcSetters)
#include "tst_tx_channel_leveler_alc_setters.moc"
