// Read-only paired Core settings export over the real session envelopes.
#include "MultiDeviceHarness.h"

#include "core/security/ClientDeviceIdentity.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"

#include <vector>

namespace {
class BacklogTransport final : public LoopbackTransport {
public:
    explicit BacklogTransport() : LoopbackTransport(QStringLiteral("backlogged core")) {}
    qint64 backlogBytes() const override { return bytesQueued; }
    qint64 bytesQueued = 0;
};

SessionMessage answerFor(const LoopbackTransport* app, quint32 id)
{
    for (const QByteArray& wire : app->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::CommandResult && message.commandId == id) {
            return message;
        }
    }
    return {};
}

SessionMessage invoke(Core& core, LoopbackTransport* app, const QByteArray& verb,
                      const QList<MirrorUpdate>& args = {})
{
    const quint32 id = core.nextCommandId;
    core.invoke(app, verb, args);
    return answerFor(app, id);
}

QString stringValue(const SessionMessage& message, const QByteArray& name)
{
    for (const MirrorUpdate& field : message.updates) {
        if (field.name == name) return field.value.toString();
    }
    return {};
}

qint64 integerValue(const SessionMessage& message, const QByteArray& name)
{
    for (const MirrorUpdate& field : message.updates) {
        if (field.name == name) return field.value.toLongLong();
    }
    return -1;
}

QList<MirrorUpdate> idAndOffset(const QString& id, qint64 offset)
{
    return {utf8("transferId", id), int64("offset", offset)};
}

std::shared_ptr<const ClientDeviceIdentity> pairDesktop(Core& core, const QString& keyDir)
{
    auto key = std::make_shared<const ClientDeviceIdentity>(
        ClientDeviceIdentity::loadOrCreate(keyDir));
    if (!key->isValid()) return {};
    PairedDevice record;
    record.id = key->fingerprint();
    record.publicKeySpki = key->publicKeySpki();
    record.name = QStringLiteral("Desktop");
    record.kind = QStringLiteral("computer");
    return core.server->deviceStore()->add(record) ? key : nullptr;
}

LoopbackTransport* connectDesktop(Core& core, StationClient& client)
{
    auto* app = new LoopbackTransport(QStringLiteral("window"));
    auto* station = new LoopbackTransport(QStringLiteral("core"));
    app->setPeerCertificateSha256(core.certSha256());
    station->linkTo(app);
    client.startSession(app, {}, {}, core.server->stationIdentity().fingerprint());
    core.server->acceptTransport(station);
    return station;
}
} // namespace

class TstSettingsBackupExport : public QObject {
    Q_OBJECT
private slots:
    void pairedSnapshotIsStableAndChunked();
    void unpairedAndUnnegotiatedPeersCannotExport();
    void transferIsSessionBoundAndExpires();
    void fourPairedSessionsKeepSeparateSnapshots();
    void absoluteLifetimeEndsActiveTransfer();
    void malformedRequestsAndBacklogDoNotLeakSnapshot();
    void exportIdentifiersAreValidatedBeforeNarrowing();
    void beginRefusedByOnAirModelFlag();
    void pairedClientReportsOnlyCompleteValidatedXml();
    void malformedAndReorderedClientRepliesStayPrivate();
    void capabilityLossAndUnrelatedLargeFrame();
    void unrelatedCommandTextDoesNotBecomeAnExportRequest();
    void completionCallbackMayDestroyClient();
    void disconnectCallbackMayDestroyClient();
    void completionCallbackMayReconnectWithoutRetiringNewSession();
};

