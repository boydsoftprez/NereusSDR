// no-port-check: NereusSDR-original native grips, reserved chrome and ownership regressions.
// Modification history (NereusSDR):
//   2026-10-03 — Deterministic widget-local hover delivery by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QWindow>
#include <QMenu>
#include <QDrag>
#include <QTimer>
#include <QScreen>
#include <QMouseEvent>
#include "core/AppSettings.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
using namespace NereusSDR;
class TstContainerChrome : public QObject
{
    Q_OBJECT
    void capture(QWidget* window, const QString& name)
    {
        QTest::qWait(80);
        for (auto* meter : window->findChildren<MeterWidget*>()) {
            if (!meter->isVisible()) {
                continue;
            }
#ifdef NEREUS_GPU_SPECTRUM
            QVERIFY(meter->windowHandle());
#ifdef Q_OS_MAC
            QCOMPARE(meter->windowHandle()->surfaceType(), QSurface::MetalSurface);
#endif

            QSignalSpy frames(meter, &QRhiWidget::frameSubmitted);
            meter->update();
            QTRY_VERIFY_WITH_TIMEOUT(frames.count() > 0, 3000);
            QVERIFY(!meter->grabFramebuffer().isNull());
#endif
        }
        const QString dir = qEnvironmentVariable("TASK7_CAPTURE_DIR");
        if (dir.isEmpty()) {
            return;
        }
        QVERIFY(QDir().mkpath(dir));
        const QPixmap pixels = window->grab();
        QVERIFY(!pixels.isNull());
        QVERIFY(pixels.save(dir + "/" + name + ".png"));
    }
  private slots:
    void reservedHeaderFocusAndHiddenRecovery()
    {
        ContainerWidget c;
        QPushButton content("An ordinary control");
        c.setContent(&content);
        c.setHeaderMode(HeaderMode::Reveal);
        c.setDockMode(DockMode::OverlayDocked);
        c.resize(420, 260);
        c.show();
        QVERIFY(QTest::qWaitForWindowExposed(&c));
        QApplication::setActiveWindow(&c);
        content.setFocus();
        QTRY_VERIFY(content.hasFocus());
        QTRY_VERIFY(c.chromeVisible());
        const QRect geometry = content.geometry();
        const QPoint position = content.mapTo(&c, QPoint());
        QWidget other;
        other.show();
        other.setFocus();
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&c, &leave);
        QCoreApplication::processEvents();
        QCOMPARE(content.geometry(), geometry);
        QCOMPARE(content.mapTo(&c, QPoint()), position);
        // Deliver the hover to this fixture without depending on the
        // physical pointer or another native window owning desktop focus.
        const QPoint hoverPosition(10, 10);
        QMouseEvent hover(QEvent::MouseMove, hoverPosition,
                          c.mapToGlobal(hoverPosition), Qt::NoButton,
                          Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&c, &hover);
        QVERIFY(c.chromeVisible());
        QCOMPARE(content.mapTo(&c, QPoint()), position);
        c.setNoControls(true);
        c.setHeaderMode(HeaderMode::Hidden);
        QVERIFY(!c.chromeVisible());
        c.setLocked(true);
        c.recoverChrome();
        QVERIFY(c.chromeVisible());
        QCOMPARE(content.mapTo(&c, QPoint()), position);
        QSignalSpy settings(&c, &ContainerWidget::settingsRequested);
        for (auto* button : c.findChildren<QPushButton*>()) {
            if (button->toolTip() == "Container settings") {
                QTest::mouseClick(button, Qt::LeftButton);
            }
        }
        QCOMPARE(settings.count(), 1);
        auto* resizeGrip=c.findChild<QWidget*>("containerResizeGrip");QVERIFY(resizeGrip);QVERIFY(!resizeGrip->isVisible());
        const QSize lockedSize=c.size();QTest::mousePress(resizeGrip,Qt::LeftButton);QTest::mouseMove(resizeGrip,QPoint(80,80));QTest::mouseRelease(resizeGrip,Qt::LeftButton);QCOMPARE(c.size(),lockedSize);
        c.setHeaderMode(HeaderMode::Hidden);
        QTest::keyPress(&c, Qt::Key_Shift);
        QCoreApplication::processEvents();
        QVERIFY(c.chromeVisible());
        QTest::keyRelease(&c, Qt::Key_Shift);
        c.setHeaderMode(HeaderMode::Always);
        QVERIFY(c.chromeVisible());
        c.setHeaderMode(HeaderMode::Reveal);
        content.setFocus();
        QCoreApplication::processEvents();
        QVERIFY(c.chromeVisible());
        // Detach a borrowed stack fixture before its owner's QWidget teardown.
        content.setParent(nullptr);
    }
    void nativeMixedGripsDropPopReturnAndLegacyBounds()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        QWidget window;
        window.setWindowTitle("Nereus Containers — native arrangement milestone");
        window.resize(820, 700);
        auto* layout = new QVBoxLayout(&window);
        auto* splitter = new QSplitter(&window);
        layout->addWidget(splitter);
        auto* pane = new QWidget(splitter);
        pane->setMinimumWidth(280);
        splitter->addWidget(pane);
        ContainerContentRegistry registry;
        QPushButton borrowed("RX applet — explicit control");
        borrowed.setMinimumHeight(64);
        registry.attachSingleton("applet:rx", &borrowed);
        int clicks = 0;
        connect(&borrowed, &QPushButton::clicked, this, [&] { ++clicks; });
        MeterPoller poller;
        ContainerManager manager(pane, splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        connect(&manager, &ContainerManager::meterReadyForPolling, &poller,
                &MeterPoller::addTarget);
        poller.start();
        WorkspaceDocument d;
        d.mainContainerId = "main";
        ContainerDocument main;
        main.id = "main";
        main.name = "Mixed objects";
        main.layout = ContentLayout::VerticalStack;
        auto clock = registry.makeEntry("meter.clock"), rx = registry.makeEntry("applet:rx"),
             signal = registry.makeEntry("meter.sMeter");
        main.contents = {clock, rx, signal};
        d.containers = {main};
        QCOMPARE(manager.commitWorkspace(d, 0).status, CommitStatus::Saved);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* host = manager.contentHost("main");
        auto* controller = manager.arrangeController();
        QVERIFY(host && controller);
        QTRY_COMPARE(host->meterSurfaces().size(), 2);
        QTRY_VERIFY(host->gripGeometry(clock.id).isValid());
        QTest::qWait(80);
        for (const auto& row : host->entryRows()) {
            const QRect grip = host->gripGeometry(row.entryId);
            const QRect boundary = host->entryBoundary(row.entryId);
            QVERIFY(grip.right() < boundary.left());
            QVERIFY(host->findChild<QWidget*>("entryGrip_" + row.entryId)->isVisible());
        }
        capture(&window, "01-mixed-grips");
        capture(manager.container("main"), "01-mixed-container");
        QTest::qWait(250);
        manager.saveState();
        QTest::mouseClick(&borrowed, Qt::LeftButton);
        QCOMPARE(clicks, 1);
        const auto before = store.snapshot();
        const auto targetCount = [&] {
            int count = 0;
            for (auto* c : manager.allContainers()) {
                if (auto* h = manager.contentHost(c->id())) {
                    count += h->meterSurfaces().size();
                }
            }
            return count;
        };
        QCOMPARE(poller.targetCountForTest(), targetCount());
        // Native viewport receives the same QDropEvent used by the OS drag manager.
        auto* viewport = host->findChild<QScrollArea*>()->viewport();
        QMimeData mime;
        mime.setData(ContainerArrangeController::kMimeType, controller->mimeData(clock.id));
        QDragEnterEvent enter(QPoint(40, viewport->height() - 5), Qt::MoveAction, &mime,
                              Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &enter);
        QVERIFY(enter.isAccepted());
        QDragMoveEvent movement(QPoint(40, viewport->height() - 5), Qt::MoveAction, &mime,
                                Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &movement);
        QVERIFY(movement.isAccepted());
        QVERIFY(host->findChild<QWidget*>("containerInsertionLine")->isVisible());
        capture(&window, "02-insertion-indicator");
        QDropEvent drop(QPointF(40, viewport->height() - 5), Qt::MoveAction, &mime, Qt::LeftButton,
                        Qt::NoModifier);
        QApplication::sendEvent(viewport, &drop);
        QVERIFY(drop.isAccepted());
        QCOMPARE(store.snapshot().containers[0].contents.last().id, clock.id);
        QCOMPARE(poller.targetCountForTest(), targetCount());
        capture(&window, "03-reordered");
        capture(manager.container("main"), "03-reordered-container");
        const auto canceled = ContainerDocumentCodec::encode(store.snapshot());
        QDragLeaveEvent canceledDrop;
        QApplication::sendEvent(viewport, &canceledDrop);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), canceled);
        QVERIFY(!host->findChild<QWidget*>("containerInsertionLine")->isVisible());
        QMenu menu;
        host->addContentsMenu(menu);
        QVERIFY(!menu.actions().isEmpty());
        QMenu* rxMenu = nullptr;
        for (auto* item : menu.actions()[0]->menu()->actions()) {
            if (item->text() == rx.name) {
                rxMenu = item->menu();
            }
        }
        QVERIFY(rxMenu);
        QAction* pop = nullptr;
        for (auto* action : rxMenu->actions()) {
            if (action->text() == "Pop Out") {
                pop = action;
            }
        }
        QVERIFY(pop);
        pop->trigger();
        d = store.snapshot();
        const QString shell = d.containers.last().id;
        QVERIFY(d.containers.last().popOutShell);
        QCOMPARE(borrowed.window(), manager.container(shell)->window());
        QTest::mouseClick(&borrowed, Qt::LeftButton);
        QCOMPARE(clicks, 2);
        capture(manager.container(shell)->window(), "04-popped-applet");
        QPointer<MeterWidget> old = host->meterSurfaces().last();
        QVERIFY(controller->move(clock.id, shell, 1).ok);
        QVERIFY(old.isNull());
        QCOMPARE(poller.targetCountForTest(), targetCount());
        capture(manager.container(shell)->window(), "05-mixed-popout");
        manager.container(shell)->window()->close();
        QTRY_VERIFY(!manager.container(shell));
        QCOMPARE(borrowed.window(), &window);
        QCOMPARE(poller.targetCountForTest(), targetCount());
        capture(&window, "06-returned");
        QTest::mouseClick(&borrowed, Qt::LeftButton);
        QCOMPARE(clicks, 3);
        d = store.snapshot();
        d.containers[0].header = HeaderMode::Reveal;
        QCOMPARE(manager.commitWorkspace(d, d.revision).status, CommitStatus::Saved);
        auto* c = manager.container("main");
        QApplication::setActiveWindow(&window);
        borrowed.setFocus();
        QTRY_VERIFY(borrowed.hasFocus());
        QTRY_VERIFY(c->chromeVisible());
        const QPoint body = host->mapTo(c, QPoint());
        capture(&window, "07-reveal-focus");
        d = store.snapshot();
        d.containers[0].header = HeaderMode::Hidden;
        d.containers[0].locked = true;
        QCOMPARE(manager.commitWorkspace(d, d.revision).status, CommitStatus::Saved);
        QVERIFY(!c->chromeVisible());
        QCOMPARE(host->mapTo(c, QPoint()), body);
        capture(&window, "08-hidden");
        c->recoverChrome();
        QVERIFY(c->chromeVisible());
        const auto recoveredViewport = host->mapTo(c, QPoint());
        d = store.snapshot();
        d.containers[0].name = "Recovered hidden controls";
        QCOMPARE(manager.commitWorkspace(d, d.revision).status, CommitStatus::Saved);
        QVERIFY(c->chromeVisible());
        QCOMPARE(host->mapTo(c, QPoint()), recoveredViewport);
        capture(&window, "09-hidden-recovery");
        d = store.snapshot();
        d.containers[0].locked = false;
        ContainerDocument legacy;
        legacy.id = "legacy";
        legacy.name = "Customized retained canvas";
        legacy.layout = ContentLayout::LegacyCanvas;
        legacy.dockMode = DockMode::Floating;
        legacy.geometry = QRect(180, 180, 380, 360);
        auto primitive = registry.makeEntry("BAR");
        primitive.canvasRect = QRectF(.15, .32, .7, .28);
        primitive.paintOrder = 19;
        BarItem barConfiguration;barConfiguration.setRange(0,100);barConfiguration.setBarColor(QColor("#00b4d8"));barConfiguration.setAttackRatio(1);
        primitive.config["legacyRecord"]=barConfiguration.serialize();
        auto second = registry.makeEntry("TEXT");
        second.canvasRect = QRectF(.1, .02, .8, .2);
        second.paintOrder = 28;
        TextItem caption;caption.setLabel("Retained custom caption");caption.setIdleText("No source reading");caption.setFontSize(14);
        second.config["legacyRecord"]=caption.serialize();
        legacy.contents = {primitive, second};
        d.containers.append(legacy);
        QCOMPARE(manager.commitWorkspace(d, d.revision).status, CommitStatus::Saved);
        auto* canvas = manager.contentHost("legacy");
        QTest::qWait(80);
        const auto row = canvas->entryRows()[0];
        const QPoint origin = row.widget->mapTo(canvas, QPoint());
        const QRect expected =
            QRectF(origin.x() + row.widget->width() * .15, origin.y() + row.widget->height() * .32,
                   row.widget->width() * .7, row.widget->height() * .28)
                .toAlignedRect();
        QCOMPARE(canvas->entryBoundary(primitive.id), expected);
        QVERIFY(canvas->gripGeometry(primitive.id).right() < expected.left());
        QVERIFY(controller->move(primitive.id, "legacy", 2).ok);
        d = store.snapshot();
        const auto retained = d.containers.last();
        QCOMPARE(retained.layout, ContentLayout::LegacyCanvas);
        QCOMPARE(retained.contents.last(), primitive);
        QCOMPARE(canvas->meterSurfaces()[0]->items()[0]->zOrder(), 19);
        QCOMPARE(canvas->meterSurfaces()[0]->items()[1]->zOrder(), 28);
        capture(manager.container("legacy")->window(), "10-legacy-canvas");
        d = store.snapshot();
        d.containers[0].config["showOnRx"] = false;
        d.containers[0].config["showOnTx"] = true;
        QCOMPARE(manager.commitWorkspace(d, d.revision).status, CommitStatus::Saved);
        QVERIFY(!manager.container("main")->isVisible());
        const auto visibilityBytes = ContainerDocumentCodec::encode(store.snapshot());
        manager.setTransmitting(true);
        QVERIFY(manager.container("main")->isVisible());
        manager.setTransmitting(false);
        QVERIFY(!manager.container("main")->isVisible());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), visibilityBytes);
        poller.stop();
    }
    void clampsMinimaAndAutoHeight()
    {
        QCOMPARE(FloatingContainer::clampedGeometry(QRect(9000, -500, 20, 3),
                                                    QRect(100, 100, 800, 600), QSize(260, 24)),
                 QRect(640, 100, 260, 24));
        QCOMPARE(FloatingContainer::clampedGeometry(QRect(-9000, 9000, 2000, 1000),
                                                    QRect(0, 0, 800, 600), QSize(300, 50)),
                 QRect(0, 0, 800, 600));
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        QWidget pane;
        pane.resize(800, 600);
        QSplitter splitter;
        ContainerContentRegistry registry;
        ContainerManager manager(&pane, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        WorkspaceDocument d;
        d.mainContainerId = "M";
        ContainerDocument c;
        c.id = "M";
        d.containers = {c};
        ContainerDocument f;
        f.id = "F";
        f.layout = ContentLayout::VerticalStack;
        f.dockMode = DockMode::Floating;
        f.autoHeight = true;
        f.contents = {registry.makeEntry("meter.clock")};
        f.geometry = QRect(99999, -1000, 10, 10);
        d.containers.append(f);
        ContainerDocument overlay;overlay.id="O";overlay.dockMode=DockMode::OverlayDocked;overlay.anchor=AxisLock::BottomRight;overlay.geometry=QRect(100,100,300,160);d.containers.append(overlay);
        QCOMPARE(manager.commitWorkspace(d, 0).status, CommitStatus::Saved);
        auto* anchored=manager.container("O");QVERIFY(anchored);manager.updateDockedPositions(40,30);QCOMPARE(anchored->pos(),QPoint(140,130));
        auto* host = manager.contentHost("F");
        auto* form = manager.container("F")->window();
        QVERIFY(form->width() >= 260);
        QCOMPARE(form->height(), host->preferredContentHeight() + ContainerWidget::kTitleBarHeight);
        QVERIFY(form->screen()->availableGeometry().contains(form->geometry()));
    }
};
QTEST_MAIN(TstContainerChrome)
#include "tst_container_chrome.moc"
