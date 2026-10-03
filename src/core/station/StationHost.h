#pragma once
// no-port-check: NereusSDR-original. Task 48, step 1.
// Shared station hosting around a borrowed RadioModel.

#include "core/daemon/DisplayLoadInputs.h"
#include "core/daemon/StationStatusPage.h"
#include "core/platform/ThreadPlacement.h"
#include "core/session/DnsSdAdvertiser.h"
#include "core/session/StationLanAnnouncer.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayLoadGovernor.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QPointer>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>

class QTimer;

namespace NereusSDR {

class AppSettings;
class DaemonMediaHub;
class DaemonTelemetryController;
class RadioModel;
class SharedHostSampler;
class StationRadios;
class StationRendezvous;
class StationServer;

struct StationHostOptions {
    struct HostingDevice {
        QString name;
        QString shortName;
    };
    // All fields are copied; neither construction nor option conversion loads
    // AppSettings or provisions identity. The caller must own the profile
    // before start(). Settings and StationRadios must outlive stop(). Normally
    // the caller stops the Host before destroying the model; unexpected early
    // model destruction is observed and tears down the Host without deleting
    // the borrowed model or accessing it again.
    AppSettings* settings {nullptr};
    QString securityDirectory; // empty: CertificateStore's resolved profile directory
    // A desktop window with no network session. Installed before the first
    // listener or rendezvous path can admit an external device.
    std::optional<HostingDevice> hostingDevice;
    StationRadios* stationRadios {nullptr};
    std::function<QString()> selectedRadioMac;
    QString coreName;
    QString remoteBind;
    int remotePort {0};
    bool statusPage {false};
    int statusPort {0};
    bool pairingLanClickAllowed {true};
    bool remoteTransmitAllowed {true};
    QString supportConfigPath;
    QList<quint16> linkMajors;
    int audioBitrate {48000};
    bool audioLosslessAllowed {true};
    bool displayAdaptive {true};
    std::optional<DisplayBudgetLimits> displayBudgetLimits;
    QStringList rendezvousServers;
    bool relayAllowed {true};
};

/// How devices can reach a running Core (iPhone app plan Tasks 49 and 78
/// item 8): the listener, Bonjour on this network and the remote access
/// service, as they stand now.
struct StationReach {
    bool listening {false};
    bool listenerRetryPending {false};
    /// Bonjour can run on this computer.
    bool bonjourAvailable {false};
    /// The Core is advertised by Bonjour now.
    bool bonjourActive {false};
    /// A remote access service is set up for this Core.
    bool serviceConfigured {false};
    /// The Core is registered with it now.
    bool serviceRegistered {false};
    /// The service's host name (the one in use, else the first set up).
    QString serviceHost;
};

class StationHost : public QObject {
    Q_OBJECT
public:
    StationHost(RadioModel* model, const StationHostOptions& options);
    ~StationHost() override;

    // start() is the caller's ownership boundary. A later handover step will
    // require station.lock before this method can create identity or listeners.
    bool start();
    // Close ingress immediately during a nested radio connect. Dependencies
    // remain alive until stop() can finish after that stack unwinds.
    void quiesce();
    void stop();

    StationServer* server() const { return m_stationServer.get(); }
    StationStatusPage* statusPage() const { return m_statusPage.get(); }
    QString coreLabel() const;
    StationRadioStatus radioStatus() const;
    QString statusPageAddress() const;
    bool listenerReady() const;
    StationReach reach() const;
    bool listenerRetryPending() const;
    int listenAttemptCount() const { return m_stationListenAttemptCount; }
    static QHostAddress listenerAddressFor(const QString& bind);
    static StationLanPairing stationLanPairingFor(const StationServer& server);

#ifdef NEREUS_BUILD_TESTS
    void setListenRetryIntervalsForTest(int initialMs, int maximumMs);
    void setDnsSdAdvertiserForTest(std::unique_ptr<DnsSdAdvertiser> advertiser);
    void setServerCreatedForTest(std::function<void(StationServer*)> callback)
    {
        m_serverCreatedForTest = std::move(callback);
    }
    void setDisplayLoadSourcesForTest(std::function<DisplayLoadInputs()> inputs,
                                      std::function<qint64()> clock,
                                      std::function<DisplayBudgetCharge()> accepted);
    StationLanAnnouncement stationAnnouncementForTest() const { return m_stationAnnouncement; }
    DnsSdRecord dnsSdRecordForTest() const { return m_dnsSdRecord; }
    bool stationAnnouncedForTest() const;
    bool dnsSdAdvertisedForTest() const;
    DaemonMediaHub* mediaHubForTest() const { return m_mediaHub.get(); }
    int listenNextDelayForTest() const { return m_stationListenNextDelayMs; }
#endif

private:
    void startRendezvous();
    void attemptStationServerListen();
    void scheduleStationServerListenRetry();
    void cancelStationServerListenRetry();
    void updateStationAnnouncement();
    void startSessionTelemetry(quint64 epoch);
    void endSessionTelemetry(quint64 epoch);
    DisplayLoadInputs gatherDisplayLoadInputs();
    bool anyDisplayBudgetInForce() const;
    void evaluateDisplayLoad();
    bool publishDisplayBudget(const std::optional<DisplayLoadDecision>& decision);

    QPointer<RadioModel> m_radioModel;
    StationHostOptions m_options;
    bool m_started {false};
    bool m_quiescing {false};
    bool m_quiescingInProgress {false};
    bool m_quiescePending {false};
    bool m_listenInProgress {false};
    bool m_stopping {false};
    bool m_stopPending {false};
    quint64 m_generation {0};
    std::unique_ptr<StationServer> m_stationServer;
    std::unique_ptr<StationStatusPage> m_statusPage;
    std::unique_ptr<StationRendezvous> m_rendezvous;
    std::unique_ptr<StationLanAnnouncer> m_stationAnnouncer;
    std::unique_ptr<DnsSdAdvertiser> m_dnsSdAdvertiser;
    StationLanAnnouncement m_stationAnnouncement;
    DnsSdRecord m_dnsSdRecord;
    std::optional<std::pair<quint16, DnsSdRecord>> m_dnsSdAttempt;
    std::unique_ptr<QTimer> m_stationListenRetryTimer;
    int m_stationListenRetryInitialMs {1000};
    int m_stationListenRetryMaximumMs {30000};
    int m_stationListenNextDelayMs {1000};
    int m_stationListenAttemptCount {0};
    QString m_stationListenBind;
    bool m_stationListenArmed {false};
    quint16 m_stationListenPort {0};
    std::unique_ptr<DaemonMediaHub> m_mediaHub;
    std::map<quint64, std::unique_ptr<DaemonTelemetryController>> m_telemetryControllers;
    std::shared_ptr<SharedHostSampler> m_hostSampler;
    std::unique_ptr<DisplayLoadGovernor> m_displayGovernor;
    std::unique_ptr<QTimer> m_displayGovernorTimer;
    QElapsedTimer m_displayGovernorClock;
    PlacementPlan m_placementPlan;
    std::optional<quint64> m_placementPlanRevision;
    std::function<DisplayLoadInputs()> m_displayLoadInputsForTest;
    std::function<qint64()> m_displayGovernorNowForTest;
    std::function<DisplayBudgetCharge()> m_acceptedDisplayChargeForTest;
#ifdef NEREUS_BUILD_TESTS
    std::function<void(StationServer*)> m_serverCreatedForTest;
#endif
};

} // namespace NereusSDR
