// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 7.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), first-look fixes: a key already down, the
//                Linux pairing, menu bar menus. AI-assisted via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/KeyboardRouter.h"

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/HotKeyChord.h"
#include "gui/MainWindow.h"
#include "gui/PanFloatingWindow.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/containers/FreeCanvasSurface.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#ifdef Q_OS_MAC
#include "gui/hotkeys/MacMenuTrackingWatch.h"
#endif
#include "gui/widgets/StatusToast.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QTextEdit>

#include <optional>

namespace NereusSDR {

namespace {

const HotKeySource& keyboardSource()
{
    static const HotKeySource kSource{HotKeySourceKind::Keyboard, QString()};
    return kSource;
}

bool isArrow(int key)
{
    return key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down;
}

bool isBareModifier(int key)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
        return true;
    default:
        return false;
    }
}

bool takesTypedText(const QWidget* widget)
{
    if (qobject_cast<const QLineEdit*>(widget) != nullptr || qobject_cast<const QTextEdit*>(widget) != nullptr
        || qobject_cast<const QPlainTextEdit*>(widget) != nullptr
        || qobject_cast<const QAbstractSpinBox*>(widget) != nullptr) {
        return true;
    }
    const auto* const combo = qobject_cast<const QComboBox*>(widget);
    return combo != nullptr && combo->isEditable();
}

// One physical key, whatever its modifiers did between press and release. Qt
// reports Shift+comma as Less going down and Comma coming up, so the native key
// comes first. It is zero when the platform gives none (and for A on a Mac).
qint64 physicalKey(const QKeyEvent* event)
{
#ifdef Q_OS_LINUX
    constexpr bool kPreferScanCode = true;
#else
    constexpr bool kPreferScanCode = false;
#endif
    return KeyboardRouter::physicalKeyOf(event->nativeScanCode(), event->nativeVirtualKey(), event->key(),
                                         kPreferScanCode);
}

} // namespace

// Linux reports the key sym as the virtual key, and that moves with Shift. Its
// scan code is the keycode, which does not:
// From Qt qtbase src/plugins/platforms/xcb/qxcbkeyboard.cpp [6.11], handleKeyEvent:
//   xkb_state_key_get_one_sym(xkbState, code), passed as (code, sym) to
//   handleExtendedKeyEvent's (nativeScanCode, nativeVirtualKey).
// From Qt qtbase src/plugins/platforms/wayland/qwaylandinputdevice.cpp [6.11],
//   keyboard_key: the same, with code = key + 8.
qint64 KeyboardRouter::physicalKeyOf(quint32 nativeScanCode, quint32 nativeVirtualKey, int key, bool preferScanCode)
{
    constexpr qint64 kNativeKeyTag = qint64{1} << 32;
    constexpr qint64 kScanCodeTag = qint64{2} << 32;
    if (preferScanCode && nativeScanCode != 0) {
        return kScanCodeTag + nativeScanCode;
    }
    return nativeVirtualKey != 0 ? kNativeKeyTag + nativeVirtualKey : qint64{key};
}

KeyboardRouter::KeyboardRouter(HotKeyDispatcher* dispatcher, HotKeyBindings* bindings, MainWindow* window)
    : m_dispatcher(dispatcher)
    , m_bindings(bindings)
    , m_window(window)
{
    // The dispatcher lets go of what is held when the switch goes off.
    connect(m_bindings, &HotKeyBindings::enabledChanged, this, [this](bool on) {
        if (!on) {
            m_down.clear();
        }
    });
    // Not dropHolds: the dispatcher has already let go of every source. A key's
    // release after wake finds no entry, and one lost in sleep can't eat the next press.
    connect(m_dispatcher, &HotKeyDispatcher::holdsDroppedBySleep, this, [this] { m_down.clear(); });
    qApp->installEventFilter(this);
#ifdef Q_OS_MAC
    // Every native menu, the Apple menu included, which has no QMenu. A menu with
    // a QMenu drops twice, and the second finds nothing left to drop. Parented here.
    new MacMenuTrackingWatch([this] { dropHolds(); }, this);
#endif
    // A menu can hold on to the key-up, natively on a Mac. Menus added later
    // arrive as ActionAdded, in the filter.
    for (const QMenuBar* const bar : m_window->findChildren<QMenuBar*>()) {
        watchMenuBar(bar);
    }
}

void KeyboardRouter::watchMenuBar(const QMenuBar* bar)
{
    const QList<QAction*> actions = bar->actions();
    for (const QAction* const action : actions) {
        watchMenu(action);
    }
}

