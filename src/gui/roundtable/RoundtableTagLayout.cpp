// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableTagLayout.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableTagLayout.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-03: The view of another frequency (overlay design D22, D23) shares
//               the card's frame and reading row: paintCardFrame, paintCardRow.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-03: The live card and tags on the pan: placeRoundtableCard,
//               paintRoundtableCard, roundtableMergeDoubleTags. The text-row
//               card path is gone. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
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
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
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
    double width = 0.0;
};

// A tag or a fold, centered on x.
struct Item {
    double x = 0.0;
    QStringList roomIds;
    bool folded = false;
    double width = 0.0;
};

// Whether a group's tags can sit side by side, each no further than the
// group's widest tag from its own frequency. Side by side, tag k's center sits
// at start + c_k, c_k being the widths and gaps before it; the best start puts
// the largest move as small as it can be, half the spread of x_k - c_k.
bool slidesApart(const QList<Visible>& visible, const QList<int>& group, double gap)
{
    double lowest = 0.0;
    double highest = 0.0;
    double center = 0.0;
    double widest = 0.0;
    for (int k = 0; k < group.size(); ++k) {
        const Visible& tag = visible.at(group.at(k));
        if (k > 0) {
            center += (visible.at(group.at(k - 1)).width + tag.width) / 2.0 + gap;
        }
        const double offset = tag.x - center;
        lowest = k == 0 ? offset : std::min(lowest, offset);
        highest = k == 0 ? offset : std::max(highest, offset);
        widest = std::max(widest, tag.width);
    }
    return (highest - lowest) / 2.0 <= widest;
}

