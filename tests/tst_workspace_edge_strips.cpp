// no-port-check: NereusSDR-original workspace preview four-edge strip regressions.
// Modification history (NereusSDR):
//   2026-10-26 - emptyStripTakesNoSpace sets the split at the 260 px
//                 floor and raises it only to the content's minimum, as
//                 the strip does; a longer default header title no longer
//                 moves the expected split. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-12 - Seed the baseline panel split at the 260 px container
//                 floor before raising its content minimum, matching
//                 production ordering. J.J. Boyd (KG4VCF), AI-assisted
//                 via OpenAI Codex.
//   2026-10-07 - Strip placement, Move to Edge, the drop preview and the
//                 edge drag commit and cancel for the workspace preview by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Strip minimum and default thickness, lock on strip handles,
//                 opaque free containers, pop-out size, and Reveal and Hidden
//                 inside strips, by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - The right strip and its handle take exactly today's Main
//                 Panel and splitter handle width; the fixture's middle
//                 splitter is the app's EdgeDockSplitter. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - The right strip's handle draws today's 3 px and grabs
//                 what today's 3 px handle grabs. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - The look has no Lock setting: a container committed locked
//                 is unlocked, its strip handles resize and Move to Edge
//                 moves it. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Headers are shown or hidden: a hidden header in a strip
//                 shows only while arranging. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-11 - The drop overlay and the Arrange outline are masked to the
//                 pixels they draw, so neither covers a native surface on
//                 Windows. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
#include <QtTest>
#include <QApplication>
#include <QMainWindow>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTemporaryDir>
#include <memory>
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// Offline fixture: a main window whose central widget is the four-edge
// area around a plain centre pane, in an EdgeDockSplitter as in
// MainWindow. No radio, discovery or poller.
struct EdgeFixture {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath("edge-strips.xml")};
    ContainerWorkspaceStore store{settings};
    ContainerContentRegistry registry;
    QMainWindow window;
    QSplitter* middle{nullptr};
    QWidget* center{nullptr};
    EdgeDockArea* area{nullptr};
    std::unique_ptr<ContainerManager> manager;

    EdgeFixture()
    {
        middle = new EdgeDockSplitter(Qt::Horizontal, &window);
        center = new QWidget(middle);
        center->setMinimumWidth(400);
        middle->addWidget(center);
        area = new EdgeDockArea(middle, center, &window);
        window.setCentralWidget(area);
        window.resize(1200, 800);
        manager = std::make_unique<ContainerManager>(center, middle);
        manager->setEdgeDockArea(area);
        manager->setWorkspaceAdapter(&store, &registry);
    }

    ContainerDocument container(const QString& id, DockEdge edge)
    {
        ContainerDocument result;
        result.id = id;
        result.name = id;
        result.layout = ContentLayout::VerticalStack;
        result.dockMode = DockMode::PanelDocked;
        result.edge = edge;
        result.header = HeaderMode::Shown;
        ContentEntry entry = registry.makeEntry(QStringLiteral("meter.signalMaxBin"));
        entry.id = id + QStringLiteral("-meter");
        entry.context["sliceId"] = 0;
        result.contents.append(entry);
        return result;
    }

    // A container of several stacked meters: taller than an end strip's
    // 112 px floor.
    ContainerDocument tallContainer(const QString& id, DockEdge edge, HeaderMode header)
    {
        ContainerDocument result = container(id, edge);
        result.header = header;
        for (const QString& type : {QStringLiteral("meter.comp"), QStringLiteral("meter.signalMaxBin")}) {
            ContentEntry entry = registry.makeEntry(type);
            entry.id = id + QStringLiteral("-") + type;
            entry.context["sliceId"] = 0;
            result.contents.append(entry);
        }
        return result;
    }

    bool commit(const QList<ContainerDocument>& containers)
    {
        WorkspaceDocument document = store.snapshot();
        document.containers = containers;
        document.mainContainerId = containers.first().id;
        const auto result = manager->commitWorkspace(document, document.revision);
        settle();
        return result.status == CommitStatus::Saved;
    }

    void show()
    {
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
    }

    QStringList stripIds(DockEdge edge) const
    {
        QStringList ids;
        for (QWidget* w : area->strip(edge)->containers()) {
            if (auto* c = qobject_cast<ContainerWidget*>(w)) {
                ids.append(c->id());
            }
        }
        return ids;
    }

    QStringList documentIds(DockEdge edge) const
    {
        QStringList ids;
        for (const auto& d : store.snapshot().containers) {
            if (d.dockMode == DockMode::PanelDocked && d.edge == edge) {
                ids.append(d.id);
            }
        }
        return ids;
    }

    const ContainerDocument* document(const QString& id) const
    {
        static ContainerDocument found;
        for (const auto& d : store.snapshot().containers) {
            if (d.id == id) {
                found = d;
                return &found;
            }
        }
        return nullptr;
    }
};

