/*  eq.c

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
// tests/tst_tx_channel_eq_setters.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the 6 TX EQ wrappers added in Phase 3M-3a-i Task B-1:
//
//   setTxEqRunning(bool)                       → SetTXAEQRun
//   setTxEqGraph10(array<int,11>)              → SetTXAGrphEQ10
//   setTxEqProfile(vector<double>, vector<double>) → SetTXAEQProfile
//   setTxEqNc(int)                             → SetTXAEQNC
//   setTxEqMp(bool)                            → SetTXAEQMP
//   setTxEqCtfmode(int)                        → SetTXAEQCtfmode
//   setTxEqWintype(int)                        → SetTXAEQWintype
//
// Test strategy: pure smoke / does-not-crash, matching the convention
// established by tst_tx_channel_tx_post_gen_setters.cpp (Phase 3M-1c E.2-E.6).
//
// WDSP setter calls in unit tests fall through the
// `txa[m_channelId].rsmpin.p == nullptr` null-guard added in 3M-1a D.3
// (channel was never opened via OpenChannel).  Without HAVE_WDSP, the
// stub path is exercised.  Either way the wrapper must complete without UB.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-29 — New test for Phase 3M-3a-i Task B-1: 7 TX EQ wrapper
//                 setters (Run / Graph10 / Profile / Nc / Mp / Ctfmode /
//                 Wintype). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp.

#include <QtTest/QtTest>

#include "core/TxChannel.h"

#include <array>
#include <vector>

using namespace NereusSDR;

// WDSP TX channel ID — from Thetis cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;

class TestTxChannelEqSetters : public QObject {
    Q_OBJECT

private slots:

    // ── B-1.1: setTxEqRunning ────────────────────────────────────────────────
    //
    // Wraps SetTXAEQRun(channel, run ? 1 : 0).
    // From Thetis wdsp/eq.c:742-747 [v2.10.3.13].
    // Routed through Stage::Eqp dispatch in setStageRunning.

    void setTxEqRunning_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqRunning(true);
    }

    void setTxEqRunning_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqRunning(false);
    }

    // ── B-1.2: setTxEqGraph10 (10-band graphic EQ) ───────────────────────────
    //
    // Wraps SetTXAGrphEQ10(channel, txeq[]) — txeq is an int[11]
    // (preamp + 10 band gains).  Thetis-default G shape from TXA.c:113.
    // From Thetis wdsp/eq.c:859 [v2.10.3.13].

    void setTxEqGraph10_thetisDefaults_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // From Thetis wdsp/TXA.c:113 [v2.10.3.13]:
        //   default_G[11] = {0.0, -12, -12, -12, -1, +1, +4, +9, +12, -10, -10}
        const std::array<int, 11> shape{0, -12, -12, -12, -1, 1, 4, 9, 12, -10, -10};
        ch.setTxEqGraph10(shape);
    }

    void setTxEqGraph10_zeroShape_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        const std::array<int, 11> flat{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        ch.setTxEqGraph10(flat);
    }

    // ── B-1.3: setTxEqProfile (custom freq/gain pairs) ──────────────────────
    //
    // Wraps SetTXAEQProfile(channel, nfreqs, F[], G[], NULL).
    // From Thetis wdsp/eq.c:779 [v2.10.3.13].
    // freqs10.size() == 10, gains11.size() == 11.

    void setTxEqProfile_thetisDefaults_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // From Thetis wdsp/TXA.c:112-113 [v2.10.3.13].
        const std::vector<double> freqs10{32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
        const std::vector<double> gains11{0, -12, -12, -12, -1, 1, 4, 9, 12, -10, -10};
        ch.setTxEqProfile(freqs10, gains11);
    }

    void setTxEqProfile_sizeMismatchFreqs_logsWarningAndReturns()
    {
        TxChannel ch(kTxChannelId);
        // freqs10 has wrong size (9 instead of 10) → early-return with qCWarning.
        const std::vector<double> badFreqs{32, 63, 125, 250, 500, 1000, 2000, 4000, 8000};
        const std::vector<double> gains11{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        // Should not crash; logs and returns.
        ch.setTxEqProfile(badFreqs, gains11);
    }

    void setTxEqProfile_sizeMismatchGains_logsWarningAndReturns()
    {
        TxChannel ch(kTxChannelId);
        const std::vector<double> freqs10{32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
        // gains11 has wrong size (10 instead of 11) → early-return with qCWarning.
        const std::vector<double> badGains{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        ch.setTxEqProfile(freqs10, badGains);
    }

    // ── B-1.4: setTxEqNc (filter coefficients) ──────────────────────────────
    //
    // Wraps SetTXAEQNC(channel, nc).  Default 2048 per WDSP create_eqp.
    // From Thetis wdsp/eq.c:750 [v2.10.3.13].

    void setTxEqNc_default2048_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqNc(2048);
    }

    void setTxEqNc_smallerValue_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqNc(512);
    }

    // ── B-1.5: setTxEqMp (minimum phase) ────────────────────────────────────
    //
    // Wraps SetTXAEQMP(channel, mp).
    // From Thetis wdsp/eq.c:767 [v2.10.3.13].

    void setTxEqMp_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqMp(false);
    }

    void setTxEqMp_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqMp(true);
    }

    // ── B-1.6: setTxEqCtfmode ───────────────────────────────────────────────
    //
    // Wraps SetTXAEQCtfmode(channel, mode).
    // From Thetis wdsp/eq.c:807 [v2.10.3.13].

    void setTxEqCtfmode_zero_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqCtfmode(0);
    }

    // ── B-1.7: setTxEqWintype ───────────────────────────────────────────────
    //
    // Wraps SetTXAEQWintype(channel, wintype).
    // From Thetis wdsp/eq.c:819 [v2.10.3.13].

    void setTxEqWintype_zero_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqWintype(0);
    }

    // ── Mixed call sequence: full configure → enable workflow ───────────────
    //
    // Mirrors the typical Setup-page handler order:
    //   apply DSP-tier settings (Nc/Mp/Ctfmode/Wintype) → apply profile →
    //   toggle Run.

    void mixedConfigureSequence_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxEqNc(2048);
        ch.setTxEqMp(false);
        ch.setTxEqCtfmode(0);
        ch.setTxEqWintype(0);
        const std::vector<double> freqs10{32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
        const std::vector<double> gains11{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        ch.setTxEqProfile(freqs10, gains11);
        ch.setTxEqRunning(true);
        ch.setTxEqRunning(false);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelEqSetters)
#include "tst_tx_channel_eq_setters.moc"
