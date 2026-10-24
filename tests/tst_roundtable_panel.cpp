// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_panel.cpp  (NereusSDR)
// =================================================================
//
// RoundtablePanel (Roundtable design 11.1, 11.2 Panel) over a window's
// RoundtableModel posed from the Core's `roundtable` object, on a test
// clock: the order of the sections, the readings (nearest first, AWAY,
// stale past 20 s with an amber age), earlier turns and Name, the read bar
// and Tune A here, the keyed TX line, the connection line's states, Send
// while not live, drafts, the counter, Not sent and Try again, the message
// menus, the mute fold, the new and mention pills, following at the end,
// and the keyboard. The applet: it follows the active slice, Tune A here
// tunes that slice, tuning it anywhere goes back to following it, and
// Archive opens the web sign-in link. The pop-out window: its three shapes
// by width, the Wide preset and the saved geometry, the close box docking
// the panel back (a close from code does not), Put it back here through
// the applet panel, and the room list's keyboard.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: Created (Roundtable plan Task 13). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <cmath>

#include <QApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QToolButton>
#include <QUrl>
#include <QListWidget>
#include <QVBoxLayout>
#include <qpa/qwindowsysteminterface.h>

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "core/WdspTypes.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/RoundtableApplet.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr qint64 kNow = 1'800'000'000'000LL;
const QString kRoom = QStringLiteral("lsb-7185000");
const QString kOther = QStringLiteral("lsb-7249000");

qint64 g_now = kNow;

QtMessageHandler g_previousHandler = nullptr;

// The offscreen platform's notes about what it cannot do (raise, grab,
// size hints) and its font aliases are not the panel's; everything else
// goes through.
void quietPlatformNotes(QtMsgType type, const QMessageLogContext& context, const QString& text)
{
    if (text.startsWith(QLatin1String("This plugin does not support"))
        || text.startsWith(QLatin1String("Populating font family aliases"))) {
        return;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, text);
    }
}

class RecordingLink : public IStationLink {
public:
    QStringList calls;
    CommandOutcome answer{true, QString()};

    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }

    bool roundtableAvailable() const override { return true; }
    CommandOutcome requestRoundtablePost(const QString& roomId, const QString& text,
                                         const QString&) override
    {
        return note(QStringLiteral("post %1 %2").arg(roomId, text));
    }
    CommandOutcome requestRoundtableDelete(const QString& id) override
    {
        return note(QStringLiteral("delete ") + id);
    }
    CommandOutcome requestRoundtableName(const QString& turnId, const QString& callsign) override
    {
        return note(QStringLiteral("name %1 %2").arg(turnId, callsign));
    }
    CommandOutcome requestRoundtableReport(const QString& kind, const QString& target,
                                           const QString& reason) override
    {
        return note(QStringLiteral("report %1 %2 %3").arg(kind, target, reason));
    }
    CommandOutcome requestRoundtableRead(const QString& roomId) override
    {
        return note(QStringLiteral("read ") + roomId);
    }
    CommandOutcome requestRoundtableUnread(const QString& roomId) override
    {
        return note(QStringLiteral("unread ") + roomId);
    }
    CommandOutcome requestRoundtableMute(const QString& callsign) override
    {
        return note(QStringLiteral("mute ") + callsign);
    }
    CommandOutcome requestRoundtableUnmute(const QString& callsign) override
    {
        return note(QStringLiteral("unmute ") + callsign);
    }
    CommandOutcome requestRoundtableMarkMentionRead(qint64 upToMs) override
    {
        return note(QStringLiteral("markMentionRead %1").arg(upToMs));
    }
    CommandOutcome requestRoundtableWebSignIn() override
    {
        return note(QStringLiteral("webSignIn"));
    }

private:
    CommandOutcome note(const QString& call)
    {
        calls.append(call);
        return answer;
    }
};

QJsonObject reading(double dbm, qint64 ageMs)
{
    return QJsonObject{{QStringLiteral("dbm"), dbm}, {QStringLiteral("t"), g_now - ageMs},
                       {QStringLiteral("present"), true}};
}

QJsonObject member(const QString& ref, const QString& callsign, double km, const QJsonValue& rd,
                   bool you = false, bool away = false)
{
    QJsonObject m{{QStringLiteral("ref"), ref},          {QStringLiteral("callsign"), callsign},
                  {QStringLiteral("nth"), 1},            {QStringLiteral("grid"), QStringLiteral("EM73")},
                  {QStringLiteral("offsetHz"), 0},       {QStringLiteral("away"), away},
                  {QStringLiteral("keyed"), false},      {QStringLiteral("you"), you},
                  {QStringLiteral("reading"), rd}};
    if (km >= 0) {
        m.insert(QStringLiteral("km"), km);
    }
    return m;
}

QJsonObject talker(const QString& ref, const QString& callsign)
{
    return QJsonObject{{QStringLiteral("ref"), ref}, {QStringLiteral("callsign"), callsign},
                       {QStringLiteral("nth"), 1}};
}

QJsonObject turn(qint64 id, const QString& kind, const QJsonArray& talkers, qint64 start,
                 const QJsonValue& end, bool mine = false)
{
    return QJsonObject{{QStringLiteral("id"), id},       {QStringLiteral("kind"), kind},
                       {QStringLiteral("talkers"), talkers}, {QStringLiteral("start"), start},
                       {QStringLiteral("end"), end},     {QStringLiteral("names"), QJsonArray{}},
                       {QStringLiteral("mine"), mine}};
}

QJsonObject message(qint64 id, const QString& ref, const QString& callsign, const QString& text,
                    const QString& state = QStringLiteral("shown"))
{
    return QJsonObject{{QStringLiteral("id"), id},     {QStringLiteral("ref"), ref},
                       {QStringLiteral("callsign"), callsign}, {QStringLiteral("nth"), 1},
                       {QStringLiteral("t"), g_now - 60000 + id}, {QStringLiteral("state"), state},
                       {QStringLiteral("text"), text}};
}

