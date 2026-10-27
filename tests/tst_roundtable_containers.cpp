// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_containers.cpp  (NereusSDR)
// =================================================================
//
// The Roundtable applet as a container item like every other, in the real
// MainWindow with the workspace look on (the app's default), offscreen:
// moving it to a new container applies, the container Pop Out is the
// normal pop-out container (Roundtable's own window stays on its own
// arrow), and that container snaps to an edge. A refresh of the rooms
// while live leaves the saved workspace alone.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: Created for the container bugs JJ found on PR 657.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: the container's Pop Out and Return while Roundtable's own
//               window is open. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QJsonObject>
#include <QLoggingCategory>

#include <chrono>
#include <memory>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/applets/RoundtableApplet.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

constexpr QSize kWindowSize(1500, 950);
constexpr int kSettleTimeoutMs = 10000;
const QString kRoundtableType = QStringLiteral("applet:roundtable");
const QString kRoom = QStringLiteral("lsb-7185000");
const QString kOther = QStringLiteral("lsb-7249000");

void settle()
{
    for (int pass = 0; pass < 8; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// A window of a Core, as far as Roundtable asks: it is available, and every
// action is taken.
class QuietLink : public IStationLink {
public:
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool roundtableAvailable() const override { return true; }
};

// The window's Roundtable, live in room 7.185 on slice A, its rooms
// refreshed as the Core refreshes them.
struct LiveRoundtable {
    QuietLink link;
    RoundtableState state;
    RadioModel* radio = nullptr;
    RoundtableModel* model = nullptr;

    explicit LiveRoundtable(RadioModel* r) : radio(r), model(r->roundtableModel())
    {
        model->detach();
        model->attachStation(&link, &state);
        state.applyStationValue("linkSince", 0);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        state.applyStationValue("sliceRooms", QStringLiteral("{\"0\":\"%1\"}").arg(kRoom));
        refresh(5);
    }

    ~LiveRoundtable()
    {
        model->detach();
        model->attachLocal(radio->roundtableClient(), radio->roundtableState());
    }

    void refresh(int members)
    {
        const QJsonArray rooms{
            QJsonObject{{QStringLiteral("id"), kRoom}, {QStringLiteral("hz"), 7185000},
                        {QStringLiteral("members"), members}},
            QJsonObject{{QStringLiteral("id"), kOther}, {QStringLiteral("hz"), 7249000},
                        {QStringLiteral("members"), 3}}};
        model->applyRooms(QJsonObject{{QStringLiteral("rooms"), rooms}});
    }
};

std::unique_ptr<MainWindow> buildWindow()
{
    WorkspacePreview::setEnabledForTest(true);
    auto& app = *qobject_cast<QApplication*>(QCoreApplication::instance());
    applyDarkPalette(app);
    applyAppBaselineQss(app);
    applyWorkspacePalette(app);
    auto window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                               MainWindow::ConnectionStartup::Deferred);
    window->radioModel()->setBoardForTest(HPSDRHW::Saturn);
    window->resize(kWindowSize);
    window->show();
    window->resize(kWindowSize);
    settle();
    return window;
}

struct Placement {
    QString containerId;
    QString entryId;
};

Placement roundtableIn(const WorkspaceDocument& document)
{
    for (const auto& c : document.containers) {
        for (const auto& e : c.contents) {
            if (e.typeId == kRoundtableType) {
                return {c.id, e.id};
            }
        }
    }
    return {};
}

const ContainerDocument* containerIn(const WorkspaceDocument& document, const QString& id)
{
    for (const auto& c : document.containers) {
        if (c.id == id) {
            return &c;
        }
    }
    return nullptr;
}

// NEREUS_ROUNDTABLE_CONTAINER_CAPTURE_DIR set: `widget`'s window saved as
// <dir>/<name>.png for a reviewer (offscreen grabs).
void capture(QWidget* widget, const QString& name)
{
    const QString dir = qEnvironmentVariable("NEREUS_ROUNDTABLE_CONTAINER_CAPTURE_DIR");
    if (dir.isEmpty() || widget == nullptr) {
        return;
    }
    QDir().mkpath(dir);
    settle();
    widget->window()->grab().save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
}

// Every shown pop-out and own window, saved as <name>-<n>.png.
void captureTopLevels(QWidget* main, const QString& name)
{
    int n = 0;
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top != main && top->isVisible() && top->isWindow() && top->width() > 100
            && (qobject_cast<RoundtableWindow*>(top) != nullptr
                || top->inherits("NereusSDR::FloatingContainer"))) {
            capture(top, QStringLiteral("%1-%2").arg(name).arg(++n));
        }
    }
}

