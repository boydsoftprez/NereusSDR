// no-port-check: test-only — AppletPanelWidget float / dock round-trip for
// a canFloat() applet (AM Mod Monitor).  Verifies the applet leaves and
// re-enters the panel intact, visibility requests follow the floating
// window, and the Applet<Id>Floating key persists the state.
#include <QtTest>
#include "core/AppSettings.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/ModMonitorApplet.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include <QTemporaryDir>
#include <QSplitter>

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

    void managedFloatDockUsesAtomicController()
    {
        QTemporaryDir dir;AppSettings settings(dir.filePath("settings.xml"));ContainerWorkspaceStore store(settings);QVERIFY(store.load().ok);
        AppletPanelWidget panel;panel.setManagedWorkspace(true);auto* mm=new ModMonitorApplet(nullptr);panel.addApplet(mm);
        QWidget pane;QSplitter splitter;ContainerContentRegistry registry;registry.attachSingleton("applet:mod_monitor",mm);ContainerManager manager(&pane,&splitter);manager.setWorkspaceAdapter(&store,&registry);panel.setArrangeController(manager.arrangeController());
        WorkspaceDocument d;d.mainContainerId="main";ContainerDocument c;c.id="main";c.layout=ContentLayout::VerticalStack;c.contents={registry.makeEntry("applet:mod_monitor")};d.containers={c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        panel.floatApplet(mm);QVERIFY(panel.isAppletFloating(mm));QCOMPARE(store.snapshot().containers.size(),2);QVERIFY(store.snapshot().containers.last().popOutShell);const quint64 revision=store.snapshot().revision;
        panel.dockApplet(mm);QVERIFY(!panel.isAppletFloating(mm));QCOMPARE(store.snapshot().containers.size(),1);QCOMPARE(store.snapshot().revision,revision+1);QCOMPARE(registry.singletonView("applet:mod_monitor"),mm);
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
