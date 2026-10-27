// no-port-check: NereusSDR-original offscreen regressions for the place the operator gave a container.
// Modification history (NereusSDR):
//   2026-10-26 - Right after a good reload ends a state that refused every
//                 save (the saved layout changed underneath), the geometry
//                 capture runs again with no other commit first: a header
//                 anchor change is saved by the next resize, and a drag is
//                 saved. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - A layout saved with no pane size is shown for the pane as
//                 it settles after the window's first layout. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Containers keep the place the operator gave them: a free
//                 container restored before the pane has its size, a pane
//                 too small for it, anchoring, operator drags and grip
//                 resizes, the look-off layout copied for the look-on one,
//                 and a profile captured live. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QApplication>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <memory>
#include <optional>

#include "core/AppSettings.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/WorkspaceProfiles.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

constexpr QSize kPaneSize(1000, 700);
constexpr QSize kSmallPaneSize(500, 300);
constexpr QRect kFirstRect(100, 80, 300, 200);
constexpr QRect kSecondRect(520, 300, 320, 240);
// The geometry capture's own debounce is 200 ms; the cases that prove
// nothing was saved wait this long, as the plan's acceptance states.
constexpr int kNothingSavedWaitMs = 300;
constexpr int kSaveTimeoutMs = 10000;

// A free container as the manager itself would write it back, so a save
// that changes nothing leaves the document's bytes alone.
ContainerDocument freeContainer(const QString& id, const QRect& geometry, AxisLock anchor = AxisLock::Left)
{
    ContainerDocument d;
    d.id = id;
    d.name = id;
    d.layout = ContentLayout::VerticalStack;
    d.dockMode = DockMode::OverlayDocked;
    d.anchor = anchor;
    d.geometry = geometry;
    d.config = {{QStringLiteral("enabled"), true},
                {QStringLiteral("showOnRx"), true},
                {QStringLiteral("showOnTx"), true},
                {QStringLiteral("border"), true},
                {QStringLiteral("pinOnTop"), false}};
    return d;
}

WorkspaceDocument workspace(const QVector<ContainerDocument>& containers)
{
    WorkspaceDocument document;
    document.mainContainerId = containers.first().id;
    document.containers = containers;
    return document;
}

// A pane that is a child of its window, as the spectrum pane is: it has
// no size of its own until the window is shown and laid out.
struct Rig {
    QWidget host;
    QWidget* pane{nullptr};
    ContainerContentRegistry registry;
    std::unique_ptr<ContainerManager> manager;

    explicit Rig(ContainerWorkspaceStore& store, const QSize& size = kPaneSize)
    {
        auto* layout = new QVBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        pane = new QWidget(&host);
        layout->addWidget(pane);
        host.resize(size);
        manager = std::make_unique<ContainerManager>(pane, nullptr);
        manager->setWorkspaceAdapter(&store, &registry);
    }

    bool show()
    {
        host.show();
        return QTest::qWaitFor([this] { return pane->size() == host.size(); }, kSaveTimeoutMs);
    }

    bool resizePane(const QSize& size)
    {
        host.resize(size);
        return QTest::qWaitFor([this, size] { return pane->size() == size; }, kSaveTimeoutMs);
    }

    QRect shown(const QString& id) const
    {
        ContainerWidget* c = manager->container(id);
        return c != nullptr ? c->geometry() : QRect();
    }

    // Lets a scheduled geometry capture run, and proves it has.
    bool settleCapture()
    {
        QTest::qWait(kNothingSavedWaitMs);
        return QTest::qWaitFor([this] { return !manager->geometryCommitPendingForTest(); }, kSaveTimeoutMs);
    }
};

std::optional<QSize> storedPane(const ContainerWorkspaceStore& store, const QString& id)
{
    for (const auto& d : store.snapshot().containers) {
        if (d.id == id) {
            return d.geometryPane;
        }
    }
    return std::nullopt;
}

QRect storedGeometry(const ContainerWorkspaceStore& store, const QString& id)
{
    for (const auto& d : store.snapshot().containers) {
        if (d.id == id) {
            return d.geometry;
        }
    }
    return {};
}