// The container widget the applet is shown in.
ContainerWidget* shownIn(QWidget* applet)
{
    for (QWidget* up = applet->parentWidget(); up != nullptr; up = up->parentWidget()) {
        if (auto* c = qobject_cast<ContainerWidget*>(up)) {
            return c;
        }
    }
    return nullptr;
}

struct Window {
    std::unique_ptr<MainWindow> main;
    ContainerManager* manager = nullptr;
    ContainerWorkspaceStore* store = nullptr;
    RoundtableApplet* applet = nullptr;

    bool open()
    {
        main = buildWindow();
        manager = main->findChild<ContainerManager*>();
        store = manager != nullptr ? manager->workspaceStore() : nullptr;
        applet = main->findChild<RoundtableApplet*>();
        return manager != nullptr && store != nullptr && applet != nullptr;
    }

    bool idle() const
    {
        return QTest::qWaitFor([this] { return !manager->geometryCommitPendingForTest(); },
                               kSettleTimeoutMs);
    }

    // The workspace has a Roundtable entry (a first launch places it).
    bool placed() const { return !roundtableIn(store->snapshot()).entryId.isEmpty(); }
};

} // namespace

class TstRoundtableContainers : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false\n"
                                                        "nereus.connection.warning=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("roundtable-containers")));
        qApp->setStyle(QStringLiteral("Fusion"));
        qApp->setQuitOnLastWindowClosed(false);
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{30});
    }

    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }

    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
        RadioDiscovery::clearHoldOffForTest();
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    void aRoomsRefreshLeavesTheWorkspaceAlone()
    {
        Window w;
        QVERIFY(w.open());
        LiveRoundtable live(w.main->radioModel());
        settle();
        QVERIFY(w.idle());
        QVERIFY(w.placed());
        const quint64 revision = w.store->snapshot().revision;
        for (int members = 6; members < 12; ++members) {
            live.refresh(members);
            settle();
            QTest::qWait(250);
        }
        QVERIFY(w.idle());
        QCOMPARE(w.store->snapshot().revision, revision);
        w.main->close();
    }

    void moveToANewContainerApplies()
    {
        // Container Settings: New container, Move to container..., Apply
        // (ContainerSettingsDialog, its commit through the manager with the
        // workspace look on), with the rooms refreshing meanwhile.
        Window w;
        QVERIFY(w.open());
        LiveRoundtable live(w.main->radioModel());
        settle();
        QVERIFY(w.idle());
        ContainerManager* manager = w.manager;
        ContainerEditSession session(*w.store, [manager](const WorkspaceDocument& d, quint64 revision) {
            return manager->commitWorkspace(d, revision);
        });
        WorkspaceDocument draft = session.draft();
        const Placement before = roundtableIn(draft);
        QVERIFY(!before.entryId.isEmpty());
        ContainerDocument fresh;
        fresh.id = QStringLiteral("roundtable-new");
        fresh.name = QStringLiteral("New container");
        fresh.layout = ContentLayout::VerticalStack;
        draft.containers.append(fresh);
        QVERIFY(ContainerArrangeController::moveDraft(draft, before.entryId, fresh.id, 0).ok);
        session.setDraft(draft);
        for (int members = 6; members < 10; ++members) {
            live.refresh(members);
            settle();
            QTest::qWait(250);
        }
        const CommitResult result = session.apply();
        QVERIFY2(result.status == CommitStatus::Saved, qPrintable(result.error));
        settle();
        QCOMPARE(roundtableIn(w.store->snapshot()).containerId, fresh.id);
        ContainerWidget* shown = shownIn(w.applet);
        QVERIFY(shown != nullptr);
        QCOMPARE(shown->id(), fresh.id);
        QVERIFY(w.applet->isVisible());
        w.main->close();
    }

    void containerSettingsMovesItToANewContainer_data()
    {
        QTest::addColumn<bool>("ownWindow");
        QTest::newRow("in its container") << false;
        QTest::newRow("in its own window") << true;
    }

    void containerSettingsMovesItToANewContainer()
    {
        QFETCH(bool, ownWindow);
        // The same, through the Container Settings dialog itself: New,
        // back to Roundtable's container, Move to container... > New
        // container, Apply.
        Window w;
        QVERIFY(w.open());
        LiveRoundtable live(w.main->radioModel());
        settle();
        QVERIFY(w.idle());
        const Placement before = roundtableIn(w.store->snapshot());
        QVERIFY(!before.entryId.isEmpty());
        if (ownWindow) {
            emit w.applet->panel()->popOutRequested();
            settle();
            QVERIFY(w.applet->isPoppedOut());
        }
        ContainerSettingsDialog dialog(w.manager->container(before.containerId), w.main.get(), w.manager);
        dialog.resize(1200, 900);
        dialog.show();
        settle();
        auto* create = dialog.findChild<QPushButton*>(QStringLiteral("createContainer"));
        auto* move = dialog.findChild<QPushButton*>(QStringLiteral("moveContent"));
        auto* items = dialog.findChild<QListWidget*>(QStringLiteral("containerDraftContents"));
        QVERIFY(create != nullptr && move != nullptr && items != nullptr);
        create->click();
        settle();
        QString freshId;
        for (const auto& c : dialog.editSession()->draft().containers) {
            if (containerIn(w.store->snapshot(), c.id) == nullptr) {
                freshId = c.id;
            }
        }
        QVERIFY(!freshId.isEmpty());
        dialog.selectDraftContainer(before.containerId);
        settle();
        int row = -1;
        for (int i = 0; i < items->count(); ++i) {
            if (items->item(i)->data(Qt::UserRole).toString() == before.entryId) {
                row = i;
            }
        }
        QVERIFY(row >= 0);
        items->setCurrentRow(row);
        settle();
        move->click();
        settle();
        QAction* target = nullptr;
        for (QMenu* menu : dialog.findChildren<QMenu*>()) {
            for (QAction* action : menu->actions()) {
                if (action->text() == QStringLiteral("New container")) {
                    target = action;
                }
            }
        }
        QVERIFY(target != nullptr);
        QVERIFY(target->isEnabled());
        target->trigger();
        settle();
        capture(&dialog, ownWindow ? QStringLiteral("dialog-moved-own-window") : QStringLiteral("dialog-moved"));
        for (int members = 6; members < 10; ++members) {
            live.refresh(members);
            settle();
            QTest::qWait(250);
        }
        auto* apply = [&dialog]() -> QPushButton* {
            for (QPushButton* b : dialog.findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("Apply")) {
                    return b;
                }
            }
            return nullptr;
        }();
        QVERIFY(apply != nullptr);
        apply->click();
        settle();
        // The dialog's transaction line says why, when Apply is refused.
        QStringList said;
        for (QLabel* label : dialog.findChildren<QLabel*>(Qt::FindDirectChildrenOnly)) {
            if (label->wordWrap() && !label->text().isEmpty()) {
                said.append(label->text());
            }
        }
        capture(&dialog, ownWindow ? QStringLiteral("dialog-applied-own-window") : QStringLiteral("dialog-applied"));
        QVERIFY2(roundtableIn(w.store->snapshot()).containerId == freshId, qPrintable(said.join(QLatin1String(" | "))));
        ContainerWidget* shown = shownIn(w.applet);
        QVERIFY(shown != nullptr);
        QCOMPARE(shown->id(), freshId);
        dialog.reject();
        w.main->close();
    }

    void dragToAnotherContainerDrops()
    {
        // The arrange drag: the drag's data names the revision it began
        // at; a rooms refresh during the drag must not make it stale.
        Window w;
        QVERIFY(w.open());
        LiveRoundtable live(w.main->radioModel());
        settle();
        QVERIFY(w.idle());
        WorkspaceDocument d = w.store->snapshot();
        ContainerDocument other;
        other.id = QStringLiteral("roundtable-target");
        other.name = QStringLiteral("Target");
        other.layout = ContentLayout::VerticalStack;
        other.dockMode = DockMode::Floating;
        other.geometry = QRect(200, 200, 360, 300);
        d.containers.append(other);
        QCOMPARE(w.manager->commitWorkspace(d, d.revision).status, CommitStatus::Saved);
        settle();
        QVERIFY(w.idle());
        ContainerArrangeController* arrange = w.manager->arrangeController();
        const Placement before = roundtableIn(w.store->snapshot());
        const QByteArray data = arrange->mimeData(before.entryId);
        for (int members = 6; members < 10; ++members) {
            live.refresh(members);
            settle();
            QTest::qWait(250);
        }
        const ArrangeResult result = arrange->drop(data, other.id, 0);
        QVERIFY2(result.ok, qPrintable(result.error));
        settle();
        QCOMPARE(roundtableIn(w.store->snapshot()).containerId, other.id);
        w.main->close();
    }

    void popOutIsTheContainerPopOut()
    {
        // The container's Pop Out (entry header, entry menu) gives Roundtable
        // a pop-out container as it does every other item; Roundtable's own
        // window stays on the panel's own pop-out arrow.
        Window w;
        QVERIFY(w.open());
        LiveRoundtable live(w.main->radioModel());
        settle();
        QVERIFY(w.idle());
        ContainerArrangeController* arrange = w.manager->arrangeController();
        const WorkspaceDocument before = w.store->snapshot();
        const Placement was = roundtableIn(before);
        capture(w.main.get(), QStringLiteral("popout-0-main-before"));
        QVERIFY(arrange->popOut(was.entryId).ok);
        settle();
        capture(w.main.get(), QStringLiteral("popout-1-main"));
        captureTopLevels(w.main.get(), QStringLiteral("popout-2-window"));
        const WorkspaceDocument after = w.store->snapshot();
        QCOMPARE(after.containers.size(), before.containers.size() + 1);
        const Placement now = roundtableIn(after);
        QVERIFY(now.containerId != was.containerId);
        QCOMPARE(now.entryId, was.entryId);
        const ContainerDocument* shell = containerIn(after, now.containerId);
        QVERIFY(shell != nullptr);
        QVERIFY(shell->popOutShell);
        QCOMPARE(shell->dockMode, DockMode::Floating);
        QVERIFY(!w.applet->isPoppedOut());
        ContainerWidget* shown = shownIn(w.applet);
        QVERIFY(shown != nullptr);
        QCOMPARE(shown->id(), now.containerId);
        QVERIFY(shown->isFloating());
        QVERIFY(w.applet->isVisible());

        // Its own arrow still opens Roundtable's own window.
        emit w.applet->panel()->popOutRequested();
        settle();
        QVERIFY(w.applet->isPoppedOut());
        QVERIFY(w.applet->floatingWindow()->isVisible());
        QCOMPARE(w.store->snapshot().containers.size(), after.containers.size());
        w.applet->dockFloatingWindow();
        settle();

        // Return to remembered container puts it back.
        QVERIFY(arrange->returnEntry(was.entryId).ok);
        settle();
        QCOMPARE(roundtableIn(w.store->snapshot()).containerId, was.containerId);
        w.main->close();
    }

    void theContainerPopOutWithTheOwnWindowOpen()
    {
        // Roundtable's own window open, then the container's Pop Out: the
        // applet (its "in its own window" note) moves to a pop-out
        // container, the own window keeps the panel, and putting the panel
        // back puts it in the pop-out container.
        Window w;
        QVERIFY(w.open());
        LiveRoundtable live(w.main->radioModel());
        settle();
        QVERIFY(w.idle());
        RoundtablePanel* panel = w.applet->panel();
        QPointer<RoundtablePanel> alive(panel);
        emit panel->popOutRequested();
        settle();
        QVERIFY(w.applet->isPoppedOut());
        RoundtableWindow* own = w.applet->floatingWindow();
        QVERIFY(own != nullptr && own->isVisible());

        ContainerArrangeController* arrange = w.manager->arrangeController();
        const WorkspaceDocument before = w.store->snapshot();
        const Placement was = roundtableIn(before);
        QVERIFY(arrange->popOut(was.entryId).ok);
        settle();
        const WorkspaceDocument after = w.store->snapshot();
        QCOMPARE(after.containers.size(), before.containers.size() + 1);
        const Placement now = roundtableIn(after);
        QVERIFY(now.containerId != was.containerId);
        const ContainerDocument* shell = containerIn(after, now.containerId);
        QVERIFY(shell != nullptr);
        QVERIFY(shell->popOutShell);
        ContainerWidget* shown = shownIn(w.applet);
        QVERIFY(shown != nullptr);
        QCOMPARE(shown->id(), now.containerId);
        QVERIFY(w.applet->isVisible());
        // The own window still holds the panel; the applet shows its note.
        QVERIFY(w.applet->isPoppedOut());
        QCOMPARE(w.applet->floatingWindow(), own);
        QVERIFY(own->isVisible());
        QVERIFY(alive);
        QCOMPARE(panel->window(), own);
        auto* note = w.applet->findChild<QWidget*>(QStringLiteral("roundtablePoppedNote"));
        QVERIFY(note != nullptr && note->isVisible());

        // Put back: the panel goes into the applet, in the pop-out container.
        w.applet->findChild<QPushButton*>(QStringLiteral("roundtablePutBackBtn"))->click();
        settle();
        QVERIFY(!w.applet->isPoppedOut());
        QVERIFY(!own->isVisible());
        QVERIFY(alive);
        QCOMPARE(panel->parentWidget(), w.applet);
        QVERIFY(panel->isVisible());
        QCOMPARE(shownIn(w.applet)->id(), now.containerId);

        // The own window opened from the pop-out container, then the
        // container returned: the panel survives the shell going away.
        emit panel->popOutRequested();
        settle();
        QVERIFY(w.applet->isPoppedOut());
        QVERIFY(arrange->returnEntry(was.entryId).ok);
        settle();
        QCOMPARE(roundtableIn(w.store->snapshot()).containerId, was.containerId);
        QVERIFY(alive);
        QVERIFY(w.applet->isPoppedOut());
        QVERIFY(w.applet->floatingWindow() != nullptr && w.applet->floatingWindow()->isVisible());
        w.applet->findChild<QPushButton*>(QStringLiteral("roundtablePutBackBtn"))->click();
        settle();
        QVERIFY(alive);
        QCOMPARE(panel->parentWidget(), w.applet);
        w.main->close();
    }

    void aPoppedOutRoundtableSnapsToAnEdge()
    {
        Window w;
        QVERIFY(w.open());
        LiveRoundtable live(w.main->radioModel());
        settle();
        QVERIFY(w.idle());
        ContainerArrangeController* arrange = w.manager->arrangeController();
        const Placement was = roundtableIn(w.store->snapshot());
        QVERIFY(arrange->popOut(was.entryId).ok);
        settle();
        const Placement shell = roundtableIn(w.store->snapshot());
        QVERIFY(shell.containerId != was.containerId);
        w.manager->moveContainerToEdge(shell.containerId, DockEdge::Left);
        settle();
        capture(w.main.get(), QStringLiteral("snap-main"));
        const ContainerDocument* snapped = containerIn(w.store->snapshot(), shell.containerId);
        QVERIFY(snapped != nullptr);
        QCOMPARE(snapped->dockMode, DockMode::PanelDocked);
        QCOMPARE(snapped->edge, DockEdge::Left);
        ContainerWidget* shown = shownIn(w.applet);
        QVERIFY(shown != nullptr);
        QCOMPARE(shown->id(), shell.containerId);
        QVERIFY(shown->isPanelDocked());
        QVERIFY(w.applet->isVisible());
        w.main->close();
    }
};

QTEST_MAIN(TstRoundtableContainers)
#include "tst_roundtable_containers.moc"
