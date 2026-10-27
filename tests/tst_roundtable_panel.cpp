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
//   2026-10-24: Roundtable plan Task 15: fitsTheRealAppletColumn, every
//               scene at the applet column's 260 px minimum, a 280 px
//               viewport and the design's 340 px, nothing past the
//               viewport and long callsigns elided with a tooltip.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25: takesTheWorkspaceLook: with the look on every color the
//               panel writes is a look role (or the slice's own color on
//               its letter badge), with it off the classic one; a slice B
//               room shows B in B's color. The stale age is the look's warn
//               status. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-25: aMissingOwnReadingShowsNone: hidden, your own reading of
//               -400 (a cleared slice) shows "none", not S0. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25: sectionsInOrder takes its two-line width from the header's
//               measured parts, not 340 px; aTenCharacterFrequencyFits: a
//               14.212.000 room fits 260-298 px, the mode moving under the
//               frequency when the row does not fit. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-26: aRoomsRefreshLeavesThePickedRowAlone: a rooms refresh of
//               the same rooms tells accessibility nothing about the pop-out
//               list's rows or pick (JJ's log on PR 657).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: managedPopOutIsThePopOutContainer replaces
//               managedPopOutOpensTheRoundtableWindow: the container's Pop
//               Out gives Roundtable a pop-out container like every applet;
//               its own window opens from its own arrow (JJ, PR 657).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: putItBackThroughTheAppletPanel and emptySliceOffersRooms:
//               the own-window button's tooltip and accessible name are
//               "Open in its own window", and its click still opens the
//               own window. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-26: aRoomsRefreshLeavesThePickedRowAlone turns accessibility on
//               through the platform when QAccessible::setActive does not
//               (Qt 6.8, Linux CI) and checks it is on, so both halves run
//               there too. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <cmath>
#include <functional>

#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonObject>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUrl>
#include <QListWidget>
#include <QVBoxLayout>
// The private header brings in macOS's MacTypes.h and its `Style` typedef,
// so the look's Style constants below are named NereusSDR::Style.
#include <private/qguiapplication_p.h>
#include <qpa/qplatformaccessibility.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qwindowsysteminterface.h>

#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "core/WdspTypes.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/ModMonitorApplet.h"
#include "gui/applets/RoundtableApplet.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "gui/StyleConstants.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
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

// The header row's parts as fitCount measures them: everything in the row
// but the count (margins and spacings in), the count on one line and on two,
// and how much of the panel's width is not the room the row has.
struct HeaderFit {
    int row = 0;
    int spacing = 0;
    int oneLine = 0;
    int twoLines = 0;
    int outside = 0;
};

HeaderFit headerFit(QWidget* panel, const QString& oneLine, const QString& twoLines, bool withTag = true)
{
    HeaderFit fit;
    auto* body = panel->findChild<QWidget*>(QStringLiteral("roundtableHeaderBody"));
    auto* freq = panel->findChild<QLabel*>(QStringLiteral("roundtableFrequency"));
    QHBoxLayout* row = nullptr;
    for (int i = 0; i < body->layout()->count(); ++i) {
        auto* candidate = qobject_cast<QHBoxLayout*>(body->layout()->itemAt(i)->layout());
        if (candidate != nullptr && candidate->indexOf(freq) >= 0) {
            row = candidate;
        }
    }
    fit.spacing = row->spacing();
    const QMargins margins = body->layout()->contentsMargins();
    fit.row = margins.left() + margins.right();
    int parts = 0;
    for (const QString& name : {QStringLiteral("roundtableTag"), QStringLiteral("roundtableFrequency"),
                                QStringLiteral("roundtableMode"), QStringLiteral("roundtablePopBtn"),
                                QStringLiteral("roundtableMenuBtn")}) {
        for (QWidget* w : body->findChildren<QWidget*>(name)) {
            const bool tag = name == QLatin1String("roundtableTag");
            if (row->indexOf(w) >= 0 && (tag ? withTag && w->isVisibleTo(body) : true)) {
                fit.row += w->sizeHint().width();
                ++parts;
            }
        }
    }
    fit.row += (parts - 1) * fit.spacing;
    QLabel* count = nullptr;
    for (QLabel* label : panel->findChildren<QLabel*>(QStringLiteral("roundtableCount"))) {
        if (label->isVisibleTo(panel)) {
            count = label;
        }
    }
    count->ensurePolished();
    const QFontMetrics metrics(count->font());
    fit.oneLine = metrics.horizontalAdvance(oneLine);
    for (const QString& line : twoLines.split(QLatin1Char('\n'))) {
        fit.twoLines = std::max(fit.twoLines, metrics.horizontalAdvance(line));
    }
    auto* scroll = panel->findChild<QScrollArea*>(QStringLiteral("roundtableScroll"));
    const int viewport = scroll->viewport()->width();
    const int room = scroll->verticalScrollBar()->isVisible()
                      ? viewport
                      : viewport - panel->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    fit.outside = panel->width() - room;
    return fit;
}

void settle()
{
    for (int i = 0; i < 4; ++i) {
        QCoreApplication::processEvents();
    }
}

// Turns the platform's accessibility on or off. Qt 6.11's
// QAccessible::setActive does that; Qt 6.8's (Linux CI) only tells the
// activation observers and leaves QAccessible::isActive() false, so no
// item view sends an event at all. There the platform's own switch is
// set, as Qt's tst_qaccessibility does (tst_cat_remote_setup likewise).
void setAccessibilityActive(bool active)
{
    QAccessible::setActive(active);
    if (QAccessible::isActive() == active) {
        return;
    }
    if (QPlatformAccessibility* platform = QGuiApplicationPrivate::platformIntegration()->accessibility()) {
        platform->setActive(active);
    }
}

