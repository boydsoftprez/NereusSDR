// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableTagLayout.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableTagLayout.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-03: Overlay redesign truth rules and structured card model
//               (design D1 to D9, D12, D13, D19). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-11-03: Verify live double members and share sample-independent room structure signatures.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-11-02: Passive active-frequency reports and authoritative combined
//               doubles. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-30: Approved frequency-first Roundtable desktop surfaces.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 14).
//   2026-10-24: Roundtable plan Task 15: your own turn is found from the
//               talkers and your row (roundtableTurnIsYours), not from
//               `mine`. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-25: Hidden shows as Hidden: the flag line's "Hidden" and the
//               dimmed marking, its color the workspace look's role (JJ's
//               answer of 2026-10-25). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "RoundtableTagLayout.h"

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableWire.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RoundtableModel.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// S9 is -73 dBm, 6 dB an S-unit (the mockup's sText).
constexpr double kS9Dbm = -73.0;
constexpr double kDbPerSUnit = 6.0;

// D3: how far from the dial a turn or a station still counts, inclusive.
constexpr qint64 kNearbyHz = 5000;
// D4: how long "Double ended 3s ago" stays.
constexpr qint64 kDoubleEndedNoteMs = 8000;
// D13: past this a reading is old.
constexpr qint64 kOldReadingMs = 20000;
// D11: the meter runs on the flag's S scale.
constexpr double kMeterFloorDbm = -121.0;
constexpr double kMeterSpanDb = 88.0;
// D12: a held peak falls this fast.
constexpr double kPeakFallDbPerSecond = 2.0;
// The tracker forgets everything past this many listeners. A lost tick comes
// straight back, so a long session cannot grow it without bound.
constexpr int kPeakTrackerMaxEntries = 1024;

// One tag on the pan before it is placed.
struct Visible {
    double x = 0.0;
    qint64 hz = 0;
    QString roomId;
    bool pinned = false;  // mine, active or mentionsMe: never folds
    bool active = false;
};

// A tag or a fold, centered on x.
struct Item {
    double x = 0.0;
    QStringList roomIds;
    bool folded = false;
};

// Whether a group's tags can sit side by side, each no further than one
// tag's width from its own frequency. With the tags `step` apart, tag k
// sits at start + k * step; the best start puts the largest move as small
// as it can be, half the spread of x_k - k * step.
bool slidesApart(const QList<Visible>& visible, const QList<int>& group, double step,
                 double tagWidth)
{
    double lowest = 0.0;
    double highest = 0.0;
    for (int k = 0; k < group.size(); ++k) {
        const double offset = visible.at(group.at(k)).x - k * step;
        lowest = k == 0 ? offset : std::min(lowest, offset);
        highest = k == 0 ? offset : std::max(highest, offset);
    }
    return (highest - lowest) / 2.0 <= tagWidth;
}

// Tags that would overlap their neighbor share a group, which reaches
// `reach` pixels at most. A group whose unpinned tags number two or more
// folds them when they cannot move aside, or always when `forceFold`.
QList<Item> groupTags(const QList<Visible>& visible, double tagWidth, double gap, double reach,
                      bool forceFold)
{
    const double step = tagWidth + gap;
    QList<QList<int>> groups;
    for (int i = 0; i < visible.size(); ++i) {
        if (!groups.isEmpty()) {
            const QList<int>& group = groups.last();
            const double fromLast = visible.at(i).x - visible.at(group.last()).x;
            const double fromFirst = visible.at(i).x - visible.at(group.first()).x;
            if (fromLast < step && fromFirst < reach) {
                groups.last().append(i);
                continue;
            }
        }
        groups.append(QList<int>{i});
    }

    QList<Item> items;
    for (const QList<int>& group : groups) {
        QList<int> unpinned;
        for (int index : group) {
            if (!visible.at(index).pinned) {
                unpinned.append(index);
            }
        }
        const bool fold = unpinned.size() >= 2
            && (forceFold || !slidesApart(visible, group, step, tagWidth));
        for (int index : group) {
            if (!fold || visible.at(index).pinned) {
                items.append(Item{visible.at(index).x, QStringList{visible.at(index).roomId}, false});
            }
        }
        if (fold) {
            Item folded;
            folded.folded = true;
            double sum = 0.0;
            for (int index : unpinned) {
                sum += visible.at(index).x;
                folded.roomIds.append(visible.at(index).roomId);
            }
            folded.x = sum / unpinned.size();
            items.append(folded);
        }
    }
    std::stable_sort(items.begin(), items.end(),
                     [](const Item& a, const Item& b) { return a.x < b.x; });
    return items;
}

// A stretch of the pan tags may use, its left and right edges.
struct Stretch {
    double left = 0.0;
    double right = 0.0;
    int capacity = 0;
};

// The pan less the keep-out stretches, each side `gap` from an edge or a
// keep-out, with how many tags each holds.
QList<Stretch> stretchesOf(double width, double tagWidth, double gap, QList<RoundtableKeepOut> keepOut)
{
    std::sort(keepOut.begin(), keepOut.end(),
              [](const RoundtableKeepOut& a, const RoundtableKeepOut& b) { return a.left < b.left; });
    QList<Stretch> out;
    double left = gap;
    const auto close = [&](double right) {
        if (right - left >= tagWidth) {
            const int capacity = static_cast<int>(std::floor((right - left + gap) / (tagWidth + gap)));
            out.append(Stretch{left, right, capacity});
        }
    };
    for (const RoundtableKeepOut& block : keepOut) {
        if (block.right <= 0.0 || block.left >= width || block.right <= block.left) {
            continue;
        }
        close(std::min(block.left - gap, width - gap));
        left = std::max(left, block.right + gap);
    }
    close(width - gap);
    return out;
}

int capacityOf(const QList<Stretch>& stretches)
{
    int total = 0;
    for (const Stretch& stretch : stretches) {
        total += stretch.capacity;
    }
    return total;
}

// The stretch nearest x.
qsizetype nearestStretch(const QList<Stretch>& stretches, double x)
{
    qsizetype best = 0;
    double bestDistance = -1.0;
    for (qsizetype k = 0; k < stretches.size(); ++k) {
        const Stretch& stretch = stretches.at(k);
        const double distance = x < stretch.left ? stretch.left - x : x > stretch.right ? x - stretch.right : 0.0;
        if (bestDistance < 0.0 || distance < bestDistance) {
            best = k;
            bestDistance = distance;
        }
    }
    return best;
}

RoundtableGlyph glyphOfState(const QString& state)
{
    if (state == QLatin1String("talking")) {
        return RoundtableGlyph::Talking;
    }
    if (state == QLatin1String("doubled")) {
        return RoundtableGlyph::Doubled;
    }
    if (state == QLatin1String("unknown")) {
        return RoundtableGlyph::Unknown;
    }
    return RoundtableGlyph::Quiet;
}

QString callsignOf(const QJsonValue& talker)
{
    // The summary lists callsigns; a room view lists {ref, callsign, nth}.
    return talker.isObject() ? talker.toObject().value(QStringLiteral("callsign")).toString()
                             : talker.toString();
}

// The room view's open turn that is yours, when you are transmitting there.
bool youAreTalking(const QJsonObject& room)
{
    for (const QJsonValue& value : room.value(QStringLiteral("now")).toArray()) {
        const QJsonObject turn = value.toObject();
        if (room.value(QStringLiteral("view")).toString() == QLatin1String("frequency")
            && turn.value(QStringLiteral("hz")).toInteger() != room.value(QStringLiteral("hz")).toInteger()) { continue; }
        if (roundtableTurnIsYours(room, turn, true)) {
            return true;
        }
    }
    return false;
}

