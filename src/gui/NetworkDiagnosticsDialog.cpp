// src/gui/NetworkDiagnosticsDialog.cpp
// =================================================================
// src/gui/NetworkDiagnosticsDialog.cpp  (NereusSDR)
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Created for Phase 3Q sub-PR-3 (shell-chrome redesign).
//                NereusSDR-original code (no Thetis port; no upstream
//                attribution required). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/NetworkDiagnosticsDialog.h"
#include "models/RadioModel.h"
#include "core/AudioEngine.h"
#include "core/RadioConnection.h"

#if defined(Q_OS_LINUX)
#  include "core/audio/LinuxAudioBackend.h"
#endif

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

// ─── styling constants ────────────────────────────────────────────────────────
// Color palette matches the shell-chrome redesign (docs/architecture/
// 2026-04-30-shell-chrome-redesign-design.md).
static constexpr const char* kValueStyle =
    "color: #c8d8e8;"
    "font-family: 'SF Mono', Menlo, monospace;"
    "font-size: 11px;";

static constexpr const char* kFieldStyle =
    "color: #8aa8c0;"
    "font-family: 'SF Mono', Menlo, monospace;"
    "font-size: 11px;";

static constexpr const char* kSectionHeaderStyle =
    "color: #5fa8ff;"
    "font-family: 'SF Mono', Menlo, monospace;"
    "font-size: 11px;"
    "font-weight: bold;"
    "padding: 6px 0;"
    "border-bottom: 1px solid #203040;";

// ─── helpers ──────────────────────────────────────────────────────────────────

QLabel* NetworkDiagnosticsDialog::makeValueLabel()
{
    QLabel* lbl = new QLabel(QStringLiteral("—"), this);
    lbl->setStyleSheet(QString::fromUtf8(kValueStyle));
    return lbl;
}

QLabel* NetworkDiagnosticsDialog::makeFieldLabel(const QString& text)
{
    QLabel* lbl = new QLabel(text, this);
    lbl->setStyleSheet(QString::fromUtf8(kFieldStyle));
    return lbl;
}

// ─── section builders ─────────────────────────────────────────────────────────

static void addSectionHeader(QGridLayout* grid, int row, const QString& title)
{
    QLabel* hdr = new QLabel(title);
    hdr->setStyleSheet(QString::fromUtf8(kSectionHeaderStyle));
    grid->addWidget(hdr, row, 0, 1, 4);
}

void NetworkDiagnosticsDialog::buildConnectionSection(QGridLayout* grid, int& row)
{
    addSectionHeader(grid, row++, tr("Connection"));

    auto addRow = [&](const QString& field, QLabel*& valueOut) {
        grid->addWidget(makeFieldLabel(field), row, 0);
        valueOut = makeValueLabel();
        grid->addWidget(valueOut, row, 1, 1, 3);
        ++row;
    };

    addRow(tr("Status"),   m_statusLabel);
    addRow(tr("Uptime"),   m_uptimeLabel);
    addRow(tr("Radio"),    m_radioLabel);
    addRow(tr("Protocol"), m_protocolLabel);
    addRow(tr("IP"),       m_ipLabel);
    addRow(tr("MAC"),      m_macLabel);
}

void NetworkDiagnosticsDialog::buildNetworkSection(QGridLayout* grid, int& row)
{
    addSectionHeader(grid, row++, tr("Network"));

    auto addRow = [&](const QString& field, QLabel*& valueOut) {
        grid->addWidget(makeFieldLabel(field), row, 0);
        valueOut = makeValueLabel();
        grid->addWidget(valueOut, row, 1, 1, 3);
        ++row;
    };

    addRow(tr("Latency (RTT)"), m_rttLabel);
    addRow(tr("Max RTT"),       m_maxRttLabel);
    addRow(tr("Jitter"),        m_jitterLabel);
    addRow(tr("Packet loss"),   m_lossLabel);
    addRow(tr("▲ TX rate"), m_txRateLabel);
    addRow(tr("▼ RX rate"), m_rxRateLabel);
    addRow(tr("Sample rate"),   m_sampleRateLabel);
    addRow(tr("UDP seen"),      m_udpSeenLabel);
}

