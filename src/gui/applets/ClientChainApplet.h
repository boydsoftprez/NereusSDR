// =================================================================
// src/gui/applets/ClientChainApplet.h  (NereusSDR)
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

#pragma once

#ifdef HAVE_WEBSOCKETS

#include "AppletWidget.h"

#include <memory>

class QCheckBox;
class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;
class QWebSocket;

namespace NereusSDR {

struct TciClientSession;
class TciServer;

// ClientChainApplet — per-client TCI connection detail applet.
//
// Layout (per design doc §2.4 + §8.3):
//   Top bar : auto-refresh checkbox + bind info label
//   Per-client row (one per connected client):
//     TX badge | peer | name (User-Agent) | duration
//     subscription badges (IQ + Audio per slice, TX direction)
//     last command snippet | drop counter | disconnect button
//   Empty state (shown when server stopped or no clients connected):
//     bind address + connection hint
//
// Refresh cadence: 1 Hz (1000 ms QTimer) per design doc §8.3.
// Auto-refresh checkbox pauses polling without destroying the widget.
//
// Drop counter: combined sendQueue.dropCount() (outbound backpressure)
// + txFramesDropped (inbound TX without mutex) shown as "N dropped"
// in red text.  Can be split per-direction in a later phase.
class ClientChainApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit ClientChainApplet(TciServer* server, QWidget* parent = nullptr);
    ~ClientChainApplet() override = default;

    QString appletId()    const override { return QStringLiteral("tci_clients"); }
    QString appletTitle() const override { return QStringLiteral("TCI Clients"); }
    void    syncFromModel() override;

protected:
    void showEvent(QShowEvent* ev) override;
    void hideEvent(QHideEvent* ev) override;

private slots:
    void refresh();
    void onAutoRefreshToggled(bool on);
    void onClientConnected(QWebSocket* socket);
    void onClientDisconnected(QWebSocket* socket);
    void onDisconnectClicked();  // sender()->property("ws") = the row's QWebSocket*

private:
    // Tear down current client rows and reconstruct from m_server->clients()
    // snapshot.  Full rebuild per tick is acceptable for 1-3 clients.
    void rebuildRows();

    // Build the empty-state panel (server stopped or no clients).
    void buildEmptyState();

    // Build one row widget for the given socket + session pair.
    QWidget* buildClientRow(QWebSocket* socket,
                            const std::shared_ptr<TciClientSession>& session);

    // Format elapsed milliseconds as "Xh Ym Zs" or "Ym Zs" or "Zs".
    QString formatDuration(qint64 elapsedMs) const;

    // Format the age of lastCommandAt (ms epoch) as "Xs ago" or "just now".
    QString formatLastCommandAge(qint64 lastCommandAtMs) const;

    TciServer*   m_server{nullptr};
    QTimer*      m_refreshTimer{nullptr};

    // Top bar
    QCheckBox*   m_autoRefreshCheck{nullptr};
    QLabel*      m_bindInfoLabel{nullptr};

    // Scrollable rows area — rebuilt on every refresh tick.
    QVBoxLayout* m_rowsLayout{nullptr};

    // Shown when server is stopped or there are no connected clients.
    QWidget*     m_emptyStatePanel{nullptr};
};

} // namespace NereusSDR

#endif // HAVE_WEBSOCKETS
