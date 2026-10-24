// no-port-check: NereusSDR-original.
// =================================================================
// src/core/roundtable/RoundtableState.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableState.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 11).
// =================================================================

#include "core/roundtable/RoundtableState.h"

#include "core/AppSettings.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonValue>

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace NereusSDR {

namespace {

// Wire spec 6.1 (design C1, C25): how far either side of a room's center a
// slice of each family may be and still be in that room.
constexpr qint64 kSidebandWidthHz = 1500;
constexpr qint64 kAmWidthHz = 3000;
constexpr qint64 kFmWidthHz = 6000;
constexpr qint64 kCwWidthHz = 300;

qint64 familyWidthHz(const QString& family)
{
    if (family == QLatin1String("am")) {
        return kAmWidthHz;
    }
    if (family == QLatin1String("fm")) {
        return kFmWidthHz;
    }
    if (family == QLatin1String("cw")) {
        return kCwWidthHz;
    }
    return kSidebandWidthHz;  // lsb, usb, digl, digu, fdvl, fdvu
}

QString compact(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject parseObject(const QVariant& value)
{
    return QJsonDocument::fromJson(value.toString().toUtf8()).object();
}

// Wire spec 5.2: clientId, 1 to 64 bytes of printable ASCII.
bool isClientId(const QString& id)
{
    if (id.isEmpty() || id.size() > 64) {
        return false;
    }
    return std::all_of(id.cbegin(), id.cend(), [](QChar c) {
        return c.unicode() >= 0x21 && c.unicode() <= 0x7E;
    });
}

// Wire spec 5.2: an id is a whole number from 1 to 9007199254740991.
bool isWireId(const QString& text)
{
    bool ok = false;
    const qint64 id = text.toLongLong(&ok);
    return ok && id >= 1 && id <= 9007199254740991LL && QString::number(id) == text;
}

constexpr qint64 kMaxUnixMs = 9007199254740991LL;

} // namespace

RoundtableState::RoundtableState(QObject* parent)
    : QObject(parent)
{
}

RoundtableState::~RoundtableState()
{
    if (m_settings != nullptr && m_settingsObserver >= 0) {
        m_settings->removeWriteObserver(m_settingsObserver);
    }
}

QString RoundtableState::readOnlyReason()
{
    return QStringLiteral("The Core reports its Roundtable connection here. Use the Roundtable "
                          "controls to change it.");
}

QString RoundtableState::notConnectedReason()
{
    return QStringLiteral("Roundtable is not connected, so this can't be sent.");
}

QString RoundtableState::unreadableReason()
{
    return QStringLiteral("The Core could not read this request.");
}

QString RoundtableState::mentionCounts() const
{
    return compact(m_mentionCounts);
}

QString RoundtableState::sliceRooms() const
{
    return compact(m_sliceRooms);
}

int RoundtableState::mentionCount(const QString& roomId) const
{
    return m_mentionCounts.value(roomId).toInt();
}

QString RoundtableState::sliceRoomId(int sliceId) const
{
    return m_sliceRooms.value(QString::number(sliceId)).toString();
}

AppSettings* RoundtableState::settings() const
{
    return m_settings != nullptr ? m_settings : &AppSettings::instance();
}

QStringList RoundtableState::muteList() const
{
    if (isBound()) {
        return m_muteList;
    }
    // A window reads the Core's list through the settings it shares.
    QStringList list;
    for (const QString& part : settings()->value(QLatin1String(kMuteListKey)).toString()
                                   .split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString base = part.trimmed().toUpper();
        if (!base.isEmpty() && !list.contains(base)) {
            list.append(base);
        }
    }
    return list;
}

bool RoundtableState::muted(const QString& callsign) const
{
    const QString base = RoundtableWire::baseCallsign(callsign);
    return !base.isEmpty() && muteList().contains(base);
}

qint64 RoundtableState::now() const
{
    return m_clock ? m_clock() : QDateTime::currentMSecsSinceEpoch();
}

// ---------------------------------------------------------------- the Core

void RoundtableState::bindClient(RoundtableClient* client, AppSettings* settings)
{
    if (m_client) {
        disconnect(m_client, nullptr, this, nullptr);
    }
    if (m_settings != nullptr && m_settingsObserver >= 0) {
        m_settings->removeWriteObserver(m_settingsObserver);
        m_settingsObserver = -1;
    }
    m_client = client;
    m_settings = settings;
    if (client == nullptr) {
        return;
    }
    if (m_settings != nullptr) {
        // A window's settings write (Roundtable/Hidden and the rest) and
        // the Core's own both land here; the client follows either.
        m_settingsObserver = m_settings->addWriteObserver([this](const QString& key) {
            if (key.startsWith(QLatin1String("Roundtable/"))) {
                reloadSettings();
            }
        });
    }
    connect(client, &RoundtableClient::linkStateChanged, this,
            [this](RoundtableClient::LinkState state) { onLinkState(state); });
    connect(client, &RoundtableClient::mentionReceived, this,
            [this](const QJsonObject& mention) { onMention(mention); });
    m_linkState = client->linkState();
    reloadSettings();
}

void RoundtableState::reloadSettings()
{
    AppSettings* s = settings();
    const QString answer = s->value(QLatin1String(kFirstRunAnswerKey)).toString();
    const bool answered = answer == QLatin1String("Live") || answer == QLatin1String("Hidden");
    const bool hidden = s->value(QLatin1String(kHiddenKey), QStringLiteral("False")).toString()
        == QLatin1String("True");
    bool okSeen = false;
    const qint64 seen = s->value(QLatin1String(kMentionsSeenKey)).toString().toLongLong(&okSeen);
    QStringList mutes;
    for (const QString& part : s->value(QLatin1String(kMuteListKey)).toString()
                                   .split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString base = RoundtableWire::baseCallsign(part.trimmed());
        if (!base.isEmpty() && !mutes.contains(base)) {
            mutes.append(base);
        }
    }
    // Hidden before the answer, so a Core answered "Hidden" never opens a
    // station connection on the way (D25).
    if (m_client) {
        m_client->setHidden(hidden);
        m_client->setFirstRunAnswered(answered);
        if (okSeen && seen > 0) {
            m_client->markMentionsSeen(seen);
        }
    }
    const bool changed = answered != m_firstRunAnswered || hidden != m_hidden;
    m_firstRunAnswered = answered;
    m_hidden = hidden;
    m_muteList = mutes;
    m_mentionsSeenMs = okSeen ? std::max<qint64>(0, seen) : 0;
    const QJsonObject before = m_mentionCounts;
    recount();
    if (changed && before == m_mentionCounts) {
        emit stateChanged();
    }
}

void RoundtableState::onLinkState(LinkState state)
{
    if (state == m_linkState) {
        return;
    }
    m_linkState = state;
    m_linkSince = now();
    emit stateChanged();
}

void RoundtableState::onMention(const QJsonObject& mention)
{
    const QJsonObject message = mention.value(QStringLiteral("message")).toObject();
    const qint64 id = message.value(QStringLiteral("id")).toInteger();
    if (id <= 0) {
        return;
    }
    m_mentions.insert(id, Mention{mention.value(QStringLiteral("room")).toString(),
                                  message.value(QStringLiteral("t")).toInteger(),
                                  RoundtableWire::baseCallsign(
                                      message.value(QStringLiteral("callsign")).toString())});
    recount();
}

void RoundtableState::recount()
{
    // U6: a message naming this station counts until the read mark passes
    // it. A muted station's messages are folded away (11.1), so they do not
    // count either.
    QJsonObject counts;
    for (auto it = m_mentions.begin(); it != m_mentions.end();) {
        if (it->t <= m_mentionsSeenMs) {
            it = m_mentions.erase(it);
            continue;
        }
        if (!m_muteList.contains(it->sender) && !it->room.isEmpty()) {
            counts.insert(it->room, counts.value(it->room).toInt() + 1);
        }
        ++it;
    }
    if (counts != m_mentionCounts) {
        m_mentionCounts = counts;
        emit stateChanged();
    }
}

void RoundtableState::setSlices(const QList<RoundtableSliceState>& slices)
{
    m_slices = slices;
    replace();
}

void RoundtableState::setRooms(const QJsonArray& summaries)
{
    QHash<QString, Listed> rooms;
    for (const QJsonValue& value : summaries) {
        const QJsonObject summary = value.toObject();
        const QString id = summary.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        rooms.insert(id, Listed{summary.value(QStringLiteral("family")).toString(),
                                summary.value(QStringLiteral("hz")).toInteger()});
    }
    m_rooms = rooms;
    replace();
}

bool RoundtableState::isListedRoom(const QString& roomId) const
{
    return m_rooms.contains(roomId);
}

void RoundtableState::replace()
{
    // Wire spec 6.3: a slice is in the listed room of its family whose
    // center is nearest, within the family's width, the lower center on a
    // tie; it stays in its room while its family is unchanged and it is
    // still within the width. A slice in no listed room (a candidate, or
    // none) has no room id yet.
    QJsonObject next;
    for (const RoundtableSliceState& slice : std::as_const(m_slices)) {
        const QString family = RoundtableWire::familyOfMode(slice.mode);
        if (family.isEmpty()) {
            continue;
        }
        const qint64 width = familyWidthHz(family);
        const QString key = QString::number(slice.id);
        const QString previous = m_sliceRooms.value(key).toString();
        const auto held = m_rooms.constFind(previous);
        if (held != m_rooms.cend() && held->family == family
            && std::llabs(slice.hz - held->hz) <= width) {
            next.insert(key, previous);
            continue;
        }
        QString best;
        qint64 bestDistance = std::numeric_limits<qint64>::max();
        qint64 bestCenter = 0;
        for (auto it = m_rooms.cbegin(); it != m_rooms.cend(); ++it) {
            if (it->family != family) {
                continue;
            }
            const qint64 distance = std::llabs(slice.hz - it->hz);
            if (distance > width) {
                continue;
            }
            if (distance < bestDistance || (distance == bestDistance && it->hz < bestCenter)) {
                best = it.key();
                bestDistance = distance;
                bestCenter = it->hz;
            }
        }
        next.insert(key, best.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(best));
    }
    if (next != m_sliceRooms) {
        m_sliceRooms = next;
        emit stateChanged();
    }
}

bool RoundtableState::requireClient(QString* reason) const
{
    if (m_client) {
        return true;
    }
    if (reason != nullptr) {
        *reason = QStringLiteral("This Core does not run Roundtable.");
    }
    return false;
}

bool RoundtableState::requireLive(QString* reason) const
{
    if (!requireClient(reason)) {
        return false;
    }
    if (m_client->linkState() == LinkState::Live) {
        return true;
    }
    if (reason != nullptr) {
        *reason = notConnectedReason();
    }
    return false;
}

bool RoundtableState::post(const QString& roomId, const QString& text, const QString& clientId,
                           QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    if (roomId.isEmpty() || text.isEmpty() || !isClientId(clientId)) {
        if (reason != nullptr) {
            *reason = unreadableReason();
        }
        return false;
    }
    // The answer is the client's postResult, for this clientId.
    m_client->post(roomId, text, clientId);
    return true;
}

bool RoundtableState::deleteMessage(const QString& messageId, QString* reason)
{
    if (!isWireId(messageId)) {
        if (reason != nullptr) {
            *reason = unreadableReason();
        }
        return false;
    }
    if (!requireLive(reason)) {
        return false;
    }
    m_client->deleteMessage(messageId);
    return true;
}

bool RoundtableState::nameTurn(const QString& turnId, const QString& name, QString* reason)
{
    if (!isWireId(turnId) || !RoundtableWire::isCallsign(name.trimmed())) {
        if (reason != nullptr) {
            *reason = isWireId(turnId)
                ? QStringLiteral("Name a turn with the callsign you heard.")
                : unreadableReason();
        }
        return false;
    }
    if (!requireLive(reason)) {
        return false;
    }
    m_client->nameTurn(turnId, name);
    return true;
}

bool RoundtableState::report(const QString& kind, const QString& target, const QString& reason,
                             QString* refusal)
{
    bool readable = false;
    if (kind == QLatin1String("message") || kind == QLatin1String("turn")) {
        readable = isWireId(target);
    } else if (kind == QLatin1String("station")) {
        const qsizetype slash = target.indexOf(QLatin1Char('/'));
        readable = slash > 0 && slash + 1 < target.size();
    }
    if (!readable) {
        if (refusal != nullptr) {
            *refusal = unreadableReason();
        }
        return false;
    }
    if (!requireLive(refusal)) {
        return false;
    }
    m_client->report(kind, target, reason);
    return true;
}

bool RoundtableState::readRoom(const QString& roomId, QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    if (!isListedRoom(roomId)) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable has no room there right now.");
        }
        return false;
    }
    m_client->readRoom(roomId);
    return true;
}

