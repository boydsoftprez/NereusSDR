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
// 3M-3a-ii Batch 1, plus the 3 TX phrot parameter wrappers added in
// Phase 3M-3a-ii Batch 1.6:
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
//   PhRot (3):  setTxPhrotCornerHz(double)   [3M-3a-ii Batch 1.6]
//               setTxPhrotNstages(int)       [3M-3a-ii Batch 1.6]
//               setTxPhrotReverse(bool)      [3M-3a-ii Batch 1.6]
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
//   2026-04-30 — Phase 3M-3a-ii Batch 1.5 — bundled cfcomp upgraded to
//                 Thetis v2.10.3.13.  Qg/Qe assertions reworded from
//                 "validated and dropped" to "live pass-through" smoke;
//                 empty-Qg/Qe → nullptr coverage retained; length-mismatch
//                 coverage retained.  J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-04-30 — Phase 3M-3a-ii Batch 1.6 — extended with smoke coverage
//                 for the 3 new TX phrot parameter wrappers
//                 (setTxPhrotCornerHz / setTxPhrotNstages /
//                 setTxPhrotReverse) plus a Stage::PhRot setStageRunning
//                 regression arm.  J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
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
    // Wraps SetTXACFCOMPprofile.  Validates length consistency on F/G/E (and
    // any non-empty Qg/Qe), then forwards F/G/E plus Qg/Qe (or nullptr for
    // empty-vector skirt) into WDSP.
    // From Thetis wdsp/cfcomp.c:656-698 [v2.10.3.13] — full 7-arg signature
    // (bundled at third_party/wdsp/src/cfcomp.c since 3M-3a-ii Batch 1.5).

    void setTxCfcProfile_threeBand_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Three-band profile with empty Qg/Qe — exercises the
        // empty-vector → nullptr forwarding path (linear-interpolation
        // skirt branch in cfcomp.c:171-183).
        std::vector<double> F = {200.0, 1000.0, 3000.0};
        std::vector<double> G = {0.0,   3.0,    0.0};
        std::vector<double> E = {6.0,   6.0,    6.0};
        std::vector<double> Qg;  // empty — forwards as nullptr
        std::vector<double> Qe;  // empty — forwards as nullptr
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_withQgQe_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Same profile with explicit Qg/Qe present — exercises the live
        // pass-through path (Gaussian-tail Q-shaped interpolation branch
        // in cfcomp.c:184-296).
        std::vector<double> F  = {200.0, 1000.0, 3000.0};
        std::vector<double> G  = {0.0,   3.0,    0.0};
        std::vector<double> E  = {6.0,   6.0,    6.0};
        std::vector<double> Qg = {1.0,   1.0,    1.0};
        std::vector<double> Qe = {1.0,   1.0,    1.0};
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_withQgOnly_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Qg present, Qe empty — exercises the asymmetric skirt path
        // (Q-shaped gain skirt + linear-interpolation ceiling skirt).
        // Confirms either Qg or Qe may be NULL independently.
        std::vector<double> F  = {200.0, 1000.0, 3000.0};
        std::vector<double> G  = {0.0,   3.0,    0.0};
        std::vector<double> E  = {6.0,   6.0,    6.0};
        std::vector<double> Qg = {1.0,   1.0,    1.0};
        std::vector<double> Qe;  // empty — forwards as nullptr
        ch.setTxCfcProfile(F, G, E, Qg, Qe);
    }

    void setTxCfcProfile_withQeOnly_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Qe present, Qg empty — exercises the other asymmetric skirt path
        // (linear-interpolation gain skirt + Q-shaped ceiling skirt).
        std::vector<double> F  = {200.0, 1000.0, 3000.0};
        std::vector<double> G  = {0.0,   3.0,    0.0};
        std::vector<double> E  = {6.0,   6.0,    6.0};
        std::vector<double> Qg;  // empty — forwards as nullptr
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

    // 3M-3a-ii Batch 1.6: TX phrot parameter wrappers ─────────────────────────
    //
    // Three thin wrappers over the WDSP TXA phase-rotator parameter setters
    // that 3M-3a-i deferred (the Run flag was wired via Stage::PhRot in
    // 3M-1; the parameter setters belong with the CFC tab schema work in
    // 3M-3a-ii — their persistence keys live in the Thetis tpDSPCFC tab).
    //
    //   setTxPhrotCornerHz(double)  → SetTXAPHROTCorner    (iir.c:675)
    //   setTxPhrotNstages(int)      → SetTXAPHROTNstages   (iir.c:686)
    //   setTxPhrotReverse(bool)     → SetTXAPHROTReverse   (iir.c:697)
    //
    // Test strategy: pure smoke / does-not-crash, matching the rest of the
    // file.  WDSP setter calls fall through the rsmpin.p == nullptr null-
    // guard in HAVE_WDSP-linked builds; HAVE_WDSP-undefined builds exercise
    // the stub path.

    // ── B-3.10: setTxPhrotCornerHz ──────────────────────────────────────────
    //
    // Wraps SetTXAPHROTCorner(channel, hz).
    // From Thetis wdsp/iir.c:675-683 [v2.10.3.13].

    void setTxPhrotCornerHz_typical_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPhrotCornerHz(338.0);  // Thetis tpDSPCFC default freq
    }

    void setTxPhrotCornerHz_zero_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPhrotCornerHz(0.0);
    }

    void setTxPhrotCornerHz_repeated_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Each call rebuilds the all-pass bank (decalc + calc inside WDSP).
        // Smoke that repeated sets don't crash on an uninitialised channel.
        ch.setTxPhrotCornerHz(200.0);
        ch.setTxPhrotCornerHz(338.0);
        ch.setTxPhrotCornerHz(500.0);
    }

    // ── B-3.11: setTxPhrotNstages ───────────────────────────────────────────
    //
    // Wraps SetTXAPHROTNstages(channel, nstages).
    // From Thetis wdsp/iir.c:686-694 [v2.10.3.13].

    void setTxPhrotNstages_typical_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPhrotNstages(8);  // Thetis tpDSPCFC default stages
    }

    void setTxPhrotNstages_one_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPhrotNstages(1);
    }

    void setTxPhrotNstages_repeated_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        // Each call rebuilds the coefficient bank.  Smoke repeated sets.
        ch.setTxPhrotNstages(4);
        ch.setTxPhrotNstages(8);
        ch.setTxPhrotNstages(12);
    }

    // ── B-3.12: setTxPhrotReverse ───────────────────────────────────────────
    //
    // Wraps SetTXAPHROTReverse(channel, reverse ? 1 : 0).
    // From Thetis wdsp/iir.c:697-703 [v2.10.3.13].  Cheap — no rebuild.

    void setTxPhrotReverse_true_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPhrotReverse(true);
    }

    void setTxPhrotReverse_false_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPhrotReverse(false);
    }

    void setTxPhrotReverse_idempotentToggle_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPhrotReverse(true);
        ch.setTxPhrotReverse(true);   // redundant — still safe
        ch.setTxPhrotReverse(false);
        ch.setTxPhrotReverse(false);  // redundant — still safe
    }

    // ── Stage::PhRot in setStageRunning regression ──────────────────────────
    //
    // Stage::PhRot was wired to SetTXAPHROTRun in 3M-3a-i (the parameter
    // setters above are deferred Batch 1.6 work).  Verify the case arm
    // remains a no-throw smoke alongside the new parameter coverage.

    void setStageRunning_phrot_on_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::PhRot, true);
    }

    void setStageRunning_phrot_off_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::PhRot, false);
    }

    // ── PhRot configure-then-engage sequence ────────────────────────────────
    //
    // Mirrors a typical Setup-page flow: configure all three parameters,
    // then engage Stage::PhRot via setStageRunning.

    void phrotConfigureSequence_doesNotCrash()
    {
        TxChannel ch(kTxChannelId);
        ch.setTxPhrotCornerHz(338.0);
        ch.setTxPhrotNstages(8);
        ch.setTxPhrotReverse(false);
        ch.setStageRunning(TxChannel::Stage::PhRot, true);
        ch.setStageRunning(TxChannel::Stage::PhRot, false);
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
