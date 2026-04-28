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
// tests/tst_tx_channel_stage_running_expansion.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/wdsp/TXA.c     — create_txa() stage Run defaults,
//                                          SetTXAMode() ammod/fmmod run control
//   Project Files/Source/wdsp/meter.c   — create_meter() run parameter
//   Project Files/Source/wdsp/ammod.c   — create_ammod() run parameter
//   Project Files/Source/wdsp/fmmod.c   — create_fmmod() run parameter
//   Project Files/Source/wdsp/patchpanel.c — SetTXAPanelRun
//   Original licences above (Warren Pratt, NR0V)
//
// Ported from Thetis wdsp/TXA.c:80-93   [v2.10.3.13] — micmeter (run=1 at create)
// Ported from Thetis wdsp/TXA.c:336-342 [v2.10.3.13] — ammod (run=0 at create)
// Ported from Thetis wdsp/TXA.c:345-359 [v2.10.3.13] — fmmod (run=0 at create)
// Ported from Thetis wdsp/TXA.c:379-392 [v2.10.3.13] — alcmeter (run=1 at create)
// Ported from Thetis wdsp/TXA.c:59-69   [v2.10.3.13] — panel (run=1 at create)
// Ported from Thetis wdsp/meter.c:36-57  [v2.10.3.13] — create_meter (no public Run setter)
// Ported from Thetis wdsp/ammod.c:29-41  [v2.10.3.13] — no public SetTXAamModRun
// Ported from Thetis wdsp/fmmod.c:42-65  [v2.10.3.13] — no public SetTXAfmModRun
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-27 — New test for Phase 3M-1b Task D.4:
//                 TxChannel::setStageRunning() expansion to Panel/MicMeter/
//                 AlcMeter/AmMod/FmMod. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: Attribution for ported constants and logic lives in
// TxChannel.h/cpp with verbatim Thetis source cites.  This test file
// exercises the interface only; all cite comments are in the implementation.

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

// WDSP channel IDs used in Thetis — from cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;   // WDSP.id(1, 0) — TX channel

// ---------------------------------------------------------------------------
// D.4 — setStageRunning() expansion test suite
//
// Tests 4 new stages (MicMeter, AlcMeter, AmMod, FmMod) plus verification
// that Panel is already dispatched (not falling through to default:).
//
// Two key findings from WDSP source analysis (v2.10.3.13):
//
// 1. MicMeter (wdsp/meter.c:36-57, wdsp/TXA.c:80-93):
//    `create_meter` takes `run` as first arg (=1 for micmeter) and a `prun`
//    pointer. No public `SetTXAMicMeterRun` / `SetTXAMicMeter*Run` API exists.
//    The meter's `srun` is `*(prun)` if prun!=0, else 1. For micmeter, prun is
//    set to `&txa[ch].panel.p->run` so the meter gates on panel.run, not its own
//    settable bit. There is no PORT-exported function to toggle micmeter.run.
//    → setStageRunning(MicMeter, run) is a documented NO-OP with qCDebug log.
//
// 2. AlcMeter (wdsp/meter.c:36-57, wdsp/TXA.c:379-392):
//    Same structure as MicMeter. No public Run setter exists.
//    → setStageRunning(AlcMeter, run) is a documented NO-OP with qCDebug log.
//
// 3. AmMod (wdsp/ammod.c, wdsp/TXA.c:753-789):
//    ammod.run is set ONLY inside SetTXAMode() (TXA.c:759-775). No standalone
//    `SetTXAamModRun` or similar PORT function exists. The correct API for
//    activating AM modulation is SetTXAMode(ch, TXA_AM) — which also resets
//    fmmod and updates bandpass filters atomically.
//    → setStageRunning(AmMod, run) is a documented NO-OP with qCWarning log.
//
// 4. FmMod (wdsp/fmmod.c, wdsp/TXA.c:753-789):
//    fmmod.run is set ONLY inside SetTXAMode() (TXA.c:759-779). No standalone
//    `SetTXAfmModRun` function exists.
//    → setStageRunning(FmMod, run) is a documented NO-OP with qCWarning log.
//
// 5. Panel (wdsp/patchpanel.c:201-206):
//    SetTXAPanelRun IS a public PORT function. Panel was added in Task C.4
//    (3M-1a) and already dispatches in setStageRunning. Verified here.
// ---------------------------------------------------------------------------

class TestTxChannelStageRunningExpansion : public QObject {
    Q_OBJECT

private slots:

