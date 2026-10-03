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
//   2026-04-28 — Phase 3M-1b Task K.2 — setMoxCheck(MoxCheckFn) implemented.
//                 setMox(true) now checks m_moxCheck() BEFORE Codex P2 safety
//                 effects; on rejection emits moxRejected(reason) and returns.
//   2026-04-28 — Phase 3M-1b Task H.3 — recomputeVoxHangTime(),
//                 recomputeAntiVoxGain(), setVoxHangTime(int ms),
//                 setAntiVoxGain(int dB) implemented.  Originally also added
//                 setAntiVoxSourceVax(bool); removed by 3M-3a-iv post-bench
//                 refactor (see 2026-05-07 entry below).
//                 Ports the Thetis formulae:
//                   ms→seconds (setup.cs:18899 [v2.10.3.13]):
//                     cmaster.SetDEXPHoldTime(0, Value / 1000.0)
//                   dB→linear/voltage (setup.cs:18989 [v2.10.3.13]):
//                     cmaster.SetAntiVOXGain(0, Math.Pow(10.0, dB / 20.0))
//                   CMSetAntiVoxSourceWhat useVAC=false
//                   (cmaster.cs:937-942 [v2.10.3.13]):
//                     all RX slots (RX1, RX1S, RX2) get source=1.
//                 useVax=true rejected with qCWarning (deferred to 3M-3a).
//   2026-04-28 — Phase 3M-1c Tasks C.2 / C.3 / C.4 — multicast Pre/Post MOX
//                 state-change signals wired:
//                   moxChanging(rx, oldMox, newMox) Pre — emitted in setMox
//                     after the idempotent guard but BEFORE m_mox commit, so
//                     subscribers see the OLD value via isMox().
//                     Ports console.cs:29324 [v2.10.3.13]
//                     (MoxPreChangeHandlers, // MW0LGE_21k8).
//                   moxChanged(rx, oldMox, newMox) Post — emitted after the
//                     timer walk completes (onRfDelayElapsed / onPttOutElapsed)
//                     parallel to the existing moxStateChanged(bool) emit.
//                     Ports console.cs:29677 [v2.10.3.13]
//                     (MoxChangeHandlers, // MW0LGE_21a).
//                   setRx2Enabled(bool) / setVfobTx(bool) idempotent setters
//                     added so RadioModel can keep activeRxForTx() current.
//                   activeRxForTx() = (m_rx2Enabled && m_vfobTx) ? 2 : 1
//                     matches the Thetis dispatch expression verbatim.
//   2026-05-07 — Phase 3M-3a-iv post-bench refactor (Option A): removed
//                 setAntiVoxSourceVax(bool) implementation, antiVoxSourceWhatRequested
//                 emit, and m_antiVoxSourceVax / m_antiVoxSourceVaxInitialized state.
//                 NereusSDR-architectural divergence from Thetis chkAntiVoxSource
//                 (RX vs VAC at cmaster.cs:912-943 [v2.10.3.13]); see commit
//                 message for rationale.  J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 : G-05 (JJ's ruling 2026-09-28): an operator's release
//                 waits for the transmit I/Q send ring to drain after the TX
//                 channel's drain and before mox_delay, for at most the
//                 ring's own length; TX inhibit, the PA trip, receive-only
//                 and the Core's stops never wait. NereusSDR-original.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 : RADE end-of-over callsigns: an operator's release sends
//                 the end-of-over tail before the TX→RX walk's phase 1
//                 (setEndOfOverTail, beginTxToRxTeardown split out of
//                 setMox). NereusSDR-original. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 : Task 33 (R-IOS-03): the TX→RX walk follows Thetis's
//                 unkey (console.cs:29651-29685 [v2.10.3.15]): txDrainRequested
//                 first, a bounded wait for the drain, mox_delay, then
//                 txaFlushed and hardwareFlipped(false). StopAllTx's
//                 _stop_all_tx latch (latchStopAllTx) and _manual_mox clear
//                 (clearManualMox). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 7 fix wave: TX
//                 inhibit and the PA trip gate every keying source and
//                 unkey (setTxInhibited, setPaTripped; console.cs:25470,
//                 15341-15363, 29364-29371 [v2.10.3.15]). A TX-interlock
//                 refusal emits moxRejected (M2); a held source's repeat
//                 refusal is quiet, one message per press (M3). A VOX
//                 level is dropped when VOX stops running (M5), and a TCI
//                 release that falls back to another source runs the MOX
//                 pre-check first (M6, R-R3-36). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Task 34 merge with the gaps lane: StopAllTx's latch is read
//                 in pollPtt's receive branch (console.cs:25479-25492
//                 [v2.10.3.15]); clearManualMox clears the manual key too.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 34 (R-IOS-02, R-IOS-13, rulings 8.5,
//                 8.8, 8.13): the keying gate at the press edge (tryPollKey)
//                 and in setMox; setMox(bool, KeyerIdentity); releases by
//                 keyer; admitStationKey; onTakeFinished; every refusal
//                 also as a TxRefusal (moxRefused). NereusSDR-original.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 16: receive only
//                 (setRxOnly, Thetis _rx_only; console.cs:15312-15334,
//                 25470, 29378-29382 [v2.10.3.15]) refuses every key with
//                 its reason and unkeys. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-25 - Task 16 fix wave (M2): transmitBlockReason (the words
//                 setMox refuses with) and transmitBlockChanged. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: M1 refusalBeforeTheGate, the
//               checks that refuse a key are asked before the keying
//               gate; M10 KeyerIdentity::session. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26: Transmit group fix wave 2, Important 2: a refused TUNE or
//               two-tone takes nothing (admitKey asks TX inhibit, the PA
//               trip, receive only and the interlock before the gate; a
//               take whose key never starts is released). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13): the
//               radio's mic press edge while another device's key is on asks
//               the gate (a take); holdOffHeldMic; programKeyRefusal. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix wave, I1 (ruling 8.9): the
//               mic's press edge asks the gate whenever another device
//               holds transmit (setOtherDeviceHolds), whatever its key
//               (TUNE, two-tone, a tuner autotune, VOX). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix round 2 (R-IOS-02, R-IOS-03,
//               R-IOS-13): anyPttSourceHeld and pttSourcesReleased (the
//               amplifier's owed switch waits for every PTT source); the
//               radio's mic keys after a take only while the press that
//               took is still down (a second press during the take is
//               refused and keys nothing later). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix round 3 (R-IOS-02, R-IOS-03,
//               R-IOS-13): a CAT or TCI release that never keyed, and a
//               refused CAT or TCI level, report pttSourcesReleased. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: HL2 port part 2: setTxInhibited takes the reason the
//               inhibit is shown with ("I/O Board: Fault Code N" for the
//               HL2 I/O board fault, mi0bot console.cs:25876-25885
//               [@c26a8a4]); the refusal and transmitBlockReason carry it.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: slice control plan Task 7: the noTransmitSlice refusal
//               from the mox check. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: TX safety: setRadioLinkDown, the lost radio link's gate
//               (Thetis console.cs:27488-27493 [v2.10.3.15]). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: TGXL tune lane (JJ's ruling): a tunerPress key's take
//               (ruling 8.9) ends with tunerTakeFinished; admitKey records
//               a take (lastAdmitTook). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis state-machine
// derived values are cited inline below.

#include "core/MoxController.h"

#include <QSignalBlocker>
#include "core/LogCategories.h"

#include <algorithm>

