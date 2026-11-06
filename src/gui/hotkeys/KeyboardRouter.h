// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 7.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), first-look fixes: a key already down, the
//                Linux pairing, menu bar menus. AI-assisted via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Turns key presses into hot key actions: an application event filter, because
// a hold must see its release and a QShortcut has no release signal. It applies
// the design's "where keys work" rules and then calls the dispatcher. It never
// sets focus. Main thread only.
//
// The window installs this before ArrangeHoldFilter, because Qt calls the last
// installed application filter first: that filter must see each key before this
// one eats it.
#pragma once

#include "gui/NereusGuiExport.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QAction;
class QKeyEvent;
class QMenuBar;
class QWidget;

namespace NereusSDR {

class HotKeyBindings;
class HotKeyDispatcher;
class MainWindow;

class NEREUS_GUI_EXPORT KeyboardRouter : public QObject {
    Q_OBJECT

public:
    KeyboardRouter(HotKeyDispatcher* dispatcher, HotKeyBindings* bindings, MainWindow* window);

    // Canonical chord text, or empty for a bare modifier or a key with no
    // portable name. Num comes off the arrows, which macOS reports as keypad keys.
    static QString chordFromEvent(const QKeyEvent* event);

    // What pairs a press with its release: one physical key, whatever its
    // modifiers did in between. `preferScanCode` is true on Linux.
    static qint64 physicalKeyOf(quint32 nativeScanCode, quint32 nativeVirtualKey, int key, bool preferScanCode);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool handlePress(QWidget* receiver, QKeyEvent* event);
    bool handleRelease(const QKeyEvent* event);
    bool handlePressOfKeyDown(QWidget* receiver, QKeyEvent* event, QHash<qint64, QString>::iterator down);
    void noteFocusIn(QWidget* widget, Qt::FocusReason reason);
    void watchMenuBar(const QMenuBar* bar);
    void watchMenu(const QAction* action);
    void dropHolds();
    bool windowTakesKeys(const QWidget* receiver) const;
    bool focusKeeps(int key) const;

    HotKeyDispatcher* m_dispatcher;
    HotKeyBindings* m_bindings;
    MainWindow* m_window;

    QHash<qint64, QString> m_down;      // keys this router ate the press of, by physical key
    QPointer<QWidget> m_tabFocused;     // reached with Tab or Shift+Tab, until focus moves on
    quint64 m_dropCount{0};
};

} // namespace NereusSDR
