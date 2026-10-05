// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <algorithm>
#include <QTemporaryDir>
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/session/StationClient.h"
#include "core/session/SessionMessages.h"
#include "fakes/LoopbackTransport.h"
#include "gui/CoreRenameController.h"
#include "core/AppSettings.h"
#include "core/session/RemoteDevicesState.h"
using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
namespace {
struct SignedPeer {
    QTemporaryDir dir;
    StationIdentity key = StationIdentity::loadOrCreate(dir.path());
    QByteArray certificate = QByteArray(32, 'c');
    QByteArray challenge = QByteArray(32, 'q');
    std::shared_ptr<ClientDeviceIdentity> device = std::make_shared<ClientDeviceIdentity>(
        ClientDeviceIdentity::loadOrCreate(dir.filePath(QStringLiteral("device"))));
    QPointer<LoopbackTransport> far;
    QList<SessionMessage> received;
    QList<SessionMessage> sent;
    bool validDeviceProof = false;
    bool held = false;
    bool sendHello = true;
    bool paired = true;
    std::function<void()> beforeAuthResult;
    QByteArray expectedIdentity;
    std::function<void()> beforeHello;
    std::function<void(StationClient&, const SessionMessage&)> synchronousHandler;
    int adminVersion = 1;
    std::function<void(const SessionMessage&)> commandHandler;
    void attach(StationClient& client) {
        auto* near = new LoopbackTransport(QStringLiteral("rename client"));
        far = new LoopbackTransport(QStringLiteral("signed Core"), &client);
        near->linkTo(far);
        near->setPeerCertificateSha256(certificate);
        QObject::connect(near, &LoopbackTransport::outboundText, &client, [this, &client](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) {
                sent.append(message);
                if (synchronousHandler) { synchronousHandler(client, message); }
            }
        });
        client.setDeviceIdentity(device, QStringLiteral("Test computer"));
        QObject::connect(far, &SessionTransport::textReceived, &client, [this](const QByteArray& bytes) {
            SessionMessage decoded;
            if (!SessionMessages::decode(bytes, &decoded)) { return; }
            received.append(decoded);
            if (decoded.kind == SessionMessageKind::CommandInvoke && commandHandler) { commandHandler(decoded); }
            if (decoded.kind == SessionMessageKind::AuthRequest && decoded.device) {
                const auto& d = *decoded.device;
                const QByteArray spki = StationIdentity::fromBase64Url(d.publicKey);
                validDeviceProof = StationIdentity::verify(spki,
                    DeviceAuthenticator::transcript(challenge, certificate, key.publicKeySpki(), spki),
                    StationIdentity::fromBase64Url(d.signature));
                if (beforeAuthResult) { beforeAuthResult(); }
                if (held) { far->sendText(SessionMessages::encode(SessionMessages::sessionHeld({}, 1))); return; }
                far->sendText(SessionMessages::encode(SessionMessages::authResult(validDeviceProof, {}, false)));
                StationCapabilities caps;
                caps.radioIdentityEntries = true;
                caps.deviceAdminVersion = adminVersion;
                caps.sessionHolderEntry = true;
                caps.sessionHolderVersion = 1;
                caps.recordStreamVersion = 1;
                far->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
                far->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
            }
        });
        client.startSession(near, paired ? QString() : QStringLiteral("legacy token"), {}, paired ? (expectedIdentity.isEmpty() ? key.fingerprint() : expectedIdentity) : QByteArray());
        if (beforeHello) { beforeHello(); }
        if (!sendHello) { return; }
        SessionMessage hello = SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 0,
            QStringLiteral("Signed Core"), LinkVersion::supportedMajors(), {{QByteArrayLiteral("deviceAuth"), 1}});
        hello.challenge = StationIdentity::toBase64Url(challenge);
        hello.stationIdentity = SessionStationIdentity{StationIdentity::toBase64Url(key.publicKeySpki()),
            StationIdentity::toBase64Url(key.certBinding(certificate))};
        far->sendText(SessionMessages::encode(hello));
    }
};
struct RenameHarness {
    SignedPeer peer;
    StationClient borrowed{nullptr, nullptr};
    AppSettings settings{peer.dir.filePath(QStringLiteral("settings.xml"))};
    CoreTargetStore store{settings};
    SavedCoreTarget target;
    quint64 generation = 17;
    quint64 authorityEpoch = 42;
    bool sameCoreActive = false;
    bool borrow = false;
    int constructed = 0;
    QPointer<StationClient> temporary;
    std::unique_ptr<CoreRenameController> helper;
    bool init(bool useBorrow = false) {
        borrow = useBorrow;
        sameCoreActive = useBorrow;
        target.id = QStringLiteral("one");
        target.label = QStringLiteral("Legacy label");
        target.connection.url = QStringLiteral("wss://listener.test:47910");
        target.connection.identityFingerprint = peer.key.fingerprint();
        if (!store.load() || !store.upsert(target)) { return false; }
        if (useBorrow) { peer.attach(borrowed); }
        helper = std::make_unique<CoreRenameController>(store, [this](const QByteArray&) {
            return CoreRenameController::Snapshot{borrow ? &borrowed : nullptr, sameCoreActive, generation, peer.device->fingerprint()};
        }, peer.device, [this](const CoreRenameController::Request& r) { return r.epoch == authorityEpoch; }, nullptr,
        [this] {
            ++constructed;
            auto client = std::make_unique<StationClient>(nullptr, nullptr, nullptr, LinkVersion::supportedMajors(), StationClient::SessionPurpose::RenameOnly);
            temporary = client.get();
            return client;
        }, [this](StationClient& client, const RemoteStationOptions& options) { peer.expectedIdentity = options.identityFingerprint; peer.attach(client); }, CoreRenameController::Limits{200, 80, 25, 15});
        helper->inspectTarget(target.id);
        return true;
    }
    CoreRenameController::Request request(quint64 id = 1) const {
        return {id, target.id, target.connection.identityFingerprint, store.targetIncarnation(target.id), authorityEpoch, QStringLiteral("KG4VCF/New")};
    }
    void answer(const SessionMessage& message, bool accepted = true, const QString& reason = {}) {
        if (peer.far) { peer.far->sendText(SessionMessages::encode(SessionMessages::commandResult(message.commandVerb, message.commandId, accepted, reason, {}))); }
    }
    int count(const QByteArray& verb) const {
        int count = 0;
        for (const auto& message : peer.sent) { if (message.commandVerb == verb) { ++count; } }
        return count;
    }
};

}
class TstCoreRename : public QObject {
    Q_OBJECT
private slots:
    void renameAdmissionDoesNotDeclareControls() {
        SignedPeer peer;
        StationClient client(nullptr, nullptr, nullptr, LinkVersion::supportedMajors(), StationClient::SessionPurpose::RenameOnly);
        peer.attach(client);
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 1500);
        QVERIFY(peer.validDeviceProof);
        QTRY_VERIFY_WITH_TIMEOUT(!peer.received.isEmpty(), 500);
        for (const auto& message : peer.sent) {
            if (message.kind == SessionMessageKind::Hello) {
                QVERIFY2(!message.features.contains(QByteArrayLiteral("remoteTx")), "Rename client must not declare remote TX");
            }
        }
    }
    void renameAdmissionDoesNotSubscribeToRecords() {
        SignedPeer peer;
        StationClient client(nullptr, nullptr, nullptr, LinkVersion::supportedMajors(), StationClient::SessionPurpose::RenameOnly);
        peer.attach(client);
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 1500);
        QVERIFY(peer.validDeviceProof);
        for (const auto& message : peer.sent) {
            QVERIFY2(message.commandVerb != QByteArrayLiteral("records.subscribe"), "Rename admission must never subscribe to records");
        }
    }

    void borrowedAcceptedResultSavesNameAndKeepsSession() {
        SignedPeer peer;
        StationClient client(nullptr, nullptr);
        peer.attach(client);
        QTRY_VERIFY_WITH_TIMEOUT(client.deviceAdminAvailable(), 1500);
        AppSettings settings(peer.dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = QStringLiteral("one");
        target.label = QStringLiteral("Original local label");
        target.connection.url = QStringLiteral("wss://listener.test:47910");
        target.connection.identityFingerprint = peer.key.fingerprint();
        QVERIFY(store.upsert(target));
        CoreRenameController helper(store, [&](const QByteArray&) {
            return CoreRenameController::Snapshot{&client, true, 17, peer.device->fingerprint()};
        }, peer.device, [](const CoreRenameController::Request& r) { return r.epoch == 42; }, nullptr, {}, {}, {500, 200, 10, 10});
        helper.inspectTarget(target.id);
        QSignalSpy result(&helper, &CoreRenameController::finished);
        QObject::connect(peer.far, &SessionTransport::textReceived, &helper, [&](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message) && message.commandVerb == QByteArrayLiteral("station.rename")) {
                peer.far->sendText(SessionMessages::encode(SessionMessages::commandResult(message.commandVerb,
                    message.commandId, true, {}, {})));
            }
        });
        helper.rename({1, target.id, peer.key.fingerprint(), store.targetIncarnation(target.id), 42, QStringLiteral("KG4VCF/Updated")});
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Accepted);
        QVERIFY(store.target(target.id)->lastKnownCoreName.has_value());
        QCOMPARE(store.target(target.id)->lastKnownCoreName->name, QStringLiteral("KG4VCF/Updated"));
        QCOMPARE(store.target(target.id)->label, target.label);
        QVERIFY(client.isHandshakeComplete());
        for (const auto& message : peer.sent) { QVERIFY(message.commandVerb != QByteArrayLiteral("session.leave")); }
    }

    void temporaryAcceptedLeavesAndSendsOnlyRenameTraffic() {
        RenameHarness f; QVERIFY(f.init());
        f.peer.commandHandler = [&](const SessionMessage& m) { f.answer(m); };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request());
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(!f.temporary, 1000);
        QCOMPARE(f.constructed, 1);
        QCOMPARE(f.count("station.rename"), 1);
        QCOMPARE(f.count("session.leave"), 1);
        QVERIFY(f.peer.validDeviceProof);
        for (const auto& m : f.peer.sent) {
            if (m.kind == SessionMessageKind::CommandInvoke) { QVERIFY(m.commandVerb == "station.rename" || m.commandVerb == "session.leave"); }
            else { QVERIFY(m.kind == SessionMessageKind::Hello || m.kind == SessionMessageKind::AuthRequest); }
            if (m.kind == SessionMessageKind::Hello) {
                QVERIFY(m.features.contains("deviceAuth")); QVERIFY(m.features.contains("sessionHolder"));
                QVERIFY(!m.features.contains("remoteTx")); QVERIFY(!m.features.contains("sliceAccess"));
                QVERIFY(!m.features.contains("mediaDirect")); QVERIFY(!m.features.contains("txWatchPath"));
                QVERIFY(!m.features.contains("txWatchRelay"));
            }
        }
    }
    void refusedRenamePreservesNameAndBorrowedSession() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.commandHandler = [&](const SessionMessage& m) { f.answer(m, false, QStringLiteral("Core refused this name.")); };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Refused);
        QCOMPARE(result.first().at(3).toString(), QStringLiteral("Core refused this name."));
        QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName); QVERIFY(f.borrowed.isHandshakeComplete());
        QCOMPARE(f.count("session.leave"), 0); QCOMPARE(f.constructed, 0);
    }
    void heldAdmissionNeverTakesOver() {
        RenameHarness f; f.peer.held = true; QVERIFY(f.init());
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Refused);
        QCOMPARE(f.count("station.rename"), 0); QCOMPARE(f.count("session.takeover"), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!f.temporary, 1000);
    }
    void sameCorePendingExcludesTemporaryBeforeConstruction() {
        RenameHarness f; QVERIFY(f.init()); f.sameCoreActive = true;
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Refused);
        QCOMPARE(f.constructed, 0); QVERIFY(f.peer.sent.isEmpty());
    }
    void wrongRequestIdentityAndIncarnationCannotDial() {
        RenameHarness f; QVERIFY(f.init()); QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        auto request = f.request(); request.pairedIdentity = QByteArray(32, 'x'); f.helper->rename(request);
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::StaleTarget);
        request = f.request(2); ++request.incarnation; f.helper->rename(request);
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 2, 1000); QCOMPARE(f.constructed, 0);
    }
    void unknownTimeoutDoesNotResend() {
        RenameHarness f; QVERIFY(f.init()); QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::UnknownOutcome);
        QCOMPARE(f.count("station.rename"), 1); QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName);
        QTRY_VERIFY_WITH_TIMEOUT(!f.temporary, 1000);
    }
    void acceptedAnswerWaitsForFreshAuthoritativeName() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.borrowed.remoteDevices()->applyObject("devices", {{2, "stationLabel", MirrorWireKind::Utf8, QStringLiteral("KG4VCF/Old")}});
        f.peer.commandHandler = [&](const SessionMessage& m) {
            f.answer(m);
            QTimer::singleShot(5, f.helper.get(), [&] { f.borrowed.remoteDevices()->applyObject("devices",
                {{2, "stationLabel", MirrorWireKind::Utf8, QStringLiteral("KG4VCF/Canonical")}}); });
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(result.first().at(2).toString(), QStringLiteral("KG4VCF/Canonical"));
        QCOMPARE(f.store.target(f.target.id)->lastKnownCoreName->name, QStringLiteral("KG4VCF/Canonical"));
    }
    void acceptedWithoutBroadcastNeverCachesOldName() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.borrowed.remoteDevices()->applyObject("devices", {{2, "stationLabel", MirrorWireKind::Utf8, QStringLiteral("KG4VCF/Old")}});
        f.peer.commandHandler = [&](const SessionMessage& m) { f.answer(m); };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(result.first().at(2).toString(), QStringLiteral("KG4VCF/New"));
        QCOMPARE(f.store.target(f.target.id)->lastKnownCoreName->name, QStringLiteral("KG4VCF/New"));
    }
    void forgetRecreateRetiresLateResult() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.commandHandler = [&](const SessionMessage& m) { QVERIFY(f.store.remove(f.target.id)); QVERIFY(f.store.upsert(f.target)); f.answer(m); };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::StaleTarget);
        QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName); QVERIFY(f.borrowed.isHandshakeComplete());
    }
    void cancelledResultCannotReachReplacementAuthority() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.commandHandler = [&](const SessionMessage& m) { f.helper->cancel(); ++f.authorityEpoch; f.answer(m); };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Cancelled);
        QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName); QVERIFY(!f.helper->pending());
    }
    void unsupportedSameCoreDoesNotCreateTemporary() {
        RenameHarness f; f.peer.adminVersion = 0; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.isHandshakeComplete(), 1000);
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Refused);
        QCOMPARE(f.constructed, 0); QCOMPARE(f.count("station.rename"), 0);
    }
    void durableSaveFailureCannotRevokeAcceptedRename() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.commandHandler = [&](const SessionMessage& m) {
            QVERIFY(QFile::remove(f.settings.filePath())); QVERIFY(QDir().mkpath(f.settings.filePath())); f.answer(m);
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::AcceptedLocalSaveFailed);
        QCOMPARE(result.first().at(2).toString(), QStringLiteral("KG4VCF/New"));
        QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName); QVERIFY(f.borrowed.isHandshakeComplete());
    }

    void sameCoreAppearingBeforeHelloBlocksAuthentication() {
        RenameHarness f; QVERIFY(f.init()); f.peer.beforeHello = [&] { f.sameCoreActive = true; };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Refused);
        QCOMPARE(f.count("station.rename"), 0);
        for (const auto& m : f.peer.sent) { QVERIFY(m.kind != SessionMessageKind::AuthRequest); }
    }
    void wrongCoreCannotReceiveDeviceProofOrRename() {
        RenameHarness f; QVERIFY(f.init());
        QTemporaryDir other;
        f.peer.key = StationIdentity::loadOrCreate(other.path());
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Refused);
        QCOMPARE(f.count("station.rename"), 0); QVERIFY(!f.peer.validDeviceProof);
        for (const auto& m : f.peer.sent) { QVERIFY(m.kind != SessionMessageKind::AuthRequest); }
    }
    void silentAdmissionIsBounded() {
        RenameHarness f; QVERIFY(f.init()); f.peer.sendHello = false;
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::TimedOut);
        QCOMPARE(f.count("station.rename"), 0); QTRY_VERIFY_WITH_TIMEOUT(!f.temporary, 1000);
    }
    void synchronousResultIsQueuedUntilCommandIdIsKnown() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.synchronousHandler = [&](StationClient& client, const SessionMessage& m) {
            if (m.commandVerb == "station.rename") {
                emit client.commandResponse(SessionMessages::commandResult(m.commandVerb, m.commandId, true, {}, {}));
            }
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QVERIFY(result.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Accepted);
        QCOMPARE(f.count("station.rename"), 1);
    }
    void acceptedCleanupTimeoutDoesNotRevokeRename() {
        RenameHarness f; QVERIFY(f.init());
        f.peer.commandHandler = [&](const SessionMessage& m) { if (m.commandVerb == "station.rename") { f.answer(m); } };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(!f.temporary, 1000); QCOMPARE(f.count("session.leave"), 1);
        QVERIFY(f.store.target(f.target.id)->lastKnownCoreName);
    }
    void commandResultWrongVerbCannotAcceptRename() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.commandHandler = [&](const SessionMessage& m) {
            f.peer.far->sendText(SessionMessages::encode(SessionMessages::commandResult("station.other", m.commandId, true, {}, {})));
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::UnknownOutcome);
        QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName);
    }
    void synchronousNameBroadcastBeforeResultIsAuthoritative() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.synchronousHandler = [&](StationClient& client, const SessionMessage& m) {
            if (m.commandVerb == "station.rename") {
                client.remoteDevices()->applyObject("devices", {{2, "stationLabel", MirrorWireKind::Utf8, QStringLiteral("KG4VCF/Synchronous")}});
                emit client.commandResponse(SessionMessages::commandResult(m.commandVerb, m.commandId, true, {}, {}));
            }
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(result.first().at(2).toString(), QStringLiteral("KG4VCF/Synchronous"));
    }

    void coordinatorChangedBeforeCommandCannotRename() {
        RenameHarness f; QVERIFY(f.init()); f.peer.beforeAuthResult = [&] { ++f.generation; };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Refused);
        QCOMPARE(f.count("station.rename"), 0); QTRY_VERIFY_WITH_TIMEOUT(!f.temporary, 1000);
    }
    void queuedResultCannotCrossClientEpoch() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        SignedPeer replacement;
        f.peer.synchronousHandler = [&](StationClient& client, const SessionMessage& m) {
            if (m.commandVerb == "station.rename") {
                emit client.commandResponse(SessionMessages::commandResult(m.commandVerb, m.commandId, true, {}, {}));
                replacement.attach(client);
            }
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        const quint32 oldEpoch = f.borrowed.sessionEpoch();
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QVERIFY(f.borrowed.sessionEpoch() > oldEpoch);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::UnknownOutcome);
        QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName); QCOMPARE(f.count("session.leave"), 0);
    }
    void authorityChangeBeforeAcceptedAnswerPreventsCacheWrite() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.commandHandler = [&](const SessionMessage& m) { ++f.authorityEpoch; f.answer(m); };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::StaleTarget);
        QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName);
    }
    void reloadInvalidatesCapturedTargetLease() {
        RenameHarness f; QVERIFY(f.init(true)); QTRY_VERIFY_WITH_TIMEOUT(f.borrowed.deviceAdminAvailable(), 1000);
        f.peer.commandHandler = [&](const SessionMessage& m) { QVERIFY(f.store.load()); f.answer(m); };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::StaleTarget);
        QVERIFY(!f.store.target(f.target.id)->lastKnownCoreName);
    }
    void invalidLabelCannotOpenAnySession() {
        RenameHarness f; QVERIFY(f.init()); QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        auto request = f.request(); request.name = QStringLiteral("not a valid label");
        f.helper->rename(request); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreRenameController::Outcome>(result.first().at(1)), CoreRenameController::Outcome::Refused);
        QCOMPARE(f.constructed, 0); QVERIFY(f.peer.sent.isEmpty());
    }
    void renameOnlyNeverFallsBackToTokenEnrollment() {
        SignedPeer peer; peer.paired = false;
        StationClient client(nullptr, nullptr, nullptr, LinkVersion::supportedMajors(), StationClient::SessionPurpose::RenameOnly);
        QSignalSpy ended(&client, &StationClient::sessionEnded);
        peer.attach(client);
        QTRY_VERIFY_WITH_TIMEOUT(!ended.isEmpty() || std::any_of(peer.sent.cbegin(), peer.sent.cend(),
            [](const SessionMessage& m) { return m.kind == SessionMessageKind::AuthRequest; }), 1000);
        for (const auto& m : peer.sent) { QVERIFY2(m.kind != SessionMessageKind::AuthRequest, "RenameOnly must not use token enrollment"); }
    }
    void ordinaryTokenAuthenticationIsPreserved() {
        SignedPeer peer; peer.paired = false;
        StationClient client(nullptr, nullptr);
        peer.attach(client);
        QTRY_VERIFY_WITH_TIMEOUT(std::any_of(peer.sent.cbegin(), peer.sent.cend(),
            [](const SessionMessage& m) { return m.kind == SessionMessageKind::AuthRequest; }), 1000);
        for (const auto& m : peer.sent) {
            if (m.kind == SessionMessageKind::AuthRequest) {
                QCOMPARE(m.token, QStringLiteral("legacy token"));
                QVERIFY(!m.device);
            }
        }
    }
    void outgoingHelloAuthorityChangePreventsAuthentication() {
        RenameHarness f; QVERIFY(f.init());
        f.peer.synchronousHandler = [&](StationClient&, const SessionMessage& m) {
            if (m.kind == SessionMessageKind::Hello) { ++f.generation; }
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        for (const auto& m : f.peer.sent) {
            QVERIFY2(m.kind != SessionMessageKind::AuthRequest, "The final admission guard must run immediately before auth.request");
        }
        QCOMPARE(f.count("station.rename"), 0);
    }
    void finishingTemporarySessionExcludesAnotherAdmission() {
        RenameHarness f; QVERIFY(f.init());
        f.peer.commandHandler = [&](const SessionMessage& m) { if (m.commandVerb == "station.rename") { f.answer(m); } };
        bool once = false;
        f.peer.synchronousHandler = [&](StationClient&, const SessionMessage& m) {
            if (m.commandVerb == "session.leave" && !once) { once = true; f.helper->rename(f.request(2)); }
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request()); QTRY_VERIFY_WITH_TIMEOUT(result.count() >= 2, 1000);
        QCOMPARE(f.constructed, 1);
        QCOMPARE(f.count("station.rename"), 1);
    }
    void temporaryLeaseSurvivesDisconnectUntilObjectRetirement() {
        RenameHarness f; QVERIFY(f.init());
        f.peer.commandHandler = [&](const SessionMessage& m) { if (m.commandVerb == "station.rename") { f.answer(m); } };
        bool observedClosedLease = false;
        f.peer.synchronousHandler = [&](StationClient& client, const SessionMessage& m) {
            if (m.commandVerb == "session.leave") {
                QObject::connect(&client, &StationClient::sessionEnded, &client, [&] {
                    observedClosedLease = f.helper->ownsTemporaryAdmission(f.target.connection.identityFingerprint);
                });
            }
        };
        QSignalSpy result(f.helper.get(), &CoreRenameController::finished);
        f.helper->rename(f.request());
        QVERIFY(f.helper->ownsTemporaryAdmission(f.target.connection.identityFingerprint));
        QVERIFY(!f.helper->ownsTemporaryAdmission(QByteArray(32, 'x')));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!f.temporary, 1000);
        QVERIFY(observedClosedLease);
        QVERIFY(!f.helper->ownsTemporaryAdmission(f.target.connection.identityFingerprint));
    }

};
QTEST_MAIN(TstCoreRename)
#include "tst_core_rename.moc"
