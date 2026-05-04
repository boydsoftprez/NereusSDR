// =================================================================
// src/core/TwoToneController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. The two-tone IMD-test activation flow is a
// faithful port of the Thetis chkTestIMD_CheckedChanged handler:
//   setup.cs:11040-11191 [v2.10.3.13] — chkTestIMD_CheckedChanged
//   setup.cs:34409-34418 [v2.10.3.13] — setupTwoTonePulse() pulse profile
//   setup.cs:34387-34407 [v2.10.3.13] — updateTwoTonePulseInfo() (info only)
//   console.cs:44728-44760 [v2.10.3.13] — chk2TONE_CheckedChanged
//                 (TUN auto-stop + 300 ms settle delay)
//   setup.Designer.cs:61753-61858 [v2.10.3.13] — pulse-profile defaults
//                 (window=10 pps, percent=25 %, ramp=9 ms)
//
// Architectural deviation from plan §I.1:
//   The plan's literal text says "Implement TransmitModel::setTwoTone(bool)
//   handler". The activation flow has many side-effect dependencies
//   (MoxController, TxChannel, power management, mode lookup) that don't
//   belong on a state-only model. Style this on MoxController /
//   StepAttenuatorController / AlexController — sibling controllers, not
//   methods on the model. RadioModel will own the controller in Phase L.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-29 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//                 Phase 3M-1c chunk I (I.1-I.5) — TwoToneController class
//                 (activation handler).  Ports the chkTestIMD_CheckedChanged
//                 orchestration logic (setup.cs:11040-11191 [v2.10.3.13])
//                 plus the chk2TONE_CheckedChanged TUN auto-stop
//                 (console.cs:44728-44760 [v2.10.3.13]).  RadioModel
//                 ownership wires up in Phase L.
//   2026-05-03 — Phase 4 Agent 4B of issue #167 PA-cal safety hotfix —
//                 wires TwoToneController through Phase 3C
//                 TransmitModel::setPowerUsingTargetDbm so the PA gain
//                 compensation applies during the IMD test (txMode=2
//                 routing per console.cs:46693-46708 [v2.10.3.13]).
//                 Adds setPaProfileManager(PaProfileManager*) injection,
//                 setTwoToneActive(true) on start / false on stop, and
//                 a single bTwoTone=true wrapper invocation that pumps
//                 audio_volume through TransmitModel::audioVolumeChanged
//                 to RadioModel's TX path.  J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis-derived activation
// flow is cited inline below.

#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "core/WdspTypes.h"
#include "models/TransmitModel.h"  // for DrivePowerSource enum

