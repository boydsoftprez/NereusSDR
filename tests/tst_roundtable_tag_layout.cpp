// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_tag_layout.cpp  (NereusSDR)
// =================================================================
//
// The Roundtable tags on a panadapter (design 11.2 Panadapter tags, U10):
// where layoutRoundtableTags puts them and which fold. Crowded tags fold
// into +N; a fold holds two or more; a lone neighbor moves aside; folds
// widen only when the tags still don't fit; your slice's room and rooms
// that name you never fold; zooming in opens folds. Then the tags a
// RoundtableModel gives (labels, members, mine, mentions, your own turn)
// and how a pan shows them for each link state (U4). Then a pan's drawing:
// the tags along the foot of the spectrum, clear of the VFO flags and the
// band plan; a click tunes, +N lists its rooms by frequency and a row
// tunes; offline greys them, connecting hides them; no tune while keyed.
// Collapsing the pan's toolbar lays the tags out again in the independent
// GPU child, or schedules a CPU repaint. Reports leave the GPU caches clean.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-04: placeRoundtableInspector. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-11-03: Overlay redesign truth rules and card model (design D1 to D9,
//               D12, D13, D19). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-30: Approved frequency-first Roundtable desktop surfaces.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-24: Created (Roundtable plan Task 14). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-24: Roundtable plan Task 15: the toolbar collapse case; your
//               own turn is the one you talk in, not `mine`.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-25: while you are hidden the pan shows its tags dimmed, the
//               rooms, talkers and counts as when live.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25: the hidden tags dim the same through the GPU overlay's
//               blend as on the CPU path. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>

#include <QAbstractButton>
#include <QAction>
#include <QContextMenuEvent>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QScopeGuard>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "core/ConnectionState.h"
#include "core/WdspTypes.h"
#include "gui/applets/RoundtableApplet.h"
#include "gui/SpectrumOverlayPanel.h"
#define private public
#include "gui/SpectrumWidget.h"
#undef private
#include "gui/roundtable/RoundtableFrequencyPeek.h"
#include "gui/roundtable/RoundtableTagLayout.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/VfoWidget.h"
#include "models/BandPlanManager.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr double kTagWidth = 80.0;
constexpr double kGap = 4.0;
// A pan short enough that the open toolbar reaches the tag row.
constexpr int kShortPanHeight = 300;

// A station as the directory and a view list it.
QJsonObject cardPerson(const char* source, const char* ref, const char* call, qint64 hz, bool keyed)
{
    return QJsonObject{{"sourceRoom", source}, {"ref", ref}, {"callsign", call}, {"hz", hz},
                       {"family", "lsb"}, {"keyed", keyed}};
}

QJsonObject cardTurn(qint64 id, const char* kind, qint64 hz, const char* source, qint64 start,
                     const QList<QJsonObject>& talkers)
{
    QJsonArray names;
    for (const QJsonObject& talker : talkers) { names.append(talker); }
    return QJsonObject{{"id", id}, {"kind", kind}, {"hz", hz}, {"sourceRoom", source}, {"start", start},
                       {"end", QJsonValue::Null}, {"talkers", names}};
}

// A listener with a present reading of a turn.
QJsonObject cardReader(const char* source, const char* ref, const char* call, qint64 turn, double dbm, qint64 t)
{
    QJsonObject member = cardPerson(source, ref, call, 7185000, false);
    member.insert("reading", QJsonObject{{"turn", turn}, {"dbm", dbm}, {"present", true}, {"t", t}});
    return member;
}

RoundtableTag tagAt(const QString& id, qint64 hz, bool mine = false, bool mentionsMe = false)
{
    RoundtableTag tag;
    tag.roomId = id;
    tag.hz = hz;
    tag.state = static_cast<int>(RoundtableGlyph::Talking);
    tag.label = id;
    tag.members = 3;
    tag.mine = mine;
    tag.mentionsMe = mentionsMe;
    return tag;
}

// Five rooms 50 Hz apart on 7.185 MHz; `special` (0 to 4) is mine or
// names you, -1 for none.
QList<RoundtableTag> fiveClose(int special = -1, bool mentions = false)
{
    QList<RoundtableTag> tags;
    for (int i = 0; i < 5; ++i) {
        const bool pinned = i == special;
        tags.append(tagAt(QStringLiteral("r%1").arg(i), 7185000 + i * 50, pinned && !mentions,
                          pinned && mentions));
    }
    return tags;
}

bool anyOverlap(const QList<RoundtableTagPlacement>& placements)
{
    for (int i = 0; i < placements.size(); ++i) {
        for (int j = i + 1; j < placements.size(); ++j) {
            const QRectF a = placements.at(i).rect;
            const QRectF b = placements.at(j).rect;
            if (a.left() < b.right() && b.left() < a.right()) {
                return true;
            }
        }
    }
    return false;
}

class NullLink : public IStationLink {
public:
    bool frequency = false;
    bool roundtableFrequencyAvailable() const override { return frequency; }
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    CommandOutcome requestRoundtableReadAt(qint64, const QString&) override { return {true, {}}; }
    bool roundtableAvailable() const override { return true; }
};

QJsonObject summary(const QString& id, qint64 hz, const QString& state, const QJsonArray& talkers,
                    int members)
{
    return QJsonObject{{QStringLiteral("id"), id},         {QStringLiteral("hz"), hz},
                       {QStringLiteral("state"), state},   {QStringLiteral("talkers"), talkers},
                       {QStringLiteral("members"), members}};
}

QtMessageHandler g_previousHandler = nullptr;

constexpr double kPanCenterHz = 7'200'000.0;
constexpr double kPanSpanHz = 192'000.0;

// A 40 m pan 1200 x 760 with the band plan on and slice 0's flag at
// 7.200, drawn by the CPU painter into an image the pan's size.
struct Pan {
    BandPlanManager plans;
    SpectrumWidget widget;
    VfoWidget* flag = nullptr;

    explicit Pan(int height = 760)
    {
        plans.loadPlans();
        widget.setUpdatesEnabled(false);
        widget.resize(1200, height);
        widget.setSampleRate(kPanSpanHz);
        widget.setDdcCenterFrequency(kPanCenterHz);
        widget.setFrequencyRange(kPanCenterHz, kPanSpanHz);
        widget.setBandPlanManager(&plans);
        widget.setConnectionState(ConnectionState::Connected);
        flag = widget.addVfoWidget(0);
        flag->setFrequency(kPanCenterHz);
        widget.show();
        widget.updateVfoPositions();
    }

    QImage draw()
    {
        QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        QPainter painter(&image);
        widget.drawRoundtableTagsForTest(painter, widget.roundtableTagSpectrumRectForTest());
        return image;
    }
};

QList<RoundtableTag> panTags()
{
    QList<RoundtableTag> tags;
    // Under the flag, two lone rooms, and five 50 Hz apart that fold.
    RoundtableTag mine = tagAt(QStringLiteral("lsb-7200000"), 7200000, true);
    mine.label = QStringLiteral("W1TLK");
    tags.append(mine);
    RoundtableTag quiet = tagAt(QStringLiteral("lsb-7150000"), 7150000);
    quiet.state = static_cast<int>(RoundtableGlyph::Quiet);
    quiet.label = QStringLiteral("Quiet");
    tags.append(quiet);
    RoundtableTag doubled = tagAt(QStringLiteral("lsb-7250000"), 7250000);
    doubled.state = static_cast<int>(RoundtableGlyph::Doubled);
    doubled.label = QStringLiteral("Double");
    tags.append(doubled);
    for (int i = 0; i < 5; ++i) {
        RoundtableTag close = tagAt(QStringLiteral("lsb-%1").arg(7185000 + i * 50), 7185000 + i * 50);
        close.label = QStringLiteral("K%1ABC").arg(i);
        tags.append(close);
    }
    return tags;
}

const SpectrumWidget::RoundtableTagHit* hitFor(const SpectrumWidget& widget, const QString& roomId)
{
    for (const SpectrumWidget::RoundtableTagHit& hit : widget.roundtableTagHitsForTest()) {
        if (hit.roomIds.contains(roomId)) {
            return &hit;
        }
    }
    return nullptr;
}

// A card with `rows` listeners, nearest first.
RoundtableCardModel talkingCard(int rows = 3, RoundtableCardModel::Kind kind = RoundtableCardModel::Kind::Talking)
{
    RoundtableCardModel card;
    card.kind = kind;
    card.hz = 7200000;
    card.keyed = kind == RoundtableCardModel::Kind::Double ? QStringList{QStringLiteral("W1AW"), QStringLiteral("K4ABC")}
                                                           : QStringList{QStringLiteral("W1AW")};
    card.clockMs = 83000;
    const QStringList calls{"N4RDR", "KD8LMN", "AA4QQ", "VE3XYZ", "W9ABC", "K1ABC", "N3XYZ"};
    for (int i = 0; i < rows; ++i) {
        RoundtableCardRow row;
        row.call = calls.at(i % calls.size());
        row.dbm = -66.0 - 8.0 * i;
        row.ageMs = 2000 + 1000 * i;
        row.peakDbm = row.dbm + 6.0;
        row.hasPeak = true;
        card.rows.append(row);
    }
    return card;
}

QFontMetrics cardMetrics()
{
    QFont font = WorkspacePreview::readableFont(QFont());
    font.setPixelSize(WorkspacePreview::readableTextPixelSize(12));
    font.setWeight(QFont::Bold);
    return QFontMetrics(font);
}

// The board's desktop band: dial 570 of a 1143 wide pan, the tool strip to 80,
// the scale from 1109, the foot at 303, and a flag 250 wide.
const QRect kBoardPan(80, 32, 1109 - 80, 303 - 32);

QRect boardFlag(int dialX, bool flipped = false)
{
    return flipped ? QRect(dialX - 252, 30, 276, 162) : QRect(dialX - 24, 30, 276, 162);
}

bool hasColor(const QImage& image, const QRect& area, const QColor& want)
{
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (QColor(image.pixel(x, y)).name() == want.name()) {
                return true;
            }
        }
    }
    return false;
}

constexpr int kAgeColumnWidth = 16;  // kCardAgeMinWidth: every age has this much
constexpr double kSameColorCosine = 0.97;  // The card's grey text reads 0.92 against amber.
constexpr double kBlendMinCoverage = 0.33;
constexpr double kBlendMaxOffLine = 16.0;
constexpr double kBlendMinLine = 100.0;  // Darker colors are too near the background to tell.

// Text never need hold its exact color: antialiasing blends a glyph into what
// is behind it, and a thin stem in a small font may never reach full coverage.
// These two probes read text by the blend, so they hold with any font.

// The color most of `area` is filled with: what text there is drawn over.
QColor backgroundIn(const QImage& image, const QRect& area)
{
    QHash<QRgb, int> counts;
    QRgb most = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            const QRgb rgb = image.pixel(x, y);
            if (++counts[rgb] > counts.value(most)) { most = rgb; }
        }
    }
    return QColor(most);
}

// How far `pixel` lies from `background` toward `want` (1 is all the way), and
// how far off that line it lies, in color levels.
struct BlendReading {
    double coverage = 0.0;
    double offLine = 0.0;
};

BlendReading readBlend(const QColor& pixel, const QColor& background, const QColor& want)
{
    const std::array<double, 3> line{double(want.red() - background.red()), double(want.green() - background.green()),
                                     double(want.blue() - background.blue())};
    const std::array<double, 3> ink{double(pixel.red() - background.red()), double(pixel.green() - background.green()),
                                    double(pixel.blue() - background.blue())};
    const double lengthSquared = line[0] * line[0] + line[1] * line[1] + line[2] * line[2];
    BlendReading reading;
    reading.coverage = (ink[0] * line[0] + ink[1] * line[1] + ink[2] * line[2]) / lengthSquared;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const double rest = ink[i] - reading.coverage * line[i];
        reading.offLine += rest * rest;
    }
    reading.offLine = std::sqrt(reading.offLine);
    return reading;
}

// True when the ink in `area` that differs from `before` adds up to `want`:
// its edges and any color fringes sum to the way the color lies from the
// background, and any other color of text points elsewhere.
bool changedInkIsColor(const QImage& before, const QImage& image, const QRect& area, const QColor& want)
{
    const QColor background = backgroundIn(image, area);
    std::array<double, 3> sum{0.0, 0.0, 0.0};
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (before.pixel(x, y) == image.pixel(x, y)) { continue; }
            const QColor pixel(image.pixel(x, y));
            sum[0] += pixel.red() - background.red();
            sum[1] += pixel.green() - background.green();
            sum[2] += pixel.blue() - background.blue();
        }
    }
    const std::array<double, 3> line{double(want.red() - background.red()), double(want.green() - background.green()),
                                     double(want.blue() - background.blue())};
    const double sumLength = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
    const double lineLength = std::sqrt(line[0] * line[0] + line[1] * line[1] + line[2] * line[2]);
    if (sumLength < lineLength) { return false; }  // Less ink than one whole pixel.
    return (sum[0] * line[0] + sum[1] * line[1] + sum[2] * line[2]) / (sumLength * lineLength) >= kSameColorCosine;
}

// True when any pixel of `area` is `want`, or a bright `want` blended over the
// area's background at a third coverage or more. For checks that a color is absent.
bool hasColorOrBlend(const QImage& image, const QRect& area, const QColor& want)
{
    if (hasColor(image, area, want)) { return true; }
    const QColor background = backgroundIn(image, area);
    if (std::hypot(want.red() - background.red(), want.green() - background.green(), want.blue() - background.blue())
        < kBlendMinLine) {
        return false;
    }
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            const BlendReading reading = readBlend(QColor(image.pixel(x, y)), background, want);
            if (reading.coverage >= kBlendMinCoverage && reading.offLine <= kBlendMaxOffLine) {
                return true;
            }
        }
    }
    return false;
}

const SpectrumWidget::RoundtableTagHit* cardHit(const SpectrumWidget& widget)
{
    for (const SpectrumWidget::RoundtableTagHit& hit : widget.roundtableTagHitsForTest()) {
        if (hit.kind == SpectrumWidget::RoundtableTagHit::Kind::Card) {
            return &hit;
        }
    }
    return nullptr;
}

// A live frequency-capable model and a clock the test moves.
struct CardRig {
    NullLink link;
    RoundtableState state;
    RoundtableModel model;
    qint64 now = 1800000000000LL;