QJsonArray members()
{
    return QJsonArray{member(QStringLiteral("me"), QStringLiteral("KG4VCF"), -1, QJsonValue::Null, true),
                      member(QStringLiteral("a"), QStringLiteral("W4ABC"), 640, reading(-73, 2000)),
                      member(QStringLiteral("b"), QStringLiteral("N3XYZ"), 120, reading(-85, 1000)),
                      member(QStringLiteral("c"), QStringLiteral("K9AWY"), 900, reading(-90, 1000),
                             false, true),
                      member(QStringLiteral("t"), QStringLiteral("W1TLK"), 300, reading(-60, 500))};
}

QJsonObject roomOf(const QString& id, const QJsonArray& now, const QJsonArray& earlier,
                   const QJsonArray& messages, const QJsonArray& who = members())
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("room")}, {QStringLiteral("id"), id},
                       {QStringLiteral("members"), who},               {QStringLiteral("now"), now},
                       {QStringLiteral("earlier"), earlier},           {QStringLiteral("messages"), messages}};
}

QJsonObject talkingRoom(const QJsonArray& messages = {})
{
    return roomOf(kRoom,
                  QJsonArray{turn(30, QStringLiteral("talking"),
                                  QJsonArray{talker(QStringLiteral("t"), QStringLiteral("W1TLK"))},
                                  g_now - 42000, QJsonValue::Null)},
                  QJsonArray{turn(20, QStringLiteral("unknown"), QJsonArray{}, g_now - 300000,
                                  g_now - 240000),
                             turn(10, QStringLiteral("talking"),
                                  QJsonArray{talker(QStringLiteral("a"), QStringLiteral("W4ABC"))},
                                  g_now - 600000, g_now - 400000)},
                  messages);
}

struct Posed {
    RecordingLink link;
    RoundtableState state;
    RoundtableModel model;

    Posed()
    {
        model.attachStation(&link, &state);
        setLink(RoundtableModel::LinkState::Live);
        state.applyStationValue("sliceRooms", QStringLiteral("{\"0\":\"%1\"}").arg(kRoom));
        QJsonArray rooms{
            QJsonObject{{QStringLiteral("id"), kRoom}, {QStringLiteral("hz"), 7185000},
                        {QStringLiteral("members"), 5}},
            QJsonObject{{QStringLiteral("id"), kOther}, {QStringLiteral("hz"), 7249000},
                        {QStringLiteral("members"), 3}}};
        model.applyRooms(QJsonObject{{QStringLiteral("rooms"), rooms}});
    }

    ~Posed() { model.detach(); }

    void setLink(RoundtableModel::LinkState s, qint64 since = 0)
    {
        state.applyStationValue("linkSince", since);
        state.applyStationValue("linkState", static_cast<int>(s));
    }
};

template <typename T>
T* child(QWidget* root, const QString& name)
{
    return root->findChild<T*>(name);
}

QStringList texts(QWidget* root, const QString& name)
{
    QStringList out;
    for (QLabel* label : root->findChildren<QLabel*>(name)) {
        if (label->isVisibleTo(root)) {
            out.append(label->text());
        }
    }
    return out;
}

int yIn(QWidget* root, QWidget* w)
{
    return w->mapTo(root, QPoint(0, 0)).y();
}

QMenu* openMenu(const QString& name)
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* menu = qobject_cast<QMenu*>(w); menu != nullptr && menu->objectName() == name
                                                  && menu->isVisible()) {
            return menu;
        }
    }
    return nullptr;
}

