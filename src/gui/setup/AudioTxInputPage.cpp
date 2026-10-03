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
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "AudioTxInputPage.h"
#include "CaptureStatusText.h"

#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/AudioEngine.h"
#include "core/session/IStationLink.h"
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

// PortAudio enumeration — only the opaque struct access and hostApis() are
// used here (no direct Pa_* calls); PortAudioBus wraps the C API.
#include "core/audio/PortAudioBus.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Static constant: discrete buffer-size steps exposed by the slider.
// ---------------------------------------------------------------------------
const QVector<int> AudioTxInputPage::kBufferSizes = {
    64, 128, 256, 512, 1024, 2048, 4096, 8192
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Returns "N samples (M ms @ 48 kHz)" for the given sample count.
// Reference sample rate is 48 000 Hz (standard NereusSDR audio rate).
/*static*/ QString AudioTxInputPage::latencyString(int samples)
{
    // latency_ms = samples / 48000.0 * 1000.0
    const double ms = static_cast<double>(samples) / 48000.0 * 1000.0;
    return QStringLiteral("%1 samples (%2 ms @ 48 kHz)")
        .arg(samples)
        .arg(ms, 0, 'f', 1);
}

// Returns the OS-default PortAudio host API index.
// Falls back to 0 if enumeration yields nothing (safe for test environments
// where Pa_Initialize() has not been called).
/*static*/ int AudioTxInputPage::defaultHostApiIndex()
{
    const QVector<PortAudioBus::HostApiInfo> apis = PortAudioBus::hostApis();

#if defined(Q_OS_MACOS)
    // macOS: CoreAudio is identified by name "Core Audio".
    for (const auto& api : apis) {
        if (api.name.contains(QLatin1String("Core Audio"), Qt::CaseInsensitive)) {
            return api.index;
        }
    }
#elif defined(Q_OS_LINUX)
    // Linux: prefer PipeWire if NEREUS_HAVE_PIPEWIRE is defined and enumerable,
    // else fall back to Pulse.
    for (const auto& api : apis) {
        if (api.name.contains(QLatin1String("PipeWire"), Qt::CaseInsensitive)) {
            return api.index;
        }
    }
    for (const auto& api : apis) {
        if (api.name.contains(QLatin1String("Pulse"), Qt::CaseInsensitive)) {
            return api.index;
        }
    }
#elif defined(Q_OS_WIN)
    // Windows: prefer WASAPI.
    for (const auto& api : apis) {
        if (api.name.contains(QLatin1String("WASAPI"), Qt::CaseInsensitive)) {
            return api.index;
        }
    }
#endif

    // Fallback: first enumerated API, or -1 (PA default) if none available.
    return apis.isEmpty() ? -1 : apis.first().index;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AudioTxInputPage::AudioTxInputPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TX Input"), model, parent)
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

    // R-R3-36: the PC Mic controls show the one audio/TxInput config the
    // engine holds (the Devices page edits the same one) and follow every
    // change to it, whichever page made it.
    if (AudioEngine* eng = engine()) {
        applyTxInputConfigToControls(eng->txInputConfig());
        connect(eng, &AudioEngine::txInputConfigChanged,
                this, &AudioTxInputPage::applyTxInputConfigToControls);
        connect(eng, &AudioEngine::captureStatusChanged,
                this, [this](const CaptureSupervisor::Status&) { refreshCaptureStatus(); });
        connect(m_retryCaptureBtn, &QPushButton::clicked,
                this, [this]() {
                    if (AudioEngine* e = engine()) {
                        e->retryCapture();
                    }
                });
    } else {
        applyTxInputConfigToControls(AudioDeviceConfig{});
    }
    refreshCaptureStatus();

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

void AudioTxInputPage::applyTxInputConfigToControls(const AudioDeviceConfig& cfg)
{
    m_applyingTxInputConfig = true;

    // Backend: -1 (PortAudio default) is shown as the OS-default API.
    const int effectiveApi = (cfg.hostApiIndex == -1) ? defaultHostApiIndex()
                                                      : cfg.hostApiIndex;
    if (m_backendCombo) {
        const int idx = m_backendCombo->findData(effectiveApi);
        if (idx >= 0) {
            QSignalBlocker blk(m_backendCombo);
            m_backendCombo->setCurrentIndex(idx);
        }
    }
    populateDeviceCombo(effectiveApi);

    // Device: empty is the "(default)" entry. A named device that is not
    // present stays selected under its own name, so the page never shows a
    // different microphone than the one configured.
    if (m_deviceCombo) {
        QSignalBlocker blk(m_deviceCombo);
        int idx = 0;
        if (!cfg.deviceName.isEmpty()) {
            idx = m_deviceCombo->findData(cfg.deviceName);
            if (idx < 0) {
                m_deviceCombo->addItem(
                    QStringLiteral("%1 (not available)").arg(cfg.deviceName),
                    cfg.deviceName);
                idx = m_deviceCombo->count() - 1;
            }
        }
        m_deviceCombo->setCurrentIndex(idx);
    }

    // Buffer.
    if (m_bufferSlider) {
        const int pos = kBufferSizes.indexOf(cfg.bufferSamples);
        if (pos >= 0) {
            QSignalBlocker blk(m_bufferSlider);
            m_bufferSlider->setValue(pos);
        }
    }
    updateBufferLabel(cfg.bufferSamples);

    m_applyingTxInputConfig = false;
}

// Persists exactly as the Devices page TX Input card does, then hands the
// config to the engine (which reports it back through txInputConfigChanged).
void AudioTxInputPage::commitTxInputConfig(const AudioDeviceConfig& cfg)
{
    AudioEngine* eng = engine();
    if (!eng) {
        return;
    }
    cfg.saveToSettings(QStringLiteral("audio/TxInput"));
    AppSettings::instance().save();
    eng->setTxInputConfig(cfg);
}

void AudioTxInputPage::refreshCaptureStatus()
{
    AudioEngine* eng = engine();
    const CaptureSupervisor::Status status =
        eng ? eng->captureStatus() : CaptureSupervisor::Status{};
    if (m_captureStatusLabel) {
        m_captureStatusLabel->setText(captureStatusText(status));
    }
    if (m_retryCaptureBtn) {
        m_retryCaptureBtn->setEnabled(
            eng != nullptr && status.state == CaptureSupervisor::Status::State::Failed);
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
    gateTransmitControls({m_micGainSlider, m_micGainLabel,
                          m_hermesGroup, m_orionGroup, m_saturnGroup},
        m_heldSettingsPermitted && m_heldStationAvailable,
        m_heldStationAvailable ? m_heldSettingsReason : m_heldStationReason);
}

// ---------------------------------------------------------------------------
// Build helpers
// ---------------------------------------------------------------------------

void AudioTxInputPage::buildPage(bool radioMicSelectable, HPSDRHW hw)
{
    // ── Mic Source group box (I.1) ────────────────────────────────────────────
    auto* srcGrp = new QGroupBox(QStringLiteral("Mic Source"), this);
    m_micSourceGroup = srcGrp;
    auto* srcLayout = new QVBoxLayout(srcGrp);

    m_pcMicBtn    = new QRadioButton(QStringLiteral("PC Mic"), srcGrp);
    m_radioMicBtn = new QRadioButton(QStringLiteral("Radio Mic"), srcGrp);
    m_vaxMicBtn   = new QRadioButton(QStringLiteral("VAX TX (virtual device)"), srcGrp);
    m_vaxMicBtn->setToolTip(QStringLiteral(
        "Use audio routed to the \"NereusSDR TX\" CoreAudio device by a "
        "3rd-party app (FreeDV, WSJT-X, etc.) as the TX mic input. "
        "Pulled from /nereussdr-vax-tx shared memory."));

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
    if (m_radioMicNeedsAddOn) {
        m_radioMicBtn->setToolTip(RadioModel::radioMicAddOnNote());
        m_radioMicNoteLabel = new QLabel(RadioModel::radioMicAddOnNote(), srcGrp);
        m_radioMicNoteLabel->setWordWrap(true);
        srcLayout->addWidget(m_radioMicNoteLabel);
    }
    srcLayout->addWidget(m_vaxMicBtn);

    contentLayout()->insertWidget(0, srcGrp);

    // UI → Model: user toggles a radio button.
    connect(m_buttonGroup, &QButtonGroup::idToggled,
            this, &AudioTxInputPage::onMicSourceButtonToggled);

    // ── PC Mic group box (I.2) ────────────────────────────────────────────────
    auto* pcMicGroupContainer = new QVBoxLayout();
    buildPcMicGroup(pcMicGroupContainer);
    contentLayout()->addLayout(pcMicGroupContainer);

    // ── Radio Mic per-family group boxes (I.3) ────────────────────────────────
    auto* radioMicContainer = new QVBoxLayout();
    buildHermesRadioMicGroup(radioMicContainer);
    buildOrionRadioMicGroup(radioMicContainer);
    buildSaturnRadioMicGroup(radioMicContainer);
    contentLayout()->addLayout(radioMicContainer);

    // Show PC Mic group only when PC Mic is selected.
    updatePcMicGroupVisibility(MicSource::Pc);
    // All Radio Mic groups hidden initially (PC Mic is selected by default).
    updateRadioMicGroupVisibility(MicSource::Pc, hw);
}

void AudioTxInputPage::buildPcMicGroup(QVBoxLayout* parentLayout)
{
    m_pcMicGroup = new QGroupBox(QStringLiteral("PC Mic"), this);
    auto* grpLayout = new QFormLayout(m_pcMicGroup);
    grpLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grpLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // ── Row 1: Backend ────────────────────────────────────────────────────────
    m_backendCombo = new QComboBox(this);
    populateBackendCombo();
    grpLayout->addRow(QStringLiteral("Backend:"), m_backendCombo);
    connect(m_backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioTxInputPage::onBackendChanged);

    // ── Row 2: Device ─────────────────────────────────────────────────────────
    m_deviceCombo = new QComboBox(this);
    // Initial population uses the current backend combo selection.
    // Actual seeding happens in the constructor after buildPage() returns.
    grpLayout->addRow(QStringLiteral("Device:"), m_deviceCombo);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioTxInputPage::onDeviceChanged);

    // ── Row 3: Buffer size ────────────────────────────────────────────────────
    m_bufferSlider = new QSlider(Qt::Horizontal, this);
    m_bufferSlider->setMinimum(0);
    m_bufferSlider->setMaximum(kBufferSizes.size() - 1);
    m_bufferSlider->setSingleStep(1);
    m_bufferSlider->setPageStep(1);
    // Default: index of 512 samples in kBufferSizes.
    const int defaultBufIdx = kBufferSizes.indexOf(512);
    m_bufferSlider->setValue(defaultBufIdx >= 0 ? defaultBufIdx : 3);

    m_bufferLabel = new QLabel(latencyString(512), this);
    m_bufferLabel->setMinimumWidth(220);

    auto* bufRow = new QHBoxLayout();
    bufRow->addWidget(m_bufferSlider);
    bufRow->addWidget(m_bufferLabel);
    grpLayout->addRow(QStringLiteral("Buffer:"), bufRow);

    connect(m_bufferSlider, &QSlider::valueChanged,
            this, &AudioTxInputPage::onBufferSliderChanged);

    // ── Row 4: Test Mic + VU bar ──────────────────────────────────────────────
    m_testMicBtn = new QPushButton(QStringLiteral("Test Mic"), this);
    m_testMicBtn->setCheckable(true);
    m_testMicBtn->setToolTip(
        QStringLiteral("Click to open the selected PC mic and see the live level"));

    m_vuBar = new HGauge(this);
    m_vuBar->setRange(0.0, 100.0);
    m_vuBar->setYellowStart(80.0);
    m_vuBar->setRedStart(95.0);
    m_vuBar->setValue(0.0);
    m_vuBar->setMinimumWidth(120);

    auto* testRow = new QHBoxLayout();
    testRow->addWidget(m_testMicBtn);
    testRow->addWidget(m_vuBar, 1);
    grpLayout->addRow(QStringLiteral(""), testRow);

    connect(m_testMicBtn, &QPushButton::toggled,
            this, &AudioTxInputPage::onTestMicToggled);

    // ── Microphone status + Retry (R-R3-36) ───────────────────────────────────
    m_captureStatusLabel = new QLabel(this);
    m_captureStatusLabel->setObjectName(QStringLiteral("captureStatus"));
    m_captureStatusLabel->setWordWrap(true);
    m_retryCaptureBtn = new QPushButton(QStringLiteral("Retry microphone"), this);
    m_retryCaptureBtn->setObjectName(QStringLiteral("retryCapture"));
    m_retryCaptureBtn->setEnabled(false);

    auto* statusRow = new QHBoxLayout();
    statusRow->addWidget(m_captureStatusLabel, 1);
    statusRow->addWidget(m_retryCaptureBtn);
    grpLayout->addRow(QStringLiteral(""), statusRow);

    // ── Row 5: Mic Gain ───────────────────────────────────────────────────────
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

    parentLayout->addWidget(m_pcMicGroup);
}

// ---------------------------------------------------------------------------
// Populate backend combo from PortAudioBus::hostApis()
// ---------------------------------------------------------------------------

void AudioTxInputPage::populateBackendCombo()
{
    if (!m_backendCombo) { return; }

    QSignalBlocker blk(m_backendCombo);
    m_backendCombo->clear();

    const QVector<PortAudioBus::HostApiInfo> apis = PortAudioBus::hostApis();
    if (apis.isEmpty()) {
        // PortAudio not initialized (e.g. in headless tests): show placeholder.
        m_backendCombo->addItem(QStringLiteral("(no audio APIs available)"), -1);
        return;
    }

    for (const auto& api : apis) {
        m_backendCombo->addItem(api.name, api.index);
    }

    // Select the OS-default API.
    const int defApi = defaultHostApiIndex();
    for (int i = 0; i < m_backendCombo->count(); ++i) {
        if (m_backendCombo->itemData(i).toInt() == defApi) {
            m_backendCombo->setCurrentIndex(i);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Populate device combo from PortAudioBus::inputDevicesFor()
// ---------------------------------------------------------------------------

void AudioTxInputPage::populateDeviceCombo(int hostApiIndex)
{
    if (!m_deviceCombo) { return; }

    QSignalBlocker blk(m_deviceCombo);
    m_deviceCombo->clear();

    if (hostApiIndex < 0) {
        m_deviceCombo->addItem(QStringLiteral("(default)"), QString());
        return;
    }

    const QVector<PortAudioBus::DeviceInfo> devices =
        PortAudioBus::inputDevicesFor(hostApiIndex);

    if (devices.isEmpty()) {
        m_deviceCombo->addItem(QStringLiteral("(no input devices)"), QString());
        return;
    }

    // First entry: use the PA default device for this host API.
    m_deviceCombo->addItem(QStringLiteral("(default)"), QString());

    for (const auto& dev : devices) {
        m_deviceCombo->addItem(dev.name, dev.name);
    }
}

// ---------------------------------------------------------------------------
// Buffer label update
// ---------------------------------------------------------------------------

void AudioTxInputPage::updateBufferLabel(int samples)
{
    if (m_bufferLabel) {
        m_bufferLabel->setText(latencyString(samples));
    }
}

// ---------------------------------------------------------------------------
// PC Mic group visibility: show group only when PC Mic is selected
// ---------------------------------------------------------------------------

void AudioTxInputPage::updatePcMicGroupVisibility(MicSource source)
{
    if (m_pcMicGroup) {
        m_pcMicGroup->setVisible(source == MicSource::Pc);
    }
}

// ---------------------------------------------------------------------------
// Radio Mic per-family group visibility (I.3)
// Shows the appropriate family group when Radio Mic is selected AND
// caps.hasMicJack == true. The hasMicJack gate is implicit: if hasMicJack
// is false the Radio Mic button is disabled so source can never be Radio in
// normal use; these groups are also explicitly hidden in that case.
// ---------------------------------------------------------------------------

void AudioTxInputPage::updateRadioMicGroupVisibility(MicSource source, HPSDRHW hw)
{
    // Only show a Radio Mic group when Radio Mic is actually selected.
    const bool radioMicActive = (source == MicSource::Radio);

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

    if (m_hermesGroup) { m_hermesGroup->setVisible(radioMicActive && isHermes); }
    if (m_orionGroup)  { m_orionGroup->setVisible(radioMicActive && isOrion);  }
    if (m_saturnGroup) { m_saturnGroup->setVisible(radioMicActive && isSaturn); }
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
    model()->transmitModel().setMicSource(source);
    updatePcMicGroupVisibility(source);
    updateRadioMicGroupVisibility(source, m_hw);
}

// ---------------------------------------------------------------------------
// Slot: Model → UI, mic source changed (I.1)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onModelMicSourceChanged(MicSource source)
{
    syncButtonsFromModel(source);
    updatePcMicGroupVisibility(source);
    updateRadioMicGroupVisibility(source, m_hw);
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
// Slot: Backend combo changed (I.2 Row 1)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onBackendChanged(int comboIndex)
{
    if (!m_backendCombo) { return; }
    if (m_applyingTxInputConfig) { return; }

    const int hostApiIndex = m_backendCombo->itemData(comboIndex).toInt();

    // Repopulate device combo for the new host API.
    populateDeviceCombo(hostApiIndex);

    // R-R3-36: one audio/TxInput config. The Devices card stores the API by
    // name (driverApi) and index; keep both in step. Device name resets to
    // the default when the backend changes.
    AudioEngine* eng = engine();
    if (!eng) { return; }
    AudioDeviceConfig cfg = eng->txInputConfig();
    cfg.hostApiIndex = hostApiIndex;
    cfg.driverApi = (hostApiIndex < 0) ? QString() : m_backendCombo->itemText(comboIndex);
    cfg.deviceName.clear();
    commitTxInputConfig(cfg);
}

// ---------------------------------------------------------------------------
// Slot: Device combo changed (I.2 Row 2)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onDeviceChanged(int comboIndex)
{
    if (!m_deviceCombo) { return; }
    if (m_updatingFromModel || m_applyingTxInputConfig) { return; }

    AudioEngine* eng = engine();
    if (!eng) { return; }
    AudioDeviceConfig cfg = eng->txInputConfig();
    cfg.deviceName = m_deviceCombo->itemData(comboIndex).toString();
    commitTxInputConfig(cfg);
}

// ---------------------------------------------------------------------------
// Slot: Buffer slider changed (I.2 Row 3)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onBufferSliderChanged(int sliderPos)
{
    if (sliderPos < 0 || sliderPos >= kBufferSizes.size()) { return; }

    const int samples = kBufferSizes[sliderPos];
    updateBufferLabel(samples);
    if (m_applyingTxInputConfig) { return; }

    AudioEngine* eng = engine();
    if (!eng) { return; }
    AudioDeviceConfig cfg = eng->txInputConfig();
    cfg.bufferSamples = samples;
    commitTxInputConfig(cfg);
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
    m_hermesGroup = new QGroupBox(m_hw == HPSDRHW::HermesLite
                                      ? QStringLiteral("Radio Mic (Hermes Lite 2)")
                                      : QStringLiteral("Radio Mic (Hermes / Atlas)"),
                                  this);
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
    if (m_hw == HPSDRHW::HermesLite && m_radioMicNeedsAddOn) {
        const QString note = RadioModel::radioMicAddOnNote();
        micInBtn->setToolTip(note);
        lineInBtn->setToolTip(note);
        m_hermesMicBoostChk->setToolTip(note);
        m_hermesLineInGainSlider->setToolTip(note);
    }

    parentLayout->addWidget(m_hermesGroup);
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
