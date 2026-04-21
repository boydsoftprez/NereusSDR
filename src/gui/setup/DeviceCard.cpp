// =================================================================
// src/gui/setup/DeviceCard.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Devices card widget.
// See DeviceCard.h for the full header.
//
// Sub-Phase 12 Task 12.2 (2026-04-20): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

#include "DeviceCard.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/audio/PortAudioBus.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

// Shared style constants — match SetupPage / STYLEGUIDE.md palette.
static const char* kGroupStyle =
    "QGroupBox {"
    "  border: 1px solid #203040;"
    "  border-radius: 4px;"
    "  margin-top: 8px;"
    "  padding-top: 12px;"
    "  font-weight: bold;"
    "  color: #8aa8c0;"
    "}"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";

static const char* kComboStyle =
    "QComboBox {"
    "  background: #152535;"
    "  border: 1px solid #203040;"
    "  border-radius: 3px;"
    "  color: #c8d8e8;"
    "  padding: 2px 6px;"
    "}"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #152535; color: #c8d8e8; "
    "  selection-background-color: #00b4d8; }";

static const char* kCheckStyle =
    "QCheckBox { color: #c8d8e8; spacing: 4px; }"
    "QCheckBox::indicator { width: 12px; height: 12px; border: 1px solid #203040;"
    "  border-radius: 2px; background: #0f0f1a; }"
    "QCheckBox::indicator:checked { background: #00b4d8; }";

static const char* kLabelStyle = "QLabel { color: #c8d8e8; font-size: 12px; }";

static const char* kDimLabelStyle = "QLabel { color: #607080; font-size: 11px; }";

// Pill style for the negotiated-format readout.
static const char* kPillStyleOk =
    "QLabel {"
    "  background: #152530;"
    "  border: 1px solid #203040;"
    "  border-radius: 8px;"
    "  color: #80c8a0;"
    "  font-size: 10px;"
    "  padding: 2px 8px;"
    "}";

static const char* kPillStyleError =
    "QLabel {"
    "  background: #301520;"
    "  border: 1px solid #603040;"
    "  border-radius: 8px;"
    "  color: #e05060;"
    "  font-size: 10px;"
    "  padding: 2px 8px;"
    "}";

static const char* kPillStyleApplying =
    "QLabel {"
    "  background: #203040;"
    "  border: 1px solid #304050;"
    "  border-radius: 8px;"
    "  color: #c8d8e8;"
    "  font-size: 10px;"
    "  padding: 2px 8px;"
    "}";

// Sample rates offered in the combo. Include 384k for future-proofing;
// hardware that can't support it will fail on open and show the red pill.
static const QStringList kSampleRates = {
    QStringLiteral("44100"),
    QStringLiteral("48000"),
    QStringLiteral("88200"),
    QStringLiteral("96000"),
    QStringLiteral("176400"),
    QStringLiteral("192000"),
    QStringLiteral("384000"),
};

static const QStringList kBitDepths = {
    QStringLiteral("16"),
    QStringLiteral("24"),
    QStringLiteral("32"),
};

static const QStringList kChannels = {
    QStringLiteral("1"),
    QStringLiteral("2"),
};

// Buffer sizes in samples. Derived ms shown next to the combo.
static const QList<int> kBufferSizes = { 64, 128, 256, 512, 1024, 2048 };

// Compute derived milliseconds label from samples + sample rate.
static QString bufferMs(int samples, int sampleRate)
{
    if (sampleRate <= 0) {
        return QStringLiteral("? ms");
    }
    const double ms = static_cast<double>(samples) / static_cast<double>(sampleRate) * 1000.0;
    return QStringLiteral("%1 ms").arg(ms, 0, 'f', 1);
}

} // namespace

// ---------------------------------------------------------------------------
// DeviceCard construction
// ---------------------------------------------------------------------------

