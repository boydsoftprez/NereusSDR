// no-port-check: NereusSDR-original. CAT setup from a connected desktop: the
// Core's `stationCat` object, its four commands and the `catLog` stream.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_station_cat.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// The Core publishes its CAT (StationCatModel, filled from CatService) as
// the read-only `stationCat` object to a peer that declared stationCat 1,
// takes setStationCatChannel, setStationCatGlobal, testStationCatCommand
// and refreshStationCatDevices from it, resolves a binding's slice ids to
// the Core's live incarnations, and sends its CAT log as the `catLog`
// record stream. Loopback only; no radio.
//
// The design: docs/architecture/2026-10-07-remote-cat-setup-plan.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created (CAT setup from a connected
//                                    desktop, stationCatVersion 1).
//                                    AI tooling: Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"
#include "SessionWait.h"

#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include <memory>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "core/cat/CatService.h"
#include "core/cat/StationCatController.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"
#include "models/StationCatModel.h"

using namespace NereusSDR;

namespace {

const QString kChannelRefused = QStringLiteral(
    "Configuration refused: check address, port, format and exclusive device assignment.");
const QString kGlobalRefused = QStringLiteral(
    "Configuration refused: check PTT source, sampled inputs and device assignment.");
const QString kNotDeclared = QStringLiteral("This Core cannot set up its CAT from here.");
const QString kOlderApp = QStringLiteral("Update this app to set up CAT on this Core.");

const QHash<QByteArray, int> kStationCat{{QByteArrayLiteral("stationCat"), 1}};

quint16 freePort()
{
    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost, 0)) { return 0; }
    const quint16 port = reservation.serverPort();
    reservation.close();
    return port;
}

QString text(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

MirrorUpdate intArg(const char* name, qint64 value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Int64, QVariant(value)};
}

MirrorUpdate textArg(const char* name, const QString& value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Utf8, QVariant(value)};
}

// One command through the Core's dispatcher, answered at once.
SessionMessage run(SessionCommandDispatcher& dispatcher, const QByteArray& verb,
                   const QList<MirrorUpdate>& arguments)
{
    static quint32 id = 0;
    QSignalSpy results(&dispatcher, &SessionCommandDispatcher::commandResultReady);
    dispatcher.dispatch(SessionMessages::commandInvoke(verb, ++id, arguments));
    return results.isEmpty() ? SessionMessage{}
                             : qvariant_cast<SessionMessage>(results.last().first());
}

QJsonObject channelConfig(const StationCatModel& model, int channel)
{
    return model.channelObject(channel).value(QStringLiteral("config")).toObject();
}

QJsonObject channelStatus(const StationCatModel& model, int channel)
{
    return model.channelObject(channel).value(QStringLiteral("status")).toObject();
}

QJsonObject globalConfig(const StationCatModel& model)
{
    return model.globalObject().value(QStringLiteral("config")).toObject();
}

// The latest `stationCat` property `name` an app saw, parsed.
QJsonObject seen(const LoopbackTransport* app, const QString& name)
{
    return StationCatModel::fromText(latest(app->received(), QStringLiteral("stationCat"), name)
                                         .toString());
}

// The `catLog` records an app was sent.
QList<QJsonObject> catLogRecords(const LoopbackTransport* app)
{
    QList<QJsonObject> records;
    for (const QJsonObject& batch : ofType(app->received(), QStringLiteral("record.batch"))) {
        if (batch.value(QStringLiteral("stream")).toString() != QStringLiteral("catLog")) {
            continue;
        }
        for (const QJsonValue& upsert : batch.value(QStringLiteral("upserts")).toArray()) {
            records.append(upsert.toObject().value(QStringLiteral("fields")).toObject());
        }
    }
    return records;
}

bool sawCatLog(const LoopbackTransport* app)
{
    for (const QJsonObject& batch : ofType(app->received(), QStringLiteral("record.batch"))) {
        if (batch.value(QStringLiteral("stream")).toString() == QStringLiteral("catLog")) {
            return true;
        }
    }
    return false;
}

} // namespace

