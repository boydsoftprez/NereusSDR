// =================================================================
// src/core/MoxController.h  (NereusSDR)
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
//                 Timer constants derived from console.cs:19659-19698
//                 and console.cs:18494-18502 [v2.10.3.13].
//                 State-machine walk through transient states replaces
//                 the B.2 direct-to-terminal jump in setMox().
//   2026-04-25 — Phase 3M-1a Task B.4 — 6 phase signals added.
//                 Codex P1: subscribers attach to named phase boundary
//                 signals, not to individual low-level setters.
//                 hardwareFlipped(bool isTx) chosen (Option B) so a
//                 single subscriber slot handles both RX→TX and TX→RX
//                 hardware routing transitions.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis state-machine
// derived values are cited inline below.

#pragma once

#include <QObject>
#include <QTimer>
#include "core/PttMode.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// MoxState — the 7-state TX/RX transition machine.
//
// State machine derived from chkMOX_CheckedChanged2
// (console.cs:29311-29678 [v2.10.3.13]).
//
// Transient states are visited only during timer-driven transitions.
// B.3 wires QTimer chains to make them dwell for the correct interval.
//
// Naming note: TxToRxInFlight is used for what Thetis calls mox_delay
// (SSB/FM) or key_up_delay (CW) — both are 10 ms by default. The name
// is intentionally neutral so that 3M-2 can branch on CW vs non-CW from
// this state without changing the enum.
// ---------------------------------------------------------------------------
enum class MoxState {
    Rx,                // idle, receiver active
    RxToTxRfDelay,     // waiting for rf_delay (30 ms default) before TX channel on
    RxToTxMoxDelay,    // reserved: mox_delay settle (non-CW RX→TX); not used in 3M-1a
    Tx,                // transmitting
    TxToRxInFlight,    // mox_delay (SSB/FM 10ms) or key_up_delay (CW 10ms) in-flight clear
    TxToRxBreakIn,     // reserved: break-in settle; not started in 3M-1a (3M-2 CW QSK)
    TxToRxFlush,       // waiting for ptt_out_delay (20 ms) before RX channels on
};

// ---------------------------------------------------------------------------
// MoxController — drives the MOX/PTT state machine.
//
// Lives on the main thread; will be owned by RadioModel (Task G.1).
//
// Codex P2 (PR #139): safety effects in setMox() execute BEFORE the
// idempotent guard so that a repeated setMox(true) call cannot skip
// them. The body of runMoxSafetyEffects() is empty in B.2/B.3; Task F.1
// wires AlexController routing, ATT-on-TX, and the MOX wire bit.
//
// Timer behaviour:
//   RX→TX path: Rx → RxToTxRfDelay (30ms) → Tx
//   TX→RX path: Tx → TxToRxInFlight (10ms, mox_delay) → TxToRxFlush
//               (20ms, ptt_out_delay) → Rx
//   spaceDelay (0ms default): m_spaceDelayTimer declared but skipped
//     when kSpaceDelayMs == 0, matching Thetis
//     `if (space_mox_delay > 0) Thread.Sleep(...)` pattern.
//   breakInDelay (300ms): declared for 3M-2 CW QSK; NOT started in
//     any 3M-1a path.
//
// moxStateChanged fires at end of walk (TX fully engaged or fully
// released), not at setMox() entry, so subscribers see a definitive
// "MOX is on/off" rather than "MOX command initiated".
// ---------------------------------------------------------------------------
class MoxController : public QObject {
    Q_OBJECT

public:
    explicit MoxController(QObject* parent = nullptr);
    ~MoxController() override;

    // ── Timer constants (from Thetis console.cs:19659-19698 [v2.10.3.13]) ──
    //
    // From Thetis console.cs:19687 — private int rf_delay = 30 [v2.10.3.13]
    static constexpr int kRfDelayMs      = 30;
    // From Thetis console.cs:19659 — private int mox_delay = 10 [v2.10.3.13]
    static constexpr int kMoxDelayMs     = 10;
    // From Thetis console.cs:19669 — private int space_mox_delay = 0 [v2.10.3.13]
    static constexpr int kSpaceDelayMs   = 0;
    // From Thetis console.cs:19677 — private int key_up_delay = 10 [v2.10.3.13]
    static constexpr int kKeyUpDelayMs   = 10;
    // From Thetis console.cs:19694 — private int ptt_out_delay = 20 [v2.10.3.13]
    static constexpr int kPttOutDelayMs  = 20;
    // From Thetis console.cs:18494 — private double break_in_delay = 300 [v2.10.3.13]
    // 3M-2 CW QSK; not used in any 3M-1a path.
    static constexpr int kBreakInDelayMs = 300;

