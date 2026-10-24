// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_station_roundtable_link.cpp  (NereusSDR)
// =================================================================
//
// Roundtable on the station link (Roundtable design 10.3, plan Task 11):
// the stationRoundtableVersion capability and the read-only `roundtable`
// object only for a peer that declared roundtable 1; the roundtable.*
// verbs and their plain refusals; a room's stream there before the read
// answer that names it, and gone ten minutes after nothing needs it; two
// remote windows sharing the Core's first-run answer and mention read
// mark; the Core's Roundtable settings written from a window, and the two
// panel preferences kept by the window.
//
// The Core's RoundtableClient never connects: each test feeds it the views
// a Roundtable server would send (its own signals), so nothing leaves
// 127.0.0.1 and nothing waits on a network.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: Created (Roundtable plan Task 11). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>
#include <optional>

#include "OperatorWording.h"
#include "SessionWait.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/RadioConnection.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kRoom = QStringLiteral("usb-14200000");
const QString kOtherRoom = QStringLiteral("cw-7030000");
const QString kUnreadable = QStringLiteral("The Core could not read this request.");
const QString kNoRoom = QStringLiteral("Roundtable has no room there right now.");
const QString kUpdateApp = QStringLiteral("Update this app to use Roundtable on this Core.");
const QString kAnswered =
    QStringLiteral("Someone already answered the Roundtable question on this Core.");

MirrorUpdate utf8(const char* name, const QString& value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(value)};
}
MirrorUpdate boolean(const char* name, bool value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(value)};
}
MirrorUpdate i64(const char* name, qint64 value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(qlonglong(value))};
}

QList<SessionMessage> receivedMessages(const LoopbackTransport* peer)
{
    QList<SessionMessage> messages;
    for (const QByteArray& wire : peer->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)) {
            messages.append(message);
        }
    }
    return messages;
}

bool snapshotDone(const LoopbackTransport* peer)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::SnapshotComplete) {
            return true;
        }
    }
    return false;
}

std::optional<SessionMessage> resultFor(const LoopbackTransport* peer, quint32 id)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::CommandResult && m.commandId == id) {
            return m;
        }
    }
    return std::nullopt;
}

std::optional<qint64> capabilityOf(const LoopbackTransport* peer, const QByteArray& name)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind != SessionMessageKind::Capabilities) {
            continue;
        }
        for (const MirrorUpdate& u : m.updates) {
            if (u.name == name) {
                return u.value.toLongLong();
            }
        }
    }
    return std::nullopt;
}

bool sawRoundtableObject(const LoopbackTransport* peer)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.objectKey == "roundtable" || m.className == "RoundtableState") {
            return true;
        }
    }
    return false;
}

// The record batches `peer` was sent for `stream`.
QList<RecordBatch> batchesFor(const LoopbackTransport* peer, const QString& stream)
{
    QList<RecordBatch> batches;
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::RecordBatch && m.recordBatch.stream == stream) {
            batches.append(m.recordBatch);
        }
    }
    return batches;
}

QString valueOf(const SessionMessage& m, const QByteArray& name)
{
    for (const MirrorUpdate& u : m.updates) {
        if (u.name == name) {
            return u.value.toString();
        }
    }
    return {};
}

SessionMessage invoke(LoopbackTransport* peer, const QByteArray& verb,
                      const QList<MirrorUpdate>& arguments)
{
    static quint32 nextId = 7000;
    const quint32 id = ++nextId;
    peer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
    std::optional<SessionMessage> answer;
    QDeadlineTimer deadline(5000);
    while (!(answer = resultFor(peer, id)) && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    if (!answer) {
        SessionMessage none;
        none.reason = QStringLiteral("no answer to ") + QString::fromUtf8(verb);
        return none;
    }
    return *answer;
}

QJsonObject roomsFull()
{
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("rooms")},
        {QStringLiteral("rooms"),
         QJsonArray{QJsonObject{{QStringLiteral("id"), kRoom},
                                {QStringLiteral("family"), QStringLiteral("usb")},
                                {QStringLiteral("hz"), 14200000},
                                {QStringLiteral("members"), 2}},
                    QJsonObject{{QStringLiteral("id"), kOtherRoom},
                                {QStringLiteral("family"), QStringLiteral("cw")},
                                {QStringLiteral("hz"), 7030000},
                                {QStringLiteral("members"), 1}}}}};
}

