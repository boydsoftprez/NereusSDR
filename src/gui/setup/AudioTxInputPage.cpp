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
//
// Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "AudioTxInputPage.h"

#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "core/BoardCapabilities.h"
#include "core/AudioEngine.h"
#include "gui/HGauge.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

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
    const bool hasMicJack = model
        ? model->boardCapabilities().hasMicJack
        : true;  // safe default: don't disable Radio Mic for null model
    m_hw = model
        ? model->boardCapabilities().board
        : HPSDRHW::Unknown;

    buildPage(hasMicJack, m_hw);

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

        // Seed the PC Mic group controls from model session state.
        // Backend combo: resolve -1 to the OS default if the model still
        // holds the initial sentinel.
        const int storedApi = tx->pcMicHostApiIndex();
        const int effectiveApi = (storedApi == -1) ? defaultHostApiIndex() : storedApi;
        if (m_backendCombo) {
            const QVector<PortAudioBus::HostApiInfo> apis = PortAudioBus::hostApis();
            for (int i = 0; i < apis.size(); ++i) {
                if (apis[i].index == effectiveApi) {
                    QSignalBlocker blk(m_backendCombo);
                    m_backendCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
        populateDeviceCombo(effectiveApi);

        // Seed device combo selection from stored device name.
        const QString storedDevice = tx->pcMicDeviceName();
        if (m_deviceCombo && !storedDevice.isEmpty()) {
            const int idx = m_deviceCombo->findText(storedDevice);
            if (idx >= 0) {
                QSignalBlocker blk(m_deviceCombo);
                m_deviceCombo->setCurrentIndex(idx);
            }
        }

        // Seed buffer slider from stored buffer samples.
        const int storedBuf = tx->pcMicBufferSamples();
        if (m_bufferSlider) {
            const int pos = kBufferSizes.indexOf(storedBuf);
            if (pos >= 0) {
                QSignalBlocker blk(m_bufferSlider);
                m_bufferSlider->setValue(pos);
            }
            updateBufferLabel(storedBuf);
        }

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
        if (m_hermesLineInGainSlider) {
            QSignalBlocker blk(m_hermesLineInGainSlider);
            const int sliderVal = static_cast<int>(tx->lineInBoost());
            m_hermesLineInGainSlider->setValue(sliderVal);
            if (m_hermesLineInGainLabel) {
                m_hermesLineInGainLabel->setText(lineInBoostLabel(sliderVal));
            }
        }
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

        // Live model → UI connections for PC Mic session state.
        // Buffer samples: model change → slider position.
        connect(tx, &TransmitModel::pcMicBufferSamplesChanged,
                this, [this](int samples) {
                    if (!m_bufferSlider) { return; }
                    const int pos = kBufferSizes.indexOf(samples);
                    if (pos >= 0) {
                        QSignalBlocker blk(m_bufferSlider);
                        m_bufferSlider->setValue(pos);
                    }
                    updateBufferLabel(samples);
                });

        // Host API index: model change → backend combo selection.
        connect(tx, &TransmitModel::pcMicHostApiIndexChanged,
                this, [this](int hostApiIndex) {
                    if (!m_backendCombo) { return; }
                    for (int i = 0; i < m_backendCombo->count(); ++i) {
                        if (m_backendCombo->itemData(i).toInt() == hostApiIndex) {
                            QSignalBlocker blk(m_backendCombo);
                            m_backendCombo->setCurrentIndex(i);
                            break;
                        }
                    }
                });
    }

    // Set up the VU timer (10 ms refresh, stopped until Test Mic is pressed).
    m_vuTimer = new QTimer(this);
    m_vuTimer->setInterval(10);
    connect(m_vuTimer, &QTimer::timeout, this, &AudioTxInputPage::onVuTimerTick);
}

AudioTxInputPage::~AudioTxInputPage()
{
    // Stop VU timer on destruction to prevent dangling callbacks.
    if (m_vuTimer) {
        m_vuTimer->stop();
    }
}

// ---------------------------------------------------------------------------
// Build helpers
// ---------------------------------------------------------------------------

void AudioTxInputPage::buildPage(bool hasMicJack, HPSDRHW hw)
{
    // ── Mic Source group box (I.1) ────────────────────────────────────────────
    auto* srcGrp = new QGroupBox(QStringLiteral("Mic Source"), this);
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

    // Gate Radio Mic on hasMicJack capability.
    if (!hasMicJack) {
        m_radioMicBtn->setEnabled(false);
        m_radioMicBtn->setToolTip(
            QStringLiteral("Radio mic jack not present on Hermes Lite 2"));
    }

    srcLayout->addWidget(m_pcMicBtn);
    srcLayout->addWidget(m_radioMicBtn);
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
        QStringLiteral("Click to sample the selected PC mic and see the live level"));

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

    // ── Rows 6-7: Mic Gain Min / Max spinboxes (Task 21) ─────────────────────
    //
    // From Thetis setup.cs:9678-9694 [v2.10.3.13+501e3f51] (handlers) +
    // setup.designer.cs:46808-46866 [v2.10.3.13+501e3f51] (designer ranges):
    //   udMicGainMin: -96..0  (designer Min=-96, Max=0)
    //   udMicGainMax:   1..70 (designer Min=1, Max=70)
    // ValueChanged handler simply assigns console.MicGainMin / MicGainMax;
    // upstream then propagates to the slider bounds. We mirror that here
    // by updating m_micGainSlider->setMinimum / setMaximum live so the
    // user sees the new range immediately.
    m_micGainMinSpin = new QSpinBox(this);
    m_micGainMinSpin->setObjectName(QStringLiteral("micGainMinSpin"));
    m_micGainMinSpin->setRange(-96, 0);
    if (model()) {
        m_micGainMinSpin->setValue(model()->transmitModel().micGainMinDb());
    }
    grpLayout->addRow(QStringLiteral("Mic Gain Min (dB):"), m_micGainMinSpin);
    connect(m_micGainMinSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) {
                if (model()) {
                    model()->transmitModel().setMicGainMinDb(v);
                }
                if (m_micGainSlider) { m_micGainSlider->setMinimum(v); }
            });

    m_micGainMaxSpin = new QSpinBox(this);
    m_micGainMaxSpin->setObjectName(QStringLiteral("micGainMaxSpin"));
    m_micGainMaxSpin->setRange(1, 70);
    if (model()) {
        m_micGainMaxSpin->setValue(model()->transmitModel().micGainMaxDb());
    }
    grpLayout->addRow(QStringLiteral("Mic Gain Max (dB):"), m_micGainMaxSpin);
    connect(m_micGainMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) {
                if (model()) {
                    model()->transmitModel().setMicGainMaxDb(v);
                }
                if (m_micGainSlider) { m_micGainSlider->setMaximum(v); }
            });

    // Reverse: model -> spin (handles Setup-page reopen / external changes
    // such as MIC profile load).
    if (model()) {
        TransmitModel* tx = &model()->transmitModel();
        connect(tx, &TransmitModel::micGainMinDbChanged,
                m_micGainMinSpin, &QSpinBox::setValue);
        connect(tx, &TransmitModel::micGainMaxDbChanged,
                m_micGainMaxSpin, &QSpinBox::setValue);

        // Initial slider range to match the persisted bounds. This overrides
        // the default per-board range from BoardCapabilities so a user-edited
        // Min/Max persisted in AppSettings is honoured on re-open.
        if (m_micGainSlider) {
            m_micGainSlider->setMinimum(tx->micGainMinDb());
            m_micGainSlider->setMaximum(tx->micGainMaxDb());
        }
    }

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

