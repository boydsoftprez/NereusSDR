#pragma once

// =================================================================
// src/gui/setup/FourO3ADiagnosticsTab.h  (NereusSDR)
// =================================================================
//
// Diagnostics tab for the 4O3A integration page (Setup -> CAT &
// Network -> 4O3A -> Diagnostics).
//
// Shows live state of the SmartSDR API listener and its peers:
//   1. Connection state  -- listener bind status, peer list,
//                           subscribed-amp interlock state, uptime.
//   2. Disconnect log    -- PGXL/TGXL TCP open/close events so the
//                           operator can spot flaky LAN.
//
// 2026-05-22 menu cleanup: the Recent FlexAPI Commands + Fault
// History panels were removed (no producer wired). Per-amp fault
// histories live in their detail tabs.
//
// NereusSDR-original.  No Thetis upstream; the diagnostic surfaces
// are NereusSDR-specific (Thetis has no equivalent 4O3A backplane).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-21 -- Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QTimer;

namespace NereusSDR {

class RadioModel;

class FourO3ADiagnosticsTab : public QWidget {
    Q_OBJECT

public:
    explicit FourO3ADiagnosticsTab(RadioModel* model, QWidget* parent = nullptr);

private slots:
    // Refresh the connection state block on a 1 Hz tick: listener
    // bind status, peer list, master gate state.  Light enough to
    // run live without burning CPU.
    void refreshConnectionState();

    // Append a disconnect/reconnect event.  Hooked to PgxlConnection
    // and TgxlConnection state changes.
    void appendConnectionEvent(const QString& line);

private:
    RadioModel*       m_model{nullptr};

    // Section 1: connection state.  Multi-line label refreshed on tick.
    QLabel*           m_connectionStateLabel{nullptr};

    // Section 2: disconnect / reconnect log.
    QPlainTextEdit*   m_disconnectLog{nullptr};
    bool              m_disconnectLogEmpty{true};
};

}  // namespace NereusSDR
