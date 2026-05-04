// =================================================================
// src/core/TwoToneController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file.  Activation flow ports
// chkTestIMD_CheckedChanged (setup.cs:11040-11191 [v2.10.3.13]).
// See TwoToneController.h header for the full attribution block.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-29 — Phase 3M-1c chunk I.1-I.5 — see header.
//   2026-05-03 — Phase 4 Agent 4B of issue #167 PA-cal hotfix — wires
//                start()/stop() through Phase 3C
//                TransmitModel::setPowerUsingTargetDbm with bTwoTone=true.
//                See header for full attribution.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis-derived activation flow
// is cited inline below.

#include "TwoToneController.h"

#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "core/TxChannel.h"
#include "models/Band.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QLoggingCategory>
#include <QtMath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TwoToneController::TwoToneController(QObject* parent)
    : QObject(parent)
{
    m_moxReleaseSettleTimer.setSingleShot(true);
    m_moxReleaseSettleTimer.setInterval(kMoxReleaseSettleMs);
    connect(&m_moxReleaseSettleTimer, &QTimer::timeout,
            this, &TwoToneController::onMoxReleaseSettleElapsed);

    m_tuneReleaseSettleTimer.setSingleShot(true);
    m_tuneReleaseSettleTimer.setInterval(kTuneReleaseSettleMs);
    connect(&m_tuneReleaseSettleTimer, &QTimer::timeout,
            this, &TwoToneController::onTuneReleaseSettleElapsed);

    m_freq2DelayTimer.setSingleShot(true);
    connect(&m_freq2DelayTimer, &QTimer::timeout,
            this, &TwoToneController::onFreq2DelayElapsed);

    m_deactivationSettleTimer.setSingleShot(true);
    m_deactivationSettleTimer.setInterval(kMoxReleaseSettleMs);
    connect(&m_deactivationSettleTimer, &QTimer::timeout,
            this, &TwoToneController::onDeactivationSettleElapsed);
}

TwoToneController::~TwoToneController() = default;

// ---------------------------------------------------------------------------
// Dependency injection
// ---------------------------------------------------------------------------
void TwoToneController::setTransmitModel(TransmitModel* tx)
{
    m_tx = tx;
}

void TwoToneController::setTxChannel(TxChannel* tx)
{
    m_txChannel = tx;
}

void TwoToneController::setMoxController(MoxController* mox)
{
    // Disconnect any previous moxRejected hookup before swapping.
    if (m_moxController) {
        disconnect(m_moxController, &MoxController::moxRejected,
                   this, &TwoToneController::onMoxRejected);
    }
    m_moxController = mox;
    if (m_moxController) {
        connect(m_moxController, &MoxController::moxRejected,
                this, &TwoToneController::onMoxRejected,
                Qt::UniqueConnection);
    }
}

void TwoToneController::setSliceModel(SliceModel* slice)
{
    m_slice = slice;
}

void TwoToneController::setPaProfileManager(PaProfileManager* mgr)
{
    // Phase 4B of #167.  Non-owning pointer.  RadioModel injects the
    // manager on connect; tests inject a manager they own directly.
    // nullptr -> Phase 4B wrapper invocation is skipped and the
    // controller falls back to its pre-Phase-4B behaviour (TXPostGen +
    // MOX + Fixed-mode setPower snapshot only).
    m_paProfileManager = mgr;
}

void TwoToneController::setPowerOn(bool on)
{
    m_powerOn = on;
}

void TwoToneController::setSettleDelaysMs(int moxReleaseMs, int tuneReleaseMs)
{
    m_moxReleaseSettleTimer.setInterval(moxReleaseMs);
    m_tuneReleaseSettleTimer.setInterval(tuneReleaseMs);
    m_deactivationSettleTimer.setInterval(moxReleaseMs);
}