void mouse(QWidget* target, QEvent::Type type, const QPoint& global, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(target->mapFromGlobal(global)), QPointF(global), Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

// A press on `handle`, a move by `by`, a release: what the operator does.
void dragBy(QWidget* handle, const QPoint& by)
{
    const QPoint start = handle->mapToGlobal(handle->rect().center());
    mouse(handle, QEvent::MouseButtonPress, start, Qt::LeftButton);
    if (!by.isNull()) {
        mouse(handle, QEvent::MouseMove, start + by, Qt::LeftButton);
    }
    mouse(handle, QEvent::MouseButtonRelease, start + by, Qt::NoButton);
}

} // namespace

class TstContainerRememberedPlace : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false")); }
    void init() { WorkspacePreview::setEnabledForTest(false); }
    void cleanup() { WorkspacePreview::setEnabledForTest(false); }

    // Spec "The stacking bug", cause 1: restore runs before the pane has
    // its size.
    void restoreBeforeThePaneIsLaidOutKeepsSavedPlaces()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("restore.settings"));
        {
            AppSettings settings(path);
            ContainerWorkspaceStore store(settings);
            QCOMPARE(store.commit(workspace({freeContainer(QStringLiteral("a"), kFirstRect),
                                             freeContainer(QStringLiteral("b"), kSecondRect)}), 0).status,
                     CommitStatus::Saved);
        }
        AppSettings settings(path);
        settings.load();
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());
        const QString beforeRaw = settings.value(QStringLiteral("ContainerWorkspace")).toString();

        Rig rig(store);
        const QSize unlaidOut = rig.pane->size();
        QVERIFY2(unlaidOut != kPaneSize, "the pane must not have its size yet");
        rig.manager->restoreState();
        qInfo() << "dock parent size at restore:" << unlaidOut << "a:" << rig.shown(QStringLiteral("a"))
                << "b:" << rig.shown(QStringLiteral("b"));
        QVERIFY(rig.show());
        QVERIFY(rig.settleCapture());
        QCOMPARE(rig.pane->size(), kPaneSize);
        qInfo() << "after the pane has its size, a:" << rig.shown(QStringLiteral("a"))
                << "b:" << rig.shown(QStringLiteral("b"));
        QCOMPARE(rig.shown(QStringLiteral("a")), kFirstRect);
        QCOMPARE(rig.shown(QStringLiteral("b")), kSecondRect);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
        QCOMPARE(settings.value(QStringLiteral("ContainerWorkspace")).toString(), beforeRaw);
    }

    // Cause 3: a pane smaller than at save.
    void smallPaneShowsContainersInsideAndKeepsTheirPlace()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("small.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(workspace({freeContainer(QStringLiteral("a"), kFirstRect),
                                                        freeContainer(QStringLiteral("b"), kSecondRect)}), 0)
                     .status,
                 CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        QCOMPARE(rig.shown(QStringLiteral("a")), kFirstRect);
        QCOMPARE(rig.shown(QStringLiteral("b")), kSecondRect);
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());

        QVERIFY(rig.resizePane(kSmallPaneSize));
        rig.manager->updateDockedPositions(rig.pane->width(), rig.pane->height());
        QVERIFY2(rig.pane->rect().contains(rig.shown(QStringLiteral("a"))), "a is wholly inside the pane");
        QVERIFY2(rig.pane->rect().contains(rig.shown(QStringLiteral("b"))), "b is wholly inside the pane");
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);

        QVERIFY(rig.resizePane(kPaneSize));
        rig.manager->updateDockedPositions(rig.pane->width(), rig.pane->height());
        QCOMPARE(rig.shown(QStringLiteral("a")), kFirstRect);
        QCOMPARE(rig.shown(QStringLiteral("b")), kSecondRect);
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
    }

    void rightAnchoredContainerFollowsThePaneAndKeepsItsPlace()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("anchor.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        const QRect saved(600, 100, 300, 200);
        QCOMPARE(rig.manager->commitWorkspace(workspace({freeContainer(QStringLiteral("a"), saved, AxisLock::Right)}), 0)
                     .status,
                 CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        QCOMPARE(rig.shown(QStringLiteral("a")), saved);
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());

        QVERIFY(rig.resizePane(QSize(1200, 700)));
        rig.manager->updateDockedPositions(rig.pane->width(), rig.pane->height());
        QCOMPARE(rig.shown(QStringLiteral("a")).x(), 800);
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);

        QVERIFY(rig.resizePane(kPaneSize));
        rig.manager->updateDockedPositions(rig.pane->width(), rig.pane->height());
        QCOMPARE(rig.shown(QStringLiteral("a")), saved);
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
    }

    // A layout saved with no pane size (one from before places carried
    // it, or one no operator has moved) is for the pane as it settles
    // after the window's first layout, not for the first size the pane is
    // given on the way there. The real spectrum pane is given two.
    void layoutWithoutAPaneSizeIsShownForTheSettledPane()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settled.settings"));
        const QRect saved(600, 100, 300, 200);
        const QSize firstSize(1041, 700);
        {
            AppSettings settings(path);
            ContainerWorkspaceStore store(settings);
            QCOMPARE(store.commit(workspace({freeContainer(QStringLiteral("a"), saved, AxisLock::Right)}), 0).status,
                     CommitStatus::Saved);
        }
        AppSettings settings(path);
        settings.load();
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        QVERIFY(!storedPane(store, QStringLiteral("a")).has_value());
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());

        Rig rig(store, firstSize);
        rig.manager->restoreState();
        rig.host.show();
        QCOMPARE(rig.pane->size(), firstSize);
        // The pane's second size arrives before the window has settled.
        rig.host.resize(kPaneSize);
        QCOMPARE(rig.pane->size(), kPaneSize);
        qInfo() << "pane" << firstSize << "then" << kPaneSize << "a:" << rig.shown(QStringLiteral("a"));
        QCOMPARE(rig.shown(QStringLiteral("a")), saved);
        QVERIFY(rig.settleCapture());
        QCOMPARE(rig.shown(QStringLiteral("a")), saved);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);

        // From the settled pane on, the anchor works as it always did.
        QVERIFY(rig.resizePane(QSize(1200, 700)));
        QCOMPARE(rig.shown(QStringLiteral("a")).x(), 800);
        QVERIFY(rig.resizePane(kPaneSize));
        QCOMPARE(rig.shown(QStringLiteral("a")), saved);
        QVERIFY(rig.settleCapture());
        // Still never saved without an operator move.
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
        QVERIFY(!storedPane(store, QStringLiteral("a")).has_value());
    }

    void changesAfterAReloadAreSavedAtOnce()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("reload.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(workspace({freeContainer(QStringLiteral("a"), kFirstRect)}), 0).status,
                 CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        // Another writer replaces the saved layout: every save is refused.
        WorkspaceDocument external = store.snapshot();
        external.revision += 5;
        settings.setValue(QStringLiteral("ContainerWorkspace"),
                          QString::fromUtf8(ContainerDocumentCodec::encode(external)));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
            QStringLiteral("^Container workspace commit refused: Conflict - Workspace changed; reload before editing$")));
        WorkspaceDocument edit = store.snapshot();
        edit.containers[0].name = QStringLiteral("renamed while stuck");
        QCOMPARE(rig.manager->commitWorkspace(edit, edit.revision).status, CommitStatus::Conflict);
        QVERIFY(!rig.manager->storageError().isEmpty());
        // A good reload ends it; what changes next saves with no commit between.
        QVERIFY(store.load().ok);
        ContainerWidget* a = rig.manager->container(QStringLiteral("a"));
        QVERIFY(a);
        QPushButton* axis = nullptr;
        for (QPushButton* button : a->findChildren<QPushButton*>()) {
            if (button->toolTip().startsWith(QStringLiteral("Axis lock"))) {
                axis = button;
            }
        }
        QVERIFY(axis);
        axis->click();
        const AxisLock anchor = a->axisLock();
        QVERIFY(anchor != AxisLock::Left);
        // The container is resized by the app, not dragged: the capture that
        // follows saves the header's anchor.
        a->resize(a->size() + QSize(0, 1));
        QVERIFY(rig.settleCapture());
        QTRY_COMPARE_WITH_TIMEOUT(store.snapshot().containers[0].anchor, anchor, kSaveTimeoutMs);
        auto* bar = rig.manager->container(QStringLiteral("a"))->findChild<QWidget*>(QStringLiteral("containerTitleBar"));
        QVERIFY(bar);
        dragBy(bar, QPoint(40, 30));
        QCOMPARE(rig.shown(QStringLiteral("a")), kFirstRect.translated(40, 30));
        QVERIFY(rig.settleCapture());
        QTRY_COMPARE_WITH_TIMEOUT(storedGeometry(store, QStringLiteral("a")), kFirstRect.translated(40, 30),
                                  kSaveTimeoutMs);
    }

    void operatorDragIsSaved()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("drag.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(workspace({freeContainer(QStringLiteral("a"), kFirstRect)}), 0).status,
                 CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        QCOMPARE(storedGeometry(store, QStringLiteral("a")), kFirstRect);
        auto* bar = rig.manager->container(QStringLiteral("a"))->findChild<QWidget*>(QStringLiteral("containerTitleBar"));
        QVERIFY(bar);
        dragBy(bar, QPoint(40, 30));
        QCOMPARE(rig.shown(QStringLiteral("a")), kFirstRect.translated(40, 30));
        QVERIFY(rig.settleCapture());
        QTRY_COMPARE_WITH_TIMEOUT(storedGeometry(store, QStringLiteral("a")), kFirstRect.translated(40, 30),
                                  kSaveTimeoutMs);
        // The pane it was placed in is saved with it.
        QCOMPARE(storedPane(store, QStringLiteral("a")), std::optional<QSize>(kPaneSize));
    }

    // A press and release with no move re-bases nothing, even while the
    // container is shown away from its place.
    void pressAndReleaseWithNoMoveSavesNothing()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("press.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(workspace({freeContainer(QStringLiteral("b"), kSecondRect)}), 0).status,
                 CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());
        QVERIFY(rig.resizePane(kSmallPaneSize));
        QVERIFY(rig.shown(QStringLiteral("b")) != kSecondRect);
        auto* bar = rig.manager->container(QStringLiteral("b"))->findChild<QWidget*>(QStringLiteral("containerTitleBar"));
        QVERIFY(bar);
        dragBy(bar, QPoint());
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
        QVERIFY(rig.resizePane(kPaneSize));
        QCOMPARE(rig.shown(QStringLiteral("b")), kSecondRect);
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
    }

    // A move the app makes is never saved; the same move reported as the
    // operator's is, and not while capture is suppressed.
    void onlyAnOperatorMoveChangesTheRememberedPlace()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("remember.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(workspace({freeContainer(QStringLiteral("a"), kFirstRect)}), 0).status,
                 CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());
        ContainerWidget* a = rig.manager->container(QStringLiteral("a"));
        a->move(a->pos() + QPoint(25, 15));
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
        // Showing them again is safe to repeat and puts it back.
        rig.manager->placeFreeContainers();
        rig.manager->placeFreeContainers();
        QCOMPARE(a->geometry(), kFirstRect);
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);

        rig.manager->setGeometryCaptureSuppressed(true);
        a->move(a->pos() + QPoint(25, 15));
        rig.manager->rememberOperatorGeometry(QStringLiteral("a"));
        rig.manager->setGeometryCaptureSuppressed(false);
        QVERIFY(rig.settleCapture());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);

        rig.manager->rememberOperatorGeometry(QStringLiteral("a"));
        QTRY_COMPARE_WITH_TIMEOUT(storedGeometry(store, QStringLiteral("a")), kFirstRect.translated(25, 15),
                                  kSaveTimeoutMs);
        QCOMPARE(storedPane(store, QStringLiteral("a")), std::optional<QSize>(kPaneSize));
    }

    // Placed, then the pane resized, then quit and a start at that pane
    // size: shown exactly where it was showing at quit.
    void placedContainerRestartsWhereItWasShowing_data()
    {
        QTest::addColumn<int>("anchor");
        QTest::addColumn<QPoint>("shift");
        QTest::newRow("AxisLock::Left") << int(AxisLock::Left) << QPoint(0, 100);
        QTest::newRow("AxisLock::Right") << int(AxisLock::Right) << QPoint(200, 100);
    }

    void placedContainerRestartsWhereItWasShowing()
    {
        QFETCH(int, anchor);
        QFETCH(QPoint, shift);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("showing.settings"));
        const QSize resized(1200, 800);
        const QRect placed = kFirstRect.translated(40, 30);
        QRect showingAtQuit;
        QByteArray atQuit;
        {
            AppSettings settings(path);
            ContainerWorkspaceStore store(settings);
            Rig rig(store);
            QVERIFY(rig.show());
            QCOMPARE(rig.manager->commitWorkspace(
                         workspace({freeContainer(QStringLiteral("a"), kFirstRect, static_cast<AxisLock>(anchor))}), 0)
                         .status,
                     CommitStatus::Saved);
            QVERIFY(rig.settleCapture());
            auto* bar = rig.manager->container(QStringLiteral("a"))->findChild<QWidget*>(QStringLiteral("containerTitleBar"));
            QVERIFY(bar);
            dragBy(bar, QPoint(40, 30));
            QTRY_COMPARE_WITH_TIMEOUT(storedGeometry(store, QStringLiteral("a")), placed, kSaveTimeoutMs);
            QVERIFY(rig.resizePane(resized));
            QVERIFY(rig.settleCapture());
            showingAtQuit = rig.shown(QStringLiteral("a"));
            QCOMPARE(showingAtQuit, placed.translated(shift));
            rig.manager->saveState();
            QCOMPARE(storedGeometry(store, QStringLiteral("a")), placed);
            QCOMPARE(storedPane(store, QStringLiteral("a")), std::optional<QSize>(kPaneSize));
            atQuit = ContainerDocumentCodec::encode(store.snapshot());
        }
        AppSettings settings(path);
        settings.load();
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), atQuit);
        Rig rig(store, resized);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        QVERIFY(rig.settleCapture());
        QCOMPARE(rig.pane->size(), resized);
        QCOMPARE(rig.shown(QStringLiteral("a")), showingAtQuit);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), atQuit);
    }

    void operatorGripResizeIsSaved()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("grip.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(workspace({freeContainer(QStringLiteral("a"), kFirstRect)}), 0).status,
                 CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        auto* grip = rig.manager->container(QStringLiteral("a"))->findChild<QWidget*>(QStringLiteral("containerResizeGrip"));
        QVERIFY(grip);
        dragBy(grip, QPoint(40, 30));
        const QRect resized(kFirstRect.topLeft(), kFirstRect.size() + QSize(40, 30));
        QCOMPARE(rig.shown(QStringLiteral("a")), resized);
        QVERIFY(rig.settleCapture());
        QTRY_COMPARE_WITH_TIMEOUT(storedGeometry(store, QStringLiteral("a")), resized, kSaveTimeoutMs);
    }

    // Today's look: the window of one popped-out object follows its
    // content's height and has no resize corner (unchanged; the workspace
    // look alone lets the operator set its height).
    void lookOffAPoppedOutObjectsWindowIsAutoHeightWithNoCorner()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("shell.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        ContainerDocument a = freeContainer(QStringLiteral("a"), kFirstRect);
        ContentEntry entry = rig.registry.makeEntry(QStringLiteral("meter.signalMaxBin"));
        entry.id = QStringLiteral("a-meter");
        entry.context["sliceId"] = 0;
        a.contents.append(entry);
        ContentEntry other = rig.registry.makeEntry(QStringLiteral("meter.signalMaxBin"));
        other.id = QStringLiteral("a-meter-2");
        other.context["sliceId"] = 0;
        a.contents.append(other);
        QCOMPARE(rig.manager->commitWorkspace(workspace({a}), 0).status, CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        QVERIFY(rig.manager->arrangeController());
        QVERIFY(rig.manager->arrangeController()->popOut(QStringLiteral("a-meter")).ok);
        QVERIFY(rig.settleCapture());
        const ContainerDocument shell = store.snapshot().containers.last();
        QVERIFY(shell.popOutShell);
        QVERIFY(shell.autoHeight);
        ContainerWidget* c = rig.manager->container(shell.id);
        QVERIFY(c && c->isFloating());
        QVERIFY(c->autoHeight());
        QVERIFY(!c->operatorSizesHeight());
        auto* grip = c->findChild<QWidget*>(QStringLiteral("containerResizeGrip"));
        QVERIFY(grip);
        QVERIFY(!grip->isVisible());
        // A press and drag on it does nothing.
        const QSize before = c->window()->size();
        dragBy(grip, QPoint(0, 40));
        QCOMPARE(c->window()->size(), before);
        QVERIFY(c->autoHeight());
        QVERIFY(rig.settleCapture());
        QVERIFY(store.snapshot().containers.last().autoHeight);
        // Back in the main window before the rig goes (offscreen platform).
        rig.manager->recoverContainer(shell.id);
        QCoreApplication::processEvents();
    }

    // Cause 6: the first run with the look on copies the look-off layout,
    // which was measured for a pane of a different width.
    void lookOffLayoutCopiedForLookOnIsNotClamped()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("look.settings"));
        const QRect saved(600, 400, 300, 200);
        {
            AppSettings settings(path);
            ContainerWorkspaceStore store(settings);
            QCOMPARE(store.commit(workspace({freeContainer(QStringLiteral("a"), saved)}), 0).status, CommitStatus::Saved);
        }
        WorkspacePreview::setEnabledForTest(true);
        AppSettings settings(path);
        settings.load();
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        QCOMPARE(store.workspaceKey(), QStringLiteral("ContainerWorkspacePreview"));
        QCOMPARE(storedGeometry(store, QStringLiteral("a")), saved);
        QWidget dock;
        dock.resize(500, 700);
        ContainerContentRegistry registry;
        ContainerManager manager(&dock, nullptr);
        manager.setWorkspaceAdapter(&store, &registry);
        manager.restoreState();
        manager.saveState();
        ContainerWidget* a = manager.container(QStringLiteral("a"));
        QVERIFY(a);
        QVERIFY2(dock.rect().contains(a->geometry()), "shown inside the narrower pane");
        QCOMPARE(storedGeometry(store, QStringLiteral("a")), saved);
        if (settings.contains(QStringLiteral("ContainerWorkspacePreview"))) {
            const DocumentResult decoded = ContainerDocumentCodec::decode(
                settings.value(QStringLiteral("ContainerWorkspacePreview")).toString().toUtf8());
            QVERIFY2(decoded.ok, qPrintable(decoded.error));
            QCOMPARE(decoded.document.containers.first().geometry, saved);
        }
    }

    // A profile captured live holds remembered places, not what a small
    // window is showing.
    void profileCapturedLiveHoldsRememberedPlaces()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("profile.settings")));
        ContainerWorkspaceStore store(settings);
        Rig rig(store);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(workspace({freeContainer(QStringLiteral("a"), kFirstRect),
                                                        freeContainer(QStringLiteral("b"), kSecondRect)}), 0)
                     .status,
                 CommitStatus::Saved);
        QVERIFY(rig.settleCapture());
        QVERIFY(rig.resizePane(kSmallPaneSize));
        rig.manager->updateDockedPositions(rig.pane->width(), rig.pane->height());
        QVERIFY(rig.shown(QStringLiteral("b")) != kSecondRect);
        WorkspaceProfiles profiles(settings, rig.manager.get());
        QVERIFY(profiles.load().ok());
        QVERIFY(profiles.save().ok());
        const WorkspaceProfile* active = profiles.profile(profiles.activeId());
        QVERIFY(active);
        QCOMPARE(active->workspace.containers.size(), 2);
        QCOMPARE(active->workspace.containers[0].geometry, kFirstRect);
        QCOMPARE(active->workspace.containers[1].geometry, kSecondRect);
    }
};

QTEST_MAIN(TstContainerRememberedPlace)
#include "tst_container_remembered_place.moc"
