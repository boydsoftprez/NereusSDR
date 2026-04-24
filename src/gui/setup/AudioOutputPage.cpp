// =================================================================
// src/gui/setup/AudioOutputPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Output page.
// See AudioOutputPage.h for the full header.
//
// Phase 3O Task 20 (2026-04-24): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

// SPDX-License-Identifier: GPL-2.0-or-later
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

#include "AudioOutputPage.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Style constants — dark palette matching AudioVaxPage / STYLEGUIDE.md
// ---------------------------------------------------------------------------
namespace {

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

static const char* kInfoLabelStyle =
    "QLabel {"
    "  color: #506070;"
    "  font-size: 11px;"
    "  font-family: monospace;"
    "}";

static const char* kTelemetryValueStyle =
    "QLabel {"
    "  color: #00b4d8;"
    "  font-size: 11px;"
    "  font-family: monospace;"
    "}";

static const char* kQuantumBtnStyle =
    "QPushButton {"
    "  background: #152535;"
    "  border: 1px solid #203040;"
    "  border-radius: 3px;"
    "  color: #8aa8c0;"
    "  font-size: 11px;"
    "  padding: 3px 8px;"
    "  min-width: 44px;"
    "}"
    "QPushButton:hover   { background: #1d3045; }"
    "QPushButton:checked { background: #0f3050; border-color: #00b4d8; color: #00b4d8; }";

static const char* kRtWarningStyle =
    "QLabel {"
    "  background: #2a1a00;"
    "  border: 1px solid #b87300;"
    "  border-radius: 3px;"
    "  color: #e8a030;"
    "  font-size: 10px;"
    "  padding: 2px 6px;"
    "}";

static const char* kSectionLabelStyle =
    "QLabel {"
    "  color: #8aa8c0;"
    "  font-weight: bold;"
    "  font-size: 12px;"
    "}";

// ---------------------------------------------------------------------------
// sinkChoices — placeholder PipeWire sink list for Task 20.
// Live node enumeration is deferred to a later task.
// ---------------------------------------------------------------------------
static QStringList sinkChoices()
{
    return QStringList{
        QStringLiteral("follow default"),
        QStringLiteral("alsa_output.example"),
    };
}

} // namespace

// ============================================================================
// OutputCard
// ============================================================================

OutputCard::OutputCard(const QString& title,
                       Role            role,
                       const QString&  settingsPrefix,
                       QWidget*        parent)
    : QGroupBox(title, parent)
    , m_role(role)
    , m_settingsPrefix(settingsPrefix)
{
    buildLayout();
    setStyleSheet(QLatin1String(kGroupStyle));
    loadFromSettings();
}

