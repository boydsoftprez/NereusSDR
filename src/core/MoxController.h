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
//   cmaster.cs:1054-1059 [v2.10.3.13] — CMSetTXAVoxThresh: mic-boost-aware
//     scaling applied to VOX attack threshold before SetDEXPAttackThreshold.
//   setup.cs:18908-18912 [v2.10.3.13] — udDEXPThreshold_ValueChanged:
//     dB-to-linear conversion (Math.Pow(10.0, dB / 20.0)) before calling
//     CMSetTXAVoxThresh; confirms thresh parameter is linear amplitude.
//   setup.cs:18896-18900 [v2.10.3.13] — udDEXPHold_ValueChanged:
//     ms→seconds conversion (Value / 1000.0) before SetDEXPHoldTime.
//   setup.cs:18986-18990 [v2.10.3.13] — udAntiVoxGain_ValueChanged:
//     dB-to-linear conversion (Math.Pow(10.0, dB / 20.0)) before calling
//     SetAntiVOXGain; confirms voltage-amplitude scaling (/20.0).
//   cmaster.cs:912-943 [v2.10.3.13] — CMSetAntiVoxSourceWhat:
//     when useVAC==false (lines 937-942), all RX slots (RX1, RX1S, RX2)
//     get source=1 (local-RX audio for antivox reference).
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
//   2026-04-28 — Phase 3M-1b Task H.1 — setVoxEnabled(bool) and
//                 onModeChanged(DSPMode) slots added with voice-family
//                 mode-gate. voxRunRequested(bool) phase signal added.
//                 Internal recomputeVoxRun() emits idempotently on the
//                 gated value (voxEnabled && isVoiceMode(currentMode)).
//                 Ports CMSetTXAVoxRun logic from
//                 cmaster.cs:1039-1052 [v2.10.3.13].
//   2026-04-28 — Phase 3M-1b Task H.2 — setVoxThreshold(int dB),
//                 onMicBoostChanged(bool), setVoxGainScalar(float) slots
//                 added. voxThresholdRequested(double) phase signal added.
//                 Internal computeScaledThreshold() / recomputeVoxThreshold()
//                 helpers port the Thetis mic-boost-aware scaling from
//                 CMSetTXAVoxThresh (cmaster.cs:1054-1059 [v2.10.3.13])
//                 with the dB→linear conversion from
//                 udDEXPThreshold_ValueChanged (setup.cs:18911 [v2.10.3.13]).
//   2026-04-28 — Phase 3M-1b Task H.3 — setVoxHangTime(int ms),
//                 setAntiVoxGain(int dB) slots added.  Originally also
//                 added setAntiVoxSourceVax(bool) and antiVoxSourceWhatRequested,
//                 both removed by 3M-3a-iv post-bench refactor (see 2026-05-07
//                 entry below).  Ports ms→seconds conversion for SetDEXPHoldTime
//                 (setup.cs:18899 [v2.10.3.13]) and dB→linear gain conversion
//                 for SetAntiVOXGain (setup.cs:18989 [v2.10.3.13], /20.0
//                 voltage scaling).
//   2026-04-28 — Phase 3M-1b Task K.2 — moxRejected(QString) signal added.
//                 setMoxCheck(MoxCheckFn) installs a std::function<> callback
//                 that setMox(true) consults before the existing Codex P2
//                 safety effects. On rejection, emits moxRejected(reason)
//                 and returns without state advance (early-out BEFORE Codex P2
//                 so safety effects only fire on accepted requests).
//   2026-04-28 — Phase 3M-1b Task H.4 — 7 PTT-source dispatch slots added:
//                 Accepted (5): onMicPttFromRadio, onCatPtt, onVoxActive,
//                   onSpacePtt, onX2Ptt.  Each sets the corresponding
//                   PttMode (Mic/Cat/Vox/Space/X2) before driving
//                   setMox(pressed).  PttMode is NOT cleared on
//                   setMox(false) — cleared by F.1 hardwareFlipped(false)
//                   subscribers per the F.1 contract (symmetric with
//                   setTune(false) behaviour). Ordering follows the
//                   PollPTT dispatch in console.cs:25463-25507 [v2.10.3.13].
//                 Rejected (2): onCwPtt, onTciPtt.  Log qCWarning to
//                   lcDsp and return without state mutation.  CW deferred
//                   to 3M-2; TCI deferred to 3J.  Rejection follows the
//                   qCWarning-and-return pattern (formerly modeled on
//                   setAntiVoxSourceVax(true) from H.3, which was removed
//                   in 3M-3a-iv).
//                 RadioModel: H.4 adds the MoxController API only;
//                   upstream signal sources land in later phases (H.5
//                   mic_ptt extraction, 3K CAT, 3M-3a SPACE/VOX/X2).
//   2026-04-28 — Phase 3M-1c Tasks C.2 / C.3 / C.4 — multicast Pre/Post
//                 MOX state-change signals added:
//                   moxChanging(int rx, bool oldMox, bool newMox) — Pre
//                     (Thetis MoxPreChangeHandlers, console.cs:29324
//                     [v2.10.3.13], // MW0LGE_21k8). Subscribers can
//                     defensively freeze readings before the transition.
//                   moxChanged(int rx, bool oldMox, bool newMox) — Post
//                     (Thetis MoxChangeHandlers, console.cs:29677
//                     [v2.10.3.13], // MW0LGE_21a). Parallels existing
//                     1-arg moxStateChanged(bool) — additive overload,
//                     not a replacement.
//                 The rx argument is the RECEIVER INDEX THAT OWNS THE TX
//                   PATH: (m_rx2Enabled && m_vfobTx) ? 2 : 1. RX2 alone
//                   without VFOBTX still yields rx==1 because TX comes
//                   off VFO-A. setRx2Enabled / setVfobTx public slots
//                   are added (idempotent, no-emit) for future RadioModel
//                   wiring; both default to false (NereusSDR has no RX2
//                   wired yet).
//   2026-05-07 — Phase 3M-3a-iv post-bench refactor (Option A): removed
//                 setAntiVoxSourceVax(bool) slot, antiVoxSourceWhatRequested
//                 signal, and m_antiVoxSourceVax / m_antiVoxSourceVaxInitialized
//                 members.  Thetis chkAntiVoxSource (RX vs VAC) at
//                 setup.designer.cs:44646-44657 [v2.10.3.13] does not map
//                 to NereusSDR's architecture: VAX is a digital-mode app bus
//                 with no mic-feedback path, so the audio output device is
//                 the only valid anti-VOX cancellation reference.  See
//                 commit message for full rationale.  J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 : RADE end-of-over callsigns: setEndOfOverTail,
//                 onEndOfOverTailDone, abortEndOfOverTail,
//                 endOfOverTailChanged and kEndOfOverTailMaxMs (an
//                 operator's release keeps the radio keyed, at most 1 s, for
//                 FreeDV's end-of-over frame before the TX→RX walk).
//                 NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 : isReleasing (MOX off, hardware still keyed), so the
//                 Core's stops act during the TX drain window. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 : G-05 (JJ's ruling 2026-09-28): SendRingDrain,
//                 setSendRingDrain, abortSendRingWait, kSendRingPollMs (an
//                 operator's release waits for the transmit I/Q send ring
//                 to drain, after the TX channel's drain and before
//                 mox_delay, bounded by the ring's own length).
//                 NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 : Task 33 (R-IOS-03): txDrainRequested, onTxDrained,
//                 setAwaitsTxDrain and kTxDrainTimeoutMs (the TX→RX walk
//                 drains before the hardware flip, as Thetis does);
//                 latchStopAllTx and clearManualMox for StopAllTx.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 7: keying sources
//                 follow Thetis PollPTT, chkMOX_Click and
//                 chkMOX_CheckedChanged2 (console.cs [v2.10.3.15]). The
//                 PTT-source slots record each source's level and run one
//                 PollPTT pass (pollPtt); a source keys only from receive
//                 and outside a manual key, and releases only the mode it
//                 set. setMox(false) clears the PTT mode and the CAT and
//                 TCI levels itself, as chkMOX_CheckedChanged2 does (the
//                 hardwareFlipped(false) subscriber the old F.1 note
//                 described never existed). New: isManualKey /
//                 setManualKey (Thetis _manual_mox / console.ManualMox),
//                 onMoxButton (chkMOX_Click), onTciPtt keys with
//                 PttMode::Tci and falls back as
//                 getFallbackPTTModeAfterTCIRelease does. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 7 fix wave: TX
//                 inhibit and the PA trip gate every source
//                 (setTxInhibited / setPaTripped, PollPTT's _tx_inhibit and
//                 _ganymede_pa_issue, console.cs:25470 [v2.10.3.15]).
//                 A TX-interlock refusal emits moxRejected; a held
//                 source's repeat refusal is quiet (tryPollKey,
//                 m_refusedHeld). A VOX level is dropped when VOX stops
//                 running; a TCI release that falls back runs the MOX
//                 pre-check (R-R3-36). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 7 follow-up: a
//                 held source refused because the microphone is not
//                 ready is never queued (m_notQueuedHeld, R-R3-36). A CAT
//                 or TCI rising edge is a new press (N2). CAT and TCI
//                 requests are dropped, not held, under TX inhibit or a
//                 PA trip (N3). isTciPttHeld() for TciServer's TX audio
//                 lock. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25 - iPhone app plan Task 34 (R-IOS-02, R-IOS-13, rulings
//                 8.8 and 8.13): the keying gate (setKeyingGate), asked on
//                 every press edge and every remote key before the PTT
//                 mode or MOX changes; KeyerIdentity and
//                 setMox(bool, const KeyerIdentity&); a release unkeys only
//                 its keyer's key; every refusal also as a TxRefusal
//                 (moxRefused). NereusSDR-original. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 35 (R-IOS-13): admitKey(keyer) and
//                 setTune(bool, const KeyerIdentity&), so a remote device's
//                 TUNE and two-tone ask and key as that device.
//                 NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 16: setRxOnly /
//                 isRxOnly, Thetis _rx_only (console.cs:15312-15334,
//                 25470 and 29378 [v2.10.3.15]), the third gate that
//                 refuses every key and unkeys. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Task 16 fix wave (M2): transmitBlockReason and
//                 transmitBlockChanged, so the TGXL autotune and the Tuner
//                 applet follow the gate. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
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
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               holdOffHeldMic and programKeyRefusal; the radio's mic press
//               while another device's key is on asks the keying gate (a
//               take). J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
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
//   2026-09-29: HL2 port part 2: setTxInhibited takes the reason the
//               inhibit is shown with ("I/O Board: Fault Code N" for the
//               HL2 I/O board fault, mi0bot console.cs:25876-25885
//               [@c26a8a4]); the refusal and transmitBlockReason carry it.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: TX safety: setRadioLinkDown, a fourth gate that refuses
//               every key while the link to the radio is lost, as Thetis
//               disables chkMOX, chkTUN and chk2TONE when it loses sync
//               (console.cs:27488-27493 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: TGXL tune lane (JJ's ruling): KeyerIdentity::tunerPress,
//               the Tuner Genius's own front-panel TUNE, which takes
//               transmit as the radio's PTT does (ruling 8.9);
//               lastAdmitTook() and tunerTakeFinished(). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis state-machine
// derived values are cited inline below.

