// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "core/session/RemoteStationOptions.h"
#include "gui/GuiConnectionController.h"
#include "RendezvousTestHarness.h"
#include "gui/RemoteConnectionController.h"
#include "core/session/RendezvousWire.h"
using namespace NereusSDR;
class TstCoreConnectionCandidates : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        AppSettings::instance().setValue(QStringLiteral("RemoteAccessServers"), QStringLiteral("ws://127.0.0.1:1"));
    }
    void learnedServiceIdentityDoesNotRetireAuthenticatedLearning() {
        SavedCoreTarget target; target.id = QStringLiteral("paired");
        target.connection.identityFingerprint = QByteArray(32, 'i');
        target.connection.url = QStringLiteral("wss://listener.test:47910");
        StationStartupSelection selected{target.connection, target.id, {}};
        target.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        QVERIFY2(GuiConnectionController::authenticatedSelectionMatchesSaved(selected, target),
                 "A newly authenticated service ID must not stop subsequent history/inventory/capability observations");
        target.connection.identityFingerprint = QByteArray(32, 'x');
        QVERIFY(!GuiConnectionController::authenticatedSelectionMatchesSaved(selected, target));
        target.connection.identityFingerprint = selected.connection.identityFingerprint;
        target.connection.reachFromAnywhere = false;
        QVERIFY(!GuiConnectionController::authenticatedSelectionMatchesSaved(selected, target));
    }
    void transientUnionPreservesHistoryAndRequiresPairedTrust() {
        SavedCoreTarget target;
        target.connection.identityFingerprint = QByteArray(32, 'i');
        target.manualAddresses = {QStringLiteral("wss://one.test:47910"), QStringLiteral("wss://two.test:47910")};
        target.connection.cachedAddresses = {QStringLiteral("wss://one.test:47910")};
        target.connection.coreAddresses = {QStringLiteral("wss://two.test:47910"), QStringLiteral("wss://three.test:47910")};
        const auto snapshot = GuiConnectionController::connectionOptionsForTarget(target);
        QCOMPARE(snapshot.directCandidates.size(), 3);
        QCOMPARE(snapshot.cachedAddresses, target.connection.cachedAddresses);
        QCOMPARE(snapshot.coreAddresses, target.connection.coreAddresses);
        QVERIFY(target.connection.directCandidates.isEmpty());
        target.connection.allowUnpinned = true;
        QVERIFY(GuiConnectionController::connectionOptionsForTarget(target).directCandidates.isEmpty());
        target.connection.allowUnpinned = false; target.connection.identityFingerprint.clear();
        QVERIFY(GuiConnectionController::connectionOptionsForTarget(target).directCandidates.isEmpty());
    }
    void freshManualOnlyReadinessAndFailedServiceReachRealPairedDirectCore() {
        NereusSDR::Test::Rendezvous::Core core;
        QVERIFY(core.server->listen(QHostAddress::LocalHost, 0));
        QTemporaryDir keyDir;
        const auto key = std::make_shared<const ClientDeviceIdentity>(ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        SavedCoreTarget target;
        target.id = QStringLiteral("paired");
        target.connection.identityFingerprint = core.server->stationIdentity().fingerprint();
        target.connection.rendezvousId = RendezvousWire::rendezvousId(core.server->stationIdentity().publicKeySpki());
        target.manualAddresses = {core.url().toString()};
        StationClient client(nullptr, nullptr); client.setDeviceIdentity(key, QStringLiteral("Test computer"));
        RemoteConnectionController controls(&client, nullptr, target.connection);
        controls.setCurrentOptionsSource([&] { return std::optional(GuiConnectionController::connectionOptionsForTarget(target)); });
        QVERIFY(controls.canConnect()); controls.connectToStation();
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 3000);
        QCOMPARE(client.connectedUrl(), core.url()); QVERIFY(target.connection.cachedAddresses.isEmpty());
        const auto route = client.serviceRoute(); QVERIFY(bool(route.currentControlChannelVersion));
        target.connection.controlChannelVersion = 0;
        target.connection.negativeControlObservedMs = QDateTime::currentMSecsSinceEpoch();
        QCOMPARE(route.currentControlChannelVersion(), 0);
        target.connection.negativeControlObservedMs -= RemoteStationOptions::kNegativeControlLifetimeMs;
        QCOMPARE(route.currentControlChannelVersion(), -1);
        const quint32 epoch = client.sessionEpoch();
        target.manualAddresses = {QStringLiteral("wss://127.0.0.1:1")};
        QVERIFY(!controls.canConnect()); controls.connectToStation();
        QCOMPARE(client.connectedUrl(), core.url()); QCOMPARE(client.sessionEpoch(), epoch);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }
    void oldServiceCapabilityRemainsAnHonestAttemptNote() {
        NereusSDR::Test::Rendezvous::Core core;
        QTemporaryDir keyDir;
        const auto key = std::make_shared<const ClientDeviceIdentity>(ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        SavedCoreTarget target; target.connection.identityFingerprint = core.server->stationIdentity().fingerprint();
        target.connection.rendezvousId = RendezvousWire::rendezvousId(core.server->stationIdentity().publicKeySpki());
        target.connection.controlChannelVersion = 0; target.connection.negativeControlObservedMs = QDateTime::currentMSecsSinceEpoch();
        target.manualAddresses = {QStringLiteral("wss://127.0.0.1:1")};
        StationClient client(nullptr, nullptr); client.setDeviceIdentity(key, QStringLiteral("Test computer"));
        RemoteConnectionController controls(&client, nullptr, target.connection);
        controls.setCurrentOptionsSource([&] { return std::optional(GuiConnectionController::connectionOptionsForTarget(target)); });
        controls.connectToStation();
        bool note = false;
        for (const auto& attempt : client.connectionAttempt().tries) {
            if (attempt.outcome == StationConnectionAttempt::Outcome::CoreTooOld) { note = true; }
        }
        QVERIFY2(note, "Version 0 must remain a CoreTooOld service explanation");
        client.disconnectFromStation(QStringLiteral("test complete"));
    }
    void retiredCandidateLeaseStopsRetryBeforeOldUrlReuse() {
        StationClient client(nullptr, nullptr);
        bool alive = true; int reads = 0;
        client.setCandidateSource([&]() -> std::optional<StationClient::ConnectionCandidates> {
            ++reads;
            if (!alive) { return std::nullopt; }
            return StationClient::ConnectionCandidates{{QUrl(QStringLiteral("wss://127.0.0.1:1"))}, {}};
        });
        client.setReconnectBackoffUnitMs(10);
        QObject::connect(&client, &StationClient::reconnectScheduled, &client, [&](int, int) { alive = false; });
        QSignalSpy ended(&client, &StationClient::sessionEnded);
        client.connectToStation({}, {}, {}, false, QByteArray(32, 'i'));
        QTRY_VERIFY_WITH_TIMEOUT(reads >= 2, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.isConnectionActive(), 1000);
        QVERIFY(client.connectedUrl().isEmpty()); QVERIFY(!client.isReconnectPending());
        QVERIFY(ended.last().at(0).toString().contains(QStringLiteral("saved Core changed")));
    }

    void pairedDirectNeedsNoServiceName() {
        RemoteStationOptions options;
        options.identityFingerprint = QByteArray(32, 'i');
        options.cachedAddresses = {QStringLiteral("wss://listener.test:47910")};
        options.reachFromAnywhere = false;
        QVERIFY(options.hasAuthenticatedDirectAddresses());
        QVERIFY2(options.isValidRemoteTarget(), "A proved paired direct route must not require a service ID");
        QVERIFY(GuiConnectionController::isReadyToConnect(options));
    }
    void retryUsesAnAddressRetainedWhileWaiting() {
        NereusSDR::Test::Rendezvous::Core core;
        QVERIFY(core.server->listen(QHostAddress::LocalHost, 0));
        QTemporaryDir keyDir;
        const auto key = std::make_shared<const ClientDeviceIdentity>(ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost, 0));
        const QUrl stale(QStringLiteral("wss://127.0.0.1:%1").arg(closed.serverPort()));
        closed.close();
        StationClient client(nullptr, nullptr);
        client.setDeviceIdentity(key, QStringLiteral("Test computer"));
        client.setReconnectBackoffUnitMs(50);
        QList<QUrl> retained{stale};
        client.setCandidateSource([&]() -> std::optional<StationClient::ConnectionCandidates> {
            return StationClient::ConnectionCandidates{retained, {}};
        });
        QSignalSpy retries(&client, &StationClient::reconnectScheduled);
        QObject::connect(&client, &StationClient::reconnectScheduled, &client, [&](int attempt, int) {
            if (attempt == 1) { retained = {core.url()}; }
        });
        client.connectToStation({}, {}, {}, false, core.server->stationIdentity().fingerprint());
        QTRY_VERIFY_WITH_TIMEOUT(!retries.isEmpty(), 1500);
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 3000);
        QCOMPARE(client.connectedUrl(), core.url());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

};
QTEST_MAIN(TstCoreConnectionCandidates)
#include "tst_core_connection_candidates.moc"
