// =================================================================
// src/core/MoxController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. The MOX state machine and its enumerated
// states are designed for NereusSDR's Qt6 architecture; logic and
// timer constants are derived from Thetis:
//   console.cs:29311-29678 [v2.10.3.13] — chkMOX_CheckedChanged2
//   console.cs:19659-19698 [v2.10.3.13] — mox_delay / space_mox_delay /
//     key_up_delay / rf_delay / ptt_out_delay field declarations
//   console.cs:18494-18502 [v2.10.3.13] — break_in_delay field declaration
//
// Upstream file has no per-member inline attribution tags in this
// state-machine region except where noted with inline cites below.
//
// Disambiguation: this class is the *radio-level* MOX state machine.
// PttMode (src/core/PttMode.h) carries the Thetis PTTMode enum.
// PttSource (src/core/PttSource.h) is a NereusSDR-native enum tracking
// the UI surface that triggered the PTT event (Diagnostics page).
// None of these three are supersets of each other; all coexist.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-25 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//                 Task: Phase 3M-1a Task B.2 — MoxController skeleton
//                 (Codex P2 ordering). State-machine transitions
//                 derived from chkMOX_CheckedChanged2
//                 (console.cs:29311-29678 [v2.10.3.13]).
//   2026-04-25 — Phase 3M-1a Task B.3 — 6 QTimer chains wired.
//                 Replaces direct-to-terminal advanceState jump with
//                 timer-driven walk through transient states.
//                 moxStateChanged now fires at end of walk (fully
//                 engaged / fully released) rather than at setMox entry.
//   2026-04-25 — Phase 3M-1a Task B.4 — 6 phase signals wired.
//                 txAboutToBegin / hardwareFlipped(true) / txReady emitted
//                 in setMox(true) and onRfDelayElapsed.
//                 txAboutToEnd / hardwareFlipped(false) / txaFlushed /
//                 rxReady emitted in setMox(false), onKeyUpDelayElapsed,
//                 and onPttOutElapsed.
//                 Codex P1: subscribers attach to phase signals.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis state-machine
// derived values are cited inline below.

#include "core/MoxController.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MoxController::MoxController(QObject* parent)
    : QObject(parent)
    , m_rfDelayTimer(this)
    , m_moxDelayTimer(this)
    , m_spaceDelayTimer(this)
    , m_keyUpDelayTimer(this)
    , m_pttOutDelayTimer(this)
    , m_breakInDelayTimer(this)
{
    // All timers are single-shot — each fires once then stops.
    m_rfDelayTimer.setSingleShot(true);
    m_moxDelayTimer.setSingleShot(true);
    m_spaceDelayTimer.setSingleShot(true);
    m_keyUpDelayTimer.setSingleShot(true);
    m_pttOutDelayTimer.setSingleShot(true);
    m_breakInDelayTimer.setSingleShot(true);

    // Set default intervals from Thetis constants.
    // From Thetis console.cs:19687 — private int rf_delay = 30 [v2.10.3.13]
    m_rfDelayTimer.setInterval(kRfDelayMs);
    // From Thetis console.cs:19659 — private int mox_delay = 10 [v2.10.3.13]
    m_moxDelayTimer.setInterval(kMoxDelayMs);
    // From Thetis console.cs:19669 — private int space_mox_delay = 0 [v2.10.3.13]
    m_spaceDelayTimer.setInterval(kSpaceDelayMs);
    // From Thetis console.cs:19677 — private int key_up_delay = 10 [v2.10.3.13]
    m_keyUpDelayTimer.setInterval(kKeyUpDelayMs);
    // From Thetis console.cs:19694 — private int ptt_out_delay = 20 [v2.10.3.13]
    m_pttOutDelayTimer.setInterval(kPttOutDelayMs);
    // From Thetis console.cs:18494 — private double break_in_delay = 300 [v2.10.3.13]
    m_breakInDelayTimer.setInterval(kBreakInDelayMs);

    // Wire timer timeouts to their advancement slots.
    connect(&m_rfDelayTimer,      &QTimer::timeout, this, &MoxController::onRfDelayElapsed);
    connect(&m_moxDelayTimer,     &QTimer::timeout, this, &MoxController::onMoxDelayElapsed);
    connect(&m_spaceDelayTimer,   &QTimer::timeout, this, &MoxController::onSpaceDelayElapsed);
    connect(&m_keyUpDelayTimer,   &QTimer::timeout, this, &MoxController::onKeyUpDelayElapsed);
    connect(&m_pttOutDelayTimer,  &QTimer::timeout, this, &MoxController::onPttOutElapsed);
    connect(&m_breakInDelayTimer, &QTimer::timeout, this, &MoxController::onBreakInDelayElapsed);
}