/*static*/ bool AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW hw)
{
    // From Thetis setup.cs:19830-20405 [v2.10.3.13+501e3f51], the
    // pnlGeneralHardwareORION.Enabled bit is true only for the
    // following HPSDRModel values (mapped to HPSDRHW via boardForModel):
    //   ANAN200D     -> Orion       (true)
    //   ANAN7000D    -> OrionMKII   (true)
    //   ANAN8000D    -> OrionMKII   (true)
    //   ANVELINAPRO3 -> OrionMKII   (true)
    //   ANAN_G2      -> Saturn      (true)  // added G8NJJ
    //   ANAN_G2_1K   -> Saturn      (true)  // added G8NJJ
    //   REDPITAYA    -> OrionMKII   (false) //DH1KLM [original tag from
    //                                        setup.cs:20355 — REDPITAYA case
    //                                        sets pnlGeneralHardwareORION
    //                                        .Enabled=false at :20360]
    //
    // SaturnMKII is the NereusSDR-original Saturn board-revision slot;
    // grouped with Saturn for mic-jack purposes per spec §4.5.
    //
    // All other boards (Hermes / HermesII / Angelia / Atlas / HermesLite /
    // HermesLiteRxOnly / Andromeda / Unknown) fall through to the
    // default-false branch.
    switch (hw) {
        case HPSDRHW::Orion:
        case HPSDRHW::OrionMKII:
        case HPSDRHW::Saturn:
        case HPSDRHW::SaturnMKII:
            return true;
        case HPSDRHW::Atlas:
        case HPSDRHW::Hermes:
        case HPSDRHW::HermesII:
        case HPSDRHW::Angelia:
        case HPSDRHW::HermesLite:
        case HPSDRHW::HermesLiteRxOnly:
        case HPSDRHW::Andromeda:
        case HPSDRHW::Unknown:
            return false;
    }
    return false;
}

