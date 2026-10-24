// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-15 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code: a
// saved Core whose learned addresses are IPv6 only still moves to its own
// IPv4 address.
// 2026-10-22 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code: a
// saved paired Core's race also dials the addresses it announces on this
// network (aSavedPairedCoreAlsoDialsWhereItAnnouncesItself).
// 2026-10-22: ports a test hands to a later listener come from
// unusedLoopbackPort() (UnusedLoopbackPort.h), below the kernel's ephemeral
// range, so a parallel test cannot take one between the pick and the listen.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "UnusedLoopbackPort.h"
#include <QtTest>
#include "core/session/RemoteStationOptions.h"
#include "gui/GuiConnectionController.h"
#include "RendezvousTestHarness.h"
#include "gui/RemoteConnectionController.h"
#include "core/session/RendezvousWire.h"
#include "core/session/StationRendezvous.h"
#include "core/session/PathRacer.h"
#include "core/session/StationLanAnnouncement.h"
#include "core/settings/SettingsProxy.h"
using namespace NereusSDR;

namespace {
// A refused loopback dial reports in milliseconds on Linux and macOS but
// takes about 3.4 s on Windows, which retries the SYN after the reset.
constexpr int kRefusedDialMs = 8000;
}
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
    // 2026-10-22, seen live: a Core saved for remote access, listed under
    // Cores on this network at 192.168.109.19:47910, was raced through the
    // remote access service alone on every retry ("Race started with 1
    // attempt(s)"). Its announcement now joins the race when it carries
    // the saved identity; another identity, or a pin alone, does not.
    void aSavedPairedCoreAlsoDialsWhereItAnnouncesItself() {
        SavedCoreTarget target; target.id = QStringLiteral("strix");
        target.connection.identityFingerprint = QByteArray(kStationLanIdentityBytes, 'i');
        target.connection.reachFromAnywhere = true;
        target.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        target.manualAddresses = {QStringLiteral("wss://saved.test:47910")};
        StationLanEndpoint same;
        // The announcement's certificate pin; a Core paired by identity
        // saves none (RemoteStationOptions::isValidRemoteTarget).
        const QString pin = QStringLiteral("AB:").repeated(31) + QStringLiteral("AB");
        same.announcement = {47910, pin, QStringLiteral("STRIX"), {},
            QStringLiteral("00:00:00:00:00:00"), false};
        same.announcement.schema = kStationLanAnnouncementSchema2;
        same.announcement.identity = target.connection.identityFingerprint;
        same.announcement.claimed = true;
        same.address = QHostAddress(QStringLiteral("192.168.109.19"));
        QVERIFY(!encodeStationLanAnnouncement(same.announcement).isEmpty());
        StationLanEndpoint sameOverIpv6 = same;
        sameOverIpv6.address = QHostAddress(QStringLiteral("2001:db8::19"));
        StationLanEndpoint another = same;
        another.announcement.identity = QByteArray(kStationLanIdentityBytes, 'x');
        another.address = QHostAddress(QStringLiteral("192.168.109.20"));
        StationLanEndpoint pinOnly = same;
        pinOnly.announcement.schema = kStationLanAnnouncementSchema1;
        pinOnly.announcement.identity.clear();
        pinOnly.announcement.claimed = false;
        pinOnly.address = QHostAddress(QStringLiteral("192.168.109.21"));

        const RemoteStationOptions saved = GuiConnectionController::connectionOptionsForTarget(target);
        QCOMPARE(saved.directCandidates, QStringList({QStringLiteral("wss://saved.test:47910")}));
        QVERIFY(GuiConnectionController::isReadyToConnect(saved));
        const RemoteStationOptions raced = GuiConnectionController::withAdvertisedAddresses(
            saved, {another, same, pinOnly, sameOverIpv6, same});
        QCOMPARE(raced.directCandidates,
                 QStringList({QStringLiteral("wss://saved.test:47910"),
                              QStringLiteral("wss://192.168.109.19:47910"),
                              QStringLiteral("wss://[2001:db8::19]:47910")}));
        QVERIFY(GuiConnectionController::isReadyToConnect(raced));

        // Without a saved identity, or saved to connect unpinned, nothing
        // announced is added.
        RemoteStationOptions unpinned = saved;
        unpinned.allowUnpinned = true;
        QCOMPARE(GuiConnectionController::withAdvertisedAddresses(unpinned, {same}).directCandidates,
                 unpinned.directCandidates);
        RemoteStationOptions unpaired = saved;
        unpaired.identityFingerprint.clear();
        QCOMPARE(GuiConnectionController::withAdvertisedAddresses(unpaired, {same}).directCandidates,
                 unpaired.directCandidates);
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
        QTRY_VERIFY_WITH_TIMEOUT(reads >= 2, kRefusedDialMs);
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
        const quint16 closedPort = unusedLoopbackPort();
        QVERIFY(closedPort != 0);
        const QUrl stale(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort));
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
        QTRY_VERIFY_WITH_TIMEOUT(!retries.isEmpty(), kRefusedDialMs);
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 3000);
        QCOMPARE(client.connectedUrl(), core.url());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A saved Core whose learned Core addresses are IPv6 only (a Core behind
    // a home router lists no private IPv4, CoreAddresses::dialable) and
    // whose own address is the IPv4 listener that answers. The IPv6
    // addresses fail at once, as on a computer with no IPv6 route home; the
    // service wins; then the IPv4 listener opens and the better-path search
    // must move the session to it.
    void ipv6OnlyLearnedAddressesStillReachTheEntryAddress() {
        using namespace NereusSDR::Test::Rendezvous;
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        const quint16 port = freeTcpPort();
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        AppSettings::instance().setValue(QStringLiteral("RemoteAccessServers"), service.url().toString());
        const auto restore = qScopeGuard([] {
            AppSettings::instance().setValue(QStringLiteral("RemoteAccessServers"), QStringLiteral("ws://127.0.0.1:1"));
        });
        QTemporaryDir keyDir;
        const auto key = std::make_shared<const ClientDeviceIdentity>(ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));

        SavedCoreTarget target;
        target.id = QStringLiteral("legacy-core");
        target.label = QStringLiteral("127.0.0.1");
        target.connection.url = QStringLiteral("wss://127.0.0.1:%1").arg(port);
        target.connection.identityFingerprint = core.server->stationIdentity().fingerprint();
        target.connection.rendezvousId = rendezvous.client()->stationId();
        target.connection.relayAllowed = 1;
        target.connection.controlChannelVersion = 1;
        const QString v6a = QStringLiteral("wss://[::1]:%1").arg(freeTcpPort());
        const QString v6b = QStringLiteral("wss://[::1]:%1").arg(freeTcpPort());
        target.connection.coreAddresses = {v6a, v6b};
        target.connection.cachedAddresses = {v6a, v6b, target.connection.url};

        RadioModel remote{RadioModel::Role::Remote};
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setDeviceIdentity(key, QStringLiteral("Test computer"));
        client.setHeartbeatIntervalMs(0);
        client.setUpgradeScheduleForTest({300});
        QSignalSpy moved(&client, &StationClient::pathChanged);
        RemoteConnectionController controls(&client, &remote, target.connection);
        controls.setCurrentOptionsSource([&] {
            RemoteStationOptions current = GuiConnectionController::connectionOptionsForTarget(target);
            return std::optional(current);
        });
        QVERIFY(controls.canConnect());
        controls.connectToStation();
        QString why;
        QVERIFY2(waitForHandshake(client, kServiceConnectBudgetMs, &why), qPrintable(why));
        QVERIFY2(client.pathRank() > int(PathRacer::ThisNetwork),
                 qPrintable(client.connectionAttempt().summary()));

        QVERIFY(core.server->listen(QHostAddress::LocalHost, port));
        QTRY_COMPARE_WITH_TIMEOUT(moved.size(), 1, 30000);
        QCOMPARE(client.pathRank(), int(PathRacer::ThisNetwork));
        QCOMPARE(client.connectedUrl(), QUrl(target.connection.url));
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

};
QTEST_MAIN(TstCoreConnectionCandidates)
#include "tst_core_connection_candidates.moc"