DeviceCard::DeviceCard(const QString& prefix,
                       Role role,
                       bool enableCheckbox,
                       QWidget* parent)
    : QGroupBox(parent)
    , m_prefix(prefix)
    , m_role(role)
{
    setStyleSheet(QLatin1String(kGroupStyle));
    buildLayout();

    // Enable checkbox (Headphones + VAX channels).  Inserted as the first
    // visible row of the card rather than using QGroupBox::setCheckable(true),
    // which clips the group title on platforms whose native checkable-title
    // indicator eats part of the title padding.  The checkbox persists via
    // audio/<prefix>/Enabled (separate from the 10-field AudioDeviceConfig
    // round-trip, since "enabled" is a bus-open decision, not a device param).
    if (enableCheckbox) {
        m_enableChk = new QCheckBox(QStringLiteral("Enabled"), this);
        m_enableChk->setStyleSheet(QLatin1String(kCheckStyle));
        auto* outer = qobject_cast<QVBoxLayout*>(layout());
        if (outer) {
            auto* headerRow = new QHBoxLayout;
            headerRow->setSpacing(4);
            headerRow->addWidget(m_enableChk);
            headerRow->addStretch(1);
            outer->insertLayout(0, headerRow);
        }
        connect(m_enableChk, &QCheckBox::toggled, this, [this](bool on) {
            if (m_suppressSignals) {
                return;
            }
            AppSettings::instance().setValue(
                m_prefix + QStringLiteral("/Enabled"),
                on ? QStringLiteral("True") : QStringLiteral("False"));
            AppSettings::instance().save();
            emit enabledChanged(on);
        });
    }

    loadFromSettings();
}

