// =================================================================
// tests/tst_container_popout_removal.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR original. Removing a popped-out container: the settings
// dialog opened from the container's own menu survives the removal of
// the shell it was opened over, and no removed or emptied container
// leaves a blank shell. Offscreen; nothing connects, keys or discovers.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09 - New. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QContextMenuEvent>
#include <QListWidget>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <functional>

#include "core/AppSettings.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/FloatingContainer.h"

using namespace NereusSDR;

namespace {

constexpr int kWatchdogMs = 5000;

QPushButton* buttonWithText(QWidget* root, const QString& text)
{
    for (auto* button : root->findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

// A workspace with a main container and a home container B holding
// two entries; the first is popped out to a floating shell.
struct PoppedOut {
    QTemporaryDir dir;
    AppSettings settings{ dir.filePath(QStringLiteral("settings")) };
    ContainerWorkspaceStore store{ settings };
    ContainerContentRegistry registry;
    QWidget root;
    QSplitter* splitter = new QSplitter(&root);
    ContainerManager manager{ &root, splitter };
    QString shellId;
    QString entryId;

    PoppedOut()
    {
        root.resize(1000, 700);
        manager.setWorkspaceAdapter(&store, &registry);
        WorkspaceDocument d;
        d.mainContainerId = QStringLiteral("main");
        ContainerDocument main;
        main.id = QStringLiteral("main");
        ContainerDocument home;
        home.id = QStringLiteral("B");
        home.layout = ContentLayout::VerticalStack;
        home.dockMode = DockMode::OverlayDocked;
        home.geometry = QRect(20, 20, 300, 240);
        const auto popped = registry.makeEntry(QStringLiteral("TEXT"));
        home.contents = { popped, registry.makeEntry(QStringLiteral("TEXT")) };
        d.containers = { main, home };
        entryId = popped.id;
        root.show();
        if (manager.commitWorkspace(d, 0).status != CommitStatus::Saved) {
            return;
        }
        ContainerArrangeController arrange(store, &manager);
        if (!arrange.popOut(entryId).ok) {
            return;
        }
        shellId = store.snapshot().containers.last().id;
        QCoreApplication::processEvents();
    }
    bool hasDocument(const QString& id) const
    {
        for (const auto& c : store.snapshot().containers) {
            if (c.id == id) {
                return true;
            }
        }
        return false;
    }
};

// Drains deferred deletes the way returning to the main loop does.
void drain()
{
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
}

// Every visible top-level window that shows a container with nothing in it.
QStringList blankShells(const ContainerWorkspaceStore& store)
{
    QStringList blank;
    const auto snapshot = store.snapshot();
    for (QWidget* top : QApplication::topLevelWidgets()) {
        auto* form = qobject_cast<FloatingContainer*>(top);
        if (!form || !form->isVisible()) {
            continue;
        }
        auto* c = form->findChild<ContainerWidget*>();
        const auto doc = std::find_if(snapshot.containers.cbegin(), snapshot.containers.cend(),
                                      [c](const ContainerDocument& d) { return c && d.id == c->id(); });
        const bool empty = !c || !c->isVisible() || doc == snapshot.containers.cend()
            || std::none_of(doc->contents.cbegin(), doc->contents.cend(),
                            [](const ContentEntry& e) { return e.visible; });
        if (empty) {
            blank << (c ? c->id() : QStringLiteral("(no container)"));
        }
    }
    return blank;
}

} // namespace

class TstContainerPopoutRemoval : public QObject
{
    Q_OBJECT

private slots:
    // JJ, 2026-10-09: pop a container out, open Container Settings from
    // its right-click menu, remove the container and Apply. The dialog
    // was a child of the shell, which the removal deleted while the
    // dialog was still open: "pointer being freed was not allocated".
    void settingsRemoveFromTheShellMenu()
    {
        PoppedOut w;
        QVERIFY(!w.shellId.isEmpty());
        QPointer<ContainerWidget> shell = w.manager.container(w.shellId);
        QVERIFY(shell);
        QVERIFY(shell->isFloating());
        QPointer<QWidget> form = shell->window();
        QVERIFY(qobject_cast<FloatingContainer*>(form.data()));
        QVERIFY(form->isVisible());

        bool removed = false;
        bool applied = false;
        bool dialogOpened = false;
        QPointer<QDialog> seen;
        std::function<void(int)> drive;
        // The menu, then the dialog it opens, each run their own loop;
        // these steps run inside them as the clicks would.
        QTimer::singleShot(0, this, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            QVERIFY(menu);
            QAction* settings = nullptr;
            for (QAction* a : menu->actions()) {
                if (a->text().startsWith(QStringLiteral("Container Settings"))) {
                    settings = a;
                }
            }
            QVERIFY(settings);
            // The dialog opens once the menu has returned; wait for it.
            drive = [&](int tries) {
                auto* dialog = qobject_cast<ContainerSettingsDialog*>(QApplication::activeModalWidget());
                if (!dialog) {
                    if (tries > 0) {
                        QTimer::singleShot(10, this, [&drive, tries] { drive(tries - 1); });
                    }
                    return;
                }
                dialogOpened = true;
                seen = dialog;
                QPushButton* remove = buttonWithText(dialog, QStringLiteral("Remove / return contents"));
                QPushButton* apply = buttonWithText(dialog, QStringLiteral("Apply"));
                QVERIFY(remove && apply);
                remove->click();
                removed = true;
                apply->click();
                applied = true;
                // The deferred deletes posted by the Apply run in the
                // dialog's loop before it closes.
                QTimer::singleShot(50, this, [&] {
                    if (seen) {
                        seen->reject();
                    }
                });
            };
            QTimer::singleShot(0, this, [&drive] { drive(200); });
            menu->setActiveAction(settings);
            settings->trigger();
            menu->close();
        });
        QTimer::singleShot(kWatchdogMs, this, [] {
            if (auto* modal = QApplication::activeModalWidget()) {
                modal->close();
            }
            if (auto* popup = QApplication::activePopupWidget()) {
                popup->close();
            }
        });
        const QPoint at = shell->rect().center();
        QContextMenuEvent event(QContextMenuEvent::Mouse, at, shell->mapToGlobal(at));
        QApplication::sendEvent(shell.data(), &event);
        drain();

        QVERIFY(dialogOpened);
        QVERIFY(removed);
        QVERIFY(applied);
        QVERIFY2(!w.hasDocument(w.shellId), "the removed container is still in the workspace");
        QVERIFY2(w.manager.container(w.shellId) == nullptr, "the manager still lists the container");
        QVERIFY2(shell.isNull(), "the container widget was not destroyed");
        QVERIFY2(form.isNull() || !form->isVisible(), "the shell is still on screen");
        QVERIFY2(blankShells(w.store).isEmpty(), qPrintable(blankShells(w.store).join(QLatin1Char(','))));
        QVERIFY(w.hasDocument(QStringLiteral("B")));
    }

    void emptiedShellCloses_data()
    {
        QTest::addColumn<QString>("action");
        for (const char* action : { "hideContainer", "returnShell", "returnEntry", "moveEntry",
                                    "settingsRemoveObject" }) {
            QTest::newRow(action) << QString::fromLatin1(action);
        }
    }

    // A popped-out shell that loses its only entry, or is hidden or
    // returned, leaves the screen; it is never left open and empty.
    void emptiedShellCloses()
    {
        QFETCH(QString, action);
        PoppedOut w;
        QVERIFY(!w.shellId.isEmpty());
        QPointer<ContainerWidget> shell = w.manager.container(w.shellId);
        QVERIFY(shell);
        QPointer<QWidget> form = shell->window();
        QVERIFY(form->isVisible());
        ContainerArrangeController arrange(w.store, &w.manager);
        if (action == QLatin1String("hideContainer")) {
            emit shell->hideContainerRequested();
        } else if (action == QLatin1String("returnShell")) {
            emit shell->returnContainerRequested();
        } else if (action == QLatin1String("returnEntry")) {
            QVERIFY(arrange.returnEntry(w.entryId).ok);
        } else if (action == QLatin1String("moveEntry")) {
            QVERIFY(arrange.move(w.entryId, QStringLiteral("B"), 0).ok);
        } else {
            // Container Settings on the shell: select its object, Remove
            // object, Apply.
            ContainerSettingsDialog dialog(shell.data(), nullptr, &w.manager);
            auto* list = dialog.findChild<QListWidget*>(QStringLiteral("containerDraftContents"));
            QVERIFY(list);
            QCOMPARE(list->count(), 1);
            list->setCurrentRow(0);
            QPushButton* remove = buttonWithText(&dialog, QStringLiteral("Remove object"));
            QPushButton* apply = buttonWithText(&dialog, QStringLiteral("Apply"));
            QVERIFY(remove && apply);
            remove->click();
            apply->click();
            dialog.reject();
        }
        drain();
        qInfo().noquote() << action << "shell document:" << w.hasDocument(w.shellId)
                          << "widget:" << !shell.isNull()
                          << "form visible:" << (!form.isNull() && form->isVisible());
        QVERIFY2(blankShells(w.store).isEmpty(), qPrintable(blankShells(w.store).join(QLatin1Char(','))));
        // A shell that lost its last object is gone from the workspace.
        if (action == QLatin1String("moveEntry") || action == QLatin1String("settingsRemoveObject")) {
            QVERIFY(!w.hasDocument(w.shellId));
            QVERIFY(shell.isNull());
        }
    }

    // "Return all objects and close shell" from the shell's own menu
    // deletes the container that owns the menu while the menu is open.
    void returnShellFromItsMenu()
    {
        PoppedOut w;
        QVERIFY(!w.shellId.isEmpty());
        QPointer<ContainerWidget> shell = w.manager.container(w.shellId);
        QVERIFY(shell);
        bool triggered = false;
        QTimer::singleShot(0, this, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            QVERIFY(menu);
            for (QAction* a : menu->actions()) {
                if (a->text() == QStringLiteral("Return all objects and close shell")) {
                    triggered = true;
                    menu->setActiveAction(a);
                    a->trigger();
                    break;
                }
            }
            if (auto* open = QApplication::activePopupWidget()) {
                open->close();
            }
        });
        const QPoint at = shell->rect().center();
        QContextMenuEvent event(QContextMenuEvent::Mouse, at, shell->mapToGlobal(at));
        QApplication::sendEvent(shell.data(), &event);
        drain();
        QVERIFY(triggered);
        QVERIFY(!w.hasDocument(w.shellId));
        QVERIFY(shell.isNull());
        QVERIFY2(blankShells(w.store).isEmpty(), qPrintable(blankShells(w.store).join(QLatin1Char(','))));
    }
};

QTEST_MAIN(TstContainerPopoutRemoval)
#include "tst_container_popout_removal.moc"
