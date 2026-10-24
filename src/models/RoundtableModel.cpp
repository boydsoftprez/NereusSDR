// no-port-check: NereusSDR-original.
// =================================================================
// src/models/RoundtableModel.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableModel.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 11).
//   2026-10-24: Roundtable plan Task 12: available(), muteList(), the
//               Core's Roundtable settings and settingChanged. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-24: Roundtable plan Task 13: earlier turns of the last 30
//               minutes only, pruned on a timer, and the test clock.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "models/RoundtableModel.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/roundtable/RoundtableState.h"
#include "core/roundtable/RoundtableWire.h"
#include "core/session/IStationLink.h"
#include "core/session/RecordStream.h"

#include <QDateTime>
#include <QJsonValue>

#include <algorithm>

namespace NereusSDR {

namespace {

// The record's kind travels as `record`: a turn has a `kind` of its own.
const QString kRecordKey = QStringLiteral("record");

QJsonObject without(QJsonObject object, const QString& key)
{
    object.remove(key);
    return object;
}

qint64 idOf(const QJsonObject& object)
{
    return object.value(QStringLiteral("id")).toInteger();
}

// A turn's names by count, highest first, then by name.
QJsonObject withSortedNames(QJsonObject turn)
{
    QList<QJsonObject> names;
    for (const QJsonValue& value : turn.value(QStringLiteral("names")).toArray()) {
        names.append(value.toObject());
    }
    std::sort(names.begin(), names.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const qint64 ca = a.value(QStringLiteral("count")).toInteger();
        const qint64 cb = b.value(QStringLiteral("count")).toInteger();
        if (ca != cb) {
            return ca > cb;
        }
        return a.value(QStringLiteral("name")).toString() < b.value(QStringLiteral("name")).toString();
    });
    QJsonArray sorted;
    for (const QJsonObject& name : names) {
        sorted.append(name);
    }
    turn.insert(QStringLiteral("names"), sorted);
    return turn;
}

std::function<qint64()>& testClock()
{
    static std::function<qint64()> clock;
    return clock;
}

bool isOpen(const QJsonObject& turn)
{
    // An open turn's end is null (or left out on the way).
    const QJsonValue end = turn.value(QStringLiteral("end"));
    return end.isNull() || end.isUndefined();
}

bool endedBefore(const QJsonObject& turn, qint64 cutoffMs)
{
    return !isOpen(turn) && turn.value(QStringLiteral("end")).toInteger() < cutoffMs;
}

} // namespace

qint64 RoundtableModel::nowMs()
{
    const std::function<qint64()>& clock = testClock();
    return clock ? clock() : QDateTime::currentMSecsSinceEpoch();
}

void RoundtableModel::setClockForTest(std::function<qint64()> clock)
{
    testClock() = std::move(clock);
}

QString RoundtableModel::roomStreamName(const QString& roomId)
{
    return QLatin1String(kRoomStreamPrefix) + roomId;
}

QString RoundtableModel::roomIdOfStream(const QString& stream)
{
    if (!stream.startsWith(QLatin1String(kRoomStreamPrefix))) {
        return {};
    }
    return stream.mid(static_cast<qsizetype>(qstrlen(kRoomStreamPrefix)));
}

QHash<QString, QJsonObject> RoundtableModel::roomsRecords(const QJsonArray& rooms)
{
    QHash<QString, QJsonObject> records;
    for (const QJsonValue& value : rooms) {
        QJsonObject fields = value.toObject();
        const QString id = fields.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        fields.insert(kRecordKey, QStringLiteral("room"));
        records.insert(id, fields);
    }
    return records;
}

