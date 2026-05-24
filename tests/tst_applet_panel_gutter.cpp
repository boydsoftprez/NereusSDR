// Verify A1: AppletPanelWidget reserves an 8 px scrollbar gutter on the right.
// Per docs/architecture/ui-audit-polish-plan.md §A1.

#include <QApplication>
#include <QtTest/QtTest>
#include "gui/applets/AppletPanelWidget.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMenu>
#include <QPushButton>

using NereusSDR::AppletPanelWidget;

class TstAppletPanelGutter : public QObject {
    Q_OBJECT
private slots:
    void stackLayoutReservesEightPxRightMargin();
    void bannerButtonHiddenUntilMenuSet();
    void bannerButtonShownAndOpensMenuOnceSet();
};

void TstAppletPanelGutter::stackLayoutReservesEightPxRightMargin()
{
    AppletPanelWidget panel;
    auto* scroll = panel.findChild<QScrollArea*>();
    QVERIFY(scroll != nullptr);
    auto* stackWidget = scroll->widget();
    QVERIFY(stackWidget != nullptr);
    auto* stackLayout = qobject_cast<QVBoxLayout*>(stackWidget->layout());
    QVERIFY(stackLayout != nullptr);

    QMargins m = stackLayout->contentsMargins();
    QCOMPARE(m.right(), 8);  // 8 px reserved for the scrollbar
    QCOMPARE(m.left(), 0);
    QCOMPARE(m.top(), 0);
    QCOMPARE(m.bottom(), 0);
}

void TstAppletPanelGutter::bannerButtonHiddenUntilMenuSet()
{
    AppletPanelWidget panel;
    auto* btn = panel.findChild<QPushButton*>(
        QStringLiteral("appletPanelBannerButton"));
    QVERIFY(btn != nullptr);
    QVERIFY(!btn->isVisible());
}

void TstAppletPanelGutter::bannerButtonShownAndOpensMenuOnceSet()
{
    AppletPanelWidget panel;
    panel.show();
    auto* menu = new QMenu(&panel);
    menu->addAction(QStringLiteral("dummy"));
    panel.setBannerMenu(menu);

    auto* btn = panel.findChild<QPushButton*>(
        QStringLiteral("appletPanelBannerButton"));
    QVERIFY(btn != nullptr);
    QVERIFY(btn->isVisible());
    QCOMPARE(btn->menu(), menu);
}

QTEST_MAIN(TstAppletPanelGutter)
#include "tst_applet_panel_gutter.moc"