void TstSettingsBackupExport::pairedSnapshotIsStableAndChunked()
{
    Core core(true);
    Device device(QStringLiteral("Desktop"), QStringLiteral("computer"));
    core.pair(device);
    LoopbackTransport* app = core.signIn(device, {{"deviceAuth", 1}, {"settingsBackup", 1}});
    QVERIFY(admitted(app));
    QCOMPARE(capability(app->received(), QStringLiteral("settingsBackupVersion")),
             std::optional<qint64>(1));
    core.settings->setValue(QStringLiteral("LargeBackupValue"), QString(1100000, QLatin1Char('z')));
    const QByteArray expected = core.settings->exportLocalXml();
    QVERIFY(expected.size() > 1024 * 1024);

    const SessionMessage begin = invoke(core, app, "station.settingsExport.begin");
    QVERIFY(begin.accepted);
    QCOMPARE(begin.updates.size(), 3);
    QCOMPARE(integerValue(begin, "byteLength"), expected.size());
    QCOMPARE(stringValue(begin, "sha256").toLatin1(),
             QCryptographicHash::hash(expected, QCryptographicHash::Sha256).toHex());
    const QString id = stringValue(begin, "transferId");
    QCOMPARE(id.size(), 32);
    core.settings->setValue(QStringLiteral("LargeBackupValue"), QStringLiteral("changed"));

    QByteArray output;
    qint64 offset = 0;
    while (offset < expected.size()) {
        const SessionMessage read = invoke(core, app, "station.settingsExport.read",
                                           idAndOffset(id, offset));
        QVERIFY2(read.accepted, qPrintable(read.reason));
        QCOMPARE(read.updates.size(), 3);
        QCOMPARE(integerValue(read, "offset"), offset);
        const QByteArray part = QByteArray::fromBase64(stringValue(read, "data").toLatin1());
        QVERIFY(part.size() > 0 && part.size() <= 256 * 1024);
        output += part;
        offset += part.size();
    }
    QCOMPARE(output, expected);
    const SessionMessage retired = invoke(core, app, "station.settingsExport.read",
                                          idAndOffset(id, 0));
    QVERIFY(!retired.accepted);
}

void TstSettingsBackupExport::unpairedAndUnnegotiatedPeersCannotExport()
{
    Core core(true);
    Device device(QStringLiteral("Desktop"), QStringLiteral("computer"));
    core.pair(device);
    const QList<MirrorUpdate> none;
    LoopbackTransport* token = core.tokenSignIn({{"settingsBackup", 1}});
    QVERIFY(admitted(token));
    QVERIFY(!capability(token->received(), QStringLiteral("settingsBackupVersion")));
    QVERIFY(!invoke(core, token, "station.settingsExport.begin", none).accepted);

    LoopbackTransport* missing = core.signIn(device, {{"deviceAuth", 1}});
    QVERIFY(admitted(missing));
    QVERIFY(!capability(missing->received(), QStringLiteral("settingsBackupVersion")));
    QVERIFY(!invoke(core, missing, "station.settingsExport.begin").accepted);

    LoopbackTransport* old = core.signIn(device, {{"deviceAuth", 1}, {"settingsBackup", 1}},
                                        quint16(kRadioIdentitySessionProtocolMinor - 1));
    QVERIFY(admitted(old));
    QVERIFY(!invoke(core, old, "station.settingsExport.begin").accepted);
}