QHash<QString, QJsonObject> RoundtableModel::roomRecords(const QJsonObject& room)
{
    QHash<QString, QJsonObject> records;
    const QJsonArray members = room.value(QStringLiteral("members")).toArray();
    for (qsizetype i = 0; i < members.size(); ++i) {
        const QJsonObject member = members.at(i).toObject();
        const QString ref = member.value(QStringLiteral("ref")).toString();
        if (ref.isEmpty()) {
            continue;
        }
        QJsonObject fields = without(member, QStringLiteral("reading"));
        fields.insert(kRecordKey, QStringLiteral("member"));
        fields.insert(QStringLiteral("seq"), static_cast<qint64>(i));
        records.insert(QStringLiteral("member:") + ref, fields);
        const QJsonValue reading = member.value(QStringLiteral("reading"));
        if (reading.isObject()) {
            QJsonObject readingFields = reading.toObject();
            readingFields.insert(kRecordKey, QStringLiteral("reading"));
            readingFields.insert(QStringLiteral("ref"), ref);
            records.insert(QStringLiteral("reading:") + ref, readingFields);
        }
    }
    const auto addTurn = [&records](const QJsonObject& turn) {
        const qint64 id = idOf(turn);
        if (id <= 0) {
            return;
        }
        QJsonObject fields = without(turn, QStringLiteral("names"));
        fields.insert(kRecordKey, QStringLiteral("turn"));
        records.insert(QStringLiteral("turn:") + QString::number(id), fields);
        for (const QJsonValue& value : turn.value(QStringLiteral("names")).toArray()) {
            const QJsonObject name = value.toObject();
            const QString callsign = name.value(QStringLiteral("name")).toString();
            if (callsign.isEmpty()) {
                continue;
            }
            records.insert(QStringLiteral("name:%1:%2").arg(id).arg(callsign),
                           QJsonObject{{kRecordKey, QStringLiteral("name")},
                                       {QStringLiteral("turn"), id},
                                       {QStringLiteral("name"), callsign},
                                       {QStringLiteral("count"), name.value(QStringLiteral("count"))}});
        }
    };
    for (const QJsonValue& value : room.value(QStringLiteral("now")).toArray()) {
        addTurn(value.toObject());
    }
    for (const QJsonValue& value : room.value(QStringLiteral("earlier")).toArray()) {
        addTurn(value.toObject());
    }
    for (const QJsonValue& value : room.value(QStringLiteral("messages")).toArray()) {
        QJsonObject fields = value.toObject();
        const qint64 id = idOf(fields);
        if (id <= 0) {
            continue;
        }
        fields.insert(kRecordKey, QStringLiteral("message"));
        records.insert(QStringLiteral("message:") + QString::number(id), fields);
    }
    return records;
}

QStringList RoundtableModel::stationSettingNames()
{
    return {QStringLiteral("AutoStart"), QStringLiteral("AwayChoice"),
            QStringLiteral("HighlightMentions"), QStringLiteral("Callsign"),
            QStringLiteral("GridSquare")};
}

QString RoundtableModel::stationSettingDefault(const QString& name)
{
    // The defaults SpotSourceHost and RoundtableClient read (design C15).
    if (name == QLatin1String("AutoStart") || name == QLatin1String("HighlightMentions")) {
        return QStringLiteral("True");
    }
    if (name == QLatin1String("AwayChoice")) {
        return QStringLiteral("Report");
    }
    return {};
}

RoundtableModel::RoundtableModel(QObject* parent)
    : QObject(parent)
{
    // Every placement's own writes, and on a Core a device's write applied
    // through the settings proxy, land here.
    m_settingsObserver = AppSettings::instance().addWriteObserver([this](const QString& key) {
        noteSettingChanged(key);
    });
    m_pruneTimer.setSingleShot(true);
    connect(&m_pruneTimer, &QTimer::timeout, this, &RoundtableModel::pruneEarlierTurns);
}

RoundtableModel::~RoundtableModel()
{
    if (m_settingsObserver >= 0) {
        AppSettings::instance().removeWriteObserver(m_settingsObserver);
    }
}

void RoundtableModel::detach()
{
    for (const QMetaObject::Connection& connection : std::as_const(m_connections)) {
        disconnect(connection);
    }
    m_connections.clear();
    m_client = nullptr;
    m_state = nullptr;
    m_link = nullptr;
    const bool hadRooms = !m_summaries.isEmpty();
    const QStringList viewed = viewedRoomIds();
    m_summaries.clear();
    m_views.clear();
    m_records.clear();
    m_readingRoomId.clear();
    if (hadRooms) {
        emit roomsChanged();
    }
    for (const QString& id : viewed) {
        emit roomChanged(id);
    }
}

