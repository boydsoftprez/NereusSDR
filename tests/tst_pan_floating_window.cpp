// =================================================================
// tests/tst_pan_floating_window.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 8: PanFloatingWindow construct + dock signal.
//
// 2026-08-08: extended for the bench-reported float defects — the popped-out
// pan froze ("QRhiWidget: No QRhi" once per display tick) and arrived with no
// container chrome at all. Covers the adopt/take reparent contract, the
// floating-only title strip, and per-pan geometry persistence.
//
// Modification history (NereusSDR):
//   2026-10-22 - Application quit closes a floating pan without docking it
//                 (ported from Longpath tests/tst_pan_floating_window.cpp:
//                 33-52 [@383f058], Longpath PR 83); the close box still
//                 docks. By J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Claude Code.
// =================================================================
//
// --- From tst_pan_floating_window.cpp ---
// =================================================================
// tests/tst_pan_floating_window.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 8: PanFloatingWindow construct + dock signal.
// =================================================================
#include <QtTest/QtTest>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestPanFloatingWindow : public QObject {
    Q_OBJECT
private slots:
    void adopted_applet_is_reachable_and_owned()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        auto* w = new PanFloatingWindow(nullptr);
        w->adoptApplet(applet);
        QCOMPARE(w->applet(), applet);
        QCOMPARE(w->panId(), QStringLiteral("pan-floated"));
        // adoptApplet goes through QVBoxLayout::addWidget, which reparents the
        // applet to the window, so deleting the window deletes the applet
        // (single owner). No separate `delete applet;` here; double-free.
        delete w;
    }

    // The reparent must never pass through setParent(nullptr). An orphaned
    // QRhiWidget-hosting applet becomes a transient top-level, and on macOS
    // creating and destroying that NSWindow can corrupt the main window's
    // responder chain. Asserted by watching the parent at every step: it goes
    // splitter-stand-in -> window -> destination, never null.
    void adopt_and_take_never_orphan_the_applet()
    {
        QWidget stand_in;          // stands in for the splitter
        QWidget destination;
        auto* applet = new PanadapterApplet(QStringLiteral("pan-0"), &stand_in);
        QCOMPARE(applet->parentWidget(), &stand_in);

        auto* w = new PanFloatingWindow(nullptr);
        w->adoptApplet(applet);
        QVERIFY(applet->parentWidget() != nullptr);
        QVERIFY(applet->parentWidget() != &stand_in);

        PanadapterApplet* taken = w->takeApplet(&destination);
        QCOMPARE(taken, applet);
        QCOMPARE(applet->parentWidget(), &destination);
        QCOMPARE(w->applet(), nullptr);

        QPointer<PanadapterApplet> guarded(applet);
        delete w;                  // must not take the applet with it
        QVERIFY2(!guarded.isNull(),
                 "takeApplet left the applet owned by the window");
        QCOMPARE(applet->parentWidget(), &destination);
    }

    // From Longpath tests/tst_pan_floating_window.cpp:33-52 [@383f058]
    //   adapter: NereusSDR's window takes the applet through adoptApplet(),
    //   and Longpath's kFensterFrist exposure timeout is the QtTest default.
    // 2026-09-24: mit schwebendem Panadapter liess sich Longpath nicht
    // beenden. Beim Beenden schliesst Qt alle Fenster der Reihe nach und
    // bricht ab, sobald eines ablehnt -- und dieses lehnte ab, solange die
    // Abbau-Sperre noch nicht gesetzt war. closeAllWindows() ist genau
    // dieser Weg (nicht spontan). Das Fenster muss zugehen, ohne anzudocken.
    void closing_all_windows_is_not_refused_and_does_not_dock()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-0"));
        auto* w = new PanFloatingWindow(nullptr);
        w->adoptApplet(applet);
        QSignalSpy dockSpy(w, &PanFloatingWindow::dockRequested);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));

        QApplication::closeAllWindows();

        QVERIFY2(!w->isVisible(),
                 "a refused close cancels the whole application quit");
        QCOMPARE(dockSpy.count(), 0);
        delete w;
    }

    // The operator's close box arrives from the window system as a
    // spontaneous close, and still asks the stack to dock the pan.
    void close_box_still_requests_dock()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-close-box"));
        auto* w = new PanFloatingWindow(nullptr);
        w->adoptApplet(applet);
        QSignalSpy dockSpy(w, &PanFloatingWindow::dockRequested);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));
        QVERIFY(w->windowHandle() != nullptr);

        QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::SynchronousDelivery>(
            w->windowHandle());

        QCOMPARE(dockSpy.count(), 1);
        QCOMPARE(dockSpy.at(0).at(0).toString(), QStringLiteral("pan-close-box"));
        QVERIFY2(w->isVisible(),
                 "the close box docks through the stack; the window stays until it does");
        delete w;
    }

    void dock_requested_signal_emitted_on_request_dock()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        auto* w = new PanFloatingWindow(nullptr);
        w->adoptApplet(applet);
        QSignalSpy spy(w, &PanFloatingWindow::dockRequested);
        QVERIFY(spy.isValid());
        w->requestDock();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-floated"));
        delete w;
    }

    void externally_destroyed_applet_is_guarded()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        auto* w = new PanFloatingWindow(nullptr);
        w->adoptApplet(applet);
        QPointer<PanadapterApplet> guarded(applet);

        delete applet;

        QVERIFY(guarded.isNull());
        QCOMPARE(w->applet(), nullptr);
        QCOMPARE(w->panId(), QString());
        w->requestDock();
        delete w;
    }

    // The bench complaint was that a floated pan "does not live inside a
    // container": no name, no dock affordance. The strip is floating-only, so
    // assert both edges — absent while docked, present while floating.
    void title_strip_appears_only_while_floating()
    {
        PanadapterApplet applet(QStringLiteral("pan-1"));
        auto stripVisible = [&applet]() {
            // The strip is the applet's only direct QWidget child that is not
            // the spectrum host or the status overlay, so find it by the Dock
            // button it carries.
            const QList<QPushButton*> buttons =
                applet.findChildren<QPushButton*>();
            for (QPushButton* b : buttons) {
                if (b->toolTip().contains(QStringLiteral("Dock"))) {
                    return b->parentWidget()->isVisibleTo(&applet);
                }
            }
            return false;
        };

        QVERIFY(!applet.isFloating());
        QVERIFY2(!stripVisible(), "docked pan must not grow a title strip");

        applet.setFloatingState(true);
        QVERIFY(applet.isFloating());
        QVERIFY2(stripVisible(), "floating pan must show its title strip");

        applet.setFloatingState(false);
        QVERIFY(!applet.isFloating());
        QVERIFY2(!stripVisible(), "docking must take the title strip away");
    }

    // The status strip (CH / WIDE / TX badges) is positioned in applet
    // coordinates, so it has to step down past the title strip or it prints
    // over the pan name and the Dock button's row.
    void status_overlay_steps_below_the_title_strip_when_floating()
    {
        PanadapterApplet applet(QStringLiteral("pan-3"));
        applet.resize(800, 400);

        QWidget* overlay = nullptr;
        const QList<QWidget*> kids = applet.findChildren<QWidget*>();
        for (QWidget* w : kids) {
            if (QString::fromLatin1(w->metaObject()->className())
                    .contains(QStringLiteral("SpectrumStatusOverlay"))) {
                overlay = w;
            }
        }
        QVERIFY2(overlay, "applet has no status overlay to place");

        // Establish the docked baseline through the same code path rather
        // than reading the as-constructed geometry: an applet that has never
        // been shown has not run repositionStatusOverlay yet, so its overlay
        // still sits at the origin and every later comparison would be
        // against a number the production path never produces.
        applet.setFloatingState(false);
        const int dockedTop = overlay->geometry().top();
        applet.setFloatingState(true);
        const int floatingTop = overlay->geometry().top();

        QVERIFY2(floatingTop > dockedTop,
                 qPrintable(QStringLiteral("overlay top stayed at %1 while "
                                           "floating (docked %2) — it is "
                                           "sitting on the title strip")
                                .arg(floatingTop).arg(dockedTop)));

        applet.setFloatingState(false);
        QCOMPARE(overlay->geometry().top(), dockedTop);
    }

    void dock_button_emits_dock_requested_with_this_pans_id()
    {
        PanadapterApplet applet(QStringLiteral("pan-2"));
        applet.setFloatingState(true);
        QSignalSpy spy(&applet, &PanadapterApplet::dockRequested);

        QPushButton* dockBtn = nullptr;
        const QList<QPushButton*> buttons = applet.findChildren<QPushButton*>();
        for (QPushButton* b : buttons) {
            if (b->toolTip().contains(QStringLiteral("Dock"))) { dockBtn = b; }
        }
        QVERIFY2(dockBtn, "floating title strip has no Dock button");

        dockBtn->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-2"));
    }

    // Geometry is keyed per pan, so two popped-out pans keep their own
    // positions rather than the second one landing on top of the first.
    void geometry_round_trips_per_pan()
    {
        auto& s = AppSettings::instance();
        const QString key = QStringLiteral("FloatingPan_pan-geo_Geometry");
        s.setValue(key, QString());

        auto* applet = new PanadapterApplet(QStringLiteral("pan-geo"));
        auto* w = new PanFloatingWindow(nullptr);
        w->adoptApplet(applet);
        w->resize(640, 480);
        w->saveWindowGeometry();

        const QString stored = s.value(key, QString()).toString();
        QVERIFY2(!stored.isEmpty(), "saveWindowGeometry wrote nothing");

        w->resize(900, 700);
        w->restoreWindowGeometry();
        QCOMPARE(w->size(), QSize(640, 480));

        delete w;
    }

    // A pan with no applet has no id to key on; writing under an empty name
    // would collide across every such window.
    void geometry_save_is_a_noop_without_an_applet()
    {
        auto& s = AppSettings::instance();
        const QString emptyKey = QStringLiteral("FloatingPan__Geometry");
        s.setValue(emptyKey, QString());

        PanFloatingWindow w(nullptr);
        w.saveWindowGeometry();

        QVERIFY(s.value(emptyKey, QString()).toString().isEmpty());
    }
};

QTEST_MAIN(TestPanFloatingWindow)
#include "tst_pan_floating_window.moc"