QString labelOf(RoundtableGlyph glyph, const QJsonArray& talkers)
{
    switch (glyph) {
    case RoundtableGlyph::You:
        return QStringLiteral("You");
    case RoundtableGlyph::Talking:
        if (!talkers.isEmpty() && !callsignOf(talkers.first()).isEmpty()) {
            return callsignOf(talkers.first());
        }
        return QStringLiteral("Unknown");
    case RoundtableGlyph::Doubled:
        return QStringLiteral("Double");
    case RoundtableGlyph::Unknown:
        return QStringLiteral("Unknown");
    default:
        return QStringLiteral("Quiet");
    }
}

QString familyOfContext(const QString& context)
{
    return RoundtableModel::isFrequencyContext(context) ? context.mid(10).section(QLatin1Char('-'), 0, 0) : QString();
}

QString personName(const QJsonObject& person)
{
    QString name = person.value(QStringLiteral("callsign")).toString();
    const int nth = person.value(QStringLiteral("nth")).toInt(1);
    if (nth > 1) { name += QStringLiteral(" (%1)").arg(nth); }
    return name;
}

// How far a turn sits from `hz`.
qint64 turnDistance(const QJsonObject& view, const QJsonObject& turn, qint64 hz)
{
    const qint64 turnHz = turn.value(QStringLiteral("hz")).toInteger();
    // A legacy room's turns name no frequency: the room is the frequency.
    if (turnHz <= 0) { return 0; }
    if (turn.value(QStringLiteral("kind")).toString() == QLatin1String("doubled")) {
        // Doubles use the RF room center on the wire. An explicit
        // endpoint or a source-scoped receiver/turn association can
        // connect that authoritative overlap to this exact dial.
        const QString source = turn.value(QStringLiteral("sourceRoom")).toString();
        for (const QJsonValue& endpoint : turn.value(QStringLiteral("talkers")).toArray()) {
            const QJsonObject person = endpoint.toObject();
            if (person.value(QStringLiteral("sourceRoom")).toString() == source
                && person.value(QStringLiteral("hz")).toInteger() == hz) {
                return 0;
            }
        }
        for (const QJsonValue& value : view.value(QStringLiteral("members")).toArray()) {
            const QJsonObject member = value.toObject();
            if (member.value(QStringLiteral("sourceRoom")).toString() == source
                && member.value(QStringLiteral("hz")).toInteger() == hz
                && member.value(QStringLiteral("reading")).toObject().value(QStringLiteral("turn")) == turn.value(QStringLiteral("id"))) {
                return 0;
            }
        }
    }
    return std::llabs(turnHz - hz);
}

struct ChosenTurn {
    QJsonObject turn;
    bool historical = false;
};

// D3: the exact-Hz turn, else the nearest live turn within the window, and on
// a tie the one that started last. Only when no live turn is near, the turn
// that ended last. `scoped` views skip a turn with no source room: its turn id
// means nothing outside one.
ChosenTurn chooseTurn(const QJsonObject& view, qint64 hz, bool scoped)
{
    ChosenTurn chosen;
    qint64 bestDistance = -1;
    qint64 bestStart = 0;
    for (const QJsonValue& value : view.value(QStringLiteral("now")).toArray()) {
        const QJsonObject turn = value.toObject();
        if (turn.value(QStringLiteral("end")).isDouble()
            || (scoped && turn.value(QStringLiteral("sourceRoom")).toString().isEmpty())) {
            continue;
        }
        const qint64 distance = turnDistance(view, turn, hz);
        const qint64 start = turn.value(QStringLiteral("start")).toInteger();
        if (distance > kNearbyHz) { continue; }
        if (bestDistance < 0 || distance < bestDistance || (distance == bestDistance && start > bestStart)) {
            chosen.turn = turn;
            bestDistance = distance;
            bestStart = start;
        }
    }
    if (!chosen.turn.isEmpty()) { return chosen; }
    qint64 newest = -1;
    for (const QJsonValue& value : view.value(QStringLiteral("earlier")).toArray()) {
        const QJsonObject turn = value.toObject();
        if ((scoped && turn.value(QStringLiteral("sourceRoom")).toString().isEmpty())
            || turnDistance(view, turn, hz) > kNearbyHz) {
            continue;
        }
        const qint64 end = turn.value(QStringLiteral("end")).toInteger();
        if (end > newest) {
            chosen.turn = turn;
            chosen.historical = true;
            newest = end;
        }
    }
    return chosen;
}

// Who a turn's talkers are: a live double's current participants, or everyone
// for any other turn. `scoped` is false for a legacy room, whose talkers carry
// no source room to check against.
struct TurnPeople {
    QSet<QString> ids;
    QStringList names;
    bool complete = true;
};

TurnPeople peopleOf(const QJsonObject& view, const QJsonObject& turn, bool scoped)
{
    TurnPeople people;
    const QString source = turn.value(QStringLiteral("sourceRoom")).toString();
    const QJsonArray talkers = scoped ? roundtableTurnParticipants(view, turn, &people.complete)
                                      : turn.value(QStringLiteral("talkers")).toArray();
    for (const QJsonValue& value : talkers) {
        const QJsonObject talker = value.toObject();
        const QString ref = talker.value(QStringLiteral("ref")).toString();
        const QString name = personName(talker);
        const QString id = talker.value(QStringLiteral("sourceRoom")).toString(source) + QLatin1Char('/')
            + (ref.isEmpty() ? name : ref);
        if (!people.ids.contains(id)) {
            people.ids.insert(id);
            if (!name.isEmpty()) { people.names.append(name); }
        }
    }
    return people;
}

// The listeners with a usable reading of this turn, nearest first. A raw
// overlap sample describes combined RF, never one transmitter.
QList<QJsonObject> turnReporters(const QJsonObject& view, const QJsonObject& turn, const QString& family,
                                 const QSet<QString>& talkerIds, bool historical)
{
    const QString source = turn.value(QStringLiteral("sourceRoom")).toString();
    QList<QJsonObject> reporters;
    for (const QJsonValue& value : view.value(QStringLiteral("members")).toArray()) {
        const QJsonObject member = value.toObject();
        const QString memberSource = member.value(QStringLiteral("sourceRoom")).toString();
        const QString id = memberSource + QLatin1Char('/') + member.value(QStringLiteral("ref")).toString();
        const QJsonObject reading = member.value(QStringLiteral("reading")).toObject();
        const QJsonValue dbmField = reading.value(QStringLiteral("dbm"));
        const double dbm = dbmField.toDouble(-400);
        // Turn ids are local to a source room; they cannot cross that scope.
        if (memberSource == source && !talkerIds.contains(id) && (historical || !member.value(QStringLiteral("keyed")).toBool())
            && (!member.contains(QStringLiteral("family")) || member.value(QStringLiteral("family")).toString() == family)
            && reading.value(QStringLiteral("turn")) == turn.value(QStringLiteral("id"))
            && reading.value(QStringLiteral("present")).toBool() && dbmField.isDouble() && std::isfinite(dbm)
            && dbm >= RoundtableWire::kMinDbm && dbm <= RoundtableWire::kMaxDbm) {
            reporters.append(member);
        }
    }
    std::stable_sort(reporters.begin(), reporters.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const double da = a.value(QStringLiteral("km")).isDouble() ? a.value(QStringLiteral("km")).toDouble() : 1e20;
        const double db = b.value(QStringLiteral("km")).isDouble() ? b.value(QStringLiteral("km")).toDouble() : 1e20;
        return da < db;
    });
    return reporters;
}

// Every station in the band list, as the directory lists them.
QJsonArray directoryStations(const RoundtableModel* model)
{
    QJsonArray stations;
    for (const QJsonValue& room : model->rooms()) {
        for (const QJsonValue& station : room.toObject().value(QStringLiteral("stations")).toArray()) {
            stations.append(station);
        }
    }
    return stations;
}