void OutputCard::buildLayout()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setSpacing(6);
    root->setContentsMargins(10, 14, 10, 10);

    // ── Target sink ──────────────────────────────────────────────────────────
    {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* lbl = new QLabel(tr("Target sink:"), this);
        lbl->setFixedWidth(110);
        lbl->setStyleSheet(QLatin1String(kInfoLabelStyle));

        m_sinkCombo = new QComboBox(this);
        m_sinkCombo->setToolTip(tr(
            "PipeWire sink node to stream audio to.\n"
            "\"follow default\" tracks the system default output."));
        for (const QString& s : sinkChoices()) {
            m_sinkCombo->addItem(s);
        }

        row->addWidget(lbl);
        row->addWidget(m_sinkCombo, 1);
        root->addLayout(row);

        connect(m_sinkCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &OutputCard::onSinkComboChanged);
    }

    // ── Node info (read-only) ─────────────────────────────────────────────
    {
        QGroupBox* infoBox = new QGroupBox(tr("Node info"), this);
        QFormLayout* form = new QFormLayout(infoBox);
        infoBox->setStyleSheet(QLatin1String(kGroupStyle));
        form->setSpacing(3);
        form->setContentsMargins(8, 12, 8, 8);

        auto makeInfoRow = [&](const QString& key, QLabel*& valueLabel) {
            QLabel* keyLbl = new QLabel(key, infoBox);
            keyLbl->setStyleSheet(QLatin1String(kInfoLabelStyle));
            valueLabel = new QLabel(QStringLiteral("—"), infoBox);
            valueLabel->setStyleSheet(QLatin1String(kInfoLabelStyle));
            form->addRow(keyLbl, valueLabel);
        };

        makeInfoRow(QStringLiteral("node.name"),    m_nodeNameLabel);
        makeInfoRow(QStringLiteral("node.id"),      m_nodeIdLabel);
        makeInfoRow(QStringLiteral("media.role"),   m_mediaRoleLabel);
        makeInfoRow(tr("rate (Hz)"),                m_rateLabel);
        makeInfoRow(tr("format"),                   m_formatLabel);

        root->addWidget(infoBox);
    }

    // ── Quantum preset buttons ─────────────────────────────────────────────
    {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* lbl = new QLabel(tr("quantum:"), this);
        lbl->setFixedWidth(110);
        lbl->setToolTip(tr(
            "PipeWire process quantum (frames per callback).\n"
            "Lower values reduce latency but increase CPU load.\n"
            "128 requires real-time scheduling (RT)."));
        lbl->setStyleSheet(QLatin1String(kInfoLabelStyle));
        row->addWidget(lbl);

        auto makeBtn = [&](const QString& text, int q) -> QPushButton* {
            QPushButton* btn = new QPushButton(text, this);
            btn->setCheckable(true);
            btn->setStyleSheet(QLatin1String(kQuantumBtnStyle));
            connect(btn, &QPushButton::clicked, this, [this, q]() {
                onQuantumPreset(q);
            });
            row->addWidget(btn);
            return btn;
        };

        m_q128Btn  = makeBtn(QStringLiteral("128"),  128);
        m_q256Btn  = makeBtn(QStringLiteral("256"),  256);
        m_q512Btn  = makeBtn(QStringLiteral("512"),  512);
        m_q1024Btn = makeBtn(QStringLiteral("1024"), 1024);

        m_qCustomBtn = new QPushButton(tr("custom…"), this);
        m_qCustomBtn->setCheckable(true);
        m_qCustomBtn->setStyleSheet(QLatin1String(kQuantumBtnStyle));
        connect(m_qCustomBtn, &QPushButton::clicked, this, &OutputCard::onCustomQuantum);
        row->addWidget(m_qCustomBtn);

        row->addStretch();
        root->addLayout(row);

        // Sidetone @ quantum 128 RT warning (hidden initially)
        if (m_role == Role::Sidetone) {
            m_rtWarningLabel = new QLabel(
                QStringLiteral("⚙  quantum 128 requires RT scheduling — check /proc/sys/kernel/sched_rt_runtime_us"),
                this);
            m_rtWarningLabel->setStyleSheet(QLatin1String(kRtWarningStyle));
            m_rtWarningLabel->setWordWrap(true);
            m_rtWarningLabel->setVisible(false);
            root->addWidget(m_rtWarningLabel);
        }
    }

    // ── Telemetry block ───────────────────────────────────────────────────
    // TODO(later-task): wire telemetry signal from AudioEngine's actual buses.
    // AudioEngine owns the PipeWireBus instances; exposing them here requires
    // API additions not in Task 20's plan. Labels show placeholder text until
    // Task 22's SetupDialog wiring reveals the right surface.
    {
        QGroupBox* telemBox = new QGroupBox(tr("Live telemetry  (1 Hz)"), this);
        QFormLayout* form = new QFormLayout(telemBox);
        telemBox->setStyleSheet(QLatin1String(kGroupStyle));
        form->setSpacing(3);
        form->setContentsMargins(8, 12, 8, 8);

        auto makeRow = [&](const QString& key, QLabel*& valueLabel) {
            QLabel* keyLbl = new QLabel(key, telemBox);
            keyLbl->setStyleSheet(QLatin1String(kInfoLabelStyle));
            valueLabel = new QLabel(tr("(awaiting open)"), telemBox);
            valueLabel->setStyleSheet(QLatin1String(kTelemetryValueStyle));
            form->addRow(keyLbl, valueLabel);
        };

        makeRow(tr("measuredLatency (ms)"),        m_measuredLatencyLabel);
        makeRow(tr("our-ring / pw-quantum / dev"),  m_ringBreakdownLabel);
        makeRow(tr("xruns"),                        m_xrunLabel);
        makeRow(tr("process-cb CPU (pct / abs ms / quantum ms)"), m_cpuLabel);
        makeRow(tr("stream state"),                 m_stateLabel);

        root->addWidget(telemBox);
    }

    // ── Primary-only: volume slider + mute-on-TX ─────────────────────────
    if (m_role == Role::Primary) {
        {
            QHBoxLayout* row = new QHBoxLayout;
            QLabel* lbl = new QLabel(tr("Volume:"), this);
            lbl->setFixedWidth(110);
            lbl->setStyleSheet(QLatin1String(kInfoLabelStyle));

            m_volumeSlider = new QSlider(Qt::Horizontal, this);
            m_volumeSlider->setRange(0, 100);
            m_volumeSlider->setValue(100);
            m_volumeSlider->setToolTip(tr("Output stream volume (0–100)"));

            m_volumeValueLabel = new QLabel(QStringLiteral("100"), this);
            m_volumeValueLabel->setFixedWidth(30);
            m_volumeValueLabel->setStyleSheet(QLatin1String(kInfoLabelStyle));

            connect(m_volumeSlider, &QSlider::valueChanged,
                    this, &OutputCard::onVolumeChanged);

            row->addWidget(lbl);
            row->addWidget(m_volumeSlider, 1);
            row->addWidget(m_volumeValueLabel);
            root->addLayout(row);
        }

        {
            QHBoxLayout* row = new QHBoxLayout;
            QLabel* lbl = new QLabel(QStringLiteral(""), this);
            lbl->setFixedWidth(110);

            m_muteOnTxCheck = new QCheckBox(tr("Mute on TX"), this);
            m_muteOnTxCheck->setToolTip(tr(
                "Mute primary output while transmitting to prevent TX audio "
                "from leaking into the monitor path."));

            connect(m_muteOnTxCheck, &QCheckBox::toggled,
                    this, &OutputCard::onMuteOnTxChanged);

            row->addWidget(lbl);
            row->addWidget(m_muteOnTxCheck);
            row->addStretch();
            root->addLayout(row);
        }
    }
}

