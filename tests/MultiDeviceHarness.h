#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/MultiDeviceHarness.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 74 (R-IOS-02, R-IOS-30): the several-devices
// harness of tst_station_multi_session (Tasks 71 to 73), shared by
// tst_station_multi_session, tst_confirm_step and tst_antenna_kept: one
// Core over the in-process loopback, in scratch directories, with an
// injected monotonic clock, and devices signed in by keys made at run time.
// Its helpers sit in an unnamed namespace: include it once per test
// executable.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 74 (R-IOS-02, R-IOS-30),
//               copied from tst_station_multi_session's harness, with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: fix wave after the group review of Tasks 71 to 76: the one
//               harness; tst_station_multi_session includes it instead of
//               its own copy. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: merge of the trunk into the transmit lane: the transmit
//               helpers of StationMultiSessionHarness.h (iPhone app plan
//               Task 34), which it replaces; latestCapabilityIf is its
//               latestCapability. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C1: openFakeMicrophoneLines
//               (allowTransmit). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: one TLS identity per test process (R-R3-49). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QCryptographicHash>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <atomic>
#include <memory>
#include <optional>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/DeviceLayoutStore.h"
#include "core/SliceOwnership.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationIdentity.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/RemoteKeying.h"
#include "core/session/StationServer.h"
#include "core/WdspTypes.h"
#include "core/MoxController.h"
#include "core/safety/TransmitHolder.h"
#include "core/TxSliceArbiter.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "models/TransmitModel.h"
#include "core/dsp/DspAssetService.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"


using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kCoreFull = QStringLiteral("The Core already has four devices connected.");
const QString kSameDevice = QStringLiteral("This device connected again.");
const QString kUnknownVerb =
    QStringLiteral("The Core does not know this request. Updating the Core may help.");

const QHash<QByteArray, int> kHolder{{"deviceAuth", 1}, {"sessionHolder", 1}};

// A device key made at run time, with the words it signs in with.
struct Device {
    QTemporaryDir dir;
    StationIdentity key = StationIdentity::loadOrCreate(dir.path());
    QString name = QStringLiteral("iPhone");
    QString kind = QStringLiteral("phone");
    QString shortName;

    Device() = default;
    Device(const QString& n, const QString& k, const QString& s = QString())
        : name(n), kind(k), shortName(s)
    {
    }

    PairedDevice record() const
    {
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = name;
        device.kind = kind;
        return device;
    }

    QString id() const { return StationIdentity::toBase64Url(key.fingerprint()); }