    CardRig()
    {
        link.frequency = true;
        model.attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        RoundtableModel::setClockForTest([this] { return now; });
        model.applyRooms(QJsonObject{{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
    }
    ~CardRig()
    {
        RoundtableModel::setClockForTest({});
        model.detach();
    }
    // The directory as the wire sends it: each source room with its state.
    void rooms(const QJsonArray& summaries)
    {
        model.applyRooms(QJsonObject{{"frequencyVersion", 1}, {"rooms", summaries}});
    }
    void directory(const QList<QJsonObject>& stations)
    {
        QJsonArray list;
        for (const QJsonObject& station : stations) { list.append(station); }
        model.applyRooms(QJsonObject{{"frequencyVersion", 1}, {"rooms", QJsonArray{
            QJsonObject{{"id", "lsb-7184000"}, {"stations", list}}}}});
    }
    void view(qint64 hz, const QList<QJsonObject>& open, const QList<QJsonObject>& ended,
              const QList<QJsonObject>& members)
    {
        const auto array = [](const QList<QJsonObject>& items) {
            QJsonArray out;
            for (const QJsonObject& item : items) { out.append(item); }
            return out;
        };
        model.applyRoom(QJsonObject{{"id", QStringLiteral("lsb-%1").arg(hz)}, {"view", "frequency"},
            {"family", "lsb"}, {"hz", hz}, {"members", array(members)}, {"now", array(open)},
            {"earlier", array(ended)}, {"messages", QJsonArray{}}});
    }
    RoundtableCardModel card(qint64 hz, RoundtablePeakTracker* peaks = nullptr) const
    {
        return roundtableCardModelFor(&model, {}, RoundtableModel::frequencyContextId(hz, QStringLiteral("lsb")),
                                      static_cast<quint64>(hz), peaks);
    }
};

} // namespace

class TestRoundtableTagLayout : public QObject {
    Q_OBJECT

private slots:
    void exactCochannelPeopleStayDistinctFromGeometricFolds()
    {
        NullLink link;
        link.frequency = true;
        RoundtableState state;
        RoundtableModel model;
        model.attachStation(&link, &state);
        QJsonArray stations;
        for (int slice = 0; slice < 2; ++slice) {
            stations.append(QJsonObject{{QStringLiteral("ref"), QStringLiteral("same-ref")},
                {QStringLiteral("sourceRoom"), QStringLiteral("lsb-%1").arg(7185000 + slice * 1000)},
                {QStringLiteral("slice"), slice}, {QStringLiteral("hz"), 7185123},
                {QStringLiteral("family"), QStringLiteral("lsb")}, {QStringLiteral("callsign"), QStringLiteral("W4ABC")},
                {QStringLiteral("nth"), slice + 1}, {QStringLiteral("keyed"), slice == 0}});
        }
        QJsonObject summary{{QStringLiteral("id"), QStringLiteral("lsb-7185000")},
            {QStringLiteral("hz"), 7185000}, {QStringLiteral("stations"), stations}};
        model.applyRooms(QJsonObject{{QStringLiteral("frequencyVersion"), 1}, {QStringLiteral("rooms"), QJsonArray{summary}}});
        const auto tags = roundtableTagsFor(&model, {});
        QCOMPARE(tags.size(), 1);
        QCOMPARE(tags.first().hz, 7185123LL);
        QCOMPARE(tags.first().roomId, QStringLiteral("frequency:lsb-7185123"));
        QCOMPARE(tags.first().stations.size(), 2);
        QVERIFY(tags.first().stations.at(0).toObject().value(QStringLiteral("sourceRoom"))
                != tags.first().stations.at(1).toObject().value(QStringLiteral("sourceRoom")));
        const auto layout = layoutRoundtableTags(tags, 100, 7150000, 800, 80, 4);
        QCOMPARE(layout.size(), 1);
        QVERIFY(!layout.first().folded);
    }

    void ownTurnsRequireEveryScopedEndpointInFrequencyViews()
    {
        const QJsonObject ownA{{"sourceRoom", "lsb-7184000"}, {"ref", "same"}, {"you", true}, {"keyed", true}};
        const QJsonObject ownB{{"sourceRoom", "lsb-7188000"}, {"ref", "second"}, {"you", true}, {"keyed", false}};
        QJsonObject room{{"id", "lsb-7185123"}, {"view", "frequency"}, {"members", QJsonArray{ownA, ownB}}};
        QJsonObject other{{"kind", "talking"}, {"sourceRoom", "lsb-7188000"}, {"talkers", QJsonArray{
            QJsonObject{{"sourceRoom", "lsb-7188000"}, {"ref", "same"}}}}};
        // Own A is keyed, but the same wire ref at B is another endpoint.
        QVERIFY(!roundtableTurnIsYours(room, other, true));
        other.insert("talkers", QJsonArray{QJsonObject{{"sourceRoom", "lsb-7188000"}, {"ref", "unrelated"}}});
        QVERIFY(!roundtableTurnIsYours(room, other, true));
        other.insert("talkers", QJsonArray{QJsonObject{{"sourceRoom", "lsb-7188000"}, {"ref", "second"}}});
        QVERIFY(roundtableTurnIsYours(room, other, true));
        QVERIFY(roundtableTurnIsYours(room, other, false));
        other.insert("kind", "doubled");
        QVERIFY(!roundtableTurnIsYours(room, other, true));
        other.insert("kind", "talking");
        other.insert("talkers", QJsonArray{});
        QVERIFY(!roundtableTurnIsYours(room, other, true));
        // Only the legacy single RF room keeps its scoped keyed fallback.
        room.remove("view");
        room.insert("id", "lsb-7184000");
        room.insert("members", QJsonArray{ownA});
        other.insert("sourceRoom", "lsb-7184000");
        QVERIFY(roundtableTurnIsYours(room, other, true));
        QVERIFY(!roundtableTurnIsYours(room, other, false));
        other.insert("sourceRoom", "lsb-7188000");
        QVERIFY(!roundtableTurnIsYours(room, other, true));
    }

    void frequencyFlagsDoNotClaimOtherFrequencyTurnsOrReports()
    {
        NullLink link;
        link.frequency = true;
        RoundtableState state;
        RoundtableModel model;
        model.attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        QJsonObject own{{"sourceRoom", "lsb-7184000"}, {"ref", "same"}, {"keyed", true},
            {"hz", 7185123}, {"family", "lsb"}, {"callsign", "KG4VCF"}};
        QJsonObject other{{"sourceRoom", "lsb-7188000"}, {"ref", "same"}, {"keyed", true},
            {"hz", 7188000}, {"family", "lsb"}, {"callsign", "K4ABC"}};
        model.applyRooms(QJsonObject{{"frequencyVersion", 1}, {"rooms", QJsonArray{
            QJsonObject{{"id", "lsb-7184000"}, {"stations", QJsonArray{own, other}}}}}});
        const QJsonObject turnA{{"id", 1}, {"kind", "talking"}, {"hz", 7185123}, {"sourceRoom", "lsb-7184000"},
            {"start", 1}, {"talkers", QJsonArray{own}}};
        const QJsonObject turnB{{"id", 100}, {"kind", "talking"}, {"hz", 7188000}, {"sourceRoom", "lsb-7188000"},
            {"start", 2}, {"talkers", QJsonArray{other}}};
        const QJsonObject reporterA{{"ref", "reportA"}, {"sourceRoom", "lsb-7184000"},
            {"reading", QJsonObject{{"present", true}, {"turn", 1}, {"dbm", -73}, {"t", 1}}}};
        const QJsonObject reporterB{{"ref", "reportB"}, {"sourceRoom", "lsb-7188000"},
            {"reading", QJsonObject{{"present", true}, {"turn", 100}, {"dbm", -55}, {"t", 1}}}};
        for (qint64 hz : {7185123LL, 7188000LL}) {
            // The wire marks you on the member alone.
            QJsonObject ownMember = own;
            ownMember.insert("you", true);
            model.applyRoom(QJsonObject{{"id", QStringLiteral("lsb-%1").arg(hz)}, {"view", "frequency"},
                {"family", "lsb"}, {"hz", hz}, {"members", QJsonArray{ownMember, other, reporterA, reporterB}},
                {"now", QJsonArray{turnA, turnB}}, {"earlier", QJsonArray{}}, {"messages", QJsonArray{}}});
        }
        const auto flagB = roundtableFlagLineFor(&model, 1, 7188000, QStringLiteral("lsb"));
        QVERIFY(flagB.glyph != static_cast<int>(RoundtableGlyph::You));
        QVERIFY(!flagB.text.contains(QStringLiteral("hear you")));
        const auto flagA = roundtableFlagLineFor(&model, 0, 7185123, QStringLiteral("lsb"));
        QCOMPARE(flagA.glyph, static_cast<int>(RoundtableGlyph::You));
        QCOMPARE(flagA.text, QStringLiteral("Roundtable · 1 hear you"));
    }

    void scopedCochannelReportsAndPlacementDedup()
    {
        NullLink link;
        link.frequency = true;
        RoundtableState state;
        RoundtableModel model;
        model.attachStation(&link, &state);
        const QJsonObject person{{"sourceRoom", "lsb-7184000"}, {"ref", "one"}, {"slice", 0},
            {"hz", 7185123}, {"family", "lsb"}, {"callsign", "W1AW"}};
        QJsonObject secondSlice = person;
        secondSlice.insert("slice", 1);
        QJsonObject otherPerson = person;
        otherPerson.insert("ref", "two");
        QJsonObject otherHz = person;
        otherHz.insert("hz", 7185500);
        model.applyRooms(QJsonObject{{"frequencyVersion", 1}, {"rooms", QJsonArray{
            QJsonObject{{"id", "lsb-7184000"}, {"stations", QJsonArray{person, secondSlice, otherPerson, otherHz}}}}}});
        const auto tags = roundtableTagsFor(&model, {});
        QCOMPARE(tags.size(), 2);
        QCOMPARE(tags.first().stations.size(), 2);
        QCOMPARE(tags.last().stations.size(), 1);
        QJsonObject talker{{"sourceRoom", "lsb-7184000"}, {"ref", "one"}, {"callsign", "W1AW"}};
        QJsonObject turn{{"id", 12}, {"kind", "talking"}, {"hz", 7185123}, {"sourceRoom", "lsb-7184000"},
            {"start", 1000}, {"talkers", QJsonArray{talker}}};
        QJsonObject member{{"sourceRoom", "lsb-7184000"}, {"ref", "two"}, {"callsign", "K4ABC"},
            {"reading", QJsonObject{{"turn", 12}, {"dbm", -73}, {"present", true}, {"t", 2000}}}};
        QJsonObject view{{"family", "lsb"}, {"now", QJsonArray{turn}}, {"members", QJsonArray{member}}};
        auto rows = roundtableTagReportLines(tags.first(), view, 27000);
        QCOMPARE(rows.first(), QStringLiteral("Talking: W1AW"));
        QVERIFY(rows.contains(QStringLiteral("K4ABC  -73  25s stale")));
        // Off the dial but within 5 kHz: the nearest live turn, its readings.
        turn.insert("hz", 7185500);
        view.insert("now", QJsonArray{turn});
        rows = roundtableTagReportLines(tags.first(), view, 27000);
        QCOMPARE(rows.first(), QStringLiteral("Talking: W1AW"));
        QVERIFY(rows.contains(QStringLiteral("K4ABC  -73  25s stale")));
        turn.insert("hz", 7185123 + 5001);
        view.insert("now", QJsonArray{turn});
        QCOMPARE(roundtableTagReportLines(tags.first(), view, 27000), QStringList{});
        turn.insert("hz", 7185123);
        turn.insert("kind", "doubled");
        view.insert("now", QJsonArray{turn});
        rows = roundtableTagReportLines(tags.first(), view, 27000);
        QVERIFY(rows.join(' ').contains(QStringLiteral("Combined RF")));
        QVERIFY(rows.join(' ').contains(QStringLiteral("-73")));
        turn.insert("kind", "talking");
        turn.insert("id", 13);
        view.insert("now", QJsonArray{turn});
        rows = roundtableTagReportLines(tags.first(), view, 27000);
        QVERIFY(!rows.join(' ').contains(QStringLiteral("K4ABC")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("-73")));
    }

    void oneReporterAndDoublesUseScopedCurrentTurnSamples()
    {
        AppSettings::instance().setValue(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("Both"));
        const auto restore = qScopeGuard([] { AppSettings::instance().remove(QStringLiteral("Roundtable/SignalFormat")); });
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        const QJsonObject a{{"sourceRoom", "lsb-7184000"}, {"ref", "a"}, {"family", "lsb"},
            {"hz", tag.hz}, {"callsign", "W1AW"}, {"keyed", true}};
        QJsonObject b = a;
        b.insert("ref", "b"); b.insert("callsign", "K4ABC"); b.insert("nth", 2);
        QJsonObject c = a;
        c.insert("ref", "c"); c.insert("callsign", "N3XYZ");
        QJsonObject listener{{"sourceRoom", "lsb-7184000"}, {"ref", "rx"}, {"callsign", "VK3ABC/P"},
            {"reading", QJsonObject{{"turn", 12}, {"present", true}, {"dbm", -61}, {"t", 25000}}}};
        // One directory entry is enough: it may be the only receiver.
        tag.stations = {listener};
        QJsonObject turn{{"id", 12}, {"kind", "talking"}, {"hz", tag.hz}, {"sourceRoom", "lsb-7184000"},
            {"start", 1000}, {"end", QJsonValue::Null}, {"talkers", QJsonArray{a}}};
        QJsonObject view{{"family", "lsb"}, {"now", QJsonArray{turn}}, {"members", QJsonArray{listener}}};
        QStringList rows = roundtableTagReportLines(tag, view, 27000);
        QVERIFY2(rows.join(' ').contains(QStringLiteral("VK3ABC/P  S9 +12 / -61  2s")), qPrintable(rows.join(QLatin1Char('\n'))));
        QVERIFY(rows.first().contains(QStringLiteral("W1AW")));
        turn.insert("kind", "doubled"); turn.insert("talkers", QJsonArray{a, b, c});
        QJsonObject transmitting = b;
        transmitting.insert("reading", listener.value("reading"));
        QJsonObject wrongSource = listener;
        wrongSource.insert("sourceRoom", "lsb-7190000"); wrongSource.insert("callsign", "WRONG");
        view.insert("now", QJsonArray{turn}); view.insert("members", QJsonArray{listener, a, transmitting, c, wrongSource});
        rows = roundtableTagReportLines(tag, view, 27000);
        QVERIFY(rows.first().contains(QStringLiteral("Double")));
        QVERIFY(rows.first().contains(QStringLiteral("3")));
        QVERIFY(rows.first().contains(QStringLiteral("K4ABC (2)")));
        QVERIFY(rows.first().contains(QStringLiteral("N3XYZ")));
        QVERIFY(rows.join(' ').contains(QStringLiteral("Combined RF")));
        QVERIFY(rows.join(' ').contains(QStringLiteral("VK3ABC/P  S9 +12 / -61  2s")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("K4ABC (2)  S9")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("WRONG")));
        // One remains keyed: old combined samples do not become its report.
        QJsonObject single = turn;
        single.insert("id", 13); single.insert("kind", "talking"); single.insert("talkers", QJsonArray{a});
        turn.insert("end", 26000);
        view.insert("now", QJsonArray{single}); view.insert("earlier", QJsonArray{turn});
        rows = roundtableTagReportLines(tag, view, 27000);
        QVERIFY(!rows.join(' ').contains(QStringLiteral("Double")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("-61")));
        QVERIFY(rows.join(' ').contains(QStringLiteral("No matching reports")));
        view.insert("now", QJsonArray{});
        rows = roundtableTagReportLines(tag, view, 60000);
        QVERIFY(rows.first().startsWith(QStringLiteral("Last double")));
        QVERIFY(rows.join(' ').contains(QStringLiteral("stale")));
    }

    void offCenterDoubleUsesEndpointsAndOnlyCurrentlyKeyedParticipants()
    {
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        const QJsonObject a{{"sourceRoom", "lsb-7185000"}, {"ref", "a"}, {"callsign", "W1AW"},
            {"hz", tag.hz}, {"keyed", true}, {"family", "lsb"}};
        QJsonObject b = a; b.insert("ref", "b"); b.insert("callsign", "K4ABC");
        QJsonObject c = a; c.insert("ref", "c"); c.insert("callsign", "N3XYZ");
        tag.stations = {a};
        QJsonObject turn{{"id", 31}, {"kind", "doubled"}, {"hz", 7185000}, {"sourceRoom", "lsb-7185000"},
            {"start", 1000}, {"end", QJsonValue::Null}, {"talkers", QJsonArray{a, b, c}}};
        QJsonObject view{{"family", "lsb"}, {"now", QJsonArray{turn}}, {"members", QJsonArray{a, b, c}}};
        auto rows = roundtableTagReportLines(tag, view, 27000);
        QVERIFY2(!rows.isEmpty() && rows.first().contains(QStringLiteral("Double (3)")), qPrintable(rows.join(' ')));
        c.insert("keyed", false);
        c.insert("reading", QJsonObject{{"turn", 31}, {"present", true}, {"dbm", -73}, {"t", 26000}});
        view.insert("members", QJsonArray{a, b, c});
        rows = roundtableTagReportLines(tag, view, 27000);
        QVERIFY(rows.first().contains(QStringLiteral("W1AW")));
        QVERIFY(rows.first().contains(QStringLiteral("K4ABC")));
        QVERIFY(!rows.first().contains(QStringLiteral("N3XYZ")));
        QVERIFY(rows.join(' ').contains(QStringLiteral("N3XYZ  -73  1s")));
        turn.insert("talkers", QJsonArray{QJsonObject{{"sourceRoom", "lsb-7185000"}, {"ref", "x"}, {"hz", 7185999}}});
        view.insert("now", QJsonArray{turn});
        // An exact-dial receiver explicitly ties the combined sample to
        // this source's turn, even when an endpoint is unavailable.
        rows = roundtableTagReportLines(tag, view, 27000);
        QVERIFY(rows.join(' ').contains(QStringLiteral("Combined RF")));
        // With neither, the room center is still the nearest live turn (D3).
        c.insert("hz", 7185999); view.insert("members", QJsonArray{c});
        QVERIFY(roundtableTagReportLines(tag, view, 27000).first().startsWith(QStringLiteral("Double")));
        turn.insert("kind", "unknown"); view.insert("now", QJsonArray{turn});
        QCOMPARE(roundtableTagReportLines(tag, view, 27000).first(), QStringLiteral("Unknown signal"));
        // Past the window it says nothing.
        turn.insert("hz", 7185123 + 5001); view.insert("now", QJsonArray{turn});
        QVERIFY(roundtableTagReportLines(tag, view, 27000).isEmpty());
    }

    void liveDoubleCannotTreatMissingOrNullRefsAsCurrentKeyedPeople_data()
    {
        QTest::addColumn<QString>("caseName");
        QTest::newRow("removed former participant") << QStringLiteral("removed");
        QTest::newRow("null participant ref") << QStringLiteral("null");
        QTest::newRow("unknown non-null participant ref") << QStringLiteral("unknown");
    }

    void liveDoubleCannotTreatMissingOrNullRefsAsCurrentKeyedPeople()
    {
        QFETCH(QString, caseName);
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        const QJsonObject a{{"sourceRoom", "lsb-7184000"}, {"ref", "a"}, {"callsign", "W1AW"}, {"keyed", true}};
        const QJsonObject b{{"sourceRoom", "lsb-7184000"}, {"ref", "b"}, {"callsign", "K4ABC"}, {"keyed", true}};
        QJsonObject c{{"sourceRoom", "lsb-7184000"}, {"ref", "c"}, {"callsign", "N3XYZ"}, {"keyed", true}};
        if (caseName == QLatin1String("null")) { c.insert("ref", QJsonValue::Null); }
        if (caseName == QLatin1String("unknown")) { c.insert("ref", "unknown-ref"); }
        tag.stations = {a};
        QJsonObject turn{{"id", 41}, {"kind", "doubled"}, {"sourceRoom", "lsb-7184000"}, {"hz", tag.hz},
            {"start", 1}, {"end", QJsonValue::Null}, {"talkers", QJsonArray{a, b, c}}};
        QJsonObject view{{"family", "lsb"}, {"now", QJsonArray{turn}}, {"members", QJsonArray{a, b}}};
        const auto current = roundtableTurnParticipants(view, turn);
        QCOMPARE(current.size(), 2);
        const QStringList lines = roundtableTagReportLines(tag, view, 1000);
        QVERIFY(!lines.first().contains(QStringLiteral("N3XYZ")));
        QVERIFY(!lines.first().contains(QStringLiteral("(3)")));
        QVERIFY(lines.first().contains(QStringLiteral("identities updating")));
        view.insert("members", QJsonArray{});
        QVERIFY(roundtableTagReportLines(tag, view, 1000).first().contains(QStringLiteral("unavailable")));
        turn.insert("end", 900);
        view.insert("now", QJsonArray{}); view.insert("earlier", QJsonArray{turn});
        QCOMPARE(roundtableTurnParticipants(view, turn).size(), 3);
        QVERIFY(roundtableTagReportLines(tag, view, 1000).first().contains(QStringLiteral("N3XYZ")));
    }

    void theSelectedContextIsIndependentOfAllOwnSliceTags()
    {
        NullLink link;
        link.frequency = true;
        RoundtableState state;
        RoundtableModel model;
        model.attachStation(&link, &state);
        QJsonArray stations;
        for (qint64 hz : {7185123LL, 7188123LL}) {
            stations.append(QJsonObject{{"sourceRoom", "lsb-7184000"}, {"ref", QString::number(hz)},
                {"hz", hz}, {"family", "lsb"}, {"callsign", "KG4VCF"}, {"you", true}});
        }
        model.applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{
            QJsonObject{{"id", "lsb-7184000"}, {"stations", stations}}}}});
        const QString selected = RoundtableModel::frequencyContextId(7188123, QStringLiteral("lsb"));
        const auto tags = roundtableTagsFor(&model, {}, selected);
        QCOMPARE(tags.size(), 2);
        QVERIFY(tags.first().mine);
        QVERIFY(tags.last().mine);
        QVERIFY(!tags.first().active);
        QVERIFY(tags.last().active);
        const auto placed = layoutRoundtableTags(tags, 100, 7180000, 220, 208, 4);
        QCOMPARE(placed.size(), 1);
        QCOMPARE(placed.first().roomIds, QStringList{selected});
    }

    void aSelectedRfReportDoesNotRequireDirectoryPresenceOrInventQuietMarkers()
    {
        NullLink link; link.frequency = true;
        RoundtableState state;
        RoundtableModel model;
        model.attachStation(&link, &state);
        model.applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
        const QString selected = RoundtableModel::frequencyContextId(7185123, QStringLiteral("lsb"));
        QVERIFY(roundtableTagsFor(&model, {}, selected).isEmpty());
        const qint64 now = RoundtableModel::nowMs();
        const QJsonObject receiver{{"sourceRoom", "lsb-7184000"}, {"ref", "rx"}, {"callsign", "K4ABC"},
            {"reading", QJsonObject{{"turn", 71}, {"present", true}, {"dbm", -73}, {"t", now}}}};
        QJsonObject turn{{"id", 71}, {"kind", "talking"}, {"hz", 7185123}, {"sourceRoom", "lsb-7184000"},
            {"start", now}, {"end", QJsonValue::Null}, {"talkers", QJsonArray{
                QJsonObject{{"ref", "tx"}, {"sourceRoom", "lsb-7184000"}, {"callsign", "W1AW"}}}}};
        QJsonObject view{{"view", "frequency"}, {"id", "lsb-7185123"}, {"hz", 7185123}, {"family", "lsb"},
            {"members", QJsonArray{receiver}}, {"now", QJsonArray{turn}}};
        model.applyRoom(view);
        auto tags = roundtableTagsFor(&model, {}, selected);
        QCOMPARE(tags.size(), 1);
        QVERIFY(tags.first().active);
        QCOMPARE(tags.first().state, static_cast<int>(RoundtableGlyph::Talking));
        QVERIFY(tags.first().reportLines.join(' ').contains(QStringLiteral("K4ABC")));
        turn.insert("kind", "unknown"); turn.insert("talkers", QJsonArray{});
        view.insert("now", QJsonArray{turn}); model.applyRoom(view);
        tags = roundtableTagsFor(&model, {}, selected);
        QCOMPARE(tags.first().state, static_cast<int>(RoundtableGlyph::Unknown));
        QCOMPARE(tags.first().reportLines.first(), QStringLiteral("Unknown signal"));
        QVERIFY(!tags.first().reportLines.join(' ').contains(QStringLiteral("W1AW")));
    }

    void cochannelReportsFilterBeforeTheDisplayLimit_data()
    {
        QTest::addColumn<bool>("unknown");
        QTest::newRow("known") << false;
        QTest::newRow("unknown RF") << true;
    }

    void cochannelReportsFilterBeforeTheDisplayLimit()
    {
        QFETCH(bool, unknown);
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        tag.stations = QJsonArray{QJsonObject{{"sourceRoom", "lsb-7184000"}, {"family", "lsb"}},
            QJsonObject{{"sourceRoom", "lsb-7184000"}, {"family", "lsb"}}};
        const QJsonObject talker{{"sourceRoom", "lsb-7184000"}, {"ref", "talker"}, {"callsign", "W1AW"}};
        QJsonArray members{talker};
        for (int i = 0; i < 4; ++i) {
            members.append(QJsonObject{{"sourceRoom", "lsb-7184000"}, {"ref", QString::number(i)},
                {"callsign", QStringLiteral("N%1ABC").arg(i)}, {"km", i},
                {"reading", QJsonObject{{"turn", i == 2 ? 11 : 12}, {"dbm", i == 1 ? -400 : -73},
                    {"present", i != 0}, {"t", 2000}}}});
        }
        const QJsonObject turn{{"id", 12}, {"kind", unknown ? "unknown" : "talking"}, {"hz", tag.hz},
            {"sourceRoom", "lsb-7184000"}, {"start", 1000}, {"talkers", unknown ? QJsonArray{} : QJsonArray{talker}}};
        const QJsonObject view{{"family", "lsb"}, {"now", QJsonArray{turn}}, {"members", members}};
        const QStringList rows = roundtableTagReportLines(tag, view, 27000);
        QVERIFY(rows.contains(QStringLiteral("N3ABC  -73  25s stale")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("N0ABC")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("N1ABC")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("N2ABC")));
    }

    void cochannelReportsRespectSelectedSignalFormat_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<QString>("sample");
        QTest::addColumn<QString>("heading");
        QTest::addColumn<QString>("reporter");
        QTest::addColumn<int>("nth");
        QTest::newRow("dBm") << QStringLiteral("dBm") << QStringLiteral("K4ABC  -61  25s stale")
            << QStringLiteral("Heard by     dBm    Age") << QStringLiteral("K4ABC") << 1;
        QTest::newRow("S-units") << QStringLiteral("S-units") << QStringLiteral("K4ABC  S9 +12  25s stale")
            << QStringLiteral("Heard by     S-units    Age") << QStringLiteral("K4ABC") << 1;
        QTest::newRow("Both") << QStringLiteral("Both") << QStringLiteral("K4ABC  S9 +12 / -61  25s stale")
            << QStringLiteral("Heard by     S / dBm    Age") << QStringLiteral("K4ABC") << 1;
        QTest::newRow("Both duplicate") << QStringLiteral("Both") << QStringLiteral("KG4VCF (2)  S9 +12 / -61  25s stale")
            << QStringLiteral("Heard by     S / dBm    Age") << QStringLiteral("KG4VCF") << 2;
        QTest::newRow("Both portable duplicate") << QStringLiteral("Both") << QStringLiteral("VK3ABC/P (2)  S9 +12 / -61  25s stale")
            << QStringLiteral("Heard by     S / dBm    Age") << QStringLiteral("VK3ABC/P") << 2;
    }

    void cochannelReportsRespectSelectedSignalFormat()
    {
        QFETCH(QString, format);
        QFETCH(QString, sample);
        QFETCH(QString, heading);
        QFETCH(QString, reporter);
        QFETCH(int, nth);
        AppSettings::instance().setValue(QStringLiteral("Roundtable/SignalFormat"), format);
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        tag.stations = QJsonArray{QJsonObject{{"sourceRoom", "lsb-7184000"}, {"family", "lsb"}},
            QJsonObject{{"sourceRoom", "lsb-7184000"}, {"family", "lsb"}}};
        const QJsonObject talker{{"sourceRoom", "lsb-7184000"}, {"ref", "talker"}, {"callsign", "W1AW"}};
        const QJsonObject member{{"sourceRoom", "lsb-7184000"}, {"ref", "reporter"}, {"callsign", reporter}, {"nth", nth},
            {"reading", QJsonObject{{"turn", 12}, {"dbm", -61}, {"present", true}, {"t", 2000}}}};
        const QJsonObject turn{{"id", 12}, {"kind", "talking"}, {"hz", tag.hz},
            {"sourceRoom", "lsb-7184000"}, {"start", 1000}, {"talkers", QJsonArray{talker}}};
        const QJsonObject view{{"family", "lsb"}, {"now", QJsonArray{turn}}, {"members", QJsonArray{talker, member}}};
        const QStringList rows = roundtableTagReportLines(tag, view, 27000);
        AppSettings::instance().remove(QStringLiteral("Roundtable/SignalFormat"));
        QVERIFY2(rows.contains(sample), qPrintable(rows.join('\n')));
        QVERIFY(rows.contains(heading));
    }

    // ---- The inspector: placement (overlay design D34, D37; overlay.js placeIns) ----

    void theInspectorStandsOnTheFootPastTheFlag()
    {
        const int dialX = 400;
        const QRect flag = boardFlag(dialX);
        const QRect placed = placeRoundtableInspector(kBoardPan, dialX, {flag}, false, 180);
        QCOMPARE(placed.width(), 300);
        QCOMPARE(placed.height(), 180);
        // At the stretch's edge, 4 px past the flag, not centered on anything.
        QCOMPARE(placed.left(), flag.left() + flag.width() + 4);
        QCOMPARE(placed.top() + placed.height(), 303);
        QVERIFY(!placed.intersects(flag));
    }

    void theInspectorIsCutToAShortPanAndStaysInside()
    {
        const int dialX = 400;
        const QRect flag = boardFlag(dialX);
        const QRect shortPan(80, 183, 1029, 120);
        const QRect placed = placeRoundtableInspector(shortPan, dialX, {flag}, false, 400);
        QCOMPARE(placed.height(), 120);
        QCOMPARE(placed.top(), 183);
        QCOMPARE(placed.top() + placed.height(), 303);
        QCOMPARE(placed.width(), 300);
        QVERIFY(!placed.intersects(flag));
        // Tall enough, it asks for no more than it wants.
        const QRect tall = placeRoundtableInspector(kBoardPan, dialX, {flag}, false, 90);
        QCOMPARE(tall.height(), 90);
        QCOMPARE(tall.top() + tall.height(), 303);
    }

    void theInspectorFollowsAFlippedFlag()
    {
        const int dialX = 800;
        const QRect flag = boardFlag(dialX, true);
        const QRect placed = placeRoundtableInspector(kBoardPan, dialX, {flag}, true, 180);
        QCOMPARE(placed.width(), 300);
        // Ends 4 px short of the flag, on the far side from the dial.
        QCOMPARE(placed.left() + placed.width(), flag.left() - 4);
        QVERIFY(!placed.intersects(flag));
        QCOMPARE(placed.top() + placed.height(), 303);
    }

    // The two stretches differ from the card's: a 260 stretch counts, 259 does not,
    // and narrower than 300 it takes what there is.
    void theInspectorsStretchIsAtLeast260()
    {
        const QRect pan(0, 0, 1000, 300);
        const int dialX = 300;
        const auto flagEnding = [](int right) { return QRect(100, 0, right - 100, 100); };
        // Past the flag there is 1000 - 736 = 264 wide: narrower than 300, still taken.
        QRect placed = placeRoundtableInspector(pan, dialX, {flagEnding(732)}, false, 100);
        QCOMPARE(placed.left(), 736);
        QCOMPARE(placed.width(), 264);
        // 1000 - 741 = 259 is too narrow, and so is what the flag leaves on the near
        // side: no stretch has room, so it stays open at the pan's edge.
        placed = placeRoundtableInspector(pan, dialX, {flagEnding(737)}, false, 100);
        QCOMPARE(placed.left(), pan.left());
        QCOMPARE(placed.width(), 300);
    }

    void theInspectorUsesTheDialOnlyWhenItLiesInsideTheStretch()
    {
        const QRect pan(0, 0, 1000, 300);
        // Nothing in the way: the stretch holds the dial, so it centers on it.
        QCOMPARE(placeRoundtableInspector(pan, 500, {}, false, 100).left(), 350);
        // Clamped to the stretch near an end.
        QCOMPARE(placeRoundtableInspector(pan, 40, {}, false, 100).left(), 0);
        QCOMPARE(placeRoundtableInspector(pan, 980, {}, false, 100).left(), 700);
        // A flag out of the pan's height keeps nothing clear.
        QCOMPARE(placeRoundtableInspector(pan, 500, {QRect(400, -200, 200, 100)}, false, 100).left(), 350);
    }

    void theInspectorStaysOpenWithNoFreeStretch()
    {
        const QRect wall(80, 30, 1029, 162);
        const QRect placed = placeRoundtableInspector(kBoardPan, 570, {wall}, false, 180);
        QCOMPARE(placed.left(), kBoardPan.left());
        QCOMPARE(placed.width(), 300);
        QCOMPARE(placed.top() + placed.height(), 303);
        // A pan narrower than the inspector holds it inside, and an empty one has no spot.
        const QRect narrow = placeRoundtableInspector(QRect(10, 0, 200, 300), 50, {}, false, 100);
        QCOMPARE(narrow.left(), 10);
        QCOMPARE(narrow.width(), 200);
        QVERIFY(placeRoundtableInspector(QRect(), 50, {}, false, 100).isNull());
    }

    // ---- The live card: placement (overlay design D16, D19) ----

    void theCardSitsPastTheFlagOnTheFoot()
    {
        const QFontMetrics fm = cardMetrics();
        const int dialX = 570;
        const QRect flag = boardFlag(dialX);
        const RoundtableCardPlacement placed = placeRoundtableCard(talkingCard(), kBoardPan, dialX, {flag}, false, fm);
        QVERIFY(!placed.card.isNull());
        QCOMPARE(placed.card.width(), 232);
        QVERIFY(placed.card.left() > flag.right());
        QCOMPARE(placed.card.top() + placed.card.height(), 303);
        QVERIFY(!placed.beside);
        QCOMPARE(placed.rows, 3);
        QCOMPARE(placed.moreRows, 0);
    }

    void theCardComesBackOnTheNearSideWithoutRoomPastTheFlag()
    {
        const QFontMetrics fm = cardMetrics();
        // 199 free past the flag: too few.
        int dialX = 654;
        QRect flag = boardFlag(dialX);
        QCOMPARE(flag.left() + flag.width() + 4 + 199, 1109);
        RoundtableCardPlacement placed = placeRoundtableCard(talkingCard(), kBoardPan, dialX, {flag}, false, fm);
        QVERIFY(placed.beside);
        QCOMPARE(placed.card.width(), 232);
        QVERIFY(placed.card.right() < flag.left());
        // 200 free: past the flag at that width.
        flag.translate(-1, 0);
        dialX -= 1;
        placed = placeRoundtableCard(talkingCard(), kBoardPan, dialX, {flag}, false, fm);
        QVERIFY(!placed.beside);
        QCOMPARE(placed.card.width(), 200);
        QVERIFY(placed.card.left() > flag.right());
    }

    void aFlippedFlagPutsTheCardPastItOnTheLeft()
    {
        const QFontMetrics fm = cardMetrics();
        const int dialX = 1000;
        const QRect flag = boardFlag(dialX, true);
        const RoundtableCardPlacement placed = placeRoundtableCard(talkingCard(), kBoardPan, dialX, {flag}, true, fm);
        QVERIFY(!placed.beside);
        QVERIFY(placed.card.right() < flag.left());
        QCOMPARE(placed.card.width(), 232);
    }

    void theCardNeverCrossesAFlagOrLeavesThePan_data()
    {
        QTest::addColumn<int>("dialX");
        QTest::addColumn<bool>("flipped");
        for (int dialX = 90; dialX < 1100; dialX += 97) {
            QTest::addRow("right of %d", dialX) << dialX << false;
            QTest::addRow("left of %d", dialX) << dialX << true;
        }
    }

    void theCardNeverCrossesAFlagOrLeavesThePan()
    {
        QFETCH(int, dialX);
        QFETCH(bool, flipped);
        const QFontMetrics fm = cardMetrics();
        const QVector<QRect> flags{boardFlag(dialX, flipped), QRect(600, 30, 90, 162)};
        const RoundtableCardPlacement placed = placeRoundtableCard(talkingCard(7), kBoardPan, dialX, flags, flipped, fm);
        if (placed.card.isNull()) {
            return;
        }
        QVERIFY(kBoardPan.contains(placed.card));
        for (const QRect& flag : flags) {
            QVERIFY(!placed.card.intersects(flag));
        }
    }

    void noStretchWideEnoughMeansNoCard()
    {
        // A flag each side, 199 between them and neither side of them wider.
        const QFontMetrics fm = cardMetrics();
        const QRect pan(0, 0, 803, 300);
        const QVector<QRect> flags{QRect(199, 0, 201, 100), QRect(603, 0, 200, 100)};
        const RoundtableCardPlacement placed = placeRoundtableCard(talkingCard(), pan, 500, flags, false, fm);
        QVERIFY(placed.card.isNull());
        QCOMPARE(placed.rows, 0);
    }

    void rowsGiveWayFirstOnAShortPan()
    {
        const QFontMetrics fm = cardMetrics();
        RoundtableCardPlacement placed = placeRoundtableCard(talkingCard(7), kBoardPan, 570, {boardFlag(570)}, false, fm);
        QCOMPARE(placed.rows, 5);
        QCOMPARE(placed.moreRows, 2);
        const RoundtableCardModel card = talkingCard(5, RoundtableCardModel::Kind::Double);
        int last = 5;
        int zeroRowsHeight = 0;
        for (int height = 220; height >= 40; height -= 4) {
            const QRect pan(80, 303 - height, 1029, height);
            placed = placeRoundtableCard(card, pan, 570, {}, false, fm);
            QVERIFY(!placed.card.isNull());
            QVERIFY2(placed.rows <= last, "a shorter pan never shows more rows");
            QCOMPARE(placed.moreRows, 5 - placed.rows);
            QVERIFY(placed.card.height() <= height);
            QCOMPARE(placed.card.top() + placed.card.height(), 303);
            last = placed.rows;
            if (placed.rows == 0 && zeroRowsHeight == 0) { zeroRowsHeight = height; }
        }
        QVERIFY(zeroRowsHeight > 0);
    }

    void aShortPanKeepsTheDoubleHeadAndNames()
    {
        const QFontMetrics fm = cardMetrics();
        const RoundtableCardModel card = talkingCard(5, RoundtableCardModel::Kind::Double);
        // Room for the rail, the head and one line of names, nothing else.
        const int height = 2 + 3 + 5 + 5 + 16 + 3 + 15 + 2 + 2;
        const QRect pan(0, 0, 800, height);
        const RoundtableCardPlacement placed = placeRoundtableCard(card, pan, 100, {}, false, fm);
        QVERIFY(!placed.card.isNull());
        QCOMPARE(placed.rows, 0);
        QCOMPARE(placed.moreRows, 5);
        QImage image(800, height, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        {
            QPainter painter(&image);
            paintRoundtableCard(painter, card, placed, QStringLiteral("dBm"), fm, false);
        }
        // The rail's stripes and the red name dot are both there.
        QVERIFY(hasColor(image, QRect(placed.card.left() + 2, placed.card.top() + 1, 40, 3), QColor(0xe0, 0x6c, 0x6e)));
        QVERIFY(hasColor(image, placed.card.adjusted(7, 24, -7, 0), QColor(0xe0, 0x6c, 0x6e)));
    }

    void valueUpdatesChangeNoCardGeometry()
    {
        const QFontMetrics fm = cardMetrics();
        RoundtableCardModel card = talkingCard(4);
        const QVector<QRect> flags{boardFlag(570)};
        const RoundtableCardPlacement first = placeRoundtableCard(card, kBoardPan, 570, flags, false, fm);
        for (int i = 0; i < 40; ++i) {
            for (RoundtableCardRow& row : card.rows) {
                row.dbm -= 3.0;
                row.ageMs += 37000;
                row.old = row.ageMs > 20000;
                row.peakDbm = row.dbm + 1.0;
            }
            card.clockMs += 61000;
            const RoundtableCardPlacement again = placeRoundtableCard(card, kBoardPan, 570, flags, false, fm);
            QCOMPARE(again.card, first.card);
            QCOMPARE(again.rows, first.rows);
        }
    }

    // ---- The live card: drawing (D10, D11, D13, D15) ----

    void aTalkingCardStandsOnThePanAndClickingItAsksForTheInspector()
    {
        Pan pan;
        const QString room = QStringLiteral("frequency:lsb-7200000");
        RoundtableTag tag = tagAt(room, 7200000);
        tag.active = true;
        tag.stations = {QJsonObject{{"callsign", "W1AW"}}};
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        pan.widget.setRoundtableCard(talkingCard(), room);
        const QImage image = pan.draw();
        // The card speaks for the frequency: no tag of its own.
        QCOMPARE(pan.widget.roundtableTagHitsForTest().size(), 1);
        const auto* hit = cardHit(pan.widget);
        QVERIFY(hit != nullptr);
        QCOMPARE(hit->hz, 7200000);
        QCOMPARE(hit->roomIds, QStringList{room});
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        QVERIFY(spec.contains(hit->rect));
        QVERIFY(!pan.widget.receiverFlagRegion().intersects(hit->rect));
        QCOMPARE(hit->rect.bottom() + 1, spec.bottom() + 1 - pan.widget.bandPlanStripHeightForTest() - 8);
        // Border, rail and meter in the talking green.
        const QColor green(0x4c, 0xaf, 0x6a);
        QCOMPARE(QColor(image.pixel(hit->rect.center().x(), hit->rect.top())).name(), green.name());
        QCOMPARE(QColor(image.pixel(hit->rect.center().x(), hit->rect.top() + 2)).name(), green.name());
        // A click asks for the inspector (D25); there is no menu of its own, no tune.
        const QPoint click = hit->rect.center();
        QSignalSpy read(&pan.widget, &SpectrumWidget::roundtableReadRequested);
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::roundtableRoomClicked);
        QSignalSpy plainTune(&pan.widget, &SpectrumWidget::frequencyClicked);
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, click);
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().first().toString(), room);
        QCOMPARE(tuned.size(), 0);
        QCOMPARE(plainTune.size(), 0);
        QVERIFY(pan.widget.window()->findChild<QMenu*>() == nullptr);
    }

    void theShortMenuOffersTheInspectorTuneMessagesAndSignalWithItsGates()
    {
        Pan pan;
        const QString room = QStringLiteral("frequency:lsb-7180000");
        pan.widget.setRoundtableTags({}, RoundtableTagDisplay::Live);
        AppSettings::instance().setValue(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("dBm"));
        const auto action = [](QMenu& menu, const char* name) {
            return menu.findChild<QAction*>(QString::fromLatin1(name));
        };
        const auto rows = [](QMenu& menu) {
            QStringList out;
            for (QAction* row : menu.actions()) {
                if (!row->isSeparator()) { out.append(row->text()); }
            }
            return out;
        };

        QMenu menu;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, menu);
        QCOMPARE(rows(menu), (QStringList{QStringLiteral("7.180 MHz"), QStringLiteral("Open inspector"),
                                          QStringLiteral("Tune slice A here"), QStringLiteral("Messages"),
                                          QStringLiteral("Signal: dBm · Change")}));
        QVERIFY(action(menu, "roundtableMenuTune")->isEnabled());

        // Open or Close, and it only asks.
        SpectrumWidget::RoundtableMenuOptions opened;
        opened.inspectorOpen = true;
        QMenu closing;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, closing, opened);
        QCOMPARE(action(closing, "roundtableMenuInspector")->text(), QStringLiteral("Close inspector"));
        QSignalSpy read(&pan.widget, &SpectrumWidget::roundtableReadRequested);
        action(closing, "roundtableMenuInspector")->trigger();
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().first().toString(), room);

        // Messages only asks.
        QSignalSpy messages(&pan.widget, &SpectrumWidget::roundtableMessagesRequested);
        action(menu, "roundtableMenuMessages")->trigger();
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.first().first().toString(), room);

        // The inspector's own ⋯: the same list with Read reports, and no Open or Close.
        SpectrumWidget::RoundtableMenuOptions inspector;
        inspector.fromInspector = true;
        QMenu more;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, more, inspector);
        QVERIFY(action(more, "roundtableMenuInspector") == nullptr);
        QCOMPARE(rows(more), (QStringList{QStringLiteral("7.180 MHz"), QStringLiteral("Tune slice A here"),
                                          QStringLiteral("Messages"), QStringLiteral("Signal: dBm · Change"),
                                          QStringLiteral("Read reports")}));
        QSignalSpy reports(&pan.widget, &SpectrumWidget::roundtableReportsRequested);
        action(more, "roundtableMenuRead")->trigger();
        QCOMPARE(reports.size(), 1);

        // Signal moves the shared key along, with nothing else closing.
        QSignalSpy cycled(&pan.widget, &SpectrumWidget::roundtableSignalFormatCycled);
        action(menu, "roundtableMenuSignal")->trigger();
        QCOMPARE(cycled.size(), 1);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/SignalFormat")).toString(),
                 QStringLiteral("S-units"));
        QMenu second;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, second);
        QCOMPARE(action(second, "roundtableMenuSignal")->text(), QStringLiteral("Signal: S-units · Change"));
        AppSettings::instance().remove(QStringLiteral("Roundtable/SignalFormat"));

        // Tune tunes here, and is greyed with its reason, never hidden.
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::roundtableRoomClicked);
        action(menu, "roundtableMenuTune")->trigger();
        QCOMPARE(tuned.size(), 1);
        QCOMPARE(tuned.first().first().toString(), room);
        QMenu there;
        pan.widget.buildRoundtableCardMenu(room, 7180000, true, there);
        QVERIFY(!action(there, "roundtableMenuTune")->isEnabled());
        QCOMPARE(action(there, "roundtableMenuTune")->toolTip(), QStringLiteral("Slice A is already here"));
        pan.widget.setMoxOverlay(true);
        QMenu keyed;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, keyed);
        QVERIFY(!action(keyed, "roundtableMenuTune")->isEnabled());
        QCOMPARE(action(keyed, "roundtableMenuTune")->toolTip(), QStringLiteral("Not while you transmit"));
        pan.widget.setMoxOverlay(false);
        // What only the window knows: another device keying, no radio.
        SpectrumWidget::RoundtableMenuOptions waiting;
        waiting.tuneAllowed = false;
        QMenu remote;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, remote, waiting);
        QVERIFY(!action(remote, "roundtableMenuTune")->isEnabled());

        // Another slice's letter, and no slice at all.
        pan.widget.setRoundtableActiveSliceName(QStringLiteral("B"));
        QMenu lettered;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, lettered);
        QCOMPARE(action(lettered, "roundtableMenuTune")->text(), QStringLiteral("Tune slice B here"));
        pan.widget.setRoundtableActiveSliceName(QString());
        QMenu none;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, none);
        QCOMPARE(action(none, "roundtableMenuTune")->text(), QStringLiteral("Tune slice here"));
        QVERIFY(!action(none, "roundtableMenuTune")->isEnabled());
    }

    void yourOwnCompactTagAndAnotherAskForTheInspectorAndAFoldAsksForItsList()
    {
        Pan pan;
        RoundtableTag mine = tagAt(QStringLiteral("frequency:lsb-7150000"), 7150000);
        mine.active = true;
        mine.stations = {QJsonObject{{"callsign", "W1AW"}}};
        RoundtableTag other = tagAt(QStringLiteral("frequency:lsb-7170000"), 7170000);
        other.stations = {QJsonObject{{"callsign", "K4ABC"}}};
        RoundtableTag foldA = tagAt(QStringLiteral("frequency:lsb-7185000"), 7185000);
        RoundtableTag foldB = tagAt(QStringLiteral("frequency:lsb-7185050"), 7185050);
        RoundtableTag foldC = tagAt(QStringLiteral("frequency:lsb-7185100"), 7185100);
        pan.widget.setRoundtableTags({mine, other, foldA, foldB, foldC}, RoundtableTagDisplay::Live);
        pan.draw();
        QSignalSpy read(&pan.widget, &SpectrumWidget::roundtableReadRequested);
        QSignalSpy fold(&pan.widget, &SpectrumWidget::roundtableFoldClicked);
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::roundtableRoomClicked);
        QSignalSpy plainTune(&pan.widget, &SpectrumWidget::frequencyClicked);

        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, hitFor(pan.widget, mine.roomId)->rect.center());
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().first().toString(), mine.roomId);

        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, hitFor(pan.widget, other.roomId)->rect.center());
        QCOMPARE(read.size(), 2);
        QCOMPARE(read.last().first().toString(), other.roomId);

        // A fold on a capable server asks for its list, never a menu; never a tune.
        const auto* foldHit = hitFor(pan.widget, foldA.roomId);
        QVERIFY(foldHit != nullptr);
        QVERIFY(foldHit->folded);
        const QStringList foldRooms = foldHit->roomIds;
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, foldHit->rect.center());
        QCOMPARE(fold.size(), 1);
        QCOMPARE(fold.first().first().toStringList(), foldRooms);
        QVERIFY(pan.widget.window()->findChild<QMenu*>() == nullptr);
        QCOMPARE(read.size(), 2);
        QCOMPARE(tuned.size(), 0);
        QCOMPARE(plainTune.size(), 0);

        // What the list says.
        const QVector<RoundtableFoldRow> rows = pan.widget.roundtableFoldRows(foldRooms);
        QCOMPARE(rows.size(), foldRooms.size());
        QCOMPARE(rows.at(1).hz, 7185050);
        // A double in the list names who.
        RoundtableTag doubled = foldB;
        doubled.state = static_cast<int>(RoundtableGlyph::Doubled);
        doubled.keyed = {QStringLiteral("AA5ABC"), QStringLiteral("K0ABC")};
        doubled.label = QStringLiteral("Double");
        pan.widget.setRoundtableTags({doubled}, RoundtableTagDisplay::Live);
        QCOMPARE(pan.widget.roundtableFoldRows({doubled.roomId}).first().who, QStringLiteral("Double · AA5ABC + K0ABC"));
        // Gone from the band, gone from the list.
        QVERIFY(pan.widget.roundtableFoldRows({QStringLiteral("frequency:lsb-1")}).isEmpty());
    }

    // D36: tested before the pan's own menus, and the pan's context menu stays away.
    void aRightClickOnATagAsksForTheShortMenuBeforeThePans()
    {
        Pan pan;
        RoundtableTag other = tagAt(QStringLiteral("frequency:lsb-7140000"), 7140000);
        RoundtableTag legacy = tagAt(QStringLiteral("lsb-7120000"), 7120000);
        RoundtableTag foldA = tagAt(QStringLiteral("frequency:lsb-7185000"), 7185000);
        RoundtableTag foldB = tagAt(QStringLiteral("frequency:lsb-7185050"), 7185050);
        RoundtableTag foldC = tagAt(QStringLiteral("frequency:lsb-7185100"), 7185100);
        pan.widget.setRoundtableTags({other, legacy, foldA, foldB, foldC}, RoundtableTagDisplay::Live);
        pan.draw();
        QSignalSpy asked(&pan.widget, &SpectrumWidget::roundtableTagMenuRequested);
        QSignalSpy plainTune(&pan.widget, &SpectrumWidget::frequencyClicked);
        const auto contextAccepted = [&pan](const QPoint& at) {
            QContextMenuEvent event(QContextMenuEvent::Mouse, at, pan.widget.mapToGlobal(at));
            event.ignore();
            QApplication::sendEvent(&pan.widget, &event);
            return event.isAccepted();
        };
        const QPoint at = hitFor(pan.widget, other.roomId)->rect.center();
        QTest::mouseClick(&pan.widget, Qt::RightButton, Qt::NoModifier, at);
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.first().first().toString(), other.roomId);
        QCOMPARE(asked.first().at(1).toPoint(), pan.widget.mapToGlobal(at));
        QVERIFY(contextAccepted(at));
        QCOMPARE(plainTune.size(), 0);

        // A fold has no menu, and the pan's stays away from it too.
        const QPoint foldAt = hitFor(pan.widget, foldA.roomId)->rect.center();
        QTest::mouseClick(&pan.widget, Qt::RightButton, Qt::NoModifier, foldAt);
        QCOMPARE(asked.size(), 1);
        QVERIFY(contextAccepted(foldAt));

        // A legacy server's tag keeps what it had, and so does the bare band.
        const QPoint legacyAt = hitFor(pan.widget, legacy.roomId)->rect.center();
        QVERIFY(!contextAccepted(legacyAt));
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        QVERIFY(!contextAccepted(QPoint(spec.center().x(), spec.top() + 10)));
        QCOMPARE(asked.size(), 1);

        // Ctrl keeps its notch.
        QSignalSpy notch(&pan.widget, &SpectrumWidget::notchCreateRequested);
        QTest::mouseClick(&pan.widget, Qt::RightButton, Qt::ControlModifier, at);
        QCOMPARE(notch.size(), 1);
        QCOMPARE(asked.size(), 1);
    }

    // D34, D37: one box, not two; tags keep clear; and none of it reaches the static overlay.
    void theOpenInspectorHidesTheCardAndTagsKeepClearOfIt()
    {
        Pan pan;
        const QString room = QStringLiteral("frequency:lsb-7200000");
        RoundtableTag mine = tagAt(room, 7200000);
        mine.active = true;
        RoundtableTag near = tagAt(QStringLiteral("frequency:lsb-7212000"), 7212000);
        RoundtableTag far = tagAt(QStringLiteral("frequency:lsb-7150000"), 7150000);
        pan.widget.setRoundtableTags({mine, near, far}, RoundtableTagDisplay::Live);
        pan.widget.setRoundtableCard(talkingCard(), room);
        pan.draw();
        QVERIFY(cardHit(pan.widget) != nullptr);
        const QRect cardRect = cardHit(pan.widget)->rect;
        const SpectrumWidget::RoundtableInspectorSpot spot = pan.widget.roundtableInspectorSpot(0, 0);
        const QRect inspector = placeRoundtableInspector(spot.pan, spot.dialX, spot.flags, spot.flagFlipped, 300);
        QVERIFY(inspector.isValid());
        // In the card's spot, the edge by the flag the same, on the same foot line.
        QVERIFY(spot.flagFlipped);
        QCOMPARE(inspector.right(), cardRect.right());
        QCOMPARE(inspector.bottom(), cardRect.bottom());

        QCoreApplication::processEvents();
        pan.widget.clearOverlayStaticDirtyForTest();
        const QString viewed = near.roomId;
        pan.widget.setRoundtableViewedRoom(viewed);
        pan.widget.setRoundtableInspectorRect(inspector);
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
        const QImage open = pan.draw();
        QVERIFY(cardHit(pan.widget) == nullptr);
        // Where the card's green was is nothing while the inspector is the box.
        QVERIFY(QColor(open.pixel(cardRect.center().x(), cardRect.top())).name() != QStringLiteral("#4caf6a"));
        for (const SpectrumWidget::RoundtableTagHit& hit : pan.widget.roundtableTagHitsForTest()) {
            QVERIFY2(!hit.rect.intersects(inspector), qPrintable(hit.roomIds.join(QLatin1Char(' '))));
        }
        QVERIFY(hitFor(pan.widget, far.roomId) != nullptr);

        // Closed: the card returns where it was.
        pan.widget.setRoundtableInspectorRect(QRect());
        pan.widget.setRoundtableViewedRoom(QString());
        pan.draw();
        QVERIFY(cardHit(pan.widget) != nullptr);
        QCOMPARE(cardHit(pan.widget)->rect, cardRect);

        // A thousand moves never touch the static overlay.
        QCoreApplication::processEvents();
        pan.widget.clearOverlayStaticDirtyForTest();
        for (int i = 0; i < 1000; ++i) {
            pan.widget.setRoundtableInspectorRect(inspector.translated(i % 7, 0));
        }
        pan.widget.setRoundtableInspectorRect(QRect());
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
    }

    // D37: your compact tag goes while it shows your own, and stays while it shows another.
    void yourCompactTagGoesOnlyWhileItShowsYourOwn()
    {
        Pan pan;
        const QString room = QStringLiteral("frequency:lsb-7180000");
        RoundtableTag mine = tagAt(room, 7180000);
        mine.active = true;
        RoundtableTag other = tagAt(QStringLiteral("frequency:lsb-7150000"), 7150000);
        pan.widget.setRoundtableTags({mine, other}, RoundtableTagDisplay::Live);
        pan.draw();
        QVERIFY(hitFor(pan.widget, room) != nullptr);
        const QRect inspector(900, 200, 280, 200);
        pan.widget.setRoundtableInspectorRect(inspector);
        pan.widget.setRoundtableViewedRoom(other.roomId);
        pan.draw();
        QVERIFY(hitFor(pan.widget, room) != nullptr);
        pan.widget.setRoundtableViewedRoom(room);
        pan.draw();
        QVERIFY(hitFor(pan.widget, room) == nullptr);
        QVERIFY(hitFor(pan.widget, other.roomId) != nullptr);
    }

    // D37: the spot needs no card or tag, follows the flag, and a pan without the
    // slice uses its own flag, or the clicked tag.
    void theInspectorsSpotNeedsNoCardAndFollowsTheFlag()
    {
        Pan pan;
        pan.flag->setFrequency(7200000);
        pan.widget.updateVfoPositions();
        QSignalSpy changed(&pan.widget, &SpectrumWidget::roundtableInspectorSpotChanged);
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        const SpectrumWidget::RoundtableInspectorSpot quiet = pan.widget.roundtableInspectorSpot(0, 0);
        QCOMPARE(quiet.pan.bottom() + 1, spec.bottom() + 1 - pan.widget.bandPlanStripHeightForTest() - 8);
        QCOMPARE(quiet.pan.top(), spec.top() + 2);
        QCOMPARE(quiet.dialX, spec.left() + spec.width() / 2);
        QVERIFY(!quiet.flags.isEmpty());
        QCOMPARE(quiet.flagFlipped, pan.flag->geometry().center().x() < quiet.dialX);

        // The flag moves and the spot says so, with whichever side the flag now hangs on.
        pan.flag->setFrequency(7290000);
        pan.widget.updateVfoPositions();
        QVERIFY(changed.size() >= 1);
        const SpectrumWidget::RoundtableInspectorSpot right = pan.widget.roundtableInspectorSpot(0, 0);
        QVERIFY(right.dialX > quiet.dialX);
        QVERIFY(right.flagFlipped);
        pan.flag->setFrequency(7110000);
        pan.widget.updateVfoPositions();
        const SpectrumWidget::RoundtableInspectorSpot left = pan.widget.roundtableInspectorSpot(0, 0);
        QVERIFY(left.dialX < quiet.dialX);
        QVERIFY(!left.flagFlipped);

        // A resize says so too.
        const int before = changed.size();
        pan.widget.resize(1100, 700);
        QVERIFY(changed.size() > before);

        // The slice named is not on this pan: this pan's own flag.
        const SpectrumWidget::RoundtableInspectorSpot own = pan.widget.roundtableInspectorSpot(7, 7150000);
        QCOMPARE(own.dialX, pan.widget.roundtableInspectorSpot(0, 0).dialX);

        // A pan with no slice: the clicked tag.
        SpectrumWidget empty;
        empty.setUpdatesEnabled(false);
        empty.resize(1200, 760);
        empty.setSampleRate(kPanSpanHz);
        empty.setDdcCenterFrequency(kPanCenterHz);
        empty.setFrequencyRange(kPanCenterHz, kPanSpanHz);
        const QRect emptySpec = empty.roundtableTagSpectrumRectForTest();
        const SpectrumWidget::RoundtableInspectorSpot byTag = empty.roundtableInspectorSpot(0, static_cast<qint64>(kPanCenterHz));
        QCOMPARE(byTag.dialX, emptySpec.left() + emptySpec.width() / 2);
        QVERIFY(byTag.flags.isEmpty());
    }

    // The line's right-click: the flag's own menu unless a frequency server asks for the short one.
    void aFlagLinesRightClickAsksOnlyWhenTheMenuIsOn()
    {
        Pan pan;
        QAbstractButton* line = pan.flag->roundtableLineButtonForTest();
        QVERIFY(line != nullptr);
        QCOMPARE(line->contextMenuPolicy(), Qt::DefaultContextMenu);
        pan.flag->setRoundtableLineMenuEnabled(true);
        QCOMPARE(line->contextMenuPolicy(), Qt::CustomContextMenu);
        QSignalSpy asked(pan.flag, &VfoWidget::roundtableLineMenuRequested);
        QSignalSpy clicked(pan.flag, &VfoWidget::roundtableLineClicked);
        const QPoint at(5, 5);
        QContextMenuEvent event(QContextMenuEvent::Mouse, at, line->mapToGlobal(at));
        QApplication::sendEvent(line, &event);
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.first().first().toPoint(), line->mapToGlobal(at));
        QCOMPARE(clicked.size(), 0);
        pan.flag->setRoundtableLineMenuEnabled(false);
        QCOMPARE(line->contextMenuPolicy(), Qt::DefaultContextMenu);
    }

    // 120 px usable, another frequency, through the same spot the window uses.
    void aVeryShortPanStillOpensTheInspectorInside()
    {
        Pan pan;
        for (int height = 760; height > 100 && pan.widget.roundtableInspectorSpot(0, 0).pan.height() > 120; --height) {
            pan.widget.resize(1200, height);
        }
        const SpectrumWidget::RoundtableInspectorSpot spot = pan.widget.roundtableInspectorSpot(0, 0);
        QCOMPARE(spot.pan.height(), 120);
        auto* peek = new RoundtableFrequencyPeek(&pan.widget);
        peek->setSlice(7200000, QStringLiteral("lsb"), QStringLiteral("A"));
        peek->setInspectorSpot(spot.pan, spot.dialX, spot.flags, spot.flagFlipped);
        peek->showFor(7188600, QStringLiteral("lsb"));
        RoundtableCardModel card = talkingCard(3);
        card.hz = 7188600;
        peek->refresh(card, -71.0, false);
        pan.widget.setRoundtableInspectorRect(peek->inspectorRect());
        QCOMPARE(peek->inspectorRect().height(), 120);
        QVERIFY(spot.pan.contains(peek->inspectorRect()));
        QVERIFY(peek->foldedForTest());
        QVERIFY(peek->maxScrollForTest() > 0);
        QVERIFY(peek->bodyHeightForTest() > 0);
        const QString captures = qEnvironmentVariable("NEREUS_ROUNDTABLE_ROUTING_CAPTURES");
        if (!captures.isEmpty()) {
            // The fixture holds the pan's updates, and its children's with them.
            pan.widget.setUpdatesEnabled(true);
            pan.widget.show();
            QCoreApplication::processEvents();
            QImage image = pan.draw();
            image.fill(Qt::black);
            QPainter painter(&image);
            painter.drawImage(peek->inspectorRect().topLeft(), peek->grab().toImage());
            painter.end();
            QVERIFY(image.copy(0, 0, 360, 130).save(captures + QStringLiteral("/usable-120-another-frequency.png")));
        }
    }

    void aDoubleCardHasStripesOnRailAndMeters()
    {
        Pan pan;
        pan.widget.setRoundtableTags({}, RoundtableTagDisplay::Live);
        RoundtableCardModel card = talkingCard(2, RoundtableCardModel::Kind::Double);
        card.rows[1].you = true;
        pan.widget.setRoundtableCard(card, QStringLiteral("frequency:lsb-7200000"));
        const QImage image = pan.draw();
        const auto* hit = cardHit(pan.widget);
        QVERIFY(hit != nullptr);
        const QRect rail(hit->rect.left() + 6, hit->rect.top() + 1, hit->rect.width() - 12, 3);
        QVERIFY(hasColor(image, rail, QColor(0xe0, 0x6c, 0x6e)));
        QVERIFY(hasColor(image, rail, QColor(0x3a, 0x1a, 0x1b)));
        QCOMPARE(QColor(image.pixel(hit->rect.center().x(), hit->rect.top())).name(), QStringLiteral("#c25a5c"));
        const QRect all = hit->rect.adjusted(6, 40, -6, -2);
        QVERIFY(hasColor(image, all, QColor(0x5d, 0x2c, 0x2e)));
        // Your own row is striped cyan.
        QVERIFY(hasColor(image, all, QColor(0x00, 0xd4, 0xff)));
        QVERIFY(hasColor(image, all, QColor(0x0d, 0x4a, 0x58)));
    }

    // What a thin stem looks like with a font that never reaches full coverage.
    void theTextColorProbesHoldWithNoPixelAtFullCoverage()
    {
        const QColor background(0x1b, 0x1b, 0x1f);
        const QColor amber(0xd8, 0xa5, 0x5f);
        const QColor grey(0xa6, 0xa6, 0xac);
        const auto stem = [&background](const QColor& color, double coverage) {
            QImage image(16, 12, QImage::Format_ARGB32_Premultiplied);
            image.fill(background);
            const QColor ink(int(background.red() + coverage * (color.red() - background.red())),
                             int(background.green() + coverage * (color.green() - background.green())),
                             int(background.blue() + coverage * (color.blue() - background.blue())));
            for (int y = 2; y < 9; ++y) { image.setPixelColor(7, y, ink); }
            return image;
        };
        QImage nothing(16, 12, QImage::Format_ARGB32_Premultiplied);
        nothing.fill(background);
        const QRect all = nothing.rect();
        const QImage amberStem = stem(amber, 0.6);
        QVERIFY(!hasColor(amberStem, all, amber));
        QVERIFY(changedInkIsColor(nothing, amberStem, all, amber));
        QVERIFY(hasColorOrBlend(amberStem, all, amber));
        const QImage greyStem = stem(grey, 0.6);
        QVERIFY(!changedInkIsColor(nothing, greyStem, all, amber));
        QVERIFY(!hasColorOrBlend(greyStem, all, amber));
        // No ink is not the color.
        QVERIFY(!changedInkIsColor(nothing, nothing, all, amber));
    }

    void anOldRowGoesGreyWithAnAmberAgeAndNoTick()
    {
        const QFontMetrics fm = cardMetrics();
        const auto drawOne = [&fm](bool old, bool hasPeak) {
            RoundtableCardModel card = talkingCard(1);
            card.rows[0].old = old;
            card.rows[0].hasPeak = hasPeak;
            const QRect pan(0, 0, 300, 200);
            const RoundtableCardPlacement placed = placeRoundtableCard(card, pan, 10, {}, false, fm);
            QImage image(300, 200, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            QPainter painter(&image);
            paintRoundtableCard(painter, card, placed, QStringLiteral("dBm"), fm, false);
            painter.end();
            return std::pair<QImage, QRect>(image, placed.card);
        };
        const auto [fresh, freshRect] = drawOne(false, true);
        const QRect meter = freshRect.adjusted(6, freshRect.height() - 10, -6, -4);
        QVERIFY(hasColor(fresh, meter, QColor(0x4c, 0xaf, 0x6a)));
        // The tick, at 0.85 over the track or bar: nearly white.
        bool tick = false;
        for (int y = meter.top(); y <= meter.bottom(); ++y) {
            for (int x = meter.left(); x <= meter.right(); ++x) {
                tick = tick || QColor(fresh.pixel(x, y)).lightness() > 180;
            }
        }
        QVERIFY(tick);
        const auto [old, oldRect] = drawOne(true, true);
        QCOMPARE(oldRect, freshRect);
        QVERIFY(hasColor(old, meter, QColor(0x60, 0x60, 0x66)));
        QVERIFY(!hasColor(old, meter, QColor(0x4c, 0xaf, 0x6a)));
        for (int y = meter.top(); y <= meter.bottom(); ++y) {
            for (int x = meter.left(); x <= meter.right(); ++x) {
                QVERIFY(QColor(old.pixel(x, y)).lightness() <= 180);
            }
        }
        // The age is amber: the narrowest age column, right of any value, above
        // the meter. The caption's unit shares the column and does not change.
        const QRect ageColumn(meter.right() + 1 - kAgeColumnWidth, oldRect.top(), kAgeColumnWidth, meter.top() - oldRect.top());
        QVERIFY(changedInkIsColor(fresh, old, ageColumn, QColor(0xd8, 0xa5, 0x5f)));
    }

    void aFrozenCardDrawsNoLiveColor()
    {
        Pan pan;
        pan.widget.setRoundtableTags({}, RoundtableTagDisplay::Frozen);
        RoundtableCardModel card = talkingCard(3, RoundtableCardModel::Kind::Double);
        card.frozen = true;
        card.note = QStringLiteral("Offline since 14:32 UTC");
        for (RoundtableCardRow& row : card.rows) { row.hasPeak = false; }
        pan.widget.setRoundtableCard(card, QStringLiteral("frequency:lsb-7200000"));
        const QImage image = pan.draw();
        const auto* hit = cardHit(pan.widget);
        QVERIFY(hit != nullptr);
        QCOMPARE(QColor(image.pixel(hit->rect.center().x(), hit->rect.top())).name(), QStringLiteral("#606066"));
        for (const QColor& live : {QColor(0xe0, 0x6c, 0x6e), QColor(0xc2, 0x5a, 0x5c), QColor(0x4c, 0xaf, 0x6a),
                                   QColor(0x3a, 0x1a, 0x1b), QColor(0x5d, 0x2c, 0x2e), QColor(0x00, 0xd4, 0xff)}) {
            QVERIFY2(!hasColorOrBlend(image, hit->rect, live), qPrintable(live.name()));
        }
    }

    void aDroppedLinkCardFromTheModelDrawsNoGreen()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        rig.view(7185000, {cardTurn(5, "talking", 7185000, "lsb-7184000", rig.now - 4000, {a})}, {}, {a});
        rig.state.applyStationValue("linkSince", rig.now - 1000);
        rig.state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Offline));
        const RoundtableCardModel card = rig.card(7185000);
        QVERIFY(card.frozen);
        Pan pan;
        pan.widget.setRoundtableTags(roundtableTagsFor(&rig.model, {}, RoundtableModel::frequencyContextId(7185000, QStringLiteral("lsb"))),
                                     roundtableTagDisplayFor(&rig.model));
        pan.widget.setRoundtableCard(card, RoundtableModel::frequencyContextId(7185000, QStringLiteral("lsb")));
        const QImage image = pan.draw();
        const auto* hit = cardHit(pan.widget);
        QVERIFY(hit != nullptr);
        // Grey head and border, no green anywhere on it.
        QVERIFY(!hasColorOrBlend(image, hit->rect, QColor(0x4c, 0xaf, 0x6a)));
        // The plain text does not claim to be live either.
        const QString context = RoundtableModel::frequencyContextId(7185000, QStringLiteral("lsb"));
        for (const RoundtableTag& tag : roundtableTagsFor(&rig.model, {}, context)) {
            QVERIFY2(!tag.reportLines.join(' ').contains(QStringLiteral("Talking")), qPrintable(tag.reportLines.join(' ')));
        }
    }

    void theClockIsOnADoublesHeadOnly_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<bool>("clock");
        QTest::newRow("double") << static_cast<int>(RoundtableCardModel::Kind::Double) << true;
        QTest::newRow("talking") << static_cast<int>(RoundtableCardModel::Kind::Talking) << false;
        QTest::newRow("you") << static_cast<int>(RoundtableCardModel::Kind::You) << false;
        QTest::newRow("unknown") << static_cast<int>(RoundtableCardModel::Kind::Unknown) << false;
    }

    void theClockIsOnADoublesHeadOnly()
    {
        QFETCH(int, kind);
        QFETCH(bool, clock);
        const QFontMetrics fm = cardMetrics();
        const auto draw = [&fm, kind](qint64 clockMs) {
            RoundtableCardModel card = talkingCard(1, static_cast<RoundtableCardModel::Kind>(kind));
            card.clockMs = clockMs;
            const RoundtableCardPlacement placed = placeRoundtableCard(card, QRect(0, 0, 300, 200), 10, {}, false, fm);
            QImage image(300, 200, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            QPainter painter(&image);
            paintRoundtableCard(painter, card, placed, QStringLiteral("dBm"), fm, false);
            painter.end();
            return image;
        };
        QCOMPARE(draw(83000) != draw(754000), clock);
    }

    void aHiddenStationsCardDims()
    {
        Pan pan;
        pan.widget.setRoundtableTags({}, RoundtableTagDisplay::Dimmed);
        RoundtableCardModel card = talkingCard();
        card.note = QStringLiteral("You are hidden");
        pan.widget.setRoundtableCard(card, QStringLiteral("frequency:lsb-7200000"));
        const QImage image = pan.draw();
        const auto* hit = cardHit(pan.widget);
        QVERIFY(hit != nullptr);
        QVERIFY(QColor(image.pixel(hit->rect.center().x(), hit->rect.top())).name() != QStringLiteral("#4caf6a"));
    }

    void nobodyTalkingDrawsNoCardOnlyTheCompactTag()
    {
        Pan pan;
        const QString room = QStringLiteral("frequency:lsb-7190000");
        RoundtableTag tag = tagAt(room, 7190000);
        tag.active = true;
        tag.stations = {QJsonObject{{"callsign", "W1AW"}}, QJsonObject{{"callsign", "K4ABC"}}};
        tag.members = 2;
        tag.label = QStringLiteral("W1AW");
        tag.state = static_cast<int>(RoundtableGlyph::Quiet);
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        RoundtableCardModel quiet;
        quiet.hz = 7190000;
        pan.widget.setRoundtableCard(quiet, room);
        pan.draw();
        QCOMPARE(cardHit(pan.widget), nullptr);
        const auto* hit = hitFor(pan.widget, room);
        QVERIFY(hit != nullptr);
        QCOMPARE(hit->rect.height(), 22);
        QCOMPARE(hit->kind, SpectrumWidget::RoundtableTagHit::Kind::Tag);
        QCOMPARE(hit->hz, 7190000);
    }

    void bothPathsDrawTheSameHits()
    {
        Pan pan;
        const QString room = QStringLiteral("frequency:lsb-7200000");
        RoundtableTag other = tagAt(QStringLiteral("frequency:lsb-7150000"), 7150000);
        other.stations = {QJsonObject{{"callsign", "K4ABC"}}};
        pan.widget.setRoundtableTags({other}, RoundtableTagDisplay::Live);
        pan.widget.setRoundtableCard(talkingCard(), room);
        pan.draw();
        const QList<SpectrumWidget::RoundtableTagHit> cpu = pan.widget.roundtableTagHitsForTest();
        QCOMPARE(cpu.size(), 2);
#ifdef NEREUS_GPU_SPECTRUM
        // The GPU path paints in its child, onto its overlay format.
        QImage overlay(pan.widget.size(), QImage::Format_RGBA8888_Premultiplied);
        overlay.fill(Qt::transparent);
        QVERIFY(pan.widget.m_roundtableTagLayer != nullptr);
        pan.widget.m_roundtableTagLayer->setUpdatesEnabled(true);
        pan.widget.m_roundtableTagLayer->render(&overlay);
        const QList<SpectrumWidget::RoundtableTagHit> gpu = pan.widget.roundtableTagHitsForTest();
        QCOMPARE(gpu.size(), cpu.size());
        for (qsizetype i = 0; i < cpu.size(); ++i) {
            QCOMPARE(gpu.at(i).rect, cpu.at(i).rect);
            QCOMPARE(gpu.at(i).kind, cpu.at(i).kind);
            QCOMPARE(gpu.at(i).hz, cpu.at(i).hz);
        }
#endif
    }

    void cardValueUpdatesLeaveBothOverlaysClean()
    {
        Pan pan;
        RoundtableCardModel card = talkingCard(4);
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        int invalidations = 0;
        for (int report = 0; report < 1000; ++report) {
            pan.widget.clearOverlayStaticDirtyForTest();
            pan.widget.clearOverlayDynamicDirtyForTest();
            card.rows[0].dbm = -60.0 - report % 20;
            card.clockMs += 200;
            pan.widget.setRoundtableCard(card, QStringLiteral("frequency:lsb-7200000"));
            if (pan.widget.overlayStaticDirtyForTest()) { ++invalidations; }
#ifdef NEREUS_GPU_SPECTRUM
            if (pan.widget.overlayDynamicDirtyForTest()) { ++invalidations; }
#endif
        }
        QCOMPARE(invalidations, 0);
    }

    // ---- The compact tags (D20) ----

    void aDoubleTagIsRedStripedAndNamesTwo()
    {
        Pan pan;
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7150000"), 7150000);
        tag.state = static_cast<int>(RoundtableGlyph::Doubled);
        tag.label = QStringLiteral("Double");
        tag.keyed = {QStringLiteral("W1AW"), QStringLiteral("K4ABC"), QStringLiteral("N3XYZ")};
        tag.stations = {QJsonObject{{"callsign", "W1AW"}, {"sourceRoom", "lsb-7150000"}}};
        tag.members = 12;
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const QImage image = pan.draw();
        const auto* hit = hitFor(pan.widget, tag.roomId);
        QVERIFY(hit != nullptr);
        QCOMPARE(hit->rect.height(), 22 + 2 * 16);
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        QCOMPARE(hit->rect.bottom() + 1, spec.bottom() + 1 - pan.widget.bandPlanStripHeightForTest() - 8);
        QCOMPARE(QColor(image.pixel(hit->rect.center().x(), hit->rect.top())).name(), QStringLiteral("#c25a5c"));
        const QRect stripes(hit->rect.left() + 5, hit->rect.top() + 1, hit->rect.width() - 10, 2);
        QVERIFY(hasColor(image, stripes, QColor(0xe0, 0x6c, 0x6e)));
        QVERIFY(hasColor(image, stripes, QColor(0x3a, 0x1a, 0x1b)));
        // The whole word shows, however many stations are there.
        QFont font = WorkspacePreview::readableFont(QFont(), &image);
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(11));
        font.setWeight(QFont::Bold);
        const int wordEnd = hit->rect.left() + 21 + QFontMetrics(font, &image).horizontalAdvance(QStringLiteral("Double"));
        int rightmostRed = 0;
        for (int y = hit->rect.top() + 4; y < hit->rect.top() + 20; ++y) {
            for (int x = hit->rect.left() + 21; x < hit->rect.right() - 1; ++x) {
                const QColor color(image.pixel(x, y));
                if (color.red() > 150 && color.green() < 130) { rightmostRed = std::max(rightmostRed, x); }
            }
        }
        QVERIFY2(rightmostRed >= wordEnd - 3, qPrintable(QStringLiteral("%1 of %2").arg(rightmostRed).arg(wordEnd)));
        // Frozen: grey, and the stripes stop.
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Frozen);
        const QImage frozen = pan.draw();
        QVERIFY(!hasColorOrBlend(frozen, hit->rect, QColor(0xe0, 0x6c, 0x6e)));
        QVERIFY(!hasColorOrBlend(frozen, hit->rect, QColor(0x3a, 0x1a, 0x1b)));
    }

    void eachTagIsSizedToItsOwnText()
    {
        const auto tagNamed = [](qint64 hz, const QString& label) {
            RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-%1").arg(hz), hz);
            tag.label = label;
            tag.stations = {QJsonObject{{"callsign", label}}};
            tag.members = 1;
            return tag;
        };
        Pan pan;
        RoundtableTag lengthy = tagNamed(7230000, QStringLiteral("VK3ABCDEFGHIJKLMNOPQRSTUVW/PORTABLE"));
        const RoundtableTag shortName = tagNamed(7120000, QStringLiteral("K1A"));
        const RoundtableTag longer = tagNamed(7150000, QStringLiteral("KD8LMN/QRP"));
        pan.widget.setRoundtableTags({shortName, longer, lengthy}, RoundtableTagDisplay::Live);
        pan.draw();
        const int shortWidth = hitFor(pan.widget, shortName.roomId)->rect.width();
        const int longerWidth = hitFor(pan.widget, longer.roomId)->rect.width();
        QVERIFY2(shortWidth < longerWidth, qPrintable(QStringLiteral("%1, %2").arg(shortWidth).arg(longerWidth)));
        // A name too long for the bound is cut.
        QCOMPARE(hitFor(pan.widget, lengthy.roomId)->rect.width(), 128);
        // A value update on another tag moves nothing, and the text decides, not the list.
        pan.widget.setRoundtableTags({shortName}, RoundtableTagDisplay::Live);
        pan.draw();
        QCOMPARE(hitFor(pan.widget, shortName.roomId)->rect.width(), shortWidth);
    }

    void tagWidthsAreOnlyMeasuredForNewText()
    {
        Pan pan;
        auto tags = panTags();
        pan.widget.setRoundtableTags(tags, RoundtableTagDisplay::Live);
        pan.draw();
        const qsizetype texts = pan.widget.m_roundtableTagWidths.size();
        QVERIFY(texts > 0);
        QVERIFY(pan.widget.m_roundtableTagsMeasured);
        // Another paint measures nothing; a report on the same text adds nothing.
        pan.draw();
        tags[0].reportLines = {QStringLiteral("K4ABC  -73  1s")};
        pan.widget.setRoundtableTags(tags, RoundtableTagDisplay::Live);
        QVERIFY(!pan.widget.m_roundtableTagsMeasured);
        pan.draw();
        QCOMPARE(pan.widget.m_roundtableTagWidths.size(), texts);
        tags[0].label = QStringLiteral("N9NEW");
        pan.widget.setRoundtableTags(tags, RoundtableTagDisplay::Live);
        pan.draw();
        QCOMPARE(pan.widget.m_roundtableTagWidths.size(), texts + 1);
    }

    void aPerTagWidthLaysOutWithoutOverlapAndFoldsAtTheFallback()
    {
        // Pure: wider tags need more room, and never overlap.
        QList<RoundtableTag> tags = fiveClose();
        for (qsizetype i = 0; i < tags.size(); ++i) { tags[i].width = 40.0 + 10.0 * i; }
        const auto apart = layoutRoundtableTags(tags, 1.0, 7184800, 800.0, 56.0, 4.0);
        QCOMPARE(apart.size(), 5);
        QVERIFY(!anyOverlap(apart));
        for (qsizetype i = 0; i < apart.size(); ++i) { QCOMPARE(apart.at(i).rect.width(), 40.0 + 10.0 * i); }
        const auto folded = layoutRoundtableTags(tags, 100.0, 7150000, 800.0, 56.0, 4.0);
        QCOMPARE(folded.size(), 1);
        QVERIFY(folded.first().folded);
        QCOMPARE(folded.first().rect.width(), 56.0);
        // A narrow tag still takes a strip a wide one could not.
        QList<RoundtableTag> pair{tagAt(QStringLiteral("wide"), 7150100), tagAt(QStringLiteral("small"), 7150500)};
        pair[0].width = 120.0;
        pair[1].width = 30.0;
        const auto strip = layoutRoundtableTags(pair, 1.0, 7150000, 800.0, 56.0, 4.0, {{0.0, 400.0}, {440.0, 800.0}});
        QCOMPARE(strip.size(), 1);
        QCOMPARE(strip.first().roomIds, QStringList{QStringLiteral("small")});
        QVERIFY(strip.first().rect.left() >= 404.0 && strip.first().rect.right() <= 436.0);
    }

    // The board's main case: lower sideband, the flag right of the dial.
    void onLowerSidebandTheCardIsPastTheFlagOnTheRight()
    {
        Pan pan;
        pan.flag->setMode(DSPMode::LSB);
        pan.widget.updateVfoPositions();
        pan.widget.refreshReceiverFlagRegion();
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        const int dialX = pan.widget.hzToX(7200000.0, spec);
        QVERIFY(pan.flag->geometry().left() >= dialX - 30);
        pan.widget.setRoundtableTags({}, RoundtableTagDisplay::Live);
        pan.widget.setRoundtableCard(talkingCard(), QStringLiteral("frequency:lsb-7200000"));
        pan.draw();
        const auto* hit = cardHit(pan.widget);
        QVERIFY(hit != nullptr);
        QVERIFY(hit->rect.left() > pan.flag->geometry().right());
        QVERIFY(spec.contains(hit->rect));
        QCOMPARE(hit->rect.bottom() + 1, spec.bottom() + 1 - pan.widget.bandPlanStripHeightForTest() - 8);
    }

    void onLowerSidebandWithoutRoomTheCardComesBackBeforeTheFlag()
    {
        Pan pan;
        pan.flag->setMode(DSPMode::LSB);
        pan.flag->setFrequency(7236000.0);
        pan.widget.updateVfoPositions();
        pan.widget.refreshReceiverFlagRegion();
        RoundtableCardModel card = talkingCard();
        card.hz = 7236000;
        pan.widget.setRoundtableTags({}, RoundtableTagDisplay::Live);
        pan.widget.setRoundtableCard(card, QStringLiteral("frequency:lsb-7236000"));
        pan.draw();
        const auto* hit = cardHit(pan.widget);
        QVERIFY(hit != nullptr);
        QVERIFY2(hit->rect.right() < pan.flag->geometry().left(),
                 qPrintable(QStringLiteral("card to %1, flag from %2").arg(hit->rect.right()).arg(pan.flag->geometry().left())));
    }

    void aTagNamesItsTalkerAndPlusTheRest()
    {
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7150000"), 7150000);
        tag.stations = {QJsonObject{{"callsign", "K4ABC"}}};
        tag.members = 1;
        tag.label = QStringLiteral("K4ABC");
        Pan pan;
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const QImage alone = pan.draw();
        const QRect rect = hitFor(pan.widget, tag.roomId)->rect;
        QCOMPARE(rect.height(), 22);
        tag.stations.append(QJsonObject{{"callsign", "W1AW"}});
        tag.stations.append(QJsonObject{{"callsign", "N3XYZ"}});
        tag.members = 3;
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const QImage three = pan.draw();
        QVERIFY(hitFor(pan.widget, tag.roomId)->rect.height() == 22);
        QVERIFY2(alone.copy(rect) != three.copy(hitFor(pan.widget, tag.roomId)->rect), "+2 is drawn");
        QCOMPARE(roundtableSignalText(-61.0, QStringLiteral("dBm")), QString(QChar(0x2212)) + QStringLiteral("61"));
        QCOMPARE(roundtableSignalText(-61.0, QStringLiteral("S-units")), QStringLiteral("S9 +12"));
        QCOMPARE(roundtableSignalText(-61.0, QStringLiteral("Both")),
                 QStringLiteral("S9 +12 ") + QChar(0x2212) + QStringLiteral("61"));
    }

    void oneDoubleAcrossSeveralFrequenciesInOneRoomIsOneTag()
    {
        const auto doubleAt = [](qint64 hz, const QStringList& keyed, const QString& source) {
            RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-%1").arg(hz), hz);
            tag.state = static_cast<int>(RoundtableGlyph::Doubled);
            tag.label = QStringLiteral("Double");
            tag.keyed = keyed;
            tag.stations = {QJsonObject{{"callsign", keyed.first()}, {"sourceRoom", source}}};
            return tag;
        };
        const QStringList both{QStringLiteral("W1AW"), QStringLiteral("K4ABC")};
        const QString room = QStringLiteral("lsb-7184000");
        const QList<RoundtableTag> merged = roundtableMergeDoubleTags({
            doubleAt(7185000, both, room), doubleAt(7185040, both, room), doubleAt(7185080, both, room),
            // Another double in the same room, and one in another room, stay.
            doubleAt(7187000, {QStringLiteral("N3XYZ"), QStringLiteral("W9ABC")}, room),
            doubleAt(7185100, both, QStringLiteral("lsb-7186000"))});
        QCOMPARE(merged.size(), 3);
        QCOMPARE(merged.first().hz, 7185000);
        QCOMPARE(merged.first().keyed, both);
        QCOMPARE(merged.first().mergedHz, (QList<qint64>{7185040, 7185080}));
        QCOMPARE(merged.first().members, 3);
        // On the pan.
        Pan pan;
        pan.widget.setRoundtableTags({doubleAt(7150000, both, room), doubleAt(7150040, both, room)},
                                     RoundtableTagDisplay::Live);
        pan.draw();
        QCOMPARE(pan.widget.roundtableTagHitsForTest().size(), 1);
        QVERIFY(!pan.widget.roundtableTagHitsForTest().first().folded);
        QCOMPARE(pan.widget.roundtableTags().size(), 2);
    }

    void aFrequencyAnotherSlicesFlagSpeaksForHasNoTag()
    {
        Pan pan;
        VfoWidget* other = pan.widget.addVfoWidget(1);
        other->setFrequency(7150000.0);
        other->setActiveSlice(false);
        pan.widget.updateVfoPositions();
        const QString room = QStringLiteral("frequency:lsb-7150000");
        RoundtableTag there = tagAt(room, 7150000);
        there.stations = {QJsonObject{{"callsign", "K4ABC"}}};
        RoundtableTag elsewhere = tagAt(QStringLiteral("frequency:lsb-7240000"), 7240000);
        elsewhere.stations = {QJsonObject{{"callsign", "N3XYZ"}}};
        pan.widget.setRoundtableTags({there, elsewhere}, RoundtableTagDisplay::Live);
        pan.draw();
        QCOMPARE(hitFor(pan.widget, room), nullptr);
        QVERIFY(hitFor(pan.widget, elsewhere.roomId) != nullptr);
    }

    void aTagWithNoClearSpotIsLeftOffNeverOverlapping()
    {
        // The active flag and the card leave little of a narrow pan.
        Pan pan;
        pan.widget.resize(560, 760);
        pan.widget.setFrequencyRange(kPanCenterHz, kPanSpanHz);
        pan.widget.updateVfoPositions();
        QList<RoundtableTag> tags;
        for (int i = 0; i < 12; ++i) {
            RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-%1").arg(7110000 + i * 15000), 7110000 + i * 15000);
            tag.stations = {QJsonObject{{"callsign", QStringLiteral("K%1ABC").arg(i)}}};
            tag.mine = true;  // never folds, so some must be left off
            tags.append(tag);
        }
        pan.widget.setRoundtableTags(tags, RoundtableTagDisplay::Live);
        pan.widget.setRoundtableCard(talkingCard(), QStringLiteral("frequency:lsb-7200000"));
        pan.draw();
        const auto& hits = pan.widget.roundtableTagHitsForTest();
        QVERIFY(cardHit(pan.widget) != nullptr);
        QVERIFY(hits.size() < 13);
        const QRegion flags = pan.widget.receiverFlagRegion();
        for (qsizetype i = 0; i < hits.size(); ++i) {
            QVERIFY(!flags.intersects(hits.at(i).rect));
            for (qsizetype j = i + 1; j < hits.size(); ++j) {
                QVERIFY2(!hits.at(i).rect.intersects(hits.at(j).rect), "two things overlap on the pan");
            }
        }
    }

    // NEREUS_ROUNDTABLE_CARD_CAPTURES=<dir>: the card over a spectrum, by the CPU
    // painter and the real flag chrome, offscreen.
    void capturesForTheBoard_data()
    {
        QTest::addColumn<QString>("scene");
        QTest::addColumn<int>("height");
        QTest::newRow("talking") << QStringLiteral("talking") << 640;
        QTest::newRow("double") << QStringLiteral("double") << 640;
        QTest::newRow("frozen") << QStringLiteral("frozen") << 640;
        QTest::newRow("reconnecting") << QStringLiteral("reconnecting") << 640;
        QTest::newRow("short") << QStringLiteral("short") << 300;
        QTest::newRow("near-side") << QStringLiteral("near-side") << 640;
        QTest::newRow("flipped") << QStringLiteral("flipped") << 640;
    }

    void capturesForTheBoard()
    {
        const QString directory = qEnvironmentVariable("NEREUS_ROUNDTABLE_CARD_CAPTURES");
        if (directory.isEmpty()) { return; }
        QFETCH(QString, scene);
        QFETCH(int, height);
        AppSettings::instance().setValue(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("Both"));
        Pan pan(height);
        RoundtableCardModel card = talkingCard(7);
        RoundtableTagDisplay display = RoundtableTagDisplay::Live;
        // Lower sideband hangs the flag right of the dial, as the board does;
        // upper sideband is the flipped flag.
        pan.flag->setMode(scene == QLatin1String("flipped") ? DSPMode::USB : DSPMode::LSB);
        if (scene == QLatin1String("near-side")) {
            card.hz = 7236000;
            pan.flag->setFrequency(7236000.0);
        }
        pan.widget.updateVfoPositions();
        pan.widget.refreshReceiverFlagRegion();
        if (scene == QLatin1String("double") || scene == QLatin1String("short")) {
            card = talkingCard(4, RoundtableCardModel::Kind::Double);
            card.rows[1].you = true;
        }
        card.rows[2].old = card.rows.size() > 2;
        if (scene == QLatin1String("frozen") || scene == QLatin1String("reconnecting")) {
            card.frozen = true;
            card.note = scene == QLatin1String("frozen") ? QStringLiteral("Offline since 14:32 UTC")
                                                         : QStringLiteral("Reconnecting");
            for (RoundtableCardRow& row : card.rows) { row.hasPeak = false; }
            display = RoundtableTagDisplay::Frozen;
        }
        RoundtableTag dbl = tagAt(QStringLiteral("frequency:lsb-7150000"), 7150000);
        dbl.state = static_cast<int>(RoundtableGlyph::Doubled);
        dbl.label = QStringLiteral("Double");
        dbl.keyed = {QStringLiteral("KD8LMN"), QStringLiteral("AA4QQ")};
        dbl.stations = {QJsonObject{{"callsign", "KD8LMN"}, {"sourceRoom", "lsb-7150000"}}};
        RoundtableTag talk = tagAt(QStringLiteral("frequency:lsb-7125000"), 7125000);
        talk.state = static_cast<int>(RoundtableGlyph::Talking);
        talk.label = QStringLiteral("VE3XYZ");
        talk.members = 3;
        talk.stations = {QJsonObject{{"callsign", "VE3XYZ"}}, QJsonObject{{"callsign", "K1A"}}, QJsonObject{{"callsign", "K1B"}}};
        RoundtableTag quiet = tagAt(QStringLiteral("frequency:lsb-7170000"), 7170000);
        quiet.state = static_cast<int>(RoundtableGlyph::Quiet);
        quiet.label = QStringLiteral("W9ABC");
        quiet.members = 2;
        quiet.stations = {QJsonObject{{"callsign", "W9ABC"}}, QJsonObject{{"callsign", "K1C"}}};
        pan.widget.setRoundtableTags({talk, dbl, quiet}, display);
        pan.widget.setRoundtableCard(card, QStringLiteral("frequency:lsb-%1").arg(card.hz));
        AppSettings::instance().remove(QStringLiteral("Roundtable/SignalFormat"));
        pan.widget.setUpdatesEnabled(true);

        QImage product(pan.widget.size(), QImage::Format_ARGB32_Premultiplied);
        product.fill(QColor(15, 15, 26));
        QPainter painter(&product);
        const QRect spectrum = pan.widget.roundtableTagSpectrumRectForTest();
        QVector<float> fixture(spectrum.width(), -125.0f);
        for (int x = 0; x < fixture.size(); ++x) {
            fixture[x] += static_cast<float>(3 * std::sin(x * 0.23) + 58 * std::exp(-std::pow((x - 300) / 20.0, 2)));
        }
        pan.widget.setRenderedPixelsForTest(fixture);
        pan.widget.drawFrequencyGridForTest(painter, spectrum);
        pan.widget.drawSpectrumForTest(painter, spectrum);
        pan.widget.drawFreqScaleForTest(painter, pan.widget.freqScaleRectForTest());
        pan.widget.drawRoundtableTagsForTest(painter, spectrum);
        for (QWidget* child : pan.widget.findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            if (child->isVisible() && child != pan.widget.m_roundtableTagLayer) { child->render(&painter, child->pos()); }
        }
        painter.end();
        const QRect crop(0, 0, pan.widget.width(), spectrum.bottom() + 1);
        QVERIFY(product.copy(crop).save(directory + QLatin1Char('/') + scene + QStringLiteral(".png")));
    }

    void selectedCochannelKeepsTheOnlyFreeSlot()
    {
        const auto placed = layoutRoundtableTags({tagAt("earlier", 10), tagAt("neighbor", 20),
            tagAt("selected", 90, true)}, 1, 0, 100, 80, 4);
        QCOMPARE(placed.size(), 1);
        QCOMPARE(placed.first().roomIds, QStringList{QStringLiteral("selected")});
        QVERIFY(!placed.first().folded);
    }

    void fiveTagsFiftyHertzApartFoldIntoOne()
    {
        // 100 Hz per pixel: the five sit within 2 px of each other.
        const auto placements = layoutRoundtableTags(fiveClose(), 100.0, 7150000, 800.0, kTagWidth, kGap);
        QCOMPARE(placements.size(), 1);
        QVERIFY(placements.first().folded);
        QCOMPARE(placements.first().roomIds,
                 (QStringList{QStringLiteral("r0"), QStringLiteral("r1"), QStringLiteral("r2"),
                              QStringLiteral("r3"), QStringLiteral("r4")}));
        // The fold sits on the rooms, 350 px across.
        QVERIFY(std::abs(placements.first().rect.center().x() - 351.0) < 2.0);
        QCOMPARE(placements.first().rect.width(), kTagWidth);
    }

    void zoomingInOpensTheFold()
    {
        // 1 Hz per pixel: 50 px apart, room for 40 px tags.
        const auto placements = layoutRoundtableTags(fiveClose(), 1.0, 7184800, 800.0, 40.0, kGap);
        QCOMPARE(placements.size(), 5);
        for (int i = 0; i < 5; ++i) {
            QVERIFY(!placements.at(i).folded);
            QCOMPARE(placements.at(i).roomIds, QStringList{QStringLiteral("r%1").arg(i)});
            // Each on its own frequency.
            QCOMPARE(placements.at(i).rect.center().x(), 200.0 + i * 50.0);
        }
        QVERIFY(!anyOverlap(placements));

        // Between the two, a middle zoom opens part of it.
        const auto middle = layoutRoundtableTags(fiveClose(), 4.0, 7184000, 800.0, 40.0, kGap);
        QVERIFY(middle.size() > 1);
    }

    void yourSlicesRoomStaysOutOfTheFold()
    {
        const auto placements =
            layoutRoundtableTags(fiveClose(2), 100.0, 7150000, 800.0, kTagWidth, kGap);
        QCOMPARE(placements.size(), 2);
        int folds = 0;
        for (const RoundtableTagPlacement& placement : placements) {
            if (placement.folded) {
                ++folds;
                QCOMPARE(placement.roomIds,
                         (QStringList{QStringLiteral("r0"), QStringLiteral("r1"), QStringLiteral("r3"),
                                      QStringLiteral("r4")}));
            } else {
                QCOMPARE(placement.roomIds, QStringList{QStringLiteral("r2")});
            }
        }
        QCOMPARE(folds, 1);
        QVERIFY(!anyOverlap(placements));
    }

    void aRoomThatNamesYouStaysOutOfTheFold()
    {
        const auto placements =
            layoutRoundtableTags(fiveClose(0, true), 100.0, 7150000, 800.0, kTagWidth, kGap);
        QCOMPARE(placements.size(), 2);
        QVERIFY(!placements.first().folded);
        QCOMPARE(placements.first().roomIds, QStringList{QStringLiteral("r0")});
        QVERIFY(placements.last().folded);
        QCOMPARE(placements.last().roomIds.size(), 4);
    }

    void twoOverlappingTagsMoveApartInsteadOfFolding()
    {
        // 10 px apart with 80 px tags and the pan nearly empty.
        const QList<RoundtableTag> tags{tagAt(QStringLiteral("a"), 7185000),
                                        tagAt(QStringLiteral("b"), 7186000)};
        const auto placements = layoutRoundtableTags(tags, 100.0, 7150000, 800.0, kTagWidth, kGap);
        QCOMPARE(placements.size(), 2);
        QVERIFY(!placements.at(0).folded);
        QVERIFY(!placements.at(1).folded);
        QVERIFY(!anyOverlap(placements));
        QVERIFY(placements.at(1).rect.left() - placements.at(0).rect.right() >= kGap - 1e-9);
        // Each stays within a tag's width of its own frequency.
        QVERIFY(std::abs(placements.at(0).rect.center().x() - 350.0) <= kTagWidth);
        QVERIFY(std::abs(placements.at(1).rect.center().x() - 360.0) <= kTagWidth);
    }

    void aLoneNeighborOfYourRoomMovesAside()
    {
        // A fold holds two or more: your room and one other, on one
        // frequency, sit side by side.
        const QList<RoundtableTag> tags{tagAt(QStringLiteral("mine"), 7185000, true),
                                        tagAt(QStringLiteral("other"), 7185050)};
        const auto placements = layoutRoundtableTags(tags, 100.0, 7150000, 800.0, kTagWidth, kGap);
        QCOMPARE(placements.size(), 2);
        QVERIFY(!placements.at(0).folded);
        QVERIFY(!placements.at(1).folded);
        QVERIFY(!anyOverlap(placements));
    }

    void foldsWidenOnlyWhenTheTagsDoNotFit()
    {
        // Four pairs, each pair 10 px apart. Across 800 px the eight tags
        // fit side by side: none fold.
        QList<RoundtableTag> wide;
        for (int pair = 0; pair < 4; ++pair) {
            const qint64 hz = 7150000 + (100 + pair * 200) * 100;
            wide.append(tagAt(QStringLiteral("w%1a").arg(pair), hz));
            wide.append(tagAt(QStringLiteral("w%1b").arg(pair), hz + 1000));
        }
        const auto open = layoutRoundtableTags(wide, 100.0, 7150000, 800.0, kTagWidth, kGap);
        QCOMPARE(open.size(), 8);
        for (const RoundtableTagPlacement& placement : open) {
            QVERIFY(!placement.folded);
        }
        QVERIFY(!anyOverlap(open));

        // Across 400 px eight tags (676 px) do not fit, four (340 px) do:
        // each pair folds.
        QList<RoundtableTag> narrow;
        for (int pair = 0; pair < 4; ++pair) {
            const qint64 hz = 7150000 + (50 + pair * 100) * 100;
            narrow.append(tagAt(QStringLiteral("n%1a").arg(pair), hz));
            narrow.append(tagAt(QStringLiteral("n%1b").arg(pair), hz + 1000));
        }
        const auto folded = layoutRoundtableTags(narrow, 100.0, 7150000, 400.0, kTagWidth, kGap);
        QCOMPARE(folded.size(), 4);
        for (const RoundtableTagPlacement& placement : folded) {
            QVERIFY(placement.folded);
            QCOMPARE(placement.roomIds.size(), 2);
        }
        QVERIFY(!anyOverlap(folded));
    }

    void placementsStayOnThePanAndOffPanRoomsAreLeftOut()
    {
        const QList<RoundtableTag> tags{tagAt(QStringLiteral("left"), 7150100),
                                        tagAt(QStringLiteral("right"), 7229900),
                                        tagAt(QStringLiteral("off"), 7240000),
                                        tagAt(QStringLiteral("below"), 7140000)};
        const auto placements = layoutRoundtableTags(tags, 100.0, 7150000, 800.0, kTagWidth, kGap);
        QCOMPARE(placements.size(), 2);
        QCOMPARE(placements.first().roomIds, QStringList{QStringLiteral("left")});
        QCOMPARE(placements.last().roomIds, QStringList{QStringLiteral("right")});
        for (const RoundtableTagPlacement& placement : placements) {
            QVERIFY(placement.rect.left() >= kGap - 1e-9);
            QVERIFY(placement.rect.right() <= 800.0 - kGap + 1e-9);
        }
        QVERIFY(layoutRoundtableTags(tags, 0.0, 7150000, 800.0, kTagWidth, kGap).isEmpty());
        QVERIFY(layoutRoundtableTags(tags, 100.0, 7150000, 0.0, kTagWidth, kGap).isEmpty());
    }

    void tagsKeepClearOfAFlagOverTheirRow()
    {
        // A flag over 300 to 560 px: the room under its left half goes to
        // the left of it, the one under its right half to the right.
        const QList<RoundtableTag> tags{tagAt(QStringLiteral("left"), 7150000 + 340 * 100),
                                        tagAt(QStringLiteral("right"), 7150000 + 540 * 100),
                                        tagAt(QStringLiteral("far"), 7150000 + 700 * 100)};
        const QList<RoundtableKeepOut> flag{{300.0, 560.0}};
        const auto placements =
            layoutRoundtableTags(tags, 100.0, 7150000, 800.0, kTagWidth, kGap, flag);
        QCOMPARE(placements.size(), 3);
        QVERIFY(placements.at(0).rect.right() <= 300.0 - kGap + 1e-9);
        QVERIFY(placements.at(1).rect.left() >= 560.0 + kGap - 1e-9);
        QVERIFY(!anyOverlap(placements));
        for (const RoundtableTagPlacement& placement : placements) {
            QVERIFY(placement.rect.right() <= 300.0 || placement.rect.left() >= 560.0);
        }

        // Only the stretches beside the flag count: across 800 px with
        // 600 px kept out, the five rooms 50 Hz apart fold, and with the
        // whole pan kept out nothing is placed.
        const auto crowded = layoutRoundtableTags(fiveClose(), 1.0, 7184800, 800.0, 40.0, kGap,
                                                  {{100.0, 700.0}});
        QVERIFY(crowded.size() < 5);
        QVERIFY(!anyOverlap(crowded));
        for (const RoundtableTagPlacement& placement : crowded) {
            QVERIFY(placement.rect.right() <= 100.0 || placement.rect.left() >= 700.0);
        }
        QVERIFY(layoutRoundtableTags(tags, 100.0, 7150000, 800.0, kTagWidth, kGap, {{0.0, 800.0}}).isEmpty());
    }

    void theModelsRoomsBecomeTags()
    {
        NullLink link;
        RoundtableState state;
        RoundtableModel model;
        model.attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        state.applyStationValue("sliceRooms", QStringLiteral("{\"0\":\"lsb-7185000\"}"));
        state.applyStationValue("mentionCounts", QStringLiteral("{\"lsb-7212000\":2}"));
        model.applyRooms(QJsonObject{
            {QStringLiteral("rooms"),
             QJsonArray{summary(QStringLiteral("lsb-7185000"), 7185000, QStringLiteral("talking"),
                                QJsonArray{QStringLiteral("W1TLK")}, 9),
                        summary(QStringLiteral("lsb-7200000"), 7200000, QStringLiteral("doubled"),
                                QJsonArray{QStringLiteral("K4ABC"), QStringLiteral("N3XYZ")}, 4),
                        summary(QStringLiteral("lsb-7212000"), 7212000, QStringLiteral("unknown"),
                                QJsonArray{}, 3),
                        summary(QStringLiteral("lsb-7230000"), 7230000, QStringLiteral("quiet"),
                                QJsonArray{}, 2)}}});

        const QList<RoundtableTag> tags = roundtableTagsFor(&model, QList<int>{0, 1});
        QCOMPARE(tags.size(), 4);
        QCOMPARE(tags.at(0).roomId, QStringLiteral("lsb-7185000"));
        QCOMPARE(tags.at(0).hz, 7185000);
        QCOMPARE(tags.at(0).label, QStringLiteral("W1TLK"));
        QCOMPARE(tags.at(0).state, static_cast<int>(RoundtableGlyph::Talking));
        QCOMPARE(tags.at(0).members, 9);
        QVERIFY(tags.at(0).mine);
        QVERIFY(!tags.at(0).mentionsMe);
        QCOMPARE(tags.at(1).label, QStringLiteral("Double"));
        QCOMPARE(tags.at(1).state, static_cast<int>(RoundtableGlyph::Doubled));
        QCOMPARE(tags.at(2).label, QStringLiteral("Unknown"));
        QCOMPARE(tags.at(2).state, static_cast<int>(RoundtableGlyph::Unknown));
        QVERIFY(tags.at(2).mentionsMe);
        QVERIFY(!tags.at(2).mine);
        QCOMPARE(tags.at(3).label, QStringLiteral("Quiet"));
        QCOMPARE(tags.at(3).state, static_cast<int>(RoundtableGlyph::Quiet));

        // Your own turn in your slice's room: You, in cyan (U8).
        model.applyRoom(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("room")},
            {QStringLiteral("id"), QStringLiteral("lsb-7185000")},
            {QStringLiteral("members"),
             QJsonArray{QJsonObject{{QStringLiteral("ref"), QStringLiteral("me")},
                                    {QStringLiteral("callsign"), QStringLiteral("KG4VCF")},
                                    {QStringLiteral("you"), true}}}},
            // Yours: you are its talker (`mine` is the name you gave a
            // turn, wire spec 6.9).
            {QStringLiteral("now"),
             QJsonArray{QJsonObject{{QStringLiteral("id"), 40}, {QStringLiteral("kind"), QStringLiteral("talking")},
                                    {QStringLiteral("talkers"),
                                     QJsonArray{QJsonObject{{QStringLiteral("ref"), QStringLiteral("me")},
                                                            {QStringLiteral("callsign"), QStringLiteral("KG4VCF")},
                                                            {QStringLiteral("nth"), 1}}}},
                                    {QStringLiteral("start"), 1}, {QStringLiteral("end"), QJsonValue::Null},
                                    {QStringLiteral("names"), QJsonArray{}}, {QStringLiteral("mine"), QJsonValue::Null}}}},
            {QStringLiteral("earlier"), QJsonArray{}},
            {QStringLiteral("messages"), QJsonArray{}}});
        const QList<RoundtableTag> keyed = roundtableTagsFor(&model, QList<int>{0});
        QCOMPARE(keyed.at(0).state, static_cast<int>(RoundtableGlyph::You));
        QCOMPARE(keyed.at(0).label, QStringLiteral("You"));

        for (const RoundtableTag& tag : tags + keyed) {
            QVERIFY2(OperatorWording::isPlain(tag.label), qPrintable(tag.label));
        }
        QVERIFY(roundtableTagsFor(nullptr, QList<int>{0}).isEmpty());
        model.detach();
    }

    void meterRunsOnTheFlagScale()
    {
        QCOMPARE(roundtableMeterFraction(-121.0), 0.0);
        QCOMPARE(roundtableMeterFraction(-33.0), 1.0);
        QCOMPARE(roundtableMeterFraction(-200.0), 0.0);
        QCOMPARE(roundtableMeterFraction(0.0), 1.0);
        QCOMPARE(roundtableMeterFraction(-77.0), 0.5);
    }

    void peakRisesAtOnceFallsTwoDbASecondAndResetsOnANewTurn()
    {
        RoundtablePeakTracker peaks;
        const QString room = QStringLiteral("lsb-7184000");
        const QString rx = QStringLiteral("rx");
        QCOMPARE(peaks.update(room, rx, 1, -80.0, 1000), -80.0);
        QCOMPARE(peaks.update(room, rx, 1, -60.0, 1000), -60.0);
        QCOMPARE(peaks.update(room, rx, 1, -90.0, 3000), -64.0);
        // Never below the reading itself.
        QCOMPARE(peaks.update(room, rx, 1, -50.0, 3000), -50.0);
        QCOMPARE(peaks.update(room, rx, 1, -90.0, 103000), -90.0);
        peaks.update(room, rx, 1, -40.0, 104000);
        QCOMPARE(peaks.update(room, rx, 2, -70.0, 104000), -70.0);
        // Another listener, and the same ref in another source room, hold apart.
        QCOMPARE(peaks.update(room, QStringLiteral("other"), 2, -85.0, 104000), -85.0);
        QCOMPARE(peaks.update(QStringLiteral("lsb-7188000"), rx, 2, -95.0, 104000), -95.0);
        QCOMPARE(peaks.update(room, rx, 2, -100.0, 104000), -70.0);
    }

    void keyedStationsMerelyNearEachOtherAreNotADouble()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        const QJsonObject b = cardPerson("lsb-7188000", "b", "K4ABC", 7188000, true);
        rig.directory({a, b});
        QVERIFY(!roundtableIsDouble({}, QJsonArray{QJsonObject{{"id", "lsb-7184000"}, {"stations", QJsonArray{a}}}, QJsonObject{{"id", "lsb-7188000"}, {"stations", QJsonArray{b}}}}, 7187000, QStringLiteral("lsb")));
        for (const RoundtableTag& tag : roundtableTagsFor(&rig.model, {})) {
            QCOMPARE(tag.state, static_cast<int>(RoundtableGlyph::Talking));
        }
        // No view yet, and with an open turn each: the nearest talker either way.
        for (bool withView : {false, true}) {
            if (withView) {
                rig.view(7187000, {cardTurn(1, "talking", 7185000, "lsb-7184000", 100, {a}),
                                   cardTurn(2, "talking", 7188000, "lsb-7188000", 200, {b})}, {}, {a, b});
            }
            const RoundtableFlagLine flag = roundtableFlagLineFor(&rig.model, 0, 7187000, QStringLiteral("lsb"));
            QCOMPARE(flag.glyph, static_cast<int>(RoundtableGlyph::Talking));
            QCOMPARE(flag.text, QStringLiteral("Roundtable · 2 · K4ABC"));
            const RoundtableCardModel card = rig.card(7187000);
            QCOMPARE(card.kind, RoundtableCardModel::Kind::Talking);
            QCOMPARE(card.keyed, QStringList{QStringLiteral("K4ABC")});
        }
    }

    void aDoubleNamesBothOnAnyFrequency()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        const QJsonObject b = cardPerson("lsb-7184000", "b", "K4ABC", 7185040, true);
        rig.directory({a, b});
        // Known only from the directory: two keyed in one source room, on tags
        // of their own 40 Hz apart, neither active.
        const QStringList both{QStringLiteral("W1AW"), QStringLiteral("K4ABC")};
        const QList<RoundtableTag> directoryOnly = roundtableTagsFor(&rig.model, {},
            RoundtableModel::frequencyContextId(7100000, QStringLiteral("lsb")));
        QCOMPARE(directoryOnly.size(), 2);
        for (const RoundtableTag& tag : directoryOnly) {
            QCOMPARE(tag.state, static_cast<int>(RoundtableGlyph::Doubled));
            QCOMPARE(tag.label, QStringLiteral("Double"));
            QCOMPARE(QSet<QString>(tag.keyed.cbegin(), tag.keyed.cend()), QSet<QString>(both.cbegin(), both.cend()));
        }
        QCOMPARE(rig.card(7185000).kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(rig.card(7185000).keyed, both);
        // An open doubled turn, the room's center on the wire: active or not.
        const QJsonObject turn = cardTurn(5, "doubled", 7184000, "lsb-7184000", 100, {a, b});
        rig.view(7185000, {turn}, {}, {a, b});
        const QString context = RoundtableModel::frequencyContextId(7185000, QStringLiteral("lsb"));
        for (const QString& active : {context, RoundtableModel::frequencyContextId(7100000, QStringLiteral("lsb"))}) {
            for (const RoundtableTag& tag : roundtableTagsFor(&rig.model, {}, active)) {
                if (tag.roomId == context) {
                    QCOMPARE(tag.state, static_cast<int>(RoundtableGlyph::Doubled));
                    QCOMPARE(tag.keyed, both);
                }
            }
        }
        const RoundtableCardModel card = rig.card(7185000);
        QCOMPARE(card.kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(card.keyed, both);
        QCOMPARE(card.clockMs, rig.now - 100);
        QVERIFY(roundtableFlagLineFor(&rig.model, 0, 7185000, QStringLiteral("lsb")).text.endsWith(QStringLiteral("Double")));
    }

    // D1: a double in the room next door says nothing about this frequency. Two
    // rooms 3 kHz apart: one quiet station, and a keyed pair.
    void aDoubleNextDoorLeavesAQuietRoomQuietAndIsOneTag()
    {
        CardRig rig;
        const QJsonObject q = cardPerson("lsb-7200000", "q", "W2QQQ", 7200000, false);
        const QJsonObject a = cardPerson("lsb-7203000", "a", "W1AW", 7203000, true);
        const QJsonObject b = cardPerson("lsb-7203000", "b", "K4ABC", 7203040, true);
        rig.rooms(QJsonArray{
            QJsonObject{{"id", "lsb-7200000"}, {"family", "lsb"}, {"state", "quiet"}, {"stations", QJsonArray{q}}},
            QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "doubled"},
                {"talkers", QJsonArray{"W1AW", "K4ABC"}}, {"stations", QJsonArray{a, b}}}});
        const QList<RoundtableTag> tags = roundtableTagsFor(&rig.model, {});
        QCOMPARE(tags.size(), 3);
        QCOMPARE(tags.at(0).hz, 7200000);
        QCOMPARE(tags.at(0).state, static_cast<int>(RoundtableGlyph::Quiet));
        QCOMPARE(tags.at(0).label, QStringLiteral("W2QQQ"));
        QVERIFY(tags.at(0).keyed.isEmpty());
        QCOMPARE(tags.at(1).state, static_cast<int>(RoundtableGlyph::Doubled));
        QCOMPARE(tags.at(2).state, static_cast<int>(RoundtableGlyph::Doubled));

        const QList<RoundtableTag> onPan = roundtableMergeDoubleTags(tags);
        QCOMPARE(onPan.size(), 2);
        QCOMPARE(onPan.at(1).mergedHz, QList<qint64>{7203040});
        QCOMPARE(onPan.at(1).keyed, (QStringList{QStringLiteral("W1AW"), QStringLiteral("K4ABC")}));

        // The card reads by D3, within 5 kHz: the tag above stays quiet (see below).
        QCOMPARE(rig.card(7200000).kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(rig.card(7203000).kind, RoundtableCardModel::Kind::Double);
        // So does the flag, by the card's rule, from before any stream.
        QCOMPARE(roundtableFlagLineFor(&rig.model, 0, 7200000, QStringLiteral("lsb")).glyph,
                 static_cast<int>(RoundtableGlyph::Doubled));
        const QJsonArray rooms = rig.model.rooms();
        QVERIFY(!roundtableIsDouble({}, rooms, 7200000, QStringLiteral("lsb")));
        QVERIFY(roundtableIsDouble({}, rooms, 7203000, QStringLiteral("lsb")));
    }

    // Clicking quiet tag Q opens a view that holds the double 3 kHz away. The view
    // and card say Double (D3), Q's tag stays quiet, and one double is one tag
    // (D1, D20), whether Q is only viewed or is your slice's own frequency.
    void aViewedQuietTagNextToADoubleStaysQuietAndOneTagSaysDouble()
    {
        CardRig rig;
        const QJsonObject q = cardPerson("lsb-7200000", "q", "W2QQQ", 7200000, false);
        const QJsonObject a = cardPerson("lsb-7203000", "a", "W1AW", 7203000, true);
        const QJsonObject b = cardPerson("lsb-7203000", "b", "K4ABC", 7203040, true);
        rig.rooms(QJsonArray{
            QJsonObject{{"id", "lsb-7200000"}, {"family", "lsb"}, {"state", "quiet"}, {"stations", QJsonArray{q}}},
            QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "doubled"},
                {"talkers", QJsonArray{"W1AW", "K4ABC"}}, {"stations", QJsonArray{a, b}}}});
        const QString context = RoundtableModel::frequencyContextId(7200000, QStringLiteral("lsb"));
        QVERIFY(rig.model.readAt(7200000, QStringLiteral("lsb")));
        rig.view(7200000, {cardTurn(5, "doubled", 7203000, "lsb-7203000", 100, {a, b})}, {}, {q, a, b});
        QCOMPARE(rig.card(7200000).kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(rig.card(7200000).keyed, (QStringList{QStringLiteral("W1AW"), QStringLiteral("K4ABC")}));
        for (const QString& active : {QString(), context}) {
            const QList<RoundtableTag> onPan = roundtableMergeDoubleTags(roundtableTagsFor(&rig.model, {}, active));
            QCOMPARE(onPan.size(), qsizetype(2));
            QCOMPARE(onPan.at(0).hz, qint64(7200000));
            QCOMPARE(onPan.at(0).state, static_cast<int>(RoundtableGlyph::Quiet));
            QCOMPARE(onPan.at(0).label, QStringLiteral("W2QQQ"));
            QVERIFY(onPan.at(0).keyed.isEmpty());
            QCOMPARE(onPan.at(1).state, static_cast<int>(RoundtableGlyph::Doubled));
            QCOMPARE(onPan.at(1).mergedHz, QList<qint64>{7203040});
        }
        const QString captures = qEnvironmentVariable("NEREUS_ROUNDTABLE_CARD_CAPTURES");
        if (!captures.isEmpty()) {
            Pan pan;
            pan.widget.setRoundtableTags(roundtableTagsFor(&rig.model, {}), RoundtableTagDisplay::Live);
            const QImage shot = pan.draw();
            const QRect tagsRow = pan.widget.roundtableTagSpectrumRectForTest();
            QVERIFY(shot.copy(QRect(380, tagsRow.bottom() - 110, 460, 120)).save(captures + QStringLiteral("/viewed-quiet-next-to-double.png")));
        }
        // A talker next door is the same: the card names them, the tag stays quiet.
        rig.view(7200000, {cardTurn(6, "talking", 7203000, "lsb-7203000", 100, {a})}, {}, {q, a});
        QCOMPARE(rig.card(7200000).kind, RoundtableCardModel::Kind::Talking);
        const QList<RoundtableTag> talking = roundtableTagsFor(&rig.model, {}, context);
        QCOMPARE(talking.first().state, static_cast<int>(RoundtableGlyph::Quiet));
        // Q's own room doubling is its own news.
        const QJsonObject k = cardPerson("lsb-7200000", "k", "N3XYZ", 7200020, true);
        rig.view(7200000, {cardTurn(7, "doubled", 7200000, "lsb-7200000", 100, {k, q})}, {}, {k, q});
        QCOMPARE(roundtableTagsFor(&rig.model, {}, context).first().state, static_cast<int>(RoundtableGlyph::Doubled));
    }

    // D3 before the stream arrives: the chosen keyed station's room is doubled, so
    // the card says Double with its names from the start, and does not flip. Tags
    // stay under the source-room rule.
    void aPendingCardNextToADoubleSaysDoubleFromTheStart()
    {
        CardRig rig;
        const QJsonObject q = cardPerson("lsb-7200000", "q", "W2QQQ", 7200000, false);
        const QJsonObject a = cardPerson("lsb-7203000", "a", "W1AW", 7203000, true);
        const QJsonObject b = cardPerson("lsb-7203000", "b", "K4ABC", 7203040, true);
        const QJsonObject quietRoom{{"id", "lsb-7200000"}, {"family", "lsb"}, {"state", "quiet"}, {"stations", QJsonArray{q}}};
        rig.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "doubled"},
            {"talkers", QJsonArray{"W1AW", "K4ABC"}}, {"stations", QJsonArray{a, b}}}});
        QVERIFY(rig.model.readAt(7200000, QStringLiteral("lsb")));
        const RoundtableCardModel pending = rig.card(7200000);
        QCOMPARE(pending.kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(pending.keyed, (QStringList{QStringLiteral("W1AW"), QStringLiteral("K4ABC")}));
        const QString captures = qEnvironmentVariable("NEREUS_ROUNDTABLE_CARD_CAPTURES");
        if (!captures.isEmpty()) {
            Pan pan;
            pan.widget.setUpdatesEnabled(true);
            RoundtableFrequencyPeek peek(&pan.widget);
            peek.setSlice(7188600, QStringLiteral("lsb"), QStringLiteral("A"));
            peek.showFor(7200000, QStringLiteral("lsb"), QPoint(40, 40));
            peek.refresh(pending, std::nullopt, false);
            QVERIFY(peek.grab().save(captures + QStringLiteral("/pending-view-next-to-double.png")));
        }
        const QList<RoundtableTag> tags = roundtableTagsFor(&rig.model, {});
        QCOMPARE(tags.first().state, static_cast<int>(RoundtableGlyph::Quiet));
        // One left of it: still that double, naming who is on it.
        rig.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "doubled"},
            {"talkers", QJsonArray{"W1AW", "GONE"}}, {"stations", QJsonArray{a}}}});
        QCOMPARE(rig.card(7200000).kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(rig.card(7200000).keyed, QStringList{QStringLiteral("W1AW")});
        // A room that is not doubled: one talker, named.
        rig.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "talking"},
            {"talkers", QJsonArray{"W1AW"}}, {"stations", QJsonArray{a}}}});
        QCOMPARE(rig.card(7200000).kind, RoundtableCardModel::Kind::Talking);
        QCOMPARE(rig.card(7200000).keyed, QStringList{QStringLiteral("W1AW")});
        // Two keyed in one source room are a double before its summary says so (D1).
        rig.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "talking"},
            {"talkers", QJsonArray{"W1AW"}}, {"stations", QJsonArray{a, b}}}});
        QCOMPARE(rig.card(7200000).kind, RoundtableCardModel::Kind::Double);
        // Two keyed merely near each other, in different rooms, are not.
        QJsonObject elsewhere = b;
        elsewhere.insert("sourceRoom", "lsb-7204000");
        rig.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "talking"},
                                 {"talkers", QJsonArray{"W1AW"}}, {"stations", QJsonArray{a}}},
                             QJsonObject{{"id", "lsb-7204000"}, {"family", "lsb"}, {"state", "talking"},
                                 {"talkers", QJsonArray{"K4ABC"}}, {"stations", QJsonArray{elsewhere}}}});
        QCOMPARE(rig.card(7200000).kind, RoundtableCardModel::Kind::Talking);
    }

    // The flag reads the same rule as the card before the stream arrives: it says
    // Double from the start, viewed or not, and does not flip when the stream comes.
    void aPendingFlagNextToADoubleSaysDoubleFromTheStart()
    {
        CardRig rig;
        const QJsonObject q = cardPerson("lsb-7200000", "q", "W2QQQ", 7200000, false);
        const QJsonObject a = cardPerson("lsb-7203000", "a", "W1AW", 7203000, true);
        const QJsonObject b = cardPerson("lsb-7203000", "b", "K4ABC", 7203040, true);
        const QJsonObject quietRoom{{"id", "lsb-7200000"}, {"family", "lsb"}, {"state", "quiet"}, {"stations", QJsonArray{q}}};
        const auto flag = [&rig] { return roundtableFlagLineFor(&rig.model, 0, 7200000, QStringLiteral("lsb")); };
        const QString doubleText = QStringLiteral("Roundtable \u00b7 3 \u00b7 Double");
        rig.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "doubled"},
            {"talkers", QJsonArray{"W1AW", "K4ABC"}}, {"stations", QJsonArray{a, b}}}});
        // Nobody viewing, then a read with no stream yet.
        QCOMPARE(flag().text, doubleText);
        QCOMPARE(flag().glyph, static_cast<int>(RoundtableGlyph::Doubled));
        QVERIFY(rig.model.readAt(7200000, QStringLiteral("lsb")));
        QCOMPARE(flag().text, doubleText);
        // The tags stay under the source-room rule.
        QCOMPARE(roundtableTagsFor(&rig.model, {}).first().state, static_cast<int>(RoundtableGlyph::Quiet));
        // The stream arrives: the same word.
        rig.view(7200000, {cardTurn(5, "doubled", 7203000, "lsb-7203000", 100, {a, b})}, {}, {q, a, b});
        QCOMPARE(flag().text, doubleText);

        // A stream that says nobody is on the air is not a double from the directory.
        rig.view(7200000, {}, {}, {q});
        QVERIFY(flag().glyph != static_cast<int>(RoundtableGlyph::Doubled));
        CardRig next;
        const auto nextFlag = [&next] { return roundtableFlagLineFor(&next.model, 0, 7200000, QStringLiteral("lsb")); };
        // One left of it: still that double.
        next.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "doubled"},
            {"talkers", QJsonArray{"W1AW", "GONE"}}, {"stations", QJsonArray{a}}}});
        QCOMPARE(nextFlag().glyph, static_cast<int>(RoundtableGlyph::Doubled));
        // Two keyed in one source room are a double before its summary says so.
        next.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "talking"},
            {"talkers", QJsonArray{"W1AW"}}, {"stations", QJsonArray{a, b}}}});
        QCOMPARE(nextFlag().glyph, static_cast<int>(RoundtableGlyph::Doubled));
        // A room that is not doubled: one talker, named.
        next.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "talking"},
            {"talkers", QJsonArray{"W1AW"}}, {"stations", QJsonArray{a}}}});
        QCOMPARE(nextFlag().glyph, static_cast<int>(RoundtableGlyph::Talking));
        QCOMPARE(nextFlag().text, QStringLiteral("Roundtable \u00b7 2 \u00b7 W1AW"));
        // Two keyed merely near each other, in different rooms, are not a double.
        QJsonObject elsewhere = b;
        elsewhere.insert("sourceRoom", "lsb-7204000");
        next.rooms(QJsonArray{quietRoom, QJsonObject{{"id", "lsb-7203000"}, {"family", "lsb"}, {"state", "talking"},
                                  {"talkers", QJsonArray{"W1AW"}}, {"stations", QJsonArray{a}}},
                              QJsonObject{{"id", "lsb-7204000"}, {"family", "lsb"}, {"state", "talking"},
                                  {"talkers", QJsonArray{"K4ABC"}}, {"stations", QJsonArray{elsewhere}}}});
        QCOMPARE(nextFlag().glyph, static_cast<int>(RoundtableGlyph::Talking));
    }

    // One callsign on two slices doubling itself keeps both names with their
    // numbers: a summary's talkers are per endpoint, and so are its names.
    void aDoubledSummaryNamesEachEndpointOfOneCallsign()
    {
        CardRig rig;
        QJsonObject second = cardPerson("lsb-7184000", "k2", "K4ABC", 7185000, false);
        second.insert("nth", 2);
        const QJsonObject first = cardPerson("lsb-7184000", "k1", "K4ABC", 7185000, false);
        rig.rooms(QJsonArray{QJsonObject{{"id", "lsb-7184000"}, {"family", "lsb"}, {"state", "doubled"},
            {"talkers", QJsonArray{"K4ABC", "K4ABC", "N0GONE"}}, {"stations", QJsonArray{second, first}}}});
        const QStringList both{QStringLiteral("K4ABC (2)"), QStringLiteral("K4ABC")};
        QCOMPARE(roundtableTagsFor(&rig.model, {}).first().keyed, both);
        QCOMPARE(rig.card(7185000).keyed, both);
        // One talker of the two: only as many names as the summary has endpoints.
        rig.rooms(QJsonArray{QJsonObject{{"id", "lsb-7184000"}, {"family", "lsb"}, {"state", "doubled"},
            {"talkers", QJsonArray{"K4ABC", "N0GONE"}}, {"stations", QJsonArray{second, first}}}});
        QCOMPARE(roundtableTagsFor(&rig.model, {}).first().keyed, QStringList{QStringLiteral("K4ABC (2)")});
        // Keyed, each with its number.
        QJsonObject keyedSecond = second;
        keyedSecond.insert("keyed", true);
        QJsonObject keyedFirst = first;
        keyedFirst.insert("keyed", true);
        rig.rooms(QJsonArray{QJsonObject{{"id", "lsb-7184000"}, {"family", "lsb"}, {"state", "doubled"},
            {"talkers", QJsonArray{"K4ABC", "K4ABC"}}, {"stations", QJsonArray{keyedSecond, keyedFirst}}}});
        QCOMPARE(rig.card(7185000).keyed, both);
    }

    // Ruling I4a and D5: one participant left, the turn is still doubled, and only
    // who is on it now is named, with their slice number. Nobody keyed yet: only
    // those the room still lists, never one who has gone.
    void aDoubleWithOneLeftIsStillADoubleAndNamesNobodyGone()
    {
        CardRig rig;
        QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        a.insert("nth", 2);
        const auto setRoom = [&rig](const QJsonObject& first) {
            rig.rooms(QJsonArray{QJsonObject{{"id", "lsb-7184000"}, {"family", "lsb"}, {"state", "doubled"},
                {"talkers", QJsonArray{"W1AW", "N0GONE"}},
                {"stations", QJsonArray{first, cardPerson("lsb-7184000", "q", "W2QQQ", 7185000, false)}}}});
        };
        setRoom(a);
        const QStringList oneLeft{QStringLiteral("W1AW (2)")};
        const QList<RoundtableTag> tags = roundtableTagsFor(&rig.model, {});
        QCOMPARE(tags.size(), 1);
        QCOMPARE(tags.first().state, static_cast<int>(RoundtableGlyph::Doubled));
        QCOMPARE(tags.first().keyed, oneLeft);
        QCOMPARE(rig.card(7185000).kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(rig.card(7185000).keyed, oneLeft);
        QVERIFY(roundtableFlagLineFor(&rig.model, 0, 7185000, QStringLiteral("lsb")).text.endsWith(QStringLiteral("Double")));

        // Nobody keyed in the directory: the summary's name, as the directory has it.
        a.insert("keyed", false);
        setRoom(a);
        QCOMPARE(roundtableTagsFor(&rig.model, {}).first().keyed, oneLeft);
        QCOMPARE(rig.card(7185000).keyed, oneLeft);
    }

    // Ruling I4b: tags follow the source room, so a listener's frequency in the
    // doubled room joins the one merged Double (D20).
    void aListenersFrequencyInTheDoubledRoomMergesIntoItsDouble()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        const QJsonObject b = cardPerson("lsb-7184000", "b", "K4ABC", 7185040, true);
        const QJsonObject q = cardPerson("lsb-7184000", "q", "W2QQQ", 7185500, false);
        rig.rooms(QJsonArray{QJsonObject{{"id", "lsb-7184000"}, {"family", "lsb"}, {"state", "doubled"},
            {"talkers", QJsonArray{"W1AW", "K4ABC"}}, {"stations", QJsonArray{a, b, q}}}});
        const QList<RoundtableTag> onPan = roundtableMergeDoubleTags(roundtableTagsFor(&rig.model, {}));
        QCOMPARE(onPan.size(), 1);
        QCOMPARE(onPan.first().state, static_cast<int>(RoundtableGlyph::Doubled));
        QCOMPARE(onPan.first().mergedHz, (QList<qint64>{7185040, 7185500}));
    }

    // Clicking a red tag reads it; the empty view until the stream arrives knows
    // nothing, so the tag stays what the directory says.
    void aTagKeepsItsDoubleWhileItsReadIsPending()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        const QJsonObject b = cardPerson("lsb-7184000", "b", "K4ABC", 7185040, true);
        rig.directory({a, b});
        const QString context = RoundtableModel::frequencyContextId(7185000, QStringLiteral("lsb"));
        QVERIFY(rig.model.readAt(7185000, QStringLiteral("lsb")));
        QCOMPARE(rig.model.readingRoomId(), context);
        const QJsonObject pending = rig.model.room(context);
        QVERIFY(!pending.isEmpty());
        QVERIFY(pending.value("members").toArray().isEmpty());
        for (const RoundtableTag& tag : roundtableTagsFor(&rig.model, {})) {
            QCOMPARE(tag.state, static_cast<int>(RoundtableGlyph::Doubled));
            QCOMPARE(tag.keyed.size(), 2);
        }
        QVERIFY(roundtableIsDouble(pending, rig.model.rooms(), 7185000, QStringLiteral("lsb")));
        QVERIFY(roundtableFlagLineFor(&rig.model, 0, 7185000, QStringLiteral("lsb")).text.endsWith(QStringLiteral("Double")));
        // A stream that knows better wins: members and no open turn.
        rig.view(7185000, {}, {}, {a});
        QVERIFY(!roundtableIsDouble(rig.model.room(context), rig.model.rooms(), 7185000, QStringLiteral("lsb")));
    }

    // D15: amber on any frequency the summary calls unknown, not only the active one.
    void anUnknownRoomDrawsAmberOnEveryFrequency()
    {
        CardRig rig;
        const QJsonObject q = cardPerson("lsb-7184000", "q", "W2QQQ", 7185000, false);
        rig.rooms(QJsonArray{QJsonObject{{"id", "lsb-7184000"}, {"family", "lsb"}, {"state", "unknown"},
            {"talkers", QJsonArray{}}, {"stations", QJsonArray{q}}}});
        const QList<RoundtableTag> tags = roundtableTagsFor(&rig.model, {},
            RoundtableModel::frequencyContextId(7100000, QStringLiteral("lsb")));
        QCOMPARE(tags.size(), 1);
        QVERIFY(!tags.first().active);
        QCOMPARE(tags.first().state, static_cast<int>(RoundtableGlyph::Unknown));
        QCOMPARE(tags.first().label, QStringLiteral("Unknown"));
        QCOMPARE(rig.card(7185000).kind, RoundtableCardModel::Kind::Unknown);
        const RoundtableFlagLine flag = roundtableFlagLineFor(&rig.model, 0, 7185000, QStringLiteral("lsb"));
        QCOMPARE(flag.glyph, static_cast<int>(RoundtableGlyph::Unknown));
        // Someone keyed here names themselves: they outrank a summary's unknown.
        const QJsonObject keyed = cardPerson("lsb-7184000", "k", "K4ABC", 7186000, true);
        rig.rooms(QJsonArray{QJsonObject{{"id", "lsb-7184000"}, {"family", "lsb"}, {"state", "unknown"},
            {"talkers", QJsonArray{}}, {"stations", QJsonArray{keyed}}}});
        QCOMPARE(roundtableTagsFor(&rig.model, {}).first().state, static_cast<int>(RoundtableGlyph::Talking));
    }

    // The wire marks you on a member; its talkers never carry it (rooms.py).
    void whichOfADoubleIsYouComesFromTheMembers()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        const QJsonObject b = cardPerson("lsb-7184000", "b", "KG4VCF", 7185040, true);
        QVERIFY(!a.contains("you") && !b.contains("you"));
        QJsonObject bMember = b;
        bMember.insert("you", true);
        rig.view(7185000, {cardTurn(5, "doubled", 7184000, "lsb-7184000", 100, {a, b})}, {}, {a, bMember});
        const RoundtableCardModel doubled = rig.card(7185000);
        QCOMPARE(doubled.kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(doubled.keyedYou, QStringLiteral("KG4VCF"));
        // The same ref in another source room is someone else.
        QJsonObject stranger = b;
        stranger.insert("sourceRoom", "lsb-7188000");
        stranger.insert("you", true);
        rig.view(7185000, {cardTurn(5, "doubled", 7184000, "lsb-7184000", 100, {a, b})}, {}, {a, b, stranger});
        QVERIFY(rig.card(7185000).keyedYou.isEmpty());
    }

    // The card's Tune goes where the live turn is: a talker's own frequency, a
    // double's participant nearest the dial.
    void theCardCarriesTheLiveTurnsExactFrequency()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185040, true);
        const QJsonObject b = cardPerson("lsb-7184000", "b", "K4ABC", 7185010, true);
        const QJsonObject c = cardPerson("lsb-7184000", "c", "N3XYZ", 7184900, true);
        rig.view(7185000, {cardTurn(5, "talking", 7185040, "lsb-7184000", 100, {a})}, {}, {a});
        QCOMPARE(rig.card(7185000).liveHz, 7185040);
        rig.view(7185000, {cardTurn(6, "doubled", 7184000, "lsb-7184000", 100, {a, b, c})}, {}, {a, b, c});
        QCOMPARE(rig.card(7185000).liveHz, 7185010);
        // No live turn: nowhere to go.
        QJsonObject ended = cardTurn(7, "talking", 7185040, "lsb-7184000", 100, {a});
        ended.insert("end", 200);
        rig.view(7185000, {}, {ended}, {a});
        QCOMPARE(rig.card(7185000).liveHz, 0);
        rig.directory({a, b});
        rig.view(7185010, {}, {}, {});
        QCOMPARE(rig.card(7185010).liveHz, 0);
    }

    // The real click: card, menu, Tune, on the exact frequency in the card's family.
    // D22, D36: your own frequency's Tune is greyed whichever turn is live; a tag's
    // Tune goes to that tag's own frequency.
    void aMenusTuneGoesToItsOwnFrequency()
    {
        Pan pan;
        pan.flag->setActiveSlice(true);
        pan.widget.setRoundtableActiveSliceName(QStringLiteral("A"));
        const QString room = RoundtableModel::frequencyContextId(7180000, QStringLiteral("lsb"));
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::roundtableRoomClicked);
        QMenu menu;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, menu);
        QAction* tune = menu.findChild<QAction*>(QStringLiteral("roundtableMenuTune"));
        QVERIFY(tune != nullptr);
        QVERIFY(tune->isEnabled());
        const QString captures = qEnvironmentVariable("NEREUS_ROUNDTABLE_CARD_CAPTURES");
        if (!captures.isEmpty()) {
            QVERIFY(menu.grab().save(captures + QStringLiteral("/short-menu.png")));
        }
        tune->trigger();
        QCOMPARE(tuned.size(), 1);
        QCOMPARE(tuned.first().first().toString(), room);

        // Keyed: greyed, on a pan that never had the overlay, and a trigger does nothing.
        pan.widget.setDisplayKeyed(true, 0, 0);
        QMenu keyed;
        pan.widget.buildRoundtableCardMenu(room, 7180000, false, keyed);
        tune = keyed.findChild<QAction*>(QStringLiteral("roundtableMenuTune"));
        QVERIFY(!tune->isEnabled());
        tune->trigger();
        QCOMPARE(tuned.size(), 1);
        pan.widget.setDisplayKeyed(false, 0, 0);

        // Your own: greyed.
        QMenu own;
        pan.widget.buildRoundtableCardMenu(room, 7180000, true, own);
        tune = own.findChild<QAction*>(QStringLiteral("roundtableMenuTune"));
        QVERIFY(!tune->isEnabled());
    }

    // Two pans: only the transmitting one has the overlay, and both are keyed.
    void everyRoundtableTuneWaitsOnEveryPanWhileKeyed()
    {
        Pan transmitting;
        Pan other;
        const QString id = RoundtableModel::frequencyContextId(7180000, QStringLiteral("lsb"));
        RoundtableTag frequency = tagAt(id, 7180000);
        frequency.stations = {QJsonObject{{"callsign", "W1AW"}}};
        RoundtableTag legacy = tagAt(QStringLiteral("lsb-7150000"), 7150000);
        RoundtableTag cochannel = tagAt(QStringLiteral("lsb-7120000"), 7120000);
        cochannel.stations = {QJsonObject{{"callsign", "K4ABC"}}};
        for (Pan* pan : {&transmitting, &other}) {
            pan->widget.setRoundtableTags({frequency, legacy, cochannel}, RoundtableTagDisplay::Live);
            pan->draw();
            pan->widget.setDisplayKeyed(true, 0, 0);
        }
        transmitting.widget.setMoxOverlay(true);
        QVERIFY(!other.widget.isMoxOverlayActive());
        for (Pan* pan : {&transmitting, &other}) {
            SpectrumWidget& widget = pan->widget;
            QSignalSpy tuned(&widget, &SpectrumWidget::roundtableRoomClicked);

            QMenu card;
            widget.buildRoundtableCardMenu(id, 7180000, false, card);
            QAction* tune = card.findChild<QAction*>(QStringLiteral("roundtableMenuTune"));
            QVERIFY(!tune->isEnabled());
            tune->trigger();

            QMenu single;
            widget.buildRoundtableFoldMenu({id}, single);
            for (QAction* action : single.actions()) {
                if (action->text().startsWith(QLatin1String("Tune"))) {
                    QVERIFY(!action->isEnabled());
                    action->trigger();
                }
            }
            QMenu folded;
            widget.buildRoundtableFoldMenu({legacy.roomId, cochannel.roomId}, folded);
            for (QAction* action : folded.actions()) {
                action->trigger();
                if (action->menu() != nullptr) {
                    for (QAction* inner : action->menu()->actions()) {
                        if (inner->text().startsWith(QLatin1String("Tune"))) {
                            QVERIFY(!inner->isEnabled());
                            inner->trigger();
                        }
                    }
                }
            }
            const auto* hit = hitFor(widget, legacy.roomId);
            QVERIFY(hit != nullptr);
            QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, hit->rect.center());
            QCOMPARE(tuned.size(), 0);
        }
        // Back on the air's other side, the other pan tunes again.
        other.widget.setDisplayKeyed(false, 0, 0);
        QSignalSpy tuned(&other.widget, &SpectrumWidget::roundtableRoomClicked);
        other.draw();
        QTest::mouseClick(&other.widget, Qt::LeftButton, Qt::NoModifier,
                          hitFor(other.widget, QStringLiteral("lsb-7150000"))->rect.center());
        QCOMPARE(tuned.size(), 1);
    }

    // A pan inside a window has a native window of its own, which cannot be a
    // popup's transient parent: the fold's menu sits on the top level, as the card's.
    void aFoldsMenuIsParentedToTheWindow()
    {
        QWidget top;
        top.resize(1300, 800);
        Pan pan;
        pan.widget.setParent(&top);
        pan.widget.move(0, 0);
        top.show();
        pan.widget.show();
        pan.widget.setRoundtableTags(fiveClose(), RoundtableTagDisplay::Live);
        pan.draw();
        const QList<SpectrumWidget::RoundtableTagHit>& hits = pan.widget.roundtableTagHitsForTest();
        const auto it = std::find_if(hits.cbegin(), hits.cend(),
                                     [](const SpectrumWidget::RoundtableTagHit& hit) { return hit.folded; });
        QVERIFY(it != hits.cend());
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, it->rect.center());
        auto* menu = top.findChild<QMenu*>(QStringLiteral("roundtableFoldMenu"), Qt::FindDirectChildrenOnly);
        QVERIFY(menu != nullptr);
        menu->close();
        pan.widget.setParent(nullptr);
    }

    // D14: values redraw in place. Only structure places the card again.
    void aValueOnlyChangeDoesNotPlaceTheCardAgain()
    {
        Pan pan;
        const QString room = RoundtableModel::frequencyContextId(7200000, QStringLiteral("lsb"));
        RoundtableCardModel card = talkingCard(2);
        card.clockMs = 1000;
        pan.widget.setRoundtableTags({}, RoundtableTagDisplay::Live);
        pan.widget.setRoundtableCard(card, room);
        pan.draw();
        const int placements = pan.widget.roundtableCardPlacementsForTest();
        QVERIFY(placements >= 1);
        const QRect rect = pan.widget.roundtableTagHitsForTest().first().rect;

        const QImage before = pan.draw();
        card.clockMs += 61000;
        card.rows[0].dbm -= 7.0;
        card.rows[0].ageMs += 200;
        pan.widget.setRoundtableCard(card, room);
        QVERIFY(pan.draw() != before);
        QCOMPARE(pan.widget.roundtableCardPlacementsForTest(), placements);
        QCOMPARE(pan.widget.roundtableTagHitsForTest().first().rect, rect);

        // A new row is structure.
        card.rows.append(card.rows.first());
        card.rows.last().call = QStringLiteral("N3NEW");
        pan.widget.setRoundtableCard(card, room);
        pan.draw();
        QCOMPARE(pan.widget.roundtableCardPlacementsForTest(), placements + 1);
    }

    void theTurnShownIsExactThenNearestThenLatest()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185040, true);
        const QJsonObject b = cardPerson("lsb-7184000", "b", "K4ABC", 7186000, true);
        const QJsonObject c = cardPerson("lsb-7188000", "c", "N3XYZ", 7188000, true);
        const QJsonObject ta = cardTurn(1, "talking", 7185040, "lsb-7184000", 100, {a});
        const QJsonObject tb = cardTurn(2, "talking", 7186000, "lsb-7184000", 200, {b});
        const QJsonObject tc = cardTurn(3, "talking", 7188000, "lsb-7188000", 300, {c});
        const auto shown = [&rig](qint64 hz) { return rig.card(hz).keyed.join(QLatin1Char(',')); };
        // 40 Hz off the dial: found, and the view says so too.
        rig.view(7185000, {ta}, {}, {a});
        QCOMPARE(shown(7185000), QStringLiteral("W1AW"));
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185000"), 7185000);
        QCOMPARE(roundtableTagReportLines(tag, rig.model.room(tag.roomId), rig.now).first(), QStringLiteral("Talking: W1AW"));
        // 1 kHz beats 3 kHz, in either list order and whichever started last.
        rig.view(7185000, {tb, tc}, {}, {b, c});
        QCOMPARE(shown(7185000), QStringLiteral("K4ABC"));
        // An exact turn beats a nearer-started one.
        rig.view(7186000, {tb, ta}, {}, {a, b});
        QCOMPARE(shown(7186000), QStringLiteral("K4ABC"));
        // Equal distance: the later start.
        QJsonObject below = cardTurn(4, "talking", 7184000, "lsb-7184000", 900, {a});
        rig.view(7185000, {tb, below}, {}, {a, b});
        QCOMPARE(shown(7185000), QStringLiteral("W1AW"));
        below.insert("start", 50);
        rig.view(7185000, {below, tb}, {}, {a, b});
        QCOMPARE(shown(7185000), QStringLiteral("K4ABC"));
        // The window is inclusive.
        rig.view(7181040, {ta}, {}, {a});
        QCOMPARE(rig.card(7181040).kind, RoundtableCardModel::Kind::Talking);
        rig.view(7180039, {ta}, {}, {a});
        QCOMPARE(rig.card(7180039).kind, RoundtableCardModel::Kind::Quiet);
        rig.view(7180040, {ta}, {}, {a});
        QCOMPARE(rig.card(7180040).kind, RoundtableCardModel::Kind::Talking);
    }

    void afterADoubleTheNewTurnStartsEmptyAndSaysSo()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185040, true);
        const QJsonObject b = cardPerson("lsb-7184000", "b", "K4ABC", 7185000, false);
        const qint64 ended = rig.now - 3500;
        QJsonObject doubled = cardTurn(5, "doubled", 7184000, "lsb-7184000", ended - 9000, {a, b});
        doubled.insert("end", ended);
        const QJsonObject single = cardTurn(6, "talking", 7185040, "lsb-7184000", ended, {a});
        const QJsonObject rx = cardReader("lsb-7184000", "rx", "VK3ABC", 5, -61, ended - 100);
        rig.view(7185000, {single}, {doubled}, {a, b, rx});
        RoundtableCardModel card = rig.card(7185000);
        QCOMPARE(card.kind, RoundtableCardModel::Kind::Talking);
        QVERIFY(card.rows.isEmpty());
        QCOMPARE(card.note, QStringLiteral("Double ended 3s ago"));
        // A new turn's readings arrive one by one.
        const QJsonObject heard = cardReader("lsb-7184000", "rx", "VK3ABC", 6, -70, rig.now);
        rig.view(7185000, {single}, {doubled}, {a, b, heard});
        card = rig.card(7185000);
        QCOMPARE(card.rows.size(), 1);
        QCOMPARE(card.rows.first().dbm, -70.0);
        rig.now = ended + 7999;
        QCOMPARE(rig.card(7185000).note, QStringLiteral("Double ended 7s ago"));
        rig.now = ended + 8000;
        QVERIFY(rig.card(7185000).note.isEmpty());
    }

    void aLiveDoubleDropsTheDepartedAndAnEndedOneKeepsEveryName()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        const QJsonObject b = cardPerson("lsb-7184000", "b", "K4ABC", 7185000, true);
        const QJsonObject c = cardPerson("lsb-7184000", "c", "N3XYZ", 7185000, false);
        QJsonObject turn = cardTurn(5, "doubled", 7185000, "lsb-7184000", 100, {a, b, c});
        rig.view(7185000, {turn}, {}, {a, b, c});
        RoundtableCardModel card = rig.card(7185000);
        QCOMPARE(card.kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(card.keyed, (QStringList{QStringLiteral("W1AW"), QStringLiteral("K4ABC")}));
        turn.insert("end", rig.now - 1000);
        rig.view(7185000, {}, {turn}, {a, b, c});
        card = rig.card(7185000);
        QCOMPARE(card.kind, RoundtableCardModel::Kind::LastDouble);
        QCOMPARE(card.keyed, (QStringList{QStringLiteral("W1AW"), QStringLiteral("K4ABC"), QStringLiteral("N3XYZ")}));
        QCOMPARE(card.endedAgoMs, 1000);
    }

    void aLiveTalkerIsNeverCaptionedAsPast()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185040, true);
        QJsonObject old = cardTurn(4, "talking", 7185000, "lsb-7184000", rig.now - 60000, {a});
        old.insert("end", rig.now - 50000);
        const QJsonObject live = cardTurn(5, "talking", 7185040, "lsb-7184000", 100, {a});
        rig.view(7185000, {live}, {old}, {a});
        QCOMPARE(rig.card(7185000).kind, RoundtableCardModel::Kind::Talking);
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185000"), 7185000);
        const QStringList rows = roundtableTagReportLines(tag, rig.model.room(tag.roomId), rig.now);
        QVERIFY2(!rows.join(' ').contains(QStringLiteral("Last")), qPrintable(rows.join('\n')));
        // With no open turn, it is history.
        rig.view(7185000, {}, {old}, {a});
        QCOMPARE(rig.card(7185000).kind, RoundtableCardModel::Kind::LastTalker);
        QCOMPARE(roundtableTagReportLines(tag, rig.model.room(tag.roomId), rig.now).first(), QStringLiteral("Last talker: W1AW"));
    }

    void aDroppedLinkFreezesAndHiddenOnlyDims()
    {
        using LinkState = RoundtableModel::LinkState;
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        const QJsonObject rx = cardReader("lsb-7184000", "rx", "VK3ABC", 5, -61, rig.now - 1000);
        rig.view(7185000, {cardTurn(5, "talking", 7185000, "lsb-7184000", rig.now - 4000, {a})}, {}, {a, rx});
        RoundtablePeakTracker peaks;
        RoundtableCardModel card = rig.card(7185000, &peaks);
        QVERIFY(!card.frozen);
        QVERIFY(card.note.isEmpty());
        QVERIFY(card.rows.first().hasPeak);

        rig.state.applyStationValue("linkState", static_cast<int>(LinkState::Reconnecting));
        card = rig.card(7185000, &peaks);
        QVERIFY(card.frozen);
        QCOMPARE(card.kind, RoundtableCardModel::Kind::Talking);
        QCOMPARE(card.note, QStringLiteral("Reconnecting"));
        // No tick, and the ages keep counting.
        QVERIFY(!card.rows.first().hasPeak);
        QCOMPARE(card.rows.first().ageMs, 1000);
        rig.now += 5000;
        QCOMPARE(rig.card(7185000, &peaks).rows.first().ageMs, 6000);

        rig.state.applyStationValue("linkState", static_cast<int>(LinkState::Offline));
        card = rig.card(7185000, &peaks);
        QVERIFY(card.frozen);
        QCOMPARE(card.note, QStringLiteral("Offline"));
        // 1800000000000 is 08:00 UTC. The clock stops where the link dropped.
        const qint64 turnStart = rig.now - 9000;
        rig.view(7185000, {cardTurn(5, "talking", 7185000, "lsb-7184000", turnStart, {a})}, {}, {a, rx});
        rig.state.applyStationValue("linkSince", rig.now - 1000);
        card = rig.card(7185000, &peaks);
        QCOMPARE(card.note, QStringLiteral("Offline since 08:00 UTC"));
        QCOMPARE(card.clockMs, 8000);

        rig.state.applyStationValue("linkState", static_cast<int>(LinkState::Hidden));
        card = rig.card(7185000, &peaks);
        QVERIFY(!card.frozen);
        QCOMPARE(card.note, QStringLiteral("You are hidden"));
        QCOMPARE(roundtableTagDisplayFor(&rig.model), RoundtableTagDisplay::Dimmed);
    }

    void rowsArePresentListenersNeverTransmitters()
    {
        CardRig rig;
        QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        QJsonObject b = cardPerson("lsb-7184000", "b", "K4ABC", 7185000, true);
        a.insert("reading", cardReader("lsb-7184000", "a", "W1AW", 5, -50, rig.now).value("reading"));
        b.insert("reading", cardReader("lsb-7184000", "b", "K4ABC", 5, -50, rig.now).value("reading"));
        QJsonObject far = cardReader("lsb-7184000", "far", "VK3ABC", 5, -61, rig.now - 1000);
        far.insert("km", 15000.4); far.insert("bearingDeg", 95); far.insert("offsetHz", -40);
        QJsonObject near = cardReader("lsb-7184000", "near", "KG4VCF", 5, -70, rig.now - 1000);
        near.insert("km", 12); near.insert("bearingDeg", 200); near.insert("you", true);
        QJsonObject gone = cardReader("lsb-7184000", "gone", "N0GONE", 5, -80, rig.now - 1000);
        QJsonObject goneReading = gone.value("reading").toObject();
        goneReading.insert("present", false);
        gone.insert("reading", goneReading);
        QJsonObject stranger = cardReader("lsb-7188000", "far", "W0RONG", 5, -80, rig.now - 1000);
        QJsonObject otherTurn = cardReader("lsb-7184000", "other", "W0OLD", 4, -80, rig.now - 1000);

        rig.view(7185000, {cardTurn(5, "talking", 7185000, "lsb-7184000", 100, {a})}, {},
                 {a, far, near, gone, stranger, otherTurn});
        RoundtableCardModel card = rig.card(7185000);
        QCOMPARE(card.rows.size(), 2);
        QCOMPARE(card.rows.at(0).call, QStringLiteral("KG4VCF"));
        QVERIFY(card.rows.at(0).you);
        QCOMPARE(card.rows.at(0).km, std::optional<int>(12));
        QCOMPARE(card.rows.at(1).call, QStringLiteral("VK3ABC"));
        QCOMPARE(card.rows.at(1).km, std::optional<int>(15000));
        QCOMPARE(card.rows.at(1).bearingDeg, std::optional<int>(95));
        QCOMPARE(card.rows.at(1).offsetHz, -40);

        // Combined RF: the same rows, no path, and no transmitter.
        const QJsonObject doubled = cardTurn(5, "doubled", 7185000, "lsb-7184000", 100, {a, b});
        rig.view(7185000, {doubled}, {}, {a, b, far, near, gone, stranger, otherTurn});
        card = rig.card(7185000);
        QCOMPARE(card.kind, RoundtableCardModel::Kind::Double);
        QCOMPARE(card.rows.size(), 2);
        for (const RoundtableCardRow& row : card.rows) {
            QVERIFY(!row.km.has_value());
            QVERIFY(!row.bearingDeg.has_value());
        }
        // Still blank once it is history.
        QJsonObject ended = doubled;
        ended.insert("end", rig.now - 1000);
        rig.view(7185000, {}, {ended}, {a, b, far});
        card = rig.card(7185000);
        QCOMPARE(card.kind, RoundtableCardModel::Kind::LastDouble);
        QCOMPARE(card.rows.size(), 1);
        QVERIFY(!card.rows.first().km.has_value());
    }

    void anOldRowLosesItsPeak()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, true);
        const QJsonObject atLimit = cardReader("lsb-7184000", "limit", "K1AA", 5, -60, rig.now - 20000);
        const QJsonObject past = cardReader("lsb-7184000", "past", "K2BB", 5, -60, rig.now - 20001);
        const QJsonObject turn = cardTurn(5, "talking", 7185000, "lsb-7184000", 100, {a});
        rig.view(7185000, {turn}, {}, {a, atLimit, past});
        RoundtablePeakTracker peaks;
        RoundtableCardModel card = rig.card(7185000, &peaks);
        QCOMPARE(card.rows.size(), 2);
        QVERIFY(!card.rows.at(0).old);
        QVERIFY(card.rows.at(0).hasPeak);
        QCOMPARE(card.rows.at(0).peakDbm, -60.0);
        QVERIFY(card.rows.at(1).old);
        QVERIFY(!card.rows.at(1).hasPeak);
        // Without a tracker there is nothing to hold.
        QVERIFY(!rig.card(7185000).rows.at(0).hasPeak);
        // An ended turn holds no peak: it has stopped moving.
        QJsonObject ended = turn;
        ended.insert("end", rig.now - 1000);
        const QJsonObject fresh = cardReader("lsb-7184000", "limit", "K1AA", 5, -60, rig.now - 1000);
        rig.view(7185000, {}, {ended}, {a, fresh});
        QVERIFY(!rig.card(7185000, &peaks).rows.first().hasPeak);
    }

    void nobodyOnTheFrequencyIsQuietWithThePeopleListed()
    {
        CardRig rig;
        const QJsonObject a = cardPerson("lsb-7184000", "a", "W1AW", 7185000, false);
        QJsonObject second = a;
        second.insert("slice", 1);
        const QJsonObject elsewhere = cardPerson("lsb-7184000", "b", "K4ABC", 7185040, false);
        rig.directory({a, second, elsewhere});
        const RoundtableCardModel card = rig.card(7185000);
        QCOMPARE(card.kind, RoundtableCardModel::Kind::Quiet);
        QCOMPARE(card.hz, quint64(7185000));
        QCOMPARE(card.people, QStringList{QStringLiteral("W1AW")});
        QVERIFY(card.keyed.isEmpty());
        QCOMPARE(roundtableCardModelFor(nullptr, {}, {}, 7185000, nullptr).kind, RoundtableCardModel::Kind::Quiet);
    }

    void offlineGreysConnectingHides()
    {
        NullLink link;
        RoundtableState state;
        RoundtableModel model;
        QCOMPARE(roundtableTagDisplayFor(nullptr), RoundtableTagDisplay::Hidden);
        model.attachStation(&link, &state);
        using LinkState = RoundtableModel::LinkState;
        const QList<std::pair<LinkState, RoundtableTagDisplay>> cases{
            {LinkState::Off, RoundtableTagDisplay::Hidden},
            {LinkState::Connecting, RoundtableTagDisplay::Hidden},
            {LinkState::Live, RoundtableTagDisplay::Live},
            {LinkState::Reconnecting, RoundtableTagDisplay::Frozen},
            // You are hidden: everything as when live, dimmed.
            {LinkState::Hidden, RoundtableTagDisplay::Dimmed},
            {LinkState::Offline, RoundtableTagDisplay::Frozen}};
        for (const auto& [link_state, display] : cases) {
            state.applyStationValue("linkState", static_cast<int>(link_state));
            QCOMPARE(roundtableTagDisplayFor(&model), display);
        }
        model.detach();
    }

    void initTestCase()
    {
        // A pan's start-up lines say nothing about the tags.
        QLoggingCategory::setFilterRules(QStringLiteral("nereussdr.bandplan.info=false\n"
                                                        "nereussdr.rt_audio.info=false\n"
                                                        "nereus.memlock.info=false\n"
                                                        "nereus.spectrum.info=false\n"
                                                        "nereus.connection=false\n"
                                                        "nereus.freedv.rade.bridge.debug=false\n"
                                                        "qt.qpa.fonts.warning=false"));
        // Offscreen there is no GPU: the pan says so once per pan.
        g_previousHandler = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext& context, const QString& message) {
                if (message.startsWith(QLatin1String("QRhiWidget: QRhi is not supported"))) {
                    return;
                }
                g_previousHandler(type, context, message);
            });
    }

    void aPanDrawsItsTagsAlongTheFootClearOfFlagsAndBandPlan()
    {
        Pan pan;
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        pan.draw();
        const auto& hits = pan.widget.roundtableTagHitsForTest();
        // Mine, the quiet and doubled rooms, and one fold of five.
        QCOMPARE(hits.size(), 4);
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        const int bandH = pan.widget.bandPlanStripHeightForTest();
        QVERIFY(bandH > 0);
        const int bandTop = spec.bottom() + 1 - bandH;
        const QRegion flags = pan.widget.receiverFlagRegion();
        QVERIFY(!flags.isEmpty());
        for (const auto& hit : hits) {
            QCOMPARE(hit.rect.height(), 22);
            QVERIFY(hit.rect.left() >= spec.left());
            QVERIFY(hit.rect.right() <= spec.right());
            // Above the band plan, with room for the tick.
            QCOMPARE(hit.rect.bottom() + 1, bandTop - 8);
            QVERIFY2(!flags.intersects(hit.rect), "a tag is under a VFO flag");
        }
        const auto* fold = hitFor(pan.widget, QStringLiteral("lsb-7185100"));
        QVERIFY(fold != nullptr);
        QVERIFY(fold->folded);
        QCOMPARE(fold->roomIds.size(), 5);
        QVERIFY(!hitFor(pan.widget, QStringLiteral("lsb-7200000"))->folded);
    }

    void aShortPansTagsMoveOutFromUnderTheFlag()
    {
        // A pan this short has its flag over the tag row.
        Pan pan(300);
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        pan.draw();
        const QRegion flags = pan.widget.receiverFlagRegion();
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        const int bandTop = spec.bottom() + 1 - pan.widget.bandPlanStripHeightForTest();
        QVERIFY(flags.intersects(QRect(spec.left(), bandTop - 30, spec.width(), 22)));
        const auto& hits = pan.widget.roundtableTagHitsForTest();
        QVERIFY(hits.size() >= 3);
        for (const auto& hit : hits) {
            QVERIFY2(!flags.intersects(hit.rect), "a tag is under a VFO flag");
        }
        QVERIFY(hitFor(pan.widget, QStringLiteral("lsb-7200000")) != nullptr);
    }

    void modernFrequencyClickReadsDetailsWithoutTuningEvenDuringTx()
    {
        Pan pan;
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        tag.stations = {QJsonObject{{"callsign", "K4ABC"}}};
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        pan.draw();
        const auto* hit = hitFor(pan.widget, tag.roomId);
        QVERIFY(hit);
        const QPoint click = hit->rect.center();
        QSignalSpy read(&pan.widget, &SpectrumWidget::roundtableReadRequested);
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::roundtableRoomClicked);
        QSignalSpy plainTune(&pan.widget, &SpectrumWidget::frequencyClicked);
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, click);
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().first().toString(), tag.roomId);
        QCOMPARE(tuned.size(), 0);
        QCOMPARE(plainTune.size(), 0);
        pan.widget.setMoxOverlay(true);
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, click);
        QCOMPARE(read.size(), 2);
        QCOMPARE(tuned.size(), 0);
        QCOMPARE(plainTune.size(), 0);
        QMenu secondary;
        pan.widget.buildRoundtableFoldMenu({tag.roomId}, secondary);
        for (QAction* action : secondary.actions()) {
            if (action->text() == QLatin1String("Tune slice A here")) {
                QVERIFY(!action->isEnabled());
                action->trigger();
            }
        }
        QCOMPARE(tuned.size(), 0);
        RoundtableTag other = tagAt(QStringLiteral("frequency:lsb-7185500"), 7185500);
        pan.widget.setRoundtableTags({tag, other}, RoundtableTagDisplay::Live);
        QMenu folded;
        pan.widget.buildRoundtableFoldMenu({tag.roomId, other.roomId}, folded);
        int choices = 0;
        for (QAction* action : folded.actions()) {
            QVERIFY(action->menu() == nullptr);
            if (action->isEnabled() && !action->isSeparator()) {
                ++choices;
                action->trigger();
            }
        }
        QCOMPARE(choices, 2);
        QCOMPARE(read.size(), 4);
        QCOMPARE(tuned.size(), 0);
    }

    void aTagClickTunesAndAFoldListsItsRooms()
    {
        Pan pan;
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        pan.draw();
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::roundtableRoomClicked);
        QSignalSpy plainTune(&pan.widget, &SpectrumWidget::frequencyClicked);

        const auto* quietHit = hitFor(pan.widget, QStringLiteral("lsb-7150000"));
        QVERIFY(quietHit != nullptr);
        const QRect quiet = quietHit->rect;
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, quiet.center());
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toString(), QStringLiteral("lsb-7150000"));
        QCOMPARE(plainTune.count(), 0);

        // The fold's list: a heading, then the rooms by frequency.
        const auto* fold = hitFor(pan.widget, QStringLiteral("lsb-7185000"));
        QVERIFY(fold != nullptr);
        const QStringList foldRooms = fold->roomIds;
        QMenu menu;
        pan.widget.buildRoundtableFoldMenu(foldRooms, menu);
        QList<QAction*> rows;
        for (QAction* action : menu.actions()) {
            if (!action->isSeparator() && action->isEnabled()) {
                rows.append(action);
            }
        }
        QCOMPARE(menu.actions().first()->text(), QStringLiteral("5 frequencies here"));
        QVERIFY(!menu.actions().first()->isEnabled());
        QCOMPARE(rows.size(), 5);
        QCOMPARE(rows.first()->text(), QStringLiteral("7.185   K0ABC   3"));
        QCOMPARE(rows.at(1)->text(), QStringLiteral("7.18505   K1ABC   3"));
        for (QAction* row : menu.actions()) {
            if (row->isSeparator()) {
                continue;
            }
            QVERIFY2(OperatorWording::isPlain(row->text()), qPrintable(row->text()));
        }
        rows.at(2)->trigger();
        QCOMPARE(tuned.count(), 2);
        QCOMPARE(tuned.last().first().toString(), QStringLiteral("lsb-7185100"));

        // Keyed, a tag tunes nothing.
        pan.widget.setMoxOverlay(true);
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, quiet.center());
        rows.at(0)->trigger();
        QCOMPARE(tuned.count(), 2);
    }

    void aTagTunesTheActiveSliceToItsRoom()
    {
        // As MainWindow wires it: a pan's tag click to the applet's tune.
        NullLink link;
        RoundtableState state;
        RoundtableModel model;
        model.attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        model.applyRooms(QJsonObject{
            {QStringLiteral("rooms"),
             QJsonArray{summary(QStringLiteral("lsb-7150000"), 7150000, QStringLiteral("quiet"), QJsonArray{}, 2),
                        summary(QStringLiteral("usb-14212000"), 14212000, QStringLiteral("talking"),
                                QJsonArray{QStringLiteral("VE3XYZ")}, 12)}}});
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14212000.0);
        RoundtableApplet applet(&radio, &model, nullptr);

        Pan pan;
        QObject::connect(&pan.widget, &SpectrumWidget::roundtableRoomClicked, &applet,
                         &RoundtableApplet::tuneToRoom);
        pan.widget.setRoundtableTags(roundtableTagsFor(&model, QList<int>{0}), roundtableTagDisplayFor(&model));
        pan.draw();
        const auto* hit = hitFor(pan.widget, QStringLiteral("lsb-7150000"));
        QVERIFY(hit != nullptr);
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, hit->rect.center());
        QCOMPARE(std::llround(slice->frequency()), 7150000LL);
        QCOMPARE(slice->dspMode(), DSPMode::LSB);
        model.detach();
    }

    void offlineGreysThePansTagsAndConnectingHidesThem()
    {
        Pan pan;
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        const QImage live = pan.draw();
        const auto* doubledHit = hitFor(pan.widget, QStringLiteral("lsb-7250000"));
        QVERIFY(doubledHit != nullptr);
        const QRect doubled = doubledHit->rect;
        const QPoint edge(doubled.center().x(), doubled.top());
        QCOMPARE(QColor(live.pixel(edge)).name(), QStringLiteral("#c25a5c"));

        // A dropped link draws no live color (D7).
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Frozen);
        const QImage frozen = pan.draw();
        QCOMPARE(pan.widget.roundtableTagHitsForTest().size(), 4);
        QCOMPARE(QColor(frozen.pixel(edge)).name(), QStringLiteral("#606066"));

        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Hidden);
        pan.draw();
        QVERIFY(pan.widget.roundtableTagHitsForTest().isEmpty());
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::roundtableRoomClicked);
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, doubled.center());
        QCOMPARE(tuned.count(), 0);
    }

    void hiddenDimsThePansTagsAndKeepsTheirRooms()
    {
        Pan pan;
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        const QImage live = pan.draw();
        const QList<SpectrumWidget::RoundtableTagHit> liveHits = pan.widget.roundtableTagHitsForTest();
        const auto* doubledHit = hitFor(pan.widget, QStringLiteral("lsb-7250000"));
        QVERIFY(doubledHit != nullptr);
        const QRect doubled = doubledHit->rect;
        const QPoint edge(doubled.center().x(), doubled.top());
        QCOMPARE(QColor(live.pixel(edge)).name(), QStringLiteral("#c25a5c"));
        // The glyph's center: the tag's 1 px border, 6 px pad, 9 px glyph.
        const QPoint glyphCenter(doubled.left() + 1 + 6 + 4, doubled.center().y());

        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Dimmed);
        const QImage dimmed = pan.draw();
        // Every room still shows where it did, a click still tunes.
        QCOMPARE(pan.widget.roundtableTagHitsForTest().size(), liveHits.size());
        for (qsizetype i = 0; i < liveHits.size(); ++i) {
            QCOMPARE(pan.widget.roundtableTagHitsForTest().at(i).rect, liveHits.at(i).rect);
        }
        // The border keeps its red, dimmed: neither the live red nor the
        // last-known grey.
        const QColor border(dimmed.pixel(edge));
        QVERIFY2(border.name() != QStringLiteral("#c25a5c") && border.name() != QStringLiteral("#606066"),
                 qPrintable(border.name()));
        QVERIFY2(border.red() < 0xc2 * 0.7 && border.red() > border.green() + 20, qPrintable(border.name()));
        const QColor liveGlyph(live.pixel(glyphCenter));
        const QColor dimGlyph(dimmed.pixel(glyphCenter));
        QVERIFY2(dimGlyph.red() < liveGlyph.red() * 0.7, qPrintable(dimGlyph.name() + liveGlyph.name()));
        // The talker and count are drawn as when live.
        // Below the double's striped top edge, which dims with the marking.
        const QRect text(glyphCenter.x() + 10, doubled.top() + 3, doubled.right() - glyphCenter.x() - 18,
                         doubled.height() - 5);
        QCOMPARE(dimmed.copy(text), live.copy(text));

        QSignalSpy tuned(&pan.widget, &SpectrumWidget::roundtableRoomClicked);
        QTest::mouseClick(&pan.widget, Qt::LeftButton, Qt::NoModifier, doubled.center());
        QCOMPARE(tuned.count(), 1);
    }