MoxController::~MoxController() = default;

// ---------------------------------------------------------------------------
// setTimerIntervals — test seam. FOR TESTING ONLY.
//
// Overrides the Thetis default intervals. Production code must not call
// this. Tests use setTimerIntervals(0, 0, 0, 0, 0, 0) for synchronous-
// equivalent behavior so QCoreApplication::processEvents() drives the
// entire state walk without waiting for wall-clock time.
// ---------------------------------------------------------------------------
void MoxController::setTimerIntervals(int rfMs, int moxMs, int spaceMs,
                                      int keyUpMs, int pttOutMs, int breakInMs)
{
    m_rfDelayTimer.setInterval(rfMs);
    m_moxDelayTimer.setInterval(moxMs);
    m_spaceDelayTimer.setInterval(spaceMs);
    m_keyUpDelayTimer.setInterval(keyUpMs);
    m_pttOutDelayTimer.setInterval(pttOutMs);
    m_breakInDelayTimer.setInterval(breakInMs);
}

// ---------------------------------------------------------------------------
// setMox — Codex P2-ordered MOX toggle.
//
// State machine derived from chkMOX_CheckedChanged2
// (console.cs:29311-29678 [v2.10.3.13]).
//
// ORDERING MUST NOT BE CHANGED (Codex P2, PR #139):
//   Step 1 — safety effects run BEFORE the idempotent guard.
//             A repeated setMox(true) must still drive safety effects
//             (e.g. re-assert Alex routing if the band changed under us)
//             even if m_mox is already true.
//   Step 2 — idempotent guard: bail if no real state change.
//   Step 3 — commit new state.
//   Step 4 — start timer-driven walk through transient states.
//             moxStateChanged fires at the END of the walk, not here.
//
// RX→TX path (non-CW, per console.cs:29589-29598 [v2.10.3.13]):
//   Rx → RxToTxRfDelay (rf_delay 30ms) → Tx
//   After rf_delay fires: AudioMOXChanged + WDSP TX on (wired in F.1).
//
// TX→RX path (non-CW, per console.cs:29602-29628 [v2.10.3.13]):
//   console.cs:29603: Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE
//   spaceDelay (default 0ms, skipped when 0) →
//   WDSP TX off (wired in F.1) →
//   Tx → TxToRxInFlight (mox_delay 10ms) →
//   TxToRxFlush (ptt_out_delay 20ms) → Rx
//   After ptt_out_delay fires: WDSP RX channels on (wired in F.1).
// ---------------------------------------------------------------------------
void MoxController::setMox(bool on)
{
    // ── Step 1: Safety effects (BEFORE idempotent guard — Codex P2) ─────────
    // F.1 wires: AlexController::applyAntennaForBand(currentBand, isTx)
    //            StepAttenuatorController TX-path activation / RX restore
    //            RadioConnection::setMoxBit(isTx) + setTrxRelayBit(isTx)
    runMoxSafetyEffects(on);

    // ── Step 2: Idempotent guard ──────────────────────────────────────────────
    if (m_mox == on) {
        return;  // no real transition — no state advance, no timer chain
    }

    // ── Step 3: Commit new MOX state ─────────────────────────────────────────
    m_mox = on;

    // ── Step 4: Start timer-driven walk ───────────────────────────────────────
    // Cancel any timers still running from a previous (rapid) transition.
    stopAllTimers();

    if (on) {
        // RX→TX path:
        // From Thetis console.cs:29589-29598 [v2.10.3.13]:
        //   if (rf_delay > 0) Thread.Sleep(rf_delay);
        //   AudioMOXChanged(tx);
        //   WDSP.SetChannelState(WDSP.id(1, 0), 1, 0);
        // Note: console.cs:29603 (5 lines past cite end) carries
        //   Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE
        //
        // Phase signal ordering (Codex P1 — pre-code review §1.4):
        //   Step 3 — emit txAboutToBegin()
        //   Step 5 — emit hardwareFlipped(true) BEFORE rf_delay starts
        //             (matches HdwMOXChanged at console.cs:29569-29588
        //              [v2.10.3.13] which fires before Thread.Sleep(rf_delay))
        //   Walk: Rx → RxToTxRfDelay → (timer fires) → Tx
        //   Step 9 — emit txReady() in onRfDelayElapsed()
        //   Step 10 — emit moxStateChanged(true) after txReady()
        emit txAboutToBegin();                          // step 3
        emit hardwareFlipped(true);                     // step 5 — before rfDelay
        advanceState(MoxState::RxToTxRfDelay);
        m_rfDelayTimer.start();
    } else {
        // TX→RX path:
        // From Thetis console.cs:29602-29628 [v2.10.3.13]:
        //   if (space_mox_delay > 0) Thread.Sleep(space_mox_delay);  // default 0 // from PSDR MW0LGE
        //   ... WDSP TX off ...
        //   if (mox_delay > 0) Thread.Sleep(mox_delay);              // 10ms, non-CW
        //   ... AudioMOXChanged + HdwMOXChanged ...
        //   if (ptt_out_delay > 0) Thread.Sleep(ptt_out_delay);      // 20ms
        //   ... WDSP RX on ...
        //
        // Phase signal ordering (Codex P1 — pre-code review §1.4):
        //   Step 3 — emit txAboutToEnd()
        //   Step 4 — emit hardwareFlipped(false) — symmetric with RX→TX:
        //             routing clears before in-flight sample flush.
        //   Walk: Tx → TxToRxInFlight (keyUpDelayTimer, 10ms) →
        //              TxToRxFlush   (pttOutDelayTimer, 20ms) → Rx
        //   Step 7 — emit txaFlushed() in onKeyUpDelayElapsed()
        //   Step 12 — emit rxReady() in onPttOutElapsed()
        //   Step 13 — emit moxStateChanged(false) after rxReady()
        //
        // spaceDelay is skipped when kSpaceDelayMs == 0 (matches the
        // Thetis `if (space_mox_delay > 0)` guard).
        emit txAboutToEnd();                            // step 3
        emit hardwareFlipped(false);                    // step 4 — before flush
        advanceState(MoxState::TxToRxInFlight);
        m_keyUpDelayTimer.start();
    }
    // NOTE: moxStateChanged is NOT emitted here.  It is emitted at the END
    // of the timer walk (in onRfDelayElapsed for TX, onPttOutElapsed for RX)
    // so subscribers see "MOX fully engaged" / "MOX fully released".
}