// The keyed stations within the window, nearest first, each person once however
// many of their slices the directory lists.
QList<QJsonObject> keyedNear(const QJsonArray& stations, qint64 hz, const QString& family)
{
    QList<QJsonObject> keyed;
    QSet<QString> seen;
    for (const QJsonValue& value : stations) {
        const QJsonObject station = value.toObject();
        if (!station.value(QStringLiteral("keyed")).toBool()
            || station.value(QStringLiteral("family")).toString() != family
            || std::llabs(station.value(QStringLiteral("hz")).toInteger() - hz) > kNearbyHz) {
            continue;
        }
        const QString id = station.value(QStringLiteral("sourceRoom")).toString() + QLatin1Char('/')
            + station.value(QStringLiteral("ref")).toString();
        if (!seen.contains(id)) {
            seen.insert(id);
            keyed.append(station);
        }
    }
    std::stable_sort(keyed.begin(), keyed.end(), [hz](const QJsonObject& a, const QJsonObject& b) {
        return std::llabs(a.value(QStringLiteral("hz")).toInteger() - hz)
            < std::llabs(b.value(QStringLiteral("hz")).toInteger() - hz);
    });
    return keyed;
}

// D1's directory rule: the names of the first source room with two or more of
// these keyed stations. Empty when no room has two.
QStringList directoryDoubleNames(const QList<QJsonObject>& keyed)
{
    QHash<QString, QStringList> bySource;
    for (const QJsonObject& station : keyed) {
        const QString source = station.value(QStringLiteral("sourceRoom")).toString();
        if (source.isEmpty()) { continue; }
        QStringList& names = bySource[source];
        names.append(personName(station));
    }
    for (const QJsonObject& station : keyed) {
        const QStringList names = bySource.value(station.value(QStringLiteral("sourceRoom")).toString());
        if (names.size() >= 2) { return names; }
    }
    return {};
}

// The view only when it covers this family: another family's view says nothing
// about this frequency.
QJsonObject viewForFamily(const QJsonObject& view, const QString& family)
{
    return view.value(QStringLiteral("family")).toString() == family ? view : QJsonObject();
}

// Who is on a double, for the tag and the card: the open turn's current
// participants when the view knows, otherwise the directory's.
QStringList doubleNames(const QJsonObject& view, const QJsonArray& stations, qint64 hz, const QString& family)
{
    if (!view.isEmpty()) {
        const ChosenTurn chosen = chooseTurn(view, hz, true);
        return peopleOf(view, chosen.turn, true).names;
    }
    return directoryDoubleNames(keyedNear(stations, hz, family));
}

} // namespace

double roundtableMeterFraction(double dbm)
{
    return std::clamp((dbm - kMeterFloorDbm) / kMeterSpanDb, 0.0, 1.0);
}

double RoundtablePeakTracker::update(const QString& sourceRoom, const QString& ref, qint64 turnId, double dbm, qint64 nowMs)
{
    auto room = m_rooms.find(sourceRoom);
    if (room != m_rooms.end()) {
        const auto held = room.value().find(ref);
        if (held != room.value().end()) {
            Held& entry = held.value();
            if (entry.turnId == turnId) {
                const double elapsedSeconds = static_cast<double>(std::max<qint64>(0, nowMs - entry.atMs)) / 1000.0;
                entry.peakDbm = std::max(entry.peakDbm - kPeakFallDbPerSecond * elapsedSeconds, dbm);
            } else {
                entry.turnId = turnId;
                entry.peakDbm = dbm;
            }
            entry.atMs = nowMs;
            return entry.peakDbm;
        }
    }
    if (m_entries >= kPeakTrackerMaxEntries) {
        m_rooms.clear();
        m_entries = 0;
    }
    m_rooms[sourceRoom].insert(ref, Held{turnId, dbm, nowMs});
    ++m_entries;
    return dbm;
}

bool roundtableIsDouble(const QJsonObject& view, const QJsonArray& stations, qint64 hz, const QString& family)
{
    const QJsonObject known = viewForFamily(view, family);
    if (!known.isEmpty()) {
        const ChosenTurn chosen = chooseTurn(known, hz, true);
        return !chosen.turn.isEmpty() && !chosen.historical
            && chosen.turn.value(QStringLiteral("kind")).toString() == QLatin1String("doubled");
    }
    return !directoryDoubleNames(keyedNear(stations, hz, family)).isEmpty();
}

RoundtableCardModel roundtableCardModelFor(const RoundtableModel* model, const QList<int>& sliceIds,
                                           const QString& activeContext, quint64 hz, RoundtablePeakTracker* peaks)
{
    using Kind = RoundtableCardModel::Kind;
    using LinkState = RoundtableModel::LinkState;
    RoundtableCardModel card;
    card.hz = hz;
    if (model == nullptr) { return card; }
    const qint64 now = RoundtableModel::nowMs();
    const qint64 dial = static_cast<qint64>(hz);

    // D7: a dropped link freezes, and says which way it dropped.
    const LinkState link = model->linkState();
    card.frozen = roundtableTagDisplayFor(model) == RoundtableTagDisplay::Frozen;
    QString linkNote;
    if (link == LinkState::Reconnecting) {
        linkNote = QStringLiteral("Reconnecting");
    } else if (link == LinkState::Offline) {
        linkNote = model->linkSince() > 0
            ? QStringLiteral("Offline since %1 UTC")
                  .arg(QDateTime::fromMSecsSinceEpoch(model->linkSince(), QTimeZone::utc()).toString(QStringLiteral("HH:mm")))
            : QStringLiteral("Offline");
    } else if (link == LinkState::Hidden) {
        linkNote = QStringLiteral("You are hidden");
    }
    card.note = linkNote;

    // A frequency card reads its own context; a legacy room card reads the
    // room the first of this station's slices sits in.
    QString context = activeContext;
    for (int sliceId : sliceIds) {
        if (!context.isEmpty()) { break; }
        context = model->sliceRoomId(sliceId);
    }
    if (context.isEmpty()) { return card; }
    const bool scoped = RoundtableModel::isFrequencyContext(context);
    QJsonObject view = model->room(context);
    const QString family = scoped ? familyOfContext(context) : view.value(QStringLiteral("family")).toString();
    view = viewForFamily(view, family);
    const QJsonArray stations = scoped ? directoryStations(model) : QJsonArray();

    QSet<QString> seenPeople;
    for (const QJsonValue& value : stations) {
        const QJsonObject station = value.toObject();
        if (station.value(QStringLiteral("hz")).toInteger() != dial
            || station.value(QStringLiteral("family")).toString() != family) {
            continue;
        }
        const QString id = station.value(QStringLiteral("sourceRoom")).toString() + QLatin1Char('/')
            + station.value(QStringLiteral("ref")).toString();
        if (!seenPeople.contains(id)) {
            seenPeople.insert(id);
            card.people.append(personName(station));
        }
    }

    const ChosenTurn chosen = view.isEmpty() ? ChosenTurn() : chooseTurn(view, dial, scoped);
    if (chosen.turn.isEmpty()) {
        // Known only from the directory (D1): who is keyed, with no readings.
        if (view.isEmpty()) {
            const QList<QJsonObject> keyed = keyedNear(stations, dial, family);
            card.keyed = directoryDoubleNames(keyed);
            if (!card.keyed.isEmpty()) {
                card.kind = Kind::Double;
            } else if (!keyed.isEmpty()) {
                card.kind = keyed.first().value(QStringLiteral("you")).toBool() ? Kind::You : Kind::Talking;
                card.keyed = QStringList{personName(keyed.first())};
            }
        }
        return card;
    }

    const QJsonObject& turn = chosen.turn;
    const bool doubled = turn.value(QStringLiteral("kind")).toString() == QLatin1String("doubled");
    const TurnPeople people = peopleOf(view, turn, scoped);
    card.keyed = people.names;
    const qint64 start = turn.value(QStringLiteral("start")).toInteger();
    if (chosen.historical) {
        card.kind = doubled ? Kind::LastDouble : Kind::LastTalker;
        card.endedAgoMs = std::max<qint64>(0, now - turn.value(QStringLiteral("end")).toInteger());
    } else {
        if (doubled) {
            card.kind = Kind::Double;
        } else if (roundtableTurnIsYours(view, turn, true)) {
            card.kind = Kind::You;
        } else if (turn.value(QStringLiteral("kind")).toString() == QLatin1String("unknown") || people.names.isEmpty()) {
            card.kind = Kind::Unknown;
            card.keyed.clear();
        } else {
            card.kind = Kind::Talking;
        }
        // The clock stops where the link dropped: past that nothing is known.
        const qint64 clockEnd = card.frozen && model->linkSince() > 0 ? model->linkSince() : now;
        card.clockMs = std::max<qint64>(0, clockEnd - start);
    }

    // D4: the remaining talker's new turn says the double just ended. Its rows
    // come only from readings of this turn, so none carry over. A link note
    // takes the one line while the link is not live.
    if (!chosen.historical && !doubled && linkNote.isEmpty()) {
        qint64 newestEnd = -1;
        for (const QJsonValue& value : view.value(QStringLiteral("earlier")).toArray()) {
            const QJsonObject earlier = value.toObject();
            if (earlier.value(QStringLiteral("kind")).toString() == QLatin1String("doubled")
                && earlier.value(QStringLiteral("sourceRoom")) == turn.value(QStringLiteral("sourceRoom"))
                && turnDistance(view, earlier, dial) <= kNearbyHz) {
                newestEnd = std::max(newestEnd, earlier.value(QStringLiteral("end")).toInteger());
            }
        }
        const qint64 ago = std::max<qint64>(0, now - newestEnd);
        if (newestEnd >= 0 && ago < kDoubleEndedNoteMs) {
            card.note = QStringLiteral("Double ended %1s ago").arg(ago / 1000);
        }
    }

    // D2, D9, D12, D13: present listeners, never a transmitter; combined RF has
    // no path; a tick only on a fresh reading of a live turn on a live link.
    const qint64 turnId = turn.value(QStringLiteral("id")).toInteger();
    const bool ticks = peaks != nullptr && !chosen.historical && !card.frozen;
    for (const QJsonObject& member : turnReporters(view, turn, family, people.ids, chosen.historical)) {
        const QJsonObject reading = member.value(QStringLiteral("reading")).toObject();
        RoundtableCardRow row;
        row.call = personName(member);
        row.dbm = reading.value(QStringLiteral("dbm")).toDouble();
        row.ageMs = std::max<qint64>(0, now - reading.value(QStringLiteral("t")).toInteger());
        row.offsetHz = member.value(QStringLiteral("offsetHz")).toInt();
        row.you = member.value(QStringLiteral("you")).toBool();
        row.old = row.ageMs > kOldReadingMs;
        row.peakDbm = row.dbm;
        if (!doubled) {
            if (member.value(QStringLiteral("km")).isDouble()) {
                row.km = qRound(member.value(QStringLiteral("km")).toDouble());
            }
            if (member.value(QStringLiteral("bearingDeg")).isDouble()) {
                row.bearingDeg = member.value(QStringLiteral("bearingDeg")).toInt();
            }
        }
        if (ticks) {
            row.peakDbm = peaks->update(member.value(QStringLiteral("sourceRoom")).toString(),
                                        member.value(QStringLiteral("ref")).toString(), turnId, row.dbm, now);
            row.hasPeak = !row.old;
        }
        card.rows.append(row);
    }
    return card;
}