#ifdef NEREUS_GPU_SPECTRUM
    // The GPU path draws the tags into its premultiplied overlay texture
    // and composites that onto the frame with the overlay pipeline's blend;
    // the dimmed (translucent) tags come out as the CPU path paints them.
    void hiddenTagsDimTheSameOnTheGpuPath()
    {
        Pan pan;
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Dimmed);
        const QRect specRect = pan.widget.roundtableTagSpectrumRectForTest();
        const QColor frame(0x30, 0x40, 0x50);

        QImage cpu(pan.widget.size(), QImage::Format_ARGB32_Premultiplied);
        cpu.fill(frame);
        {
            QPainter painter(&cpu);
            pan.widget.drawRoundtableTagsForTest(painter, specRect);
        }
        QImage overlay(pan.widget.size(), QImage::Format_RGBA8888_Premultiplied);
        overlay.fill(Qt::transparent);
        {
            QPainter painter(&overlay);
            pan.widget.drawRoundtableTagsForTest(painter, specRect);
        }
        const QList<SpectrumWidget::RoundtableTagHit> hits = pan.widget.roundtableTagHitsForTest();
        QVERIFY(!hits.isEmpty());

        const QRhiGraphicsPipeline::TargetBlend blend = SpectrumWidget::overlayTargetBlend();
        QVERIFY(blend.enable);
        bool known = true;
        const auto factor = [&known](QRhiGraphicsPipeline::BlendFactor f, double srcAlpha) {
            switch (f) {
            case QRhiGraphicsPipeline::Zero:
                return 0.0;
            case QRhiGraphicsPipeline::One:
                return 1.0;
            case QRhiGraphicsPipeline::SrcAlpha:
                return srcAlpha;
            case QRhiGraphicsPipeline::OneMinusSrcAlpha:
                return 1.0 - srcAlpha;
            default:
                known = false;
                return 0.0;
            }
        };
        // A fold's border stays its opaque grey; the dimmed tags' are translucent.
        int translucent = 0;
        for (const SpectrumWidget::RoundtableTagHit& hit : hits) {
            const QPoint edge(hit.rect.center().x(), hit.rect.top());
            // The texel as uploaded: premultiplied RGBA bytes.
            const uchar* texel = overlay.constScanLine(edge.y()) + 4 * edge.x();
            const double a = texel[3] / 255.0;
            if (a > 0.0 && a < 1.0) {
                ++translucent;
            }
            const double src[3] = {texel[0] / 255.0, texel[1] / 255.0, texel[2] / 255.0};
            const double dst[3] = {frame.redF(), frame.greenF(), frame.blueF()};
            const double fs = factor(blend.srcColor, a);
            const double fd = factor(blend.dstColor, a);
            QVERIFY(known);
            int gpu[3];
            for (int c = 0; c < 3; ++c) {
                gpu[c] = static_cast<int>(std::lround(std::clamp(src[c] * fs + dst[c] * fd, 0.0, 1.0) * 255.0));
            }
            const QColor want(cpu.pixel(edge));
            const QColor got(gpu[0], gpu[1], gpu[2]);
            QVERIFY2(std::abs(got.red() - want.red()) <= 1 && std::abs(got.green() - want.green()) <= 1
                         && std::abs(got.blue() - want.blue()) <= 1,
                     qPrintable(QStringLiteral("GPU %1 vs CPU %2").arg(got.name(), want.name())));
        }
        QVERIFY(translucent > 0);
    }