namespace NereusSDR {

class MoxController;
class PaProfileManager;
class SliceModel;
class TxChannel;

// ---------------------------------------------------------------------------
// TwoToneController — orchestrates the two-tone IMD-test start/stop flow.
//
// Lives on the main thread, owned by RadioModel (wired in Phase L).
// Holds non-owning pointers to TransmitModel, TxChannel, MoxController, and
// SliceModel. Exposes a single setActive(bool on) slot that is the canonical
// activation entry point. UI surfaces (TxApplet 2-TONE button, Setup → Test
// → Two-Tone page Start/Stop button) call setActive(true/false).
//
// Activation flow (when setActive(true) is called):
//
//   1. Power-on precondition (qCWarning + return; UI surfaces its own toast).
//      From Thetis setup.cs:11063-11071 [v2.10.3.13].
//
//   2. If MOX is currently engaged, release it first and wait 200 ms before
//      continuing.  From Thetis setup.cs:11072-11077 [v2.10.3.13]:
//          if (console.MOX) {
//              Audio.MOX = false;
//              console.MOX = false;
//              await Task.Delay(200); // MW0LGE_21a
//          }
//
//   2b. If TUN is currently active, release it first and wait 300 ms before
//       continuing.  From Thetis console.cs:44732-44741 [v2.10.3.13]:
//          if (chk2TONE.Checked && chkTUN.Checked) {
//              chkTUN.Checked = false;
//              ...
//              await Task.Delay(300);
//          }
//      NereusSDR has no published "isTuneToneActive()" getter on TxChannel,
//      so the TUN auto-stop path is currently a TODO no-op (left as a
//      DONE_WITH_CONCERNS flag for follow-up).
//
//   3. Read tone parameters from TransmitModel (twoToneFreq1/2,
//      twoToneLevel, twoToneFreq2Delay, twoToneInvert, twoTonePulsed,
//      twoToneDrivePowerSource).
//      Compute ttmag1 = ttmag2 = 0.49999 * 10^(level/20).
//      MAGIC NUMBER PRESERVED VERBATIM per source-first protocol —
//      0.49999 from setup.cs:11056 [v2.10.3.13].
//
//   4. If invert tones is true AND mode is LSB / DIGL / CWL, flip both
//      freq1 and freq2 signs.  From setup.cs:11058-11062 [v2.10.3.13].
//
//   5. Pulsed vs continuous branch:
//        Pulsed: setTxPostGenTTPulseFreq(window) + setTxPostGenTTPulseDutyCycle
//                + setTxPostGenTTPulseTransition + setTxPostGenTTPulseIQOut(true)
//                + setTxPostGenMode(7) + setTxPostGenTTPulseToneFreq1/2
//                + setTxPostGenTTPulseMag1.  Mag2 deferred per Freq2Delay.
//        Continuous: setTxPostGenMode(1) + setTxPostGenTTFreq1/2
//                    + setTxPostGenTTMag1.  Mag2 deferred per Freq2Delay.
//      From setup.cs:11079-11106 [v2.10.3.13].
//
//   6. setTxPostGenRun(true).  From setup.cs:11107 [v2.10.3.13].
//
//   7. DrivePowerSource handling:
//        Fixed       → snapshot current PWR; override with twoTonePower().
//        DriveSlider → no override (use current PWR slider).
//        TuneSlider  → no override (use current TUN slider).
//      From setup.cs:11109-11120 [v2.10.3.13].
//      NereusSDR deviation: Thetis's SetPowerUsingTargetDBM helper does
//      not yet exist here; we use twoTonePower() directly as the override
//      value for Fixed mode.  See I.1 step 7 note.
//
//   8. Engage MOX via m_moxController->setMox(true).
//      The existing BandPlanGuard MoxCheckFn (3M-1b K.1) gates this: if
//      the current TX mode is CW (CWL / CWU), the request is rejected and
//      we propagate the rejection to our own twoToneActiveChanged(false).
//      From setup.cs:11122-11131 [v2.10.3.13].
//
//   9. If twoToneFreq2Delay() > 0, schedule a one-shot timer to apply
//      Mag2 after the delay (continuous → setTxPostGenTTMag2; pulsed →
//      setTxPostGenTTPulseMag2).  From setup.cs:11102-11105 + 11138-11141
//      [v2.10.3.13].
//
//  10. Set m_active = true; emit twoToneActiveChanged(true).
//
// Deactivation flow (when setActive(false) is called):
//
//   1. Release MOX via m_moxController->setMox(false), then schedule a
//      200 ms one-shot timer.  From setup.cs:11151-11152 [v2.10.3.13].
//   2. After 200 ms: setTxPostGenRun(false).  From setup.cs:11166
//      [v2.10.3.13].
//   3. Restore PWR if Fixed was active (snapshotted in step 7).
//      From setup.cs:11157-11162 [v2.10.3.13].
//   4. Set m_active = false; emit twoToneActiveChanged(false).
//
// Thread safety: all slots and signals are main-thread only.
// ---------------------------------------------------------------------------
class TwoToneController : public QObject
{
    Q_OBJECT

public:
    explicit TwoToneController(QObject* parent = nullptr);
    ~TwoToneController() override;