void RoundtableModel::attachLocal(RoundtableClient* client, RoundtableState* state)
{
    detach();
    m_client = client;
    if (client != nullptr) {
        m_connections.append(connect(client, &RoundtableClient::roomsReceived, this,
                                     &RoundtableModel::applyRooms));
        m_connections.append(connect(client, &RoundtableClient::roomsDeltaReceived, this,
                                     &RoundtableModel::applyRoomsDelta));
        m_connections.append(connect(client, &RoundtableClient::roomReceived, this,
                                     &RoundtableModel::applyRoom));
        m_connections.append(connect(client, &RoundtableClient::roomDeltaReceived, this,
                                     &RoundtableModel::applyRoomDelta));
        m_connections.append(connect(client, &RoundtableClient::postResult, this,
                                     &RoundtableModel::postResult));
        m_connections.append(connect(client, &RoundtableClient::webLinkReady, this,
                                     &RoundtableModel::webLinkReady));
        m_connections.append(connect(client, &RoundtableClient::errorReceived, this,
                                     [this](const QString& code, const QString& reason) {
            // The archive's own failures (the key, the server out of
            // reach) answer the web sign-in or archive request.
            if (code == QLatin1String("no-key") || code == QLatin1String("unreachable")) {
                refuse(QStringLiteral("webSignIn"), reason);
            }
        }));
    }
    connectState(state);
}

void RoundtableModel::attachStation(IStationLink* link, RoundtableState* state)
{
    detach();
    m_link = link;
    connectState(state);
}

void RoundtableModel::connectState(RoundtableState* state)
{
    m_state = state;
    if (state != nullptr) {
        m_connections.append(connect(state, &RoundtableState::stateChanged, this,
                                     &RoundtableModel::onStateChanged));
    }
    onStateChanged();
}

void RoundtableModel::onStateChanged()
{
    const LinkState link = linkState();
    const qint64 since = linkSince();
    const bool firstRun = firstRunAnswered();
    const bool isHidden = hidden();
    const QString mentions = m_state ? m_state->mentionCounts() : QString();
    const QString sliceRooms = m_state ? m_state->sliceRooms() : QString();
    if (link != m_seenLinkState || since != m_seenLinkSince || isHidden != m_seenHidden) {
        m_seenLinkState = link;
        m_seenLinkSince = since;
        m_seenHidden = isHidden;
        emit linkChanged();
    }
    if (firstRun != m_seenFirstRun) {
        m_seenFirstRun = firstRun;
        emit firstRunChanged();
    }
    if (mentions != m_seenMentions) {
        m_seenMentions = mentions;
        emit mentionsChanged();
    }
    if (sliceRooms != m_seenSliceRooms) {
        m_seenSliceRooms = sliceRooms;
        emit sliceRoomsChanged();
    }
}

// ---------------------------------------------------------------- views

void RoundtableModel::applyRooms(const QJsonObject& full)
{
    m_summaries.clear();
    for (const QJsonValue& value : full.value(QStringLiteral("rooms")).toArray()) {
        const QJsonObject summary = value.toObject();
        const QString id = summary.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            m_summaries.insert(id, summary);
        }
    }
    pushRoomsToState();
    emit roomsChanged();
}

void RoundtableModel::applyRoomsDelta(const QJsonObject& delta)
{
    for (const QString& key : {QStringLiteral("add"), QStringLiteral("change")}) {
        for (const QJsonValue& value : delta.value(key).toArray()) {
            const QJsonObject summary = value.toObject();
            const QString id = summary.value(QStringLiteral("id")).toString();
            if (!id.isEmpty()) {
                m_summaries.insert(id, summary);
            }
        }
    }
    for (const QJsonValue& value : delta.value(QStringLiteral("remove")).toArray()) {
        m_summaries.remove(value.toString());
    }
    pushRoomsToState();
    emit roomsChanged();
}

