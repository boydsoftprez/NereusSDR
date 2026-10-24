// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_model.cpp  (NereusSDR)
// =================================================================
//
// RoundtableModel in its two placements (Roundtable design 10.3): fed from
// the Core's own RoundtableClient (Everything) and fed from the Core's
// record streams (a window), the same scripted rooms read back equal from
// rooms() and room(). The window's records travel through a RecordStream
// exactly as the Core builds them (roomsRecords, roomRecords, upserts in id
// order, removes for what went). A window's actions go to its link; a
// model with neither placement refuses them plainly.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: Created (Roundtable plan Task 11). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-24: Roundtable plan Task 13: earlier turns of the last 30
//               minutes in both placements, on a test clock. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include <algorithm>

#include "OperatorWording.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "core/session/RecordStream.h"
#include "models/RoundtableModel.h"

using namespace NereusSDR;

namespace {

const QString kRoom = QStringLiteral("usb-14200000");
const QString kOtherRoom = QStringLiteral("cw-7030000");

QJsonObject member(const QString& ref, const QString& callsign, const QJsonValue& reading)
{
    return QJsonObject{{QStringLiteral("ref"), ref},
                       {QStringLiteral("callsign"), callsign},
                       {QStringLiteral("grid"), QStringLiteral("EM73")},
                       {QStringLiteral("reading"), reading}};
}

QJsonObject reading(double dbm, qint64 t)
{
    return QJsonObject{{QStringLiteral("dbm"), dbm}, {QStringLiteral("t"), t}};
}

QJsonObject turn(qint64 id, qint64 start, const QJsonValue& end, const QJsonArray& names)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("kind"), QStringLiteral("voice")},
                       {QStringLiteral("start"), start},
                       {QStringLiteral("end"), end},
                       {QStringLiteral("names"), names}};
}

QJsonObject name(const QString& callsign, int count)
{
    return QJsonObject{{QStringLiteral("name"), callsign}, {QStringLiteral("count"), count}};
}

QJsonObject message(qint64 id, qint64 t, const QString& callsign, const QString& text)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("t"), t},
                       {QStringLiteral("callsign"), callsign},
                       {QStringLiteral("text"), text}};
}

QJsonObject summary(const QString& id, const QString& family, qint64 hz, int members)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("family"), family},
                       {QStringLiteral("hz"), hz},
                       {QStringLiteral("members"), members}};
}

QJsonObject roomsFull()
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("rooms")},
                       {QStringLiteral("rooms"),
                        QJsonArray{summary(kRoom, QStringLiteral("usb"), 14200000, 2),
                                   summary(kOtherRoom, QStringLiteral("cw"), 7030000, 1)}}};
}

QJsonObject roomFull()
{
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("room")},
        {QStringLiteral("id"), kRoom},
        {QStringLiteral("members"),
         QJsonArray{member(QStringLiteral("a/1"), QStringLiteral("W1AW"), reading(-73.5, 1000)),
                    member(QStringLiteral("b/0"), QStringLiteral("KG4VCF"), QJsonValue::Null)}},
        {QStringLiteral("now"),
         QJsonArray{turn(11, 900, QJsonValue::Null,
                         QJsonArray{name(QStringLiteral("W1AW"), 1),
                                    name(QStringLiteral("K1ABC"), 3)})}},
        {QStringLiteral("earlier"),
         QJsonArray{turn(9, 100, 400, QJsonArray{}),
                    turn(10, 450, 800, QJsonArray{name(QStringLiteral("N0CALL"), 2)})}},
        {QStringLiteral("messages"),
         QJsonArray{message(21, 950, QStringLiteral("W1AW"), QStringLiteral("good signal")),
                    message(20, 940, QStringLiteral("KG4VCF"), QStringLiteral("hello all"))}}};
}

