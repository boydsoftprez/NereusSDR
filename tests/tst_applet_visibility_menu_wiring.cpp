// Verify the two QMenu structures (top menu + banner menu) wired in
// MainWindow stay in sync via AppletVisibilityController. This test
// builds the menu structures the same way MainWindow does, without
// instantiating MainWindow itself.
// no-port-check: NereusSDR-original — no Thetis source.

#include <QtTest/QtTest>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QHash>
#include <QSignalSpy>
#include "gui/applets/AppletVisibilityController.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstAppletVisibilityMenuWiring : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    void both_menus_have_5_actions_initially_checked();
    void toggling_top_menu_action_updates_banner_menu();
    void toggling_banner_action_updates_top_menu();
    void no_recursive_emit_loop();
};

namespace {
struct Wired {
    AppletVisibilityController* ctl;
    QMenu* topMenu;
    QMenu* bannerMenu;
    QHash<QString, QAction*> topActions;
    QHash<QString, QAction*> bannerActions;
};

// Mirror of the MainWindow setup, in test scope.
Wired buildMenus(QObject* parent)
{
    Wired w;
    w.ctl = new AppletVisibilityController(parent);
    w.ctl->registerApplet(QStringLiteral("Rx"),
                          QStringLiteral("RX"), true);
    w.ctl->registerApplet(QStringLiteral("Tx"),
                          QStringLiteral("TX"), true);
    w.ctl->registerApplet(QStringLiteral("PhoneCw"),
                          QStringLiteral("Phone / CW"), true);
    w.ctl->registerApplet(QStringLiteral("Vax"),
                          QStringLiteral("VAX"), true);
    w.ctl->registerApplet(QStringLiteral("PureSignal"),
                          QStringLiteral("PureSignal"), true);

    w.topMenu = new QMenu(qobject_cast<QWidget*>(parent));
    w.bannerMenu = new QMenu(qobject_cast<QWidget*>(parent));

    auto wireMenu = [&w](QMenu* menu, QHash<QString, QAction*>& sink) {
        for (const QString& id : w.ctl->registeredIds()) {
            QAction* act = menu->addAction(w.ctl->displayName(id));
            act->setCheckable(true);
            act->setChecked(w.ctl->isVisible(id));
            QObject::connect(act, &QAction::toggled,
                             [w, id](bool checked) {
                w.ctl->setVisible(id, checked);
            });
            sink.insert(id, act);
        }
    };
    wireMenu(w.topMenu,    w.topActions);
    wireMenu(w.bannerMenu, w.bannerActions);

    auto syncMenu = [&w](QHash<QString, QAction*>& sink) {
        QObject::connect(w.ctl, &AppletVisibilityController::visibilityChanged,
                         [sink](const QString& id, bool v) {
            if (auto* act = sink.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(v);
            }
        });
    };
    syncMenu(w.topActions);
    syncMenu(w.bannerActions);

    return w;
}
} // anon

void TstAppletVisibilityMenuWiring::both_menus_have_5_actions_initially_checked()
{
    QObject parent;
    Wired w = buildMenus(&parent);
    QCOMPARE(w.topMenu->actions().size(),    5);
    QCOMPARE(w.bannerMenu->actions().size(), 5);
    for (QAction* a : w.topMenu->actions())    { QVERIFY(a->isChecked()); }
    for (QAction* a : w.bannerMenu->actions()) { QVERIFY(a->isChecked()); }
}

void TstAppletVisibilityMenuWiring::toggling_top_menu_action_updates_banner_menu()
{
    QObject parent;
    Wired w = buildMenus(&parent);
    QAction* topTx = w.topActions.value(QStringLiteral("Tx"));
    QAction* banTx = w.bannerActions.value(QStringLiteral("Tx"));

    topTx->toggle();
    QVERIFY(!topTx->isChecked());
    QVERIFY(!banTx->isChecked());
    QVERIFY(!w.ctl->isVisible(QStringLiteral("Tx")));
}

void TstAppletVisibilityMenuWiring::toggling_banner_action_updates_top_menu()
{
    QObject parent;
    Wired w = buildMenus(&parent);
    QAction* topPs = w.topActions.value(QStringLiteral("PureSignal"));
    QAction* banPs = w.bannerActions.value(QStringLiteral("PureSignal"));

    banPs->toggle();
    QVERIFY(!banPs->isChecked());
    QVERIFY(!topPs->isChecked());
    QVERIFY(!w.ctl->isVisible(QStringLiteral("PureSignal")));
}

void TstAppletVisibilityMenuWiring::no_recursive_emit_loop()
{
    QObject parent;
    Wired w = buildMenus(&parent);
    QSignalSpy spy(w.ctl, &AppletVisibilityController::visibilityChanged);

    w.topActions.value(QStringLiteral("Vax"))->toggle();
    QCOMPARE(spy.count(), 1);

    w.bannerActions.value(QStringLiteral("Vax"))->toggle();
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(TstAppletVisibilityMenuWiring)
#include "tst_applet_visibility_menu_wiring.moc"
