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
//   console.cs:29978-30157 [v2.10.3.13] — chkTUN_CheckedChanged (TUN
//     slot; only the _manual_mox + _current_ptt_mode flag assignments
//     at lines 30093-30094 and the _manual_mox clear at line 30142
//     are ported here; the remainder is split across Tasks C.3 / G.3 / G.4)
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
//   2026-04-25 — Phase 3M-1a Task B.5 — setTune(bool) implemented.
//                 Drives MOX through the existing state machine and
//                 manages m_manualMox / m_pttMode = Manual flags.
//                 Ports the flag-assignment block at
//                 console.cs:30081-30094 and the clear at
//                 console.cs:30142 [v2.10.3.13]. The fuller
//                 chkTUN_CheckedChanged behaviour is split across
//                 Tasks C.3 / G.3 / G.4.
//   2026-04-28 — Phase 3M-1b Task H.1 — isVoiceMode(), recomputeVoxRun(),
//                 setVoxEnabled(bool), onModeChanged(DSPMode) implemented.
//                 Voice-family gate: LSB/USB/DSB/AM/SAM/FM/DIGL/DIGU.
//                 recomputeVoxRun() emits voxRunRequested idempotently on
//                 the gated value (not the raw inputs).
//                 Ports CMSetTXAVoxRun (cmaster.cs:1039-1052 [v2.10.3.13]).
//   2026-04-28 — Phase 3M-1b Task H.2 — computeScaledThreshold(),
//                 recomputeVoxThreshold(), setVoxThreshold(int),
//                 onMicBoostChanged(bool), setVoxGainScalar(float) implemented.
//                 Ports the two-step Thetis formula:
//                   1. dB→linear (setup.cs:18911 [v2.10.3.13]):
//                        thresh = Math.Pow(10.0, dB / 20.0)
//                   2. mic-boost scaling (cmaster.cs:1057 [v2.10.3.13]):
//                        if (MicBoost) thresh *= VOXGain
//                 recomputeVoxThreshold() emits voxThresholdRequested
//                 idempotently (NAN sentinel primes WDSP on first call).
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
// isVoiceMode — returns true for the 8 voice-family DSP modes.
//
// Porting from cmaster.cs:CMSetTXAVoxRun:1043-1050 — original C# logic:
//   bool run = Audio.VOXEnabled &&
//       (mode == DSPMode.LSB  ||
//        mode == DSPMode.USB  ||
//        mode == DSPMode.DSB  ||
//        mode == DSPMode.AM   ||
//        mode == DSPMode.SAM  ||
//        mode == DSPMode.FM   ||
//        mode == DSPMode.DIGL ||
//        mode == DSPMode.DIGU);
//
// From Thetis Project Files/Source/Console/cmaster.cs:1039-1052 [v2.10.3.13]
// ---------------------------------------------------------------------------
bool MoxController::isVoiceMode(DSPMode mode) const noexcept
{
    return mode == DSPMode::LSB
        || mode == DSPMode::USB
        || mode == DSPMode::DSB
        || mode == DSPMode::AM
        || mode == DSPMode::SAM
        || mode == DSPMode::FM
        || mode == DSPMode::DIGL
        || mode == DSPMode::DIGU;
}

// ---------------------------------------------------------------------------
// recomputeVoxRun — recalculate gated VOX state and emit if changed.
//
// Idempotent on the EMITTED (gated) value, not the raw inputs.
// This prevents spurious voxRunRequested(false) calls when the user
// toggles VOX while already in CW mode (gated stays false both times).
//
// From Thetis Project Files/Source/Console/cmaster.cs:1039-1052 [v2.10.3.13]
//   cmaster.SetDEXPRunVox(id, run);  ← 'run' is the gated value
// ---------------------------------------------------------------------------
void MoxController::recomputeVoxRun()
{
    const bool gated = m_voxEnabled && isVoiceMode(m_currentMode);
    if (gated == m_lastVoxRunGated) {
        return;  // idempotent on emitted state; no spurious emit
    }
    m_lastVoxRunGated = gated;
    emit voxRunRequested(gated);
}

