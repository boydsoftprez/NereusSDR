// =================================================================
// src/gui/setup/FourO3ADiagnosticsTab.cpp  (NereusSDR)
// =================================================================
//
// See FourO3ADiagnosticsTab.h for the design overview.  Implementation
// keeps the four sections in fixed-size vertical layout so the
// diagnostics view stays compact; the recent-command log and
// disconnect log can scroll independently inside their QPlainTextEdit
// widgets if the rings grow longer than the visible area.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-21 -- Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include "FourO3ADiagnosticsTab.h"

#include "core/PgxlConnection.h"
#include "core/SmartSdrApiListener.h"
#include "core/TgxlConnection.h"
#include "models/RadioModel.h"

#include <QDateTime>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

FourO3ADiagnosticsTab::FourO3ADiagnosticsTab(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    // ── Section 1: Connection State ───────────────────────────────
    auto* stateBox = new QGroupBox(tr("Connection State"), this);
    auto* stateLayout = new QVBoxLayout(stateBox);
    m_connectionStateLabel = new QLabel(tr("(awaiting first refresh)"), stateBox);
    m_connectionStateLabel->setStyleSheet(
        QStringLiteral("font-family: monospace; font-size: 11px;"));
    m_connectionStateLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    stateLayout->addWidget(m_connectionStateLabel);
    root->addWidget(stateBox);

    // ── Section 2: Recent FlexAPI Commands ────────────────────────
    auto* cmdBox = new QGroupBox(
        tr("Recent FlexAPI Commands (last %1)").arg(kMaxCommandLines),
        this);
    auto* cmdLayout = new QVBoxLayout(cmdBox);
    m_commandLog = new QPlainTextEdit(cmdBox);
    m_commandLog->setReadOnly(true);
    m_commandLog->setMaximumBlockCount(kMaxCommandLines);
    m_commandLog->setStyleSheet(
        QStringLiteral("font-family: monospace; font-size: 11px;"));
    m_commandLog->setPlaceholderText(
        tr("No FlexAPI commands yet -- waiting for PGXL/TGXL traffic."));
    cmdLayout->addWidget(m_commandLog);
    root->addWidget(cmdBox, /*stretch=*/1);

    // ── Section 3: Fault History ──────────────────────────────────
    auto* faultBox = new QGroupBox(tr("Fault History (all amps)"), this);
    auto* faultLayout = new QVBoxLayout(faultBox);
    m_faultHistory = new QPlainTextEdit(faultBox);
    m_faultHistory->setReadOnly(true);
    m_faultHistory->setStyleSheet(
        QStringLiteral("font-family: monospace; font-size: 11px;"));
    m_faultHistory->setPlaceholderText(tr("No faults logged yet."));
    m_faultHistory->setFixedHeight(80);
    faultLayout->addWidget(m_faultHistory);
    root->addWidget(faultBox);

    // ── Section 4: Disconnect / Reconnect Log ─────────────────────
    auto* dropBox = new QGroupBox(tr("Disconnect / Reconnect Log"), this);
    auto* dropLayout = new QVBoxLayout(dropBox);
    m_disconnectLog = new QPlainTextEdit(dropBox);
    m_disconnectLog->setReadOnly(true);
    m_disconnectLog->setStyleSheet(
        QStringLiteral("font-family: monospace; font-size: 11px;"));
    m_disconnectLog->setPlaceholderText(tr("No disconnect events yet."));
    m_disconnectLog->setFixedHeight(80);
    dropLayout->addWidget(m_disconnectLog);
    root->addWidget(dropBox);

    // Wire connection-event signals so the disconnect log captures
    // PGXL/TGXL TCP state changes live.  Both connection objects emit
    // stateChanged with the human-readable target state; we tag the
    // source so the operator can tell which peer dropped.
    if (m_model) {
        if (auto* pgxl = m_model->pgxlConnection()) {
            connect(pgxl, &PgxlConnection::connected, this, [this]() {
                appendConnectionEvent(QStringLiteral("PGXL: connected"));
            });
            connect(pgxl, &PgxlConnection::disconnected, this, [this]() {
                appendConnectionEvent(QStringLiteral("PGXL: disconnected"));
            });
        }
        if (auto* tgxl = m_model->tgxlConnection()) {
            connect(tgxl, &TgxlConnection::connected, this, [this]() {
                appendConnectionEvent(QStringLiteral("TGXL: connected"));
            });
            connect(tgxl, &TgxlConnection::disconnected, this, [this]() {
                appendConnectionEvent(QStringLiteral("TGXL: disconnected"));
            });
        }
    }

    // Connection-state tick (1 Hz).  Light enough to run while the
    // tab is visible; QTimer's auto-disable on widget hide is fine
    // because we're refreshing a label, not pulling heavy data.
    auto* tick = new QTimer(this);
    tick->setInterval(1000);
    connect(tick, &QTimer::timeout,
            this, &FourO3ADiagnosticsTab::refreshConnectionState);
    tick->start();
    refreshConnectionState();  // initial paint
}

void FourO3ADiagnosticsTab::refreshConnectionState()
{
    if (!m_connectionStateLabel) { return; }

    QStringList lines;
    SmartSdrApiListener* listener =
        m_model ? m_model->smartSdrListener() : nullptr;
    const bool listening = listener && listener->isListening();
    const bool gateOn = m_model && m_model->fourO3AEnabled();

    lines << tr("Master gate:        %1")
                 .arg(gateOn ? tr("ENABLED") : tr("disabled"));
    lines << tr("FlexAPI listener:   %1")
                 .arg(listening ? tr("Listening on TCP 4992")
                               : (gateOn ? tr("bind failed")
                                         : tr("not started")));

    // Peer state.  We only report bound / not bound here; the
    // PGXL/TGXL detail tabs show the per-peer telemetry.
    auto* pgxl = m_model ? m_model->pgxlConnection() : nullptr;
    auto* tgxl = m_model ? m_model->tgxlConnection() : nullptr;
    lines << tr("PowerGenius XL:     %1")
                 .arg(pgxl && pgxl->isConnected() ? tr("connected")
                                                  : tr("not connected"));
    lines << tr("Tuner Genius XL:    %1")
                 .arg(tgxl && tgxl->isConnected() ? tr("connected")
                                                  : tr("not connected"));

    m_connectionStateLabel->setText(lines.join('\n'));
}

void FourO3ADiagnosticsTab::appendCommand(const QString& line)
{
    if (!m_commandLog) { return; }
    const QString stamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_commandLog->appendPlainText(stamp + "  " + line);
}

void FourO3ADiagnosticsTab::appendConnectionEvent(const QString& line)
{
    if (!m_disconnectLog) { return; }
    const QString stamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_disconnectLog->appendPlainText(stamp + "  " + line);
    m_disconnectLogEmpty = false;
}

}  // namespace NereusSDR