#endif

    void theDimmedMarkingIsTheLooksRole()
    {
        // The look off: the fixed state color, dimmed.
        QColor talking = roundtableGlyphColor(static_cast<int>(RoundtableGlyph::Talking));
        talking.setAlphaF(kRoundtableDimmedAlpha);
        QCOMPARE(roundtableDimmedColor(static_cast<int>(RoundtableGlyph::Talking)), talking);
        // The look on: the look's status and accent roles, dimmed.
        struct LookOff {
            ~LookOff() { WorkspacePreview::setEnabledForTest(false); }
        } restore;
        WorkspacePreview::setEnabledForTest(true);
        const QList<std::pair<RoundtableGlyph, QColor>> roles{
            {RoundtableGlyph::Talking,
             WorkspacePreview::statusColour(roundtableGlyphColor(static_cast<int>(RoundtableGlyph::Talking)), "ok")},
            {RoundtableGlyph::Unknown,
             WorkspacePreview::statusColour(roundtableGlyphColor(static_cast<int>(RoundtableGlyph::Unknown)), "warn")},
            {RoundtableGlyph::Doubled,
             WorkspacePreview::statusColour(roundtableGlyphColor(static_cast<int>(RoundtableGlyph::Doubled)),
                                            "danger")},
            {RoundtableGlyph::You,
             WorkspacePreview::roleColour(roundtableGlyphColor(static_cast<int>(RoundtableGlyph::You)), "accent")}};
        for (const auto& [g, role] : roles) {
            QColor expected = role;
            expected.setAlphaF(kRoundtableDimmedAlpha);
            QCOMPARE(roundtableDimmedColor(static_cast<int>(g)), expected);
        }
    }

    void tagsWithoutAChildSchedulePaintWithoutANewFft()
    {
        // Exercise the CPU renderer's absent-child condition in either build.
        // Count delivered paint events, without feeding spectrum data or
        // manually calling the drawing helper (which would mask this bug).
        class PaintCounter : public SpectrumWidget {
        public:
            int paints = 0;
        protected:
            void paintEvent(QPaintEvent*) override { ++paints; }
        } widget;
        if (widget.m_roundtableTagLayer) {
            widget.m_roundtableTagLayer->hide();
            widget.m_roundtableTagLayer = nullptr;
        }
        widget.resize(1200, 760);
        widget.show();
        QTRY_VERIFY(widget.paints > 0);
        const auto expectPaint = [&widget](auto change) {
            const int before = widget.paints;
            change();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
            QCoreApplication::processEvents();
            return widget.paints > before;
        };
        QVERIFY(expectPaint([&] {
            widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        }));
        auto tags = panTags();
        tags[0].label = QStringLiteral("Updated report");
        QVERIFY(expectPaint([&] {
            widget.setRoundtableTags(tags, RoundtableTagDisplay::Frozen);
        }));
        QVERIFY(expectPaint([&] {
            widget.setRoundtableTags(tags, RoundtableTagDisplay::Hidden);
        }));
        auto* toolbar = new SpectrumOverlayPanel(&widget);
        toolbar->show();
        QCoreApplication::processEvents();
        QVERIFY(expectPaint([&] { toolbar->hide(); }));
        QVERIFY(expectPaint([&] { toolbar->show(); }));
    }

    void clockGeometryFollowsBothSplitPathsWithoutWaitingForATick()
    {
        SpectrumWidget widget;
        widget.resize(1000, 700);
        widget.show();
        widget.setWfTimestampPosition(SpectrumWidget::TimestampPosition::Left);
        widget.setSpectrumFrac(0.7f);
        QCOMPARE(widget.m_wfTimestampLabel->y(), widget.waterfallRectForTest().top() + 2);
        widget.m_draggingDivider = true;
        QTest::mouseMove(&widget, QPoint(200, 180));
        widget.m_draggingDivider = false;
        QCOMPARE(widget.m_wfTimestampLabel->y(), widget.waterfallRectForTest().top() + 2);
    }

    void clockTicksStayBelowFlagsFlyoutsAndTheDisconnectBanner()
    {
        SpectrumWidget widget;
        widget.resize(1000, 700);
        widget.show();
        widget.setConnectionState(ConnectionState::Connected);
        widget.setWfTimestampPosition(SpectrumWidget::TimestampPosition::Left);
        auto* flag = widget.addVfoWidget(0);
        auto* toolbar = new SpectrumOverlayPanel(&widget);
        toolbar->show();
        for (QPushButton* button : toolbar->findChildren<QPushButton*>()) {
            if (button->text() == QLatin1String("Display")) { button->click(); break; }
        }
        auto* flyout = widget.findChild<QWidget*>(QStringLiteral("displayPill"));
        QVERIFY(flyout);
        QVERIFY(flyout->isVisibleTo(&widget));
        widget.setConnectionState(ConnectionState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(widget.m_wfTimestampTicker, "timeout", Qt::DirectConnection));
        // Qt stores widget siblings in actual stacking order; raise/lower
        // changes this order even for mouse-transparent clock labels.
        const QObjectList stack = widget.children();
        const int clock = stack.indexOf(widget.m_wfTimestampLabel);
        QVERIFY(clock < stack.indexOf(flag));
        QVERIFY(clock < stack.indexOf(flyout));
        QVERIFY(clock < stack.indexOf(widget.m_disconnectLabel));
    }

    void flagKeepoutChangesRefreshTagsWithoutInvalidatingTheStaticTexture()
    {
        Pan pan;
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        pan.widget.m_receiverFlagRegion = QRegion(QRect(-100, -100, 10, 10));
        pan.widget.clearOverlayStaticDirtyForTest();
        pan.widget.refreshReceiverFlagRegion();
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
        QVERIFY(!pan.widget.receiverFlagRegion().isEmpty());
    }

    void theGpuTagChildReceivesPaintsAfterAReportWithoutANewFft()
    {
#ifdef NEREUS_GPU_SPECTRUM
        class QuietSpectrum : public SpectrumWidget {
            void paintEvent(QPaintEvent*) override {}
        } widget;
        class Paints : public QObject {
        public:
            int count = 0;
            bool eventFilter(QObject*, QEvent* event) override {
                if (event->type() == QEvent::Paint) { ++count; }
                return false;
            }
        } paints;
        widget.resize(1000, 700);
        widget.setSampleRate(kPanSpanHz);
        widget.setFrequencyRange(kPanCenterHz, kPanSpanHz);
        QVERIFY(widget.m_roundtableTagLayer);
        widget.m_roundtableTagLayer->installEventFilter(&paints);
        widget.show();
        QTRY_VERIFY(paints.count > 0);
        const int before = paints.count;
        widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        QTRY_VERIFY(paints.count > before);
        QVERIFY(!widget.roundtableTagHitsForTest().isEmpty());
#endif
    }

    void clockTicksUpdateOnlyTheLabel()
    {
        Pan pan;
        pan.widget.setWfTimestampPosition(SpectrumWidget::TimestampPosition::Left);
        QVERIFY(pan.widget.m_wfTimestampLabel);
        QVERIFY(pan.widget.m_wfTimestampTicker);
        pan.widget.m_wfTimestampLabel->setText(QStringLiteral("old"));
        pan.widget.clearOverlayStaticDirtyForTest();
        QVERIFY(QMetaObject::invokeMethod(pan.widget.m_wfTimestampTicker, "timeout", Qt::DirectConnection));
        QVERIFY(pan.widget.m_wfTimestampLabel->text() != QStringLiteral("old"));
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
        QVERIFY(pan.widget.m_wfTimestampLabel->isVisibleTo(&pan.widget));
        pan.widget.setWfTimestampPosition(SpectrumWidget::TimestampPosition::None);
        QVERIFY(!pan.widget.m_wfTimestampLabel->isVisibleTo(&pan.widget));
    }

    void busyReportsDoNotInvalidateTheStaticSpectrumTexture()
    {
        Pan pan;
        auto tags = panTags();
        tags[0].active = true;
        tags[0].reportLines = {QStringLiteral("Talking: W1AW"), QStringLiteral("Heard by     dBm    Age"),
            QStringLiteral("K4ABC  -73  1s"), QStringLiteral("Open details")};
        int staticInvalidations = 0;
        for (int report = 0; report < 1000; ++report) {
            pan.widget.clearOverlayStaticDirtyForTest();
            tags[0].label = QStringLiteral("S%1").arg(report % 10);
            tags[0].reportLines[2] = QStringLiteral("K4ABC  %1  1s").arg(-73 - report % 10);
            pan.widget.setRoundtableTags(tags, RoundtableTagDisplay::Live);
            if (pan.widget.overlayStaticDirtyForTest()) { ++staticInvalidations; }
        }
        qInfo("1000 tag sample updates: static spectrum texture invalidations=%d", staticInvalidations);
        QCOMPARE(staticInvalidations, 0);
    }

    void newTagsLeaveBothGpuOverlaysClean()
    {
        Pan pan;
        pan.widget.clearOverlayStaticDirtyForTest();
        pan.widget.clearOverlayDynamicDirtyForTest();
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
#ifdef NEREUS_GPU_SPECTRUM
        // Tags paint in their own widget. Neither GPU overlay is rebuilt.
        QVERIFY(!pan.widget.overlayDynamicDirtyForTest());
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
        QVERIFY(!pan.widget.overlayDynamicDirtyForTest());
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
#endif
    }

    void collapsingThePansToolbarRedrawsTheTags()
    {
        // A short pan, so the open toolbar down the left side reaches the
        // tag row, and one tag under it.
        Pan pan(kShortPanHeight);
        auto* toolbar = new SpectrumOverlayPanel(&pan.widget);
        toolbar->move(4, 4);
        toolbar->show();
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        const double lowHz = kPanCenterHz - kPanSpanHz / 2.0;
        const qint64 hz = static_cast<qint64>(lowHz + 20.0 * kPanSpanHz / spec.width());
        const QString id = QStringLiteral("lsb-%1").arg(hz);
        pan.widget.setRoundtableTags({tagAt(id, hz)}, RoundtableTagDisplay::Live);
        pan.draw();
        const auto* open = hitFor(pan.widget, id);
        QVERIFY(open != nullptr);
        const QRect openRect = open->rect;
        QVERIFY2(toolbar->geometry().bottom() >= openRect.top(),
                 qPrintable(QStringLiteral("toolbar %1 px tall, tag row at %2")
                                .arg(toolbar->height()).arg(openRect.top())));
        QVERIFY(openRect.left() > toolbar->geometry().right());

        pan.widget.clearOverlayStaticDirtyForTest();
        pan.widget.clearOverlayDynamicDirtyForTest();
        QSignalSpy collapsed(toolbar, &SpectrumOverlayPanel::collapsed);
        auto* collapse = qobject_cast<QPushButton*>(toolbar->childAt(3, 3));
        QVERIFY(collapse != nullptr);
        collapse->click();
        QCOMPARE(collapsed.count(), 1);
        QCOMPARE(collapsed.first().first().toBool(), true);
        QVERIFY(toolbar->geometry().bottom() < openRect.top());
#ifdef NEREUS_GPU_SPECTRUM
        QVERIFY(!pan.widget.overlayDynamicDirtyForTest());
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
#endif
        pan.draw();
        const auto* shut = hitFor(pan.widget, id);
        QVERIFY(shut != nullptr);
        QVERIFY2(shut->rect.left() < openRect.left(),
                 qPrintable(QStringLiteral("tag at %1, was %2").arg(shut->rect.left()).arg(openRect.left())));

        // Opened again, the tag moves aside again.
        pan.widget.clearOverlayStaticDirtyForTest();
        pan.widget.clearOverlayDynamicDirtyForTest();
        collapse->click();
#ifdef NEREUS_GPU_SPECTRUM
        QVERIFY(!pan.widget.overlayDynamicDirtyForTest());
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
#endif
        pan.draw();
        QCOMPARE(hitFor(pan.widget, id)->rect, openRect);
    }

    void frequenciesAsTheFoldListShowsThem()
    {
        QCOMPARE(roundtableFrequencyText(7185000), QStringLiteral("7.185"));
        QCOMPARE(roundtableFrequencyText(7030500), QStringLiteral("7.0305"));
        QCOMPARE(roundtableFrequencyText(14074150), QStringLiteral("14.07415"));
        QCOMPARE(roundtableFrequencyText(14074151), QStringLiteral("14.074151"));
    }
};

QTEST_MAIN(TestRoundtableTagLayout)
#include "tst_roundtable_tag_layout.moc"
