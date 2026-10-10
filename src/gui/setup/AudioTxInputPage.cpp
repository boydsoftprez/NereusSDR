// =================================================================
// src/gui/setup/AudioTxInputPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → TX Input page.
// See AudioTxInputPage.h for the full header.
//
// Phase 3M-1b Task I.1 (2026-04-28): Top-level mic-source selector.
// Phase 3M-1b Task I.2 (2026-04-28): PC Mic group box (5 rows: backend,
//   device, buffer size, Test Mic + VU, Mic Gain).
// R-R3-36 Task 6 (2026-09-22): PC Mic controls edit the shared
//   audio/TxInput config; Test Mic holds a real capture demand; microphone
//   status and Retry beside Test Mic.
// R-R3-36 (2026-09-23): usable in a remote window through
//   RadioModel::localAudioDevices(); the controls held for the radio follow
//   the transmit permission.
// R-R3-49 parity Task 3 (2026-09-25): Mic Gain and the radio microphone
//   groups follow the transmit settings gate (transmitSettingsVersion 3)
//   and change the Core's values off the air; the mic source keeps the
//   transmit permission.
// R-R3-49 / R-IOS-18 (2026-09-29): Setup description version 15 ids on Mic
//   Gain and the radio microphone groups, whose titles use parentheses.
// Radio codec lane (2026-09-30): Radio Mic opens on the Hermes Lite 2 with
//   the audio add-on note and the Hermes group; Saturn G2 Mic Tip-Ring; the
//   Orion group disabled on the Red Pitaya; Line In Gain in 1.5 dB steps.
//
// Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-04: reset Qt6.11 Cocoa's popup accessibility cache before
// refreshing the PC Mic device list. J.J. Boyd (KG4VCF),
// AI-assisted via OpenAI Codex.
// 2026-10-09 (R-SPK-21, R-AUD-01): VAX TX (virtual device) greyed on
// Windows with the PC Mic route; each system's tooltip names its own
// transmit device. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
// Code.
// 2026-10-06: R-SPK-21 (Microphone), R-SPK-22. Titled "Microphone"; one PC
// microphone card (the Devices page's DeviceCard on audio/TxInput, with
// Test Mic, the capture status and Retry), a Mic gain group, and the
// sections for sources not picked greyed out, never hidden. Keys and
// nereusSetupIds unchanged. The radio mic placeholder and family groups
// follow currentRadioChanged. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.
// 2026-10-09: native audio plan Task 16 (R-AUD-03, R-AUD-09, R-AUD-14): the
// PC microphone card follows the engine's device catalogue and the mic
// role's state. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// Fix round (R-AUD-09, R-AUD-11): the status line reads "PC mic not
// connected" (or in use) in amber while the chosen mic is.
// Fix round 2 (R-AUD-09, R-AUD-11, R-AUD-24): Retry microphone is greyed,
// with its reason, for a mic not connected or in use, which resumes by
// itself; it stays for other mic failures.
// 2026-10-10: the PC microphone card's Driver row is in front, above
// Device (DeviceCard's move; a comment here follows it). J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "AudioTxInputPage.h"
#include "CaptureStatusText.h"
#include "DeviceCard.h"

#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "core/BoardCapabilities.h"
#include "core/AudioEngine.h"
#include "core/session/IStationLink.h"
#include "core/session/RemoteTransmitClient.h"
#include "gui/HGauge.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

namespace {

// R-SPK-21: a section that is not picked stays in view, greyed. The page's
// check box and radio button styles have no disabled look, so greyed and
// live rows read the same; these are the DeviceCard's greyed colours (D10).
constexpr auto kGreyedStyle =
    "QCheckBox:disabled { color: #506070; }"
    "QCheckBox::indicator:disabled:checked { background: #405060; border-color: #405060; }"
    "QRadioButton:disabled { color: #506070; }"
    "QRadioButton::indicator:disabled:checked { background: #405060; border-color: #405060; }"
    "QGroupBox:disabled { color: #506070; }"
    "QLabel:disabled { color: #506070; }";

} // namespace

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AudioTxInputPage::AudioTxInputPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Microphone"), model, parent)
{
    // Radio codec lane: a board with a mic jack, or the HL2 with its
    // audio add-on board, takes the radio mic.
    const bool radioMicSelectable = model
        ? model->boardCapabilities().radioMicSelectable()
        : true;  // safe default: don't disable Radio Mic for null model
    m_hw = model
        ? model->boardCapabilities().board
        : HPSDRHW::Unknown;
    m_radioMicNeedsAddOn = model && model->boardCapabilities().radioMicNeedsAddOn;
    m_orionMicPanelAvailable = !model
        || RadioModel::orionMicPanelAvailable(model->hardwareProfile().model);

    setStyleSheet(styleSheet() + QLatin1String(kGreyedStyle));
    buildPage(radioMicSelectable, m_hw);

    // Wire two-way sync with TransmitModel.
    if (model) {
        TransmitModel* tx = &model->transmitModel();

        // Model → UI: reflect external setMicSource() calls into the buttons.
        connect(tx, &TransmitModel::micSourceChanged,
                this, &AudioTxInputPage::onModelMicSourceChanged);

        // Model → UI: reflect external setMicGainDb() calls into the slider.
        connect(tx, &TransmitModel::micGainDbChanged,
                this, &AudioTxInputPage::onModelMicGainDbChanged);

        // R-SPK-21: the radio mic placeholder and family groups follow the
        // board when the radio changes.
        connect(model, &RadioModel::currentRadioChanged,
                this, [this]() { onCurrentRadioChanged(); });

        // Apply the current model state at construction.
        syncButtonsFromModel(tx->micSource());

        // Seed mic gain slider — clamp stored model value to the per-board
        // slider range so a value persisted for a different board doesn't
        // land outside the slider bounds.
        if (m_micGainSlider) {
            QSignalBlocker blk(m_micGainSlider);
            const int clampedGain = qBound(
                m_micGainSlider->minimum(),
                tx->micGainDb(),
                m_micGainSlider->maximum());
            m_micGainSlider->setValue(clampedGain);
            if (m_micGainLabel) {
                m_micGainLabel->setText(
                    QStringLiteral("%1 dB").arg(clampedGain));
            }
        }

        // ── Model → UI wiring for Radio Mic flags (I.3) ─────────────────────────
        connect(tx, &TransmitModel::lineInChanged,
                this, &AudioTxInputPage::onModelLineInChanged);
        connect(tx, &TransmitModel::micBoostChanged,
                this, &AudioTxInputPage::onModelMicBoostChanged);
        connect(tx, &TransmitModel::lineInBoostChanged,
                this, &AudioTxInputPage::onModelLineInBoostChanged);
        connect(tx, &TransmitModel::micTipRingChanged,
                this, &AudioTxInputPage::onModelMicTipRingChanged);
        connect(tx, &TransmitModel::micBiasChanged,
                this, &AudioTxInputPage::onModelMicBiasChanged);
        connect(tx, &TransmitModel::micPttDisabledChanged,
                this, &AudioTxInputPage::onModelMicPttDisabledChanged);
        connect(tx, &TransmitModel::micXlrChanged,
                this, &AudioTxInputPage::onModelMicXlrChanged);

        // Seed Radio Mic group widgets from current model state.
        // Hermes family: lineIn + micBoost + lineInBoost
        if (m_hermesMicInputGroup) {
            m_updatingFromModel = true;
            QAbstractButton* lineInBtn = m_hermesMicInputGroup->button(1);  // id=1 = Line In
            QAbstractButton* micInBtn  = m_hermesMicInputGroup->button(0);  // id=0 = Mic In
            if (tx->lineIn()) { if (lineInBtn) lineInBtn->setChecked(true); }
            else              { if (micInBtn)  micInBtn->setChecked(true); }
            m_updatingFromModel = false;
        }
        if (m_hermesMicBoostChk) {
            QSignalBlocker blk(m_hermesMicBoostChk);
            m_hermesMicBoostChk->setChecked(tx->micBoost());
        }
        showLineInBoost(tx->lineInBoost());
        // Orion family: micTipRing + micBias + micPttDisabled + micBoost
        if (m_orionMicTipRingChk) {
            QSignalBlocker blk(m_orionMicTipRingChk);
            m_orionMicTipRingChk->setChecked(tx->micTipRing());
        }
        if (m_orionMicBiasChk) {
            QSignalBlocker blk(m_orionMicBiasChk);
            m_orionMicBiasChk->setChecked(tx->micBias());
        }
        if (m_orionMicPttDisabledChk) {
            QSignalBlocker blk(m_orionMicPttDisabledChk);
            m_orionMicPttDisabledChk->setChecked(tx->micPttDisabled());
        }
        if (m_orionMicBoostChk) {
            QSignalBlocker blk(m_orionMicBoostChk);
            m_orionMicBoostChk->setChecked(tx->micBoost());
        }
        // Saturn family: micXlr + micPttDisabled + micBias + micBoost
        if (m_saturnMicInputGroup) {
            m_updatingFromModel = true;
            QAbstractButton* xlrBtn   = m_saturnMicInputGroup->button(1);  // id=1 = XLR
            QAbstractButton* jackBtn  = m_saturnMicInputGroup->button(0);  // id=0 = 3.5mm
            if (tx->micXlr()) { if (xlrBtn)  xlrBtn->setChecked(true); }
            else              { if (jackBtn) jackBtn->setChecked(true); }
            m_updatingFromModel = false;
        }
        if (m_saturnMicPttDisabledChk) {
            QSignalBlocker blk(m_saturnMicPttDisabledChk);
            m_saturnMicPttDisabledChk->setChecked(tx->micPttDisabled());
        }
        if (m_saturnMicBiasChk) {
            QSignalBlocker blk(m_saturnMicBiasChk);
            m_saturnMicBiasChk->setChecked(tx->micBias());
        }
        if (m_saturnMicBoostChk) {
            QSignalBlocker blk(m_saturnMicBoostChk);
            m_saturnMicBoostChk->setChecked(tx->micBoost());
        }
        if (m_saturnMicTipRingChk) {
            QSignalBlocker blk(m_saturnMicTipRingChk);
            m_saturnMicTipRingChk->setChecked(tx->micTipRing());
        }
    }

    // R-R3-36 / R-SPK-21: the PC microphone card edits the one
    // audio/TxInput config the engine holds and follows every change to it.
    if (AudioEngine* eng = engine()) {
        wirePcMicCard();
        connect(eng, &AudioEngine::captureStatusChanged,
                this, [this](const CaptureSupervisor::Status&) { refreshCaptureStatus(); });
        // R-AUD-09, R-AUD-11: a missing or held mic reads so on the line.
        connect(eng, &AudioEngine::roleStatusChanged, this,
                [this](AudioRole role, const AudioRoleStatus&) {
                    if (role == AudioRole::TxInput) {
                        refreshCaptureStatus();
                    }
                });
        connect(m_retryCaptureBtn, &QPushButton::clicked,
                this, [this]() {
                    if (AudioEngine* e = engine()) {
                        e->retryCapture();
                    }
                });
    }
    refreshCaptureStatus();

    if (model) {
        connect(model, &RadioModel::remoteMicSourceStateChanged,
                this, [this]() { applyHeldControlGate(); });
        connect(&model->transmitModel(), &TransmitModel::moxChanged,
                this, [this](bool) { applyHeldControlGate(); });
    }
    applyHeldControlGate();

    // Set up the VU timer (10 ms refresh, stopped until Test Mic is pressed).
    m_vuTimer = new QTimer(this);
    m_vuTimer->setInterval(10);
    connect(m_vuTimer, &QTimer::timeout, this, &AudioTxInputPage::onVuTimerTick);

    // R-R3-49 (parity Task 3): in a remote window Mic Gain and the radio
    // microphone groups start closed until the Core says it takes them
    // (SetupDialog pushes setTransmitSettingsPermittedAt(3, ...)).
    if (model && !model->ownsLocalDsp()) {
        setTransmitSettingsPermittedAt(3, false, QString());
    }
}

