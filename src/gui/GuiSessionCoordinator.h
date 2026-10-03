// no-port-check: NereusSDR-original. R-R3-38 complete station-session ownership.
#pragma once

#include "gui/StationStartupSelection.h"
#include "gui/StationServiceManager.h"

#include <QObject>
#include <QTimer>
#include <memory>

namespace NereusSDR {
class MainWindow;
class SettingsProxy;
class GuiDesktopStationRuntime;
class StationRadios;
struct RadioInfo;

// Roles are immutable. Switching stations replaces the whole operating
// window/model/session; ordinary reconnect stays inside the existing window.
class GuiSessionCoordinator final : public QObject {
    Q_OBJECT
public:
    explicit GuiSessionCoordinator(QObject* parent = nullptr);
    ~GuiSessionCoordinator() override;

    MainWindow* window() const { return m_window.get(); }
    StationStartupSelection selection() const { return m_selection; }
    quint64 generation() const { return m_generation; }
    // Configure before creating a window, after main has acquired station.lock.
    bool configureDesktopStation(const QString& profile, bool profileOwned,
                                 StationServiceOptions options = {});
    GuiDesktopStationRuntime* desktopRuntime() const { return m_desktopRuntime.get(); }
    StationRadios* stationRadios() const { return m_stationRadios.get(); }
    bool prepareApplicationQuit(QString* error = nullptr);
    std::optional<StationServiceOptions> backgroundServiceOptions() const
    { return m_backgroundServiceOptions; }
    bool canReplace(const StationStartupSelection&, QString* error = nullptr) const;

    // Synchronous retirement: UI callers must queue this after the outgoing
    // widget's event handler returns. Reentrant and active-TX switches fail
    // before touching the active session. Does not persist the selection.
    bool replace(const StationStartupSelection&, bool startConnection,
                 QString* error = nullptr);
    void shutdown();

    /// iPhone app Task 18 (R-IOS-08): the live window's client enrolled
    /// this computer's key with its Core. The current selection is trusted
    /// by that identity from now on, as the saved Core is, so the two still
    /// match; the window itself is kept.
    void noteStationIdentity(const QByteArray& identityFingerprint)
    {
        m_selection.connection.identityFingerprint = identityFingerprint;
    }

signals:
    void windowChanged(NereusSDR::MainWindow* window);
    void connectionsRequested();
    void stationOperationFailed(const QString& reason);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void retireWindow();
    void installDesktopStation();
    void refreshStationRadios();
    void finishHostedDiscovery(quint64 generation);
    void connectHostedRadio(const RadioInfo& radio);
    void retryHostedRadio(quint64 generation);
    void queueHostedRadioChange(const QString& mac);
    void finishHostedRadioChange(const QString& mac, quint64 generation);

    std::unique_ptr<SettingsProxy> m_proxy;
    std::unique_ptr<MainWindow> m_window;
    // Host borrows the registry and model; retire it before either owner.
    std::unique_ptr<StationRadios> m_stationRadios;
    std::unique_ptr<GuiDesktopStationRuntime> m_desktopRuntime;
    QString m_profile;
    StationServiceOptions m_serviceOptions;
    std::optional<StationServiceOptions> m_backgroundServiceOptions;
    bool m_desktopConfigured = false;
    bool m_profileOwned = false;
    bool m_quitPrepared = false;
    bool m_preparingQuit = false;
    bool m_hostedStartupPending = false;
    bool m_hostedRadioRecovery = false;
    bool m_retiringHostedRadio = false;
    bool m_hostedRadioAttempted = false;
    QTimer m_hostedDiscoveryRetry;
    StationStartupSelection m_selection;
    quint64 m_generation = 0;
    bool m_replacing = false;
};
} // namespace NereusSDR