// The real MainWindow runs the panel in a managed workspace: a
// container hosts the applet and every Pop Out goes through the
// arrange controller.
struct ManagedWorkspace {
    QTemporaryDir dir;
    AppSettings settings{dir.filePath(QStringLiteral("settings.xml"))};
    ContainerWorkspaceStore store{settings};
    AppletPanelWidget host;
    QWidget pane;
    QSplitter splitter;
    ContainerContentRegistry registry;
    std::unique_ptr<ContainerManager> manager;
    RoundtableApplet* roundtable = nullptr;
    ModMonitorApplet* monitor = nullptr;
    QString roundtableEntry;
    QString monitorEntry;

    explicit ManagedWorkspace(RoundtableModel* model)
    {
        host.setManagedWorkspace(true);
        roundtable = new RoundtableApplet(nullptr, model, nullptr);
        monitor = new ModMonitorApplet(nullptr);
        host.addApplet(roundtable);
        host.addApplet(monitor);
        registry.attachSingleton(QStringLiteral("applet:roundtable"), roundtable);
        registry.attachSingleton(QStringLiteral("applet:mod_monitor"), monitor);
        manager = std::make_unique<ContainerManager>(&pane, &splitter);
        manager->setWorkspaceAdapter(&store, &registry);
        host.setArrangeController(manager->arrangeController());
        WorkspaceDocument d;
        d.mainContainerId = QStringLiteral("main");
        ContainerDocument c;
        c.id = QStringLiteral("main");
        c.layout = ContentLayout::VerticalStack;
        const ContentEntry rt = registry.makeEntry(QStringLiteral("applet:roundtable"));
        const ContentEntry mm = registry.makeEntry(QStringLiteral("applet:mod_monitor"));
        roundtableEntry = rt.id;
        monitorEntry = mm.id;
        c.contents = {rt, mm};
        d.containers = {c};
        ok = manager->commitWorkspace(d, 0).status == CommitStatus::Saved;
        pane.resize(340, 900);
        pane.show();
    }

    ~ManagedWorkspace()
    {
        host.setArrangeController(nullptr);
    }

