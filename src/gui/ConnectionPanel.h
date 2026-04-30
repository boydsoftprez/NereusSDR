#pragma once

// =================================================================
// src/gui/ConnectionPanel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucRadioList.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

/*  ucRadioList.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "core/RadioDiscovery.h"
#include "core/RadioConnection.h"
#include "core/HardwareProfile.h"
#include "core/ConnectionState.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>

namespace NereusSDR {

class RadioModel;

// ConnectionPanel — Thetis ucRadioList-equivalent radio list UI.
//
// Porting note: Thetis ucRadioList.cs is a custom-painted card list showing
// 4 info lines per radio. clsDiscoveredRadioPicker.cs shows a DataGridView
// with columns: Hardware, IP, Base Port, Mac Address, Protocol, Version.
// NereusSDR uses QTableWidget with 8 columns per plan §5.2, combining both
// Thetis source patterns.
//
// Column layout (Task 5 — 3Q polish):
//   Col 0  ●  — state pill (Online/Stale/Offline/Connected)
//   Col 1  Name — displayName() from BoardCapsTable
//   Col 2  Board — HPSDRHW name
//   Col 3  Protocol — P1 / P2
//   Col 4  IP — radio IP address
//   Col 5  Last Seen — relative age ("just now", "4 m ago", etc.)
//   Col 6  Firmware — firmware version integer
//   Col 7  In-Use — "free" / "in use"
//
// Color coding:
//   Green row  : online + free
//   Amber row  : online + in-use
//   Grey row   : offline (was seen, not responding)
//   Red row    : error
//
// Source: ucRadioList.cs (ramdor/Thetis), clsDiscoveredRadioPicker.cs
class ConnectionPanel : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionPanel(RadioModel* model, QWidget* parent = nullptr);
    ~ConnectionPanel() override;

    // Update the connection status display.
    void setStatusText(const QString& text);

    // -------------------------------------------------------------------------
    // State pill — represents the recency / connectivity of a discovered radio.
    //
    // Thresholds match design §7.3:
    //   Online    : lastSeen < 60 s ago
    //   Stale     : lastSeen 60 s – 5 min ago
    //   Offline   : lastSeen > 5 min ago, or never seen (lastSeenMs == 0)
    //   Connected : currently-connected radio (overrides age-based states)
    // -------------------------------------------------------------------------
    enum class StatePill : int {
        Online    = 0,   // last seen < 60 s ago
        Stale     = 1,   // last seen 60 s – 5 min ago
        Offline   = 2,   // last seen > 5 min, or never seen
        Connected = 3,   // currently connected (overrides aging)
    };

    // Pure function — easy to test, easy to reuse.
    // lastSeenMs == 0 → Offline (never seen).
    static StatePill statePillForLastSeen(qint64 lastSeenMs, qint64 nowMs);

public slots:
    void onRadioDiscovered(const NereusSDR::RadioInfo& info);
    void onRadioUpdated(const NereusSDR::RadioInfo& info);
    void onRadioLost(const QString& macAddress);
    void onConnectionStateChanged();

    // Phase 3Q Task 10: select the table row whose MAC matches `mac`.
    // No-op if the MAC is not currently in the table (defensive — the
    // auto-connect failure path may call this before discovery populates
    // the offline entry for the saved radio).
    void highlightMac(const QString& mac);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onForgetClicked();         // Phase 3I Task 15 — removes radio from saved list
    void onAddManuallyClicked();    // Phase 3I Task 16 — AddCustomRadioDialog
    void onEditClicked();           // Edit selected saved entry (3Q polish)
    void onScanClicked();           // Phase 3Q Task 5 — single ↻ Scan button
    void onRadioSelectionChanged();
    void onTableDoubleClicked(int row, int column);
    void onContextMenuRequested(const QPoint& pos);
    void updateStatusStrip();       // Phase 3Q Task 5 — status strip update
    void refreshLastSeenColumn();   // Phase 3Q Task 5 — periodic relative-time refresh

private:
    void buildUI();
    QWidget* buildStatusStrip();    // Phase 3Q Task 5 — top-of-panel connection status widget
    void setPillIconForRow(int row, const QString& mac);  // Pixmap pill icon (3Q polish)
    void updateButtonStates();
    void applyRowColor(int row, const RadioInfo& info);
    void populateRow(int row, const RadioInfo& info);
    // Insert a new row or update an existing row for `info`.
    // online=false renders the row with offline colour (manual adds start offline).
    void upsertRowForInfo(const RadioInfo& info, bool online);

    void onModelComboChanged(int index);
    void updateDetailPanel();

    // Get the RadioInfo for the currently selected table row
    RadioInfo selectedRadio() const;
    // Find row index by MAC, returns -1 if not found
    int rowForMac(const QString& mac) const;

    // Relative-time helper — renders ms age as "just now", "4 m ago", etc.
    static QString relativeTime(qint64 lastSeenMs, qint64 nowMs);

    RadioModel* m_radioModel;

    // Phase 3Q Task 5: state strip and scan button
    QWidget*       m_statusStrip{nullptr};
    QLabel*        m_stripPillLabel{nullptr};
    QLabel*        m_stripInfoLabel{nullptr};
    QPushButton*   m_stripDisconnectBtn{nullptr};
    QLabel*        m_stripReconnectLabel{nullptr};

    // Widgets
    QTableWidget*  m_radioTable{nullptr};
    QPushButton*   m_scanBtn{nullptr};          // Phase 3Q Task 5 — replaces Start/Stop
    QPushButton*   m_connectBtn{nullptr};
    QPushButton*   m_addManuallyBtn{nullptr};
    QPushButton*   m_editBtn{nullptr};
    QPushButton*   m_forgetBtn{nullptr};
    QPushButton*   m_closeBtn{nullptr};
    QLabel*        m_statusLabel{nullptr};

    // Detail panel (Phase 3I-RP)
    QGroupBox*   m_detailGroup{nullptr};
    QLabel*      m_detailBoardLabel{nullptr};
    QLabel*      m_detailProtoLabel{nullptr};
    QLabel*      m_detailFwLabel{nullptr};
    QLabel*      m_detailIpLabel{nullptr};
    QLabel*      m_detailMacLabel{nullptr};
    QComboBox*   m_modelCombo{nullptr};
    QLabel*      m_modelHintLabel{nullptr};
    QCheckBox*   m_autoConnectCheck{nullptr};   // Phase 3Q Task 5 — auto-connect-on-launch

    // Track discovered radios by MAC (row data role = MAC string)
    QMap<QString, RadioInfo> m_discoveredRadios;

    // Phase 3Q Task 5 — per-MAC last-seen timestamps (ms since epoch).
    // Updated by onRadioDiscovered / onRadioUpdated; read by statePillForLastSeen()
    // and relativeTime() to populate the Last Seen column.
    QMap<QString, qint64> m_lastSeenMs;

    // Periodic timer to refresh the relative-time "Last Seen" column.
    QTimer m_lastSeenRefreshTimer;

    // Phase 3Q Task 5 — track previous connection state so auto-open/close
    // only fires on transitions, not on the first construction read.
    bool m_everSawConnectionState{false};
    ConnectionState m_prevConnectionState{ConnectionState::Disconnected};

    static constexpr int kMacRole = Qt::UserRole + 1;

    // Table column indices — per plan §5.2 + clsDiscoveredRadioPicker.cs column order.
    // Task 5 (3Q): ColMac removed from the table; MAC now lives only in the detail panel.
    // ColLastSeen replaces ColMac.
    enum Col : int {
        ColStatus   = 0,  // state pill dot
        ColName     = 1,  // Hardware name
        ColBoard    = 2,  // Board type
        ColProtocol = 3,  // P1/P2
        ColIp       = 4,  // IP address
        ColLastSeen = 5,  // relative last-seen time (was ColMac)
        ColFirmware = 6,  // Firmware version
        ColInUse    = 7,  // "free" / "in use"
        ColCount    = 8
    };
};

} // namespace NereusSDR
