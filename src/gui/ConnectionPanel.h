#pragma once

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
