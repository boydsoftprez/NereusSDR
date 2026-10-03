#pragma once
// no-port-check: NereusSDR-original presentation for the local Core.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui/SetupPage.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <functional>

class QCheckBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace NereusSDR {

class ConnectedDevicesList;
class RemoteDevicesState;

class RemoteStationPage : public SetupPage {
    Q_OBJECT
public:
    struct Device {
        QByteArray id;
        QString name;
        QString pairedText;
        QString lastSeenText;
        bool revocable = true;
        /// Slice control plan Task 8b: removing it first stops the Core
        /// accepting its pairing token (it joined with the token, which
        /// still works); the removal asks before it goes ahead.
        bool removalStopsPairingToken = false;
        /// Why Revoke is unavailable, when it is; empty for the page's own
        /// words.
        QString revokeReason;
    };
    struct State {
        bool available = false;
        QString unavailableReason;
        bool runCore = false;
        bool keepRunning = false;
        bool startWithComputer = false;
        bool busy = false;
        bool transmitting = false;
        QString stationName;
        QString reachabilityText;
        QString pairingCode;
        QString keyBackupPath;
        bool pairingOpen = false;
        /// LINK-I4: pairing through the remote access service is off.
        bool servicePairingShut = false;
        bool keyBackupAcknowledged = false;
        QVector<Device> devices;
    };

    explicit RemoteStationPage(QWidget* parent = nullptr);
    void setState(const State& state);
    /// Task 8b: asks the operator to go ahead (title, text, the go-ahead
    /// button's words); true to go ahead. A message box unless set (tests).
    using Confirmation = std::function<bool(const QString& title, const QString& text,
                                            const QString& goAhead)>;
    void setConfirmation(Confirmation confirmation) { m_confirmation = std::move(confirmation); }
    const State& state() const { return m_state; }
    /// iPhone app plan Task 78 item 8 (R-IOS-07): who is connected to the
    /// Core on this computer now, from its session registry.
    void setConnectedDevices(RemoteDevicesState* devices);
    ConnectedDevicesList* connectedList() const { return m_connectedList; }

signals:
    void runCoreRequested(bool enabled);
    void keepRunningRequested(bool enabled);
    void startWithComputerRequested(bool enabled);
    void renameRequested(const QString& name);
    void revokeRequested(const QByteArray& id);
    /// Task 8b: remove `id`, stopping the Core accepting its pairing token
    /// first, as the operator confirmed.
    void revokeStoppingPairingTokenRequested(const QByteArray& id);
    void addDeviceRequested();
    void keyBackupAcknowledgedRequested();
    // Opens this window's Connections through SetupDialog/MainWindow.
    void connectionsRequested();

private:
    void refresh();
    void rebuildDevices();
    void applyGate(QWidget* control, bool allowed, const QString& reason);
    bool confirm(const QString& title, const QString& text, const QString& goAhead);
    State m_state;
    Confirmation m_confirmation;
    QCheckBox* m_runCore = nullptr;
    QCheckBox* m_keepRunning = nullptr;
    QCheckBox* m_startWithComputer = nullptr;
    QLabel* m_reason = nullptr;
    QLabel* m_name = nullptr;
    QPushButton* m_rename = nullptr;
    QLabel* m_reachability = nullptr;
    QLabel* m_pairingCode = nullptr;
    QLabel* m_pairingInstruction = nullptr;
    QLabel* m_servicePairingShut = nullptr;
    QVBoxLayout* m_devicesLayout = nullptr;
    QWidget* m_deviceRows = nullptr;
    QPushButton* m_addDevice = nullptr;
    QLabel* m_backupPath = nullptr;
    QGroupBox* m_backupGroup = nullptr;
    QPushButton* m_backupAcknowledged = nullptr;
    quint64 m_deviceGeneration = 0;
    ConnectedDevicesList* m_connectedList = nullptr;
};

} // namespace NereusSDR
