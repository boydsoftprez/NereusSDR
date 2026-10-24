// =================================================================
// tests/tst_scoped_child_widget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   tests/tst_scoped_child_widget.cpp [@383f058], original header from
//   Longpath source is included below.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE). Upstream source has no
// top-of-file GPL header; project-level LICENSE applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22  Ported for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted transformation via Claude Code. The four
//               helper cases are Longpath's; NereusSDR adds two cases that
//               close a real widget's window while its right-click menu
//               is open (RotorDialWidget, VfoWidget). Before the helper,
//               both ended the test process: the menu lived on the stack
//               and was freed twice.
//   2026-10-22  J.J. Boyd (KG4VCF): dialogs too. A case closes the
//               gradient picker's window while its color dialog is open,
//               and one checks the owner forwards constructor arguments.
//               AI-assisted transformation via Claude Code.
//   2026-10-22  J.J. Boyd (KG4VCF): the S-meter's heap menu too, and the
//               close-while-open cases now assert the window died inside
//               exec() and the menu or dialog is gone. AI-assisted
//               transformation via Claude Code.
// =================================================================
//
// --- From tst_scoped_child_widget.cpp ---
//
// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_scoped_child_widget.cpp  (Longpath)
// =================================================================
//
// Am 2026-08-30 stuerzte Longpath mit "pointer being freed was not
// allocated" ab: der Betreiber hatte ein Kontextmenue offen (ein
// STAPEL-lokales QMenu mitten in seinem eigenen exec()) und schloss
// dabei das Fenster. Die Aufraeumschleife im closeEvent sammelte das
// Menue als Top-Level-Fenster ein und rief deleteLater() darauf —
// "delete this" auf eine Stapeladresse.
//
// Damals wurde die Schleife entschaerft. Die Ursache — ein Qt-Kind auf
// dem Stapel, das seine eigene Ereignisschleife dreht — blieb. Dieser
// Prüfstand haelt fest, was ScopedChildWidget dagegen leistet:
// normales Aufraeumen, und Aufraeumen NACHDEM das Elternteil das Kind
// schon mitgenommen hat. Mit einem rohen Zeiger statt des QPointer
// faellt der zweite Fall in ein use-after-free.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QColorDialog>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <memory>

#include "gui/ScopedChildWidget.h"
#include "gui/SMeterWidget.h"
#include "gui/widgets/GradientPickerWidget.h"
#include "gui/widgets/RotorDialWidget.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

// NereusSDR: how often and how long the close-while-open cases look for
// the menu before giving up (the menu is then closed so exec() returns).
constexpr int kPollIntervalMs = 10;
constexpr int kMaxPolls = 300;

// NereusSDR: what a close-while-open run saw.
struct CloseWhileOpenResult {
    bool sawOpen = false;          // the popup or dialog came up
    bool windowDiedInLoop = false; // the window was deleted inside exec()
    bool openOneGone = false;      // the popup or dialog no longer exists
};

// NereusSDR: runs `open` (which enters a nested exec() loop) and, once
// `currentOpen` returns the popup or dialog, deletes `window` (the target's
// top level), the way a closed pan or a torn-down floating window takes its
// children with it. Reports, once exec() has returned, whether the window
// died inside the loop and whether the popup or dialog is gone.
CloseWhileOpenResult closeWindowWhileOpen(QWidget* window,
                                          const std::function<QWidget*()>& currentOpen,
                                          const std::function<void()>& open)
{
    QPointer<QWidget> watchWindow(window);
    QPointer<QWidget> watchOpen;
    CloseWhileOpenResult result;
    int polls = 0;
    QTimer poll;
    poll.setInterval(kPollIntervalMs);
    QObject::connect(&poll, &QTimer::timeout, &poll, [&]() {
        ++polls;
        if (QWidget* up = currentOpen()) {
            result.sawOpen = true;
            watchOpen = up;
            poll.stop();
            delete watchWindow.data();
            result.windowDiedInLoop = watchWindow.isNull();
            return;
        }
        if (polls >= kMaxPolls) {
            poll.stop();
            if (QWidget* popup = QApplication::activePopupWidget()) {
                popup->close();
            }
            if (QWidget* modal = QApplication::activeModalWidget()) {
                modal->close();
            }
        }
    });
    poll.start();
    open();
    poll.stop();
    result.openOneGone = result.sawOpen && watchOpen.isNull();
    return result;
}

