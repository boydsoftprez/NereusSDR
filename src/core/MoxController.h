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
//   2026-04-25 — Phase 3M-1a Task B.5 — setTune(bool) slot added.
//                 Drives MOX through the existing state machine and
//                 manages the m_manualMox / m_pttMode = Manual flags.
//                 Scope: MoxController-side TUN flag management only.
//                 Fuller chkTUN_CheckedChanged behaviour (CW→LSB/USB swap,
//                 meter-mode lock, tune-power lookup, gen1 tone, ATU async,
//                 NetworkIO.SetUserOut, Apollo auto-tune, 2-TONE pre-stop)
//                 is split across Tasks C.3 / G.3 / G.4.
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
    bool     isMox()      const noexcept { return m_mox; }
    MoxState state()      const noexcept { return m_state; }
    PttMode  pttMode()    const noexcept { return m_pttMode; }
    // isManualMox: true while MOX is engaged via the TUN button.
    //
    // Mirrors Thetis _manual_mox (console.cs:240 [v2.10.3.13]):
    //   "True if the MOX button was clicked on (not PTT)"
    // In NereusSDR, TUN goes through setTune() which sets this flag.
    // setMox() does NOT touch this flag (Thetis sets it via chkMOX_
    // CheckedChanged2 only; NereusSDR narrows that to the TUN path).
    // F.1 subscribers wanting to distinguish a TUN-triggered MOX from a
    // raw setMox(true) call should read this getter inside their
    // hardwareFlipped(bool isTx) slot. External code must not set this
    // directly — call setTune() instead.
    bool     isManualMox() const noexcept { return m_manualMox; }

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
    // setTune: engage / release the TUN function.
    //
    // Drives MOX through the full state machine and manages the
    // m_manualMox / m_pttMode = Manual flags per the Thetis TUN-on
    // "go for it" block and TUN-off path in chkTUN_CheckedChanged
    // (console.cs:29978-30157 [v2.10.3.13]).
    //
    // Specifically ports the flag assignments at:
    //   console.cs:30093 — _current_ptt_mode = PTTMode.MANUAL  [v2.10.3.13]
    //   console.cs:30094 — _manual_mox = true                  [v2.10.3.13]
    //   console.cs:30142 — _manual_mox = false                 [v2.10.3.13]
    //
    // Note: Thetis sets flags AFTER chkMOX.Checked = true (line 30081)
    // and AFTER await Task.Delay(100) (line 30083). NereusSDR sets them
    // BEFORE setMox(true) so that phase-signal subscribers (F.1) see a
    // consistent m_manualMox=true / m_pttMode=Manual snapshot when their
    // slots fire. This ordering deviation is intentional and documented.
    // See pre-code review §3.2 for the rationale.
    //
    // Scope (3M-1a B.5): this slot owns the MoxController-side flag
    // management and MOX-state engagement only. The fuller Thetis
    // chkTUN_CheckedChanged behaviour — CW→LSB/USB swap, meter-mode lock,
    // tune-power lookup, gen1 tone setup, ATU async, NetworkIO.SetUserOut,
    // Apollo auto-tune, 2-TONE pre-stop — is split across:
    //   - TxChannel::setTuneTone (Task C.3) for gen1 tone
    //   - TransmitModel per-band tune-power (Task G.3)
    //   - RadioModel TUNE function port (Task G.4) for the rest
    // Those tasks call setTune() after doing their prep, or subscribe to
    // MoxController phase signals for ordered hardware-flip side-effects.
    //
    // F.1 contract: m_pttMode is intentionally NOT cleared on TUN-off.
    // The F.1 RadioModel subscriber is responsible for resetting it via
    // the hardwareFlipped(false) signal path (matches Thetis behaviour
    // where chkMOX_CheckedChanged2 sets _current_ptt_mode = NONE in its
    // TX→RX branch at console.cs:29539 [v2.10.3.13]). The full rationale
    // is in MoxController.cpp in the setTune(false) body comment.
    void setTune(bool on);

    // setMox: Codex P2-ordered slot.
    //
    // Order (must not be reordered):
    //   1. runMoxSafetyEffects(on)       — safety effects fire FIRST
    //   2. idempotent guard              — skip state advance if no change
    //   3. m_mox = on                   — commit new state
    //   4. start timer-driven walk      — transient states then terminal
    //   5. emit moxStateChanged(on)      — fires at END of walk (Codex P1)
    //
    // runMoxSafetyEffects is the Codex P2 hook — it fires on every
    // setMox() call, including idempotent ones, BEFORE the m_mox==on guard.
    // 3M-1a has no Codex-P2-required-on-every-call effects identified, so
    // the body stays empty in this phase. F.1 wires Alex routing, ATT-on-TX,
    // and MOX/T-R wire bits via hardwareFlipped(bool isTx) subscribers in
    // RadioModel — NOT by filling runMoxSafetyEffects.
    // DO NOT insert an early-return guard above the runMoxSafetyEffects
    // call — that would regress Codex P2.
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

    void txAboutToBegin();          // RX→TX phase 1 of 3 — synchronous; safety-relevant prep
    void hardwareFlipped(bool isTx);// Both directions; synchronous; subscribers wire Alex/ATT/MOX-bit
    void txReady();                 // RX→TX phase 3 of 3 — fires after rfDelay timer
    void txAboutToEnd();            // TX→RX phase 1 of 4 — synchronous; teardown entry
    void txaFlushed();              // TX→RX phase 3 of 4 — fires after keyUpDelay; in-flight samples cleared
    void rxReady();                 // TX→RX phase 4 of 4 — fires after pttOutDelay

    // ── TUN state signal (diagnostic) ────────────────────────────────────────
    // manualMoxChanged is NOT a Codex P1 phase signal. F.1 subscribers should
    // continue to wire to the 6 phase signals above; manualMoxChanged is a
    // diagnostic-level emit for code that needs to react to TUN flag changes
    // independently of the MOX state walk (e.g. UI button highlight).
    //
    // Emitted when m_manualMox transitions (set/cleared only by setTune()).
    // Subscribers who need to distinguish a TUN-originated MOX from a
    // direct setMox() call can attach here. Phase-signal subscribers (F.1)
    // typically use hardwareFlipped(bool isTx) instead — its payload is
    // sufficient for routing decisions.
    void manualMoxChanged(bool isManual);

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
    // 3M-1a: intentionally empty. The plan's F.1 task does not fill this
    // body — it wires Alex routing, ATT-on-TX, and MOX wire bits to the
    // hardwareFlipped(bool isTx) signal in RadioModel instead.
    //
    // This hook stays available for any future Codex-P2-required-on-every-
    // call effects (e.g., re-drop MOX on PA fault, per PR #139 pattern).
    // No 3M-1a effects need this; reassess in 3M-1b/3M-3.
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
    // m_manualMox: true while MOX is engaged via the TUN button.
    // From Thetis console.cs:240 [v2.10.3.13]:
    //   "private bool _manual_mox; // True if the MOX button was clicked on (not PTT)"
    // Set/cleared only by setTune() — never set by setMox() directly.
    bool     m_manualMox{false};

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