void RoundtableModel::upsertMember(RoomView& view, const QJsonObject& member)
{
    const QString ref = member.value(QStringLiteral("ref")).toString();
    if (ref.isEmpty()) {
        return;
    }
    QJsonObject stored = member;
    const auto held = view.members.constFind(ref);
    // A row keeps its place in joining order while it stays (6.8).
    stored.insert(QStringLiteral("seq"), held != view.members.cend()
                                             ? held->value(QStringLiteral("seq")).toInteger()
                                             : view.nextSeq++);
    view.members.insert(ref, stored);
}

void RoundtableModel::trimMessages(RoomView& view)
{
    if (view.messages.size() <= kRoomMessages) {
        return;
    }
    QList<qint64> ids = view.messages.keys();
    std::sort(ids.begin(), ids.end());
    for (qsizetype i = 0; i < ids.size() - kRoomMessages; ++i) {
        view.messages.remove(ids.at(i));
    }
}

void RoundtableModel::applyRoom(const QJsonObject& full)
{
    const QString id = full.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        return;
    }
    RoomView view;
    for (const QJsonValue& value : full.value(QStringLiteral("members")).toArray()) {
        upsertMember(view, value.toObject());
    }
    for (const QString& key : {QStringLiteral("now"), QStringLiteral("earlier")}) {
        for (const QJsonValue& value : full.value(key).toArray()) {
            const QJsonObject turn = value.toObject();
            if (idOf(turn) > 0) {
                view.turns.insert(idOf(turn), turn);
            }
        }
    }
    for (const QJsonValue& value : full.value(QStringLiteral("messages")).toArray()) {
        const QJsonObject message = value.toObject();
        if (idOf(message) > 0) {
            view.messages.insert(idOf(message), message);
        }
    }
    trimMessages(view);
    pruneView(view, nowMs() - kEarlierWindowMs);
    m_views.insert(id, view);
    schedulePrune();
    emit roomChanged(id);
}

void RoundtableModel::applyRoomDelta(const QJsonObject& delta)
{
    const QString id = delta.value(QStringLiteral("room")).toString();
    const auto it = m_views.find(id);
    if (it == m_views.end()) {
        return; // the client asks again for a view it lost
    }
    if (delta.value(QStringLiteral("closed")).toBool()) {
        m_views.erase(it);
        if (m_readingRoomId == id) {
            m_readingRoomId.clear();
            emit readingRoomChanged();
        }
        emit roomChanged(id);
        return;
    }
    RoomView& view = it.value();
    for (const QJsonValue& value : delta.value(QStringLiteral("members")).toArray()) {
        upsertMember(view, value.toObject());
    }
    for (const QJsonValue& value : delta.value(QStringLiteral("left")).toArray()) {
        view.members.remove(value.toString());
    }
    for (const QJsonValue& value : delta.value(QStringLiteral("turns")).toArray()) {
        const QJsonObject turn = value.toObject();
        if (idOf(turn) > 0) {
            view.turns.insert(idOf(turn), turn);
        }
    }
    for (const QJsonValue& value : delta.value(QStringLiteral("messages")).toArray()) {
        const QJsonObject message = value.toObject();
        if (idOf(message) > 0) {
            view.messages.insert(idOf(message), message);
        }
    }
    trimMessages(view);
    pruneView(view, nowMs() - kEarlierWindowMs);
    schedulePrune();
    emit roomChanged(id);
}

void RoundtableModel::pushRoomsToState()
{
    // Only the Core's own state places its slices in rooms.
    if (m_client && m_state && m_state->isBound()) {
        m_state->setRooms(rooms());
    }
}

// ---------------------------------------------------------------- streams

bool RoundtableModel::applyRecordBatch(const RecordBatch& batch)
{
    const bool roomsStream = batch.stream == QLatin1String(kRoomsStream);
    const QString roomId = roomIdOfStream(batch.stream);
    if (!roomsStream && roomId.isEmpty()) {
        return false;
    }
    QHash<QString, QJsonObject>& records = m_records[batch.stream];
    if (batch.reset) {
        records.clear();
    }
    for (const RecordUpsert& upsert : batch.upserts) {
        records.insert(upsert.id, upsert.fields);
    }
    for (const QString& id : batch.removes) {
        records.remove(id);
    }
    if (roomsStream) {
        rebuildRoomsFromRecords();
    } else {
        rebuildRoomFromRecords(roomId);
    }
    return true;
}

