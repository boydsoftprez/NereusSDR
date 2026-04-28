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

#include <QButtonGroup>
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

    buildPage(hasMicJack);

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

        // Seed mic gain slider.
        if (m_micGainSlider) {
            QSignalBlocker blk(m_micGainSlider);
            m_micGainSlider->setValue(tx->micGainDb());
            if (m_micGainLabel) {
                m_micGainLabel->setText(
                    QStringLiteral("%1 dB").arg(tx->micGainDb()));
            }
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

void AudioTxInputPage::buildPage(bool hasMicJack)
{
    // ── Mic Source group box (I.1) ────────────────────────────────────────────
    auto* srcGrp = new QGroupBox(QStringLiteral("Mic Source"), this);
    auto* srcLayout = new QVBoxLayout(srcGrp);

    m_pcMicBtn    = new QRadioButton(QStringLiteral("PC Mic"), srcGrp);
    m_radioMicBtn = new QRadioButton(QStringLiteral("Radio Mic"), srcGrp);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->addButton(m_pcMicBtn,    static_cast<int>(MicSource::Pc));
    m_buttonGroup->addButton(m_radioMicBtn, static_cast<int>(MicSource::Radio));

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

    contentLayout()->insertWidget(0, srcGrp);

    // UI → Model: user toggles a radio button.
    connect(m_buttonGroup, &QButtonGroup::idToggled,
            this, &AudioTxInputPage::onMicSourceButtonToggled);

    // ── PC Mic group box (I.2) ────────────────────────────────────────────────
    auto* pcMicGroupContainer = new QVBoxLayout();
    buildPcMicGroup(pcMicGroupContainer);
    contentLayout()->addLayout(pcMicGroupContainer);

    // Show PC Mic group only when PC Mic is selected.
    updatePcMicGroupVisibility(MicSource::Pc);
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
    m_micGainSlider = new QSlider(Qt::Horizontal, this);
    m_micGainSlider->setMinimum(TransmitModel::kMicGainDbMin);
    m_micGainSlider->setMaximum(TransmitModel::kMicGainDbMax);
    m_micGainSlider->setSingleStep(1);
    m_micGainSlider->setValue(-6);  // TransmitModel default

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
}

// ---------------------------------------------------------------------------
// Slot: Model → UI, mic source changed (I.1)
// ---------------------------------------------------------------------------

void AudioTxInputPage::onModelMicSourceChanged(MicSource source)
{
    syncButtonsFromModel(source);
    updatePcMicGroupVisibility(source);
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

} // namespace NereusSDR