void KeyboardRouter::watchMenu(const QAction* action)
{
    if (auto* const menu = action->menu<QMenu*>()) {
        connect(menu, &QMenu::aboutToShow, this, &KeyboardRouter::dropHolds, Qt::UniqueConnection);
    }
}

QString KeyboardRouter::chordFromEvent(const QKeyEvent* event)
{
    const int key = event->key();
    if (key == 0 || key == Qt::Key_unknown || isBareModifier(key)) {
        return {};
    }
    const QString name = QKeySequence(QKeyCombination(Qt::Key(key))).toString(QKeySequence::PortableText);
    std::optional<HotKeyChord> chord = HotKeyChord::parse(name);
    if (!chord.has_value()) {
        return {};
    }
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    chord->ctrl = modifiers.testFlag(Qt::ControlModifier);
    chord->shift = modifiers.testFlag(Qt::ShiftModifier);
    chord->alt = modifiers.testFlag(Qt::AltModifier);
    chord->meta = modifiers.testFlag(Qt::MetaModifier);
    chord->num = modifiers.testFlag(Qt::KeypadModifier) && !isArrow(key);
    return chord->toString();
}

// This runs on every event in the application, so anything else leaves at the switch.
bool KeyboardRouter::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
        // Qt hands a key to the window and then to the focused widget and up its
        // parents. The press decides at the first widget: a key run here is eaten
        // there, and one passed on is passed on again by the same rules, so
        // nothing runs twice.
        if (watched->isWidgetType()) {
            return handlePress(static_cast<QWidget*>(watched), static_cast<QKeyEvent*>(event));
        }
        return false;
    case QEvent::KeyRelease:
        // No rule applies to letting go. It can arrive anywhere, and it must work.
        return handleRelease(static_cast<QKeyEvent*>(event));
    case QEvent::ActionAdded:
    case QEvent::ActionChanged:
        // A menu goes into the bar as an action, and setMenu can come later.
        if (const auto* const bar = qobject_cast<const QMenuBar*>(watched); bar != nullptr && bar->window() == m_window) {
            watchMenu(static_cast<QActionEvent*>(event)->action());
        }
        return false;
    case QEvent::FocusIn:
        if (watched->isWidgetType()) {
            noteFocusIn(static_cast<QWidget*>(watched), static_cast<QFocusEvent*>(event)->reason());
        }
        return false;
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate: {
        // A status toast asks not to take the keyboard, so its showing is no
        // change of window, even where the platform reports one (offscreen does).
        const QWidget* const active = QApplication::activeWindow();
        if (qobject_cast<const StatusToast*>(active) == nullptr) {
            dropHolds();
        }
        return false;
    }
    case QEvent::WindowBlocked:
    case QEvent::ApplicationDeactivate:
        // The release is going to some other window now, or nowhere.
        dropHolds();
        return false;
    case QEvent::ApplicationStateChange:
        // Both this and the window events are watched: macOS can miss the window event.
        if (static_cast<QApplicationStateChangeEvent*>(event)->applicationState() != Qt::ApplicationActive) {
            dropHolds();
        }
        return false;
    default:
        return false;
    }
}

bool KeyboardRouter::handlePress(QWidget* receiver, QKeyEvent* event)
{
    if (!m_bindings->enabled()) {
        return false;
    }
    const auto down = m_down.find(physicalKey(event));
    if (down != m_down.end()) {
        if (!pressAfterLostRelease(event, down.value())) {
            return handlePressOfKeyDown(receiver, event, down);
        }
        m_down.erase(down);
    }
    const QString chord = chordFromEvent(event);
    if (chord.isEmpty()) {
        return false;
    }
    const QString id = m_bindings->actionForChord(chord);
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr || !windowTakesKeys(receiver) || focusKeeps(event->key())) {
        return false;
    }

    m_down.insert(physicalKey(event), id);
    event->accept();
    // Design 7.6: repeat adds to a tune or a level. It never presses anything
    // else again. A repeat of a key not in m_down lost its press to a failsafe.
    if (event->isAutoRepeat() && action->gesture != HotKeyGesture::Turn) {
        return true;
    }

    const quint64 dropsBefore = m_dropCount;
    m_dispatcher->press(id, keyboardSource());
    // The press itself opened a dialog. The hold joined the ledger after that
    // drop, and its release can no longer be paired.
    if (m_dropCount != dropsBefore) {
        m_dispatcher->releaseAll(keyboardSource());
    }
    return true;
}