void RoundtableModel::rebuildRoomsFromRecords()
{
    m_summaries.clear();
    const QHash<QString, QJsonObject> records = m_records.value(QLatin1String(kRoomsStream));
    for (auto it = records.cbegin(); it != records.cend(); ++it) {
        m_summaries.insert(it.key(), without(it.value(), kRecordKey));
    }
    emit roomsChanged();
}

void RoundtableModel::rebuildRoomFromRecords(const QString& roomId)
{
    const QHash<QString, QJsonObject> records = m_records.value(roomStreamName(roomId));
    if (records.isEmpty()) {
        if (m_views.remove(roomId) > 0) {
            emit roomChanged(roomId);
        }
        return;
    }
    RoomView view;
    QHash<QString, QJsonObject> readings;
    QHash<qint64, QJsonArray> names;
    for (auto it = records.cbegin(); it != records.cend(); ++it) {
        const QJsonObject& fields = it.value();
        const QString kind = fields.value(kRecordKey).toString();
        if (kind == QLatin1String("member")) {
            QJsonObject member = without(fields, kRecordKey);
            view.members.insert(member.value(QStringLiteral("ref")).toString(), member);
        } else if (kind == QLatin1String("reading")) {
            readings.insert(fields.value(QStringLiteral("ref")).toString(),
                            without(without(fields, kRecordKey), QStringLiteral("ref")));
        } else if (kind == QLatin1String("turn")) {
            const QJsonObject turn = without(fields, kRecordKey);
            view.turns.insert(idOf(turn), turn);
        } else if (kind == QLatin1String("name")) {
            names[fields.value(QStringLiteral("turn")).toInteger()].append(
                QJsonObject{{QStringLiteral("name"), fields.value(QStringLiteral("name"))},
                            {QStringLiteral("count"), fields.value(QStringLiteral("count"))}});
        } else if (kind == QLatin1String("message")) {
            const QJsonObject message = without(fields, kRecordKey);
            view.messages.insert(idOf(message), message);
        }
    }
    for (auto it = view.members.begin(); it != view.members.end(); ++it) {
        const auto reading = readings.constFind(it.key());
        it->insert(QStringLiteral("reading"), reading != readings.cend()
                                                  ? QJsonValue(reading.value())
                                                  : QJsonValue(QJsonValue::Null));
    }
    for (auto it = view.turns.begin(); it != view.turns.end(); ++it) {
        it->insert(QStringLiteral("names"), names.value(it.key()));
    }
    pruneView(view, nowMs() - kEarlierWindowMs);
    m_views.insert(roomId, view);
    schedulePrune();
    emit roomChanged(roomId);
}

void RoundtableModel::applyPostResult(const QString& clientId, bool ok,
                                      const QString& messageIdOrReason)
{
    emit postResult(clientId, ok, messageIdOrReason);
}

void RoundtableModel::applyWebLink(const QUrl& url)
{
    emit webLinkReady(url);
}

void RoundtableModel::applyRefusal(const QString& verb, const QString& reason)
{
    if (verb == QLatin1String("read") && !m_readingRoomId.isEmpty()) {
        m_readingRoomId.clear();
        emit readingRoomChanged();
    }
    emit actionRefused(verb, reason);
}

void RoundtableModel::clearStationRooms()
{
    const bool hadRooms = !m_summaries.isEmpty();
    const QStringList viewed = viewedRoomIds();
    m_records.clear();
    m_summaries.clear();
    m_views.clear();
    if (hadRooms) {
        emit roomsChanged();
    }
    for (const QString& id : viewed) {
        emit roomChanged(id);
    }
}

// ---------------------------------------------------------------- readers

QJsonArray RoundtableModel::rooms() const
{
    QList<QJsonObject> list = m_summaries.values();
    std::sort(list.begin(), list.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const qint64 ha = a.value(QStringLiteral("hz")).toInteger();
        const qint64 hb = b.value(QStringLiteral("hz")).toInteger();
        if (ha != hb) {
            return ha < hb;
        }
        return a.value(QStringLiteral("id")).toString() < b.value(QStringLiteral("id")).toString();
    });
    QJsonArray out;
    for (const QJsonObject& summary : list) {
        out.append(summary);
    }
    return out;
}