bool roundtableTurnIsYours(const QJsonObject& room, const QJsonObject& turn, bool open)
{
    if (turn.isEmpty() || turn.value(QStringLiteral("kind")).toString() == QLatin1String("doubled")) {
        return false;
    }
    const bool frequency = room.value(QStringLiteral("view")).toString() == QLatin1String("frequency");
    const QString roomId = room.value(QStringLiteral("id")).toString();
    const QString turnSource = turn.value(QStringLiteral("sourceRoom")).toString();
    bool legacyKeyed = false;
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        const QJsonObject you = value.toObject();
        const QString ref = you.value(QStringLiteral("ref")).toString();
        const QString source = you.value(QStringLiteral("sourceRoom")).toString();
        if (!you.value(QStringLiteral("you")).toBool() || ref.isEmpty()) { continue; }
        for (const QJsonValue& value : turn.value(QStringLiteral("talkers")).toArray()) {
            const QJsonObject talker = value.toObject();
            const QString talkerSource = talker.value(QStringLiteral("sourceRoom")).toString();
            if (talker.value(QStringLiteral("ref")).toString() == ref && talkerSource == source
                && (!frequency || !source.isEmpty())) {
                return true;
            }
        }
        // An old single RF room may not have named its on-air talker yet.
        // That fallback never crosses an explicit room boundary and is never
        // used by a neighborhood containing independent simultaneous turns.
        if (!frequency && (source.isEmpty() || source == roomId)
            && (turnSource.isEmpty() || turnSource == roomId) && you.value(QStringLiteral("keyed")).toBool()) {
            legacyKeyed = true;
        }
    }
    return open && legacyKeyed;
}

QList<RoundtableTagPlacement> layoutRoundtableTags(const QList<RoundtableTag>& tags, double hzPerPixel,
                                                   qint64 leftHz, double width, double tagWidth,
                                                   double gap)
{
    return layoutRoundtableTags(tags, hzPerPixel, leftHz, width, tagWidth, gap, {});
}

QList<RoundtableTagPlacement> layoutRoundtableTags(const QList<RoundtableTag>& tags, double hzPerPixel,
                                                   qint64 leftHz, double width, double tagWidth,
                                                   double gap, const QList<RoundtableKeepOut>& keepOut)
{
    QList<RoundtableTagPlacement> out;
    if (!(hzPerPixel > 0.0) || !(width > 0.0) || !(tagWidth > 0.0)) {
        return out;
    }
    gap = std::max(0.0, gap);

    QList<Visible> visible;
    for (const RoundtableTag& tag : tags) {
        const double x = static_cast<double>(tag.hz - leftHz) / hzPerPixel;
        if (x < 0.0 || x > width) {
            continue;
        }
        visible.append(Visible{x, tag.hz, tag.roomId, tag.mine || tag.active || tag.mentionsMe, tag.active});
    }
    std::stable_sort(visible.begin(), visible.end(), [](const Visible& a, const Visible& b) {
        return a.hz != b.hz ? a.hz < b.hz : a.roomId < b.roomId;
    });

    const QList<Stretch> stretches = stretchesOf(width, tagWidth, gap, keepOut);
    const int capacity = capacityOf(stretches);
    if (capacity <= 0) {
        return out;
    }

    // First only the groups that cannot move aside fold. If the tags still
    // don't fit across the pan, every overlapping group folds, and then the
    // groups reach further each round until they fit or span the pan.
    double reach = tagWidth + gap;
    bool forceFold = false;
    QList<Item> items;
    for (;;) {
        items = groupTags(visible, tagWidth, gap, reach, forceFold);
        if (items.size() <= capacity || reach > width) {
            break;
        }
        if (!forceFold) {
            forceFold = true;
        } else {
            reach *= 1.5;
        }
    }

    // A narrow free strip can hold fewer items than the pinned tags plus
    // folds. Discard unpinned items first so an earlier geometric fold cannot
    // consume the selected cochannel's only slot.
    const auto pinned = [&](const Item& item) {
        return std::any_of(visible.cbegin(), visible.cend(), [&](const Visible& tag) {
            return tag.pinned && item.roomIds.contains(tag.roomId);
        });
    };
    while (items.size() > capacity) {
        auto removable = std::find_if(items.rbegin(), items.rend(), [&](const Item& item) { return !pinned(item); });
        if (removable == items.rend()) {
            removable = std::find_if(items.rbegin(), items.rend(), [&](const Item& item) {
                return std::none_of(visible.cbegin(), visible.cend(), [&](const Visible& tag) {
                    return tag.active && item.roomIds.contains(tag.roomId);
                });
            });
        }
        if (removable == items.rend()) { break; }
        items.erase(std::next(removable).base());
    }

    // Each tag to the stretch nearest its frequency that still has room,
    // keeping their order: a stretch already passed is not gone back to.
    // Tags that find no room are left out (only when even the widest folds
    // do not fit, a narrow pan).
    QList<QList<qsizetype>> byStretch(stretches.size());
    qsizetype current = 0;
    for (qsizetype k = 0; k < items.size(); ++k) {
        qsizetype target = std::max(nearestStretch(stretches, items.at(k).x), current);
        while (target < stretches.size() && byStretch.at(target).size() >= stretches.at(target).capacity) {
            ++target;
        }
        if (target >= stretches.size()) {
            break;
        }
        byStretch[target].append(k);
        current = target;
    }

    // Within a stretch, side by side, as near their frequencies as the
    // space allows: pushed right off its left edge and their left
    // neighbors, then back left off its right edge and their right
    // neighbors.
    for (qsizetype s = 0; s < stretches.size(); ++s) {
        const QList<qsizetype>& members = byStretch.at(s);
        QList<double> lefts;
        double edge = stretches.at(s).left;
        for (qsizetype k : members) {
            const double left = std::max(items.at(k).x - tagWidth / 2.0, edge);
            lefts.append(left);
            edge = left + tagWidth + gap;
        }
        edge = stretches.at(s).right;
        for (qsizetype m = members.size() - 1; m >= 0; --m) {
            lefts[m] = std::min(lefts.at(m), edge - tagWidth);
            edge = lefts.at(m) - gap;
        }
        for (qsizetype m = 0; m < members.size(); ++m) {
            const Item& item = items.at(members.at(m));
            out.append(RoundtableTagPlacement{QRectF(lefts.at(m), 0.0, tagWidth, 0.0), item.roomIds,
                                              item.folded});
        }
    }
    return out;
}