AudioTxInputPage::~AudioTxInputPage()
{
    // R-R3-36: a destroyed page gives up its Test Mic capture demand.
    m_testMicLease.release();
    // Stop VU timer on destruction to prevent dangling callbacks.
    if (m_vuTimer) {
        m_vuTimer->stop();
    }
}

// R-R3-36: a hidden page (another Setup page selected, the dialog closed)
// stops its Test Mic, which releases the capture demand.
void AudioTxInputPage::hideEvent(QHideEvent* event)
{
    if (m_testMicBtn && m_testMicBtn->isChecked()) {
        m_testMicBtn->setChecked(false);
    }
    SetupPage::hideEvent(event);
}

// ---------------------------------------------------------------------------
// Shared audio/TxInput config (R-R3-36)
// ---------------------------------------------------------------------------

AudioEngine* AudioTxInputPage::engine()
{
    // R-R3-36: the PC microphone is this computer's, in a remote window
    // too (Test Mic opens it there); see RadioModel::localAudioDevices().
    return model() ? model()->localAudioDevices() : nullptr;
}

QGroupBox* AudioTxInputPage::pcMicGroupBox() const
{
    return m_pcMicCard;
}

QComboBox* AudioTxInputPage::deviceCombo() const
{
    return m_pcMicCard ? m_pcMicCard->deviceCombo() : nullptr;
}

QComboBox* AudioTxInputPage::driverApiCombo() const
{
    return m_pcMicCard ? m_pcMicCard->driverApiCombo() : nullptr;
}

QComboBox* AudioTxInputPage::bufferSizeCombo() const
{
    return m_pcMicCard ? m_pcMicCard->bufferSizeCombo() : nullptr;
}

// R-R3-36 / R-SPK-21: the card persists audio/TxInput itself, then the page
// hands the config to the engine; the engine's change (from this card, a
// TransmitModel pcMic* setter or anything else) reloads the card, as the
// Devices page did.
void AudioTxInputPage::wirePcMicCard()
{
    AudioEngine* eng = engine();
    if (!eng || !m_pcMicCard) {
        return;
    }
    // R-AUD-03, R-AUD-09, R-AUD-14: the engine's mic list and the mic
    // role's state.
    m_pcMicCard->setAudioEngine(eng);
    connect(m_pcMicCard, &DeviceCard::configChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                if (m_updatingFromEngine) { return; }
                if (AudioEngine* e = engine()) {
                    e->setTxInputConfig(cfg);
                }
            });
    connect(eng, &AudioEngine::txInputConfigChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                m_updatingFromEngine = true;
                QSignalBlocker blocker(m_pcMicCard);
                m_pcMicCard->loadFromSettings();
                m_pcMicCard->updateNegotiatedPill(cfg);
                m_updatingFromEngine = false;
            });
}