void TstSettingsBackupExport::transferIsSessionBoundAndExpires()
{
    Core core;
    Device a(QStringLiteral("A"), QStringLiteral("computer"));
    Device b(QStringLiteral("B"), QStringLiteral("computer"));
    core.pair(a);
    core.pair(b);
    qint64 now = 0;
    core.server->setSettingsExportClockForTest([&now]() { return now; });
    const QHash<QByteArray, int> features{{"deviceAuth", 1}, {"settingsBackup", 1}};
    LoopbackTransport* appA = core.signIn(a, features);
    LoopbackTransport* appB = core.signIn(b, features);
    QVERIFY(admitted(appA) && admitted(appB));
    const SessionMessage begin = invoke(core, appA, "station.settingsExport.begin");
    QVERIFY(begin.accepted);
    const QString id = stringValue(begin, "transferId");
    QVERIFY(!invoke(core, appB, "station.settingsExport.read", idAndOffset(id, 0)).accepted);
    QVERIFY(!invoke(core, appA, "station.settingsExport.read", idAndOffset(id, 1)).accepted);
    QVERIFY(!invoke(core, appA, "station.settingsExport.begin").accepted);
    now = 30000;
    core.server->expireSettingsExportsForTest();
    QVERIFY(!invoke(core, appA, "station.settingsExport.read", idAndOffset(id, 0)).accepted);
    const SessionMessage fresh = invoke(core, appA, "station.settingsExport.begin");
    QVERIFY(fresh.accepted);
    const QString freshId = stringValue(fresh, "transferId");
    QVERIFY(invoke(core, appA, "station.settingsExport.cancel", {utf8("transferId", freshId)}).accepted);
    QVERIFY(!invoke(core, appA, "station.settingsExport.read", idAndOffset(freshId, 0)).accepted);
    const SessionMessage replacement = invoke(core, appA, "station.settingsExport.begin");
    QVERIFY(replacement.accepted);
    LoopbackTransport* newerA = core.signIn(a, features);
    QVERIFY(admitted(newerA));
    QVERIFY(!invoke(core, newerA, "station.settingsExport.read",
                    idAndOffset(stringValue(replacement, "transferId"), 0)).accepted);
    const SessionMessage beforeRevoke = invoke(core, newerA, "station.settingsExport.begin");
    QVERIFY(beforeRevoke.accepted);
    QVERIFY(core.server->deviceStore()->remove(a.key.fingerprint()));
    QTRY_VERIFY(!newerA->isOpen());
    QVERIFY(invoke(core, appB, "station.settingsExport.begin").accepted);
}

void TstSettingsBackupExport::fourPairedSessionsKeepSeparateSnapshots()
{
    Core core;
    const QHash<QByteArray, int> features{{"deviceAuth", 1}, {"settingsBackup", 1}};
    std::vector<std::unique_ptr<Device>> devices;
    QSet<QString> ids;
    QList<LoopbackTransport*> apps;
    for (int i = 0; i < 4; ++i) {
        devices.push_back(std::make_unique<Device>(QStringLiteral("Device %1").arg(i),
                                                    QStringLiteral("computer")));
        core.pair(*devices.back());
        LoopbackTransport* app = core.signIn(*devices.back(), features);
        QVERIFY(admitted(app));
        apps.append(app);
        const SessionMessage result = invoke(core, app, "station.settingsExport.begin");
        QVERIFY(result.accepted);
        ids.insert(stringValue(result, "transferId"));
    }
    QCOMPARE(ids.size(), 4);
    for (LoopbackTransport* app : apps) {
        QVERIFY(!invoke(core, app, "station.settingsExport.begin").accepted);
    }
}

void TstSettingsBackupExport::absoluteLifetimeEndsActiveTransfer()
{
    Core core;
    Device device(QStringLiteral("Desktop"), QStringLiteral("computer"));
    core.pair(device);
    qint64 now = 0;
    core.server->setSettingsExportClockForTest([&now]() { return now; });
    LoopbackTransport* app = core.signIn(device, {{"deviceAuth", 1}, {"settingsBackup", 1}});
    QVERIFY(admitted(app));
    core.settings->setValue(QStringLiteral("LongValue"), QString(300000, QLatin1Char('x')));
    const SessionMessage begin = invoke(core, app, "station.settingsExport.begin");
    QVERIFY(begin.accepted);
    const QString id = stringValue(begin, "transferId");
    now = 29000;
    const SessionMessage first = invoke(core, app, "station.settingsExport.read", idAndOffset(id, 0));
    QVERIFY(first.accepted);
    const qint64 next = QByteArray::fromBase64(stringValue(first, "data").toLatin1()).size();
    QCOMPARE(next, 256LL * 1024);
    now = 120000;
    core.server->expireSettingsExportsForTest();
    QVERIFY(!invoke(core, app, "station.settingsExport.read", idAndOffset(id, next)).accepted);
}

