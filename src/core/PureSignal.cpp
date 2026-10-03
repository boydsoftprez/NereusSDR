// no-port-check: NereusSDR-original wrapper class.  See PureSignal.h
// header banner for the rationale.
//
// =================================================================
// src/core/PureSignal.cpp  (NereusSDR)
// =================================================================
//
// Implementation of PureSignal coordinator — see PureSignal.h for the
// design.  All ported logic carries explicit Thetis cites so the GPL
// attribution chain is clear and the inline-tag preservation script can
// verify the upstream linkage.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Created by J.J. Boyd (KG4VCF) for Phase 3M-4 Task 7
//                 PureSignal coordinator, with AI-assisted source-first
//                 protocol via Anthropic Claude Code.
//   2026-09-25 : R-R3-39 (station Task 32) by J.J. Boyd (KG4VCF): with a
//                 transmit lane the TX delay is applied there and reported
//                 back (psTxDelayApplied), the correction stop is chosen
//                 there from the RF gate, and the poll reads the status the
//                 lane caches. AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#include "PureSignal.h"

#include "LogCategories.h"
#include "MoxController.h"
#include "models/PureSignalSettings.h"
#include "PsFeedbackChannel.h"
#include "StepAttenuatorController.h"
#include "TwoToneController.h"
#include "TxChannel.h"
#include "WdspEngine.h"

#include <QLoggingCategory>
#include <QDateTime>

#include <cmath>
#include <algorithm>
#include <iterator>
#include <utility>

namespace NereusSDR {

// 100 ms cadence — same as Thetis's PSForm thread loop (PSForm.cs:154-186
// [v2.10.3.13] PSLoop runs timer1code every 10 ms when _power and counts
// 10 cycles between timer2code calls; net is one timer1 per 10 ms and one
// timer2 per ~100 ms).  NereusSDR uses 100 ms for both — the Thetis 10 ms
// timer1 cadence was for fine-grained _GetPSpeakval polling on a label;
// for Q_PROPERTY signal updates 100 ms is enough and avoids a hot timer
// on the main thread.  AmpView (Task 9) does its own faster polling for
// the chart update.
static constexpr int kPollIntervalMs = 100;
static constexpr int kAutoAttIntervalMs = 100;

PureSignal::PureSignal(WdspEngine* engine,
                       TxChannel* tx,
                       PsFeedbackChannel* fb,
                       MoxController* mox,
                       StepAttenuatorController* stepAtt,
                       TwoToneController* twoTone,
                       QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_tx(tx)
    , m_fb(fb)
    , m_mox(mox)
    , m_stepAtt(stepAtt)
    , m_twoTone(twoTone)
{
    m_fallbackSettings = new PureSignalSettings(this);
    m_settings = m_fallbackSettings;
    bindSettingsSignals();
    // Legacy direct-construction tests use the owned defaults immediately.
    // RadioModel replaces this object before initialization, which resets the
    // operational gate until applyAcceptedSettingsToEngine() succeeds.
    m_operationalSettingsApplied = true;
    // Wire QTimer cadence and connect ticks.  Timers are started by
    // setEnabled(true); we don't start them in the ctor so a freshly-
    // constructed PureSignal sitting idle (e.g. before any user enables
    // PS-A) doesn't burn CPU on the main thread.
    m_pollTimer.setInterval(kPollIntervalMs);
    m_pollTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_pollTimer, &QTimer::timeout, this, &PureSignal::pollTimerTick);

    m_autoAttTimer.setInterval(kAutoAttIntervalMs);
    m_autoAttTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_autoAttTimer, &QTimer::timeout, this,
            &PureSignal::autoAttentionTick);

    // From Thetis PSForm.cs:1064-1071 [v2.10.3.13] — `static puresignal()`
    // zero-fills the 16-int info arrays.  std::array zero-init in the
    // header takes care of that here, but be explicit so a future
    // refactor doesn't drift.
    std::memset(m_info, 0, sizeof(m_info));
    std::memset(m_oldInfo, 0, sizeof(m_oldInfo));

    // MoxController fan-out — drive SetPSMox(true/false) on RX↔TX flip.
    // PSEnabled property (PSForm.cs:202-269 [v2.10.3.13]) does the
    // SetPSControl-fanout half; SetPSMox is the per-MOX-event signal.
    if (m_mox) {
        connect(m_mox, &MoxController::hardwareFlipped, this,
                &PureSignal::onMoxChanged);
    }

    // Phase 3M-4 bench-fix: auto-start the poll + auto-attention timers
    // on construction.  Earlier comment said "don't start them in the ctor
    // so a freshly-constructed PureSignal sitting idle doesn't burn CPU"
    // — but no UI path calls setEnabled(true), so the poll loop never ran
    // and getPSInfo / FB level / corrections-being-applied / cal counters
    // never updated.  Mirrors Thetis PSForm starting timer1 on Show.
    // 100ms timer reading getPSInfo is trivially cheap; calcc returns
    // LRESET (zero-fill) when no TX so emit traffic is also zero.
    m_enabled = true;
    m_pollTimer.start();
    m_autoAttTimer.start();

    connectTxChannelSignals();
}

PureSignal::~PureSignal()
{
    m_pollTimer.stop();
    m_autoAttTimer.stop();
    retirePendingFileOperation();
    // Best-effort: leave the WDSP engine in a clean state on destruction.
    // From Thetis PSForm.cs:140-145 [v2.10.3.13] (StopPSThread sets
    // _ps_closing then joins; we rely on QTimer::stop being synchronous
    // on the main thread).
    if (m_tx) {
        m_tx->setPSMox(false);
        requestNativeCorrectionStop();
    }
}

void PureSignal::setTxChannel(TxChannel* tx)
{
    if (tx != m_tx) {
        retireSessionOperations();
        m_operationalSettingsApplied = false;
        if (m_tx) {
            disconnect(m_tx, &TxChannel::psTxDelayApplied, this, nullptr);
        }
    }
    m_tx = tx;
    connectTxChannelSignals();
}

void PureSignal::connectTxChannelSignals()
{
    if (!m_tx) {
        return;
    }
    // R-R3-39: with a transmit lane the applied TX delay arrives from there.
    // One connection per channel: any earlier one is dropped first.
    disconnect(m_tx, &TxChannel::psTxDelayApplied, this, nullptr);
    connect(m_tx, &TxChannel::psTxDelayApplied, this,
            [this](double actualSeconds) { noteAppliedTxDelayNs(actualSeconds * 1.0e9); });
}

void PureSignal::applyTxDelaySeconds(double seconds)
{
    if (!m_tx) {
        return;
    }
    if (m_tx->controlLane() != nullptr) {
        // R-R3-39: SetPSTXDelay runs on the transmit lane, which reports the
        // delay it applied through psTxDelayApplied.
        m_tx->requestPSTXDelay(seconds);
        return;
    }
    noteAppliedTxDelayNs(m_tx->setPSTXDelay(seconds) * 1.0e9);
}

void PureSignal::noteAppliedTxDelayNs(double actual)
{
    if (actual != m_appliedTxDelayNs) {
        m_appliedTxDelayNs = actual;
        emit appliedTxDelayNsChanged(actual);
    }
}

void PureSignal::setPsFeedbackChannel(PsFeedbackChannel* fb)
{
    m_fb = fb;
}

void PureSignal::setSettings(PureSignalSettings* settings)
{
    PureSignalSettings* replacement = settings ? settings : m_fallbackSettings;
    if (replacement == m_settings) return;
    if (m_settings) disconnect(m_settings, nullptr, this, nullptr);
    m_settings = replacement;
    m_operationalSettingsApplied = false;
    bindSettingsSignals();
}

PureSignalSettings* PureSignal::settings() const noexcept
{
    return m_settings.data();
}

void PureSignal::setOperationalPermissionPredicate(OperationalPredicate predicate)
{
    m_permissionPredicate = std::move(predicate);
}

void PureSignal::setOperationalReadinessPredicate(OperationalPredicate predicate)
{
    m_readinessPredicate = std::move(predicate);
}

bool PureSignal::canActuate() const
{
    return (!m_permissionPredicate || m_permissionPredicate())
        && (!m_readinessPredicate || m_readinessPredicate());
}

bool PureSignal::isAutoCalEnabled() const noexcept
{
    return m_settings && m_settings->autoCalEnabled();
}

bool PureSignal::autoAttenuate() const noexcept
{
    return m_settings && m_settings->autoAttenuate();
}

bool PureSignal::quickAttenuate() const noexcept
{
    return m_settings && m_settings->quickAttenuate();
}

double PureSignal::moxDelay() const noexcept
{
    // Fix wave RD-I7: with no settings, Thetis udPSMoxDelay's Value 0.2.
    // From Thetis PSForm.Designer.cs:368-372 [v2.10.3.15].
    return m_settings ? m_settings->moxDelaySeconds() : PureSignalSettingsValues{}.moxDelaySeconds;
}

double PureSignal::calDelay() const noexcept
{
    return m_settings ? m_settings->loopDelaySeconds() : 0.0;
}

int PureSignal::ampDelay() const noexcept
{
    return m_settings ? static_cast<int>(std::lround(m_settings->requestedTxDelayNs())) : 150;
}

bool PureSignal::runCalibrationProcessing() const noexcept
{
    return m_settings && m_settings->runCalibrationProcessing();
}

double PureSignal::hwPeak() const noexcept
{
    if (m_settings && m_settings->hardwarePeakOverrideEnabled()) {
        return m_settings->hardwarePeakOverride();
    }
    return m_caps.psDefaultPeak;
}

void PureSignal::bindSettingsSignals()
{
    if (!m_settings) return;
    connect(m_settings, &PureSignalSettings::autoCalEnabledChanged, this,
            [this](bool enabled) {
        emit autoCalEnabledChanged(enabled);
        if (!m_operationalSettingsApplied || m_settingsHydrationDepth > 0) return;
        if (enabled) startAutomaticCalibration();
        else requestOperationalStop();
    });
    connect(m_settings, &PureSignalSettings::runCalibrationProcessingChanged,
            this, [this](bool run) {
        if (!m_operationalSettingsApplied || !m_tx
            || m_settingsHydrationDepth > 0) return;
        if (!run || canActuate()) m_tx->setPSRunCal(run ? 1 : 0);
    });
    connect(m_settings, &PureSignalSettings::autoAttenuateChanged,
            this, &PureSignal::autoAttenuateChanged);
    connect(m_settings, &PureSignalSettings::quickAttenuateChanged,
            this, &PureSignal::quickAttenuateChanged);
    connect(m_settings, &PureSignalSettings::moxDelaySecondsChanged,
            this, [this](double value) {
        if (m_operationalSettingsApplied && m_tx
            && m_settingsHydrationDepth == 0) m_tx->setPSMoxDelay(value);
        emit moxDelayChanged(value);
    });
    connect(m_settings, &PureSignalSettings::loopDelaySecondsChanged,
            this, [this](double value) {
        if (m_operationalSettingsApplied && m_tx
            && m_settingsHydrationDepth == 0) m_tx->setPSLoopDelay(value);
        emit calDelayChanged(value);
    });
    connect(m_settings, &PureSignalSettings::requestedTxDelayNsChanged,
            this, [this](double value) {
        if (m_operationalSettingsApplied && m_tx
            && m_settingsHydrationDepth == 0) {
            applyTxDelaySeconds(value * 1.0e-9);
        }
        emit ampDelayChanged(static_cast<int>(std::lround(value)));
    });
    const auto applyPeak = [this] {
        if (m_operationalSettingsApplied && m_tx
            && m_settingsHydrationDepth == 0) m_tx->setPSHWPeak(hwPeak());
        emit hwPeakChanged(hwPeak());
    };
    connect(m_settings, &PureSignalSettings::hardwarePeakOverrideEnabledChanged,
            this, [applyPeak](bool) { applyPeak(); });
    connect(m_settings, &PureSignalSettings::hardwarePeakOverrideChanged,
            this, [applyPeak](double) { applyPeak(); });
}