void OutputCard::loadFromSettings()
{
    auto& s = AppSettings::instance();

    // Sink node name
    const QString sink = s.value(m_settingsPrefix + QStringLiteral("/SinkNodeName"),
                                 QString()).toString();
    if (!sink.isEmpty()) {
        const int idx = m_sinkCombo->findText(sink);
        if (idx >= 0) {
            m_sinkCombo->setCurrentIndex(idx);
        } else {
            // Node not in placeholder list — add it so the saved value is shown.
            m_sinkCombo->addItem(sink);
            m_sinkCombo->setCurrentText(sink);
        }
    } else {
        // Default: "follow default" (index 0)
        m_sinkCombo->setCurrentIndex(0);
    }

    // Quantum
    const int defaultQ = (m_role == Role::Sidetone) ? 128
                       : (m_role == Role::Monitor)   ? 256
                                                     : 512;
    m_currentQuantum = s.value(m_settingsPrefix + QStringLiteral("/Quantum"),
                               defaultQ).toInt();

    // Update quantum button check states
    auto setChecked = [&](QPushButton* btn, int q) {
        QSignalBlocker sb(btn);
        btn->setChecked(m_currentQuantum == q);
    };
    setChecked(m_q128Btn,  128);
    setChecked(m_q256Btn,  256);
    setChecked(m_q512Btn,  512);
    setChecked(m_q1024Btn, 1024);

    const bool isCustom = (m_currentQuantum != 128) && (m_currentQuantum != 256)
                       && (m_currentQuantum != 512) && (m_currentQuantum != 1024);
    {
        QSignalBlocker sb(m_qCustomBtn);
        m_qCustomBtn->setChecked(isCustom);
        if (isCustom) {
            m_qCustomBtn->setText(QStringLiteral("custom (%1)").arg(m_currentQuantum));
        }
    }

    // RT warning for Sidetone
    if (m_rtWarningLabel) {
        m_rtWarningLabel->setVisible(m_currentQuantum == 128);
    }

    // Primary: volume + mute-on-TX
    if (m_role == Role::Primary) {
        const int vol = s.value(QStringLiteral("Audio/Output/Primary/Volume"), 100).toInt();
        if (m_volumeSlider) {
            QSignalBlocker sb(m_volumeSlider);
            m_volumeSlider->setValue(vol);
            if (m_volumeValueLabel) {
                m_volumeValueLabel->setText(QString::number(vol));
            }
        }

        const bool muteOnTx =
            s.value(QStringLiteral("Audio/Output/Primary/MuteOnTx"), QStringLiteral("False"))
             .toString() == QStringLiteral("True");
        if (m_muteOnTxCheck) {
            QSignalBlocker sb(m_muteOnTxCheck);
            m_muteOnTxCheck->setChecked(muteOnTx);
        }
    }
}