#include <cmath>

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
    , m_txDrainTimeoutTimer(this)
{
    // All timers are single-shot — each fires once then stops.
    m_rfDelayTimer.setSingleShot(true);
    m_moxDelayTimer.setSingleShot(true);
    m_spaceDelayTimer.setSingleShot(true);
    m_keyUpDelayTimer.setSingleShot(true);
    m_pttOutDelayTimer.setSingleShot(true);
    m_breakInDelayTimer.setSingleShot(true);
    m_txDrainTimeoutTimer.setSingleShot(true);
    m_endOfOverTailTimer.setSingleShot(true);
    // G-05: the poll repeats until the ring drains; the bound is one-shot,
    // its interval set from the ring's length when a wait starts.
    m_sendRingPollTimer.setSingleShot(false);
    m_sendRingPollTimer.setInterval(kSendRingPollMs);
    m_sendRingPollTimer.setTimerType(Qt::PreciseTimer);
    m_sendRingCeilingTimer.setSingleShot(true);
    m_sendRingCeilingTimer.setTimerType(Qt::PreciseTimer);

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
    // Task 33: WDSP SetChannelState's 100 x Sleep(1) drain bound.
    m_txDrainTimeoutTimer.setInterval(kTxDrainTimeoutMs);
    m_endOfOverTailTimer.setInterval(kEndOfOverTailMaxMs);
    m_txDrainTimeoutTimer.setTimerType(Qt::PreciseTimer);

    // Wire timer timeouts to their advancement slots.
    connect(&m_rfDelayTimer,      &QTimer::timeout, this, &MoxController::onRfDelayElapsed);
    connect(&m_moxDelayTimer,     &QTimer::timeout, this, &MoxController::onMoxDelayElapsed);
    connect(&m_spaceDelayTimer,   &QTimer::timeout, this, &MoxController::onSpaceDelayElapsed);
    connect(&m_keyUpDelayTimer,   &QTimer::timeout, this, &MoxController::onKeyUpDelayElapsed);
    connect(&m_pttOutDelayTimer,  &QTimer::timeout, this, &MoxController::onPttOutElapsed);
    connect(&m_breakInDelayTimer, &QTimer::timeout, this, &MoxController::onBreakInDelayElapsed);
    connect(&m_txDrainTimeoutTimer, &QTimer::timeout, this, &MoxController::onTxDrainTimedOut);
    connect(&m_endOfOverTailTimer, &QTimer::timeout, this, &MoxController::onEndOfOverTailTimedOut);
    connect(&m_sendRingPollTimer, &QTimer::timeout, this, &MoxController::onSendRingPoll);
    connect(&m_sendRingCeilingTimer, &QTimer::timeout, this, &MoxController::onSendRingCeiling);
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

void MoxController::setTxDrainTimeoutMsForTest(int ms)
{
    m_txDrainTimeoutTimer.setInterval(ms);
}

void MoxController::setEndOfOverTailMaxMsForTest(int ms)
{
    m_endOfOverTailTimer.setInterval(ms);
}

// ---------------------------------------------------------------------------
// RADE end-of-over callsigns: the end-of-over tail (NereusSDR-original).
//
// FreeDV sends its end-of-over frame after the operator lets go of PTT and
// before it drops PTT: the release sets endingTx, the TX thread queues the
// EOO and 200 ms of silence once the remaining audio is through, and PTT
// drops once that has been queued and played out.
//   From freedv-gui src/ongui.cpp:1479-1523 [@a4ae053] (the wait on
//   g_eoo_enqueued, then on the output FIFO, before PTT off) and
//   src/pipeline/TxRxThread.cpp:808-847 [@a4ae053] (restartTxVocoder once
//   per ending, then the step's queued output).
// Here the release commits MOX off at once (every keying rule sees the
// release), keeps the hardware keyed and the TX channel running while the
// tail goes out, then walks on exactly as an unkey without a tail.
// ---------------------------------------------------------------------------
void MoxController::setEndOfOverTail(EndOfOverTailFn fn)
{
    m_endOfOverTail = std::move(fn);
}

void MoxController::onEndOfOverTailDone()
{
    if (!m_waitingForEndOfOverTail) {
        return;   // a tail this walk is not waiting for
    }
    finishEndOfOverTail();
}

void MoxController::abortEndOfOverTail()
{
    if (!m_waitingForEndOfOverTail) {
        return;
    }
    qCInfo(lcDsp) << "MoxController: transmit stopped; the end-of-over tail is not sent";
    finishEndOfOverTail();
}

void MoxController::onEndOfOverTailTimedOut()
{
    if (!m_waitingForEndOfOverTail) {
        return;
    }
    qCInfo(lcDsp) << "MoxController: the end-of-over tail did not finish within"
                  << m_endOfOverTailTimer.interval()
                  << "ms; releasing the radio without the rest of it";
    finishEndOfOverTail();
}

void MoxController::finishEndOfOverTail()
{
    m_waitingForEndOfOverTail = false;
    m_endOfOverTailTimer.stop();
    emit endOfOverTailChanged(false);
    beginTxToRxTeardown();
}

// ---------------------------------------------------------------------------
// Task 33: the TX drain wait and the StopAllTx latch.
// ---------------------------------------------------------------------------
void MoxController::setAwaitsTxDrain(bool on)
{
    m_awaitTxDrain = on;
    if (!on && m_waitingForTxDrain) {
        // Nothing will report the drain any more: go on with the walk.
        finishTxDrainWait();
    }
}

void MoxController::latchStopAllTx()
{
    // From Thetis console.cs:45329 [v2.10.3.15]: _stop_all_tx = true;
    // The PTT poll then clears it once nothing is pressed
    // (console.cs:25483-25491 [v2.10.3.15]). Thetis polls every millisecond,
    // so with nothing held the latch is gone before the next press; this
    // controller polls only on a source event, so it is clear at once when
    // no source is held.
    // //[2.10.3.6]MWLGE fixes #518  [original inline comment from console.cs:25481]
    m_stopAllTxLatched = m_micPtt || m_catPtt || m_voxPtt || m_tciPtt;
    if (m_stopAllTxLatched) {
        qCInfo(lcDsp) << "MoxController: transmit stopped; a held PTT will not"
                         " key again until it is released";
    }
}

void MoxController::clearManualMox()
{
    // From Thetis console.cs:45332 [v2.10.3.15]: _manual_mox = false;
    const bool wasManual = m_manualMox;
    m_manualMox = false;
    if (wasManual) {
        emit manualMoxChanged(false);
    }
    // The same Thetis flag gates PollPTT (m_manualKey). Clearing it runs
    // one poll pass, which the latch above keeps from keying.
    setManualKey(false);
}

void MoxController::onTxDrained()
{
    if (!m_waitingForTxDrain) {
        return;   // a drain this walk is not waiting for
    }
    finishTxDrainWait();
}

void MoxController::onTxDrainTimedOut()
{
    if (!m_waitingForTxDrain) {
        return;
    }
    // Info, not a warning: WDSP's own drain timeout is silent, and the
    // emergency stop (gate closed first) always ends here.
    qCInfo(lcDsp) << "MoxController: the TX channel's drain did not report within"
                  << m_txDrainTimeoutTimer.interval()
                  << "ms; releasing the hardware without it";
    finishTxDrainWait();
}

void MoxController::finishTxDrainWait()
{
    m_waitingForTxDrain = false;
    m_txDrainTimeoutTimer.stop();
    // G-05: the send ring drains first, while the hardware is still keyed.
    if (beginSendRingWait()) {
        return;
    }
    // From Thetis console.cs:29667-29669 [v2.10.3.15]:
    //   if (mox_delay > 0)
    //       Thread.Sleep(mox_delay); // default 10, allows in-flight samples to clear
    m_keyUpDelayTimer.start();
}

// ---------------------------------------------------------------------------
// G-05 (JJ's ruling 2026-09-28): the unkey waits for the send ring.
// NereusSDR-original. Thetis's outbound buffer releases each frame to its
// sender as soon as the frame fills (OutBound, ChannelMaster/obbuffs.c:
// 100-129 [v2.10.3.15]), and its mox_delay of 10 ms "allows in-flight
// samples to clear" (console.cs:29667-29669 [v2.10.3.15]). NereusSDR's
// send ring paces frames to the radio and holds up to 84 ms (Protocol 1) or
// 341 ms (Protocol 2), so an operator's release waits for it to drain
// before mox_delay, and never longer than its length. The Core's stops, a
// disconnect, TX inhibit, the PA trip and receive-only release at once.
// ---------------------------------------------------------------------------
void MoxController::setSendRingDrain(SendRingDrain drain)
{
    m_sendRing = std::move(drain);
}

bool MoxController::beginSendRingWait()
{
    if (!m_sendRing.drained || !m_sendRing.lengthMs) {
        return false;
    }
    if (transmitBlocked()) {
        return false;   // TX inhibit, the PA trip, receive-only: at once
    }
    if (m_sendRing.permitted && !m_sendRing.permitted()) {
        return false;   // the Core's stops and a disconnect: at once
    }
    const double lengthMs = m_sendRing.lengthMs();
    if (!(lengthMs > 0.0)) {
        return false;   // no send ring
    }
    if (m_sendRing.drained()) {
        return false;   // nothing queued: no wait
    }
    m_sendRingCeilingMs = static_cast<int>(std::ceil(lengthMs));
    m_waitingForSendRing = true;
    m_sendRingCeilingTimer.start(m_sendRingCeilingMs);
    m_sendRingPollTimer.start();
    return true;
}

void MoxController::onSendRingPoll()
{
    if (!m_waitingForSendRing) {
        return;
    }
    if (m_sendRing.drained && !m_sendRing.drained()) {
        return;
    }
    finishSendRingWait();
}

void MoxController::onSendRingCeiling()
{
    if (!m_waitingForSendRing) {
        return;
    }
    qCInfo(lcDsp) << "MoxController: the transmit send ring did not drain within its length of"
                  << m_sendRingCeilingMs << "ms; releasing the hardware without the rest";
    finishSendRingWait();
}

void MoxController::abortSendRingWait()
{
    if (!m_waitingForSendRing) {
        return;
    }
    qCInfo(lcDsp) << "MoxController: transmit stopped; not waiting for the send ring";
    finishSendRingWait();
}

void MoxController::finishSendRingWait()
{
    m_waitingForSendRing = false;
    m_sendRingPollTimer.stop();
    m_sendRingCeilingTimer.stop();
    // From Thetis console.cs:29667-29669 [v2.10.3.15]:
    //   if (mox_delay > 0)
    //       Thread.Sleep(mox_delay); // default 10, allows in-flight samples to clear
    m_keyUpDelayTimer.start();
}

// ---------------------------------------------------------------------------
// setMoxCheck — install (or remove) the BandPlanGuard pre-check callback.
//
// The callback is stored and called from setMox(true) BEFORE the Codex P2
// safety effects hook. If the callback returns ok==false, moxRejected(reason)
// is emitted and setMox returns immediately without advancing state.
//
// Passing nullptr clears the check (backwards-compatible bypass, no rejection).
// Must be called from the main thread before the first setMox() call, matching
// MoxController's main-thread-only contract.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// iPhone app plan Task 34: the keying gate (rulings 8.8, 8.13).
// ---------------------------------------------------------------------------
KeyerIdentity KeyerIdentity::station(PttMode source)
{
    KeyerIdentity keyer;
    keyer.deviceId = QByteArray(kStationDeviceId);
    keyer.source = source;
    keyer.program = source == PttMode::Tci || source == PttMode::Cat;
    return keyer;
}

void MoxController::setKeyingGate(KeyingGateFn gate)
{
    m_keyingGate = std::move(gate);
}

void MoxController::setOtherDeviceHolds(OtherDeviceHoldsFn probe)
{
    m_otherDeviceHolds = std::move(probe);
}

void MoxController::setMox(bool on, const KeyerIdentity& keyer)
{
    if (!on) {
        // Ruling 8.5: a release unkeys only its keyer's key. Unkeying is
        // never asked of the gate.
        if (m_mox && m_currentKeyer.deviceId == keyer.deviceId) {
            setMox(false);
        }
        return;
    }
    if (m_mox) {
        // Already keyed: this keyer's repeat runs the safety effects again
        // (Codex P2). Another keyer's key is never replaced; it is refused
        // with the gate's words (another device holds transmit).
        if (m_currentKeyer.deviceId == keyer.deviceId) {
            m_keyAdmitted = true;
            m_admittedKeyer = keyer;
            setMox(true);
            m_keyAdmitted = false;
            return;
        }
        KeyingAnswer answer;
        if (m_keyingGate) {
            answer = m_keyingGate(keyer.source, keyer);
        }
        const TxRefusal refusal = answer.verdict == KeyingVerdict::Refuse && !answer.refusal.isEmpty()
            ? answer.refusal
            : TxRefusals::changingHands();
        reportRefusal(refusal.text, refusal, /*quiet=*/false);
        return;
    }
    // Fix wave M1: a key TX inhibit, the PA trip, receive only, the band
    // plan or the interlock would refuse never reaches the gate, so it
    // takes nothing; setMox(true) refuses it with its own words below.
    if (m_keyingGate && refusalBeforeTheGate().isEmpty()) {
        const KeyingAnswer answer = m_keyingGate(keyer.source, keyer);
        if (answer.verdict != KeyingVerdict::Admit) {
            if (answer.verdict == KeyingVerdict::Refuse) {
                reportRefusal(answer.refusal.text, answer.refusal, /*quiet=*/false);
            }
            return;
        }
    }
    m_keyAdmitted = true;
    m_admittedKeyer = keyer;
    setMox(true);
    m_keyAdmitted = false;
}

void MoxController::onTakeFinished(const KeyerIdentity& keyer, bool took)
{
    // Ruling 8.9: the press that asked for the take keys only if it is
    // still down; a press released meanwhile keys nothing. Only a station
    // PTT source is followed here; a remote keyer sends its key again.
    if (keyer.isStation() && keyer.tunerPress) {
        // TGXL tune lane: the tuner's press is followed by its tune cycle,
        // which keys only while the tuner still asks for the carrier.
        emit tunerTakeFinished(took);
        return;
    }
    if (!took || !keyer.isStation()) {
        return;
    }
    const quint8 bit = keyer.source == PttMode::Tci ? kRefusedTci
                     : keyer.source == PttMode::Cat ? kRefusedCat
                     : keyer.source == PttMode::Mic ? kRefusedMic
                     : keyer.source == PttMode::Vox ? kRefusedVox
                                                    : 0;
    if (bit == 0 || !isLevelHeld(bit)) {
        return;
    }
    if (bit == kRefusedMic) {
        // Task 77 fix round 2: the mic that is down now is the press that
        // took, not a later one refused while the take ran.
        if (!m_micTakePressDown) {
            return;
        }
        m_micTakePressDown = false;
    }
    clearHeldBits(bit);
    pollPtt();
}

bool MoxController::admitStationKey(PttMode source)
{
    return admitKey(KeyerIdentity::station(source));
}

bool MoxController::admitKey(const KeyerIdentity& keyer)
{
    m_lastAdmitTook = false;
    if (!m_keyingGate) {
        return true;
    }
    if (m_mox && m_currentKeyer.deviceId == keyer.deviceId) {
        return true;   // the keyer's own key is on: nothing to ask
    }
    // Fix wave 2, Important 2: a start TX inhibit, the PA trip, receive
    // only or the interlock refuses never reaches the gate, so it takes
    // nothing.
    if (transmitBlocked()) {
        const TxRefusal blocked = transmitBlockRefusal();
        reportRefusal(blocked.text, blocked, /*quiet=*/false);
        return false;
    }
    if (const TxRefusal interlocked = interlockRefusal(); !interlocked.isEmpty()) {
        // Asked again aloud, as setMox asks it, so the denied toast and the
        // fault log hear this refusal too.
        QString deniedReason;
        const QMetaObject::Connection capture =
            connect(m_interlockPolicy, &TxInterlockPolicy::denied, this,
                    [&deniedReason](const QString& reason) { deniedReason = reason; });
        m_interlockPolicy->evaluateTxRequest(m_ampPresent, m_ampInOperate, m_lastSwr);
        disconnect(capture);
        reportRefusal(QStringLiteral("TX interlock blocked: %1").arg(deniedReason), interlocked,
                      /*quiet=*/false);
        return false;
    }
    const KeyingAnswer answer = m_keyingGate(keyer.source, keyer);
    if (m_mox || answer.verdict != KeyingVerdict::Admit) {
        // Another device's key is on, or the gate refused or took.
        const TxRefusal refusal = answer.refusal.isEmpty() ? TxRefusals::changingHands()
                                                           : answer.refusal;
        if (answer.verdict != KeyingVerdict::Take) {
            reportRefusal(refusal.text, refusal, /*quiet=*/false);
        } else {
            m_lastAdmitTook = true;
        }
        return false;
    }
    return true;
}

TxRefusal MoxController::refusalForCheck(const safety::BandPlanGuard::MoxCheckResult& result)
{
    if (result.refusalCode == TxRefusals::kMicNotReady) {
        return TxRefusals::micNotReady();
    }
    if (result.refusalCode == TxRefusals::kStationReceiveOnly) {
        return TxRefusals::stationReceiveOnly();
    }
    // Slice control plan Task 7: a Core with no slice.
    if (result.refusalCode == TxRefusals::kNoTransmitSlice) {
        return TxRefusals::noTransmitSlice();
    }
    return TxRefusals::bandPlan(result.reason);
}

// ---------------------------------------------------------------------------
// Fix wave M1: what setMox(true) would refuse before any keying state
// changes (TX inhibit, the PA trip, receive only, the band plan and the
// interlock), asked as a question: nothing is reported, no signal is sent.
// The keying gate, which may make a device the holder of transmit, is
// asked only after these pass, so a key they refuse takes nothing.
// ---------------------------------------------------------------------------
TxRefusal MoxController::refusalBeforeTheGate() const
{
    if (transmitBlocked()) {
        return transmitBlockRefusal();
    }
    if (m_moxCheck) {
        const auto result = m_moxCheck();
        if (!result.ok) {
            return refusalForCheck(result);
        }
    }
    return interlockRefusal();
}

TxRefusal MoxController::interlockRefusal() const
{
    if (m_interlockPolicy) {
        bool allowed = false;
        {
            const QSignalBlocker quiet(m_interlockPolicy);
            allowed = m_interlockPolicy->evaluateTxRequest(m_ampPresent, m_ampInOperate, m_lastSwr);
        }
        if (!allowed) {
            const TxInterlockPolicy::Denial denial = m_interlockPolicy->lastDenial();
            return denial == TxInterlockPolicy::Denial::AmpStandby ? TxRefusals::ampStandby()
                 : denial == TxInterlockPolicy::Denial::Swr        ? TxRefusals::swr()
                                                                   : TxRefusals::interlock();
        }
    }
    return {};
}

void MoxController::reportRefusal(const QString& reason, const TxRefusal& refusal, bool quiet)
{
    m_lastRefusal = refusal;
    if (quiet) {
        return;
    }
    emit moxRejected(reason);
    emit moxRefused(refusal);
}

void MoxController::setMoxCheck(MoxCheckFn check)
{
    m_moxCheck = std::move(check);
}

// ---------------------------------------------------------------------------
// Phase 3P-II Task 87: setInterlockPolicy / onAmpStateChanged / onAmpSwrUpdated
//
// setInterlockPolicy: wire the TxInterlockPolicy that setMox(true) consults
// immediately after the BandPlanGuard (K.2) check and before the Codex P2
// safety effects.  Passing nullptr removes the policy.
//
// onAmpStateChanged: update the cached amplifier presence + OPERATE flag
// so that evaluateTxRequest() sees a consistent snapshot at every setMox
// call.  Wired from RadioModel amplifierChanged / ampStateChanged lambdas.
//
// onAmpSwrUpdated: update the cached SWR ratio from the PGXL meter stream.
// Wired from RadioModel ampMetersChanged(float fwd, float swr) lambda.
// Only the swr argument is forwarded.
// ---------------------------------------------------------------------------
void MoxController::setInterlockPolicy(TxInterlockPolicy* policy)
{
    m_interlockPolicy = policy;
}

void MoxController::onAmpStateChanged(bool hasAmp, bool inOperate)
{
    m_ampPresent   = hasAmp;
    m_ampInOperate = inOperate;
    // Phase 3P-II review fix I2: forward to the interlock policy so it can
    // track the OPERATE-on rising edge for the grace-period gate.
    if (m_interlockPolicy) {
        m_interlockPolicy->onAmpStateChanged(hasAmp, inOperate);
    }
}

void MoxController::onAmpSwrUpdated(float swr)
{
    m_lastSwr = swr;
}

// ---------------------------------------------------------------------------
// C.4 — setRx2Enabled / setVfobTx
//
// Idempotent setters that update the internal flags consumed by
// activeRxForTx(), the helper that produces the rx argument carried by the
// moxChanging / moxChanged multicast signals (Pre/Post).
//
// No signal is emitted — these slots only mirror upstream RadioModel state.
// Both flags default to false; the emitted rx argument is therefore always 1
// until RadioModel calls these setters (NereusSDR has no RX2 wired yet —
// scheduled for Phase 3F).
//
// From Thetis console.cs:29324 [v2.10.3.13] (MoxPreChangeHandlers) and
// Upstream tags preserved: //MW0LGE (from cited console.cs:29326) [v2.10.3.15]
//                console.cs:29677 [v2.10.3.13] (MoxChangeHandlers):
//   rx2_enabled && VFOBTX ? 2 : 1
// ---------------------------------------------------------------------------
void MoxController::setRx2Enabled(bool enabled)
{
    if (m_rx2Enabled == enabled) {
        return;  // idempotent — no internal state churn
    }
    m_rx2Enabled = enabled;
}

void MoxController::setVfobTx(bool enabled)
{
    if (m_vfobTx == enabled) {
        return;  // idempotent — no internal state churn
    }
    m_vfobTx = enabled;
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
    if (gated != m_lastVoxRunGated) {
        m_lastVoxRunGated = gated;
        emit voxRunRequested(gated);
    }

    // Task 7 fix wave, M5: once VOX stops running, DEXP pushes nothing, not
    // even pushvox(0) (Thetis wdsp dexp.c:328-339 [v2.10.3.15]: both pushes
    // sit under a->run_vox). A level left set by a switch to a non-voice
    // mode (or TUN-off restoring CW) would then key VOX on the first pass
    // after a return to a voice mode, before DEXP's first push. Thetis has
    // the same stale Audio.VOXActive (set only at cmaster.cs:1945), but its
    // VOX could not key from it before PollPTT; NereusSDR's now can, so the
    // level is dropped here and the pass releases a VOX key.
    if (!gated && m_voxPtt) {
        m_voxPtt = false;
        clearHeldBits(kRefusedVox);
        pollPtt();
    }
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
    // Receiver and transmit gaps plan, Task 7. From Thetis
    // chkVOX_CheckedChanged, console.cs:28904-28906 [v2.10.3.15]:
    //   else { Audio.VOXActive = false; ... }
    // WDSP sends no pushvox(0) once VOX stops running, so without this a
    // VOX key would never release. The next PollPTT pass releases it.
    if (!on && m_voxPtt) {
        m_voxPtt = false;
        clearHeldBits(kRefusedVox);
        pollPtt();
    }
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
// m_pttMode is not reset here: setMox(false) clears it, as the TX-to-RX
// path in chkMOX_CheckedChanged2 sets _current_ptt_mode = PTTMode.NONE
// (console.cs:29547 [v2.10.3.15]).
// ---------------------------------------------------------------------------
void MoxController::setTune(bool on, const KeyerIdentity& keyer)
{
    // iPhone app plan Task 35: the TUN-on below, keyed as `keyer`.
    if (!on) {
        setTune(false);
        return;
    }
    m_tuneKeyer = keyer;
    m_tuneForKeyer = true;
    setTune(true);
    m_tuneForKeyer = false;
}

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
        // The same Thetis flag gates PollPTT (isManualKey()). Receiver and
        // transmit gaps plan, Task 7.
        m_manualKey = true;

        // Engage MOX. setMox runs Codex P2 (safety effect) → idempotent
        // guard → state commit → emit txAboutToBegin → emit
        // hardwareFlipped(true) → start rfDelay → ... → txReady.
        // From Thetis console.cs:30081 [v2.10.3.13]: chkMOX.Checked = true;
        // Task 35: a remote device's TUNE keys as that device.
        if (m_tuneForKeyer) {
            setMox(true, m_tuneKeyer);
        } else {
            setMox(true);
        }

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
        // NOTE: m_pttMode is not reset here: the setMox(false) above
        // cleared it, as chkMOX_CheckedChanged2's TX-to-RX branch does
        // (console.cs:29547 [v2.10.3.15]).
        //
        // m_manualKey stays set: Thetis clears _manual_mox only at the end
        // of TUN-off, after the tone and power are restored
        // (console.cs:30193 [v2.10.3.15]). RadioModel::completeTuneOff calls
        // setManualKey(false) there.
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
    // ── Task 7 fix wave, I2: TX inhibit and the PA trip refuse every key ─────
    //
    // From Thetis chkMOX_CheckedChanged2, console.cs:29364-29371 [v2.10.3.15]:
    //   if(chkMOX.Checked && _ganymede_pa_issue)
    //   {
    //       // abort the change if there is a ganymede pa issue
    //       chkMOX.CheckedChanged -= chkMOX_CheckedChanged2;
    //       chkMOX.Checked = false;
    //       chkMOX.CheckedChanged += chkMOX_CheckedChanged2;
    //       return;
    //   }
    // and the TXInhibit setter, console.cs:15341-15363 [v2.10.3.15], which
    // disables the MOX, TUN, two-tone and VOX buttons while inhibited:
    //   chkTUN.Enabled = !_tx_inhibit;
    //   chk2TONE.Enabled = !_tx_inhibit; //MW0LGE_21a
    //   chkVOX.Enabled = !_tx_inhibit;
    // NereusSDR refuses here, where every key passes (MOX button, container
    // button, TUN, two-tone, and the PollPTT sources if one got past the
    // gate in pollPtt), rather than greying the buttons. This is stricter
    // than Thetis for TX inhibit: Thetis disables the buttons but a CAT or
    // programmatic MOX write is not refused. A refusal is reported through
    // moxRejected, so the MOX button drops back, TUN runs its TUN-off path
    // and two-tone cleans up, as for any refused key. The PollPTT sources
    // never reach this: the gate in pollPtt skips them silently, as
    // Thetis's PollPTT does.
    //
    // Task 16: receive only refuses here too. From Thetis
    // chkMOX_CheckedChanged2, console.cs:29378-29382 [v2.10.3.15]:
    //   if (_rx_only && chkMOX.Checked)
    //   {
    //       chkMOX.Checked = false;
    //       return;
    //   }
    if (on && transmitBlocked()) {
        // Task 34: the same refusal as a TxRefusal (paProtection,
        // stationReceiveOnly, or for the TX inhibit input, interlock).
        reportRefusal(transmitBlockReason(), transmitBlockRefusal(), /*quiet=*/false);
        if (!m_mox) {
            dropPttOnUnkey();
        }
        return;
    }

    // ── iPhone app plan Task 34: the keying gate (rulings 8.8, 8.13) ──────────
    //
    // Who may key: asked with the source and the keyer before MOX changes,
    // beside the band plan below and the interlock after it. A PTT source
    // asked already at its press edge (tryPollKey, before its mode was
    // set), and a remote key in setMox(on, keyer); both arrive here with
    // m_keyAdmitted set. Any other key here is the station device's: the
    // MOX and TUNE buttons, two-tone, a local caller. A repeated
    // setMox(true) while keyed is not a key, so it is not asked.
    // Fix wave M1: only a key the checks below would let through asks it.
    if (on && !m_mox && m_keyingGate && !m_keyAdmitted && refusalBeforeTheGate().isEmpty()) {
        const KeyerIdentity keyer = KeyerIdentity::station(m_pttMode);
        const KeyingAnswer answer = m_keyingGate(m_pttMode, keyer);
        if (answer.verdict != KeyingVerdict::Admit) {
            if (answer.verdict == KeyingVerdict::Refuse) {
                reportRefusal(answer.refusal.text, answer.refusal, m_quietRefusal);
            }
            // A refused or taken key ends as every refused key does.
            dropPttOnUnkey();
            return;
        }
    }
    // Task 34 (ruling 8.5): while another device's key is on, a station
    // key (the MOX or TUNE button, two-tone) never rides on it: the carrier
    // is that device's. Refused with the gate's words; nothing changes.
    if (on && m_mox && !m_keyAdmitted && !m_currentKeyer.isStation()) {
        KeyingAnswer answer;
        if (m_keyingGate) {
            answer = m_keyingGate(m_pttMode, KeyerIdentity::station(m_pttMode));
        }
        reportRefusal(answer.refusal.isEmpty() ? TxRefusals::changingHands().text
                                               : answer.refusal.text,
                      answer.refusal.isEmpty() ? TxRefusals::changingHands() : answer.refusal,
                      /*quiet=*/false);
        return;
    }

    // ── K.2: BandPlanGuard pre-check (BEFORE Codex P2 safety effects) ────────
    //
    // When a MoxCheckFn is installed and the caller is requesting TX-on,
    // consult the callback before doing ANYTHING else. If rejected, emit
    // moxRejected(reason) and return immediately — no safety effects, no
    // state advance, no idempotent guard consumption.
    //
    // Rationale: safety effects (runMoxSafetyEffects) are designed to fire on
    // every setMox() including idempotent repeats (Codex P2). Emitting a
    // rejection is not a "safety effect" — it is a guard that aborts the
    // entire call. Placing the guard here (before Step 1) means:
    //   - Safety effects ONLY fire when MOX is actually going to proceed.
    //   - Rejected calls are cheap (no routing changes, no state mutation).
    //   - The Codex P2 invariant is preserved for accepted calls.
    if (on && m_moxCheck) {
        const auto result = m_moxCheck();
        if (!result.ok) {
            // R-R3-36: tryPollKey reads this to hold a refused source off
            // until it is pressed again (m_notQueuedHeld).
            m_lastRefusalNotQueued = result.notQueued;
            // Task 7 fix wave, M3: a held source's repeat refusal is quiet.
            // Task 34: its TxRefusal by the code the check names (none: the
            // band plan).
            reportRefusal(result.reason, refusalForCheck(result), m_quietRefusal);
            // Thetis refuses a key by unchecking chkMOX, which runs the
            // TX-to-RX branch of chkMOX_CheckedChanged2 (PTT mode NONE, CAT
            // and TCI PTT dropped). Only from receive: a repeated
            // setMox(true) while keyed is not a key in Thetis at all.
            if (!m_mox) {
                dropPttOnUnkey();
            }
            return;
        }
    }

    // ── Phase 3P-II Task 87: TxInterlockPolicy gate ───────────────────────────
    //
    // Placed AFTER the BandPlanGuard check (K.2) and BEFORE the Codex P2
    // safety effects so:
    //   - BandPlan violations are already caught by the stricter band guard.
    //   - Safety effects (Alex routing, ATT) only fire for accepted TX requests.
    //
    // evaluateTxRequest may emit warned(reason) or denied(reason) on the policy
    // object.  The warned/denied signals are plumbed to the operator UI toast
    // in Task 97.  For this task the signals exist but have no UI subscriber.
    if (on && m_interlockPolicy) {
        // Task 7 fix wave, M2: a refusal here is reported through
        // moxRejected like the check above, so the MOX button drops back,
        // TUN runs its TUN-off path and two-tone cleans up (it listens only
        // to moxRejected). The text matches MainWindow's denied toast, which
        // folds a repeat of the same message into one.
        //
        // M3: a held source's repeat refusal is quiet: the policy is asked
        // with its signals blocked. evaluateTxRequest only reads state, so
        // when it allows the key it is asked again unblocked, so a Warn-mode
        // warning still reaches the operator.
        QString deniedReason;
        bool allowed = false;
        if (m_quietRefusal) {
            {
                const QSignalBlocker quiet(m_interlockPolicy);
                allowed = m_interlockPolicy->evaluateTxRequest(
                    m_ampPresent, m_ampInOperate, m_lastSwr);
            }
            if (allowed) {
                allowed = m_interlockPolicy->evaluateTxRequest(
                    m_ampPresent, m_ampInOperate, m_lastSwr);
            }
        } else {
            const QMetaObject::Connection capture =
                connect(m_interlockPolicy, &TxInterlockPolicy::denied, this,
                        [&deniedReason](const QString& reason) { deniedReason = reason; });
            allowed = m_interlockPolicy->evaluateTxRequest(
                m_ampPresent, m_ampInOperate, m_lastSwr);
            disconnect(capture);
        }
        if (!allowed) {
            // denied() signal already emitted by the policy (unless quiet).
            // Task 34: the amplifier in standby and the SWR over its limit
            // have refusals of their own (ampStandby offers operateAmp).
            const TxInterlockPolicy::Denial denial = m_interlockPolicy->lastDenial();
            reportRefusal(QStringLiteral("TX interlock blocked: %1").arg(deniedReason),
                          denial == TxInterlockPolicy::Denial::AmpStandby ? TxRefusals::ampStandby()
                          : denial == TxInterlockPolicy::Denial::Swr      ? TxRefusals::swr()
                                                                          : TxRefusals::interlock(),
                          m_quietRefusal);
            // A refused key ends like a Thetis refusal (see above).
            if (!m_mox) {
                dropPttOnUnkey();
            }
            return;
        }
    }

    // ── Step 1: Safety effects (BEFORE idempotent guard — Codex P2) ─────────
    // F.1 wires: AlexController::applyAntennaForBand(currentBand, isTx)
    //            StepAttenuatorController TX-path activation / RX restore
    //            RadioConnection::setMoxBit(isTx) + setTrxRelayBit(isTx)
    runMoxSafetyEffects(on);

    // ── Step 2: Idempotent guard ──────────────────────────────────────────────
    if (m_mox == on) {
        return;  // no real transition — no state advance, no timer chain
    }

    // ── C.2: Pre signal (multicast, before m_mox commit) ─────────────────────
    // From Thetis console.cs:29322-29324 [v2.10.3.13]:
    // Upstream tags preserved: //MW0LGE (from cited console.cs:29326) [v2.10.3.15]
    //   bool bOldMox = _mox; //MW0LGE_21b used for state change delgates at end of fn
    //   MoxPreChangeHandlers?.Invoke(rx2_enabled && VFOBTX ? 2 : 1, _mox, chkMOX.Checked); // MW0LGE_21k8
    //
    // Emit BEFORE m_mox commit so subscribers' isMox() snapshot still reflects
    // the OLD value. Subscribers (PS form, MeterPoller, recorder…) can
    // defensively freeze readings between Pre and Post.
    //
    // Note: Thetis' Pre fires before the rx_only short-circuit and the VAC
    // bypass logic; in NereusSDR we have no rx_only equivalent at this layer
    // (RadioModel-level RX-only enforcement lives elsewhere) so the placement
    // BETWEEN the idempotent guard and the m_mox commit is the most faithful
    // translation. By construction, m_mox != on here (idempotent guard passed).
    emit moxChanging(activeRxForTx(), m_mox, on);  // MW0LGE_21k8 — Pre

    // ── Task 7: an unkey clears the PTT state ─────────────────────────────────
    // chkMOX_CheckedChanged2 does this on every unkey, whichever source
    // unchecked chkMOX (see dropPttOnUnkey).
    if (!on) {
        dropPttOnUnkey();
    }

    // ── Step 3: Commit new MOX state ─────────────────────────────────────────
    m_mox = on;
    // Task 34: who this key is for (the gate admitted it for them, or the
    // station device's own key); an unkey leaves nobody's key on.
    m_currentKeyer = on ? (m_keyAdmitted ? m_admittedKeyer : KeyerIdentity::station(m_pttMode))
                        : KeyerIdentity::station(PttMode::None);

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
        // TX→RX path (Task 33: Thetis's order, the drain first and the
        // hardware after it).
        // From Thetis console.cs:29651-29685 [v2.10.3.15]:
        //   if (space_mox_delay > 0)
        //       Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE
        //   _mox = tx;
        //   psform.Mox = tx;
        //   WDSP.SetChannelState(WDSP.id(1, 0), 0, 1);  // turn off the transmitter (no action if it's already off)
        //   ... if (mox_delay > 0) Thread.Sleep(mox_delay); // default 10, allows in-flight samples to clear
        //   UpdateDDCs(rx2_enabled);
        //   UpdateAAudioMixerStates();
        //   AudioMOXChanged(tx);    // set audio.cs to RX
        //   HdwMOXChanged(tx, freq);// flip the hardware
        //   ...
        //   if (ptt_out_delay > 0)
        //       Thread.Sleep(ptt_out_delay);  //wcp:  added 2018-12-24, time for HW to switch
        //   WDSP.SetChannelState(WDSP.id(0, 0), 1, 0);  // turn on appropriate receivers
        //
        // Phase signal ordering:
        //   Phase 1 of 5: emit txAboutToEnd()
        //   Phase 2 of 5: emit txDrainRequested(); the TX channel drains
        //                  while the hardware is still keyed, so WDSP's
        //                  down-slew goes out on the air.
        //   Walk: Tx → TxToRxInFlight (the drain, then keyUpDelayTimer,
        //              10 ms) → TxToRxFlush (pttOutDelayTimer, 20 ms) → Rx
        //   Phase 3 of 5: emit txaFlushed() in onKeyUpDelayElapsed()
        //   Phase 4 of 5: emit hardwareFlipped(false) right after it
        //   Phase 5 of 5: emit rxReady() in onPttOutElapsed()
        //   moxStateChanged(false) emitted after rxReady() (diagnostic signal)
        //
        // spaceDelay is skipped when kSpaceDelayMs == 0 (matches the
        // Thetis `if (space_mox_delay > 0)` guard).
        //
        // RADE end-of-over callsigns: an operator's release first sends the
        // end-of-over tail (at most kEndOfOverTailMaxMs), with the hardware
        // still keyed, and phase 1 follows it. Never under TX inhibit, the
        // PA trip or receive-only: those unkeys stop transmit at once.
        if (m_endOfOverTail && !transmitBlocked() && m_endOfOverTail()) {
            // The tail is on before stateChanged, so a subscriber of the
            // state change (RemoteKeying, the transmit state) already sees
            // it.
            m_waitingForEndOfOverTail = true;
            m_endOfOverTailTimer.start();
            advanceState(MoxState::TxToRxInFlight);
            emit endOfOverTailChanged(true);
            return;
        }
        beginTxToRxTeardown();
    }
    // NOTE: moxStateChanged is NOT emitted here.  It is emitted at the END
    // of the timer walk (in onRfDelayElapsed for TX, onPttOutElapsed for RX)
    // so subscribers see "MOX fully engaged" / "MOX fully released".
}