QJsonObject roomDelta()
{
    // A third station joins, the first leaves, a turn ends and another
    // opens, and a message arrives.
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("roomDelta")},
        {QStringLiteral("room"), kRoom},
        {QStringLiteral("members"),
         QJsonArray{member(QStringLiteral("c/2"), QStringLiteral("K1ABC"), reading(-80.0, 1200)),
                    member(QStringLiteral("b/0"), QStringLiteral("KG4VCF"), reading(-61.0, 1210))}},
        {QStringLiteral("left"), QJsonArray{QStringLiteral("a/1")}},
        {QStringLiteral("turns"),
         QJsonArray{turn(11, 900, 1100, QJsonArray{name(QStringLiteral("K1ABC"), 4)}),
                    turn(12, 1150, QJsonValue::Null, QJsonArray{})}},
        {QStringLiteral("messages"),
         QJsonArray{message(22, 1160, QStringLiteral("K1ABC"), QStringLiteral("thanks"))}}};
}

// The Core's side of one stream: what StationServer::syncRoundtableStream
// does (removes what went, upserts what changed, in id order), and the one
// window subscribed to it.
class CoreStream {
public:
    explicit CoreStream(const QString& name, int capacity)
        : m_stream(name, capacity)
    {
    }

    // The window subscribes; its first batch.
    RecordBatch subscribe() { return m_stream.subscribe(this, m_stream.capacity()); }

    // The records as the Core's model now gives them; what the window is
    // owed (empty when nothing changed).
    RecordBatch sync(const QHash<QString, QJsonObject>& records)
    {
        for (auto it = m_held.begin(); it != m_held.end();) {
            if (!records.contains(it.key())) {
                m_stream.remove(it.key());
                it = m_held.erase(it);
            } else {
                ++it;
            }
        }
        QStringList ids = records.keys();
        std::sort(ids.begin(), ids.end());
        for (const QString& id : ids) {
            const auto was = m_held.constFind(id);
            if (was != m_held.cend() && was.value() == records.value(id)) {
                continue;
            }
            m_stream.upsert(id, records.value(id));
            m_held.insert(id, records.value(id));
        }
        RecordBatch batch;
        for (const auto& [peer, pending] : m_stream.takePending()) {
            if (peer == this) {
                batch = pending;
            }
        }
        return batch;
    }

private:
    RecordStream m_stream;
    QHash<QString, QJsonObject> m_held;
};

// A window's link: records what the model asked for, and answers as told.
class RecordingLink : public IStationLink {
public:
    QStringList calls;
    CommandOutcome answer{true, QString()};

    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }

    bool roundtableAvailable() const override { return true; }
    CommandOutcome requestRoundtablePost(const QString& roomId, const QString& text,
                                         const QString& clientId) override
    {
        return note(QStringLiteral("post %1 %2 %3").arg(roomId, text, clientId));
    }
    CommandOutcome requestRoundtableDelete(const QString& id) override
    {
        return note(QStringLiteral("delete ") + id);
    }
    CommandOutcome requestRoundtableName(const QString& turnId, const QString& callsign) override
    {
        return note(QStringLiteral("name %1 %2").arg(turnId, callsign));
    }
    CommandOutcome requestRoundtableReport(const QString& kind, const QString& target,
                                           const QString& reason) override
    {
        return note(QStringLiteral("report %1 %2 %3").arg(kind, target, reason));
    }
    CommandOutcome requestRoundtableRead(const QString& roomId) override
    {
        return note(QStringLiteral("read ") + roomId);
    }
    CommandOutcome requestRoundtableUnread(const QString& roomId) override
    {
        return note(QStringLiteral("unread ") + roomId);
    }
    CommandOutcome requestRoundtableSetHidden(bool hidden) override
    {
        return note(QStringLiteral("setHidden %1").arg(hidden));
    }
    CommandOutcome requestRoundtableAnswerFirstRun(bool live) override
    {
        return note(QStringLiteral("answerFirstRun %1").arg(live));
    }
    CommandOutcome requestRoundtableMute(const QString& callsign) override
    {
        return note(QStringLiteral("mute ") + callsign);
    }
    CommandOutcome requestRoundtableUnmute(const QString& callsign) override
    {
        return note(QStringLiteral("unmute ") + callsign);
    }
    CommandOutcome requestRoundtableMarkMentionRead(qint64 upToMs) override
    {
        return note(QStringLiteral("markMentionRead %1").arg(upToMs));
    }
    CommandOutcome requestRoundtableWebSignIn() override
    {
        return note(QStringLiteral("webSignIn"));
    }
    CommandOutcome requestRoundtableDeleteArchive(const QString& confirm) override
    {
        return note(QStringLiteral("deleteArchive ") + confirm);
    }