void AudioTxInputPage::refreshCaptureStatus()
{
    AudioEngine* eng = engine();
    const CaptureSupervisor::Status status =
        eng ? eng->captureStatus() : CaptureSupervisor::Status{};
    if (m_captureStatusLabel) {
        // R-AUD-09, R-AUD-11: the chosen mic missing or held by another
        // program reads in amber, as tx-mic-mockup.html's badge does.
        const QString missing =
            eng ? micRoleStatusText(eng->roleStatus(AudioRole::TxInput)) : QString();
        m_captureStatusLabel->setText(missing.isEmpty() ? captureStatusText(status) : missing);
        m_captureStatusLabel->setStyleSheet(
            missing.isEmpty() ? QString() : QStringLiteral("QLabel { color: #e0a030; }"));
    }
    if (m_retryCaptureBtn) {
        // R-AUD-09, R-AUD-11, R-AUD-24: a mic not connected or in use by
        // another program resumes by itself when it comes back; "Retry stays
        // for other mic failures".
        // The role status says so through the engine's retries too: each
        // retry opens a new capture, which is not Failed until the helper
        // answers again, while the role stays silent with its reason.
        using Reason = CaptureSupervisor::Status::Reason;
        const bool failed = status.state == CaptureSupervisor::Status::State::Failed;
        const AudioRoleStatus mic = eng ? eng->roleStatus(AudioRole::TxInput) : AudioRoleStatus{};
        const bool roleWaiting = mic.state == AudioRoleState::Silent
                                 && (mic.reason == AudioRoleReason::NotConnected
                                     || mic.reason == AudioRoleReason::InUse);
        const bool resumesByItself = roleWaiting
            || (failed && (status.reason == Reason::DeviceNotFound
                           || status.reason == Reason::DeviceInUse));
        m_retryCaptureBtn->setEnabled(eng != nullptr && failed && !resumesByItself);
        m_retryCaptureBtn->setToolTip(
            resumesByItself
                ? QStringLiteral("The mic resumes by itself when it comes back.")
                : QString());
    }
}

// R-R3-36: the controls held for the radio follow the transmit permission;
// this computer's microphone controls do not. See the header.
void AudioTxInputPage::setTransmitPermitted(bool permitted, const QString& reason)
{
    m_heldTransmitPermitted = permitted;
    m_heldTransmitReason = reason.isEmpty()
        ? tr("Remote transmit controls are not available from this Core.")
        : reason;
    applyHeldControlGate();
}

// R-R3-21 / R-R3-10: the held controls are the Core's settings as well.
void AudioTxInputPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_heldStationAvailable = available;
    m_heldStationReason = reason.isEmpty()
        ? tr("Connect to the Core to change these.") : reason;
    applyHeldControlGate();
}

// R-R3-49 (parity Task 3): Mic Gain (micGainDb) and the radio microphone
// groups are transmit settings that key nothing; in a remote window they
// change the Core's values while its radio is off the air.
void AudioTxInputPage::setTransmitSettingsPermittedAt(int version, bool permitted,
                                                      const QString& reason)
{
    if (version != 3) {
        return;
    }
    m_heldSettingsPermitted = permitted;
    m_heldSettingsReason = reason.isEmpty()
        ? IStationLink::transmitSettingsUnavailableReason() : reason;
    applyHeldControlGate();
}

// One gate for both conditions on each control: the save/restore helper
// keeps one saved state per control, so the conditions are combined here
// rather than stacked. The mic source follows the transmit permission (C2);
// Mic Gain and the radio microphone groups the transmit settings gate.
void AudioTxInputPage::applyHeldControlGate()
{
    gateTransmitControls({m_micSourceGroup},
        m_heldTransmitPermitted && m_heldStationAvailable,
        m_heldStationAvailable ? m_heldTransmitReason : m_heldStationReason);
    if (model() && !model()->ownsLocalDsp()) {
        const QString common = model()->micSourceChangeReason(MicSource::Pc);
        if (m_heldTransmitPermitted && m_heldStationAvailable && !common.isEmpty()) {
            gateTransmitControls({m_micSourceGroup}, false, common);
        }
        const QString radioReason = model()->micSourceChangeReason(MicSource::Radio);
        m_radioMicBtn->setEnabled(radioReason.isEmpty() && model()->boardCapabilities().radioMicSelectable());
        m_radioMicBtn->setToolTip(radioReason.isEmpty()
            ? (m_radioMicNeedsAddOn ? RadioModel::radioMicAddOnNote() : QString()) : radioReason);
        QString status = radioReason;
        if (auto* source = model()->stationLink() ? model()->stationLink()->remoteTransmit() : nullptr) {
            if (!source->micSourceSettled()) {
                status = source->micSourceReason();
            } else if (source->acceptedMicSource() == RemoteMicSource::RadioMic) {
                status = QStringLiteral("Radio microphone at the Core (no microphone stream from this computer). ")
                    + remoteRadioVoxReason() + QStringLiteral(" ") + remoteRadioProgramReason();
            }
        }
        m_micSelectionStatusLabel->setText(status);
        m_micSelectionStatusLabel->setVisible(!status.isEmpty());
    }
    gateTransmitControls({m_micGainSlider, m_micGainLabel,
                          m_hermesGroup, m_orionGroup, m_saturnGroup},
        m_heldSettingsPermitted && m_heldStationAvailable,
        m_heldStationAvailable ? m_heldSettingsReason : m_heldStationReason);
}

// ---------------------------------------------------------------------------
// Build helpers
// ---------------------------------------------------------------------------

/*static*/ AudioTxInputPage::HostSystem AudioTxInputPage::thisSystem()
{
#if defined(Q_OS_WIN)
    return HostSystem::Windows;
#elif defined(Q_OS_MAC)
    return HostSystem::Mac;
#else
    return HostSystem::Linux;
#endif
}

// The device names are the ones the code publishes: "NereusSDR TX" is the
// Mac VAX driver's input device (hal-plugin/NereusSDRVAX.cpp) and the
// PulseAudio sink's description (LinuxPipeBus.cpp); "NereusSDR TX input"
// is the PipeWire stream's node description (PipeWireBus.cpp).
/*static*/ QString AudioTxInputPage::vaxSourceToolTip(HostSystem system)
{
    switch (system) {
    case HostSystem::Mac:
        return QStringLiteral(
            "Transmits the audio a program such as FreeDV or WSJT-X plays to the "
            "\"NereusSDR TX\" device. Set that program's audio output to NereusSDR TX.");
    case HostSystem::Linux:
        return QStringLiteral(
            "Transmits the audio a program such as FreeDV or WSJT-X plays to the "
            "\"NereusSDR TX\" sink. With PipeWire, connect the program's audio "
            "output to the \"NereusSDR TX input\" stream.");
    case HostSystem::Windows:
        break;
    }
    return TransmitModel::vaxSourceUnavailableReason();
}