void TstSettingsBackupExport::malformedRequestsAndBacklogDoNotLeakSnapshot()
{
    Core core;
    Device device(QStringLiteral("Desktop"), QStringLiteral("computer"));
    core.pair(device);
    auto* app = new LoopbackTransport(QStringLiteral("window"));
    auto* station = new BacklogTransport;
    station->linkTo(app);
    core.clients.append(app);
    core.server->acceptTransport(station);
    QTRY_VERIFY(!app->received().isEmpty());
    Core::send(app, core.deviceAuth(app, device), kSessionProtocolMinor,
               {{"deviceAuth", 1}, {"settingsBackup", 1}});
    QVERIFY(admitted(app));

    station->bytesQueued = 1024 * 1024;
    QVERIFY(!invoke(core, app, "station.settingsExport.begin").accepted);
    station->bytesQueued = 0;
    const SessionMessage begin = invoke(core, app, "station.settingsExport.begin");
    QVERIFY(begin.accepted);
    const QString id = stringValue(begin, "transferId");

    // A generic SessionMessages decode narrows 0.5 to ordinal zero; the
    // export's raw-envelope check must reject it before that narrowing.
    const quint32 fractionalId = core.nextCommandId++;
    QJsonObject malformed = QJsonDocument::fromJson(SessionMessages::encode(
        SessionMessages::commandInvoke("station.settingsExport.read", fractionalId,
                                       idAndOffset(id, 0)))).object();
    QJsonArray args = malformed.value(QStringLiteral("args")).toArray();
    QJsonObject first = args.at(0).toObject();
    first.insert(QStringLiteral("ordinal"), 0.5);
    args.replace(0, first);
    malformed.insert(QStringLiteral("args"), args);
    app->sendText(QJsonDocument(malformed).toJson(QJsonDocument::Compact));
    QTRY_VERIFY(answerFor(app, fractionalId).kind == SessionMessageKind::CommandResult);
    QVERIFY(!answerFor(app, fractionalId).accepted);

    // Invalid requests do not consume the next offset or extend expiry.
    QVERIFY(!invoke(core, app, "station.settingsExport.read", idAndOffset(id, -1)).accepted);
    station->bytesQueued = 1024 * 1024;
    QVERIFY(!invoke(core, app, "station.settingsExport.read", idAndOffset(id, 0)).accepted);
    station->bytesQueued = 0;
    QVERIFY(!invoke(core, app, "station.settingsExport.read", idAndOffset(id, 0)).accepted);
    QVERIFY(app->isOpen());
}

void TstSettingsBackupExport::exportIdentifiersAreValidatedBeforeNarrowing()
{
    for (bool result : {false, true}) {
        const SessionMessage valid = result
            ? SessionMessages::commandResult("station.settingsExport.read", 1, false, {}, {})
            : SessionMessages::commandInvoke("station.settingsExport.read", 1, {});
        QJsonObject envelope = QJsonDocument::fromJson(SessionMessages::encode(valid)).object();
        for (const QJsonValue& invalid : QList<QJsonValue>{-1.0, 0.0, 0.5, 1.5,
                 4294967296.0, 1.0e100, QStringLiteral("1"), QJsonValue(QJsonValue::Null)}) {
            envelope.insert(QStringLiteral("id"), invalid);
            SessionMessage decoded;
            QVERIFY(!SessionMessages::decode(QJsonDocument(envelope).toJson(QJsonDocument::Compact),
                                              &decoded));
        }
        envelope.insert(QStringLiteral("id"), 4294967295.0);
        SessionMessage decoded;
        QVERIFY(SessionMessages::decode(QJsonDocument(envelope).toJson(QJsonDocument::Compact),
                                         &decoded));
        QCOMPARE(decoded.commandId, quint32(0xffffffff));
    }
}

void TstSettingsBackupExport::beginRefusedByOnAirModelFlag()
{
    Core core;
    Device device(QStringLiteral("Desktop"), QStringLiteral("computer"));
    core.pair(device);
    LoopbackTransport* app = core.signIn(device, {{"deviceAuth", 1}, {"settingsBackup", 1}});
    QVERIFY(admitted(app));
    // Pure model latch only: the fixture has no TX channel or RF connection.
    core.model->transmitModel().setMox(true);
    QString reason;
    QVERIFY(core.model->stationOnAirRefusal(&reason));
    const SessionMessage refused = invoke(core, app, "station.settingsExport.begin");
    QVERIFY(!refused.accepted);
    QCOMPARE(refused.reason, reason);
    core.model->transmitModel().setMox(false);
    QVERIFY(invoke(core, app, "station.settingsExport.begin").accepted);
}

