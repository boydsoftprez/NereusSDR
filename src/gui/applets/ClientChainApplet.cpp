// =================================================================
// src/gui/applets/ClientChainApplet.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — per-client TCI connection detail applet.
//
// This file contains no ported Thetis logic; it is a new UI surface
// built to NereusSDR design conventions (Template C, plain English
// Qt strings, AppSettings persistence, StyleConstants palette).
//
// Pattern-matches TciApplet (Phase 21, commit 0b615a7) for style
// consistency: same header, same QTimer-based refresh lifecycle,
// same StyleConstants color palette, same showEvent/hideEvent guard.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 22.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.
//                Per-client rows with TX badge, peer + User-Agent,
//                connect duration, subscription badges, last command,
//                combined drop counter, and disconnect button.
//                Empty state when server stopped or no clients.
//                1 Hz refresh with auto-refresh checkbox at top.
//                TciServer accessors clients() + activeTxAudioClient()
//                added in same commit (Phase 22).
// =================================================================

#ifdef HAVE_WEBSOCKETS

#include "ClientChainApplet.h"

#include "core/TciClientSession.h"
#include "core/TciServer.h"
#include "gui/StyleConstants.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QWebSocket>

namespace NereusSDR {

// ── File-local helpers ────────────────────────────────────────────────────────

namespace {

// Small colored badge label: fixed-height pill with rounded corners.
QLabel* makeBadge(const QString& text, const char* bg, const char* fg,
                  QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background: %1; color: %2;"
        "  font-size: 8px; font-weight: bold;"
        "  border-radius: 3px; padding: 1px 4px;"
        "}"
    ).arg(QLatin1String(bg), QLatin1String(fg)));
    lbl->setFixedHeight(14);
    return lbl;
}

// Secondary label — matches TciApplet::makeSecondaryLabel style.
QLabel* makeSecondaryLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }"
    ).arg(Style::kTextSecondary));
    return lbl;
}

// Tertiary label (dimmer).
QLabel* makeTertiaryLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 9px; }"
    ).arg(Style::kTextTertiary));
    return lbl;
}

// Horizontal divider — matches AppletWidget::divider() style.
QFrame* makeHRule(QWidget* parent)
{
    auto* f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Plain);
    f->setStyleSheet(QStringLiteral(
        "QFrame { color: %1; }"
    ).arg(Style::kBorderSubtle));
    f->setFixedHeight(1);
    return f;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ClientChainApplet::ClientChainApplet(TciServer* server, QWidget* parent)
    : AppletWidget(nullptr, parent)
    , m_server(server)
{
    // ── Root layout ───────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("TCI Clients")));

    // ── Top bar: auto-refresh checkbox + bind info ────────────────────────────
    auto* topBar = new QWidget(this);
    {
        auto* hbox = new QHBoxLayout(topBar);
        hbox->setContentsMargins(4, 2, 4, 2);
        hbox->setSpacing(4);

        m_autoRefreshCheck = new QCheckBox(QStringLiteral("Auto-refresh"), topBar);
        m_autoRefreshCheck->setChecked(true);
        m_autoRefreshCheck->setToolTip(
            QStringLiteral("Refresh the client list automatically every second"));
        m_autoRefreshCheck->setStyleSheet(QStringLiteral(
            "QCheckBox { color: %1; font-size: 10px; }"
            "QCheckBox::indicator { width: 12px; height: 12px; }"
        ).arg(Style::kTextSecondary));
        connect(m_autoRefreshCheck, &QCheckBox::toggled,
                this, &ClientChainApplet::onAutoRefreshToggled);
        hbox->addWidget(m_autoRefreshCheck);

        hbox->addStretch();

        m_bindInfoLabel = makeTertiaryLabel(QString(), topBar);
        hbox->addWidget(m_bindInfoLabel);
    }
    root->addWidget(topBar);
    root->addWidget(makeHRule(this));

    // ── Rows area (rebuilt by refresh()) ─────────────────────────────────────
    auto* rowsContainer = new QWidget(this);
    rowsContainer->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; }"
    ).arg(Style::kPanelBg));
    m_rowsLayout = new QVBoxLayout(rowsContainer);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(0);
    root->addWidget(rowsContainer, 1);

    // Empty state (built once, toggled by rebuildRows()).
    m_emptyStatePanel = new QWidget(rowsContainer);
    {
        auto* vbox = new QVBoxLayout(m_emptyStatePanel);
        vbox->setContentsMargins(8, 6, 8, 6);
        vbox->setSpacing(4);
        buildEmptyState();
    }
    m_rowsLayout->addWidget(m_emptyStatePanel);
    m_rowsLayout->addStretch();

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // ── Connect TciServer signals ─────────────────────────────────────────────
    if (m_server) {
        connect(m_server, &TciServer::clientConnected,
                this, &ClientChainApplet::onClientConnected);
        connect(m_server, &TciServer::clientDisconnected,
                this, &ClientChainApplet::onClientDisconnected);
    }

    // ── 1 Hz refresh timer ────────────────────────────────────────────────────
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout,
            this, &ClientChainApplet::refresh);

    // Initial paint.
    refresh();
}

