// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_devices.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 13 (R-IOS-08): devices, rename and revoke on the
// Core.
//
// Who may, first:
//
//   - the `devices` object goes only to a connection at minor 11 whose
//     hello declares deviceAuth; a window that declares nothing (today's
//     desktop) and an older one receive none, and the four verbs are
//     refused for them in plain words;
//   - a removed device's connection ends at once with session.end
//     "This device was removed from the Core.", retryable false, code
//     deviceRemoved, whatever removed it (the verb, or the store directly
//     as the console and a reset do), and the device cannot sign in again;
//     revoking the requester's own device ends its connection just after
//     the result;
//   - station.retireToken is refused until a device is paired; once
//     accepted it deletes the token file and ends every connection signed
//     in by token (including one that enrolled its device key), code
//     pairingRequired; a later token sign-in is refused;
//   - raw settings writes and removes of StationLabel and
//     StationKeyBackupAcknowledged are refused.
//
// Then what the object says: the label follows StationCallsign until the
// first rename; a rename checks the label rule and emits the label change;
// the key backup starts false and becomes true only through its verb, for
// this key; every change moves revision once.
//
// Keys are made at run time in scratch directories.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R1-I1): the last device is not
//               revoked while no token is active. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I3): a computer enrolled
//               through the token is not revoked while the token works. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25):
//                                    recordStreamVersion and the record
//                                    streams. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 8b: a
//                                    computer that joined with the token
//                                    removed in one step, and each sign-in's
//                                    name stored. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationIdentity.h"
#include "core/security/StationLabel.h"
#include "core/security/TokenStore.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/StationServer.h"
#include "models/RadioModel.h"

#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kRemoved = QStringLiteral("This device was removed from the Core.");
const QString kPairFirst = QStringLiteral("This Core uses paired devices. Pair this device first.");
const QString kUpdateApp = QStringLiteral("Update this app to manage this Core's paired devices.");
const QString kNameRefusal =
    QStringLiteral("This Core keeps its own name. Update this app to rename it.");

// A device key made at run time.
struct Device {
    QTemporaryDir dir;
    StationIdentity key = StationIdentity::loadOrCreate(dir.path());

    PairedDevice record(const QString& name = QStringLiteral("Shack iPhone")) const
    {
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = name;
        device.kind = QStringLiteral("phone");
        return device;
    }

    QString id() const { return StationIdentity::toBase64Url(key.fingerprint()); }

    QString shortName;  // Part C fix wave: sent when not empty
    // Slice control plan Task 8b: the name it signs in with.
    QString name = QStringLiteral("Shack iPhone");

    SessionDeviceBlock block(const QByteArray& challenge, const QByteArray& certSha256,
                             const QByteArray& stationSpki) const
    {
        return SessionDeviceBlock{
            id(), StationIdentity::toBase64Url(key.publicKeySpki()), name,
            QStringLiteral("phone"),
            StationIdentity::toBase64Url(key.sign(DeviceAuthenticator::transcript(
                challenge, certSha256, stationSpki, key.publicKeySpki()))),
            shortName};
    }
};

QList<QJsonObject> ofType(const QList<QByteArray>& received, const QString& type)
{
    QList<QJsonObject> out;
    for (const QByteArray& wire : received) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() == type) {
            out.append(o);
        }
    }
    return out;
}

QJsonObject firstOfType(const QList<QByteArray>& received, const QString& type)
{
    const QList<QJsonObject> all = ofType(received, type);
    return all.isEmpty() ? QJsonObject{} : all.first();
}

bool sentDevicesObject(const QList<QByteArray>& received)
{
    for (const QJsonObject& o : ofType(received, QStringLiteral("object.create"))) {
        if (o.value(QStringLiteral("key")).toString() == QStringLiteral("devices")) {
            return true;
        }
    }
    for (const QJsonObject& o : ofType(received, QStringLiteral("schema"))) {
        if (o.value(QStringLiteral("class")).toString() == QStringLiteral("StationDevicesFacade")) {
            return true;
        }
    }
    return false;
}

MirrorUpdate utf8(const char* name, const QString& value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Utf8, QVariant(value)};
}

// One Core over the loopback, in scratch directories.
struct Core {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;
    QList<LoopbackTransport*> clients;
    quint32 nextCommandId = 1;