// The mockup's sText: S1 to S9, then S9 +N.
QString roundtableSUnitText(double dbm)
{
    const int over = static_cast<int>(std::lround(dbm - kS9Dbm));
    if (over < 1) {
        const int units = static_cast<int>(std::lround(9.0 - (kS9Dbm - dbm) / kDbPerSUnit));
        return QStringLiteral("S%1").arg(std::clamp(units, 0, 9));
    }
    return QStringLiteral("S9 +%1").arg(over);
}

QString roundtableRoomStructureSignature(QJsonObject room)
{
    QJsonArray members;
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        QJsonObject member = value.toObject();
        if (member.value(QStringLiteral("reading")).isObject()) {
            QJsonObject reading = member.value(QStringLiteral("reading")).toObject();
            for (const QString& field : {QStringLiteral("dbm"), QStringLiteral("t"), QStringLiteral("km"), QStringLiteral("bearingDeg")}) {
                reading.remove(field);
            }
            member.insert(QStringLiteral("reading"), reading);
        }
        member.remove(QStringLiteral("km"));
        member.remove(QStringLiteral("bearingDeg"));
        members.append(member);
    }
    room.insert(QStringLiteral("members"), members);
    return QString::fromUtf8(QJsonDocument(room).toJson(QJsonDocument::Compact));
}

QJsonArray roundtableTurnParticipants(const QJsonObject& view, const QJsonObject& turn, bool* identitiesComplete)
{
    const QString source = turn.value(QStringLiteral("sourceRoom")).toString();
    const bool liveDouble = turn.value(QStringLiteral("kind")).toString() == QLatin1String("doubled")
        && !turn.value(QStringLiteral("end")).isDouble();
    bool complete = true;
    QJsonArray participants;
    for (const QJsonValue& value : turn.value(QStringLiteral("talkers")).toArray()) {
        const QJsonObject person = value.toObject();
        const QString personSource = person.value(QStringLiteral("sourceRoom")).toString(source);
        if (!source.isEmpty() && personSource != source) { complete = false; continue; }
        bool stillKeyed = !liveDouble;
        bool currentKnown = !liveDouble;
        const QString ref = person.value(QStringLiteral("ref")).toString();
        if (liveDouble && !source.isEmpty() && !ref.isEmpty()) {
            for (const QJsonValue& current : view.value(QStringLiteral("members")).toArray()) {
                const QJsonObject member = current.toObject();
                if (member.value(QStringLiteral("sourceRoom")).toString() == personSource
                    && member.value(QStringLiteral("ref")).isString()
                    && member.value(QStringLiteral("ref")).toString() == ref
                    && member.value(QStringLiteral("keyed")).isBool()) {
                    currentKnown = true;
                    stillKeyed = member.value(QStringLiteral("keyed")).toBool();
                    break;
                }
            }
        }
        complete = complete && currentKnown;
        if (stillKeyed) { participants.append(person); }
    }
    if (liveDouble && participants.size() < 2) { complete = false; }
    if (identitiesComplete) { *identitiesComplete = complete; }
    return participants;
}

QStringList roundtableTagReportLines(const RoundtableTag& tag, const QJsonObject& view, qint64 nowMs)
{
    const QString family = tag.roomId.startsWith(QLatin1String("frequency:"))
        ? tag.roomId.mid(10).section(QLatin1Char('-'), 0, 0)
        : (tag.stations.isEmpty() ? QJsonObject() : tag.stations.first().toObject()).value(QStringLiteral("family")).toString();
    if (family.isEmpty() || view.value(QStringLiteral("family")).toString() != family) { return {}; }
    // D3: the exact turn, else the nearest live one. A live turn is never
    // captioned as past (D6).
    const ChosenTurn found = chooseTurn(view, tag.hz, true);
    const QJsonObject& chosen = found.turn;
    const bool historical = found.historical;
    if (chosen.isEmpty()) { return {}; }
    const QString kind = chosen.value(QStringLiteral("kind")).toString();
    const bool doubled = kind == QLatin1String("doubled");
    const TurnPeople people = peopleOf(view, chosen, true);
    const bool identitiesComplete = people.complete;
    const QSet<QString>& talkerIds = people.ids;
    const QStringList& names = people.names;
    QString heading = historical ? QStringLiteral("Last unknown signal") : QStringLiteral("Unknown signal");
    if (doubled) {
        const QString label = historical ? QStringLiteral("Last double") : QStringLiteral("Double");
        if (!historical && !identitiesComplete) {
            heading = names.isEmpty() ? QStringLiteral("Double · keyed identities unavailable")
                : QStringLiteral("Double · identities updating: ") + names.join(QStringLiteral(" + "));
        } else {
            heading = label + (talkerIds.size() > 2 ? QStringLiteral(" (%1)").arg(talkerIds.size()) : QString())
                + (names.isEmpty() ? QStringLiteral(" · identities unknown") : QStringLiteral(": ") + names.join(QStringLiteral(" + ")));
        }
    } else if (kind != QLatin1String("unknown")) {
        if (talkerIds.size() != 1 || names.size() != 1) {
            heading = historical ? QStringLiteral("Last unattributed signal") : QStringLiteral("Unattributed signal");
        } else {
            heading = (historical ? QStringLiteral("Last talker: ") : QStringLiteral("Talking: ")) + names.first();
        }
    }
    const QList<QJsonObject> reporters = turnReporters(view, chosen, family, talkerIds, historical);
    const QString format = AppSettings::instance().value(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("dBm")).toString();
    const QString unit = format == QLatin1String("S-units") ? QStringLiteral("S-units")
        : format == QLatin1String("Both") ? QStringLiteral("S / dBm") : QStringLiteral("dBm");
    QStringList lines{heading};
    if (!reporters.isEmpty()) {
        lines.append((doubled ? QStringLiteral("Combined RF · Heard by · ") : QStringLiteral("Heard by     "))
            + unit + (doubled ? QStringLiteral(" · Age") : QStringLiteral("    Age")));
    } else {
        lines.append(doubled ? QStringLiteral("Combined RF · No matching reports") : QStringLiteral("No matching reports"));
    }
    for (const QJsonObject& member : std::as_const(reporters)) {
        const QJsonObject reading = member.value(QStringLiteral("reading")).toObject();
        const double dbm = reading.value(QStringLiteral("dbm")).toDouble();
        const qint64 ageMs = std::max<qint64>(0, nowMs - reading.value(QStringLiteral("t")).toInteger());
        const qint64 seconds = ageMs / 1000;
        QString age = seconds < 60 ? QStringLiteral("%1s").arg(seconds)
            : seconds < 3600 ? QStringLiteral("%1m").arg(seconds / 60) : QStringLiteral("%1h").arg(seconds / 3600);
        if (ageMs > kOldReadingMs) { age += QStringLiteral(" stale"); }
        const QString dbmText = QString::number(qRound(dbm));
        const QString signal = format == QLatin1String("S-units") ? roundtableSUnitText(dbm)
            : format == QLatin1String("Both") ? roundtableSUnitText(dbm) + QStringLiteral(" / ") + dbmText : dbmText;
        lines.append(QStringLiteral("%1  %2  %3").arg(personName(member), signal, age));
    }
    lines.append(QStringLiteral("Open details"));
    return lines;
}

