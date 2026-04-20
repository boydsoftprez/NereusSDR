// =================================================================
// tests/tst_container_widget_access.cpp  (NereusSDR)
// =================================================================
//
// Edit-container refactor Task 15 — on-container edit affordances.
//
// Verifies the two discoverable entry points added to
// ContainerWidget: right-click context menu and header double-click.
// Both route into the pre-existing ContainerWidget::settingsRequested
// signal (wired to the settings dialog by ContainerManager). The
// context menu additionally exposes Rename / Duplicate / Delete and a
// Dock Mode submenu which emit the 4 new signals added in Task 15.
//
// no-port-check: test scaffolding; widget access surface is
// NereusSDR-original.
// =================================================================

#include <QtTest/QtTest>
#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalSpy>

#include "gui/containers/ContainerWidget.h"

using namespace NereusSDR;

class TstContainerWidgetAccess : public QObject
{
    Q_OBJECT

private slots:
    void contextMenuHelper_editActionEmitsSettings();
    void contextMenuHelper_renameActionEmitsRename();
    void contextMenuHelper_duplicateActionEmitsDuplicate();
    void contextMenuHelper_deleteActionEmitsDelete();
    void contextMenuHelper_dockModeActionsEmitModeChange();
    void doubleClickHeader_emitsSettings();
    void doubleClickBody_doesNotEmit();
    void doubleClickLocked_doesNotEmit();
};

// -------------------------------------------------------------------
// Context menu — exercise via the buildContextMenu() helper so the
// test does not have to dismiss a modal popup. Actions are parented
// to the temporary QMenu and triggered directly.
// -------------------------------------------------------------------

namespace {

QAction* findActionByText(QMenu* menu, const QString& text)
{
    if (!menu) { return nullptr; }
    for (QAction* a : menu->actions()) {
        if (a->text() == text) { return a; }
        if (a->menu()) {
            if (QAction* nested = findActionByText(a->menu(), text)) {
                return nested;
            }
        }
    }
    return nullptr;
}

} // namespace

void TstContainerWidgetAccess::contextMenuHelper_editActionEmitsSettings()
{
    ContainerWidget w;
    QMenu menu;
    w.buildContextMenu(&menu);

    QSignalSpy spy(&w, &ContainerWidget::settingsRequested);
    QAction* act = findActionByText(&menu, QStringLiteral("Edit..."));
    QVERIFY(act != nullptr);
    act->trigger();
    QCOMPARE(spy.count(), 1);
}

void TstContainerWidgetAccess::contextMenuHelper_renameActionEmitsRename()
{
    ContainerWidget w;
    QMenu menu;
    w.buildContextMenu(&menu);

    QSignalSpy spy(&w, &ContainerWidget::renameRequested);
    QAction* act = findActionByText(&menu, QStringLiteral("Rename..."));
    QVERIFY(act != nullptr);
    act->trigger();
    QCOMPARE(spy.count(), 1);
}

void TstContainerWidgetAccess::contextMenuHelper_duplicateActionEmitsDuplicate()
{
    ContainerWidget w;
    QMenu menu;
    w.buildContextMenu(&menu);

    QSignalSpy spy(&w, &ContainerWidget::duplicateRequested);
    QAction* act = findActionByText(&menu, QStringLiteral("Duplicate"));
    QVERIFY(act != nullptr);
    act->trigger();
    QCOMPARE(spy.count(), 1);
}

void TstContainerWidgetAccess::contextMenuHelper_deleteActionEmitsDelete()
{
    ContainerWidget w;
    QMenu menu;
    w.buildContextMenu(&menu);

    QSignalSpy spy(&w, &ContainerWidget::deleteRequested);
    QAction* act = findActionByText(&menu, QStringLiteral("Delete"));
    QVERIFY(act != nullptr);
    act->trigger();
    QCOMPARE(spy.count(), 1);
}

void TstContainerWidgetAccess::contextMenuHelper_dockModeActionsEmitModeChange()
{
    ContainerWidget w;
    w.setDockMode(DockMode::OverlayDocked);
    QMenu menu;
    w.buildContextMenu(&menu);

    QSignalSpy spy(&w, &ContainerWidget::dockModeChangeRequested);

    QAction* floating = findActionByText(&menu, QStringLiteral("Floating"));
    QVERIFY(floating != nullptr);
    floating->trigger();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<DockMode>(), DockMode::Floating);

    QAction* panel = findActionByText(&menu, QStringLiteral("Panel"));
    QVERIFY(panel != nullptr);
    panel->trigger();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).value<DockMode>(), DockMode::PanelDocked);

    QAction* overlay = findActionByText(&menu, QStringLiteral("Overlay"));
    QVERIFY(overlay != nullptr);
    overlay->trigger();
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(0).value<DockMode>(), DockMode::OverlayDocked);
}

// -------------------------------------------------------------------
// Header double-click — only fires settingsRequested when the title
// bar is visible, the widget is unlocked, and the click lands on
// the title-bar strip (y < kTitleBarHeight).
// -------------------------------------------------------------------

void TstContainerWidgetAccess::doubleClickHeader_emitsSettings()
{
    ContainerWidget w;
    w.resize(300, 200);
    w.setTitleBarVisible(true);
    QSignalSpy spy(&w, &ContainerWidget::settingsRequested);

    const QPoint pos(50, 5);  // header strip — y < kTitleBarHeight (22)
    QMouseEvent ev(QEvent::MouseButtonDblClick, pos, w.mapToGlobal(pos),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&w, &ev);

    QCOMPARE(spy.count(), 1);
}

void TstContainerWidgetAccess::doubleClickBody_doesNotEmit()
{
    ContainerWidget w;
    w.resize(300, 200);
    w.setTitleBarVisible(true);
    QSignalSpy spy(&w, &ContainerWidget::settingsRequested);

    const QPoint pos(50, 100);  // well below the 22-px header strip
    QMouseEvent ev(QEvent::MouseButtonDblClick, pos, w.mapToGlobal(pos),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&w, &ev);

    QCOMPARE(spy.count(), 0);
}

void TstContainerWidgetAccess::doubleClickLocked_doesNotEmit()
{
    ContainerWidget w;
    w.resize(300, 200);
    w.setTitleBarVisible(true);
    w.setLocked(true);
    QSignalSpy spy(&w, &ContainerWidget::settingsRequested);

    const QPoint pos(50, 5);
    QMouseEvent ev(QEvent::MouseButtonDblClick, pos, w.mapToGlobal(pos),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&w, &ev);

    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TstContainerWidgetAccess)
#include "tst_container_widget_access.moc"