    // ── Getters ──────────────────────────────────────────────────────────────
    bool     isMox()    const noexcept { return m_mox; }
    MoxState state()    const noexcept { return m_state; }
    PttMode  pttMode()  const noexcept { return m_pttMode; }

    // ── Setter ───────────────────────────────────────────────────────────────
    // setPttMode: idempotent; emits pttModeChanged on actual transition.
    void setPttMode(PttMode mode);

    // ── Test seam ─────────────────────────────────────────────────────────────
    // setTimerIntervals: override the default Thetis timer durations.
    //
    // FOR TESTING ONLY. Production code must use the kXxxMs defaults.
    // Pass all-zeros for synchronous-equivalent behavior in unit tests:
    //   ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
    // so QCoreApplication::processEvents() drives the entire walk
    // without waiting for wall-clock time.
    void setTimerIntervals(int rfMs, int moxMs, int spaceMs,
                           int keyUpMs, int pttOutMs, int breakInMs);

public slots:
    // setMox: Codex P2-ordered slot.
    //
    // Order (must not be reordered):
    //   1. runMoxSafetyEffects(on)       — safety effects fire FIRST
    //   2. idempotent guard              — skip state advance if no change
    //   3. m_mox = on                   — commit new state
    //   4. start timer-driven walk      — transient states then terminal
    //   5. emit moxStateChanged(on)      — fires at END of walk (Codex P1)
    //
    // Task F.1 fills runMoxSafetyEffects with Alex routing, ATT-on-TX,
    // and the MOX wire bit. DO NOT insert an early-return guard above
    // the runMoxSafetyEffects call — that would regress Codex P2.
    void setMox(bool on);

signals:
    // ── Phase signals (Codex P1) ──────────────────────────────────────────────
    //
    // Subscribers attach HERE, not to individual low-level setters.
    // F.1 wires Alex routing, ATT-on-TX, and MOX wire bits to these signals.
    // moxStateChanged / stateChanged are retained as diagnostic signals only;
    // production subscribers must use the phase signals below.
    //
    // Phase signals derived from chkMOX_CheckedChanged2 RX→TX/TX→RX ordering
    // (console.cs:29311-29678 [v2.10.3.13]).
    // See pre-code review §1.4 for emit point rationale.

    // RX→TX phase signals (in order):
    //   txAboutToBegin  — entry to RX→TX walk; display overlay, ATT prep.
    //   hardwareFlipped — hardware routing committed; fired BEFORE rfDelay
    //                     so Alex routing + ATT-on-TX assertions precede
    //                     the 30 ms TX settle (matches Thetis HdwMOXChanged
    //                     at console.cs:29569-29588 [v2.10.3.13], which occurs
    //                     BEFORE Thread.Sleep(rf_delay)).
    //   txReady         — TX walk complete; TX I/Q stream + audio MOX on.
    //
    // TX→RX phase signals (in order):
    //   txAboutToEnd    — entry to TX→RX walk; teardown begins.
    //   hardwareFlipped — hardware routing released (isTx=false); fired right
    //                     after txAboutToEnd so routing clears before in-flight
    //                     sample flush (symmetric with RX→TX position).
    //   txaFlushed      — after mox_delay / key_up_delay (in-flight samples
    //                     cleared); TX channel may now be torn down.
    //   rxReady         — TX→RX walk complete; RX channels active.
    //
    // hardwareFlipped(bool isTx):
    //   true  — RX→TX: assert Alex routing, ATT-on-TX, MOX wire bit.
    //   false — TX→RX: release Alex routing, ATT-on-TX, clear MOX wire bit.
    //   One subscriber slot handles both directions via the bool payload.

