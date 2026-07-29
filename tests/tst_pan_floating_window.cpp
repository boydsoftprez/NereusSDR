// =================================================================
// tests/tst_pan_floating_window.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 8: PanFloatingWindow construct + dock signal.
// =================================================================
#include <QtTest/QtTest>
#include <QPointer>
#include <QSignalSpy>
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"

using namespace NereusSDR;

class TestPanFloatingWindow : public QObject {
    Q_OBJECT
private slots:
    void construct_with_applet_keeps_applet_reachable()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        PanFloatingWindow* w = new PanFloatingWindow(applet, nullptr);
        QCOMPARE(w->applet(), applet);
        QCOMPARE(w->panId(), QStringLiteral("pan-floated"));
        // QVBoxLayout::addWidget reparents the applet to the window, so
        // deleting the window also deletes the applet (single owner). No
        // separate `delete applet;` here; that would be a double-free.
        delete w;
    }

    void dock_requested_signal_emitted_on_request_dock()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        PanFloatingWindow* w = new PanFloatingWindow(applet, nullptr);
        QSignalSpy spy(w, &PanFloatingWindow::dockRequested);
        w->requestDock();
        QCOMPARE(spy.count(), 1);
        delete w;
    }

    void externally_destroyed_applet_is_guarded()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        auto* w = new PanFloatingWindow(applet, nullptr);
        QPointer<PanadapterApplet> guarded(applet);

        delete applet;

        QVERIFY(guarded.isNull());
        QCOMPARE(w->applet(), nullptr);
        QCOMPARE(w->panId(), QString());
        w->requestDock();
        delete w;
    }
};

QTEST_MAIN(TestPanFloatingWindow)
#include "tst_pan_floating_window.moc"