void OutputCard::saveToSettings()
{
    auto& s = AppSettings::instance();

    // Sink node name — empty string when "follow default" is selected.
    const QString sink = m_sinkCombo->currentIndex() == 0
                       ? QString()
                       : m_sinkCombo->currentText();
    s.setValue(m_settingsPrefix + QStringLiteral("/SinkNodeName"), sink);

    // Quantum
    s.setValue(m_settingsPrefix + QStringLiteral("/Quantum"),
               QString::number(m_currentQuantum));

    // Primary extras
    if (m_role == Role::Primary) {
        if (m_volumeSlider) {
            s.setValue(QStringLiteral("Audio/Output/Primary/Volume"),
                       QString::number(m_volumeSlider->value()));
        }
        if (m_muteOnTxCheck) {
            s.setValue(QStringLiteral("Audio/Output/Primary/MuteOnTx"),
                       m_muteOnTxCheck->isChecked() ? QStringLiteral("True")
                                                    : QStringLiteral("False"));
        }
    }

    s.save();
}

// ---------------------------------------------------------------------------
// OutputCard slots
// ---------------------------------------------------------------------------

void OutputCard::onSinkComboChanged(int /*index*/)
{
    const QString sink = m_sinkCombo->currentIndex() == 0
                       ? QString()
                       : m_sinkCombo->currentText();
    auto& s = AppSettings::instance();
    s.setValue(m_settingsPrefix + QStringLiteral("/SinkNodeName"), sink);
    s.save();
    emit sinkNodeNameChanged(sink);
}

void OutputCard::onQuantumPreset(int quantum)
{
    m_currentQuantum = quantum;

    // Update button check states
    auto setChecked = [&](QPushButton* btn, int q) {
        QSignalBlocker sb(btn);
        btn->setChecked(m_currentQuantum == q);
    };
    setChecked(m_q128Btn,  128);
    setChecked(m_q256Btn,  256);
    setChecked(m_q512Btn,  512);
    setChecked(m_q1024Btn, 1024);
    {
        QSignalBlocker sb(m_qCustomBtn);
        m_qCustomBtn->setChecked(false);
        m_qCustomBtn->setText(tr("custom…"));
    }

    // RT warning
    if (m_rtWarningLabel) {
        m_rtWarningLabel->setVisible(m_currentQuantum == 128);
    }

    auto& s = AppSettings::instance();
    s.setValue(m_settingsPrefix + QStringLiteral("/Quantum"),
               QString::number(m_currentQuantum));
    s.save();
    emit quantumChanged(m_currentQuantum);
}