#pragma once

#include <QObject>
#include <QTimer>
#include <functional>
#include <limits>
#include "core/PttMode.h"
#include "core/WdspTypes.h"
#include "core/safety/BandPlanGuard.h"
#include "core/safety/TxRefusal.h"
// Phase 3P-II Task 87: TxInterlockPolicy gate in setMox(true).
#include "core/TxInterlockPolicy.h"

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
// KeyerIdentity: who a key is for (iPhone app plan Task 34, ruling 8.13).
//
// The station device is the operating position at the radio: the radio's
// own PTT and the Core's local callers of setMox(bool) (the MOX and TUNE
// buttons, two-tone, a local TCI or CAT server). A remote device keys with
// setMox(bool, const KeyerIdentity&) and its own device id.
// ---------------------------------------------------------------------------
struct KeyerIdentity {
    /// The device the key is for: a device id as the Core's session
    /// registry knows it, or kStationDeviceId.
    QByteArray deviceId;
    /// The keying source (PttMode::Mic for the radio's own PTT, Tci, Cat,
    /// Vox, Manual for TUNE, None for a MOX button or a remote key).
    PttMode source{PttMode::None};
    /// A program's key (TCI, CAT): it never takes transmit (D58, D63).
    bool program{false};
    /// Fix wave M10: the connection a remote key came on (the session
    /// owner, "station:<id>"), so the keying gate judges that connection,
    /// not another of the same device's; empty for the station's own keys.
    /// Not part of who the keyer is (operator== leaves it out).
    QString session;
    /// TGXL tune lane (JJ's ruling, 2026-09-30): the station's tune carrier
    /// for the Tuner Genius's own front-panel TUNE (its LAN PTT `transmit
    /// tune on`, or its tuning state on a desktop's Tuner page). A press at
    /// the station: it takes transmit from another device as the radio's
    /// own PTT does (ruling 8.9). Set only by RadioModel for a cycle the
    /// tuner started; never by a device or a remote command. Not part of
    /// who the keyer is (operator== leaves it out).
    bool tunerPress{false};

    /// The same id as SliceOwnership::stationDevice().
    static constexpr char kStationDeviceId[] = "station";
    static KeyerIdentity station(PttMode source);
    bool isStation() const { return deviceId == kStationDeviceId; }
    bool operator==(const KeyerIdentity& other) const
    {
        return deviceId == other.deviceId && source == other.source && program == other.program;
    }
};

// The keying gate's answer (ruling 8.13): admit the key; refuse it with a
// reason; or take transmit first, then key if the press is still down.
enum class KeyingVerdict { Admit, Refuse, Take };
struct KeyingAnswer {
    KeyingVerdict verdict{KeyingVerdict::Admit};
    TxRefusal refusal;
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
//   TX→RX path: Tx → TxToRxInFlight (the TX drain when awaitsTxDrain,
//               then 10ms mox_delay) → TxToRxFlush (20ms, ptt_out_delay) → Rx
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
    // Task 33: the longest the TX→RX walk waits for the TX channel's drain
    // before it goes on to mox_delay. Thetis's drain blocks at most this
    // long: WDSP SetChannelState's `const int timeout = 100;` loop of
    // Sleep(1) (wdsp/channel.c SetChannelState [v2.10.3.15]). NereusSDR runs
    // the drain on the transmit lane, so the walk must not wait on a lane
    // that is busy for longer.
    static constexpr int kTxDrainTimeoutMs = 100;
    // RADE end-of-over callsigns: the longest an operator's release keeps
    // the radio on the air for an end-of-over tail. JJ's ruling (Core/GUI
    // session, 2026-09-23): the tail lasts at most 1 s. NereusSDR-original;
    // FreeDV bounds the same wait at 2 s for the EOO to queue plus 1 s for
    // the audio to drain (freedv-gui src/ongui.cpp:1479-1523 [@a4ae053]).
    static constexpr int kEndOfOverTailMaxMs = 1000;
    // G-05: how often the unkey's wait for the send ring looks at it.
    // NereusSDR-original; the same 1 ms step as WDSP SetChannelState's
    // drain loop of Sleep(1) (wdsp/channel.c SetChannelState [v2.10.3.15]).
    // The wait's bound is not a constant: it is the ring's own length.
    static constexpr int kSendRingPollMs = 1;

    // ── Getters ──────────────────────────────────────────────────────────────
    bool     isMox()      const noexcept { return m_mox; }
    MoxState state()      const noexcept { return m_state; }
    PttMode  pttMode()    const noexcept { return m_pttMode; }
    // isManualMox: true while MOX is engaged via the TUN button.
    //
    // Mirrors Thetis _manual_mox (console.cs:240 [v2.10.3.13]):
    //   "True if the MOX button was clicked on (not PTT)"
    // In NereusSDR, TUN goes through setTune() which sets this flag.
    // setMox() does NOT touch this flag. Thetis sets _manual_mox from
    // chkMOX_Click, TUN and two-tone; NereusSDR keeps this flag for the TUN
    // path only and carries the Thetis flag as isManualKey() (Task 7).
    // F.1 subscribers wanting to distinguish a TUN-triggered MOX from a
    // raw setMox(true) call should read this getter inside their
    // hardwareFlipped(bool isTx) slot. External code must not set this
    // directly — call setTune() instead.
    bool     isManualMox() const noexcept { return m_manualMox; }

    // isManualKey: Thetis _manual_mox as Thetis sets it.
    //
    // From Thetis console.cs:246 [v2.10.3.15]:
    //   private bool _manual_mox; // True if the MOX button was clicked on (not PTT)
    // Thetis sets it from three places: the MOX button (chkMOX_Click,
    // console.cs:29730-29747), TUN (console.cs:30145, cleared at 30193) and
    // two-tone (console.ManualMox, setup.cs:11162 and 11193). While it is
    // set, PollPTT does nothing (console.cs:25470): no source keys, and no
    // source's release unkeys.
    //
    // NereusSDR keeps isManualMox() for the TUN button alone (the TUNE
    // button, PGXL/TGXL and the tuner applet read it that way), so the
    // Thetis flag lives here under its own name. setTune(true) sets both.
    bool     isManualKey() const noexcept { return m_manualKey; }

    // isTxInhibited / isPaTripped: the two PollPTT gates that block every
    // keying source (Task 7 fix wave, I2). See setTxInhibited / setPaTripped.
    bool     isTxInhibited() const noexcept { return m_txInhibited; }
    bool     isPaTripped()   const noexcept { return m_paTripped; }
    // isRxOnly: Thetis _rx_only, the third PollPTT gate (Task 16). See
    // setRxOnly.
    bool     isRxOnly()      const noexcept { return m_rxOnly; }

    // isTciPttHeld: Thetis _tci_ptt, the level an app's trx last left
    // (onTciPtt). False after a refused trx (the refusal drops it) and
    // under TX inhibit or a PA trip; true while a trx:N,true is held off by
    // a manual key. TciServer reads it after a trx to give back the TX
    // audio lock when the trx keyed nothing (Task 7 follow-up, item 5).
    bool     isTciPttHeld()  const noexcept { return m_tciPtt; }

    // iPhone app plan Task 77 fix round 2: a PTT source (mic, CAT, TCI, or
    // VOX triggering) is down now, keyed or held off. The Core never
    // switches the Power Genius while one is: a key may be about to start.
    bool     anyPttSourceHeld() const noexcept
    {
        return m_micPtt || m_catPtt || m_voxPtt || m_tciPtt;
    }

    // ── K.2: MOX pre-check callback ──────────────────────────────────────────
    //
    // setMoxCheck: install a BandPlanGuard check callback for setMox(true).
    //
    // When set, setMox(true) calls m_moxCheck() BEFORE the Codex P2 safety
    // effects. If the result.ok == false, emits moxRejected(result.reason)
    // and returns immediately without running safety effects or advancing state.
    // setMox(false) NEVER consults the callback — the release path is always
    // unconditional.
    //
    // The callback is a std::function<> that closes over RadioModel state
    // (current region, freq, mode, bands, flags). RadioModel installs it once
    // at construction via the lambda pattern:
    //   m_moxController->setMoxCheck([this]() -> safety::BandPlanGuard::MoxCheckResult {
    //       return m_bandPlan.checkMoxAllowed(...);
    //   });
    //
    // If no callback is installed (nullptr), setMox(true) proceeds as before
    // (backwards-compatible default — no rejection possible).
    //
    // Thread safety: setMoxCheck must be called from the main thread before
    // any setMox() call, consistent with MoxController's main-thread-only
    // contract.
    using MoxCheckFn = std::function<safety::BandPlanGuard::MoxCheckResult()>;
    void setMoxCheck(MoxCheckFn check);

    // ── iPhone app plan Task 34: the keying gate (rulings 8.8, 8.13) ────────
    //
    // setKeyingGate: asked on every press edge and every remote key, with
    // the source and the keyer, before the PTT mode or MOX changes. Fix
    // wave M1: it is asked last, after every check that refuses a key
    // without deciding who holds transmit: TX inhibit, the PA trip and
    // receive only, then the band plan (and the microphone check), then the
    // interlock. A key those refuse never reaches the gate, so it takes
    // nothing.
    // A Core that serves devices installs it (StationServer, from
    // TransmitHolder); with none installed every key is admitted as before,
    // so a desktop on its own is unchanged. Unkeying is never asked.
    //
    // A PTT source (mic, CAT, VOX, TCI) asks at its press edge in
    // tryPollKey, before its mode is set; a refused or taken press is held
    // off until the source is released, so a level every status frame
    // repeats acts once per edge. setMox(true) asks for the local callers
    // (the station device); setMox(true, keyer) for a remote key.
    using KeyingGateFn = std::function<KeyingAnswer(PttMode source, const KeyerIdentity& keyer)>;
    void setKeyingGate(KeyingGateFn gate);
    bool hasKeyingGate() const noexcept { return static_cast<bool>(m_keyingGate); }
    /// iPhone app plan Task 77 fix wave, I1 (ruling 8.9): whether a device
    /// other than the station device holds transmit now (TransmitHolder's
    /// state), whatever its key: tx.key, TUNE, two-tone, a Tuner Genius
    /// autotune or VOX. While it does, every press edge of the radio's own
    /// PTT asks the gate (a take), and a press that takes nothing is held
    /// off until it is released. Without it, the press edge asks only
    /// while another keyer's key is on.
    using OtherDeviceHoldsFn = std::function<bool()>;
    void setOtherDeviceHolds(OtherDeviceHoldsFn probe);