// Tags that would overlap their neighbor share a group, which reaches
// `reach` pixels at most. A group whose unpinned tags number two or more
// folds them when they cannot move aside, or always when `forceFold`.
QList<Item> groupTags(const QList<Visible>& visible, double foldWidth, double gap, double reach,
                      bool forceFold)
{
    QList<QList<int>> groups;
    for (int i = 0; i < visible.size(); ++i) {
        if (!groups.isEmpty()) {
            const QList<int>& group = groups.last();
            const double fromLast = visible.at(i).x - visible.at(group.last()).x;
            const double fromFirst = visible.at(i).x - visible.at(group.first()).x;
            const double step = (visible.at(group.last()).width + visible.at(i).width) / 2.0 + gap;
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
            && (forceFold || !slidesApart(visible, group, gap));
        for (int index : group) {
            if (!fold || visible.at(index).pinned) {
                items.append(Item{visible.at(index).x, QStringList{visible.at(index).roomId}, false,
                                  visible.at(index).width});
            }
        }
        if (fold) {
            Item folded;
            folded.folded = true;
            folded.width = foldWidth;
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
};

// The pan less the keep-out stretches, each side `gap` from an edge or a
// keep-out.
QList<Stretch> stretchesOf(double width, double gap, QList<RoundtableKeepOut> keepOut)
{
    std::sort(keepOut.begin(), keepOut.end(),
              [](const RoundtableKeepOut& a, const RoundtableKeepOut& b) { return a.left < b.left; });
    QList<Stretch> out;
    double left = gap;
    const auto close = [&](double right) {
        if (right > left) {
            out.append(Stretch{left, right});
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

// How many of `items`, in order, the stretches hold side by side. An item that
// finds no room is passed over, and the next may still fit.
qsizetype itemsThatFit(const QList<Item>& items, const QList<Stretch>& stretches, double gap)
{
    qsizetype placed = 0;
    qsizetype current = 0;
    double used = 0.0;  // of the current stretch, with a gap after each tag in it
    for (const Item& item : items) {
        qsizetype s = current;
        double taken = used;
        while (s < stretches.size() && taken + item.width > stretches.at(s).right - stretches.at(s).left) {
            ++s;
            taken = 0.0;
        }
        if (s < stretches.size()) {
            current = s;
            used = taken + item.width + gap;
            ++placed;
        }
    }
    return placed;
}

// The stretch nearest x that is wide enough for `width`; -1 when none is.
qsizetype nearestStretch(const QList<Stretch>& stretches, double x, double width)
{
    qsizetype best = -1;
    double bestDistance = -1.0;
    for (qsizetype k = 0; k < stretches.size(); ++k) {
        const Stretch& stretch = stretches.at(k);
        if (stretch.right - stretch.left < width) {
            continue;
        }
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
    QList<qint64> hz;  // each participant's own frequency, where the turn names it
    bool complete = true;
    QString you;
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
            if (talker.value(QStringLiteral("hz")).isDouble()) { people.hz.append(talker.value(QStringLiteral("hz")).toInteger()); }
            // The wire marks you on a member, never on a talker.
            for (const QJsonValue& memberValue : view.value(QStringLiteral("members")).toArray()) {
                const QJsonObject member = memberValue.toObject();
                if (member.value(QStringLiteral("you")).toBool() && !ref.isEmpty()
                    && member.value(QStringLiteral("ref")).toString() == ref
                    && (!scoped || member.value(QStringLiteral("sourceRoom")).toString()
                                       == talker.value(QStringLiteral("sourceRoom")).toString(source))) {
                    people.you = name;
                    break;
                }
            }
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

// Every station in these summaries, as the directory lists them.
QJsonArray stationsOfRooms(const QJsonArray& rooms)
{
    QJsonArray stations;
    for (const QJsonValue& room : rooms) {
        for (const QJsonValue& station : room.toObject().value(QStringLiteral("stations")).toArray()) {
            stations.append(station);
        }
    }
    return stations;
}

QJsonArray directoryStations(const RoundtableModel* model)
{
    return stationsOfRooms(model->rooms());
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

// What one frequency is by its state in the directory: "doubled", "talking",
// "unknown" or "quiet", with who is keyed.
struct FrequencyTruth {
    QString state = QStringLiteral("quiet");
    QStringList keyed;
};

// D1's directory rule, read only from the source rooms this frequency's own
// stations sit in: a double in the room next door says nothing about this one.
// Two keyed stations in one of them are a double; so is a summary that says so,
// which holds while one participant is left (D5).
FrequencyTruth directoryTruth(const QJsonArray& rooms, const QJsonArray& stations, qint64 hz, const QString& family)
{
    QSet<QString> sources;
    QStringList keyedHere;
    QSet<QString> seenHere;
    for (const QJsonValue& value : stations) {
        const QJsonObject station = value.toObject();
        if (station.value(QStringLiteral("hz")).toInteger() != hz
            || station.value(QStringLiteral("family")).toString() != family) {
            continue;
        }
        sources.insert(station.value(QStringLiteral("sourceRoom")).toString());
        const QString id = station.value(QStringLiteral("sourceRoom")).toString() + QLatin1Char('/')
            + station.value(QStringLiteral("ref")).toString();
        if (station.value(QStringLiteral("keyed")).toBool() && !seenHere.contains(id)) {
            seenHere.insert(id);
            keyedHere.append(personName(station));
        }
    }
    const QList<QJsonObject> near = keyedNear(stations, hz, family);
    QHash<QString, QStringList> keyedBySource;
    for (const QJsonObject& station : near) {
        const QString source = station.value(QStringLiteral("sourceRoom")).toString();
        if (!source.isEmpty() && sources.contains(source)) { keyedBySource[source].append(personName(station)); }
    }
    for (const QJsonObject& station : near) {
        const QStringList names = keyedBySource.value(station.value(QStringLiteral("sourceRoom")).toString());
        if (names.size() >= 2) { return {QStringLiteral("doubled"), names}; }
    }
    bool unknown = false;
    for (const QJsonValue& value : rooms) {
        const QJsonObject summary = value.toObject();
        const QString id = summary.value(QStringLiteral("id")).toString();
        if (!sources.contains(id) || (summary.contains(QStringLiteral("family"))
                                      && summary.value(QStringLiteral("family")).toString() != family)) {
            continue;
        }
        const QString state = summary.value(QStringLiteral("state")).toString();
        if (state == QLatin1String("doubled")) {
            QStringList names = keyedBySource.value(id);
            if (names.isEmpty()) {
                // Nobody keyed in the directory yet: the summary's talkers carry
                // no slice number and may have left, so only those still listed
                // here are named, as the directory names them (D5).
                const QJsonArray talkers = summary.value(QStringLiteral("talkers")).toArray();
                QSet<QString> named;
                for (const QJsonValue& stationValue : stations) {
                    const QJsonObject station = stationValue.toObject();
                    const QString call = station.value(QStringLiteral("callsign")).toString();
                    if (station.value(QStringLiteral("sourceRoom")).toString() == id && !named.contains(call)
                        && talkers.contains(QJsonValue(call))) {
                        named.insert(call);
                        names.append(personName(station));
                    }
                }
            }
            return {QStringLiteral("doubled"), names};
        }
        unknown = unknown || state == QLatin1String("unknown");
    }
    if (!keyedHere.isEmpty()) { return {QStringLiteral("talking"), keyedHere}; }
    return {unknown ? QStringLiteral("unknown") : QStringLiteral("quiet"), {}};
}

// A read whose stream has not arrived has an empty view of its own: it knows
// nothing yet, and says nothing against the directory.
bool viewIsPending(const QJsonObject& view)
{
    return view.value(QStringLiteral("members")).toArray().isEmpty()
        && view.value(QStringLiteral("now")).toArray().isEmpty();
}

// The view only when it covers this family: another family's view says nothing
// about this frequency.
QJsonObject viewForFamily(const QJsonObject& view, const QString& family)
{
    return view.value(QStringLiteral("family")).toString() == family ? view : QJsonObject();
}

// What one frequency is now: its open turn when a view knows, otherwise what the
// directory knows.
FrequencyTruth frequencyTruth(const QJsonObject& view, const QJsonArray& rooms, const QJsonArray& stations, qint64 hz,
                              const QString& family)
{
    const QJsonObject known = viewForFamily(view, family);
    const ChosenTurn chosen = known.isEmpty() ? ChosenTurn() : chooseTurn(known, hz, true);
    if (chosen.turn.isEmpty() && viewIsPending(known)) {
        return directoryTruth(rooms, stations, hz, family);
    }
    if (chosen.turn.isEmpty() || chosen.historical) { return {}; }
    const QString kind = chosen.turn.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("unknown")) { return {QStringLiteral("unknown"), {}}; }
    return {kind == QLatin1String("doubled") ? QStringLiteral("doubled") : QStringLiteral("talking"),
            peopleOf(known, chosen.turn, true).names};
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

bool roundtableIsDouble(const QJsonObject& view, const QJsonArray& rooms, qint64 hz, const QString& family)
{
    return frequencyTruth(view, rooms, stationsOfRooms(rooms), hz, family).state == QLatin1String("doubled");
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

    // A view's list: each station against the viewed frequency (D22). Until the
    // stream arrives the directory says who is nearby, as the server's members do.
    const QJsonArray members = view.value(QStringLiteral("members")).toArray();
    const QJsonArray& listed = members.isEmpty() ? stations : members;
    QSet<QString> seenListed;
    for (const QJsonValue& value : listed) {
        const QJsonObject person = value.toObject();
        const qint64 personHz = person.value(QStringLiteral("hz")).toInteger(dial);
        if (std::abs(personHz - dial) > kNearbyHz
            || (person.contains(QStringLiteral("family")) && person.value(QStringLiteral("family")).toString() != family)) {
            continue;
        }
        const QString id = person.value(QStringLiteral("sourceRoom")).toString() + QLatin1Char('/')
            + person.value(QStringLiteral("ref")).toString();
        if (!seenListed.contains(id)) {
            seenListed.insert(id);
            card.onFrequency.append(RoundtableCardPerson{personName(person), person.value(QStringLiteral("keyed")).toBool(),
                                                         person.value(QStringLiteral("you")).toBool(), personHz - dial});
        }
    }

    const ChosenTurn chosen = view.isEmpty() ? ChosenTurn() : chooseTurn(view, dial, scoped);
    if (chosen.turn.isEmpty()) {
        // Known only from the directory (D1): who is keyed, with no readings. A
        // read whose stream has not arrived has an empty view of its own.
        if (viewIsPending(view)) {
            const FrequencyTruth truth = directoryTruth(model->rooms(), stations, dial, family);
            const QList<QJsonObject> keyed = keyedNear(stations, dial, family);
            if (truth.state == QLatin1String("doubled")) {
                card.kind = Kind::Double;
                card.keyed = truth.keyed;
            } else if (truth.state == QLatin1String("unknown")) {
                card.kind = Kind::Unknown;
            } else if (!keyed.isEmpty()) {
                card.kind = Kind::Talking;
                card.keyed = QStringList{personName(keyed.first())};
            }
        }
        return card;
    }

    const QJsonObject& turn = chosen.turn;
    const bool doubled = turn.value(QStringLiteral("kind")).toString() == QLatin1String("doubled");
    const TurnPeople people = peopleOf(view, turn, scoped);
    card.keyed = people.names;
    card.keyedYou = people.you;
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
        // A double's turn hz is its room's center: Tune goes to who is on it.
        const qint64 turnHz = turn.value(QStringLiteral("hz")).toInteger();
        card.liveHz = turnHz > 0 ? turnHz : dial;
        if (doubled && !people.hz.isEmpty()) {
            card.liveHz = *std::min_element(people.hz.cbegin(), people.hz.cend(), [dial](qint64 a, qint64 b) {
                return std::llabs(a - dial) < std::llabs(b - dial);
            });
        }
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
            // As the panel: a reading's own path first, then the member's.
            const QJsonValue km = reading.contains(QStringLiteral("km")) ? reading.value(QStringLiteral("km"))
                                                                         : member.value(QStringLiteral("km"));
            const QJsonValue bearing = reading.contains(QStringLiteral("bearingDeg"))
                ? reading.value(QStringLiteral("bearingDeg")) : member.value(QStringLiteral("bearingDeg"));
            if (km.isDouble()) { row.km = qRound(km.toDouble()); }
            if (bearing.isDouble()) { row.bearingDeg = bearing.toInt(); }
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
        visible.append(Visible{x, tag.hz, tag.roomId, tag.mine || tag.active || tag.mentionsMe, tag.active,
                               tag.width > 0.0 ? tag.width : tagWidth});
    }
    std::stable_sort(visible.begin(), visible.end(), [](const Visible& a, const Visible& b) {
        return a.hz != b.hz ? a.hz < b.hz : a.roomId < b.roomId;
    });

    const QList<Stretch> stretches = stretchesOf(width, gap, keepOut);
    if (stretches.isEmpty()) {
        return out;
    }
    // A tag wider than every stretch has no clear spot, and is left off.
    double roomiest = 0.0;
    for (const Stretch& stretch : stretches) {
        roomiest = std::max(roomiest, stretch.right - stretch.left);
    }
    visible.removeIf([roomiest](const Visible& tag) { return tag.width > roomiest; });

    // First only the groups that cannot move aside fold. If the tags still
    // don't fit across the pan, every overlapping group folds, and then the
    // groups reach further each round until they fit or span the pan.
    double widest = tagWidth;
    for (const Visible& tag : std::as_const(visible)) {
        widest = std::max(widest, tag.width);
    }
    double reach = widest + gap;
    bool forceFold = false;
    QList<Item> items;
    for (;;) {
        items = groupTags(visible, tagWidth, gap, reach, forceFold);
        if (itemsThatFit(items, stretches, gap) == items.size() || reach > width) {
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
    while (itemsThatFit(items, stretches, gap) < items.size()) {
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
    QList<double> used(stretches.size(), 0.0);
    qsizetype current = 0;
    for (qsizetype k = 0; k < items.size(); ++k) {
        const double itemWidth = items.at(k).width;
        const qsizetype nearest = nearestStretch(stretches, items.at(k).x, itemWidth);
        if (nearest < 0) {
            continue;
        }
        qsizetype target = std::max(nearest, current);
        while (target < stretches.size()
               && used.at(target) + itemWidth > stretches.at(target).right - stretches.at(target).left) {
            ++target;
        }
        if (target >= stretches.size()) {
            continue;
        }
        byStretch[target].append(k);
        used[target] += itemWidth + gap;
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
            const double left = std::max(items.at(k).x - items.at(k).width / 2.0, edge);
            lefts.append(left);
            edge = left + items.at(k).width + gap;
        }
        edge = stretches.at(s).right;
        for (qsizetype m = members.size() - 1; m >= 0; --m) {
            lefts[m] = std::min(lefts.at(m), edge - items.at(members.at(m)).width);
            edge = lefts.at(m) - gap;
        }
        for (qsizetype m = 0; m < members.size(); ++m) {
            const Item& item = items.at(members.at(m));
            out.append(RoundtableTagPlacement{QRectF(lefts.at(m), 0.0, item.width, 0.0), item.roomIds,
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

QList<RoundtableTag> roundtableTagsFor(const RoundtableModel* model, const QList<int>& sliceIds, const QString& activeContext)
{
    QList<RoundtableTag> out;
    if (model == nullptr) {
        return out;
    }
    if (model->frequencyAvailable()) {
        QHash<QString, int> cochannel;
        const QJsonArray summaries = model->rooms();
        const QJsonArray directory = stationsOfRooms(summaries);
        for (const QJsonValue& roomValue : summaries) {
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
            const FrequencyTruth truth = frequencyTruth(ownView, summaries, directory, tag.hz, family);
            if (truth.state == QLatin1String("doubled")) {
                tag.state = static_cast<int>(RoundtableGlyph::Doubled);
                tag.label = QStringLiteral("Double");
                tag.keyed = truth.keyed;
            } else if (truth.state == QLatin1String("unknown")) {
                // D15: amber on every frequency, not only the active one.
                tag.state = static_cast<int>(RoundtableGlyph::Unknown);
                tag.label = labelOf(RoundtableGlyph::Unknown, {});
                tag.keyed.clear();
            }
            QJsonObject view = model->room(tag.roomId);
            if (view.isEmpty()) { view = model->room(model->readingRoomId()); }
            tag.active = tag.roomId == activeContext;
            if (tag.active) {
                tag.reportLines = roundtableTagReportLines(tag, view, RoundtableModel::nowMs());
                // A dropped link never claims to be live (D7), in the text too.
                if (roundtableTagDisplayFor(model) == RoundtableTagDisplay::Frozen && !tag.reportLines.isEmpty()
                    && tag.reportLines.first().startsWith(QLatin1String("Talking: "))) {
                    tag.reportLines[0].replace(0, 9, QStringLiteral("Last known: "));
                }
                // What the tag says comes from the card, never from the text.
                const RoundtableCardModel card = roundtableCardModelFor(model, sliceIds, tag.roomId,
                                                                        static_cast<quint64>(tag.hz), nullptr);
                if (card.kind == RoundtableCardModel::Kind::Double) {
                    tag.state = static_cast<int>(RoundtableGlyph::Doubled);
                    tag.label = QStringLiteral("Double");
                    tag.keyed = card.keyed;
                } else if (card.kind == RoundtableCardModel::Kind::Unknown) {
                    tag.state = static_cast<int>(RoundtableGlyph::Unknown);
                    tag.label = labelOf(RoundtableGlyph::Unknown, {});
                } else if (card.kind == RoundtableCardModel::Kind::Talking || card.kind == RoundtableCardModel::Kind::You) {
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
        const QJsonArray summaries = model->rooms();
        const FrequencyTruth truth = frequencyTruth(view, summaries, stationsOfRooms(summaries), hz, family);
        if (truth.state == QLatin1String("doubled")) {
            state = QStringLiteral("doubled");
        } else if (!chosen.turn.isEmpty() && !chosen.historical) {
            const bool unknown = chosen.turn.value(QStringLiteral("kind")).toString() == QLatin1String("unknown");
            for (const QJsonValue& value : chosen.turn.value(QStringLiteral("talkers")).toArray()) {
                if (!unknown) { talkers.append(value.toObject().value(QStringLiteral("callsign"))); }
            }
            state = talkers.isEmpty() ? QStringLiteral("unknown") : QStringLiteral("talking");
        } else if (truth.state == QLatin1String("unknown")) {
            state = QStringLiteral("unknown");
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

// ---------------------------------------------------------------------------
// The live card on the pan (overlay design D10, D11, D13 to D16, D19). Sizes and
// colors are the board's .rc and .rr rules in overlay.css.
// ---------------------------------------------------------------------------
namespace {

constexpr int kCardWidth = 232;
constexpr int kCardMinWidth = 200;
constexpr int kCardMaxRows = 5;
constexpr int kCardFlagGap = 4;
constexpr int kCardBorder = 1;
constexpr int kCardRail = 3;
constexpr int kCardPadTop = 5;
constexpr int kCardPadBottom = 5;
constexpr int kCardPadSide = 7;
constexpr int kCardGap = 3;
constexpr int kCardHeadLine = 16;
constexpr int kCardNameLine = 15;
constexpr int kCardNameGap = 2;
constexpr int kCardNamesPad = 2;
constexpr int kCardSmallLine = 12;
constexpr int kCardCaption = 16;  // a 1 px rule, 3 px, a 12 px line
constexpr int kCardRowLine = 13;
constexpr int kCardMeter = 3;
constexpr int kCardRowGap = 3;
constexpr int kCardFoot = 13;
constexpr int kCardNameRowGap = 10;
constexpr int kCardNameDot = 7;
constexpr int kCardTextGap = 5;
constexpr int kCardValueGap = 6;
constexpr int kCardAgeMinWidth = 16;
constexpr qreal kCardRadius = 5.0;
constexpr qreal kCardGlyphSize = 9.0;
constexpr qreal kCardDimmedOpacity = 0.86;
constexpr double kMeterMarks[] = {0.136, 0.273, 0.409, 0.545, 0.773};

// D15.
const QColor kColorTalking(0x4c, 0xaf, 0x6a);
const QColor kColorUnknown(0xd8, 0xa5, 0x5f);
const QColor kColorDouble(0xc2, 0x5a, 0x5c);
const QColor kColorDoubleText(0xe0, 0x6c, 0x6e);
const QColor kColorDoubleRailOff(0x3a, 0x1a, 0x1b);
const QColor kColorDoubleMeterOff(0x5d, 0x2c, 0x2e);
const QColor kColorYou(0x00, 0xd4, 0xff);
const QColor kColorYouMeterOff(0x0d, 0x4a, 0x58);
const QColor kColorQuiet(0x7e, 0x7e, 0x85);
const QColor kColorLastBorder(0x60, 0x60, 0x66);
const QColor kColorFill(10, 10, 12, 235);  // rgba(10,10,12,0.92)
const QColor kColorRule(0x2a, 0x2a, 0x2e);
const QColor kColorDim(0xa6, 0xa6, 0xac);
const QColor kColorText(0xec, 0xec, 0xf0);
const QColor kColorLastText(0xc8, 0xc8, 0xce);
const QColor kColorOldText(0x90, 0x90, 0x96);
const QColor kColorOldBar(0x60, 0x60, 0x66);
const QColor kColorMeterTrack(0x26, 0x26, 0x2a);
const QColor kColorPeak(0xf4, 0xf4, 0xf6, 217);  // opacity 0.85
const QColor kColorFrequency(0x79, 0xa8, 0xd2);

int cardLine(int base, const QFontMetrics& fm)
{
    // The board's line heights at the normal text size; a larger workspace text
    // size grows every line by the same ratio.
    return static_cast<int>(std::ceil(base * std::max(1.0, fm.height() / static_cast<double>(kCardNameLine))));
}

bool cardIsLast(const RoundtableCardModel& card)
{
    return card.kind == RoundtableCardModel::Kind::LastTalker || card.kind == RoundtableCardModel::Kind::LastDouble;
}

// What fits on the card at one height. Rows go first (D19), then the details
// around them; the head and the keyed names are the last to go.
struct CardPlan {
    int rows = 0;
    int moreRows = 0;
    int names = 0;
    int moreKeyed = 0;
    bool namesInRow = false;
    bool lastNames = false;
    bool note = false;
    bool caption = false;
    bool emptyLine = false;
    int height = 0;
};

int planHeight(const CardPlan& plan, const QFontMetrics& fm)
{
    const auto line = [&fm](int base) { return cardLine(base, fm); };
    int body = line(kCardHeadLine);
    const auto add = [&body](int section) { body += kCardGap + section; };
    if (plan.names > 0) {
        const int stacked = plan.namesInRow ? 1 : plan.names;
        add(stacked * line(kCardNameLine) + (stacked - 1) * kCardNameGap + kCardNamesPad);
    }
    if (plan.moreKeyed > 0) { add(line(kCardSmallLine)); }
    if (plan.lastNames) { add(line(kCardSmallLine)); }
    if (plan.note) { add(line(kCardSmallLine)); }
    if (plan.caption) { add(line(kCardCaption)); }
    if (plan.rows > 0) {
        add(plan.rows * (line(kCardRowLine) + 1 + kCardMeter) + (plan.rows - 1) * kCardRowGap);
    } else if (plan.emptyLine) {
        add(line(kCardSmallLine));
    }
    if (plan.moreRows > 0) { add(line(kCardFoot)); }
    return 2 * kCardBorder + kCardRail + kCardPadTop + kCardPadBottom + body;
}

bool twoNamesFit(const RoundtableCardModel& card, int width, const QFontMetrics& fm)
{
    if (card.keyed.size() != 2) { return false; }
    const int each = kCardNameDot + kCardTextGap;
    const int needed = 2 * each + kCardNameRowGap + fm.horizontalAdvance(card.keyed.at(0))
        + fm.horizontalAdvance(card.keyed.at(1));
    return needed <= width - 2 * (kCardBorder + kCardPadSide);
}

// The whole plan for `rows` rows within `limit` pixels. Height 0 when even the
// head does not fit.
CardPlan planCard(const RoundtableCardModel& card, int width, int rows, int limit, const QFontMetrics& fm)
{
    CardPlan plan;
    plan.rows = rows;
    plan.moreRows = std::max(0, static_cast<int>(card.rows.size()) - rows);
    if (card.kind == RoundtableCardModel::Kind::Double) {
        plan.names = static_cast<int>(card.keyed.size());
        plan.namesInRow = twoNamesFit(card, width, fm);
    }
    plan.lastNames = card.kind == RoundtableCardModel::Kind::LastDouble && !card.keyed.isEmpty();
    plan.note = !card.note.isEmpty();
    plan.caption = true;
    plan.emptyLine = card.rows.isEmpty();

    plan.height = planHeight(plan, fm);
    const auto shed = [&](auto change) {
        if (plan.height > limit) {
            change();
            plan.height = planHeight(plan, fm);
        }
    };
    if (rows > 0) { return plan; }
    shed([&] { plan.emptyLine = false; });
    shed([&] { plan.caption = false; });
    shed([&] { plan.moreRows = 0; });
    shed([&] { plan.note = false; });
    shed([&] { plan.lastNames = false; });
    // Every name while room allows, then "+N keyed".
    while (plan.height > limit && plan.names > 0) {
        if (plan.namesInRow) {
            plan.namesInRow = false;
        }
        --plan.names;
        plan.moreKeyed = static_cast<int>(card.keyed.size()) - plan.names;
        plan.height = planHeight(plan, fm);
    }
    shed([&] { plan.moreKeyed = 0; });
    if (plan.height > limit) { plan.height = 0; }
    return plan;
}

int rowsThatFit(const RoundtableCardModel& card, int width, int limit, const QFontMetrics& fm)
{
    for (int rows = std::min(kCardMaxRows, static_cast<int>(card.rows.size())); rows > 0; --rows) {
        if (planCard(card, width, rows, limit, fm).height <= limit) { return rows; }
    }
    return 0;
}

QFont cardFont(const QPaintDevice* device, int pixels, int weight, bool mono = false)
{
    QFont font = mono ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
                      : WorkspacePreview::readableFont(QFont(), device);
    font.setPixelSize(WorkspacePreview::readableTextPixelSize(pixels));
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

QString ageText(qint64 ms)
{
    const qint64 seconds = std::max<qint64>(0, ms) / 1000;
    return seconds < 60     ? QStringLiteral("%1s").arg(seconds)
        : seconds < 3600    ? QStringLiteral("%1m").arg(seconds / 60)
                            : QStringLiteral("%1h").arg(seconds / 3600);
}

QString clockText(qint64 ms)
{
    const qint64 seconds = std::max<qint64>(0, ms) / 1000;
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString cardHeadText(const RoundtableCardModel& card)
{
    using Kind = RoundtableCardModel::Kind;
    switch (card.kind) {
    case Kind::Double:
        return card.keyed.size() > 2 ? QStringLiteral("Double · %1").arg(card.keyed.size())
                                     : QStringLiteral("Double");
    case Kind::You:
        return QStringLiteral("You · on the air");
    case Kind::LastDouble:
        return QStringLiteral("Last double");
    case Kind::Talking:
    case Kind::LastTalker:
        if (!card.keyed.isEmpty()) { return card.keyed.first(); }
        return QStringLiteral("Unknown");
    case Kind::Unknown:
    case Kind::Quiet:
        break;
    }
    return QStringLiteral("Unknown");
}

QString cardCaptionText(const RoundtableCardModel& card)
{
    using Kind = RoundtableCardModel::Kind;
    if (card.frozen) {
        return card.kind == Kind::Double ? QStringLiteral("Last known · combined") : QStringLiteral("Last known");
    }
    switch (card.kind) {
    case Kind::Double:
    case Kind::LastDouble:
        return QStringLiteral("Combined RF");
    case Kind::You:
        return QStringLiteral("Hearing you");
    case Kind::LastTalker:
        return QStringLiteral("Last heard %1 ago").arg(ageText(card.endedAgoMs));
    default:
        return QStringLiteral("Heard by");
    }
}

int cardGlyph(const RoundtableCardModel& card)
{
    using Kind = RoundtableCardModel::Kind;
    switch (card.kind) {
    case Kind::Talking: return static_cast<int>(RoundtableGlyph::Talking);
    case Kind::You: return static_cast<int>(RoundtableGlyph::You);
    case Kind::Double:
    case Kind::LastDouble: return static_cast<int>(RoundtableGlyph::Doubled);
    case Kind::Unknown: return static_cast<int>(RoundtableGlyph::Unknown);
    case Kind::LastTalker:
    case Kind::Quiet: break;
    }
    return static_cast<int>(RoundtableGlyph::Quiet);
}

QColor cardWhoColor(const RoundtableCardModel& card)
{
    using Kind = RoundtableCardModel::Kind;
    if (card.frozen || cardIsLast(card)) { return kColorLastText; }
    if (card.kind == Kind::Double) { return kColorDoubleText; }
    if (card.kind == Kind::You) { return kColorYou; }
    if (card.kind == Kind::Unknown) { return kColorUnknown; }
    return kColorText;
}

// The fill, the rail (striped for a double, D10) and the border of a card, and
// of a view of another frequency.
void paintCardFrame(QPainter& p, const QRect& rect, const RoundtableCardModel& card, bool dimmed)
{
    using Kind = RoundtableCardModel::Kind;
    const bool isDouble = card.kind == Kind::Double && !card.frozen;
    QColor border = roundtableCardColor(card);
    if (dimmed) { border = QColor(160, 160, 168, 140); }
    const QRectF body = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(kColorFill);
    p.drawRoundedRect(body, kCardRadius, kCardRadius);

    // The rail, inside the rounded corners: striped for a double (D10).
    {
        p.save();
        QPainterPath inner;
        inner.addRoundedRect(body, kCardRadius, kCardRadius);
        p.setClipPath(inner, Qt::IntersectClip);
        const QRectF rail(rect.left() + kCardBorder, rect.top() + kCardBorder, rect.width() - 2 * kCardBorder, kCardRail);
        if (isDouble) {
            paintRoundtableStripes(p, rail, kColorDoubleText, kColorDoubleRailOff, 5.0, 4.0);
        } else {
            p.fillRect(rail, border);
        }
        p.restore();
    }
    p.setPen(QPen(border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(body, kCardRadius, kCardRadius);
}

// The fonts of a reading row, made once a paint and not per row.
struct CardRowFonts {
    QFont call;
    QFont value;
    QFont age;
    QFontMetrics valueMetrics;
    QFontMetrics ageMetrics;
    explicit CardRowFonts(const QPaintDevice* device)
        : call(cardFont(device, 11, QFont::DemiBold))
        , value(cardFont(device, 11, QFont::DemiBold, true))
        , age(cardFont(device, 10, QFont::Medium, true))
        , valueMetrics(value, device)
        , ageMetrics(age, device)
    {
    }
};

// One reading row at `y`: who, the path when there is one, signal, age, and a
// meter beneath (D11). Returns its height. `h` is the text line's height.
int paintCardRow(QPainter& p, const CardRowFonts& fonts, const RoundtableCardModel& card, const RoundtableCardRow& row,
                 int bodyLeft, int bodyRight, int y, int h, const QString& format, const QString& path)
{
    using Kind = RoundtableCardModel::Kind;
    const bool frozen = card.frozen;
    const int bodyWidth = bodyRight - bodyLeft;
    const bool old = row.old;
    QColor callColor = kColorText;
    if (frozen) {
        callColor = kColorLastText;
    } else if (old) {
        callColor = kColorOldText;
    } else if (row.you) {
        callColor = kColorYou;
    }
    const QColor valueColor = frozen ? kColorLastText : old ? kColorOldText : kColorText;
    const QColor ageColor = old && !frozen ? kColorUnknown : kColorDim;

    const QString age = ageText(row.ageMs);
    const int ageWidth = std::max(kCardAgeMinWidth, fonts.ageMetrics.horizontalAdvance(age));
    const QString value = roundtableSignalText(row.dbm, format);
    const int valueWidth = fonts.valueMetrics.horizontalAdvance(value);
    const int ageLeft = bodyRight - ageWidth;
    const int valueLeft = ageLeft - kCardValueGap - valueWidth;
    p.setFont(fonts.age);
    p.setPen(ageColor);
    p.drawText(QRect(ageLeft, y, ageWidth, h), Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, age);
    p.setFont(fonts.value);
    p.setPen(valueColor);
    p.drawText(QRect(valueLeft, y, valueWidth + 1, h), Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, value);
    int callRight = valueLeft - kCardValueGap;
    if (!path.isEmpty()) {
        // The view's path column (D22), dim like the age.
        p.setFont(fonts.age);
        p.setPen(kColorDim);
        const int pathWidth = fonts.ageMetrics.horizontalAdvance(path);
        callRight -= pathWidth;
        p.drawText(QRect(callRight, y, pathWidth + 1, h), Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, path);
        callRight -= kCardValueGap;
    }
    p.setFont(fonts.call);
    p.setPen(callColor);
    const QString who = !row.label.isEmpty() ? row.label : row.you ? QStringLiteral("You") : row.call;
    p.drawText(QRect(bodyLeft, y, std::max(0, callRight - bodyLeft), h),
               Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, who);
    const int meterY = y + h + 1;

    const QRectF meter(bodyLeft, meterY, bodyWidth, kCardMeter);
    p.fillRect(meter, kColorMeterTrack);
    for (double mark : kMeterMarks) {
        p.fillRect(QRectF(std::floor(bodyLeft + mark * bodyWidth), meterY, 1, kCardMeter), Qt::black);
    }
    const qreal fraction = roundtableMeterFraction(row.dbm);
    const QRectF bar(bodyLeft, meterY, fraction * bodyWidth, kCardMeter);
    if (frozen) {
        p.fillRect(bar, kColorQuiet);
    } else if (old) {
        p.fillRect(bar, kColorOldBar);
    } else if (card.kind == Kind::Double) {
        if (row.you) {
            paintRoundtableStripes(p, bar, kColorYou, kColorYouMeterOff, 3.0, 3.0);
        } else {
            paintRoundtableStripes(p, bar, kColorDoubleText, kColorDoubleMeterOff, 3.0, 3.0);
        }
    } else if (cardIsLast(card)) {
        p.fillRect(bar, kColorQuiet);
    } else if (row.you || card.kind == Kind::You) {
        p.fillRect(bar, kColorYou);
    } else {
        p.fillRect(bar, card.kind == Kind::Unknown ? kColorUnknown : kColorTalking);
    }
    if (row.hasPeak && !old && !frozen) {
        const qreal tick = std::floor(bodyLeft + roundtableMeterFraction(row.peakDbm) * (bodyWidth - 1));
        p.fillRect(QRectF(tick, meterY, 1, kCardMeter), kColorPeak);
    }
    return h + 1 + kCardMeter;
}

} // namespace

QString roundtableSignalText(double dbm, const QString& format)
{
    // The board writes a true minus sign.
    QString number = QString::number(qRound(dbm));
    number.replace(QLatin1Char('-'), QChar(0x2212));
    if (format == QLatin1String("S-units")) { return roundtableSUnitText(dbm); }
    if (format == QLatin1String("Both")) { return roundtableSUnitText(dbm) + QLatin1Char(' ') + number; }
    return number;
}

QColor roundtableCardColor(const RoundtableCardModel& card)
{
    using Kind = RoundtableCardModel::Kind;
    if (card.frozen || cardIsLast(card)) { return kColorLastBorder; }
    switch (card.kind) {
    case Kind::Talking: return kColorTalking;
    case Kind::You: return kColorYou;
    case Kind::Double: return kColorDouble;
    case Kind::Unknown: return kColorUnknown;
    default: return kColorLastBorder;
    }
}

bool roundtableCardSameStructure(const RoundtableCardModel& a, const RoundtableCardModel& b)
{
    if (a.kind != b.kind || a.hz != b.hz || a.keyed != b.keyed || a.keyedYou != b.keyedYou || a.people != b.people
        || a.frozen != b.frozen || a.note.isEmpty() != b.note.isEmpty() || a.rows.size() != b.rows.size()
        || a.onFrequency.size() != b.onFrequency.size()) {
        return false;
    }
    for (qsizetype i = 0; i < a.rows.size(); ++i) {
        if (a.rows.at(i).call != b.rows.at(i).call || a.rows.at(i).you != b.rows.at(i).you) { return false; }
    }
    for (qsizetype i = 0; i < a.onFrequency.size(); ++i) {
        const RoundtableCardPerson& x = a.onFrequency.at(i);
        const RoundtableCardPerson& y = b.onFrequency.at(i);
        if (x.name != y.name || x.keyed != y.keyed || x.you != y.you || x.offsetHz != y.offsetHz) { return false; }
    }
    return true;
}

RoundtableCardPlacement placeRoundtableCard(const RoundtableCardModel& card, const QRect& pan, int dialX,
                                            const QVector<QRect>& flags, bool flagFlipped, const QFontMetrics& fm)
{
    RoundtableCardPlacement placed;
    if (pan.isEmpty()) { return placed; }

    // The stretches of the pan between keep-outs. Half-open: [left, right).
    QVector<QRect> blocks;
    blocks.reserve(flags.size());
    for (const QRect& flag : flags) {
        if (flag.intersects(pan)) { blocks.append(flag); }
    }
    std::sort(blocks.begin(), blocks.end(), [](const QRect& a, const QRect& b) { return a.left() < b.left(); });
    struct Span { int left; int right; };
    QVector<Span> spans;
    spans.reserve(blocks.size() + 1);
    int left = pan.left();
    const int panRight = pan.left() + pan.width();
    for (const QRect& block : std::as_const(blocks)) {
        const int end = block.left() - kCardFlagGap;
        if (end > left) { spans.append(Span{left, end}); }
        left = std::max(left, block.left() + block.width() + kCardFlagGap);
    }
    if (panRight > left) { spans.append(Span{left, panRight}); }

    // Nearest the dial, then widest. The far side of the flag first (D16).
    const auto choose = [&](bool farOnly) -> std::optional<Span> {
        std::optional<Span> best;
        int bestDistance = 0;
        for (const Span& span : std::as_const(spans)) {
            const int wide = span.right - span.left;
            if (wide < kCardMinWidth) { continue; }
            if (farOnly && !(flagFlipped ? span.right <= dialX : span.left >= dialX)) { continue; }
            const int distance = dialX < span.left ? span.left - dialX : dialX > span.right ? dialX - span.right : 0;
            if (!best || distance < bestDistance
                || (distance == bestDistance && wide > best->right - best->left)) {
                best = span;
                bestDistance = distance;
            }
        }
        return best;
    };
    std::optional<Span> span = choose(true);
    placed.beside = !span.has_value();
    if (!span) { span = choose(false); }
    if (!span) {
        placed.beside = false;
        return placed;
    }

    const int width = std::min(kCardWidth, span->right - span->left);
    const int limit = pan.height();
    const int rows = rowsThatFit(card, width, limit, fm);
    const CardPlan plan = planCard(card, width, rows, limit, fm);
    if (plan.height <= 0) {
        placed.beside = false;
        return placed;
    }
    placed.rows = rows;
    placed.moreRows = static_cast<int>(card.rows.size()) - rows;
    const int x = std::clamp(dialX - width / 2, span->left, span->right - width);
    placed.card = QRect(x, pan.top() + pan.height() - plan.height, width, plan.height);
    return placed;
}

void paintRoundtableStripes(QPainter& p, const QRectF& rect, const QColor& on, const QColor& off, qreal onWidth,
                            qreal offWidth)
{
    p.save();
    p.setClipRect(rect, Qt::IntersectClip);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect, off);
    p.setPen(Qt::NoPen);
    p.setBrush(on);
    // A 135 degree gradient's stripes lean like "/": each is a parallelogram.
    const qreal sqrt2 = std::sqrt(2.0);
    const qreal period = (onWidth + offWidth) * sqrt2;
    const qreal stripe = onWidth * sqrt2;
    const qreal height = rect.height();
    for (qreal x = rect.left() - height; x < rect.right(); x += period) {
        const QPointF corners[4] = {QPointF(x + height, rect.top()), QPointF(x + height + stripe, rect.top()),
                                    QPointF(x + stripe, rect.bottom()), QPointF(x, rect.bottom())};
        p.drawConvexPolygon(corners, 4);
    }
    p.restore();
}

void paintRoundtableCard(QPainter& p, const RoundtableCardModel& card, const RoundtableCardPlacement& placed,
                         const QString& format, const QFontMetrics& fm, bool dimmed)
{
    using Kind = RoundtableCardModel::Kind;
    if (placed.card.isNull()) { return; }
    const QRect rect = placed.card;
    const CardPlan plan = planCard(card, rect.width(), placed.rows, rect.height(), fm);
    const bool frozen = card.frozen;
    const bool isDouble = card.kind == Kind::Double && !frozen;
    const auto line = [&fm](int base) { return cardLine(base, fm); };
    const QPaintDevice* device = p.device();

    const QFont whoFont = cardFont(device, 13, QFont::Bold);
    const QFont nameFont = cardFont(device, 12, QFont::Bold);
    const QFont smallFont = cardFont(device, 10, QFont::Medium);
    const CardRowFonts rowFonts(device);
    const QFont clockFont = cardFont(device, 10, QFont::DemiBold, true);
    const QFontMetrics whoMetrics(whoFont, device);
    const QFontMetrics smallMetrics(smallFont, device);
    const QFontMetrics clockMetrics(clockFont, device);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (dimmed) { p.setOpacity(p.opacity() * kCardDimmedOpacity); }

    paintCardFrame(p, rect, card, dimmed);

    const int bodyLeft = rect.left() + kCardBorder + kCardPadSide;
    const int bodyRight = rect.left() + rect.width() - kCardBorder - kCardPadSide;
    const int bodyWidth = bodyRight - bodyLeft;
    int y = rect.top() + kCardBorder + kCardRail + kCardPadTop;

    // Head: glyph, who, clock, frequency.
    {
        const int h = line(kCardHeadLine);
        const bool grey = frozen || cardIsLast(card);
        paintRoundtableGlyph(p, QPointF(bodyLeft + kCardGlyphSize / 2.0, y + h / 2.0), kCardGlyphSize, cardGlyph(card),
                             grey, false);
        const int textLeft = bodyLeft + static_cast<int>(kCardGlyphSize) + kCardTextGap;
        const QString frequency = roundtableFrequencyText(static_cast<qint64>(card.hz));
        const int frequencyWidth = clockMetrics.horizontalAdvance(frequency);
        // The clock is on a double's head only (D10).
        const bool hasClock = card.kind == Kind::Double && card.clockMs > 0;
        const QString clock = hasClock ? clockText(card.clockMs) : QString();
        const int clockWidth = hasClock ? clockMetrics.horizontalAdvance(clock) + kCardTextGap : 0;
        const int room = std::max(0, bodyRight - textLeft - frequencyWidth - clockWidth - kCardTextGap);
        const QString who = whoMetrics.elidedText(cardHeadText(card), Qt::ElideRight, room);
        const QColor whoColor = cardWhoColor(card);
        p.setFont(whoFont);
        p.setPen(whoColor);
        p.drawText(QRect(textLeft, y, room, h), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, who);
        p.setFont(clockFont);
        if (hasClock) {
            p.setPen(kColorDim);
            p.drawText(QRect(textLeft + whoMetrics.horizontalAdvance(who) + kCardTextGap, y, clockWidth, h),
                       Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, clock);
        }
        p.setPen(frozen ? kColorDim : kColorFrequency);
        p.drawText(QRect(bodyRight - frequencyWidth, y, frequencyWidth, h),
                   Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, frequency);
        y += h;
    }

    // The keyed names of a double, side by side when two fit (D10). A name that
    // does not fit is cut, never overlapped.
    if (plan.names > 0) {
        y += kCardGap;
        const int h = line(kCardNameLine);
        const QColor dot = frozen ? kColorQuiet : kColorDoubleText;
        p.setFont(nameFont);
        const auto drawName = [&](const QString& name, int x, int lineY, int room) {
            const bool you = !card.keyedYou.isEmpty() && name == card.keyedYou;
            p.setPen(Qt::NoPen);
            p.setBrush(frozen ? dot : you ? kColorYou : dot);
            p.drawEllipse(QRectF(x, lineY + (h - kCardNameDot) / 2.0, kCardNameDot, kCardNameDot));
            p.setPen(frozen ? kColorLastText : you ? kColorYou : kColorText);
            p.setBrush(Qt::NoBrush);
            const int textX = x + kCardNameDot + kCardTextGap;
            p.drawText(QRect(textX, lineY, std::max(0, room - kCardNameDot - kCardTextGap), h),
                       Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                       you ? QStringLiteral("You") : name);
        };
        if (plan.namesInRow) {
            int x = bodyLeft;
            for (int i = 0; i < plan.names; ++i) {
                const int wide = kCardNameDot + kCardTextGap + fm.horizontalAdvance(card.keyed.at(i));
                drawName(card.keyed.at(i), x, y, wide + 1);
                x += wide + kCardNameRowGap;
            }
            y += h;
        } else {
            for (int i = 0; i < plan.names; ++i) {
                drawName(card.keyed.at(i), bodyLeft, y, bodyWidth);
                y += h;
                if (i + 1 < plan.names) { y += kCardNameGap; }
            }
        }
        y += kCardNamesPad;
    }
    p.setFont(smallFont);
    const auto smallLine = [&](const QString& text, const QColor& color, int indent = 0) {
        y += kCardGap;
        const int h = line(kCardSmallLine);
        p.setPen(color);
        p.drawText(QRect(bodyLeft + indent, y, bodyWidth - indent, h),
                   Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                   smallMetrics.elidedText(text, Qt::ElideRight, bodyWidth - indent));
        y += h;
    };
    if (plan.moreKeyed > 0) { smallLine(QStringLiteral("+%1 keyed").arg(plan.moreKeyed), kColorDim); }
    if (plan.lastNames) { smallLine(card.keyed.join(QStringLiteral(" + ")), kColorDim); }
    if (plan.note) {
        // Reconnecting is amber, with the waiting glyph (D7).
        const bool waiting = frozen && card.note == QLatin1String("Reconnecting");
        if (waiting) {
            paintRoundtableGlyph(p, QPointF(bodyLeft + 4.0, y + kCardGap + line(kCardSmallLine) / 2.0), 8.0,
                                 static_cast<int>(RoundtableGlyph::Waiting));
            p.setFont(smallFont);
        }
        smallLine(card.note, waiting ? kColorUnknown : kColorDim, waiting ? 12 : 0);
    }

    // Caption, with the signal format at the right (D10).
    if (plan.caption) {
        y += kCardGap;
        const int h = line(kCardCaption);
        p.setPen(QPen(kColorRule, 1));
        p.drawLine(QPointF(bodyLeft, y + 0.5), QPointF(bodyRight, y + 0.5));
        const QRect captionRect(bodyLeft, y + 4, bodyWidth, h - 4);
        p.setFont(smallFont);
        if (plan.rows > 0) {
            const QString unit = format == QLatin1String("S-units") ? QStringLiteral("S")
                : format == QLatin1String("Both") ? QStringLiteral("S / dBm") : QStringLiteral("dBm");
            p.setPen(kColorDim);
            p.drawText(captionRect, Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, unit);
        }
        p.setPen(isDouble ? kColorDoubleText : kColorDim);
        p.drawText(captionRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, cardCaptionText(card));
        y += h;
    }

    // One row per listener: who, signal, age, and a meter beneath (D11).
    for (int i = 0; i < plan.rows && i < card.rows.size(); ++i) {
        const RoundtableCardRow& row = card.rows.at(i);
        y += i == 0 ? kCardGap : kCardRowGap;
        y += paintCardRow(p, rowFonts, card, row, bodyLeft, bodyRight, y, line(kCardRowLine), format, QString());
    }
    if (plan.rows == 0 && plan.emptyLine) {
        p.setFont(smallFont);
        const bool waiting = !frozen && !dimmed && !card.note.isEmpty();
        smallLine(waiting ? QStringLiteral("Waiting for readings") : QStringLiteral("No readings yet"), kColorDim);
    }
    if (plan.moreRows > 0) {
        y += kCardGap;
        const int h = line(kCardFoot);
        p.setFont(smallFont);
        p.setPen(kColorFrequency);
        p.drawText(QRect(bodyLeft, y, bodyWidth, h), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                   QStringLiteral("+%1 more ›").arg(plan.moreRows));
    }
    p.restore();
}

namespace {

constexpr int kViewSectionGap = 4;   // .rc-peek .rc__body
constexpr int kViewPersonLine = 14;  // .pk-p
constexpr int kViewPersonGap = 2;    // .pk-people
constexpr int kViewPersonGlyph = 11;
constexpr int kViewOffsetMinWidth = 46;  // .pk-o
constexpr int kViewCloseRoom = 18;   // .rc-peek .rc__f
const QColor kColorViewSub(0x79, 0xa8, 0xd2);  // .pk-sub
const QColor kColorKeyedText = kColorTalking;

QString viewFamilyText(const QString& family)
{
    if (family == QLatin1String("fdvl")) { return QStringLiteral("FreeDV L"); }
    if (family == QLatin1String("fdvu")) { return QStringLiteral("FreeDV U"); }
    return family.toUpper();
}

QString viewCaptionText(const RoundtableCardModel& card)
{
    return cardCaptionText(card);
}

// Walks the view's words top to bottom. With a painter it draws them; without
// one it only measures, so the height and the drawing can never disagree.
// Returns the height used from the rect's top.
int runRoundtableView(QPainter* p, const RoundtableViewModel& view, const QRect& rect, int rowLimit,
                      const QPaintDevice* device)
{
    using Kind = RoundtableCardModel::Kind;
    const RoundtableCardModel& card = view.card;
    const bool frozen = card.frozen;
    const QFont whoFont = cardFont(device, 13, QFont::Bold);
    const QFont smallFont = cardFont(device, 10, QFont::Medium);
    const QFont personFont = cardFont(device, 11, QFont::DemiBold);
    const QFont clockFont = cardFont(device, 10, QFont::DemiBold, true);
    const QFontMetrics fm(cardFont(device, 12, QFont::Bold), device);
    const QFontMetrics whoMetrics(whoFont, device);
    const QFontMetrics smallMetrics(smallFont, device);
    const QFontMetrics personMetrics(personFont, device);
    const QFontMetrics clockMetrics(clockFont, device);
    const auto line = [&fm](int base) { return cardLine(base, fm); };

    const int bodyLeft = rect.left() + kCardBorder + kCardPadSide;
    const int bodyRight = rect.left() + rect.width() - kCardBorder - kCardPadSide;
    const int bodyWidth = bodyRight - bodyLeft;
    int y = rect.top() + kCardBorder + kCardRail + kCardPadTop;

    // Head: glyph, who, clock on any live turn, frequency, room for the close box.
    {
        const int h = line(kCardHeadLine);
        if (p != nullptr) {
            paintRoundtableGlyph(*p, QPointF(bodyLeft + kCardGlyphSize / 2.0, y + h / 2.0), kCardGlyphSize,
                                 cardGlyph(card), frozen || cardIsLast(card), false);
            const int textLeft = bodyLeft + static_cast<int>(kCardGlyphSize) + kCardTextGap;
            const QString frequency = roundtableFrequencyText(static_cast<qint64>(card.hz));
            const int frequencyWidth = clockMetrics.horizontalAdvance(frequency);
            const int frequencyRight = bodyRight - kViewCloseRoom;
            // A dropped link has no live clock (D7).
            const bool live = !frozen && !cardIsLast(card) && card.kind != Kind::Quiet && card.clockMs > 0;
            const QString clock = live ? clockText(card.clockMs) : QString();
            const int clockWidth = live ? clockMetrics.horizontalAdvance(clock) + kCardTextGap : 0;
            const int room = std::max(0, frequencyRight - textLeft - frequencyWidth - clockWidth - kCardTextGap);
            const QString who = whoMetrics.elidedText(roundtableViewHeadText(card), Qt::ElideRight, room);
            p->setFont(whoFont);
            p->setPen(card.kind == Kind::LastTalker ? kColorLastText : cardWhoColor(card));
            p->drawText(QRect(textLeft, y, room, h), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, who);
            p->setFont(clockFont);
            if (live) {
                p->setPen(kColorDim);
                p->drawText(QRect(textLeft + whoMetrics.horizontalAdvance(who) + kCardTextGap, y, clockWidth, h),
                            Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, clock);
            }
            p->setPen(frozen ? kColorDim : kColorFrequency);
            p->drawText(QRect(frequencyRight - frequencyWidth, y, frequencyWidth, h),
                        Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, frequency);
        }
        y += h;
    }

    const auto smallLine = [&](const QString& text, const QColor& color, int indent = 0) {
        y += kViewSectionGap;
        const int h = line(kCardSmallLine);
        if (p != nullptr) {
            p->setFont(smallFont);
            p->setPen(color);
            p->drawText(QRect(bodyLeft + indent, y, bodyWidth - indent, h),
                        Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                        smallMetrics.elidedText(text, Qt::ElideRight, bodyWidth - indent));
        }
        y += h;
    };
    const auto caption = [&](const QString& left, const QString& right, const QColor& leftColor) {
        y += kViewSectionGap;
        const int h = line(kCardCaption);
        if (p != nullptr) {
            p->setPen(QPen(kColorRule, 1));
            p->drawLine(QPointF(bodyLeft, y + 0.5), QPointF(bodyRight, y + 0.5));
            const QRect captionRect(bodyLeft, y + 4, bodyWidth, h - 4);
            p->setFont(smallFont);
            p->setPen(kColorDim);
            p->drawText(captionRect, Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, right);
            p->setPen(leftColor);
            p->drawText(captionRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, left);
        }
        y += h;
    };

    smallLine(roundtableViewSubText(view), kColorViewSub);
    const QString link = roundtableViewLinkText(card);
    if (!link.isEmpty()) {
        const bool waiting = card.note == QLatin1String("Reconnecting");
        if (waiting && p != nullptr) {
            paintRoundtableGlyph(*p, QPointF(bodyLeft + 4.0, y + kViewSectionGap + line(kCardSmallLine) / 2.0), 8.0,
                                 static_cast<int>(RoundtableGlyph::Waiting));
        }
        smallLine(link, waiting ? kColorUnknown : kColorDim, waiting ? 12 : 0);
    }

    // On frequency: each station, "keyed", and its offset from the viewed frequency.
    caption(QStringLiteral("On frequency · %1").arg(card.onFrequency.size()), QStringLiteral("Tuned"), kColorDim);
    if (card.onFrequency.isEmpty()) {
        smallLine(QStringLiteral("No stations named here"), kColorDim);
    }
    const int personLine = line(kViewPersonLine);
    for (qsizetype i = 0; i < card.onFrequency.size(); ++i) {
        const RoundtableCardPerson& person = card.onFrequency.at(i);
        y += i == 0 ? kViewSectionGap : kViewPersonGap;
        if (p != nullptr) {
            const int glyph = !person.keyed ? static_cast<int>(RoundtableGlyph::Quiet)
                : card.kind == Kind::Double ? static_cast<int>(RoundtableGlyph::Doubled)
                : person.you ? static_cast<int>(RoundtableGlyph::You) : static_cast<int>(RoundtableGlyph::Talking);
            paintRoundtableGlyph(*p, QPointF(bodyLeft + kViewPersonGlyph / 2.0, y + personLine / 2.0), kCardGlyphSize,
                                 glyph, frozen, false);
            const QString offset = roundtableViewOffsetText(person.offsetHz);
            const int offsetWidth = std::max(kViewOffsetMinWidth, smallMetrics.horizontalAdvance(offset));
            const int offsetLeft = bodyRight - offsetWidth;
            p->setFont(smallFont);
            p->setPen(kColorDim);
            p->drawText(QRect(offsetLeft, y, offsetWidth, personLine),
                        Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, offset);
            int nameRight = offsetLeft - kCardTextGap;
            if (person.keyed) {
                const QString keyed = QStringLiteral("keyed");
                const int keyedWidth = smallMetrics.horizontalAdvance(keyed);
                nameRight -= keyedWidth;
                p->setPen(frozen ? kColorDim : card.kind == Kind::Double ? kColorDoubleText : kColorKeyedText);
                p->drawText(QRect(nameRight, y, keyedWidth + 1, personLine),
                            Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, keyed);
                nameRight -= kCardTextGap;
            }
            const int nameLeft = bodyLeft + kViewPersonGlyph + kCardTextGap;
            p->setFont(personFont);
            p->setPen(person.keyed && !frozen ? kColorText : kColorLastText);
            // A callsign that does not fit is cut, never overlapped.
            p->drawText(QRect(nameLeft, y, std::max(0, nameRight - nameLeft), personLine),
                        Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                        person.you ? QStringLiteral("You") : person.name);
        }
        y += personLine;
    }

    // The readings, as the card's rows, with the path unless D2 blanks it.
    const QVector<RoundtableCardRow> rows = roundtableViewRows(view);
    const QString unit = view.format == QLatin1String("S-units") ? QStringLiteral("S")
        : view.format == QLatin1String("Both") ? QStringLiteral("S / dBm") : QStringLiteral("dBm");
    const bool combined = !frozen && card.kind == Kind::Double;
    caption(viewCaptionText(card), unit, combined ? kColorDoubleText : kColorDim);
    const int rowCount = std::min<int>(rowLimit, static_cast<int>(rows.size()));
    if (rows.isEmpty()) {
        smallLine(QStringLiteral("No readings yet"), kColorDim);
    } else if (rowCount > 0) {
        const bool showPath = card.kind != Kind::Double && card.kind != Kind::LastDouble && card.kind != Kind::Unknown;
        const int rowLine = line(kCardRowLine);
        if (p == nullptr) {
            y += kViewSectionGap + rowCount * (rowLine + 1 + kCardMeter) + (rowCount - 1) * kCardRowGap;
        } else {
            const CardRowFonts rowFonts(device);
            for (int i = 0; i < rowCount; ++i) {
                y += i == 0 ? kViewSectionGap : kCardRowGap;
                const RoundtableCardRow& row = rows.at(i);
                y += paintCardRow(*p, rowFonts, card, row, bodyLeft, bodyRight, y, rowLine, view.format,
                                  showPath ? roundtableViewPathText(row, view.miles, view.bearing) : QString());
            }
        }
    }
    if (rowCount < rows.size()) {
        y += kCardGap;
        const int h = line(kCardFoot);
        if (p != nullptr) {
            p->setFont(smallFont);
            p->setPen(kColorFrequency);
            p->drawText(QRect(bodyLeft, y, bodyWidth, h), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                        QStringLiteral("+%1 more ›").arg(rows.size() - rowCount));
        }
        y += h;
    }
    return y - rect.top();
}

} // namespace

QString roundtableViewHeadText(const RoundtableCardModel& card)
{
    using Kind = RoundtableCardModel::Kind;
    switch (card.kind) {
    case Kind::Quiet:
    case Kind::LastTalker:
        return QStringLiteral("Nobody talking");
    case Kind::Unknown:
        return QStringLiteral("Unknown signal");
    default:
        return cardHeadText(card);
    }
}

QString roundtableViewSubText(const RoundtableViewModel& view)
{
    const double awayKhz = (static_cast<double>(view.card.hz) - static_cast<double>(view.sliceHz)) / 1000.0;
    QString text = QStringLiteral("Viewing, not tuned");
    if (std::abs(awayKhz) >= 0.05) {
        // The board writes a true minus sign.
        text += QStringLiteral(" · %1%2 kHz from slice %3")
                    .arg(awayKhz > 0 ? QStringLiteral("+") : QString(QChar(0x2212)))
                    .arg(std::abs(awayKhz), 0, 'f', 1)
                    .arg(view.sliceName);
    }
    return text + QStringLiteral(" · ") + viewFamilyText(view.family);
}

QString roundtableViewLinkText(const RoundtableCardModel& card)
{
    if (card.note == QLatin1String("Reconnecting")) { return QStringLiteral("Reconnecting · last known"); }
    if (card.note.startsWith(QLatin1String("Offline"))) { return card.note + QStringLiteral(" · last known"); }
    return card.note;
}

QString roundtableViewOffsetText(qint64 offsetHz)
{
    if (offsetHz == 0) { return QStringLiteral("on freq"); }
    return QStringLiteral("%1%2 Hz")
        .arg(offsetHz > 0 ? QStringLiteral("+") : QString(QChar(0x2212)))
        .arg(std::abs(offsetHz));
}

QString roundtableViewPathText(const RoundtableCardRow& row, bool miles, bool bearing)
{
    QStringList parts;
    if (row.km) {
        // Same conversion as the Roundtable panel's path column.
        constexpr double kKmPerMile = 1.609344;
        const qint64 distance = std::llround(*row.km / (miles ? kKmPerMile : 1.0));
        parts.append(QStringLiteral("%1 %2").arg(QLocale().toString(distance), miles ? QStringLiteral("mi") : QStringLiteral("km")));
    }
    if (bearing && row.bearingDeg && *row.bearingDeg >= 0 && *row.bearingDeg < 360) {
        parts.append(QStringLiteral("%1°").arg(*row.bearingDeg, 3, 10, QLatin1Char('0')));
    }
    return parts.join(QLatin1Char(' '));
}

QVector<RoundtableCardRow> roundtableViewRows(const RoundtableViewModel& view)
{
    using Kind = RoundtableCardModel::Kind;
    const RoundtableCardModel& card = view.card;
    QVector<RoundtableCardRow> rows;
    rows.reserve(card.rows.size() + 1);
    const bool onAir = card.kind == Kind::Talking || card.kind == Kind::You || card.kind == Kind::Double
        || card.kind == Kind::Unknown;
    // Your pan speaks only for now, so it stays out of a frozen card (D7).
    if (onAir && !card.frozen && view.panDbm) {
        RoundtableCardRow pan;
        pan.label = QStringLiteral("Your pan");
        pan.you = true;
        pan.dbm = *view.panDbm;
        pan.peakDbm = view.panPeakDbm;
        pan.hasPeak = true;
        rows.append(pan);
    }
    rows += card.rows;
    return rows;
}

int roundtableViewHeight(const RoundtableViewModel& view, int width, int rowLimit, const QPaintDevice* device)
{
    return runRoundtableView(nullptr, view, QRect(0, 0, width, 0), rowLimit, device);
}

void paintRoundtableView(QPainter& p, const RoundtableViewModel& view, const QRect& rect, int rowLimit)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const bool dimmed = view.card.note == QLatin1String("You are hidden");
    if (dimmed) { p.setOpacity(p.opacity() * kCardDimmedOpacity); }
    paintCardFrame(p, rect, view.card, dimmed);
    runRoundtableView(&p, view, rect, rowLimit, p.device());
    p.restore();
}

QList<RoundtableTag> roundtableMergeDoubleTags(const QList<RoundtableTag>& tags)
{
    const auto sourcesOf = [](const RoundtableTag& tag) {
        QSet<QString> found;
        for (const QJsonValue& value : tag.stations) {
            const QString source = value.toObject().value(QStringLiteral("sourceRoom")).toString();
            if (!source.isEmpty()) { found.insert(source); }
        }
        return found;
    };
    QList<RoundtableTag> out;
    out.reserve(tags.size());
    QList<QSet<QString>> sources;
    sources.reserve(tags.size());
    for (const RoundtableTag& tag : tags) {
        const QSet<QString> source = tag.state == static_cast<int>(RoundtableGlyph::Doubled) ? sourcesOf(tag) : QSet<QString>();
        bool merged = false;
        if (!source.isEmpty()) {
            for (qsizetype i = 0; i < out.size() && !merged; ++i) {
                RoundtableTag& kept = out[i];
                // One double in one source room, even with no names known yet
                // or a frequency that holds stations of another room as well.
                if (!sources.at(i).intersects(source)
                    || (!tag.keyed.isEmpty() && !kept.keyed.isEmpty()
                        && std::none_of(tag.keyed.cbegin(), tag.keyed.cend(),
                                        [&kept](const QString& name) { return kept.keyed.contains(name); }))) {
                    continue;
                }
                for (const QString& name : tag.keyed) {
                    if (!kept.keyed.contains(name)) { kept.keyed.append(name); }
                }
                for (const QJsonValue& station : tag.stations) { kept.stations.append(station); }
                kept.members = static_cast<int>(kept.stations.size());
                kept.mine = kept.mine || tag.mine;
                kept.active = kept.active || tag.active;
                kept.mentionsMe = kept.mentionsMe || tag.mentionsMe;
                kept.mergedHz.append(tag.hz);
                merged = true;
            }
        }
        if (!merged) {
            out.append(tag);
            sources.append(source);
        }
    }
    return out;
}

} // namespace NereusSDR