    // ── Pulse-profile defaults (Designer constants from
    //     setup.Designer.cs:61753-61858 [v2.10.3.13]). ─────────────────────
    //
    // window: nudPulsed_TwoTone_window default = 10 pps (Designer line 61803)
    // percent: nudPulsed_TwoTone_percent default = 25 % (Designer line 61853)
    // ramp:    nudPulsed_TwoTone_ramp default = 9 ms (Designer line 61753)
    //
    // From Thetis setup.cs:34409-34418 [v2.10.3.13] — setupTwoTonePulse():
    //   TXPostGenTTPulseFreq      = (int)nudPulsed_TwoTone_window.Value;
    //   TXPostGenTTPulseDutyCycle = (float)(nudPulsed_TwoTone_percent.Value)/100f;
    //   TXPostGenTTPulseTransition= (float)(nudPulsed_TwoTone_ramp.Value)/1000f;
    static constexpr int    kPulseWindowPpsDefault  = 10;
    static constexpr int    kPulsePercentDefault    = 25;
    static constexpr int    kPulseRampMsDefault     = 9;

    // ── Settle delay constants (NereusSDR ms; from Thetis Task.Delay
    //     await values). ────────────────────────────────────────────────────
    //
    // From Thetis setup.cs:11076 / 11152 [v2.10.3.13]:
    //   await Task.Delay(200); // MW0LGE_21a
    static constexpr int kMoxReleaseSettleMs = 200;
    //
    // From Thetis console.cs:44740 [v2.10.3.13]:
    //   await Task.Delay(300);
    static constexpr int kTuneReleaseSettleMs = 300;

    // ── Dependency injection (set once at construction time, before any
    //     setActive call). ─────────────────────────────────────────────────
    //
    // All four pointers must be non-null for setActive(true) to proceed.
    // Pass nullptr to clear (e.g. on shutdown).  TwoToneController does
    // NOT take ownership.
    void setTransmitModel(TransmitModel* tx);
    void setTxChannel(TxChannel* tx);
    void setMoxController(MoxController* mox);
    void setSliceModel(SliceModel* slice);

    // PaProfileManager injection (Phase 4B of #167) — non-owning pointer
    // to the active-PA-profile bank.  When set, setActive(true) routes the
    // two-tone start through TransmitModel::setPowerUsingTargetDbm with
    // bTwoTone=true so the PA gain compensation applies during the IMD
    // test (txMode=2 per console.cs:46693-46708 [v2.10.3.13]).  When
    // nullptr (or when activeProfile() returns nullptr — pre-load), the
    // wrapper invocation is skipped and the controller falls back to its
    // pre-Phase-4B behaviour (TXPostGen + MOX + Fixed-mode setPower
    // snapshot only).  This keeps tests + early boot states working.
    void setPaProfileManager(PaProfileManager* mgr);

    // setPowerOn: precondition gate.  setActive(true) returns early if
    // m_powerOn == false (mirrors !console.PowerOn at setup.cs:11063
    // [v2.10.3.13]).  RadioModel sets this from RadioConnection state
    // in Phase L.  Default true so unit tests don't have to flip it.
    void setPowerOn(bool on);

    // ── Test seam ──────────────────────────────────────────────────────────
    // Override the default settle / Freq2-delay timer durations.  FOR
    // TESTING ONLY — production code must use the kXxx defaults.
    // Pass 0 ms for synchronous-equivalent test behaviour.
    void setSettleDelaysMs(int moxReleaseMs, int tuneReleaseMs);

