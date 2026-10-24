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
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
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
        visible.append(Visible{x, tag.hz, tag.roomId, tag.mine || tag.mentionsMe});
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

void paintRoundtableGlyph(QPainter& p, const QPointF& center, qreal size, int glyph, bool grey)
{
    const QColor color = grey ? roundtableStaleColor() : roundtableGlyphColor(glyph);
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