class TstStationCat : public QObject {
    Q_OBJECT

private slots:
    void init() { clearCat(); }
    void cleanup() { clearCat(); }

    // The published object follows CatService: a channel's settings, the
    // global settings, the live state and counts, PTT and AI.
    void publishedObjectFollowsCatService()
    {
        RadioModel model;
        model.addSlice();
        StationCatModel* station = model.stationCatModel();
        QVERIFY(station != nullptr);
        QVERIFY(model.stationCatController() != nullptr);
        CatService& service = *model.catService();

        // Before any change: the four channels, the global settings and
        // what this computer offers are already there.
        for (int channel = 1; channel <= StationCatModel::kChannels; ++channel) {
            QVERIFY(!station->channelObject(channel).isEmpty());
        }
        QCOMPARE(globalConfig(*station).value(QStringLiteral("rigIdentity")).toString(),
                 service.globalConfig().rigIdentity);
        const QJsonObject platform = station->platformObject();
        QVERIFY(platform.value(QStringLiteral("serial")).isBool());
        QVERIFY(platform.value(QStringLiteral("pty")).isBool());
        QVERIFY(platform.value(QStringLiteral("markSpaceParity")).isBool());
        QVERIFY(platform.value(QStringLiteral("oneAndHalfStop")).isBool());
        QVERIFY(platform.value(QStringLiteral("serialDevices")).isArray());

        // A channel's settings.
        const quint16 port = freePort();
        CatEndpointConfig config;
        config.tcpEnabled = true;
        config.tcpPort = port;
        config.binding.primarySliceId = 0;
        QVERIFY(service.reconfigureChannel(1, config));
        QCOMPARE(channelConfig(*station, 1).value(QStringLiteral("tcpPort")).toInt(), int(port));
        QVERIFY(channelConfig(*station, 1).value(QStringLiteral("tcpEnabled")).toBool());
        QCOMPARE(channelConfig(*station, 1).value(QStringLiteral("primarySliceId")).toInt(), 0);
        QCOMPARE(channelConfig(*station, 1).value(QStringLiteral("secondarySliceId")).toInt(), -1);
        QVERIFY(station->channelObject(1).value(QStringLiteral("primaryValid")).toBool());
        QVERIFY(station->channelObject(1).value(QStringLiteral("secondaryValid")).toBool());

        // The global settings.
        CatGlobalConfig global = service.globalConfig();
        global.rigIdentity = QStringLiteral("TS-480");
        global.rttyDiguHz = 1500;
        QVERIFY(service.reconfigureGlobal(global));
        QCOMPARE(globalConfig(*station).value(QStringLiteral("rigIdentity")).toString(),
                 QStringLiteral("TS-480"));
        QCOMPARE(globalConfig(*station).value(QStringLiteral("rttyDiguHz")).toInt(), 1500);

        // Started: the state, the listener and its address, then a client.
        const QString pttBefore = station->globalObject().value(QStringLiteral("pttState")).toString();
        QCOMPARE(pttBefore, service.pttState());
        service.startConfigured();
        QVERIFY(service.isListening(1));
        QCOMPARE(channelStatus(*station, 1).value(QStringLiteral("state")).toString(),
                 service.channelState(1));
        QCOMPARE(channelStatus(*station, 1).value(QStringLiteral("tcp")).toString(),
                 service.transportState(1, CatTransportKind::Tcp));
        QCOMPARE(channelStatus(*station, 1).value(QStringLiteral("tcpBoundPort")).toInt(),
                 int(service.boundPort(1)));
        QCOMPARE(channelStatus(*station, 1).value(QStringLiteral("tcpBoundAddress")).toString(),
                 QStringLiteral("127.0.0.1"));
        QCOMPARE(channelStatus(*station, 1).value(QStringLiteral("tcpClients")).toInt(), 0);
        // PTT follows CatService once started (off here: no PTT source).
        QCOMPARE(station->globalObject().value(QStringLiteral("pttState")).toString(),
                 service.pttState());
        QVERIFY(service.pttState() != pttBefore);
        {
            QTcpSocket client;
            client.connectToHost(QHostAddress::LocalHost, service.boundPort(1));
            NEREUS_TRY_VERIFY(
                channelStatus(*station, 1).value(QStringLiteral("tcpClients")).toInt() == 1);
            QCOMPARE(service.clientCount(1), 1);
            client.disconnectFromHost();
        }
        NEREUS_TRY_VERIFY(
            channelStatus(*station, 1).value(QStringLiteral("tcpClients")).toInt() == 0);

        // AI for this run.
        QVERIFY(!station->globalObject().value(QStringLiteral("aiActive")).toBool());
        service.setAutoInformation(true);
        QVERIFY(station->globalObject().value(QStringLiteral("aiActive")).toBool());
        service.setAutoInformation(false);
        QVERIFY(!station->globalObject().value(QStringLiteral("aiActive")).toBool());
        service.stopAll();

        // Only the Core runs the publisher; a window's copy only holds
        // what arrives, and nobody writes it.
        RadioModel window(RadioModel::Role::Remote);
        QVERIFY(window.stationCatModel() != nullptr);
        QVERIFY(window.stationCatController() == nullptr);
        QVERIFY(OperatorWording::isPlain(StationCatModel::readOnlyReason()));
    }