// ── Empty state ───────────────────────────────────────────────────────────────

void ClientChainApplet::buildEmptyState()
{
    auto* vbox = qobject_cast<QVBoxLayout*>(m_emptyStatePanel->layout());
    if (!vbox) {
        return;
    }

    auto* hintLbl = makeSecondaryLabel(
        QStringLiteral("No TCI clients connected."), m_emptyStatePanel);
    hintLbl->setWordWrap(true);
    vbox->addWidget(hintLbl);

    auto* subHintLbl = makeTertiaryLabel(
        QStringLiteral("Connect an SDR application that supports the ExpertSDR3 TCI protocol."),
        m_emptyStatePanel);
    subHintLbl->setWordWrap(true);
    vbox->addWidget(subHintLbl);
}

// ── Row builder ───────────────────────────────────────────────────────────────

QWidget* ClientChainApplet::buildClientRow(
    QWebSocket* socket,
    const std::shared_ptr<TciClientSession>& session)
{
    auto* row = new QWidget(this);
    row->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 1px solid %2; }"
    ).arg(Style::kPanelBg, Style::kBorderSubtle));

    auto* vbox = new QVBoxLayout(row);
    vbox->setContentsMargins(6, 4, 6, 4);
    vbox->setSpacing(2);

    // ── Top line: TX badge | peer | name | duration ───────────────────────────
    {
        auto* hbox = new QHBoxLayout;
        hbox->setSpacing(4);

        // TX badge — orange when this socket holds the TX audio mutex.
        const bool isTxActive = (m_server && socket == m_server->activeTxAudioClient());
        if (isTxActive) {
            hbox->addWidget(makeBadge(QStringLiteral("TX"), "#804000", "#ffb800", row));
        } else {
            // Transparent spacer to keep peer label aligned.
            auto* spacer = new QLabel(row);
            spacer->setFixedSize(28, 14);
            hbox->addWidget(spacer);
        }

        // Peer: "ip:port"
        auto* peerLbl = new QLabel(session->peer.isEmpty()
                                       ? QStringLiteral("(unknown peer)")
                                       : session->peer,
                                   row);
        peerLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: bold; }"
        ).arg(Style::kTextPrimary));
        peerLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        hbox->addWidget(peerLbl);

        // Name: User-Agent (best-effort)
        auto* nameLbl = makeSecondaryLabel(
            QStringLiteral("(%1)").arg(session->userAgent), row);
        nameLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        hbox->addWidget(nameLbl);

        hbox->addStretch();

        // Connection duration
        auto* durLbl = makeTertiaryLabel(
            formatDuration(session->connectedAt.elapsed()), row);
        hbox->addWidget(durLbl);

        vbox->addLayout(hbox);
    }

    // ── Middle line: subscription badges ─────────────────────────────────────
    {
        auto* hbox = new QHBoxLayout;
        hbox->setSpacing(3);

        // IQ subscription badges (one per subscribed slice).
        for (const int rx : std::as_const(session->iqStreamEnabled)) {
            hbox->addWidget(makeBadge(
                QStringLiteral("IQ %1").arg(rx),
                "#004060", "#00b4d8", row));
        }

        // Audio subscription badges (one per subscribed slice).
        for (const int rx : std::as_const(session->audioStreamEnabled)) {
            hbox->addWidget(makeBadge(
                QStringLiteral("Audio %1").arg(rx),
                "#006040", "#00ff88", row));
        }

        // TX direction badge (orange when active TX client).
        if (m_server && socket == m_server->activeTxAudioClient()) {
            hbox->addWidget(makeBadge(
                QStringLiteral("TX audio"), "#804000", "#ffb800", row));
        }

        if (session->iqStreamEnabled.isEmpty()
            && session->audioStreamEnabled.isEmpty()
            && !(m_server && socket == m_server->activeTxAudioClient())) {
            hbox->addWidget(makeTertiaryLabel(
                QStringLiteral("no subscriptions"), row));
        }

        hbox->addStretch();

        // RX sensors badge
        if (session->rxSensorsEnabled) {
            hbox->addWidget(makeBadge(
                QStringLiteral("RX sens"), "#003060", "#60b8e0", row));
        }
        // TX sensors badge
        if (session->txSensorsEnabled) {
            hbox->addWidget(makeBadge(
                QStringLiteral("TX sens"), "#402000", "#c08040", row));
        }

        vbox->addLayout(hbox);
    }

    // ── Bottom line: last command | drop counter | disconnect button ──────────
    {
        auto* hbox = new QHBoxLayout;
        hbox->setSpacing(6);

        // Last command in monospace, truncated to 40 chars.
        const QString cmdSnippet = session->lastCommand.isEmpty()
                                       ? QStringLiteral("(no command yet)")
                                       : session->lastCommand.left(40);
        auto* cmdLbl = new QLabel(cmdSnippet, row);
        const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        cmdLbl->setFont(monoFont);
        cmdLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px; }"
        ).arg(Style::kTextTertiary));
        cmdLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        hbox->addWidget(cmdLbl, 1);

        // Last command age
        if (session->lastCommandAt > 0) {
            auto* ageLbl = makeTertiaryLabel(
                formatLastCommandAge(session->lastCommandAt), row);
            hbox->addWidget(ageLbl);
        }

        hbox->addStretch();

        // Drop counter — combined outbound + TX drops shown in red.
        const int drops = session->sendQueue.dropCount() + session->txFramesDropped;
        if (drops > 0) {
            auto* dropLbl = new QLabel(
                QStringLiteral("%1 dropped").arg(drops), row);
            dropLbl->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
            ).arg(Style::kRedBorder));
            hbox->addWidget(dropLbl);
        }

        // Disconnect button — stores the QWebSocket* in a property so the
        // slot can retrieve it without a lambda capture.
        auto* disconnectBtn = new QPushButton(QStringLiteral("Disconnect"), row);
        disconnectBtn->setFixedHeight(18);
        disconnectBtn->setToolTip(
            QStringLiteral("Force-disconnect this TCI client"));
        disconnectBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1; border: 1px solid %2; border-radius: 3px;"
            "  color: %3; font-size: 9px; font-weight: bold; padding: 1px 4px;"
            "}"
            "QPushButton:hover { background: %4; }"
        ).arg(Style::kButtonBg, Style::kRedBorder, Style::kRedBorder,
              "#3a0a0a"));
        disconnectBtn->setProperty("ws", QVariant::fromValue(socket));
        connect(disconnectBtn, &QPushButton::clicked,
                this, &ClientChainApplet::onDisconnectClicked);
        hbox->addWidget(disconnectBtn);

        vbox->addLayout(hbox);
    }

    return row;
}