void press(QWidget* target, const QPoint& global)
{
    const QPoint local = target->mapFromGlobal(global);
    QMouseEvent event(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void move(QWidget* target, const QPoint& global)
{
    const QPoint local = target->mapFromGlobal(global);
    QMouseEvent event(QEvent::MouseMove, local, global, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void release(QWidget* target, const QPoint& global)
{
    const QPoint local = target->mapFromGlobal(global);
    QMouseEvent event(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

QPoint areaGlobal(EdgeDockArea* area, const QPoint& local)
{
    return area->mapToGlobal(local);
}

} // namespace

class TstWorkspaceEdgeStrips : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WorkspacePreview::setEnabledForTest(true); }
    void cleanupTestCase() { WorkspacePreview::setEnabledForTest(false); }

    void twoLeftContainersKeepOrderAndExtentsAcrossResize()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("b", DockEdge::Left),
                          f.container("main", DockEdge::Right)}));
        f.show();
        QCOMPARE(f.stripIds(DockEdge::Left), (QStringList{"a", "b"}));
        QCOMPARE(f.stripIds(DockEdge::Right), QStringList{"main"});
        EdgeDockStrip* left = f.area->strip(DockEdge::Left);
        QVERIFY(left->isVisible());
        QCOMPARE(left->width(), EdgeDockStrip::kSideThicknessPx);

        left->setExtents({400, 200});
        settle();
        const QList<int> before = left->extents();
        QVERIFY(before[0] > before[1]);
        const double ratio = double(before[0]) / before[1];

        f.window.resize(1500, 1000);
        settle();
        QCOMPARE(f.stripIds(DockEdge::Left), (QStringList{"a", "b"}));
        // The strip keeps its thickness; the centre takes the new room.
        QCOMPARE(left->width(), EdgeDockStrip::kSideThicknessPx);
        const QList<int> after = left->extents();
        QVERIFY(after[0] + after[1] > before[0] + before[1]);
        QVERIFY2(qAbs(double(after[0]) / after[1] - ratio) < 0.05,
                 qPrintable(QStringLiteral("%1/%2 vs %3").arg(after[0]).arg(after[1]).arg(ratio)));

        // Saved extents restore the same order and proportions.
        f.manager->saveState();
        const auto saved = f.store.snapshot().arrangement.strips.value(DockEdge::Left);
        QCOMPARE(saved.extents.value("a"), after[0]);
        QCOMPARE(saved.extents.value("b"), after[1]);
        QVERIFY(!f.store.snapshot().extensions.contains("splitterSizes"));
    }

    void emptyStripTakesNoSpace()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right)}));
        f.show();
        for (DockEdge edge : {DockEdge::Left, DockEdge::Top, DockEdge::Bottom}) {
            QVERIFY(f.area->strip(edge)->isHidden());
        }
        QCOMPARE(f.area->verticalSplitter()->sizes().at(0), 0);
        QCOMPARE(f.area->verticalSplitter()->sizes().at(2), 0);
        QCOMPARE(f.area->middleSplitter()->sizes().at(0), 0);
        // Only the right strip and its 3 px handle sit beside the centre.
        QCOMPARE(f.center->height(), f.area->height());
        QCOMPARE(f.center->width() + f.area->strip(DockEdge::Right)->width()
                     + EdgeDockStrip::kRightHandlePx, f.area->width());
        // Today's share: 256 of 1280.
        // Today's Main Panel width is the strip's width: the 3 px-handle
        // splitter's 1024/256 split with the panel raised to its minimum.
        // The right strip has no padding.
        f.window.resize(1512, 800);
        settle();
        // The split is set at the container's 260 px floor; the content's
        // own minimum (its header title included) is applied after it, as
        // the strip's floor (EdgeDockArea::migratedRightThickness).
        constexpr int kPadding = 2 * EdgeDockStrip::kRightPaddingPx;
        const int minimum = qMax(f.manager->container("main")->minimumWidth(),
                                 f.manager->container("main")->minimumSizeHint().width());
        const int panel = qMax(256, ContainerWidget::kMinContainerWidth);
        const int split = int(std::lround((f.area->width() - 3) * double(panel) / (1024 + panel)));
        // The value the 1512 px preview profile shows today.
        QCOMPARE(split, 306);
        const int share = qMax(split, minimum);
        QVERIFY2(qAbs(f.area->strip(DockEdge::Right)->width() - (share + kPadding)) <= 2,
                 qPrintable(QStringLiteral("%1 vs %2").arg(f.area->strip(DockEdge::Right)->width())
                                .arg(share + kPadding)));
        QVERIFY(qAbs(f.manager->container("main")->width() - share) <= 2);
    }

    // JJ's choice (2026-10-08): the right strip and its handle take
    // exactly the width today's Main Panel and splitter handle take, so
    // the spectrum and the Main Panel come out the same size as today and
    // the spectrum area is pixel-comparable with the baseline. "Today" is
    // the switch-off layout itself: a 3 px-handle QSplitter of spectrum
    // and a panel initially at the 260 px container floor, setSizes({1024,
    // 256}), followed by the queued content minimum raise.
    void rightStripTakesTodaysPanelWidthExactly()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right)}));
        f.show();
        ContainerWidget* main = f.manager->container("main");
        EdgeDockStrip* right = f.area->strip(DockEdge::Right);
        const int minimum = qMax(main->minimumWidth(), main->minimumSizeHint().width());

        QWidget today;
        today.setAttribute(Qt::WA_DontShowOnScreen);
        auto* splitter = new QSplitter(Qt::Horizontal, &today);
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(EdgeDockArea::kMigratedHandlePx);
        auto* spectrum = new QWidget(splitter);
        spectrum->setMinimumWidth(f.center->minimumWidth());
        auto* panel = new QWidget(splitter);
        panel->setMinimumWidth(ContainerWidget::kMinContainerWidth);
        splitter->addWidget(spectrum);
        splitter->addWidget(panel);
        splitter->setSizes({EdgeDockArea::kMigratedTotalShare - EdgeDockArea::kMigratedPanelShare,
                            EdgeDockArea::kMigratedPanelShare});
        today.show();

        // The window widths of the visual review profiles.
        for (int width : {1280, 1512, 1920}) {
            f.window.resize(width, 800);
            settle();
            today.resize(f.area->width(), 800);
            splitter->setGeometry(today.rect());
            // Today's initial split precedes the queued content minimum
            // raise (ContainerManager); a larger settled minimum floors
            // that split rather than changing its initial proportions.
            panel->setMinimumWidth(ContainerWidget::kMinContainerWidth);
            splitter->setSizes({EdgeDockArea::kMigratedTotalShare - EdgeDockArea::kMigratedPanelShare,
                                EdgeDockArea::kMigratedPanelShare});
            panel->setMinimumWidth(minimum);
            splitter->refresh();
            settle();
            const QString at = QStringLiteral("at %1 px").arg(width);
            // No padding: the Main Panel's content is the whole strip.
            QCOMPARE(main->width(), right->width());
            QVERIFY2(main->width() == panel->width(),
                     qPrintable(QStringLiteral("panel %1 vs today's %2 %3 (minimum %4, declared %5, hint %6)")
                                    .arg(main->width()).arg(panel->width()).arg(at)
                                    .arg(minimum).arg(main->minimumWidth()).arg(main->minimumSizeHint().width())));
            // The handle on the spectrum side is today's 3 px.
            QSplitterHandle* handle = f.middle->handle(f.middle->indexOf(right));
            QVERIFY(handle != nullptr && handle->isVisible());
            // It draws today's 3 px and grabs what today's handle grabs:
            // the same geometry around the same split, mouse input over
            // all of it, and the same drawn (masked) part.
            QSplitterHandle* todayHandle = splitter->handle(1);
            QCOMPARE(handle->sizeHint().width(), EdgeDockArea::kMigratedHandlePx);
            QCOMPARE(handle->contentsRect().width(), EdgeDockArea::kMigratedHandlePx);
            QCOMPARE(handle->width(), todayHandle->width());
            QCOMPARE(handle->x() - f.center->geometry().right(),
                     todayHandle->x() - spectrum->geometry().right());
            QCOMPARE(handle->contentsRect(), todayHandle->contentsRect());
            QCOMPARE(handle->testAttribute(Qt::WA_MouseNoMask),
                     todayHandle->testAttribute(Qt::WA_MouseNoMask));
            QCOMPARE(handle->mask(), todayHandle->mask());
            QVERIFY2(f.center->width() == spectrum->width(),
                     qPrintable(QStringLiteral("spectrum %1 vs today's %2 %3")
                                    .arg(f.center->width()).arg(spectrum->width()).arg(at)));
            QCOMPARE(right->mapTo(f.area, QPoint()).x(), panel->x());
        }
        // The value the 1512 px preview profile shows today (Qt 6.11): the
        // spectrum 1203 px, the handle 3, the Main Panel 306 from x 1206.
        if (minimum == 260) {
            f.window.resize(1512, 800);
            settle();
            QCOMPARE(f.area->width(), 1512);
            QCOMPARE(f.center->width(), 1203);
            QCOMPARE(right->mapTo(f.area, QPoint()).x(), 1206);
            QCOMPARE(main->width(), 306);
        }
        // A dragged thickness still wins.
        f.area->setThickness(DockEdge::Right, 400);
        settle();
        QCOMPARE(right->width(), 400);
        QCOMPARE(main->width(), 400);
        QCOMPARE(f.center->width(), f.area->width() - 400 - EdgeDockStrip::kRightHandlePx);
    }

    void sideStripNeverClipsItsWidestContainer()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("main", DockEdge::Right)}));
        f.show();
        ContainerWidget* a = f.manager->container("a");
        EdgeDockStrip* left = f.area->strip(DockEdge::Left);
        int widest = qMax(a->minimumWidth(), a->minimumSizeHint().width());
        if (auto* scroll = a->contentHolder()->findChild<QScrollArea*>(); scroll && scroll->widget()) {
            widest = qMax(widest, scroll->widget()->minimumSizeHint().width());
        }
        constexpr int kPadding = 2 * EdgeDockStrip::kPaddingPx;
        QCOMPARE(left->minimumWidth(), widest + kPadding);
        // A thickness below the minimum (saved, or dragged) never clips it.
        f.area->setThickness(DockEdge::Left, 100);
        settle();
        QVERIFY2(left->width() >= widest + kPadding, qPrintable(QString::number(left->width())));
        QVERIFY(a->width() >= widest);
        QCOMPARE(f.area->thickness(DockEdge::Left), widest + kPadding);
        // Dragging the area handle cannot go below it either.
        QSplitterHandle* handle = f.area->middleSplitter()->handle(1);
        const QPoint grab = handle->mapToGlobal(handle->rect().center());
        press(handle, grab);
        move(handle, grab - QPoint(200, 0));
        release(handle, grab - QPoint(200, 0));
        settle();
        QVERIFY2(left->width() >= widest + kPadding, qPrintable(QString::number(left->width())));
    }

    void endStripDefaultShowsTallestContainerWhole()
    {
        constexpr int kPadding = 2 * EdgeDockStrip::kPaddingPx;
        for (HeaderMode header : {HeaderMode::Shown, HeaderMode::Hidden}) {
            EdgeFixture f;
            QVERIFY(f.commit({f.tallContainer("top", DockEdge::Top, header),
                              f.container("main", DockEdge::Right)}));
            f.show();
            ContainerWidget* top = f.manager->container("top");
            const int content = f.manager->contentHost("top")->preferredContentHeight();
            const int headerExtent = top->headerExtent();
            // A hidden header reserves no room.
            QCOMPARE(headerExtent, header == HeaderMode::Shown ? WorkspaceGroupHeader::kHeight : 0);
            QVERIFY2(content + headerExtent + kPadding > EdgeDockStrip::kEndThicknessPx,
                     qPrintable(QString::number(content)));
            EdgeDockStrip* strip = f.area->strip(DockEdge::Top);
            QCOMPARE(strip->height(), content + headerExtent + kPadding);
            QVERIFY(top->contentHolder()->height() >= content);
            // Made smaller by the user, the content scrolls instead of clipping.
            const int smaller = content + headerExtent + kPadding - 30;
            f.area->setThickness(DockEdge::Top, smaller);
            settle();
            QCOMPARE(strip->height(), smaller);
            auto* scroll = top->contentHolder()->findChild<QScrollArea*>();
            QVERIFY(scroll != nullptr);
            QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        }
        // 112 px stays the least a short container starts at.
        EdgeFixture f;
        QVERIFY(f.commit({f.container("top", DockEdge::Top), f.container("main", DockEdge::Right)}));
        f.show();
        const int content = f.manager->contentHost("top")->preferredContentHeight()
            + f.manager->container("top")->headerExtent() + kPadding;
        QCOMPARE(f.area->strip(DockEdge::Top)->height(),
                 qMax(int(EdgeDockStrip::kEndThicknessPx), content));
    }

    // The look has no Lock setting: a container committed locked is
    // unlocked, and the handles beside it resize.
    void lockedContainerDoesNotBlockStripHandles()
    {
        EdgeFixture f;
        ContainerDocument a = f.container("a", DockEdge::Left);
        a.locked = true;
        QVERIFY(f.commit({a, f.container("b", DockEdge::Left), f.container("main", DockEdge::Right)}));
        f.show();
        QVERIFY(!f.document("a")->locked);
        QVERIFY(!f.manager->container("a")->isLocked());
        EdgeDockStrip* left = f.area->strip(DockEdge::Left);
        const auto drag = [&](QSplitterHandle* handle, const QPoint& by) {
            const QPoint grab = handle->mapToGlobal(handle->rect().center());
            press(handle, grab);
            move(handle, grab + by);
            release(handle, grab + by);
            settle();
        };
        const QList<int> before = left->extents();
        drag(left->splitter()->handle(1), QPoint(0, 80));
        QVERIFY(left->extents() != before);
        // The area handle beside the strip resizes it too.
        const int width = left->width();
        drag(f.area->middleSplitter()->handle(1), QPoint(80, 0));
        QVERIFY(left->width() != width);
    }

    void freeContainerPaintsItsWholeRect()
    {
        EdgeFixture f;
        f.center->setAutoFillBackground(true);
        QPalette red = f.center->palette();
        red.setColor(QPalette::Window, Qt::red);
        f.center->setPalette(red);
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("main", DockEdge::Right)}));
        f.show();
        f.manager->placeContainerFree("a");
        settle();
        // Taller than its content, so part of it holds no content at all.
        WorkspaceDocument document = f.store.snapshot();
        for (auto& d : document.containers) {
            if (d.id == "a") {
                d.autoHeight = false;
                d.geometry = QRect(40, 40, 300, 420);
            }
        }
        QCOMPARE(f.manager->commitWorkspace(document, document.revision).status, CommitStatus::Saved);
        settle();
        ContainerWidget* a = f.manager->container("a");
        QCOMPARE(a->parentWidget(), f.center);
        QCOMPARE(a->geometry(), QRect(40, 40, 300, 420));
        const QImage image = f.window.grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        const QRect r = a->geometry().translated(f.center->mapTo(&f.window, QPoint()));
        // Its corners, outside the plate's radius, and inside it: its
        // edges, its centre, its foot.
        constexpr int kInset = 12;
        for (const QPoint& p : {QPoint(r.left() + 1, r.bottom() - 1), QPoint(r.right() - 1, r.bottom() - 1),
                                QPoint(r.left() + kInset, r.bottom() - kInset),
                                QPoint(r.right() - kInset, r.bottom() - kInset),
                                QPoint(r.center().x(), r.bottom() - kInset),
                                QPoint(r.center().x(), r.center().y() + 100),
                                QPoint(r.left() + kInset, r.center().y())}) {
            const QColor c = image.pixelColor(QPoint(int(p.x() * dpr), int(p.y() * dpr)));
            QVERIFY2(c.red() < 128, qPrintable(QStringLiteral("%1,%2 %3")
                                                   .arg(p.x()).arg(p.y()).arg(c.name())));
        }
    }

    void popOutFromAStripKeepsTheContainerSize()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("main", DockEdge::Right)}));
        f.show();
        ContainerWidget* a = f.manager->container("a");
        const int width = a->width();
        const int preferred = f.manager->contentHost("a")->preferredContentHeight()
            + WorkspaceGroupHeader::kHeight;
        f.manager->floatContainer("a");
        settle();
        const ContainerDocument* d = f.document("a");
        QCOMPARE(d->dockMode, DockMode::Floating);
        QCOMPARE(d->geometry.width(), width);
        QVERIFY2(d->geometry.height() <= qMax(120, preferred),
                 qPrintable(QString::number(d->geometry.height())));
        QVERIFY(a->window() != &f.window);
        QCOMPARE(a->window()->size(), d->geometry.size());
        // Docked back before teardown: see the Task 4 report on the Qt
        // offscreen plugin's clearHash.
        f.manager->moveContainerToEdge("a", DockEdge::Left);
        settle();
        QCOMPARE(f.document("a")->dockMode, DockMode::PanelDocked);
    }

    void hiddenHeadersShowOnlyWhileArrangingInsideStrips()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.tallContainer("h", DockEdge::Top, HeaderMode::Hidden),
                          f.tallContainer("arr", DockEdge::Top, HeaderMode::Hidden),
                          f.container("main", DockEdge::Right)}));
        f.show();
        const int thickness = f.area->strip(DockEdge::Top)->height();
        ContainerWidget* h = f.manager->container("h");
        ContainerWidget* arr = f.manager->container("arr");
        for (ContainerWidget* c : {h, arr}) {
            QCOMPARE(c->headerExtent(), 0);
            QVERIFY(!c->chromeVisible());
        }
        const QPoint contentTop = h->contentHolder()->mapTo(h, QPoint());

        // Hovering the top of a hidden header shows nothing.
        const auto hover = [](QWidget* target, const QPoint& position) {
            QMouseEvent event(QEvent::MouseMove, position, target->mapToGlobal(position),
                              Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(target, &event);
        };
        hover(h, QPoint(40, 6));
        QTest::qWait(400);
        QVERIFY(!h->chromeVisible());
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), thickness);

        // Arrange draws them over the content, and turning Arrange off
        // hides them; the strip never changes thickness.
        f.manager->setArrangeMode(true);
        settle();
        QVERIFY(h->chromeVisible());
        QVERIFY(arr->chromeVisible());
        QCOMPARE(h->contentHolder()->mapTo(h, QPoint()), contentTop);
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), thickness);
        f.manager->setArrangeMode(false);
        settle();
        QVERIFY(!h->chromeVisible());
        QVERIFY(!arr->chromeVisible());
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), thickness);
    }

    void moveToEdgeReordersAndCommits()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("b", DockEdge::Left),
                          f.container("main", DockEdge::Right)}));
        f.show();
        f.manager->moveContainerToEdge("b", DockEdge::Left, 0);
        settle();
        QCOMPARE(f.documentIds(DockEdge::Left), (QStringList{"b", "a"}));
        QCOMPARE(f.stripIds(DockEdge::Left), (QStringList{"b", "a"}));
        f.manager->moveContainerToEdge("a", DockEdge::Top);
        settle();
        QCOMPARE(f.stripIds(DockEdge::Top), QStringList{"a"});
        QVERIFY(f.area->strip(DockEdge::Top)->isVisible());
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), EdgeDockStrip::kEndThicknessPx);
        QCOMPARE(f.document("a")->edge, DockEdge::Top);
        // Place Free: over the centre pane, out of every strip.
        f.manager->placeContainerFree("b");
        settle();
        QCOMPARE(f.document("b")->dockMode, DockMode::OverlayDocked);
        QCOMPARE(f.manager->container("b")->parentWidget(), f.center);
        QVERIFY(f.area->strip(DockEdge::Left)->isHidden());
        // The look has no Lock setting: a container committed locked moves.
        WorkspaceDocument locked = f.store.snapshot();
        for (auto& d : locked.containers) {
            d.locked = d.id == "main";
        }
        QCOMPARE(f.manager->commitWorkspace(locked, locked.revision).status, CommitStatus::Saved);
        settle();
        QVERIFY(!f.document("main")->locked);
        f.manager->moveContainerToEdge("main", DockEdge::Left);
        settle();
        QCOMPARE(f.document("main")->edge, DockEdge::Left);
    }

    void dropPreviewSeamShowsBetweenContainers()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right), f.container("b", DockEdge::Right)}));
        f.show();
        f.manager->showDropPreviewForTest(DockEdge::Right, 1);
        settle();
        QVERIFY(f.area->targetBandsVisible());
        QVERIFY(f.area->dropPreviewVisible());
        const QRect preview = f.area->dropPreviewRect();
        const QRect first(f.manager->container("main")->mapTo(f.area, QPoint()), f.manager->container("main")->size());
        const QRect second(f.manager->container("b")->mapTo(f.area, QPoint()), f.manager->container("b")->size());
        QCOMPARE(preview.height(), EdgeDockStrip::kInsertPreviewPx);
        QVERIFY(preview.center().y() > first.top() && preview.center().y() < second.bottom());
        QVERIFY(qAbs(preview.center().y() - (first.bottom() + second.top()) / 2) <= 2);
        QVERIFY(f.area->strip(DockEdge::Right)->geometry().contains(preview.center()));

        // An empty edge previews the strip it would open, inside its padding.
        f.manager->showDropPreviewForTest(DockEdge::Left, 0);
        settle();
        QCOMPARE(f.area->dropPreviewRect().left(), EdgeDockStrip::kPaddingPx);
        QCOMPARE(f.area->dropPreviewRect().width(), EdgeDockStrip::kSideThicknessPx - 2 * EdgeDockStrip::kPaddingPx);
        QCOMPARE(f.area->targetBandRect(DockEdge::Left), QRect(0, 0, EdgeDockArea::kTargetBandPx, f.area->height()));

        f.manager->clearDropPreviewForTest();
        QVERIFY(!f.area->dropPreviewVisible());
        QVERIFY(!f.area->targetBandsVisible());
        // Arrange shows the bands without a preview.
        f.manager->setArrangeMode(true);
        QVERIFY(f.area->targetBandsVisible());
        QVERIFY(!f.area->dropPreviewVisible());
        f.manager->setArrangeMode(false);
        QVERIFY(!f.area->targetBandsVisible());
    }

    // The see-through overlays cover only the pixels they draw. Beside a
    // native surface (the spectrum, a meter) Qt gives each of them a native
    // window, and on Windows that window is not blended with the surface
    // under it: an overlay over the whole pane blanked the spectrum for all
    // of Arrange.
    void dropOverlayCoversOnlyItsBandsAndPreview()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right), f.container("b", DockEdge::Right)}));
        f.show();
        auto* overlay = f.area->findChild<QWidget*>(QStringLiteral("edgeDropOverlay"));
        QVERIFY(overlay);
        QVERIFY(!overlay->isVisible());
        const auto bands = [&f] {
            QRegion region;
            for (DockEdge edge : {DockEdge::Left, DockEdge::Right, DockEdge::Top, DockEdge::Bottom}) {
                region += f.area->targetBandRect(edge);
            }
            return region;
        };
        const auto paneCenter = [&f] { return f.center->mapTo(f.area, f.center->rect().center()); };

        f.manager->setArrangeMode(true);
        settle();
        QVERIFY(overlay->isVisible());
        QCOMPARE(overlay->geometry(), f.area->rect());
        QCOMPARE(overlay->mask(), bands());
        QVERIFY(!overlay->mask().contains(paneCenter()));
        // Every container's inside is clear of it too.
        for (const char* id : {"main", "b"}) {
            ContainerWidget* c = f.manager->container(id);
            QVERIFY(!overlay->mask().contains(c->mapTo(f.area, c->rect().center())));
        }

        // The mask follows the area's size.
        f.window.resize(1000, 700);
        settle();
        QCOMPARE(overlay->geometry(), f.area->rect());
        QCOMPARE(overlay->mask(), bands());
        QVERIFY(overlay->mask().contains(QPoint(f.area->width() - 1, f.area->height() - 1)));
        QVERIFY(!overlay->mask().contains(paneCenter()));

        // With a preview: the bands and the preview's rectangle, no more.
        f.manager->showDropPreviewForTest(DockEdge::Right, 1);
        settle();
        const QRect preview = f.area->dropPreviewRect();
        QVERIFY(preview.isValid());
        QCOMPARE(overlay->mask(), bands() + preview);
        QVERIFY(!overlay->mask().contains(paneCenter()));
        f.manager->showDropPreviewForTest(DockEdge::Left, 0);
        settle();
        QVERIFY(f.area->dropPreviewRect() != preview);
        QCOMPARE(overlay->mask(), bands() + f.area->dropPreviewRect());
        f.manager->clearDropPreviewForTest();
        settle();
        QCOMPARE(overlay->mask(), bands());

        // Everything it draws lies inside the mask: painted without the
        // mask, no pixel outside it is touched.
        f.manager->showDropPreviewForTest(DockEdge::Right, 1);
        settle();
        const QRegion shown = overlay->mask();
        QImage image(overlay->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        overlay->clearMask();
        overlay->render(&image, QPoint(), QRegion(), QWidget::RenderFlags());
        overlay->setMask(shown);
        int inside = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) == 0) {
                    continue;
                }
                QVERIFY2(shown.contains(QPoint(x, y)),
                         qPrintable(QStringLiteral("drawn outside the mask at %1,%2").arg(x).arg(y)));
                ++inside;
            }
        }
        QVERIFY(inside > 0);
        f.manager->clearDropPreviewForTest();

        // Nothing to draw: hidden, not a full-size see-through window.
        f.manager->setArrangeMode(false);
        settle();
        QVERIFY(!overlay->isVisible());

        // A drag outside Arrange shows the same bands and preview.
        f.manager->showDropPreviewForTest(DockEdge::Right, 1);
        settle();
        QVERIFY(overlay->isVisible());
        QCOMPARE(overlay->mask(), bands() + f.area->dropPreviewRect());
        QVERIFY(!overlay->mask().contains(paneCenter()));
        f.manager->clearDropPreviewForTest();
        settle();
        QVERIFY(!overlay->isVisible());
    }

    void arrangeOutlineCoversOnlyItsFrame()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right), f.container("b", DockEdge::Right)}));
        f.show();
        ContainerWidget* c = f.manager->container("main");
        auto* outline = c->findChild<QWidget*>(QStringLiteral("containerArrangeOutline"));
        QVERIFY(outline);
        QVERIFY(!outline->isVisible());
        const auto frame = [](const QRect& r) {
            // 1 px at the left and top, 2 px at the right and bottom: on a
            // 2x screen the pen along the far edges reaches half a pixel in.
            return QRegion(r).subtracted(QRegion(r.adjusted(1, 1, -2, -2)));
        };

        f.manager->setArrangeMode(true);
        settle();
        QVERIFY(outline->isVisible());
        QCOMPARE(outline->geometry(), c->rect());
        QCOMPARE(outline->mask(), frame(outline->rect()));
        QVERIFY(!outline->mask().contains(outline->rect().center()));
        QVERIFY(outline->mask().contains(QPoint(0, 0)));
        QVERIFY(outline->mask().contains(outline->rect().bottomRight()));
        QVERIFY(!outline->mask().contains(QPoint(1, 1)));
        QVERIFY(!outline->mask().contains(outline->rect().bottomRight() - QPoint(2, 2)));

        // The frame follows the container's size.
        const QSize before = outline->size();
        f.area->setThickness(DockEdge::Right, f.area->strip(DockEdge::Right)->width() + 60);
        settle();
        QVERIFY(outline->size() != before);
        QCOMPARE(outline->geometry(), c->rect());
        QCOMPARE(outline->mask(), frame(outline->rect()));

        // Everything it draws lies inside the frame.
        QImage image(outline->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        const QRegion shown = outline->mask();
        outline->clearMask();
        outline->render(&image, QPoint(), QRegion(), QWidget::RenderFlags());
        outline->setMask(shown);
        int inside = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) == 0) {
                    continue;
                }
                QVERIFY2(shown.contains(QPoint(x, y)),
                         qPrintable(QStringLiteral("drawn outside the frame at %1,%2").arg(x).arg(y)));
                ++inside;
            }
        }
        QVERIFY(inside > 0);

        // The resize corner and edges stay above the outline.
        auto* grip = c->findChild<QWidget*>(QStringLiteral("containerResizeGrip"));
        QVERIFY(grip);
        const QObjectList& siblings = c->children();
        if (grip->isVisible()) {
            QVERIFY(siblings.indexOf(grip) > siblings.indexOf(outline));
        }

        f.manager->setArrangeMode(false);
        settle();
        QVERIFY(!outline->isVisible());
    }

    void dropTargetsResolveEdgesStripsAndDetach()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right), f.container("b", DockEdge::Right)}));
        f.show();
        const QRect r = f.area->rect();
        auto target = f.area->dropTargetAt(areaGlobal(f.area, r.center() - QPoint(150, 0)));
        QVERIFY(!target.valid);
        QVERIFY(!target.detach);
        target = f.area->dropTargetAt(areaGlobal(f.area, QPoint(10, r.center().y())));
        QVERIFY(target.valid);
        QCOMPARE(target.edge, DockEdge::Left);
        QCOMPARE(target.index, 0);
        target = f.area->dropTargetAt(areaGlobal(f.area, QPoint(r.center().x() - 150, r.height() - 10)));
        QVERIFY(target.valid);
        QCOMPARE(target.edge, DockEdge::Bottom);
        const QRect firstRight(f.manager->container("main")->mapTo(f.area, QPoint()), f.manager->container("main")->size());
        target = f.area->dropTargetAt(areaGlobal(f.area, firstRight.center() + QPoint(0, firstRight.height() / 2 - 2)));
        QVERIFY(target.valid);
        QCOMPARE(target.edge, DockEdge::Right);
        QCOMPARE(target.index, 1);
        target = f.area->dropTargetAt(f.window.mapToGlobal(QPoint(f.window.width() + 200, 40)));
        QVERIFY(!target.valid);
        QVERIFY(target.detach);
    }

    void headerDragCommitsOnlyOnAValidRelease()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right), f.container("b", DockEdge::Right)}));
        f.show();
        ContainerWidget* b = f.manager->container("b");
        QWidget* header = b->workspaceHeader();
        QVERIFY(header);
        const QPoint grab = header->mapToGlobal(QPoint(header->width() / 2, header->height() / 2));
        const QPoint nearTop = areaGlobal(f.area, QPoint(f.area->width() / 3, 12));
        const QPoint middleOfCenter = areaGlobal(f.area, QPoint(f.area->width() / 3, f.area->height() / 2));
        const quint64 revision = f.store.snapshot().revision;

        // Escape cancels: no commit, preview gone.
        press(header, grab);
        move(header, grab + QPoint(0, 40));
        move(header, nearTop);
        QVERIFY(f.area->dropPreviewVisible());
        QVERIFY(f.area->targetBandsVisible());
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(b, &escape);
        release(header, nearTop);
        settle();
        QVERIFY(!f.area->dropPreviewVisible());
        QVERIFY(!f.area->targetBandsVisible());
        QCOMPARE(f.store.snapshot().revision, revision);
        QCOMPARE(f.document("b")->edge, DockEdge::Right);

        // A release with no valid destination restores.
        press(header, grab);
        move(header, grab + QPoint(0, 40));
        move(header, middleOfCenter);
        QVERIFY(!f.area->dropPreviewVisible());
        release(header, middleOfCenter);
        settle();
        QCOMPARE(f.store.snapshot().revision, revision);
        QCOMPARE(f.stripIds(DockEdge::Right), (QStringList{"main", "b"}));

        // A release on the preview commits.
        press(header, grab);
        move(header, grab + QPoint(0, 40));
        move(header, nearTop);
        QVERIFY(f.area->dropPreviewVisible());
        release(header, nearTop);
        settle();
        QVERIFY(!f.area->dropPreviewVisible());
        QCOMPARE(f.document("b")->edge, DockEdge::Top);
        QCOMPARE(f.stripIds(DockEdge::Top), QStringList{"b"});

        // Pulled out of the window: a floating shell.
        header = f.manager->container("b")->workspaceHeader();
        const QPoint grabTop = header->mapToGlobal(QPoint(header->width() / 2, header->height() / 2));
        const QPoint outside = f.window.mapToGlobal(QPoint(f.window.width() + 300, 100));
        press(header, grabTop);
        move(header, grabTop + QPoint(40, 0));
        move(header, outside);
        release(header, outside);
        settle();
        QCOMPARE(f.document("b")->dockMode, DockMode::Floating);
        QVERIFY(f.area->strip(DockEdge::Top)->isHidden());
        // Move to Edge brings it back to a strip.
        f.manager->moveContainerToEdge("b", DockEdge::Left);
        settle();
        QCOMPARE(f.document("b")->dockMode, DockMode::PanelDocked);
        QCOMPARE(f.stripIds(DockEdge::Left), QStringList{"b"});
    }

    void gridAlignmentIsSavedAndSnaps()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right)}));
        QVERIFY(!f.manager->gridAlignment());
        QSignalSpy changed(f.manager.get(), &ContainerManager::gridAlignmentChanged);
        f.manager->setGridAlignment(true);
        QCOMPARE(changed.count(), 1);
        QVERIFY(f.store.snapshot().arrangement.gridAlignment);
        QCOMPARE(f.manager->container("main")->gridAlignment(), kWorkspaceSnapGridPx);
        QCOMPARE(workspaceSnappedTopLeft(QPoint(13, 21)), QPoint(16, 24));
        QCOMPARE(workspaceSnappedTopLeft(QPoint(11, 3)), QPoint(8, 0));
        QCOMPARE(workspaceSnappedTopLeft(QPoint(13, 21), 1), QPoint(13, 21));
    }

    void switchOffKeepsTheSplitter()
    {
        WorkspacePreview::setEnabledForTest(false);
        {
            EdgeFixture f;
            QVERIFY(f.commit({f.container("main", DockEdge::Left)}));
            QCOMPARE(f.manager->container("main")->parentWidget(), f.middle);
            QCOMPARE(f.stripIds(DockEdge::Left), QStringList{});
        }
        WorkspacePreview::setEnabledForTest(true);
    }
};

QTEST_MAIN(TstWorkspaceEdgeStrips)
#include "tst_workspace_edge_strips.moc"