    // The keyer of the key now on (station() while unkeyed).
    const KeyerIdentity& currentKeyer() const noexcept { return m_currentKeyer; }
    /// Identity of this exact admission check; absent outside a key attempt.
    std::optional<KeyerIdentity> keyAttemptIdentity() const { return m_keyAttemptIdentity; }

    // The last refusal, as moxRefused sent it.
    const TxRefusal& lastRefusal() const noexcept { return m_lastRefusal; }
    /// TGXL tune lane: whether the last admitKey() was answered with a take
    /// (ruling 8.9): nothing keyed now, and the take's end decides.
    bool lastAdmitTook() const noexcept { return m_lastAdmitTook; }

    // A take the gate asked for has ended (ruling 8.9): with `took` true
    // and the keyer's press still down, the press keys now, as a new key
    // through the gate. A station PTT source is still down when its level
    // is; a remote keyer's press is its caller's to send again.
    void onTakeFinished(const KeyerIdentity& keyer, bool took);
    /// iPhone app plan Task 77 (ruling 8.9): the radio's mic, if held now,
    /// takes and keys nothing more until it is released; the Core calls
    /// this when transmit is taken from the station device.
    void holdOffHeldMic();
    /// iPhone app plan Task 77 (ruling 8.14): the keying gate's refusal
    /// for a program's key from `keyer` (TCI, CAT), asked as a question
    /// (a program's key never takes transmit, so asking changes nothing);
    /// empty when the gate would admit it or there is no gate.
    TxRefusal programKeyRefusal(const KeyerIdentity& keyer) const;

    // A station key that starts more than MOX (TUNE, two-tone) asks the gate
    // before it changes anything, so a refused start never releases or rides
    // another device's key. True when admitted (the gate's side effects
    // apply: transmit unheld becomes the station's); false after reporting
    // the refusal through moxRejected / moxRefused. Always true with no
    // gate installed.
    bool admitStationKey(PttMode source);
    // iPhone app plan Task 35: the same question for any keyer (a remote
    // device's TUNE or two-tone). admitStationKey(source) is
    // admitKey(KeyerIdentity::station(source)).
    bool admitKey(const KeyerIdentity& keyer);

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
    // FOR TESTING ONLY: the drain wait's bound (kTxDrainTimeoutMs).
    void setTxDrainTimeoutMsForTest(int ms);
    // FOR TESTING ONLY: the end-of-over tail's bound (kEndOfOverTailMaxMs).
    void setEndOfOverTailMaxMsForTest(int ms);

    // ── RADE end-of-over callsigns: the end-of-over tail ─────────────────────
    //
    // An operator's release (setMox(false) while nothing blocks transmit)
    // asks the installed function whether a tail goes out first. It returns
    // true when it started one: the walk then keeps the radio keyed, in
    // TxToRxInFlight, until onEndOfOverTailDone() or kEndOfOverTailMaxMs,
    // and only then emits txAboutToEnd and txDrainRequested. It is never
    // asked for a key (the tail keys nothing), for an unkey while TX
    // inhibit, the PA trip or receive-only holds, or when the release did
    // not change MOX. A new key during the tail ends it (stopAllTimers), as
    // does abortEndOfOverTail(), which the Core's stops call so a stop
    // never waits for a tail. RadioModel installs it for RADE (FreeDV's
    // end-of-over frame and 200 ms of silence).
    using EndOfOverTailFn = std::function<bool()>;
    void setEndOfOverTail(EndOfOverTailFn fn);
    void onEndOfOverTailDone();
    void abortEndOfOverTail();
    bool isEndOfOverTailActive() const noexcept { return m_waitingForEndOfOverTail; }
    // isReleasing: MOX is off and the TX to RX walk has not yet released the
    // hardware (an end-of-over tail, the TX channel's drain, the send ring's
    // wait or mox_delay), so the radio is still on the air. Thetis's unkey
    // runs this stretch synchronously inside chkMOX_CheckedChanged2, so
    // nothing can ask about it there; here the walk is timer-driven and the
    // stops must treat this stretch as transmitting.
    bool isReleasing() const noexcept
    {
        return !m_mox && m_state == MoxState::TxToRxInFlight;
    }

    // ── Task 33: the TX drain in the TX→RX walk ──────────────────────────────
    //
    // setAwaitsTxDrain(true): after txDrainRequested the walk waits for
    // onTxDrained() (or kTxDrainTimeoutMs) before mox_delay starts, as
    // Thetis's SetChannelState(tx, 0, 1) blocks before its Sleep(mox_delay).
    // RadioModel sets it while a TX channel is wired; with it false (no TX
    // channel) mox_delay starts at once.
    void setAwaitsTxDrain(bool on);
    bool awaitsTxDrain() const noexcept { return m_awaitTxDrain; }

    // ── G-05: the unkey waits for the send ring ──────────────────────────────
    //
    // JJ's ruling (2026-09-28): at the end of an over the hardware is not
    // released while the transmit I/Q send ring still holds audio, and the
    // wait lasts no longer than the ring's own length, so a stuck link can
    // never hold the transmitter on longer than the audio the ring can hold.
    // It runs after the TX channel's drain and before mox_delay, so Thetis's
    // delays and their order are unchanged (Thetis's mox_delay "allows
    // in-flight samples to clear"; NereusSDR's send ring can hold more than
    // that). Only an operator's release waits: never when permitted() says
    // no (RadioModel: the Core's stops and a disconnect), never under TX
    // inhibit, the PA trip or receive-only, and abortSendRingWait() (which
    // those stops call) ends a wait at once. Without drained or lengthMs,
    // or with a length of zero or less, there is no wait.
    struct SendRingDrain {
        std::function<bool()> permitted;   // an operator's release, not a stop
        std::function<bool()> drained;     // nothing left the sender puts on the wire
        std::function<double()> lengthMs;  // the ring's own length, the wait's bound
    };
    void setSendRingDrain(SendRingDrain drain);
    void abortSendRingWait();
    bool isSendRingWaitActive() const noexcept { return m_waitingForSendRing; }
    // The bound of the wait running now (or of the last one): the ring's
    // length rounded up to a whole millisecond.
    int sendRingWaitCeilingMs() const noexcept { return m_sendRingCeilingMs; }

    // ── Task 33: StopAllTx's _stop_all_tx latch ──────────────────────────────
    //
    // From Thetis console.cs:45324-45342 [v2.10.3.15] (StopAllTx sets
    // _stop_all_tx = true) and console.cs:25479-25492 [v2.10.3.15] (the PTT
    // poll consumes it):
    //   // we can come in here from a ToT ( StopAllTX() ) //[2.10.3.6]MWLGE fixes #518
    //   // however we dont want switch anything back on, unless all of the above have been released
    // latchStopAllTx: while a PTT source (mic, CAT, VOX, TCI) is still held,
    // pollPtt keys nothing; the latch clears once every one of them has
    // been released (at once when none is held).
    void latchStopAllTx();
    bool isStopAllTxLatched() const noexcept { return m_stopAllTxLatched; }

    // Task 33: StopAllTx's `_manual_mox = false;` (console.cs:45332
    // [v2.10.3.15]). Clears the flag and emits manualMoxChanged(false) if
    // it was set, and clears PollPTT's manual key (setManualKey(false));
    // setTune(false) remains the TUN path's release.
    void clearManualMox();

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
    // PTT mode on TUN-off: setTune(false) does not touch m_pttMode itself;
    // its setMox(false) clears it, as chkMOX_CheckedChanged2 does in its
    // TX-to-RX branch (console.cs:29547 [v2.10.3.15]).
    //
    // Manual key on TUN-off: setTune(false) leaves isManualKey() set.
    // Thetis clears _manual_mox only at the end of TUN-off, after the tone
    // and the power are restored (console.cs:30193 [v2.10.3.15]), so no
    // mic PTT or VOX can key while the tune tone is still running. The
    // owner of that completion (RadioModel::completeTuneOff) calls
    // setManualKey(false).
    void setTune(bool on);
    // iPhone app plan Task 35: TUNE keyed as `keyer` (setMox(true, keyer));
    // off is setTune(false).
    void setTune(bool on, const KeyerIdentity& keyer);

    // onMoxButton: the MOX button (TxApplet, container buttons).
    //
    // Ports chkMOX_Click (console.cs:29730-29747 [v2.10.3.15]) with the
    // CheckedChanged it follows:
    //   on:  _manual_mox = true; chkMOX.Checked = true
    //        (a refused key leaves the button off, so _manual_mox = false)
    //   off: chkMOX.Checked = false; _manual_mox = false
    // Thetis's MOX button does not set a PTT mode; the mode stays
    // PTTMode.NONE (only TUN sets PTTMode.MANUAL, console.cs:30144).
    // chkMOX_Click's off branch also turns TUN and two-tone off; those live
    // on RadioModel, so RadioModel::setMoxFromButton does that part.
    void onMoxButton(bool on);

    // setManualKey: Thetis console.ManualMox (console.cs:10668-10672
    // [v2.10.3.15]). Two-tone sets it before keying and clears it after
    // its release settle (setup.cs:11162, 11193 [v2.10.3.15]); RadioModel
    // clears it at the end of TUN-off. Clearing it runs one PollPTT pass,
    // as Thetis's next poll would: a source still held may key.
    void setManualKey(bool on);

    // clearPttSources: drop every recorded PTT-source level without a pass.
    // RadioModel calls it when the connection is torn down: Thetis polls PTT
    // only while the radio is on (console.cs:25465 [v2.10.3.15]), and no
    // source reports a release after the connection is gone.
    void clearPttSources();

    // setTxInhibited: Thetis console.TXInhibit (console.cs:15341-15363
    // [v2.10.3.15]). RadioModel feeds it from TxInhibitMonitor. While set,
    // PollPTT skips every source (console.cs:25470) and setMox(true)
    // refuses every other key with moxRejected; setting it unkeys an active
    // transmission (if (_tx_inhibit && chkMOX.Checked) chkMOX.Checked =
    // false). The manual key is left as it is.
    // `reason` is the words the inhibit is shown and refused with; empty
    // keeps the TX inhibit input's own refusal (TxRefusals::txInhibited).
    // HL2 port part 2: the HL2 I/O board fault passes mi0bot's
    // "I/O Board: Fault Code N" (console.cs:25876-25885 [@c26a8a4]).
    void setTxInhibited(bool on, const QString& reason = QString());
    const QString& txInhibitReason() const noexcept { return m_txInhibitReason; }