// ---------------------------------------------------------------------------
// buildLayout — construct the 7-row form + pill
// ---------------------------------------------------------------------------
void DeviceCard::buildLayout()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 12, 8, 8);
    outer->setSpacing(4);

    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(4);

    auto makeLabel = [](const QString& text) -> QLabel* {
        auto* l = new QLabel(text);
        l->setStyleSheet(QLatin1String(kLabelStyle));
        return l;
    };

    // ── Row 1: Driver API ────────────────────────────────────────────────
    m_driverApiCombo = new QComboBox;
    m_driverApiCombo->setStyleSheet(QLatin1String(kComboStyle));
    // Populate from PortAudio host APIs (requires Pa_Initialize done).
    const auto apis = PortAudioBus::hostApis();
    m_driverApiCombo->addItem(QStringLiteral("(PortAudio default)"),
                              QVariant::fromValue(-1));
    for (const auto& api : apis) {
        m_driverApiCombo->addItem(api.name, QVariant::fromValue(api.index));
    }
    form->addRow(makeLabel(QStringLiteral("Driver API:")), m_driverApiCombo);

    // ── Row 2: Device ────────────────────────────────────────────────────
    m_deviceCombo = new QComboBox;
    m_deviceCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_deviceCombo->setMinimumWidth(200);
    populateDeviceCombo();
    form->addRow(makeLabel(QStringLiteral("Device:")), m_deviceCombo);

    // ── Row 3: Sample rate + Auto-match checkbox ─────────────────────────
    {
        auto* srRow = new QHBoxLayout;
        srRow->setSpacing(6);
        m_sampleRateCombo = new QComboBox;
        m_sampleRateCombo->setStyleSheet(QLatin1String(kComboStyle));
        for (const QString& r : kSampleRates) {
            m_sampleRateCombo->addItem(r + QStringLiteral(" Hz"), r.toInt());
        }
        m_autoMatchSampleRate = new QCheckBox(QStringLiteral("Auto-match"));
        m_autoMatchSampleRate->setStyleSheet(QLatin1String(kCheckStyle));
        m_autoMatchSampleRate->setToolTip(QStringLiteral(
            "Use the sample rate the device reports as its default"));
        srRow->addWidget(m_sampleRateCombo);
        srRow->addWidget(m_autoMatchSampleRate);
        srRow->addStretch();
        form->addRow(makeLabel(QStringLiteral("Sample rate:")), srRow);
    }

    // ── Row 4: Bit depth ─────────────────────────────────────────────────
    m_bitDepthCombo = new QComboBox;
    m_bitDepthCombo->setStyleSheet(QLatin1String(kComboStyle));
    for (const QString& d : kBitDepths) {
        m_bitDepthCombo->addItem(d + QStringLiteral(" bit"), d.toInt());
    }
    form->addRow(makeLabel(QStringLiteral("Bit depth:")), m_bitDepthCombo);

    // ── Row 5: Channels ──────────────────────────────────────────────────
    m_channelsCombo = new QComboBox;
    m_channelsCombo->setStyleSheet(QLatin1String(kComboStyle));
    for (const QString& c : kChannels) {
        m_channelsCombo->addItem(c == QStringLiteral("1")
                                     ? QStringLiteral("1 (Mono)")
                                     : QStringLiteral("2 (Stereo)"),
                                 c.toInt());
    }
    form->addRow(makeLabel(QStringLiteral("Channels:")), m_channelsCombo);

    // ── Row 6: Buffer size + derived-ms readout ──────────────────────────
    {
        auto* bufRow = new QHBoxLayout;
        bufRow->setSpacing(6);
        m_bufferSizeCombo = new QComboBox;
        m_bufferSizeCombo->setStyleSheet(QLatin1String(kComboStyle));
        for (int sz : kBufferSizes) {
            m_bufferSizeCombo->addItem(QStringLiteral("%1 samples").arg(sz),
                                       QVariant::fromValue(sz));
        }
        m_bufferMsLabel = new QLabel;
        m_bufferMsLabel->setStyleSheet(QLatin1String(kDimLabelStyle));
        m_bufferMsLabel->setMinimumWidth(50);
        bufRow->addWidget(m_bufferSizeCombo);
        bufRow->addWidget(m_bufferMsLabel);
        bufRow->addStretch();
        form->addRow(makeLabel(QStringLiteral("Buffer size:")), bufRow);
    }

    // ── Row 7: Options (WASAPI) ──────────────────────────────────────────
    {
        auto* optRow = new QHBoxLayout;
        optRow->setSpacing(10);
        m_exclusiveChk   = new QCheckBox(QStringLiteral("Exclusive"));
        m_eventDrivenChk = new QCheckBox(QStringLiteral("Event-driven"));
        m_bypassMixerChk = new QCheckBox(QStringLiteral("Bypass mixer"));
        for (QCheckBox* chk : { m_exclusiveChk, m_eventDrivenChk, m_bypassMixerChk }) {
            chk->setStyleSheet(QLatin1String(kCheckStyle));
            chk->setToolTip(QStringLiteral("WASAPI only"));
            optRow->addWidget(chk);
        }
        optRow->addStretch();
        form->addRow(makeLabel(QStringLiteral("Options:")), optRow);
    }

    // ── TX-input extras (Input role only) ────────────────────────────────
    if (m_role == Role::Input) {
        m_monitorDuringTxChk = new QCheckBox(
            QStringLiteral("Monitor TX input during transmit"));
        m_monitorDuringTxChk->setStyleSheet(QLatin1String(kCheckStyle));
        m_monitorDuringTxChk->setToolTip(QStringLiteral(
            "Route microphone input through the monitor bus while transmitting"));

        m_toneCheckChk = new QCheckBox(
            QStringLiteral("Enable tone check (A-440 Hz burst on PTT)"));
        m_toneCheckChk->setStyleSheet(QLatin1String(kCheckStyle));
        m_toneCheckChk->setToolTip(QStringLiteral(
            "Inject a 440 Hz test tone to verify TX input routing on first PTT"));

        form->addRow(makeLabel(QString()), m_monitorDuringTxChk);
        form->addRow(makeLabel(QString()), m_toneCheckChk);
    }

    outer->addLayout(form);

    // ── Negotiated-format pill ───────────────────────────────────────────
    {
        auto* pillRow = new QHBoxLayout;
        pillRow->setSpacing(4);
        auto* pillLbl = new QLabel(QStringLiteral("Negotiated:"));
        pillLbl->setStyleSheet(QLatin1String(kDimLabelStyle));
        m_negotiatedPill = new QLabel(QStringLiteral("(not yet applied)"));
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleApplying));
        pillRow->addWidget(pillLbl);
        pillRow->addWidget(m_negotiatedPill);
        pillRow->addStretch();
        outer->addLayout(pillRow);
    }

    setLayout(outer);

    // ── Wire all controls to the commit slot ────────────────────────────
    // Use event-filter on QComboBox / QCheckBox inside the card so wheel
    // events inside a scroll area don't leak into control-changes.
    auto connectCombo = [this](QComboBox* combo) {
        if (!combo) { return; }
        // Block wheel events on combos inside scroll areas.
        combo->installEventFilter(this);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DeviceCard::onAnyControlChanged);
    };
    connectCombo(m_driverApiCombo);
    connectCombo(m_sampleRateCombo);
    connectCombo(m_bitDepthCombo);
    connectCombo(m_channelsCombo);
    // Buffer-size uses a 200 ms intra-control debounce (addendum §2.1).
    // Other combos fire immediately.
    if (m_bufferSizeCombo) {
        m_bufferSizeCombo->installEventFilter(this);
        m_bufferSizeDebounceTimer = new QTimer(this);
        m_bufferSizeDebounceTimer->setSingleShot(true);
        m_bufferSizeDebounceTimer->setInterval(200);
        connect(m_bufferSizeDebounceTimer, &QTimer::timeout,
                this, &DeviceCard::onAnyControlChanged);
        connect(m_bufferSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { m_bufferSizeDebounceTimer->start(); });
    }

    // Device combo fires populateDeviceCombo on driver-API change, then
    // also commits via the device-combo's own currentIndexChanged.
    connect(m_driverApiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                populateDeviceCombo();
                // onAnyControlChanged is called via the device combo's
                // own signal after population — no double-commit here.
            });
    connectCombo(m_deviceCombo);

    auto connectCheck = [this](QCheckBox* chk) {
        if (!chk) { return; }
        connect(chk, &QCheckBox::toggled,
                this, &DeviceCard::onAnyControlChanged);
    };
    connectCheck(m_autoMatchSampleRate);
    connectCheck(m_exclusiveChk);
    connectCheck(m_eventDrivenChk);
    connectCheck(m_bypassMixerChk);
    if (m_monitorDuringTxChk) { connectCheck(m_monitorDuringTxChk); }
    if (m_toneCheckChk)       { connectCheck(m_toneCheckChk); }

    // Buffer-size or sample-rate change → update derived-ms label.
    connect(m_bufferSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBufferMsLabel(); });
    connect(m_sampleRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBufferMsLabel(); });
}