void RoundtableState::unreadRoom(const QString& roomId)
{
    if (m_client) {
        m_client->unreadRoom(roomId);
    }
}

bool RoundtableState::setOperatorHidden(bool hidden, QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    AppSettings* s = settings();
    s->setValue(QLatin1String(kHiddenKey), hidden ? QStringLiteral("True") : QStringLiteral("False"));
    s->saveInBackground();
    // The write observer has fed the client; one without an observer
    // (settings not bound) is fed here.
    if (m_settingsObserver < 0) {
        reloadSettings();
    }
    return true;
}

bool RoundtableState::answerFirstRun(bool live, QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    if (m_firstRunAnswered) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Someone already answered the Roundtable question on this "
                                     "Core.");
        }
        return false;
    }
    AppSettings* s = settings();
    // Hidden first: a Core answered "Stay hidden" goes from read only to
    // hidden without a station connection in between (D25).
    s->setValue(QLatin1String(kHiddenKey), live ? QStringLiteral("False") : QStringLiteral("True"));
    s->setValue(QLatin1String(kFirstRunAnswerKey),
                live ? QStringLiteral("Live") : QStringLiteral("Hidden"));
    s->saveInBackground();
    if (m_settingsObserver < 0) {
        reloadSettings();
    }
    return true;
}