void PureSignal::initializeAutoCalPreference(bool enabled)
{
    if (!m_settings) return;
    beginSettingsHydration();
    m_settings->initializeAutoCalPreference(enabled);
    endSettingsHydration();
}

void PureSignal::beginSettingsHydration()
{
    ++m_settingsHydrationDepth;
}

void PureSignal::endSettingsHydration()
{
    if (m_settingsHydrationDepth > 0) {
        --m_settingsHydrationDepth;
    }
}

bool PureSignal::resumeAutomaticCalibrationPreference()
{
    if (!isAutoCalEnabled() || !runCalibrationProcessing()
        || !m_operationalSettingsApplied || !canActuate()) {
        return false;
    }
    startAutomaticCalibration();
    return true;
}

void PureSignal::startAutomaticCalibration()
{
    if (!canActuate() || !runCalibrationProcessing()) {
        return;
    }
    retirePendingRestoreOperation();
    m_autoON = true;
    m_OFF = false;
    m_aaLastSeenAttemptCount = m_calAttempts.load();
    if (m_tx) {
        m_tx->setPSControl(/*reset=*/0, /*mancal=*/0,
                           /*automode=*/1, /*turnon=*/0);
    }
}

void PureSignal::requestOperationalStop()
{
    m_autoON = false;
    m_OFF = true;
    if (m_tx) {
        requestNativeCorrectionStop();
    }
}

void PureSignal::requestNativeCorrectionStop()
{
    if (!m_tx) {
        return;
    }
    if (m_tx->controlLane() != nullptr) {
        // R-R3-39: the transmit lane makes the same choice below, from the
        // RF gate as the keying calls queued before this one leave it.
        m_tx->stopPsCorrection();
        return;
    }
    if (!m_tx->isRunning()) {
        // With TXA quiescent no audio or PSCC block can finish the normal
        // END/reset transitions. The native helper fences stale workers and
        // publishes a durable CALCC/IQC acknowledgement synchronously.
        m_tx->stopPsCorrectionQuiescent();
        return;
    }
    // Request the active-stream IQC END directly; this does not depend on a
    // later paired-feedback pscc block. TXA audio drains the ramp and the
    // run/busy readback remains authoritative for completion.
    m_tx->requestPsCorrectionStop();
}

void PureSignal::clearTransientOperationsForOff()
{
    m_autoON = false;
    m_singleCalON = false;
    m_restoreON = false;
    m_performingSingleCal = false;
    m_performingSingleCalRetries = 0;
    m_saveAutoOn = 0;
    m_saveSingleCalOn = 0;
    m_deltaDb = 0;
    m_aaState = AutoAttenuateState::Monitor;
    m_OFF = true;
    m_cmdState = m_tx ? CommandState::TurnOff : CommandState::Off;
}

bool PureSignal::applyAcceptedSettingsToEngine()
{
    if (!m_settings || !m_tx || !canActuate()) {
        return false;
    }
    constexpr int kThetisPsRate = 192000;
    const int feedbackRate = m_caps.psSampleRate > 0
        ? m_caps.psSampleRate : kThetisPsRate;
    m_tx->setPSFeedbackRate(feedbackRate);
    if (m_fb && m_caps.psSampleRate > 0) {
        m_fb->setSampleRate(m_caps.psSampleRate);
    }
    m_tx->setPSMoxDelay(m_settings->moxDelaySeconds());
    m_tx->setPSLoopDelay(m_settings->loopDelaySeconds());
    applyTxDelaySeconds(m_settings->requestedTxDelayNs() * 1.0e-9);
    m_tx->setPSHWPeak(hwPeak());
    m_tx->setPSRunCal(m_settings->runCalibrationProcessing() ? 1 : 0);
    m_operationalSettingsApplied = true;
    return true;
}

bool PureSignal::applyCurrentCorrection()
{
    if (!m_tx || !canActuate()) {
        return false;
    }
    retirePendingRestoreOperation();
    const auto available = m_tx->psCorrectionAvailable();
    if (!available || !*available || !m_tx->applyPsCorrection()) {
        return false;
    }
    beginSettingsHydration();
    forceAutoCalDisable();
    endSettingsHydration();
    m_autoON = false;
    m_OFF = false;
    m_restoreON = false;
    m_performingSingleCal = false;
    m_singleCalON = false;
    m_cmdState = CommandState::StayOn;
    // Applying retained IQC curves does not require feedback collection.
    setPsEnabledWithFanOut(false);
    return true;
}

void PureSignal::retireSessionOperations()
{
    retirePendingFileOperation();
    clearTransientOperationsForOff();
    if (m_tx) {
        // Preserve the desired settings object while forcing the retiring
        // native session through its durable CALCC/IQC Off acknowledgement.
        requestNativeCorrectionStop();
        m_tx->setPSMox(false);
    } else {
        setPsEnabledWithFanOut(false);
    }
    m_cachedDisplaySnapshot.reset();
    m_ampViewSubscribed = false;
}

// ── Cal lifecycle ──────────────────────────────────────────────────────────

void PureSignal::singleCalibrate()
{
    if (!canActuate() || !runCalibrationProcessing()) {
        return;
    }
    retirePendingRestoreOperation();
    // From Thetis PSForm.cs:466-478 btnPSCalibrate_Click [v2.10.3.13]:
    //   if (_singlecalON) { _singlecalON = false; return; }
    //   console.ForcePureSignalAutoCalDisable();
    //   _singlecalON = true;
    //   console.PSState = false;
    //
    // Also covers PSForm.cs:481-484 SingleCalrun [v2.10.3.13]:
    //   //-W2PA Adds capability for CAT control via console
    //   public void SingleCalrun() { btnPSCalibrate_Click(...); }
    // — same body, exposed for CAT clients.  We collapse both into this
    // entry point since NereusSDR's CAT layer reaches PureSignal directly.
    if (m_singleCalON) {
        m_singleCalON = false;
        return;
    }
    beginSettingsHydration();
    forceAutoCalDisable();
    endSettingsHydration();
    m_autoON = false;
    m_OFF = false;
    m_singleCalON = true;
    // The cmd-state machine picks up _singlecalON on the next tick.
    // ForcePS sends the SetPSControl(1, 0, 0, 0) immediately so the engine
    // resets to LRESET before the SingleCalibrate state runs.
    if (m_tx) {
        m_tx->setPSControl(/*reset=*/1, /*mancal=*/0,
                           /*automode=*/0, /*turnon=*/0);
    }
    emit calibrationStarted();
}

void PureSignal::setPsEnabledWithFanOut(bool on)
{
    // Codex Fix C — emit psEnabledChanged when the cmd-state machine flips
    // PSEnabled.  Ports the entry-condition check in every cmd-state case:
    //
    //   PSForm.cs:634  case eCMDState.OFF:                       if (PSEnabled)  PSEnabled = false;
    //   PSForm.cs:646  case eCMDState.TurnOnAutoCalibrate:       if (!PSEnabled) PSEnabled = true;
    //   PSForm.cs:662  case eCMDState.TurnOnSingleCalibrate:     if (!PSEnabled) PSEnabled = true;
    //   PSForm.cs:678  case eCMDState.StayON:                    if (PSEnabled)  PSEnabled = false;
    //   PSForm.cs:705  case eCMDState.TurnOFF:                   if (!PSEnabled) PSEnabled = true;
    //   PSForm.cs:720  case eCMDState.IntiateRestoredCorrection: if (!PSEnabled) PSEnabled = true;
    //                                                                  [all v2.10.3.13]
    //
    // The PSEnabled property setter at PSForm.cs:235-269 [v2.10.3.13] is the
    // fan-out itself — it calls UpdateDDCs / SetPureSignal / SendHighPriority
    // / setPSRunCal.  In NereusSDR the calcc-side setPSRunCal is already
    // wired inline at each cmd-state case (see pollTimerTick); the radio /
    // DDC / step-attenuator side moves to subscribers of psEnabledChanged
    // (RadioModel::wireConnectionSignals).
    if (on == m_psEnabled) {
        return;
    }
    m_psEnabled = on;
    emit psEnabledChanged(on);
}

void PureSignal::setEnabled(bool enabled)
{
    if (enabled && !canActuate()) {
        return;
    }
    if (!enabled) {
        retirePendingFileOperation();
        beginSettingsHydration();
        forceAutoCalDisable();
        endSettingsHydration();
        clearTransientOperationsForOff();
        m_autoAttTimer.stop();
        if (!m_pollTimer.isActive()) {
            m_pollTimer.start();
        }
        if (m_tx) {
            requestNativeCorrectionStop();
        }
    }
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    if (m_enabled) {
        m_OFF = false;
        m_pollTimer.start();
        m_autoAttTimer.start();
    } else {
        // The stop request and transient-state retirement were issued above,
        // including when this setter is called repeatedly with false.
    }
    emit enabledChanged(m_enabled);
}

double PureSignal::getHwPeak() const
{
    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): expose calcc's current
    // hardware peak so PsForm GetPk label can mirror Thetis PSForm.cs:614
    // [v2.10.3.13].  Returns 0.0 when no TX channel is bound (e.g.
    // pre-connect).  Const-cast required because TxChannel::getPSHWPeak
    // wraps a non-const WDSP call (GetPSHWPeak); the WDSP call itself
    // is read-only despite the missing const qualifier.
    if (!m_tx) {
        return 0.0;
    }
    return const_cast<TxChannel*>(m_tx)->getPSHWPeak();
}

void PureSignal::setAutoCalEnabled(bool on)
{
    // From Thetis PSForm.cs:272-289 AutoCalEnabled property [v2.10.3.13]:
    //   _autocal_enabled = value;
    //   if (_autocal_enabled) { _autoON = true;  console.PSState = true;  }
    //   else                  { _OFF    = true;  console.PSState = false; }
    if (m_settings) m_settings->setAutoCalEnabled(on);
}

