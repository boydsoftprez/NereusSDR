// no-port-check: NereusSDR-original. R-R3-38 discovery is never trust.
#include <QtTest>
#include <QCheckBox>
#include <QFile>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QUdpSocket>
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/session/StationClient.h"
#include "core/security/DeviceStore.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/StationServer.h"
#include "gui/ConnectionSelector.h"
#include "gui/CoreTargetEditor.h"
#include "gui/GuiConnectionController.h"
#include "gui/MainWindow.h"
#include "gui/StationLanSelection.h"
#include "models/RadioModel.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/UpgradedCoreToken.h"
#include "OperatorWording.h"
using namespace NereusSDR;

namespace {
SavedCoreTarget savedCore(const QString& fingerprint, const QString& token)
{
    return {QStringLiteral("shack"), QStringLiteral("Saved Shack"),
        {QStringLiteral("wss://old-address.invalid:4711"), token, fingerprint, false}, {}, {}};
}
StationLanAnnouncement advertisement(StationServer& server)
{
    return {server.serverPort(), server.certificateFingerprint(), QStringLiteral("Test Core"),
        QStringLiteral("Saturn"), QStringLiteral("AA:BB:CC:DD:EE:01"), false};
}
QPushButton* connectButton(GuiConnectionController& controller)
{
    return controller.selector()->findChild<QPushButton*>(QStringLiteral("connectionSelectorConnect"));
}
}

