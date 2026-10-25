// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_flag_line.cpp  (NereusSDR)
// =================================================================
//
// The Roundtable line on a VFO flag (design 11.2 VFO flag line, U3, U4,
// U7, U8): "Roundtable · 9 · K4ABC" with the room's state glyph, @N when
// messages name you, "N hear you" while you transmit there, the link state
// when Roundtable is not live, and nothing while it is off or not running
// here. Then the flag: the line at its foot, its text fitting the flag's
// width, the fold hiding it, and a click asking for the panel.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: Created (Roundtable plan Task 14). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-24: Roundtable plan Task 15: the keyed room's turn is yours by
//               its talker, as the server sends it. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAbstractButton>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSignalSpy>

#include "OperatorWording.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "gui/roundtable/RoundtableTagLayout.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RoundtableModel.h"

using namespace NereusSDR;

namespace {

class NullLink : public IStationLink {
public:
    explicit NullLink(bool roundtable = true) : m_roundtable(roundtable) {}
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool roundtableAvailable() const override { return m_roundtable; }

private:
    bool m_roundtable;
};

QJsonObject summary(const QString& id, qint64 hz, const QString& state, const QJsonArray& talkers,
                    int members)
{
    return QJsonObject{{QStringLiteral("id"), id},         {QStringLiteral("hz"), hz},
                       {QStringLiteral("state"), state},   {QStringLiteral("talkers"), talkers},
                       {QStringLiteral("members"), members}};
}

QJsonObject member(const QString& ref, bool you, bool present)
{
    QJsonObject out{{QStringLiteral("ref"), ref}, {QStringLiteral("callsign"), ref},
                    {QStringLiteral("you"), you}};
    if (!you) {
        out.insert(QStringLiteral("reading"),
                   QJsonObject{{QStringLiteral("present"), present}, {QStringLiteral("t"), 1}});
    }
    return out;
}

// A model on a link that offers Roundtable, live, with slice 0 in a room
// of nine where K4ABC talks and slice 1 in a doubled room.
struct Station {
    NullLink link;
    RoundtableState state;
    RoundtableModel model;

    explicit Station(bool roundtable = true) : link(roundtable)
    {
        model.attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        state.applyStationValue("sliceRooms",
                                QStringLiteral("{\"0\":\"lsb-7185000\",\"1\":\"lsb-7200000\","
                                               "\"2\":\"lsb-7212000\",\"3\":\"lsb-7230000\"}"));
        model.applyRooms(QJsonObject{
            {QStringLiteral("rooms"),
             QJsonArray{summary(QStringLiteral("lsb-7185000"), 7185000, QStringLiteral("talking"),
                                QJsonArray{QStringLiteral("K4ABC")}, 9),
                        summary(QStringLiteral("lsb-7200000"), 7200000, QStringLiteral("doubled"),
                                QJsonArray{QStringLiteral("K4ABC"), QStringLiteral("N3XYZ")}, 4),
                        summary(QStringLiteral("lsb-7212000"), 7212000, QStringLiteral("unknown"),
                                QJsonArray{}, 3),
                        summary(QStringLiteral("lsb-7230000"), 7230000, QStringLiteral("quiet"),
                                QJsonArray{}, 2)}}});
    }
    ~Station() { model.detach(); }

    void setLink(RoundtableModel::LinkState linkState)
    {
        state.applyStationValue("linkState", static_cast<int>(linkState));
    }