// ---------------------------------------------------------------------------
// setVoxEnabled — engage/disengage VOX with voice-family mode-gate.
//
// Updates m_voxEnabled and calls recomputeVoxRun(). If the mode is not
// in the voice family (e.g. CWL), the gated result stays false and no
// signal is emitted.
//
// Wired by RadioModel H.1:
//   TransmitModel::voxEnabledChanged → MoxController::setVoxEnabled
//
// From Thetis Project Files/Source/Console/cmaster.cs:1039-1052 [v2.10.3.13]:
//   bool run = Audio.VOXEnabled && (mode == DSPMode.LSB || ...)
//   cmaster.SetDEXPRunVox(id, run);
// ---------------------------------------------------------------------------
void MoxController::setVoxEnabled(bool on)
{
    m_voxEnabled = on;
    recomputeVoxRun();
}

// ---------------------------------------------------------------------------
// onModeChanged — re-evaluate voice-family gate on TX DSP mode change.
//
// Updates m_currentMode and calls recomputeVoxRun().  If VOX is not
// enabled, the gated result stays false regardless of mode and no signal
// is emitted.  If VOX is enabled, a mode change from LSB (voice) to CWL
// (non-voice) emits voxRunRequested(false); reverse emits true.
//
// Wired by RadioModel H.1:
//   SliceModel::dspModeChanged → MoxController::onModeChanged
//
// From Thetis Project Files/Source/Console/cmaster.cs:1039-1052 [v2.10.3.13]:
//   DSPMode mode = Audio.TXDSPMode;   // re-read on each invocation
//   bool run = Audio.VOXEnabled && (mode == DSPMode.LSB || ...)
//   cmaster.SetDEXPRunVox(id, run);
// ---------------------------------------------------------------------------
void MoxController::onModeChanged(DSPMode mode)
{
    m_currentMode = mode;
    recomputeVoxRun();
}