// ---------------------------------------------------------------------------
// setActive — canonical entry point
// ---------------------------------------------------------------------------
//
// From Thetis setup.cs:11040-11191 [v2.10.3.13] — chkTestIMD_CheckedChanged.
// ---------------------------------------------------------------------------
void TwoToneController::setActive(bool on)
{
    if (on == m_active && !m_activationInFlight) {
        // Idempotent: already in the requested state and not mid-walk.
        return;
    }

    if (on) {
        // ── Stage 1: power-on precondition.  From Thetis setup.cs:11063-11071
        //     [v2.10.3.13]:
        //       if (!console.PowerOn) {
        //           MessageBox.Show("Power must be on to run this test.", ...);
        //           chkTestIMD.Checked = false;
        //           return;
        //       }
        if (!m_powerOn) {
            qCWarning(lcDsp).noquote()
                << "TwoToneController: power must be on to run two-tone test "
                   "— ignoring activation request.";
            // Emit a transition to false so any optimistic UI highlight
            // gets reverted.  setActive(false) on inactive is harmless,
            // but skip the timer walk by emitting directly.
            if (m_active) {
                m_active = false;
                emit twoToneActiveChanged(false);
            }
            return;
        }

        if (!m_tx || !m_txChannel || !m_moxController) {
            qCWarning(lcDsp).noquote()
                << "TwoToneController: missing dependencies (tx/txChannel/mox); "
                   "cannot activate.";
            return;
        }

        m_activationInFlight = true;

        // ── Stage 2: if MOX is currently engaged, release first.  From Thetis
        //     setup.cs:11072-11077 [v2.10.3.13]:
        //       if (console.MOX) {
        //           Audio.MOX = false;
        //           console.MOX = false;
        //           await Task.Delay(200); // MW0LGE_21a
        //       }
        if (m_moxController->isMox()) {
            m_moxController->setMox(false);
            m_moxReleaseSettleTimer.start();
            return;
        }

        // ── Stage 2b: if TUN is currently active, release first.  From Thetis
        //     console.cs:44732-44741 [v2.10.3.13] — chk2TONE_CheckedChanged:
        //       if (chk2TONE.Checked && chkTUN.Checked) {
        //           chkTUN.Checked = false;
        //           ...
        //           await Task.Delay(300);
        //       }
        //
        // NOTE: TxChannel currently has no published "isTuneToneActive()"
        //       getter (the gen1 PostGen state is internal).  Adding one
        //       just for this check is out of I scope; punt the auto-stop
        //       path until a future polish phase that surfaces TUN state
        //       upward.  See I.3 note in the plan + DONE_WITH_CONCERNS.
        // TODO(3M-1c-polish): expose TxChannel TUN-active state so we can
        //                     trigger m_tuneReleaseSettleTimer here.

        continueActivation();
    } else {
        // Deactivation.  From Thetis setup.cs:11149-11177 [v2.10.3.13].
        if (!m_active && !m_activationInFlight) {
            return;
        }

        // Cancel any in-flight activation timers before starting teardown.
        m_moxReleaseSettleTimer.stop();
        m_tuneReleaseSettleTimer.stop();
        m_freq2DelayTimer.stop();

        if (m_moxController) {
            m_moxController->setMox(false);
        }
        // From Thetis setup.cs:11151-11152 [v2.10.3.13]:
        //   console.MOX = false;
        //   await Task.Delay(200); // MW0LGE_21a
        m_deactivationSettleTimer.start();
    }
}

// ---------------------------------------------------------------------------
// onMoxReleaseSettleElapsed — Stage 2 of activation
// ---------------------------------------------------------------------------
void TwoToneController::onMoxReleaseSettleElapsed()
{
    // After the 200 ms MOX-release settle, continue the activation walk.
    // (Stage 2b — TUN auto-stop — is currently a TODO; if it lands later,
    // it would chain in here.)
    continueActivation();
}

// ---------------------------------------------------------------------------
// onTuneReleaseSettleElapsed — Stage 2b of activation
// ---------------------------------------------------------------------------
void TwoToneController::onTuneReleaseSettleElapsed()
{
    // From Thetis console.cs:44740 [v2.10.3.13]:
    //   await Task.Delay(300);
    continueActivation();
}

