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
//   2026-10-13 — Opt-in ASIO buffer logging; no visible diagnostics change.
//                J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-30 — Created for Phase 3Q sub-PR-3 (shell-chrome redesign).
//                NereusSDR-original code (no Thetis port; no upstream
//                attribution required). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: Jitter, Packet loss and Packet gap are hidden
//                (UnbuiltFeatures) until they are measured for a local
//                radio. A remote window opens RemoteDiagnosticsDialog
//                instead. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-25 - R-R3-32 / R-R3-49 (parity Task 6): Jitter, Packet loss,
//                Packet gap and UDP seen are measured for a local radio
//                (RadioLinkStats) and shown. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The Connection status word reads
//                WorkspacePreview::statusColourName() (the status colour
//                choice). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-10-09 - The section headers read WorkspacePreview::roleColourName()
//                (title-text) with the preview on. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-14 - The Audio Backend row names the engine in use in the
//                words of Settings > Audio's "Sound system" line
//                (SoundSystemLine::describeEngine), not the build's
//                platform. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

#include "gui/NetworkDiagnosticsDialog.h"
#include "StyleConstants.h"
#include "gui/styles/WorkspacePreview.h"
#include <optional>
#include "models/RadioModel.h"
#include "core/AudioEngine.h"
#include "core/RadioConnection.h"
#include "gui/setup/SoundSystemLine.h"

#if defined(Q_OS_LINUX)
#  include "core/audio/LinuxAudioBackend.h"
#endif

#include <QDialogButtonBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

// Explicitly off by default, including outside the application's filter.
// Opt in with QT_LOGGING_RULES="nereus.audio.asio.buffer.debug=true".
Q_LOGGING_CATEGORY(lcAsioBufferDiagnostics, "nereus.audio.asio.buffer", QtWarningMsg)

// ─── styling constants ────────────────────────────────────────────────────────
// Color palette matches the shell-chrome redesign (docs/architecture/
// 2026-04-30-shell-chrome-redesign-design.md).
// Per docs/architecture/ui-audit-polish-plan.md §D — Task 23.
// All color values verified against StyleConstants.h:
//   Style::kTextPrimary = #c8d8e8  (kValueStyle color — exact match)
//   Style::kTitleText   = #8aa8c0  (kFieldStyle color — exact match)
//   Style::kBorderSubtle = #203040 (kSectionHeaderStyle border — exact match)
//
// §D exception (no canonical match):
//   #5fa8ff — diagnostic section header blue; brighter than kAccent (#00b4d8),
//              and has cyan/blue rather than cyan tint. Kept with note.
//   Monospace font stack is diagnostics-specific (all three constants).
static constexpr const char* kValueStyle =
    "color: #c8d8e8;"           // Style::kTextPrimary
    "font-family: 'SF Mono', Menlo, monospace;"
    "font-size: 11px;";

static constexpr const char* kFieldStyle =
    "color: #8aa8c0;"           // Style::kTitleText
    "font-family: 'SF Mono', Menlo, monospace;"
    "font-size: 11px;";

static constexpr const char* kSectionHeaderStyle =
    "color: #5fa8ff;"           // §D exception: diagnostics section header blue
    "font-family: 'SF Mono', Menlo, monospace;"
    "font-size: 11px;"
    "font-weight: bold;"
    "padding: 6px 0;"
    "border-bottom: 1px solid #203040;";  // Style::kBorderSubtle

// ─── helpers ──────────────────────────────────────────────────────────────────

QLabel* NetworkDiagnosticsDialog::makeValueLabel()
{
    QLabel* lbl = new QLabel(QStringLiteral("–"), this);
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
    // The header blue is decoration: the header text with the preview on.
    hdr->setStyleSheet(QString::fromUtf8(kSectionHeaderStyle)
                           .replace(QStringLiteral("#5fa8ff"),
                                    WorkspacePreview::roleColourName("#5fa8ff", "title-text")));
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
    // The Sound system line's words can be a sentence (PipeWire, older
    // drivers in use); wrap rather than widen the dialog.
    m_audioBackendLabel->setWordWrap(true);
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
        m_paVoltLabel->setText(tr("– (not reported)"));
    }
    if (m_adcOvlLabel) {
        m_adcOvlLabel->setText(tr("None"));
    }
}

// ─── buildUi ──────────────────────────────────────────────────────────────────

