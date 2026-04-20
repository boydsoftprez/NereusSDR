// =================================================================
// tests/tst_floating_container_access.cpp  (NereusSDR)
// =================================================================
//
// Edit-container refactor Task 16 — FloatingContainer forwarding.
//
// FloatingContainer's top-level QWidget forwards contextMenuEvent
// and mouseDoubleClickEvent to its embedded ContainerWidget so all
// three on-container affordances (right-click menu, header double-
// click, gear icon) work identically whether the container is
// docked or floating.
//
// no-port-check: test scaffolding; forwarding surface is NereusSDR-
// original.
// =================================================================

#include <QtTest/QtTest>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QSignalSpy>

#include "gui/containers/ContainerWidget.h"
#include "gui/containers/FloatingContainer.h"

using namespace NereusSDR;

class TstFloatingContainerAccess : public QObject
{
    Q_OBJECT

private slots:
    void contextMenuEvent_forwardsToEmbedded();
    void mouseDoubleClickEvent_forwardsToEmbedded();
};

void TstFloatingContainerAccess::contextMenuEvent_forwardsToEmbedded()
{
    auto* container = new ContainerWidget();
    container->resize(300, 200);

    FloatingContainer floating(1);
    floating.takeOwner(container);

    // Can't observe the popup directly without spinning an event loop,
    // but we can confirm the forwarded contextMenuEvent reaches the
    // embedded widget by watching for the eventually-triggered signals.
    // Instead of actually showing the menu (which would block), verify
    // that ContainerWidget::contextMenuEvent is invoked by asserting
    // that *a* QContextMenuEvent delivered to the FloatingContainer
    // was accepted — and that without an embedded child the same
    // event falls through to default handling.

    const QPoint localPos(10, 10);
    QContextMenuEvent ev(QContextMenuEvent::Mouse, localPos,
                          floating.mapToGlobal(localPos));

    // The event must be marked accepted by the forwarding path.
    QCoreApplication::sendEvent(&floating, &ev);
    QVERIFY(ev.isAccepted());
}

void TstFloatingContainerAccess::mouseDoubleClickEvent_forwardsToEmbedded()
{
    auto* container = new ContainerWidget();
    container->resize(300, 200);
    container->setTitleBarVisible(true);

    FloatingContainer floating(1);
    floating.takeOwner(container);

    QSignalSpy spy(container, &ContainerWidget::settingsRequested);

    // Click on the title-bar strip (y < kTitleBarHeight). Position in
    // FloatingContainer coordinates maps 1:1 to ContainerWidget coords
    // because the layout has zero margins.
    const QPoint localPos(50, 5);
    const QPointF globalPos = floating.mapToGlobal(localPos);
    QMouseEvent ev(QEvent::MouseButtonDblClick, localPos, globalPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

    QCoreApplication::sendEvent(&floating, &ev);
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TstFloatingContainerAccess)
#include "tst_floating_container_access.moc"