QJsonObject RoundtableModel::room(const QString& roomId) const
{
    const auto it = m_views.constFind(roomId);
    if (it == m_views.cend()) {
        return {};
    }
    const RoomView& view = it.value();
    // The head comes from the id (6.3), so both placements agree.
    const qsizetype dash = roomId.indexOf(QLatin1Char('-'));
    const QString family = dash > 0 ? roomId.left(dash) : QString();
    const qint64 hz = dash > 0 ? roomId.mid(dash + 1).toLongLong() : 0;

    QList<QJsonObject> members = view.members.values();
    std::sort(members.begin(), members.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a.value(QStringLiteral("seq")).toInteger() < b.value(QStringLiteral("seq")).toInteger();
    });
    QJsonArray memberArray;
    for (const QJsonObject& member : members) {
        memberArray.append(without(member, QStringLiteral("seq")));
    }

    QList<QJsonObject> open;
    QList<QJsonObject> ended;
    // Wire spec 5.4.6: only the turns that ended in the last 30 minutes,
    // even between the prune timer's runs.
    const qint64 cutoff = nowMs() - kEarlierWindowMs;
    for (const QJsonObject& turn : view.turns) {
        if (isOpen(turn)) {
            open.append(turn);
        } else if (!endedBefore(turn, cutoff)) {
            ended.append(turn);
        }
    }
    std::sort(open.begin(), open.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const qint64 sa = a.value(QStringLiteral("start")).toInteger();
        const qint64 sb = b.value(QStringLiteral("start")).toInteger();
        return sa != sb ? sa < sb : idOf(a) < idOf(b);
    });
    std::sort(ended.begin(), ended.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const qint64 ea = a.value(QStringLiteral("end")).toInteger();
        const qint64 eb = b.value(QStringLiteral("end")).toInteger();
        return ea != eb ? ea > eb : idOf(a) > idOf(b);
    });
    QJsonArray now;
    for (const QJsonObject& turn : open) {
        now.append(withSortedNames(turn));
    }
    QJsonArray earlier;
    for (const QJsonObject& turn : ended) {
        earlier.append(withSortedNames(turn));
    }

    QList<qint64> messageIds = view.messages.keys();
    std::sort(messageIds.begin(), messageIds.end());
    QJsonArray messages;
    for (const qint64 id : messageIds) {
        messages.append(view.messages.value(id));
    }

    return QJsonObject{{QStringLiteral("id"), roomId},
                       {QStringLiteral("band"), RoundtableWire::bandOf(hz)},
                       {QStringLiteral("family"), family},
                       {QStringLiteral("hz"), hz},
                       {QStringLiteral("members"), memberArray},
                       {QStringLiteral("now"), now},
                       {QStringLiteral("earlier"), earlier},
                       {QStringLiteral("messages"), messages}};
}

QStringList RoundtableModel::viewedRoomIds() const
{
    QStringList ids = m_views.keys();
    std::sort(ids.begin(), ids.end());
    return ids;
}

QString RoundtableModel::sliceRoomId(int sliceId) const
{
    return m_state ? m_state->sliceRoomId(sliceId) : QString();
}

RoundtableModel::LinkState RoundtableModel::linkState() const
{
    return m_state ? m_state->linkState() : LinkState::Off;
}

qint64 RoundtableModel::linkSince() const
{
    return m_state ? m_state->linkSince() : 0;
}

bool RoundtableModel::firstRunAnswered() const
{
    return m_state && m_state->firstRunAnswered();
}

bool RoundtableModel::hidden() const
{
    return m_state && m_state->hidden();
}

int RoundtableModel::mentionCount(const QString& roomId) const
{
    return m_state ? m_state->mentionCount(roomId) : 0;
}

bool RoundtableModel::muted(const QString& callsign) const
{
    return m_state && m_state->muted(callsign);
}

QStringList RoundtableModel::muteList() const
{
    return m_state ? m_state->muteList() : QStringList();
}