    // setPaTripped: Thetis _ganymede_pa_issue. RadioModel feeds it from
    // RadioModel::paTripped() and on every trip message. Same gates as
    // setTxInhibited (console.cs:25470, and chkMOX_CheckedChanged2's abort
    // at console.cs:29364-29371 [v2.10.3.15]); setting it unkeys.
    void setPaTripped(bool on);

    // setRxOnly: Thetis console.RXOnly (console.cs:15312-15334
    // [v2.10.3.15]). RadioModel feeds it from Setup's Receive Only and the
    // HL2 receive-only kit (Task 16). While set, PollPTT skips every source
    // (console.cs:25470), setMox(true) refuses every other key with
    // moxRejected(reason) (chkMOX_CheckedChanged2, console.cs:29378-29382),
    // CAT and TCI requests are dropped as under TX inhibit, and setting it
    // unkeys an active transmission (if (_rx_only && chkMOX.Checked)
    // chkMOX.Checked = false). `reason` is the plain words a refusal
    // shows; empty keeps the default.
    void setRxOnly(bool on, const QString& reason = QString());
    QString rxOnlyReason() const { return m_rxOnlyReason; }
    static QString defaultRxOnlyReason();

    // setRadioLinkDown: TX safety (2026-09-30). RadioModel sets it while the
    // link to the radio is lost and until it is back. While set, setMox(true)
    // refuses every key with moxRejected("The link to the radio is down.")
    // and TxRefusals::radioLinkDown, PollPTT skips every source, CAT and TCI
    // requests are dropped as under TX inhibit, and setting it unkeys an
    // active transmission. From Thetis console.cs:27488-27493 [v2.10.3.15],
    // run when loss of sync powers the radio off:
    //   chkMOX.Checked = false;
    //   chkMOX.Enabled = false;
    //   chkTUN.Checked = false;
    //   chkTUN.Enabled = false;
    //   chk2TONE.Checked = false;  // MW0LGE_21a
    //   chk2TONE.Enabled = false;
    void setRadioLinkDown(bool on);
    bool isRadioLinkDown() const noexcept { return m_radioLinkDown; }

    // transmitBlockReason: the words setMox(true) refuses with while TX
    // inhibit, a PA trip, receive only or a lost radio link holds (the lost
    // link first, then the trip, then receive only, then TX inhibit); empty
    // when none does. Task 16 fix
    // wave (M2). transmitBlockChanged reports a change.
    QString transmitBlockReason() const;
    // iPhone app plan Task 34: the same gate as a TxRefusal (empty when
    // none holds).
    TxRefusal transmitBlockRefusal() const;

    // setVoxEnabled: engage/disengage VOX with voice-family mode-gate.
    //
    // From Thetis CMSetTXAVoxRun (cmaster.cs:1039-1052 [v2.10.3.13]):
    //   VOX fires only when the TX DSP mode is in the voice family:
    //   LSB, USB, DSB, AM, SAM, FM, DIGL, DIGU.  In CW (CWL, CWU),
    //   SPEC, or DRM the gated result is always false regardless of the
    //   voxEnabled flag value.
    //
    // Idempotent on the GATED value: if the gate result does not change
    // (e.g. VOX toggled while in CW mode), no signal is emitted and no
    // WDSP call is made.  This prevents spurious SetDEXPRunVox(false)
    // calls when the user operates the VOX button in a non-voice mode.
    //
    // Wired by RadioModel H.1:
    //   TransmitModel::voxEnabledChanged → MoxController::setVoxEnabled
    //
    // From Thetis cmaster.cs:1039-1052 [v2.10.3.13]:
    //   bool run = Audio.VOXEnabled && (mode == DSPMode.LSB || ...)
    //   cmaster.SetDEXPRunVox(id, run);
    void setVoxEnabled(bool on);

    // onModeChanged: re-evaluate the voice-family gate when the TX DSP
    // mode changes.
    //
    // Called whenever SliceModel::dspModeChanged fires on the active
    // slice.  Updates m_currentMode and calls recomputeVoxRun().
    //
    // From Thetis CMSetTXAVoxRun (cmaster.cs:1039-1052 [v2.10.3.13]):
    //   DSPMode mode = Audio.TXDSPMode;  // re-read at every call site
    //
    // Wired by RadioModel H.1:
    //   SliceModel::dspModeChanged → MoxController::onModeChanged
    void onModeChanged(DSPMode mode);

    // ── H.2: VOX threshold with mic-boost-aware scaling ──────────────────────

    // setVoxThreshold: set the VOX attack threshold in dB and recompute.
    //
    // Ports Thetis CMSetTXAVoxThresh (cmaster.cs:1054-1059 [v2.10.3.13]):
    //   if (Audio.console.MicBoost) thresh *= (double)Audio.VOXGain;
    //   cmaster.SetDEXPAttackThreshold(id, thresh);
    //
    // The caller (setup.cs:18911 [v2.10.3.13]) converts dB to linear
    // amplitude first:
    //   Math.Pow(10.0, (double)udDEXPThreshold.Value / 20.0)
    // then passes to CMSetTXAVoxThresh.  This slot performs both the
    // dB→linear conversion AND the mic-boost scaling so the RadioModel
    // wiring is a single direct connection.
    //
    // Wired by RadioModel H.2:
    //   TransmitModel::voxThresholdDbChanged → MoxController::setVoxThreshold
    void setVoxThreshold(int dB);

    // onMicBoostChanged: re-evaluate the scaled threshold when the
    // mic-boost flag changes.
    //
    // From Thetis CMSetTXAVoxThresh (cmaster.cs:1057 [v2.10.3.13]):
    //   if (Audio.console.MicBoost) thresh *= (double)Audio.VOXGain;
    //
    // When MicBoost toggles, the effective threshold changes even if the
    // raw dB value has not.  This slot updates m_micBoost and calls
    // recomputeVoxThreshold().
    //
    // Re-evaluation trigger is also noted in setup.cs:7684-7687 [v2.10.3.13]:
    //   chk20dbMicBoost_CheckedChanged re-runs udVOXGain_ValueChanged
    //   (which calls CMSetTXAVoxThresh) whenever mic-boost changes.
    //
    // Wired by RadioModel H.2:
    //   TransmitModel::micBoostChanged → MoxController::onMicBoostChanged
    void onMicBoostChanged(bool boost);

    // setVoxGainScalar: update the mic-boost gain scalar and recompute.
    //
    // From Thetis audio.cs:194-202 [v2.10.3.13]:
    //   private static float vox_gain = 1.0f;  // default 1.0
    //   // Used in CMSetTXAVoxThresh when MicBoost is on
    //
    // NereusSDR exposes this as TransmitModel::voxGainScalar.  Changing
    // it re-evaluates the scaled threshold even if dB and micBoost are
    // unchanged.
    //
    // Wired by RadioModel H.2:
    //   TransmitModel::voxGainScalarChanged → MoxController::setVoxGainScalar
    void setVoxGainScalar(float scalar);

    // ── H.3: VOX hang-time + anti-VOX gain + anti-VOX source path ────────────

    // setVoxHangTime: set the DEXP (downward expander / VOX) hold time in ms.
    //
    // Converts ms → seconds before emitting voxHangTimeRequested(double).
    // TxChannel::setVoxHangTime (D.3) wraps WDSP SetDEXPHoldTime which takes
    // seconds.
    //
    // From Thetis Project Files/Source/Console/setup.cs:18896-18900 [v2.10.3.13]:
    //   private void udDEXPHold_ValueChanged(object sender, EventArgs e)
    //   {
    //       if (initializing) return;
    //       cmaster.SetDEXPHoldTime(0, (double)udDEXPHold.Value / 1000.0);
    //   }
    //
    // Wired by RadioModel H.3:
    //   TransmitModel::voxHangTimeMsChanged → MoxController::setVoxHangTime
    void setVoxHangTime(int ms);

    // setAntiVoxGain: set the anti-VOX gain in dB.
    //
    // Converts dB → linear amplitude (voltage scaling: /20.0) before emitting
    // antiVoxGainRequested(double). TxChannel::setAntiVoxGain (D.3) wraps WDSP
    // SetAntiVOXGain which takes a linear double.
    //
    // From Thetis Project Files/Source/Console/setup.cs:18986-18990 [v2.10.3.13]:
    //   private void udAntiVoxGain_ValueChanged(object sender, EventArgs e)
    //   {
    //       if (initializing) return;
    //       cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
    //   }
    //
    // NaN sentinel m_lastAntiVoxGainEmitted forces first-call emit so WDSP is
    // primed at startup regardless of the default value.
    //
    // Wired by RadioModel H.3:
    //   TransmitModel::antiVoxGainDbChanged → MoxController::setAntiVoxGain
    void setAntiVoxGain(int dB);

    // 3M-3a-iv post-bench refactor (Option A): setAntiVoxSourceVax slot
    // removed.  Thetis chkAntiVoxSource at setup.designer.cs:44646-44657
    // [v2.10.3.13] selects between RX and VAC as the anti-VOX cancellation
    // reference; that choice does not map to NereusSDR's architecture, where
    // VAX is a digital-mode app bus with no mic-feedback path and the audio
    // output device is therefore the only valid source.  See class header /
    // commit message for the architectural rationale.

    // setAntiVoxTau: set the anti-VOX detector smoothing time-constant in ms.
    //
    // Mirrors udAntiVoxTau_ValueChanged from Thetis
    // Project Files/Source/Console/setup.cs:18992-18996 [v2.10.3.13]:
    //   private void udAntiVoxTau_ValueChanged(object sender, EventArgs e)
    //   {
    //       if (initializing) return;
    //       cmaster.SetAntiVOXDetectorTau(0, (double)udAntiVoxTau.Value / 1000.0);
    //   }
    //
    // Range from setup.designer.cs:44661-44688 [v2.10.3.13]:
    //   Minimum=1, Maximum=500, Increment=1, default Value=20.
    //
    // Emits antiVoxDetectorTauRequested(seconds) only when the converted
    // (ms / 1000.0) value changes vs the most recent emission.  NaN sentinel
    // m_lastAntiVoxTauEmitted forces first-call emit so the WDSP DEXP block
    // is primed at startup.
    //
    // Range is clamped defensively to [1, 500] in case the upstream caller
    // (TransmitModel::setAntiVoxTauMs) ever emits an out-of-range value.
    //
    // Wired by RadioModel H.3 (Phase 3M-3a-iv Task 9):
    //   TransmitModel::antiVoxTauMsChanged → MoxController::setAntiVoxTau
    //   MoxController::antiVoxDetectorTauRequested → TxWorkerThread::setAntiVoxDetectorTau
    void setAntiVoxTau(int ms);