void PureSignal::forcePS()
{
    // From Thetis PSForm.cs:924-954 [v2.10.3.13] — ForcePS body.  The
    // SetPSControl fan-out is implemented here; the persisted-state pushes
    // (LoopDelay / TXDelay / MoxDelay / AutoAttenuate / OnTop /
    // QuickAttenuate / Show2ToneMeasurements)
    // are issued by the UI surfaces (PsForm) at task 11+ — they bind their
    // controls' value-changed signals to TxChannel setter methods directly.
    if (!m_tx || !canActuate()) {
        return;
    }
    if (!m_autoON) {
        m_tx->setPSControl(/*reset=*/1, /*mancal=*/0,
                           /*automode=*/0, /*turnon=*/0);
    } else {
        m_tx->setPSControl(/*reset=*/0, /*mancal=*/0,
                           /*automode=*/1, /*turnon=*/0);
    }
}

void PureSignal::reset()
{
    // From Thetis PSForm.cs:486-491 btnPSReset_Click [v2.10.3.13]:
    //   console.ForcePureSignalAutoCalDisable();
    //   if (!_OFF) _OFF = true;
    //   console.PSState = false;
    // Invalidate an asynchronous restore before requesting Off. The vendor
    // cancel barrier guarantees that no stale IQC mutation can occur after
    // this call returns; worker completion remains observable as Cancelled.
    retirePendingFileOperation();
    beginSettingsHydration();
    forceAutoCalDisable();
    endSettingsHydration();
    clearTransientOperationsForOff();
    if (m_tx) {
        requestNativeCorrectionStop();
    }
}

void PureSignal::forceAutoCalDisable()
{
    // From Thetis console.cs:43705 [v2.10.3.13]:
    //   public void ForcePureSignalAutoCalDisable() {
    //       chkFWCATUBypass.Checked = false;
    //   }
    // The fan-out via chkFWCATUBypass_CheckedChanged then sets
    // psform.AutoCalEnabled = false (console.cs:43712 [v2.10.3.13]).
    setAutoCalEnabled(false);
}

void PureSignal::setDefaultPeaks()
{
    // From Thetis PSForm.cs:547-550 SetDefaultPeaks [v2.10.3.13]:
    //   psdefpeak(HardwareSpecific.PSDefaultPeak);
    // psdefpeak (PSForm.cs:371-381 [v2.10.3.13]) writes the value into
    // txtPSpeak, which fires PSpeak_TextChanged (PSForm.cs:787-794
    // [v2.10.3.13]), which updates _PShwpeak, calls SetPSHWPeak, and
    // refreshes pbWarningSetPk via UpdateWarningSetPk.
    //
    // Route through setHwPeak() so the m_hwPeak cache + hwPeakChanged
    // signal fire alongside the WDSP push.  Otherwise PsForm's editable
    // txtPSpeak field — which subscribes to hwPeakChanged — would
    // continue to display the old per-bench value after a Default click,
    // even though WDSP itself is using the new psDefaultPeak.
    if (m_settings) m_settings->setHardwarePeakOverrideEnabled(false);
    if (m_tx && m_operationalSettingsApplied) m_tx->setPSHWPeak(m_caps.psDefaultPeak);
}

// ── Save / restore ─────────────────────────────────────────────────────────

bool PureSignal::saveCorrections(const QString& filename)
{
    // From Thetis PSForm.cs:524-532 btnPSSave_Click [v2.10.3.13]:
    //   System.IO.Directory.CreateDirectory(console.AppDataPath + "PureSignal\\");
    //   SaveFileDialog savefile1 = new SaveFileDialog();
    //   ...
    //   if (savefile1.ShowDialog() == DialogResult.OK)
    //       puresignal.PSSaveCorr(_txachannel, savefile1.FileName);
    // Directory creation + dialog handling lives in the UI layer (Task 8
    // PsForm); this method just forwards the user-chosen filename to
    // calcc.  Returns false when the TX channel isn't wired yet (e.g.
    // before WdspEngine init lambda runs) or filename is empty.
    return beginSaveCorrections(filename).has_value();
}

bool PureSignal::restoreCorrections(const QString& filename)
{
    // From Thetis PSForm.cs:534-545 btnPSRestore_Click [v2.10.3.13]:
    //   ...
    //   if (openfile1.ShowDialog() == DialogResult.OK) {
    //       console.ForcePureSignalAutoCalDisable();
    //       _OFF = false;
    //       puresignal.PSRestoreCorr(_txachannel, openfile1.FileName);
    //       _restoreON = true;
    //   }
    return beginRestoreCorrections(filename).has_value();
}

std::optional<Ps3FileOperationToken> PureSignal::beginSaveCorrections(
    const QString& filename)
{
    if (!m_tx || m_pendingFileOperation) {
        return std::nullopt;
    }
    const auto completion = m_tx->psSaveCorr(filename);
    if (!completion) {
        return std::nullopt;
    }
    Ps3FileOperationToken token{Ps3FileOperationKind::Save,
                                m_sessionGeneration, *completion};
    m_pendingFileOperation = token;
    emit fileOperationAccepted(static_cast<int>(token.kind),
                               token.sessionGeneration,
                               token.nativeCompletionGeneration);
    return token;
}

std::optional<Ps3FileOperationToken> PureSignal::beginRestoreCorrections(
    const QString& filename)
{
    if (!m_tx || m_pendingFileOperation || !canActuate()) {
        return std::nullopt;
    }
    const auto completion = m_tx->psRestoreCorr(filename);
    if (!completion) {
        return std::nullopt;
    }
    Ps3FileOperationToken token{Ps3FileOperationKind::Restore,
                                m_sessionGeneration, *completion};
    m_pendingFileOperation = token;
    emit fileOperationAccepted(static_cast<int>(token.kind),
                               token.sessionGeneration,
                               token.nativeCompletionGeneration);
    return token;
}

void PureSignal::retirePendingFileOperation()
{
    if (!m_pendingFileOperation) {
        return;
    }
    const auto token = *m_pendingFileOperation;
    if (m_tx) {
        m_tx->cancelPsFileOperation(token.kind);
    }
    m_pendingFileOperation.reset();
    emit fileOperationRetired(static_cast<int>(token.kind),
                              token.sessionGeneration,
                              token.nativeCompletionGeneration);
}

void PureSignal::retirePendingRestoreOperation()
{
    if (m_pendingFileOperation
        && m_pendingFileOperation->kind == Ps3FileOperationKind::Restore) {
        retirePendingFileOperation();
    }
}

void PureSignal::pollFileOperation()
{
    if (!m_pendingFileOperation || !m_tx) {
        return;
    }
    const auto token = *m_pendingFileOperation;
    if (token.sessionGeneration != m_sessionGeneration) {
        retirePendingFileOperation();
        return;
    }
    const auto status = m_tx->psFileOperationStatus(token.kind);
    if (!status || status->pending) {
        return;
    }
    if (status->generation != token.nativeCompletionGeneration) {
        // A later native generation cannot satisfy an older host request.
        retirePendingFileOperation();
        return;
    }
    m_pendingFileOperation.reset();
    emit fileOperationCompleted(static_cast<int>(token.kind),
                                static_cast<int>(status->result),
                                token.sessionGeneration,
                                token.nativeCompletionGeneration);
    if (token.kind == Ps3FileOperationKind::Restore
        && status->result == Ps3FileOperationResult::Success
        && canActuate()) {
        // Restore owns the next correction state. Clear the desired auto
        // preference without dispatching an intervening operational Off.
        beginSettingsHydration();
        forceAutoCalDisable();
        endSettingsHydration();
        m_OFF = false;
        m_restoreON = true;
    }
}

// ── Two-tone integration ──────────────────────────────────────────────────

void PureSignal::setTwoToneOn(bool on)
{
    // From Thetis PSForm.cs:508-522 btnPSTwoToneGen_Click [v2.10.3.13]:
    //   if (_ttgenON == false) { ...; _ttgenON = true; console.SetupForm.TTgenrun = true; }
    //   else                   { ...; _ttgenON = false; console.SetupForm.TTgenrun = false; }
    // In NereusSDR the TwoToneController owns the activation orchestrator
    // (chunk I); here we forward setActive(on) when wired.
    if (on && !canActuate()) return;
    if (m_twoTone) {
        m_twoTone->setActive(on);
    }
}

// ── Status reads ──────────────────────────────────────────────────────────

QColor PureSignal::feedbackColour() const
{
    return computeFeedbackColour(m_feedbackLevel.load());
}

QColor PureSignal::computeFeedbackColour(int level) const
{
    // From Thetis PSForm.cs:1123-1138 FeedbackColourLevel [v2.10.3.13]:
    //   if (FeedbackLevel > 181) {
    //       if (_bInvertRedBlue) return Color.Red;
    //       return Color.DodgerBlue;
    //   }
    //   else if (FeedbackLevel > 128) return Color.Lime;
    //   else if (FeedbackLevel > 90)  return Color.Yellow;
    //   else {
    //       if (_bInvertRedBlue) return Color.DodgerBlue;
    //       return Color.Red;
    //   }
    // Color values match System.Drawing exact RGBs:
    //   DodgerBlue = #1E90FF
    //   Lime       = #00FF00
    //   Yellow     = #FFFF00
    //   Red        = #FF0000
    if (level > 181) {
        if (m_invertRedBlue) {
            return QColor(0xFF, 0x00, 0x00); // Red
        }
        return QColor(0x1E, 0x90, 0xFF); // DodgerBlue
    }
    if (level > 128) {
        return QColor(0x00, 0xFF, 0x00); // Lime
    }
    if (level > 90) {
        return QColor(0xFF, 0xFF, 0x00); // Yellow
    }
    if (m_invertRedBlue) {
        return QColor(0x1E, 0x90, 0xFF); // DodgerBlue
    }
    return QColor(0xFF, 0x00, 0x00); // Red
}

// ── UI mirror state ───────────────────────────────────────────────────────

void PureSignal::setInvertRedBlue(bool on)
{
    if (on == m_invertRedBlue) {
        return;
    }
    m_invertRedBlue = on;
    emit invertRedBlueChanged(m_invertRedBlue);
    emit feedbackColourChanged(feedbackColour());
}

void PureSignal::setHideFeedback(bool on)
{
    if (on == m_hideFeedback) {
        return;
    }
    m_hideFeedback = on;
    emit hideFeedbackChanged(m_hideFeedback);
}

// ── Calibration option setters (Task 8 PsForm-driven) ────────────────────
//
// Each setter early-returns when the new value matches the cached value.
// When m_tx is wired, the value is forwarded to the corresponding TxChannel
// calcc setter; otherwise the cache is updated and the WDSP push happens
// later via forcePS() (which the UI will call after wiring lands).
//
// The forwarding maps mirror PSForm.cs *_CheckedChanged / *_ValueChanged
// handlers byte-for-byte [v2.10.3.13] — see the inline cite in each
// declaration block in PureSignal.h.

void PureSignal::setAutoAttenuate(bool on)
{
    if (m_settings) m_settings->setAutoAttenuate(on);
}