void AudioTxInputPage::buildPage(bool radioMicSelectable, HPSDRHW hw)
{
    // ── Mic Source group box (I.1) ────────────────────────────────────────────
    auto* srcGrp = new QGroupBox(QStringLiteral("Source"), this);
    m_micSourceGroup = srcGrp;
    auto* srcLayout = new QVBoxLayout(srcGrp);

    m_pcMicBtn    = new QRadioButton(QStringLiteral("PC Mic"), srcGrp);
    m_radioMicBtn = new QRadioButton(QStringLiteral("Radio Mic"), srcGrp);
    m_vaxMicBtn   = new QRadioButton(QStringLiteral("VAX TX (virtual device)"), srcGrp);
    // R-SPK-21, R-AUD-01: the VAX transmit device exists on macOS and Linux
    // only; on Windows the choice is greyed with the route through PC Mic,
    // and TransmitModel falls a saved VAX choice back to PC Mic (as an
    // unselectable Radio Mic falls back through the HL2 lock).
    const bool vaxAvailable = model() ? model()->transmitModel().vaxSourceAvailable()
                                      : TransmitModel::kVaxSourceAvailableOnThisSystem;
    m_vaxMicBtn->setEnabled(vaxAvailable);
    m_vaxMicBtn->setToolTip(vaxAvailable ? vaxSourceToolTip(thisSystem())
                                         : TransmitModel::vaxSourceUnavailableReason());

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->addButton(m_pcMicBtn,    static_cast<int>(MicSource::Pc));
    m_buttonGroup->addButton(m_radioMicBtn, static_cast<int>(MicSource::Radio));
    m_buttonGroup->addButton(m_vaxMicBtn,   static_cast<int>(MicSource::Vax));

    // PC Mic is selected by default.
    m_pcMicBtn->setChecked(true);

    // Gate Radio Mic on the board taking the radio mic (a mic jack, or the
    // HL2's audio add-on board).
    if (!radioMicSelectable) {
        m_radioMicBtn->setEnabled(false);
        m_radioMicBtn->setToolTip(
            QStringLiteral("Radio mic jack not present on Hermes Lite 2"));
    }

    srcLayout->addWidget(m_pcMicBtn);
    srcLayout->addWidget(m_radioMicBtn);
    // Radio codec lane: the HL2 gateware cannot report its AK4951 audio
    // add-on board, so Radio Mic stays open with a plain note, as mi0bot
    // leaves Mic In / Line In open on every model (mi0bot setup.cs:14566-14589
    // [@c26a8a4]).
    // Built on every board and shown only on one that needs the add-on, so
    // onCurrentRadioChanged can show it when an HL2 connects later.
    if (m_radioMicNeedsAddOn) {
        m_radioMicBtn->setToolTip(RadioModel::radioMicAddOnNote());
    }
    m_radioMicNoteLabel = new QLabel(RadioModel::radioMicAddOnNote(), srcGrp);
    m_radioMicNoteLabel->setObjectName(QStringLiteral("radioMicAddOnNote"));
    m_radioMicNoteLabel->setWordWrap(true);
    m_radioMicNoteLabel->setVisible(m_radioMicNeedsAddOn);
    srcLayout->addWidget(m_radioMicNoteLabel);
    srcLayout->addWidget(m_vaxMicBtn);
    m_micSelectionStatusLabel = new QLabel(srcGrp);
    m_micSelectionStatusLabel->setObjectName(QStringLiteral("remoteMicSelectionStatus"));
    m_micSelectionStatusLabel->setWordWrap(true);
    m_micSelectionStatusLabel->hide();
    srcLayout->addWidget(m_micSelectionStatusLabel);
    // R-SPK-21: every source's section stays in view (audio-setup.html).
    auto* srcNote = new QLabel(QStringLiteral(
        "Pick where transmit audio comes from. The other sections stay visible "
        "but greyed out until you pick them."), srcGrp);
    srcNote->setObjectName(QStringLiteral("micSourceNote"));
    srcNote->setWordWrap(true);
    srcLayout->addWidget(srcNote);

    addContent(srcGrp);

    // UI → Model: user toggles a radio button.
    connect(m_buttonGroup, &QButtonGroup::idToggled,
            this, &AudioTxInputPage::onMicSourceButtonToggled);

    // ── PC Mic group box (I.2) ────────────────────────────────────────────────
    auto* pcMicGroupContainer = new QVBoxLayout();
    buildPcMicGroup(pcMicGroupContainer);
    addContent(pcMicGroupContainer);

    // ── Radio Mic per-family group boxes (I.3) ────────────────────────────────
    // R-SPK-21: inside one container, which is greyed out unless Radio Mic
    // is picked. The transmit gates disable the groups themselves, so the
    // two never fight over one widget's state.
    m_radioMicSection = new QWidget(this);
    m_radioMicSection->setObjectName(QStringLiteral("radioMicSection"));
    auto* radioMicContainer = new QVBoxLayout(m_radioMicSection);
    radioMicContainer->setContentsMargins(0, 0, 0, 0);
    buildHermesRadioMicGroup(radioMicContainer);
    buildOrionRadioMicGroup(radioMicContainer);
    buildSaturnRadioMicGroup(radioMicContainer);
    buildRadioMicPlaceholder(radioMicContainer);
    addContent(m_radioMicSection);

    // ── Mic gain (R-SPK-21) ───────────────────────────────────────────────
    auto* micGainContainer = new QVBoxLayout();
    buildMicGainGroup(micGainContainer);
    addContent(micGainContainer);

    // This board's radio mic group (or the placeholder) is in view; the
    // source picked decides which section is live.
    updateRadioMicGroupVisibility(hw);
    updateSourceSections(MicSource::Pc);
}

void AudioTxInputPage::buildPcMicGroup(QVBoxLayout* parentLayout)
{
    // R-SPK-21 / R-SPK-22: the Devices page's microphone card, on the same
    // audio/TxInput keys, is the PC microphone section. Driver and Device
    // stay in front; Sample rate, Bit depth, Channels, Buffer size, Delay
    // and Negotiated fold under Device details. The card's Monitor
    // TX input and tone check rows follow UnbuiltFeatures as before.
    m_pcMicCard = new DeviceCard(QStringLiteral("audio/TxInput"),
                                 DeviceCard::Role::Input, false, this);
    m_pcMicCard->setObjectName(QStringLiteral("pcMicrophoneGroup"));
    m_pcMicCard->setTitle(QStringLiteral("PC microphone"));

    // ── Test Mic + VU bar ─────────────────────────────────────────────────────
    auto* testRowWidget = new QWidget(m_pcMicCard);
    auto* testRow = new QHBoxLayout(testRowWidget);
    testRow->setContentsMargins(0, 0, 0, 0);
    m_testMicBtn = new QPushButton(QStringLiteral("Test Mic"), testRowWidget);
    m_testMicBtn->setCheckable(true);
    m_testMicBtn->setToolTip(
        QStringLiteral("Click to open the selected PC mic and see the live level"));

    m_vuBar = new HGauge(testRowWidget);
    m_vuBar->setRange(0.0, 100.0);
    m_vuBar->setYellowStart(80.0);
    m_vuBar->setRedStart(95.0);
    m_vuBar->setValue(0.0);
    m_vuBar->setMinimumWidth(120);

    testRow->addWidget(m_testMicBtn);
    testRow->addWidget(m_vuBar, 1);
    m_pcMicCard->addBelowDevice(testRowWidget);

    connect(m_testMicBtn, &QPushButton::toggled,
            this, &AudioTxInputPage::onTestMicToggled);

    // ── Microphone status + Retry (R-R3-36), once in Setup ────────────────────
    auto* statusRowWidget = new QWidget(m_pcMicCard);
    auto* statusRow = new QHBoxLayout(statusRowWidget);
    statusRow->setContentsMargins(0, 0, 0, 0);
    m_captureStatusLabel = new QLabel(statusRowWidget);
    m_captureStatusLabel->setObjectName(QStringLiteral("captureStatus"));
    m_captureStatusLabel->setWordWrap(true);
    m_retryCaptureBtn = new QPushButton(QStringLiteral("Retry microphone"), statusRowWidget);
    m_retryCaptureBtn->setObjectName(QStringLiteral("retryCapture"));
    m_retryCaptureBtn->setEnabled(false);

    statusRow->addWidget(m_captureStatusLabel, 1);
    statusRow->addWidget(m_retryCaptureBtn);
    m_pcMicCard->addBelowDevice(statusRowWidget);

    parentLayout->addWidget(m_pcMicCard);
}