void AudioTxInputPage::updateRadioMicGroupVisibility(MicSource source, HPSDRHW hw)
{
    // Only show a Radio Mic group when Radio Mic is actually selected.
    const bool radioMicActive = (source == MicSource::Radio);

    // Hermes-class mic-jack panel: per Thetis, only the Hermes / HermesII
    // boards expose the radMicIn / radLineIn radio-button pair on
    // grpHardwareConfigHermes.  Atlas (HPSDR/Metis) and Angelia have a
    // different radio-mic UI not yet ported.
    const bool isHermes = (hw == HPSDRHW::Hermes
                        || hw == HPSDRHW::HermesII);

    // Orion-class mic-jack panel: tighter per-SKU gating that mirrors the
    // Thetis pnlGeneralHardwareORION.Enabled enable map.
    const bool isOrion  = boardShowsOrionMicJackPanel(hw);

    // Saturn G2 panel: gated separately per Thetis setup.cs:6181-6189
    // [v2.10.3.13+501e3f51] (panelSaturnMicInput.Enabled).
    const bool isSaturn = (hw == HPSDRHW::Saturn
                        || hw == HPSDRHW::SaturnMKII);

    if (m_hermesGroup) { m_hermesGroup->setVisible(radioMicActive && isHermes); }
    if (m_orionGroup)  { m_orionGroup->setVisible(radioMicActive && isOrion);  }
    if (m_saturnGroup) { m_saturnGroup->setVisible(radioMicActive && isSaturn); }
}

// ---------------------------------------------------------------------------
// lineInBoostLabel: format dB label for the Line In Gain slider (I.3)
// ---------------------------------------------------------------------------

/*static*/ QString AudioTxInputPage::lineInBoostLabel(int sliderValue)
{
    return QStringLiteral("%1 dB").arg(sliderValue);
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

    const int hostApiIndex = m_backendCombo->itemData(comboIndex).toInt();

    // Repopulate device combo for the new host API.
    populateDeviceCombo(hostApiIndex);

    // Persist to TransmitModel session state.
    if (model()) {
        model()->transmitModel().setPcMicHostApiIndex(hostApiIndex);
        // Device name resets to default when backend changes.
        model()->transmitModel().setPcMicDeviceName(QString());
    }
}

// ---------------------------------------------------------------------------
// Slot: Device combo changed (I.2 Row 2)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onDeviceChanged(int comboIndex)
{
    if (!m_deviceCombo) { return; }
    if (m_updatingFromModel) { return; }

    const QString deviceName = m_deviceCombo->itemData(comboIndex).toString();

    if (model()) {
        model()->transmitModel().setPcMicDeviceName(deviceName);
    }
}

// ---------------------------------------------------------------------------
// Slot: Buffer slider changed (I.2 Row 3)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onBufferSliderChanged(int sliderPos)
{
    if (sliderPos < 0 || sliderPos >= kBufferSizes.size()) { return; }

    const int samples = kBufferSizes[sliderPos];
    updateBufferLabel(samples);

    if (model()) {
        model()->transmitModel().setPcMicBufferSamples(samples);
    }
}

