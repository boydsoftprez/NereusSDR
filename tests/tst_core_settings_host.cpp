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
#include "gui/setup/CoreAudioSetupPage.h"
#include "gui/RemoteMediaController.h"
#include "gui/RemoteAudioWidget.h"
#include <QComboBox>
#include <QTimer>
#include <QWheelEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QScopeGuard>
#include "gui/widgets/GuardedSlider.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/session/RemoteDevicesState.h"
#include "fakes/LoopbackTransport.h"
#include "core/AppSettings.h"
#include "core/session/StationClient.h"
#include "gui/GuiConnectionController.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
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
    void attach(StationClient& client, bool enrollment = false, bool preserveDevice = false) {
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
        if (!preserveDevice) { client.setDeviceIdentity(device, QStringLiteral("Existing fake computer")); }
        client.startSession(near, enrollment ? QStringLiteral("legacy-token") : QString(),
            enrollment ? QString::fromLatin1(certificate.toHex(':').toUpper()) : QString(), enrollment ? QByteArray() : core.fingerprint());
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
    void sharedCoreAudioDestinationExistsWithoutDialingOrChangingSelection() {
        CoreTargetStore initial(AppSettings::instance());
        QVERIFY(initial.load()); QVERIFY(initial.upsert(saved(QStringLiteral("one"), 'a')));
        QVERIFY(initial.select(QStringLiteral("one")));
        GuiConnectionController controller; controller.start({});
        MainWindow* window = controller.sessions()->window();
        const quint64 generation = controller.sessions()->generation();
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window,
                [&dialog](SetupDialog* created) { dialog = created; });
        window->openCoreSettings(QStringLiteral("one")); QVERIFY(dialog);
        auto* tree = dialog->findChild<QTreeWidget*>(); QVERIFY(tree);
        bool found = false;
        for (QTreeWidgetItemIterator item(tree); *item; ++item) {
            if ((*item)->text(0) == QStringLiteral("Audio with the Core") && (*item)->parent()
                && (*item)->parent()->text(0) == QStringLiteral("Cores")) { found = true; }
        }
        QVERIFY2(found, "Cores must register the shared current-window Audio destination");
        dialog->selectPage(QStringLiteral("Audio with the Core"));
        auto* audioPage = dialog->findChild<CoreAudioSetupPage*>(); QVERIFY(audioPage);
        auto* quality = audioPage->findChild<QComboBox*>(QStringLiteral("remoteAudioQuality")); QVERIFY(quality);
        QCOMPARE(quality->count(), 3);
        QVERIFY(hasText(*audioPage, QStringLiteral("No authenticated Core")));
        dialog->coreConnectionDetailsRequested();
        auto* panel = window->findChild<RemoteConnectionPanel*>(); QVERIFY(panel);
        auto* otherQuality = panel->findChild<QComboBox*>(QStringLiteral("remoteAudioQuality")); QVERIFY(otherQuality);
        quality->setCurrentIndex(1);
        QCOMPARE(window->findChild<RemoteMediaController*>()->audioQualityChoice(), RemoteAudioQualityChoice::SaveData);
        QCOMPARE(otherQuality->currentIndex(), 1);
        otherQuality->setCurrentIndex(0);
        QCOMPARE(quality->currentIndex(), 0);
        auto* timer = audioPage->findChild<QTimer*>(QStringLiteral("remoteAudioPanelTimer")); QVERIFY(timer);
        QVERIFY(timer->isActive());
        dialog->selectPage(QStringLiteral("Your Cores")); QVERIFY(!timer->isActive());
        QCOMPARE(controller.sessions()->generation(), generation);
        QCOMPARE(controller.coreTargetStore().selectedId(), QStringLiteral("one"));
        QVERIFY(!window->findChild<StationClient*>()->isConnectionActive());
        controller.shutdown();
    }
    void closedAudioQualityWheelNeverChangesComputerPreference_data() {
        QTest::addColumn<bool>("focused"); QTest::addColumn<bool>("locked");
        QTest::newRow("focused-unlocked") << true << false;
        QTest::newRow("unfocused-unlocked") << false << false;
        QTest::newRow("focused-locked") << true << true;
        QTest::newRow("unfocused-locked") << false << true;
    }
    void closedAudioQualityWheelNeverChangesComputerPreference() {
        QFETCH(bool, focused); QFETCH(bool, locked);
        const bool previousLock = ControlsLock::isLocked();
        const auto restore = qScopeGuard([previousLock] { ControlsLock::setLocked(previousLock); });
        ControlsLock::setLocked(locked);
        CoreTargetStore initial(AppSettings::instance());
        QVERIFY(initial.load()); QVERIFY(initial.upsert(saved(QStringLiteral("one"), 'a')));
        QVERIFY(initial.select(QStringLiteral("one")));
        GuiConnectionController controller; controller.start({});
        MainWindow* window = controller.sessions()->window();
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window, [&dialog](SetupDialog* d) { dialog = d; });
        window->openCoreSettings(QStringLiteral("one")); QVERIFY(dialog);
        dialog->resize(820, 600); dialog->selectPage(QStringLiteral("Audio with the Core"));
        dialog->raise(); dialog->activateWindow(); QApplication::setActiveWindow(dialog);
        auto* page = dialog->findChild<CoreAudioSetupPage*>(); QVERIFY(page);
        auto* quality = page->findChild<QComboBox*>(QStringLiteral("remoteAudioQuality")); QVERIFY(quality);
        auto* media = window->findChild<RemoteMediaController*>(); QVERIFY(media);
        media->setAudioQualityChoice(RemoteAudioQualityChoice::High);
        auto* scroll = page->findChild<QScrollArea*>(); QVERIFY(scroll);
        // Disconnected facts are short; reserve tall content to exercise the real scroll parent.
        scroll->widget()->setMinimumHeight(900);
        QCoreApplication::processEvents();
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        scroll->verticalScrollBar()->setValue(0);
        QCoreApplication::processEvents();
        QApplication::setActiveWindow(dialog);
        if (focused) { quality->setFocus(Qt::OtherFocusReason); } else { dialog->findChild<QTreeWidget*>()->setFocus(); }
        QVERIFY2(quality->hasFocus() == focused, qPrintable(QStringLiteral("focus=%1 expected=%2 visible=%3 enabled=%4 pagevisible=%5 focuswidget=%6")
            .arg(quality->hasFocus()).arg(focused).arg(quality->isVisible()).arg(quality->isEnabled()).arg(page->isVisible())
            .arg(QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : QStringLiteral("none"))));
        const QVariant preference = AppSettings::instance().value(QLatin1String(RemoteMediaController::kAudioProfileSettingKey));
        const RemoteAudioStatus status = media->audioStatus();
        const quint64 packets = media->micPacketsSent();
        QWheelEvent wheel(QPointF(quality->rect().center()), QPointF(quality->mapToGlobal(quality->rect().center())),
            QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(quality, &wheel);
        QCOMPARE(media->audioQualityChoice(), RemoteAudioQualityChoice::High);
        QCOMPARE(quality->currentIndex(), 0);
        QCOMPARE(AppSettings::instance().value(QLatin1String(RemoteMediaController::kAudioProfileSettingKey)), preference);
        QCOMPARE(media->audioStatus(), status); QCOMPARE(media->micPacketsSent(), packets);
        QVERIFY(!wheel.isAccepted());
        // sendEvent is synthetic; Qt native delivery forwards the ignored wheel.
        QWheelEvent forwarded(QPointF(scroll->viewport()->rect().center()),
            QPointF(scroll->viewport()->mapToGlobal(scroll->viewport()->rect().center())), QPoint(), QPoint(0, -120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(scroll->viewport(), &forwarded);
        QVERIFY2(scroll->verticalScrollBar()->value() > 0, "Scroll container must consume the ignored wheel gesture");
        QVERIFY(!window->findChild<StationClient*>()->isConnectionActive());
        controller.shutdown();
    }
    void audioQualityIntentionalSelectionKeepsExistingControlsLockBehavior_data() {
        QTest::addColumn<bool>("locked");
        QTest::newRow("unlocked") << false; QTest::newRow("locked") << true;
    }
    void audioQualityIntentionalSelectionKeepsExistingControlsLockBehavior() {
        QFETCH(bool, locked);
        const bool previous = ControlsLock::isLocked();
        const auto restore = qScopeGuard([previous] { ControlsLock::setLocked(previous); });
        ControlsLock::setLocked(locked);
        CoreTargetStore initial(AppSettings::instance()); QVERIFY(initial.load());
        QVERIFY(initial.upsert(saved(QStringLiteral("one"), 'a'))); QVERIFY(initial.select(QStringLiteral("one")));
        GuiConnectionController controller; controller.start({}); MainWindow* window = controller.sessions()->window();
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window, [&dialog](SetupDialog* d) { dialog = d; });
        window->openCoreSettings(QStringLiteral("one")); dialog->selectPage(QStringLiteral("Audio with the Core"));
        auto* quality = dialog->findChild<CoreAudioSetupPage*>()->findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        auto* media = window->findChild<RemoteMediaController*>(); QVERIFY(quality && media);
        media->setAudioQualityChoice(RemoteAudioQualityChoice::High);
        QCoreApplication::processEvents(); QApplication::setActiveWindow(dialog); quality->setFocus();
        QTest::mouseClick(quality, Qt::LeftButton); QTRY_VERIFY(quality->view()->isVisible());
        QTest::keyClick(quality->view(), Qt::Key_Down); QTest::keyClick(quality->view(), Qt::Key_Return);
        QTRY_COMPARE(media->audioQualityChoice(), RemoteAudioQualityChoice::SaveData);
        QCOMPARE(AppSettings::instance().value(QLatin1String(RemoteMediaController::kAudioProfileSettingKey)).toString(), QStringLiteral("SaveData"));
        quality->setFocus(); QTest::keyClick(quality, Qt::Key_Up);
        QCOMPARE(media->audioQualityChoice(), RemoteAudioQualityChoice::High);
        controller.shutdown();
    }
    void acceptedCoreRenamePropagatesSavedRowWithoutChangingLegacyAlias() {
        HostFixture f; QVERIFY(f.init(false));
        auto target = saved(QStringLiteral("one"), 'a');
        target.connection.identityFingerprint = f.core.fingerprint(); target.label = QStringLiteral("My local alias");
        CoreTargetStore initial(AppSettings::instance()); QVERIFY(initial.load());
        QVERIFY(initial.upsert(target)); QVERIFY(initial.select(target.id));
        GuiConnectionController controller; controller.start({}); MainWindow* window = controller.sessions()->window();
        auto* client = window->findChild<StationClient*>(); QVERIFY(client);
        f.attach(*client, false, true); QTRY_VERIFY(client->deviceAdminAvailable());
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window, [&dialog](SetupDialog* d) { dialog = d; });
        window->openCoreSettings(target.id); QVERIFY(dialog);
        auto* page = dialog->findChild<CoresSetupPage*>(); QVERIFY(page);
        submit(*page, QStringLiteral("KG4VCF/True_Renamed_Core")); QTRY_COMPARE(f.renames().size(), 1);
        f.answer(f.renames().last(), true);
        QTRY_VERIFY(controller.coreTargetStore().target(target.id)->lastKnownCoreName.has_value());
        const auto accepted = *controller.coreTargetStore().target(target.id);
        auto* tree = controller.selector()->findChild<QTreeWidget*>(); QVERIFY(tree);
        QTRY_VERIFY_WITH_TIMEOUT(!tree->findItems(QStringLiteral("KG4VCF/True_Renamed_Core"),
            Qt::MatchContains | Qt::MatchRecursive, 0).isEmpty(), 1000);
        QCOMPARE(accepted.label, target.label); QCOMPARE(accepted.connection.identityFingerprint, target.connection.identityFingerprint);
        QCOMPARE(accepted.connection.cachedAddresses, target.connection.cachedAddresses); QCOMPARE(accepted.manualAddresses, target.manualAddresses);
        controller.shutdown();
    }
    void savedRowUsesTrustedCoreNameAndKeepsLegacyAliasExplicit() {
        SavedCoreTarget target = saved(QStringLiteral("one"), 'a');
        target.label = QStringLiteral("My local alias");
        target.lastKnownCoreName = AuthenticatedCoreName{QStringLiteral("KG4VCF/True_Core"), target.connection.identityFingerprint, 1};
        const ConnectionTargetRow row = GuiConnectionController::savedCoreRow(target, true);
        QVERIFY2(row.name.contains(QStringLiteral("KG4VCF/True_Core")), "Saved rows must propagate the trusted Core-wide name");
        QVERIFY(row.name.contains(QStringLiteral("last known")));
        QCOMPARE(target.label, QStringLiteral("My local alias"));
        QVERIFY(GuiConnectionController::savedCoreDetails(target).contains(QStringLiteral("Name on this computer: My local alias")));
        for (const bool wrongIdentity : {false, true}) {
            auto untrusted = target;
            if (wrongIdentity) { untrusted.lastKnownCoreName->pairedIdentity = QByteArray(32, 'b'); }
            else { untrusted.lastKnownCoreName->name = QStringLiteral("invalid name"); }
            const auto fallback = GuiConnectionController::savedCoreRow(untrusted, true);
            QVERIFY(!fallback.name.contains(QStringLiteral("True_Core")));
            QVERIFY(fallback.name.contains(QStringLiteral("My local alias")));
            QVERIFY(fallback.name.contains(QStringLiteral("local name")));
        }
        target.lastKnownCoreName.reset(); target.label.clear();
        QVERIFY(GuiConnectionController::savedCoreRow(target, true).name.contains(QStringLiteral("host.invalid")));
    }
    void enrolledLegacySameRowConnectCreatesFreshCanonicalSession_data() {
        QTest::addColumn<QString>("refusal");
        QTest::newRow("recover") << QString();
        QTest::newRow("active-TX-mirror") << QStringLiteral("tx");
        QTest::newRow("failed-durable-select") << QStringLiteral("save");
        QTest::newRow("failed-canonical-reload") << QStringLiteral("load");
    }
    void enrolledLegacySameRowConnectCreatesFreshCanonicalSession() {
        QFETCH(QString, refusal);
        HostFixture f; QVERIFY(f.init(false));
        auto target = saved(QStringLiteral("legacy"), 'a');
        target.connection.identityFingerprint.clear(); target.connection.token = QStringLiteral("legacy-token");
        target.connection.fingerprint = QString::fromLatin1(QByteArray(32, 'c').toHex(':').toUpper());
        CoreTargetStore initial(AppSettings::instance()); QVERIFY(initial.load());
        QVERIFY(initial.upsert(target)); QVERIFY(initial.select(target.id));
        GuiConnectionController controller; controller.start({});
        MainWindow* oldWindow = controller.sessions()->window();
        QPointer<StationClient> oldClient = oldWindow->findChild<StationClient*>(); QVERIFY(oldClient);
        const auto device = oldClient->existingDeviceIdentity(); QVERIFY(device && device->isValid());
        const quint64 generation = controller.sessions()->generation();
        const quint64 oldIncarnation = controller.coreTargetStore().targetIncarnation(target.id);
        f.attach(*oldClient, true, true);
        QTRY_VERIFY(oldClient->isHandshakeComplete()); QVERIFY(f.validProof);
        QCOMPARE(controller.sessions()->generation(), generation); QCOMPARE(controller.sessions()->window(), oldWindow);
        QCOMPARE(controller.coreTargetStore().target(target.id)->connection.identityFingerprint, f.core.fingerprint());
        QVERIFY(controller.coreTargetStore().targetIncarnation(target.id) != oldIncarnation);
        oldClient->disconnectFromStation(QStringLiteral("explicit fixture disconnect"), false);
        QVERIFY(!oldClient->isConnectionActive());
        QVERIFY(!oldWindow->findChild<RemoteConnectionController*>()->canConnect()); // Old trust lease stays retired.
        controller.selector()->setSelectedKey(QStringLiteral("saved:") + target.id);
        auto* connectButton = push(*controller.selector(), "connectionSelectorConnect"); QVERIFY(connectButton);
        QVERIFY2(connectButton->isEnabled(), "Explicit same-row Connect must recover through a fresh canonical session");
        QSignalSpy changed(controller.sessions(), &GuiSessionCoordinator::windowChanged);
        connect(controller.sessions(), &GuiSessionCoordinator::windowChanged, &controller, [&](MainWindow* next) {
            if (!next) { return; }
            auto* fresh = next->findChild<StationClient*>(); QVERIFY(fresh);
            QCOMPARE(fresh->existingDeviceIdentity(), device);
            f.attach(*fresh, false, true);
        });
        if (!refusal.isEmpty()) {
            if (refusal == QLatin1String("tx")) {
                // Mirrored fake remote state only: no MoxController or hardware keying.
                oldWindow->radioModel()->transmitModel().setMox(true);
            } else if (refusal == QLatin1String("save")) {
                // Hold the canonical store loaded, but make its durable settings path unwritable.
                const QString path = AppSettings::instance().filePath();
                QVERIFY(QFile::rename(path, path + QStringLiteral(".fixture-original")));
                QVERIFY(QDir().mkdir(path));
            } else {
                AppSettings::instance().setValue(QStringLiteral("ConnectionTargets/V3"), QStringLiteral("bad JSON"));
                QVERIFY(!controller.coreTargetStore().load());
                QCOMPARE(controller.coreTargetStore().targetIncarnation(target.id), quint64(0));
            }
            // Also protects a queued request whose row became unavailable after rendering.
            controller.selector()->connectRequested(QStringLiteral("saved:") + target.id);
            bool processed = false; QTimer::singleShot(0, &controller, [&] { processed = true; });
            QTRY_VERIFY(processed);
            QCOMPARE(controller.sessions()->generation(), generation); QCOMPARE(changed.size(), 0);
            QCOMPARE(controller.sessions()->window(), oldWindow); QVERIFY(oldClient && !oldClient->isConnectionActive());
            if (refusal == QLatin1String("tx")) { oldWindow->radioModel()->transmitModel().setMox(false); }
            if (refusal == QLatin1String("save")) {
                const QString path = AppSettings::instance().filePath(); QVERIFY(QDir().rmdir(path));
                QVERIFY(QFile::rename(path + QStringLiteral(".fixture-original"), path));
            }
            controller.shutdown(); return;
        }
        oldClient->remoteDevices()->applyObject("devices", {{2, "stationLabel", MirrorWireKind::Utf8,
            QStringLiteral("KG4VCF/Stale_Lease")}});
        QVERIFY(!controller.coreTargetStore().target(target.id)->lastKnownCoreName);
        connectButton->click();
        QTRY_COMPARE(controller.sessions()->generation(), generation + 1);
        QCOMPARE(changed.size(), 1); QVERIFY(oldClient.isNull());
        StationClient* fresh = controller.sessions()->window()->findChild<StationClient*>(); QVERIFY(fresh);
        QTRY_VERIFY(fresh->isHandshakeComplete()); QVERIFY(f.validProof); QVERIFY(fresh->signedInWithDeviceKey());
        QCOMPARE(controller.sessions()->selection().savedId, target.id);
        QCOMPARE(controller.sessions()->selection().connection.identityFingerprint, f.core.fingerprint());
        controller.shutdown();
    }
    void groupedAudioFactsPreserveIndependentReceiveMicrophoneAndAppFormats() {
        RemoteAudioStatus status;
        status.state = RemoteAudioStatus::State::Playing;
        status.detailNegotiated = true;
        status.encoder = OpusEncoderProfile{};
        status.encoder->targetBitrate = 24000;
        status.microphoneFormat = QStringLiteral("Radio microphone at the Core (no microphone stream from this computer)");
        RemoteReceiverAudioStatus receiver; receiver.sliceId = 2;
        receiver.state = RemoteReceiverAudioStatus::State::Receiving;
        receiver.runningProfile = RemoteAudioProfile::Opus; receiver.encoder = OpusEncoderProfile{};
        receiver.encoder->targetBitrate = 48000; status.receivers.append(receiver);
        RemoteAudioReceiverTelemetry health; health.expectedPackets = 20; health.missingPackets = 2;
        health.concealedPackets = 3; health.arrivalJitterMs = 6; health.speakerQueuedMs = 8; health.jitterHoldMs = 60;
        const auto sections = formatRemoteAudioDetailSections(status, health);
        QVERIFY(sections.receive.contains(QStringLiteral("24\u00A0kbit/s target")));
        QVERIFY(sections.receive.contains(QStringLiteral("Missing packets: 2 of 20")));
        QVERIFY(sections.receive.contains(QStringLiteral("Gaps filled: 3")));
        QVERIFY(sections.receive.contains(QStringLiteral("Network buffer: 60")));
        QVERIFY(!sections.receive.contains(QStringLiteral("Radio microphone")));
        QVERIFY(sections.microphone.contains(QStringLiteral("no microphone stream from this computer")));
        QVERIFY(sections.apps.contains(QStringLiteral("Receiver C for apps: Receiving, Opus 48")));
        QVERIFY(!sections.apps.contains(QStringLiteral("24")));
        const QString full = formatRemoteAudioDetails(status, health);
        QVERIFY(full.indexOf(QStringLiteral("Current microphone format")) < full.indexOf(QStringLiteral("Output:")));
    }
    void bottomBannerUsesAuthenticatedCoreNameBeforeConnectionAddress() {
        HostFixture f; QVERIFY(f.init(false));
        CoreTargetStore initial(AppSettings::instance()); QVERIFY(initial.load());
        auto target = saved(QStringLiteral("one"), 'a');
        target.connection.identityFingerprint = f.core.fingerprint();
        QVERIFY(initial.upsert(target)); QVERIFY(initial.select(target.id));
        GuiConnectionController controller; controller.start({});
        MainWindow* window = controller.sessions()->window(); QVERIFY(window);
        StationClient* client = window->findChild<StationClient*>(); QVERIFY(client);
        f.attach(*client);
        QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 1000);
        client->remoteDevices()->applyObject("devices", {{2, "stationLabel", MirrorWireKind::Utf8,
            QStringLiteral("KG4VCF/Authenticated_Core")}});
        auto* label = window->findChild<QLabel*>(QStringLiteral("StationBlock_Label")); QVERIFY(label);
        QTRY_COMPARE_WITH_TIMEOUT(label->text(), QStringLiteral("KG4VCF/Authenticated_Core"), 1000);
        QVERIFY(!label->text().contains(QStringLiteral("Untrusted hello")));
        client->remoteDevices()->applyObject("devices", {{2, "stationLabel", MirrorWireKind::Utf8,
            QStringLiteral("KG4VCF/Renamed_Core")}});
        QTRY_COMPARE_WITH_TIMEOUT(label->text(), QStringLiteral("KG4VCF/Renamed_Core"), 1000);
        QVERIFY(controller.coreTargetStore().target(QStringLiteral("one"))->lastKnownCoreName);
        QCOMPARE(controller.coreTargetStore().target(QStringLiteral("one"))->lastKnownCoreName->name, QStringLiteral("KG4VCF/Renamed_Core"));
        QVERIFY(controller.coreTargetStore().upsert(saved(QStringLiteral("two"), 'b')));
        const quint64 generation = controller.sessions()->generation();
        QPointer<SetupDialog> dialog;
        connect(window, &MainWindow::setupDialogCreated, window, [&dialog](SetupDialog* created) { dialog = created; });
        window->openCoreSettings(QStringLiteral("two")); QVERIFY(dialog);
        auto* cores = dialog->findChild<CoresSetupPage*>(); QVERIFY(cores);
        QVERIFY(cores->findChild<QLabel*>(QStringLiteral("actualCoreName"))->text() != QStringLiteral("KG4VCF/Renamed_Core"));
        cores->audioRequested();
        auto* audio = dialog->findChild<CoreAudioSetupPage*>(); QVERIFY(audio);
        QVERIFY(hasText(*audio, QStringLiteral("This window’s Core: KG4VCF/Renamed_Core")));
        QCOMPARE(controller.sessions()->generation(), generation);
        QCOMPARE(controller.coreTargetStore().selectedId(), QStringLiteral("one"));
        client->disconnectFromStation(QStringLiteral("fixture disconnected"));
        QTRY_VERIFY_WITH_TIMEOUT(label->text().contains(QStringLiteral("last known")), 1000);
        QVERIFY(label->text().contains(QStringLiteral("KG4VCF/Renamed_Core")));
        QVERIFY(hasText(*audio, QStringLiteral("No authenticated Core connection")));
        controller.shutdown();
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