void NetworkDiagnosticsDialog::buildUi()
{
    // §D exception: #141c28 — diagnostics dialog bg; darker blue-black than
    // kAppBg (#0f0f1a) to visually separate from the main window.
    setStyleSheet(QStringLiteral(
        "QDialog { background: #141c28; }"  // §D exception: diagnostics-specific bg
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
    // §D: color #8aa8c0 = Style::kTitleText (exact match).
    // §D exceptions: bg #1e2c3a / border #2e4050 / hover #253545 / pressed #1a2530
    //   — diagnostics button surface; between kButtonBg (#1a2a3a) and kBorderSubtle
    //   (#203040), chosen to contrast with the #141c28 dialog bg.
    resetBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  color: %1;"                       // Style::kTitleText
            "  background: #1e2c3a;"             // §D exception: diagnostics btn bg
            "  border: 1px solid #2e4050;"       // §D exception: diagnostics btn border
            "  border-radius: 3px;"
            "  padding: 4px 10px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover { background: #253545; }"   // §D exception
            "QPushButton:pressed { background: #1a2530; }" // §D exception
        ).arg(Style::kTitleText));
    connect(resetBtn, &QPushButton::clicked,
            this, &NetworkDiagnosticsDialog::onResetSessionStats);
    btnRow->addWidget(resetBtn);

    QPushButton* closeBtn = new QPushButton(tr("Close"), this);
    // §D: color #c8d8e8 = Style::kTextPrimary (exact match).
    // §D exceptions: bg #1e3050 / border #2e4060 / hover #253d60 / pressed #1a2a48
    //   — diagnostics close button uses a more prominent blue tint to distinguish
    //   it from the reset button; no canonical match.
    closeBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  color: %1;"                       // Style::kTextPrimary
            "  background: #1e3050;"             // §D exception: diagnostics close btn bg
            "  border: 1px solid #2e4060;"       // §D exception
            "  border-radius: 3px;"
            "  padding: 4px 16px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover { background: #253d60; }"   // §D exception
            "QPushButton:pressed { background: #1a2a48; }" // §D exception
        ).arg(Style::kTextPrimary));
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
    logAsioBufferDiagnostics();
    // ── Connection section ────────────────────────────────────────────────────
    if (m_statusLabel) {
        const bool connected = m_model && m_model->isConnected();
        m_statusLabel->setText(
            connected
                ? QStringLiteral("<span style='color:%1;'>&#x25cf; Connected</span>")
                      .arg(WorkspacePreview::statusColourName("#5fff8a", "ok"))
                : QStringLiteral("<span style='color:%1;'>&#x25cf; Disconnected</span>")
                      .arg(WorkspacePreview::statusColourName("#ff6060", "danger")));
        m_statusLabel->setTextFormat(Qt::RichText);
    }

    if (m_uptimeLabel) {
        m_uptimeLabel->setText(
            m_model ? m_model->connectionUptimeText() : QStringLiteral("–"));
    }

    if (m_radioLabel) {
        m_radioLabel->setText(
            m_model ? m_model->connectedRadioName() : QStringLiteral("–"));
    }

    if (m_protocolLabel) {
        if (m_model && m_model->isConnected()) {
            const QString proto  = m_model->connectionProtocolText();
            const QString fw     = m_model->connectionFirmwareText();
            m_protocolLabel->setText(
                tr("OpenHPSDR P%1 \xc2\xb7 %2").arg(proto, fw));
        } else {
            m_protocolLabel->setText(QStringLiteral("–"));
        }
    }

    if (m_ipLabel) {
        m_ipLabel->setText(
            m_model ? m_model->connectionIpText() : QStringLiteral("–"));
    }

    if (m_macLabel) {
        m_macLabel->setText(
            m_model ? m_model->connectionMacText() : QStringLiteral("–"));
    }

    // ── Network section ───────────────────────────────────────────────────────
    if (m_rttLabel) {
        m_rttLabel->setText(
            m_lastRttMs >= 0
                ? QString::asprintf("%d ms", m_lastRttMs)
                : QStringLiteral("– ms"));
    }

    if (m_maxRttLabel) {
        m_maxRttLabel->setText(
            m_maxRttMs > 0
                ? QString::asprintf("%d ms", m_maxRttMs)
                : QStringLiteral("– ms"));
    }

    // R-R3-32 / R-R3-49 (parity Task 6): jitter, packet loss, packet gap
    // and UDP packets seen come from the connection's own counters
    // (RadioLinkStats); a value not measured says so.
    const RadioConnection* linkConn =
        (m_model && m_model->isConnected()) ? m_model->connection() : nullptr;
    const std::optional<RadioLinkStats::Snapshot> link =
        linkConn ? std::optional<RadioLinkStats::Snapshot>(linkConn->linkStats())
                 : std::nullopt;
    const QString notMeasured = tr("Not measured");
    if (m_jitterLabel) {
        m_jitterLabel->setText(link && link->jitterMs
            ? QString::asprintf("%.2f ms", *link->jitterMs) : notMeasured);
    }

    if (m_lossLabel) {
        m_lossLabel->setText(link && link->packetLossPercent
            ? QString::asprintf("%.2f%%", *link->packetLossPercent) : notMeasured);
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
            m_txRateLabel->setText(QStringLiteral("–"));
        }
    }

    if (m_rxRateLabel) {
        if (m_model && m_model->connection() && m_model->isConnected()) {
            const double mbps = m_model->connection()->rxByteRate(1000);
            m_rxRateLabel->setText(QString::asprintf("%.2f Mbps", mbps));
        } else {
            m_rxRateLabel->setText(QStringLiteral("–"));
        }
    }

    if (m_sampleRateLabel) {
        m_sampleRateLabel->setText(
            m_model ? m_model->connectionSampleRateText() : QStringLiteral("–"));
    }

    if (m_udpSeenLabel) {
        m_udpSeenLabel->setText(link ? QString::number(link->udpPacketsSeen) : notMeasured);
    }

    // ── Audio section ─────────────────────────────────────────────────────────
    if (m_audioBackendLabel) {
        // The engine in use, named as Settings > Audio > Outputs' "Sound
        // system" line names it (the device catalogue's system, ASIO and
        // the older drivers the saved choices use).
        m_audioBackendLabel->setText(m_audio ? SoundSystemLine::describeEngine(m_audio.data())
                                             : QStringLiteral("–"));
    }

    if (m_audioBufferLabel) {
        m_audioBufferLabel->setText(QStringLiteral("–")); // no production source yet
    }

    if (m_underrunLabel) {
        m_underrunLabel->setText(QString::number(m_underrunCount));
    }

    if (m_packetGapLabel) {
        // The longest interval between two datagrams from the radio in the
        // last second (RadioLinkStats).
        m_packetGapLabel->setText(link && link->packetGapMs
            ? QString::asprintf("%.1f ms", *link->packetGapMs) : notMeasured);
    }

    // ── Radio Telemetry section ───────────────────────────────────────────────
    // PA voltage is updated live by signal lambda in the ctor; only
    // needs a disconnected-state reset here. (PSU row dropped per
    // Thetis source-first audit — see ctor.)
    if (m_paVoltLabel && (!m_model || !m_model->isConnected())) {
        if (m_lastUserAdc0Volts < 0.0f) {
            m_paVoltLabel->setText(tr("– (not reported)"));
        }
    }

    // ADC overload — no live source in sub-PR-3; placeholder "None".
    // (sub-PR-4 will wire RadioConnection::adcOverloadChanged if needed.)
    if (m_adcOvlLabel) {
        m_adcOvlLabel->setText(tr("None"));
    }
}