QStringList roundtableCochannelRows(const RoundtableTag& tag, int maxRows, const QFontMetrics* metrics, int textWidth)
{
    if (maxRows <= 0) { return {}; }
    const QString details = QStringLiteral("Reports ›");
    if (maxRows == 1) { return {details}; }
    const auto fits = [metrics, textWidth](const QString& row) {
        return !metrics || textWidth <= 0 || (metrics->horizontalAdvance(row) <= textWidth
            && metrics->elidedText(row, Qt::ElideRight, textWidth) == row);
    };
    QStringList rows{roundtableFrequencyText(tag.hz) + QStringLiteral(" MHz")};
    QStringList reports = tag.reportLines;
    if (!reports.isEmpty() && reports.last() == QLatin1String("Open details")) { reports.removeLast(); }
    const int sampleStart = reports.size() > 1 && reports.at(1).contains(QLatin1String("Heard by"))
        ? 2 : static_cast<int>(reports.size());
    const int sampleCount = reports.size() - sampleStart;
    int omittedNames = 0;
    if (!reports.isEmpty() && rows.size() + 1 < maxRows) {
        const QString heading = reports.first();
        if (fits(heading)) {
            rows.append(heading);
        } else if (heading.contains(QLatin1String("double"), Qt::CaseInsensitive) && heading.contains(QLatin1Char(':'))) {
            const QString prefix = heading.section(QLatin1Char(':'), 0, 0);
            if (fits(prefix)) { rows.append(prefix); }
            else {
                rows.append(heading.startsWith(QLatin1String("Last double")) ? QStringLiteral("Last double") : QStringLiteral("Double"));
                if (heading.contains(QLatin1String("updating")) && rows.size() + 1 < maxRows) {
                    rows.append(QStringLiteral("Names updating"));
                }
            }
            const QStringList names = heading.section(QLatin1Char(':'), 1).trimmed().split(QStringLiteral(" + "));
            // Wrap only between whole identities, and reserve room for the
            // combined-RF header plus at least one measured receiver row.
            QString group;
            int grouped = 0;
            const int reserveRf = sampleCount > 0 ? 2 : (reports.size() > 1 ? 1 : 0);
            int shownNames = 0;
            const auto appendGroup = [&]() {
                if (!group.isEmpty() && rows.size() + reserveRf + 1 < maxRows) {
                    rows.append(group);
                    shownNames += grouped;
                }
                group.clear(); grouped = 0;
            };
            for (const QString& name : names) {
                const QString combined = group.isEmpty() ? name : group + QStringLiteral(" + ") + name;
                if (!fits(combined)) { appendGroup(); }
                if (fits(name)) {
                    group = group.isEmpty() ? name : group + QStringLiteral(" + ") + name;
                    ++grouped;
                }
            }
            appendGroup();
            omittedNames = names.size() - shownNames;
        } else if (heading.contains(QLatin1String("double"), Qt::CaseInsensitive)) {
            rows.append(heading.startsWith(QLatin1String("Last double")) ? QStringLiteral("Last double") : QStringLiteral("Double"));
            if (rows.size() + 1 < maxRows) { rows.append(QStringLiteral("Names unavailable")); }
        } else {
            const QString fallback = heading.contains(QLatin1String("Unknown"), Qt::CaseInsensitive)
                ? QStringLiteral("Unknown signal") : QStringLiteral("Talker · name in details");
            if (fits(fallback)) { rows.append(fallback); }
        }
    }
    for (int i = 1; i < sampleStart && rows.size() + 1 < maxRows; ++i) {
        if (fits(reports.at(i))) {
            rows.append(reports.at(i));
        } else if (reports.at(i).contains(QLatin1String("Combined RF"))) {
            const QString unit = reports.at(i).contains(QLatin1String("S / dBm")) ? QStringLiteral("S / dBm")
                : reports.at(i).contains(QLatin1String("S-units")) ? QStringLiteral("S-units") : QStringLiteral("dBm");
            rows.append(QStringLiteral("Combined RF · ") + unit);
        }
    }
    int shown = 0;
    for (int i = sampleStart; i < reports.size() && rows.size() + 1 < maxRows; ++i) {
        if (fits(reports.at(i))) { rows.append(reports.at(i)); ++shown; }
    }
    const int omitted = sampleCount - shown;
    if (omittedNames > 0 && omitted > 0) {
        rows.append(QStringLiteral("+%1 names, +%2 reports ›").arg(omittedNames).arg(omitted));
    } else if (omittedNames > 0) {
        rows.append(QStringLiteral("+%1 names · details ›").arg(omittedNames));
    } else {
        rows.append(omitted > 0 ? QStringLiteral("+%1 reports · details ›").arg(omitted) : details);
    }
    return rows;
}