    SessionDeviceBlock block(const QByteArray& challenge, const QByteArray& certSha256,
                             const QByteArray& stationSpki) const
    {
        return SessionDeviceBlock{
            id(), StationIdentity::toBase64Url(key.publicKeySpki()), name, kind,
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

// The latest value of `property` on the object `key`, from its create or a
// later delta; invalid when none arrived.
QJsonValue latest(const QList<QByteArray>& received, const QString& key, const QString& property)
{
    QJsonValue value;
    for (const QByteArray& wire : received) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        const QString type = o.value(QStringLiteral("type")).toString();
        if ((type != QStringLiteral("object.create") && type != QStringLiteral("delta"))
            || o.value(QStringLiteral("key")).toString() != key) {
            continue;
        }
        for (const QJsonValue& p : o.value(QStringLiteral("properties")).toArray()) {
            if (p.toObject().value(QStringLiteral("name")).toString() == property) {
                value = p.toObject().value(QStringLiteral("value"));
            }
        }
    }
    return value;
}

QJsonArray connectedList(const LoopbackTransport* app)
{
    return QJsonDocument::fromJson(
               latest(app->received(), QStringLiteral("connectedDevices"),
                      QStringLiteral("listJson"))
                   .toString()
                   .toUtf8())
        .array();
}

QJsonArray devicesList(const LoopbackTransport* app)
{
    return QJsonDocument::fromJson(
               latest(app->received(), QStringLiteral("devices"), QStringLiteral("listJson"))
                   .toString()
                   .toUtf8())
        .array();
}

QJsonObject entryFor(const QJsonArray& list, const QString& deviceId)
{
    for (const QJsonValue& v : list) {
        if (v.toObject().value(QStringLiteral("deviceId")).toString() == deviceId
            || v.toObject().value(QStringLiteral("id")).toString() == deviceId) {
            return v.toObject();
        }
    }
    return {};
}

bool sentConnectedDevices(const QList<QByteArray>& received)
{
    for (const QJsonObject& o : ofType(received, QStringLiteral("object.create"))) {
        if (o.value(QStringLiteral("key")).toString() == QStringLiteral("connectedDevices")) {
            return true;
        }
    }
    for (const QJsonObject& o : ofType(received, QStringLiteral("schema"))) {
        if (o.value(QStringLiteral("class")).toString()
            == QStringLiteral("ConnectedDevicesFacade")) {
            return true;
        }
    }
    return false;
}

std::optional<qint64> capability(const QList<QByteArray>& received, const QString& name)
{
    const QJsonObject caps = firstOfType(received, QStringLiteral("capabilities"));
    for (const QJsonValue& p : caps.value(QStringLiteral("properties")).toArray()) {
        if (p.toObject().value(QStringLiteral("name")).toString() == name) {
            return p.toObject().value(QStringLiteral("value")).toInteger();
        }
    }
    return std::nullopt;
}

// One Core over the loopback, in scratch directories, with an injected
// monotonic clock for its sessions.
struct Core {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;
    QList<LoopbackTransport*> clients;
    quint32 nextCommandId = 1;
    qint64 now = 0;

    explicit Core(bool upgradedWithToken = false)
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
        const QString dir = upgradedWithToken
                                ? NereusSDR::Test::seedUpgradedCoreToken(securityDir.path())
                                : NereusSDR::Test::seedCoreIdentity(securityDir.path());
        // R-R3-49: one TLS certificate per test process (UpgradedCoreToken.h).
        server = std::make_unique<StationServer>(model.get(), *settings,
                                                 NereusSDR::Test::withSharedTlsIdentity(dir));
        server->setHeartbeatIntervalMs(0);
        server->deviceSessions()->setClock([this]() { return now; });
    }

    ~Core()
    {
        server.reset();
        qDeleteAll(clients);
    }

    DeviceSessionRegistry& sessions() const { return *server->deviceSessions(); }

    void pair(const Device& device) const
    {
        QVERIFY(server->deviceStore()->add(device.record()));
    }

    QByteArray certSha256() const
    {
        QString pin = server->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        return QByteArray::fromHex(pin.toLatin1());
    }

    LoopbackTransport* open(const QString& address = QStringLiteral("192.0.2.7"))
    {
        auto* app = new LoopbackTransport(QStringLiteral("app"));
        auto* station = new LoopbackTransport(QStringLiteral("station"));
        station->setPeerAddress(address);
        station->linkTo(app);
        clients.append(app);
        server->acceptTransport(station);
        const bool greeted = QTest::qWaitFor([app]() { return !app->received().isEmpty(); }, 5000);
        Q_UNUSED(greeted);
        return app;
    }

    // Hello at `minor` declaring `features`, then `auth`; waits for the end
    // of the connect sequence or the close.
    static void send(LoopbackTransport* app, const SessionMessage& auth, quint16 minor,
                     const QHash<QByteArray, int>& features)
    {
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, minor, 0, QStringLiteral("NereusSDR iPhone"),
            {kSessionProtocolMajor}, features)));
        app->sendText(SessionMessages::encode(auth));
        const bool settled = QTest::qWaitFor(
            [app]() {
                return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))
                    || app->receivedKinds().contains(QByteArrayLiteral("session.held"))
                    || !app->isOpen();
            },
            5000);
        Q_UNUSED(settled);
    }

    SessionMessage deviceAuth(LoopbackTransport* app, const Device& device) const
    {
        const QJsonObject hello = firstOfType(app->received(), QStringLiteral("hello"));
        const QByteArray challenge =
            StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
        return SessionMessages::authRequest(
            QString(), device.block(challenge, certSha256(), server->stationIdentity().publicKeySpki()));
    }

    // A device's sign-in, however it ends: the client end.
    LoopbackTransport* signIn(const Device& device,
                              const QHash<QByteArray, int>& features = kHolder,
                              quint16 minor = kSessionProtocolMinor)
    {
        LoopbackTransport* app = open();
        send(app, deviceAuth(app, device), minor, features);
        return app;
    }

    LoopbackTransport* tokenSignIn(const QHash<QByteArray, int>& features = {},
                                   const QString& address = QStringLiteral("192.0.2.20"))
    {
        LoopbackTransport* app = open(address);
        send(app, SessionMessages::authRequest(server->token()), kSessionProtocolMinor, features);
        return app;
    }

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
        Q_UNUSED(answered);
        return find();
    }
};

bool admitted(const LoopbackTransport* app)
{
    return app != nullptr && app->isOpen()
        && app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"));
}

QJsonObject endOf(LoopbackTransport* app)
{
    const bool closed = QTest::qWaitFor([app]() { return !app->isOpen(); }, 5000);
    Q_UNUSED(closed);
    return firstOfType(app->received(), QStringLiteral("session.end"));
}