void AudioTxInputPage::buildMicGainGroup(QVBoxLayout* parentLayout)
{
    // R-SPK-21: Mic gain is its own group; it applies to every source. The
    // row keeps its "Mic Gain:" label, which the Setup description names.
    m_micGainGroup = new QGroupBox(QStringLiteral("Mic gain"), this);
    m_micGainGroup->setObjectName(QStringLiteral("micGainGroup"));
    auto* grpLayout = new QFormLayout(m_micGainGroup);
    grpLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grpLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // ── Mic Gain ───────────────────────────────────────────────────────
    // Range is read from BoardCapabilities::micGainMinDb / micGainMaxDb.
    //
    // From Thetis console.cs:19151-19171 [v2.10.3.13]:
    //   private int mic_gain_min = -40;  // runtime default for all known boards
    //   private int mic_gain_max = 10;   // runtime default for all known boards
    //
    // Unknown board fallback uses TransmitModel::kMicGainDbMin/Max (-50/+70).
    // The initial slider value clamps to the per-board range so an initial
    // value of -6 dB is always valid within [-40, +10] (known boards) or
    // [-50, +70] (Unknown).
    //
    // Board capabilities are static at construction time for 3M-1b.
    // TODO [3M-1b I.x]: refresh slider range on currentRadioChanged when
    // RadioModel emits a capability-change signal (dynamic re-eval).
    const int micGainMin = model()
        ? model()->boardCapabilities().micGainMinDb
        : TransmitModel::kMicGainDbMin;
    const int micGainMax = model()
        ? model()->boardCapabilities().micGainMaxDb
        : TransmitModel::kMicGainDbMax;

    m_micGainSlider = new QSlider(Qt::Horizontal, this);
    m_micGainSlider->setProperty("nereusSetupId", "audio.txInput.micGain");
    m_micGainSlider->setMinimum(micGainMin);
    m_micGainSlider->setMaximum(micGainMax);
    m_micGainSlider->setSingleStep(1);
    // Default -6 dB — clamp to range in case the initial model value falls
    // outside (e.g. a loaded -50 dB value on a board with max = +10).
    m_micGainSlider->setValue(qBound(micGainMin, -6, micGainMax));

    m_micGainLabel = new QLabel(QStringLiteral("-6 dB"), this);
    m_micGainLabel->setMinimumWidth(60);

    auto* gainRow = new QHBoxLayout();
    gainRow->addWidget(m_micGainSlider);
    gainRow->addWidget(m_micGainLabel);
    grpLayout->addRow(QStringLiteral("Mic Gain:"), gainRow);

    connect(m_micGainSlider, &QSlider::valueChanged,
            this, &AudioTxInputPage::onMicGainSliderChanged);

    auto* gainNote = new QLabel(
        QStringLiteral("Applies to whichever source is picked above."), m_micGainGroup);
    gainNote->setObjectName(QStringLiteral("micGainNote"));
    gainNote->setWordWrap(true);
    grpLayout->addRow(gainNote);

    parentLayout->addWidget(m_micGainGroup);
}

// ---------------------------------------------------------------------------
// Source sections (R-SPK-21): never hidden, greyed unless picked
// ---------------------------------------------------------------------------

void AudioTxInputPage::updateSourceSections(MicSource source)
{
    // setEnabled on the card and on the radio section's container only:
    // the transmit gates own the radio mic groups' and Mic Gain's state.
    if (m_pcMicCard) {
        m_pcMicCard->setEnabled(source == MicSource::Pc);
    }
    if (m_radioMicSection) {
        m_radioMicSection->setEnabled(source == MicSource::Radio);
    }
}

// ---------------------------------------------------------------------------
// Radio Mic per-family group visibility (I.3, R-SPK-21)
// This board's family group is in view whichever source is picked (its
// section is greyed unless Radio Mic is picked); the other families' groups
// do not apply to this board and stay hidden. A board with no group shows
// the placeholder.
// ---------------------------------------------------------------------------

void AudioTxInputPage::updateRadioMicGroupVisibility(HPSDRHW hw)
{
    // The Hermes Lite 2 takes the Hermes group's Mic In / Line In, boost and
    // Line In Gain through its AK4951 add-on board (P1CodecHl2).
    const bool isHermes = (hw == HPSDRHW::Hermes
                        || hw == HPSDRHW::HermesII
                        || hw == HPSDRHW::Angelia
                        || hw == HPSDRHW::Atlas
                        || (hw == HPSDRHW::HermesLite && m_radioMicNeedsAddOn));
    const bool isOrion  = (hw == HPSDRHW::Orion
                        || hw == HPSDRHW::OrionMKII);
    const bool isSaturn = (hw == HPSDRHW::Saturn
                        || hw == HPSDRHW::SaturnMKII);

    if (m_hermesGroup) { m_hermesGroup->setVisible(isHermes); }
    if (m_orionGroup)  { m_orionGroup->setVisible(isOrion);  }
    if (m_saturnGroup) { m_saturnGroup->setVisible(isSaturn); }
    if (m_radioMicPlaceholder) {
        m_radioMicPlaceholder->setVisible(!isHermes && !isOrion && !isSaturn);
    }
}

void AudioTxInputPage::buildRadioMicPlaceholder(QVBoxLayout* parentLayout)
{
    // R-SPK-21: a board without a radio mic group still shows the section,
    // with why there is nothing to set (audio-setup.html).
    m_radioMicPlaceholder = new QGroupBox(QStringLiteral("Radio microphone"), this);
    m_radioMicPlaceholder->setObjectName(QStringLiteral("radioMicPlaceholder"));
    auto* layout = new QVBoxLayout(m_radioMicPlaceholder);
    m_radioMicPlaceholderNote = new QLabel(m_radioMicPlaceholder);
    m_radioMicPlaceholderNote->setObjectName(QStringLiteral("radioMicPlaceholderNote"));
    m_radioMicPlaceholderNote->setWordWrap(true);
    refreshRadioMicPlaceholderNote(m_hw);
    layout->addWidget(m_radioMicPlaceholderNote);
    parentLayout->addWidget(m_radioMicPlaceholder);
}

void AudioTxInputPage::refreshRadioMicPlaceholderNote(HPSDRHW hw)
{
    if (m_radioMicPlaceholderNote) {
        m_radioMicPlaceholderNote->setText(hw == HPSDRHW::Unknown
            ? QStringLiteral("Connect a radio to set up its mic jack.")
            : QStringLiteral("This radio has no mic jack."));
    }
}

// R-SPK-21: the radio changed (connect, disconnect, another board).
// Everything the constructor built from the board follows it: Radio Mic's
// state, tooltip and add-on note, the Hermes group's title and add-on
// notes, the placeholder's note and which family group is in view, so a
// page opened before the radio connected does not keep the old board.
void AudioTxInputPage::onCurrentRadioChanged()
{
    if (!model()) {
        return;
    }
    const BoardCapabilities& caps = model()->boardCapabilities();
    const HPSDRHW hw = caps.board;
    m_hw = hw;
    m_radioMicNeedsAddOn = caps.radioMicNeedsAddOn;
    if (m_radioMicNoteLabel) {
        m_radioMicNoteLabel->setVisible(m_radioMicNeedsAddOn);
    }
    if (m_hermesGroup) {
        m_hermesGroup->setTitle(hermesGroupTitle(hw));
    }
    applyHermesAddOnNotes(hw);
    refreshRadioMicPlaceholderNote(hw);
    updateRadioMicGroupVisibility(hw);
    if (model()->ownsLocalDsp()) {
        const bool selectable = caps.radioMicSelectable();
        m_radioMicBtn->setEnabled(selectable);
        m_radioMicBtn->setToolTip(!selectable
            ? QStringLiteral("Radio mic jack not present on Hermes Lite 2")
            : (m_radioMicNeedsAddOn ? RadioModel::radioMicAddOnNote() : QString()));
    } else {
        // A remote window's Radio Mic follows the Core's reasons too.
        applyHeldControlGate();
    }
}

// ---------------------------------------------------------------------------
// lineInBoostLabel: format dB label for the Line In Gain slider (I.3)
// ---------------------------------------------------------------------------

/*static*/ QString AudioTxInputPage::lineInBoostLabel(double dB)
{
    // One decimal place, as Thetis's udLineInBoost shows it.
    return QStringLiteral("%1 dB").arg(dB, 0, 'f', 1);
}