void PureSignal::setQuickAttenuate(bool on)
{
    if (m_settings) m_settings->setQuickAttenuate(on);
}

void PureSignal::setMoxDelay(double seconds)
{
    if (m_settings) m_settings->setMoxDelaySeconds(seconds);
}

void PureSignal::setCalDelay(double seconds)
{
    if (m_settings) m_settings->setLoopDelaySeconds(seconds);
}

void PureSignal::setAmpDelay(int ns)
{
    if (m_settings) m_settings->setRequestedTxDelayNs(ns);
}

void PureSignal::setRunCalibrationProcessing(bool run)
{
    if (m_settings) m_settings->setRunCalibrationProcessing(run);
}

void PureSignal::setLoopback(bool on)
{
    if (on == m_loopback) { return; }
    m_loopback = on;
    // UI-only mirror; the panadapter wire-through is Task 13.
    emit loopbackChanged(on);
}

void PureSignal::setShow2ToneMeasurements(bool on)
{
    if (on == m_show2Tone) { return; }
    m_show2Tone = on;
    // UI-only mirror; the SpectrumWidget IMD overlay is Task 12.  From
    // Thetis PSForm.cs:968-971 chkShow2ToneMeasurements_CheckedChanged
    // [v2.10.3.13]: Display.ShowIMDMeasurments = chkShow2ToneMeasurements.Checked;
    emit show2ToneMeasurementsChanged(on);
}

void PureSignal::setHwPeak(double peak)
{
    if (!m_settings) return;
    auto values = m_settings->values();
    values.hardwarePeakOverride = peak;
    values.hardwarePeakOverrideEnabled = true;
    m_settings->apply(values);
}

// ── Per-board defaults ────────────────────────────────────────────────────

void PureSignal::applyBoardCapabilities(const BoardCapabilities& caps)
{
    m_caps = caps;
    // Push the per-board default peak through the calcc engine.  The
    // Thetis equivalent is psdefpeak (PSForm.cs:371-381 [v2.10.3.13])
    // chained from setDefaultPeaks; here we apply it directly.
    if (m_tx && m_operationalSettingsApplied) {
        m_tx->setPSHWPeak(hwPeak());
    }
    // Push the per-board feedback rate to the feedback channel.
    //
    // From Thetis cmaster.cs:535 [v2.10.3.13]:
    //   puresignal.SetPSFeedbackRate(txch, ps_rate);   // ps_rate=192000 universally
    // The cmaster.cs:424 [v2.10.3.13] declaration:
    //   private static int ps_rate = 192000;
    // shows the rate is a SINGLE universal constant — Thetis does NOT
    // branch on board for SetPSFeedbackRate.
    //
    // HL2's psSampleRate=0 sentinel applies to the DDC sample rate (codec
    // sets rate[0]=rate[1]=rx1Rate per mi0bot console.cs:8472-8488
    // [v2.10.3.13-beta2] "HL2 can work at a high sample rate"), NOT to the
    // calcc feedback-rate clock — calcc.c:1069 [v2.10.3.13] stores
    // `a->rate = rate;` and uses it as the delay-time divisor:
    //   a->ctrl.moxsamps  = (int)(a->rate * a->ctrl.moxdelay);   // calcc.c:1070
    //   a->ctrl.waitsamps = (int)(a->rate * a->ctrl.loopdelay);  // calcc.c:1071
    // Passing 0 produces moxsamps=0 + waitsamps=0 + bogus state-machine
    // timeouts.  Resolve the sentinel here to the universal Thetis value.
    constexpr int kThetisPsRate = 192000;   // cmaster.cs:424 [v2.10.3.13]
    const int psFeedbackRateHz = (m_caps.psSampleRate > 0)
                                  ? m_caps.psSampleRate
                                  : kThetisPsRate;
    if (m_tx && m_operationalSettingsApplied) {
        m_tx->setPSFeedbackRate(psFeedbackRateHz);
    }
    if (m_fb && m_caps.psSampleRate > 0) {
        m_fb->setSampleRate(m_caps.psSampleRate);
    }

    // Bulk push the three PS timing parameters to WDSP — mirrors the
    // udPS*_ValueChanged(this, e) chain Thetis fires at PSForm.cs:942-944
    // [v2.10.3.13] when the form initializes.  Without these pushes, WDSP
    // sits at its create_calcc / create_delay zero defaults (calcc.c:190-205
    // [v2.10.3.13]) regardless of what the C++ Q_PROPERTY mirror values
    // are.  The setAmpDelay/setMoxDelay/setCalDelay setters all early-return
    // when called with the value they already have, so a "click button"
    // user flow never re-pushes initial values either.
    //
    // Concrete failure mode pre-fix (HL2 bench, 7 MHz, 2-Tone):
    //   - WDSP txdelay=0 (we never pushed 150 ns); calcc compares TX[t]
    //     vs FB[t] (no cable-delay compensation) → iqc LUT cc[0]≈-0.54,
    //     cs[0]≈-0.85 → constant ~120° phase rotation across envelope
    //     range.  Thetis on the same radio shows AmpView Phs Corr flat
    //     near 0° because Thetis pushed 150 ns at PSForm init.
    //   - calcc moxdelay=0 (we never pushed 0.2 s) → LMOXDELAY state
    //     short-circuits immediately into LSETUP without the settling
    //     pause Thetis uses.
    if (m_tx) {
        // From Thetis PSForm.cs:505 udPSPhnum_ValueChanged [v2.10.3.13]:
        //   double actual_delay = puresignal.SetPSTXDelay(_txachannel,
        //       (double)udPSPhnum.Value * 1.0e-09);
        applyTxDelaySeconds(static_cast<double>(ampDelay()) * 1.0e-9);
        // From Thetis PSForm.cs:495 udPSMoxDelay_ValueChanged [v2.10.3.13]:
        //   puresignal.SetPSMoxDelay(_txachannel, (double)udPSMoxDelay.Value);
        m_tx->setPSMoxDelay(moxDelay());
        // From Thetis PSForm.cs:500 udPSCalWait_ValueChanged [v2.10.3.13]:
        //   puresignal.SetPSLoopDelay(_txachannel, (double)udPSCalWait.Value);
        m_tx->setPSLoopDelay(calDelay());
    }
}

std::optional<Ps3Snapshot> PureSignal::ps3DisplaySnapshot(
    std::uint64_t sessionGeneration,
    std::uint64_t sequence,
    std::int64_t capturedAtUnixMilliseconds)
{
    if (!m_tx || !m_ampViewSubscribed) {
        return std::nullopt;
    }
    if (sessionGeneration != m_sessionGeneration) return std::nullopt;
    if (m_cachedDisplaySnapshot
        && capturedAtUnixMilliseconds - m_cachedDisplaySnapshot->capturedAtUnixMilliseconds
               < kPollIntervalMs) {
        return m_cachedDisplaySnapshot;
    }
    m_cachedDisplaySnapshot = m_tx->getPs3DisplaySnapshot(
        sessionGeneration, sequence, capturedAtUnixMilliseconds);
    return m_cachedDisplaySnapshot;
}

void PureSignal::setAmpViewSubscribed(bool subscribed)
{
    if (m_ampViewSubscribed == subscribed) return;
    m_ampViewSubscribed = subscribed;
    if (!subscribed) m_cachedDisplaySnapshot.reset();
}

void PureSignal::setSessionGeneration(std::uint64_t generation)
{
    if (generation == m_sessionGeneration) return;
    retireSessionOperations();
    m_sessionGeneration = generation;
    m_statusSequence = 0;
    m_cachedDisplaySnapshot.reset();
    m_ampViewSubscribed = false;
}

void PureSignal::setTimersEnabled(bool on)
{
    if (on) {
        if (!m_pollTimer.isActive())    m_pollTimer.start();
        if (!m_autoAttTimer.isActive()) m_autoAttTimer.start();
    } else {
        m_pollTimer.stop();
        m_autoAttTimer.stop();
    }
}

// ── MOX integration ───────────────────────────────────────────────────────

void PureSignal::onMoxChanged(bool mox)
{
    m_lastMox = mox;
    // From Thetis design — calcc enters its TX-aware state on MOX up and
    // transitions to LSTAYON / LWAIT on MOX down.  SetPSMox is the only
    // MOX-event signal needed.  PSEnabled fan-out (cmaster routing-bit
    // load + audio-mixer state + DSPRunCal etc., PSForm.cs:202-269
    // [v2.10.3.13]) is handled by the per-board codec layer (Task 5)
    // and the ReceiverManager DDC routing (Task 6) — this coordinator
    // doesn't duplicate that work.
    if (m_tx && (!mox || canActuate())) {
        m_tx->setPSMox(mox);
    }

    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): on each MOX-on transition
    // while PS-A is armed, re-sync m_aaLastSeenCalCount = m_calCount so
    // the FIRST autoAttentionTick after MOX engages waits for calcc to
    // actually start a NEW cycle before firing.  Without this, calCount
    // may have drifted between PS-on and MOX-on (e.g. if a PRIOR MOX
    // cycle left calCount at N, and the next MOX-on starts cycle N+1),
    // and the first auto-att fire would read info[4] from BEFORE the
    // post-MOX PSCC pump has had time to fill its buffer — producing
    // the same stale-data 31 dB ATT slam observed on the G2E bench at
    // 11:58:03.570.  Cite: NereusSDR-only divergence (Thetis's parallel
    // CalibrationAttemptsChanged gate uses _info[5] vs _oldInfo[5]
    // which is auto-synced every timer1 tick — see setAutoCalEnabled
    // header for the full story).
    if (mox && isAutoCalEnabled()) {
        m_aaLastSeenAttemptCount = m_calAttempts.load();
    }
}

// ── Polling tick (timer1code port) ────────────────────────────────────────

bool PureSignal::hasInfoChanged(const int* current16) const
{
    // From Thetis PSForm.cs:1086-1095 HasInfoChanged [v2.10.3.13]:
    //   for (int n = 0; n < 16; n++)
    //     if (_info[n] != _oldInfo[n]) return true;
    //   return false;
    for (int i = 0; i < 16; ++i) {
        if (current16[i] != m_oldInfo[i]) {
            return true;
        }
    }
    return false;
}

void PureSignal::pollTimerTick()
{
    // Phase 3M-4 bench-fix Round 2: thin wrapper.  Reads the info[]
    // vector from WDSP via TxChannel::getPSInfo, then dispatches to
    // processNewInfo() which holds change detection, signal emission,
    // and the cmd-state machine.  Tests bypass the WDSP read by calling
    // processNewInfo() directly with a synthetic info[] buffer.
    if (!m_tx) {
        return;
    }
    pollFileOperation();
    if (!m_enabled && m_cmdState == CommandState::Off && !m_OFF) {
        updateStatusSnapshot(++m_statusSequence,
                             QDateTime::currentMSecsSinceEpoch());
        return;
    }
    // Step 1: snapshot old info, read new info.  From Thetis PSForm.cs:
    // 1077-1085 GetInfo [v2.10.3.13]:
    //   fixed (void* dest = &_oldInfo[0])
    //   fixed (void* src  = &_info[0])
    //     Win32.memcpy(dest, src, 16 * sizeof(int));
    //   fixed (int* ptr = &(_info[0]))
    //     GetPSInfo(txachannel, ptr);
    int newInfo[16] = {};
    m_tx->getPSInfo(newInfo);
    processNewInfo(newInfo);
    updateStatusSnapshot(++m_statusSequence,
                         QDateTime::currentMSecsSinceEpoch());
}