QList<RoundtableTag> roundtableTagsFor(const RoundtableModel* model, const QList<int>& sliceIds, const QString& activeContext)
{
    QList<RoundtableTag> out;
    if (model == nullptr) {
        return out;
    }
    if (model->frequencyAvailable()) {
        QHash<QString, int> cochannel;
        const QJsonArray directory = directoryStations(model);
        for (const QJsonValue& roomValue : model->rooms()) {
            const QJsonObject summary = roomValue.toObject();
            for (const QJsonValue& value : summary.value(QStringLiteral("stations")).toArray()) {
                QJsonObject station = value.toObject();
                const qint64 hz = station.value(QStringLiteral("hz")).toInteger();
                const QString family = station.value(QStringLiteral("family")).toString();
                if (hz <= 0 || family.isEmpty()) {
                    continue;
                }
                const QString context = RoundtableModel::frequencyContextId(hz, family);
                if (!cochannel.contains(context)) {
                    RoundtableTag tag;
                    tag.roomId = context;
                    tag.hz = hz;
                    cochannel.insert(context, out.size());
                    out.append(tag);
                }
                RoundtableTag& tag = out[cochannel.value(context)];
                const QString identity = station.value(QStringLiteral("sourceRoom")).toString() + QLatin1Char('/')
                    + station.value(QStringLiteral("ref")).toString();
                bool duplicate = false;
                for (qsizetype index = 0; index < tag.stations.size(); ++index) {
                    QJsonObject previous = tag.stations.at(index).toObject();
                    if (previous.value(QStringLiteral("sourceRoom")).toString() + QLatin1Char('/')
                        + previous.value(QStringLiteral("ref")).toString() == identity) {
                        previous.insert(QStringLiteral("keyed"), previous.value(QStringLiteral("keyed")).toBool()
                                        || station.value(QStringLiteral("keyed")).toBool());
                        tag.stations[index] = previous;
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) { tag.stations.append(station); }
                tag.members = tag.stations.size();
                const QString call = station.value(QStringLiteral("callsign")).toString();
                const int nth = station.value(QStringLiteral("nth")).toInt(1);
                const QString label = nth > 1 ? QStringLiteral("%1 (%2)").arg(call).arg(nth) : call;
                if (tag.label.isEmpty()) {
                    tag.label = label;
                }
                if (station.value(QStringLiteral("keyed")).toBool()) {
                    tag.state = static_cast<int>(RoundtableGlyph::Talking);
                    // The tag names who is talking, not who joined first.
                    if (tag.keyed.isEmpty()) { tag.label = label; }
                    if (!tag.keyed.contains(label)) { tag.keyed.append(label); }
                }
                const bool own = station.value(QStringLiteral("you")).toBool();
                tag.mine = tag.mine || own;
                tag.mentionsMe = model->mentionCount(context) > 0;
            }
        }
        if (!activeContext.isEmpty() && !cochannel.contains(activeContext)) {
            RoundtableTag active;
            active.roomId = activeContext;
            active.hz = activeContext.mid(10).section(QLatin1Char('-'), 1).toLongLong();
            active.active = true;
            active.reportLines = roundtableTagReportLines(active, model->room(activeContext), RoundtableModel::nowMs());
            if (!active.reportLines.isEmpty()) { out.append(active); }
        }
        for (RoundtableTag& tag : out) {
            const QString family = familyOfContext(tag.roomId);
            const QJsonObject ownView = viewForFamily(model->room(tag.roomId), family);
            // D1, for every frequency, not only the active one.
            const bool isDouble = roundtableIsDouble(ownView, directory, tag.hz, family);
            if (isDouble) {
                tag.state = static_cast<int>(RoundtableGlyph::Doubled);
                tag.label = QStringLiteral("Double");
                tag.keyed = doubleNames(ownView, directory, tag.hz, family);
            }
            QJsonObject view = model->room(tag.roomId);
            if (view.isEmpty()) { view = model->room(model->readingRoomId()); }
            tag.active = tag.roomId == activeContext;
            if (tag.active) {
                tag.reportLines = roundtableTagReportLines(tag, view, RoundtableModel::nowMs());
                if (!tag.reportLines.isEmpty() && tag.reportLines.first().startsWith(QLatin1String("Double"))) {
                    tag.state = static_cast<int>(RoundtableGlyph::Doubled);
                    tag.label = QStringLiteral("Double");
                    tag.keyed = doubleNames(viewForFamily(view, family), directory, tag.hz, family);
                } else if (!tag.reportLines.isEmpty() && tag.reportLines.first().startsWith(QLatin1String("Unknown signal"))) {
                    tag.state = static_cast<int>(RoundtableGlyph::Unknown);
                } else if (!tag.reportLines.isEmpty() && tag.reportLines.first().startsWith(QLatin1String("Talking:"))) {
                    tag.state = static_cast<int>(RoundtableGlyph::Talking);
                }
            }
        }
        std::sort(out.begin(), out.end(), [](const RoundtableTag& a, const RoundtableTag& b) {
            return a.hz == b.hz ? a.roomId < b.roomId : a.hz < b.hz;
        });
        return out;
    }
    QSet<QString> mine;
    for (int sliceId : sliceIds) {
        const QString roomId = model->sliceRoomId(sliceId);
        if (!roomId.isEmpty()) {
            mine.insert(roomId);
        }
    }
    for (const QJsonValue& value : model->rooms()) {
        const QJsonObject summary = value.toObject();
        RoundtableTag tag;
        tag.roomId = summary.value(QStringLiteral("id")).toString();
        tag.hz = summary.value(QStringLiteral("hz")).toInteger();
        if (tag.roomId.isEmpty() || tag.hz <= 0) {
            continue;
        }
        tag.mine = mine.contains(tag.roomId);
        RoundtableGlyph glyph = glyphOfState(summary.value(QStringLiteral("state")).toString());
        // U8: your own turn is cyan, in your slice's room (the room you
        // view, which says which turn is yours).
        if (tag.mine && youAreTalking(model->room(tag.roomId))) {
            glyph = RoundtableGlyph::You;
        }
        tag.state = static_cast<int>(glyph);
        tag.label = labelOf(glyph, summary.value(QStringLiteral("talkers")).toArray());
        tag.members = summary.value(QStringLiteral("members")).toInt();
        tag.mentionsMe = model->mentionCount(tag.roomId) > 0;
        out.append(tag);
    }
    return out;
}

RoundtableTagDisplay roundtableTagDisplayFor(const RoundtableModel* model)
{
    using LinkState = RoundtableModel::LinkState;
    if (model == nullptr || !model->available()) {
        return RoundtableTagDisplay::Hidden;
    }
    switch (model->linkState()) {
    case LinkState::Off:
    case LinkState::Connecting:
        return RoundtableTagDisplay::Hidden;
    case LinkState::Reconnecting:
    case LinkState::Offline:
        // A dropped link never claims to be live (overlay design D7).
        return RoundtableTagDisplay::Frozen;
    case LinkState::Hidden:
        // You still see everything (D21), the marking dimmed.
        return RoundtableTagDisplay::Dimmed;
    case LinkState::Live:
        return RoundtableTagDisplay::Live;
    }
    return RoundtableTagDisplay::Hidden;
}

RoundtableFlagLine roundtableFlagLineFor(const RoundtableModel* model, int sliceId, qint64 hz, const QString& family)
{
    using LinkState = RoundtableModel::LinkState;
    RoundtableFlagLine line;
    if (model == nullptr || !model->available()) {
        return line;
    }
    QString prefix = QStringLiteral("Roundtable \u00b7 ");
    line.visible = true;
    switch (model->linkState()) {
    case LinkState::Off:
        line.visible = false;
        return line;
    case LinkState::Connecting:
        line.glyph = static_cast<int>(RoundtableGlyph::Waiting);
        line.text = prefix + QStringLiteral("Connecting");
        return line;
    case LinkState::Reconnecting:
        line.glyph = static_cast<int>(RoundtableGlyph::Waiting);
        line.text = prefix + QStringLiteral("Reconnecting");
        return line;
    case LinkState::Offline:
        line.glyph = static_cast<int>(RoundtableGlyph::Offline);
        line.text = prefix + QStringLiteral("Offline");
        return line;
    case LinkState::Hidden:
        // Hidden still sees everything (D21): the room, as when live, with
        // Hidden in place of Roundtable (the line still fits the flag with
        // @N and a long callsign) and the glyph dimmed.
        prefix = QStringLiteral("Hidden \u00b7 ");
        line.dimmed = true;
        break;
    case LinkState::Live:
        break;
    }

    const bool frequency = model->frequencyAvailable() && hz > 0 && !family.isEmpty();
    const QString roomId = frequency ? RoundtableModel::frequencyContextId(hz, family)
                                     : model->sliceRoomId(sliceId);
    QJsonObject summary;
    if (frequency) {
        int count = 0;
        QJsonArray stations;
        for (const RoundtableTag& tag : roundtableTagsFor(model, {})) {
            if (tag.roomId.mid(10).section(QLatin1Char('-'), 0, 0) != family
                || std::llabs(tag.hz - hz) > kNearbyHz) { continue; }
            count += tag.members;
            for (const QJsonValue& value : tag.stations) { stations.append(value); }
        }
        // D1 and D3: two keyed stations merely near each other are not a
        // double. The open turn says who it is; the directory otherwise names
        // the nearest keyed station.
        const QJsonObject view = viewForFamily(model->room(roomId), family);
        const ChosenTurn chosen = view.isEmpty() ? ChosenTurn() : chooseTurn(view, hz, true);
        QJsonArray talkers;
        QString state = QStringLiteral("quiet");
        if (roundtableIsDouble(view, stations, hz, family)) {
            state = QStringLiteral("doubled");
        } else if (!chosen.turn.isEmpty() && !chosen.historical) {
            const bool unknown = chosen.turn.value(QStringLiteral("kind")).toString() == QLatin1String("unknown");
            for (const QJsonValue& value : chosen.turn.value(QStringLiteral("talkers")).toArray()) {
                if (!unknown) { talkers.append(value.toObject().value(QStringLiteral("callsign"))); }
            }
            state = talkers.isEmpty() ? QStringLiteral("unknown") : QStringLiteral("talking");
        } else {
            for (const QJsonObject& station : keyedNear(stations, hz, family)) {
                talkers.append(station.value(QStringLiteral("callsign")));
            }
            if (!talkers.isEmpty()) { state = QStringLiteral("talking"); }
        }
        summary = QJsonObject{{QStringLiteral("members"), count}, {QStringLiteral("talkers"), talkers},
            {QStringLiteral("state"), state}};
    } else if (!roomId.isEmpty()) {
        for (const QJsonValue& value : model->rooms()) {
            if (value.toObject().value(QStringLiteral("id")).toString() == roomId) {
                summary = value.toObject();
                break;
            }
        }
    }
    const int members = summary.value(QStringLiteral("members")).toInt();
    if (summary.isEmpty() || members <= 0) {
        line.glyph = static_cast<int>(RoundtableGlyph::Quiet);
        line.text = prefix + QStringLiteral("Nobody here");
        return line;
    }
    line.mentions = model->mentionCount(roomId);

    // U3: while you transmit there, how many stations hear you: the others
    // whose reading of you is present.
    const QJsonObject room = model->room(roomId);
    if (youAreTalking(room)) {
        QSet<qint64> ownTurns;
        if (frequency) {
            for (const QJsonValue& value : room.value(QStringLiteral("now")).toArray()) {
                const QJsonObject turn = value.toObject();
                if (turn.value(QStringLiteral("hz")).toInteger() == hz && roundtableTurnIsYours(room, turn, true)) {
                    ownTurns.insert(turn.value(QStringLiteral("id")).toInteger());
                }
            }
        }
        int hearing = 0;
        for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
            const QJsonObject member = value.toObject();
            const QJsonValue reading = member.value(QStringLiteral("reading"));
            if (!member.value(QStringLiteral("you")).toBool() && reading.isObject()
                && reading.toObject().value(QStringLiteral("present")).toBool(true)
                && (!frequency || ownTurns.contains(reading.toObject().value(QStringLiteral("turn")).toInteger()))) {
                ++hearing;
            }
        }
        line.glyph = static_cast<int>(RoundtableGlyph::You);
        line.text = prefix + QStringLiteral("%1 hear you").arg(hearing);
        return line;
    }

    const RoundtableGlyph glyph = glyphOfState(summary.value(QStringLiteral("state")).toString());
    line.glyph = static_cast<int>(glyph);
    line.text = prefix + QStringLiteral("%1 \u00b7 %2")
                             .arg(members)
                             .arg(labelOf(glyph, summary.value(QStringLiteral("talkers")).toArray()));
    return line;
}