// Shows a Line In Gain in the slider (half decibels) and its label.
void AudioTxInputPage::showLineInBoost(double dB)
{
    if (!m_hermesLineInGainSlider) { return; }
    {
        QSignalBlocker blk(m_hermesLineInGainSlider);
        m_hermesLineInGainSlider->setValue(
            static_cast<int>(std::lround(dB * kLineInGainSliderScale)));
    }
    if (m_hermesLineInGainLabel) {
        m_hermesLineInGainLabel->setText(lineInBoostLabel(
            double(m_hermesLineInGainSlider->value()) / kLineInGainSliderScale));
    }
}

// ---------------------------------------------------------------------------
// Slot: Mic Source radio button toggled (UI → Model, I.1)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onMicSourceButtonToggled(int id, bool checked)
{
    if (m_updatingFromModel) { return; }
    if (!checked) { return; }  // only act on the newly-selected button

    if (!model()) { return; }

    const MicSource source = static_cast<MicSource>(id);
    model()->requestMicSource(source);
    // The requested choice becomes visible only after the Core's answer.
    onModelMicSourceChanged(model()->transmitModel().micSource());
    applyHeldControlGate();
}

// ---------------------------------------------------------------------------
// Slot: Model → UI, mic source changed (I.1)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onModelMicSourceChanged(MicSource source)
{
    syncButtonsFromModel(source);
    updateSourceSections(source);
}

void AudioTxInputPage::syncButtonsFromModel(MicSource source)
{
    if (!m_buttonGroup) { return; }

    m_updatingFromModel = true;
    QAbstractButton* btn = m_buttonGroup->button(static_cast<int>(source));
    if (btn) {
        btn->setChecked(true);
    }
    m_updatingFromModel = false;
}

// ---------------------------------------------------------------------------
// Slot: Test Mic button toggled (I.2 Row 4)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onTestMicToggled(bool checked)
{
    if (checked) {
        // R-R3-36: a real capture demand; the selected input opens even
        // before a radio is connected, and the status row reports progress.
        if (AudioEngine* eng = engine()) {
            m_testMicLease = eng->acquireCaptureDemand(CaptureSupervisor::Demand::TestMic);
        }
        m_vuTimer->start();
        if (m_testMicBtn) {
            m_testMicBtn->setText(QStringLiteral("Stop Test"));
        }
    } else {
        // Releases only this page's demand; an active session keeps its own.
        m_testMicLease.release();
        m_vuTimer->stop();
        if (m_vuBar) {
            m_vuBar->setValue(0.0);
        }
        if (m_testMicBtn) {
            m_testMicBtn->setText(QStringLiteral("Test Mic"));
        }
    }
}

// ---------------------------------------------------------------------------
// Slot: VU timer tick — read PC Mic input level and update HGauge (I.2 Row 4)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onVuTimerTick()
{
    if (!m_vuBar) { return; }

    float level = 0.0f;
    if (AudioEngine* eng = engine()) {
        // Peak level of the capture reader; 0.0f until capture is Ready.
        level = eng->pcMicInputLevel();
    }

    // Scale from normalized [0.0, 1.0] to gauge range [0, 100].
    m_vuBar->setValue(static_cast<double>(level) * 100.0);
}

// ---------------------------------------------------------------------------
// Slot: Mic Gain slider changed (I.2 Row 5, UI → Model)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onMicGainSliderChanged(int value)
{
    if (m_updatingFromModel) { return; }

    if (m_micGainLabel) {
        m_micGainLabel->setText(QStringLiteral("%1 dB").arg(value));
    }
    if (model()) {
        model()->transmitModel().setMicGainDb(value);
    }
}

// ---------------------------------------------------------------------------
// Slot: Model Mic Gain changed (I.2 Row 5, Model → UI)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onModelMicGainDbChanged(int dB)
{
    if (!m_micGainSlider) { return; }

    m_updatingFromModel = true;
    {
        QSignalBlocker blk(m_micGainSlider);
        m_micGainSlider->setValue(dB);
    }
    if (m_micGainLabel) {
        m_micGainLabel->setText(QStringLiteral("%1 dB").arg(dB));
    }
    m_updatingFromModel = false;
}

// ===========================================================================
// ── Radio Mic per-family group build helpers (I.3) ──────────────────────────
// ===========================================================================

// ---------------------------------------------------------------------------
// buildHermesRadioMicGroup — Hermes / Atlas / HermesII / Angelia family
// ---------------------------------------------------------------------------

void AudioTxInputPage::buildHermesRadioMicGroup(QVBoxLayout* parentLayout)
{
    m_hermesGroup = new QGroupBox(hermesGroupTitle(m_hw), this);
    auto* grpLayout = new QVBoxLayout(m_hermesGroup);

    // ── Row 1: Mic In / Line In radio buttons ─────────────────────────────────
    auto* micInBtn  = new QRadioButton(QStringLiteral("Mic In"),  m_hermesGroup);
    auto* lineInBtn = new QRadioButton(QStringLiteral("Line In"), m_hermesGroup);
    micInBtn->setChecked(true);  // Hermes default: mic input active

    m_hermesMicInputGroup = new QButtonGroup(this);
    m_hermesMicInputGroup->setProperty("nereusSetupId", "audio.txInput.hermesLineIn");
    m_hermesMicInputGroup->addButton(micInBtn,  0);  // id=0 → lineIn=false
    m_hermesMicInputGroup->addButton(lineInBtn, 1);  // id=1 → lineIn=true

    auto* inputRow = new QHBoxLayout();
    inputRow->addWidget(micInBtn);
    inputRow->addWidget(lineInBtn);
    inputRow->addStretch();
    grpLayout->addLayout(inputRow);

    connect(m_hermesMicInputGroup, &QButtonGroup::idToggled,
            this, &AudioTxInputPage::onHermesMicInputToggled);

    // ── Row 2: +20 dB Mic Boost checkbox ────────────────────────────────────
    m_hermesMicBoostChk = new QCheckBox(QStringLiteral("+20 dB Mic Boost"), m_hermesGroup);
    m_hermesMicBoostChk->setProperty("nereusSetupId", "audio.txInput.hermesMicBoost");
    m_hermesMicBoostChk->setChecked(true);  // TransmitModel default: true
    grpLayout->addWidget(m_hermesMicBoostChk);

    connect(m_hermesMicBoostChk, &QCheckBox::toggled,
            this, &AudioTxInputPage::onHermesMicBoostToggled);

    // ── Row 3: Line In Gain slider ───────────────────────────────────────────
    // Range kLineInBoostMin (-34.5) to kLineInBoostMax (12) in
    // kLineInBoostStep (1.5 dB) steps, as Thetis's udLineInBoost (setup.
    // designer.cs:47006-47034 [v2.10.3.15]: Increment 1.5, Minimum -34.5,
    // Maximum 12, one decimal). The slider counts half decibels
    // (kLineInGainSliderScale), so -34.5 dB is -69 and a step is 3.
    m_hermesLineInGainSlider = new QSlider(Qt::Horizontal, m_hermesGroup);
    m_hermesLineInGainSlider->setProperty("nereusSetupId", "audio.txInput.hermesLineInGain");
    m_hermesLineInGainSlider->setProperty("nereusSetupScale", double(kLineInGainSliderScale));
    m_hermesLineInGainSlider->setMinimum(
        static_cast<int>(std::lround(TransmitModel::kLineInBoostMin * kLineInGainSliderScale)));
    m_hermesLineInGainSlider->setMaximum(
        static_cast<int>(std::lround(TransmitModel::kLineInBoostMax * kLineInGainSliderScale)));
    m_hermesLineInGainSlider->setSingleStep(
        static_cast<int>(std::lround(TransmitModel::kLineInBoostStep * kLineInGainSliderScale)));
    m_hermesLineInGainSlider->setPageStep(m_hermesLineInGainSlider->singleStep());
    m_hermesLineInGainSlider->setValue(0);  // TransmitModel default: 0.0 dB

    m_hermesLineInGainLabel = new QLabel(lineInBoostLabel(0.0), m_hermesGroup);
    m_hermesLineInGainLabel->setMinimumWidth(60);

    auto* gainRow = new QHBoxLayout();
    gainRow->addWidget(new QLabel(QStringLiteral("Line In Gain:"), m_hermesGroup));
    gainRow->addWidget(m_hermesLineInGainSlider, 1);
    gainRow->addWidget(m_hermesLineInGainLabel);
    grpLayout->addLayout(gainRow);

    connect(m_hermesLineInGainSlider, &QSlider::valueChanged,
            this, &AudioTxInputPage::onHermesLineInGainChanged);

    // On the Hermes Lite 2 these settings reach the AK4951 on its audio
    // add-on board, which the gateware cannot report, so each row carries
    // the same note as Radio Mic (the Setup description's tooltip).
    applyHermesAddOnNotes(m_hw);

    parentLayout->addWidget(m_hermesGroup);
}