// ── Rebuild rows ──────────────────────────────────────────────────────────────

void ClientChainApplet::rebuildRows()
{
    if (!m_rowsLayout) {
        return;
    }

    // Remove all items except m_emptyStatePanel and the trailing stretch.
    // Iterate in reverse so indices stay valid while we remove.
    for (int i = m_rowsLayout->count() - 1; i >= 0; --i) {
        QLayoutItem* item = m_rowsLayout->itemAt(i);
        if (!item) {
            continue;
        }
        QWidget* w = item->widget();
        if (!w || w == m_emptyStatePanel) {
            continue;
        }
        m_rowsLayout->removeItem(item);
        delete w;
        delete item;
    }

    // Update bind-info label.
    if (m_bindInfoLabel) {
        if (m_server && m_server->isRunning()) {
            m_bindInfoLabel->setText(
                QStringLiteral("127.0.0.1:%1").arg(m_server->port()));
        } else {
            m_bindInfoLabel->setText(QStringLiteral("Server not running"));
        }
    }

    if (!m_server || !m_server->isRunning()) {
        if (m_emptyStatePanel) {
            m_emptyStatePanel->setVisible(true);
        }
        return;
    }

    const auto snapshot = m_server->clients();
    const bool hasClients = !snapshot.isEmpty();

    if (m_emptyStatePanel) {
        m_emptyStatePanel->setVisible(!hasClients);
    }

    if (hasClients) {
        // Insert rows before the stretch (last item).
        // Find the stretch's index (the trailing addStretch item).
        int stretchIdx = m_rowsLayout->count() - 1;

        for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
            QWebSocket* socket = it.key();
            const std::shared_ptr<TciClientSession>& session = it.value();
            if (!socket || !session) {
                continue;
            }
            QWidget* rowWidget = buildClientRow(socket, session);
            m_rowsLayout->insertWidget(stretchIdx, rowWidget);
            ++stretchIdx;
        }
    }
}