// Fix wave, the full-Core wording (the several-devices design, 15.2): an
// older window cannot answer the fifth-device question, so it is told to
// update or try later.
const QString kOlderWindowCoreFull =
    QStringLiteral("The Core is full. Update NereusSDR to take a device's place, or try again later.");

void verifyCoreFull(LoopbackTransport* app, const QString& reason = kCoreFull)
{
    const QJsonObject result = firstOfType(app->received(), QStringLiteral("auth.result"));
    QCOMPARE(result.value(QStringLiteral("accepted")).toBool(false), true);
    const QJsonObject end = endOf(app);
    QVERIFY(!app->isOpen());
    QCOMPARE(end.value(QStringLiteral("reason")).toString(), reason);
    QCOMPARE(end.value(QStringLiteral("retryable")).toBool(false), true);
    QVERIFY(!end.contains(QStringLiteral("code")));
    // Nothing of a session reached it.
    QVERIFY(!app->receivedKinds().contains(QByteArrayLiteral("capabilities")));
    QVERIFY(!app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));
}

void verifyHeld(LoopbackTransport* app, QJsonObject* out = nullptr)
{
    const QJsonObject auth = firstOfType(app->received(), QStringLiteral("auth.result"));
    QCOMPARE(auth.value(QStringLiteral("accepted")).toBool(), true);
    const QJsonObject held = firstOfType(app->received(), QStringLiteral("session.held"));
    QCOMPARE(held.value(QStringLiteral("devices")).toArray().size(), 4);
    QVERIFY(held.value(QStringLiteral("revision")).isDouble());
    QVERIFY(app->isOpen());
    QVERIFY(!app->receivedKinds().contains(QByteArrayLiteral("capabilities")));
    QVERIFY(!app->receivedKinds().contains(QByteArrayLiteral("settings.snapshot")));
    QVERIFY(!app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));
    if (out) *out = held;
}

// Waits until `app`'s latest connectedDevices list satisfies `test`.
bool waitForList(const LoopbackTransport* app, const std::function<bool(const QJsonArray&)>& test)
{
    return QTest::qWaitFor([app, &test]() { return test(connectedList(app)); }, 5000);
}

QStringList idsOf(const QJsonArray& list)
{
    QStringList ids;
    for (const QJsonValue& v : list) {
        ids.append(v.toObject().value(QStringLiteral("deviceId")).toString());
    }
    return ids;
}

MirrorUpdate utf8(const char* name, const QString& value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Utf8, QVariant(value)};
}

MirrorUpdate int64(const char* name, qint64 value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Int64, QVariant(value)};
}

// Every value `property` of `key` carried in a delta from message `from` on.
QList<QJsonValue> deltaValues(const QList<QByteArray>& received, int from, const QString& key,
                              const QString& property)
{
    QList<QJsonValue> values;
    for (int i = std::max(0, from); i < received.size(); ++i) {
        const QJsonObject o = QJsonDocument::fromJson(received.at(i)).object();
        if (o.value(QStringLiteral("type")).toString() != QStringLiteral("delta")
            || o.value(QStringLiteral("key")).toString() != key) {
            continue;
        }
        for (const QJsonValue& p : o.value(QStringLiteral("properties")).toArray()) {
            if (p.toObject().value(QStringLiteral("name")).toString() == property) {
                values.append(p.toObject().value(QStringLiteral("value")));
            }
        }
    }
    return values;
}

int countOfType(const QList<QByteArray>& received, int from, const QString& type)
{
    int count = 0;
    for (int i = std::max(0, from); i < received.size(); ++i) {
        if (QJsonDocument::fromJson(received.at(i)).object().value(QStringLiteral("type")).toString()
            == type) {
            ++count;
        }
    }
    return count;
}

// Task 73: whether `key` was created on `app` and not destroyed since.
bool holds(const LoopbackTransport* app, const QString& key)
{
    bool held = false;
    for (const QByteArray& wire : app->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("key")).toString() != key) {
            continue;
        }
        const QString type = o.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("object.create")) {
            held = true;
        } else if (type == QStringLiteral("object.destroy")) {
            held = false;
        }
    }
    return held;
}

// Every key with `prefix` `app` holds now.
QStringList heldKeys(const LoopbackTransport* app, const QString& prefix)
{
    QStringList keys;
    for (const QByteArray& wire : app->received()) {
        const QString key = QJsonDocument::fromJson(wire).object().value(QStringLiteral("key")).toString();
        if (key.startsWith(prefix) && !keys.contains(key) && holds(app, key)) {
            keys.append(key);
        }
    }
    keys.sort();
    return keys;
}