// ---------------------------------------------------------------------------
// continueActivation — stages 3-10 of the activation flow
// ---------------------------------------------------------------------------
void TwoToneController::continueActivation()
{
    if (!m_tx || !m_txChannel || !m_moxController) {
        m_activationInFlight = false;
        return;
    }

    // ── Stage 3: read tone parameters from TransmitModel and compute the
    //     magnitude.  From Thetis setup.cs:11052-11056 [v2.10.3.13]:
    //       double ttfreq1 = (double)udTestIMDFreq1.Value;
    //       double ttfreq2 = (double)udTestIMDFreq2.Value;
    //       double ttmag = (double)udTwoToneLevel.Value;
    //       double ttmag1, ttmag2;
    //       ttmag1 = ttmag2 = 0.49999 * Math.Pow(10.0, ttmag / 20.0);
    //
    // The literal 0.49999 MUST be preserved verbatim per source-first
    // protocol (CLAUDE.md "Constants and Magic Numbers").
    double ttfreq1 = static_cast<double>(m_tx->twoToneFreq1());
    double ttfreq2 = static_cast<double>(m_tx->twoToneFreq2());
    const double ttLevel = m_tx->twoToneLevel();
    const double ttmag1 = 0.49999 * std::pow(10.0, ttLevel / 20.0); // setup.cs:11056 [v2.10.3.13]
    const double ttmag2 = ttmag1; // ttmag1 = ttmag2 = ... [setup.cs:11056]

    // ── Stage 4: mode-aware invert.  From Thetis setup.cs:11057-11062
    //     [v2.10.3.13]:
    //       DSPMode mode = console.radio.GetDSPTX(0).CurrentDSPMode;
    //       if (chkInvertTones.Checked && ((mode == DSPMode.CWL) ||
    //                                      (mode == DSPMode.DIGL) ||
    //                                      (mode == DSPMode.LSB))) {
    //           ttfreq1 = -ttfreq1;
    //           ttfreq2 = -ttfreq2;
    //       }
    const DSPMode txMode = currentTxMode();
    if (m_tx->twoToneInvert() && isLowerSidebandMode(txMode)) {
        ttfreq1 = -ttfreq1;
        ttfreq2 = -ttfreq2;
    }

    // ── Stage 5: pulsed vs continuous branch.  From Thetis setup.cs:11079-
    //     11106 [v2.10.3.13].
    const bool pulsed = m_tx->twoTonePulsed();
    const int  freq2DelayMs = m_tx->twoToneFreq2Delay();

    if (pulsed) {
        // From Thetis setup.cs:11083-11092 [v2.10.3.13]:
        //   setupTwoTonePulse();
        //   console.radio.GetDSPTX(0).TXPostGenMode = 7; // pulsed two tone
        //   console.radio.GetDSPTX(0).TXPostGenTTPulseToneFreq1 = ttfreq1;
        //   console.radio.GetDSPTX(0).TXPostGenTTPulseToneFreq2 = ttfreq2;
        //   console.radio.GetDSPTX(0).TXPostGenTTPulseMag1 = ttmag1;
        //   if ((int)udFreq2Delay.Value == 0)
        //       console.radio.GetDSPTX(0).TXPostGenTTPulseMag2 = ttmag2;
        //   else
        //       console.radio.GetDSPTX(0).TXPostGenTTPulseMag2 = 0.0;
        //
        // setupTwoTonePulse() applies the Designer-default pulse profile.
        // From Thetis setup.cs:34409-34418 [v2.10.3.13]:
        //   TXPostGenTTPulseIQOut      = true;                                  [line 34414]
        //   TXPostGenTTPulseFreq       = (int)nudPulsed_TwoTone_window.Value;   [line 34415]
        //   TXPostGenTTPulseDutyCycle  = (float)(percent.Value)/100f;            [line 34416]
        //   TXPostGenTTPulseTransition = (float)(ramp.Value)/1000f;              [line 34417]
        m_txChannel->setTxPostGenTTPulseIQOut(true);
        m_txChannel->setTxPostGenTTPulseFreq(kPulseWindowPpsDefault);
        m_txChannel->setTxPostGenTTPulseDutyCycle(
            static_cast<double>(kPulsePercentDefault) / 100.0);
        m_txChannel->setTxPostGenTTPulseTransition(
            static_cast<double>(kPulseRampMsDefault) / 1000.0);

        m_txChannel->setTxPostGenMode(7);
        m_txChannel->setTxPostGenTTPulseToneFreq1(ttfreq1);
        m_txChannel->setTxPostGenTTPulseToneFreq2(ttfreq2);
        m_txChannel->setTxPostGenTTPulseMag1(ttmag1);

        if (freq2DelayMs == 0) {
            m_txChannel->setTxPostGenTTPulseMag2(ttmag2);
        } else {
            m_txChannel->setTxPostGenTTPulseMag2(0.0);
        }
    } else {
        // From Thetis setup.cs:11096-11105 [v2.10.3.13]:
        //   console.radio.GetDSPTX(0).TXPostGenMode = 1;
        //   console.radio.GetDSPTX(0).TXPostGenTTFreq1 = ttfreq1;
        //   console.radio.GetDSPTX(0).TXPostGenTTFreq2 = ttfreq2;
        //   console.radio.GetDSPTX(0).TXPostGenTTMag1 = ttmag1;
        //   //MW0LGE_21a change to delay Freq2 output. Fixes problems with some Amps frequency counters
        //   if ((int)udFreq2Delay.Value == 0)
        //       console.radio.GetDSPTX(0).TXPostGenTTMag2 = ttmag2;
        //   else
        //       console.radio.GetDSPTX(0).TXPostGenTTMag2 = 0.0;
        m_txChannel->setTxPostGenMode(1);
        m_txChannel->setTxPostGenTTFreq1(ttfreq1);
        m_txChannel->setTxPostGenTTFreq2(ttfreq2);
        m_txChannel->setTxPostGenTTMag1(ttmag1);

        // MW0LGE_21a change to delay Freq2 output. Fixes problems with
        // some Amps frequency counters  [from setup.cs:11101 [v2.10.3.13]]
        if (freq2DelayMs == 0) {
            m_txChannel->setTxPostGenTTMag2(ttmag2);
        } else {
            m_txChannel->setTxPostGenTTMag2(0.0);
        }
    }

    // ── Stage 6: setTxPostGenRun(true).  From Thetis setup.cs:11107
    //     [v2.10.3.13]:
    //       console.radio.GetDSPTX(0).TXPostGenRun = 1;
    m_txChannel->setTxPostGenRun(true);

    // ── Stage 7: DrivePowerSource handling.  From Thetis setup.cs:11109-
    //     11120 [v2.10.3.13]:
    //       //MW0LGE_22b
    //       // remember old power //MW0LGE_22b
    //       if (console.TwoToneDrivePowerOrigin == DrivePowerSource.FIXED)
    //           console.PreviousPWR = console.PWR;
    //       // set power
    //       int new_pwr = console.SetPowerUsingTargetDBM(out bool bUseConstrain,
    //                                                    out double targetdBm,
    //                                                    true, true, true);
    //       if (console.TwoToneDrivePowerOrigin == DrivePowerSource.FIXED) {
    //           console.PWRSliderLimitEnabled = false;
    //           console.PWR = new_pwr;
    //       }
    //
    // NereusSDR deviation: SetPowerUsingTargetDBM is a Thetis-internal helper
    // that doesn't yet exist here.  The cleanest semantic match for "Fixed
    // mode" is to use twoTonePower() (the Setup-page-fixed value) directly
    // as the override.  See I.1 step 7 note.
    m_savedPwrValid = false;
    if (m_tx->twoToneDrivePowerSource() == DrivePowerSource::Fixed) {
        m_savedPwr = m_tx->power();
        m_savedPwrValid = true;
        m_tx->setPower(m_tx->twoTonePower());
    }

    // ── Stage 8: engage MOX.  From Thetis setup.cs:11122-11131 [v2.10.3.13]:
    //       console.ManualMox = true;
    //       console.TwoTone = true; // MW0LGE_21a
    //       Audio.MOX = true;
    //       console.MOX = true;
    //       if (!console.MOX) {
    //           chkTestIMD.Checked = false;
    //           return;
    //       }
    //
    // The (!console.MOX) check above corresponds to BandPlanGuard rejecting
    // the request.  The MoxController emits moxRejected(...) on rejection;
    // we catch that via onMoxRejected() and run the cleanup there.
    m_moxController->setMox(true);

    // If the setMox call above resulted in immediate rejection (synchronous
    // moxRejected emission), m_active will already be false here and we
    // should not commit.  Use isMox() to detect: a successful setMox(true)
    // walk completes synchronously when MoxController has 0-ms test timers,
    // but most importantly, on rejection isMox() will still be false.
    //
    // In production (real timer durations), MOX is mid-walk after setMox(true)
    // returns; we still commit m_active=true here because the walk WILL
    // complete unless rejected (and rejection is signalled synchronously by
    // moxRejected — see onMoxRejected which clears m_active synchronously).
    if (!m_activationInFlight) {
        // onMoxRejected fired synchronously; nothing more to do.
        return;
    }

    // ── Stage 8b: Phase 4B of #167 — PA-cal hotfix integration.
    //
    // From Thetis console.cs:46693-46708 [v2.10.3.13] — chk2TONE_CheckedChanged
    // SetPowerUsingTargetDBM txMode=2 drive-source enum routing.  In Thetis,
    // chk2TONE.Checked is the runtime mirror that drives txMode=2 inside
    // SetPowerUsingTargetDBM (console.cs:46667-46668).  The wrapper invocation
    // routes through the active drive-source enum (DriveSlider / TuneSlider /
    // Fixed) and emits TransmitModel::audioVolumeChanged so RadioModel can
    // pump audio_volume to TxChannel (iq_gain) + RadioConnection (wire_byte).
    //
    // Sequenced AFTER MOX engagement (Stage 8) so:
    //   - if BandPlanGuard rejected MOX, we never emit audio_volume to the
    //     wire (onMoxRejected has already fired and m_activationInFlight is
    //     cleared above; we return early then).
    //   - the MOX state is consistent with Thetis console.cs:11122-11125
    //     ordering (ManualMox = TwoTone = MOX = true) before the wrapper
    //     reads it.
    //
    // Skips when:
    //   - m_paProfileManager is null (test seam + pre-RadioModel-wired state).
    //   - activeProfile() returns null (manager exists but unloaded).
    // In both cases the controller falls back to its pre-Phase-4B behaviour
    // (TXPostGen + MOX + Fixed-mode setPower snapshot only).  This keeps the
    // existing tst_two_tone_controller suite green and matches early-boot
    // RadioModel state where the manager hasn't loaded yet.
    if (m_paProfileManager) {
        if (const PaProfile* profile = m_paProfileManager->activeProfile()) {
            // Mark TransmitModel two-tone-active FIRST so the wrapper's
            // txMode determination resolves to 2 (per console.cs:46667-46668
            // [v2.10.3.13] — else if (chk2TONE.Checked) txMode = 2).
            m_tx->setTwoToneActive(true);

            // Resolve current TX band from the slice.  bandFromFrequency
            // never returns Band::XVTR — XVTR slot is set explicitly by UI
            // when transverter mode is active and isn't yet wired here
            // (deferred per plan §"Open follow-ups").  GEN/SWL bands fall
            // through to the sentinel fallback in computeAudioVolume
            // (PaProfile::getGainForBand returns 1000.0f for Band::XVTR
            // and out-of-range Bands → linear fallback in the math kernel).
            const Band band = m_slice
                ? bandFromFrequency(m_slice->frequency())
                : Band::Band20m;

            // From Thetis console.cs:46693-46708 [v2.10.3.13] —
            // chk2TONE.Checked txMode=2 drive-source enum routing.  Caller
            // composes wire_byte + iq_gain from result.audioVolume; that's
            // RadioModel's job (wired in Phase 4A).  Here we just need the
            // audioVolumeChanged signal to fire so the TX path picks up the
            // PA-cal-corrected volume.
            (void) m_tx->setPowerUsingTargetDbm(
                *profile, band,
                /*bSetPower=*/true,
                /*bFromTune=*/false,
                /*bTwoTone=*/true);
        }
    }

    // ── Stage 9: Freq2Delay deferred Mag2.  From Thetis setup.cs:11134-11142
    //     [v2.10.3.13]:
    //       if ((int)udFreq2Delay.Value > 0) {
    //           await Task.Delay((int)udFreq2Delay.Value);
    //           if (pulsed)
    //               console.radio.GetDSPTX(0).TXPostGenTTPulseMag2 = ttmag2;
    //           else
    //               console.radio.GetDSPTX(0).TXPostGenTTMag2 = ttmag2;
    //       }
    if (freq2DelayMs > 0) {
        m_pulsedAtMag2Defer = pulsed;
        m_deferredMag2 = ttmag2;
        m_freq2DelayTimer.setInterval(freq2DelayMs);
        m_freq2DelayTimer.start();
    }

    // ── Stage 10: commit active state.
    m_activationInFlight = false;
    if (!m_active) {
        m_active = true;
        emit twoToneActiveChanged(true);
    }
}

