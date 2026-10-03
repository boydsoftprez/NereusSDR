#pragma once
// no-port-check: NereusSDR-original desktop Remote Access runtime.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/daemon/DaemonConfig.h"
#include "core/station/StationHost.h"
#include "gui/DesktopStationController.h"
#include "gui/StationServiceManager.h"
#include "gui/setup/RemoteStationPage.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <memory>

namespace NereusSDR {
class AppSettings;
class RadioModel;
class SetupDialog;
class StationRadios;

// Borrowed from the desktop coordinator. The radio registry and callback
// owners must outlive this runtime and its StationHost.
struct RuntimeStationBindings {
    StationRadios* stationRadios = nullptr;
    std::function<QString()> selectedRadioMac;
    QList<quint16> linkMajors;
};

// Borrows the desktop's one local model and its singleton settings. The caller
// keeps both alive until stop() and destroys this runtime before the model.
class RemoteDevicesState;

class GuiDesktopStationRuntime final : public QObject {
    Q_OBJECT
public:
    /// The Remote Access page's reach line for a running Core (iPhone app
    /// plan Tasks 49 and 78 item 8): where it listens, whether Bonjour
    /// makes it known on this network, and whether the remote access
    /// service has it registered. `bind` empty means every interface.
    static QString reachText(const QString& bind, int port, const StationReach& reach);

    GuiDesktopStationRuntime(RadioModel* model, AppSettings* settings,
                             const QString& profile, bool profileOwned,
                             StationServiceOptions serviceOptions = {},
                             RuntimeStationBindings bindings = {},
                             QObject* parent = nullptr);
    GuiDesktopStationRuntime(RadioModel* model, AppSettings* settings,
                             const QString& profile, bool profileOwned,
                             StationServiceOptions serviceOptions, QObject* parent);
    ~GuiDesktopStationRuntime() override;

    DesktopStationController* controller() const { return m_controller.get(); }
    bool restore();
    void bindSetupDialog(SetupDialog* dialog);
    const RemoteStationPage::State& state() const { return m_state; }
    /// iPhone app plan Task 78 item 8 (R-IOS-07): who is connected to the
    /// Core on this computer now, read from its session registry each
    /// refresh; empty while no Core runs here. The Remote Access page's
    /// Connected now list shows it.
    RemoteDevicesState* connectedDevices();
    const DaemonConfig& config() const { return m_config; }
    void refresh();
    void stop();
    bool prepareForRetirement(bool requestBackground, QString* error);
    bool backgroundStartWanted() const { return m_backgroundStartWanted; }
    StationServiceOptions backgroundServiceOptions() const { return m_serviceOptions; }

    void setLifecycleBusy(bool busy);
    bool setRunCore(bool enabled);
    bool setKeepRunning(bool enabled);
    bool setStartWithComputer(bool enabled);
    bool renameStation(const QString& name);
    bool revokeDevice(const QByteArray& id);
    /// Slice control plan Task 8b: the Core stops accepting its pairing
    /// token, then removes `id` (StationDevicesFacade::retireTokenAndRevoke).
    bool revokeDeviceStoppingPairingToken(const QByteArray& id);
    bool addDevice();
    bool acknowledgeKeyBackup();

signals:
    void operationFailed(const QString& reason);
    void stateChanged();

private:
    bool ownershipAvailable(QString* reason = nullptr) const;
    bool available(QString* reason = nullptr) const;
    bool actionAllowed(QString* reason = nullptr) const;
    bool loadConfig(QString* reason);
    bool ensureBackgroundConfig(QString* reason);
    void bindPage(RemoteStationPage* page);
    void fail(const QString& reason);
    void attachHostSignals();
    void updateState();
    static bool sameState(const RemoteStationPage::State& a,
                          const RemoteStationPage::State& b);

    QPointer<RadioModel> m_model;
    AppSettings* m_settings = nullptr;
    QString m_profileDirectory;
    bool m_profileOwned = false;
    QString m_profileError;
    QString m_configurationError;
    DaemonConfig m_config;
    bool m_lifecycleBusy = false;
    StationServiceOptions m_serviceOptions;
    std::unique_ptr<StationServiceManager> m_service;
    std::unique_ptr<DesktopStationController> m_controller;
    RemoteStationPage::State m_state;
    RemoteDevicesState* m_hostDevices = nullptr;
    QList<QPointer<RemoteStationPage>> m_pages;
    QTimer m_refreshTimer;
    bool m_actionActive = false;
    bool m_retiring = false;
    bool m_closed = false;
    bool m_retirementPrepared = false;
    bool m_pendingBackgroundRequest = false;
    bool m_keepRunning = false;
    bool m_backgroundStartWanted = false;
    bool m_startWithComputer = false;
};

} // namespace NereusSDR