// On a Mac a repeat is marked, so an unmarked press of a Press action means its
// release went missing and the operator pressed again. Cocoa is known to lose
// key-ups for key equivalents; Qt forwards them to its windows
// (qtbase qcocoaapplication.mm [6.11], qt_maybeSendKeyEquivalentUpEvent), and this
// is for any that still go. A hold never presses again, on any platform.
bool KeyboardRouter::pressAfterLostRelease(const QKeyEvent* event, const QString& heldId)
{
#ifdef Q_OS_MAC
    const HotKeyAction* const held = HotKeyActionList::find(heldId);
    return !event->isAutoRepeat() && held != nullptr && held->gesture == HotKeyGesture::Press;
#else
    Q_UNUSED(event);
    Q_UNUSED(heldId);
    return false;
#endif
}

// Another press of a key still down, marked as a repeat or not (Windows can send
// a repeat with changed modifiers as a fresh press), under whatever chord its
// modifiers make now. What the key went down as stays until its release, so a
// hold's release can't go astray and nothing presses twice. Only tuning and
// levels go on, as the chord now says: hold Right, add Shift for tens.
bool KeyboardRouter::handlePressOfKeyDown(QWidget* receiver, QKeyEvent* event,
                                          QHash<qint64, QString>::iterator down)
{
    // Focus can have moved under the held key. Typed text still keeps its keys.
    if (!windowTakesKeys(receiver) || focusKeeps(event->key())) {
        return false;
    }
    event->accept();
    const HotKeyAction* const held = HotKeyActionList::find(down.value());
    const QString id = m_bindings->actionForChord(chordFromEvent(event));
    const HotKeyAction* const now = HotKeyActionList::find(id);
    if (held == nullptr || now == nullptr || held->gesture != HotKeyGesture::Turn
        || now->gesture != HotKeyGesture::Turn) {
        return true;
    }
    down.value() = id;
    m_dispatcher->press(id, keyboardSource());
    return true;
}

bool KeyboardRouter::handleRelease(const QKeyEvent* event)
{
    const qint64 key = physicalKey(event);
    const auto down = m_down.constFind(key);
    if (down == m_down.constEnd()) {
        return false;
    }
    if (event->isAutoRepeat()) {
        return true;
    }
    const QString id = down.value();
    m_down.erase(down);
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action != nullptr && action->gesture == HotKeyGesture::Hold && !m_down.values().contains(id)) {
        m_dispatcher->release(id, keyboardSource());
    }
    return true;
}

void KeyboardRouter::noteFocusIn(QWidget* widget, Qt::FocusReason reason)
{
    if (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason) {
        m_tabFocused = widget;
        return;
    }
    // Coming back from another window or a menu is not focus moving on.
    const bool cameBack = reason == Qt::ActiveWindowFocusReason || reason == Qt::PopupFocusReason;
    if (!cameBack || widget != m_tabFocused) {
        m_tabFocused.clear();
    }
}

void KeyboardRouter::dropHolds()
{
    ++m_dropCount;
    m_down.clear();
    m_dispatcher->releaseAll(keyboardSource());
}

// Design section 6, Windows.
bool KeyboardRouter::windowTakesKeys(const QWidget* receiver) const
{
    QWidget* const active = QApplication::activeWindow();
    if (active == nullptr || receiver->window() != active) {
        return false;
    }
    if (QApplication::activeModalWidget() != nullptr || QApplication::activePopupWidget() != nullptr
        || QWidget::keyboardGrabber() != nullptr) {
        return false;
    }
    return active == m_window || qobject_cast<PanFloatingWindow*>(active) != nullptr
        || qobject_cast<FloatingContainer*>(active) != nullptr;
}

// Design section 6, who wins inside those windows.
bool KeyboardRouter::focusKeeps(int key) const
{
    const QWidget* const focus = QApplication::focusWidget();
    if (takesTypedText(focus)) {
        return true;
    }
    if (isArrow(key)) {
        const ContainerManager* const manager = m_window->containerManager();
        if (manager != nullptr
            && (manager->arrangeMode()
                || (manager->arrangeHeld() && focus != nullptr
                    && qobject_cast<const FreeCanvasSurface*>(focus->parentWidget()) != nullptr))) {
            return true;
        }
    }
    // V2: a control reached with Tab keeps its arrows and its Space.
    return focus != nullptr && focus == m_tabFocused && (isArrow(key) || key == Qt::Key_Space);
}

} // namespace NereusSDR