// ---------------------------------------------------------------------------
// setTune — engage / release the TUN function.
//
// From Thetis chkTUN_CheckedChanged (console.cs:29978-30157 [v2.10.3.13]).
//
// This method ports ONLY the MoxController-side flag management and MOX
// engagement from that handler. The broader TUN effects (CW→LSB/USB mode
// swap, meter-mode lock, tune-power lookup, gen1 tone setup, ATU async,
// NetworkIO.SetUserOut, Apollo auto-tune, 2-TONE pre-stop) live in other
// tasks — see the full scope note in the header comment for setTune.
//
// Relevant Thetis lines (the "go for it" block):
//   console.cs:30081 — chkMOX.Checked = true;              [v2.10.3.13]
//   console.cs:30083 — await Task.Delay(100); // MW0LGE_21k8
//   console.cs:30090 — // MW0LGE_21k8 moved below mox
//   console.cs:30093 — _current_ptt_mode = PTTMode.MANUAL; [v2.10.3.13]
//   console.cs:30094 — _manual_mox = true;                 [v2.10.3.13]
// And the TUN-off path:
//   console.cs:30106 — chkMOX.Checked = false;             [v2.10.3.13]
//   console.cs:30142 — _manual_mox = false;                [v2.10.3.13]
//
// ORDERING NOTE vs Thetis:
// In Thetis, the flags are set AFTER chkMOX.Checked = true (line 30081)
// and AFTER an awaited 100 ms delay (line 30083). NereusSDR sets them
// BEFORE setMox(true) so that phase-signal subscribers (F.1) see a
// consistent m_manualMox=true / m_pttMode=Manual snapshot when their
// slots fire on the synchronous txAboutToBegin / hardwareFlipped(true)
// emissions inside setMox(). This ordering deviation is intentional.
// See pre-code review §3.2 for the rationale; it is safe because
// NereusSDR's setMox() is synchronous (no async/await equivalent),
// and the 100 ms settle delay that Thetis uses to interleave the flag
// set with the hardware switch does not apply here.
//
// For TUN-off: MOX is released BEFORE the flag is cleared, matching the
// spirit of Thetis (line 30106 precedes 30142) and ensuring that any
// phase-signal subscriber that fires during the TX→RX walk (e.g.
// txAboutToEnd, hardwareFlipped(false)) still observes m_manualMox=true.
// m_pttMode is NOT reset here: per Thetis, it clears indirectly via the
// TX→RX path in chkMOX_CheckedChanged2 (console.cs:29496 [v2.10.3.13])
// which sets _current_ptt_mode = PTTMode.NONE. In NereusSDR that reset
// belongs to the F.1 hardwareFlipped(false) subscriber in RadioModel.
// ---------------------------------------------------------------------------
void MoxController::setTune(bool on)
{
    if (on) {
        // ── TUN-on: set flags BEFORE engaging MOX ────────────────────────
        // (Ordering deviation from Thetis documented above.)

        // From Thetis console.cs:30093 [v2.10.3.13]:
        //   _current_ptt_mode = PTTMode.MANUAL;
        // MW0LGE_21k8 moved below mox  [original inline comment console.cs:30090]
        setPttMode(PttMode::Manual);      // idempotent; emits pttModeChanged on transition

        // From Thetis console.cs:30094 [v2.10.3.13]:
        //   _manual_mox = true;
        const bool wasManual = m_manualMox;
        m_manualMox = true;
        if (!wasManual) {
            emit manualMoxChanged(true);
        }

        // Engage MOX. setMox runs Codex P2 (safety effect) → idempotent
        // guard → state commit → emit txAboutToBegin → emit
        // hardwareFlipped(true) → start rfDelay → ... → txReady.
        // From Thetis console.cs:30081 [v2.10.3.13]: chkMOX.Checked = true;
        setMox(true);

    } else {
        // ── TUN-off: release MOX BEFORE clearing the flag ─────────────────
        // From Thetis console.cs:30106 [v2.10.3.13]: chkMOX.Checked = false;
        setMox(false);

        // From Thetis console.cs:30142 [v2.10.3.13]: _manual_mox = false;
        const bool wasManual = m_manualMox;
        m_manualMox = false;
        if (wasManual) {
            emit manualMoxChanged(false);
        }
        // NOTE: m_pttMode is intentionally NOT reset here.
        // Thetis clears _current_ptt_mode indirectly via the TX→RX path
        // in chkMOX_CheckedChanged2 (console.cs:29496 [v2.10.3.13]).
        // In NereusSDR that belongs to F.1's hardwareFlipped(false) subscriber.
    }
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
        // Phase signal ordering (Codex P1):
        //   Phase 1 of 3 — emit txAboutToBegin()
        //   Phase 2 of 3 — emit hardwareFlipped(true) BEFORE rf_delay starts
        //                  (matches HdwMOXChanged at console.cs:29569-29588
        //                   [v2.10.3.13] which fires before Thread.Sleep(rf_delay))
        //   Walk: Rx → RxToTxRfDelay → (timer fires) → Tx
        //   Phase 3 of 3 — emit txReady() in onRfDelayElapsed()
        //   moxStateChanged(true) emitted after txReady() (diagnostic signal)
        emit txAboutToBegin();                          // RX→TX phase 1 of 3
        emit hardwareFlipped(true);                     // RX→TX phase 2 of 3 — before rfDelay
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
        // Phase signal ordering (Codex P1):
        //   Phase 1 of 4 — emit txAboutToEnd()
        //   Phase 2 of 4 — emit hardwareFlipped(false) — symmetric with RX→TX:
        //                  routing clears before in-flight sample flush.
        //   Walk: Tx → TxToRxInFlight (keyUpDelayTimer, 10ms) →
        //              TxToRxFlush   (pttOutDelayTimer, 20ms) → Rx
        //   Phase 3 of 4 — emit txaFlushed() in onKeyUpDelayElapsed()
        //   Phase 4 of 4 — emit rxReady() in onPttOutElapsed()
        //   moxStateChanged(false) emitted after rxReady() (diagnostic signal)
        //
        // spaceDelay is skipped when kSpaceDelayMs == 0 (matches the
        // Thetis `if (space_mox_delay > 0)` guard).
        emit txAboutToEnd();                            // TX→RX phase 1 of 4
        emit hardwareFlipped(false);                    // TX→RX phase 2 of 4 — before flush
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
// runMoxSafetyEffects — Codex P2 hook; intentionally empty in 3M-1a.
//
// Called on EVERY setMox() invocation, including idempotent ones, so
// that safety effects cannot be skipped by a repeated call.
//
// 3M-1a: intentionally empty. The plan's F.1 task does not fill this
// body — it wires Alex routing, ATT-on-TX, and MOX wire bits to the
// hardwareFlipped(bool isTx) signal in RadioModel instead (subscriber
// model, fires only on real transitions).
//
// This hook stays available for any future Codex-P2-required-on-every-
// call effects (e.g., re-drop MOX on PA fault, per PR #139 pattern).
// No 3M-1a effects need this; reassess in 3M-1b/3M-3.
// ---------------------------------------------------------------------------
void MoxController::runMoxSafetyEffects(bool /*newMox*/)
{
    // intentionally empty in 3M-1a — see comment above
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
// Phase signals (Codex P1):
//   Advance to terminal Tx state
//   RX→TX phase 3 of 3 — emit txReady() — TX I/Q stream + audio MOX from this point
//   emit moxStateChanged(true) (diagnostic / integration signal)
void MoxController::onRfDelayElapsed()
{
    // TODO [3M-1a F.1]: AudioMOXChanged(true) + WDSP TX channel on here.
    advanceState(MoxState::Tx);
    emit txReady();                                     // RX→TX phase 3 of 3
    emit moxStateChanged(true);                         // diagnostic signal
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
// Phase signals (Codex P1):
//   TX→RX phase 3 of 4 — emit txaFlushed() — in-flight samples cleared; TX channel may stop
//   Advance to TxToRxFlush state, then start ptt_out_delay timer
void MoxController::onKeyUpDelayElapsed()
{
    // TODO [3M-1a F.1]: UpdateDDCs + UpdateAAudioMixerStates + AudioMOXChanged(false)
    //                   + HdwMOXChanged(false) here.
    emit txaFlushed();                                  // TX→RX phase 3 of 4
    advanceState(MoxState::TxToRxFlush);
    m_pttOutDelayTimer.start();
}

// onPttOutElapsed — fires after ptt_out_delay (20ms) on TX→RX path.
//
// From Thetis console.cs:29627-29628 [v2.10.3.13]:
//   if (ptt_out_delay > 0)
//       Thread.Sleep(ptt_out_delay);  //wcp:  added 2018-12-24, time for HW to switch
//
// Phase signals (Codex P1):
//   Advance to terminal Rx state
//   TX→RX phase 4 of 4 — emit rxReady() — RX channel active from this point
//   emit moxStateChanged(false) (diagnostic / integration signal)
void MoxController::onPttOutElapsed()
{
    // TODO [3M-1a F.1]: WDSP.SetChannelState(WDSP.id(0, 0), 1, 0) (RX1 on) here.
    advanceState(MoxState::Rx);
    emit rxReady();                                     // TX→RX phase 4 of 4
    emit moxStateChanged(false);                        // diagnostic signal
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

// ===========================================================================
// H.2 — VOX threshold with mic-boost-aware scaling
//
// Porting from Thetis Project Files/Source/Console/cmaster.cs:1054-1059
// [v2.10.3.13] — CMSetTXAVoxThresh — original C# logic:
//
//   public static void CMSetTXAVoxThresh(int id, double thresh)
//   {
//       //double thresh = (double)Audio.VOXThreshold;
//       if (Audio.console.MicBoost) thresh *= (double)Audio.VOXGain;
//       cmaster.SetDEXPAttackThreshold(id, thresh);
//   }
//
// The caller (setup.cs:18908-18912 [v2.10.3.13]) converts dB to linear
// amplitude before calling CMSetTXAVoxThresh:
//
//   private void udDEXPThreshold_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.CMSetTXAVoxThresh(0, Math.Pow(10.0, (double)udDEXPThreshold.Value / 20.0));
//       console.VOXSens = (int)udDEXPThreshold.Value;
//   }
//
// NereusSDR folds both steps into MoxController so the RadioModel wiring
// is a single TransmitModel::voxThresholdDbChanged → setVoxThreshold(int).
// ===========================================================================

// ---------------------------------------------------------------------------
// computeScaledThreshold — compute linear WDSP attack threshold.
//
// Two-step Thetis formula (no clamping; Thetis applies none):
//   1. dB → linear (setup.cs:18911 [v2.10.3.13]):
//        thresh = pow(10.0, dB / 20.0)
//   2. Mic-boost scaling (cmaster.cs:1057 [v2.10.3.13]):
//        if (MicBoost) thresh *= VOXGain
// ---------------------------------------------------------------------------
double MoxController::computeScaledThreshold() const noexcept
{
    // From Thetis Project Files/Source/Console/setup.cs:18911 [v2.10.3.13]
    // Math.Pow(10.0, (double)udDEXPThreshold.Value / 20.0)
    double thresh = std::pow(10.0, static_cast<double>(m_voxThresholdDb) / 20.0);

    // From Thetis Project Files/Source/Console/cmaster.cs:1057 [v2.10.3.13]
    // if (Audio.console.MicBoost) thresh *= (double)Audio.VOXGain;
    if (m_micBoost) {
        thresh *= static_cast<double>(m_voxGainScalar);
    }

    return thresh;
}

// ---------------------------------------------------------------------------
// recomputeVoxThreshold — emit voxThresholdRequested if computed value changed.
//
// Idempotent on the EMITTED double: qFuzzyCompare prevents spurious re-emits
// from floating-point noise after equivalent input changes.  std::isnan on
// m_lastVoxThresholdEmitted forces the very first call to always emit so
// WDSP is primed at startup.
// ---------------------------------------------------------------------------
void MoxController::recomputeVoxThreshold()
{
    const double thresh = computeScaledThreshold();

    // NAN sentinel: first call always emits (primes WDSP regardless of value).
    if (!std::isnan(m_lastVoxThresholdEmitted)
        && qFuzzyCompare(thresh, m_lastVoxThresholdEmitted)) {
        return;  // idempotent on emitted double; no spurious emit
    }

    m_lastVoxThresholdEmitted = thresh;
    emit voxThresholdRequested(thresh);
}

// ---------------------------------------------------------------------------
// setVoxThreshold — set dB and recompute the scaled threshold.
//
// From Thetis TransmitModel::voxThresholdDb (ptbVOX slider value).
// Wired by RadioModel H.2: voxThresholdDbChanged → setVoxThreshold.
// ---------------------------------------------------------------------------
void MoxController::setVoxThreshold(int dB)
{
    m_voxThresholdDb = dB;
    recomputeVoxThreshold();
}

// ---------------------------------------------------------------------------
// onMicBoostChanged — update mic-boost flag and recompute threshold.
//
// From Thetis chk20dbMicBoost_CheckedChanged (setup.cs:7684-7687 [v2.10.3.13]):
//   re-runs udVOXGain_ValueChanged whenever mic-boost changes, which calls
//   CMSetTXAVoxThresh with the current threshold value.
// Wired by RadioModel H.2: TransmitModel::micBoostChanged → onMicBoostChanged.
// ---------------------------------------------------------------------------
void MoxController::onMicBoostChanged(bool boost)
{
    m_micBoost = boost;
    recomputeVoxThreshold();
}

// ---------------------------------------------------------------------------
// setVoxGainScalar — update the mic-boost gain scalar and recompute threshold.
//
// From Thetis audio.cs:194-202 [v2.10.3.13]:
//   private static float vox_gain = 1.0f;
// Wired by RadioModel H.2: TransmitModel::voxGainScalarChanged → setVoxGainScalar.
// ---------------------------------------------------------------------------
void MoxController::setVoxGainScalar(float scalar)
{
    m_voxGainScalar = scalar;
    recomputeVoxThreshold();
}

} // namespace NereusSDR