class TestStationLanSelection : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("lan-selection-%1").arg(QCoreApplication::applicationPid()));
        QVERIFY(QSslSocket::supportsSsl());
    }
    void init()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        Test::markAudioFirstRunDone();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery guard;
        guard.holdOffScans(std::chrono::minutes{5});
    }
    void cleanup()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        RadioDiscovery::clearHoldOffForTest();
    }
    void cleanupTestCase()
    {
        QFile::remove(AppSettings::instance().filePath());
        QFile::remove(AppSettings::instance().filePath() + QStringLiteral(".bak"));
    }
    void matchingRequiresCompleteSavedPinAndDoesNotPickAmongDuplicates()
    {
        StationLanEndpoint endpoint;
        endpoint.announcement = {4711, QStringLiteral("AB:").repeated(31) + QStringLiteral("AB"),
            QStringLiteral("Core"), {}, QStringLiteral("00:00:00:00:00:00"), false};
        const auto correct = savedCore(endpoint.announcement.fingerprint.toLower(), QStringLiteral("token"));
        auto unpinned = correct;
        unpinned.connection.fingerprint.clear();
        unpinned.connection.allowUnpinned = true;
        auto noToken = correct;
        noToken.connection.token.clear();
        auto wrong = correct;
        wrong.connection.fingerprint[0] = QLatin1Char('C');
        const auto matches = matchingSavedCores(endpoint, {unpinned, noToken, wrong, correct});
        QCOMPARE(matches.size(), 1);
        QCOMPARE(matches.first().id, correct.id);
        auto duplicate = correct;
        duplicate.id = QStringLiteral("another-account");
        QCOMPARE(matchingSavedCores(endpoint, {correct, duplicate}).size(), 2);
        endpoint.announcement.fingerprint.clear();
        QVERIFY(matchingSavedCores(endpoint, {unpinned}).isEmpty());
    }
    void aSavedCoreThatForgotThisComputerOffersPair()
    {
        // Part C fix wave (R2-M1): found by its identity, announcing itself
        // unclaimed (reset from its console, or this computer removed), the
        // saved Core's LAN row offers Pair, not Connect.
        StationLanEndpoint endpoint;
        endpoint.announcement = {4711, QStringLiteral("AB:").repeated(31) + QStringLiteral("AB"),
            QStringLiteral("Core"), {}, QStringLiteral("00:00:00:00:00:00"), false};
        endpoint.announcement.schema = kStationLanAnnouncementSchema2;
        endpoint.announcement.identity = QByteArray(kStationLanIdentityBytes, '\x42');
        endpoint.announcement.label = QStringLiteral("KG4VCF");
        endpoint.announcement.claimed = false;
        endpoint.announcement.pairing = StationLanPairing::Click;
        QVERIFY(!encodeStationLanAnnouncement(endpoint.announcement).isEmpty());
        SavedCoreTarget paired = savedCore(QString(), QString());
        paired.connection.identityFingerprint = endpoint.announcement.identity;
        QCOMPARE(matchingSavedCores(endpoint, {paired}).size(), 1);

        ConnectionTargetRow row = GuiConnectionController::lanCoreRow(endpoint, {paired});
        QVERIFY(row.pairable);
        QVERIFY(!row.connectable);
        QCOMPARE(row.state, QStringLiteral("Not paired"));
        endpoint.announcement.pairing = StationLanPairing::Code;
        row = GuiConnectionController::lanCoreRow(endpoint, {paired});
        QVERIFY(row.pairable);
        QCOMPARE(row.state, QStringLiteral("Pairs with its code"));
        // Its pairing closed after too many wrong codes: neither.
        endpoint.announcement.pairing = StationLanPairing::Closed;
        row = GuiConnectionController::lanCoreRow(endpoint, {paired});
        QVERIFY(!row.pairable);
        QVERIFY(!row.connectable);
        QCOMPARE(row.state, QStringLiteral("Pairing is closed on the Core"));

        // Claimed (still paired with this computer, or with others): Connect.
        endpoint.announcement.claimed = true;
        endpoint.announcement.pairing = StationLanPairing::Code;
        row = GuiConnectionController::lanCoreRow(endpoint, {paired});
        QVERIFY(!row.pairable);
        QVERIFY(row.connectable);
        QCOMPARE(row.state, QStringLiteral("Saved, ready to connect"));
    }

    // iPhone app plan Task 25 (R-IOS-16): a Core waiting for a radio to be
    // chosen says so in the list, as the phone's does, before connecting.
    void aCoreWaitingForARadioSaysSo()
    {
        StationLanEndpoint endpoint;
        endpoint.announcement = {4711, QStringLiteral("AB:").repeated(31) + QStringLiteral("AB"),
            QStringLiteral("Core"), {}, QStringLiteral("00:00:00:00:00:00"), false};
        endpoint.announcement.schema = kStationLanAnnouncementSchema2;
        endpoint.announcement.identity = QByteArray(kStationLanIdentityBytes, '\x43');
        endpoint.announcement.devicesConnected = 0;
        endpoint.announcement.radio = StationLanRadio::Waiting;
        QVERIFY(!encodeStationLanAnnouncement(endpoint.announcement).isEmpty());
        QCOMPARE(GuiConnectionController::lanCoreRow(endpoint, {}).radioText,
                 QStringLiteral("Waiting for a radio"));
        endpoint.announcement.radio = StationLanRadio::Offline;
        QCOMPARE(GuiConnectionController::lanCoreRow(endpoint, {}).radioText,
                 QStringLiteral("Radio (advertised offline)"));
        endpoint.announcement.radio.reset();   // a Core from before the state
        QCOMPARE(GuiConnectionController::lanCoreRow(endpoint, {}).radioText,
                 QStringLiteral("Radio (advertised offline)"));
    }

    void detailsForAnUnclaimedCoreWhosePairingClosedSayWhereItReopens()
    {
        // Part C follow-up (R-IOS-08): the details text takes the same
        // branch the row does. An unclaimed Core whose pairing closed after
        // too many wrong codes is not "paired with other devices".
        StationLanAnnouncement advertised{4711, QStringLiteral("AB:").repeated(31)
            + QStringLiteral("AB"), QStringLiteral("Core"), {},
            QStringLiteral("00:00:00:00:00:00"), false};
        advertised.schema = kStationLanAnnouncementSchema2;
        advertised.identity = QByteArray(kStationLanIdentityBytes, '\x42');
        advertised.claimed = false;
        advertised.pairing = StationLanPairing::Closed;
        const QString closed = GuiConnectionController::lanCoreNextStep(advertised);
        QCOMPARE(closed, QStringLiteral("No device is paired with this Core. Its pairing "
                                        "closed after too many wrong codes. Run nereusd pairing "
                                        "open on the Core's computer to open it again."));
        QVERIFY(!closed.contains(QStringLiteral("paired with other devices")));
        QVERIFY2(OperatorWording::isPlain(closed), qPrintable(closed));

        // The other branches are unchanged.
        advertised.pairing = StationLanPairing::Click;
        QCOMPARE(GuiConnectionController::lanCoreNextStep(advertised),
                 QStringLiteral("No device is paired with this Core. Select Pair to pair "
                                "this computer with it."));
        advertised.pairing = StationLanPairing::Code;
        QCOMPARE(GuiConnectionController::lanCoreNextStep(advertised),
                 QStringLiteral("This Core pairs with its code. Select Pair and type the code "
                                "the Core shows."));
        advertised.claimed = true;
        advertised.pairing = StationLanPairing::Closed;
        QCOMPARE(GuiConnectionController::lanCoreNextStep(advertised),
                 QStringLiteral("This Core is paired with other devices. Open pairing on the "
                                "Core, or on a device paired with it, then add it by code."));
    }

    void discoveredAddressUsesSavedPinWithoutRewritingAddress()
    {
        QTemporaryDir directory;
        AppSettings stationSettings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel radio;
        StationServer server(&radio, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        const auto saved = savedCore(server.certificateFingerprint(), server.token());
        CoreTargetStore store(AppSettings::instance());
        QVERIFY(store.load());
        QVERIFY(store.upsert(saved));
        QVERIFY(store.select(QStringLiteral("local")));
        GuiConnectionController controller;
        controller.start({});
        auto* discovery = controller.findChild<StationLanDiscovery*>();
        QVERIFY(discovery && discovery->start(0));
        controller.showConnections();
        QUdpSocket sender;
        const auto bytes = encodeStationLanAnnouncement(advertisement(server));
        QCOMPARE(sender.writeDatagram(bytes, QHostAddress::LocalHost, discovery->port()), bytes.size());
        QTRY_COMPARE(discovery->endpoints().size(), 1);
        controller.selector()->setSelectedKey(QStringLiteral("lan:") + discovery->endpoints().first().key());
        QVERIFY(connectButton(controller)->isEnabled());
        connectButton(controller)->click();
        QTRY_VERIFY(server.hasAuthenticatedSession());
        StationClient* client = controller.sessions()->window()->findChild<StationClient*>();
        QVERIFY(client);
        QTRY_VERIFY(client->isHandshakeComplete());
        const quint64 generation = controller.sessions()->generation();
        QTRY_VERIFY(!connectButton(controller)->isEnabled());
        connectButton(controller)->click();
        QCoreApplication::processEvents();
        QCOMPARE(controller.sessions()->generation(), generation);
        auto* disconnectButton = controller.selector()->findChild<QPushButton*>(QStringLiteral("connectionSelectorDisconnect"));
        QVERIFY(disconnectButton && disconnectButton->isEnabled());
        disconnectButton->click();
        QTRY_VERIFY(!client->isHandshakeComplete());
        QTRY_VERIFY(connectButton(controller)->isEnabled());
        connectButton(controller)->click();
        QTRY_VERIFY(client->isHandshakeComplete());
        QCOMPARE(controller.sessions()->generation(), generation);
        QVERIFY(!controller.sessions()->window()->radioModel()->isConnected());
        QCOMPARE(controller.sessions()->selection().savedAddressBeforeDiscovery, saved.connection.url);
        QVERIFY(store.load());
        QCOMPARE(store.selectedId(), saved.id);
        QCOMPARE(store.target(saved.id)->connection.url, saved.connection.url);
        QCOMPARE(store.target(saved.id)->connection.fingerprint, saved.connection.fingerprint);
        controller.shutdown();
    }
    void unknownAnnouncementNeverSeedsCredentialsOrDialsOnSelection()
    {
        QTemporaryDir directory;
        AppSettings stationSettings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel radio;
        StationServer server(&radio, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        GuiConnectionController controller;
        controller.start({});
        QPointer<MainWindow> original = controller.sessions()->window();
        auto* discovery = controller.findChild<StationLanDiscovery*>();
        QVERIFY(discovery && discovery->start(0));
        controller.showConnections();
        QUdpSocket sender;
        const auto bytes = encodeStationLanAnnouncement(advertisement(server));
        QCOMPARE(sender.writeDatagram(bytes, QHostAddress::LocalHost, discovery->port()), bytes.size());
        QTRY_COMPARE(discovery->endpoints().size(), 1);
        controller.selector()->setSelectedKey(QStringLiteral("lan:") + discovery->endpoints().first().key());
        bool inspected = false;
        QTimer::singleShot(0, &controller, [&] {
            // queueConnect runs first; its modal editor then delivers this.
            QTimer::singleShot(0, &controller, [&] {
                auto* editor = controller.selector()->findChild<CoreTargetEditor*>();
                if (!editor) { return; }
                inspected = true;
                QVERIFY(editor->target().connection.token.isEmpty());
                QVERIFY(editor->target().connection.fingerprint.isEmpty());
                QVERIFY(!editor->target().connection.allowUnpinned);
                QCOMPARE(editor->target().connection.url, discovery->endpoints().first().url().toString());
                editor->reject();
            });
        });
        connectButton(controller)->click();
        QTRY_VERIFY(inspected);
        QVERIFY(original && controller.sessions()->window() == original);
        QVERIFY(!server.hasAuthenticatedSession());
        CoreTargetStore store(AppSettings::instance());
        QVERIFY(store.load());
        QVERIFY(store.targets().isEmpty());
        controller.shutdown();
    }
    void coresAreListedByLabelOrElseByName()
    {
        // iPhone app Task 16: a schema-2 Core is listed by its label; a
        // Core from before Task 16 (schema 1) by its name.
        GuiConnectionController controller;
        controller.start({});
        auto* discovery = controller.findChild<StationLanDiscovery*>();
        QVERIFY(discovery && discovery->start(0));
        controller.showConnections();
        StationLanAnnouncement labelled{4711, QStringLiteral("AB:").repeated(31) + QStringLiteral("AB"),
            QStringLiteral("Test Core"), {}, QStringLiteral("00:00:00:00:00:00"), false};
        labelled.schema = kStationLanAnnouncementSchema;
        labelled.identity = QByteArray(kStationLanIdentityBytes, '\x11');
        labelled.label = QStringLiteral("KG4VCF/shack");
        labelled.pairing = StationLanPairing::Click;
        StationLanAnnouncement older{4712, QStringLiteral("CD:").repeated(31) + QStringLiteral("CD"),
            QStringLiteral("Older Core"), {}, QStringLiteral("00:00:00:00:00:00"), false};
        QUdpSocket sender;
        for (const StationLanAnnouncement& packet : {labelled, older}) {
            const auto bytes = encodeStationLanAnnouncement(packet);
            QVERIFY(!bytes.isEmpty());
            QCOMPARE(sender.writeDatagram(bytes, QHostAddress::LocalHost, discovery->port()), bytes.size());
        }
        QTRY_COMPARE(discovery->endpoints().size(), 2);
        auto* tree = controller.selector()->findChild<QTreeWidget*>(QStringLiteral("connectionSelectorTargets"));
        QVERIFY(tree);
        const auto listed = [tree](const QString& text) {
            for (int group = 0; group < tree->topLevelItemCount(); ++group) {
                for (int row = 0; row < tree->topLevelItem(group)->childCount(); ++row) {
                    if (tree->topLevelItem(group)->child(row)->text(0) == text) {
                        return true;
                    }
                }
            }
            return false;
        };
        QTRY_VERIFY(listed(QStringLiteral("KG4VCF/shack (advertised)")));
        QVERIFY(listed(QStringLiteral("Older Core (advertised)")));
        QVERIFY(!listed(QStringLiteral("Test Core (advertised)")));
        controller.shutdown();
    }
    void spoofedKnownAnnouncementStillFailsTlsPin()
    {
        QTemporaryDir directory;
        AppSettings stationSettings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel radio;
        StationServer server(&radio, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        auto packet = advertisement(server);
        packet.fingerprint[0] = packet.fingerprint[0] == QLatin1Char('A') ? QLatin1Char('B') : QLatin1Char('A');
        const auto saved = savedCore(packet.fingerprint, QStringLiteral("must-not-be-sent"));
        CoreTargetStore store(AppSettings::instance());
        QVERIFY(store.load());
        QVERIFY(store.upsert(saved));
        GuiConnectionController controller;
        controller.start({});
        auto* discovery = controller.findChild<StationLanDiscovery*>();
        QVERIFY(discovery && discovery->start(0));
        controller.showConnections();
        QUdpSocket sender;
        const auto bytes = encodeStationLanAnnouncement(packet);
        QCOMPARE(sender.writeDatagram(bytes, QHostAddress::LocalHost, discovery->port()), bytes.size());
        QTRY_COMPARE(discovery->endpoints().size(), 1);
        controller.selector()->setSelectedKey(QStringLiteral("lan:") + discovery->endpoints().first().key());
        connectButton(controller)->click();
        QTRY_VERIFY(controller.sessions()->window()->findChild<StationClient*>());
        StationClient* client = controller.sessions()->window()->findChild<StationClient*>();
        QTRY_VERIFY(client->lastError().contains(QStringLiteral("does not match")));
        QVERIFY(!client->lastError().contains(saved.connection.fingerprint));
        QVERIFY(!client->lastError().contains(server.certificateFingerprint()));
        QVERIFY(!client->isHandshakeComplete());
        QVERIFY(!client->isReconnectPending());
        QVERIFY(!server.hasAuthenticatedSession());
        QVERIFY(store.load());
        QCOMPARE(store.target(saved.id)->connection.fingerprint, saved.connection.fingerprint);
        controller.shutdown();
    }
    // iPhone app Task 18 (R-IOS-08): a Core on this network that no
    // device has paired with offers Pair. One click pairs this computer
    // over TLS, saves the Core under Your Cores by its identity and label,
    // and connects to it by this computer's key: no token and no pin.
    void unclaimedCorePairsWithOneClickThenConnectsByKey()
    {
        QTemporaryDir directory;
        AppSettings stationSettings(directory.filePath(QStringLiteral("station.settings")));
        stationSettings.setValue(QStringLiteral("StationCallsign"), QStringLiteral("KG4VCF"));
        RadioModel radio;
        StationServer server(&radio, stationSettings,
                             NereusSDR::Test::seedCoreIdentity(directory.path()));
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        GuiConnectionController controller;
        controller.start({});
        auto* discovery = controller.findChild<StationLanDiscovery*>();
        QVERIFY(discovery && discovery->start(0));
        controller.showConnections();
        StationLanAnnouncement packet = advertisement(server);
        packet.schema = kStationLanAnnouncementSchema;
        packet.claimed = false;
        packet.identity = server.stationIdentity().fingerprint();
        packet.label = QStringLiteral("KG4VCF");
        packet.pairing = StationLanPairing::Click;
        QUdpSocket sender;
        const auto bytes = encodeStationLanAnnouncement(packet);
        QCOMPARE(sender.writeDatagram(bytes, QHostAddress::LocalHost, discovery->port()), bytes.size());
        QTRY_COMPARE(discovery->endpoints().size(), 1);
        const QString key = QStringLiteral("lan:") + discovery->endpoints().first().key();
        controller.selector()->setSelectedKey(key);
        QCOMPARE(connectButton(controller)->text(), QStringLiteral("Pair"));
        QVERIFY(connectButton(controller)->isVisible() || !controller.selector()->isVisible());
        connectButton(controller)->click();

        QTRY_VERIFY_WITH_TIMEOUT(server.hasAuthenticatedSession(), 20000);
        StationClient* client = controller.sessions()->window()->findChild<StationClient*>();
        QVERIFY(client);
        QTRY_VERIFY(client->isHandshakeComplete());
        QCOMPARE(client->stationIdentityFingerprint(), server.stationIdentity().fingerprint());
        QCOMPARE(server.deviceStore()->list().size(), 1);
        QCOMPARE(server.deviceStore()->list().first().kind, QStringLiteral("computer"));
        QVERIFY(!server.deviceStore()->list().first().enrolledThroughToken);

        CoreTargetStore store(AppSettings::instance());
        QVERIFY(store.load());
        QCOMPARE(store.targets().size(), 1);
        const SavedCoreTarget saved = store.targets().first();
        QCOMPARE(saved.label, QStringLiteral("KG4VCF"));
        QCOMPARE(saved.connection.identityFingerprint, server.stationIdentity().fingerprint());
        QVERIFY(saved.connection.token.isEmpty());
        QCOMPARE(store.selectedId(), saved.id);
        QCOMPARE(QUrl(saved.connection.url).port(), int(server.serverPort()));
        // The announcement now matches the saved Core by its identity.
        QCOMPARE(matchingSavedCores(discovery->endpoints().first(), store.targets()).size(), 1);
        controller.shutdown();
    }

    // An existing saved Core (address, token and pin) enrols this
    // computer's key on its next sign-in, with nothing typed, shows as
    // paired, and connects by key afterwards: even once its token is
    // retired.
    void savedCoreEnrolsOnItsNextSignInThenConnectsByKey()
    {
        QTemporaryDir directory;
        AppSettings stationSettings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel radio;
        StationServer server(&radio, stationSettings,
                             NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        SavedCoreTarget saved = savedCore(server.certificateFingerprint(), server.token());
        saved.connection.url = QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort());
        {
            CoreTargetStore store(AppSettings::instance());
            QVERIFY(store.load());
            QVERIFY(store.upsert(saved));
            QVERIFY(store.select(QStringLiteral("local")));
        }
        QCOMPARE(GuiConnectionController::savedCoreRow(saved, true).state,
                 QStringLiteral("Disconnected"));
        GuiConnectionController controller;
        controller.start({});
        controller.showConnections();
        controller.selector()->setSelectedKey(QStringLiteral("saved:") + saved.id);
        connectButton(controller)->click();
        QTRY_VERIFY_WITH_TIMEOUT(server.hasAuthenticatedSession(), 20000);
        StationClient* client = controller.sessions()->window()->findChild<StationClient*>();
        QVERIFY(client);
        QTRY_VERIFY(client->isHandshakeComplete());
        QTRY_VERIFY(!client->stationIdentityFingerprint().isEmpty());
        QCOMPARE(server.deviceStore()->list().size(), 1);
        QVERIFY(server.deviceStore()->list().first().enrolledThroughToken);

        CoreTargetStore store(AppSettings::instance());
        QTRY_VERIFY(store.load()
                    && !store.target(saved.id)->connection.identityFingerprint.isEmpty());
        const SavedCoreTarget enrolled = *store.target(saved.id);
        QCOMPARE(enrolled.connection.identityFingerprint, server.stationIdentity().fingerprint());
        // Its trust details are kept as they were.
        QCOMPARE(enrolled.connection.url, saved.connection.url);
        QCOMPARE(enrolled.connection.fingerprint, saved.connection.fingerprint);
        QCOMPARE(GuiConnectionController::savedCoreRow(enrolled, true).state,
                 QStringLiteral("Paired"));
        QCOMPARE(controller.sessions()->selection().connection.identityFingerprint,
                 enrolled.connection.identityFingerprint);

        // Retiring the token ends this token sign-in with pairingRequired;
        // Connect then signs in by key, and no new window is made.
        const quint64 generation = controller.sessions()->generation();
        QVERIFY(server.devicesFacade()->retireToken().accepted);
        QTRY_VERIFY(!client->isConnectionActive());
        QCOMPARE(client->lastEndReport().kind, StationEndReport::Kind::PairingRequired);
        controller.selector()->setSelectedKey(QStringLiteral("saved:") + saved.id);
        QTRY_VERIFY(connectButton(controller)->isEnabled());
        connectButton(controller)->click();
        QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 20000);
        QCOMPARE(controller.sessions()->generation(), generation);
        QCOMPARE(server.deviceStore()->list().size(), 1);
        controller.shutdown();
    }
};
QTEST_MAIN(TestStationLanSelection)
#include "tst_station_lan_selection.moc"