// ── AppletWidget overrides ────────────────────────────────────────────────────

void ClientChainApplet::syncFromModel()
{
    refresh();
}

void ClientChainApplet::showEvent(QShowEvent* ev)
{
    AppletWidget::showEvent(ev);
    // Trigger an immediate refresh then start the timer.
    refresh();
    if (m_refreshTimer && m_autoRefreshCheck && m_autoRefreshCheck->isChecked()) {
        m_refreshTimer->start();
    }
}

void ClientChainApplet::hideEvent(QHideEvent* ev)
{
    AppletWidget::hideEvent(ev);
    if (m_refreshTimer) {
        m_refreshTimer->stop();
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ClientChainApplet::refresh()
{
    rebuildRows();
}

void ClientChainApplet::onAutoRefreshToggled(bool on)
{
    if (!m_refreshTimer) {
        return;
    }
    if (on) {
        m_refreshTimer->start(1000);
    } else {
        m_refreshTimer->stop();
    }
}

void ClientChainApplet::onClientConnected(QWebSocket* socket)
{
    Q_UNUSED(socket)
    // Trigger an immediate rebuild so the new client appears without waiting
    // for the next 1 Hz tick.
    refresh();
}

void ClientChainApplet::onClientDisconnected(QWebSocket* socket)
{
    Q_UNUSED(socket)
    refresh();
}

void ClientChainApplet::onDisconnectClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }
    auto* ws = btn->property("ws").value<QWebSocket*>();
    if (ws) {
        ws->close();
    }
}

// ── Formatters ────────────────────────────────────────────────────────────────

QString ClientChainApplet::formatDuration(qint64 elapsedMs) const
{
    if (elapsedMs < 0) {
        elapsedMs = 0;
    }
    const qint64 totalSeconds = elapsedMs / 1000;
    const qint64 hours   = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
    }
    if (minutes > 0) {
        return QStringLiteral("%1m %2s").arg(minutes).arg(seconds);
    }
    return QStringLiteral("%1s").arg(seconds);
}

QString ClientChainApplet::formatLastCommandAge(qint64 lastCommandAtMs) const
{
    const qint64 nowMs  = QDateTime::currentMSecsSinceEpoch();
    const qint64 ageMs  = nowMs - lastCommandAtMs;
    if (ageMs < 2000) {
        return QStringLiteral("just now");
    }
    const qint64 ageSec = ageMs / 1000;
    if (ageSec < 60) {
        return QStringLiteral("%1s ago").arg(ageSec);
    }
    if (ageSec < 3600) {
        return QStringLiteral("%1m ago").arg(ageSec / 60);
    }
    return QStringLiteral("%1h ago").arg(ageSec / 3600);
}

} // namespace NereusSDR

#endif // HAVE_WEBSOCKETS