bool RoundtableState::mute(const QString& callsign, QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    const QString base = RoundtableWire::baseCallsign(callsign.trimmed());
    if (!RoundtableWire::isCallsign(callsign.trimmed()) || base.isEmpty()) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable can only mute a callsign.");
        }
        return false;
    }
    QStringList list = m_muteList;
    if (!list.contains(base)) {
        list.append(base);
        AppSettings* s = settings();
        s->setValue(QLatin1String(kMuteListKey), list.join(QLatin1Char(',')));
        s->saveInBackground();
        if (m_settingsObserver < 0) {
            reloadSettings();
        }
    }
    return true;
}

bool RoundtableState::unmute(const QString& callsign, QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    const QString base = RoundtableWire::baseCallsign(callsign.trimmed());
    if (!RoundtableWire::isCallsign(callsign.trimmed()) || base.isEmpty()) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable can only mute a callsign.");
        }
        return false;
    }
    QStringList list = m_muteList;
    if (list.removeAll(base) > 0) {
        AppSettings* s = settings();
        s->setValue(QLatin1String(kMuteListKey), list.join(QLatin1Char(',')));
        s->saveInBackground();
        if (m_settingsObserver < 0) {
            reloadSettings();
        }
    }
    return true;
}

bool RoundtableState::markMentionRead(qint64 upToMs, QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    if (upToMs < 0 || upToMs > kMaxUnixMs) {
        if (reason != nullptr) {
            *reason = unreadableReason();
        }
        return false;
    }
    // The mark only moves forward: read on one device is read on all (U6).
    if (upToMs > m_mentionsSeenMs) {
        AppSettings* s = settings();
        s->setValue(QLatin1String(kMentionsSeenKey), QString::number(upToMs));
        s->saveInBackground();
        if (m_settingsObserver < 0) {
            reloadSettings();
        }
    }
    return true;
}