    bool ok = false;
};

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
                                                        "nereus.freedv.*.debug=false\n"
                                                        "nereus.container.debug=false"));
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
        // Where the row fits the count on two lines but not on one, it takes
        // two; wider, one. The width comes from the header's measured parts,
        // which differ by platform font.
        const HeaderFit fit = headerFit(&panel, QStringLiteral("5 in the room · 1 away"),
                                        QStringLiteral("5 in the room\n1 away"));
        QVERIFY2(fit.twoLines < fit.oneLine, qPrintable(QStringLiteral("%1 %2").arg(fit.twoLines).arg(fit.oneLine)));
        panel.resize(fit.outside + fit.row + fit.spacing + (fit.twoLines + fit.oneLine) / 2, 1400);
        settle();
        settle();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableCount")),
                 QStringList{QStringLiteral("5 in the room\n1 away")});
        panel.resize(600, 1400);
        settle();
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
                QVERIFY(age->styleSheet().contains(
                    WorkspacePreview::statusColourName(NereusSDR::Style::kAmberText, "warn")));
                QCOMPARE(age->text(), QStringLiteral("20 s"));
            }
        }
    }

    void aMissingOwnReadingShowsNone()
    {
        // Hidden, your own reading is the NOT SENT row. A cleared slice
        // reads -400: no reading, "none" like every other row, not S0.
        Posed p;
        p.model.applyRoom(talkingRoom());
        p.setLink(RoundtableModel::LinkState::Hidden);
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1400);
        panel.followSlice(0);
        panel.show();
        settle();
        panel.setOwnReading(-400.0, g_now);
        settle();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableNotSentTag")), QStringList{QStringLiteral("NOT SENT")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingValue")),
                 (QStringList{QStringLiteral("S7"), QStringLiteral("S9"), QStringLiteral("S6"),
                              QStringLiteral("none")}));
        // A real reading of yours reads as one.
        panel.setOwnReading(-73.0, g_now);
        settle();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableReadingValue")),
                 (QStringList{QStringLiteral("S7"), QStringLiteral("S9"), QStringLiteral("S6"),
                              QStringLiteral("S9")}));
    }

    void takesTheWorkspaceLook()
    {
        struct LookOff {
            ~LookOff() { WorkspacePreview::setEnabledForTest(false); }
        } restore;
        const QString sliceB = QColor(static_cast<QRgb>(ControlRanges::sliceColour(1))).name();
        QSet<QString> palette;
        for (const auto& e : Workspace::WorkspacePalette::glassAndDepth().entries()) {
            palette.insert(QString::fromLatin1(e.hex));
        }
        // A slice letter badge is in its slice's color, a data color.
        for (const std::uint32_t rgb : ControlRanges::kSliceColours) {
            palette.insert(QColor(static_cast<QRgb>(rgb)).name());
        }
        static const QRegularExpression hex(QStringLiteral("#[0-9a-fA-F]{6}\\b"));
        for (const bool look : {true, false}) {
            WorkspacePreview::setEnabledForTest(look);
            Posed p;
            // Slice B in the room: B in B's color.
            p.state.applyStationValue("sliceRooms", QStringLiteral("{\"1\":\"%1\"}").arg(kRoom));
            p.model.applyRoom(talkingRoom(QJsonArray{
                message(1, QStringLiteral("a"), QStringLiteral("W4ABC"), QStringLiteral("good copy"))}));
            RoundtablePanel panel(&p.model);
            panel.resize(340, 1400);
            panel.followSlice(1);
            panel.show();
            settle();
            QCOMPARE(panel.roomId(), kRoom);
            auto* tag = child<QLabel>(&panel, QStringLiteral("roundtableTag"));
            QVERIFY(tag != nullptr);
            QCOMPARE(tag->text(), QStringLiteral("B"));
            QVERIFY2(tag->styleSheet().contains(QStringLiteral("background: %1").arg(sliceB)),
                     qPrintable(tag->styleSheet()));
            QList<QWidget*> widgets{&panel};
            widgets.append(panel.findChildren<QWidget*>());
            QStringList sheets;
            for (QWidget* w : std::as_const(widgets)) {
                sheets.append(w->styleSheet());
            }
            const QString all = sheets.join(QLatin1Char('\n'));
            if (look) {
                // No color of the panel's own outside the look's palette
                // and the slice colors.
                QStringList strays;
                for (QWidget* w : std::as_const(widgets)) {
                    for (auto it = hex.globalMatch(w->styleSheet()); it.hasNext();) {
                        const QString v = it.next().captured(0).toLower();
                        if (!palette.contains(v)) {
                            strays.append(QStringLiteral("%1 in %2").arg(v, w->styleSheet()));
                        }
                    }
                }
                strays.removeDuplicates();
                QVERIFY2(strays.isEmpty(), qPrintable(strays.join(QLatin1Char(' '))));
                // The look's accent, not the classic one.
                QVERIFY(all.contains(QStringLiteral("#4a7ba8")));
                QVERIFY(!all.contains(QLatin1String(NereusSDR::Style::kAccent)));
            } else {
                // The classic look: the classic accent, no workspace one.
                QVERIFY(all.contains(QLatin1String(NereusSDR::Style::kAccent)));
                QVERIFY(!all.contains(QStringLiteral("#4a7ba8")));
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
        // The mockup's hint: who you hear, then your reading as an S unit
        // once you have one.
        QCOMPARE(child<QLabel>(&panel, QStringLiteral("roundtableHint"))->text(),
                 QStringLiteral("Sent with your reading: hearing W1TLK"));
        QJsonObject withMine = talkingRoom();
        QJsonArray who = members();
        who[0] = member(QStringLiteral("me"), QStringLiteral("KG4VCF"), -1, reading(-73, 500), true);
        withMine.insert(QStringLiteral("members"), who);
        p.model.applyRoom(withMine);
        settle();
        QCOMPARE(child<QLabel>(&panel, QStringLiteral("roundtableHint"))->text(),
                 QStringLiteral("Sent with your reading: hearing W1TLK · S9"));
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

    void fitsTheRightColumnAndTheMessagesColumn()
    {
        // A muted fold, an AWAY tag and long text never widen the panel
        // past the 340 px right column or the 320 px messages column.
        AppSettings::instance().setValue(QStringLiteral("Roundtable/MuteList"), QStringLiteral("N3XYZ"));
        Posed p;
        p.model.applyRoom(talkingRoom(QJsonArray{
            message(1, QStringLiteral("b"), QStringLiteral("N3XYZ"), QStringLiteral("muted")),
            message(2, QStringLiteral("a"), QStringLiteral("W4ABC"),
                    QStringLiteral("Who was that on at the top of the hour? Strong but no ID."))}));
        RoundtablePanel panel(&p.model);
        panel.resize(340, 1400);
        panel.followSlice(0);
        panel.show();
        settle();
        auto* content = child<QWidget>(&panel, QStringLiteral("roundtableContent"));
        QVERIFY(content->width() <= 340);
        // Room for a vertical scroll bar too: a content minimum between the
        // two widths makes the scroll area flip between them.
        const int withBar = 340 - panel.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        QVERIFY2(content->minimumSizeHint().width() <= withBar,
                 qPrintable(QString::number(content->minimumSizeHint().width())));

        panel.resize(900, 700);
        panel.setSplit(true);
        settle();
        auto* scroll = child<QScrollArea>(&panel, QStringLiteral("roundtableMessagesScroll"));
        auto* messages = child<QWidget>(&panel, QStringLiteral("roundtableMessages"));
        QVERIFY(messages->width() <= scroll->viewport()->width());
        // The rows sit at the top of the column.
        QWidget* last = nullptr;
        for (QWidget* w : messages->findChildren<QWidget*>()) {
            if (w->property("navKey").toString() == QLatin1String("m:2")) {
                last = w;
            }
        }
        QVERIFY(last != nullptr);
        QVERIFY(last->mapTo(messages, QPoint(0, last->height())).y() < 300);
    }

    void readingAnotherRoomFitsTheViewport()
    {
        // Reading another room at the docked width: the read bar (spec
        // 11.1, "Reading 7.249 · Tune A here · Back to my slice"), the
        // two-line count, every readings column and the AWAY note all sit
        // inside the viewport.
        Posed p;
        p.model.applyRoom(talkingRoom());
        QJsonObject other = talkingRoom(QJsonArray{
            message(9, QStringLiteral("b"), QStringLiteral("N3XYZ"), QStringLiteral("Anyone up for a contact on 40?"))});
        other.insert(QStringLiteral("id"), kOther);
        p.model.applyRoom(other);
        RoundtablePanel panel(&p.model);
        panel.resize(340, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        panel.readRoom(kOther);
        settle();
        QVERIFY(panel.isReading());
        auto* scroll = child<QScrollArea>(&panel, QStringLiteral("roundtableScroll"));
        auto* content = child<QWidget>(&panel, QStringLiteral("roundtableContent"));
        const int viewport = scroll->viewport()->width();
        // What asks for more than the viewport, for the failure message.
        QStringList asking;
        for (QWidget* w : content->findChildren<QWidget*>()) {
            const int hint = w->layout() != nullptr ? w->layout()->minimumSize().width()
                                                    : w->minimumSizeHint().width();
            if (w->isVisibleTo(content) && std::max(hint, w->minimumWidth()) > viewport) {
                asking.append(QStringLiteral("%1 %2 %3").arg(QString::fromLatin1(w->metaObject()->className()),
                                                              w->objectName()).arg(std::max(hint, w->minimumWidth())));
            }
        }
        QVERIFY2(content->width() <= viewport,
                 qPrintable(QStringLiteral("content %1 in %2: %3").arg(content->width()).arg(viewport)
                                .arg(asking.join(QStringLiteral("; ")))));
        QStringList wide;
        for (QWidget* w : content->findChildren<QWidget*>()) {
            if (!w->isVisibleTo(content)) {
                continue;
            }
            const int right = w->mapTo(content, QPoint(w->width(), 0)).x();
            if (right > viewport) {
                wide.append(QStringLiteral("%1 %2 ends at %3").arg(QString::fromLatin1(w->metaObject()->className()),
                                                                    w->objectName()).arg(right));
            }
        }
        QVERIFY2(wide.isEmpty(), qPrintable(wide.join(QStringLiteral("; "))));
        // Room for a scroll bar too, so the scroll area cannot flip.
        const int withBar = 340 - panel.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        QVERIFY2(content->minimumSizeHint().width() <= withBar,
                 qPrintable(QString::number(content->minimumSizeHint().width())));
        // Where the row fits the count on two lines and no more, NOT TUNED
        // HERE moves to its own line and the count takes two. The width
        // comes from the header's measured parts, which differ by font.
        const HeaderFit fit = headerFit(&panel, QStringLiteral("5 in the room · 1 away"),
                                        QStringLiteral("5 in the room\n1 away"), false);
        panel.resize(fit.outside + fit.row + fit.spacing + fit.twoLines + 2, 900);
        settle();
        settle();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTag")), QStringList{QStringLiteral("NOT TUNED HERE")});
        for (QLabel* tag : panel.findChildren<QLabel*>(QStringLiteral("roundtableTag"))) {
            if (tag->isVisibleTo(&panel)) {
                QVERIFY(yIn(&panel, tag) + tag->height()
                        <= yIn(&panel, child<QLabel>(&panel, QStringLiteral("roundtableFrequency"))));
            }
        }
        QCOMPARE(texts(&panel, QStringLiteral("roundtableCount")),
                 QStringList{QStringLiteral("5 in the room\n1 away")});
        // Wide, the tag is back in the row and the count on one line.
        panel.resize(700, 900);
        settle();
        QCOMPARE(texts(&panel, QStringLiteral("roundtableCount")),
                 QStringList{QStringLiteral("5 in the room · 1 away")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTag")), QStringList{QStringLiteral("NOT TUNED HERE")});
        QLabel* inRow = nullptr;
        for (QLabel* tag : panel.findChildren<QLabel*>(QStringLiteral("roundtableTag"))) {
            if (tag->isVisibleTo(&panel)) {
                inRow = tag;
            }
        }
        QVERIFY(inRow != nullptr);
        QCOMPARE(inRow->mapTo(&panel, QPoint(0, 0)).y(),
                 child<QLabel>(&panel, QStringLiteral("roundtableFrequency"))->mapTo(&panel, QPoint(0, 0)).y()
                     + (child<QLabel>(&panel, QStringLiteral("roundtableFrequency"))->height() - inRow->height()) / 2);
        panel.resize(340, 900);
        settle();
        // The bar's three parts in the spec's order, with its separators.
        auto* bar = child<QWidget>(&panel, QStringLiteral("roundtableReadBar"));
        QVERIFY(bar->isVisible());
        // In layout order: the bar may wrap onto two lines.
        QStringList parts;
        std::function<void(QLayout*)> walk = [&parts, &walk, bar](QLayout* layout) {
            for (int i = 0; i < layout->count(); ++i) {
                QLayoutItem* item = layout->itemAt(i);
                if (item->layout() != nullptr) {
                    walk(item->layout());
                } else if (QWidget* w = item->widget(); w != nullptr && w->isVisibleTo(bar)) {
                    if (auto* label = qobject_cast<QLabel*>(w)) {
                        parts.append(label->text());
                    } else if (auto* button = qobject_cast<QPushButton*>(w)) {
                        parts.append(button->text());
                    } else if (w->layout() != nullptr) {
                        walk(w->layout());
                    }
                }
            }
        };
        walk(bar->layout());
        QCOMPARE(parts.join(QLatin1Char(' ')), QStringLiteral("Reading 7.249 · Tune A here · Back to my slice"));
    }

    void fitsTheRealAppletColumn_data()
    {
        QTest::addColumn<int>("scene");
        QTest::addColumn<int>("width");
        const QStringList scenes{QStringLiteral("talking"),  QStringLiteral("unknown"),
                                 QStringLiteral("doubled"),  QStringLiteral("keyed"),
                                 QStringLiteral("quiet"),    QStringLiteral("reading"),
                                 QStringLiteral("hidden"),   QStringLiteral("naming"),
                                 QStringLiteral("muted"),    QStringLiteral("mention")};
        // 260: the applet column's minimum (Style::kAppletPanelW). 298: the
        // column at 1512x869 (a 280 px viewport inside the applet frame),
        // 280 that viewport itself. 340: the design's width.
        for (int width : {260, 280, 298, 340}) {
            for (int i = 0; i < scenes.size(); ++i) {
                QTest::newRow(qPrintable(QStringLiteral("%1 %2").arg(scenes.at(i)).arg(width))) << i << width;
            }
        }
    }

    void fitsTheRealAppletColumn()
    {
        // Every scene in the real applet column: no widget runs past the
        // viewport, no label is clipped, and a long callsign elides with the
        // whole one in a tooltip rather than widening the panel.
        QFETCH(int, scene);
        QFETCH(int, width);
        Posed p;
        QJsonArray who = members();
        who.append(member(QStringLiteral("l"), QStringLiteral("KH6/W1LONGCALL"), 4800, reading(-79, 1500)));
        const QJsonArray chat{
            message(1, QStringLiteral("b"), QStringLiteral("N3XYZ"), QStringLiteral("muted")),
            message(2, QStringLiteral("a"), QStringLiteral("W4ABC"),
                    QStringLiteral("Who was that on at the top of the hour? Strong but no ID.")),
            message(3, QStringLiteral("l"), QStringLiteral("KH6/W1LONGCALL"),
                    QStringLiteral("KG4VCF are you there? Good signal into Hawaii tonight."))};
        QJsonObject room = talkingRoom(chat);
        room.insert(QStringLiteral("members"), who);
        const QJsonArray pair{talker(QStringLiteral("t"), QStringLiteral("W1TLK")),
                              talker(QStringLiteral("l"), QStringLiteral("KH6/W1LONGCALL"))};
        switch (scene) {
        case 1:
            room.insert(QStringLiteral("now"),
                        QJsonArray{turn(31, QStringLiteral("unknown"), {}, g_now - 9000, QJsonValue::Null)});
            break;
        case 2:
            room.insert(QStringLiteral("now"),
                        QJsonArray{turn(31, QStringLiteral("doubled"), pair, g_now - 9000, QJsonValue::Null)});
            break;
        case 3:
            room.insert(QStringLiteral("now"),
                        QJsonArray{turn(40, QStringLiteral("talking"),
                                        QJsonArray{talker(QStringLiteral("me"), QStringLiteral("KG4VCF"))},
                                        g_now - 65000, QJsonValue::Null, true)});
            break;
        case 4:
            room.insert(QStringLiteral("now"), QJsonArray{});
            break;
        case 8:
            AppSettings::instance().setValue(QStringLiteral("Roundtable/MuteList"), QStringLiteral("N3XYZ"));
            break;
        case 9:
            p.state.applyStationValue("mentionCounts", QStringLiteral("{\"%1\":1}").arg(kRoom));
            break;
        default:
            break;
        }
        p.model.applyRoom(room);
        if (scene == 5) {
            QJsonObject other = talkingRoom(chat);
            other.insert(QStringLiteral("members"), who);
            other.insert(QStringLiteral("id"), kOther);
            p.model.applyRoom(other);
        }
        if (scene == 6) {
            p.setLink(RoundtableModel::LinkState::Hidden);
        }
        RoundtablePanel panel(&p.model);
        panel.resize(width, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        if (scene == 5) {
            panel.readRoom(kOther);
            settle();
            QVERIFY(panel.isReading());
        }
        if (scene == 6) {
            panel.setOwnReading(-73.0, g_now);
            settle();
            QCOMPARE(texts(&panel, QStringLiteral("roundtableConnectionLine")),
                     QStringList{QStringLiteral("You are hidden")});
        }
        if (scene == 7) {
            auto names = panel.findChildren<QPushButton*>(QStringLiteral("roundtableTurnNameBtn"));
            QVERIFY(!names.isEmpty());
            names.first()->click();
            settle();
            QVERIFY(child<QLineEdit>(&panel, QStringLiteral("roundtableNameInput")) != nullptr);
        }
        // The resize-driven rebuild runs on the next turn of the loop.
        settle();

        auto* scroll = child<QScrollArea>(&panel, QStringLiteral("roundtableScroll"));
        auto* content = child<QWidget>(&panel, QStringLiteral("roundtableContent"));
        const int viewport = scroll->viewport()->width();
        QVERIFY(viewport > 0);
        // What the content may ask for: the viewport, less a scroll bar
        // when none shows (so one appearing cannot clip the content).
        const int allowed = scroll->verticalScrollBar()->isVisible()
                             ? viewport
                             : viewport - panel.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        QStringList wide;
        if (content->width() > viewport) {
            wide.append(QStringLiteral("content %1").arg(content->width()));
        }
        for (QWidget* w : content->findChildren<QWidget*>()) {
            if (!w->isVisibleTo(content)) {
                continue;
            }
            const int right = w->mapTo(content, QPoint(w->width(), 0)).x();
            const int hint = w->layout() != nullptr ? w->layout()->minimumSize().width()
                                                    : w->minimumSizeHint().width();
            // Elided or wrapped, never clipped: a label narrower than its
            // minimum (an eliding label's is its kept characters) is cut.
            if (auto* label = qobject_cast<QLabel*>(w); label != nullptr && !label->wordWrap()
                                                         && !label->text().isEmpty() && label->pixmap().isNull()
                                                         && label->width() < label->minimumSizeHint().width()) {
                wide.append(QStringLiteral("QLabel %1 \"%2\" clipped to %3 of %4")
                                .arg(label->objectName(), label->text())
                                .arg(label->width())
                                .arg(label->minimumSizeHint().width()));
            }
            if (right > viewport || std::max(hint, w->minimumWidth()) > allowed) {
                wide.append(QStringLiteral("%1 %2 ends at %3, asks %4")
                                .arg(QString::fromLatin1(w->metaObject()->className()), w->objectName())
                                .arg(right)
                                .arg(std::max(hint, w->minimumWidth())));
            }
        }
        QVERIFY2(wide.isEmpty(), qPrintable(QStringLiteral("viewport %1: %2").arg(viewport).arg(
                                                wide.join(QStringLiteral("; ")))));
        // Room for a scroll bar too, so the scroll area cannot flip.
        const int withBar = width - panel.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        QVERIFY2(content->minimumSizeHint().width() <= withBar,
                 qPrintable(QString::number(content->minimumSizeHint().width())));

        // The long callsign's reading row elides it, with the whole
        // callsign in a tooltip once it is painted short. (Doubled, it is
        // talking; keyed, the rows are of you; quiet, a list of names.)
        if (scene != 2 && scene != 3 && scene != 4) {
            QLabel* longCall = nullptr;
            for (QLabel* label : panel.findChildren<QLabel*>(QStringLiteral("roundtableReadingCall"))) {
                if (label->text() == QLatin1String("KH6/W1LONGCALL") && label->isVisibleTo(&panel)) {
                    longCall = label;
                }
            }
            QVERIFY(longCall != nullptr);
            longCall->repaint();
            const int whole = longCall->fontMetrics().horizontalAdvance(longCall->text());
            if (longCall->width() < whole) {
                QCOMPARE(longCall->toolTip(), QStringLiteral("KH6/W1LONGCALL"));
            }
            // Six characters always show whole.
            QVERIFY2(longCall->width() >= longCall->fontMetrics().horizontalAdvance(QStringLiteral("KH6/W1")),
                     qPrintable(QString::number(longCall->width())));
        }
    }

    void aTenCharacterFrequencyFits_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<bool>("reading");
        for (int width : {260, 280, 298}) {
            QTest::newRow(qPrintable(QStringLiteral("tuned %1").arg(width))) << width << false;
            QTest::newRow(qPrintable(QStringLiteral("reading %1").arg(width))) << width << true;
        }
    }

    void aTenCharacterFrequencyFits()
    {
        // A 20 m room ("14.212.000", ten characters) in the docked column:
        // the header fits, and when the row would not fit even with the
        // count below it, the mode moves down beside the count.
        QFETCH(int, width);
        QFETCH(bool, reading);
        const QString twenty = QStringLiteral("usb-14212000");
        Posed p;
        if (!reading) {
            p.state.applyStationValue("sliceRooms", QStringLiteral("{\"0\":\"%1\"}").arg(twenty));
        }
        QJsonArray rooms{
            QJsonObject{{QStringLiteral("id"), kRoom}, {QStringLiteral("hz"), 7185000},
                        {QStringLiteral("members"), 5}},
            QJsonObject{{QStringLiteral("id"), twenty}, {QStringLiteral("hz"), 14212000},
                        {QStringLiteral("members"), 5}}};
        p.model.applyRooms(QJsonObject{{QStringLiteral("rooms"), rooms}});
        p.model.applyRoom(talkingRoom());
        QJsonObject room = talkingRoom();
        room.insert(QStringLiteral("id"), twenty);
        p.model.applyRoom(room);
        RoundtablePanel panel(&p.model);
        panel.resize(width, 900);
        panel.followSlice(0);
        panel.show();
        settle();
        if (reading) {
            panel.readRoom(twenty);
            settle();
            QVERIFY(panel.isReading());
        }
        settle();
        QCOMPARE(panel.roomId(), twenty);
        QCOMPARE(texts(&panel, QStringLiteral("roundtableFrequency")), QStringList{QStringLiteral("14.212.000")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableMode")), QStringList{QStringLiteral("USB")});
        QCOMPARE(texts(&panel, QStringLiteral("roundtableTag")).size(), 1);

        auto* scroll = child<QScrollArea>(&panel, QStringLiteral("roundtableScroll"));
        auto* content = child<QWidget>(&panel, QStringLiteral("roundtableContent"));
        auto* header = child<QWidget>(&panel, QStringLiteral("roundtableHeader"));
        const int viewport = scroll->viewport()->width();
        const int allowed = scroll->verticalScrollBar()->isVisible()
                             ? viewport
                             : viewport - panel.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        QStringList wide;
        QList<QWidget*> parts = header->findChildren<QWidget*>();
        parts.prepend(header);
        for (QWidget* w : parts) {
            if (!w->isVisibleTo(content)) {
                continue;
            }
            const int right = w->mapTo(content, QPoint(w->width(), 0)).x();
            const int hint = w->layout() != nullptr ? w->layout()->minimumSize().width()
                                                    : w->minimumSizeHint().width();
            if (auto* label = qobject_cast<QLabel*>(w); label != nullptr && !label->text().isEmpty()
                                                         && label->width() < label->minimumSizeHint().width()) {
                wide.append(QStringLiteral("QLabel %1 \"%2\" clipped to %3 of %4")
                                .arg(label->objectName(), label->text())
                                .arg(label->width())
                                .arg(label->minimumSizeHint().width()));
            }
            if (right > viewport || std::max(hint, w->minimumWidth()) > allowed) {
                wide.append(QStringLiteral("%1 %2 ends at %3, asks %4")
                                .arg(QString::fromLatin1(w->metaObject()->className()), w->objectName())
                                .arg(right)
                                .arg(std::max(hint, w->minimumWidth())));
            }
        }
        QVERIFY2(wide.isEmpty(), qPrintable(QStringLiteral("viewport %1: %2").arg(viewport).arg(
                                                wide.join(QStringLiteral("; ")))));

        // The mode is in the row exactly when the row (count aside) fits.
        const HeaderFit fit = headerFit(&panel, QStringLiteral("5 in the room · 1 away"),
                                        QStringLiteral("5 in the room\n1 away"));
        QLabel* freq = child<QLabel>(&panel, QStringLiteral("roundtableFrequency"));
        QLabel* mode = nullptr;
        for (QLabel* label : panel.findChildren<QLabel*>(QStringLiteral("roundtableMode"))) {
            if (label->isVisibleTo(&panel)) {
                mode = label;
            }
        }
        QVERIFY(mode != nullptr);
        const bool rowFits = fit.row <= width - fit.outside;
        const int freqBottom = yIn(&panel, freq) + freq->height();
        QVERIFY2(rowFits ? yIn(&panel, mode) < freqBottom : yIn(&panel, mode) >= freqBottom,
                 qPrintable(QStringLiteral("row %1 in %2, mode at %3, frequency ends %4")
                                .arg(fit.row)
                                .arg(width - fit.outside)
                                .arg(yIn(&panel, mode))
                                .arg(freqBottom)));
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

    // The readings are rebuilt every second (refreshNow). A layout pass that
    // runs before the new readings show must not lay that section out empty:
    // the scroll range shrank by its height, a view near the end but not at
    // it was pulled up that far, and it stayed there when the readings came
    // back (the window-of-Core flow's "mention read" screen on Windows).
    void aRefreshKeepsAViewNearTheEnd()
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
        auto* readings = child<QWidget>(&panel, QStringLiteral("roundtableReadings"));
        QScrollBar* bar = child<QScrollArea>(&panel, QStringLiteral("roundtableScroll"))->verticalScrollBar();
        QCOMPARE(bar->value(), bar->maximum());
        constexpr int kAboveTheEnd = 20;
        QVERIFY2(readings->height() > kAboveTheEnd, qPrintable(QString::number(readings->height())));
        bar->setValue(bar->maximum() - kAboveTheEnd);
        settle();
        const int max = bar->maximum();
        const int value = bar->value();
        QCOMPARE(value, max - kAboveTheEnd);

        panel.refreshNow();
        // The layout passes alone, before any other posted event (the order
        // a slower machine met).
        for (int i = 0; i < 6; ++i) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        }
        QCOMPARE(bar->maximum(), max);
        QCOMPARE(bar->value(), value);
        settle();
        QCOMPARE(bar->maximum(), max);
        QCOMPARE(bar->value(), value);
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
        // The empty view's own-window button is the same button.
        int shown = 0;
        for (QToolButton* own : panel.findChildren<QToolButton*>(QStringLiteral("roundtablePopBtn"))) {
            QCOMPARE(own->toolTip(), QStringLiteral("Open in its own window"));
            QCOMPARE(own->accessibleName(), QStringLiteral("Open in its own window"));
            shown += own->isVisible() ? 1 : 0;
        }
        QCOMPARE(shown, 1);
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
        // The room column has the width for the count on one line.
        QCOMPARE(texts(panel, QStringLiteral("roundtableCount")),
                 QStringList{QStringLiteral("5 in the room · 1 away")});

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
        // Not valid on Windows while AppletWidget was not exported from the
        // GUI DLL: the signal's address here was MinGW's import thunk.
        QVERIFY(docked.isValid());

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

        // The own-window button says what it does, to the eye and to
        // accessibility, so it is not taken for the container's Pop Out.
        auto* own = child<QToolButton>(panel, QStringLiteral("roundtablePopBtn"));
        QCOMPARE(own->toolTip(), QStringLiteral("Open in its own window"));
        QCOMPARE(own->accessibleName(), QStringLiteral("Open in its own window"));
        QAccessibleInterface* face = QAccessible::queryAccessibleInterface(own);
        QVERIFY(face != nullptr);
        QCOMPARE(face->text(QAccessible::Name), QStringLiteral("Open in its own window"));

        own->click();
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

    void managedPopOutIsThePopOutContainer()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        ManagedWorkspace w(&p.model);
        QVERIFY(w.ok);
        settle();
        // A container hosts the applet, not the panel.
        QVERIFY(w.roundtable->parentWidget() != &w.host);
        QVERIFY(!w.roundtable->isPoppedOut());

        // The panel's own arrow finds the panel through the container and
        // opens Roundtable's own window; the entry stays where it was.
        ContainerArrangeController* arrange = w.manager->arrangeController();
        const quint64 revision = w.store.snapshot().revision;
        emit w.roundtable->panel()->popOutRequested();
        settle();
        QVERIFY(w.roundtable->isPoppedOut());
        QVERIFY(w.roundtable->floatingWindow()->isVisible());
        QVERIFY(w.host.isAppletFloating(w.roundtable));
        QCOMPARE(w.store.snapshot().containers.size(), 1);
        QCOMPARE(w.store.snapshot().revision, revision);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletroundtableFloating")).toString(),
                 QStringLiteral("True"));

        // Its close box docks it, and the panel knows.
        QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::SynchronousDelivery>(
            w.roundtable->floatingWindow()->windowHandle());
        settle();
        QVERIFY(!w.roundtable->isPoppedOut());
        QVERIFY(!w.host.isAppletFloating(w.roundtable));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletroundtableFloating")).toString(),
                 QStringLiteral("False"));

        // Put it back here docks it too.
        emit w.roundtable->panel()->popOutRequested();
        settle();
        QVERIFY(w.roundtable->isPoppedOut());
        child<QPushButton>(w.roundtable, QStringLiteral("roundtablePutBackBtn"))->click();
        settle();
        QVERIFY(!w.roundtable->isPoppedOut());
        QVERIFY(!w.host.isAppletFloating(w.roundtable));
        QCOMPARE(w.store.snapshot().containers.size(), 1);

        // The container menu's Pop Out: a pop-out container, as for every
        // other applet (JJ, PR 657), not Roundtable's own window.
        QVERIFY(arrange->popOut(w.roundtableEntry).ok);
        settle();
        QCOMPARE(w.store.snapshot().containers.size(), 2);
        QVERIFY(w.store.snapshot().containers.last().popOutShell);
        QCOMPARE(w.store.snapshot().containers.last().contents.first().id, w.roundtableEntry);
        QVERIFY(!w.roundtable->isPoppedOut());
        QVERIFY(w.host.isAppletFloating(w.roundtable));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletroundtableFloating")).toString(),
                 QStringLiteral("False"));
        QVERIFY(arrange->returnEntry(w.roundtableEntry).ok);
        settle();
        QCOMPARE(w.store.snapshot().containers.size(), 1);
        QVERIFY(!w.host.isAppletFloating(w.roundtable));

        // So does every other applet.
        QVERIFY(arrange->popOut(w.monitorEntry).ok);
        settle();
        QCOMPARE(w.store.snapshot().containers.size(), 2);
        QVERIFY(w.store.snapshot().containers.last().popOutShell);
        QCOMPARE(w.store.snapshot().containers.last().contents.first().id, w.monitorEntry);
        QVERIFY(w.host.isAppletFloating(w.monitor));
        QVERIFY(!w.roundtable->isPoppedOut());
    }

    void managedWindowReopensAtLaunch()
    {
        Posed p;
        p.model.applyRoom(talkingRoom());
        // Last session: the window at 700 x 480 at (40, 60), open at quit.
        {
            QWidget owner;
            RoundtableWindow window(&p.model, &owner);
            window.showAtSavedGeometry();
            settle();
            window.resize(700, 480);
            window.move(40, 60);
            settle();
            window.close();
        }
        QVERIFY(!AppSettings::instance().value(QLatin1String(RoundtableWindow::kGeometryKey)).toString().isEmpty());
        AppSettings::instance().setValue(QStringLiteral("AppletroundtableFloating"), QStringLiteral("True"));

        {
            ManagedWorkspace w(&p.model);
            QVERIFY(w.ok);
            settle();
            QVERIFY(w.roundtable->isPoppedOut());
            RoundtableWindow* window = w.roundtable->floatingWindow();
            QVERIFY(window->isVisible());
            QCOMPARE(window->size(), QSize(700, 480));
            QCOMPARE(window->pos(), QPoint(40, 60));
            QCOMPARE(window->screen(), QGuiApplication::screenAt(QPoint(40 + 350, 60 + 240)));
            QVERIFY(w.host.isAppletFloating(w.roundtable));
            // No shell for it, and the monitor's own key did not move it.
            QCOMPARE(w.store.snapshot().containers.size(), 1);
            QVERIFY(!w.host.isAppletFloating(w.monitor));

            // Quit closes the window without docking it: it stays open for
            // the next launch.
            QApplication::closeAllWindows();
            settle();
            QVERIFY(!window->isVisible());
            QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletroundtableFloating")).toString(),
                     QStringLiteral("True"));
        }

        // The next launch: open again; its close box docks it, and the
        // launch after that leaves it docked.
        {
            ManagedWorkspace w(&p.model);
            QVERIFY(w.ok);
            settle();
            QVERIFY(w.roundtable->isPoppedOut());
            QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::SynchronousDelivery>(
                w.roundtable->floatingWindow()->windowHandle());
            settle();
            QVERIFY(!w.roundtable->isPoppedOut());
            QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletroundtableFloating")).toString(),
                     QStringLiteral("False"));
        }
        {
            ManagedWorkspace w(&p.model);
            QVERIFY(w.ok);
            settle();
            QVERIFY(!w.roundtable->isPoppedOut());
        }
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

    void aRoomsRefreshLeavesThePickedRowAlone()
    {
        // JJ's log (2026-10-26): about every 2 s, with a room picked in the
        // pop-out's list, "Cannot create accessible child interface for
        // object: QListWidget(name = "roundtableRoomList") index: 1 type:
        // QAccessible::SelectionRemove" and "Invalid child in
        // QAccessibleEvent". Each rooms refresh cleared and refilled the
        // list, so accessibility heard the pick removed and added again.
        // A refresh of the same rooms now tells accessibility only of the
        // rows' new text, nothing about the rows or the pick; one that
        // brings a room in refills it with no event for a row that is gone.
        Posed p;
        p.model.applyRoom(talkingRoom());
        RoundtableWindow window(&p.model);
        auto* panel = new RoundtablePanel(&p.model);
        panel->followSlice(0);
        window.takePanel(panel);
        window.resize(1200, 600);
        window.show();
        settle();
        auto* rooms = child<QListWidget>(&window, QStringLiteral("roundtableRoomList"));
        QCOMPARE(rooms->count(), 3);
        // 7.249 picked: the heading, 7.185, 7.249.
        rooms->setCurrentRow(2);
        QVERIFY(rooms->currentItem()->isSelected());
        settle();

        // Accessibility active offscreen; as the macOS platform does, every
        // event's interface is asked for.
        struct Heard {
            int listRowEvents = 0;
            int stale = 0;
        };
        static Heard s_heard;
        s_heard = Heard{};
        const QAccessible::UpdateHandler previous = QAccessible::installUpdateHandler(
            [](QAccessibleEvent* event) {
                // The rows or the pick changing; a row's new text (a model
                // DataChanged) is news accessibility should have.
                const bool rowsOrPick = event->type() == QAccessible::SelectionRemove
                    || event->type() == QAccessible::SelectionAdd || event->type() == QAccessible::Selection
                    || (event->type() == QAccessible::TableModelChanged
                        && static_cast<QAccessibleTableModelChangeEvent*>(event)->modelChangeType()
                               != QAccessibleTableModelChangeEvent::DataChanged);
                if (rowsOrPick && event->object() != nullptr
                    && event->object()->objectName() == QLatin1String("roundtableRoomList")) {
                    ++s_heard.listRowEvents;
                }
                if (event->child() >= 0 && event->accessibleInterface() == nullptr) {
                    ++s_heard.stale;
                }
            });
        setAccessibilityActive(true);
        // Off, no item view sends an event and both halves below would
        // pass or fail on nothing.
        QVERIFY(QAccessible::isActive());
        QTest::failOnWarning(QRegularExpression(QStringLiteral("Cannot create accessible child interface")));
        QTest::failOnWarning(QRegularExpression(QStringLiteral("Invalid child in QAccessibleEvent")));
        const auto refresh = [&p](int members) {
            const QJsonArray fresh{
                QJsonObject{{QStringLiteral("id"), kRoom}, {QStringLiteral("hz"), 7185000},
                            {QStringLiteral("members"), members}},
                QJsonObject{{QStringLiteral("id"), kOther}, {QStringLiteral("hz"), 7249000},
                            {QStringLiteral("members"), 3}}};
            p.model.applyRooms(QJsonObject{{QStringLiteral("rooms"), fresh}});
            settle();
        };

        // The refresh every couple of seconds: the same rooms, new counts,
        // with the window shown and put away (docked: hidden, not closed).
        QListWidgetItem* picked = rooms->currentItem();
        for (int members = 6; members < 9; ++members) {
            refresh(members);
        }
        window.hide();
        settle();
        refresh(9);
        refresh(10);
        window.show();
        settle();
        QCOMPARE(s_heard.listRowEvents, 0);
        QCOMPARE(rooms->currentItem(), picked);
        QVERIFY(rooms->item(1)->text().contains(QStringLiteral("    10")));

        // A refresh that brings a room in below the heading and above the
        // pick: refilled, the pick kept, nothing about a row that is gone.
        const QJsonArray grown{
            QJsonObject{{QStringLiteral("id"), kRoom}, {QStringLiteral("hz"), 7185000},
                        {QStringLiteral("members"), 4}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("lsb-7200000")}, {QStringLiteral("hz"), 7200000},
                        {QStringLiteral("members"), 2}},
            QJsonObject{{QStringLiteral("id"), kOther}, {QStringLiteral("hz"), 7249000},
                        {QStringLiteral("members"), 3}}};
        p.model.applyRooms(QJsonObject{{QStringLiteral("rooms"), grown}});
        settle();
        setAccessibilityActive(false);
        QAccessible::installUpdateHandler(previous);

        QCOMPARE(rooms->count(), 4);
        QVERIFY(s_heard.listRowEvents > 0);
        QCOMPARE(s_heard.stale, 0);
        QVERIFY(rooms->currentItem() != nullptr);
        QCOMPARE(rooms->currentItem()->text().left(9), QStringLiteral("7.249.000"));
        QVERIFY(rooms->currentItem()->isSelected());
    }
};

QTEST_MAIN(TstRoundtablePanel)
#include "tst_roundtable_panel.moc"

