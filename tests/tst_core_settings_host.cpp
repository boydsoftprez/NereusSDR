// no-port-check: NereusSDR-original host integration regressions.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest/QtTest>
#include <QApplication>
#include <QFile>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QDir>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include "gui/CoreSettingsHost.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/session/RemoteDevicesState.h"
#include "fakes/LoopbackTransport.h"
#include "core/AppSettings.h"
#include "core/session/StationClient.h"
#include "gui/GuiConnectionController.h"
#include "gui/MainWindow.h"
#include "gui/RemoteConnectionController.h"
#include "gui/SetupDialog.h"
#include "gui/setup/CoresSetupPage.h"
#include "fakes/MainWindowTestSettings.h"
using namespace NereusSDR;
namespace {
SavedCoreTarget saved(const QString& id, char identity) {
    SavedCoreTarget t;
    t.id = id; t.label = id;
    t.connection.url = QStringLiteral("wss://host.invalid:41000");
    t.connection.identityFingerprint = QByteArray(32, identity);
    t.autoConnect = false;
    return t;
}
QPushButton* push(QWidget& page, const char* name) {
    return page.findChild<QPushButton*>(QString::fromLatin1(name));
}
bool hasText(QWidget& page, const QString& text) {
    for (QLabel* label : page.findChildren<QLabel*>()) {
        if (label->text().contains(text)) { return true; }
    }
    return false;
}
void submit(CoresSetupPage& page, const QString& name = QStringLiteral("KG4VCF/New")) {
    push(page, "renameCore")->click();
    page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->setText(name);
    push(page, "saveCoreName")->click();
}
struct HostFixture {
    QTemporaryDir dir;
    AppSettings settings{dir.filePath(QStringLiteral("settings.xml"))};
    CoreTargetStore store{settings};
    StationIdentity core = StationIdentity::loadOrCreate(dir.filePath(QStringLiteral("core")));
    std::shared_ptr<const ClientDeviceIdentity> device = std::make_shared<const ClientDeviceIdentity>(
        ClientDeviceIdentity::loadOrCreate(dir.filePath(QStringLiteral("device"))));
    StationClient live{nullptr, nullptr};
    QPointer<Test::LoopbackTransport> far;
    QPointer<StationClient> temporary;
    QList<SessionMessage> commands;
    std::unique_ptr<CoreSettingsHost> host;
    bool borrow = true;
    bool ordinaryPending = false;
    quint64 generation = 17;
    int temporaryCount = 0;
    bool validProof = false;
    void attach(StationClient& client) {
        auto* near = new Test::LoopbackTransport(QStringLiteral("host fake client"));
        auto* peer = new Test::LoopbackTransport(QStringLiteral("host signed fake Core"), &client);
        near->linkTo(peer);
        const QByteArray certificate(32, 'c');
        const QByteArray challenge(32, 'q');
        near->setPeerCertificateSha256(certificate);
        far = peer;
        QObject::connect(peer, &SessionTransport::textReceived, &client,
            [this, peer, certificate, challenge](const QByteArray& bytes) {
                SessionMessage message;
                if (!SessionMessages::decode(bytes, &message)) { return; }
                if (message.kind == SessionMessageKind::AuthRequest && message.device) {
                    const auto& d = *message.device;
                    const QByteArray spki = StationIdentity::fromBase64Url(d.publicKey);
                    validProof = StationIdentity::verify(spki,
                        DeviceAuthenticator::transcript(challenge, certificate, core.publicKeySpki(), spki),
                        StationIdentity::fromBase64Url(d.signature));
                    peer->sendText(SessionMessages::encode(SessionMessages::authResult(validProof, {}, false)));
                    StationCapabilities caps;
                    caps.radioIdentityEntries = true;
                    caps.deviceAdminVersion = 1;
                    caps.sessionHolderEntry = true;
                    caps.sessionHolderVersion = 1;
                    peer->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
                    peer->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
                } else if (message.kind == SessionMessageKind::CommandInvoke) { commands.append(message); }
            });
        client.setDeviceIdentity(device, QStringLiteral("Existing fake computer"));
        client.startSession(near, {}, {}, core.fingerprint());
        SessionMessage hello = SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 0,
            QStringLiteral("Untrusted hello name"), LinkVersion::supportedMajors(), {{QByteArrayLiteral("deviceAuth"), 1}});
        hello.challenge = StationIdentity::toBase64Url(challenge);
        hello.stationIdentity = SessionStationIdentity{StationIdentity::toBase64Url(core.publicKeySpki()),
            StationIdentity::toBase64Url(core.certBinding(certificate))};
        peer->sendText(SessionMessages::encode(hello));
    }
    bool init(bool borrowed = true, bool keyLoaded = true) {
        borrow = borrowed;
        if (!store.load() || !store.upsert(saved(QStringLiteral("one"), 'a'))
            || !store.upsert(saved(QStringLiteral("two"), 'b'))) { return false; }
        auto target = *store.target(QStringLiteral("one"));
        target.connection.identityFingerprint = core.fingerprint();
        if (!store.upsert(target)) { return false; }
        if (borrow) { attach(live); }
        host = std::make_unique<CoreSettingsHost>(store, [this] {
            CoreSettingsContext context;
            context.targetId = QStringLiteral("one");
            context.pairedIdentity = core.fingerprint();
            context.epoch = generation;
            context.authenticated = borrow && live.isHandshakeComplete();
            context.coreName = live.remoteDevices()->coreInfo().stationLabel;
            return context;
        }, [this](const QByteArray& identity) {
            return CoreRenameController::Snapshot{
                borrow && identity == core.fingerprint() ? &live : nullptr,
                identity == core.fingerprint() && (ordinaryPending || (borrow && live.isConnectionActive())),
                generation, device->fingerprint()};
        }, nullptr, [this] {
            ++temporaryCount;
            auto client = std::make_unique<StationClient>(nullptr, nullptr, nullptr,
                LinkVersion::supportedMajors(), StationClient::SessionPurpose::RenameOnly);
            temporary = client.get();
            return client;
        }, [this](StationClient& client, const RemoteStationOptions&) { attach(client); },
        CoreRenameController::Limits{1000, 1000, 20, 500});
        if (keyLoaded) { host->setExistingDeviceIdentity(device); }
        return true;
    }
    void answer(const SessionMessage& message, bool accepted, const QString& reason = {}) {
        if (far) { far->sendText(SessionMessages::encode(SessionMessages::commandResult(
            message.commandVerb, message.commandId, accepted, reason, {}))); }
    }
    QList<SessionMessage> renames() const {
        QList<SessionMessage> result;
        for (const auto& command : commands) {
            if (command.commandVerb == QByteArrayLiteral("station.rename")) { result.append(command); }
        }
        return result;
    }
};
}
class TstCoreSettingsHost final : public QObject {
    Q_OBJECT
private slots:
    void init() {
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        Test::markAudioFirstRunDone();
        QVERIFY(AppSettings::instance().save());
    }
    void existingOnlyKeyLoaderNeverMutatesOrRegeneratesPathsAndKeepsSigningIdentity() {
        QTemporaryDir directory;
        const QString missing = directory.filePath(QStringLiteral("absent/nested"));
        const ClientDeviceIdentity absent = ClientDeviceIdentity::loadExisting(missing);
        QVERIFY(!absent.isValid()); QVERIFY(!QFileInfo::exists(missing));
        const QString invalidDirectory = directory.filePath(QStringLiteral("invalid"));
        QVERIFY(QDir().mkpath(invalidDirectory));
        const QString invalidPath = QDir(invalidDirectory).filePath(QString::fromLatin1(ClientDeviceIdentity::kKeyFileName));
        QFile invalid(invalidPath); QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write("invalid P-256 fixture"); invalid.close();
        const auto invalidMode = QFile::permissions(invalidPath);
        QVERIFY(!ClientDeviceIdentity::loadExisting(invalidDirectory).isValid());
        QVERIFY(invalid.open(QIODevice::ReadOnly)); QCOMPARE(invalid.readAll(), QByteArray("invalid P-256 fixture")); invalid.close();
        QCOMPARE(QFile::permissions(invalidPath), invalidMode);
        const QString validDirectory = directory.filePath(QStringLiteral("valid"));
        const ClientDeviceIdentity original = ClientDeviceIdentity::loadOrCreate(validDirectory);
        QVERIFY(original.isValid());
        QVERIFY(QFile::setPermissions(original.keyPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup));
        const auto mode = QFile::permissions(original.keyPath());
        const ClientDeviceIdentity loaded = ClientDeviceIdentity::loadExisting(validDirectory);
        QVERIFY(loaded.isValid()); QVERIFY(!loaded.wasCreatedThisRun());
        QCOMPARE(loaded.fingerprint(), original.fingerprint());
        QCOMPARE(loaded.publicKeySpki(), original.publicKeySpki());
        const QByteArray proof = QByteArrayLiteral("Core Settings existing-key proof");
        QVERIFY(StationIdentity::verify(original.publicKeySpki(), proof, loaded.sign(proof)));
        QCOMPARE(QFile::permissions(original.keyPath()), mode);
    }
    void existingMissingProfileDoesNotPoisonOrdinarySharedCache() {
        // Also run alone in a fresh named process to exercise the empty synchronized cache.
        const QString directory = AppSettings::resolveConfigDir(AppSettings::profileOverride());
        const QString keyPath = QDir(directory).filePath(QString::fromLatin1(ClientDeviceIdentity::kKeyFileName));
        QVERIFY(!QFileInfo::exists(keyPath));
        QVERIFY(!ClientDeviceIdentity::existingForThisProfile());
        QVERIFY(!QFileInfo::exists(keyPath));
        const auto ordinary = ClientDeviceIdentity::forThisProfile();
        QVERIFY(ordinary && ordinary->isValid());
        const auto existing = ClientDeviceIdentity::existingForThisProfile();
        QCOMPARE(existing, ordinary);
    }
    void initialLocalUsesExistingProfileKeyWithoutCreatingIdentityOnInspection() {
        // Run as a named fresh-process causal regression: no prior ordinary client has loaded the cache.
        const QString directory = AppSettings::resolveConfigDir(AppSettings::profileOverride());
        const ClientDeviceIdentity key = ClientDeviceIdentity::loadOrCreate(directory);
        QVERIFY(key.isValid());
        QFile file(key.keyPath()); QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray original = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256); file.close();
        CoreTargetStore initial(AppSettings::instance());
        QVERIFY(initial.load()); QVERIFY(initial.upsert(saved(QStringLiteral("paired"), 'a')));
        QVERIFY(initial.select(QStringLiteral("local")));
        GuiConnectionController controller; controller.start({});
        MainWindow* window = controller.sessions()->window();
        QVERIFY(window && !window->findChild<StationClient*>());
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window, [&dialog](SetupDialog* created) { dialog = created; });
        window->openCoreSettings(QStringLiteral("paired"));
        QVERIFY(dialog);
        CoresSetupPage* page = dialog->findChild<CoresSetupPage*>();
        QVERIFY(page && page->inspectedIncarnation() != 0);
        QVERIFY2(push(*page, "renameCore")->isEnabled(), "A valid existing paired device key must support initial-local Settings rename without creating a key");
        const auto existing = ClientDeviceIdentity::existingForThisProfile();
        QVERIFY(existing && existing->isValid());
        QCOMPARE(existing->fingerprint(), key.fingerprint());
        QCOMPARE(existing, ClientDeviceIdentity::forThisProfile());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256), original); file.close();
        controller.shutdown();
    }
    void manageDestinationIsBoundBeforeInspectionWithoutRetargeting() {
        CoreTargetStore initial(AppSettings::instance());
        QVERIFY(initial.load());
        QVERIFY(initial.upsert(saved(QStringLiteral("one"), 'a')));
        QVERIFY(initial.upsert(saved(QStringLiteral("two"), 'b')));
        QVERIFY(initial.select(QStringLiteral("one")));
        GuiConnectionController controller;
        controller.start({});
        MainWindow* window = controller.sessions()->window();
        StationClient* client = window->findChild<StationClient*>();
        const quint64 generation = controller.sessions()->generation();
        controller.selector()->setSelectedKey(QStringLiteral("saved:two"));
        QPushButton* manage = controller.selector()->findChild<QPushButton*>(QStringLiteral("connectionSelectorManageCore"));
        QVERIFY(manage && manage->isEnabled());
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window, [&dialog](SetupDialog* created) { dialog = created; });
        manage->click();
        QVERIFY(dialog);
        CoresSetupPage* page = dialog->findChild<CoresSetupPage*>();
        QVERIFY(page);
        QCOMPARE(page->inspectedId(), QStringLiteral("two"));
        QCOMPARE(page->inspectedIncarnation(), controller.coreTargetStore().targetIncarnation(QStringLiteral("two")));
        QCOMPARE(controller.coreTargetStore().selectedId(), QStringLiteral("one"));
        QCOMPARE(controller.sessions()->generation(), generation);
        QCOMPARE(controller.sessions()->window(), window);
        QVERIFY(client && !client->isConnectionActive());
        controller.shutdown();
        QVERIFY(!manage->isEnabled());
    }
    void coreDetailsPresentsCurrentPanelWithoutGeneralConnect() {
        CoreTargetStore initial(AppSettings::instance());
        QVERIFY(initial.load());
        QVERIFY(initial.upsert(saved(QStringLiteral("one"), 'a')));
        QVERIFY(initial.select(QStringLiteral("one")));
        GuiConnectionController controller;
        controller.start({});
        MainWindow* window = controller.sessions()->window();
        QSignalSpy picker(window, &MainWindow::connectionsRequested);
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window, [&dialog](SetupDialog* created) { dialog = created; });
        window->openCoreSettings(QStringLiteral("one"));
        QVERIFY(dialog);
        dialog->coreConnectionDetailsRequested();
        RemoteConnectionPanel* panel = window->findChild<RemoteConnectionPanel*>();
        QVERIFY(panel && panel->isVisible());
        QCOMPARE(picker.size(), 0);
        QVERIFY(!window->findChild<StationClient*>()->isConnectionActive());
        // The general Connections page retains the existing picker route.
        dialog->connectionsRequested();
        QCOMPARE(picker.size(), 1);
        controller.shutdown();
    }
    void multiplePagesRouteCollidingIdsToExactOriginAndIgnoreIdleInspection() {
        HostFixture f;
        QVERIFY(f.init());
        QTRY_VERIFY_WITH_TIMEOUT(f.live.deviceAdminAvailable(), 1000);
        QVERIFY(f.validProof);
        CoresSetupPage first(&f.store), second(&f.store);
        f.host->bindPage(&first); f.host->bindPage(&second);
        first.show(); second.show();
        first.inspectTarget(QStringLiteral("one")); second.inspectTarget(QStringLiteral("one"));
        QSignalSpy firstRequests(&first, &CoresSetupPage::renameRequested);
        QSignalSpy secondRequests(&second, &CoresSetupPage::renameRequested);
        submit(first);
        QTRY_COMPARE_WITH_TIMEOUT(f.renames().size(), 1, 1000);
        second.inspectTarget(QStringLiteral("two"));
        f.host->refresh();
        QVERIFY(!push(first, "saveCoreName")->isEnabled());
        QCOMPARE(f.renames().size(), 1);
        second.inspectTarget(QStringLiteral("one"));
        submit(second);
        QTRY_COMPARE_WITH_TIMEOUT(f.renames().size(), 2, 1000);
        const auto one = qvariant_cast<CoreRenameRequest>(firstRequests.first().first());
        const auto two = qvariant_cast<CoreRenameRequest>(secondRequests.first().first());
        QCOMPARE(one.operationId, two.operationId);
        QVERIFY(one.epoch != two.epoch);
        QTRY_VERIFY(push(first, "saveCoreName")->isEnabled());
        QVERIFY(!push(second, "saveCoreName")->isEnabled());
        f.answer(f.renames().first(), true); // Late prior result cannot finish the second page.
        QCoreApplication::processEvents();
        QVERIFY(!push(second, "saveCoreName")->isEnabled());
        f.answer(f.renames().last(), false, QStringLiteral("Second rename refused"));
        QTRY_VERIFY(push(second, "saveCoreName")->isEnabled());
        QVERIFY(hasText(second, QStringLiteral("Second rename refused")));
        QVERIFY(!hasText(first, QStringLiteral("Second rename refused")));
        QVERIFY(f.live.isHandshakeComplete());
        QCOMPARE(f.temporaryCount, 0);
    }
    void ownPendingSurvivesRefreshButGenuineAuthorityLossRetiresIt() {
        HostFixture f; QVERIFY(f.init()); QTRY_VERIFY_WITH_TIMEOUT(f.live.deviceAdminAvailable(), 1000);
        CoresSetupPage page(&f.store);
        f.host->bindPage(&page); page.show();
        submit(page); QTRY_COMPARE_WITH_TIMEOUT(f.renames().size(), 1, 1000);
        f.host->refresh();
        QVERIFY(push(page, "renameCore")->isEnabled());
        QVERIFY(!push(page, "saveCoreName")->isEnabled());
        ++f.generation;
        f.host->refresh();
        QTRY_VERIFY(push(page, "saveCoreName")->isEnabled());
        f.answer(f.renames().first(), true);
        QCoreApplication::processEvents();
        QVERIFY(!f.store.target(QStringLiteral("one"))->lastKnownCoreName);
        QVERIFY(f.live.isHandshakeComplete());
    }
    void forgetRecreateAndFailedLoadNeverAuthorizeLateAnswer() {
        for (bool failedLoad : {false, true}) {
            HostFixture f; QVERIFY(f.init()); QTRY_VERIFY_WITH_TIMEOUT(f.live.deviceAdminAvailable(), 1000);
            CoresSetupPage page(&f.store); f.host->bindPage(&page); page.show();
            submit(page); QTRY_COMPARE_WITH_TIMEOUT(f.renames().size(), 1, 1000);
            const auto original = *f.store.target(QStringLiteral("one"));
            const quint64 old = f.store.targetIncarnation(original.id);
            if (failedLoad) {
                f.settings.setValue(QStringLiteral("ConnectionTargets/V3"), QStringLiteral("bad JSON"));
                QVERIFY(!f.store.load());
                QCOMPARE(f.store.targetIncarnation(original.id), quint64(0));
            } else {
                QVERIFY(f.store.remove(original.id)); QVERIFY(f.store.upsert(original));
                QVERIFY(f.store.targetIncarnation(original.id) != old);
            }
            f.host->refresh();
            f.answer(f.renames().first(), true);
            QCoreApplication::processEvents();
            QVERIFY(!f.store.target(original.id)->lastKnownCoreName);
            QVERIFY(f.live.isHandshakeComplete());
        }
    }
    void acceptedRenameWithLocalSaveFailureForwardsWarningWithoutClosingBorrowedSession() {
        HostFixture f; QVERIFY(f.init()); QTRY_VERIFY_WITH_TIMEOUT(f.live.deviceAdminAvailable(), 1000);
        CoresSetupPage page(&f.store); f.host->bindPage(&page); page.show();
        submit(page); QTRY_COMPARE_WITH_TIMEOUT(f.renames().size(), 1, 1000);
        QVERIFY(QFile::remove(f.settings.filePath())); QVERIFY(QDir().mkpath(f.settings.filePath()));
        f.answer(f.renames().first(), true);
        QTRY_VERIFY(hasText(page, QStringLiteral("accepted the rename")));
        QVERIFY(hasText(page, QStringLiteral("could not save")));
        QVERIFY(!page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->isVisible());
        QVERIFY(f.live.isHandshakeComplete());
        QVERIFY(!f.store.target(QStringLiteral("one"))->lastKnownCoreName);
    }
    void temporaryAdmissionExcludesOrdinaryConnectThroughActualRetirementAndRefreshesUi() {
        HostFixture f; QVERIFY(f.init(false));
        CoresSetupPage page(&f.store); f.host->bindPage(&page); page.show();
        QSignalSpy availability(f.host.get(), &CoreSettingsHost::admissionChanged);
        submit(page); QTRY_COMPARE_WITH_TIMEOUT(f.renames().size(), 1, 1000);
        QVERIFY(f.host->ownsTemporaryAdmission(f.core.fingerprint()));
        QVERIFY(!f.host->admissionUnavailableReason(f.core.fingerprint()).isEmpty());
        QVERIFY(push(page, "renameCore")->isEnabled()); // Own pending does not cancel itself.
        StationClient ordinary(nullptr, nullptr);
        RemoteStationOptions options = f.store.target(QStringLiteral("one"))->connection;
        RemoteConnectionController controls(&ordinary, nullptr, options);
        controls.setAdmissionUnavailableReasonSource([&] { return f.host->admissionUnavailableReason(f.core.fingerprint()); });
        QVERIFY(!controls.canConnect());
        QVERIFY(controls.detailText().contains(QStringLiteral("temporary rename")));
        controls.connectToStation(); QVERIFY(!ordinary.isConnectionActive());
        f.answer(f.renames().first(), true);
        QTRY_VERIFY(hasText(page, QStringLiteral("accepted")));
        QVERIFY(f.host->ownsTemporaryAdmission(f.core.fingerprint()));
        QVERIFY(!controls.canConnect());
        QTRY_VERIFY(!f.temporary);
        QTRY_VERIFY(!f.host->ownsTemporaryAdmission(f.core.fingerprint()));
        QTRY_VERIFY(availability.size() >= 3);
        QVERIFY(controls.canConnect());
        QVERIFY(!controls.detailText().contains(QStringLiteral("temporary rename")));
        QVERIFY(push(page, "renameCore")->isEnabled());
    }
    void currentNameAndInventoryAuthorityNeverBorrowAnotherCoreOrFailedLoadDisplay() {
        HostFixture f; QVERIFY(f.init()); QTRY_VERIFY_WITH_TIMEOUT(f.live.deviceAdminAvailable(), 1000);
        f.live.remoteDevices()->applyObject("devices", {{2, "stationLabel", MirrorWireKind::Utf8, QStringLiteral("KG4VCF/Actual")}});
        CoresSetupPage page(&f.store); f.host->bindPage(&page); page.show();
        page.inspectTarget(QStringLiteral("one"));
        QCOMPARE(page.findChild<QLabel*>(QStringLiteral("actualCoreName"))->text(), QStringLiteral("KG4VCF/Actual"));
        page.inspectTarget(QStringLiteral("two"));
        QVERIFY(page.findChild<QLabel*>(QStringLiteral("actualCoreName"))->text() != QStringLiteral("KG4VCF/Actual"));
        page.inspectTarget(QStringLiteral("one"));
        f.settings.setValue(QStringLiteral("ConnectionTargets/V3"), QStringLiteral("bad JSON"));
        QVERIFY(!f.store.load());
        f.host->refresh();
        QVERIFY(page.findChild<QLabel*>(QStringLiteral("actualCoreName"))->text() != QStringLiteral("KG4VCF/Actual"));
        QVERIFY(!push(page, "renameCore")->isEnabled());
    }
    void noLoadedKeyIsUnavailableAndTeardownRetiresTemporarySafely() {
        HostFixture f; QVERIFY(f.init(false, false));
        CoresSetupPage page(&f.store); f.host->bindPage(&page); page.show();
        QVERIFY(!push(page, "renameCore")->isEnabled());
        QVERIFY(hasText(page, QStringLiteral("has not been loaded")));
        QCOMPARE(f.temporaryCount, 0);
        f.host->setExistingDeviceIdentity(f.device);
        submit(page); QTRY_COMPARE_WITH_TIMEOUT(f.renames().size(), 1, 1000);
        QPointer<StationClient> temporary = f.temporary;
        f.host.reset();
        QVERIFY(!temporary);
        QVERIFY(!f.store.target(QStringLiteral("one"))->lastKnownCoreName);
        page.inspectTarget(QStringLiteral("two")); // Store/page remain valid after host teardown.
    }
    void candidateUnavailableReasonIsAccurateAndReentrantNewAttemptSurvives() {
        StationClient client(nullptr, nullptr);
        const QByteArray identity(32, 'a');
        client.setCandidateSource([]() -> std::optional<StationClient::ConnectionCandidates> { return std::nullopt; });
        client.setCandidateUnavailableReasonSource([] { return QStringLiteral("Temporary rename session is closing"); });
        client.connectToStation(QUrl(QStringLiteral("wss://unused.invalid:41000")), {}, {}, false, identity);
        QCOMPARE(client.lastError(), QStringLiteral("Temporary rename session is closing"));
        QVERIFY(!client.lastError().contains(QStringLiteral("saved Core changed")));
        HostFixture f; QVERIFY(f.init(false));
        client.setCandidateSource([]() -> std::optional<StationClient::ConnectionCandidates> { return std::nullopt; });
        client.setCandidateUnavailableReasonSource([&] {
            client.setCandidateUnavailableReasonSource({});
            client.setCandidateSource({});
            f.attach(client); // A newer signed session supersedes the old nullopt callback.
            return QStringLiteral("Old temporary close lease");
        });
        client.connectToStation(QUrl(QStringLiteral("wss://unused.invalid:41000")), {}, {}, false, f.core.fingerprint());
        QTRY_VERIFY(client.isHandshakeComplete());
        QVERIFY(f.validProof);
        QVERIFY(!client.lastError().contains(QStringLiteral("Old temporary")));
    }
    void lazyDialogUsesCanonicalStoreWithoutChangingSession() {
        CoreTargetStore initial(AppSettings::instance());
        QVERIFY(initial.load());
        QVERIFY(initial.upsert(saved(QStringLiteral("one"), 'a')));
        QVERIFY(initial.select(QStringLiteral("one")));
        GuiConnectionController controller;
        controller.start({});
        MainWindow* window = controller.sessions()->window();
        QVERIFY(window);
        StationClient* client = window->findChild<StationClient*>();
        QVERIFY(client && !client->isConnectionActive());
        const quint64 generation = controller.sessions()->generation();
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window,
                [&dialog](SetupDialog* created) { dialog = created; });
        window->openCoreSettings(QStringLiteral("one"));
        QVERIFY(dialog);
        CoresSetupPage* page = dialog->findChild<CoresSetupPage*>();
        QVERIFY(page);
        QVERIFY(controller.coreTargetStore().upsert(saved(QStringLiteral("two"), 'b')));
        dialog->inspectCoreTarget(QStringLiteral("two"));
        QCOMPARE(page->inspectedIncarnation(), controller.coreTargetStore().targetIncarnation(QStringLiteral("two")));
        QVERIFY(page->inspectedIncarnation() != 0);
        QCOMPARE(controller.coreTargetStore().selectedId(), QStringLiteral("one"));
        QCOMPARE(controller.sessions()->selection().savedId, QStringLiteral("one"));
        QCOMPARE(controller.sessions()->generation(), generation);
        QCOMPARE(controller.sessions()->window(), window);
        QVERIFY(!client->isConnectionActive());
        controller.shutdown();
    }
};
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    AppSettings::setProfileOverride(QStringLiteral("core-settings-host-%1").arg(QCoreApplication::applicationPid()));
    TstCoreSettingsHost test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_core_settings_host.moc"
