// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableTagLayout.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableTagLayout.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 14).
// =================================================================

#include "RoundtableTagLayout.h"

#include "models/RoundtableModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// One tag on the pan before it is placed.
struct Visible {
    double x = 0.0;
    qint64 hz = 0;
    QString roomId;
    bool pinned = false;  // mine or mentionsMe: never folds
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

bool fits(const QList<Item>& items, double width, double tagWidth, double gap)
{
    const qsizetype n = items.size();
    return n * tagWidth + (n + 1) * gap <= width;
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
        if (value.toObject().value(QStringLiteral("mine")).toBool()) {
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
        return QStringLiteral("Doubled");
    case RoundtableGlyph::Unknown:
        return QStringLiteral("Unknown");
    default:
        return QStringLiteral("Quiet");
    }
}

} // namespace

QList<RoundtableTagPlacement> layoutRoundtableTags(const QList<RoundtableTag>& tags, double hzPerPixel,
                                                   qint64 leftHz, double width, double tagWidth,
                                                   double gap)
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
        visible.append(Visible{x, tag.hz, tag.roomId, tag.mine || tag.mentionsMe});
    }
    std::stable_sort(visible.begin(), visible.end(), [](const Visible& a, const Visible& b) {
        return a.hz != b.hz ? a.hz < b.hz : a.roomId < b.roomId;
    });

    // First only the groups that cannot move aside fold. If the tags still
    // don't fit across the pan, every overlapping group folds, and then the
    // groups reach further each round until they fit or span the pan.
    double reach = tagWidth + gap;
    bool forceFold = false;
    QList<Item> items;
    for (;;) {
        items = groupTags(visible, tagWidth, gap, reach, forceFold);
        if (fits(items, width, tagWidth, gap) || reach > width) {
            break;
        }
        if (!forceFold) {
            forceFold = true;
        } else {
            reach *= 1.5;
        }
    }

    // Side by side, as near their frequencies as the space allows: pushed
    // right off the left edge and their left neighbors, then back left off
    // the right edge and their right neighbors.
    QList<double> lefts;
    double edge = gap;
    for (const Item& item : items) {
        const double left = std::max(item.x - tagWidth / 2.0, edge);
        lefts.append(left);
        edge = left + tagWidth + gap;
    }
    edge = width - gap;
    for (qsizetype k = items.size() - 1; k >= 0; --k) {
        lefts[k] = std::min(lefts.at(k), edge - tagWidth);
        edge = lefts.at(k) - gap;
    }

    for (qsizetype k = 0; k < items.size(); ++k) {
        out.append(RoundtableTagPlacement{QRectF(lefts.at(k), 0.0, tagWidth, 0.0),
                                          items.at(k).roomIds, items.at(k).folded});
    }
    return out;
}

QList<RoundtableTag> roundtableTagsFor(const RoundtableModel* model, const QList<int>& sliceIds)
{
    QList<RoundtableTag> out;
    if (model == nullptr) {
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
    case LinkState::Offline:
        return RoundtableTagDisplay::Stale;
    case LinkState::Live:
    case LinkState::Reconnecting:
    case LinkState::Hidden:
        return RoundtableTagDisplay::Live;
    }
    return RoundtableTagDisplay::Hidden;
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