// ---------------------------------------------------------------------------
// onFreq2DelayElapsed — Stage 9 of activation
// ---------------------------------------------------------------------------
void TwoToneController::onFreq2DelayElapsed()
{
    if (!m_active) {
        return; // raced with deactivation
    }
    applyMag2Now();
}

// ---------------------------------------------------------------------------
// applyMag2Now
// ---------------------------------------------------------------------------
void TwoToneController::applyMag2Now()
{
    if (!m_txChannel) {
        return;
    }
    // From Thetis setup.cs:11138-11141 [v2.10.3.13]:
    //   if (pulsed)
    //       console.radio.GetDSPTX(0).TXPostGenTTPulseMag2 = ttmag2;
    //   else
    //       console.radio.GetDSPTX(0).TXPostGenTTMag2 = ttmag2;
    if (m_pulsedAtMag2Defer) {
        m_txChannel->setTxPostGenTTPulseMag2(m_deferredMag2);
    } else {
        m_txChannel->setTxPostGenTTMag2(m_deferredMag2);
    }
}

// ---------------------------------------------------------------------------
// onDeactivationSettleElapsed — Stage 1 of deactivation completes
// ---------------------------------------------------------------------------
void TwoToneController::onDeactivationSettleElapsed()
{
    continueDeactivation();
}