// ---------------------------------------------------------------------------
// populateDeviceCombo
// ---------------------------------------------------------------------------
void DeviceCard::populateDeviceCombo()
{
    QSignalBlocker blocker(m_deviceCombo);
    const QString prevName = m_deviceCombo->currentData().toString();

    m_deviceCombo->clear();
    m_deviceCombo->addItem(QStringLiteral("(platform default)"), QString());

    const int apiIdx = m_driverApiCombo
        ? m_driverApiCombo->currentData().toInt()
        : -1;

    QVector<PortAudioBus::DeviceInfo> devices;
    if (apiIdx < 0) {
        // All APIs.
        const auto apis = PortAudioBus::hostApis();
        for (const auto& api : apis) {
            if (m_role == Role::Output) {
                const auto devs = PortAudioBus::outputDevicesFor(api.index);
                devices += devs;
            } else {
                const auto devs = PortAudioBus::inputDevicesFor(api.index);
                devices += devs;
            }
        }
    } else {
        if (m_role == Role::Output) {
            devices = PortAudioBus::outputDevicesFor(apiIdx);
        } else {
            devices = PortAudioBus::inputDevicesFor(apiIdx);
        }
    }

    int restoreIdx = 0;
    for (int i = 0; i < devices.size(); ++i) {
        m_deviceCombo->addItem(devices[i].name,
                               QVariant::fromValue(devices[i].name));
        if (devices[i].name == prevName) {
            restoreIdx = i + 1;  // +1 for the "(platform default)" entry
        }
    }
    m_deviceCombo->setCurrentIndex(restoreIdx);
}

// ---------------------------------------------------------------------------
// updateBufferMsLabel — recompute the derived milliseconds readout
// ---------------------------------------------------------------------------
void DeviceCard::updateBufferMsLabel()
{
    if (!m_bufferMsLabel) {
        return;
    }
    const int sz = m_bufferSizeCombo ? m_bufferSizeCombo->currentData().toInt() : 256;
    const int sr = m_sampleRateCombo ? m_sampleRateCombo->currentData().toInt() : 48000;
    m_bufferMsLabel->setText(bufferMs(sz, sr));
}