    // setAntiVoxRun: master enable for the WDSP DEXP anti-VOX detector.
    //
    // Mirrors chkAntiVoxEnable_CheckedChanged from Thetis setup.cs:18980-18984
    // [v2.10.3.13]:
    //   private void chkAntiVoxEnable_CheckedChanged(object sender, EventArgs e)
    //   {
    //       if (initializing) return;
    //       cmaster.SetAntiVOXRun(0, chkAntiVoxEnable.Checked);
    //   }
    //
    // 3M-3a-iv: this slot replaces the older collapsed wiring where
    // antiVoxSourceWhatRequested drove TxWorkerThread::setAntiVoxRun via a
    // !useVax inversion.  The post-bench Option A refactor then dropped the
    // source-selector entirely (see class header).
    //
    // First-call emit guard m_antiVoxRunInitialized: once any accepted call
    // has run, subsequent same-value calls are suppressed.  The first call
    // always emits so TxWorkerThread/TxChannel are primed at startup
    // regardless of default match.
    //
    // Wired by RadioModel (3M-3a-iv scope-expansion):
    //   TransmitModel::antiVoxRunChanged → MoxController::setAntiVoxRun
    //   MoxController::antiVoxRunRequested → TxWorkerThread::setAntiVoxRun
    void setAntiVoxRun(bool run);

    // ── Bench fix 2026-05-14: WDSP re-prime after late-wired TxChannel ───────
    //
    // primeWdspState: re-emit the three NaN-sentinel-guarded signals
    //   voxThresholdRequested, voxHangTimeRequested, antiVoxGainRequested
    // using the current MoxController member state, by resetting the
    // m_lastXxxEmitted sentinels and re-running the recompute() helpers.
    //
    // Motivation (reported bench symptom 2026-05-14, "VOX needs juggling to
    // prime"): TransmitModel::loadFromSettings (RadioModel.cpp:2631) is called
    // EARLY in connectToRadio, before the MoxController -> TxChannel connects
    // at RadioModel.cpp:3604/3612/3620 are established by the WDSP-init
    // lambda.  During that load the TM -> MoxController connections (wired in
    // RadioModel ctor at lines 770/812) fire setVoxThreshold / setVoxHangTime
    // / setAntiVoxGain, each consuming its NaN sentinel via the first-call
    // emit -- but those emits land in a void receiver because TxChannel
    // doesn't exist yet.  After TxChannel is wired the sentinels are already
    // consumed, so a same-value call short-circuits.  Result: WDSP retains
    // its construction-time defaults until the user moves a slider, which
    // generates a different value that passes the equality guard.
    //
    // The asymmetric design choice for antiVoxTau (RadioModel.cpp:5025) and
    // antiVoxRun (RadioModel.cpp:5051) -- defer the TM -> Mox connect until
    // after TxWorkerThread is constructed and add an explicit re-push --
    // avoids the race entirely.  primeWdspState() retrofits the equivalent
    // semantic for the three older paths without restructuring their connect
    // ordering.
    //
    // Called from RadioModel::pushTxProcessingChain (the WDSP-init lambda at
    // RadioModel.cpp:3747) after the MoxController -> TxChannel connects are
    // established.  Safe to call multiple times -- each call resets the
    // sentinels and re-emits, which is the desired behaviour on
    // disconnect/reconnect (a fresh TxChannel needs to be re-primed).
    void primeWdspState();

    // ── PTT-source slots (Thetis PollPTT) ────────────────────────────────────
    //
    // Mic PTT, CAT, VOX and TCI record their source's level and run one
    // PollPTT pass (console.cs:25463-25623 [v2.10.3.15]) over the recorded
    // levels. Thetis polls every millisecond; NereusSDR runs the pass on
    // every source event, and the radio's status frames (mic PTT, sent on
    // every frame) keep it running like a poll while connected.
    //
    // The pass follows Thetis:
    //   - Nothing happens while a manual key is on (isManualKey(), Thetis
    //     _manual_mox): no source keys and no release unkeys.
    //   - From receive (!MOX) each held source keys and sets its PTT mode,
    //     in Thetis's order TCI, CAT, mic, VOX (the last held one names the
    //     mode). PttMode is set before setMox(true), as Thetis assigns
    //     _current_ptt_mode just before chkMOX.Checked = true.
    //   - While keyed, a release unkeys only when it is the release of the
    //     source named by the PTT mode: a mic release during a VOX, CAT,
    //     TCI or manual key does nothing, and so does a VOX release during
    //     a mic key. A TCI release falls back to a still-held source
    //     (getFallbackPTTModeAfterTCIRelease) and unkeys only if none is.
    //
    // UNKEY: setMox(false) itself sets the PTT mode to None and drops the
    // CAT and TCI levels, as chkMOX_CheckedChanged2 does on every unkey
    // (console.cs:29406-29411 and 29547 [v2.10.3.15]). A refused key does
    // the same (Thetis refuses by unchecking chkMOX, which runs that branch).
    //
    // Space and X2 are not part of PollPTT: onSpacePtt / onX2Ptt set their
    // mode on press and drive setMox directly, as before.
    //
    // Rejected slot (CW): logs qCWarning(lcDsp) and returns without calling
    // setMox() or updating m_pttMode. CW keying is 3M-2.

    // onMicPttFromRadio: MIC PTT button on the radio hardware.
    //
    // Triggered when the radio's physical PTT switch / mic button is pressed
    // or released. In Thetis this maps to:
    //   PollPTT: bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
    //   _current_ptt_mode = PTTMode.MIC;                   [v2.10.3.13]
    //   From Thetis console.cs:25492 [v2.10.3.13]
    //
    // RadioConnection::micPttFromRadio calls this on every P1/P2 status
    // frame (H.5), pressed or not, so the PollPTT pass runs like Thetis's
    // poll while connected.
    //
    // Note: the slot name is "FromRadio" to distinguish hardware PTT from a
    // future software-only "mic mute" control.
    void onMicPttFromRadio(bool pressed);

    // onCatPtt: CAT (computer-aided transceiver) PTT command.
    //
    // Triggered when a CAT command (serial or network) asserts or de-asserts
    // PTT.  In Thetis this maps to:
    //   PollPTT: bool cat_ptt = (_ptt_bit_bang_enabled && ...) | _cat_ptt;
    //   _current_ptt_mode = PTTMode.CAT;                    [v2.10.3.13]
    //   From Thetis console.cs:25469 [v2.10.3.13]
    // Upstream tags preserved: //MW0LGE (from cited console.cs:25473) [v2.10.3.15]
    //
    // Full CAT integration is Phase 3K.  Wiring deferred to 3K; this slot
    // establishes the API.
    void onCatPtt(bool pressed);

    // onVoxActive: WDSP VOX activity crossing the DEXP gate.
    //
    // Triggered when WDSP's downward expander (DEXP/VOX) determines that the
    // microphone signal has exceeded the VOX threshold.  In Thetis this maps to:
    //   PollPTT: bool vox_ptt = vox_ok && Audio.VOXActive;
    //   _current_ptt_mode = PTTMode.VOX;                    [v2.10.3.13]
    //   From Thetis console.cs:25507 [v2.10.3.13]
    //
    // TxChannel::voxActiveChanged (the DEXP pushvox callback) calls this on
    // each change. VOX keys only in the voice modes, as PollPTT's VOX branch
    // requires (console.cs:25543-25555 [v2.10.3.15]).
    void onVoxActive(bool active);

    // onSpacePtt: spacebar PTT from the keyboard handler.
    //
    // Triggered when the user presses / releases the space bar with
    // spacebar_ptt enabled in console settings.  In Thetis this maps to:
    //   Console_KeyDown case Keys.Space:
    //   _current_ptt_mode = PTTMode.SPACE;                  [v2.10.3.13]
    //   chkMOX.Checked = !chkMOX.Checked;
    //   From Thetis console.cs:26680 [v2.10.3.13]
    //
    // Wired in 3M-3a or later (UI keyboard handler).  This slot establishes
    // the API.
    void onSpacePtt(bool pressed);

    // onX2Ptt: X2 jack external PTT trigger.
    //
    // Triggered when the radio's X2 input jack asserts or de-asserts.
    // PTTMode::X2 is defined in Thetis enums.cs:353 [v2.10.3.13] but the
    // X2 PTT dispatch path in Thetis is not currently extracted to PollPTT.
    // NereusSDR pre-wires the slot here for parity completeness; wiring
    // deferred to 3M-3a or later when X2 status-frame parsing lands.
    void onX2Ptt(bool pressed);

    // ── H.4: Rejected PTT-source dispatch slot (CW) ──────────────────────────
    //
    // This slot EXISTS but REJECTS all calls with qCWarning(lcDsp) + return.
    // CW is deferred to 3M-2.
    //
    // The slots are declared (rather than omitted) so that:
    //   (a) tests can verify rejection behaviour via QSignalSpy;
    //   (b) future callers can wire to a stable name without waiting for the
    //       deferred implementation;
    //   (c) the rejection point is explicit in the public API.
    //
    // Rejection pattern (qCWarning + early return; no setMox / no setPttMode
    // update) — same shape as the historical setAntiVoxSourceVax(true) path
    // (removed in 3M-3a-iv post-bench refactor):
    //   qCWarning(lcDsp) << "... rejected, deferred to 3M-2";
    //   return;   // no setMox(), no setPttMode() update

    // onCwPtt: CW keyer PTT — REJECTED (deferred to 3M-2).
    //
    // In Thetis this maps to:
    //   PollPTT: bool cw_ptt = CWInput.KeyerPTT && ...;
    //   _current_ptt_mode = PTTMode.CW;                     [v2.10.3.13]
    //   From Thetis console.cs:25475 [v2.10.3.13]
    // Upstream tags preserved: //MW0LGE (from cited console.cs:25473) [v2.10.3.15]
    //
    // 3M-2 will implement the CW keyer, sidetone, and QSK/break-in state
    // machine.  This slot logs and returns without driving MOX.
    void onCwPtt(bool pressed);

    // onTciPtt: TCI trx (the console side of Thetis TCIPTT).
    //
    // RadioModel::setMox, the shim TciProtocol invokes for trx, calls this.
    // In Thetis handleTrxMessage writes TCIPTT (console.cs:2456-2466
    // [v2.10.3.15]) and PollPTT keys with PTTMode.TCI
    // (console.cs:25507-25511). A release in TCI mode falls back to a
    // still-held CAT, mic or VOX source, or unkeys if none is held
    // (console.cs:25562-25581, getFallbackPTTModeAfterTCIRelease at
    // console.cs:25429-25461).
    void onTciPtt(bool pressed);

