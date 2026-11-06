// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 7.
//                AI-assisted implementation via Anthropic Claude Code.
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

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
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
    constexpr qint64 kNativeKeyTag = qint64{1} << 32;
    const quint32 native = event->nativeVirtualKey();
    return native != 0 ? kNativeKeyTag + native : qint64{event->key()};
}

} // namespace

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
    qApp->installEventFilter(this);
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
        if (active == nullptr || !active->testAttribute(Qt::WA_ShowWithoutActivating)) {
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
    // Design 7.6: repeat adds to a tune or a level. It never presses anything else again.
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