Ps3StatusSnapshot PureSignal::ps3StatusSnapshot() const
{
    return m_statusSnapshot;
}

void PureSignal::updateStatusSnapshot(std::uint64_t sequence,
                                      std::int64_t capturedAtUnixMilliseconds)
{
    Ps3StatusSnapshot next;
    next.channelId = m_tx ? m_tx->channelId() : -1;
    next.sessionGeneration = m_sessionGeneration;
    next.sequence = sequence;
    next.capturedAtUnixMilliseconds = capturedAtUnixMilliseconds;
    std::copy(std::begin(m_info), std::end(m_info), next.raw.begin());
    next.feedbackLevel = m_info[4];
    next.successfulCalibrations = m_info[5];
    next.solutionStatusBits = m_info[6];
    next.attemptedCalibrations = m_info[7];
    next.fileStatusBits = m_info[12];
    next.dogCount = m_info[13];
    next.correctionsApplied = m_info[14] == 1;
    next.engineState = m_info[15];
    next.solutionComparisonFailed = (m_info[6] & 0x01) != 0;
    next.overdriveOrBucketFillFailure = (m_info[6] & 0x02) != 0;
    next.saveFailed = (m_info[12] & 0x01) != 0;
    next.restoreFailed = (m_info[12] & 0x02) != 0;
    next.requestedTxDelayNs = m_settings
        ? m_settings->requestedTxDelayNs() : 0.0;
    next.appliedTxDelayNs = m_appliedTxDelayNs;
    next.hardwarePeak = m_tx ? m_tx->getPSHWPeak() : 0.0;
    next.maxTx = m_tx ? m_tx->getPSMaxTX() : 0.0;
    next.feedbackRateHz = m_caps.psSampleRate > 0 ? m_caps.psSampleRate : 192000;
    next.psEnabled = isPsEnabled();
    next.mox = m_mox ? m_mox->isMox() : m_lastMox;
    if (m_tx) {
        const auto runCal = m_tx->psRunCal();
        next.runCalibrationProcessing = runCal.value_or(false);
        const auto correction = m_tx->psCorrectionState();
        if (correction) {
            next.correctionRun = correction->run;
            next.correctionBusy = correction->busy;
        }
        const auto save = m_tx->psFileOperationStatus(Ps3FileOperationKind::Save);
        if (save) {
            next.saveGeneration = save->generation;
            next.savePending = save->pending;
            next.saveResult = static_cast<int>(save->result);
        }
        const auto restore = m_tx->psFileOperationStatus(Ps3FileOperationKind::Restore);
        if (restore) {
            next.restoreGeneration = restore->generation;
            next.restorePending = restore->pending;
            next.restoreResult = static_cast<int>(restore->result);
        }
    }
    m_statusSnapshot = next;
    emit ps3StatusChanged();
}