    // ── C.4: rx2_enabled / vfobTx state for multicast Pre/Post rx argument ───
    //
    // Idempotent setters that update the internal state used by activeRxForTx()
    // (the rx argument carried by moxChanging / moxChanged). No signal is
    // emitted from these slots — they only mirror upstream RadioModel state so
    // that the rx argument carried by Pre/Post signals stays correct.
    //
    // Default for both: false. NereusSDR does not have RX2 wired yet, so the
    // emitted rx argument is always 1 until RadioModel calls these setters.
    //
    // From Thetis console.cs:29324 [v2.10.3.13] — MoxPreChangeHandlers and
    // Upstream tags preserved: //MW0LGE (from cited console.cs:29326) [v2.10.3.15]
    //                console.cs:29677 [v2.10.3.13] — MoxChangeHandlers:
    //   rx2_enabled && VFOBTX ? 2 : 1
    void setRx2Enabled(bool enabled);
    void setVfobTx(bool enabled);

    // ── Phase 3P-II Task 87: TxInterlockPolicy gate ───────────────────────────
    //
    // setInterlockPolicy: install (or clear) the TxInterlockPolicy that
    // setMox(true) consults immediately after the BandPlanGuard check
    // (K.2) and before the Codex P2 safety effects.
    //
    // When a policy is installed and setMox(true) is called:
    //   policy->evaluateTxRequest(m_ampPresent, m_ampInOperate, m_lastSwr)
    // returns false (Block mode) => the policy emits denied(reason) and
    //   setMox returns early. The denied() signal is connected in
    //   MainWindow::buildUI() to onTxInterlockDenial() which toasts the
    //   operator via QStatusBar::showMessage.
    // returns true (Disabled/Warn) => TX proceeds; in Warn mode the policy
    //   emits warned(reason) which reaches onTxInterlockWarning similarly.
    //
    // Amp state and SWR are cached via the two slots below and updated
    // by RadioModel signal connections established after this call.
    //
    // Pass nullptr to remove the policy (reverts to pre-policy behavior).
    // Called once from RadioModel ctor after both m_moxController and
    // m_txInterlockPolicy are constructed.
    void setInterlockPolicy(TxInterlockPolicy* policy);

    // onAmpStateChanged: update the cached amplifier presence and operate
    // flags used by the interlock gate.
    //
    // RadioModel wires this from its amplifierChanged / ampStateChanged
    // lambda so MoxController always has an up-to-date snapshot without
    // holding a RadioModel pointer.
    //
    // Thread safety: must be called from the main thread (same as setMox).
    void onAmpStateChanged(bool hasAmp, bool inOperate);

    // onAmpSwrUpdated: cache the most-recent SWR ratio from the PGXL
    // meter stream.
    //
    // RadioModel wires this from ampMetersChanged(float fwd, float swr).
    // Only the swr argument is forwarded here.
    //
    // Thread safety: must be called from the main thread (same as setMox).
    void onAmpSwrUpdated(float swr);

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

    // iPhone app plan Task 34 (ruling 8.13): a remote device's key and its
    // release. On: the keying gate is asked with the keyer (and its
    // source) before anything changes; an admitted key is the keyer's.
    // Off: unkeys only when the key now on is this keyer's; another
    // keyer's key is left alone (ruling 8.5). The Core's safety stops and
    // local callers use setMox(false), which unkeys whoever is keyed.
    void setMox(bool on, const KeyerIdentity& keyer);

    // Task 33: the TX channel's unkey drain has finished (RadioModel relays
    // TxChannel::txDrained for the drain it requested). Ignored unless the
    // walk is waiting for it.
    void onTxDrained();

signals:
    // Task 16 fix wave (M2): transmitBlockReason() changed; `reason` is the
    // new value, empty when nothing blocks transmit any more.
    void transmitBlockChanged(const QString& reason);

    // ── K.2: rejection signal ────────────────────────────────────────────────
    //
    // moxRejected: emitted when setMox(true) is called but the MoxCheckFn
    // callback rejects the request (ok == false).
    //
    // Carries the human-readable reason string from BandPlanGuard::MoxCheckResult
    // suitable for display in a status-bar toast or tooltip override.
    //
    // Subscribers:
    //   - MainWindow: statusBar()->showMessage(reason, 3000)   [K.2]
    //   - TxApplet: tooltip override on m_moxBtn               [K.2]
    //
    // NOT emitted when no MoxCheckFn is installed (bypass — backwards-compat).
    // NOT emitted for setMox(false) — release is never rejected.
    void moxRejected(QString reason);

    // iPhone app plan Task 34 (R-IOS-13): every refusal moxRejected reports,
    // as the TxRefusal the link carries (its code, its sentence, its fix).
    // Emitted right after moxRejected, or alone when a held source's repeat
    // refusal is quiet.
    void moxRefused(const NereusSDR::TxRefusal& refusal);
    /// TGXL tune lane (ruling 8.9 for the tuner's front-panel TUNE): the
    /// take a tunerPress key asked for has ended; `took` says whether the
    /// station now holds transmit.
    void tunerTakeFinished(bool took);

    // iPhone app plan Task 77 fix round 2: a PTT source was released and
    // none is down now (anyPttSourceHeld() false). RadioModel retries an
    // amplifier switch it owes.
    void pttSourcesReleased();

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
    // TX→RX phase signals (in order; Task 33 follows Thetis's unkey,
    // console.cs:29651-29685 [v2.10.3.15]):
    //   txAboutToEnd      : entry to TX→RX walk; teardown begins.
    //   txDrainRequested  : the TX channel drains now, with the hardware
    //                       still keyed (Thetis SetChannelState(tx, 0, 1)).
    //                       The walk waits for onTxDrained() when
    //                       awaitsTxDrain(), bounded by kTxDrainTimeoutMs.
    //   txaFlushed        : after the drain and mox_delay / key_up_delay
    //                       (in-flight samples cleared); the RF gate closes.
    //   hardwareFlipped   : hardware routing released (isTx=false), right
    //                       after txaFlushed (Thetis HdwMOXChanged follows
    //                       Sleep(mox_delay)).
    //   rxReady           : after ptt_out_delay; RX channels active.
    //
    // hardwareFlipped(bool isTx):
    //   true  — RX→TX: assert Alex routing, ATT-on-TX, MOX wire bit.
    //   false — TX→RX: release Alex routing, ATT-on-TX, clear MOX wire bit.
    //   One subscriber slot handles both directions via the bool payload.

    void txAboutToBegin();          // RX→TX phase 1 of 3 — synchronous; safety-relevant prep
    void hardwareFlipped(bool isTx);// Both directions; synchronous; subscribers wire Alex/ATT/MOX-bit
    void txReady();                 // RX→TX phase 3 of 3 — fires after rfDelay timer
    void txAboutToEnd();            // TX→RX phase 1 of 5: synchronous; teardown entry
    void txDrainRequested();        // TX→RX phase 2 of 5: synchronous; TX channel drains (hardware still keyed)
    void txaFlushed();              // TX→RX phase 3 of 5: after the drain and keyUpDelay; in-flight samples cleared
                                    // (phase 4 of 5 is hardwareFlipped(false), right after txaFlushed)
    void rxReady();                 // TX→RX phase 5 of 5: fires after pttOutDelay

    // RADE end-of-over callsigns: an end-of-over tail began (true) or
    // ended, however it ended (false). Emitted before txAboutToEnd.
    void endOfOverTailChanged(bool active);

    // voxRunRequested: emitted when the gated VOX-run state changes.
    //
    // Carries (voxEnabled && isVoiceMode(currentMode)).  Emitted at most
    // once per gated-value transition (idempotent on the EMITTED state,
    // not on the raw input).
    //
    // Subscribers: RadioModel H.1 connects this to TxChannel::setVoxRun.
    //
    // From Thetis CMSetTXAVoxRun (cmaster.cs:1039-1052 [v2.10.3.13]):
    //   cmaster.SetDEXPRunVox(id, run);  // 'run' is the gated bool
    void voxRunRequested(bool run);

    // voxThresholdRequested: emitted when the computed WDSP attack threshold
    // changes.
    //
    // Carries the mic-boost-aware linear amplitude ready for
    // TxChannel::setVoxAttackThreshold(double).  Emitted at most once per
    // computed-value transition (idempotent on the EMITTED double).  The NAN
    // sentinel in m_lastVoxThresholdEmitted guarantees one emit on the first
    // call so WDSP is always primed at startup.
    //
    // Subscribers: RadioModel H.2 connects this (via lambda) to
    //   TxChannel::setVoxAttackThreshold(double).
    //
    // From Thetis CMSetTXAVoxThresh (cmaster.cs:1054-1059 [v2.10.3.13]):
    //   cmaster.SetDEXPAttackThreshold(id, thresh);
    //   // thresh is linear amplitude (dB→linear done by setup.cs:18911)
    void voxThresholdRequested(double thresh);

    // ── H.3 phase signals ────────────────────────────────────────────────────

    // voxHangTimeRequested: emitted when the DEXP hold time changes.
    //
    // Carries the converted value in seconds.  Idempotent on the EMITTED
    // double (NAN sentinel forces first-call emit to prime WDSP).
    //
    // Subscribers: RadioModel H.3 connects this (via lambda) to
    //   TxChannel::setVoxHangTime(double seconds).
    //
    // From Thetis setup.cs:18899 [v2.10.3.13]:
    //   cmaster.SetDEXPHoldTime(0, (double)udDEXPHold.Value / 1000.0);
    void voxHangTimeRequested(double seconds);

    // antiVoxGainRequested: emitted when the anti-VOX gain changes.
    //
    // Carries the linear amplitude value (dB→linear via /20.0, voltage
    // scaling).  Idempotent on the EMITTED double (NAN sentinel).
    //
    // Subscribers: RadioModel H.3 connects this (via lambda) to
    //   TxChannel::setAntiVoxGain(double gain).
    //
    // From Thetis setup.cs:18989 [v2.10.3.13]:
    //   cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
    void antiVoxGainRequested(double gain);

    // antiVoxDetectorTauRequested: emitted when the anti-VOX detector tau
    // changes (post ms/1000.0 scaling).
    //
    // Carries the time-constant in seconds (range [0.001, 0.500]).
    // Idempotent on the EMITTED double (NaN sentinel forces first-call emit
    // so the WDSP DEXP block is primed at startup).
    //
    // Subscribers (RadioModel H.3, wired in Phase 3M-3a-iv Task 9):
    //   TxWorkerThread::setAntiVoxDetectorTau(double seconds) — queued.
    //
    // From Thetis Project Files/Source/Console/setup.cs:18995 [v2.10.3.13]:
    //   cmaster.SetAntiVOXDetectorTau(0, (double)udAntiVoxTau.Value / 1000.0);
    void antiVoxDetectorTauRequested(double seconds);

