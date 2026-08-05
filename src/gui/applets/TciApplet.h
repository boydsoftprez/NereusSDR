// =================================================================
// src/gui/applets/TciApplet.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original — TCI status applet for Container #0 stack.
//
// This file contains no ported Thetis logic; it is a new UI surface
// built to NereusSDR design conventions (Template C, plain English
// Qt strings, AppSettings persistence, StyleConstants palette).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 21.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.
//                Header row (status dot + label + port + client count
//                + Setup button), Slice A row (level meter + gain
//                slider), TX row (level meter + gain slider), footer
//                (client count + Show clients link).  Phase 21 STUB:
//                level meters use placeholder values; real meter wiring
//                + setup/show-clients navigation in Phase 23.
//                AppSettings keys: TciSliceAGain, TciTxGain.
// =================================================================

#pragma once

#ifdef HAVE_WEBSOCKETS

#include "AppletWidget.h"

class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTimer;
class QWebSocket;

namespace NereusSDR {

class HGauge;
class TciServer;

// TciApplet — operator-facing TCI status applet.
//
// Layout (per design doc §2.3 + §8.2):
//   Header row : status dot + "TCI Server" label + port display +
//                client count + Setup button
//   Slice A row: "Slice A" label + RX audio level meter + gain slider
//   TX row     : "TX" label + TX audio level meter + gain slider
//   Footer     : client count + "Show clients" link button
//   Disabled   : collapses to a single "Enable Server" button + hint
//
// Phase 21 stubs:
//   - Level meters display placeholder (0 when stopped; small mock
//     peak when running). Real wiring deferred to Phase 24+.
//   - Setup button logs only; Phase 23 wires the navigation.
//   - Show clients button logs only; Phase 22 wires navigation.
class TciApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit TciApplet(TciServer* server, QWidget* parent = nullptr);
    ~TciApplet() override = default;

    QString appletId()    const override { return QStringLiteral("tci"); }
    QString appletTitle() const override { return QStringLiteral("TCI Server"); }
    void    syncFromModel() override;

signals:
    // Emitted when the user clicks the Setup button.
    // Phase 23 wires this to open Setup -> Network -> TCI Server.
    void setupRequested();

    // Emitted when the user clicks "Show clients ->".
    // Phase 22 wires this to navigate to the ClientChainApplet.
    void showClientsRequested();

public slots:
    // Periodic refresh (200 ms timer): polls TciServer for level updates.
    void refresh();

private slots:
    void onServerStarted(quint16 port);
    void onServerStopped();
    void onClientConnected(QWebSocket* socket);
    void onClientDisconnected(QWebSocket* socket);
    void onEnableToggled(bool on);
    void onSetupClicked();
    void onShowClientsClicked();
    void onSliceAGainChanged(int dB);
    void onTxGainChanged(int dB);

private:
    void buildUI();
    void buildHeaderRow(QVBoxLayout* vbox);
    void buildSliceRow(QVBoxLayout* vbox);
    void buildTxRow(QVBoxLayout* vbox);
    void buildFooter(QVBoxLayout* vbox);
    void buildDisabledState(QVBoxLayout* vbox);

    // Show/hide the main content vs the disabled-state placeholder.
    void applyEnabledState(bool serverRunning);

    // Update the status dot color + port label + client count labels.
    void updateStatusWidgets();

    TciServer*   m_server{nullptr};
    QTimer*      m_refreshTimer{nullptr};

    // ── Main content widgets (hidden when server is stopped) ─────────────────
    QWidget*     m_mainContent{nullptr};

    // Header row
    QLabel*      m_statusDot{nullptr};
    QLabel*      m_portLabel{nullptr};
    QLabel*      m_clientCountLabel{nullptr};
    QPushButton* m_setupButton{nullptr};

    // Slice A row
    HGauge*      m_sliceAGauge{nullptr};
    QSlider*     m_sliceAGain{nullptr};
    QLabel*      m_sliceAGainLabel{nullptr};

    // TX row
    HGauge*      m_txGauge{nullptr};
    QSlider*     m_txGain{nullptr};
    QLabel*      m_txGainLabel{nullptr};

    // Footer
    QLabel*      m_footerCount{nullptr};
    QPushButton* m_showClientsLink{nullptr};

    // ── Disabled-state placeholder (shown when server is stopped) ────────────
    QWidget*     m_disabledContent{nullptr};
    QPushButton* m_enableBtn{nullptr};

    // Phase 21 placeholder: small mock level counter for visual demo.
    int m_mockLevelPhase{0};
};

} // namespace NereusSDR

#endif // HAVE_WEBSOCKETS