void OutputCard::onCustomQuantum()
{
    bool ok = false;
    const int val = QInputDialog::getInt(this,
                        tr("Custom quantum"),
                        tr("Enter quantum frame count (power of 2, 64–8192):"),
                        m_currentQuantum,
                        64, 8192, 1, &ok);
    if (!ok) {
        // User cancelled — restore previous button state
        QSignalBlocker sb(m_qCustomBtn);
        const bool wasCustom = (m_currentQuantum != 128) && (m_currentQuantum != 256)
                            && (m_currentQuantum != 512) && (m_currentQuantum != 1024);
        m_qCustomBtn->setChecked(wasCustom);
        return;
    }

    m_currentQuantum = val;

    // Clear the preset buttons
    {
        QSignalBlocker sb1(m_q128Btn);  m_q128Btn->setChecked(false);
        QSignalBlocker sb2(m_q256Btn);  m_q256Btn->setChecked(false);
        QSignalBlocker sb3(m_q512Btn);  m_q512Btn->setChecked(false);
        QSignalBlocker sb4(m_q1024Btn); m_q1024Btn->setChecked(false);
    }
    {
        QSignalBlocker sb(m_qCustomBtn);
        m_qCustomBtn->setChecked(true);
        m_qCustomBtn->setText(QStringLiteral("custom (%1)").arg(m_currentQuantum));
    }

    if (m_rtWarningLabel) {
        m_rtWarningLabel->setVisible(m_currentQuantum == 128);
    }

    auto& s = AppSettings::instance();
    s.setValue(m_settingsPrefix + QStringLiteral("/Quantum"),
               QString::number(m_currentQuantum));
    s.save();
    emit quantumChanged(m_currentQuantum);
}

void OutputCard::onVolumeChanged(int value)
{
    if (m_volumeValueLabel) {
        m_volumeValueLabel->setText(QString::number(value));
    }
    AppSettings::instance().setValue(QStringLiteral("Audio/Output/Primary/Volume"),
                                     QString::number(value));
    AppSettings::instance().save();
}

void OutputCard::onMuteOnTxChanged(bool checked)
{
    AppSettings::instance().setValue(QStringLiteral("Audio/Output/Primary/MuteOnTx"),
                                     checked ? QStringLiteral("True")
                                             : QStringLiteral("False"));
    AppSettings::instance().save();
}

// ============================================================================
// AudioOutputPage
// ============================================================================

AudioOutputPage::AudioOutputPage(AudioEngine* eng,
                                 RadioModel*  radio,
                                 QWidget*     parent)
    : SetupPage(tr("Output"), radio, parent)
    , m_eng(eng)
    , m_radio(radio)
{
    buildPage();
}

void AudioOutputPage::buildPage()
{
    // ── Primary card ──────────────────────────────────────────────────────
    m_primaryCard = new OutputCard(
        tr("Primary output"),
        OutputCard::Role::Primary,
        QStringLiteral("Audio/Output/Primary"),
        this);
    contentLayout()->addWidget(m_primaryCard);

    // ── Sidetone card ─────────────────────────────────────────────────────
    m_sidetoneCard = new OutputCard(
        tr("Sidetone"),
        OutputCard::Role::Sidetone,
        QStringLiteral("Audio/Output/Sidetone"),
        this);

    // Sidetone disabled by default — gate it with AppSettings Enabled flag.
    {
        const bool enabled =
            AppSettings::instance()
                .value(QStringLiteral("Audio/Output/Sidetone/Enabled"),
                       QStringLiteral("False"))
                .toString() == QStringLiteral("True");
        m_sidetoneCard->setEnabled(enabled);
    }
    contentLayout()->addWidget(m_sidetoneCard);

    // ── Monitor card ──────────────────────────────────────────────────────
    m_monitorCard = new OutputCard(
        tr("Monitor (MON)"),
        OutputCard::Role::Monitor,
        QStringLiteral("Audio/Output/Monitor"),
        this);

    {
        const bool enabled =
            AppSettings::instance()
                .value(QStringLiteral("Audio/Output/Monitor/Enabled"),
                       QStringLiteral("False"))
                .toString() == QStringLiteral("True");
        m_monitorCard->setEnabled(enabled);
    }
    contentLayout()->addWidget(m_monitorCard);

    // ── Per-slice routing section ─────────────────────────────────────────
    buildPerSliceSection();

    contentLayout()->addStretch();
}