void TstSettingsBackupExport::pairedClientReportsOnlyCompleteValidatedXml()
{
    Core core;
    QTemporaryDir keyDir;
    auto key = pairDesktop(core, keyDir.path());
    QVERIFY(key);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    client.setDeviceIdentity(key, QStringLiteral("Desktop"));
    QSignalSpy completed(&remote, &RadioModel::stationSettingsBackupExportFinished);
    auto* station = connectDesktop(core, client);
    QTRY_VERIFY(client.settingsBackupExportAvailable());
    core.settings->setValue(QStringLiteral("LargeBackupValue"), QString(1100000, QLatin1Char('q')));
    const QByteArray expected = core.settings->exportLocalXml();
    const IStationLink::CommandOutcome outcome = client.requestSettingsBackupExport();
    QVERIFY(outcome.sent);
    QCOMPARE(completed.size(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 10000);
    QCOMPARE(completed.at(0).at(0).toUInt(), outcome.commandId);
    QCOMPARE(completed.at(0).at(1).toBool(), true);
    QCOMPARE(completed.at(0).at(3).toByteArray(), expected);
    const IStationLink::CommandOutcome again = client.requestSettingsBackupExport();
    QVERIFY(again.sent);
    // The first page can close after a successor page has begun exporting.
    // Its retired operation must never cancel the successor's request.
    client.cancelSettingsBackupExport(outcome.commandId);
    QCOMPARE(completed.size(), 1);
    client.cancelSettingsBackupExport(again.commandId);
    QTRY_COMPARE(completed.size(), 2);
    QCOMPARE(completed.at(1).at(0).toUInt(), again.commandId);
    QCOMPARE(completed.at(1).at(1).toBool(), false);
    QVERIFY(completed.at(1).at(3).toByteArray().isEmpty());
    QTRY_VERIFY([station]() {
        for (const QByteArray& wire : station->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::CommandInvoke
                && message.commandVerb == "station.settingsExport.cancel") return true;
        }
        return false;
    }());
    QCOMPARE(completed.size(), 2);
    client.disconnectFromStation(QStringLiteral("done"));
}

void TstSettingsBackupExport::malformedAndReorderedClientRepliesStayPrivate()
{
    Core core;
    QTemporaryDir keyDir;
    auto key = pairDesktop(core, keyDir.path());
    QVERIFY(key);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    client.setDeviceIdentity(key, QStringLiteral("Desktop"));
    QSignalSpy completed(&remote, &RadioModel::stationSettingsBackupExportFinished);
    auto* station = connectDesktop(core, client);
    QTRY_VERIFY(client.settingsBackupExportAvailable());

    // The forged begin answer is delivered before the real queued answer.
    const auto first = client.requestSettingsBackupExport();
    QVERIFY(first.sent);
    station->sendText(SessionMessages::encode(SessionMessages::commandResult(
        "station.settingsExport.begin", first.commandId, true, {}, {},
        {utf8("transferId", QStringLiteral("not-an-id")),
         int64("byteLength", 100), utf8("sha256", QString(64, QLatin1Char('0')))})));
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.at(0).at(1).toBool(), false);
    QVERIFY(completed.at(0).at(3).toByteArray().isEmpty());

    // The server's first transfer expires/cancels when the session closes;
    // a fresh paired session supplies the next independent read exchange.
    client.disconnectFromStation(QStringLiteral("new test session"));
    auto* secondStation = connectDesktop(core, client);
    QTRY_VERIFY(client.settingsBackupExportAvailable());
    bool suppressRealRead = false;
    QObject::connect(client.transport(), &SessionTransport::textReceived, &client,
                     [secondStation, &suppressRealRead](const QByteArray& wire) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::CommandResult
            && message.commandVerb == "station.settingsExport.begin" && message.accepted) {
            secondStation->setDropsOutgoing(true);
            suppressRealRead = true;
        }
    });
    QObject::connect(secondStation, &SessionTransport::textReceived, &client,
                     [secondStation, &suppressRealRead](const QByteArray& wire) {
        SessionMessage request;
        if (!suppressRealRead || !SessionMessages::decode(wire, &request)
            || request.kind != SessionMessageKind::CommandInvoke
            || request.commandVerb != "station.settingsExport.read") return;
        secondStation->setDropsOutgoing(false);
        suppressRealRead = false;
        const QString id = request.arguments.at(0).value.toString();
        secondStation->sendText(SessionMessages::encode(SessionMessages::commandResult(
            "station.settingsExport.read", request.commandId, true, {}, {},
            {utf8("transferId", id), int64("offset", 1),
             utf8("data", QStringLiteral("eA=="))})));
    });
    const auto second = client.requestSettingsBackupExport();
    QVERIFY(second.sent);
    QTRY_COMPARE(completed.size(), 2);
    QCOMPARE(completed.at(1).at(0).toUInt(), second.commandId);
    QCOMPARE(completed.at(1).at(1).toBool(), false);
    QVERIFY(completed.at(1).at(3).toByteArray().isEmpty());
    client.disconnectFromStation(QStringLiteral("done"));
}