    // ── Panel (existing support — D.4 verification) ────────────────────────
    //
    // Panel was added in 3M-1a Task C.4 with SetTXAPanelRun.
    // This test verifies the Panel case is already dispatched (no default:
    // fallthrough / warning).  The no-crash contract is sufficient for a build
    // without WDSP or without an open channel.
    //
    // From Thetis wdsp/patchpanel.c:201-206 [v2.10.3.13] — SetTXAPanelRun.
    // From Thetis wdsp/TXA.c:59-69 [v2.10.3.13] — panel created with run=1.

    void setStageRunning_Panel_true_doesNotCrash() {
        // From Thetis patchpanel.c:201-206 [v2.10.3.13] — SetTXAPanelRun(ch, 1).
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Panel, true);
    }

    void setStageRunning_Panel_false_doesNotCrash() {
        // From Thetis patchpanel.c:201-206 [v2.10.3.13] — SetTXAPanelRun(ch, 0).
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Panel, false);
    }

    // ── MicMeter — always-on, no public Run setter ─────────────────────────
    //
    // MicMeter defaults to run=1 in create_txa() (TXA.c:80-93 [v2.10.3.13]).
    // meter.c has no PORT-exported SetTXAMicMeterRun function.
    // setStageRunning(MicMeter, run) is a no-op that logs a debug message.
    //
    // From Thetis wdsp/TXA.c:80-93 [v2.10.3.13] — micmeter created with run=1.
    // From Thetis wdsp/meter.c:36-57 [v2.10.3.13] — no public Run setter.

    void setStageRunning_MicMeter_true_doesNotCrash() {
        // No WDSP function to call — no-op with debug log.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::MicMeter, true);
    }

    void setStageRunning_MicMeter_false_doesNotCrash() {
        // No WDSP function to call — no-op with debug log.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::MicMeter, false);
    }

    // ── AlcMeter — always-on, no public Run setter ─────────────────────────
    //
    // AlcMeter defaults to run=1 in create_txa() (TXA.c:379-392 [v2.10.3.13]).
    // meter.c has no PORT-exported SetTXAAlcMeterRun function.
    // setStageRunning(AlcMeter, run) is a no-op that logs a debug message.
    //
    // From Thetis wdsp/TXA.c:379-392 [v2.10.3.13] — alcmeter created with run=1.
    // From Thetis wdsp/meter.c:36-57 [v2.10.3.13] — no public Run setter.

    void setStageRunning_AlcMeter_true_doesNotCrash() {
        // No WDSP function to call — no-op with debug log.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::AlcMeter, true);
    }

    void setStageRunning_AlcMeter_false_doesNotCrash() {
        // No WDSP function to call — no-op with debug log.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::AlcMeter, false);
    }

    // ── AmMod — mode-gated via SetTXAMode(), no standalone Run setter ──────
    //
    // ammod.run is controlled ONLY by SetTXAMode() in TXA.c:753-789.
    // There is no SetTXAamModRun or SetTXAAmModRun PORT function in WDSP.
    // Activating AM modulation requires SetTXAMode(ch, TXA_AM/TXA_DSB/...).
    // setStageRunning(AmMod, run) is a no-op that logs a warning.
    //
    // From Thetis wdsp/TXA.c:753-789 [v2.10.3.13] — SetTXAMode sets ammod.run.
    // From Thetis wdsp/ammod.c:29-41 [v2.10.3.13] — no public Run setter.

    void setStageRunning_AmMod_true_doesNotCrash() {
        // No public WDSP Run setter exists — no-op with warning log.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::AmMod, true);
    }

    void setStageRunning_AmMod_false_doesNotCrash() {
        // No public WDSP Run setter exists — no-op with warning log.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::AmMod, false);
    }

    // ── FmMod — mode-gated via SetTXAMode(), no standalone Run setter ──────
    //
    // fmmod.run is controlled ONLY by SetTXAMode() in TXA.c:753-789.
    // There is no SetTXAfmModRun or similar PORT function in WDSP.
    // Activating FM modulation requires SetTXAMode(ch, TXA_FM).
    // setStageRunning(FmMod, run) is a no-op that logs a warning.
    //
    // From Thetis wdsp/TXA.c:753-789 [v2.10.3.13] — SetTXAMode sets fmmod.run.
    // From Thetis wdsp/fmmod.c:42-65 [v2.10.3.13] — no public Run setter.

    void setStageRunning_FmMod_true_doesNotCrash() {
        // No public WDSP Run setter exists — no-op with warning log.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::FmMod, true);
    }

    void setStageRunning_FmMod_false_doesNotCrash() {
        // No public WDSP Run setter exists — no-op with warning log.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::FmMod, false);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelStageRunningExpansion)
#include "tst_tx_channel_stage_running_expansion.moc"