bool RoundtableModel::available() const
{
    if (m_link != nullptr) {
        return m_link->roundtableAvailable();
    }
    return !m_client.isNull() && !m_state.isNull();
}

QString RoundtableModel::stationSetting(const QString& name) const
{
    return AppSettings::instance()
        .value(QStringLiteral("Roundtable/") + name, stationSettingDefault(name))
        .toString();
}

bool RoundtableModel::setStationSetting(const QString& name, const QString& value)
{
    if (!stationSettingNames().contains(name)) {
        qCWarning(lcDsp) << "Roundtable setting not written:" << name
                         << "changes only through Roundtable";
        return false;
    }
    auto& settings = AppSettings::instance();
    settings.setValue(QStringLiteral("Roundtable/") + name, value);
    settings.save();
    return true;
}

void RoundtableModel::noteSettingChanged(const QString& key)
{
    if (key.isEmpty() || key.startsWith(QLatin1String("Roundtable/"), Qt::CaseInsensitive)) {
        emit settingChanged(key);
    }
}

void RoundtableModel::setDraft(const QString& roomId, const QString& text)
{
    if (text.isEmpty()) {
        m_drafts.remove(roomId);
    } else {
        m_drafts.insert(roomId, text);
    }
}

// ---------------------------------------------------------------- actions

void RoundtableModel::refuse(const QString& verb, const QString& reason)
{
    emit actionRefused(verb, reason);
}

bool RoundtableModel::stationOutcome(const QString& verb, bool sent, const QString& reason)
{
    if (!sent) {
        refuse(verb, reason);
    }
    return sent;
}

namespace {

QString notRunningReason()
{
    return QStringLiteral("Roundtable is not running here.");
}

} // namespace

void RoundtableModel::post(const QString& roomId, const QString& text, const QString& clientId)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome =
            m_link->requestRoundtablePost(roomId, text, clientId);
        if (!outcome.sent) {
            emit postResult(clientId, false, outcome.reason);
        }
        return;
    }
    if (!m_client || !m_state) {
        emit postResult(clientId, false, notRunningReason());
        return;
    }
    // The client answers through postResult.
    if (!m_state->post(roomId, text, clientId, &reason)) {
        emit postResult(clientId, false, reason);
    }
}

void RoundtableModel::deleteMessage(const QString& messageId)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableDelete(messageId);
        stationOutcome(QStringLiteral("delete"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->deleteMessage(messageId, &reason)) {
        refuse(QStringLiteral("delete"), m_state ? reason : notRunningReason());
    }
}

void RoundtableModel::nameTurn(const QString& turnId, const QString& name)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableName(turnId, name);
        stationOutcome(QStringLiteral("name"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->nameTurn(turnId, name, &reason)) {
        refuse(QStringLiteral("name"), m_state ? reason : notRunningReason());
    }
}

void RoundtableModel::report(const QString& kind, const QString& target, const QString& reason)
{
    QString refusal;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome =
            m_link->requestRoundtableReport(kind, target, reason);
        stationOutcome(QStringLiteral("report"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->report(kind, target, reason, &refusal)) {
        refuse(QStringLiteral("report"), m_state ? refusal : notRunningReason());
    }
}

void RoundtableModel::readRoom(const QString& roomId)
{
    if (roomId == m_readingRoomId) {
        return;
    }
    if (!m_readingRoomId.isEmpty()) {
        unreadRoom();
    }
    QString reason;
    bool sent = false;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableRead(roomId);
        sent = stationOutcome(QStringLiteral("read"), outcome.sent, outcome.reason);
    } else if (m_state && m_state->readRoom(roomId, &reason)) {
        sent = true;
    } else {
        refuse(QStringLiteral("read"), m_state ? reason : notRunningReason());
    }
    if (sent) {
        m_readingRoomId = roomId;
        emit readingRoomChanged();
    }
}

void RoundtableModel::unreadRoom()
{
    if (m_readingRoomId.isEmpty()) {
        return;
    }
    const QString roomId = m_readingRoomId;
    m_readingRoomId.clear();
    if (m_link != nullptr) {
        m_link->requestRoundtableUnread(roomId);
    } else if (m_state) {
        m_state->unreadRoom(roomId);
    }
    emit readingRoomChanged();
}

