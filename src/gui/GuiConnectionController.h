// no-port-check: NereusSDR-original. R-R3-38 operator target selection.
// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#pragma once

#include "core/RadioDiscovery.h"
#include "core/session/StationLanDiscovery.h"
#include "core/session/StationPairingClient.h"
#include "gui/ConnectionSelector.h"
#include "gui/CoreTargetStore.h"
#include "gui/GuiSessionCoordinator.h"

#include <QMap>
#include <QPointer>
#include <QTimer>

#include <memory>

namespace NereusSDR {
class RemoteConnectionController;
class StationClient;
class CoreSettingsHost;
class ClientDeviceIdentity;

// Application-scoped connection UI. Selecting/editing a row never replaces
// the live session; an explicit Connect queues retirement after the originating
// widget's event handler has returned.
class GuiConnectionController final : public QObject {
    Q_OBJECT
public:
    explicit GuiConnectionController(QObject* parent = nullptr);
    ~GuiConnectionController() override;
    void start(const StationStartupRequest&);
    void shutdown();
    GuiSessionCoordinator* sessions() { return &m_sessions; }
    CoreTargetStore& coreTargetStore() { return m_store; }
    bool coreTargetStoreLoaded() const { return m_storeLoaded; }
    static RemoteStationOptions connectionOptionsForTarget(const SavedCoreTarget& target);
    /// Authenticated learned service metadata is separate from selected credentials.
    static bool authenticatedSelectionMatchesSaved(const StationStartupSelection& selection,
                                                   const SavedCoreTarget& target);
    ConnectionSelector* selector() const { return m_selector.get(); }

    /// iPhone app Task 18 (R-IOS-08): a Core on this network as the
    /// Connections window lists it when it is not the live one: by its
    /// label, Pair for a Core that takes new devices, Connect for one this
    /// computer has saved. `saved` is every saved Core.
    static ConnectionTargetRow lanCoreRow(const StationLanEndpoint& endpoint,
                                          const QList<SavedCoreTarget>& saved);
    /// iPhone app Task 18, Part C follow-up (R-IOS-08): the last line of a
    /// Core's details in the Connections window, what to do next with it.
    /// Takes the same branches lanCoreRow() takes for its state.
    static QString lanCoreNextStep(const StationLanAnnouncement& advertised);
    /// A saved Core as listed under Your Cores when it is not the live one.
    static ConnectionTargetRow savedCoreRow(const SavedCoreTarget& target, bool storeLoaded);
    static QString savedCoreDetails(const SavedCoreTarget& target);
    /// True when a saved Core has what a sign-in needs: its identity (a
    /// paired Core), or a token and a pin (or the bench flag).
    static bool isReadyToConnect(const RemoteStationOptions& connection);

public slots:
    void showConnections();

private:
    void attachWindow(MainWindow*);
    void refresh();
    void scan();
    void queueConnect(const QString& key);
    void connectTarget(const QString& key);
    void disconnectCurrent();
    void editCore(const QString& id = {});
    void editRadio(const QString& mac = {});
    void forgetTarget(const QString& key);
    void showDetails(const QString& key);
    void rememberAuthenticatedRadio();
    void rememberAuthenticatedCapability();
    void observeNetworkGeneration();
    // iPhone app Task 18: pairing, and the key a token sign-in enrolled.
    void pairTarget(const QString& key);
    void addByCode(const QString& address = QString());
    void onPaired(const PairedStationRecord& record);
    void onPairingFailed(const QString& reason);
    void rememberStationIdentity(const QByteArray& identityFingerprint);
    StationPairingClient* pairingClient();
    bool choose(const StationStartupSelection&, bool startConnection);

    bool observationLeaseCurrent(StationClient* client) const;
    bool needsFreshCanonicalSession(const SavedCoreTarget& target) const;
    bool canExplicitlyReplaceStaleCore(const SavedCoreTarget& target) const;
    QString m_windowTargetId;
    quint64 m_windowTargetIncarnation = 0;
    quint64 m_windowCoordinatorGeneration = 0;
    CoreTargetStore m_store;
    GuiSessionCoordinator m_sessions;
    StationLanDiscovery m_lan;
    std::unique_ptr<ConnectionSelector> m_selector;
    std::unique_ptr<StationPairingClient> m_pairing;
    QPointer<RadioDiscovery> m_discovery;
    QPointer<RemoteConnectionController> m_remoteControls;
    QMap<QString, RadioInfo> m_radios;
    QMap<QString, qint64> m_seenAt;
    QList<QMetaObject::Connection> m_windowConnections;
    quint64 m_request = 0;
    bool m_storeLoaded = false;
    bool m_shuttingDown = false;
    QTimer m_negativeExpiryTimer;
    std::shared_ptr<const ClientDeviceIdentity> m_existingDeviceIdentity;
    std::unique_ptr<CoreSettingsHost> m_coreSettings;
    QByteArray m_pendingConnectionIdentity;
};
} // namespace NereusSDR