QColor roundtableGlyphColor(int glyph)
{
    switch (static_cast<RoundtableGlyph>(glyph)) {
    case RoundtableGlyph::Talking:
        return QColor(0x4c, 0xaf, 0x6a);
    case RoundtableGlyph::Unknown:
    case RoundtableGlyph::Waiting:
        return QColor(0xd8, 0xa5, 0x5f);
    case RoundtableGlyph::Doubled:
    case RoundtableGlyph::Offline:
        return QColor(0xc2, 0x5a, 0x5c);
    case RoundtableGlyph::You:
        return QColor(0x00, 0xd4, 0xff);
    case RoundtableGlyph::Quiet:
        break;
    }
    return QColor(0x7e, 0x7e, 0x85);
}

QColor roundtableStaleColor()
{
    return QColor(0x7e, 0x7e, 0x85);
}

QColor roundtableDimmedColor(int glyph)
{
    const QColor legacy = roundtableGlyphColor(glyph);
    QColor color;
    switch (static_cast<RoundtableGlyph>(glyph)) {
    case RoundtableGlyph::Talking:
        color = WorkspacePreview::statusColour(legacy, "ok");
        break;
    case RoundtableGlyph::Unknown:
    case RoundtableGlyph::Waiting:
        color = WorkspacePreview::statusColour(legacy, "warn");
        break;
    case RoundtableGlyph::Doubled:
    case RoundtableGlyph::Offline:
        color = WorkspacePreview::statusColour(legacy, "danger");
        break;
    case RoundtableGlyph::You:
        color = WorkspacePreview::roleColour(legacy, "accent");
        break;
    case RoundtableGlyph::Quiet:
        color = WorkspacePreview::roleColour(legacy, "text-secondary");
        break;
    }
    if (!color.isValid()) {
        color = legacy;
    }
    color.setAlphaF(kRoundtableDimmedAlpha);
    return color;
}

void paintRoundtableGlyph(QPainter& p, const QPointF& center, qreal size, int glyph, bool grey, bool dimmed)
{
    const QColor color = grey     ? roundtableStaleColor()
        : dimmed                  ? roundtableDimmedColor(glyph)
                                  : roundtableGlyphColor(glyph);
    // The mockup's glyphs are 9 px across; their strokes scale with them.
    const qreal unit = size / 9.0;
    const qreal radius = size / 2.0;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    switch (static_cast<RoundtableGlyph>(glyph)) {
    case RoundtableGlyph::Talking:
    case RoundtableGlyph::You:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(center, radius, radius);
        break;
    case RoundtableGlyph::Unknown: {
        // A ring with a dot.
        const qreal stroke = 2.0 * unit;
        p.setPen(QPen(color, stroke));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, radius - stroke / 2.0, radius - stroke / 2.0);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(center, 1.5 * unit, 1.5 * unit);
        break;
    }
    case RoundtableGlyph::Doubled: {
        // A striped diamond.
        const qreal half = radius * 0.85 * 1.414;
        const QPolygonF diamond{QPointF(center.x(), center.y() - half), QPointF(center.x() + half, center.y()),
                                QPointF(center.x(), center.y() + half), QPointF(center.x() - half, center.y())};
        QPainterPath clip;
        clip.addPolygon(diamond);
        p.setClipPath(clip, Qt::IntersectClip);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        for (qreal x = center.x() - half; x < center.x() + half; x += 3.5 * unit) {
            p.drawRect(QRectF(x, center.y() - half, 2.0 * unit, 2.0 * half));
        }
        p.setClipping(false);
        p.setPen(QPen(color, unit));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(diamond);
        break;
    }
    case RoundtableGlyph::Waiting: {
        // A ring with its top quarter open: connecting or reconnecting.
        const qreal stroke = 2.0 * unit;
        const qreal r = radius - stroke / 2.0;
        p.setPen(QPen(color, stroke, Qt::SolidLine, Qt::FlatCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(center.x() - r, center.y() - r, 2.0 * r, 2.0 * r), 135 * 16, 270 * 16);
        break;
    }
    case RoundtableGlyph::Offline: {
        const qreal stroke = 2.0 * unit;
        p.setPen(QPen(color, stroke));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, radius - stroke / 2.0, radius - stroke / 2.0);
        break;
    }
    case RoundtableGlyph::Quiet: {
        const qreal stroke = 1.5 * unit;
        p.setPen(QPen(color, stroke));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, radius - stroke / 2.0, radius - stroke / 2.0);
        break;
    }
    }
    p.restore();
}

QString roundtableFrequencyText(qint64 hz)
{
    int decimals = 6;
    if (hz % 1000 == 0) {
        decimals = 3;
    } else if (hz % 100 == 0) {
        decimals = 4;
    } else if (hz % 10 == 0) {
        decimals = 5;
    }
    return QString::number(static_cast<double>(hz) / 1.0e6, 'f', decimals);
}

} // namespace NereusSDR
