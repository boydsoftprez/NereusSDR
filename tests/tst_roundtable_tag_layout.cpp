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
// and how a pan shows them for each link state (U4).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: Created (Roundtable plan Task 14). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonObject>

#include "OperatorWording.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "gui/roundtable/RoundtableTagLayout.h"
#include "models/RoundtableModel.h"

using namespace NereusSDR;

namespace {

constexpr double kTagWidth = 80.0;
constexpr double kGap = 4.0;

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

} // namespace

class TestRoundtableTagLayout : public QObject {
    Q_OBJECT

private slots:
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
            {QStringLiteral("members"), QJsonArray{}},
            {QStringLiteral("now"),
             QJsonArray{QJsonObject{{QStringLiteral("id"), 40}, {QStringLiteral("kind"), QStringLiteral("talking")},
                                    {QStringLiteral("talkers"), QJsonArray{}},
                                    {QStringLiteral("start"), 1}, {QStringLiteral("end"), QJsonValue::Null},
                                    {QStringLiteral("names"), QJsonArray{}}, {QStringLiteral("mine"), true}}}},
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
            {LinkState::Hidden, RoundtableTagDisplay::Live},
            {LinkState::Offline, RoundtableTagDisplay::Stale}};
        for (const auto& [link_state, display] : cases) {
            state.applyStationValue("linkState", static_cast<int>(link_state));
            QCOMPARE(roundtableTagDisplayFor(&model), display);
        }
        model.detach();
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