private:
    CommandOutcome note(const QString& call)
    {
        calls.append(call);
        return answer;
    }
};

// The two placements side by side: `local` fed by the Core's client, the
// window's `remote` fed by the Core's streams of `local`.
struct Placements {
    RoundtableClient client;
    RoundtableState coreState;
    RoundtableModel local;
    RoundtableState windowState;
    RecordingLink link;
    RoundtableModel remote;
    CoreStream roomsStream{QString::fromLatin1(RoundtableModel::kRoomsStream),
                           RoundtableModel::kRoomsCapacity};
    CoreStream roomStream{RoundtableModel::roomStreamName(kRoom), RoundtableModel::kRoomCapacity};

    Placements()
    {
        local.attachLocal(&client, &coreState);
        remote.attachStation(&link, &windowState);
        remote.applyRecordBatch(roomsStream.subscribe());
        remote.applyRecordBatch(roomStream.subscribe());
    }

    // What the Core sends after its model changed.
    void carry()
    {
        const RecordBatch rooms = roomsStream.sync(RoundtableModel::roomsRecords(local.rooms()));
        if (!rooms.isEmpty()) {
            remote.applyRecordBatch(rooms);
        }
        const RecordBatch room = roomStream.sync(RoundtableModel::roomRecords(local.room(kRoom)));
        if (!room.isEmpty()) {
            remote.applyRecordBatch(room);
        }
    }
};

QString compact(const QJsonObject& o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString compact(const QJsonArray& a)
{
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}

QStringList refsOf(const QJsonObject& room)
{
    QStringList refs;
    for (const QJsonValue& m : room.value(QStringLiteral("members")).toArray()) {
        refs.append(m.toObject().value(QStringLiteral("ref")).toString());
    }
    return refs;
}

} // namespace