// ---------------------------------------------------------------------------
// continueDeactivation
// ---------------------------------------------------------------------------
void TwoToneController::continueDeactivation()
{
    // From Thetis setup.cs:11151-11177 [v2.10.3.13]:
    //   console.MOX = false;
    //   await Task.Delay(200); // MW0LGE_21a
    //   Audio.MOX = false;
    //   console.ManualMox = false;
    //   console.TwoTone = false; // MW0LGE_21a
    //
    //   //MW0LGE_22b
    //   if (console.TwoToneDrivePowerOrigin == DrivePowerSource.FIXED) {
    //       console.PWRSliderLimitEnabled = true;
    //       console.PWR = console.PreviousPWR;
    //   }
    //
    //   chkTestIMD.BackColor = SystemColors.Control;
    //   console.psform.TTgenON = false;
    //   console.radio.GetDSPTX(0).TXPostGenRun = 0;

    if (m_txChannel) {
        m_txChannel->setTxPostGenRun(false);
    }

    if (m_savedPwrValid && m_tx) {
        m_tx->setPower(m_savedPwr);
        m_savedPwrValid = false;
    }

    // Phase 4B of #167: clear the TransmitModel two-tone-active mirror.
    // The next setPowerUsingTargetDbm call (e.g. from RadioModel's
    // powerChanged lambda when MOX drops) will see m_twoToneActive=false
    // and route through txMode=0 (normal mode), restoring the normal-mode
    // audio_volume to the wire.  Phase 4B intentionally does NOT itself
    // call setPowerUsingTargetDbm here — it would emit a "ghost" volume
    // against now-stale 2-tone state.
    //
    // Cite: console.cs:11151-11177 [v2.10.3.13] — setActive(false) maps
    //       console.TwoTone = false at line 11154.
    if (m_tx) {
        m_tx->setTwoToneActive(false);
    }

    if (m_active) {
        m_active = false;
        emit twoToneActiveChanged(false);
    }
    m_activationInFlight = false;
}