// ---------------------------------------------------------------------------
// currentConfig
//
// Note: autoMatchSampleRate, monitorDuringTx, and toneCheck are session-only
// UI helpers by design — not part of AudioDeviceConfig's 10 persisted fields
// and not round-tripped through loadFromSettings/saveToSettings.  If future
// requirements change, extend AudioDeviceConfig.
// ---------------------------------------------------------------------------
AudioDeviceConfig DeviceCard::currentConfig() const
{
    AudioDeviceConfig cfg;

    // deviceName: empty string from "(platform default)" entry maps to
    // AudioDeviceConfig empty deviceName → makeBus treats as platform default.
    cfg.deviceName = m_deviceCombo->currentData().toString();

    cfg.driverApi = (m_driverApiCombo && m_driverApiCombo->currentIndex() > 0)
        ? m_driverApiCombo->currentText()
        : QString();

    cfg.hostApiIndex = m_driverApiCombo
        ? m_driverApiCombo->currentData().toInt()
        : -1;

    // Auto-match is a UI preference (session-only) meant to signal "use the
    // device's own default sample rate".  That resolution isn't wired yet —
    // the engine does NOT treat sampleRate=0 as "device default" and would
    // pass it straight to makeBus → stream-open fails at 0 Hz.  Until the
    // PortAudio device-enumeration is threaded through this card, always
    // emit the combo's current rate so the bus always opens cleanly; the
    // auto-match checkbox state remains session-only UI.
    // TODO(sub-phase-12-automatch-resolve): when device-default-rate
    // lookup lands, set cfg.sampleRate to the device default here when
    // auto-match is checked.
    cfg.sampleRate = m_sampleRateCombo
        ? m_sampleRateCombo->currentData().toInt()
        : 48000;

    cfg.bitDepth      = m_bitDepthCombo  ? m_bitDepthCombo->currentData().toInt()  : 32;
    cfg.channels      = m_channelsCombo  ? m_channelsCombo->currentData().toInt()  : 2;
    cfg.bufferSamples = m_bufferSizeCombo? m_bufferSizeCombo->currentData().toInt(): 256;

    cfg.exclusiveMode = m_exclusiveChk   && m_exclusiveChk->isChecked();
    cfg.eventDriven   = m_eventDrivenChk && m_eventDrivenChk->isChecked();
    cfg.bypassMixer   = m_bypassMixerChk && m_bypassMixerChk->isChecked();

    cfg.manualLatencyMs = 0;  // Not exposed in this card; reserved for Advanced page.

    return cfg;
}

// ---------------------------------------------------------------------------
// updateNegotiatedPill
// ---------------------------------------------------------------------------
void DeviceCard::updateNegotiatedPill(const AudioDeviceConfig& negotiated,
                                      const QString& errorString)
{
    if (!m_negotiatedPill) {
        return;
    }

    if (!errorString.isEmpty()) {
        // Red pill — driver rejected the config.
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleError));
        m_negotiatedPill->setText(QStringLiteral("Error: ") + errorString);
        return;
    }

    // Green pill — show negotiated format.
    m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleOk));
    const QString name = negotiated.deviceName.isEmpty()
        ? QStringLiteral("(default)")
        : negotiated.deviceName;
    const QString sr = negotiated.sampleRate == 0
        ? QStringLiteral("auto")
        : QStringLiteral("%1 Hz").arg(negotiated.sampleRate);
    m_negotiatedPill->setText(
        QStringLiteral("%1 · %2 · %3 ch · %4 samples")
            .arg(name)
            .arg(sr)
            .arg(negotiated.channels)
            .arg(negotiated.bufferSamples));
}