class TstRoundtableModel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The scripted rooms' times are a few seconds after the epoch.
        RoundtableModel::setClockForTest([] { return qint64(2000); });
    }

    void cleanupTestCase() { RoundtableModel::setClockForTest({}); }

    void bothPlacementsHoldEqualRooms()
    {
        Placements p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        p.carry();
        QCOMPARE(compact(p.remote.rooms()), compact(p.local.rooms()));
        QCOMPARE(compact(p.remote.room(kRoom)), compact(p.local.room(kRoom)));

        emit p.client.roomDeltaReceived(roomDelta());
        emit p.client.roomsDeltaReceived(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("roomsDelta")},
            {QStringLiteral("change"),
             QJsonArray{summary(kRoom, QStringLiteral("usb"), 14200000, 2)}},
            {QStringLiteral("remove"), QJsonArray{kOtherRoom}},
            {QStringLiteral("add"),
             QJsonArray{summary(QStringLiteral("am-3885000"), QStringLiteral("am"), 3885000, 1)}}});
        p.carry();
        QCOMPARE(compact(p.remote.rooms()), compact(p.local.rooms()));
        QCOMPARE(compact(p.remote.room(kRoom)), compact(p.local.room(kRoom)));
    }

    void roomsAndRoomReadAsTheDesignSays()
    {
        Placements p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        p.carry();
        // The band list by center frequency.
        const QJsonArray rooms = p.remote.rooms();
        QCOMPARE(rooms.size(), 2);
        QCOMPARE(rooms.at(0).toObject().value(QStringLiteral("id")).toString(), kOtherRoom);
        QCOMPARE(rooms.at(1).toObject().value(QStringLiteral("id")).toString(), kRoom);
        QVERIFY(!rooms.at(0).toObject().contains(QStringLiteral("record")));

        const QJsonObject room = p.remote.room(kRoom);
        QCOMPARE(room.value(QStringLiteral("family")).toString(), QStringLiteral("usb"));
        QCOMPARE(room.value(QStringLiteral("hz")).toInteger(), 14200000);
        QCOMPARE(refsOf(room), (QStringList{QStringLiteral("a/1"), QStringLiteral("b/0")}));
        const QJsonArray now = room.value(QStringLiteral("now")).toArray();
        QCOMPARE(now.size(), 1);
        // A turn's names by count, highest first.
        const QJsonArray names = now.at(0).toObject().value(QStringLiteral("names")).toArray();
        QCOMPARE(names.at(0).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("K1ABC"));
        QCOMPARE(names.at(1).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("W1AW"));
        // Ended turns newest first; messages oldest first.
        const QJsonArray earlier = room.value(QStringLiteral("earlier")).toArray();
        QCOMPARE(earlier.at(0).toObject().value(QStringLiteral("id")).toInteger(), 10);
        QCOMPARE(earlier.at(1).toObject().value(QStringLiteral("id")).toInteger(), 9);
        const QJsonArray messages = room.value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages.at(0).toObject().value(QStringLiteral("id")).toInteger(), 20);
        QCOMPARE(messages.at(1).toObject().value(QStringLiteral("id")).toInteger(), 21);
    }

    void joiningOrderSurvivesAMemberLeaving()
    {
        Placements p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        p.carry();
        emit p.client.roomDeltaReceived(roomDelta());
        p.carry();
        // b/0 stayed in its place; c/2 joined after it.
        QCOMPARE(refsOf(p.local.room(kRoom)),
                 (QStringList{QStringLiteral("b/0"), QStringLiteral("c/2")}));
        QCOMPARE(refsOf(p.remote.room(kRoom)), refsOf(p.local.room(kRoom)));
    }

    void aRoomKeepsItsLastFiftyMessages()
    {
        Placements p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        QJsonArray many;
        for (int i = 0; i < 60; ++i) {
            many.append(message(100 + i, 2000 + i, QStringLiteral("W1AW"),
                                QStringLiteral("line %1").arg(i)));
        }
        emit p.client.roomDeltaReceived(
            QJsonObject{{QStringLiteral("room"), kRoom}, {QStringLiteral("messages"), many}});
        p.carry();
        const QJsonArray messages = p.local.room(kRoom).value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages.size(), RoundtableModel::kRoomMessages);
        QCOMPARE(messages.first().toObject().value(QStringLiteral("id")).toInteger(), 110);
        QCOMPARE(compact(p.remote.room(kRoom)), compact(p.local.room(kRoom)));
    }

    void aClosedRoomLeavesBothViews()
    {
        Placements p;
        emit p.client.roomsReceived(roomsFull());
        emit p.client.roomReceived(roomFull());
        p.carry();
        QVERIFY(!p.remote.room(kRoom).isEmpty());
        QSignalSpy changed(&p.remote, &RoundtableModel::roomChanged);
        emit p.client.roomDeltaReceived(
            QJsonObject{{QStringLiteral("room"), kRoom}, {QStringLiteral("closed"), true}});
        p.carry();
        QVERIFY(p.local.room(kRoom).isEmpty());
        QVERIFY(p.remote.room(kRoom).isEmpty());
        QVERIFY(!changed.isEmpty());
        QCOMPARE(changed.last().at(0).toString(), kRoom);
    }

    void otherStreamsAreNotRoundtables()
    {
        RoundtableModel model;
        RecordBatch spots;
        spots.stream = QStringLiteral("spots");
        spots.reset = true;
        QVERIFY(!model.applyRecordBatch(spots));
        QCOMPARE(RoundtableModel::roomIdOfStream(QStringLiteral("roundtableRoom:usb-14200000")),
                 kRoom);
        QVERIFY(RoundtableModel::roomIdOfStream(QStringLiteral("roundtableRooms")).isEmpty());
    }

    void aWindowsActionsGoToItsLink()
    {
        Placements p;
        p.remote.post(kRoom, QStringLiteral("hello"), QStringLiteral("c1"));
        p.remote.deleteMessage(QStringLiteral("21"));
        p.remote.nameTurn(QStringLiteral("11"), QStringLiteral("W1AW"));
        p.remote.report(QStringLiteral("message"), QStringLiteral("21"), QStringLiteral("spam"));
        p.remote.readRoom(kRoom);
        QCOMPARE(p.remote.readingRoomId(), kRoom);
        p.remote.unreadRoom();
        QVERIFY(p.remote.readingRoomId().isEmpty());
        p.remote.setHidden(true);
        p.remote.answerFirstRun(true);
        p.remote.mute(QStringLiteral("W1AW"));
        p.remote.unmute(QStringLiteral("W1AW"));
        p.remote.markMentionRead(1234);
        p.remote.requestWebLink();
        p.remote.deleteArchive(QStringLiteral("delete my archive"));
        QCOMPARE(p.link.calls,
                 (QStringList{QStringLiteral("post usb-14200000 hello c1"),
                              QStringLiteral("delete 21"), QStringLiteral("name 11 W1AW"),
                              QStringLiteral("report message 21 spam"),
                              QStringLiteral("read usb-14200000"),
                              QStringLiteral("unread usb-14200000"),
                              QStringLiteral("setHidden 1"), QStringLiteral("answerFirstRun 1"),
                              QStringLiteral("mute W1AW"), QStringLiteral("unmute W1AW"),
                              QStringLiteral("markMentionRead 1234"), QStringLiteral("webSignIn"),
                              QStringLiteral("deleteArchive delete my archive")}));
    }

    void aLinkThatCannotSendIsShown()
    {
        Placements p;
        p.link.answer = {false, IStationLink::roundtableUnavailableReason()};
        QSignalSpy posted(&p.remote, &RoundtableModel::postResult);
        QSignalSpy refused(&p.remote, &RoundtableModel::actionRefused);
        p.remote.post(kRoom, QStringLiteral("hello"), QStringLiteral("c1"));
        QCOMPARE(posted.size(), 1);
        QCOMPARE(posted.at(0).at(0).toString(), QStringLiteral("c1"));
        QCOMPARE(posted.at(0).at(1).toBool(), false);
        p.remote.readRoom(kRoom);
        QVERIFY(p.remote.readingRoomId().isEmpty());
        QCOMPARE(refused.size(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("read"));
        const QString reason = refused.at(0).at(1).toString();
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));

        // A refusal the Core sent back clears the room being read.
        p.link.answer = {true, QString()};
        p.remote.readRoom(kRoom);
        QCOMPARE(p.remote.readingRoomId(), kRoom);
        p.remote.applyRefusal(QStringLiteral("read"),
                              QStringLiteral("Roundtable has no room there right now."));
        QVERIFY(p.remote.readingRoomId().isEmpty());
    }

    void aModelWithNoPlacementRefusesPlainly()
    {
        RoundtableModel model;
        QSignalSpy posted(&model, &RoundtableModel::postResult);
        QSignalSpy refused(&model, &RoundtableModel::actionRefused);
        model.post(kRoom, QStringLiteral("hello"), QStringLiteral("c1"));
        model.mute(QStringLiteral("W1AW"));
        QCOMPARE(posted.size(), 1);
        QCOMPARE(posted.at(0).at(1).toBool(), false);
        QCOMPARE(refused.size(), 1);
        QVERIFY(OperatorWording::isPlain(posted.at(0).at(2).toString()));
        QVERIFY(OperatorWording::isPlain(refused.at(0).at(1).toString()));
    }

    void theWindowReadsTheCoresRoundtableObject()
    {
        Placements p;
        QSignalSpy firstRun(&p.remote, &RoundtableModel::firstRunChanged);
        QSignalSpy mentions(&p.remote, &RoundtableModel::mentionsChanged);
        QSignalSpy slices(&p.remote, &RoundtableModel::sliceRoomsChanged);
        QVERIFY(p.windowState.applyStationValue("firstRunAnswered", true));
        QVERIFY(p.windowState.applyStationValue("mentionCounts",
                                                QStringLiteral("{\"usb-14200000\":2}")));
        QVERIFY(p.windowState.applyStationValue("sliceRooms",
                                                QStringLiteral("{\"0\":\"usb-14200000\"}")));
        QVERIFY(p.remote.firstRunAnswered());
        QCOMPARE(p.remote.mentionCount(kRoom), 2);
        QCOMPARE(p.remote.sliceRoomId(0), kRoom);
        QCOMPARE(firstRun.size(), 1);
        QCOMPARE(mentions.size(), 1);
        QCOMPARE(slices.size(), 1);
        QVERIFY(!p.windowState.applyStationValue("notAProperty", 1));
        p.windowState.clearStationValues();
        QVERIFY(!p.remote.firstRunAnswered());
        QCOMPARE(p.remote.mentionCount(kRoom), 0);
    }

    // Wire spec 5.4.6: a turn that leaves the 30-minute earlier window is
    // not announced; each placement drops it itself, on its own timer.
    void earlierTurnsAreTheLastThirtyMinutes()
    {
        constexpr qint64 kMinute = 60LL * 1000LL;
        qint64 now = 100LL * kMinute;
        RoundtableModel::setClockForTest([&now] { return now; });

        QJsonObject full = roomFull();
        full.insert(QStringLiteral("now"), QJsonArray{});
        full.insert(QStringLiteral("earlier"),
                    QJsonArray{turn(1, 60LL * kMinute, 65LL * kMinute, QJsonArray{}),
                               turn(2, 70LL * kMinute, 75LL * kMinute, QJsonArray{}),
                               turn(3, 90LL * kMinute, 95LL * kMinute, QJsonArray{})});
        Placements p;
        emit p.client.roomReceived(full);
        p.carry();
        const auto earlierIds = [](const QJsonObject& room) {
            QList<qint64> ids;
            for (const QJsonValue& t : room.value(QStringLiteral("earlier")).toArray()) {
                ids.append(t.toObject().value(QStringLiteral("id")).toInteger());
            }
            return ids;
        };
        // Turn 1 ended 35 minutes ago: dropped on arrival.
        QCOMPARE(earlierIds(p.local.room(kRoom)), (QList<qint64>{3, 2}));
        QCOMPARE(earlierIds(p.remote.room(kRoom)), (QList<qint64>{3, 2}));

        // Turn 2 leaves the window: room() stops showing it at once, and
        // the prune drops it and tells the screens.
        now = 105LL * kMinute + 1;
        QCOMPARE(earlierIds(p.local.room(kRoom)), (QList<qint64>{3}));
        QSignalSpy changed(&p.local, &RoundtableModel::roomChanged);
        p.local.pruneEarlierTurns();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), kRoom);
        p.local.pruneEarlierTurns();
        QCOMPARE(changed.size(), 1); // nothing more to drop
        QCOMPARE(earlierIds(p.remote.room(kRoom)), (QList<qint64>{3}));

        // The timer does it unasked once the last one ages out: half a
        // second before turn 3 leaves, on a clock that runs.
        QElapsedTimer running;
        running.start();
        const qint64 base = 125LL * kMinute - 500;
        RoundtableModel::setClockForTest([&running, base] { return base + running.elapsed(); });
        p.local.pruneEarlierTurns(); // reschedules from the running clock
        changed.clear();
        QVERIFY(!earlierIds(p.local.room(kRoom)).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(changed.size() >= 1, 4000);
        QVERIFY(earlierIds(p.local.room(kRoom)).isEmpty());
        QVERIFY(earlierIds(p.remote.room(kRoom)).isEmpty());
        RoundtableModel::setClockForTest([] { return qint64(2000); });
    }
};

QTEST_MAIN(TstRoundtableModel)
#include "tst_roundtable_model.moc"