// ---------------------------------------------------------------------------
// setPttMode — idempotent PTT mode setter.
// ---------------------------------------------------------------------------
void MoxController::setPttMode(PttMode mode)
{
    if (m_pttMode == mode) {
        return;
    }
    m_pttMode = mode;
    emit pttModeChanged(mode);
}

// ---------------------------------------------------------------------------
// advanceState — internal state-machine step.
//
// Sets m_state and emits stateChanged. Called from setMox() to enter the
// first transient state, then from QTimer slots to drive through each
// subsequent state.
// ---------------------------------------------------------------------------
void MoxController::advanceState(MoxState newState)
{
    if (m_state == newState) {
        return;
    }
    m_state = newState;
    emit stateChanged(newState);
}

// ---------------------------------------------------------------------------
// stopAllTimers — cancel any running timers.
//
// Called at the top of setMox() to prevent stale timer firings if the
// user toggles MOX rapidly (e.g. TX→RX during RX→TX walk).
// ---------------------------------------------------------------------------
void MoxController::stopAllTimers()
{
    m_rfDelayTimer.stop();
    m_moxDelayTimer.stop();
    m_spaceDelayTimer.stop();
    m_keyUpDelayTimer.stop();
    m_pttOutDelayTimer.stop();
    // m_breakInDelayTimer is never started in 3M-1a so stop() is a no-op,
    // but include it for completeness so future 3M-2 CW code gets the guard
    // for free.
    m_breakInDelayTimer.stop();
}

// ---------------------------------------------------------------------------
// runMoxSafetyEffects — placeholder for F.1 wiring.
//
// Called on EVERY setMox() invocation, including idempotent ones, so
// that safety effects cannot be skipped by a repeated call.
//
// TODO [3M-1a F.1]: wire body with:
//   - AlexController::applyAntennaForBand(currentBand(), isTx)
//   - StepAttenuatorController TX-path activation / RX restore
//   - RadioConnection::setMoxBit(isTx) + setTrxRelayBit(isTx)
// ---------------------------------------------------------------------------
void MoxController::runMoxSafetyEffects(bool /*newMox*/)
{
    // intentionally empty in B.2/B.3 — see TODO above
}

// ---------------------------------------------------------------------------
// Timer slots
// ---------------------------------------------------------------------------