// Whether any object message of `app` (create, delta, destroy) carried
// `key`: a property.result names the key it answers and is not one.
bool everSaw(const LoopbackTransport* app, const QString& key)
{
    for (const QByteArray& wire : app->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        const QString type = o.value(QStringLiteral("type")).toString();
        if ((type == QStringLiteral("object.create") || type == QStringLiteral("delta")
             || type == QStringLiteral("object.destroy"))
            && o.value(QStringLiteral("key")).toString() == key) {
            return true;
        }
    }
    return false;
}

// Every value of the property.result for `writeId` on `app`.
QJsonObject propertyResult(const LoopbackTransport* app, qint64 writeId)
{
    for (const QJsonObject& o : ofType(app->received(), QStringLiteral("property.result"))) {
        if (o.value(QStringLiteral("writeId")).toInteger() == writeId) {
            return o;
        }
    }
    return {};
}

// The plain refusal for another device's slice (ruling 5.9).
QString ownedElsewhere(const QString& ownerName)
{
    return QStringLiteral("That slice belongs to %1. It can be changed only there.").arg(ownerName);
}

MirrorUpdate f64(const char* name, double value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Float64, QVariant(value)};
}

// Which slice ids hold at least one receiver, by receiver.
int receiversInUse(const RadioModel& model)
{
    QSet<int> streams;
    for (const SliceModel* slice : model.slices()) {
        if (slice->streamIndex() >= 0) {
            streams.insert(slice->streamIndex());
        }
    }
    return streams.size();
}

// A second slice 10 kHz from slice 0, so the two share one receiver and
// its one noise blanker (tst_mirror_inbound's co-hosting).
int addCoHostedSlice(RadioModel& model)
{
    model.configureStreamPool(5, 5, 192000);
    const int second = model.addSlice(QStringLiteral("pan-0"));
    model.sliceById(0)->setFrequency(14200000.0);
    model.sliceById(second)->setFrequency(14210000.0);
    return second;
}

// ---- iPhone app plan Task 34: transmit ------------------------------------

// A device that declares remote transmit (with several devices).
const QHash<QByteArray, int> kTransmitter{{"deviceAuth", 1}, {"sessionHolder", 1}, {"remoteTx", 1}};

// The latest value of capability `name` on `app` (capabilities is sent
// again when txPermitted changes), or none when it was never sent.
std::optional<QJsonValue> latestCapabilityIf(const QList<QByteArray>& received, const QString& name)
{
    std::optional<QJsonValue> value;
    for (const QJsonObject& caps : ofType(received, QStringLiteral("capabilities"))) {
        for (const QJsonValue& p : caps.value(QStringLiteral("properties")).toArray()) {
            if (p.toObject().value(QStringLiteral("name")).toString() == name) {
                value = p.toObject().value(QStringLiteral("value"));
            }
        }
    }
    return value;
}

bool txPermitted(const LoopbackTransport* app)
{
    const std::optional<QJsonValue> value = latestCapabilityIf(app->received(), QStringLiteral("txPermitted"));
    return value.has_value() && value->toBool();
}

// Fix wave C1: a voice key needs the device's microphone line, which a
// session-only test has no media for. Every device's line reads open and
// its buffer full at once. A test that builds a DaemonMediaController after
// this gets the real line instead.
void openFakeMicrophoneLines(Core& core)
{
    RemoteKeying* keying = core.server->remoteKeying();
    if (keying == nullptr) {
        return;
    }
    RemoteKeying::MicUplink uplink;
    uplink.carriesMic = [](const QByteArray&) { return true; };
    uplink.prime = [](const QByteArray&, std::function<void(bool)> done) { done(true); };
    uplink.endPriming = [](const QByteArray&) {};
    keying->setMicUplink(uplink);
}

// The Core allows remote transmit; MOX walks with no delays; the slice is
// on 20 m USB so the band plan admits a key.
void allowTransmit(Core& core)
{
    core.server->setRemoteTransmitAllowed(true);
    core.model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
    // No PC microphone in the test: the radio's own microphone carries the
    // audio, so the microphone-ready check has nothing to wait for.
    core.model->transmitModel().setMicSourceLocked(false);
    core.model->transmitModel().setMicSource(MicSource::Radio);
    if (SliceModel* slice = core.model->sliceById(0)) {
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000.0);
    }
    // Fix wave C1: every device's microphone line reads open.
    openFakeMicrophoneLines(core);
}

// A device's key, as a remote tx.key will send it (Task 35).
KeyerIdentity keyerFor(const Device& device)
{
    KeyerIdentity keyer;
    keyer.deviceId = device.key.fingerprint();
    keyer.source = PttMode::None;
    return keyer;
}

} // namespace