/*static*/ QString AudioTxInputPage::hermesGroupTitle(HPSDRHW hw)
{
    return hw == HPSDRHW::HermesLite ? QStringLiteral("Radio Mic (Hermes Lite 2)")
                                     : QStringLiteral("Radio Mic (Hermes / Atlas)");
}

void AudioTxInputPage::applyHermesAddOnNotes(HPSDRHW hw)
{
    const QString note = (hw == HPSDRHW::HermesLite && m_radioMicNeedsAddOn)
        ? RadioModel::radioMicAddOnNote() : QString();
    QList<QWidget*> rows{m_hermesMicBoostChk, m_hermesLineInGainSlider};
    if (m_hermesMicInputGroup) {
        rows << m_hermesMicInputGroup->button(0) << m_hermesMicInputGroup->button(1);
    }
    for (QWidget* w : rows) {
        if (w) {
            w->setToolTip(note);
        }
    }
}

// ---------------------------------------------------------------------------
// buildOrionRadioMicGroup — Orion / OrionMKII family
// ---------------------------------------------------------------------------

void AudioTxInputPage::buildOrionRadioMicGroup(QVBoxLayout* parentLayout)
{
    m_orionGroup = new QGroupBox(QStringLiteral("Radio Mic (Orion-MkII)"), this);
    auto* grpLayout = new QVBoxLayout(m_orionGroup);

    m_orionMicTipRingChk = new QCheckBox(
        QStringLiteral("Mic Tip-Ring (Tip is Mic)"), m_orionGroup);
    m_orionMicTipRingChk->setProperty("nereusSetupId", "audio.txInput.orionMicTipRing");
    m_orionMicTipRingChk->setChecked(true);  // TransmitModel default: true
    grpLayout->addWidget(m_orionMicTipRingChk);

    m_orionMicBiasChk = new QCheckBox(QStringLiteral("Mic Bias"), m_orionGroup);
    m_orionMicBiasChk->setProperty("nereusSetupId", "audio.txInput.orionMicBias");
    m_orionMicBiasChk->setChecked(false);  // TransmitModel default: false
    grpLayout->addWidget(m_orionMicBiasChk);

    m_orionMicPttDisabledChk = new QCheckBox(
        QStringLiteral("Mic PTT Disabled"), m_orionGroup);
    m_orionMicPttDisabledChk->setProperty("nereusSetupId", "audio.txInput.orionMicPttDisabled");
    m_orionMicPttDisabledChk->setChecked(false);  // TransmitModel default: false
    grpLayout->addWidget(m_orionMicPttDisabledChk);

    m_orionMicBoostChk = new QCheckBox(
        QStringLiteral("+20 dB Mic Boost"), m_orionGroup);
    m_orionMicBoostChk->setProperty("nereusSetupId", "audio.txInput.orionMicBoost");
    m_orionMicBoostChk->setChecked(true);  // TransmitModel default: true
    grpLayout->addWidget(m_orionMicBoostChk);

    connect(m_orionMicTipRingChk,    &QCheckBox::toggled,
            this, &AudioTxInputPage::onOrionMicTipRingToggled);
    connect(m_orionMicBiasChk,       &QCheckBox::toggled,
            this, &AudioTxInputPage::onOrionMicBiasToggled);
    connect(m_orionMicPttDisabledChk, &QCheckBox::toggled,
            this, &AudioTxInputPage::onOrionMicPttDisabledToggled);
    connect(m_orionMicBoostChk,      &QCheckBox::toggled,
            this, &AudioTxInputPage::onOrionMicBoostToggled);

    // Radio codec lane: Thetis greys out the ORION mic panel on the Red
    // Pitaya (RadioModel::orionMicPanelAvailable). The group stays in view,
    // disabled with its reason; the transmit gates keep this state as the
    // one they put back.
    if (!m_orionMicPanelAvailable) {
        m_orionGroup->setEnabled(false);
        m_orionGroup->setToolTip(RadioModel::orionMicPanelUnavailableReason());
        m_orionGroup->setAccessibleDescription(RadioModel::orionMicPanelUnavailableReason());
    }

    parentLayout->addWidget(m_orionGroup);
}

// ---------------------------------------------------------------------------
// buildSaturnRadioMicGroup — Saturn G2 family
// ---------------------------------------------------------------------------

void AudioTxInputPage::buildSaturnRadioMicGroup(QVBoxLayout* parentLayout)
{
    m_saturnGroup = new QGroupBox(QStringLiteral("Radio Mic (Saturn G2)"), this);
    auto* grpLayout = new QVBoxLayout(m_saturnGroup);

    // ── Row 1: 3.5 mm Jack / XLR radio buttons ───────────────────────────────
    auto* jackBtn = new QRadioButton(QStringLiteral("3.5 mm Jack"), m_saturnGroup);
    auto* xlrBtn  = new QRadioButton(QStringLiteral("XLR"),         m_saturnGroup);
    // TransmitModel default: micXlr=true → XLR selected by default.
    xlrBtn->setChecked(true);

    m_saturnMicInputGroup = new QButtonGroup(this);
    m_saturnMicInputGroup->setProperty("nereusSetupId", "audio.txInput.saturnMicXlr");
    m_saturnMicInputGroup->addButton(jackBtn, 0);  // id=0 → micXlr=false (3.5mm)
    m_saturnMicInputGroup->addButton(xlrBtn,  1);  // id=1 → micXlr=true  (XLR)

    auto* inputRow = new QHBoxLayout();
    inputRow->addWidget(jackBtn);
    inputRow->addWidget(xlrBtn);
    inputRow->addStretch();
    grpLayout->addLayout(inputRow);

    connect(m_saturnMicInputGroup, &QButtonGroup::idToggled,
            this, &AudioTxInputPage::onSaturnMicInputToggled);

    // ── Row 2: Mic Tip-Ring ─────────────────────────────────────────────────
    // Radio codec lane: Thetis enables the ORION mic panel (Tip / Ring) on
    // the G2 and G2-1K as well (setup.cs:20292, 20343 [v2.10.3.15]); its
    // Tip radio sends SetMicTipRing(0) (setup.cs:16504-16510), the same
    // TransmitModel::micTipRing the Orion group sets.
    m_saturnMicTipRingChk = new QCheckBox(
        QStringLiteral("Mic Tip-Ring (Tip is Mic)"), m_saturnGroup);
    m_saturnMicTipRingChk->setProperty("nereusSetupId", "audio.txInput.saturnMicTipRing");
    m_saturnMicTipRingChk->setChecked(true);  // TransmitModel default: true
    grpLayout->addWidget(m_saturnMicTipRingChk);
    connect(m_saturnMicTipRingChk, &QCheckBox::toggled,
            this, &AudioTxInputPage::onSaturnMicTipRingToggled);

    // ── Rows 3-5: three checkboxes ───────────────────────────────────────────
    m_saturnMicPttDisabledChk = new QCheckBox(
        QStringLiteral("Mic PTT Disabled"), m_saturnGroup);
    m_saturnMicPttDisabledChk->setProperty("nereusSetupId", "audio.txInput.saturnMicPttDisabled");
    m_saturnMicPttDisabledChk->setChecked(false);  // TransmitModel default
    grpLayout->addWidget(m_saturnMicPttDisabledChk);

    m_saturnMicBiasChk = new QCheckBox(QStringLiteral("Mic Bias"), m_saturnGroup);
    m_saturnMicBiasChk->setProperty("nereusSetupId", "audio.txInput.saturnMicBias");
    m_saturnMicBiasChk->setChecked(false);  // TransmitModel default
    grpLayout->addWidget(m_saturnMicBiasChk);

    m_saturnMicBoostChk = new QCheckBox(
        QStringLiteral("+20 dB Mic Boost"), m_saturnGroup);
    m_saturnMicBoostChk->setProperty("nereusSetupId", "audio.txInput.saturnMicBoost");
    m_saturnMicBoostChk->setChecked(true);  // TransmitModel default: true
    grpLayout->addWidget(m_saturnMicBoostChk);

    connect(m_saturnMicPttDisabledChk, &QCheckBox::toggled,
            this, &AudioTxInputPage::onSaturnMicPttDisabledToggled);
    connect(m_saturnMicBiasChk,        &QCheckBox::toggled,
            this, &AudioTxInputPage::onSaturnMicBiasToggled);
    connect(m_saturnMicBoostChk,       &QCheckBox::toggled,
            this, &AudioTxInputPage::onSaturnMicBoostToggled);

    parentLayout->addWidget(m_saturnGroup);
}