void RoundtableModel::setHidden(bool isHidden)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableSetHidden(isHidden);
        stationOutcome(QStringLiteral("setHidden"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->setOperatorHidden(isHidden, &reason)) {
        refuse(QStringLiteral("setHidden"), m_state ? reason : notRunningReason());
    }
}

void RoundtableModel::answerFirstRun(bool live)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableAnswerFirstRun(live);
        stationOutcome(QStringLiteral("answerFirstRun"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->answerFirstRun(live, &reason)) {
        refuse(QStringLiteral("answerFirstRun"), m_state ? reason : notRunningReason());
    }
}

void RoundtableModel::mute(const QString& callsign)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableMute(callsign);
        stationOutcome(QStringLiteral("mute"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->mute(callsign, &reason)) {
        refuse(QStringLiteral("mute"), m_state ? reason : notRunningReason());
    }
}

void RoundtableModel::unmute(const QString& callsign)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableUnmute(callsign);
        stationOutcome(QStringLiteral("unmute"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->unmute(callsign, &reason)) {
        refuse(QStringLiteral("unmute"), m_state ? reason : notRunningReason());
    }
}

void RoundtableModel::markMentionRead(qint64 upToMs)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome =
            m_link->requestRoundtableMarkMentionRead(upToMs);
        stationOutcome(QStringLiteral("markMentionRead"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->markMentionRead(upToMs, &reason)) {
        refuse(QStringLiteral("markMentionRead"), m_state ? reason : notRunningReason());
    }
}

void RoundtableModel::requestWebLink()
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableWebSignIn();
        stationOutcome(QStringLiteral("webSignIn"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->requestWebLink(&reason)) {
        refuse(QStringLiteral("webSignIn"), m_state ? reason : notRunningReason());
    }
}

void RoundtableModel::deleteArchive(const QString& confirm)
{
    QString reason;
    if (m_link != nullptr) {
        const IStationLink::CommandOutcome outcome = m_link->requestRoundtableDeleteArchive(confirm);
        stationOutcome(QStringLiteral("deleteArchive"), outcome.sent, outcome.reason);
    } else if (!m_state || !m_state->deleteArchive(confirm, &reason)) {
        refuse(QStringLiteral("deleteArchive"), m_state ? reason : notRunningReason());
    }
}

// ---------------------------------------------------------------- earlier

bool RoundtableModel::pruneView(RoomView& view, qint64 cutoffMs)
{
    bool pruned = false;
    for (auto it = view.turns.begin(); it != view.turns.end();) {
        if (endedBefore(it.value(), cutoffMs)) {
            it = view.turns.erase(it);
            pruned = true;
        } else {
            ++it;
        }
    }
    return pruned;
}

void RoundtableModel::pruneEarlierTurns()
{
    const qint64 cutoff = nowMs() - kEarlierWindowMs;
    QStringList changed;
    for (auto it = m_views.begin(); it != m_views.end(); ++it) {
        if (pruneView(it.value(), cutoff)) {
            changed.append(it.key());
        }
    }
    schedulePrune();
    std::sort(changed.begin(), changed.end());
    for (const QString& id : std::as_const(changed)) {
        emit roomChanged(id);
    }
}

void RoundtableModel::schedulePrune()
{
    qint64 oldestEnd = -1;
    for (const RoomView& view : std::as_const(m_views)) {
        for (const QJsonObject& turn : view.turns) {
            if (isOpen(turn)) {
                continue;
            }
            const qint64 end = turn.value(QStringLiteral("end")).toInteger();
            if (oldestEnd < 0 || end < oldestEnd) {
                oldestEnd = end;
            }
        }
    }
    if (oldestEnd < 0) {
        m_pruneTimer.stop();
        return;
    }
    // A second past the moment the oldest leaves the window, at least a
    // second from now.
    const qint64 wait = std::clamp<qint64>(oldestEnd + kEarlierWindowMs - nowMs() + 1000, 1000,
                                           kEarlierWindowMs);
    m_pruneTimer.start(static_cast<int>(wait));
}

} // namespace NereusSDR