void PureSignal::processNewInfo(const int newInfo[16])
{
    // From Thetis PSForm.cs:555-728 timer1code [v2.10.3.13].  Skeleton:
    //   1) puresignal.GetInfo(_txachannel)  — copies _info → _oldInfo,
    //      then GetPSInfo into _info.
    //   2) HasInfoChanged → update label text + per-property mirrors.
    //   3) CorrectionsBeingApplied / Correcting → drive btnPSSave.Enabled
    //      and the PS-state colour box.
    //   4) FeedbackColourLevel — apply or fade away.
    //   5) Run cmd-state machine.
    //
    // NereusSDR splits the UI updates out (Q_PROPERTY + signals) but the
    // info[] read + flag derivation + cmd-state machine port verbatim.

    // Step 2: HasInfoChanged check.  Per Thetis PSForm.cs:1086-1095, the
    // check compares _info vs _oldInfo BEFORE the GetInfo memcpy/GetPSInfo
    // overwrite.  Here we already have the new values in newInfo and the
    // previous in m_oldInfo, so the comparison is the same.
    const bool changed = hasInfoChanged(newInfo);

    // BENCH DIAGNOSTIC (Phase 3M-4 Task 17): sample info[] every ~10 ticks
    // (~1 sec) so the bench can see whether calcc is progressing. The line
    // is written only when a value it reports differs from the last line
    // written, so an idle radio no longer puts the same line in the Core's
    // journal once a second. Logged on change only, the same shape as the
    // txLpf lines in P1RadioConnection::setTxFrequency and
    // P2RadioConnection::setTxFrequency: it marks the event rather than the
    // poll cadence. A calibration in progress still changes these values
    // every sample, so the bench sees it tick as before.
    if (++m_diagTick % 10 == 0) {
        // hwPeak + maxTX added 2026-08-01 (J.J. Boyd, KG4VCF). PureSignal
        // parks in LCOLLECT on a live HL2 with both PS streams measurably
        // hot, so the question is no longer whether samples arrive but
        // whether the binning can ever complete.
        //
        // LCOLLECT sorts by n = env * hw_scale * ints and needs all `ints`
        // bins filled (calcc.c:733-775); hw_scale is 1/hwPeak. These two
        // numbers decide it between them:
        //
        //   maxTX * (1/hwPeak) reaching ~1.0   the envelope spans the bins
        //   much below 1.0                     only the low bins ever fill,
        //                                      and full_ints resets every
        //                                      4 seconds forever
        //
        // hwPeak is read back from the engine rather than from m_hwPeak, so
        // a value that never reached WDSP shows up as the default instead of
        // as the number we believe we pushed.
        const double hwPeak = m_tx ? m_tx->getPSHWPeak() : -1.0;
        const double maxTx  = m_tx ? m_tx->getPSMaxTX()  : -1.0;
        const DiagLine line{newInfo[15], newInfo[14], newInfo[5], newInfo[4],
                            newInfo[13], hwPeak, maxTx,
                            m_mox && m_mox->isMox(), isAutoCalEnabled()};
        if (m_lastDiagLine != line) {
            m_lastDiagLine = line;
            qCInfo(lcDsp).nospace()
                << "PureSignal info[]: state=" << newInfo[15]
                << " corrApplied=" << newInfo[14]
                << " calCount=" << newInfo[5]
                << " feedbackLevel=" << newInfo[4]
                << " dogCount=" << newInfo[13]
                << " hwPeak=" << hwPeak
                << " maxTX=" << maxTx
                << " binReach=" << (hwPeak > 0.0 ? maxTx / hwPeak : -1.0)
                << " (mox=" << line.mox
                << " autoCal=" << line.autoCal << ")";
        }
    }

    // From Thetis PSForm.cs:1097-1098 CalibrationAttemptsChanged
    // [v2.10.3.13]:
    //   public static bool CalibrationAttemptsChanged
    //     { get { return _info[5] != _oldInfo[5]; } }
    // Compute BEFORE the trailing memcpy at the end of this function
    // overwrites m_oldInfo with newInfo for the next tick.
    const bool calAttemptsChanged = (newInfo[7] != m_oldInfo[7]);

    if (changed) {
        // Mirror the field assignments from PSForm.cs:561-573 [v2.10.3.13]:
        //   lblPSInfo0.Text = puresignal.Info[0].ToString();
        //   ...
        //   lblPSfb2.Text   = puresignal.FeedbackLevel.ToString();   // _info[4]
        //   lblPSInfo5.Text = puresignal.CalibrationCount.ToString(); // _info[5]
        //   lblPSInfo6.Text = puresignal.Info[6].ToString();
        //   lblPSInfo13.Text = puresignal.Info[13].ToString();
        //   lblPSInfo15.Text = puresignal.Info[15].ToString();
        //
        // The Q_PROPERTY mirrors track the four interesting indices:
        //   info[4]  = FeedbackLevel
        //   info[5]  = CalibrationCount
        //   info[14] = CorrectionsBeingApplied (== 1)
        //   info[15] = EngineState
        const int newFb = newInfo[4];
        const int newCal = newInfo[5];

        if (newFb != m_feedbackLevel.exchange(newFb)) {
            emit feedbackLevelChanged(newFb);
            // From Thetis PSForm.cs:1106-1108 Correcting [v2.10.3.13]:
            //   public static bool Correcting { get { return FeedbackLevel > 90; } }
            const bool newCorr = newFb > 90;
            if (newCorr != m_correcting.exchange(newCorr)) {
                emit correctingChanged(newCorr);
            }
            emit feedbackColourChanged(computeFeedbackColour(newFb));
        }
        if (newCal != m_calCount.exchange(newCal)) {
            emit calibrationCountChanged(newCal);
        }
        const int newAttempts = newInfo[7];
        if (newAttempts != m_calAttempts.exchange(newAttempts)) {
            emit calibrationAttemptsChanged(newAttempts);
        }
    }

    // Step 3: CorrectionsBeingApplied flag — info[14] == 1 per
    // PSForm.cs:1100-1102 [v2.10.3.13].  Drives the Save-button
    // gating + PS-state colour box.
    //
    // Codex Fix D: emit correctionsBeingAppliedChanged (NOT correctingChanged)
    // to keep this predicate distinct from Correcting (FeedbackLevel > 90).
    // The legacy code crossed wires here — Save button could enable from
    // feedback level alone, and the CO badge never reached the Yellow
    // "applied-but-not-correcting" state per PSForm.cs:574-593 [v2.10.3.13].
    const bool newCorrApplied = (newInfo[14] == 1);
    if (newCorrApplied != m_correctionsApplied.exchange(newCorrApplied)) {
        emit correctionsBeingAppliedChanged(newCorrApplied);
    }

    // Step 4: feedback-colour-fade is a UI concern — handled by Task 10
    // PsaIndicatorWidget + Task 8 PsForm.  The colour value itself is
    // available via feedbackColour() / feedbackColourChanged.

    // ── Phase 3M-4 bench-fix Round 2: consolidated PSInfo dispatch ────────
    //
    // From Thetis PSForm.cs:614-619 timer1code [v2.10.3.13]:
    //   if (_autocal_enabled)
    //   {
    //       if (puresignal.HasInfoChanged)
    //           console.InfoBarFeedbackLevel(
    //               puresignal.FeedbackLevel,
    //               puresignal.IsFeedbackLevelOK,
    //               puresignal.CorrectionsBeingApplied,
    //               puresignal.CalibrationAttemptsChanged,
    //               puresignal.FeedbackColourLevel);
    //   }
    // Fan-out goes through console.InfoBarFeedbackLevel → infoBar.PSInfo
    // (console.cs:2307-2313 [v2.10.3.13]).  In NereusSDR the equivalent is
    // PureSignal::psInfoChanged → PsaIndicatorWidget::psInfo.  Per
    // PSForm.cs:1113-1115 [v2.10.3.13]:
    //   public static bool IsFeedbackLevelOK
    //     { get { return FeedbackLevel <= 256; } }
    if (isAutoCalEnabled() && changed) {
        const int level = newInfo[4];
        const bool feedbackLevelOk = (level <= 256);
        emit psInfoChanged(level, feedbackLevelOk, newCorrApplied,
                           calAttemptsChanged, computeFeedbackColour(level));
    }

    // ── Phase 3M-4 Task 13: applet-driven derived signals ──────────────────
    // calStateChanged carries the raw EngineState (info[15]).  Emitted on
    // every transition so the PureSignalApplet Cal/Run LEDs flip state as
    // calcc walks LRESET → LWAIT → LSETUP → LCOLLECT → LCALC → LSTAYON.
    // correctionPeakChanged carries the calcc HW peak (TxChannel::getPSHWPeak)
    // when it differs by > 0.001 from the prior reading; bound to the
    // PureSignalApplet correction gauge.
    // feedbackActiveChanged fires when (correcting && MOX) flips; bound to
    // the PureSignalApplet Fbk LED.
    {
        const int engineStateNow = newInfo[15];
        if (engineStateNow != m_lastEngineState) {
            m_lastEngineState = engineStateNow;
            emit calStateChanged(engineStateNow);
        }

        const double newPeak = m_tx ? m_tx->getPSHWPeak() : 0.0;
        if (std::abs(newPeak - m_lastCorrectionPeak) > 0.001) {
            m_lastCorrectionPeak = newPeak;
            emit correctionPeakChanged(newPeak);
        }

        const bool moxNow = (m_mox && m_mox->isMox());
        const bool feedbackActive = m_correcting.load() && moxNow;
        if (feedbackActive != m_lastFeedbackActive) {
            m_lastFeedbackActive = feedbackActive;
            emit feedbackActiveChanged(feedbackActive);
        }
    }

    // Step 5: cmd-state machine.  From Thetis PSForm.cs:632-727
    // [v2.10.3.13].  Use m_oldInfo[15] for the comparison since the
    // Thetis switch reads puresignal.State (info[15]) AFTER GetInfo, so
    // it should look at the new value — capture into a local first.
    const int engineStateRaw = newInfo[15];

    // Phase 3M-4 bench-fix Round 2: processNewInfo is now publicly callable
    // for tests, so guard the cmd-state machine's unguarded m_tx->setPSControl
    // calls.  Production always reaches this through pollTimerTick which
    // checks m_tx; this guard covers tests that drive signal emit gates
    // with a null TxChannel.  Save the info[] snapshot first so the
    // next-tick HasInfoChanged comparison still sees the right oldInfo.
    if (!m_tx) {
        std::memcpy(m_oldInfo, newInfo, sizeof(m_oldInfo));
        std::memcpy(m_info, newInfo, sizeof(m_info));
        return;
    }

    if (!canActuate()
        && m_cmdState != CommandState::Off
        && m_cmdState != CommandState::TurnOff) {
        m_autoON = false;
        m_singleCalON = false;
        m_restoreON = false;
        m_OFF = true;
        requestNativeCorrectionStop();
        m_cmdState = CommandState::TurnOff;
    }
    if (m_OFF
        && m_cmdState != CommandState::Off
        && m_cmdState != CommandState::TurnOff) {
        m_cmdState = CommandState::TurnOff;
    }

    switch (m_cmdState) {
    case CommandState::Off:
        // From Thetis PSForm.cs:634-650 [v2.10.3.13] case eCMDState.OFF:
        //   puresignal.SetPSControl(_txachannel, 1, 0, 0, 0);
        //   if (PSEnabled) PSEnabled = false;
        //   ...
        //   if (_restoreON)        _cmdstate = eCMDState.IntiateRestoredCorrection;
        //   else if (_autoON)      _cmdstate = eCMDState.TurnOnAutoCalibrate;
        //   else if (_singlecalON) _cmdstate = eCMDState.TurnOnSingleCalibrate;
        //   _OFF = false;
        // PSEnabled=false ports to PSForm.cs:235-263 [v2.10.3.13]:
        //   console.radio.GetDSPTX(0).PSRunCal = false;
        // which calls puresignal.SetPSRunCal(WDSP.id(thread, 0), false)
        // → calcc.c:891-898 SetPSRunCal sets runcal=0, gating pscc().
        m_tx->setPSControl(/*reset=*/1, /*mancal=*/0,
                           /*automode=*/0, /*turnon=*/0);
        if (m_OFF) {
            m_tx->setPSRunCal(1);
            m_cmdState = CommandState::TurnOff;
            break;
        }
        m_tx->setPSRunCal(runCalibrationProcessing() && canActuate() ? 1 : 0);
        // Codex Fix C — PSForm.cs:634 [v2.10.3.13]:
        //   if (PSEnabled) PSEnabled = false;
        // Fans out the radio/DDC OFF state to subscribers of psEnabledChanged.
        setPsEnabledWithFanOut(false);
        if (m_restoreON) {
            m_cmdState = CommandState::InitiateRestoredCorrection;
        } else if (m_autoON) {
            m_cmdState = CommandState::TurnOnAutoCalibrate;
        } else if (m_singleCalON) {
            m_cmdState = CommandState::TurnOnSingleCalibrate;
        }
        m_OFF = false;
        break;

    case CommandState::TurnOnAutoCalibrate:
        // From Thetis PSForm.cs:651-657 [v2.10.3.13]:
        //   puresignal.SetPSControl(_txachannel, 1, 0, 1, 0);
        //   if (!PSEnabled) PSEnabled = true;
        //   _cmdstate = eCMDState.AutoCalibrate;
        // PSEnabled=true ports to PSForm.cs:235-252 [v2.10.3.13]:
        //   console.radio.GetDSPTX(0).PSRunCal = true;
        // which sets calcc.runcal=1 — without it, pscc() returns
        // immediately and info[16] never updates (cor.cnt etc. stay 0).
        if (!runCalibrationProcessing()) {
            m_autoON = false;
            setPsEnabledWithFanOut(false);
            m_cmdState = CommandState::StayOn;
            break;
        }
        m_tx->setPSControl(/*reset=*/1, /*mancal=*/0,
                           /*automode=*/1, /*turnon=*/0);
        m_tx->setPSRunCal(1);
        // Codex Fix C — PSForm.cs:646 [v2.10.3.13]:
        //   if (!PSEnabled) PSEnabled = true;
        // Fans out the radio/DDC ON state to subscribers of psEnabledChanged
        // (UpdateDDCs / SetPureSignal / setPuresignalRun / setPsActive).
        setPsEnabledWithFanOut(true);
        m_cmdState = CommandState::AutoCalibrate;
        break;

    case CommandState::AutoCalibrate:
        // From Thetis PSForm.cs:658-666 [v2.10.3.13]:
        //   if (_OFF)              _cmdstate = eCMDState.TurnOFF;
        //   else if (_restoreON)   _cmdstate = eCMDState.IntiateRestoredCorrection;
        //   else if (_singlecalON) _cmdstate = eCMDState.TurnOnSingleCalibrate;
        if (m_OFF) {
            m_cmdState = CommandState::TurnOff;
        } else if (m_restoreON) {
            m_cmdState = CommandState::InitiateRestoredCorrection;
        } else if (m_singleCalON) {
            m_cmdState = CommandState::TurnOnSingleCalibrate;
        }
        break;

    case CommandState::TurnOnSingleCalibrate:
        // From Thetis PSForm.cs:658-665 [v2.10.3.13]:
        //   _autoON = false;
        //   _performing_single_cal = true;
        //   puresignal.SetPSControl(_txachannel, 1, 1, 0, 0);
        //   if (!PSEnabled) PSEnabled = true;
        //   _cmdstate = eCMDState.SingleCalibrate;
        // PSEnabled=true → calcc.runcal=1 (see Off-case comment above).
        if (!runCalibrationProcessing()) {
            m_singleCalON = false;
            m_performingSingleCal = false;
            setPsEnabledWithFanOut(false);
            m_cmdState = CommandState::StayOn;
            break;
        }
        m_autoON = false;
        // Codex Fix E — PSForm.cs:660 [v2.10.3.13]:
        //   _performing_single_cal = true;
        // Marks the entry into the retry-tracked window.  The flag is read
        // by the StayOn branch below to decide whether to re-arm m_singleCalON.
        m_performingSingleCal = true;
        m_tx->setPSControl(/*reset=*/1, /*mancal=*/1,
                           /*automode=*/0, /*turnon=*/0);
        m_tx->setPSRunCal(1);
        // Codex Fix C — PSForm.cs:662 [v2.10.3.13]:
        //   if (!PSEnabled) PSEnabled = true;
        // Same radio/DDC fan-out as TurnOnAutoCalibrate; pre-fix Single Cal
        // never fired this branch because the wiring was bound only to
        // autoCalEnabledChanged.
        setPsEnabledWithFanOut(true);
        m_cmdState = CommandState::SingleCalibrate;
        break;

    case CommandState::SingleCalibrate:
        // From Thetis PSForm.cs:674-684 [v2.10.3.13]:
        //   _singlecalON = false;
        //   if (_OFF)              _cmdstate = eCMDState.TurnOFF;
        //   else if (_restoreON)   _cmdstate = eCMDState.IntiateRestoredCorrection;
        //   else if (_autoON)      _cmdstate = eCMDState.TurnOnAutoCalibrate;
        //   else if (puresignal.CorrectionsBeingApplied)
        //       _cmdstate = eCMDState.StayON;
        m_singleCalON = false;
        if (m_OFF) {
            m_cmdState = CommandState::TurnOff;
        } else if (m_restoreON) {
            m_cmdState = CommandState::InitiateRestoredCorrection;
        } else if (m_autoON) {
            m_cmdState = CommandState::TurnOnAutoCalibrate;
        } else if (newCorrApplied) {
            m_cmdState = CommandState::StayOn;
            emit calibrationComplete(true);
        }
        break;

    case CommandState::StayOn:
        // From Thetis PSForm.cs:677-700 [v2.10.3.13]:
        //   case eCMDState.StayON://5:     // Stay-ON
        //       if (PSEnabled) PSEnabled = false;
        //       btnPSCalibrate.BackColor = SystemColors.Control;
        //       if (_OFF)              _cmdstate = eCMDState.TurnOFF;
        //       else if (_restoreON)   _cmdstate = eCMDState.IntiateRestoredCorrection;
        //       else if (_autoON)      _cmdstate = eCMDState.TurnOnAutoCalibrate;
        //       else if (_singlecalON) _cmdstate = eCMDState.TurnOnSingleCalibrate;
        //       else if (_performing_single_cal)
        //       {
        //           // fix for when we were performing a single cal, but needed to change attenuation
        //           _performing_single_cal = false;
        //           if (!puresignal.IsFeedbackLevelOKRange && _performing_single_cal_retries < 5)
        //           {
        //               _performing_single_cal_retries++;
        //               _singlecalON = true;
        //           }
        //           else
        //               _performing_single_cal_retries = 0;
        //       }
        //       break;
        // PSEnabled=false → calcc.runcal=0.  Calibration is converged;
        // corrections continue to be applied passively to TX without
        // further pscc() processing.
        m_tx->setPSRunCal(0);
        // Codex Fix C — PSForm.cs:678 [v2.10.3.13]:
        //   if (PSEnabled) PSEnabled = false;
        // Calibration converged: drop the radio-side fan-out (corrections
        // continue passively).  Subscribers (UpdateDDCs / SetPureSignal)
        // see the radio leave puresignal_run mode.
        setPsEnabledWithFanOut(false);
        if (m_OFF) {
            m_cmdState = CommandState::TurnOff;
        } else if (m_restoreON) {
            m_cmdState = CommandState::InitiateRestoredCorrection;
        } else if (m_autoON) {
            m_cmdState = CommandState::TurnOnAutoCalibrate;
        } else if (m_singleCalON) {
            m_cmdState = CommandState::TurnOnSingleCalibrate;
        } else if (m_performingSingleCal) {
            // Codex Fix E — PSForm.cs:688-699 [v2.10.3.13]:
            //   else if (_performing_single_cal)
            //   {
            //       // fix for when we were performing a single cal, but needed to change attenuation
            //       _performing_single_cal = false;
            //       if (!puresignal.IsFeedbackLevelOKRange && _performing_single_cal_retries < 5)
            //       {
            //           _performing_single_cal_retries++;
            //           _singlecalON = true;
            //       }
            //       else
            //           _performing_single_cal_retries = 0;
            //   }
            // Typical trigger: AutoAttenuate adjusted ATT mid-cal, calcc
            // resets, single-cal re-issued.  After 5 retries with FB still
            // outside (128, 181] we give up and reset the counter so the
            // next user-triggered single-cal starts fresh.
            //
            // fix for when we were performing a single cal, but needed to change attenuation [original from PSForm.cs:690]
            m_performingSingleCal = false;
            if (!isFeedbackLevelOKRange() && m_performingSingleCalRetries < 5) {
                m_performingSingleCalRetries++;
                m_singleCalON = true;
            } else {
                m_performingSingleCalRetries = 0;
            }
        }
        break;

    case CommandState::TurnOff:
        // From Thetis PSForm.cs:701-720 [v2.10.3.13]:
        //   if(!_autocal_enabled) _autoON = false; // only want to turn this off if autocal is off MW0LGE_21k9rc4
        //   puresignal.SetPSControl(_txachannel, 1, 0, 0, 0);
        //   if (!PSEnabled) PSEnabled = true;
        //   _OFF = false;
        //   if (_restoreON)        _cmdstate = eCMDState.IntiateRestoredCorrection;
        //   else if (_autoON)      _cmdstate = eCMDState.TurnOnAutoCalibrate;
        //   else if (_singlecalON) _cmdstate = eCMDState.TurnOnSingleCalibrate;
        //   else if (!puresignal.CorrectionsBeingApplied
        //            && puresignal.State == puresignal.EngineState.LRESET)
        //       _cmdstate = eCMDState.OFF;
        if (!isAutoCalEnabled()) {
            m_autoON = false;  // MW0LGE_21k9rc4 — only want to turn this off if autocal is off  [original inline comment from PSForm.cs:703]
        }
        // The stop was requested exactly once by the public Off/retirement
        // path (or the permission-loss guard) before entering this state.
        // Reissuing active END on every poll could restart its ramp.
        setPsEnabledWithFanOut(false);
        m_OFF = false;
        if (m_restoreON) {
            m_cmdState = CommandState::InitiateRestoredCorrection;
        } else if (m_autoON) {
            m_cmdState = CommandState::TurnOnAutoCalibrate;
        } else if (m_singleCalON) {
            m_cmdState = CommandState::TurnOnSingleCalibrate;
        } else {
            const auto correction = m_tx->psCorrectionState();
#ifdef NEREUS_BUILD_TESTS
            const auto acknowledgedCorrection = m_correctionStateForTest
                ? m_correctionStateForTest : correction;
#else
            const auto& acknowledgedCorrection = correction;
#endif
            const bool correctionStopped = acknowledgedCorrection
                && !acknowledgedCorrection->run && !acknowledgedCorrection->busy;
            if (!newCorrApplied
                && engineStateRaw == static_cast<int>(EngineState::LRESET)
                && correctionStopped) {
                m_tx->setPSRunCal(
                    runCalibrationProcessing() && canActuate() ? 1 : 0);
                setPsEnabledWithFanOut(false);
                m_cmdState = CommandState::Off;
                if (!m_enabled) m_pollTimer.stop();
            }
        }
        break;

    case CommandState::InitiateRestoredCorrection:
        // From Thetis PSForm.cs:721-728 [v2.10.3.13]:
        //   _autoON = false;
        //   puresignal.SetPSControl(_txachannel, 0, 0, 0, 1);
        //   if (!PSEnabled) PSEnabled = true;
        //   _restoreON = false;
        //   if (puresignal.State == puresignal.EngineState.LSTAYON)
        //       _cmdstate = eCMDState.StayON;
        m_autoON = false;
        m_tx->setPSControl(/*reset=*/0, /*mancal=*/0,
                           /*automode=*/0, /*turnon=*/1);
        m_tx->setPSRunCal(runCalibrationProcessing() ? 1 : 0);
        // Codex Fix C — PSForm.cs:720 [v2.10.3.13]:
        //   if (!PSEnabled) PSEnabled = true;
        // Native restore has already installed and started IQC. Retained
        // correction application does not need feedback collection, so the
        // persisted RunCal pause remains authoritative.
        setPsEnabledWithFanOut(false);
        m_restoreON = false;
        if (engineStateRaw == static_cast<int>(EngineState::LSTAYON)) {
            m_cmdState = CommandState::StayOn;
        }
        break;
    }

    // Save current → old for next tick.  Mirrors GetInfo's memcpy at
    // PSForm.cs:1079-1081 [v2.10.3.13].
    std::memcpy(m_oldInfo, newInfo, sizeof(m_oldInfo));
    std::memcpy(m_info, newInfo, sizeof(m_info));
}