QJsonObject roomFull(const QString& id)
{
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("room")},
        {QStringLiteral("id"), id},
        {QStringLiteral("members"),
         QJsonArray{QJsonObject{{QStringLiteral("ref"), QStringLiteral("a/1")},
                                {QStringLiteral("callsign"), QStringLiteral("W1AW")},
                                {QStringLiteral("reading"), QJsonValue::Null}}}},
        {QStringLiteral("now"), QJsonArray{}},
        {QStringLiteral("earlier"), QJsonArray{}},
        {QStringLiteral("messages"),
         QJsonArray{QJsonObject{{QStringLiteral("id"), 31},
                                {QStringLiteral("t"), 5000},
                                {QStringLiteral("callsign"), QStringLiteral("W1AW")},
                                {QStringLiteral("text"), QStringLiteral("hello")}}}}};
}

// A Core: its radio model (whose Roundtable client never connects), its
// settings, and its station server.
struct Core {
    QTemporaryDir dir;
    RadioModel model;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<StationServer> server;
    int peers = 0;

    Core()
    {
        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:72");
        model.setLastRadioInfoForTest(info);
        model.setConnectionStateForTest(ConnectionState::Connected);
        settings = std::make_unique<AppSettings>(dir.filePath(QStringLiteral("core.settings")));
        server = std::make_unique<StationServer>(
            &model, *settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    }

    RoundtableClient* client() const { return model.roundtableClient(); }
    RoundtableState* state() const { return model.roundtableState(); }

    // A peer signed in with the Core's token, declaring `features`.
    LoopbackTransport* connect(QObject* owner, const QHash<QByteArray, int>& features)
    {
        ++peers;
        auto* core = new LoopbackTransport(QStringLiteral("core-%1").arg(peers), owner);
        auto* peer = new LoopbackTransport(QStringLiteral("peer-%1").arg(peers), owner);
        core->linkTo(peer);
        server->acceptTransport(core);
        peer->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("roundtable-test"),
            {kSessionProtocolMajor}, features)));
        peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
        return peer;
    }

    // Two rooms listed and kRoom in view, as the server would send them.
    void listRooms()
    {
        emit client()->roomsReceived(roomsFull());
        emit client()->roomReceived(roomFull(kRoom));
    }
};

// A remote window of `core`: its model, settings and link.
struct Window {
    QTemporaryDir dir;
    RadioModel model{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    std::unique_ptr<AppSettings> settings;

    Window(Core& core, QObject* owner, const QString& name)
    {
        settings = std::make_unique<AppSettings>(dir.filePath(QStringLiteral("window.settings")));
        settings->setRemoteBackend(&proxy);
        client = std::make_unique<StationClient>(&model, &proxy);
        auto* stationEnd = new LoopbackTransport(name + QStringLiteral("-station"), owner);
        auto* clientEnd = new LoopbackTransport(name + QStringLiteral("-client"), owner);
        stationEnd->linkTo(clientEnd);
        client->startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
    }

    ~Window() { settings->setRemoteBackend(nullptr); }

    RoundtableModel* rooms() const { return model.roundtableModel(); }
};

} // namespace