bool RoundtableState::requestWebLink(QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    if (m_client->linkState() == LinkState::Off) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable is off on this Core.");
        }
        return false;
    }
    m_client->requestWebLink();
    return true;
}

bool RoundtableState::deleteArchive(const QString& confirm, QString* reason)
{
    if (!requireClient(reason)) {
        return false;
    }
    if (confirm != QLatin1String(RoundtableWire::kDeleteArchiveWords)) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Type delete my archive to confirm.");
        }
        return false;
    }
    if (m_client->linkState() == LinkState::Off) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Roundtable is off on this Core.");
        }
        return false;
    }
    m_client->deleteArchive();
    return true;
}

// ---------------------------------------------------------------- a window

bool RoundtableState::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    bool changed = false;
    if (propertyName == "linkState") {
        const int v = value.toInt();
        const LinkState next = v >= 0 && v <= static_cast<int>(LinkState::Hidden)
            ? static_cast<LinkState>(v) : LinkState::Off;
        changed = next != m_linkState;
        m_linkState = next;
    } else if (propertyName == "linkSince") {
        const qint64 next = value.toLongLong();
        changed = next != m_linkSince;
        m_linkSince = next;
    } else if (propertyName == "firstRunAnswered") {
        changed = value.toBool() != m_firstRunAnswered;
        m_firstRunAnswered = value.toBool();
    } else if (propertyName == "hidden") {
        changed = value.toBool() != m_hidden;
        m_hidden = value.toBool();
    } else if (propertyName == "mentionCounts") {
        const QJsonObject next = parseObject(value);
        changed = next != m_mentionCounts;
        m_mentionCounts = next;
    } else if (propertyName == "sliceRooms") {
        const QJsonObject next = parseObject(value);
        changed = next != m_sliceRooms;
        m_sliceRooms = next;
    } else {
        return false;
    }
    if (changed) {
        emit stateChanged();
    }
    return true;
}

void RoundtableState::clearStationValues()
{
    if (isBound()) {
        return;
    }
    const bool changed = m_linkState != LinkState::Off || m_linkSince != 0 || m_firstRunAnswered
        || m_hidden || !m_mentionCounts.isEmpty() || !m_sliceRooms.isEmpty();
    m_linkState = LinkState::Off;
    m_linkSince = 0;
    m_firstRunAnswered = false;
    m_hidden = false;
    m_mentionCounts = {};
    m_sliceRooms = {};
    if (changed) {
        emit stateChanged();
    }
}

} // namespace NereusSDR