void AudioOutputPage::buildPerSliceSection()
{
    QGroupBox* box = new QGroupBox(tr("Per-slice output routing"), this);
    box->setToolTip(tr(
        "Override the output sink on a per-slice basis.\n"
        "\"follow default\" inherits the primary-card sink."));

    QVBoxLayout* boxLayout = new QVBoxLayout(box);
    box->setStyleSheet(QLatin1String(kGroupStyle));
    boxLayout->setSpacing(6);
    boxLayout->setContentsMargins(10, 14, 10, 10);

    const QStringList choices = sinkChoices();

    if (!m_radio) {
        QLabel* noRadio = new QLabel(tr("(no radio connected)"), box);
        noRadio->setStyleSheet(QLatin1String(kInfoLabelStyle));
        boxLayout->addWidget(noRadio);
        contentLayout()->addWidget(box);
        return;
    }

    const QList<SliceModel*> slices = m_radio->slices();
    if (slices.isEmpty()) {
        QLabel* noSlice = new QLabel(tr("(no slices)"), box);
        noSlice->setStyleSheet(QLatin1String(kInfoLabelStyle));
        boxLayout->addWidget(noSlice);
        contentLayout()->addWidget(box);
        return;
    }

    for (int i = 0; i < slices.size(); ++i) {
        SliceModel* slice = slices.at(i);

        QHBoxLayout* row = new QHBoxLayout;

        // "Slice N →" label
        QLabel* sliceLbl = new QLabel(
            tr("Slice %1 →").arg(i + 1), box);
        sliceLbl->setStyleSheet(QLatin1String(kSectionLabelStyle));
        sliceLbl->setFixedWidth(70);
        row->addWidget(sliceLbl);

        // Sink combo
        QComboBox* combo = new QComboBox(box);
        for (const QString& s : choices) {
            combo->addItem(s);
        }
        combo->setToolTip(tr("Output sink for Slice %1.").arg(i + 1));

        // Set current value from SliceModel
        const QString savedSink = slice->sinkNodeName();
        if (!savedSink.isEmpty()) {
            const int idx = combo->findText(savedSink);
            if (idx >= 0) {
                combo->setCurrentIndex(idx);
            } else {
                combo->addItem(savedSink);
                combo->setCurrentText(savedSink);
            }
        }
        row->addWidget(combo, 1);

        // "current sink: …" read-only indicator
        QLabel* currentLbl = new QLabel(box);
        const QString displaySink = savedSink.isEmpty()
                                  ? tr("(follows primary)")
                                  : savedSink;
        currentLbl->setText(tr("current sink: %1").arg(displaySink));
        currentLbl->setStyleSheet(QLatin1String(kInfoLabelStyle));
        row->addWidget(currentLbl);

        boxLayout->addLayout(row);
        m_sliceSinkCombos.append(combo);

        // Wire combo → SliceModel::setSinkNodeName
        // Capture slice* and currentLbl* directly — both are owned by
        // RadioModel (lifetime >= this page) and the label is owned by box.
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [combo, currentLbl, slice](int index) {
                    const QString sink = (index == 0)
                                       ? QString()
                                       : combo->currentText();
                    slice->setSinkNodeName(sink);
                    const QString display = sink.isEmpty()
                                          ? tr("(follows primary)")
                                          : sink;
                    currentLbl->setText(tr("current sink: %1").arg(display));
                });
    }

    contentLayout()->addWidget(box);
}

} // namespace NereusSDR