// ── Auto-attention tick (timer2code port) ─────────────────────────────────

void PureSignal::autoAttentionTick()
{
    // From Thetis PSForm.cs:729-790 timer2code [v2.10.3.13].  Three-state
    // machine: Monitor → SetNewValues → RestoreOperation → Monitor.  Only
    // active when PS is enabled, _autoattenuate is true, and MOX is on.
    // Inline tags in the source range preserved verbatim per CLAUDE.md
    // "Inline comment preservation":
    //   //MW0LGE  [original inline comment from PSForm.cs:738]
    //   //[2.10.3.12]MW0LGE  [original inline comment from PSForm.cs:754]
    if (!m_enabled) {
        return;
    }

    switch (m_aaState) {
    case AutoAttenuateState::Monitor: {
        // From Thetis PSForm.cs:733-762 [v2.10.3.13]:
        //   if (_autoattenuate && puresignal.CalibrationAttemptsChanged
        //       && puresignal.NeedToRecalibrate(console.SetupForm.ATTOnTX))
        //   {
        //       if (!console.ATTOnTX) AutoAttenuate = true; //MW0LGE
        //       _autoAttenuateState = eAAState.SetNewValues;
        //       double ddB;
        //       if (puresignal.IsFeedbackLevelOK) {
        //           ddB = 20.0 * Math.Log10((double)puresignal.FeedbackLevel / 152.293);
        //           ...
        //       } else ddB = 31.1;
        //       _deltadB = (int)Math.Round(ddB, MidpointRounding.AwayFromZero); //[2.10.3.12]MW0LGE use rounding, to fix Banker's rounding issue
        //       _save_autoON = (_cmdstate == eCMDState.AutoCalibrate) ? 1 : 0;
        //       _save_singlecalON = (_cmdstate == eCMDState.SingleCalibrate) ? 1 : 0;
        //       puresignal.SetPSControl(_txachannel, 1, 0, 0, 0);
        //   }
        // Inline tags preserved verbatim per CLAUDE.md "Inline comment
        // preservation":
        //   //MW0LGE  [original inline comment from PSForm.cs:738]
        //   //[2.10.3.12]MW0LGE  [original inline comment from PSForm.cs:754]

        // We don't yet have the calibration-attempts-changed flag (it's
        // derived from info[5] != oldInfo[5]; pollTimerTick already
        // tracks calCount changes via emit calibrationCountChanged).
        // Re-derive here by checking that the call count differs from
        // our auto-attention scratch storage.  Cheaper than a separate
        // dirty bit.
        if (!m_stepAtt) {
            return;
        }
        if (!m_mox || !m_mox->isMox()) {
            return;
        }
        // Phase 3M-4 Task 17 fix: honour the m_autoAttenuate flag per
        // Thetis PSForm.cs:735 [v2.10.3.13] (`if (_autoattenuate && ...)`).
        // The earlier port omitted this check — auto-att fired even when
        // the user unchecked the AutoAttenuate box in PsForm Advanced.
        if (!autoAttenuate()) {
            return;
        }
        // Phase 3M-4 Task 17 fix: gate on CalibrationAttemptsChanged per
        // Thetis PSForm.cs:735 [v2.10.3.13] — auto-att fires once per
        // calcc cycle, not on every 100 ms timer tick.  Without this,
        // multiple ticks per cycle compute fresh deltaDb on stale
        // transient fbLevel readings (e.g. fbLevel=1 right after an
        // ATT bump but before pscc resyncs), causing ATT to oscillate
        // 0↔31 instead of converging to mid-band [128, 181].  Bench
        // evidence: ATT toggled 0↔31 every ~0.3-1 s at calCount=180
        // with fbLevel stuck at 20 — exactly the over-correction
        // pattern this guard prevents.
        const int curAttemptCount = m_calAttempts.load();
        if (curAttemptCount == m_aaLastSeenAttemptCount) {
            return;
        }
        m_aaLastSeenAttemptCount = curAttemptCount;

        const int currentAttOnTx = m_stepAtt->attOnTxValue();
        const int fbLevel = m_feedbackLevel.load();
        // From Thetis PSForm.cs:1109-1112 NeedToRecalibrate [v2.10.3.13]:
        //   return (FeedbackLevel > 181 || (FeedbackLevel <= 128 && nCurrentATTonTX > 0));
        //
        // Phase 3M-4 mi0bot audit: HL2 has its own NeedToRecalibrate_HL2
        // variant at mi0bot PSForm.cs:1142-1144 [v2.10.3.13-beta2]:
        //   return (FeedbackLevel > 181 || (FeedbackLevel <= 128 && nCurrentATTonTX > -28));
        //   // MI0BOT: Needed seperate function for HL2 as ...
        //
        // The two predicates differ only in the lower-bound: standard boards
        // use `> 0` (the floor of their 0..31 ATT range), HL2 uses `> -28`
        // (the floor of its signed -28..+31 range).  StepAttenuatorController
        // exposes the per-board floor via minAttenuation() — which already
        // returns 0 for legacy boards and -28 for HL2 (set in
        // BoardCapsTable's attenuator min).  So the unified predicate
        // `currentAttOnTx > minAttenuation()` is the byte-for-byte port of
        // both Thetis branches without an explicit board check.
        const int minAtt = m_stepAtt->minAttenuation();
        const bool needRecal = (fbLevel > 181)
            || (fbLevel <= 128 && currentAttOnTx > minAtt);
        if (!needRecal) {
            return;
        }
        qCInfo(lcDsp).nospace()
            << "PureSignal: AutoAtt fbLevel=" << fbLevel
            << " currentAttOnTx=" << currentAttOnTx
            << " → SetNewValues";

        // From Thetis PSForm.cs:738 [v2.10.3.13]:
        //   if (!console.ATTOnTX) AutoAttenuate = true; //MW0LGE
        // The AutoAttenuate setter (PSForm.cs:295-314 [v2.10.3.13]) chains:
        //   _autoattenuate = value;
        //   if (_autoattenuate) console.ATTOnTX = _autoattenuate;
        // i.e. setting AutoAttenuate=true when console.ATTOnTX is false
        // force-enables the ATT-on-TX master toggle.
        //
        // Mapping in NereusSDR:
        //   console.ATTOnTX  ≡  m_stepAtt->attOnTxEnabled()
        //   AutoAttenuate=true setter side-effect on console.ATTOnTX
        //                    ≡  m_stepAtt->setAttOnTxEnabled(true)
        //
        // _autoattenuate is already true at this point (we passed the
        // m_autoAttenuate guard above), so the only setter side-effect
        // we need to mirror is the master-toggle force-enable.
        //
        // Inline tag preserved verbatim per CLAUDE.md "Inline comment
        // preservation":
        //   //MW0LGE  [original inline comment from PSForm.cs:738]
        if (!m_stepAtt->attOnTxEnabled()) {
            m_stepAtt->setAttOnTxEnabled(true); //MW0LGE
        }

        m_aaState = AutoAttenuateState::SetNewValues;

        // From Thetis PSForm.cs:743-754 [v2.10.3.13] (legacy, non-HL2):
        //   if (puresignal.IsFeedbackLevelOK)  (FeedbackLevel <= 256)
        //       ddB = 20.0 * Math.Log10((double)puresignal.FeedbackLevel / 152.293);
        //   else
        //       ddB = 31.1;
        // Constants 152.293 + 31.1 are calcc-derived calibration anchors —
        // preserve verbatim per CLAUDE.md "Constants and Magic Numbers".
        //
        // PR #212 codex-fix B: mi0bot PSForm.cs:744-769 [v2.10.3.13-beta2]
        // splits the four clamp/fallback lines on HL2:
        //   if (HPSDRModel.HERMESLITE != HardwareSpecific.Model)
        //   {
        //       if (Double.IsNaN(ddB)) ddB = 31.1;
        //       if (ddB < -100.0) ddB = -100.0;
        //       if (ddB > +100.0) ddB = +100.0;
        //   }
        //   else
        //   {
        //       if (Double.IsNaN(ddB)) ddB = 10.0;  // MI0BOT: Handle the Not A Number situation
        //       if (ddB < -100.0) ddB = -10.0;      // MI0BOT: Handle - infinity
        //       if (ddB > +100.0) ddB = 10.0;       // MI0BOT: Handle + infinity
        //   }
        //   ...
        //   else  // !IsFeedbackLevelOK
        //   {
        //       if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
        //           ddB = 10.0;
        //       else
        //           ddB = 31.1;
        //   }
        //
        // The HL2 ATT range is signed [-28, +31] vs legacy unsigned [0, 31].
        // A transient bad feedback reading on HL2 with the legacy ±100 clamp
        // would slam ATT to the -28 floor instead of a -10 dB nudge.  The
        // HL2 clamps keep deltaDb in a much tighter envelope so each cycle
        // converges instead of flapping floor-to-ceiling.
        //
        // Inline tags preserved verbatim per CLAUDE.md "Inline comment
        // preservation":
        //   //[2.10.3.12]MW0LGE  [original from PSForm.cs:754]
        //   //MI0BOT             [originals from mi0bot PSForm.cs:758-760, 766]
        const bool isHl2 = (m_caps.board == HPSDRHW::HermesLite
                            || m_caps.board == HPSDRHW::HermesLiteRxOnly);
        double ddB;
        if (fbLevel <= 256) {
            // Thetis casts the int through double directly:
            //   ddB = 20.0 * Math.Log10((double)FeedbackLevel / 152.293);
            // C# Math.Log10(0.0) returns -Infinity; C++ std::log10(0.0)
            // likewise returns -infinity per IEEE 754.  The downstream
            // `if (ddB < -100.0)` clamp is the actual sentinel-handler for
            // that path — it's what fires the per-board fallback (-100 vs
            // -10).  We must NOT clamp safeFb up to 1 here: that would
            // mask the -inf path and prevent the HL2 -10 clamp from ever
            // firing on real fbLevel=0 readings (a documented HL2 mode
            // when calcc is mid-dropout).
            ddB = 20.0 * std::log10(static_cast<double>(fbLevel) / 152.293);
            if (!isHl2) {
                if (std::isnan(ddB)) {
                    ddB = 31.1;
                }
                if (ddB < -100.0) ddB = -100.0;
                if (ddB > +100.0) ddB = +100.0;
            } else {
                if (std::isnan(ddB)) ddB = 10.0;  // MI0BOT: Handle the Not A Number situation
                if (ddB < -100.0)    ddB = -10.0; // MI0BOT: Handle - infinity
                if (ddB > +100.0)    ddB = 10.0;  // MI0BOT: Handle + infinity
            }
        } else {
            // !IsFeedbackLevelOK (FeedbackLevel > 256) — mi0bot
            // PSForm.cs:765-768 [v2.10.3.13-beta2]:
            //   if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
            //       ddB = 10.0;
            //   else
            //       ddB = 31.1;
            ddB = isHl2 ? 10.0 : 31.1;
        }
        // From Thetis PSForm.cs:756 [v2.10.3.13] —
        //   _deltadB = (int)Math.Round(ddB, MidpointRounding.AwayFromZero);
        //   //[2.10.3.12]MW0LGE use rounding, to fix Banker's rounding issue
        // C++ std::lround is round-half-away-from-zero by default.
        m_deltaDb = static_cast<int>(std::lround(ddB));

        // From Thetis PSForm.cs:758-759 [v2.10.3.13]:
        //   _save_autoON = (_cmdstate == eCMDState.AutoCalibrate) ? 1 : 0;
        //   _save_singlecalON = (_cmdstate == eCMDState.SingleCalibrate) ? 1 : 0;
        m_saveAutoOn = (m_cmdState == CommandState::AutoCalibrate) ? 1 : 0;
        m_saveSingleCalOn = (m_cmdState == CommandState::SingleCalibrate) ? 1 : 0;

        // From Thetis PSForm.cs:761 [v2.10.3.13] — reset everything.
        if (m_tx) {
            m_tx->setPSControl(/*reset=*/1, /*mancal=*/0,
                               /*automode=*/0, /*turnon=*/0);
        }
        break;
    }

    case AutoAttenuateState::SetNewValues: {
        // From Thetis PSForm.cs:763-778 [v2.10.3.13]:
        //   _autoAttenuateState = eAAState.RestoreOperation;
        //   int newAtten;
        //   int oldAtten = console.SetupForm.ATTOnTX;
        //   if ((oldAtten + _deltadB) > 0)
        //       newAtten = oldAtten + _deltadB;
        //   else
        //       newAtten = 0;
        //   if (oldAtten != newAtten) {
        //       console.SetupForm.ATTOnTX = newAtten;
        //       if (m_bQuckAttenuate) Thread.Sleep(100);
        //   }
        //
        // Phase 3M-4 mi0bot audit: HL2 has a different clamp at
        // mi0bot PSForm.cs:786-790 [v2.10.3.13-beta2]:
        //   if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
        //   {
        //       newAtten = oldAtten + _deltadB;
        //       //MI0BOT: HL2 can handle negative up to -28, just let it be
        //       //handled in ATTOnTx section
        //   }
        //   else
        //   {
        //       if ((oldAtten + _deltadB) > 0)
        //           newAtten = oldAtten + _deltadB;
        //       else
        //           newAtten = 0;
        //   }
        //
        // The two branches differ only in the lower bound: standard boards
        // clamp to 0 (their ATT floor), HL2 lets values pass through to its
        // [-28, +31] range.  StepAttenuatorController exposes the per-board
        // floor via minAttenuation() — 0 for legacy boards, -28 for HL2.
        // So the unified clamp `max(oldAtten + deltaDb, minAttenuation())`
        // is the byte-for-byte port of both Thetis branches.  HL2's
        // SetupForm.ATTOnTX setter (m_stepAtt->setAttOnTxValue downstream)
        // does its own additional clamp at the [m_minAttDb, 31] range —
        // matching Thetis's "just let it be handled in ATTOnTx section"
        // comment.
        m_aaState = AutoAttenuateState::RestoreOperation;
        if (!m_stepAtt) {
            break;
        }
        const int oldAtten = m_stepAtt->attOnTxValue();
        const int minAtt   = m_stepAtt->minAttenuation();
        int newAtten;
        if ((oldAtten + m_deltaDb) > minAtt) {
            newAtten = oldAtten + m_deltaDb;
        } else {
            newAtten = minAtt; //MI0BOT HL2: handled by setAttOnTxValue clamp
        }
        if (oldAtten != newAtten) {
            qCInfo(lcDsp).nospace()
                << "PureSignal: AutoAtt setAttOnTx " << oldAtten
                << " → " << newAtten << " dB (deltaDb=" << m_deltaDb << ")";
            m_stepAtt->setAttOnTxValue(newAtten);
            // The Thetis Thread.Sleep(100) for QuickAttenuate is omitted
            // — NereusSDR doesn't block the main thread.  The next
            // RestoreOperation tick fires 100 ms later anyway.
        }
        break;
    }

    case AutoAttenuateState::RestoreOperation: {
        // From Thetis PSForm.cs:779-790 [v2.10.3.13]:
        //   _autoAttenuateState = eAAState.Monitor;
        //   puresignal.SetPSControl(_txachannel, 0, _save_singlecalON, _save_autoON, 0);
        m_aaState = AutoAttenuateState::Monitor;
        if (m_tx) {
            m_tx->setPSControl(/*reset=*/0,
                               /*mancal=*/m_saveSingleCalOn,
                               /*automode=*/m_saveAutoOn,
                               /*turnon=*/0);
        }
        break;
    }
    }
}

} // namespace NereusSDR