// onRfDelayElapsed — fires after rf_delay (30ms) on RX→TX path.
//
// From Thetis console.cs:29592-29598 [v2.10.3.13]:
//   if (rf_delay > 0) Thread.Sleep(rf_delay);
//   AudioMOXChanged(tx);                     ← wired in F.1
//   WDSP.SetChannelState(WDSP.id(1, 0), 1, 0); ← wired in F.1
// Note: console.cs:29603 (5 lines past cite end) carries
//   Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE
//
// Phase signals (Codex P1, pre-code review §1.4):
//   Step 8  — advance to terminal Tx state
//   Step 9  — emit txReady() — TX I/Q stream + audio MOX from this point
//   Step 10 — emit moxStateChanged(true) (diagnostic / integration signal)
void MoxController::onRfDelayElapsed()
{
    // TODO [3M-1a F.1]: AudioMOXChanged(true) + WDSP TX channel on here.
    advanceState(MoxState::Tx);
    emit txReady();                                     // step 9
    emit moxStateChanged(true);                         // step 10
}

// onMoxDelayElapsed — fires when m_moxDelayTimer elapses.
//
// Reserved for RX→TX mox_delay settle in future phases; not connected
// to any 3M-1a path. Declared to complete the 6-timer API.
void MoxController::onMoxDelayElapsed()
{
    // Not started in 3M-1a. Placeholder for future RX→TX settle phase.
}

// onSpaceDelayElapsed — fires when m_spaceDelayTimer elapses.
//
// From Thetis console.cs:29602-29604 [v2.10.3.13]:
//   if (space_mox_delay > 0) Thread.Sleep(space_mox_delay); // from PSDR MW0LGE
//
// Default is 0ms so this timer is never started in 3M-1a. Declared
// for completeness; 3M-2 may start it for non-zero space_mox_delay.
void MoxController::onSpaceDelayElapsed()
{
    // Not started in 3M-1a (kSpaceDelayMs == 0, matches Thetis skip guard).
}

// onKeyUpDelayElapsed — fires after keyUpDelay (10ms) on TX→RX path.
//
// From Thetis console.cs:29617-29618 [v2.10.3.13]:
//   if (mox_delay > 0)
//       Thread.Sleep(mox_delay); // default 10, allows in-flight samples to clear
//
// (3M-2 will also branch here for CW key_up_delay, also 10ms by default.)
//
// Phase signals (Codex P1, pre-code review §1.4):
//   Step 7  — emit txaFlushed() — in-flight samples cleared; TX channel may stop
//   Step 8  — advance to TxToRxFlush state
//   Step 9  — start ptt_out_delay timer
void MoxController::onKeyUpDelayElapsed()
{
    // TODO [3M-1a F.1]: UpdateDDCs + UpdateAAudioMixerStates + AudioMOXChanged(false)
    //                   + HdwMOXChanged(false) here.
    emit txaFlushed();                                  // step 7
    advanceState(MoxState::TxToRxFlush);
    m_pttOutDelayTimer.start();
}

// onPttOutElapsed — fires after ptt_out_delay (20ms) on TX→RX path.
//
// From Thetis console.cs:29627-29628 [v2.10.3.13]:
//   if (ptt_out_delay > 0)
//       Thread.Sleep(ptt_out_delay);  //wcp:  added 2018-12-24, time for HW to switch
//
// Phase signals (Codex P1, pre-code review §1.4):
//   Step 11 — advance to terminal Rx state
//   Step 12 — emit rxReady() — RX channel active from this point
//   Step 13 — emit moxStateChanged(false) (diagnostic / integration signal)
void MoxController::onPttOutElapsed()
{
    // TODO [3M-1a F.1]: WDSP.SetChannelState(WDSP.id(0, 0), 1, 0) (RX1 on) here.
    advanceState(MoxState::Rx);
    emit rxReady();                                     // step 12
    emit moxStateChanged(false);                        // step 13
}

// onBreakInDelayElapsed — fires when m_breakInDelayTimer elapses.
//
// From Thetis console.cs:18494 [v2.10.3.13]:
//   private double break_in_delay = 300;
//
// Reserved for 3M-2 CW QSK / break-in. NOT started from any 3M-1a path.
// Declared here so the class is structured for 3M-2 from day one.
void MoxController::onBreakInDelayElapsed()
{
    // Not started in 3M-1a. Placeholder for 3M-2 CW QSK break-in.
}

} // namespace NereusSDR