// NereusSDR: the same for `target`'s right-click menu, delivered the way Qt
// delivers it.
CloseWhileOpenResult closeWindowWhileMenuIsOpen(QWidget* window, QWidget* target)
{
    return closeWindowWhileOpen(
        window,
        []() -> QWidget* { return qobject_cast<QMenu*>(QApplication::activePopupWidget()); },
        [target] {
            const QPoint local(10, 10);
            QContextMenuEvent ev(QContextMenuEvent::Mouse, local, target->mapToGlobal(local));
            QApplication::sendEvent(target, &ev);
        });
}

}  // namespace

class TstScopedChildWidget : public QObject {
    Q_OBJECT

private slots:

    // From Longpath tests/tst_scoped_child_widget.cpp:33-45 [@383f058]
    void normalScopeExitDestroysTheChild()
    {
        QWidget parent;
        QPointer<QMenu> watch;
        {
            ScopedChildWidget<QMenu> owner(&parent);
            watch = owner.get();
            QVERIFY(watch != nullptr);
            QVERIFY(bool(owner));
            QCOMPARE(watch->parentWidget(), &parent);
        }
        QVERIFY2(watch.isNull(), "The child outlived the scope");
    }

    // From Longpath tests/tst_scoped_child_widget.cpp:47-66 [@383f058]
    void theParentMayTakeTheChildFirst()
    {
        QPointer<QMenu> watch;
        {
            auto parent = std::make_unique<QWidget>();
            ScopedChildWidget<QMenu> owner(parent.get());
            watch = owner.get();
            QVERIFY(watch != nullptr);

            // Genau der Fall von 2026-08-30: das Elternteil stirbt,
            // waehrend der Halter noch im Rahmen steht.
            parent.reset();

            QVERIFY2(watch.isNull(), "Qt did not destroy the child with its parent");
            QVERIFY2(!owner, "The owner still holds a widget that is gone");
            // Und jetzt der Rahmenausgang: er darf NICHT ein zweites Mal
            // freigeben. Mit einem rohen Zeiger endet der Lauf hier.
        }
        QVERIFY(watch.isNull());
    }

    // Der Halter ist zugleich die Auskunft "hat mein Elternteil das
    // ueberlebt?" — genau die Frage, die jede Stelle nach exec() stellt,
    // bevor sie wieder an `this` fasst.
    // From Longpath tests/tst_scoped_child_widget.cpp:68-78 [@383f058]
    void theOwnerAnswersWhetherTheParentSurvived()
    {
        auto parent = std::make_unique<QWidget>();
        ScopedChildWidget<QMenu> owner(parent.get());
        QVERIFY2(bool(owner), "A freshly built owner must hold its widget");
        parent.reset();
        QVERIFY2(!owner, "After the parent dies the owner must report empty");
    }

    // From Longpath tests/tst_scoped_child_widget.cpp:80-89 [@383f058]
    void theChildStaysUsableWhileTheParentLives()
    {
        QWidget parent;
        ScopedChildWidget<QMenu> owner(&parent);
        QMenu& menu = *owner.get();
        QAction* a = menu.addAction(QStringLiteral("Test"));
        QVERIFY(a != nullptr);
        QCOMPARE(menu.actions().size(), 1);
        QCOMPARE(menu.actions().first(), a);
    }

    // NereusSDR: a real right-click menu whose window is closed while it is
    // open. With the menu on the stack this ended the process (double free).
    void rotorDialSurvivesItsWindowClosingDuringTheMenu()
    {
        auto window = std::make_unique<QWidget>();
        QPointer<QWidget> watchWindow(window.get());
        auto* dial = new RotorDialWidget(window.get());
        dial->resize(200, 200);
        window->resize(240, 240);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QWidget* raw = window.release();  // the poll deletes it mid-menu
        const CloseWhileOpenResult run = closeWindowWhileMenuIsOpen(raw, dial);
        if (!watchWindow.isNull()) {
            delete watchWindow.data();
        }
        QVERIFY2(run.sawOpen, "The dial's right-click menu never opened");
        QVERIFY2(run.windowDiedInLoop, "The window was not deleted while the menu was open");
        QVERIFY2(run.openOneGone, "The menu outlived its window");
    }