void TstSettingsBackupExport::completionCallbackMayDestroyClient()
{
    Core core;
    QTemporaryDir keyDir;
    auto key = pairDesktop(core, keyDir.path());
    QVERIFY(key);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    auto client = std::make_unique<StationClient>(&remote, &proxy);
    client->setDeviceIdentity(key, QStringLiteral("Desktop"));
    LoopbackTransport* station = connectDesktop(core, *client);
    QTRY_VERIFY(client->settingsBackupExportAvailable());
    bool notified = false;
    QObject::connect(&remote, &RadioModel::stationSettingsBackupExportFinished, &remote,
                     [&](quint32, bool, const QString&, const QByteArray&) {
        notified = true;
        client.reset();
    });
    QVERIFY(client->requestSettingsBackupExport().sent);
    station->closeLink(QStringLiteral("link ended"));
    QTRY_VERIFY(notified);
    QVERIFY(!client);
}

void TstSettingsBackupExport::disconnectCallbackMayDestroyClient()
{
    Core core;
    QTemporaryDir keyDir;
    auto key = pairDesktop(core, keyDir.path());
    QVERIFY(key);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    auto client = std::make_unique<StationClient>(&remote, &proxy);
    client->setDeviceIdentity(key, QStringLiteral("Desktop"));
    connectDesktop(core, *client);
    QTRY_VERIFY(client->settingsBackupExportAvailable());
    bool notified = false;
    QObject::connect(&remote, &RadioModel::stationSettingsBackupExportFinished, &remote,
                     [&](quint32, bool, const QString&, const QByteArray&) {
        notified = true;
        client.reset();
    });
    QVERIFY(client->requestSettingsBackupExport().sent);
    client->disconnectFromStation(QStringLiteral("operator disconnected"));
    QVERIFY(notified);
    QVERIFY(!client);
}