    // You transmit in slice 0's room: you, two stations hearing you and one
    // whose reading of you has gone.
    void keyed()
    {
        model.applyRoom(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("room")},
            {QStringLiteral("id"), QStringLiteral("lsb-7185000")},
            {QStringLiteral("members"),
             QJsonArray{member(QStringLiteral("N0CALL"), true, true), member(QStringLiteral("K4ABC"), false, true),
                        member(QStringLiteral("W5ABC"), false, true),
                        member(QStringLiteral("KC1ABC"), false, false)}},
            {QStringLiteral("now"),
             QJsonArray{QJsonObject{{QStringLiteral("id"), 40}, {QStringLiteral("kind"), QStringLiteral("talking")},
                                    // Yours: you are its talker (`mine` is the name
                                    // you gave a turn, wire spec 6.9).
                                    {QStringLiteral("talkers"),
                                     QJsonArray{QJsonObject{{QStringLiteral("ref"), QStringLiteral("N0CALL")},
                                                            {QStringLiteral("callsign"), QStringLiteral("N0CALL")},
                                                            {QStringLiteral("nth"), 1}}}},
                                    {QStringLiteral("start"), 1}, {QStringLiteral("end"), QJsonValue::Null},
                                    {QStringLiteral("names"), QJsonArray{}}, {QStringLiteral("mine"), QJsonValue::Null}}}},
            {QStringLiteral("earlier"), QJsonArray{}},
            {QStringLiteral("messages"), QJsonArray{}}});
    }
};

int glyph(RoundtableGlyph g)
{
    return static_cast<int>(g);
}

const QString kDot = QStringLiteral(" · ");

} // namespace