// ---------------------------------------------------------------------------
// Slot: Test Mic button toggled (I.2 Row 4)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onTestMicToggled(bool checked)
{
    if (checked) {
        // Start the 10 ms VU-poll timer.
        // TODO [3M-1b I.x]: if m_txInputBus is not open, trigger
        // AudioEngine to open the PC Mic capture stream with the current
        // host API / device / buffer settings so the level reflects the
        // actual hardware. For now, pcMicInputLevel() returns 0.0f when
        // the bus is idle — the VU bar will show silent until TX is active.
        m_vuTimer->start();
        if (m_testMicBtn) {
            m_testMicBtn->setText(QStringLiteral("Stop Test"));
        }
    } else {
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
    if (model() && model()->audioEngine()) {
        // Bus-tap approach: read peak amplitude from m_txInputBus without
        // consuming any samples. The PortAudioBus callback updates txLevel()
        // (std::atomic<float>) every 10–20 ms. When the TX-input bus is not
        // open (no active capture stream), pcMicInputLevel() returns 0.0f.
        level = model()->audioEngine()->pcMicInputLevel();
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
    m_hermesGroup = new QGroupBox(QStringLiteral("Radio Mic — Hermes / Atlas"), this);
    auto* grpLayout = new QVBoxLayout(m_hermesGroup);

    // ── Row 1: Mic In / Line In radio buttons ─────────────────────────────────
    auto* micInBtn  = new QRadioButton(QStringLiteral("Mic In"),  m_hermesGroup);
    auto* lineInBtn = new QRadioButton(QStringLiteral("Line In"), m_hermesGroup);
    micInBtn->setChecked(true);  // Hermes default: mic input active

    m_hermesMicInputGroup = new QButtonGroup(this);
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
    m_hermesMicBoostChk->setChecked(true);  // TransmitModel default: true
    grpLayout->addWidget(m_hermesMicBoostChk);

    connect(m_hermesMicBoostChk, &QCheckBox::toggled,
            this, &AudioTxInputPage::onHermesMicBoostToggled);

    // ── Row 3: Line In Gain slider ───────────────────────────────────────────
    // Range: kLineInBoostMin (-34.5 → int: -34) to kLineInBoostMax (12), 1 dB steps.
    // kLineInBoostMin is a double (-34.5) cast to int at slider construction
    // time via static_cast<int>; the slider integer minimum is therefore -34.
    m_hermesLineInGainSlider = new QSlider(Qt::Horizontal, m_hermesGroup);
    m_hermesLineInGainSlider->setMinimum(static_cast<int>(TransmitModel::kLineInBoostMin));
    m_hermesLineInGainSlider->setMaximum(static_cast<int>(TransmitModel::kLineInBoostMax));
    m_hermesLineInGainSlider->setSingleStep(1);
    m_hermesLineInGainSlider->setValue(0);  // TransmitModel default: 0.0 dB

    m_hermesLineInGainLabel = new QLabel(lineInBoostLabel(0), m_hermesGroup);
    m_hermesLineInGainLabel->setMinimumWidth(60);

    auto* gainRow = new QHBoxLayout();
    gainRow->addWidget(new QLabel(QStringLiteral("Line In Gain:"), m_hermesGroup));
    gainRow->addWidget(m_hermesLineInGainSlider, 1);
    gainRow->addWidget(m_hermesLineInGainLabel);
    grpLayout->addLayout(gainRow);

    connect(m_hermesLineInGainSlider, &QSlider::valueChanged,
            this, &AudioTxInputPage::onHermesLineInGainChanged);

    parentLayout->addWidget(m_hermesGroup);
}

// ---------------------------------------------------------------------------
// buildOrionRadioMicGroup — Orion / OrionMKII family
// ---------------------------------------------------------------------------

void AudioTxInputPage::buildOrionRadioMicGroup(QVBoxLayout* parentLayout)
{
    m_orionGroup = new QGroupBox(QStringLiteral("Radio Mic — Orion-MkII"), this);
    auto* grpLayout = new QVBoxLayout(m_orionGroup);

    m_orionMicTipRingChk = new QCheckBox(
        QStringLiteral("Mic Tip-Ring (Tip is Mic)"), m_orionGroup);
    m_orionMicTipRingChk->setChecked(true);  // TransmitModel default: true
    grpLayout->addWidget(m_orionMicTipRingChk);

    m_orionMicBiasChk = new QCheckBox(QStringLiteral("Mic Bias"), m_orionGroup);
    m_orionMicBiasChk->setChecked(false);  // TransmitModel default: false
    grpLayout->addWidget(m_orionMicBiasChk);

    m_orionMicPttDisabledChk = new QCheckBox(
        QStringLiteral("Mic PTT Disabled"), m_orionGroup);
    m_orionMicPttDisabledChk->setChecked(false);  // TransmitModel default: false
    grpLayout->addWidget(m_orionMicPttDisabledChk);

    m_orionMicBoostChk = new QCheckBox(
        QStringLiteral("+20 dB Mic Boost"), m_orionGroup);
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

    parentLayout->addWidget(m_orionGroup);
}

// ---------------------------------------------------------------------------
// buildSaturnRadioMicGroup — Saturn G2 family
// ---------------------------------------------------------------------------

void AudioTxInputPage::buildSaturnRadioMicGroup(QVBoxLayout* parentLayout)
{
    m_saturnGroup = new QGroupBox(QStringLiteral("Radio Mic — Saturn G2"), this);
    auto* grpLayout = new QVBoxLayout(m_saturnGroup);

    // ── Row 1: 3.5 mm Jack / XLR radio buttons ───────────────────────────────
    auto* jackBtn = new QRadioButton(QStringLiteral("3.5 mm Jack"), m_saturnGroup);
    auto* xlrBtn  = new QRadioButton(QStringLiteral("XLR"),         m_saturnGroup);
    // TransmitModel default: micXlr=true → XLR selected by default.
    xlrBtn->setChecked(true);

    m_saturnMicInputGroup = new QButtonGroup(this);
    m_saturnMicInputGroup->addButton(jackBtn, 0);  // id=0 → micXlr=false (3.5mm)
    m_saturnMicInputGroup->addButton(xlrBtn,  1);  // id=1 → micXlr=true  (XLR)

    auto* inputRow = new QHBoxLayout();
    inputRow->addWidget(jackBtn);
    inputRow->addWidget(xlrBtn);
    inputRow->addStretch();
    grpLayout->addLayout(inputRow);

    connect(m_saturnMicInputGroup, &QButtonGroup::idToggled,
            this, &AudioTxInputPage::onSaturnMicInputToggled);

    // ── Rows 2-4: three checkboxes ───────────────────────────────────────────
    m_saturnMicPttDisabledChk = new QCheckBox(
        QStringLiteral("Mic PTT Disabled"), m_saturnGroup);
    m_saturnMicPttDisabledChk->setChecked(false);  // TransmitModel default
    grpLayout->addWidget(m_saturnMicPttDisabledChk);

    m_saturnMicBiasChk = new QCheckBox(QStringLiteral("Mic Bias"), m_saturnGroup);
    m_saturnMicBiasChk->setChecked(false);  // TransmitModel default
    grpLayout->addWidget(m_saturnMicBiasChk);

    m_saturnMicBoostChk = new QCheckBox(
        QStringLiteral("+20 dB Mic Boost"), m_saturnGroup);
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
    if (m_hermesLineInGainLabel) {
        m_hermesLineInGainLabel->setText(lineInBoostLabel(sliderValue));
    }
    if (!model()) { return; }
    model()->transmitModel().setLineInBoost(static_cast<double>(sliderValue));
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
    if (!m_hermesLineInGainSlider) { return; }
    m_updatingFromModel = true;
    {
        QSignalBlocker blk(m_hermesLineInGainSlider);
        m_hermesLineInGainSlider->setValue(static_cast<int>(dB));
    }
    if (m_hermesLineInGainLabel) {
        m_hermesLineInGainLabel->setText(lineInBoostLabel(static_cast<int>(dB)));
    }
    m_updatingFromModel = false;
}

void AudioTxInputPage::onModelMicTipRingChanged(bool on)
{
    if (!m_orionMicTipRingChk) { return; }
    m_updatingFromModel = true;
    {
        QSignalBlocker blk(m_orionMicTipRingChk);
        m_orionMicTipRingChk->setChecked(on);
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