// ---------------------------------------------------------------------------
// loadFromSettings — seed controls from persisted state
// ---------------------------------------------------------------------------
void DeviceCard::loadFromSettings()
{
    const AudioDeviceConfig cfg =
        AudioDeviceConfig::loadFromSettings(m_prefix);

    m_suppressSignals = true;

    // Device name.
    {
        const int idx = m_deviceCombo->findData(
            QVariant::fromValue(cfg.deviceName));
        m_deviceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    // Driver API — look up by display text (api.name is the item text, set in
    // buildLayout).  Empty driverApi falls through to index 0 ("(PortAudio
    // default)").  findText returns -1 on no match; guard keeps found == 0.
    if (m_driverApiCombo) {
        int found = 0;
        if (!cfg.driverApi.isEmpty()) {
            const int byName = m_driverApiCombo->findText(cfg.driverApi);
            if (byName >= 0) {
                found = byName;
            }
        }
        m_driverApiCombo->setCurrentIndex(found);
    }

    // Sample rate.
    if (m_sampleRateCombo) {
        const int idx = m_sampleRateCombo->findData(
            QVariant::fromValue(cfg.sampleRate));
        m_sampleRateCombo->setCurrentIndex(idx >= 0 ? idx : 1); // default 48000
    }

    // Bit depth.
    if (m_bitDepthCombo) {
        const int idx = m_bitDepthCombo->findData(
            QVariant::fromValue(cfg.bitDepth));
        m_bitDepthCombo->setCurrentIndex(idx >= 0 ? idx : 2); // default 32-bit
    }

    // Channels.
    if (m_channelsCombo) {
        const int idx = m_channelsCombo->findData(
            QVariant::fromValue(cfg.channels));
        m_channelsCombo->setCurrentIndex(idx >= 0 ? idx : 1); // default stereo
    }

    // Buffer size.
    if (m_bufferSizeCombo) {
        const int idx = m_bufferSizeCombo->findData(
            QVariant::fromValue(cfg.bufferSamples));
        m_bufferSizeCombo->setCurrentIndex(idx >= 0 ? idx : 2); // default 256
        // Update the derived-ms label.
        if (m_bufferMsLabel) {
            const int sr = m_sampleRateCombo
                ? m_sampleRateCombo->currentData().toInt()
                : 48000;
            m_bufferMsLabel->setText(bufferMs(cfg.bufferSamples, sr));
        }
    }

    // WASAPI options.
    if (m_exclusiveChk)   { m_exclusiveChk->setChecked(cfg.exclusiveMode); }
    if (m_eventDrivenChk) { m_eventDrivenChk->setChecked(cfg.eventDriven); }
    if (m_bypassMixerChk) { m_bypassMixerChk->setChecked(cfg.bypassMixer); }

    // Enable checkbox (Headphones + VAX channels) — restored from
    // audio/<prefix>/Enabled.  Default is unchecked on fresh install so the
    // bus only opens after explicit user consent.
    if (m_enableChk) {
        const bool on = AppSettings::instance()
                            .value(m_prefix + QStringLiteral("/Enabled"),
                                   QStringLiteral("False"))
                            .toString() == QStringLiteral("True");
        m_enableChk->setChecked(on);
    }

    m_suppressSignals = false;
}

// ---------------------------------------------------------------------------
// onAnyControlChanged — commit on every control edit
// ---------------------------------------------------------------------------
void DeviceCard::onAnyControlChanged()
{
    if (m_suppressSignals) {
        return;
    }

    const AudioDeviceConfig cfg = currentConfig();

    // Persist to AppSettings immediately.
    cfg.saveToSettings(m_prefix);
    AppSettings::instance().save();

    // Show "APPLYING" pill while the engine is rebuilding the bus.
    if (m_negotiatedPill) {
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleApplying));
        m_negotiatedPill->setText(QStringLiteral("APPLYING…"));
    }

    emit configChanged(cfg);
}

// ---------------------------------------------------------------------------
// eventFilter — block wheel events on combo boxes in scroll areas
// per CLAUDE.md: "Event-filter QSlider/QComboBox in scroll areas to block
// wheel leak."
// ---------------------------------------------------------------------------
bool DeviceCard::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* combo = qobject_cast<QComboBox*>(obj);
        if (combo && !combo->hasFocus()) {
            event->ignore();
            return true;
        }
    }
    return QGroupBox::eventFilter(obj, event);
}

} // namespace NereusSDR
