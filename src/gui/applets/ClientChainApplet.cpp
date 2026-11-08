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
//   2026-09-27 - Parity Task 23 control UI by J.J. Boyd (KG4VCF);
//                AI-assisted implementation via OpenAI Codex. The Core's
//                client stream has a labelled list and Disconnect control.
//   2026-10-09 - A client row names its own style rule, so its bottom line
//                no longer repeats under every label and badge in it.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-07 - The Core's client rows are rebuilt only when what they
//                show changes, so the box no longer resizes every second.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-07 - This computer's client rows too: while the same apps are
//                connected in the same order, a refresh updates their
//                text (connected time, last command) in place.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#ifdef HAVE_WEBSOCKETS

#include "ClientChainApplet.h"

#include "core/TciClientSession.h"
#include "core/TciServer.h"
#include "core/session/IStationLink.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/StationTciModel.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QScopedValueRollback>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QWebSocket>

#include <algorithm>
#include <utility>

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
    // Do NOT add appletTitleBar() here — AppletPanelWidget::wrapWithTitleBar
    // already prepends a host-side title bar from appletTitle(). Adding our
    // own here results in a double header. Same fix in PureSignalApplet.

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

    m_corePanel = new QWidget(this);
    m_corePanel->setObjectName(QStringLiteral("tciCoreClients"));
    m_coreRows = new QVBoxLayout(m_corePanel);
    m_coreRows->setContentsMargins(4, 4, 4, 4);
    m_coreRows->setSpacing(3);
    root->addWidget(m_corePanel);
    m_corePanel->hide();

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

void ClientChainApplet::setStationModel(RadioModel* model)
{
    m_stationModel = model;
    if (model && model->stationTciModel()) {
        connect(model->stationTciModel(), &StationTciModel::clientsChanged,
                this, &ClientChainApplet::rebuildCoreRows);
        connect(model, &RadioModel::stationLinkStateChanged,
                this, &ClientChainApplet::rebuildCoreRows);
        connect(model, &RadioModel::coreOnAirChanged,
                this, &ClientChainApplet::rebuildCoreRows);
    }
    rebuildCoreRows();
}

