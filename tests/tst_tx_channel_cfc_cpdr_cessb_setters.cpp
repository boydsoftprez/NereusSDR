/*  cfcomp.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2021 Warren Pratt, NR0V

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
// tests/tst_tx_channel_cfc_cpdr_cessb_setters.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the 9 TX CFC + CPDR + CESSB wrappers added in Phase
// 3M-3a-ii Batch 1:
//
//   CFC   (6):  setTxCfcRunning(bool)
//               setTxCfcPosition(int)
//               setTxCfcProfile(F, G, E, Qg, Qe)
//               setTxCfcPrecompDb(double)
//               setTxCfcPostEqRunning(bool)
//               setTxCfcPrePeqDb(double)
//   CPDR  (2):  setTxCpdrOn(bool)
//               setTxCpdrGainDb(double)
//   CESSB (1):  setTxCessbOn(bool)
//
// Plus regression coverage for the Stage::CfComp / Stage::Compressor /
// Stage::OsCtrl case arms in setStageRunning (already wired in 3M-1a Task
// C.4, kept under test in this batch for traceability).
//
// Test strategy: pure smoke / does-not-crash, matching the convention from
// tst_tx_channel_tx_post_gen_setters.cpp (and used by
// tst_tx_channel_eq_setters.cpp / tst_tx_channel_leveler_alc_setters.cpp).
// WDSP setter calls fall through the rsmpin.p == nullptr null-guard in
// HAVE_WDSP-linked builds; HAVE_WDSP-undefined builds exercise the stub
// path.  Profile-array edge cases (empty, mismatched length) verify the
// validation arm in setTxCfcProfile reaches the early-return without
// touching WDSP.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New test for Phase 3M-3a-ii Task B-3: 9 TX CFC/CPDR/CESSB
//                 wrapper setters + Stage::CfComp / Stage::Compressor /
//                 Stage::OsCtrl setStageRunning regression.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp.

#include <QtTest/QtTest>

#include <vector>

#include "core/TxChannel.h"

using namespace NereusSDR;

// WDSP TX channel ID — from Thetis cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;

class TestTxChannelCfcCpdrCessbSetters : public QObject {
    Q_OBJECT

private slots:

    // ── B-3.1: setTxCfcRunning ───────────────────────────────────────────────
    //
    // Wraps SetTXACFCOMPRun(channel, on ? 1 : 0).
    // From Thetis wdsp/cfcomp.c:632-641 [v2.10.3.13].

    void setTxCfcRunning_true_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcRunning(true);
    }

    void setTxCfcRunning_false_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcRunning(false);
    }

    void setTxCfcRunning_idempotentToggle_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcRunning(true);
        ch.setTxCfcRunning(true);   // redundant — still safe
        ch.setTxCfcRunning(false);
        ch.setTxCfcRunning(false);  // redundant — still safe
    }

    // ── B-3.2: setTxCfcPosition ─────────────────────────────────────────────
    //
    // Wraps SetTXACFCOMPPosition(channel, pos).
    // From Thetis wdsp/cfcomp.c:643-653 [v2.10.3.13].
    // Thetis values: 0 = pre-EQ, 1 = post-EQ.

    void setTxCfcPosition_pre_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPosition(0);
    }

    void setTxCfcPosition_post_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPosition(1);
    }

    // ── B-3.3: setTxCfcProfile ──────────────────────────────────────────────
    //
    // Wraps SetTXACFCOMPprofile.  Validates length consistency and (when the
    // bundled WDSP catches up) forwards Qg/Qe arrays.  Today Qg/Qe are
    // length-validated and dropped at the WDSP boundary.
    // From Thetis wdsp/cfcomp.c:655-698 [v2.10.3.13] (full 7-arg variant).
    // From third_party/wdsp/src/cfcomp.c:437-460 (TAPR v1.29 — bundled 5-arg).

    void setTxCfcProfile_threeBand_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Three-band profile with empty Qg/Qe — exercises the F/G/E-only path.
        std::vector<double> F = {200.0, 1000.0, 3000.0};
        std::vector<double> G = {0.0,   3.0,    0.0};
        std::vector<double> E = {6.0,   6.0,    6.0};
        std::vector<double> Qg;  // empty — Thetis-style "not provided"
        std::vector<double> Qe;  // empty — Thetis-style "not provided"
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_withQgQe_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Same profile with explicit Qg/Qe present (validated for length;
        // dropped at WDSP boundary on bundled WDSP).
        std::vector<double> F  = {200.0, 1000.0, 3000.0};
        std::vector<double> G  = {0.0,   3.0,    0.0};
        std::vector<double> E  = {6.0,   6.0,    6.0};
        std::vector<double> Qg = {1.0,   1.0,    1.0};
        std::vector<double> Qe = {1.0,   1.0,    1.0};
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_emptyF_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Empty F — should warn + early return, never touching WDSP.
        std::vector<double> F, G, E, Qg, Qe;
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_mismatchedG_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // F has 3 bands, G has 2 — mismatch should warn + early return.
        std::vector<double> F = {200.0, 1000.0, 3000.0};
        std::vector<double> G = {0.0,   3.0};  // wrong length
        std::vector<double> E = {6.0,   6.0,    6.0};
        std::vector<double> Qg, Qe;
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_mismatchedE_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // F has 3 bands, E has 4 — mismatch should warn + early return.
        std::vector<double> F = {200.0, 1000.0, 3000.0};
        std::vector<double> G = {0.0,   3.0,    0.0};
        std::vector<double> E = {6.0,   6.0,    6.0, 6.0};  // wrong length
        std::vector<double> Qg, Qe;
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_mismatchedQg_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // F has 3 bands, Qg has 2 — non-empty mismatch should warn + return.
        std::vector<double> F  = {200.0, 1000.0, 3000.0};
        std::vector<double> G  = {0.0,   3.0,    0.0};
        std::vector<double> E  = {6.0,   6.0,    6.0};
        std::vector<double> Qg = {1.0,   1.0};  // wrong length (and non-empty)
        std::vector<double> Qe;
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_mismatchedQe_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // F has 3 bands, Qe has 5 — non-empty mismatch should warn + return.
        std::vector<double> F  = {200.0, 1000.0, 3000.0};
        std::vector<double> G  = {0.0,   3.0,    0.0};
        std::vector<double> E  = {6.0,   6.0,    6.0};
        std::vector<double> Qg;
        std::vector<double> Qe = {1.0,   1.0,    1.0, 1.0, 1.0};  // wrong length
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    // ── B-3.4: setTxCfcPrecompDb ────────────────────────────────────────────
    //
    // Wraps SetTXACFCOMPPrecomp(channel, dB).
    // From Thetis wdsp/cfcomp.c:700-715 [v2.10.3.13].

    void setTxCfcPrecompDb_zero_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPrecompDb(0.0);
    }

    void setTxCfcPrecompDb_typical_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPrecompDb(6.0);
    }

    void setTxCfcPrecompDb_negative_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPrecompDb(-3.0);
    }

    // ── B-3.5: setTxCfcPostEqRunning ────────────────────────────────────────
    //
    // Wraps SetTXACFCOMPPeqRun(channel, on ? 1 : 0).
    // From Thetis wdsp/cfcomp.c:717-727 [v2.10.3.13].

    void setTxCfcPostEqRunning_true_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPostEqRunning(true);
    }

    void setTxCfcPostEqRunning_false_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPostEqRunning(false);
    }

    // ── B-3.6: setTxCfcPrePeqDb ─────────────────────────────────────────────
    //
    // Wraps SetTXACFCOMPPrePeq(channel, dB).
    // From Thetis wdsp/cfcomp.c:729-737 [v2.10.3.13].

    void setTxCfcPrePeqDb_zero_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPrePeqDb(0.0);
    }

    void setTxCfcPrePeqDb_typical_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCfcPrePeqDb(3.0);
    }

    // ── B-3.7: setTxCpdrOn ──────────────────────────────────────────────────
    //
    // Wraps SetTXACompressorRun(channel, on ? 1 : 0).  Note WDSP calls
    // TXASetupBPFilters internally on toggle (compress.c:106) — this
    // smoke-test exercises the no-crash path on an uninitialised channel.
    // From Thetis wdsp/compress.c:99-109 [v2.10.3.13].

    void setTxCpdrOn_true_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCpdrOn(true);
    }

    void setTxCpdrOn_false_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCpdrOn(false);
    }

    void setTxCpdrOn_idempotentToggle_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCpdrOn(true);
        ch.setTxCpdrOn(true);   // redundant — still safe
        ch.setTxCpdrOn(false);
        ch.setTxCpdrOn(false);  // redundant — still safe
    }

    // ── B-3.8: setTxCpdrGainDb ──────────────────────────────────────────────
    //
    // Wraps SetTXACompressorGain(channel, dB).
    // From Thetis wdsp/compress.c:111-117 [v2.10.3.13].

    void setTxCpdrGainDb_zero_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCpdrGainDb(0.0);
    }

    void setTxCpdrGainDb_typical_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCpdrGainDb(10.0);
    }

    // ── B-3.9: setTxCessbOn ─────────────────────────────────────────────────
    //
    // Wraps SetTXAosctrlRun(channel, on ? 1 : 0).  Note WDSP calls
    // TXASetupBPFilters internally (osctrl.c:148), and bp2.run is gated by
    // (compressor.run AND osctrl.run) at TXA.c:843-868 — so CESSB-on without
    // CPDR-on is effectively a no-op.  We don't enforce coupling here.
    // From Thetis wdsp/osctrl.c:142-150 [v2.10.3.13].

    void setTxCessbOn_true_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCessbOn(true);
    }

    void setTxCessbOn_false_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxCessbOn(false);
    }

    void setTxCessbOn_withoutCpdr_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // CESSB on without first turning CPDR on — match Thetis (no
        // enforcement; WDSP owns the bp2.run gating).
        ch.setTxCessbOn(true);
    }

    // ── Stage::CfComp / Stage::Compressor / Stage::OsCtrl in setStageRunning ─
    //
    // These case arms were originally added in 3M-1a Task C.4 and re-stamped
    // in 3M-3a-ii Batch 1 with the side-effect notes (TXASetupBPFilters
    // re-entry, bp2.run gating).  Verify each route remains a no-throw smoke.

    void setStageRunning_cfcomp_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::CfComp, true);
    }

    void setStageRunning_cfcomp_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::CfComp, false);
    }

    void setStageRunning_compressor_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Compressor, true);
    }

    void setStageRunning_compressor_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Compressor, false);
    }

    void setStageRunning_osctrl_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::OsCtrl, true);
    }

    void setStageRunning_osctrl_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::OsCtrl, false);
    }

    // ── Mixed configure-then-engage workflow ────────────────────────────────
    //
    // Mirrors the typical Setup-page flow: configure CFC profile + precomp +
    // post-EQ, then engage CPDR + CESSB.

    void mixedConfigureSequence_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // CFC config
        std::vector<double> F = {200.0, 1000.0, 3000.0};
        std::vector<double> G = {0.0,   3.0,    0.0};
        std::vector<double> E = {6.0,   6.0,    6.0};
        std::vector<double> Qg, Qe;
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
        ch.setTxCfcPrecompDb(6.0);
        ch.setTxCfcPostEqRunning(true);
        ch.setTxCfcPrePeqDb(3.0);
        ch.setTxCfcPosition(1);
        ch.setTxCfcRunning(true);
        // CPDR config
        ch.setTxCpdrGainDb(10.0);
        ch.setTxCpdrOn(true);
        // CESSB engage (paired with CPDR per WDSP bp2.run gating)
        ch.setTxCessbOn(true);
        // Disengage in reverse order
        ch.setTxCessbOn(false);
        ch.setTxCpdrOn(false);
        ch.setTxCfcRunning(false);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelCfcCpdrCessbSetters)
#include "tst_tx_channel_cfc_cpdr_cessb_setters.moc"
