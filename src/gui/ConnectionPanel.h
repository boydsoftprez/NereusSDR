#pragma once

// =================================================================
// src/gui/ConnectionPanel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucRadioList.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — pattern-level contributions (https://github.com/ten9876/AetherSDR)
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Connection-panel layout from AetherSDR
//                 `src/gui/ConnectionPanel.{h,cpp}`; discovery/connection
//                 logic ported from Thetis `ucRadioList.cs`.
// =================================================================

#include "core/RadioDiscovery.h"
#include "core/RadioConnection.h"
#include "core/HardwareProfile.h"

#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenu>

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
// Column layout (Task 14 — plan §5.2):
//   Col 0  ●  — connection state dot
//   Col 1  Name — displayName() from BoardCapsTable
//   Col 2  Board — HPSDRHW name
//   Col 3  Protocol — P1 / P2
//   Col 4  IP — radio IP address
//   Col 5  MAC — MAC address
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

public slots:
    void onRadioDiscovered(const NereusSDR::RadioInfo& info);
    void onRadioUpdated(const NereusSDR::RadioInfo& info);
    void onRadioLost(const QString& macAddress);
    void onConnectionStateChanged();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onForgetClicked();         // Phase 3I Task 15 — removes radio from saved list
    void onAddManuallyClicked();    // Phase 3I Task 16 — AddCustomRadioDialog
    void onStartDiscoveryClicked();
    void onStopDiscoveryClicked();
    void onRadioSelectionChanged();
    void onTableDoubleClicked(int row, int column);
    void onContextMenuRequested(const QPoint& pos);

private:
    void buildUI();
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

    RadioModel* m_radioModel;

    // Widgets
    QTableWidget*  m_radioTable{nullptr};
    QPushButton*   m_startDiscoveryBtn{nullptr};
    QPushButton*   m_stopDiscoveryBtn{nullptr};
    QPushButton*   m_connectBtn{nullptr};
    QPushButton*   m_disconnectBtn{nullptr};
    QPushButton*   m_addManuallyBtn{nullptr};
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

    // Track discovered radios by MAC (row data role = MAC string)
    QMap<QString, RadioInfo> m_discoveredRadios;

    static constexpr int kMacRole = Qt::UserRole + 1;

    // Table column indices — per plan §5.2 + clsDiscoveredRadioPicker.cs column order
    enum Col : int {
        ColStatus   = 0,  // ● dot
        ColName     = 1,  // Hardware name
        ColBoard    = 2,  // Board type
        ColProtocol = 3,  // P1/P2
        ColIp       = 4,  // IP address
        ColMac      = 5,  // MAC address
        ColFirmware = 6,  // Firmware version
        ColInUse    = 7,  // "free" / "in use"
        ColCount    = 8
    };
};

} // namespace NereusSDR