QAction* actionNamed(QMenu* menu, const QString& text)
{
    for (QAction* action : menu->actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

void settle()
{
    for (int i = 0; i < 4; ++i) {
        QCoreApplication::processEvents();
    }
}

} // namespace

class TstRoundtablePanel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        g_previousHandler = qInstallMessageHandler(quietPlatformNotes);
        // A radio with no connection says so as its slices tune; not the
        // applet's.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.warning=false\n"
                                                        "nereus.freedv.*.debug=false"));
        RoundtableModel::setClockForTest([] { return g_now; });
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"), QStringLiteral("KG4VCF"));
    }

    void cleanupTestCase()
    {
        RoundtableModel::setClockForTest({});
        qInstallMessageHandler(g_previousHandler);
    }

    void init()
    {
        g_now = kNow;
        AppSettings::instance().remove(QStringLiteral("Roundtable/MuteList"));
        AppSettings::instance().remove(QStringLiteral("Roundtable/PanelOpensOn"));
        AppSettings::instance().remove(QStringLiteral("Roundtable/EarlierTurnsShown"));
        AppSettings::instance().remove(QLatin1String(RoundtableWindow::kGeometryKey));
        AppSettings::instance().remove(QStringLiteral("AppletroundtableFloating"));
    }

    void sectionsInOrder()
    {
        Posed p;
        p.model.applyRoom(talkingRoom(QJsonArray{
            message(1, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("good copy"))}));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1400);
        panel.followSlice(0);
        panel.show();
        settle();
        QCOMPARE(panel.roomId(), kRoom);
        QWidget* content = child<QWidget>(&panel, QStringLiteral("roundtableContent"));
        QStringList order{QStringLiteral("roundtableHeader"), QStringLiteral("roundtableConnectionLine"),
                          QStringLiteral("roundtableTalker"), QStringLiteral("roundtableReadings"),
                          QStringLiteral("roundtableEarlier"), QStringLiteral("roundtableMessages")};
        int last = -1;
        for (const QString& name : order) {
            QWidget* w = child<QWidget>(content, name);
            QVERIFY2(w != nullptr && w->isVisible(), qPrintable(name));
            const int y = yIn(content, w);
            QVERIFY2(y > last, qPrintable(name));
            last = y;
        }
        // One scroll, the box pinned below it.
        auto* scroll = child<QScrollArea>(&panel, QStringLiteral("roundtableScroll"));
        auto* footer = child<QWidget>(&panel, QStringLiteral("roundtableFooter"));
        QVERIFY(yIn(&panel, footer) >= yIn(&panel, scroll) + scroll->height() - 1);
        QVERIFY(footer->isAncestorOf(child<QPlainTextEdit>(&panel, QStringLiteral("roundtableBox"))));
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTalkerName")), QStringList{QStringLiteral("W1TLK")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingsTitle")),
                 QStringList{QStringLiteral("READINGS OF W1TLK")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableCount")),
                 QStringList{QStringLiteral("5 in the room · 1 away")});
    }

    void readingsNearestFirstAwayAndStale()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1400);
        panel.followSlice(0);
        panel.show();
        settle();
        // The talker and you have no row; nearest first.
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingCall")),
                 (QStringList{QStringLiteral("N3XYZ"), QStringLiteral("W4ABC"), QStringLiteral("K9AWY")}));
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingValue")),
                 (QStringList{QStringLiteral("S7"), QStringLiteral("S9"), QStringLiteral("S6")}));
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingAge")),
                 (QStringList{QStringLiteral("1 s"), QStringLiteral("2 s"), QStringLiteral("1 s")}));
        QCOMPARE(texts(&panel, QStringLiteral("roundtableAwayTag")), QStringList{QStringLiteral("AWAY")});
        QVERIFY(child<QLabel>(&panel, QStringLiteral("roundtableAwayNote")) != nullptr);

        // Past 20 s a row is stale: grey, its age amber.
        g_now += 18500;
        panel.refreshNow();
        int stale = 0;
        for (QWidget* row : panel.findChildren<QWidget*>(QStringLiteral("roundtableReadingRow"))) {
            stale += row->property("stale").toBool() ? 1 : 0;
        }
        QCOMPARE(stale, 1); // W4ABC, 20.5 s old
        for (QWidget* row : panel.findChildren<QWidget*>(QStringLiteral("roundtableReadingRow"))) {
            if (row->property("stale").toBool()) {
                QCOMPARE(row->property("callsign").toString(), QStringLiteral("W4ABC"));
                auto* age = row->findChild<QLabel*>(QStringLiteral("roundtableReadingAge"));
                QVERIFY(age->styleSheet().contains(QStringLiteral("#d8a55f")));
                QCOMPARE(age->text(), QStringLiteral("20 s"));
            }
        }
    }

    void connectionLineAndSendWhileNotLive()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtablePanel panel(&p.model);
        panel.resize(340, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        auto* line = child<QLabel>(&panel, QStringLiteral("roundtableConnectionLine"));
        QCOMPARE(line->text(), QStringLiteral("Live · last reading 0 s ago"));
        auto* box = child<QPlainTextEdit>(&panel, QStringLiteral("roundtableBox"));
        auto* send = child<QPushButton>(&panel, QStringLiteral("roundtableSendBtn"));
        box->setPlainText(QStringLiteral("hello"));
        QVERIFY(send->isEnabled());

        p.setLink(RoundtableModel::LinkState::Reconnecting, g_now - 75000);
        QCOMPARE(line->text(), QStringLiteral("Reconnecting · nothing new for 1 min"));
        QVERIFY(!send->isEnabled());
        QCOMPARE(box->toPlainText(), QStringLiteral("hello"));
        QVERIFY(box->isEnabled());

        p.setLink(RoundtableModel::LinkState::Offline, 1'800'000'000'000LL - 3600000);
        QVERIFY(line->text().startsWith(QStringLiteral("Offline since ")));
        QVERIFY(line->text().endsWith(QStringLiteral(" UTC")));
        p.setLink(RoundtableModel::LinkState::Connecting);
        QCOMPARE(line->text(), QStringLiteral("Connecting"));
        p.setLink(RoundtableModel::LinkState::Hidden);
        QCOMPARE(line->text(), QStringLiteral("You are hidden"));
        QVERIFY(!send->isEnabled());
        QCOMPARE(box->toPlainText(), QStringLiteral("hello"));
        p.setLink(RoundtableModel::LinkState::Live);
        QVERIFY(send->isEnabled());
    }

    void draftSurvivesArrivalsAndRoomChanges()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        p.model.applyRoom(roomOf(kOther, {}, {}, {}));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        auto* box = child<QPlainTextEdit>(&panel, QStringLiteral("roundtableBox"));
        box->setPlainText(QStringLiteral("half a thought"));
        p.model.applyRoomDelta(QJsonObject{
            {QStringLiteral("room"), kRoom},
            {QStringLiteral("messages"),
             QJsonArray{message(7, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("hi"))}}});
        settle();
        QCOMPARE(box->toPlainText(), QStringLiteral("half a thought"));
        panel.readRoom(kOther);
        QCOMPARE(panel.roomId(), kOther);
        QCOMPARE(box->toPlainText(), QString());
        box->setPlainText(QStringLiteral("other room"));
        panel.backToSlice();
        QCOMPARE(box->toPlainText(), QStringLiteral("half a thought"));
        panel.readRoom(kOther);
        QCOMPARE(box->toPlainText(), QStringLiteral("other room"));
    }

    void readBarTuneAndBack()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        p.model.applyRoom(roomOf(kOther, {}, {}, {}));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        auto* bar = child<QWidget>(&panel, QStringLiteral("roundtableReadBar"));
        QVERIFY(!bar->isVisible());
        panel.readRoom(kOther);
        settle();
        QVERIFY(p.link.calls.contains(QStringLiteral("read ") + kOther));
        QVERIFY(bar->isVisible());
        QCOMPARE(child<QLabel>(&panel, QStringLiteral("roundtableReadLabel"))->text(),
                 QStringLiteral("Reading 7.249"));
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTag")), QStringList{QStringLiteral("NOT TUNED HERE")});
        // Reading only: no box to post from.
        QVERIFY(!child<QPlainTextEdit>(&panel, QStringLiteral("roundtableBox"))->isEnabled());
        QCOMPARE(child<QLabel>(&panel, QStringLiteral("roundtableHint"))->text(),
                 QStringLiteral("Tune a slice here to post. You can read everything."));

        QSignalSpy tune(&panel, &RoundtablePanel::tuneRequested);
        child<QPushButton>(&panel, QStringLiteral("roundtableTuneHereBtn"))->click();
        QCOMPARE(tune.size(), 1);
        QCOMPARE(tune.at(0).at(0).toLongLong(), 7249000LL);
        QCOMPARE(tune.at(0).at(1).toString(), QStringLiteral("lsb"));

        child<QPushButton>(&panel, QStringLiteral("roundtableBackBtn"))->click();
        QVERIFY(!panel.isReading());
        QCOMPARE(panel.roomId(), kRoom);
        QVERIFY(p.link.calls.contains(QStringLiteral("unread ") + kOther));
        QVERIFY(!bar->isVisible());
    }

    void keyedShowsReadingsOfYou()
    {
        Posed p;
        QJsonArray who = members();
        // Their readings of you while you transmit.
        p.model.applyRoom(roomOf(kRoom,
                                 QJsonArray{turn(40, QStringLiteral("talking"),
                                                 QJsonArray{talker(QStringLiteral("me"), QStringLiteral("KG4VCF"))},
                                                 g_now - 65000, QJsonValue::Null, true)},
                                 {}, {}, who));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        QVERIFY(child<QWidget>(&panel, QStringLiteral("roundtableTxLine"))->isVisible());
        QVERIFY(!child<QWidget>(&panel, QStringLiteral("roundtableHeader"))->isVisible());
        QVERIFY(!child<QWidget>(&panel, QStringLiteral("roundtableTalker"))->isVisible());
        QVERIFY(!child<QLabel>(&panel, QStringLiteral("roundtableConnectionLine"))->isVisible());
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTxText")),
                 QStringList{QStringLiteral("You · 4 hear you")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingsTitle")),
                 QStringList{QStringLiteral("READINGS OF YOU")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingCall")).size(), 4);
        QCOMPARE(texts(&panel, QStringLiteral("roundtableKeyedTime")).value(0), QStringLiteral("1:05"));
        g_now += 2000;
        panel.refreshNow();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableKeyedTime")).value(0), QStringLiteral("1:07"));
        // Not live while keyed: the connection line comes back.
        p.setLink(RoundtableModel::LinkState::Reconnecting, g_now);
        QVERIFY(child<QLabel>(&panel, QStringLiteral("roundtableConnectionLine"))->isVisible());
    }

    void earlierTurnsAndName()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1400);
        panel.followSlice(0);
        panel.show();
        settle();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTurnWho")),
                 (QStringList{QStringLiteral("Unknown"), QStringLiteral("W4ABC")}));
        auto names = panel.findChildren<QPushButton*>(QStringLiteral("roundtableTurnNameBtn"));
        QCOMPARE(names.size(), 1);
        QVERIFY(names.first()->isEnabled());
        names.first()->click();
        settle();
        auto* input = child<QLineEdit>(&panel, QStringLiteral("roundtableNameInput"));
        QVERIFY(input != nullptr);
        QVERIFY(input->hasFocus());
        QTest::keyClicks(input, QStringLiteral("k1abc"));
        QTest::keyClick(input, Qt::Key_Return);
        QVERIFY(p.link.calls.contains(QStringLiteral("name 20 K1ABC")));
        settle();
        QVERIFY(child<QLineEdit>(&panel, QStringLiteral("roundtableNameInput")) == nullptr);

        // Not tuned here, Name is off.
        p.model.applyRoom(roomOf(kOther, {},
                                 QJsonArray{turn(50, QStringLiteral("unknown"), {}, g_now - 9000,
                                                 g_now - 5000)},
                                 {}));
        panel.readRoom(kOther);
        settle();
        names = panel.findChildren<QPushButton*>(QStringLiteral("roundtableTurnNameBtn"));
        QCOMPARE(names.size(), 1);
        QVERIFY(!names.first()->isEnabled());
        QCOMPARE(names.first()->toolTip(), QStringLiteral("Tune a slice here to name a turn"));
    }

    void earlierTurnsShownSetting()
    {
        AppSettings::instance().setValue(QStringLiteral("Roundtable/EarlierTurnsShown"), QStringLiteral("5"));
        Posed p;
        QJsonArray earlier;
        for (int i = 0; i < 8; ++i) {
            earlier.append(turn(100 + i, QStringLiteral("talking"),
                                QJsonArray{talker(QStringLiteral("a"), QStringLiteral("W4ABC"))},
                                g_now - 100000 - i * 10000, g_now - 95000 - i * 10000));
        }
        p.model.applyRoom(roomOf(kRoom, {}, earlier, {}));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1400);
        panel.followSlice(0);
        panel.show();
        settle();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTurnWho")).size(), 5);
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTalkerName")),
                 QStringList{QStringLiteral("Nobody is talking")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingsTitle")),
                 QStringList{QStringLiteral("IN THE ROOM")});
    }

    void counterAndLimit()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtablePanel panel(&p.model);
        panel.resize(340, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        auto* box = child<QPlainTextEdit>(&panel, QStringLiteral("roundtableBox"));
        auto* counter = child<QLabel>(&panel, QStringLiteral("roundtableCounter"));
        auto* send = child<QPushButton>(&panel, QStringLiteral("roundtableSendBtn"));
        box->setPlainText(QString(399, QLatin1Char('x')));
        QVERIFY(!counter->isVisible());
        box->setPlainText(QString(400, QLatin1Char('x')));
        QVERIFY(counter->isVisible());
        QCOMPARE(counter->text(), QStringLiteral("400 / 500"));
        QVERIFY(send->isEnabled());
        box->setPlainText(QString(501, QLatin1Char('x')));
        QCOMPARE(counter->text(), QStringLiteral("501 / 500"));
        QVERIFY(!send->isEnabled());
        box->setPlainText(QStringLiteral("a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk"));
        QCOMPARE(counter->text(), QStringLiteral("11 lines, up to 10"));
        QVERIFY(!send->isEnabled());
    }

    void sendAndNotSent()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtablePanel panel(&p.model);
        panel.resize(340, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        auto* box = child<QPlainTextEdit>(&panel, QStringLiteral("roundtableBox"));
        QCOMPARE(child<QLabel>(&panel, QStringLiteral("roundtableHint"))->text(),
                 QStringLiteral("Sent with your reading of W1TLK"));
        box->setPlainText(QStringLiteral("first"));
        box->setFocus();
        QTest::keyClick(box, Qt::Key_Return);
        QVERIFY(p.link.calls.contains(QStringLiteral("post %1 first").arg(kRoom)));
        QCOMPARE(box->toPlainText(), QString());

        // Refused: Not sent, the text back, Try again sends it.
        p.link.answer = {false, QStringLiteral("Roundtable is not connected")};
        box->setPlainText(QStringLiteral("second"));
        child<QPushButton>(&panel, QStringLiteral("roundtableSendBtn"))->click();
        settle();
        QVERIFY(child<QWidget>(&panel, QStringLiteral("roundtableNotSent"))->isVisible());
        QCOMPARE(box->toPlainText(), QStringLiteral("second"));
        p.link.answer = {true, QString()};
        p.link.calls.clear();
        child<QPushButton>(&panel, QStringLiteral("roundtableTryAgainBtn"))->click();
        QCOMPARE(p.link.calls, QStringList{QStringLiteral("post %1 second").arg(kRoom)});
        QVERIFY(!child<QWidget>(&panel, QStringLiteral("roundtableNotSent"))->isVisible());
    }

    void messageMenusAndStates()
    {
        Posed p;
        p.model.applyRoom(talkingRoom(QJsonArray{
            message(1, QStringLiteral("me"), QStringLiteral("KG4VCF"), QStringLiteral("mine")),
            message(2, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("theirs")),
            message(3, QStringLiteral("b"), QStringLiteral("N3XYZ"), QString(), QStringLiteral("deleted"))}));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1600);
        panel.followSlice(0);
        panel.show();
        settle();
        QVERIFY(texts(&panel, QStringLiteral("roundtableMessageText")).contains(QStringLiteral("Deleted by sender")));
        auto buttons = panel.findChildren<QToolButton*>(QStringLiteral("roundtableMessageMenuBtn"));
        QCOMPARE(buttons.size(), 2); // a deleted message has no menu

        buttons.at(0)->click();
        QMenu* menu = openMenu(QStringLiteral("roundtableMessageMenu"));
        QVERIFY(menu != nullptr);
        QStringList labels;
        for (QAction* a : menu->actions()) {
            labels.append(a->text());
        }
        QCOMPARE(labels, QStringList{QStringLiteral("Delete")});
        actionNamed(menu, QStringLiteral("Delete"))->trigger();
        QVERIFY(p.link.calls.contains(QStringLiteral("delete 1")));
        menu->close();
        settle();

        buttons = panel.findChildren<QToolButton*>(QStringLiteral("roundtableMessageMenuBtn"));
        buttons.at(1)->click();
        menu = openMenu(QStringLiteral("roundtableMessageMenu"));
        QVERIFY(menu != nullptr);
        labels.clear();
        for (QAction* a : menu->actions()) {
            labels.append(a->text());
        }
        QCOMPARE(labels, (QStringList{QStringLiteral("Mute W4ABC"), QStringLiteral("Report"),
                                      QStringLiteral("Copy")}));
        QMenu* report = actionNamed(menu, QStringLiteral("Report"))->menu();
        actionNamed(report, QStringLiteral("Spam"))->trigger();
        QVERIFY(p.link.calls.contains(QStringLiteral("report message 2 spam")));
        actionNamed(menu, QStringLiteral("Copy"))->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("theirs"));
        actionNamed(menu, QStringLiteral("Mute W4ABC"))->trigger();
        QVERIFY(p.link.calls.contains(QStringLiteral("mute W4ABC")));
        menu->close();
        settle();

        // A reading's menu reports false readings about that station.
        QWidget* row = nullptr;
        for (QWidget* r : panel.findChildren<QWidget*>(QStringLiteral("roundtableReadingRow"))) {
            if (r->property("callsign").toString() == QLatin1String("N3XYZ")) {
                row = r;
            }
        }
        QVERIFY(row != nullptr);
        emit row->customContextMenuRequested(QPoint(2, 2));
        menu = openMenu(QStringLiteral("roundtableReadingMenu"));
        QVERIFY(menu != nullptr);
        actionNamed(menu, QStringLiteral("Report false readings"))->trigger();
        QVERIFY(p.link.calls.contains(QStringLiteral("report station %1/b falseReadings").arg(kRoom)));
        menu->close();
        settle();
    }

    void mutedMessagesFold()
    {
        AppSettings::instance().setValue(QStringLiteral("Roundtable/MuteList"), QStringLiteral("W4ABC"));
        Posed p;
        p.model.applyRoom(talkingRoom(QJsonArray{
            message(2, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("loud"))}));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1400);
        panel.followSlice(0);
        panel.show();
        settle();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableMutedFold")),
                 QStringList{QStringLiteral("Message from W4ABC hidden, you muted them.")});
        QVERIFY(!texts(&panel, QStringLiteral("roundtableMessageText")).contains(QStringLiteral("loud")));
        child<QPushButton>(&panel, QStringLiteral("roundtableShowMutedBtn"))->click();
        settle();
        QVERIFY(texts(&panel, QStringLiteral("roundtableMessageText")).contains(QStringLiteral("loud")));
    }

    void newPillAndFollowAtTheEnd()
    {
        AppSettings::instance().setValue(QStringLiteral("Roundtable/PanelOpensOn"), QStringLiteral("Messages"));
        Posed p;
        QJsonArray many;
        for (int i = 1; i <= 30; ++i) {
            many.append(message(i, QStringLiteral("a"), QStringLiteral("W4ABC"),
                                QStringLiteral("line %1").arg(i)));
        }
        p.model.applyRoom(talkingRoom(many));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 600);
        panel.followSlice(0);
        panel.show();
        settle();
        QScrollBar* bar = child<QScrollArea>(&panel, QStringLiteral("roundtableScroll"))->verticalScrollBar();
        // Opens on the messages: at the newest.
        QVERIFY(bar->maximum() > 0);
        QCOMPARE(bar->value(), bar->maximum());
        auto* pill = child<QPushButton>(&panel, QStringLiteral("roundtableNewPill"));

        // At the end a new message is followed; no pill.
        p.model.applyRoomDelta(QJsonObject{
            {QStringLiteral("room"), kRoom},
            {QStringLiteral("messages"),
             QJsonArray{message(31, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("next"))}}});
        settle();
        QCOMPARE(bar->value(), bar->maximum());
        QVERIFY(!pill->isVisible());

        // Scrolled up, arrivals wait under an "N new" pill.
        bar->triggerAction(QAbstractSlider::SliderToMinimum);
        settle();
        for (int i = 32; i <= 33; ++i) {
            p.model.applyRoomDelta(QJsonObject{
                {QStringLiteral("room"), kRoom},
                {QStringLiteral("messages"),
                 QJsonArray{message(i, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("more"))}}});
        }
        settle();
        QCOMPARE(bar->value(), 0);
        QVERIFY(pill->isVisible());
        QCOMPARE(pill->text(), QStringLiteral("2 new ↓"));
        pill->click();
        settle();
        QCOMPARE(bar->value(), bar->maximum());
        QVERIFY(!pill->isVisible());
    }

    void mentionPillFirstAndReadAfterTwoSeconds()
    {
        AppSettings::instance().setValue(QStringLiteral("Roundtable/PanelOpensOn"), QStringLiteral("Readings"));
        Posed p;
        QJsonArray many;
        for (int i = 1; i <= 30; ++i) {
            many.append(message(i, QStringLiteral("a"), QStringLiteral("W4ABC"),
                                i == 25 ? QStringLiteral("KG4VCF are you there?") : QStringLiteral("chat %1").arg(i)));
        }
        p.model.applyRoom(talkingRoom(many));
        p.state.applyStationValue("mentionCounts", QStringLiteral("{\"%1\":1}").arg(kRoom));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 500);
        panel.followSlice(0);
        panel.show();
        settle();
        auto* mention = child<QPushButton>(&panel, QStringLiteral("roundtableMentionPill"));
        QVERIFY(mention->isVisible());
        QCOMPARE(mention->text(), QStringLiteral("@1 A message names you ↓"));
        mention->click();
        settle();
        panel.refreshNow();
        QVERIFY(!p.link.calls.join(QLatin1Char(',')).contains(QStringLiteral("markMentionRead")));
        g_now += 2100;
        panel.refreshNow();
        QVERIFY(p.link.calls.contains(QStringLiteral("markMentionRead %1").arg(g_now - 2100 - 60000 + 25)));
    }

    void emptySliceOffersRooms()
    {
        Posed p;
        p.state.applyStationValue("sliceRooms", QStringLiteral("{}"));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 700);
        panel.followSlice(0);
        panel.setSliceFrequency(7200000);
        panel.show();
        settle();
        QVERIFY(panel.roomId().isEmpty());
        QCOMPARE(texts(&panel, QStringLiteral("roundtableEmptyTitle")),
                 QStringList{QStringLiteral("Nobody on Roundtable is tuned here.")});
        auto rooms = panel.findChildren<QPushButton*>(QStringLiteral("roundtableEmptyRoom"));
        QCOMPARE(rooms.size(), 2);
        QVERIFY(!child<QWidget>(&panel, QStringLiteral("roundtableFooter"))->isVisible());
        rooms.at(1)->click();
        QCOMPARE(panel.roomId(), kOther);
        QVERIFY(panel.isReading());
    }

    void keyboardDocked()
    {
        Posed p;
        p.model.applyRoom(talkingRoom(QJsonArray{
            message(1, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("one")),
            message(2, QStringLiteral("b"), QStringLiteral("N3XYZ"), QStringLiteral("two"))}));
        p.model.applyRoom(roomOf(kOther, {}, {},
                                 QJsonArray{message(5, QStringLiteral("x"), QStringLiteral("AB1CD"),
                                                    QStringLiteral("elsewhere"))}));
        QWidget window;
        auto* before = new QPushButton(QStringLiteral("before"), &window);
        auto* panel = new RoundtablePanel(&p.model, &window);
        auto* after = new QPushButton(QStringLiteral("after"), &window);
        auto* layout = new QVBoxLayout(&window);
        layout->addWidget(before);
        layout->addWidget(panel);
        layout->addWidget(after);
        window.resize(340, 1500);
        panel->followSlice(0);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        before->setFocus(Qt::OtherFocusReason);
        settle();

        // Tab moves in: onto the first earlier turn.
        QTest::keyClick(before, Qt::Key_Tab);
        settle();
        QWidget* focused = QApplication::focusWidget();
        QVERIFY(focused != nullptr);
        QCOMPARE(focused->property("navKey").toString(), QStringLiteral("t:20"));
        // Up and down move.
        QTest::keyClick(focused, Qt::Key_Down);
        QCOMPARE(QApplication::focusWidget()->property("navKey").toString(), QStringLiteral("t:10"));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
        QCOMPARE(QApplication::focusWidget()->property("navKey").toString(), QStringLiteral("m:1"));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Up);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Up);
        // Enter on an unknown turn opens Name.
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
        settle();
        auto* input = panel->findChild<QLineEdit*>(QStringLiteral("roundtableNameInput"));
        QVERIFY(input != nullptr && input->hasFocus());
        QTest::keyClick(input, Qt::Key_Escape);
        settle();
        QVERIFY(panel->findChild<QLineEdit*>(QStringLiteral("roundtableNameInput")) == nullptr);
        QCOMPARE(QApplication::focusWidget()->property("navKey").toString(), QStringLiteral("t:20"));

        // Escape while reading goes back to the slice; else it leaves.
        panel->readRoom(kOther);
        settle();
        panel->focusFirstItem();
        QWidget* inside = QApplication::focusWidget();
        QVERIFY(inside != nullptr && panel->isAncestorOf(inside));
        QTest::keyClick(inside, Qt::Key_Escape);
        settle();
        QVERIFY(!panel->isReading());
        QVERIFY(panel->isAncestorOf(QApplication::focusWidget()));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        settle();
        QCOMPARE(QApplication::focusWidget(), after);
    }

    void escapeWhilePoppedStepsBack()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1200);
        panel.followSlice(0);
        panel.setPopped(true);
        panel.show();
        settle();
        QVERIFY(!child<QToolButton>(&panel, QStringLiteral("roundtablePopBtn"))->isVisible());
        QSignalSpy back(&panel, &RoundtablePanel::stepBackRequested);
        auto* box = child<QPlainTextEdit>(&panel, QStringLiteral("roundtableBox"));
        QTest::keyClick(box, Qt::Key_Escape);
        QCOMPARE(back.size(), 1);
    }

    void splitMovesMessagesAndBox()
    {
        Posed p;
        p.model.applyRoom(talkingRoom(QJsonArray{
            message(1, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("one"))}));
        RoundtablePanel panel(&p.model);
        panel.resize(900, 600);
        panel.followSlice(0);
        panel.show();
        settle();
        panel.setSplit(true);
        settle();
        auto* column = child<QWidget>(&panel, QStringLiteral("roundtableMessagesColumn"));
        QVERIFY(column->isVisible());
        QCOMPARE(column->width(), RoundtablePanel::kMessagesColumnWidth);
        QVERIFY(column->isAncestorOf(child<QWidget>(&panel, QStringLiteral("roundtableMessages"))));
        QVERIFY(column->isAncestorOf(child<QWidget>(&panel, QStringLiteral("roundtableFooter"))));
        panel.setSplit(false);
        settle();
        QVERIFY(!column->isVisible());
        QVERIFY(child<QWidget>(&panel, QStringLiteral("roundtableContent"))
                    ->isAncestorOf(child<QWidget>(&panel, QStringLiteral("roundtableMessages"))));
    }

    void appletFollowsTheActiveSliceAndTunes()
    {
        Posed p;
        p.state.applyStationValue("sliceRooms",
                                  QStringLiteral("{\"0\":\"%1\",\"1\":\"%2\"}").arg(kRoom, kOther));
        p.model.applyRoom(talkingRoom());
        p.model.applyRoom(roomOf(kOther, {}, {}, {}));
        RadioModel radio;
        radio.addSlice();
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* first = radio.activeSlice();
        QVERIFY(first != nullptr);
        first->setDspMode(DSPMode::USB);
        first->setFrequency(7185000.0);
        RoundtableApplet applet(&radio, &p.model, nullptr);
        applet.resize(340, 900);
        applet.show();
        settle();
        RoundtablePanel* panel = applet.panel();
        QCOMPARE(panel->followedSlice(), first->sliceIndex());
        QCOMPARE(panel->roomId(), kRoom);
        radio.setActiveSlice(1);
        QCOMPARE(panel->roomId(), kOther);
        radio.setActiveSlice(0);
        QCOMPARE(panel->roomId(), kRoom);

        // Tune A here: the active slice to the room, in its family's mode.
        panel->readRoom(kOther);
        settle();
        child<QPushButton>(panel, QStringLiteral("roundtableTuneHereBtn"))->click();
        QCOMPARE(std::llround(first->frequency()), 7249000LL);
        QCOMPARE(first->dspMode(), DSPMode::LSB);
        QVERIFY(!panel->isReading());

        // Tuning anywhere goes back to following the slice.
        panel->readRoom(kOther);
        QVERIFY(panel->isReading());
        first->setFrequency(14200000.0);
        QVERIFY(!panel->isReading());
    }

    void appletArchiveOpensTheSignInLink()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtableApplet applet(nullptr, &p.model, nullptr);
        QList<QUrl> opened;
        applet.setUrlOpener([&opened](const QUrl& url) { opened.append(url); });
        applet.resize(340, 900);
        applet.show();
        settle();
        const QUrl link(QStringLiteral("https://roundtable.example/signin?t=1"));
        // A link nobody asked for opens nothing.
        p.model.applyWebLink(link);
        QVERIFY(opened.isEmpty());
        applet.panel()->findChild<QToolButton*>(QStringLiteral("roundtableMenuBtn"))->click();
        QMenu* menu = openMenu(QStringLiteral("roundtablePanelMenu"));
        QVERIFY(menu != nullptr);
        actionNamed(menu, QStringLiteral("Archive"))->trigger();
        menu->close();
        QVERIFY(p.link.calls.contains(QStringLiteral("webSignIn")));
        p.model.applyWebLink(link);
        QCOMPARE(opened, QList<QUrl>{link});
        p.model.applyWebLink(link);
        QCOMPARE(opened.size(), 1);
        settle();
    }

    void windowShapesByWidth()
    {
        QCOMPARE(RoundtableWindow::shapeFor(599), RoundtableWindow::Shape::OneColumn);
        QCOMPARE(RoundtableWindow::shapeFor(600), RoundtableWindow::Shape::TwoColumns);
        QCOMPARE(RoundtableWindow::shapeFor(980), RoundtableWindow::Shape::TwoColumns);
        QCOMPARE(RoundtableWindow::shapeFor(981), RoundtableWindow::Shape::ThreeColumns);

        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtableWindow window(&p.model);
        auto* panel = new RoundtablePanel(&p.model);
        panel->followSlice(0);
        window.takePanel(panel);
        QVERIFY(panel->isPopped());
        window.show();
        auto* list = child<QWidget>(&window, QStringLiteral("roundtableRoomListColumn"));
        auto* rooms = child<QListWidget>(&window, QStringLiteral("roundtableRoomList"));

        window.resize(560, 600);
        settle();
        QCOMPARE(window.shape(), RoundtableWindow::Shape::OneColumn);
        QVERIFY(!window.isListShown());
        QVERIFY(panel->isVisible());
        QVERIFY(!panel->isSplit());

        window.resize(800, 600);
        settle();
        QCOMPARE(window.shape(), RoundtableWindow::Shape::TwoColumns);
        QVERIFY(window.isListShown());
        QCOMPARE(list->width(), RoundtableWindow::kRoomListWidth);
        QVERIFY(panel->isVisible());
        QVERIFY(!panel->isSplit());

        window.resize(1200, 600);
        settle();
        QCOMPARE(window.shape(), RoundtableWindow::Shape::ThreeColumns);
        QVERIFY(window.isListShown());
        QCOMPARE(list->width(), RoundtableWindow::kRoomListWidth);
        QVERIFY(panel->isSplit());

        // The list: the "N on air" count, a band heading that is not a
        // room, and the room the slice is in marked.
        QCOMPARE(child<QLabel>(&window, QStringLiteral("roundtableOnAir"))->text(),
                 QStringLiteral("8 on air"));
        QCOMPARE(rooms->count(), 3);
        QVERIFY(!(rooms->item(0)->flags() & Qt::ItemIsSelectable));
        QVERIFY(rooms->item(1)->text().startsWith(QStringLiteral("7.185.000  LSB  ·  YOU · A")));
        QVERIFY(rooms->item(2)->text().startsWith(QStringLiteral("7.249.000  LSB")));
    }

    void windowWidePresetAndSavedGeometry()
    {
        Posed p;
        QWidget owner;
        owner.resize(400, 300);
        {
            RoundtableWindow window(&p.model, &owner);
            QVERIFY(window.isWindow());
            // The first time: the Wide preset.
            window.showAtSavedGeometry();
            settle();
            QCOMPARE(window.size(), QSize(RoundtableWindow::kWideWidth, RoundtableWindow::kWideHeight));
            window.resize(700, 480);
            window.move(40, 60);
            settle();
            QVERIFY(!AppSettings::instance().value(QLatin1String(RoundtableWindow::kGeometryKey)).toString().isEmpty());
            window.close();
        }
        RoundtableWindow again(&p.model, &owner);
        again.showAtSavedGeometry();
        settle();
        QCOMPARE(again.size(), QSize(700, 480));
        QCOMPARE(again.pos(), QPoint(40, 60));
        again.applyWidePreset();
        settle();
        QCOMPARE(again.size(), QSize(RoundtableWindow::kWideWidth, RoundtableWindow::kWideHeight));
    }

    void windowCloseBoxDocksAndCloseFromCodeDoesNot()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        QWidget top;
        auto* layout = new QVBoxLayout(&top);
        auto* applet = new RoundtableApplet(nullptr, &p.model, &top);
        layout->addWidget(applet);
        top.resize(340, 900);
        top.show();
        settle();
        RoundtablePanel* panel = applet->panel();
        QSignalSpy docked(applet, &AppletWidget::floatingWindowDocked);

        // No applet panel above it: Pop out opens the window directly.
        emit panel->popOutRequested();
        settle();
        RoundtableWindow* window = applet->floatingWindow();
        QVERIFY(window != nullptr);
        QVERIFY(window->isVisible());
        QVERIFY(applet->isPoppedOut());
        QVERIFY(window->isAncestorOf(panel));
        QVERIFY(panel->isPopped());
        QVERIFY(child<QWidget>(applet, QStringLiteral("roundtablePoppedNote"))->isVisible());

        // A close from code (the quit sweep) only hides it.
        window->close();
        settle();
        QVERIFY(!window->isVisible());
        QVERIFY(applet->isPoppedOut());
        QCOMPARE(docked.size(), 0);

        // The close box docks it back.
        emit panel->popOutRequested();
        settle();
        QVERIFY(window->isVisible());
        QVERIFY(window->windowHandle() != nullptr);
        QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::SynchronousDelivery>(
            window->windowHandle());
        settle();
        QVERIFY(!window->isVisible());
        QVERIFY(!applet->isPoppedOut());
        QCOMPARE(docked.size(), 1);
        QCOMPARE(panel->parentWidget(), applet);
        QVERIFY(panel->isVisible());
        QVERIFY(!panel->isPopped());
        QVERIFY(!panel->isSplit());
        QVERIFY(!child<QWidget>(applet, QStringLiteral("roundtablePoppedNote"))->isVisible());
    }

    void putItBackThroughTheAppletPanel()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        AppletPanelWidget host;
        auto* applet = new RoundtableApplet(nullptr, &p.model, nullptr);
        host.addApplet(applet);
        host.resize(340, 900);
        host.show();
        settle();
        RoundtablePanel* panel = applet->panel();

        child<QToolButton>(panel, QStringLiteral("roundtablePopBtn"))->click();
        settle();
        QVERIFY(host.isAppletFloating(applet));
        QVERIFY(applet->isPoppedOut());
        QVERIFY(applet->floatingWindow()->isVisible());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletroundtableFloating")).toString(),
                 QStringLiteral("True"));

        child<QPushButton>(applet, QStringLiteral("roundtablePutBackBtn"))->click();
        settle();
        QVERIFY(!host.isAppletFloating(applet));
        QVERIFY(!applet->isPoppedOut());
        QVERIFY(!applet->floatingWindow()->isVisible());
        QCOMPARE(panel->parentWidget(), applet);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletroundtableFloating")).toString(),
                 QStringLiteral("False"));

        // Again, and the close box: the panel knows it is docked.
        host.floatApplet(applet);
        settle();
        QVERIFY(host.isAppletFloating(applet));
        QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::SynchronousDelivery>(
            applet->floatingWindow()->windowHandle());
        settle();
        QVERIFY(!host.isAppletFloating(applet));
        QVERIFY(!applet->isPoppedOut());
    }

    void windowKeyboard()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        p.model.applyRoom(roomOf(kOther, {}, {},
                                 QJsonArray{message(5, QStringLiteral("x"), QStringLiteral("AB1CD"),
                                                    QStringLiteral("elsewhere"))}));
        RoundtableWindow window(&p.model);
        auto* panel = new RoundtablePanel(&p.model);
        panel->followSlice(0);
        window.takePanel(panel);
        window.resize(1200, 600);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        settle();
        auto* search = child<QLineEdit>(&window, QStringLiteral("roundtableRoomSearch"));
        auto* rooms = child<QListWidget>(&window, QStringLiteral("roundtableRoomList"));

        // / searches.
        QTest::keyClick(&window, Qt::Key_Slash);
        QVERIFY(search->hasFocus());
        QTest::keyClicks(search, QStringLiteral("7.249"));
        settle();
        QCOMPARE(rooms->count(), 2);
        // Down moves into the list, Enter goes into the room.
        QTest::keyClick(search, Qt::Key_Down);
        QVERIFY(rooms->hasFocus());
        QCOMPARE(rooms->currentItem()->text().left(9), QStringLiteral("7.249.000"));
        QTest::keyClick(rooms, Qt::Key_Return);
        settle();
        QCOMPARE(panel->roomId(), kOther);
        QVERIFY(panel->isReading());
        // Escape in the list clears the search.
        rooms->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(rooms, Qt::Key_Escape);
        settle();
        QVERIFY(search->text().isEmpty());
        QCOMPARE(rooms->count(), 3);

        // One column: Enter shows the room in place of the list, Escape in
        // the room goes back to the list.
        panel->backToSlice();
        window.resize(560, 600);
        settle();
        QVERIFY(!window.isListShown());
        QTest::keyClick(&window, Qt::Key_Slash);
        settle();
        QVERIFY(window.isListShown());
        QVERIFY(search->hasFocus());
        QTest::keyClick(search, Qt::Key_Return);
        settle();
        // The list kept its place: Enter opens the room picked before.
        QVERIFY(!window.isListShown());
        QCOMPARE(panel->roomId(), kOther);
        auto* box = child<QPlainTextEdit>(panel, QStringLiteral("roundtableBox"));
        box->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(box, Qt::Key_Escape);
        settle();
        QVERIFY(window.isListShown());
        QVERIFY(rooms->hasFocus());
    }
};

QTEST_MAIN(TstRoundtablePanel)
#include "tst_roundtable_panel.moc"
