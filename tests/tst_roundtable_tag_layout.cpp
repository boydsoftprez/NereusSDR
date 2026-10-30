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
// Collapsing the pan's toolbar lays the tags out again, in the GPU path's
// cached overlay too.
//
// =================================================================
// Modification history (NereusSDR):
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
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>

#include "OperatorWording.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "core/ConnectionState.h"
#include "core/WdspTypes.h"
#include "gui/applets/RoundtableApplet.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
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
    doubled.label = QStringLiteral("Doubled");
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
        QCOMPARE(rows.first(), QStringLiteral("Last talker: W1AW"));
        QVERIFY(rows.contains(QStringLiteral("K4ABC  -73  25s")));
        turn.insert("hz", 7185500);
        view.insert("now", QJsonArray{turn});
        QCOMPARE(roundtableTagReportLines(tags.first(), view, 27000), QStringList{QStringLiteral("Read reports · details")});
        turn.insert("hz", 7185123);
        turn.insert("kind", "doubled");
        view.insert("now", QJsonArray{turn});
        rows = roundtableTagReportLines(tags.first(), view, 27000);
        QVERIFY(rows.contains(QStringLiteral("Combined RF · unattributed")));
        QVERIFY(!rows.join(' ').contains(QStringLiteral("-73")));
        turn.insert("kind", "talking");
        turn.insert("id", 13);
        view.insert("now", QJsonArray{turn});
        rows = roundtableTagReportLines(tags.first(), view, 27000);
        QVERIFY(rows.contains(QStringLiteral("K4ABC  -  No report")));
    }

    void cochannelBoxHasRealBoundsAndRender()
    {
        Pan pan;
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123);
        tag.mine = true;
        tag.stations = QJsonArray{QJsonObject{{"callsign", "W1AW"}}, QJsonObject{{"callsign", "K4ABC"}, {"nth", 2}},
            QJsonObject{{"callsign", "N3XYZ"}}, QJsonObject{{"callsign", "W4KZO"}}, QJsonObject{{"callsign", "KG4VCF"}}};
        tag.reportLines = {QStringLiteral("Last talker: W1AW"), QStringLiteral("Heard by     dBm    Age"),
            QStringLiteral("N4XYZ  -73  25s"), QStringLiteral("Open details")};
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const QImage image = pan.draw();
        const auto* hit = hitFor(pan.widget, tag.roomId);
        QVERIFY(hit != nullptr);
        // Frequency, three distinct people, +2, Last talker, Heard by,
        // one separately named reporter/sample, and Reports details.
        QCOMPARE(hit->rect.height(), 9 * 14 + 8);
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

    void shortCochannelBoxKeepsPeopleAndDetailsWithinItsTrueHeight()
    {
        Pan pan(360);
        RoundtableTag tag = tagAt(QStringLiteral("frequency:lsb-7185123"), 7185123, true);
        tag.stations = QJsonArray{QJsonObject{{"callsign", "W1AW"}}, QJsonObject{{"callsign", "K4ABC"}},
            QJsonObject{{"callsign", "N3XYZ"}}, QJsonObject{{"callsign", "W4KZO"}}, QJsonObject{{"callsign", "KG4VCF"}}};
        tag.reportLines = {QStringLiteral("Last talker: W4KZO"), QStringLiteral("Heard by     dBm    Age"),
            QStringLiteral("N4XYZ  -73  25s"), QStringLiteral("K9XYZ  -61  2s"), QStringLiteral("Open details")};
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        pan.draw();
        const auto* hit = hitFor(pan.widget, tag.roomId);
        QVERIFY(hit != nullptr);
        const QRect spectrum = pan.widget.roundtableTagSpectrumRectForTest();
        QVERIFY(spectrum.contains(hit->rect));
        QVERIFY(!pan.widget.receiverFlagRegion().intersects(hit->rect));
        QVERIFY(hit->rect.height() >= 5 * 14 + 8);
        QCOMPARE((hit->rect.height() - 8) % 14, 0);
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
        QCOMPARE(tags.at(1).label, QStringLiteral("Doubled"));
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
            {LinkState::Reconnecting, RoundtableTagDisplay::Live},
            // You are hidden: everything as when live, dimmed.
            {LinkState::Hidden, RoundtableTagDisplay::Dimmed},
            {LinkState::Offline, RoundtableTagDisplay::Stale}};
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

    void newTagsMarkTheGpuOverlayForRedraw()
    {
        Pan pan;
        pan.widget.clearOverlayStaticDirtyForTest();
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
#ifdef NEREUS_GPU_SPECTRUM
        QVERIFY(pan.widget.overlayStaticDirtyForTest());
        // The same tags again change nothing.
        pan.widget.clearOverlayStaticDirtyForTest();
        pan.widget.setRoundtableTags(panTags(), RoundtableTagDisplay::Live);
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
        QSignalSpy collapsed(toolbar, &SpectrumOverlayPanel::collapsed);
        auto* collapse = qobject_cast<QPushButton*>(toolbar->childAt(3, 3));
        QVERIFY(collapse != nullptr);
        collapse->click();
        QCOMPARE(collapsed.count(), 1);
        QCOMPARE(collapsed.first().first().toBool(), true);
        QVERIFY(toolbar->geometry().bottom() < openRect.top());
#ifdef NEREUS_GPU_SPECTRUM
        QVERIFY(pan.widget.overlayStaticDirtyForTest());
#endif
        pan.draw();
        const auto* shut = hitFor(pan.widget, id);
        QVERIFY(shut != nullptr);
        QVERIFY2(shut->rect.left() < openRect.left(),
                 qPrintable(QStringLiteral("tag at %1, was %2").arg(shut->rect.left()).arg(openRect.left())));

        // Opened again, the tag moves aside again.
        pan.widget.clearOverlayStaticDirtyForTest();
        collapse->click();
#ifdef NEREUS_GPU_SPECTRUM
        QVERIFY(pan.widget.overlayStaticDirtyForTest());
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