    // 3M-3a-iv post-bench refactor (Option A): antiVoxSourceWhatRequested
    // signal removed alongside setAntiVoxSourceVax.  See class-header
    // comment block for the architectural rationale (Thetis chkAntiVoxSource
    // does not map to NereusSDR's architecture).

    // antiVoxRunRequested: emitted when the master anti-VOX enable changes.
    //
    // Subscribers: RadioModel (3M-3a-iv scope-expansion) connects this
    // queued to TxWorkerThread::setAntiVoxRun(bool), which forwards to
    // TxChannel::setAntiVoxRun(bool) AND flips the worker-local
    // m_antiVoxRun atomic gate that onAntiVoxSamplesReady checks.
    //
    // From Thetis cmaster.SetAntiVOXRun call at setup.cs:18983 [v2.10.3.13].
    void antiVoxRunRequested(bool run);

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

    // ── C.2 / C.3: multicast Pre/Post MOX state-change signals ──────────────
    //
    // Future Pre/Post observers (PS form, TCI server, MeterPoller, audio
    // recorder) plug into these signals.  They are an additive overlay over
    // the existing 1-arg moxStateChanged(bool) — moxStateChanged is NOT
    // renamed or removed.
    //
    // moxChanging (Pre): emitted BEFORE the state-machine transition begins.
    //   At emit time, isMox() still reflects the OLD value.  Subscribers
    //   may defensively freeze readings (e.g. MeterPoller pauses).
    //
    // moxChanged (Post): emitted AFTER the timer walk completes (parallel to
    //   the existing moxStateChanged boundary signal — fires from the same
    //   onRfDelayElapsed / onPttOutElapsed slots).  Carries the actual
    //   bOldMox stack capture from setMox so the payload is direction-correct
    //   even if upstream state was racing.
    //
    // rx argument (C.4):
    //   The receiver index that OWNS THE TX PATH:
    //     (m_rx2Enabled && m_vfobTx) ? 2 : 1
    //   RX2 alone without VFOBTX still yields rx==1 — TX comes off VFO-A.
    //
    // From Thetis console.cs:29324 [v2.10.3.13] — Pre emit point:
    // Upstream tags preserved: //MW0LGE (from cited console.cs:29326) [v2.10.3.15]
    //   MoxPreChangeHandlers?.Invoke(rx2_enabled && VFOBTX ? 2 : 1, _mox,
    //                                chkMOX.Checked); // MW0LGE_21k8
    // From Thetis console.cs:29677 [v2.10.3.13] — Post emit point:
    //   if (bOldMox != tx) MoxChangeHandlers?.Invoke(rx2_enabled && VFOBTX
    //                                ? 2 : 1, bOldMox, tx); // MW0LGE_21a
    void moxChanging(int rx, bool oldMox, bool newMox);
    void moxChanged(int rx, bool oldMox, bool newMox);

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
    void onTxDrainTimedOut();     // Task 33: the drain wait's bound
    void onEndOfOverTailTimedOut(); // RADE end-of-over tail's bound
    void onSendRingPoll();          // G-05: is the send ring empty yet
    void onSendRingCeiling();       // G-05: the ring's length has passed

private:
    // Task 33: the drain is done (or its wait timed out): start mox_delay.
    void finishTxDrainWait();
    // G-05: after the TX channel's drain, wait for the send ring when this
    // release may (true: the wait runs and starts mox_delay when it ends).
    bool beginSendRingWait();
    // G-05: the ring drained, its length passed or the wait was aborted:
    // start mox_delay.
    void finishSendRingWait();
    // The TX→RX walk from txAboutToEnd on (split out of setMox(false) so an
    // end-of-over tail can run first).
    void beginTxToRxTeardown();
    // The end-of-over tail is over (sent, timed out or aborted): go on.
    void finishEndOfOverTail();
    // Task 34: a refusal, as moxRejected(reason) and moxRefused(refusal);
    // `quiet` (a held source's repeat, M3) records it and says nothing.
    void reportRefusal(const QString& reason, const TxRefusal& refusal, bool quiet);
    static TxRefusal refusalForCheck(const safety::BandPlanGuard::MoxCheckResult& result);
    // Fix wave M1: what setMox(true) would refuse before the keying gate
    // (TX inhibit, the PA trip, receive only, the band plan, the
    // interlock), asked without reporting anything. Empty when none would.
    TxRefusal refusalBeforeTheGate() const;
    // Fix wave 2, Important 2: the interlock's refusal, asked quietly.
    // admitKey asks TX inhibit, the PA trip, receive only and this before
    // the gate, whatever the key sets up first; the band plan and the
    // microphone are judged after TUNE's mode swap, in setMox, and a take
    // whose key then never starts is released by its holder.
    TxRefusal interlockRefusal() const;
    // isVoiceMode: true for the 8 voice-family DSP modes.
    //
    // Voice family (per Thetis CMSetTXAVoxRun, cmaster.cs:1043-1050
    // [v2.10.3.13]):
    //   LSB, USB, DSB, AM, SAM, FM, DIGL, DIGU — 8 modes.
    // Excluded (VOX gate → false):
    //   CWL, CWU — CW modes (no mic audio).
    //   SPEC, DRM — special / DRM modes.
    bool isVoiceMode(DSPMode mode) const noexcept;

    // recomputeVoxRun: recalculate the gated VOX state and emit
    // voxRunRequested(bool) if the result has changed since the last emit.
    //
    // Called by setVoxEnabled() and onModeChanged() after updating their
    // respective member variables.  Idempotent on the EMITTED value so
    // that repeated inputs (e.g. VOX toggled in CW mode stays gated-false)
    // do not trigger spurious WDSP calls.
    void recomputeVoxRun();

    // computeScaledThreshold: compute the WDSP-side linear amplitude
    // from m_voxThresholdDb and the mic-boost gain.
    //
    // Ports the two-step Thetis formula verbatim:
    //   Step 1 (setup.cs:18911 [v2.10.3.13]):
    //     thresh = Math.Pow(10.0, (double)udDEXPThreshold.Value / 20.0)
    //   Step 2 (cmaster.cs:1057 [v2.10.3.13]):
    //     if (Audio.console.MicBoost) thresh *= (double)Audio.VOXGain;
    //
    // No additional clamping — Thetis applies none.
    double computeScaledThreshold() const noexcept;

    // recomputeVoxThreshold: emit voxThresholdRequested(double) if the
    // computed value differs from the last emitted value.
    //
    // Idempotent on the EMITTED double (not on any individual input).
    // Uses std::isnan(m_lastVoxThresholdEmitted) as a first-call sentinel
    // so the initial call always primes WDSP regardless of the default value.
    // Subsequent calls compare with qFuzzyCompare to suppress spurious emits
    // from floating-point noise.
    void recomputeVoxThreshold();

    // recomputeVoxHangTime: emit voxHangTimeRequested(double) if the converted
    // hang time (in seconds) differs from the last emitted value.
    //
    // NAN sentinel forces first-call emit.  Subsequent calls use qFuzzyCompare.
    void recomputeVoxHangTime();

    // recomputeAntiVoxGain: emit antiVoxGainRequested(double) if the dB→linear
    // converted gain differs from the last emitted value.
    //
    // From Thetis setup.cs:18989 [v2.10.3.13]: Math.Pow(10.0, dB / 20.0).
    // NAN sentinel forces first-call emit.
    void recomputeAntiVoxGain();

    // advanceState: sets m_state and emits stateChanged.
    void advanceState(MoxState newState);

    // stopAllTimers: cancel any in-flight timers (safety guard for
    // rapid setMox(false)/setMox(true) toggles or test teardown).
    void stopAllTimers();

    // ── K.2: MOX pre-check callback ──────────────────────────────────────────
    // Installed by RadioModel after construction. Empty (nullptr) by default.
    // setMox(true) consults this BEFORE Codex P2 safety effects. If the check
    // returns !ok, moxRejected is emitted and setMox returns early.
    MoxCheckFn m_moxCheck;

    // ── Phase 3P-II Task 87: TxInterlockPolicy gate ───────────────────────────
    // Non-owning pointer. Null by default; set by RadioModel after both
    // m_moxController and m_txInterlockPolicy are constructed.
    // setMox(true) consults this AFTER the BandPlanGuard (K.2) check and
    // BEFORE the Codex P2 safety effects.
    TxInterlockPolicy* m_interlockPolicy{nullptr};

    // Cached amplifier state snapshot (updated by onAmpStateChanged).
    // Default false/false: no amp present, not in OPERATE.
    bool  m_ampPresent{false};
    bool  m_ampInOperate{false};

    // Cached SWR from PGXL meter stream (updated by onAmpSwrUpdated).
    // Default 0.0f: no SWR reading yet.  0.0f is below any non-trivial
    // swrGateMax so the gate does not falsely trip before the first meter
    // packet arrives.
    float m_lastSwr{0.0f};

    // ── Fields ───────────────────────────────────────────────────────────────

    // ── VOX gate state (H.1) ─────────────────────────────────────────────────
    // From Thetis CMSetTXAVoxRun (cmaster.cs:1039-1052 [v2.10.3.13]):
    //   Audio.VOXEnabled checked against voice-family mode enum.
    bool     m_voxEnabled{false};          // last value from TransmitModel::voxEnabled()
    // Default USB matches SliceModel m_dspMode{DSPMode::USB} default.
    DSPMode  m_currentMode{DSPMode::USB};  // last value from SliceModel::dspMode()
    // Tracks the last emitted gated value for idempotency.
    // Initial false matches (m_voxEnabled=false && ...) so no spurious emit at startup.
    bool     m_lastVoxRunGated{false};

    // ── VOX threshold state (H.2) ─────────────────────────────────────────────
    // From Thetis cmaster.cs:1054-1059 [v2.10.3.13] — CMSetTXAVoxThresh.
    // From Thetis setup.cs:18911 [v2.10.3.13] — dB→linear conversion.
    //
    // m_voxThresholdDb: raw dB from the TransmitModel slider.
    //   Default -40 matches TransmitModel::m_voxThresholdDb = -40.
    int      m_voxThresholdDb{-40};
    //
    // m_micBoost: mirrors TransmitModel::micBoost().
    //   Default true matches console.cs:13237 [v2.10.3.13]: mic_boost = true.
    bool     m_micBoost{true};
    //
    // m_voxGainScalar: mirrors TransmitModel::voxGainScalar().
    //   Default 1.0 matches audio.cs:194 [v2.10.3.13]: vox_gain = 1.0f.
    float    m_voxGainScalar{1.0f};
    //
    // m_lastVoxThresholdEmitted: NAN sentinel forces first-call emit so
    // WDSP is always primed with the correct threshold at startup.
    double   m_lastVoxThresholdEmitted{std::numeric_limits<double>::quiet_NaN()};