void ClientChainApplet::rebuildCoreRows()
{
    if (!m_coreRows) {
        return;
    }
    IStationLink* link = m_stationModel ? m_stationModel->stationLink() : nullptr;
    CoreRowsShown next;
    next.panelShown = link && link->stationTciAvailable();
    next.serverAvailable = next.panelShown && link->stationTciServerAvailable();
    if (next.serverAvailable) {
        next.onAir = m_stationModel->isCoreOnAir();
        next.clients = m_stationModel->stationTciModel()->clients();
    }
    // Nothing the rows show has changed: leave every widget as it is.
    if (m_coreRowsShown && *m_coreRowsShown == next) {
        return;
    }
    m_coreRowsShown = next;
    while (QLayoutItem* item = m_coreRows->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    if (!next.panelShown) {
        m_corePanel->hide();
        return;
    }
    m_corePanel->show();
    auto* heading = makeSecondaryLabel(QStringLiteral("The Core's TCI server"), m_corePanel);
    m_coreRows->addWidget(heading);
    if (!next.serverAvailable) {
        auto* reason = makeTertiaryLabel(IStationLink::stationTciServerUnavailableReason(),
                                        m_corePanel);
        reason->setWordWrap(true);
        m_coreRows->addWidget(reason);
        return;
    }
    const QList<StationTciClient>& clients = next.clients;
    if (clients.isEmpty()) {
        m_coreRows->addWidget(makeTertiaryLabel(
            QStringLiteral("No apps connected to the Core's TCI server."), m_corePanel));
    }
    for (const StationTciClient& client : clients) {
        auto* row = new QWidget(m_corePanel);
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        const QString name = client.name.isEmpty() ? QStringLiteral("App") : client.name;
        auto* label = makeSecondaryLabel(
            QStringLiteral("%1 (%2)%3: %4; %5")
                .arg(name, client.address,
                     client.transmitting ? QStringLiteral(" TX") : QString(),
                     client.subscriptions.isEmpty() ? QStringLiteral("no subscriptions")
                                                     : client.subscriptions.join(QStringLiteral(", ")),
                     client.lastCommand.isEmpty() ? QStringLiteral("no command")
                                                   : client.lastCommand.left(40)), row);
        label->setWordWrap(true);
        line->addWidget(label, 1);
        auto* disconnect = new QPushButton(QStringLiteral("Disconnect"), row);
        disconnect->setObjectName(QStringLiteral("disconnectCoreTciClient"));
        const bool onAir = next.onAir;
        disconnect->setEnabled(!onAir);
        if (onAir) {
            disconnect->setToolTip(QStringLiteral("The radio is on the air. Try again when it stops."));
        }
        connect(disconnect, &QPushButton::clicked, this, [this, id = client.id] {
            IStationLink* current = m_stationModel ? m_stationModel->stationLink() : nullptr;
            if (current) {
                current->requestDisconnectStationTciClient(id);
            }
        });
        line->addWidget(disconnect);
        m_coreRows->addWidget(row);
    }
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
    // Named so the bottom line stays on the row: an unnamed QWidget rule
    // reached every label and badge in it and drew the line under each.
    row->setObjectName(QStringLiteral("tciClientRow"));
    row->setStyleSheet(QStringLiteral(
        "QWidget#tciClientRow { background: %1; border-bottom: 1px solid %2; }"
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

        // Peer: "ip:port" (text set by updateClientRowText()).
        auto* peerLbl = new QLabel(row);
        peerLbl->setObjectName(QStringLiteral("tciClientPeer"));
        peerLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: bold; }"
        ).arg(Style::kTextPrimary));
        peerLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        hbox->addWidget(peerLbl);

        // Name: User-Agent (best-effort)
        auto* nameLbl = makeSecondaryLabel(QString(), row);
        nameLbl->setObjectName(QStringLiteral("tciClientName"));
        nameLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        hbox->addWidget(nameLbl);

        hbox->addStretch();

        // Connection duration. One line, never wrapped: the counter grows
        // every second and must not change the row's height.
        auto* durLbl = makeTertiaryLabel(QString(), row);
        durLbl->setObjectName(QStringLiteral("tciClientDuration"));
        durLbl->setWordWrap(false);
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
        auto* cmdLbl = new QLabel(row);
        cmdLbl->setObjectName(QStringLiteral("tciClientCommand"));
        const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        cmdLbl->setFont(monoFont);
        cmdLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px; }"
        ).arg(Style::kTextTertiary));
        cmdLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        hbox->addWidget(cmdLbl, 1);

        // Last command age
        if (session->lastCommandAt > 0) {
            auto* ageLbl = makeTertiaryLabel(QString(), row);
            ageLbl->setObjectName(QStringLiteral("tciClientCommandAge"));
            hbox->addWidget(ageLbl);
        }

        hbox->addStretch();

        // Drop counter — combined outbound + TX drops shown in red.
        const int drops = session->sendQueue.dropCount() + session->txFramesDropped;
        if (drops > 0) {
            auto* dropLbl = new QLabel(row);
            dropLbl->setObjectName(QStringLiteral("tciClientDrops"));
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

    updateClientRowText(row, session);
    return row;
}

// The parts of a row that are separate widgets. While these stay the same a
// refresh only changes the row's text.
ClientChainApplet::LocalRowShape ClientChainApplet::shapeOf(
    QWebSocket* socket,
    const std::shared_ptr<TciClientSession>& session) const
{
    LocalRowShape shape;
    shape.socket = socket;
    shape.tx = m_server && socket == m_server->activeTxAudioClient();
    shape.iq = session->iqStreamEnabled.values();
    std::sort(shape.iq.begin(), shape.iq.end());
    shape.audio = session->audioStreamEnabled.values();
    std::sort(shape.audio.begin(), shape.audio.end());
    shape.rxSensors = session->rxSensorsEnabled;
    shape.txSensors = session->txSensorsEnabled;
    shape.hasCommandAge = session->lastCommandAt > 0;
    shape.hasDrops = (session->sendQueue.dropCount() + session->txFramesDropped) > 0;
    return shape;
}