void NetworkDiagnosticsDialog::logAsioBufferDiagnostics()
{
    if (!lcAsioBufferDiagnostics().isDebugEnabled()) {
        // Re-enabling starts new baselines: disabled time is not a delta.
        for (AsioBaseline& baseline : m_asioBaselines) {
            baseline.valid = false;
        }
        m_asioLastLogNs = 0;
        return; // no clock, bus snapshot, provider or device query
    }
    const std::int64_t nowNs = audioProbeNowNs();
    if (nowNs <= 0 || (m_asioLastLogNs != 0 && nowNs < m_asioLastLogNs)) {
        for (AsioBaseline& baseline : m_asioBaselines) {
            baseline.valid = false;
        }
        m_asioLastLogNs = nowNs > 0 ? nowNs : 0;
        return; // invalid/backward clock: no query or unsigned subtraction
    }
    constexpr std::int64_t kLogIntervalNs = 1'000'000'000;
    if (m_asioLastLogNs != 0 && nowNs >= m_asioLastLogNs
        && nowNs - m_asioLastLogNs < kLogIntervalNs) {
        return;
    }
    m_asioLastLogNs = nowNs;
    const QString pollUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    constexpr std::array<AudioRole, 2> kRoles{AudioRole::Speakers, AudioRole::Headphones};
    for (std::size_t i = 0; i < kRoles.size(); ++i) {
        AsioBaseline& baseline = m_asioBaselines[i];
        const char* role = i == 0 ? "speakers" : "headphones";
        const std::optional<AudioEngine::AsioOutputSnapshot> snapshot =
            m_audio ? m_audio->asioOutputSnapshotNow(kRoles[i]) : std::nullopt;
        if (!snapshot) {
            if (baseline.valid) {
                ++baseline.generation;
            }
            baseline.valid = false;
            qCDebug(lcAsioBufferDiagnostics).nospace()
                << "asio-buffer role=" << role << " available=false baselineGeneration="
                << baseline.generation << " counters=unavailable deltaValid=false"
                << " pollUtc=" << pollUtc << " pollMonoNs=" << nowNs
                << " moxObserved=unavailable";
            continue;
        }
        const DeviceRateMatcherStats& stats = snapshot->matcher;
        const bool deltaValid = baseline.valid
            && baseline.instanceId == snapshot->instanceId
            && baseline.roleGeneration == snapshot->roleGeneration
            && snapshot->capturedNs >= baseline.capturedNs
            && stats.dryRuns >= baseline.dryRuns && stats.overruns >= baseline.overruns;
        if (!deltaValid) {
            ++baseline.generation;
        }
        // Never subtract across a missing bus, changed lifetime or counter
        // rollback. Baseline means no interval measured, not zero events.
        const QString dryDelta = deltaValid ? QString::number(stats.dryRuns - baseline.dryRuns)
                                             : QStringLiteral("unavailable");
        const QString overDelta = deltaValid ? QString::number(stats.overruns - baseline.overruns)
                                              : QStringLiteral("unavailable");
        qCDebug(lcAsioBufferDiagnostics).nospace()
            << "asio-buffer role=" << role << " available=true utc=" << snapshot->capturedUtc
            << " captureMonoNs=" << snapshot->capturedNs << " backend=" << snapshot->backend
            << " openDeviceId=" << (snapshot->openDeviceId.isEmpty()
                ? QStringLiteral("unknown") : snapshot->openDeviceId)
            << " instanceId=" << snapshot->instanceId << " roleGeneration=" << snapshot->roleGeneration
            << " baselineGeneration=" << baseline.generation << " baseline=" << !deltaValid
            << " deltaValid=" << deltaValid << " intervalStartMonoNs="
            << (deltaValid ? QString::number(baseline.capturedNs) : QStringLiteral("unavailable"))
            << " previousMoxObserved=" << (deltaValid
                ? QString::number(baseline.moxObserved) : QStringLiteral("unavailable"))
            << " moxObserved=" << snapshot->moxObserved
            << " dryRuns=" << stats.dryRuns << " dryRunsDelta=" << dryDelta
            << " overruns=" << stats.overruns << " overrunsDelta=" << overDelta
            << " queuedOutputFrames=" << stats.queuedFrames << " fillOutputFrames=" << stats.fillFrames
            << " fillBasis=" << (stats.controlActive ? "averaged" : "instantaneous")
            << " logicalOutputFrames=" << stats.rsizeFrames << " capacityOutputFrames=" << stats.capacityFrames
            << " delayStepMs=" << stats.delayStepMs << " ratio=" << QString::number(stats.ratio, 'g', 17)
            << " controlActive=" << stats.controlActive << " appliedPacketInputFrames=" << stats.packetFrames
            << " busInputRateHz=" << snapshot->busInputFormat.sampleRate
            << " busInputChannels=" << snapshot->busInputFormat.channels
            << " busInputSample=" << static_cast<int>(snapshot->busInputFormat.sample)
            << " atomicTuple=noncoherent intervalMaySpanTxRx=true moxIsNotRxResume=true"
            << " counterScope=matcher dryRunsUnit=readerCallsLackingRequestedFrames"
            << " overrunsUnit=matcherDropOrSkipEvents driverUnderruns=unmeasured"
            << " appliedPacketZeroDoesNotProveBlock=true";
        baseline.valid = true;
        baseline.instanceId = snapshot->instanceId;
        baseline.roleGeneration = snapshot->roleGeneration;
        baseline.dryRuns = stats.dryRuns;
        baseline.overruns = stats.overruns;
        baseline.capturedNs = snapshot->capturedNs;
        baseline.moxObserved = snapshot->moxObserved;
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