// The TX→RX walk from phase 1 on (Task 33's order; see setMox above).
void MoxController::beginTxToRxTeardown()
{
    emit txAboutToEnd();                            // TX→RX phase 1 of 5
    advanceState(MoxState::TxToRxInFlight);
    const bool awaitDrain = m_awaitTxDrain;
    if (awaitDrain) {
        // Thetis's SetChannelState(tx, 0, 1) returns before mox_delay
        // starts; the drain runs on the transmit lane here, so wait for
        // it, bounded as WDSP bounds it. Armed before the request so a
        // drain that reports at once (no lane) is not missed.
        m_waitingForTxDrain = true;
        m_txDrainTimeoutTimer.start();
    }
    emit txDrainRequested();                        // TX→RX phase 2 of 5
    if (!awaitDrain) {
        // G-05: without a drain to wait for, the send ring's wait (if
        // any) comes straight after the request.
        if (beginSendRingWait()) {
            return;
        }
        m_keyUpDelayTimer.start();
    }
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
// dropPttOnUnkey: the PTT state after an unkey (or a refused key).
//
// Receiver and transmit gaps plan, Task 7. From Thetis
// chkMOX_CheckedChanged2, console.cs:29404-29411 [v2.10.3.15]:
//   bool tx = chkMOX.Checked;
//
//   //[2.10.1.0]MW0LGE changed
//   if (!tx)
//   {
//       if (CATPTT) CATPTT = false;
//       if (TCIPTT) TCIPTT = false;
//   }
// and its TX-to-RX branch, console.cs:29545-29547 [v2.10.3.15]:
//   else
//   {
//       _current_ptt_mode = PTTMode.NONE;
// Every source unkeys through chkMOX.Checked = false, so this runs on every
// unkey. A refused key ends the same way: Thetis refuses by unchecking
// chkMOX, which re-enters this branch.
// ---------------------------------------------------------------------------
void MoxController::dropPttOnUnkey()
{
    //[2.10.1.0]MW0LGE changed  [original inline comment from console.cs:29406]
    m_catPtt = false;
    m_tciPtt = false;
    clearHeldBits(kRefusedCat | kRefusedTci);
    setPttMode(PttMode::None);
}

// ---------------------------------------------------------------------------
// tryPollKey: `_current_ptt_mode = X; chkMOX.Checked = true;` for one
// PollPTT source (console.cs:25507-25555 [v2.10.3.15]). chkMOX.Checked =
// true on a checked box fires nothing, so a key is tried only from receive.
//
// Task 7 fix wave, M3: the keying is PollPTT's, tried on every pass while
// the source is held; only the refusal message is limited to the first
// refusal of each press (see m_refusedHeld).
// ---------------------------------------------------------------------------
void MoxController::tryPollKey(PttMode mode, quint8 refusedBit)
{
    if (m_mox) {
        // Task 34 (ruling 8.5): a station source never renames another
        // keyer's key, or its release would unkey that keyer.
        if (m_currentKeyer.isStation()) {
            setPttMode(mode);
        }
        return;
    }
    // ── Task 34: the keying gate, at the press edge (ruling 8.8) ─────────────
    // Asked before the mode is set, so a refused press leaves the PTT mode
    // as it was. A refused or taken press is held off until its source is
    // released (m_notQueuedHeld): the radio repeats its PTT level on every
    // status frame, and the gate acts once per edge. A refused app level
    // (CAT, TCI) is dropped, so the app is answered that nothing keyed.
    // Fix wave M1: a press TX inhibit, the PA trip, receive only, the band
    // plan or the interlock would refuse is not asked (setMox refuses it
    // below), so it takes nothing.
    if (m_keyingGate && refusalBeforeTheGate().isEmpty()) {
        const KeyerIdentity keyer = KeyerIdentity::station(mode);
        const KeyingAnswer answer = m_keyingGate(mode, keyer);
        if (answer.verdict == KeyingVerdict::Take && mode == PttMode::Mic) {
            // Task 77 fix round 2: this press took; it keys when the take
            // ends if it is still down (onTakeFinished).
            m_micTakePressDown = true;
        }
        if (answer.verdict != KeyingVerdict::Admit) {
            if (answer.verdict == KeyingVerdict::Refuse) {
                reportRefusal(answer.refusal.text, answer.refusal,
                              (m_refusedHeld & refusedBit) != 0);
            }
            m_refusedHeld |= refusedBit;
            if (isLevelHeld(refusedBit)) {
                m_notQueuedHeld |= refusedBit;
            }
            if (answer.verdict == KeyingVerdict::Refuse) {
                if (refusedBit == kRefusedCat) {
                    m_catPtt = false;
                } else if (refusedBit == kRefusedTci) {
                    m_tciPtt = false;
                }
                // Task 77 fix round 3: a dropped level is a release too.
                reportIfSourcesReleased();
            }
            return;
        }
        m_admittedKeyer = keyer;
        m_keyAdmitted = true;
    }
    setPttMode(mode);
    m_quietRefusal = (m_refusedHeld & refusedBit) != 0;
    m_lastRefusalNotQueued = false;
    setMox(true);
    m_keyAdmitted = false;
    m_quietRefusal = false;
    if (m_mox) {
        m_refusedHeld &= static_cast<quint8>(~refusedBit);
    } else {
        m_refusedHeld |= refusedBit;
        // R-R3-36: refused because the microphone is not ready, and never
        // queued. pollPtt skips this source until its level drops. A CAT or
        // TCI level is already dropped by the refusal (dropPttOnUnkey), so
        // only a source still held is latched.
        if (m_lastRefusalNotQueued && isLevelHeld(refusedBit)) {
            m_notQueuedHeld |= refusedBit;
        }
    }
    m_lastRefusalNotQueued = false;
}

// ---------------------------------------------------------------------------
// clearHeldBits: a source's level dropped (or was dropped), so its next
// level is a new press: it is told of a refusal again (M3) and is tried
// again after a never-queued refusal (R-R3-36).
// ---------------------------------------------------------------------------
bool MoxController::isLevelHeld(quint8 bit) const noexcept
{
    switch (bit) {
    case kRefusedTci: return m_tciPtt;
    case kRefusedCat: return m_catPtt;
    case kRefusedMic: return m_micPtt;
    case kRefusedVox: return m_voxPtt;
    default:          return false;
    }
}

void MoxController::clearHeldBits(quint8 bits)
{
    m_refusedHeld &= static_cast<quint8>(~bits);
    m_notQueuedHeld &= static_cast<quint8>(~bits);
    reportIfSourcesReleased();
}

void MoxController::reportIfSourcesReleased()
{
    // iPhone app plan Task 77 fix round 2: every release path clears its
    // held bits, so this is where the last release is seen.
    if (!anyPttSourceHeld()) {
        emit pttSourcesReleased();
    }
}

// ---------------------------------------------------------------------------
// pollPtt: one pass of Thetis PollPTT over the recorded source levels.
//
// Receiver and transmit gaps plan, Task 7. From Thetis PollPTT,
// console.cs:25463-25623 [v2.10.3.15]. The pass, with what is ported:
//
//   if (!_manual_mox && !_disable_ptt && !_rx_only && !_tx_inhibit && !QSKEnabled && !_ganymede_pa_issue)
//   {
//       bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
//       bool cw_ptt = CWInput.KeyerPTT && _current_breakin_mode == BreakIn.Semi; // CW serial PTT  //[2.10.3.9]MW0LGE only want to do this on semi breakin
//       ...
//       if (!_mox)
//       {
//           // we can come in here from a ToT ( StopAllTX() ) //[2.10.3.6]MWLGE fixes #518
//           ...
//           if (_tci_ptt) { _current_ptt_mode = PTTMode.TCI; chkMOX.Checked = true; }
//           if (cat_ptt)  { _current_ptt_mode = PTTMode.CAT; chkMOX.Checked = true; }
//           if ((tx_mode == CWL || CWU) && (cw_ptt || mic_ptt)) { ...CW... }
//           if ((voice modes || _all_mode_mic_ptt) && mic_ptt && _current_ptt_mode != PTTMode.CW)
//               { _current_ptt_mode = PTTMode.MIC; chkMOX.Checked = true; }
//           if (voice modes && vox_ptt) { _current_ptt_mode = PTTMode.VOX; chkMOX.Checked = true; }
//       }
//       else // else if(mox)
//       {
//           switch (_current_ptt_mode)
//           {
//               case PTTMode.TCI: if (!_tci_ptt) fallback or chkMOX.Checked = false
//               case PTTMode.CAT: if (!cat_ptt) chkMOX.Checked = false;
//               case PTTMode.MIC: if (!mic_ptt) chkMOX.Checked = false;
//               case PTTMode.CW:  if (!cw_ptt && !mic_ptt) chkMOX.Checked = false;
//               case PTTMode.VOX: if (!vox_ptt) chkMOX.Checked = false;
//           }
//       }
//   }
//
// Ported: the _manual_mox gate (m_manualKey), keying from receive in
// Thetis's order with the mode set just before the key, and the release
// switch for TCI, CAT, MIC and VOX. chkMOX.Checked = true on a box already
// checked fires nothing in Thetis, so a later held source only renames the
// mode (setMox(true) is not called again while m_mox is set).
//
// _stop_all_tx is ported (Task 33): RadioModel::stopAllTx sets it through
// latchStopAllTx, and the receive branch below skips every source until all
// are released.
//
// _tx_inhibit and _ganymede_pa_issue are ported (Task 7 fix wave, I2):
// RadioModel feeds them from TxInhibitMonitor and RadioModel::paTripped()
// (setTxInhibited, setPaTripped). While either is set the whole pass is
// skipped, as in Thetis: no source keys and none is refused per frame.
//
// Not ported, each for its own reason:
//   - _disable_ptt, QSKEnabled: no NereusSDR equivalent in this
//     controller; QSK is 3M-2. (_rx_only is ported by Task 16, setRxOnly.)
//   - The CW branch and PTTMode.CW: CW keying is 3M-2 (onCwPtt refuses).
//   - The mic's tx_mode gate (voice modes or _all_mode_mic_ptt): a mic PTT
//     keys in every mode, as it did before. NereusSDR's RADE modes are not
//     in Thetis's voice list and RADE transmits from the mic PTT.
//   - VACBypass: VAX is not VAC.
// ---------------------------------------------------------------------------
void MoxController::pollPtt()
{
    // From Thetis console.cs:25470 [v2.10.3.15]:
    //   if (!_manual_mox && !_disable_ptt && !_rx_only && !_tx_inhibit && !QSKEnabled && !_ganymede_pa_issue)
    // (cw_ptt, on the lines below it, carries: //[2.10.3.9]MW0LGE only want to do this on semi breakin  [original inline comment from console.cs:25473])
    // The manual key, TX inhibit and the PA trip are ported (Task 7 and
    // its fix wave, I2); _rx_only is ported by Task 16 (setRxOnly).
    if (m_manualKey || transmitBlocked()) {
        return;
    }

    if (!m_mox) {
        // Task 33: StopAllTx's latch.
        // From Thetis console.cs:25479-25492 [v2.10.3.15]:
        //   // we can come in here from a ToT ( StopAllTX() ) //[2.10.3.6]MWLGE fixes #518
        //   // however we dont want switch anything back on, unless all of the above have been released
        //   if (_stop_all_tx)
        //   {
        //       if (mic_ptt || cw_ptt || cat_ptt || vox_ptt || _tci_ptt)
        //       { await Task.Delay(1); continue; // skip all, and restart the loop }
        //       else
        //           _stop_all_tx = false;
        //   }
        // cw_ptt is 3M-2 (onCwPtt refuses every press).
        if (m_stopAllTxLatched) {
            if (m_micPtt || m_catPtt || m_voxPtt || m_tciPtt) {
                return;
            }
            m_stopAllTxLatched = false;
        }

        // From Thetis console.cs:25507-25511 [v2.10.3.15]
        // R-R3-36: a source refused because the microphone was not ready
        // is skipped until it is released (isHeldOff). Not in Thetis,
        // which has no microphone admission; its band-plan and interlock
        // refusals are still retried on every pass, as PollPTT does.
        if (m_tciPtt && !isHeldOff(kRefusedTci)) {
            tryPollKey(PttMode::Tci, kRefusedTci);
        }
        // From Thetis console.cs:25513-25517 [v2.10.3.15]
        if (m_catPtt && !isHeldOff(kRefusedCat)) {
            tryPollKey(PttMode::Cat, kRefusedCat);
        }
        // From Thetis console.cs:25526-25541 [v2.10.3.15] (mode gate not
        // ported, see above; PTTMode.CW is never set before 3M-2)
        if (m_micPtt && m_pttMode != PttMode::Cw && !isHeldOff(kRefusedMic)) {
            tryPollKey(PttMode::Mic, kRefusedMic);
        }
        // From Thetis console.cs:25543-25555 [v2.10.3.15]
        if (m_voxPtt && isVoiceMode(m_currentMode) && !isHeldOff(kRefusedVox)) {
            tryPollKey(PttMode::Vox, kRefusedVox);
        }
        return;
    }

    // Task 34 (ruling 8.5): the station's sources release only the station
    // device's own key; a remote keyer's key is its own to release.
    if (!m_currentKeyer.isStation()) {
        return;
    }

    switch (m_pttMode) {
    case PttMode::Tci:
        // From Thetis console.cs:25562-25581 [v2.10.3.15]
        if (!m_tciPtt) {
            // From Thetis getFallbackPTTModeAfterTCIRelease,
            // console.cs:25429-25461 [v2.10.3.15]: CAT, then CW (3M-2),
            // then MIC, then VOX (voice modes), else NONE. The mic's
            // tx_mode gate is left out as on the keying side.
            // R-R3-36: a source held off after a never-queued refusal is
            // not a fallback either; taking the key would key it without a
            // new press.
            PttMode fallback = PttMode::None;
            if (m_catPtt && !isHeldOff(kRefusedCat)) {
                fallback = PttMode::Cat;
            } else if (m_micPtt && !isHeldOff(kRefusedMic)) {
                fallback = PttMode::Mic;
            } else if (m_voxPtt && isVoiceMode(m_currentMode) && !isHeldOff(kRefusedVox)) {
                fallback = PttMode::Vox;
            }
            if (fallback == PttMode::None) {
                setMox(false);
                break;
            }
            // Task 7 fix wave, M6 (R-R3-36): the key moves off TCI audio to
            // a source that reads the PC microphone, so it is admitted as a
            // new key would be: the MOX pre-check, which holds the
            // microphone-ready check, runs first, and a refusal ends the
            // key. Not in Thetis (it has no microphone admission); applied
            // to every fallback, since CAT and VOX transmit the microphone
            // too.
            if (m_moxCheck) {
                const auto admit = m_moxCheck();
                if (!admit.ok) {
                    reportRefusal(admit.reason, refusalForCheck(admit), /*quiet=*/false);
                    // M3: that source, still held, is not told again.
                    const quint8 bit = (fallback == PttMode::Cat) ? kRefusedCat
                                     : (fallback == PttMode::Mic) ? kRefusedMic
                                                                  : kRefusedVox;
                    m_refusedHeld |= bit;
                    // R-R3-36: and, refused for the microphone, it is not
                    // keyed later without a new press.
                    if (admit.notQueued) {
                        m_notQueuedHeld |= bit;
                    }
                    setMox(false);
                    break;
                }
            }
            setPttMode(fallback);
        }
        break;
    case PttMode::Cat:
        // From Thetis console.cs:25582-25588 [v2.10.3.15]
        if (!m_catPtt) {
            setMox(false);
        }
        break;
    case PttMode::Mic:
        // From Thetis console.cs:25589-25595 [v2.10.3.15]
        if (!m_micPtt) {
            setMox(false);
        }
        break;
    case PttMode::Vox:
        // From Thetis console.cs:25603-25608 [v2.10.3.15]
        if (!m_voxPtt) {
            setMox(false);
        }
        break;
    default:
        // NONE, MANUAL, SPACE, X2: PollPTT has no case; no source releases.
        break;
    }
}

// ---------------------------------------------------------------------------
// clearPttSources: the radio is going away.
//
// Receiver and transmit gaps plan, Task 7. Thetis polls PTT only while the
// radio is on: `while (chkPower.Checked)` (console.cs:25465 [v2.10.3.15]).
// NereusSDR drops the recorded levels at disconnect instead: the TX channel
// that reports VOX is destroyed and no status frame will report the mic, so
// a level left set would key on the next connection's first pass.
// ---------------------------------------------------------------------------
void MoxController::clearPttSources()
{
    m_micPtt = false;
    m_catPtt = false;
    m_voxPtt = false;
    m_tciPtt = false;
    m_refusedHeld = 0;
    m_notQueuedHeld = 0;
    m_micTakePressDown = false;
    reportIfSourcesReleased();
}

// ---------------------------------------------------------------------------
// setTxInhibited: Thetis console.TXInhibit.
//
// Task 7 fix wave, I2. From Thetis console.cs:15341-15363 [v2.10.3.15]:
//   public bool TXInhibit
//   {
//       get { return _tx_inhibit; }
//       set
//       {
//           _tx_inhibit = value;
//           ... chkMOX.Enabled = !_tx_inhibit;
//           chkTUN.Enabled = !_tx_inhibit;
//           chk2TONE.Enabled = !_tx_inhibit; //MW0LGE_21a
//           chkVOX.Enabled = !_tx_inhibit;
//           ...
//           if (_tx_inhibit && chkMOX.Checked)
//               chkMOX.Checked = false;
//           toolStripStatusLabel_TXInhibit.Visible = _tx_inhibit;
//       }
//   }
// PollPTT skips every source while it is set (console.cs:25470) and
// setMox(true) refuses every other key. An active transmission unkeys
// here; RadioModel turns TUN and two-tone off with it. The manual key is
// left alone, as chkMOX.Checked = false (not chkMOX_Click) leaves
// _manual_mox in Thetis.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// transmitBlockReason: the words setMox(true) refuses with while one of the
// three gates holds, the PA trip first, then receive only, then TX inhibit;
// empty when none does. Task 16 fix wave (M2): RadioModel's TGXL autotune
// checks it before it sends anything to the amplifier or the tuner, and the
// Tuner applet's TUNE shows it (transmitBlockChanged).
// ---------------------------------------------------------------------------
TxRefusal MoxController::transmitBlockRefusal() const
{
    // iPhone app plan Task 34: transmitBlockReason's gate as a TxRefusal,
    // in the same order.
    if (m_radioLinkDown) {
        return TxRefusals::radioLinkDown();
    }
    if (m_paTripped) {
        return TxRefusals::paProtection();
    }
    if (m_rxOnly) {
        TxRefusal refusal = TxRefusals::stationReceiveOnly();
        refusal.text = m_rxOnlyReason;
        return refusal;
    }
    if (m_txInhibited) {
        TxRefusal refusal = TxRefusals::txInhibited();
        if (!m_txInhibitReason.isEmpty()) {
            refusal.text = m_txInhibitReason;
        }
        return refusal;
    }
    return TxRefusal{};
}

QString MoxController::transmitBlockReason() const
{
    if (m_radioLinkDown) {
        return TxRefusals::radioLinkDown().text;
    }
    if (m_paTripped) {
        return QStringLiteral("The amplifier has tripped. Reset it before transmitting.");
    }
    if (m_rxOnly) {
        return m_rxOnlyReason;
    }
    if (m_txInhibited) {
        if (!m_txInhibitReason.isEmpty()) {
            return m_txInhibitReason;
        }
        return QStringLiteral("Transmit is inhibited.");
    }
    return QString();
}

void MoxController::emitTransmitBlockIfChanged(const QString& before)
{
    const QString after = transmitBlockReason();
    if (after != before) {
        emit transmitBlockChanged(after);
    }
}

void MoxController::setTxInhibited(bool on, const QString& reason)
{
    const QString before = transmitBlockReason();
    m_txInhibited = on;
    m_txInhibitReason = on ? reason : QString();
    emitTransmitBlockIfChanged(before);
    if (on) {
        dropAppLevelsUnderBlock();
    }
    if (on && m_mox) {
        setMox(false);
    }
    // RADE end-of-over callsigns: a block ends a running tail at once too.
    // G-05: and the send ring's wait.
    if (on) {
        abortEndOfOverTail();
        abortSendRingWait();
    }
}

// ---------------------------------------------------------------------------
// dropAppLevelsUnderBlock: Task 7 follow-up, N3.
//
// Thetis keeps _cat_ptt and _tci_ptt through an inhibit or a PA trip, and
// PollPTT keys from them on its first pass after the block lifts (for
// example right after an amplifier reset), with the app's audio. NereusSDR
// drops them when the block is asserted, and onCatPtt / onTciPtt refuse a
// new request while it holds, so the app is answered trx:0,false and
// nothing keys later without a new request. The mic and VOX keep Thetis's
// behaviour: a person is holding them, and they key after the block lifts.
// ---------------------------------------------------------------------------
void MoxController::dropAppLevelsUnderBlock()
{
    m_catPtt = false;
    m_tciPtt = false;
    clearHeldBits(kRefusedCat | kRefusedTci);
}

// ---------------------------------------------------------------------------
// setPaTripped: Thetis _ganymede_pa_issue.
//
// Task 7 fix wave, I2. PollPTT skips every source while it is set
// (console.cs:25470 [v2.10.3.15]) and chkMOX_CheckedChanged2 aborts any key
// (console.cs:29364-29371). RadioModel::handleGanymedeTrip carries the
// trip handler that sets it and unkeys (Andromeda.cs, G8NJJ); an active
// transmission unkeys here too, so the controller's own MOX follows it.
// Called on every trip message, so a repeated trip unkeys again.
// ---------------------------------------------------------------------------
void MoxController::setPaTripped(bool on)
{
    const QString before = transmitBlockReason();
    m_paTripped = on;
    emitTransmitBlockIfChanged(before);
    if (on) {
        dropAppLevelsUnderBlock();   // Task 7 follow-up, N3
    }
    if (on && m_mox) {
        setMox(false);
    }
    // RADE end-of-over callsigns: a block ends a running tail at once too.
    // G-05: and the send ring's wait.
    if (on) {
        abortEndOfOverTail();
        abortSendRingWait();
    }
}

// ---------------------------------------------------------------------------
// setRxOnly: Thetis console.RXOnly (receiver and transmit gaps plan, Task 16).
//
// From Thetis console.cs:15312-15334 [v2.10.3.15]:
//   public bool RXOnly
//   {
//       get { return _rx_only; }
//       set
//       {
//           _rx_only = value;
//           if (_rx1_dsp_mode != DSPMode.SPEC &&
//               _rx1_dsp_mode != DSPMode.DRM &&
//               chkPower.Checked)
//               chkMOX.Enabled = !_rx_only;
//           chkTUN.Enabled = !_rx_only;
//           chk2TONE.Enabled = !_rx_only; // MW0LGE_21a
//           chkVOX.Enabled = !_rx_only;
//           if (_rx_only && chkMOX.Checked)
//               chkMOX.Checked = false;
//
//           if (!IsSetupFormNull)
//           {
//               if (SetupForm.RXOnly != _rx_only)
//                   SetupForm.RXOnly = _rx_only;
//           }
//       }
//   }
// The button enables are the window's (TxApplet, the container buttons,
// RadioModel::receiveOnlyDisablesMoxButton); Setup follows
// RadioModel::rxOnlyChanged. Here: the PollPTT gate (console.cs:25470),
// the refusal of every other key (setMox, console.cs:29378), and the unkey.
// CAT and TCI requests are dropped as under TX inhibit (Task 7 follow-up,
// N3), so nothing keys later without a new request.
// ---------------------------------------------------------------------------
QString MoxController::defaultRxOnlyReason()
{
    return QStringLiteral("Receive Only is on, so this radio does not transmit. "
                          "Turn it off under Setup > General > Options.");
}

void MoxController::setRxOnly(bool on, const QString& reason)
{
    const QString before = transmitBlockReason();
    m_rxOnly = on;
    m_rxOnlyReason = reason.isEmpty() ? defaultRxOnlyReason() : reason;
    emitTransmitBlockIfChanged(before);
    if (on) {
        dropAppLevelsUnderBlock();
    }
    if (on && m_mox) {
        setMox(false);
    }
    // RADE end-of-over callsigns: a block ends a running tail at once too.
    // G-05: and the send ring's wait.
    if (on) {
        abortEndOfOverTail();
        abortSendRingWait();
    }
}

// ---------------------------------------------------------------------------
// setRadioLinkDown: TX safety (2026-09-30).
//
// From Thetis console.cs:27488-27493 [v2.10.3.15], which runs when loss of
// sync powers the radio off (console.cs:21339-21340, cited at
// RadioModel::onConnectionStateChanged), just after the CW form stop
// (console.cs:27486, not part of this gate):
//   m_frmCWXForm.StopEverything(chkPower.Checked); //[2.10.3]MW0LGE
//   chkMOX.Checked = false;
//   chkMOX.Enabled = false;
//   chkTUN.Checked = false;
//   chkTUN.Enabled = false;
//   chk2TONE.Checked = false;  // MW0LGE_21a
//   chk2TONE.Enabled = false;
// Thetis greys the buttons, so no key reaches the radio until the operator
// powers it on again. NereusSDR's P1 connection reconnects by itself, so
// the gate is here, where every key passes, and it lifts when the link is
// back. It works as the other gates do: the refusal, the PollPTT skip, the
// CAT and TCI drop and the unkey.
// ---------------------------------------------------------------------------
void MoxController::setRadioLinkDown(bool on)
{
    const QString before = transmitBlockReason();
    m_radioLinkDown = on;
    emitTransmitBlockIfChanged(before);
    if (on) {
        dropAppLevelsUnderBlock();
    }
    if (on && m_mox) {
        setMox(false);
    }
    if (on) {
        abortEndOfOverTail();
        abortSendRingWait();
    }
}

// ---------------------------------------------------------------------------
// onMoxButton: the MOX button.
//
// Receiver and transmit gaps plan, Task 7. From Thetis chkMOX_Click,
// console.cs:29730-29747 [v2.10.3.15], which runs after the CheckedChanged
// that keys or unkeys:
//   if (chkMOX.Checked)			// because the CheckedChanged event fires first
//   {
//       _manual_mox = true;
//       ... (CW firmware keyer PTT out: 3M-2)
//   }
//   else
//   {
//       _manual_mox = false;
//       if (chkTUN.Checked)
//           chkTUN.Checked = false;
//       if (chk2TONE.Checked) //MW0LGE_21a
//           chk2TONE.Checked = false;
//   }
// The manual key is set before setMox(true), as setTune does, so the walk's
// subscribers and any source event during it see it; Thetis cannot poll
// inside the synchronous CheckedChanged, so the order makes no difference
// there. The TUN and two-tone part lives in RadioModel::setMoxFromButton.
// ---------------------------------------------------------------------------
void MoxController::onMoxButton(bool on)
{
    if (on) {
        m_manualKey = true;
        setMox(true);
        // A refused key leaves chkMOX unchecked, so chkMOX_Click takes its
        // else branch: _manual_mox = false. Task 34: so does a key refused
        // while another device's key is on (MOX stays on, not the
        // station's).
        if (!m_mox || !m_currentKeyer.isStation()) {
            setManualKey(false);
        }
    } else {
        // Task 34 (ruling 8.5): the station's MOX button releases only the
        // station device's own key.
        if (m_currentKeyer.isStation()) {
            setMox(false);
        }
        setManualKey(false);
    }
}

// ---------------------------------------------------------------------------
// setManualKey: Thetis console.ManualMox.
//
// From Thetis console.cs:10668-10672 [v2.10.3.15]:
//   public bool ManualMox { get { return _manual_mox; } set { _manual_mox = value; } }
// Clearing it runs one PollPTT pass, as Thetis's next poll would.
// ---------------------------------------------------------------------------
void MoxController::setManualKey(bool on)
{
    if (m_manualKey == on) {
        return;
    }
    m_manualKey = on;
    if (!on) {
        pollPtt();
    }
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
    m_txDrainTimeoutTimer.stop();
    m_waitingForTxDrain = false;
    // G-05: a new key during the send ring's wait ends the wait.
    m_sendRingPollTimer.stop();
    m_sendRingCeilingTimer.stop();
    m_waitingForSendRing = false;
    // RADE end-of-over callsigns: a new key during a tail ends the tail.
    m_endOfOverTailTimer.stop();
    if (m_waitingForEndOfOverTail) {
        m_waitingForEndOfOverTail = false;
        emit endOfOverTailChanged(false);
    }
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
    // ── C.3: Post signal (multicast, after timer walk completes) ─────────
    // From Thetis console.cs:29677 [v2.10.3.13]:
    //   if (bOldMox != tx) MoxChangeHandlers?.Invoke(rx2_enabled && VFOBTX ? 2 : 1, bOldMox, tx); // MW0LGE_21a
    // RX→TX direction: by construction we got here because setMox(true)
    // entered the walk past its idempotent guard, so bOldMox=false, tx=true.
    emit moxChanged(activeRxForTx(), false, true);      // MW0LGE_21a — Post
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
// Upstream tags preserved: //MW0LGE (from cited upstream lines) [v2.10.3.15]
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
    // DONE_WITH_CONCERNS [anan-g2e F2/F3]: When UpdateAAudioMixerStates is ported,
    // ANAN_G2E must join the HERMES 4-DDC (USB) group at console.cs:27653-27664
    // [v2.10.3.15] (F2) AND the HERMES 2-DDC (ETH) group at console.cs:27669-27679
    // [v2.10.3.15] (F3). //N1GP G2E added tags are on both cite lines in Thetis.
    emit txaFlushed();                                  // TX→RX phase 3 of 5
    // Task 33: UpdateDDCs + AudioMOXChanged(false) + HdwMOXChanged(false)
    // follow mox_delay in Thetis (console.cs:29670-29675 [v2.10.3.15]);
    // hardwareFlipped(false) carries them (ReceiverManager::setMox,
    // RadioModel::onMoxHardwareFlipped).
    emit hardwareFlipped(false);                        // TX→RX phase 4 of 5
    advanceState(MoxState::TxToRxFlush);
    m_pttOutDelayTimer.start();
}

// onPttOutElapsed — fires after ptt_out_delay (20ms) on TX→RX path.
//
// From Thetis console.cs:29627-29628 [v2.10.3.13]:
// Upstream tags preserved: //MW0LGE (from cited console.cs:29627) [v2.10.3.15]
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
    // ── C.3: Post signal (multicast, after timer walk completes) ─────────
    // From Thetis console.cs:29677 [v2.10.3.13]:
    //   if (bOldMox != tx) MoxChangeHandlers?.Invoke(rx2_enabled && VFOBTX ? 2 : 1, bOldMox, tx); // MW0LGE_21a
    // TX→RX direction: by construction we got here via setMox(false) past
    // the idempotent guard, so bOldMox=true, tx=false.
    emit moxChanged(activeRxForTx(), true, false);      // MW0LGE_21a — Post
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

// ===========================================================================
// H.3 — VOX hang-time + anti-VOX gain + anti-VOX source path
//
// Porting from Thetis Project Files/Source/Console/setup.cs [v2.10.3.13]:
//
//   udDEXPHold_ValueChanged (setup.cs:18896-18900 [v2.10.3.13]):
//     cmaster.SetDEXPHoldTime(0, (double)udDEXPHold.Value / 1000.0);
//
//   udAntiVoxGain_ValueChanged (setup.cs:18986-18990 [v2.10.3.13]):
//     cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
//
// Porting from Thetis Project Files/Source/Console/cmaster.cs:912-943 [v2.10.3.13]:
//
//   public static void CMSetAntiVoxSourceWhat()
//   {
//       bool VACEn = Audio.console.VACEnabled;
//       bool VAC2En = Audio.console.VAC2Enabled;
//       bool useVAC = Audio.AntiVOXSourceVAC;
//       int RX1 = WDSP.id(0, 0);
//       int RX1S = WDSP.id(0, 1);
//       int RX2 = WDSP.id(2, 0);
//       if (useVAC)   // use VAC audio
//       {
//           if (VACEn)
//           {
//               cmaster.SetAntiVOXSourceWhat(0, RX1,  1);
//               cmaster.SetAntiVOXSourceWhat(0, RX1S, 1);
//           }
//           else
//           {
//               cmaster.SetAntiVOXSourceWhat(0, RX1,  0);
//               cmaster.SetAntiVOXSourceWhat(0, RX1S, 0);
//           }
//           if (VAC2En)
//               cmaster.SetAntiVOXSourceWhat(0, RX2,  1);
//           else
//               cmaster.SetAntiVOXSourceWhat(0, RX2,  0);
//       }
//       else         // use audio going to hardware minus MON
//       {
//           cmaster.SetAntiVOXSourceWhat(0, RX1,  1);
//           cmaster.SetAntiVOXSourceWhat(0, RX1S, 1);
//           cmaster.SetAntiVOXSourceWhat(0, RX2,  1);
//       }
//   }
//
// The useVAC=true path depends on VAX (VAC) state machine integration that
// is deferred to 3M-3a. Only the useVAC=false path is ported in H.3.
// In 3M-1b single-TX layout, the RadioModel lambda collapses the three-slot
// iteration (RX1/RX1S/RX2) to TxChannel::setAntiVoxRun(true). The full
// per-WDSP-channel SetAntiVOXSourceWhat iteration is a 3F multi-pan concern.
// ===========================================================================

// ---------------------------------------------------------------------------
// recomputeVoxHangTime — emit voxHangTimeRequested if converted value changed.
//
// Converts m_voxHangTimeMs → seconds (/ 1000.0).
// NAN sentinel forces first-call emit to prime WDSP.
//
// From Thetis Project Files/Source/Console/setup.cs:18899 [v2.10.3.13]:
//   cmaster.SetDEXPHoldTime(0, (double)udDEXPHold.Value / 1000.0);
// ---------------------------------------------------------------------------
void MoxController::recomputeVoxHangTime()
{
    // From Thetis Project Files/Source/Console/setup.cs:18899 [v2.10.3.13]
    const double seconds = static_cast<double>(m_voxHangTimeMs) / 1000.0;

    // NAN sentinel: first call always emits (primes WDSP regardless of value).
    if (!std::isnan(m_lastVoxHangTimeEmitted)
        && qFuzzyCompare(seconds, m_lastVoxHangTimeEmitted)) {
        return;  // idempotent on emitted double; no spurious emit
    }

    m_lastVoxHangTimeEmitted = seconds;
    emit voxHangTimeRequested(seconds);
}

// ---------------------------------------------------------------------------
// setVoxHangTime — set hang time in ms and recompute.
//
// From Thetis Project Files/Source/Console/setup.cs:18896-18900 [v2.10.3.13]:
//   cmaster.SetDEXPHoldTime(0, (double)udDEXPHold.Value / 1000.0);
// Wired by RadioModel H.3: TransmitModel::voxHangTimeMsChanged → setVoxHangTime.
// ---------------------------------------------------------------------------
void MoxController::setVoxHangTime(int ms)
{
    m_voxHangTimeMs = ms;
    recomputeVoxHangTime();
}

// ---------------------------------------------------------------------------
// recomputeAntiVoxGain — emit antiVoxGainRequested if converted value changed.
//
// Applies dB→linear conversion: pow(10.0, dB / 20.0) — voltage amplitude
// scaling (same /20.0 divisor as the Thetis callsite).
//
// From Thetis Project Files/Source/Console/setup.cs:18989 [v2.10.3.13]:
//   cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
// ---------------------------------------------------------------------------
void MoxController::recomputeAntiVoxGain()
{
    // From Thetis Project Files/Source/Console/setup.cs:18989 [v2.10.3.13]
    // Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0)
    const double gain = std::pow(10.0, static_cast<double>(m_antiVoxGainDb) / 20.0);

    // NAN sentinel: first call always emits (primes WDSP regardless of value).
    if (!std::isnan(m_lastAntiVoxGainEmitted)
        && qFuzzyCompare(gain, m_lastAntiVoxGainEmitted)) {
        return;  // idempotent on emitted double; no spurious emit
    }

    m_lastAntiVoxGainEmitted = gain;
    emit antiVoxGainRequested(gain);
}

// ---------------------------------------------------------------------------
// setAntiVoxGain — set anti-VOX gain in dB and recompute.
//
// From Thetis Project Files/Source/Console/setup.cs:18986-18990 [v2.10.3.13]:
//   cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
// Wired by RadioModel H.3: TransmitModel::antiVoxGainDbChanged → setAntiVoxGain.
// ---------------------------------------------------------------------------
void MoxController::setAntiVoxGain(int dB)
{
    m_antiVoxGainDb = dB;
    recomputeAntiVoxGain();
}

// ---------------------------------------------------------------------------
// setAntiVoxTau — set anti-VOX detector smoothing time-constant in ms.
//
// Mirrors udAntiVoxTau_ValueChanged from Thetis
// Project Files/Source/Console/setup.cs:18992-18996 [v2.10.3.13]:
//   private void udAntiVoxTau_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetAntiVOXDetectorTau(0, (double)udAntiVoxTau.Value / 1000.0);
//   }
//
// Range [1, 500] ms from setup.designer.cs:44661-44688 [v2.10.3.13].
// Defensive clamp here repeats the work TransmitModel::setAntiVoxTauMs already
// does, but stays robust against accidental out-of-range emits.
//
// NaN sentinel m_lastAntiVoxTauEmitted forces first-call emit so WDSP DEXP
// block is primed at startup.
//
// Wired by RadioModel H.3 (Phase 3M-3a-iv Task 9):
//   TransmitModel::antiVoxTauMsChanged → MoxController::setAntiVoxTau
//   MoxController::antiVoxDetectorTauRequested → TxWorkerThread::setAntiVoxDetectorTau
// ---------------------------------------------------------------------------
void MoxController::setAntiVoxTau(int ms)
{
    // Defensive clamp — primary clamp lives at TransmitModel::setAntiVoxTauMs.
    // Range from setup.designer.cs:44666-44680 [v2.10.3.13]: Min=1, Max=500.
    const int clamped = std::clamp(ms, 1, 500);

    // ms→seconds conversion: setup.cs:18995 [v2.10.3.13]
    //   (double)udAntiVoxTau.Value / 1000.0
    const double seconds = static_cast<double>(clamped) / 1000.0;

    // NaN sentinel: first call always emits (primes WDSP DEXP regardless of value).
    if (!std::isnan(m_lastAntiVoxTauEmitted)
        && seconds == m_lastAntiVoxTauEmitted) {
        return;  // idempotent on emitted double; no spurious emit
    }

    m_antiVoxTauMs = clamped;
    m_lastAntiVoxTauEmitted = seconds;
    emit antiVoxDetectorTauRequested(seconds);
}

// ---------------------------------------------------------------------------
// 3M-3a-iv post-bench refactor (Option A): setAntiVoxSourceVax body removed
// alongside the antiVoxSourceWhatRequested signal and m_antiVoxSourceVax /
// m_antiVoxSourceVaxInitialized members.  Thetis chkAntiVoxSource (RX vs VAC
// at cmaster.cs:912-943 [v2.10.3.13]) does not map to NereusSDR's
// architecture: VAX is a digital-mode app bus with no mic-feedback path, so
// the audio output device is the only valid anti-VOX cancellation reference.
// See commit message and DexpVoxPage info-row for the architectural rationale.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// setAntiVoxRun() — 3M-3a-iv scope-expansion.
//
// Mirrors Thetis chkAntiVoxEnable_CheckedChanged at setup.cs:18980-18984
// [v2.10.3.13]:
//   private void chkAntiVoxEnable_CheckedChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetAntiVOXRun(0, chkAntiVoxEnable.Checked);
//   }
//
// 3M-3a-iv: replaces the older collapsed wiring where
// antiVoxSourceWhatRequested drove TxWorkerThread::setAntiVoxRun via a
// !useVax inversion.  The post-bench Option A refactor then dropped the
// source-selector entirely.
//
// First-call emit guard m_antiVoxRunInitialized: the very first accepted
// call always emits, even when run==false matches the default field value.
// ---------------------------------------------------------------------------
void MoxController::setAntiVoxRun(bool run)
{
    if (m_antiVoxRunInitialized && run == m_antiVoxRun) {
        return;  // idempotent guard
    }
    m_antiVoxRun = run;
    m_antiVoxRunInitialized = true;
    emit antiVoxRunRequested(run);
}

// ---------------------------------------------------------------------------
// primeWdspState — bench fix 2026-05-14 ("VOX needs juggling to prime").
//
// Resets the three NaN sentinels that guard the *Requested signals and
// re-runs the recompute helpers so the late-wired TxChannel receives the
// current values.  See the header comment block for the full motivation.
//
// Anti-VOX tau and anti-VOX run are NOT covered here because their TM ->
// MoxController connects are deferred to wireConnectionSignals
// (RadioModel.cpp:5025/5051) and the explicit re-push there
// (RadioModel.cpp:5043/5070) already lands after TxWorkerThread is wired.
// ---------------------------------------------------------------------------
void MoxController::primeWdspState()
{
    m_lastVoxThresholdEmitted = std::numeric_limits<double>::quiet_NaN();
    m_lastVoxHangTimeEmitted  = std::numeric_limits<double>::quiet_NaN();
    m_lastAntiVoxGainEmitted  = std::numeric_limits<double>::quiet_NaN();
    recomputeVoxThreshold();
    recomputeVoxHangTime();
    recomputeAntiVoxGain();
}

// ===========================================================================
// H.4 — PTT-source dispatch slots (MIC / CAT / VOX / SPACE / X2)
//
// Porting from Thetis Project Files/Source/Console/console.cs [v2.10.3.13]:
//
//   PollPTT method (console.cs:25416-25560 [v2.10.3.13]) — the PTT poll
//   loop that reads hardware PTT state and sets _current_ptt_mode before
//   asserting chkMOX.Checked = true.  The dispatch ordering is:
//     TCI  → _current_ptt_mode = PTTMode.TCI   (console.cs:25463)
//     CAT  → _current_ptt_mode = PTTMode.CAT   (console.cs:25469)
//     CW   → _current_ptt_mode = PTTMode.CW    (console.cs:25475)
//     MIC  → _current_ptt_mode = PTTMode.MIC   (console.cs:25492)
//     VOX  → _current_ptt_mode = PTTMode.VOX   (console.cs:25507)
//
//   Console_KeyDown case Keys.Space (console.cs:26672-26700 [v2.10.3.13]):
//     SPACE → _current_ptt_mode = PTTMode.SPACE (console.cs:26680)
//
//   PTTMode.X2 value defined in enums.cs:353 [v2.10.3.13]; the X2 dispatch
//   path is present in Thetis network-level status decoding but not in the
//   PollPTT loop directly.  NereusSDR pre-wires the slot for parity.
//
// PollPTT SOURCES (mic, CAT, VOX, TCI; receiver and transmit gaps plan,
// Task 7): each slot records its level and runs pollPtt(), one pass of
// Thetis PollPTT. See pollPtt() for the rules and what is not ported, and
// dropPttOnUnkey() for the unkey (setMox(false) clears the PTT mode itself;
// the RadioModel hardwareFlipped(false) subscriber the H.4 note named never
// existed).
//
// SPACE and X2 keep the H.4 pattern: set the mode on press, then
// setMox(pressed).
//
// REJECTION PATTERN (CW):
//   qCWarning(lcDsp) << "... rejected, deferred to 3M-2";
//   return;  — no setMox(), no setPttMode() update
// ===========================================================================

// ---------------------------------------------------------------------------
// onMicPttFromRadio — MIC PTT from radio hardware.
//
// Porting from Thetis Project Files/Source/Console/console.cs [v2.10.3.13]:
//   PollPTT: bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
//   _current_ptt_mode = PTTMode.MIC;  console.cs:25492 [v2.10.3.13]
//   chkMOX.Checked = true;            console.cs:25494 [v2.10.3.13]
//
// RadioConnection::micPttFromRadio calls this on every P1/P2 status frame.
// ---------------------------------------------------------------------------
void MoxController::onMicPttFromRadio(bool pressed)
{
    // From Thetis console.cs:25472 [v2.10.3.15]:
    //   bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
    // (the next line, cw_ptt, carries: //[2.10.3.9]MW0LGE only want to do this on semi breakin  [original inline comment from console.cs:25473])
    // The level is recorded and one PollPTT pass decides: a press keys with
    // PTTMode.MIC from receive (console.cs:25526-25541), a release unkeys
    // only in PTTMode.MIC (console.cs:25589-25595), and neither does
    // anything during a manual key. Receiver and transmit gaps plan, Task 7.
    const bool pressEdge = pressed && !m_micPtt;
    m_micPtt = pressed;
    if (!pressed) {
        m_micTakePressDown = false;   // Task 77 fix round 2
        clearHeldBits(kRefusedMic);   // M3 and R-R3-36: a new press
    }
    // iPhone app plan Task 77 (rulings 8.8, 8.9): while another device
    // holds transmit, whatever its key (tx.key, TUNE, two-tone, a Tuner
    // Genius autotune, VOX), its press edge asks the gate here: PollPTT
    // never reaches the mic then (a station source never rides another
    // keyer's key, and a manual key skips the whole pass). A press takes
    // transmit, the transfer unkeying that device first. The press keys
    // nothing now; it is held off until the take ends (onTakeFinished) or
    // it is released, so a press that took nothing never keys later
    // without a fresh one.
    // Fix wave I1: asked on the holder (TransmitHolder's state), not on
    // who is keyed. TX inhibit, the PA trip and receive only still take
    // nothing (fix wave M1). A holder on the air is taken from even when
    // the band plan or the interlock would refuse the station's own key
    // (ruling 8.9: the press stops the device first; PollPTT retries the
    // station's key after the take); an unkeyed holder is taken from only
    // when nothing before the gate refuses, as in tryPollKey.
    const bool otherDeviceHolds = m_otherDeviceHolds
                                      ? m_otherDeviceHolds()
                                      : (m_mox && !m_currentKeyer.isStation() && !m_manualKey);
    const bool onAir = m_mox || m_manualKey || m_state != MoxState::Rx;
    if (pressEdge && otherDeviceHolds && m_keyingGate
        && (onAir || refusalBeforeTheGate().isEmpty())) {
        if (transmitBlocked()) {
            const TxRefusal blocked = transmitBlockRefusal();
            reportRefusal(blocked.text, blocked, /*quiet=*/false);
        } else {
            const KeyingAnswer answer =
                m_keyingGate(PttMode::Mic, KeyerIdentity::station(PttMode::Mic));
            if (answer.verdict == KeyingVerdict::Refuse) {
                reportRefusal(answer.refusal.text, answer.refusal, /*quiet=*/false);
            }
            // Task 77 fix round 2: only the press that took keys once the
            // take ends; one refused while a take runs ("Transmit is
            // changing hands. Try again in a moment.") keys nothing later.
            m_micTakePressDown = answer.verdict == KeyingVerdict::Take;
        }
        if (m_micPtt) {
            m_refusedHeld |= kRefusedMic;
            m_notQueuedHeld |= kRefusedMic;
        }
        return;
    }
    pollPtt();
}

TxRefusal MoxController::programKeyRefusal(const KeyerIdentity& keyer) const
{
    if (!m_keyingGate || !keyer.program) {
        return {};
    }
    const KeyingAnswer answer = m_keyingGate(keyer.source, keyer);
    if (answer.verdict == KeyingVerdict::Admit) {
        return {};
    }
    return answer.refusal.isEmpty() ? TxRefusals::changingHands() : answer.refusal;
}

void MoxController::holdOffHeldMic()
{
    // iPhone app plan Task 77 (ruling 8.9): a PTT still held after another
    // device takes transmit back does not take it again; the next press
    // does. Held off until released (clearHeldBits on the release).
    if (m_micPtt) {
        m_refusedHeld |= kRefusedMic;
        m_notQueuedHeld |= kRefusedMic;
    }
}

// ---------------------------------------------------------------------------
// onCatPtt — CAT (computer-aided transceiver) PTT command.
//
// Porting from Thetis Project Files/Source/Console/console.cs [v2.10.3.13]:
//   PollPTT: bool cat_ptt = (_ptt_bit_bang_enabled && ...) | _cat_ptt;
//   _current_ptt_mode = PTTMode.CAT;  console.cs:25469 [v2.10.3.13]
//   chkMOX.Checked = true;            console.cs:25471 [v2.10.3.13]
//
// Full CAT integration is Phase 3K.  Wiring deferred to 3K.
// ---------------------------------------------------------------------------
void MoxController::onCatPtt(bool pressed)
{
    // From Thetis console.cs:25476-25477 [v2.10.3.15]:
    // (nearby: //[2.10.3.9]MW0LGE only want to do this on semi breakin  [original inline comment from console.cs:25473];
    //  //[2.10.3.6]MWLGE fixes #518  [original inline comment from console.cs:25481])
    //   bool cat_ptt = (_ptt_bit_bang_enabled && serialPTT != null && serialPTT.isPTT()) | // CAT serial PTT
    //                  (!_ptt_bit_bang_enabled && CWInput.CATPTT) | _cat_ptt;
    // Keys with PTTMode.CAT from receive (console.cs:25513-25517); a release
    // unkeys only in PTTMode.CAT (console.cs:25582-25588).
    // Task 7 follow-up, N3: refused, not kept, while TX inhibit or a PA
    // trip holds (see dropAppLevelsUnderBlock). Silent, as PollPTT's gate
    // is for every source.
    if (pressed && transmitBlocked()) {   // Task 16: and receive only
        qCInfo(lcDsp) << "MoxController: CAT PTT refused while transmit is blocked";
        return;
    }
    // Task 7 follow-up, N2: a rising edge is a new press too. A refusal
    // drops the CAT level (dropPttOnUnkey) and then marks it refused, so a
    // second request with no release between would otherwise be refused
    // without telling the operator.
    if (!pressed || !m_catPtt) {
        clearHeldBits(kRefusedCat);   // M3 and R-R3-36: a new press
    }
    m_catPtt = pressed;
    pollPtt();
    if (!pressed) {
        // Task 77 fix round 3: clearHeldBits ran with the level still set,
        // so a release that never keyed is reported here.
        reportIfSourcesReleased();
    }
}

// ---------------------------------------------------------------------------
// onVoxActive — WDSP VOX activity (DEXP gate crossing).
//
// Porting from Thetis Project Files/Source/Console/console.cs [v2.10.3.13]:
//   PollPTT: bool vox_ptt = vox_ok && Audio.VOXActive;
//   _current_ptt_mode = PTTMode.VOX;  console.cs:25507 [v2.10.3.13]
//   chkMOX.Checked = true;            console.cs:25508 [v2.10.3.13]
//
// TxChannel::voxActiveChanged (the DEXP pushvox callback) calls this.
// ---------------------------------------------------------------------------
void MoxController::onVoxActive(bool active)
{
    // From Thetis console.cs:25475 [v2.10.3.15]:
    //   bool vox_ptt = vox_ok && Audio.VOXActive;
    // (cw_ptt, two lines above it, carries: //[2.10.3.9]MW0LGE only want to do this on semi breakin  [original inline comment from console.cs:25473])
    // Keys with PTTMode.VOX from receive in the voice modes
    // (console.cs:25543-25555); a release unkeys only in PTTMode.VOX
    // (console.cs:25603-25608). vox_ok (mic mute / VAC bypass) is not
    // ported: VAX is not VAC.
    m_voxPtt = active;
    if (!active) {
        clearHeldBits(kRefusedVox);   // M3 and R-R3-36: a new press
    }
    pollPtt();
}

// ---------------------------------------------------------------------------
// onSpacePtt — spacebar PTT from the keyboard handler.
//
// Porting from Thetis Project Files/Source/Console/console.cs [v2.10.3.13]:
//   Console_KeyDown case Keys.Space (spacebar_ptt branch):
//   _current_ptt_mode = PTTMode.SPACE;   console.cs:26680 [v2.10.3.13]
//   chkMOX.Checked = !chkMOX.Checked;   console.cs:26681 [v2.10.3.13]
//
// Note: Thetis uses a toggle (!chkMOX.Checked) for spacebar PTT, not a
// direct press/release.  NereusSDR uses a press/release bool (passed by
// the keyboard handler at the call site) for cleaner state semantics.
// The caller converts the Thetis toggle pattern to press/release before
// calling this slot.  Wiring deferred to 3M-3a (UI keyboard handler).
// ---------------------------------------------------------------------------
void MoxController::onSpacePtt(bool pressed)
{
    // From Thetis console.cs:26680 [v2.10.3.13]: _current_ptt_mode = PTTMode.SPACE;
    if (pressed) {
        setPttMode(PttMode::Space);
    }
    // From Thetis console.cs:26681 [v2.10.3.13]: chkMOX.Checked = !chkMOX.Checked;
    setMox(pressed);
}

// ---------------------------------------------------------------------------
// onX2Ptt — X2 jack external PTT trigger.
//
// PttMode::X2 is defined in Thetis enums.cs:353 [v2.10.3.13].
// The X2 PTT dispatch path exists at the network level in Thetis but is
// not directly in the PollPTT loop.  NereusSDR pre-wires the slot for parity.
// Wiring deferred to 3M-3a or later when X2 status-frame parsing lands.
// ---------------------------------------------------------------------------
void MoxController::onX2Ptt(bool pressed)
{
    // PTTMode::X2 per enums.cs:353 [v2.10.3.13]
    if (pressed) {
        setPttMode(PttMode::X2);
    }
    setMox(pressed);
}

// ---------------------------------------------------------------------------
// onCwPtt — CW keyer PTT — REJECTED (deferred to 3M-2).
//
// Porting from Thetis Project Files/Source/Console/console.cs [v2.10.3.13]:
//   PollPTT: bool cw_ptt = CWInput.KeyerPTT && ...;
//   _current_ptt_mode = PTTMode.CW;  console.cs:25475 [v2.10.3.13]
//
// 3M-2 will implement the CW keyer, sidetone, and QSK/break-in state
// machine.  This slot rejects the call to prevent accidental CW MOX
// assertion before the full CW infrastructure is ready.
// Rejection follows the qCWarning-and-return shape (formerly modeled on
// setAntiVoxSourceVax(true) from H.3, removed in 3M-3a-iv).
// ---------------------------------------------------------------------------
void MoxController::onCwPtt(bool /*pressed*/)
{
    qCWarning(lcDsp) << "MoxController::onCwPtt rejected —"
                        " CW keyer PTT is deferred to 3M-2."
                        " No MOX state change.";
    return;
}

// ---------------------------------------------------------------------------
// onTciPtt: TCI trx, the console side of Thetis TCIPTT.
//
// From Thetis console.cs:2456-2466 [v2.10.3.15] (TCIPTT setter):
//   _tci_ptt = value && !_disable_ptt; // only use when we allow ptt control
// PollPTT keys with PTTMode.TCI from receive (console.cs:25507-25511); a
// release in PTTMode.TCI falls back to a still-held source or unkeys
// (console.cs:25562-25581). _disable_ptt has no NereusSDR equivalent.
// RadioModel::setMox, the shim TciProtocol invokes for trx, calls this.
// ---------------------------------------------------------------------------
void MoxController::onTciPtt(bool pressed)
{
    // Task 7 follow-up, N3: refused, not kept, while TX inhibit or a PA
    // trip holds (see dropAppLevelsUnderBlock); the app is answered
    // trx:N,false.
    if (pressed && transmitBlocked()) {   // Task 16: and receive only
        qCInfo(lcDsp) << "MoxController: TCI PTT refused while transmit is blocked";
        return;
    }
    // Task 7 follow-up, N2: a rising edge is a new press too (see
    // onCatPtt): an app's second trx:N,true after a refusal is told again.
    if (!pressed || !m_tciPtt) {
        clearHeldBits(kRefusedTci);   // M3 and R-R3-36: a new press
    }
    m_tciPtt = pressed;
    pollPtt();
    if (!pressed) {
        // Task 77 fix round 3: as onCatPtt.
        reportIfSourcesReleased();
    }
}

} // namespace NereusSDR