// Sets every changing text in a row built by buildClientRow(). Used when the
// row is built and on each refresh that keeps it.
void ClientChainApplet::updateClientRowText(
    QWidget* row,
    const std::shared_ptr<TciClientSession>& session) const
{
    if (auto* peerLbl = row->findChild<QLabel*>(QStringLiteral("tciClientPeer"))) {
        peerLbl->setText(session->peer.isEmpty() ? QStringLiteral("(unknown peer)")
                                                 : session->peer);
    }
    if (auto* nameLbl = row->findChild<QLabel*>(QStringLiteral("tciClientName"))) {
        nameLbl->setText(QStringLiteral("(%1)").arg(session->userAgent));
    }
    if (auto* durLbl = row->findChild<QLabel*>(QStringLiteral("tciClientDuration"))) {
        durLbl->setText(formatDuration(session->connectedAt.elapsed()));
    }
    if (auto* cmdLbl = row->findChild<QLabel*>(QStringLiteral("tciClientCommand"))) {
        cmdLbl->setText(session->lastCommand.isEmpty() ? QStringLiteral("(no command)")
                                                       : session->lastCommand.left(40));
    }
    if (auto* ageLbl = row->findChild<QLabel*>(QStringLiteral("tciClientCommandAge"))) {
        ageLbl->setText(formatLastCommandAge(session->lastCommandAt));
    }
    if (auto* dropLbl = row->findChild<QLabel*>(QStringLiteral("tciClientDrops"))) {
        const int drops = session->sendQueue.dropCount() + session->txFramesDropped;
        dropLbl->setText(QStringLiteral("%1 dropped").arg(drops));
    }
}

// ── Rebuild rows ──────────────────────────────────────────────────────────────

void ClientChainApplet::rebuildRows()
{
    if (!m_rowsLayout) {
        return;
    }

    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): re-entrancy guard.  The 1Hz
    // refresh timer used to race with widget destruction — if a second tick
    // fires while the previous rebuild is still tearing down QLabels, the
    // second teardown reads a half-destroyed QTextDocument's HarfBuzz
    // buffer and SIGSEGVs in hb_buffer_destroy.  Crash report:
    //   /Users/j.j.boyd/Library/Logs/DiagnosticReports/
    //     NereusSDR-2026-05-23-130427.ips
    if (m_rebuildInProgress) {
        return;
    }
    QScopedValueRollback<bool> guard(m_rebuildInProgress, true);

    // Update bind-info label.
    if (m_bindInfoLabel) {
        if (m_server && m_server->isRunning()) {
            m_bindInfoLabel->setText(
                QStringLiteral("127.0.0.1:%1").arg(m_server->port()));
        } else {
            m_bindInfoLabel->setText(QStringLiteral("Server not running"));
        }
    }

    const bool running = m_server && m_server->isRunning();
    QList<std::pair<QWebSocket*, std::shared_ptr<TciClientSession>>> entries;
    QList<LocalRowShape> shapes;
    if (running) {
        const auto snapshot = m_server->clients();
        for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
            if (!it.key() || !it.value()) {
                continue;
            }
            entries.append({it.key(), it.value()});
            shapes.append(shapeOf(it.key(), it.value()));
        }
    }

    // The same apps in the same order with the same badges: only text can
    // have changed, so update it in place. Deleting and re-adding the rows
    // made the box short for one event-loop pass and the column below it
    // jumped once a second.
    const bool rowsStillThere = std::all_of(
        m_localRows.cbegin(), m_localRows.cend(),
        [](const QPointer<QWidget>& row) { return !row.isNull(); });
    if (shapes == m_localRowShapes && rowsStillThere) {
        for (int i = 0; i < entries.size(); ++i) {
            updateClientRowText(m_localRows.at(i), entries.at(i).second);
        }
        if (m_emptyStatePanel) {
            m_emptyStatePanel->setVisible(entries.isEmpty());
        }
        return;
    }
    m_localRowShapes = shapes;
    m_localRows.clear();

    // Remove all items except m_emptyStatePanel and the trailing stretch.
    // Iterate in reverse so indices stay valid while we remove.
    //
    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): switch from synchronous
    // `delete w` to `w->deleteLater()`.  Synchronous delete inside a timer
    // slot tears down child QTextDocument / QTextLayout / HarfBuzz buffers
    // while the same widget's text-shaping pipeline may still be in flight
    // from a prior style-sheet / tooltip / focus update — the QLabel
    // destructor calls QWidgetTextControl::~QWidgetTextControl() which
    // walks the QTextDocumentPrivate fragment map and frees HarfBuzz
    // buffers that another event-loop slot is concurrently shaping.
    // deleteLater() defers the destruction to the next event-loop
    // iteration when text shaping is quiescent.
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
        // Hide immediately so the user doesn't see a half-stale row until
        // the deferred delete runs.  Detach from parent so the layout
        // doesn't see the widget in subsequent insertWidget() calls.
        w->setParent(nullptr);
        w->hide();
        w->deleteLater();
        delete item;
    }

    if (!running) {
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

        for (const auto& [socket, session] : std::as_const(entries)) {
            QWidget* rowWidget = buildClientRow(socket, session);
            m_rowsLayout->insertWidget(stretchIdx, rowWidget);
            m_localRows.append(rowWidget);
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
    rebuildCoreRows();
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