    // NereusSDR: the same for the VFO flag's right-click menu, a
    // NereusSDR-original menu converted to the same owner.
    void vfoFlagSurvivesItsWindowClosingDuringTheMenu()
    {
        auto window = std::make_unique<QWidget>();
        QPointer<QWidget> watchWindow(window.get());
        auto* flag = new VfoWidget(window.get());
        window->resize(400, 300);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QWidget* raw = window.release();  // the poll deletes it mid-menu
        const CloseWhileOpenResult run = closeWindowWhileMenuIsOpen(raw, flag);
        if (!watchWindow.isNull()) {
            delete watchWindow.data();
        }
        QVERIFY2(run.sawOpen, "The VFO flag's right-click menu never opened");
        QVERIFY2(run.windowDiedInLoop, "The window was not deleted while the menu was open");
        QVERIFY2(run.openOneGone, "The menu outlived its window");
    }

    // NereusSDR: the same for a heap menu freed by hand after exec(): the
    // S-meter's right-click menu. Freeing it after the window took it along
    // ended the process.
    void sMeterSurvivesItsWindowClosingDuringTheMenu()
    {
        auto window = std::make_unique<QWidget>();
        QPointer<QWidget> watchWindow(window.get());
        auto* meter = new SMeterWidget(window.get());
        meter->resize(200, 100);
        window->resize(240, 140);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QWidget* raw = window.release();  // the poll deletes it mid-menu
        const CloseWhileOpenResult run = closeWindowWhileMenuIsOpen(raw, meter);
        if (!watchWindow.isNull()) {
            delete watchWindow.data();
        }
        QVERIFY2(run.sawOpen, "The S-meter's right-click menu never opened");
        QVERIFY2(run.windowDiedInLoop, "The window was not deleted while the menu was open");
        QVERIFY2(run.openOneGone, "The menu outlived its window");
        // deleteLater() on a freed menu would land here.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // NereusSDR: the same for a dialog built on the stack and exec()ed: the
    // gradient picker's color dialog (double-click a stop), whose window is
    // closed while the dialog is open.
    void gradientPickerSurvivesItsWindowClosingDuringTheColorDialog()
    {
        auto window = std::make_unique<QWidget>();
        QPointer<QWidget> watchWindow(window.get());
        auto* picker = new GradientPickerWidget(window.get());
        picker->setStopsForTest({{0.0f, QColor(Qt::red), true},
                                 {1.0f, QColor(Qt::blue), true}});
        picker->resize(300, 60);
        window->resize(320, 80);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QWidget* raw = window.release();  // the poll deletes it mid-dialog
        const CloseWhileOpenResult run = closeWindowWhileOpen(
            raw,
            []() -> QWidget* { return qobject_cast<QColorDialog*>(QApplication::activeModalWidget()); },
            [picker] {
                const QPointF local(GradientPickerWidget::kPaddingPx, 12);
                QMouseEvent ev(QEvent::MouseButtonDblClick, local, picker->mapToGlobal(local),
                               Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(picker, &ev);
            });
        if (!watchWindow.isNull()) {
            delete watchWindow.data();
        }
        QVERIFY2(run.sawOpen, "The picker's color dialog never opened");
        QVERIFY2(run.windowDiedInLoop, "The window was not deleted while the dialog was open");
        QVERIFY2(run.openOneGone, "The color dialog outlived its window");
    }

    // NereusSDR: the owner forwards constructor arguments (a QMessageBox
    // built with icon, title, text, buttons and parent).
    void theOwnerForwardsConstructorArguments()
    {
        QWidget parent;
        ScopedChildWidget<QMessageBox> owner(QMessageBox::Warning, QStringLiteral("Title"),
                                             QStringLiteral("Text"), QMessageBox::Cancel,
                                             &parent);
        QVERIFY(bool(owner));
        QCOMPARE(owner.get()->parentWidget(), &parent);
        QCOMPARE(owner.get()->text(), QStringLiteral("Text"));
        QCOMPARE(owner.get()->icon(), QMessageBox::Warning);
        QCOMPARE(owner.get()->standardButtons(), QMessageBox::StandardButtons(QMessageBox::Cancel));
    }
};

QTEST_MAIN(TstScopedChildWidget)
#include "tst_scoped_child_widget.moc"