class TestRoundtableFlagLine : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.memlock.info=false\n"
                                                        "qt.qpa.fonts.warning=false"));
    }

    void aLiveRoomReadsMembersAndWhoTalks()
    {
        Station station;
        const RoundtableFlagLine line = roundtableFlagLineFor(&station.model, 0);
        QVERIFY(line.visible);
        QCOMPARE(line.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("9") + kDot
                                + QStringLiteral("K4ABC"));
        QCOMPARE(line.glyph, glyph(RoundtableGlyph::Talking));
        QCOMPARE(line.mentions, 0);

        const RoundtableFlagLine doubled = roundtableFlagLineFor(&station.model, 1);
        QCOMPARE(doubled.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("4") + kDot
                                   + QStringLiteral("Doubled"));
        QCOMPARE(doubled.glyph, glyph(RoundtableGlyph::Doubled));
        const RoundtableFlagLine unknown = roundtableFlagLineFor(&station.model, 2);
        QCOMPARE(unknown.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("3") + kDot
                                   + QStringLiteral("Unknown"));
        QCOMPARE(unknown.glyph, glyph(RoundtableGlyph::Unknown));
        const RoundtableFlagLine quiet = roundtableFlagLineFor(&station.model, 3);
        QCOMPARE(quiet.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("2") + kDot
                                 + QStringLiteral("Quiet"));
        QCOMPARE(quiet.glyph, glyph(RoundtableGlyph::Quiet));

        // A slice in no room yet.
        const RoundtableFlagLine nobody = roundtableFlagLineFor(&station.model, 5);
        QVERIFY(nobody.visible);
        QCOMPARE(nobody.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("Nobody here"));
        QCOMPARE(nobody.glyph, glyph(RoundtableGlyph::Quiet));
    }

    void messagesThatNameYouShowAsAtN()
    {
        Station station;
        station.state.applyStationValue("mentionCounts", QStringLiteral("{\"lsb-7185000\":2}"));
        const RoundtableFlagLine line = roundtableFlagLineFor(&station.model, 0);
        QCOMPARE(line.mentions, 2);
        QCOMPARE(line.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("9") + kDot
                                + QStringLiteral("K4ABC"));
        QCOMPARE(roundtableFlagLineFor(&station.model, 1).mentions, 0);
    }

    void keyedReadsHowManyHearYou()
    {
        Station station;
        station.keyed();
        const RoundtableFlagLine line = roundtableFlagLineFor(&station.model, 0);
        QCOMPARE(line.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("2 hear you"));
        QCOMPARE(line.glyph, glyph(RoundtableGlyph::You));
        // Another slice's line is unchanged.
        QCOMPARE(roundtableFlagLineFor(&station.model, 1).glyph, glyph(RoundtableGlyph::Doubled));
    }

    void theLinkStateShowsWhenNotLive()
    {
        using LinkState = RoundtableModel::LinkState;
        Station station;
        station.setLink(LinkState::Connecting);
        RoundtableFlagLine line = roundtableFlagLineFor(&station.model, 0);
        QVERIFY(line.visible);
        QCOMPARE(line.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("Connecting"));
        QCOMPARE(line.glyph, glyph(RoundtableGlyph::Waiting));

        station.setLink(LinkState::Reconnecting);
        line = roundtableFlagLineFor(&station.model, 0);
        QCOMPARE(line.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("Reconnecting"));
        QCOMPARE(line.glyph, glyph(RoundtableGlyph::Waiting));

        station.setLink(LinkState::Offline);
        line = roundtableFlagLineFor(&station.model, 0);
        QCOMPARE(line.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("Offline"));
        QCOMPARE(line.glyph, glyph(RoundtableGlyph::Offline));

        // Hidden still sees everything (D21): the room, as when live.
        station.setLink(LinkState::Hidden);
        line = roundtableFlagLineFor(&station.model, 0);
        QCOMPARE(line.text, QStringLiteral("Roundtable") + kDot + QStringLiteral("9") + kDot
                                + QStringLiteral("K4ABC"));

        station.setLink(LinkState::Off);
        QVERIFY(!roundtableFlagLineFor(&station.model, 0).visible);
    }

    void nothingWithoutRoundtable()
    {
        QVERIFY(!roundtableFlagLineFor(nullptr, 0).visible);
        Station noRoundtable(false);
        QVERIFY(!roundtableFlagLineFor(&noRoundtable.model, 0).visible);
        RoundtableModel detached;
        QVERIFY(!roundtableFlagLineFor(&detached, 0).visible);
    }

    void everyTextIsPlain()
    {
        using LinkState = RoundtableModel::LinkState;
        Station station;
        station.state.applyStationValue("mentionCounts", QStringLiteral("{\"lsb-7185000\":1}"));
        QStringList texts;
        for (int slice = 0; slice < 6; ++slice) {
            texts.append(roundtableFlagLineFor(&station.model, slice).text);
        }
        for (LinkState linkState : {LinkState::Connecting, LinkState::Reconnecting, LinkState::Offline}) {
            station.setLink(linkState);
            texts.append(roundtableFlagLineFor(&station.model, 0).text);
        }
        station.setLink(LinkState::Live);
        station.keyed();
        texts.append(roundtableFlagLineFor(&station.model, 0).text);
        for (const QString& text : texts) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
    }

    void theFlagShowsTheLineAtItsFoot()
    {
        QWidget host;
        host.resize(400, 600);
        auto* flagWidget = new VfoWidget(&host);
        VfoWidget& flag = *flagWidget;
        host.show();
        QAbstractButton* button = flag.roundtableLineButtonForTest();
        QVERIFY(button != nullptr);
        QVERIFY(button->isHidden());
        const int before = flag.height();

        RoundtableFlagLine line;
        line.visible = true;
        line.glyph = glyph(RoundtableGlyph::Talking);
        line.text = QStringLiteral("Roundtable") + kDot + QStringLiteral("9") + kDot + QStringLiteral("K4ABC");
        flag.setRoundtableLine(line);
        QVERIFY(button->isVisible());
        QCOMPARE(flag.roundtableLine(), line);
        QVERIFY(flag.height() > before);
        // At the foot of the flag, within its width.
        QVERIFY(button->geometry().left() >= 0);
        QVERIFY(button->geometry().right() < flag.width());
        for (QWidget* child : flag.findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (child != button && child->isVisible() && !child->isWindow()) {
                QVERIFY2(child->geometry().bottom() <= button->geometry().top(),
                         qPrintable(child->objectName()));
            }
        }
        QCOMPARE(button->accessibleName(), line.text);

        line.visible = false;
        flag.setRoundtableLine(line);
        QVERIFY(button->isHidden());
        QCOMPARE(flag.height(), before);
    }

    void theLongestLinesFitTheFlag()
    {
        QWidget host;
        host.resize(400, 600);
        auto* flagWidget = new VfoWidget(&host);
        VfoWidget& flag = *flagWidget;
        host.show();
        QAbstractButton* button = flag.roundtableLineButtonForTest();
        RoundtableFlagLine line;
        line.visible = true;
        line.glyph = glyph(RoundtableGlyph::Talking);
        line.mentions = 12;
        flag.setRoundtableLine(line);
        QCOMPARE(flag.width(), 252);

        // The line's own font and pads (VfoWidget.cpp RoundtableLineButton):
        // 5 px pads, a 9 px glyph, the @N badge 8 px wider than its text.
        QFont font;
        font.setPixelSize(11);
        font.setWeight(QFont::DemiBold);
        QFont badge = font;
        badge.setWeight(QFont::ExtraBold);
        const int badgeW = std::max(15, QFontMetrics(badge).horizontalAdvance(QStringLiteral("@12")) + 8);
        const int room = button->width() - (5 + 9 + 5) - 5 - badgeW - 5;
        const QStringList longest{
            QStringLiteral("Roundtable") + kDot + QStringLiteral("99") + kDot + QStringLiteral("KC1ABC/MM"),
            QStringLiteral("Roundtable") + kDot + QStringLiteral("Reconnecting"),
            QStringLiteral("Roundtable") + kDot + QStringLiteral("Nobody here"),
            QStringLiteral("Roundtable") + kDot + QStringLiteral("99") + kDot + QStringLiteral("Unknown"),
            QStringLiteral("Roundtable") + kDot + QStringLiteral("99") + kDot + QStringLiteral("Doubled"),
            QStringLiteral("Roundtable") + kDot + QStringLiteral("99 hear you")};
        for (const QString& text : longest) {
            QVERIFY2(QFontMetrics(font).horizontalAdvance(text) <= room,
                     qPrintable(QStringLiteral("%1: %2 > %3")
                                    .arg(text)
                                    .arg(QFontMetrics(font).horizontalAdvance(text))
                                    .arg(room)));
        }
    }

    void aClickAsksForThePanel()
    {
        QWidget host;
        host.resize(400, 600);
        auto* flagWidget = new VfoWidget(&host);
        VfoWidget& flag = *flagWidget;
        host.show();
        RoundtableFlagLine line;
        line.visible = true;
        line.text = QStringLiteral("Roundtable") + kDot + QStringLiteral("Connecting");
        flag.setRoundtableLine(line);
        QSignalSpy clicked(&flag, &VfoWidget::roundtableLineClicked);
        QAbstractButton* button = flag.roundtableLineButtonForTest();
        QTest::mouseClick(button, Qt::LeftButton, Qt::NoModifier, button->rect().center());
        QCOMPARE(clicked.count(), 1);
    }

    void theFoldHidesTheLine()
    {
        QWidget host;
        host.resize(400, 600);
        auto* flagWidget = new VfoWidget(&host);
        VfoWidget& flag = *flagWidget;
        host.show();
        QAbstractButton* button = flag.roundtableLineButtonForTest();
        RoundtableFlagLine line;
        line.visible = true;
        line.text = QStringLiteral("Roundtable") + kDot + QStringLiteral("Offline");
        flag.setRoundtableLine(line);
        flag.setCollapsed(true);
        QVERIFY(button->isHidden());
        flag.setCollapsed(false);
        QVERIFY(button->isVisible());

        // Roundtable stops while folded: the line stays gone on unfold.
        flag.setCollapsed(true);
        line.visible = false;
        flag.setRoundtableLine(line);
        QVERIFY(button->isHidden());
        flag.setCollapsed(false);
        QVERIFY(button->isHidden());

        // And starts while folded: it comes back on unfold, not before.
        flag.setCollapsed(true);
        line.visible = true;
        flag.setRoundtableLine(line);
        QVERIFY(button->isHidden());
        flag.setCollapsed(false);
        QVERIFY(button->isVisible());
    }
};

QTEST_MAIN(TestRoundtableFlagLine)
#include "tst_roundtable_flag_line.moc"