    void txAboutToBegin();          // RX→TX: step 3 — safety-relevant prep
    void hardwareFlipped(bool isTx);// RX→TX step 5 / TX→RX step 4 — hardware routing committed or released
    void txReady();                 // RX→TX: step 9 — TX channel runs from this point
    void txAboutToEnd();            // TX→RX: step 3 — teardown begins
    void txaFlushed();              // TX→RX: step 7 — in-flight samples cleared, TX channel may stop
    void rxReady();                 // TX→RX: step 12 — RX channel runs from this point

    // ── Boundary signals (diagnostic / integration — keep these) ─────────────
    // moxStateChanged: emitted exactly once per real transition, at the END
    // of the timer walk (TX fully engaged or fully released).
    void moxStateChanged(bool on);

    // pttModeChanged: emitted when m_pttMode transitions.
    void pttModeChanged(PttMode mode);

    // stateChanged: fires on every m_state transition; useful for tests
    // and debugging.
    void stateChanged(MoxState newState);

protected:
    // runMoxSafetyEffects is protected virtual so test subclasses can
    // override it to verify the Codex P2 ordering invariant without
    // needing a full RadioModel or RadioConnection.
    //
    // F.1 wires: AlexController::applyAntennaForBand,
    //            StepAttenuatorController TX-path, RadioConnection MOX bit.
    //
    // TODO [3M-1a F.1]: fill this body with actual safety effects.
    virtual void runMoxSafetyEffects(bool newMox);

private slots:
    // Timer slots — each fires when the corresponding QTimer elapses and
    // drives the state machine to the next state.
    void onRfDelayElapsed();
    void onMoxDelayElapsed();
    void onSpaceDelayElapsed();
    void onKeyUpDelayElapsed();
    void onPttOutElapsed();
    void onBreakInDelayElapsed(); // declared for 3M-2 CW QSK; not started in 3M-1a

private:
    // advanceState: sets m_state and emits stateChanged.
    void advanceState(MoxState newState);

    // stopAllTimers: cancel any in-flight timers (safety guard for
    // rapid setMox(false)/setMox(true) toggles or test teardown).
    void stopAllTimers();

    // ── Fields ───────────────────────────────────────────────────────────────
    bool     m_mox{false};               // single source of truth for MOX
    MoxState m_state{MoxState::Rx};      // current state-machine position
    PttMode  m_pttMode{PttMode::None};   // current PTT mode

    // ── QTimer chains (B.3) ──────────────────────────────────────────────────
    // All initialized single-shot in the constructor with kXxxMs intervals.
    // setTimerIntervals() overrides intervals for test use.
    //
    // From Thetis console.cs:29592-29628 [v2.10.3.13] — Thread.Sleep() calls
    // in chkMOX_CheckedChanged2 translated to Qt single-shot timers.
    // console.cs:29603: Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE
    QTimer m_rfDelayTimer;      // 30 ms — RX→TX: between hardware flip and TX-channel-on (non-CW)
    QTimer m_moxDelayTimer;     // 10 ms — reserved for future RX→TX use; not started in 3M-1a
    QTimer m_spaceDelayTimer;   // 0 ms  — TX→RX: initial wait before WDSP TX off; skipped when 0 // from PSDR MW0LGE
    QTimer m_keyUpDelayTimer;   // 10 ms — TX→RX: mox_delay (SSB) or key_up_delay (CW); drives TxToRxInFlight
    QTimer m_pttOutDelayTimer;  // 20 ms — TX→RX: HW settle before WDSP RX on; drives TxToRxFlush
    QTimer m_breakInDelayTimer; // 300 ms — 3M-2 CW QSK; NOT started from any B.3 logic
};

} // namespace NereusSDR

// Qt metatype registration — required so MoxState can be carried by
// QVariant / QSignalSpy::value<MoxState>() without silently returning
// a zero-initialised value on Qt6 builds that haven't called
// qRegisterMetaType<>().  Matches the pattern in WdspTypes.h:298-305.
#include <QMetaType>
Q_DECLARE_METATYPE(NereusSDR::MoxState)
