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
//   2. Recent commands   -- rolling ring of the last 50 FlexAPI
//                           frames (parsed from PGXL/TGXL traffic).
//   3. Fault history     -- placeholder when empty; otherwise a
//                           table of recorded faults (time / source
//                           / type / notes).
//   4. Disconnect log    -- PGXL/TGXL TCP open/close events so the
//                           operator can spot flaky LAN.
//
// All four sections use empty-state placeholders ("no faults logged
// yet" / "no disconnect events yet") to avoid blank dead-ends.
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

    // Append a recent-command line.  Hooked to the SmartSdrApiListener
    // log signals (rxFrame / txFrame).  Trims to last 50 lines.
    void appendCommand(const QString& line);

    // Append a disconnect/reconnect event.  Hooked to PgxlConnection
    // and TgxlConnection state changes.
    void appendConnectionEvent(const QString& line);

private:
    RadioModel*       m_model{nullptr};

    // Section 1: connection state.  Multi-line label refreshed on tick.
    QLabel*           m_connectionStateLabel{nullptr};

    // Section 2: recent commands.  Plain-text ring buffer; trimmed
    // to last kMaxCommandLines lines on every append.
    QPlainTextEdit*   m_commandLog{nullptr};
    static constexpr int kMaxCommandLines = 50;

    // Section 3: fault history.  Plain-text panel with empty-state
    // placeholder shown until the first fault row is appended.
    QPlainTextEdit*   m_faultHistory{nullptr};
    bool              m_faultHistoryEmpty{true};

    // Section 4: disconnect / reconnect log.  Same pattern.
    QPlainTextEdit*   m_disconnectLog{nullptr};
    bool              m_disconnectLogEmpty{true};
};

}  // namespace NereusSDR