    // ── Getters ────────────────────────────────────────────────────────────
    bool isActive() const noexcept { return m_active; }

public slots:
    // setActive — canonical entry point.  Drives the full activation /
    // deactivation flow.  See class comment for the full sequence.
    //
    // Idempotent: setActive(true) when already active is a no-op.  Same
    // for setActive(false) when already inactive.
    void setActive(bool on);

signals:
    // Emitted when m_active actually changes.  Subscribers should mirror
    // their UI (button highlight, toggle state) from this signal — it is
    // the authoritative source of "two-tone is running" status.
    //
    // For setActive(true) calls that fail their precondition checks
    // (power off, MOX rejected by BandPlanGuard) this signal fires with
    // payload=false so UI can revert any optimistic-on highlight.
    void twoToneActiveChanged(bool active);

private slots:
    // Stage 2 of the activation walk: fires after the MOX-release settle
    // timer (kMoxReleaseSettleMs) when MOX was on at setActive(true) entry.
    void onMoxReleaseSettleElapsed();

    // Stage 2b of the activation walk: fires after the TUN-release settle
    // timer (kTuneReleaseSettleMs) when TUN was active at setActive(true)
    // entry.  TUN-active detection is currently a TODO (see header).
    void onTuneReleaseSettleElapsed();

    // Stage 9 of the activation walk: fires after twoToneFreq2Delay() ms.
    // Applies Mag2 (continuous → TTMag2; pulsed → TTPulseMag2).
    void onFreq2DelayElapsed();

    // Stage 1 of the deactivation walk: fires after kMoxReleaseSettleMs
    // when setActive(false) is called.  Stops the gen + restores PWR.
    void onDeactivationSettleElapsed();

    // Hooked to MoxController::moxRejected so we can clean up our state
    // when the BandPlanGuard rejects the setMox(true) call we just made.
    void onMoxRejected(const QString& reason);

private:
    // Continue the activation flow after any pending settle delay.
    // Reads parameters, applies TXPostGen* setters, computes magnitude,
    // engages MOX.
    void continueActivation();

    // Continue the deactivation flow after the 200 ms MOX-release settle.
    // Stops the gen, restores PWR, emits twoToneActiveChanged(false).
    void continueDeactivation();

    // Apply the Mag2 value (continuous or pulsed).  Called immediately
    // when Freq2Delay==0, or scheduled via QTimer::singleShot otherwise.
    void applyMag2Now();

    // Whether the current TX-side DSP mode is in the LSB-family
    // (LSB / DIGL / CWL).  Used by the invert-tones branch.
    static bool isLowerSidebandMode(DSPMode mode) noexcept;

    // Helper: read TX-side DSP mode from SliceModel (defaults to USB if
    // no slice is bound).
    DSPMode currentTxMode() const;

    // ── Wiring state ───────────────────────────────────────────────────────
    QPointer<TransmitModel>  m_tx;
    QPointer<TxChannel>      m_txChannel;
    QPointer<MoxController>  m_moxController;
    QPointer<SliceModel>     m_slice;
    // Phase 4B of #167: optional PA-profile bank for the bTwoTone=true
    // setPowerUsingTargetDbm route.  Non-owning.
    QPointer<PaProfileManager> m_paProfileManager;

    // Whether the radio is powered on.  Default true so unit tests
    // don't have to flip it; RadioModel sets it appropriately in Phase L.
    bool m_powerOn{true};

    // Activation state.
    bool m_active{false};

    // Whether we're currently in the middle of an activation walk.
    // Prevents re-entrant setActive(true) calls from double-engaging.
    bool m_activationInFlight{false};

    // Freq2Delay sub-state — true if pulsed at the time we deferred Mag2.
    bool m_pulsedAtMag2Defer{false};
    // The Mag2 value we deferred — applied by applyMag2Now().
    double m_deferredMag2{0.0};

    // PWR snapshot for Fixed-power-source restore.  Valid only while
    // m_active==true AND we entered with DrivePowerSource::Fixed.
    bool m_savedPwrValid{false};
    int  m_savedPwr{0};

    // ── Timers ──────────────────────────────────────────────────────────────
    QTimer m_moxReleaseSettleTimer;
    QTimer m_tuneReleaseSettleTimer;
    QTimer m_freq2DelayTimer;
    QTimer m_deactivationSettleTimer;
};

} // namespace NereusSDR