// ---------------------------------------------------------------------------
// onMoxRejected — BandPlanGuard rejected our setMox(true) call
// ---------------------------------------------------------------------------
//
// I.5 contract: the existing 3M-1b K.1 SSB-mode allow-list rule already
// rejects CW for SSB-related operations, including two-tone (which runs
// on the SSB modulator stage).  We don't add a new BandPlanGuard rule
// for two-tone — we just clean up our own state if MoxController emits
// moxRejected after we called setMox(true).
//
// Cleanup:
//   - clear m_activationInFlight so continueActivation() returns early.
//   - revert PWR if we snapshotted it for Fixed mode.
//   - if we had committed m_active=true, revert and emit false.
// ---------------------------------------------------------------------------
void TwoToneController::onMoxRejected(const QString& reason)
{
    Q_UNUSED(reason);

    if (!m_activationInFlight && !m_active) {
        // Not our request — ignore rejection.
        return;
    }

    // Stop any in-flight activation timers.
    m_moxReleaseSettleTimer.stop();
    m_tuneReleaseSettleTimer.stop();
    m_freq2DelayTimer.stop();

    // Tear down the gen if it was started in continueActivation.
    if (m_txChannel) {
        m_txChannel->setTxPostGenRun(false);
    }

    // Restore PWR if we had snapshotted it.
    if (m_savedPwrValid && m_tx) {
        m_tx->setPower(m_savedPwr);
        m_savedPwrValid = false;
    }

    // Phase 4B of #167: clear the TransmitModel two-tone-active mirror in
    // case it was set in continueActivation Stage 8b before MOX rejection
    // landed.  Idempotent (TransmitModel::setTwoToneActive guards
    // duplicates).
    if (m_tx) {
        m_tx->setTwoToneActive(false);
    }

    m_activationInFlight = false;

    if (m_active) {
        m_active = false;
        emit twoToneActiveChanged(false);
    } else {
        // Emit twoToneActiveChanged(false) anyway so UI can revert any
        // optimistic-on highlight (TxApplet 2-TONE button toggle).
        emit twoToneActiveChanged(false);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool TwoToneController::isLowerSidebandMode(DSPMode mode) noexcept
{
    // From Thetis setup.cs:11058 [v2.10.3.13]:
    //   ((mode == DSPMode.CWL) || (mode == DSPMode.DIGL) || (mode == DSPMode.LSB))
    return mode == DSPMode::LSB
        || mode == DSPMode::DIGL
        || mode == DSPMode::CWL;
}

DSPMode TwoToneController::currentTxMode() const
{
    if (m_slice) {
        return m_slice->dspMode();
    }
    // Default USB mirrors SliceModel::m_dspMode default.
    return DSPMode::USB;
}

} // namespace NereusSDR
