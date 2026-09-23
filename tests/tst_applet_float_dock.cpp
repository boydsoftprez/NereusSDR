// no-port-check: test-only — AppletPanelWidget float / dock round-trip for
// a canFloat() applet (AM Mod Monitor).  Verifies the applet leaves and
// re-enters the panel intact, visibility requests follow the floating
// window, and the Applet<Id>Floating key persists the state.
#include <QtTest>
#include "core/AppSettings.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/ModMonitorApplet.h"

using namespace NereusSDR;

class TestAppletFloatDock : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }

    void floatThenDock_roundTrips()
    {
        AppletPanelWidget panel;
        auto* mm = new ModMonitorApplet(nullptr);
        QVERIFY(mm->canFloat());
        panel.insertApplet(0, mm);
        panel.show();
        QCOMPARE(panel.applets().size(), 1);
        QVERIFY(!panel.isAppletFloating(mm));
        QVERIFY(mm->window() == &panel);

        panel.floatApplet(mm);
        QVERIFY(panel.isAppletFloating(mm));
        QVERIFY(mm->window() != &panel);
        QVERIFY(qobject_cast<AppletFloatingWindow*>(mm->window()) != nullptr);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Appletmod_monitorFloating")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(panel.applets().size(), 1);   // still owned by the panel

        // Hiding while floating hides the window, not the (already hidden) wrapper.
        auto* win = qobject_cast<AppletFloatingWindow*>(mm->window());
        panel.setAppletVisible(mm, false);
        QVERIFY(!win->isVisible());
        panel.setAppletVisible(mm, true);
        QVERIFY(win->isVisible());

        panel.dockApplet(mm);
        QCoreApplication::processEvents();   // deleteLater on the window
        QVERIFY(!panel.isAppletFloating(mm));
        QVERIFY(mm->window() == &panel);
        QVERIFY(mm->isVisible());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Appletmod_monitorFloating")).toString(),
                 QStringLiteral("False"));

        // Double float / double dock are no-ops.
        panel.dockApplet(mm);
        panel.floatApplet(mm);
        panel.floatApplet(mm);
        QVERIFY(panel.isAppletFloating(mm));
        panel.dockApplet(mm);
        QCoreApplication::processEvents();
        QVERIFY(!panel.isAppletFloating(mm));
    }

    void restoresFloatingOnNextLaunch()
    {
        AppSettings::instance().setValue(QStringLiteral("Appletmod_monitorFloating"), QStringLiteral("True"));
        AppletPanelWidget panel;
        auto* mm = new ModMonitorApplet(nullptr);
        panel.addApplet(mm);
        panel.show();
        QTRY_VERIFY(panel.isAppletFloating(mm));   // deferred via singleShot(0)
        panel.dockApplet(mm);
        QCoreApplication::processEvents();
    }
};

QTEST_MAIN(TestAppletFloatDock)
#include "tst_applet_float_dock.moc"