void NetworkDiagnosticsDialog::buildAudioSection(QGridLayout* grid, int& row)
{
    addSectionHeader(grid, row++, tr("Audio"));

    auto addRow = [&](const QString& field, QLabel*& valueOut) {
        grid->addWidget(makeFieldLabel(field), row, 0);
        valueOut = makeValueLabel();
        grid->addWidget(valueOut, row, 1, 1, 3);
        ++row;
    };

    addRow(tr("Backend"),    m_audioBackendLabel);
    addRow(tr("Buffer"),     m_audioBufferLabel);
    addRow(tr("Underruns"),  m_underrunLabel);
    addRow(tr("Packet gap"), m_packetGapLabel);
}

void NetworkDiagnosticsDialog::buildTelemetrySection(QGridLayout* grid, int& row)
{
    addSectionHeader(grid, row++, tr("Radio Telemetry"));

    auto addRow = [&](const QString& field, QLabel*& valueOut) {
        grid->addWidget(makeFieldLabel(field), row, 0);
        valueOut = makeValueLabel();
        grid->addWidget(valueOut, row, 1, 1, 3);
        ++row;
    };

    // PSU row removed (Thetis source-first audit 2026-04-30): the
    // supply_volts (AIN6) channel is dead data — Thetis never displays
    // it (computeHermesDCVoltage has zero callers; the only voltage
    // status indicator reads user_adc0 / convertToVolts). The PA volt
    // row below is now the sole supply indicator on MkII-class boards.
    addRow(tr("PA voltage"),  m_paVoltLabel);
    addRow(tr("ADC overload"), m_adcOvlLabel);

    // Set meaningful placeholders for fields that have no live source yet.
    if (m_paVoltLabel) {
        m_paVoltLabel->setText(tr("— (not reported)"));
    }
    if (m_adcOvlLabel) {
        m_adcOvlLabel->setText(tr("None"));
    }
}

// ─── buildUi ──────────────────────────────────────────────────────────────────

void NetworkDiagnosticsDialog::buildUi()
{
    setStyleSheet(QStringLiteral(
        "QDialog { background: #141c28; }"
        "QLabel  { background: transparent; }"
    ));

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setSpacing(4);
    root->setContentsMargins(16, 12, 16, 12);

    // ── 4-column grid for all sections ────────────────────────────────────────
    QGridLayout* grid = new QGridLayout;
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 0);
    grid->setColumnStretch(3, 0);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(4);
    root->addLayout(grid);

    int row = 0;
    buildConnectionSection(grid, row);
    buildNetworkSection(grid, row);
    buildAudioSection(grid, row);
    buildTelemetrySection(grid, row);

    root->addStretch();

    // ── button row ────────────────────────────────────────────────────────────
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();

    QPushButton* resetBtn = new QPushButton(tr("Reset session stats"), this);
    resetBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: #8aa8c0;"
        "  background: #1e2c3a;"
        "  border: 1px solid #2e4050;"
        "  border-radius: 3px;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover { background: #253545; }"
        "QPushButton:pressed { background: #1a2530; }"
    ));
    connect(resetBtn, &QPushButton::clicked,
            this, &NetworkDiagnosticsDialog::onResetSessionStats);
    btnRow->addWidget(resetBtn);

    QPushButton* closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: #c8d8e8;"
        "  background: #1e3050;"
        "  border: 1px solid #2e4060;"
        "  border-radius: 3px;"
        "  padding: 4px 16px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover { background: #253d60; }"
        "QPushButton:pressed { background: #1a2a48; }"
    ));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);

    root->addLayout(btnRow);
}

// ─── constructor ──────────────────────────────────────────────────────────────