void TstSettingsBackupExport::capabilityLossAndUnrelatedLargeFrame()
{
    Core core;
    QTemporaryDir keyDir;
    auto key = pairDesktop(core, keyDir.path());
    QVERIFY(key);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    client.setDeviceIdentity(key, QStringLiteral("Desktop"));
    LoopbackTransport* station = connectDesktop(core, client);
    QTRY_VERIFY(client.settingsBackupExportAvailable());
    QSignalSpy completed(&remote, &RadioModel::stationSettingsBackupExportFinished);
    core.settings->setValue(QStringLiteral("LargeBackupValue"), QString(1100000, QLatin1Char('a')));
    QVERIFY(client.requestSettingsBackupExport().sent);
    QList<MirrorUpdate> descriptor = client.capabilities().toUpdates();
    descriptor.append(utf8("unrelatedPadding", QString(1100000, QLatin1Char('p'))));
    const QByteArray unrelated = SessionMessages::encode(SessionMessages::capabilities(descriptor));
    QVERIFY(unrelated.size() > 1024 * 1024);
    station->sendText(unrelated);
    // Mentioning the export verb in an unrelated result's free-text reason
    // must not change its message identity or cancel the pending export.
    const QByteArray unrelatedResult = SessionMessages::encode(SessionMessages::commandResult(
        "unrelated.operation", 0x7fffffff, false,
        QStringLiteral("command.result station.settingsExport.read ")
            + QString(1100000, QLatin1Char('r')), {}));
    QVERIFY(unrelatedResult.size() > 1024 * 1024);
    station->sendText(unrelatedResult);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 10000);
    QVERIFY(completed.at(0).at(1).toBool());

    // Revoking the feature during a second operation retires that operation
    // without publishing any part of the XML.
    QVERIFY(client.requestSettingsBackupExport().sent);
    StationCapabilities withoutExport = client.capabilities();
    withoutExport.settingsBackupVersion = 0;
    station->sendText(SessionMessages::encode(
        SessionMessages::capabilities(withoutExport.toUpdates())));
    QTRY_COMPARE(completed.size(), 2);
    QVERIFY(!completed.at(1).at(1).toBool());
    QVERIFY(completed.at(1).at(3).toByteArray().isEmpty());
    client.disconnectFromStation(QStringLiteral("done"));
}

void TstSettingsBackupExport::unrelatedCommandTextDoesNotBecomeAnExportRequest()
{
    Core core(true);
    Device device(QStringLiteral("Desktop"), QStringLiteral("computer"));
    core.pair(device);
    LoopbackTransport* app = core.signIn(device, {{"deviceAuth", 1}, {"settingsBackup", 1}});
    QVERIFY(admitted(app));
    const SessionMessage answer = invoke(core, app, "unrelated.operation",
        {utf8("text", QStringLiteral("station.settingsExport.begin ")
                         + QString(1100000, QLatin1Char('x')))});
    QVERIFY(app->isOpen());
    QCOMPARE(answer.kind, SessionMessageKind::CommandResult);
    QVERIFY(!answer.accepted);
    // The unknown command is refused normally; the authenticated session is
    // still usable for a real export afterwards.
    QVERIFY(invoke(core, app, "station.settingsExport.begin").accepted);
}

void TstSettingsBackupExport::completionCallbackMayReconnectWithoutRetiringNewSession()
{
    Core core;
    QTemporaryDir keyDir;
    auto key = pairDesktop(core, keyDir.path());
    QVERIFY(key);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    client.setDeviceIdentity(key, QStringLiteral("Desktop"));
    LoopbackTransport* oldStation = connectDesktop(core, client);
    QTRY_VERIFY(client.settingsBackupExportAvailable());
    const quint32 oldEpoch = client.sessionEpoch();
    QVERIFY(!client.mirroredObjectKeys().isEmpty());
    bool oldMirrorsRetiredBeforeCallback = false;
    bool oldSettingsRetiredBeforeCallback = false;
    LoopbackTransport* replacement = nullptr;
    QObject::connect(&remote, &RadioModel::stationSettingsBackupExportFinished, &remote,
                     [&](quint32, bool, const QString&, const QByteArray&) {
        oldMirrorsRetiredBeforeCallback = client.mirroredObjectKeys().isEmpty();
        oldSettingsRetiredBeforeCallback = !proxy.ready();
        replacement = connectDesktop(core, client);
    });
    QVERIFY(client.requestSettingsBackupExport().sent);
    oldStation->closeLink(QStringLiteral("link ended"));
    QTRY_VERIFY(replacement != nullptr);
    QVERIFY(oldMirrorsRetiredBeforeCallback);
    QVERIFY(oldSettingsRetiredBeforeCallback);
    QTRY_VERIFY(client.settingsBackupExportAvailable());
    QVERIFY(client.sessionEpoch() > oldEpoch);
    client.disconnectFromStation(QStringLiteral("done"));
}

QTEST_MAIN(TstSettingsBackupExport)
#include "tst_settings_backup_export.moc"