class TstStationRoundtableLink : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── The gate ───────────────────────────────────────────────────

    void onlyAPeerThatDeclaredRoundtableIsSentIt()
    {
        Core core;
        LoopbackTransport* older = core.connect(this, {});
        LoopbackTransport* newer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(older));
        NEREUS_TRY_VERIFY(snapshotDone(newer));
        QVERIFY(!capabilityOf(older, "stationRoundtableVersion").has_value());
        QVERIFY(!sawRoundtableObject(older));
        QCOMPARE(capabilityOf(newer, "stationRoundtableVersion").value_or(-1), 1);
        QVERIFY(sawRoundtableObject(newer));

        // An older app's verb is refused in words it can show.
        const SessionMessage refused =
            invoke(older, "roundtable.read", {utf8("roomId", kRoom)});
        QVERIFY(!refused.accepted);
        QCOMPARE(refused.reason, kUpdateApp);
        // And so is a subscription to the rooms.
        const SessionMessage subscribed = invoke(
            older, "records.subscribe",
            {utf8("stream", QStringLiteral("roundtableRooms")), i64("backlog", 10)});
        QVERIFY(!subscribed.accepted);
        QVERIFY(OperatorWording::isPlain(subscribed.reason));

        // Nothing about Roundtable reaches the older peer as it changes.
        const qsizetype mark = older->received().size();
        core.listRooms();
        QVERIFY(invoke(newer, "roundtable.answerFirstRun", {boolean("live", true)}).accepted);
        QTest::qWait(150);
        for (qsizetype i = mark; i < older->received().size(); ++i) {
            QVERIFY2(!older->received().at(i).contains("roundtable"),
                     older->received().at(i).constData());
        }
    }

    void theObjectIsReadOnly()
    {
        Core core;
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));
        peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "roundtable", {MirrorUpdate{0, "hidden", MirrorWireKind::Bool, QVariant(true)}})));
        QTest::qWait(150);
        QVERIFY(!core.state()->hidden());
    }

    // ── The verbs ──────────────────────────────────────────────────

    void theVerbsAnswerPlainly()
    {
        Core core;
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));

        // A request missing a name, or with one it does not know.
        SessionMessage answer = invoke(peer, "roundtable.read", {utf8("room", kRoom)});
        QCOMPARE(answer.reason, kUnreadable);
        answer = invoke(peer, "roundtable.setHidden", {utf8("hidden", QStringLiteral("yes"))});
        QCOMPARE(answer.reason, kUnreadable);

        // A room that is not listed.
        answer = invoke(peer, "roundtable.read", {utf8("roomId", QStringLiteral("usb-1"))});
        QVERIFY(!answer.accepted);
        QCOMPARE(answer.reason, kNoRoom);

        // The station connection is off: the verbs that need it say so.
        for (const auto& [verb, arguments] :
             QList<std::pair<QByteArray, QList<MirrorUpdate>>>{
                 {"roundtable.delete", {utf8("messageId", QStringLiteral("31"))}},
                 {"roundtable.name",
                  {utf8("turnId", QStringLiteral("4")), utf8("name", QStringLiteral("W1AW"))}},
                 {"roundtable.report",
                  {utf8("kind", QStringLiteral("message")), utf8("target", QStringLiteral("31")),
                   utf8("reason", QStringLiteral("spam"))}},
                 {"roundtable.post",
                  {utf8("roomId", kRoom), utf8("text", QStringLiteral("hi")),
                   utf8("clientId", QStringLiteral("c-1"))}},
                 {"roundtable.webSignIn", {}},
                 {"roundtable.deleteArchive",
                  {utf8("confirm", QStringLiteral("delete my archive"))}}}) {
            answer = invoke(peer, verb, arguments);
            QVERIFY2(!answer.accepted, verb.constData());
            QVERIFY2(OperatorWording::isPlain(answer.reason), qPrintable(answer.reason));
            QVERIFY2(answer.reason != kUnreadable, verb.constData());
        }
        answer = invoke(peer, "roundtable.deleteArchive",
                        {utf8("confirm", QStringLiteral("yes"))});
        QCOMPARE(answer.reason, QStringLiteral("Type delete my archive to confirm."));
        answer = invoke(peer, "roundtable.mute", {utf8("callsign", QStringLiteral("not a call"))});
        QCOMPARE(answer.reason, QStringLiteral("Roundtable can only mute a callsign."));

        // The Core's own answers.
        QVERIFY(invoke(peer, "roundtable.mute", {utf8("callsign", QStringLiteral("W1AW/P"))})
                    .accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MuteList")).toString(),
                 QStringLiteral("W1AW"));
        QVERIFY(invoke(peer, "roundtable.unmute", {utf8("callsign", QStringLiteral("W1AW"))})
                    .accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MuteList")).toString(), QString());
        QVERIFY(invoke(peer, "roundtable.setHidden", {boolean("hidden", true)}).accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/Hidden")).toString(),
                 QStringLiteral("True"));
        NEREUS_TRY_VERIFY(core.state()->hidden());
        QVERIFY(invoke(peer, "roundtable.markMentionRead", {i64("upToMs", 1000)}).accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MentionsSeenMs")).toString(),
                 QStringLiteral("1000"));
        // The mark only moves forward.
        QVERIFY(invoke(peer, "roundtable.markMentionRead", {i64("upToMs", 10)}).accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MentionsSeenMs")).toString(),
                 QStringLiteral("1000"));
    }

    void aReadsStreamIsThereBeforeItsAnswer()
    {
        Core core;
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));
        core.listRooms();

        const SessionMessage read = invoke(peer, "roundtable.read", {utf8("roomId", kRoom)});
        QVERIFY2(read.accepted, qPrintable(read.reason));
        const QString stream = valueOf(read, "stream");
        QCOMPARE(stream, RoundtableModel::roomStreamName(kRoom));

        const SessionMessage subscribed =
            invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 400)});
        QVERIFY2(subscribed.accepted, qPrintable(subscribed.reason));
        NEREUS_TRY_VERIFY(!batchesFor(peer, stream).isEmpty());
        const RecordBatch first = batchesFor(peer, stream).first();
        QVERIFY(first.reset);
        QStringList ids;
        for (const RecordUpsert& u : first.upserts) {
            ids.append(u.id);
        }
        QVERIFY2(ids.contains(QStringLiteral("member:a/1")), qPrintable(ids.join(',')));
        QVERIFY2(ids.contains(QStringLiteral("message:31")), qPrintable(ids.join(',')));

        // The room's changes follow.
        emit core.client()->roomDeltaReceived(QJsonObject{
            {QStringLiteral("room"), kRoom},
            {QStringLiteral("messages"),
             QJsonArray{QJsonObject{{QStringLiteral("id"), 32},
                                    {QStringLiteral("t"), 6000},
                                    {QStringLiteral("callsign"), QStringLiteral("W1AW")},
                                    {QStringLiteral("text"), QStringLiteral("again")}}}}});
        NEREUS_TRY_VERIFY(batchesFor(peer, stream).size() >= 2);
        const RecordBatch next = batchesFor(peer, stream).last();
        QVERIFY(!next.reset);
        QCOMPARE(next.upserts.size(), 1);
        QCOMPARE(next.upserts.first().id, QStringLiteral("message:32"));
    }

    void aStreamNobodyNeedsGoesAfterTenMinutes()
    {
        Core core;
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));
        core.listRooms();
        QVERIFY(invoke(peer, "roundtable.read", {utf8("roomId", kRoom)}).accepted);
        const QString stream = RoundtableModel::roomStreamName(kRoom);

        // Still read: kept however long.
        core.server->sweepRoundtableStreamsForTest(60 * 60 * 1000);
        QVERIFY(invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 1)})
                    .accepted);

        QVERIFY(invoke(peer, "roundtable.unread", {utf8("roomId", kRoom)}).accepted);
        core.server->sweepRoundtableStreamsForTest(9 * 60 * 1000);
        QVERIFY(invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 1)})
                    .accepted);
        core.server->sweepRoundtableStreamsForTest(10 * 60 * 1000);
        QVERIFY(!invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 1)})
                     .accepted);

        // A slice in the room keeps its stream.
        core.state()->setSlices({RoundtableSliceState{0, 14200300, QStringLiteral("USB"), 2700,
                                                      false}});
        QCOMPARE(core.state()->sliceRoomId(0), kRoom);
        core.server->sweepRoundtableStreamsForTest(60 * 60 * 1000);
        QVERIFY(invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 1)})
                    .accepted);
    }

    // ── Two windows ────────────────────────────────────────────────

    void aFirstRunAnswerReachesEveryWindow()
    {
        Core core;
        Window first(core, this, QStringLiteral("first"));
        Window second(core, this, QStringLiteral("second"));
        NEREUS_TRY_VERIFY(first.client->roundtableAvailable());
        NEREUS_TRY_VERIFY(second.client->roundtableAvailable());
        QVERIFY(!second.rooms()->firstRunAnswered());

        first.rooms()->answerFirstRun(true);
        NEREUS_TRY_VERIFY(second.rooms()->firstRunAnswered());
        QVERIFY(first.rooms()->firstRunAnswered());
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/FirstRunAnswer")).toString(),
                 QStringLiteral("Live"));
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/Hidden")).toString(),
                 QStringLiteral("False"));

        // Asked again from the other window: answered already.
        QSignalSpy refused(second.rooms(), &RoundtableModel::actionRefused);
        second.rooms()->answerFirstRun(false);
        NEREUS_TRY_VERIFY(!refused.isEmpty());
        QCOMPARE(refused.first().at(0).toString(), QStringLiteral("answerFirstRun"));
        QCOMPARE(refused.first().at(1).toString(), kAnswered);
    }

    void aMentionReadOnOneWindowClearsOnTheOther()
    {
        Core core;
        Window first(core, this, QStringLiteral("first"));
        Window second(core, this, QStringLiteral("second"));
        NEREUS_TRY_VERIFY(first.client->roundtableAvailable());
        NEREUS_TRY_VERIFY(second.client->roundtableAvailable());

        emit core.client()->mentionReceived(QJsonObject{
            {QStringLiteral("room"), kRoom},
            {QStringLiteral("message"),
             QJsonObject{{QStringLiteral("id"), 41},
                         {QStringLiteral("t"), 8000},
                         {QStringLiteral("callsign"), QStringLiteral("W1AW")},
                         {QStringLiteral("text"), QStringLiteral("KG4VCF are you there")}}}});
        NEREUS_TRY_COMPARE(first.rooms()->mentionCount(kRoom), 1);
        NEREUS_TRY_COMPARE(second.rooms()->mentionCount(kRoom), 1);

        first.rooms()->markMentionRead(8000);
        NEREUS_TRY_COMPARE(second.rooms()->mentionCount(kRoom), 0);
        QCOMPARE(first.rooms()->mentionCount(kRoom), 0);
    }

    void aWindowFollowsTheRoomsItReadsAndItsSlicesAreIn()
    {
        Core core;
        core.listRooms();
        emit core.client()->roomReceived(roomFull(kOtherRoom));
        Window window(core, this, QStringLiteral("window"));
        NEREUS_TRY_VERIFY(window.client->roundtableAvailable());

        // The band list.
        NEREUS_TRY_COMPARE(window.rooms()->rooms().size(), 2);
        QCOMPARE(window.rooms()->rooms(), core.model.roundtableModel()->rooms());

        // A room read from the window.
        window.rooms()->readRoom(kRoom);
        NEREUS_TRY_VERIFY(!window.rooms()->room(kRoom).isEmpty());
        QCOMPARE(window.rooms()->room(kRoom), core.model.roundtableModel()->room(kRoom));
        window.rooms()->unreadRoom();
        NEREUS_TRY_VERIFY(window.rooms()->room(kRoom).isEmpty());

        // A room the Core's slice is in, without reading it.
        core.state()->setSlices({RoundtableSliceState{0, 7030100, QStringLiteral("CWU"), 500,
                                                      false}});
        NEREUS_TRY_COMPARE(window.rooms()->sliceRoomId(0), kOtherRoom);
        NEREUS_TRY_VERIFY(!window.rooms()->room(kOtherRoom).isEmpty());
        QCOMPARE(window.rooms()->room(kOtherRoom),
                 core.model.roundtableModel()->room(kOtherRoom));

        // A read of a room that is not listed comes back refused.
        QSignalSpy refused(window.rooms(), &RoundtableModel::actionRefused);
        window.rooms()->readRoom(QStringLiteral("usb-1"));
        NEREUS_TRY_VERIFY(!refused.isEmpty());
        QCOMPARE(refused.first().at(1).toString(), kNoRoom);
        QVERIFY(window.rooms()->readingRoomId().isEmpty());
    }

    // ── Settings ───────────────────────────────────────────────────

    void theCoresSettingsAreWrittenAndThePanelsAreNot()
    {
        Core core;
        Window window(core, this, QStringLiteral("window"));
        NEREUS_TRY_VERIFY(window.client->roundtableAvailable());
        NEREUS_TRY_VERIFY(window.proxy.ready());

        window.settings->setValue(QStringLiteral("Roundtable/Callsign"),
                                  QStringLiteral("KG4VCF"));
        NEREUS_TRY_COMPARE(core.settings->value(QStringLiteral("Roundtable/Callsign")).toString(),
                           QStringLiteral("KG4VCF"));

        window.settings->setValue(QStringLiteral("Roundtable/PanelOpensOn"),
                                  QStringLiteral("Band"));
        QVERIFY(!window.proxy.handlesKey(QStringLiteral("Roundtable/PanelOpensOn")));
        QCOMPARE(window.settings->value(QStringLiteral("Roundtable/PanelOpensOn")).toString(),
                 QStringLiteral("Band"));
        QTest::qWait(150);
        QVERIFY(!core.settings->contains(QStringLiteral("Roundtable/PanelOpensOn")));
    }
};

QTEST_MAIN(TstStationRoundtableLink)
#include "tst_station_roundtable_link.moc"