// ===========================================================================
// ── Radio Mic UI→Model slots — Hermes family (I.3) ──────────────────────────
// ===========================================================================

void AudioTxInputPage::onHermesMicInputToggled(int id, bool checked)
{
    if (m_updatingFromModel) { return; }
    if (!checked) { return; }
    if (!model()) { return; }
    // id=0 → Mic In (lineIn=false), id=1 → Line In (lineIn=true)
    model()->transmitModel().setLineIn(id == 1);
}

void AudioTxInputPage::onHermesMicBoostToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicBoost(on);
}

void AudioTxInputPage::onHermesLineInGainChanged(int sliderValue)
{
    if (m_updatingFromModel) { return; }
    // The slider counts half decibels; a value between steps (a drag) snaps
    // to the nearest 1.5 dB step from the minimum, as udLineInBoost's
    // Increment does.
    const int step = m_hermesLineInGainSlider ? m_hermesLineInGainSlider->singleStep() : 1;
    const int minimum = m_hermesLineInGainSlider ? m_hermesLineInGainSlider->minimum() : 0;
    const int snapped = minimum + static_cast<int>(
        std::lround(double(sliderValue - minimum) / step)) * step;
    if (m_hermesLineInGainSlider && snapped != sliderValue) {
        QSignalBlocker blk(m_hermesLineInGainSlider);
        m_hermesLineInGainSlider->setValue(snapped);
    }
    const double dB = double(snapped) / kLineInGainSliderScale;
    if (m_hermesLineInGainLabel) {
        m_hermesLineInGainLabel->setText(lineInBoostLabel(dB));
    }
    if (!model()) { return; }
    model()->transmitModel().setLineInBoost(dB);
}

// ===========================================================================
// ── Radio Mic UI→Model slots — Orion-MkII family (I.3) ─────────────────────
// ===========================================================================

void AudioTxInputPage::onOrionMicTipRingToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicTipRing(on);
}

void AudioTxInputPage::onOrionMicBiasToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicBias(on);
}

void AudioTxInputPage::onOrionMicPttDisabledToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicPttDisabled(on);
}

void AudioTxInputPage::onOrionMicBoostToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicBoost(on);
}

// ===========================================================================
// ── Radio Mic UI→Model slots — Saturn G2 family (I.3) ──────────────────────
// ===========================================================================

void AudioTxInputPage::onSaturnMicInputToggled(int id, bool checked)
{
    if (m_updatingFromModel) { return; }
    if (!checked) { return; }
    if (!model()) { return; }
    // id=0 → 3.5mm (micXlr=false), id=1 → XLR (micXlr=true)
    model()->transmitModel().setMicXlr(id == 1);
}

void AudioTxInputPage::onSaturnMicPttDisabledToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicPttDisabled(on);
}

void AudioTxInputPage::onSaturnMicBiasToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicBias(on);
}

void AudioTxInputPage::onSaturnMicBoostToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicBoost(on);
}

void AudioTxInputPage::onSaturnMicTipRingToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    if (!model()) { return; }
    model()->transmitModel().setMicTipRing(on);
}

// ===========================================================================
// ── Radio Mic Model→UI slots — all families (I.3) ───────────────────────────
// ===========================================================================

void AudioTxInputPage::onModelLineInChanged(bool on)
{
    if (!m_hermesMicInputGroup) { return; }
    m_updatingFromModel = true;
    QAbstractButton* btn = m_hermesMicInputGroup->button(on ? 1 : 0);
    if (btn) { btn->setChecked(true); }
    m_updatingFromModel = false;
}

void AudioTxInputPage::onModelMicBoostChanged(bool on)
{
    m_updatingFromModel = true;
    if (m_hermesMicBoostChk) {
        QSignalBlocker blk(m_hermesMicBoostChk);
        m_hermesMicBoostChk->setChecked(on);
    }
    if (m_orionMicBoostChk) {
        QSignalBlocker blk(m_orionMicBoostChk);
        m_orionMicBoostChk->setChecked(on);
    }
    if (m_saturnMicBoostChk) {
        QSignalBlocker blk(m_saturnMicBoostChk);
        m_saturnMicBoostChk->setChecked(on);
    }
    m_updatingFromModel = false;
}

void AudioTxInputPage::onModelLineInBoostChanged(double dB)
{
    m_updatingFromModel = true;
    showLineInBoost(dB);
    m_updatingFromModel = false;
}

void AudioTxInputPage::onModelMicTipRingChanged(bool on)
{
    m_updatingFromModel = true;
    for (QCheckBox* box : {m_orionMicTipRingChk, m_saturnMicTipRingChk}) {
        if (box) {
            QSignalBlocker blk(box);
            box->setChecked(on);
        }
    }
    m_updatingFromModel = false;
}

void AudioTxInputPage::onModelMicBiasChanged(bool on)
{
    m_updatingFromModel = true;
    if (m_orionMicBiasChk) {
        QSignalBlocker blk(m_orionMicBiasChk);
        m_orionMicBiasChk->setChecked(on);
    }
    if (m_saturnMicBiasChk) {
        QSignalBlocker blk(m_saturnMicBiasChk);
        m_saturnMicBiasChk->setChecked(on);
    }
    m_updatingFromModel = false;
}

void AudioTxInputPage::onModelMicPttDisabledChanged(bool on)
{
    m_updatingFromModel = true;
    if (m_orionMicPttDisabledChk) {
        QSignalBlocker blk(m_orionMicPttDisabledChk);
        m_orionMicPttDisabledChk->setChecked(on);
    }
    if (m_saturnMicPttDisabledChk) {
        QSignalBlocker blk(m_saturnMicPttDisabledChk);
        m_saturnMicPttDisabledChk->setChecked(on);
    }
    m_updatingFromModel = false;
}

void AudioTxInputPage::onModelMicXlrChanged(bool on)
{
    if (!m_saturnMicInputGroup) { return; }
    m_updatingFromModel = true;
    QAbstractButton* btn = m_saturnMicInputGroup->button(on ? 1 : 0);
    if (btn) { btn->setChecked(true); }
    m_updatingFromModel = false;
}

} // namespace NereusSDR