    explicit Core(bool upgradedWithToken, const QString& security = QString())
    {
        settings = std::make_unique<AppSettings>(
            settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        model = std::make_unique<RadioModel>();
        model->setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
        info.name = QStringLiteral("Bench HL2");
        info.boardType = HPSDRHW::HermesLite;
        model->setLastRadioInfoForTest(info);
        model->setConnectionStateForTest(ConnectionState::Connected);
        model->addSlice(QStringLiteral("pan-0"));
        const QString base = security.isEmpty() ? securityDir.path() : security;
        const QString dir = upgradedWithToken ? NereusSDR::Test::seedUpgradedCoreToken(base)
                                              : NereusSDR::Test::seedCoreIdentity(base);
        server = std::make_unique<StationServer>(model.get(), *settings, dir);
        server->setHeartbeatIntervalMs(0);
    }

    ~Core()
    {
        server.reset();
        qDeleteAll(clients);
    }

    StationDevicesFacade& devices() const { return *server->devicesFacade(); }

    QByteArray certSha256() const
    {
        QString pin = server->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        return QByteArray::fromHex(pin.toLatin1());
    }

    LoopbackTransport* open()
    {
        auto* app = new LoopbackTransport(QStringLiteral("app"));
        auto* station = new LoopbackTransport(QStringLiteral("station"));
        station->setPeerAddress(QStringLiteral("192.0.2.7"));
        station->linkTo(app);
        clients.append(app);
        server->acceptTransport(station);
        const bool greeted = QTest::qWaitFor([app]() { return !app->received().isEmpty(); }, 5000);
        Q_UNUSED(greeted);  // an empty hello fails the sign-in that follows
        return app;
    }

    // Hello (at `minor`, declaring `features`) and `auth`; true once the
    // connection's snapshot is complete.
    static bool connect(LoopbackTransport* app, const SessionMessage& auth, quint16 minor,
                        const QHash<QByteArray, int>& features)
    {
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, minor, 0, QStringLiteral("NereusSDR iPhone"),
            {kSessionProtocolMajor}, features)));
        app->sendText(SessionMessages::encode(auth));
        return QTest::qWaitFor(
            [app]() {
                return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))
                    || !app->isOpen();
            },
            5000)
            && app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"));
    }

    LoopbackTransport* deviceSession(const Device& device, quint16 minor = kSessionProtocolMinor,
                                     const QHash<QByteArray, int>& features = {{"deviceAuth", 1}})
    {
        LoopbackTransport* app = open();
        const QJsonObject hello = firstOfType(app->received(), QStringLiteral("hello"));
        const QByteArray challenge =
            StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
        const SessionMessage auth = SessionMessages::authRequest(
            QString(), device.block(challenge, certSha256(), server->stationIdentity().publicKeySpki()));
        return connect(app, auth, minor, features) ? app : nullptr;
    }

    LoopbackTransport* tokenSession(const QHash<QByteArray, int>& features = {{"deviceAuth", 1}},
                                    quint16 minor = kSessionProtocolMinor,
                                    const Device* enrol = nullptr)
    {
        LoopbackTransport* app = open();
        SessionMessage auth = SessionMessages::authRequest(server->token());
        if (enrol != nullptr) {
            const QJsonObject hello = firstOfType(app->received(), QStringLiteral("hello"));
            const QByteArray challenge =
                StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
            auth = SessionMessages::authRequest(
                server->token(),
                enrol->block(challenge, certSha256(), server->stationIdentity().publicKeySpki()));
        }
        return connect(app, auth, minor, features) ? app : nullptr;
    }

    // Invokes `verb` and returns its command.result.
    QJsonObject invoke(LoopbackTransport* app, const QByteArray& verb,
                       const QList<MirrorUpdate>& arguments = {})
    {
        const quint32 id = nextCommandId++;
        app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
        const auto find = [app, id]() {
            for (const QJsonObject& o : ofType(app->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == id) {
                    return o;
                }
            }
            return QJsonObject{};
        };
        const bool answered = QTest::qWaitFor([&find]() { return !find().isEmpty(); }, 5000);
        Q_UNUSED(answered);  // an empty result fails the caller's check
        return find();
    }

    // A fresh sign-in's auth.result, refused or not.
    QJsonObject deviceSignInResult(const Device& device)
    {
        LoopbackTransport* app = open();
        const QJsonObject hello = firstOfType(app->received(), QStringLiteral("hello"));
        const QByteArray challenge =
            StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("NereusSDR iPhone"),
            {kSessionProtocolMajor}, {{"deviceAuth", 1}})));
        app->sendText(SessionMessages::encode(SessionMessages::authRequest(
            QString(), device.block(challenge, certSha256(), server->stationIdentity().publicKeySpki()))));
        const bool answered = QTest::qWaitFor(
            [app]() { return !firstOfType(app->received(), QStringLiteral("auth.result")).isEmpty(); },
            5000);
        Q_UNUSED(answered);
        return firstOfType(app->received(), QStringLiteral("auth.result"));
    }

    QJsonObject tokenSignInResult(const QString& token)
    {
        LoopbackTransport* app = open();
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("window"))));
        app->sendText(SessionMessages::encode(SessionMessages::authRequest(token)));
        const bool answered = QTest::qWaitFor(
            [app]() { return !firstOfType(app->received(), QStringLiteral("auth.result")).isEmpty(); },
            5000);
        Q_UNUSED(answered);
        return firstOfType(app->received(), QStringLiteral("auth.result"));
    }
};

// The session.end `app` received, once its connection has closed.
QJsonObject endOf(LoopbackTransport* app)
{
    const bool closed = QTest::qWaitFor([app]() { return !app->isOpen(); }, 5000);
    Q_UNUSED(closed);
    return firstOfType(app->received(), QStringLiteral("session.end"));
}