NetworkDiagnosticsDialog::NetworkDiagnosticsDialog(RadioModel* model,
                                                   AudioEngine* audio,
                                                   QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_audio(audio)
{
    setWindowTitle(tr("Network Diagnostics"));
    setMinimumSize(720, 540);
    setModal(true);

    buildUi();

    // ── wire sub-PR-2 signals ─────────────────────────────────────────────────
    if (m_model && m_model->connection()) {
        RadioConnection* conn = m_model->connection();

        connect(conn, &RadioConnection::pingRttMeasured,
                this, [this](int rttMs) {
                    m_lastRttMs = rttMs;
                    if (rttMs > m_maxRttMs) {
                        m_maxRttMs = rttMs;
                    }
                });

        // (PSU / supplyVoltsChanged wiring removed — see addRow note above.)

        connect(conn, &RadioConnection::userAdc0Changed,
                this, [this](float volts) {
                    m_lastUserAdc0Volts = volts;
                    if (m_paVoltLabel) {
                        m_paVoltLabel->setText(
                            QString::asprintf("%.2f V", static_cast<double>(volts)));
                    }
                });
    }

    if (m_audio) {
        connect(m_audio, &AudioEngine::flowStateChanged,
                this, [this](AudioEngine::FlowState state) {
                    if (state == AudioEngine::FlowState::Underrun
                            || state == AudioEngine::FlowState::Stalled) {
                        ++m_underrunCount;
                    }
                });
    }

    // ── refresh timer ─────────────────────────────────────────────────────────
    m_refreshTimer.setInterval(250);
    connect(&m_refreshTimer, &QTimer::timeout,
            this, &NetworkDiagnosticsDialog::refresh);

    refresh();
    m_refreshTimer.start();
}

// ─── refresh ─────────────────────────────────────────────────────────────────

void NetworkDiagnosticsDialog::refresh()
{
    // ── Connection section ────────────────────────────────────────────────────
    if (m_statusLabel) {
        const bool connected = m_model && m_model->isConnected();
        m_statusLabel->setText(
            connected
                ? QStringLiteral("<span style='color:#5fff8a;'>&#x25cf; Connected</span>")
                : QStringLiteral("<span style='color:#ff6060;'>&#x25cf; Disconnected</span>"));
        m_statusLabel->setTextFormat(Qt::RichText);
    }

    if (m_uptimeLabel) {
        m_uptimeLabel->setText(
            m_model ? m_model->connectionUptimeText() : QStringLiteral("—"));
    }

    if (m_radioLabel) {
        m_radioLabel->setText(
            m_model ? m_model->connectedRadioName() : QStringLiteral("—"));
    }

    if (m_protocolLabel) {
        if (m_model && m_model->isConnected()) {
            const QString proto  = m_model->connectionProtocolText();
            const QString fw     = m_model->connectionFirmwareText();
            m_protocolLabel->setText(
                tr("OpenHPSDR P%1 \xc2\xb7 %2").arg(proto, fw));
        } else {
            m_protocolLabel->setText(QStringLiteral("—"));
        }
    }

    if (m_ipLabel) {
        m_ipLabel->setText(
            m_model ? m_model->connectionIpText() : QStringLiteral("—"));
    }

    if (m_macLabel) {
        m_macLabel->setText(
            m_model ? m_model->connectionMacText() : QStringLiteral("—"));
    }

    // ── Network section ───────────────────────────────────────────────────────
    if (m_rttLabel) {
        m_rttLabel->setText(
            m_lastRttMs >= 0
                ? QString::asprintf("%d ms", m_lastRttMs)
                : QStringLiteral("— ms"));
    }

    if (m_maxRttLabel) {
        m_maxRttLabel->setText(
            m_maxRttMs > 0
                ? QString::asprintf("%d ms", m_maxRttMs)
                : QStringLiteral("— ms"));
    }

    // Jitter, packet loss, and packet gap are NYI: no production source
    // yet. Earlier revisions hardcoded "0 ms" / "0.0%" which read as
    // "perfect network" rather than "not measured" — replaced with
    // em-dash so the dialog tells the truth. Wiring these requires
    // protocol-level instrumentation (RTT-sample variance for jitter,
    // sequence-number gap detection for loss, max inter-arrival time
    // for gap) — tracked as a follow-up item.
    if (m_jitterLabel) {
        m_jitterLabel->setText(QChar(0x2014));   // em-dash
        m_jitterLabel->setToolTip(tr("Not yet measured"));
    }

    if (m_lossLabel) {
        m_lossLabel->setText(QChar(0x2014));     // em-dash
        m_lossLabel->setToolTip(tr("Not yet measured"));
    }

    // RadioConnection::txByteRate / rxByteRate already return Mbps despite
    // the misleading "ByteRate" name (see RadioConnection.h declaration).
    // Earlier revisions of this dialog applied an extra " * 8 / 1e6 " here
    // and divided the real value by 125 000 — TX/RX rates always read 0.
    // Bug caught 2026-04-30 against a live ANAN-G2.
    if (m_txRateLabel) {
        if (m_model && m_model->connection() && m_model->isConnected()) {
            const double mbps = m_model->connection()->txByteRate(1000);
            m_txRateLabel->setText(QString::asprintf("%.2f Mbps", mbps));
        } else {
            m_txRateLabel->setText(QStringLiteral("—"));
        }
    }

    if (m_rxRateLabel) {
        if (m_model && m_model->connection() && m_model->isConnected()) {
            const double mbps = m_model->connection()->rxByteRate(1000);
            m_rxRateLabel->setText(QString::asprintf("%.2f Mbps", mbps));
        } else {
            m_rxRateLabel->setText(QStringLiteral("—"));
        }
    }

    if (m_sampleRateLabel) {
        m_sampleRateLabel->setText(
            m_model ? m_model->connectionSampleRateText() : QStringLiteral("—"));
    }

    if (m_udpSeenLabel) {
        // No production source yet — would come from RadioConnection packet counter
        m_udpSeenLabel->setText(QStringLiteral("—"));
    }

    // ── Audio section ─────────────────────────────────────────────────────────
    if (m_audioBackendLabel) {
        QString backend = QStringLiteral("—");
        if (m_audio) {
#if defined(Q_OS_LINUX)
            backend = NereusSDR::toString(m_audio->linuxBackend());
#elif defined(Q_OS_MACOS)
            backend = QStringLiteral("CoreAudio");
#elif defined(Q_OS_WIN)
            backend = QStringLiteral("WASAPI");
#else
            backend = QStringLiteral("—");
#endif
        }
        m_audioBackendLabel->setText(backend);
    }

    if (m_audioBufferLabel) {
        m_audioBufferLabel->setText(QStringLiteral("—")); // no production source yet
    }

    if (m_underrunLabel) {
        m_underrunLabel->setText(QString::number(m_underrunCount));
    }

    if (m_packetGapLabel) {
        m_packetGapLabel->setText(QChar(0x2014)); // em-dash, NYI — see above note
        m_packetGapLabel->setToolTip(tr("Not yet measured"));
    }

    // ── Radio Telemetry section ───────────────────────────────────────────────
    // PA voltage is updated live by signal lambda in the ctor; only
    // needs a disconnected-state reset here. (PSU row dropped per
    // Thetis source-first audit — see ctor.)
    if (m_paVoltLabel && (!m_model || !m_model->isConnected())) {
        if (m_lastUserAdc0Volts < 0.0f) {
            m_paVoltLabel->setText(tr("— (not reported)"));
        }
    }

    // ADC overload — no live source in sub-PR-3; placeholder "None".
    // (sub-PR-4 will wire RadioConnection::adcOverloadChanged if needed.)
    if (m_adcOvlLabel) {
        m_adcOvlLabel->setText(tr("None"));
    }
}

// ─── onResetSessionStats ─────────────────────────────────────────────────────

void NetworkDiagnosticsDialog::onResetSessionStats()
{
    m_maxRttMs     = 0;
    m_underrunCount = 0;
    refresh();
}

} // namespace NereusSDR
