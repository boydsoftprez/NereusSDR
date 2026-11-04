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
#include <cmath>
#include <cstdlib>

#include <QAction>
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

bool captureCard(RoundtableTag tag, const QStringList& rows, const QString& scene)
{
    const QString directory = qEnvironmentVariable("NEREUS_ROUNDTABLE_LIVE_CAPTURES");
    if (directory.isEmpty()) { return true; }
    tag.active = true;
    tag.reportLines = rows;
    if (!rows.isEmpty() && rows.first().startsWith(QLatin1String("Double"))) {
        tag.state = static_cast<int>(RoundtableGlyph::Doubled);
    }
    Pan pan;
    pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
    const QImage image = pan.draw();
    const auto* hit = hitFor(pan.widget, tag.roomId);
    return hit && image.copy(hit->rect.adjusted(-4, -4, 4, 4)).save(directory + QLatin1Char('/') + scene + QStringLiteral(".png"));
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
        QJsonObject own{{"sourceRoom", "lsb-7184000"}, {"ref", "same"}, {"you", true}, {"keyed", true},
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
            model.applyRoom(QJsonObject{{"id", QStringLiteral("lsb-%1").arg(hz)}, {"view", "frequency"},
                {"family", "lsb"}, {"hz", hz}, {"members", QJsonArray{own, other, reporterA, reporterB}},
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
        QVERIFY(captureCard(tag, rows, QStringLiteral("single-both")));
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
        QVERIFY(captureCard(tag, rows, QStringLiteral("double-three-both")));
        // One remains keyed: old combined samples do not become its report.
        QJsonObject single = turn;
        single.insert("id", 13); single.insert("kind", "talking"); single.insert("talkers", QJsonArray{a});
        turn.insert("end", 26000);
        view.insert("now", QJsonArray{single}); view.insert("earlier", QJsonArray{turn});
        rows = roundtableTagReportLines(tag, view, 27000);
        QVERIFY(!rows.join(' ').contains(QStringLiteral("Double")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("-61")));
        QVERIFY(rows.join(' ').contains(QStringLiteral("No matching reports")));
        QVERIFY(captureCard(tag, rows, QStringLiteral("single-awaiting-new-report")));
        view.insert("now", QJsonArray{});
        rows = roundtableTagReportLines(tag, view, 60000);
        QVERIFY(rows.first().startsWith(QStringLiteral("Last double")));
        QVERIFY(rows.join(' ').contains(QStringLiteral("stale")));
        QVERIFY(captureCard(tag, rows, QStringLiteral("last-double-stale")));
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
        QVERIFY(captureCard(tag, rows, QStringLiteral("double-two-third-now-receiving")));
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
        tag.reportLines = roundtableTagReportLines(tag, view, 1000);
        QFont font = WorkspacePreview::readableFont(QFont());
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(11));
        font.setWeight(QFont::Bold); font.setStretch(125);
        const QFontMetrics metrics(font);
        const QStringList narrow = roundtableCochannelRows(tag, 8, &metrics, 194);
        QVERIFY2(narrow.join(' ').contains(QStringLiteral("Double")), qPrintable(narrow.join(' ')));
        QVERIFY(narrow.join(' ').contains(QStringLiteral("unavailable")));
        turn.insert("end", 900);
        view.insert("now", QJsonArray{}); view.insert("earlier", QJsonArray{turn});
        QCOMPARE(roundtableTurnParticipants(view, turn).size(), 3);
        QVERIFY(roundtableTagReportLines(tag, view, 1000).first().contains(QStringLiteral("N3XYZ")));
    }

    void passiveCardsPrioritizeWholeReportsAndDiscloseOmissions()
    {
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        tag.active = true;
        tag.stations = QJsonArray{QJsonObject{{"callsign", "REDUNDANT"}}, QJsonObject{{"callsign", "PRESENCE"}}};
        tag.active = true;
        tag.reportLines = {QStringLiteral("Talking: W1AW"), QStringLiteral("Heard by     dBm    Age"),
            QStringLiteral("K1ABC  -73  2s"), QStringLiteral("K2ABC  -74  2s"),
            QStringLiteral("K3ABC  -75  2s"), QStringLiteral("K4ABC  -76  2s"), QStringLiteral("Open details")};
        const QStringList rows = roundtableCochannelRows(tag, 7);
        QCOMPARE(rows.size(), 7);
        QVERIFY(rows.contains(QStringLiteral("K3ABC  -75  2s")));
        QCOMPARE(rows.last(), QStringLiteral("+1 reports · details ›"));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("REDUNDANT")));
        const QStringList roomy = roundtableCochannelRows(tag, 20);
        QVERIFY(roomy.contains(QStringLiteral("K4ABC  -76  2s")));
        QCOMPARE(roomy.last(), QStringLiteral("Reports ›"));
        QFont font = WorkspacePreview::readableFont(QFont());
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(11));
        font.setWeight(QFont::Bold);
        const QFontMetrics metrics(font);
        tag.reportLines.insert(3, QStringLiteral("VK3ABC/PORTABLE (2)  S9 +12 / -61  25s stale"));
        const QStringList narrow = roundtableCochannelRows(tag, 20, &metrics, 194);
        QVERIFY(narrow.last().contains(QStringLiteral("+1 reports")));
        QVERIFY(!narrow.join(' ').contains(QStringLiteral("VK3ABC/PORTABLE")));
        for (const QString& row : narrow) { QVERIFY(metrics.horizontalAdvance(row) <= 194); }
        tag.reportLines = {QStringLiteral("Double (3): W1AW + K4ABC (2) + N3XYZ"),
            QStringLiteral("Combined RF · Heard by · S / dBm · Age"), QStringLiteral("K1ABC  -73  2s"), QStringLiteral("Open details")};
        const QStringList doubleRows = roundtableCochannelRows(tag, 12, &metrics, 194);
        QVERIFY(doubleRows.join(' ').contains(QStringLiteral("W1AW")));
        QVERIFY(doubleRows.join(' ').contains(QStringLiteral("K4ABC (2)")));
        QVERIFY(doubleRows.join(' ').contains(QStringLiteral("N3XYZ")));
        QVERIFY(doubleRows.join(' ').contains(QStringLiteral("Combined RF")));
        QVERIFY(doubleRows.contains(QStringLiteral("K1ABC  -73  2s")));
        for (const QString& row : doubleRows) { QCOMPARE(metrics.elidedText(row, Qt::ElideRight, 194), row); }
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
        QVERIFY(captureCard(tags.first(), tags.first().reportLines, QStringLiteral("unknown-one-receiver-no-directory")));
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
        tag.stations = QJsonArray{talker, member};
        tag.reportLines = rows;
        tag.active = true;
        Pan pan;
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const QImage image = pan.draw();
        const auto* hit = hitFor(pan.widget, tag.roomId);
        QVERIFY(hit != nullptr);
        QVERIFY(pan.widget.roundtableTagSpectrumRectForTest().contains(hit->rect));
        QFont font = WorkspacePreview::readableFont(QFont(), &image);
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(11));
        font.setWeight(QFont::Bold);
        const QFontMetrics metrics(font, &image);
        if (metrics.elidedText(sample, Qt::ElideRight, hit->rect.width() - 14) == sample) {
            QCOMPARE(metrics.elidedText(sample, Qt::ElideRight, hit->rect.width() - 14), sample);
        } else {
            const QStringList finalRows = roundtableCochannelRows(tag, 20, &metrics, hit->rect.width() - 14);
            QVERIFY(finalRows.last().contains(QStringLiteral("+1 reports")));
            QVERIFY(!finalRows.contains(sample));
        }
        if (qEnvironmentVariableIsSet("NEREUS_ROUNDTABLE_FORMAT_CAPTURES")) {
            const QString path = qEnvironmentVariable("NEREUS_ROUNDTABLE_FORMAT_CAPTURES")
                + QLatin1Char('/') + format + (nth > 1 ? QLatin1Char('-') + QString(reporter).replace('/', '_') : QString())
                + QStringLiteral(".png");
            QVERIFY(image.copy(hit->rect.adjusted(-4, -4, 4, 4)).save(path));
        }
    }

    void cochannelReportWidthFollowsTheRenderedFont_data()
    {
        QTest::addColumn<bool>("constrained");
        QTest::newRow("wide font") << false;
        QTest::newRow("wide font with keepout") << true;
    }

    void cochannelReportWidthFollowsTheRenderedFont()
    {
        QFETCH(bool, constrained);
        struct FontRestore {
            QFont saved{QApplication::font()};
            ~FontRestore() { QApplication::setFont(saved); }
        } restore;
        QFont wide = restore.saved;
        wide.setStretch(125);
        QApplication::setFont(wide);
        Pan pan;
        pan.widget.removeVfoWidget(0);
        const QRect spectrum = pan.widget.roundtableTagSpectrumRectForTest();
        SpectrumOverlayPanel* toolbar = nullptr;
        if (constrained) {
            toolbar = new SpectrumOverlayPanel(&pan.widget);
            // Leave 216 pixels after the normal 4-pixel keepout/edge gaps:
            // enough for the existing card, but not the measured wide font.
            toolbar->setFixedSize(spectrum.width() - 224, spectrum.height());
            toolbar->move(spectrum.topLeft());
            toolbar->show();
        }
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123, true);
        tag.stations = QJsonArray{QJsonObject{{"callsign", "W1AW"}},
            QJsonObject{{"callsign", "VK3ABC/P"}, {"nth", 2}}};
        const QString sample = QStringLiteral("VK3ABC/P (2)  S9 +12 / -61  25s stale");
        tag.active = true;
        tag.reportLines = {QStringLiteral("Talking: W1AW"), QStringLiteral("Heard by     S / dBm    Age"),
            sample, QStringLiteral("Open details")};
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const QImage image = pan.draw();
        const auto* hit = hitFor(pan.widget, tag.roomId);
        QVERIFY(hit != nullptr);
        QVERIFY(spectrum.contains(hit->rect));
        QFont font = WorkspacePreview::readableFont(QFont(), &image);
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(11));
        font.setWeight(QFont::Bold);
        const QFontMetrics metrics(font, &image);
        const QStringList displayedRows{QStringLiteral("7.185123 MHz"), QStringLiteral("Talking: W1AW"),
            QStringLiteral("Heard by     S / dBm    Age"), sample, QStringLiteral("Reports ›")};
        int requiredWidth = 208;
        for (const QString& row : displayedRows) {
            requiredWidth = std::max(requiredWidth, metrics.horizontalAdvance(row) + 14);
        }
        QVERIFY(requiredWidth > 216);
        if (!constrained) {
            QCOMPARE(hit->rect.width(), requiredWidth);
            QCOMPARE(metrics.elidedText(sample, Qt::ElideRight, hit->rect.width() - 14), sample);
        } else {
            QCOMPARE(hit->rect.width(), 208);
            QVERIFY(!hit->rect.intersects(toolbar->geometry()));
            QCOMPARE(metrics.elidedText(QStringLiteral("Reports ›"), Qt::ElideRight, hit->rect.width() - 14),
                     QStringLiteral("Reports ›"));
            const QStringList finalRows = roundtableCochannelRows(tag, 20, &metrics, hit->rect.width() - 14);
            QVERIFY(finalRows.last().contains(QStringLiteral("+1 reports")));
            QVERIFY(!finalRows.contains(sample));
            QCOMPARE(hit->rect.height(), finalRows.size() * std::max(14, metrics.height() + 2) + 8);
            // The disclosure is painted in the last row even when the full
            // report cannot fit beside the toolbar at the current font width.
            int textPixels = 0;
            const QRect disclosure(hit->rect.left() + 7, hit->rect.bottom() - 17, hit->rect.width() - 14, 14);
            for (int y = disclosure.top(); y <= disclosure.bottom(); ++y) {
                for (int x = disclosure.left(); x <= disclosure.right(); ++x) {
                    const QColor color = image.pixelColor(x, y);
                    if (color.red() > 100 && color.green() > 100 && color.blue() > 100) {
                        ++textPixels;
                    }
                }
            }
            QVERIFY(textPixels > 10);
        }
        if (qEnvironmentVariableIsSet("NEREUS_ROUNDTABLE_FORMAT_CAPTURES")) {
            const QString path = qEnvironmentVariable("NEREUS_ROUNDTABLE_FORMAT_CAPTURES")
                + (constrained ? QStringLiteral("/wide-font-constrained.png") : QStringLiteral("/wide-font.png"));
            QVERIFY(image.copy(hit->rect.adjusted(-4, -4, 4, 4)).save(path));
        }
    }

    void cochannelBoxHasRealBoundsAndRender()
    {
        Pan pan;
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        tag.mine = true;
        tag.stations = QJsonArray{QJsonObject{{"callsign", "W1AW"}}, QJsonObject{{"callsign", "K4ABC"}, {"nth", 2}},
            QJsonObject{{"callsign", "N3XYZ"}}, QJsonObject{{"callsign", "W4KZO"}}, QJsonObject{{"callsign", "KG4VCF"}}};
        tag.active = true;
        tag.reportLines = {QStringLiteral("Talking: W1AW"), QStringLiteral("Heard by     dBm    Age"),
            QStringLiteral("N4XYZ  -73  25s stale"), QStringLiteral("Open details")};
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const QImage image = pan.draw();
        const auto* hit = hitFor(pan.widget, tag.roomId);
        QVERIFY(hit != nullptr);
        // Frequency, current talker, Heard by, one complete reporter and
        // details; presence does not consume the report card's space.
        QFont font = WorkspacePreview::readableFont(QFont(), &image);
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(11));
        font.setWeight(QFont::Bold);
        QCOMPARE(hit->rect.height(), 5 * std::max(14, QFontMetrics(font, &image).height() + 2) + 8);
        QVERIFY(hit->rect.width() >= 200);
        QVERIFY(pan.widget.roundtableTagSpectrumRectForTest().contains(hit->rect));
        QVERIFY(!hit->rect.intersects(pan.flag->geometry()));
        if (qEnvironmentVariableIsSet("NEREUS_ROUNDTABLE_CAPTURE")) {
            QImage product(pan.widget.size(), QImage::Format_ARGB32_Premultiplied);
            product.fill(QColor(15, 15, 26));
            QPainter painter(&product);
            const QRect spectrum = pan.widget.roundtableTagSpectrumRectForTest();
            QVector<float> fixture(spectrum.width(), -125.0f);
            for (int x = 0; x < fixture.size(); ++x) {
                fixture[x] += static_cast<float>(3 * std::sin(x * 0.23)
                    + 58 * std::exp(-std::pow((x - 490) / 20.0, 2)));
            }
            pan.widget.setRenderedPixelsForTest(fixture);
            pan.widget.drawFrequencyGridForTest(painter, spectrum);
            pan.widget.drawSpectrumForTest(painter, spectrum);
            pan.widget.drawFreqScaleForTest(painter, pan.widget.freqScaleRectForTest());
            pan.widget.drawRoundtableTagsForTest(painter, spectrum);
            // QRhiWidget has no offscreen paint engine; render the actual child
            // VFO chrome over the same CPU spectrum primitives used in product.
            for (QWidget* child : pan.widget.findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
                if (child->isVisible()) { child->render(&painter, child->pos()); }
            }
            painter.end();
            QVERIFY(product.save(qEnvironmentVariable("NEREUS_ROUNDTABLE_CAPTURE")));
        }
    }

    void shortCochannelBoxKeepsReportsAndDetailsWithinItsTrueHeight()
    {
        Pan pan(360);
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123, true);
        tag.stations = QJsonArray{QJsonObject{{"callsign", "W1AW"}}, QJsonObject{{"callsign", "K4ABC"}},
            QJsonObject{{"callsign", "N3XYZ"}}, QJsonObject{{"callsign", "W4KZO"}}, QJsonObject{{"callsign", "KG4VCF"}}};
        tag.active = true;
        tag.reportLines = {QStringLiteral("Last talker: W4KZO"), QStringLiteral("Heard by     dBm    Age"),
            QStringLiteral("N4XYZ  -73  25s stale"), QStringLiteral("K9XYZ  -61  2s"), QStringLiteral("Open details")};
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        pan.draw();
        const auto* hit = hitFor(pan.widget, tag.roomId);
        QVERIFY(hit != nullptr);
        const QRect spectrum = pan.widget.roundtableTagSpectrumRectForTest();
        QVERIFY(spectrum.contains(hit->rect));
        QVERIFY(!pan.widget.receiverFlagRegion().intersects(hit->rect));
        QFont font = WorkspacePreview::readableFont(QFont());
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(11));
        font.setWeight(QFont::Bold);
        const int rowHeight = std::max(14, QFontMetrics(font).height() + 2);
        QVERIFY(hit->rect.height() >= 5 * rowHeight + 8);
        QCOMPARE((hit->rect.height() - 8) % rowHeight, 0);
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
        QVERIFY(!roundtableIsDouble({}, QJsonArray{a, b}, 7187000, QStringLiteral("lsb")));
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
            if (action->text() == QLatin1String("Tune active slice here")) {
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

        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Stale);
        const QImage stale = pan.draw();
        QCOMPARE(pan.widget.roundtableTagHitsForTest().size(), 4);
        QCOMPARE(QColor(stale.pixel(edge)).name(), QStringLiteral("#606066"));

        // A dropped link draws the same: no live color (D7).
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
        const QRect text(glyphCenter.x() + 10, doubled.top() + 2, doubled.right() - glyphCenter.x() - 18,
                         doubled.height() - 4);
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
            widget.setRoundtableTags(tags, RoundtableTagDisplay::Stale);
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