void verifyEnded(LoopbackTransport* app, const QString& reason, const QString& code)
{
    const QJsonObject end = endOf(app);
    QVERIFY2(!end.isEmpty(), "no session.end arrived");
    QVERIFY(!app->isOpen());
    QCOMPARE(end.value(QStringLiteral("reason")).toString(), reason);
    QCOMPARE(end.value(QStringLiteral("retryable")).toBool(true), false);
    QCOMPARE(end.value(QStringLiteral("code")).toString(), code);
    QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
}

QJsonArray listOf(const StationDevicesFacade& devices)
{
    return QJsonDocument::fromJson(devices.listJson().toUtf8()).array();
}

} // namespace

class TstStationDevices : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // RadioModel reads AppSettings::instance(); keep this process's
        // copy private.
        const QString profile =
            QStringLiteral("station-devices-%1").arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ── Who receives the object and may use the verbs ────────────────────

    void theObjectGoesOnlyToADeviceThatDeclaresDeviceAuthAtMinor11()
    {
        Core core(/*upgradedWithToken=*/true);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));

        // Today's desktop: minor 11, no features.
        LoopbackTransport* window = core.tokenSession({});
        QVERIFY(window != nullptr);
        QVERIFY(!sentDevicesObject(window->received()));
        QJsonObject refused = core.invoke(window, "station.rename",
                                          {utf8("label", QStringLiteral("KG4VCF/shack"))});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kUpdateApp);
        QVERIFY(!core.settings->contains(QStringLiteral("StationLabel")));

        // An older peer that declares deviceAuth: not at minor 11.
        LoopbackTransport* older =
            core.deviceSession(phone, quint16(kRadioIdentitySessionProtocolMinor - 1));
        QVERIFY(older != nullptr);
        QVERIFY(!sentDevicesObject(older->received()));
        refused = core.invoke(older, "station.retireToken");
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kUpdateApp);
        QVERIFY(core.devices().tokenActive());

        // A device at minor 11 that declares deviceAuth.
        LoopbackTransport* current = core.deviceSession(phone);
        QVERIFY(current != nullptr);
        QVERIFY(sentDevicesObject(current->received()));
    }

    void capabilityIsLastInTheMinor11BlockOnly()
    {
        StationCapabilities caps;
        caps.radioIdentityEntries = true;
        caps.stationIdentityVersion = 1;
        caps.deviceAdminVersion = 1;
        const QList<MirrorUpdate> updates = caps.toUpdates();
        // Keep the original minor-11 entries contiguous and ordered. Locate
        // the block by its first entry: later append-only capabilities must
        // not make this assertion depend on the number of trailing entries.
        const QList<QByteArray> originalBlock = {
            "stationIdentityVersion", "deviceAdminVersion", "pairingVersion",
            "stationCatalogVersion", "displayExtrasVersion", "transmitSettingsVersion",
            "bandSelectVersion", "meterReadingsVersion", "dspInfoVersion",
            "recordStreamVersion", "stationRadiosVersion", "txDisplayVersion",
            "displayClockVersion", "controlChannelVersion", "txMonitorAudioVersion",
            "stationFreedvVersion", "mediaReplaceVersion", "controlSwitchVersion",
            "relayAllowed", "supportBundleVersion", "mediaTunnelVersion",
            "mediaRelayRoutingVersion"};
        auto indexOf = [&updates](const QByteArray& name) {
            for (int i = 0; i < updates.size(); ++i) {
                if (updates.at(i).name == name) { return i; }
            }
            return -1;
        };
        const int blockStart = indexOf(originalBlock.first());
        QVERIFY(blockStart >= 0);
        QVERIFY(blockStart + originalBlock.size() <= updates.size());
        for (int i = 0; i < originalBlock.size(); ++i) {
            QCOMPARE(updates.at(blockStart + i).name, originalBlock.at(i));
        }
        const int blockEnd = blockStart + originalBlock.size() - 1;
        const int remoteIq = indexOf("remoteIqVersion");
        const int txModMonitor = indexOf("txModMonitorVersion");
        const int accessoryTx = indexOf("accessoryTxVersion");
        QVERIFY(remoteIq > blockEnd);
        QVERIFY(txModMonitor > remoteIq);
        QVERIFY(accessoryTx > txModMonitor);
        QCOMPARE(StationCapabilities::fromUpdates(updates).deviceAdminVersion, 1);

        caps.radioIdentityEntries = false;
        for (const MirrorUpdate& u : caps.toUpdates()) {
            QVERIFY(u.name != "deviceAdminVersion");
        }

        Core core(false);
        QCOMPARE(core.server->deviceAdminVersion(), 1);
    }

    // ── Revoke ───────────────────────────────────────────────────────────

    void revokingItselfEndsTheConnectionAfterTheResult()
    {
        Core core(false);
        Device phone;
        Device tablet;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        // Not the last device (that is refused; see below).
        QVERIFY(core.server->deviceStore()->add(tablet.record(QStringLiteral("Shack iPad"))));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);

        const QJsonObject result = core.invoke(app, "devices.revoke", {utf8("id", phone.id())});
        QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(result.value(QStringLiteral("reason")).toString()));
        verifyEnded(app, kRemoved, QStringLiteral("deviceRemoved"));
        // The result came before the end.
        const QList<QByteArray> kinds = app->receivedKinds();
        QVERIFY(kinds.lastIndexOf(QByteArrayLiteral("command.result"))
                < kinds.lastIndexOf(QByteArrayLiteral("session.end")));
        QVERIFY(!core.server->deviceStore()->find(phone.key.fingerprint()));

        // And it cannot sign in again.
        const QJsonObject again = core.deviceSignInResult(phone);
        QCOMPARE(again.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(again.value(QStringLiteral("code")).toString(),
                 QStringLiteral("deviceNotPaired"));
    }

    void aDeviceRemovedOutsideAVerbLosesItsConnectionAtOnce()
    {
        // The console's revoke and a reset remove from the store directly.
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        QVERIFY(core.server->hasAuthenticatedSession());

        QVERIFY(core.server->deviceStore()->remove(phone.key.fingerprint()));
        // Synchronously: the session is gone before remove() returns.
        QVERIFY(!core.server->hasAuthenticatedSession());
        verifyEnded(app, kRemoved, QStringLiteral("deviceRemoved"));
    }

    void revokingAnotherDeviceLeavesThisConnectionUp()
    {
        Core core(false);
        Device phone;
        Device tablet;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        QVERIFY(core.server->deviceStore()->add(tablet.record(QStringLiteral("Shack iPad"))));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        QCOMPARE(listOf(core.devices()).size(), 2);

        const QJsonObject result = core.invoke(app, "devices.revoke", {utf8("id", tablet.id())});
        QVERIFY(result.value(QStringLiteral("accepted")).toBool());
        QVERIFY(app->isOpen());
        QVERIFY(firstOfType(app->received(), QStringLiteral("session.end")).isEmpty());
        const QJsonArray list = listOf(core.devices());
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.at(0).toObject().value(QStringLiteral("id")).toString(), phone.id());
        QCOMPARE(core.deviceSignInResult(tablet).value(QStringLiteral("code")).toString(),
                 QStringLiteral("deviceNotPaired"));
    }

    void revokingTheLastDeviceIsRefusedWhileNoTokenIsActive()
    {
        // Fix wave R1-I1: removing the last device of a Core with no token
        // would make it unclaimed again, open to one tap on its network and
        // to codes from anywhere. The pairing design closes the window for
        // good at the first pair; physical access (the console's reset) is
        // how a Core becomes unclaimed again.
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        QVERIFY(!core.devices().tokenActive());

        const QJsonObject result = core.invoke(app, "devices.revoke", {utf8("id", phone.id())});
        QCOMPARE(result.value(QStringLiteral("accepted")).toBool(true), false);
        const QString reason = result.value(QStringLiteral("reason")).toString();
        QCOMPARE(reason, QStringLiteral("Pair another device first, or reset this Core from its "
                                        "own computer."));
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QVERIFY(app->isOpen());
        QVERIFY(core.server->deviceStore()->find(phone.key.fingerprint()));
        QVERIFY(core.devices().claimed());
        QCOMPARE(core.server->pairingWindow()->state(), PairingWindow::State::ClosedClaimed);
        QVERIFY(core.server->pairingWindow()->currentCode().isEmpty());
    }

    void revokingTheLastDeviceIsAcceptedWhileTheTokenIsActive()
    {
        // The token keeps the Core claimed, so the window stays shut.
        Core core(/*upgradedWithToken=*/true);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        const QJsonObject result = core.invoke(app, "devices.revoke", {utf8("id", phone.id())});
        QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(result.value(QStringLiteral("reason")).toString()));
        verifyEnded(app, kRemoved, QStringLiteral("deviceRemoved"));
        QVERIFY(core.devices().claimed());
        QCOMPARE(core.server->pairingWindow()->state(), PairingWindow::State::ClosedClaimed);
    }

    void aComputerEnrolledThroughTheTokenIsNotRevokedWhileTheTokenWorks()
    {
        // Fix wave R1-I3: the token would enrol it again at its next
        // sign-in, so removing it while the token works keeps no one out.
        Core core(/*upgradedWithToken=*/true);
        Device computer;
        LoopbackTransport* window = core.tokenSession({{"deviceAuth", 1}}, kSessionProtocolMinor,
                                                      &computer);
        QVERIFY(window != nullptr);
        const std::optional<PairedDevice> enrolled =
            core.server->deviceStore()->find(computer.key.fingerprint());
        QVERIFY(enrolled && enrolled->enrolledThroughToken);

        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        QJsonObject result = core.invoke(app, "devices.revoke", {utf8("id", computer.id())});
        QCOMPARE(result.value(QStringLiteral("accepted")).toBool(true), false);
        const QString reason = result.value(QStringLiteral("reason")).toString();
        QCOMPARE(reason, QStringLiteral("Stop accepting the pairing token first, then remove "
                                        "this computer."));
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QVERIFY(core.server->deviceStore()->find(computer.key.fingerprint()));

        // With the token retired, it goes.
        result = core.invoke(app, "station.retireToken");
        QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(result.value(QStringLiteral("reason")).toString()));
        result = core.invoke(app, "devices.revoke", {utf8("id", computer.id())});
        QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(result.value(QStringLiteral("reason")).toString()));
        QVERIFY(!core.server->deviceStore()->find(computer.key.fingerprint()));
    }

    // Slice control plan Task 8b (JJ's bench): a computer that joined with
    // the token is removed from the Core in one step, the Core first
    // stopping the token. Afterwards the token lets nobody in, the computer
    // is gone, and another paired device still signs in.
    void aComputerThatJoinedWithTheTokenIsRemovedInOneStep()
    {
        Core core(/*upgradedWithToken=*/true);
        Device computer;
        computer.name = QStringLiteral("MacBook-Pro (radxa_5c_r3)");
        LoopbackTransport* window = core.tokenSession({{"deviceAuth", 1}}, kSessionProtocolMinor,
                                                      &computer);
        QVERIFY(window != nullptr);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        QVERIFY(core.deviceSession(phone) != nullptr);
        QVERIFY(core.devices().tokenActive());
        QVERIFY(core.devices().revokeStopsPairingToken(computer.id()));
        QVERIFY(!core.devices().revokeStopsPairingToken(phone.id()));

        const DeviceAdminResult result = core.devices().retireTokenAndRevoke(computer.id());
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QVERIFY(!core.devices().tokenActive());
        QVERIFY(!core.server->deviceStore()->find(computer.key.fingerprint()));
        QTRY_VERIFY(!window->isOpen());
        // The token no longer lets anyone in; the phone still signs in.
        QVERIFY(core.tokenSession() == nullptr);
        QVERIFY(core.deviceSession(phone) != nullptr);
        QVERIFY(core.devices().claimed());
    }

    // With no other paired device the one step is refused in the existing
    // words, and nothing changes: the token still works, the computer is
    // still paired.
    void theOneStepIsRefusedWhenItWouldLeaveNoPairedDevice()
    {
        Core core(/*upgradedWithToken=*/true);
        Device computer;
        LoopbackTransport* window = core.tokenSession({{"deviceAuth", 1}}, kSessionProtocolMinor,
                                                      &computer);
        QVERIFY(window != nullptr);
        QCOMPARE(core.server->deviceStore()->list().size(), 1);
        const DeviceAdminResult result = core.devices().retireTokenAndRevoke(computer.id());
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("Pair another device first, or reset this Core "
                                               "from its own computer."));
        QVERIFY(OperatorWording::isPlain(result.reason));
        QVERIFY(core.devices().tokenActive());
        QVERIFY(core.server->deviceStore()->find(computer.key.fingerprint()));
        QVERIFY(window->isOpen());
    }

    // Any other device: the one step is revoke() as it is.
    void theOneStepForAnyOtherDeviceIsARevoke()
    {
        Core core(/*upgradedWithToken=*/true);
        Device phone;
        Device tablet;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        QVERIFY(core.server->deviceStore()->add(tablet.record()));
        QVERIFY(!core.devices().revokeStopsPairingToken(tablet.id()));
        QVERIFY(core.devices().retireTokenAndRevoke(tablet.id()).accepted);
        QVERIFY(core.devices().tokenActive());
        QVERIFY(!core.server->deviceStore()->find(tablet.key.fingerprint()));
    }

    // Task 8b: each sign-in's name is the one the Core lists, as the short
    // name is; so a window that now names its profile is listed by it.
    void eachSignInCarriesTheNameTheCoreStores()
    {
        Core core(false);
        Device computer;
        QVERIFY(core.server->deviceStore()->add(computer.record()));
        computer.name = QStringLiteral("MacBook-Pro (radxa_5c_r3)");
        QVERIFY(core.deviceSession(computer) != nullptr);
        QCOMPARE(core.server->deviceStore()->find(computer.key.fingerprint())->name,
                 QStringLiteral("MacBook-Pro (radxa_5c_r3)"));
        // An unusable one leaves it as it is.
        computer.name = QString(65, QLatin1Char('a'));
        QVERIFY(core.deviceSession(computer) != nullptr);
        QCOMPARE(core.server->deviceStore()->find(computer.key.fingerprint())->name,
                 QStringLiteral("MacBook-Pro (radxa_5c_r3)"));
    }

    void eachSignInCarriesTheShortNameTheCoreStores()
    {
        // Part C fix wave: `shortName` in auth.request's device block,
        // outside the signed transcript; stored, and replaced at each
        // sign-in that carries a usable one.
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        phone.shortName = QStringLiteral("Grant's iPhone");
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        QCOMPARE(core.server->deviceStore()->find(phone.key.fingerprint())->shortName,
                 QStringLiteral("Grant's iPhone"));
        // Without one, or with one past 32 bytes, the stored one stays.
        for (const QString& next : {QString(), QString(33, QLatin1Char('a'))}) {
            phone.shortName = next;
            QVERIFY(core.deviceSession(phone) != nullptr);
            QCOMPARE(core.server->deviceStore()->find(phone.key.fingerprint())->shortName,
                     QStringLiteral("Grant's iPhone"));
        }
        phone.shortName = QStringLiteral("iPhone");
        QVERIFY(core.deviceSession(phone) != nullptr);
        QCOMPARE(core.server->deviceStore()->find(phone.key.fingerprint())->shortName,
                 QStringLiteral("iPhone"));

        // A window enrolling through the token keeps its short name too.
        Core upgraded(/*upgradedWithToken=*/true);
        Device window;
        window.shortName = QStringLiteral("MacBook");
        QVERIFY(upgraded.tokenSession({{"deviceAuth", 1}}, kSessionProtocolMinor, &window)
                != nullptr);
        const std::optional<PairedDevice> enrolled =
            upgraded.server->deviceStore()->find(window.key.fingerprint());
        QVERIFY(enrolled.has_value());
        QCOMPARE(enrolled->shortName, QStringLiteral("MacBook"));
    }

    void revokeRefusesWhatItCannotDo()
    {
        Core core(false);
        Device phone;
        Device stranger;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        for (const QString& id : {stranger.id(), QStringLiteral("not base64url!"), QString()}) {
            const QJsonObject result = core.invoke(app, "devices.revoke", {utf8("id", id)});
            QCOMPARE(result.value(QStringLiteral("accepted")).toBool(true), false);
            QCOMPARE(result.value(QStringLiteral("reason")).toString(),
                     QStringLiteral("That device is not paired with this Core."));
        }
        // An argument it does not take.
        const QJsonObject wrong =
            core.invoke(app, "devices.revoke", {utf8("device", phone.id())});
        QCOMPARE(wrong.value(QStringLiteral("accepted")).toBool(true), false);
        QVERIFY(app->isOpen());
        QVERIFY(core.server->deviceStore()->find(phone.key.fingerprint()));
    }

    // ── The token ────────────────────────────────────────────────────────

    void retiringTheTokenIsRefusedUntilADeviceIsPaired()
    {
        Core core(/*upgradedWithToken=*/true);
        const QString token = core.server->token();
        LoopbackTransport* window = core.tokenSession();
        QVERIFY(window != nullptr);
        const QJsonObject result = core.invoke(window, "station.retireToken");
        QCOMPARE(result.value(QStringLiteral("accepted")).toBool(true), false);
        const QString reason = result.value(QStringLiteral("reason")).toString();
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QVERIFY(reason.contains(QStringLiteral("Pair a device")));
        QVERIFY(window->isOpen());
        QVERIFY(core.devices().tokenActive());
        QVERIFY(QFile::exists(QDir(core.securityDir.path()).filePath(QStringLiteral("station-token"))));
        // The token still signs in.
        QVERIFY(core.tokenSignInResult(token).value(QStringLiteral("accepted")).toBool());
    }

    void retiringTheTokenEndsEveryTokenConnection()
    {
        Core core(/*upgradedWithToken=*/true);
        const QString token = core.server->token();
        Device window;
        // A window signing in with the token enrols its device key, and is
        // still a token connection.
        LoopbackTransport* app = core.tokenSession({{"deviceAuth", 1}}, kSessionProtocolMinor,
                                                   &window);
        QVERIFY(app != nullptr);
        QVERIFY(core.server->deviceStore()->find(window.key.fingerprint()));
        QVERIFY(core.devices().tokenActive());
        const quint32 before = core.devices().revision();

        const QJsonObject result = core.invoke(app, "station.retireToken");
        QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(result.value(QStringLiteral("reason")).toString()));
        verifyEnded(app, kPairFirst, QStringLiteral("pairingRequired"));
        QVERIFY(!core.devices().tokenActive());
        QCOMPARE(core.devices().revision(), before + 2);  // the token, then the disconnect
        QVERIFY(!QFile::exists(QDir(core.securityDir.path()).filePath(QStringLiteral("station-token"))));

        // A later token sign-in is refused; the enrolled key signs in.
        const QJsonObject refused = core.tokenSignInResult(token);
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kPairFirst);
        QCOMPARE(refused.value(QStringLiteral("code")).toString(),
                 QStringLiteral("pairingRequired"));
        QVERIFY(core.deviceSession(window) != nullptr);
    }

    void retiringTheTokenFromOutsideAConnectionEndsTokenConnections()
    {
        // The console retires it (Task 17) through the same facade.
        Core core(/*upgradedWithToken=*/true);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* window = core.tokenSession({});
        QVERIFY(window != nullptr);
        QVERIFY(core.devices().retireToken().accepted);
        QVERIFY(!core.server->hasAuthenticatedSession());
        verifyEnded(window, kPairFirst, QStringLiteral("pairingRequired"));
    }

    void aDeviceConnectionOutlivesTheTokenRetirement()
    {
        Core core(/*upgradedWithToken=*/true);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        QVERIFY(core.invoke(app, "station.retireToken").value(QStringLiteral("accepted")).toBool());
        QVERIFY(app->isOpen());
        QVERIFY(firstOfType(app->received(), QStringLiteral("session.end")).isEmpty());
        // A second retirement changes nothing.
        QVERIFY(core.invoke(app, "station.retireToken").value(QStringLiteral("accepted")).toBool());
    }

    // ── The name and the key backup ──────────────────────────────────────

    void rawWritesOfTheCoresOwnKeysAreRefused()
    {
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        for (const QString& key : {QStringLiteral("StationLabel"),
                                   QStringLiteral("stationlabel"),
                                   QStringLiteral("StationKeyBackupAcknowledged")}) {
            app->clearReceived();
            app->sendText(SessionMessages::encode(
                SessionMessages::settingsWrite(key, QStringLiteral("W1AW"), QStringLiteral("x"))));
            app->sendText(SessionMessages::encode(SessionMessages::settingsRemove(key)));
            QVERIFY(QTest::qWaitFor(
                [app]() { return ofType(app->received(), QStringLiteral("settings.reject")).size() == 2; },
                5000));
            for (const QJsonObject& reject : ofType(app->received(), QStringLiteral("settings.reject"))) {
                const QString reason = reject.value(QStringLiteral("reason")).toString();
                QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
                if (key.compare(QStringLiteral("StationLabel"), Qt::CaseInsensitive) == 0) {
                    QCOMPARE(reason, kNameRefusal);
                }
            }
            QVERIFY(!core.settings->contains(key));
        }
        QVERIFY(!core.devices().keyBackupAcknowledged());
        QCOMPARE(core.devices().stationLabel(), QString());
    }

    void theLabelFollowsTheCallsignUntilTheFirstRename()
    {
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.deviceSession(phone);
        QVERIFY(app != nullptr);
        QSignalSpy labelChanged(&core.devices(), &StationDevicesFacade::stationLabelChanged);
        QCOMPARE(core.devices().stationLabel(), QString());

        // The callsign changes through the settings (a window's write).
        app->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            QStringLiteral("StationCallsign"), QStringLiteral("KG4VCF"), QStringLiteral("x"))));
        QVERIFY(QTest::qWaitFor(
            [&core]() { return core.devices().stationLabel() == QStringLiteral("KG4VCF"); }, 5000));
        QCOMPARE(labelChanged.count(), 1);

        // An invalid label is refused with the rule.
        for (const QString& bad : {QStringLiteral("KG4VCF/shack!"), QStringLiteral(""),
                                   QStringLiteral("KG4VCF/") + QString(33, QLatin1Char('a'))}) {
            const QJsonObject refused = core.invoke(app, "station.rename", {utf8("label", bad)});
            QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
            QCOMPARE(refused.value(QStringLiteral("reason")).toString(), StationLabel::ruleText());
        }
        QVERIFY(OperatorWording::isPlain(StationLabel::ruleText()));
        QCOMPARE(OperatorWording::coreCalledStationIn(StationLabel::ruleText()), QString());
        QCOMPARE(labelChanged.count(), 1);

        const quint32 before = core.devices().revision();
        const QJsonObject renamed =
            core.invoke(app, "station.rename", {utf8("label", QStringLiteral(" KG4VCF/Shack "))});
        QVERIFY(renamed.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(core.settings->value(QStringLiteral("StationLabel")).toString(),
                 QStringLiteral("KG4VCF/Shack"));
        QCOMPARE(core.devices().stationLabel(), QStringLiteral("KG4VCF/Shack"));
        QCOMPARE(core.devices().revision(), before + 1);
        QCOMPARE(labelChanged.count(), 2);
        QCOMPARE(labelChanged.last().at(0).toString(), QStringLiteral("KG4VCF/Shack"));
        // The connected device hears it.
        QVERIFY(QTest::qWaitFor([app]() {
            for (const QJsonObject& d : ofType(app->received(), QStringLiteral("delta"))) {
                for (const QJsonValue& p : d.value(QStringLiteral("properties")).toArray()) {
                    if (d.value(QStringLiteral("key")).toString() == QStringLiteral("devices")
                        && p.toObject().value(QStringLiteral("name")).toString()
                               == QStringLiteral("stationLabel")
                        && p.toObject().value(QStringLiteral("value")).toString()
                               == QStringLiteral("KG4VCF/Shack")) {
                        return true;
                    }
                }
            }
            return false;
        }, 5000));

        // From now on the callsign no longer moves the label.
        app->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            QStringLiteral("StationCallsign"), QStringLiteral("W1AW"), QStringLiteral("x"))));
        QVERIFY(QTest::qWaitFor([&core]() {
            return core.settings->value(QStringLiteral("StationCallsign")).toString()
                == QStringLiteral("W1AW");
        }, 5000));
        QCOMPARE(core.devices().stationLabel(), QStringLiteral("KG4VCF/Shack"));
        QCOMPARE(labelChanged.count(), 2);
    }

    void theKeyBackupIsAcknowledgedOnlyThroughItsVerbForThisKey()
    {
        QTemporaryDir firstKey;
        QTemporaryDir secondKey;
        QTemporaryDir sharedSettings;
        const QString settingsPath = sharedSettings.filePath(QStringLiteral("NereusSDR.settings"));
        {
            AppSettings settings(settingsPath);
            const StationIdentity identity = StationIdentity::loadOrCreate(firstKey.path());
            DeviceStore store(firstKey.path());
            TokenStore tokens(firstKey.path());
            StationDevicesFacade devices(store, tokens, identity, settings);
            QVERIFY(!devices.keyBackupAcknowledged());
            QCOMPARE(devices.keyPath(), identity.keyPath());
            const quint32 before = devices.revision();
            QVERIFY(devices.acknowledgeKeyBackup().accepted);
            QVERIFY(devices.keyBackupAcknowledged());
            QCOMPARE(devices.revision(), before + 1);
            // Again: nothing changes.
            QVERIFY(devices.acknowledgeKeyBackup().accepted);
            QCOMPARE(devices.revision(), before + 1);
        }
        {
            // The same settings, read back: still acknowledged for this key.
            AppSettings settings(settingsPath);
            settings.load();
            const StationIdentity identity = StationIdentity::loadOrCreate(firstKey.path());
            DeviceStore store(firstKey.path());
            TokenStore tokens(firstKey.path());
            StationDevicesFacade devices(store, tokens, identity, settings);
            QVERIFY(devices.keyBackupAcknowledged());
        }
        {
            // A replaced key asks again.
            AppSettings settings(settingsPath);
            settings.load();
            const StationIdentity identity = StationIdentity::loadOrCreate(secondKey.path());
            DeviceStore store(secondKey.path());
            TokenStore tokens(secondKey.path());
            StationDevicesFacade devices(store, tokens, identity, settings);
            QVERIFY(!devices.keyBackupAcknowledged());
        }
    }

    // ── Revision and the list ────────────────────────────────────────────

    void everyChangeMovesRevisionOnce()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        DeviceStore store(dir.path());
        TokenStore tokens(dir.path());
        StationDevicesFacade devices(store, tokens, identity, settings);
        QSignalSpy changed(&devices, &StationDevicesFacade::devicesStateChanged);
        QCOMPARE(devices.claimed(), false);
        QCOMPARE(devices.tokenActive(), false);
        quint32 revision = devices.revision();

        Device phone;
        QVERIFY(store.add(phone.record()));
        QCOMPARE(devices.revision(), ++revision);
        QVERIFY(devices.claimed());

        // One sign-in: lastSeen and connected together are one change.
        devices.holdRefresh();
        store.touch(phone.key.fingerprint(), QStringLiteral("192.0.2.7"));
        devices.setConnectedDevices({phone.key.fingerprint()});
        QCOMPARE(devices.revision(), revision);
        devices.resumeRefresh();
        QCOMPARE(devices.revision(), ++revision);
        QJsonObject entry = listOf(devices).at(0).toObject();
        QCOMPARE(entry.value(QStringLiteral("connected")).toBool(), true);
        QCOMPARE(entry.value(QStringLiteral("id")).toString(), phone.id());
        QCOMPARE(entry.value(QStringLiteral("name")).toString(), QStringLiteral("Shack iPhone"));
        QCOMPARE(entry.value(QStringLiteral("kind")).toString(), QStringLiteral("phone"));
        QVERIFY(entry.value(QStringLiteral("pairedAt")).toString().endsWith(QLatin1Char('Z')));
        QVERIFY(!entry.value(QStringLiteral("lastSeen")).toString().isEmpty());
        // The same set again is no change.
        devices.setConnectedDevices({phone.key.fingerprint()});
        QCOMPARE(devices.revision(), revision);

        QVERIFY(devices.rename(QStringLiteral("KG4VCF/shack")).accepted);
        QCOMPARE(devices.revision(), ++revision);
        QVERIFY(devices.acknowledgeKeyBackup().accepted);
        QCOMPARE(devices.revision(), ++revision);
        Device tablet;
        QVERIFY(store.add(tablet.record(QStringLiteral("Shack iPad"))));
        QCOMPARE(devices.revision(), ++revision);
        QVERIFY(devices.revoke(tablet.id()).accepted);
        QCOMPARE(devices.revision(), ++revision);
        QCOMPARE(listOf(devices).size(), 1);
        QVERIFY(devices.claimed());
        QCOMPARE(changed.count(), 6);

        // Refusals change nothing: a device no longer paired, and the last
        // one while no token is active.
        QVERIFY(!devices.rename(QStringLiteral("not a label")).accepted);
        QVERIFY(!devices.revoke(tablet.id()).accepted);
        QVERIFY(!devices.revoke(phone.id()).accepted);
        // With no token, retiring it has nothing to do.
        QVERIFY(devices.retireToken().accepted);
        QCOMPARE(devices.revision(), revision);
    }

    void everyReasonIsPlainAndSaysCore()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        DeviceStore store(dir.path());
        TokenStore tokens(dir.path());
        StationDevicesFacade devices(store, tokens, identity, settings);
        const QStringList reasons{
            devices.revoke(QStringLiteral("x")).reason,
            devices.rename(QStringLiteral("!")).reason,
            devices.retireToken().reason,
            kRemoved, kPairFirst, kUpdateApp, kNameRefusal,
            QStringLiteral("This Core cannot manage its paired devices."),
            QStringLiteral("The request to remove a device was not understood."),
            QStringLiteral("The request to rename the Core was not understood."),
            QStringLiteral("The request to confirm the key backup was not understood."),
            QStringLiteral("The request to stop accepting the pairing token was not understood."),
        };
        for (const QString& reason : reasons) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
            QVERIFY2(OperatorWording::coreCalledStationIn(reason).isEmpty(), qPrintable(reason));
        }
    }
};

QTEST_GUILESS_MAIN(TstStationDevices)
#include "tst_station_devices.moc"
