#pragma once
// no-port-check: NereusSDR-original. Setup > This Core in a remote window:
// the Core's radio (Change radio).

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/setup/ThisCorePage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-IOS-18 (Manage Radios), R-R3-38,
// R-R3-49 (parity Task 21; the iPhone app plan's Task 25 This Core page,
// the operator's "Option A": Change radio lives on a This Core page, and
// Connections stays about what to connect to).
//
// Change radio lists the radios the Core can see (the `stationRadios`
// stream), the Core's radio first and marked, and offers Use this radio
// (station.selectRadio), Scan again (station.rescanRadios), the model the
// Core runs the chosen radio as (station.setRadioModel, the local Edit
// radio's model override) and Forget radio (station.forgetRadio). Every
// control is shown and disabled with a plain reason when it cannot be used:
// no Core session, a Core that does not offer it, the radio on the air, or
// (Forget) the Core's own radio. The paired devices and Add a device join
// this page with the iPhone app plan's Task 25.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Created (parity Task 21, R-IOS-18,
//                                    R-R3-38, R-R3-49). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 78 (R-IOS-07,
//                                    R-IOS-02): connectedList().
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  modelListUnavailableReason().
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/SetupPage.h"

class QGroupBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;

namespace NereusSDR {

class ConnectedDevicesList;
class RadioModel;

class ThisCorePage : public SetupPage {
    Q_OBJECT

public:
    explicit ThisCorePage(RadioModel* model, QWidget* parent = nullptr);

    void setStationSettingsAvailable(bool available, const QString& reason) override;

    /// Selects the Core's radio in the list (the menus' Edit radio and
    /// Forget radio open the page on it).
    void selectCoreRadio();

    /// Reuse these exact sections in the identity-fenced Cores hub. The page
    /// remains the command/gate owner; the hub reparents section widgets.
    QWidget* radioSection() const { return m_radioSection; }
    QList<QWidget*> devicesSections() const { return m_devicesSections; }

    /// Why the controls are disabled now, empty when they are not.
    QString unavailableReason() const;
    /// Follow-up N1: a token sign-in that enrolled this computer's key.
    /// The Core takes the radio requests from the key's next sign-in.
    static QString reconnectToChangeRadioReason();
    /// The Core did not send the models its radios can run as (a Core
    /// older than the model list), so the model choice waits.
    static QString modelListUnavailableReason();

    // For tests.
    QTreeWidget* radioList() const { return m_list; }
    QPushButton* useButton() const { return m_useButton; }
    QPushButton* scanButton() const { return m_scanButton; }
    QComboBox* modelCombo() const { return m_modelCombo; }
    QPushButton* forgetButton() const { return m_forgetButton; }
    QLabel* statusLabel() const { return m_status; }
    /// Task 78: who is connected to the Core now.
    ConnectedDevicesList* connectedList() const { return m_connectedList; }
    /// iPhone app plan Task 25: the Core's paired devices and its key.
    QWidget* pairedRows() const { return m_pairedRows; }
    QPushButton* addDeviceButton() const { return m_addDevice; }
    QLabel* pairingCodeLabel() const { return m_pairingCode; }
    QLabel* coreNameLabel() const { return m_coreName; }
    QLabel* keyBackupLabel() const { return m_keyBackup; }
    QPushButton* keyBackupButton() const { return m_keyBackupDone; }
    QLabel* devicesStatusLabel() const { return m_devicesStatus; }
    /// Why the device controls are disabled now, empty when they are not.
    QString devicesUnavailableReason() const;

private:
    void rebuildList();
    void refreshControls();
    QString selectedMac() const;
    bool selectedIsCoresRadio() const;
    void send(const QByteArray& verb, const QString& mac, int model = 0);
    void rebuildDevices();
    void sendDeviceAdmin(const QByteArray& verb, const QString& id = QString());

    QWidget* m_radioSection = nullptr;
    QList<QWidget*> m_devicesSections;
    RadioModel* m_radioModel = nullptr;
    QTreeWidget* m_list = nullptr;
    QPushButton* m_useButton = nullptr;
    QPushButton* m_scanButton = nullptr;
    QComboBox* m_modelCombo = nullptr;
    QPushButton* m_forgetButton = nullptr;
    QLabel* m_status = nullptr;
    ConnectedDevicesList* m_connectedList = nullptr;
    QWidget* m_pairedRows = nullptr;
    QVBoxLayout* m_pairedLayout = nullptr;
    QPushButton* m_addDevice = nullptr;
    QLabel* m_pairingCode = nullptr;
    QLabel* m_pairingInstruction = nullptr;
    QLabel* m_coreName = nullptr;
    QLabel* m_keyBackup = nullptr;
    QPushButton* m_keyBackupDone = nullptr;
    QLabel* m_devicesStatus = nullptr;
    /// The device request whose answer the page shows (0 for none).
    quint32 m_devicesCommandId = 0;
    bool m_stationAvailable = true;
    QString m_stationReason;
    bool m_fillingModels = false;
    // The status line is the page's own (a reason or a waiting line), not
    // a request's answer.
    bool m_statusIsPage = true;
};

} // namespace NereusSDR