    // Each command applies through CatService, and each refuses in plain
    // words: the local pages' own text for a refused channel or global
    // change.
    void eachCommandAppliesAndRefuses()
    {
        RadioModel model;
        model.addSlice();
        StationCatModel* station = model.stationCatModel();
        CatService& service = *model.catService();
        SessionCommandDispatcher dispatcher(&model);

        // setStationCatChannel.
        const quint16 port = freePort();
        QJsonObject channel = StationCatModel::channelConfigToJson(service.channelConfig(2));
        channel.insert(QStringLiteral("tcpEnabled"), true);
        channel.insert(QStringLiteral("tcpPort"), int(port));
        SessionMessage result = run(dispatcher, "setStationCatChannel",
                                    {intArg("channel", 2), textArg("config", text(channel))});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(service.channelConfig(2).tcpPort, int(port));
        QVERIFY(service.channelConfig(2).tcpEnabled);
        QCOMPARE(channelConfig(*station, 2).value(QStringLiteral("tcpPort")).toInt(), int(port));
        // Fields left out keep the channel's values.
        result = run(dispatcher, "setStationCatChannel",
                     {intArg("channel", 2),
                      textArg("config", text({{QStringLiteral("serialBaud"), 9600}}))});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(service.channelConfig(2).serialBaud, 9600);
        QCOMPARE(service.channelConfig(2).tcpPort, int(port));
        // Refused by CatService: the page's words, nothing kept.
        channel.insert(QStringLiteral("tcpPort"), 0);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("Invalid CAT listener address")));
        result = run(dispatcher, "setStationCatChannel",
                     {intArg("channel", 2), textArg("config", text(channel))});
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, kChannelRefused);
        QCOMPARE(service.channelConfig(2).tcpPort, int(port));
        // Not understood: a channel the Core does not have, a config that
        // is not an object or has a field of the wrong kind.
        for (const QList<MirrorUpdate>& arguments :
             {QList<MirrorUpdate>{intArg("channel", 5), textArg("config", text(channel))},
              QList<MirrorUpdate>{intArg("channel", 2), textArg("config", QStringLiteral("[]"))},
              QList<MirrorUpdate>{intArg("channel", 2),
                                  textArg("config", text({{QStringLiteral("tcpPort"),
                                                           QStringLiteral("80")}}))},
              QList<MirrorUpdate>{intArg("channel", 2)}}) {
            result = run(dispatcher, "setStationCatChannel", arguments);
            QVERIFY(!result.accepted);
            QVERIFY2(OperatorWording::isPlain(result.reason), qPrintable(result.reason));
        }
        QCOMPARE(service.channelConfig(2).tcpPort, int(port));

        // setStationCatGlobal.
        result = run(dispatcher, "setStationCatGlobal",
                     {textArg("config", text({{QStringLiteral("rigIdentity"),
                                               QStringLiteral("TS-50S")}}))});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(service.globalConfig().rigIdentity, QStringLiteral("TS-50S"));
        QCOMPARE(globalConfig(*station).value(QStringLiteral("rigIdentity")).toString(),
                 QStringLiteral("TS-50S"));
        result = run(dispatcher, "setStationCatGlobal",
                     {textArg("config", text({{QStringLiteral("pttEnabled"), true},
                                              {QStringLiteral("pttDeviceSource"),
                                               QStringLiteral("None")}}))});
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, kGlobalRefused);
        QVERIFY(!service.globalConfig().pttEnabled);
        result = run(dispatcher, "setStationCatGlobal",
                     {textArg("config", text({{QStringLiteral("aiEnabled"), 1}}))});
        QVERIFY(!result.accepted);
        QVERIFY2(OperatorWording::isPlain(result.reason), qPrintable(result.reason));

        // testStationCatCommand: the reply lands in lastTest.
        result = run(dispatcher, "testStationCatCommand",
                     {intArg("requestId", 7), intArg("channel", 2),
                      textArg("command", QStringLiteral("ID;"))});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QJsonObject lastTest = station->lastTestObject();
        QCOMPARE(lastTest.value(QStringLiteral("requestId")).toInteger(), 7);
        QCOMPARE(lastTest.value(QStringLiteral("channel")).toInt(), 2);
        QCOMPARE(lastTest.value(QStringLiteral("command")).toString(), QStringLiteral("ID;"));
        // TS-50S, set above, answers with its own identity.
        QCOMPARE(lastTest.value(QStringLiteral("reply")).toString(), QStringLiteral("ID013;"));
        QVERIFY(lastTest.value(QStringLiteral("accepted")).toBool());
        // Calibration is refused as the local tester refuses it.
        result = run(dispatcher, "testStationCatCommand",
                     {intArg("requestId", 8), intArg("channel", 2),
                      textArg("command", QStringLiteral("ZZLI1;"))});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        lastTest = station->lastTestObject();
        QCOMPARE(lastTest.value(QStringLiteral("requestId")).toInteger(), 8);
        QCOMPARE(lastTest.value(QStringLiteral("reply")).toString(), QStringLiteral("?;"));
        QVERIFY(!lastTest.value(QStringLiteral("accepted")).toBool());
        for (const QList<MirrorUpdate>& arguments :
             {QList<MirrorUpdate>{intArg("requestId", 9), intArg("channel", 0),
                                  textArg("command", QStringLiteral("ID;"))},
              QList<MirrorUpdate>{intArg("requestId", 9), textArg("command", QStringLiteral("ID;"))},
              QList<MirrorUpdate>{textArg("requestId", QStringLiteral("9")), intArg("channel", 1),
                                  textArg("command", QStringLiteral("ID;"))}}) {
            result = run(dispatcher, "testStationCatCommand", arguments);
            QVERIFY(!result.accepted);
            QVERIFY2(OperatorWording::isPlain(result.reason), qPrintable(result.reason));
        }
        QCOMPARE(station->lastTestObject().value(QStringLiteral("requestId")).toInteger(), 8);

        // refreshStationCatDevices.
        result = run(dispatcher, "refreshStationCatDevices", {});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QVERIFY(station->platformObject().value(QStringLiteral("serialDevices")).isArray());
        result = run(dispatcher, "refreshStationCatDevices", {intArg("channel", 1)});
        QVERIFY(!result.accepted);
        QVERIFY2(OperatorWording::isPlain(result.reason), qPrintable(result.reason));

        // A window's model has no CAT to set up.
        RadioModel window(RadioModel::Role::Remote);
        SessionCommandDispatcher remote(&window);
        result = run(remote, "refreshStationCatDevices", {});
        QVERIFY(!result.accepted);
        QVERIFY2(OperatorWording::isPlain(result.reason), qPrintable(result.reason));

        QVERIFY(OperatorWording::isPlain(kChannelRefused));
        QVERIFY(OperatorWording::isPlain(kGlobalRefused));
    }

    // A binding arrives as slice ids: a new slice binds to its live
    // incarnation on the Core; a slice closed later reads invalid, and an
    // edit that keeps the id does not silently bind the channel again.
    void bindingResolvesToTheLiveIncarnation()
    {
        RadioModel model;
        model.addSlice();
        model.addSlice();
        StationCatModel* station = model.stationCatModel();
        CatService& service = *model.catService();
        SliceOwnership* ownership = model.sliceOwnership();
        SessionCommandDispatcher dispatcher(&model);
        QVERIFY(ownership->incarnation(1) != 0);

        QJsonObject channel = StationCatModel::channelConfigToJson(service.channelConfig(1));
        channel.insert(QStringLiteral("primarySliceId"), 1);
        channel.insert(QStringLiteral("secondarySliceId"), 0);
        SessionMessage result = run(dispatcher, "setStationCatChannel",
                                    {intArg("channel", 1), textArg("config", text(channel))});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(service.channelConfig(1).binding.primarySliceId, 1);
        QCOMPARE(service.channelConfig(1).binding.primaryIncarnation, ownership->incarnation(1));
        QCOMPARE(service.channelConfig(1).binding.secondarySliceId, std::optional<int>(0));
        QCOMPARE(service.channelConfig(1).binding.secondaryIncarnation,
                 std::optional<quint64>(ownership->incarnation(0)));
        QVERIFY(station->channelObject(1).value(QStringLiteral("primaryValid")).toBool());
        QVERIFY(station->channelObject(1).value(QStringLiteral("secondaryValid")).toBool());
        const quint64 bound = ownership->incarnation(1);

        // The slice closes: invalid binding.
        model.removeSlice(1);
        NEREUS_TRY_VERIFY(!station->channelObject(1).value(QStringLiteral("primaryValid")).toBool());
        QVERIFY(station->channelObject(1).value(QStringLiteral("secondaryValid")).toBool());

        // A transport-only edit keeps the id and the old incarnation.
        channel.insert(QStringLiteral("serialBaud"), 9600);
        result = run(dispatcher, "setStationCatChannel",
                     {intArg("channel", 1), textArg("config", text(channel))});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(service.channelConfig(1).binding.primaryIncarnation, bound);
        QVERIFY(!station->channelObject(1).value(QStringLiteral("primaryValid")).toBool());

        // Another slice picked: bound to its live incarnation.
        channel.insert(QStringLiteral("primarySliceId"), 0);
        channel.insert(QStringLiteral("secondarySliceId"), -1);
        result = run(dispatcher, "setStationCatChannel",
                     {intArg("channel", 1), textArg("config", text(channel))});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(service.channelConfig(1).binding.primaryIncarnation, ownership->incarnation(0));
        QVERIFY(!service.channelConfig(1).binding.secondarySliceId.has_value());
        QVERIFY(station->channelObject(1).value(QStringLiteral("primaryValid")).toBool());
    }

    // Only a peer that declared stationCat is told stationCatVersion, sent
    // the object and the stream, and has its commands taken.
    void onlyAPeerThatDeclaredItGetsTheCoresCat()
    {
        Core core(true);
        QCOMPARE(core.server->stationCatVersion(), 1);
        LoopbackTransport* declared = core.tokenSignIn(kStationCat);
        QVERIFY(admitted(declared));
        LoopbackTransport* other = core.tokenSignIn({}, QStringLiteral("192.0.2.21"));
        QVERIFY(admitted(other));

        QCOMPARE(capability(declared->received(), QStringLiteral("stationCatVersion")),
                 std::optional<qint64>(1));
        QVERIFY(!capability(other->received(), QStringLiteral("stationCatVersion")).has_value());
        QVERIFY(holds(declared, QStringLiteral("stationCat")));
        QVERIFY(!everSaw(other, QStringLiteral("stationCat")));
        QVERIFY(!seen(declared, QStringLiteral("channel1")).isEmpty());

        // The object is the Core's to report.
        const QString before = core.model->stationCatModel()->global();
        declared->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "stationCat",
            {MirrorUpdate{0, "global", MirrorWireKind::Utf8, QVariant(QStringLiteral("{}"))}},
            7)));
        NEREUS_TRY_VERIFY(!ofType(declared->received(), QStringLiteral("property.result")).isEmpty());
        const QJsonObject written = firstOfType(declared->received(),
                                                QStringLiteral("property.result"))
                                        .value(QStringLiteral("results")).toArray()
                                        .first().toObject();
        QVERIFY(!written.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(written.value(QStringLiteral("reason")).toString(),
                 StationCatModel::readOnlyReason());
        QCOMPARE(core.model->stationCatModel()->global(), before);

        // A change reaches the peer that declared it and never the other.
        const QJsonObject accepted = core.invoke(declared, "setStationCatGlobal",
            {textArg("config", text({{QStringLiteral("rigIdentity"), QStringLiteral("TS-480")}}))});
        QVERIFY2(accepted.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(accepted.value(QStringLiteral("reason")).toString()));
        NEREUS_TRY_VERIFY(seen(declared, QStringLiteral("global")).value(QStringLiteral("config"))
                              .toObject().value(QStringLiteral("rigIdentity")).toString()
                          == QStringLiteral("TS-480"));
        QTest::qWait(50);
        QVERIFY(!everSaw(other, QStringLiteral("stationCat")));

        // Its commands and the stream are refused to the other.
        QJsonObject refused = core.invoke(other, "refreshStationCatDevices");
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kNotDeclared);
        refused = core.invoke(other, "setStationCatChannel",
            {intArg("channel", 1), textArg("config", QStringLiteral("{}"))});
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kNotDeclared);
        refused = core.invoke(other, "setStationCatGlobal", {textArg("config", QStringLiteral("{}"))});
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kNotDeclared);
        refused = core.invoke(other, "testStationCatCommand",
            {intArg("requestId", 1), intArg("channel", 1), textArg("command", QStringLiteral("ID;"))});
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kNotDeclared);
        refused = core.invoke(other, "records.subscribe",
            {textArg("stream", QStringLiteral("catLog")), intArg("backlog", 10)});
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kNotDeclared);
        core.model->catService()->testCommand(1, "ID;");
        QTest::qWait(50);
        QVERIFY(!sawCatLog(other));

        // An app older than the Core's CAT is told to update.
        LoopbackTransport* older = core.open(QStringLiteral("192.0.2.22"));
        Core::send(older, SessionMessages::authRequest(core.server->token()),
                   kRadioIdentitySessionProtocolMinor - 1, kStationCat);
        QVERIFY(admitted(older));
        QVERIFY(!capability(older->received(), QStringLiteral("stationCatVersion")).has_value());
        QVERIFY(!everSaw(older, QStringLiteral("stationCat")));
        refused = core.invoke(older, "refreshStationCatDevices");
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kOlderApp);

        QVERIFY(OperatorWording::isPlain(kNotDeclared));
        QVERIFY(OperatorWording::isPlain(kOlderApp));
    }

    // The `catLog` stream sends each CAT exchange the Core logs.
    void catLogStreamDeliversAnExchange()
    {
        Core core(true);
        LoopbackTransport* app = core.tokenSignIn(kStationCat);
        QVERIFY(admitted(app));
        const QJsonObject subscribed = core.invoke(app, "records.subscribe",
            {textArg("stream", QStringLiteral("catLog")), intArg("backlog", 100)});
        QVERIFY2(subscribed.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(subscribed.value(QStringLiteral("reason")).toString()));
        QCOMPARE(core.model->catService()->testCommand(1, "ID;"), QByteArray("ID019;"));
        NEREUS_TRY_VERIFY(catLogRecords(app).size() >= 2);
        const QList<QJsonObject> records = catLogRecords(app);
        const QJsonObject in = records.at(records.size() - 2);
        const QJsonObject out = records.last();
        QCOMPARE(in.value(QStringLiteral("channel")).toInt(), 1);
        QVERIFY(in.value(QStringLiteral("inbound")).toBool());
        QCOMPARE(in.value(QStringLiteral("text")).toString(), QStringLiteral("ID;"));
        QVERIFY(in.value(QStringLiteral("time")).toDouble() > 0);
        QCOMPARE(out.value(QStringLiteral("channel")).toInt(), 1);
        QVERIFY(!out.value(QStringLiteral("inbound")).toBool(true));
        QCOMPARE(out.value(QStringLiteral("text")).toString(), QStringLiteral("ID019;"));
        QCOMPARE(core.server->recordStreamForTest(QStringLiteral("catLog"))->capacity(),
                 StationCatModel::kLogCapacity);
    }

    // Over a real Core: a window changes a channel's TCP port, and the
    // Core's CatService and the window's mirrored object both show it.
    void windowChangesAChannelOnTheCore()
    {
        // The window's settings are migrated, as CoreInit leaves them.
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                         QStringLiteral("7"));
        Core core(true);
        core.settings->setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
        auto window = std::make_unique<RadioModel>(RadioModel::Role::Remote);
        SettingsProxy proxy;
        auto client = std::make_unique<StationClient>(window.get(), &proxy);
        window->attachStation(client.get());
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        QVERIFY(completed.wait(5000) || !completed.isEmpty());
        NEREUS_TRY_VERIFY(client->stationCatAvailable());
        QCOMPARE(client->capabilities().stationCatVersion, 1);
        StationCatModel* mirrored = window->stationCatModel();
        NEREUS_TRY_VERIFY(!mirrored->channelObject(1).isEmpty());
        QCOMPARE(mirrored->channel1(), core.model->stationCatModel()->channel1());

        const quint16 port = freePort();
        QJsonObject channel = channelConfig(*mirrored, 1);
        channel.insert(QStringLiteral("tcpEnabled"), true);
        channel.insert(QStringLiteral("tcpPort"), int(port));
        const IStationLink::CommandOutcome outcome =
            client->requestStationCatChannel(1, text(channel));
        QVERIFY2(outcome.sent, qPrintable(outcome.reason));
        NEREUS_TRY_VERIFY(core.model->catService()->channelConfig(1).tcpPort == int(port));
        QVERIFY(core.model->catService()->channelConfig(1).tcpEnabled);
        NEREUS_TRY_VERIFY(channelConfig(*mirrored, 1).value(QStringLiteral("tcpPort")).toInt()
                          == int(port));
        QVERIFY(channelConfig(*mirrored, 1).value(QStringLiteral("tcpEnabled")).toBool());

        // A test command's reply comes back on the window's object.
        QVERIFY(client->requestStationCatTest(41, 1, QStringLiteral("ID;")).sent);
        NEREUS_TRY_VERIFY(mirrored->lastTestObject().value(QStringLiteral("requestId")).toInteger()
                          == 41);
        QCOMPARE(mirrored->lastTestObject().value(QStringLiteral("reply")).toString(),
                 QStringLiteral("ID019;"));
        QVERIFY(client->requestStationCatGlobal(
                    text({{QStringLiteral("rigIdentity"), QStringLiteral("TS-480")}})).sent);
        NEREUS_TRY_VERIFY(core.model->catService()->globalConfig().rigIdentity
                          == QStringLiteral("TS-480"));
        QVERIFY(client->requestStationCatRefreshDevices().sent);

        client.reset();
    }

private:
    static void clearCat()
    {
        for (const QString& key : AppSettings::instance().allKeys()) {
            if (key.startsWith(QStringLiteral("Cat/"))) {
                AppSettings::instance().remove(key);
            }
        }
    }
};

QTEST_MAIN(TstStationCat)
#include "tst_station_cat.moc"