    // ── VOX hang-time + anti-VOX state (H.3) ─────────────────────────────────
    // From Thetis setup.cs:18896-18900 [v2.10.3.13] — SetDEXPHoldTime.
    // From Thetis setup.cs:18986-18990 [v2.10.3.13] — SetAntiVOXGain.
    // From Thetis cmaster.cs:912-943 [v2.10.3.13] — CMSetAntiVoxSourceWhat.
    //
    // m_voxHangTimeMs: raw ms from TransmitModel::voxHangTimeMs().
    //   Default 500 matches TransmitModel default (udDEXPHold designer default).
    int      m_voxHangTimeMs{500};
    //
    // m_lastVoxHangTimeEmitted: NAN sentinel forces first-call emit to prime WDSP.
    double   m_lastVoxHangTimeEmitted{std::numeric_limits<double>::quiet_NaN()};
    //
    // m_antiVoxGainDb: raw dB from TransmitModel::antiVoxGainDb().
    //   Default 0 matches TransmitModel default.
    int      m_antiVoxGainDb{0};
    //
    // m_lastAntiVoxGainEmitted: NAN sentinel forces first-call emit.
    double   m_lastAntiVoxGainEmitted{std::numeric_limits<double>::quiet_NaN()};
    //
    // m_antiVoxTauMs: raw ms from TransmitModel::antiVoxTauMs(). Range [1,500].
    //   Default 20 matches Thetis udAntiVoxTau designer default
    //   (setup.designer.cs:44685-44688 [v2.10.3.13]).
    int      m_antiVoxTauMs{20};
    //
    // m_lastAntiVoxTauEmitted: NAN sentinel forces first-call emit so the
    // WDSP DEXP block is primed at startup.  Mirrors the
    // m_lastAntiVoxGainEmitted pattern from H.3.
    double   m_lastAntiVoxTauEmitted{std::numeric_limits<double>::quiet_NaN()};
    //
    // 3M-3a-iv post-bench refactor (Option A): m_antiVoxSourceVax and
    // m_antiVoxSourceVaxInitialized members removed.  See class-header
    // comment block for the architectural rationale.
    //
    // m_antiVoxRun: mirrors TransmitModel::antiVoxRun(). Default false.
    //   3M-3a-iv scope-expansion.
    bool     m_antiVoxRun{false};
    //
    // m_antiVoxRunInitialized: false until the first accepted call to
    // setAntiVoxRun().  Forces first-call emit so TxWorkerThread/TxChannel
    // are primed even when the value matches the default.
    bool     m_antiVoxRunInitialized{false};

    // ── MOX core state ────────────────────────────────────────────────────────
    bool     m_mox{false};               // single source of truth for MOX
    MoxState m_state{MoxState::Rx};      // current state-machine position
    PttMode  m_pttMode{PttMode::None};   // current PTT mode
    // m_manualMox: true while MOX is engaged via the TUN button.
    // From Thetis console.cs:240 [v2.10.3.13]:
    //   "private bool _manual_mox; // True if the MOX button was clicked on (not PTT)"
    // Set/cleared only by setTune() — never set by setMox() directly.
    bool     m_manualMox{false};
    // m_manualKey: Thetis _manual_mox as Thetis sets it (MOX button, TUN,
    // two-tone). Gates the whole PollPTT pass. See isManualKey().
    bool     m_manualKey{false};
    // m_txInhibited / m_paTripped: Thetis _tx_inhibit and
    // _ganymede_pa_issue (console.cs:25470 [v2.10.3.15]).
    bool     m_txInhibited{false};
    QString  m_txInhibitReason;   // setTxInhibited's reason; empty for the input's own
    bool     m_paTripped{false};
    // m_rxOnly: Thetis _rx_only (Task 16), with the words a refusal shows.
    bool     m_rxOnly{false};
    QString  m_rxOnlyReason;
    // m_radioLinkDown: the link to the radio is lost (setRadioLinkDown).
    bool     m_radioLinkDown{false};
    // One predicate for the gates that block every source: the three
    // PollPTT gates and the lost radio link.
    bool     transmitBlocked() const noexcept
    {
        return m_txInhibited || m_paTripped || m_rxOnly || m_radioLinkDown;
    }
    // Emits transmitBlockChanged when transmitBlockReason() differs from
    // `before` (a gate setter's value on entry).
    void     emitTransmitBlockIfChanged(const QString& before);

    // ── PollPTT source levels (console.cs:25467-25477 [v2.10.3.15]) ──────────
    // The last level each PollPTT source reported. setMox(false) and a
    // refused key drop the CAT and TCI levels, as chkMOX_CheckedChanged2
    // clears CATPTT and TCIPTT (console.cs:29406-29411 [v2.10.3.15]).
    bool     m_micPtt{false};   // mic_ptt: PTT from radio
    bool     m_catPtt{false};   // cat_ptt
    bool     m_voxPtt{false};   // Audio.VOXActive
    bool     m_tciPtt{false};   // _tci_ptt

    // Task 7 fix wave, M3: one refusal message per press. A held source is
    // tried on every pass, as PollPTT polls; after its first refusal the
    // later ones in the same press are quiet (no moxRejected, no interlock
    // denied). A bit per source, cleared when that source's level drops.
    enum : quint8 {
        kRefusedTci = 0x01,
        kRefusedCat = 0x02,
        kRefusedMic = 0x04,
        kRefusedVox = 0x08,
    };
    quint8   m_refusedHeld{0};
    // R-R3-36 (Task 7 follow-up): the sources refused because the
    // microphone was not ready (MoxCheckResult::notQueued), same bits.
    // pollPtt does not try them, nor fall back to them, until the level
    // drops: the refusal is never queued, the operator presses again.
    // Cleared with m_refusedHeld (clearHeldBits).
    quint8   m_notQueuedHeld{0};
    // The last MOX pre-check refusal was a never-queued one. Read by
    // tryPollKey right after its setMox(true).
    bool     m_lastRefusalNotQueued{false};
    bool     isHeldOff(quint8 bit) const noexcept { return (m_notQueuedHeld & bit) != 0; }
    bool     isLevelHeld(quint8 bit) const noexcept;
    void     clearHeldBits(quint8 bits);
    // Task 77 fix round 2: emits pttSourcesReleased when none is down.
    void     reportIfSourcesReleased();
    // Task 77 fix round 2 (ruling 8.9): the radio's mic press that took
    // transmit is still down; cleared on its release. onTakeFinished keys
    // the mic only while it is set, so a later press made during the take
    // (refused "changing hands") keys nothing once the take ends.
    bool     m_micTakePressDown{false};
    // Task 7 follow-up, N3: TX inhibit or a PA trip drops the CAT and TCI
    // levels (an app's request is not held across the block).
    void     dropAppLevelsUnderBlock();
    // True only across a quiet setMox(true) from tryPollKey.
    bool     m_quietRefusal{false};
    // tryPollKey: PollPTT's `_current_ptt_mode = X; chkMOX.Checked = true;`
    // for one source, with the per-press refusal message rule.
    void tryPollKey(PttMode mode, quint8 refusedBit);

    // pollPtt: one pass of Thetis PollPTT over the recorded levels.
    void pollPtt();
    // dropPttOnUnkey: what chkMOX_CheckedChanged2 does to the PTT state on
    // an unkey (and on a refused key).
    void dropPttOnUnkey();

    // ── C.4: multicast Pre/Post rx-argument state ────────────────────────────
    // m_rx2Enabled mirrors RadioModel "RX2 enabled" flag.
    // m_vfobTx mirrors RadioModel "VFO-B is the TX VFO" flag.
    // Both default false — NereusSDR has no RX2 wired yet (3F territory).
    // activeRxForTx() returns 2 iff (m_rx2Enabled && m_vfobTx), else 1.
    //
    // From Thetis console.cs:29324 [v2.10.3.13] — MoxPreChangeHandlers and
    // Upstream tags preserved: //MW0LGE (from cited console.cs:29326) [v2.10.3.15]
    //                console.cs:29677 [v2.10.3.13] — MoxChangeHandlers:
    //   rx2_enabled && VFOBTX ? 2 : 1
    bool     m_rx2Enabled{false};
    bool     m_vfobTx{false};
    int activeRxForTx() const noexcept {
        return (m_rx2Enabled && m_vfobTx) ? 2 : 1;
    }

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
    QTimer m_txDrainTimeoutTimer; // 100 ms: Task 33, bound on the TX→RX drain wait
    QTimer m_endOfOverTailTimer;  // 1000 ms: bound on an end-of-over tail
    QTimer m_sendRingPollTimer;     // 1 ms, repeating: G-05, looks at the send ring
    QTimer m_sendRingCeilingTimer;  // the ring's length: G-05, bound on that wait

    // G-05: the unkey's wait for the send ring.
    SendRingDrain m_sendRing;
    bool m_waitingForSendRing{false};
    int  m_sendRingCeilingMs{0};

    // Task 33: the TX→RX walk waits for the TX channel's drain.
    bool m_awaitTxDrain{false};
    bool m_waitingForTxDrain{false};

    // RADE end-of-over callsigns: who starts a tail, and whether the walk
    // waits for one now.
    EndOfOverTailFn m_endOfOverTail;
    bool m_waitingForEndOfOverTail{false};

    // Task 33: StopAllTx's latch (Thetis _stop_all_tx). pollPtt consumes it
    // against the PTT source levels (m_micPtt, m_catPtt, m_voxPtt, m_tciPtt).
    bool m_stopAllTxLatched{false};

    // iPhone app plan Task 34: the keying gate, who the key now on is for,
    // and the keyer a gate-admitted setMox(true) keys for.
    KeyingGateFn  m_keyingGate;
    OtherDeviceHoldsFn m_otherDeviceHolds;
    bool m_lastAdmitTook{false};   // TGXL tune lane: see lastAdmitTook()
    KeyerIdentity m_currentKeyer{KeyerIdentity::station(PttMode::None)};
    std::optional<KeyerIdentity> m_keyAttemptIdentity;
    void setMoxImpl(bool on);
    KeyerIdentity m_admittedKeyer{KeyerIdentity::station(PttMode::None)};
    // Task 35: setTune(true, keyer) in progress, and for whom.
    KeyerIdentity m_tuneKeyer{KeyerIdentity::station(PttMode::Manual)};
    bool m_tuneForKeyer{false};
    bool          m_keyAdmitted{false};
    TxRefusal     m_lastRefusal;
};

} // namespace NereusSDR

// Qt metatype registration — required so MoxState can be carried by
// QVariant / QSignalSpy::value<MoxState>() without silently returning
// a zero-initialised value on Qt6 builds that haven't called
// qRegisterMetaType<>().  Matches the pattern in WdspTypes.h:298-305.
#include <QMetaType>
Q_DECLARE_METATYPE(NereusSDR::MoxState)
