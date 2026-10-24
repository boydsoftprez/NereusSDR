// no-port-check: NereusSDR-original workspace preview live-input regressions.
// Modification history (NereusSDR):
//   2026-10-08 - Real mouse input on the preview's Arrange headers, menus
//                 and drags, routed the way AppKit routes it through native
//                 child views, by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - The entry header's pop-out button, stacked and on a free
//                 canvas, and JJ's bottom strip (Tuner and Amplifier side by
//                 side), by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - The profile rail after a profile is added: hover, the move
//                 to another profile's button and the click, with enter,
//                 leave and moves routed as Qt's cocoa plugin routes them, by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The header hide buttons (entry, free canvas leaf, group) and
//                 Containers > Hidden, with real clicks, by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The profile rail's remove cross renders 14x14, by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The profile rail when macOS drops the popover's Enter (or
//                 sends it a lone Leave): the pointer moves the cursor, and
//                 the rail stays open or closes by where the cursor is, by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Containers > Hidden with the workspace preview off: Hide
//                 container, a closed floating window and a hidden applet
//                 come back where they were, and the menu sits in the same
//                 place in both looks, by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-08 - Duplicate always gives a shown, floating copy beside the
//                 original, from the manager and from Container Settings, in
//                 both looks, by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Menus are read by a wait that polls until the popup is
//                 shown (PopupWait), not a fixed 50 ms shot that missed it
//                 under load and left exec() waiting; generous waits, by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Four ctest entries of the one binary (TestFunctionGroups),
//                 so no entry runs near the 120 s limit under load, by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - The look has no Lock setting: no padlock in the header,
//                 and a container committed locked keeps its pop-out and
//                 hide buttons enabled, by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//
// Why this test routes clicks itself: on macOS every widget under the main
// window is a native child view (the Metal meter and spectrum leaves promote
// their siblings, and those promote their ancestors; see nativeTopology()).
// AppKit hit-tests native views top-down and delivers a click to the topmost
// view under the cursor; it lets a click through a view only when that
// view's window carries Qt::WindowTransparentForInput. The widget attribute
// WA_TransparentForMouseEvents is honoured by Qt's alien routing only, inside
// one native view. The offscreen platform does no native hit-test: a click
// sent to the top-level window is routed by QWidget::childAt, which skips
// transparent widgets, so a plain QTest::mouseClick on the window or on the
// button never sees what JJ sees. nativeTarget() reproduces the AppKit rule
// over the real widget tree, and every event below goes, through
// QWindowSystemInterface, to the native window AppKit would pick.
//   2026-10-11 - The profile rail's delays are ProfileRailDock's own
//                 constants. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Without native child views (Linux) a click is routed by
//                 Qt inside the one window: hitFor() answers for both, and
//                 the native overlay check is skipped where no meter asks
//                 for a native window. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-22 - Eleven ctest entries, each about 45 s or less under load
//                 (a case is mostly its MainWindow build); Duplicate is one
//                 function per look. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDialog>
#include <QDrag>
#include <QElapsedTimer>
#include <QInputDialog>
#include <QLabel>
#include <QLoggingCategory>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QScreen>
#include <QSet>
#include <QScrollArea>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>
#include <functional>
#include <memory>
#include <optional>
#include "TestFunctionGroups.h"
#include "fakes/MainWindowTestSettings.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "gui/MainWindow.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/containers/FreeCanvasSurface.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/meters/MeterWidget.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/containers/WorkspaceProfiles.h"
#include "gui/widgets/ProfileRail.h"
#include "gui/widgets/ProfileRailDock.h"

using namespace NereusSDR;

namespace {

// Generous on purpose: the waits below return as soon as their condition
// holds, so a long limit costs nothing on a quiet machine and keeps a
// loaded one from failing a test that is right.
constexpr int kMenuWaitMs = 30000;
// How long a PopupWait keeps looking for its popup.
constexpr int kPopupWaitMs = 60000;

// Known lines this test filters, so the log shows only what is new:
// - the offscreen platform has no GPU and does not move or stack native
//   child windows, and says so for every native view;
// - QTest notes a move outside the pressed window, which is what AppKit
//   delivers during a drag, by design here;
// - QMenu asks for the native container as its transient parent and Qt
//   refuses it ("... must be a top level window", JJ's log). The popup keeps
//   the main window as its transient parent; MenuProbe asserts that.
QtMessageHandler g_previousHandler = nullptr;
void filterPlatformNoise(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg
        && (message.startsWith(QLatin1String("This plugin does not support"))
            || message.startsWith(QLatin1String("QRhiWidget:"))
            || (message.startsWith(QLatin1String("Mouse event at"))
                && message.contains(QLatin1String("occurs outside target window")))
            || message.endsWith(QLatin1String("must be a top level window.")))) {
        return;
    }
    if (g_previousHandler) {
        g_previousHandler(type, context, message);
    }
}

void settle()
{
    for (int pass = 0; pass < 8; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

bool passesInput(const QWidget* widget)
{
    const QWindow* window = widget->windowHandle();
    return window != nullptr && window->flags().testFlag(Qt::WindowTransparentForInput);
}

// The native descendants whose nearest native ancestor is `native`, in paint
// order (the last one is on top), as AppKit holds them as subviews.
void nativeSubviews(QWidget* parent, QList<QWidget*>& out)
{
    for (QObject* object : parent->children()) {
        auto* widget = qobject_cast<QWidget*>(object);
        if (!widget || widget->isWindow()) {
            continue;
        }
        if (widget->internalWinId()) {
            out.append(widget);
        } else {
            nativeSubviews(widget, out);
        }
    }
}

bool insideClip(QWidget* widget, QWidget* native, const QPoint& global)
{
    // A native child view is clipped by its alien ancestors up to its
    // native superview.
    for (QWidget* w = widget; w; w = w->parentWidget()) {
        if (!w->rect().contains(w->mapFromGlobal(global))) {
            return false;
        }
        if (w == native) {
            break;
        }
    }
    return true;
}

// AppKit's hit-test: the topmost visible native view under the point, skipping
// views whose window is transparent for input.
QWidget* nativeTarget(QWidget* native, const QPoint& global)
{
    QList<QWidget*> subviews;
    nativeSubviews(native, subviews);
    for (auto it = subviews.crbegin(); it != subviews.crend(); ++it) {
        QWidget* candidate = *it;
        if (!candidate->isVisible() || passesInput(candidate) || !insideClip(candidate, native, global)) {
            continue;
        }
        return nativeTarget(candidate, global);
    }
    return native;
}


// Qt's cocoa plugin (qnsview_mouse.mm, mouseMovedImpl, Qt 6.11): the content
// view picks the window under the mouse with QCocoaWindow::childWindowAt, which
// walks each QWindow's children in list order (raise() moves a window last) and
// takes the last exposed one whose geometry holds the point. When that window
// changes it posts one enter/leave pair; the move itself goes only to that
// window.
QWindow* cocoaChildWindowAt(QWindow* window, const QPoint& local)
{
    QWindow* target = window;
    for (QObject* object : window->children()) {
        auto* child = qobject_cast<QWindow*>(object);
        if (child && child->handle() && child->isExposed() && child->geometry().contains(local)) {
            target = cocoaChildWindowAt(child, local - child->position());
        }
    }
    return target;
}

// Moves the cursor (QCursor::pos(), as the Mac reports it) and nothing else.
// The offscreen cursor also posts an enter and a move to the top-level window,
// which Qt would route like alien widgets: a second route beside the cocoa
// one below. Those are flushed and dropped here, before they reach a window.
class QuietCursor : public QObject {
public:
    static void setPos(const QPoint& global)
    {
        QuietCursor quiet;
        qApp->installEventFilter(&quiet);
        QCursor::setPos(global);
        QWindowSystemInterface::flushWindowSystemEvents();
        qApp->removeEventFilter(&quiet);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::Enter:
        case QEvent::Leave:
            return qobject_cast<QWindow*>(watched) != nullptr;
        default:
            return false;
        }
    }
};

class CocoaPointer {
public:
    explicit CocoaPointer(QWidget* topLevel) : m_topLevel(topLevel) {}

    QWindow* under() const { return m_under; }

    void move(const QPoint& global)
    {
        QWindow* content = m_topLevel->windowHandle();
        const QPoint windowPoint = content->mapFromGlobal(global);
        QWindow* target = cocoaChildWindowAt(content, windowPoint);
        if (target != m_under) {
            if (m_under) {
                QWindowSystemInterface::handleEnterLeaveEvent(target, m_under, QPointF(windowPoint), QPointF(global));
            } else {
                QWindowSystemInterface::handleEnterEvent(target, QPointF(windowPoint), QPointF(global));
            }
            QWindowSystemInterface::flushWindowSystemEvents();
            m_under = target;
        }
        QTest::mouseMove(target, target->mapFromGlobal(global));
        // The cursor moves too, as on the Mac. After the cocoa events, not
        // before: set first, the badge offscreen intermittently never saw
        // its Enter (the cause inside Qt's offscreen cursor was not traced).
        QuietCursor::setPos(global);
    }

    // A hand moving from `from` to `to` in `steps` moves over `ms` of event
    // loop time.
    void glide(const QPoint& from, const QPoint& to, int steps, int ms)
    {
        for (int i = 1; i <= steps; ++i) {
            move(from + (to - from) * i / steps);
            QTest::qWait(ms / steps);
        }
    }

private:
    QWidget* m_topLevel;
    QWindow* m_under{nullptr};
};

// Enter and Leave that never arrive: swallowed for every widget under the
// given roots, ahead of every other filter (an application filter installed
// last runs first). WA_UnderMouse is still set by Qt's own routing, so a test
// that wants the state macOS leaves behind clears it with clearUnderMouse().
// `deliverLeave` sends the one lone Leave cocoa can send.
class DroppedEnterLeave : public QObject {
public:
    explicit DroppedEnterLeave(QList<QWidget*> roots) : m_roots(std::move(roots))
    {
        qApp->installEventFilter(this);
    }
    ~DroppedEnterLeave() override { qApp->removeEventFilter(this); }

    void clearUnderMouse()
    {
        for (QWidget* root : m_roots) {
            root->setAttribute(Qt::WA_UnderMouse, false);
            for (QWidget* child : root->findChildren<QWidget*>()) {
                child->setAttribute(Qt::WA_UnderMouse, false);
            }
        }
    }
    void deliverLeave(QWidget* widget)
    {
        m_allow = true;
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(widget, &leave);
        m_allow = false;
    }
    int dropped() const { return m_dropped; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (m_allow || (event->type() != QEvent::Enter && event->type() != QEvent::Leave)) {
            return false;
        }
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget) {
            return false;
        }
        for (QWidget* root : m_roots) {
            if (widget == root || root->isAncestorOf(widget)) {
                ++m_dropped;
                return true;
            }
        }
        return false;
    }

private:
    QList<QWidget*> m_roots;
    bool m_allow{false};
    int m_dropped{0};
};

// Moves only the cursor, in `steps` over `ms`: no Enter or Leave from the
// test itself.
void glideCursor(const QPoint& from, const QPoint& to, int steps, int ms)
{
    for (int i = 1; i <= steps; ++i) {
        QuietCursor::setPos(from + (to - from) * i / steps);
        QTest::qWait(ms / steps);
    }
}

// A mouse gesture as AppKit delivers it: the press goes to the view the
// hit-test picks and the moves and the release follow the pressed view.
class NativeMouse {
public:
    explicit NativeMouse(QWidget* topLevel) : m_topLevel(topLevel) {}

    QWidget* targetAt(const QPoint& global) const { return nativeTarget(m_topLevel, global); }

    // Where a press at `global` lands, for a check that it lands on `wanted`.
    // With native child views (macOS) that is the view AppKit picks, as
    // targetAt(), and nothing else is accepted. Where `wanted` has no native
    // view below the window (Linux: MeterWidget and SpectrumWidget ask for a
    // native window on macOS and Windows only, so the whole window is one
    // native view) the press goes to the window and Qt routes it with
    // QWidget::childAt, which skips a widget that is transparent for the
    // mouse; that widget is the answer there.
    QWidget* hitFor(const QWidget* wanted, const QPoint& global) const
    {
        QWidget* view = targetAt(global);
        const QWidget* wantedView = wanted->internalWinId() != 0 ? wanted : wanted->nativeParentWidget();
        if (view != m_topLevel || wantedView != m_topLevel) {
            return view;
        }
        QWidget* child = view->childAt(view->mapFromGlobal(global));
        return child != nullptr ? child : view;
    }

    void press(const QPoint& global)
    {
        m_grab = targetAt(global)->windowHandle();
        QTest::mousePress(m_grab, Qt::LeftButton, Qt::NoModifier, m_grab->mapFromGlobal(global));
    }
    void move(const QPoint& global)
    {
        QTest::mouseMove(m_grab, m_grab->mapFromGlobal(global));
    }
    void release(const QPoint& global)
    {
        QTest::mouseRelease(m_grab, Qt::LeftButton, Qt::NoModifier, m_grab->mapFromGlobal(global));
        m_grab = nullptr;
    }
    void click(const QPoint& global)
    {
        press(global);
        release(global);
    }

private:
    QWidget* m_topLevel;
    QWindow* m_grab{nullptr};
};

// Qt's drop routing inside the native view AppKit picked (QWidgetWindow's
// findDnDTarget): the widget under the point, or the nearest ancestor that
// accepts drops, up to a window.
QWidget* dropReceiver(QWidget* nativeView, const QPoint& global)
{
    QWidget* widget = nativeView->childAt(nativeView->mapFromGlobal(global));
    if (!widget) {
        widget = nativeView;
    }
    while (widget && !widget->isWindow() && !widget->acceptDrops()) {
        widget = widget->parentWidget();
    }
    return widget && widget->acceptDrops() ? widget : nullptr;
}

// Watches where a pressed-button move goes. The host starts its QDrag from
// its filter on the entry header and consumes that move; a move that is not
// a drag start goes on to the header's parents. QDrag has no parent and
// sends nothing, so this is the observable start of the drag.
struct DragStartSpy : QObject {
    DragStartSpy(QWidget* header, QWidget* beyond) : m_header(header), m_beyond(beyond)
    {
        qApp->installEventFilter(this);
    }
    ~DragStartSpy() override { qApp->removeEventFilter(this); }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::MouseButtonPress && watched == m_header) {
            m_press = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        }
        if (event->type() == QEvent::MouseMove
            && static_cast<QMouseEvent*>(event)->buttons().testFlag(Qt::LeftButton)) {
            const QPoint global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            if (watched == m_header
                && (global - m_press).manhattanLength() >= QApplication::startDragDistance()) {
                ++headerMovesPastDistance;
            } else if (watched == m_beyond) {
                ++propagated;
            }
        }
        return false;
    }
    int headerMovesPastDistance{0};
    int propagated{0};
private:
    QWidget* m_header;
    QWidget* m_beyond;
    QPoint m_press;
};

QPoint centreOf(const QWidget* widget)
{
    return widget->mapToGlobal(widget->rect().center());
}

// Waits for a popup menu to be on screen, then acts on it once. Polls every
// 10 ms from the event loop, so it runs inside the menu's exec() loop and
// never acts before the popup is shown, however long the machine takes to
// open it. A fixed single shot (the old 50 ms probe) fired before the popup
// under load, found nothing, and left the menu's exec() waiting forever.
// `accept` picks which popup (a submenu, say); empty takes any. Stops by
// itself after `timeoutMs`, and when it is destroyed.
struct PopupWait {
    QTimer poll;
    QElapsedTimer clock;

    void start(std::function<bool(QMenu*)> accept, std::function<void(QMenu*)> act, int timeoutMs = kPopupWaitMs)
    {
        poll.disconnect();
        poll.setInterval(10);
        clock.start();
        QObject::connect(&poll, &QTimer::timeout, [this, accept, act, timeoutMs] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (menu && menu->isVisible() && (!accept || accept(menu))) {
                poll.stop();
                act(menu);
                return;
            }
            if (clock.elapsed() > timeoutMs) {
                poll.stop();
            }
        });
        poll.start();
    }
};

// Reads the popup a click opened, from inside its exec() loop, and acts on
// it with real clicks. Records what it saw; closes the menu when `act` does
// not pick an item. `sub` is for a test that goes on into a submenu.
struct MenuProbe {
    bool opened{false};
    QStringList actions;
    QWindow* transientParent{nullptr};
    bool transientIsTopLevel{false};
    QHash<QString, bool> enabled;
    PopupWait wait;
    PopupWait sub;

    void arm(std::function<void(QMenu*)> act = {})
    {
        wait.start({}, [this, act](QMenu* menu) {
            opened = true;
            for (QAction* action : menu->actions()) {
                if (!action->isSeparator()) {
                    actions.append(action->text());
                    enabled.insert(action->text(), action->isEnabled());
                }
            }
            if (QWindow* window = menu->windowHandle()) {
                transientParent = window->transientParent();
                transientIsTopLevel = transientParent && transientParent->isTopLevel();
            }
            if (act) {
                act(menu);
            } else {
                menu->close();
            }
        });
    }

    // After `menu` opened a submenu: acts on the submenu once it is shown.
    void onSubmenu(QMenu* menu, std::function<void(QMenu*)> act)
    {
        sub.start([menu](QMenu* open) { return open != menu; }, std::move(act));
    }
};

QAction* actionNamed(QMenu* menu, const QString& text)
{
    for (QAction* action : menu->actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

// Clicks a menu item as the user does: a click on the item's rectangle.
void clickItem(QMenu* menu, const QString& text)
{
    QAction* action = actionNamed(menu, text);
    if (!action) {
        menu->close();
        return;
    }
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center());
}


// Closes any modal dialog that opens while armed and remembers it, so a click
// that lands on a badge's remove cross fails the test instead of hanging it.
struct ModalWatch {
    QTimer timer;
    QStringList seen;
    bool acceptNamePrompt{false};

    ModalWatch()
    {
        timer.setInterval(20);
        QObject::connect(&timer, &QTimer::timeout, [this] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            if (acceptNamePrompt && qobject_cast<QInputDialog*>(dialog)) {
                dialog->accept();
                return;
            }
            seen.append(QString::fromLatin1(dialog->metaObject()->className()) + QStringLiteral(" \"")
                        + dialog->windowTitle() + QStringLiteral("\""));
            dialog->reject();
        });
        timer.start();
    }
};

// What is wrong with the fanned-out rail as drawn, or an empty string: the
// popover must hold the rail at its full size, and the badges and the + must
// sit side by side inside it, none overlapping another.
QString railLayoutProblem(ProfileRailDock* dock)
{
    QWidget* popover = dock->popover();
    ProfileRail* rail = dock->rail();
    if (popover->width() < popover->sizeHint().width() || rail->width() < rail->sizeHint().width()) {
        return QStringLiteral("the rail opened %1 px wide in a %2 px popover for %3 px of badges")
            .arg(rail->width()).arg(popover->width()).arg(rail->sizeHint().width());
    }
    QList<QWidget*> buttons;
    for (const QString& id : rail->badges()) {
        buttons.append(rail->badgeButton(id));
    }
    buttons.append(rail->plusButton());
    for (int i = 0; i < buttons.size(); ++i) {
        const QRect r = buttons[i]->geometry();
        if (!rail->rect().contains(r)) {
            return QStringLiteral("button %1 (%2) at x %3..%4 is cut off by the %5 px rail")
                .arg(i).arg(static_cast<QPushButton*>(buttons[i])->text()).arg(r.left()).arg(r.right()).arg(rail->width());
        }
        if (i > 0 && buttons[i - 1]->geometry().right() >= r.left()) {
            return QStringLiteral("button %1 (%2) at x %3 overlaps button %4 ending at x %5")
                .arg(i).arg(static_cast<QPushButton*>(buttons[i])->text()).arg(r.left()).arg(i - 1)
                .arg(buttons[i - 1]->geometry().right());
        }
    }
    return {};
}

QString describeRect(const QRect& r)
{
    return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

QRect globalRect(const QWidget* widget)
{
    return QRect(widget->mapToGlobal(QPoint()), widget->size());
}

// Sets the workspace preview switch for one test function and puts the
// previous state back afterwards. Declare it before the fixture, so the
// window is gone before the switch changes back.
struct PreviewSwitch {
    bool previous;
    explicit PreviewSwitch(bool on)
        : previous(WorkspacePreview::enabled())
    {
        WorkspacePreview::setEnabledForTest(on);
    }
    ~PreviewSwitch() { WorkspacePreview::setEnabledForTest(previous); }
    PreviewSwitch(const PreviewSwitch&) = delete;
    PreviewSwitch& operator=(const PreviewSwitch&) = delete;
};

// JJ's workspace (from his report, never from his settings): the main panel
// alone in the right strip, header Hidden, a vertical stack of 15 entries,
// strip thickness 398; a second container floating and hidden.
struct LiveFixture {
    std::unique_ptr<MainWindow> window;
    ContainerManager* manager{nullptr};
    ContainerWorkspaceStore* store{nullptr};
    ContainerContentRegistry* registry{nullptr};
    QString mainId;
    QString rxEntryId;
    QString displayEntryId;
    QString modMonitorEntryId;

    bool build()
    {
        window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window->resize(1512, 869);
        window->show();
        if (!QTest::qWaitForWindowExposed(window.get())) {
            return false;
        }
        manager = window->findChild<ContainerManager*>();
        if (!manager) {
            return false;
        }
        store = manager->workspaceStore();
        registry = manager->contentRegistry();
        WorkspaceDocument document = store->snapshot();
        ContainerDocument main;
        main.id = document.mainContainerId.isEmpty() ? QStringLiteral("main") : document.mainContainerId;
        main.name = QStringLiteral("Main Panel");
        main.layout = ContentLayout::VerticalStack;
        main.dockMode = DockMode::PanelDocked;
        main.edge = DockEdge::Right;
        main.header = HeaderMode::Hidden;
        const QStringList types{"meter.ananMulti", "applet:Display", "applet:mod_monitor", "applet:rx",
                                "applet:TX", "applet:RADE", "applet:vax", "applet:tci",
                                "applet:tci_clients", "applet:RfKit", "applet:amp", "applet:pure_signal",
                                "applet:PHCW", "applet:tuner", "applet:s_meter"};
        for (const QString& type : types) {
            ContentEntry entry = registry->makeEntry(type);
            entry.context["sliceId"] = 0;
            if (type == QStringLiteral("applet:rx")) {
                rxEntryId = entry.id;
            } else if (type == QStringLiteral("applet:Display")) {
                displayEntryId = entry.id;
            } else if (type == QStringLiteral("applet:mod_monitor")) {
                modMonitorEntryId = entry.id;
            }
            main.contents.append(entry);
        }
        ContainerDocument bars;
        bars.id = QStringLiteral("meter-bars");
        bars.name = QStringLiteral("Meter Bars");
        bars.layout = ContentLayout::VerticalStack;
        bars.dockMode = DockMode::Floating;
        bars.visible = false;
        bars.geometry = QRect(200, 200, 300, 400);
        for (int i = 0; i < 13; ++i) {
            ContentEntry entry = registry->makeEntry(QStringLiteral("meter.customBar"));
            entry.context["sliceId"] = 0;
            bars.contents.append(entry);
        }
        document.mainContainerId = main.id;
        document.containers = {main, bars};
        document.arrangement.strips[DockEdge::Right].thickness = 398;
        mainId = main.id;
        if (manager->commitWorkspace(document, document.revision).status != CommitStatus::Saved) {
            return false;
        }
        settle();
        return true;
    }

    bool enterArrange()
    {
        // Containers > Arrange Workspace, the menu bar item.
        auto* arrange = window->findChild<QAction*>(QStringLiteral("containersArrangeWorkspace"));
        if (!arrange) {
            return false;
        }
        arrange->trigger();
        settle();
        return manager->arrangeMode();
    }

    ContainerWidget* mainContainer() const { return manager->container(mainId); }
    WorkspaceGroupHeader* groupHeader() const { return mainContainer()->workspaceHeader(); }
    ContainerContentHost* host() const { return manager->contentHost(mainId); }

    WorkspaceEntryHeader* entryHeader(const QString& id) const
    {
        return host()->findChild<WorkspaceEntryHeader*>(QStringLiteral("workspaceEntryHeader_") + id);
    }

    // Scrolls an entry's header into the strip's view, as the user scrolls.
    void reveal(QWidget* widget)
    {
        if (auto* scroll = host()->findChild<QScrollArea*>()) {
            scroll->ensureWidgetVisible(widget, 0, 40);
        }
        settle();
    }

    const ContainerDocument* document(const QString& id) const
    {
        static ContainerDocument found;
        for (const auto& d : store->snapshot().containers) {
            if (d.id == id) {
                found = d;
                return &found;
            }
        }
        return nullptr;
    }

    int entryIndex(const QString& containerId, const QString& entryId) const
    {
        const ContainerDocument* d = document(containerId);
        if (!d) {
            return -1;
        }
        for (int i = 0; i < d->contents.size(); ++i) {
            if (d->contents[i].id == entryId) {
                return i;
            }
        }
        return -1;
    }

    // The one pop-out shell that holds `entryId`, or an empty id.
    QString shellHolding(const QString& entryId) const
    {
        for (const auto& d : store->snapshot().containers) {
            if (d.popOutShell && d.contents.size() == 1 && d.contents[0].id == entryId) {
                return d.id;
            }
        }
        return {};
    }

    // Commits a change to the current workspace.
    ContentEntry entry(const QString& containerId, const QString& entryId) const
    {
        for (const auto& d : store->snapshot().containers) {
            if (d.id != containerId) {
                continue;
            }
            for (const auto& e : d.contents) {
                if (e.id == entryId) {
                    return e;
                }
            }
        }
        return {};
    }

    QMenu* hiddenMenu() const
    {
        return window->findChild<QMenu*>(QStringLiteral("containersHidden"));
    }

    // Opens Containers > Hidden, reads it, and clicks `item` (empty: closes
    // it). Returns the items as listed, with "!" before a disabled one.
    QStringList openHidden(const QString& item = {})
    {
        QMenu* menu = hiddenMenu();
        if (!menu) {
            return {QStringLiteral("(no Hidden menu)")};
        }
        QStringList listed;
        PopupWait wait;
        wait.start({}, [&listed, item](QMenu* open) {
            for (QAction* action : open->actions()) {
                listed.append((action->isEnabled() ? QString() : QStringLiteral("!")) + action->text());
            }
            if (item.isEmpty()) {
                open->close();
            } else {
                clickItem(open, item);
            }
        });
        menu->exec(window->mapToGlobal(QPoint(200, 200)));
        settle();
        return listed;
    }

    // Where Containers > Hidden sits: the text of the item above it and
    // whether a separator follows it, as "<above> | <below>".
    QString hiddenMenuPlace() const
    {
        QMenu* hidden = hiddenMenu();
        if (!hidden) {
            return QStringLiteral("(no Hidden menu)");
        }
        for (QMenu* menu : window->findChildren<QMenu*>()) {
            const QList<QAction*> actions = menu->actions();
            const int at = actions.indexOf(hidden->menuAction());
            if (at < 0) {
                continue;
            }
            const QString above = at > 0 ? actions[at - 1]->text() : QStringLiteral("(first)");
            const QString below = at + 1 < actions.size()
                ? (actions[at + 1]->isSeparator() ? QStringLiteral("(separator)") : actions[at + 1]->text())
                : QStringLiteral("(last)");
            return above + QStringLiteral(" | ") + below;
        }
        return QStringLiteral("(not in a menu)");
    }

    // Right-clicks a container's chrome and picks `item` from its menu.
    bool pickFromContainerMenu(ContainerWidget* container, const QString& item)
    {
        bool picked = false;
        PopupWait wait;
        wait.start({}, [&picked, item](QMenu* open) {
            picked = actionNamed(open, item) != nullptr;
            clickItem(open, item);
        });
        const QPoint local(container->width() / 2, 8);
        QContextMenuEvent event(QContextMenuEvent::Mouse, local, container->mapToGlobal(local));
        QApplication::sendEvent(container, &event);
        settle();
        return picked;
    }

    // The Containers > Applets item for an applet.
    QAction* appletAction(const QString& text) const
    {
        for (QAction* action : window->findChildren<QAction*>()) {
            if (action->isCheckable() && action->text() == text
                && action->toolTip().startsWith(QStringLiteral("Show or hide the"))) {
                return action;
            }
        }
        return nullptr;
    }

    bool edit(const std::function<void(WorkspaceDocument&)>& change)
    {
        WorkspaceDocument document = store->snapshot();
        change(document);
        const bool saved = manager->commitWorkspace(document, document.revision).status == CommitStatus::Saved;
        settle();
        return saved;
    }


    // Back to one profile, active, before a profile test (the settings
    // profile is shared by the test functions of this process).
    bool resetProfiles(WorkspaceProfiles* profiles)
    {
        const QStringList ids = profiles->ids();
        if (ids.isEmpty()) {
            return false;
        }
        if (profiles->activeId() != ids.first() && !profiles->switchTo(ids.first()).ok()) {
            return false;
        }
        for (int i = 1; i < ids.size(); ++i) {
            if (!profiles->remove(ids.at(i)).ok()) {
                return false;
            }
        }
        settle();
        return profiles->ids().size() == 1;
    }

    // Glides from `from` onto the collapsed badge and waits for the rail to
    // fan out (the show delay).
    bool hoverOpen(CocoaPointer& pointer, ProfileRailDock* dock, const QPoint& from)
    {
        const QPoint onBadge = centreOf(dock->collapsedBadge());
        pointer.glide(from, onBadge, 4, 40);
        return QTest::qWaitFor([dock] { return dock->isOpen(); }, ProfileRailDock::kShowDelayMs + 500);
    }

    QString describe(QWidget* widget) const
    {
        return QString::fromLatin1(widget->metaObject()->className()) + QLatin1Char('#') + widget->objectName();
    }
};

// The rows and the body of the two duplicate cases (one per look) in
// TstWorkspaceLiveInput.
void addDuplicateRows(bool preview)
{
    QTest::addColumn<bool>("preview");
    QTest::addColumn<bool>("fromDialog");
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("visible");
    const int floating = int(DockMode::Floating);
    const int overlay = int(DockMode::OverlayDocked);
    const char* look = preview ? "preview on" : "preview off";
    for (const bool dialog : {false, true}) {
        const char* path = dialog ? "Container Settings" : "manager";
        QTest::addRow("%s, %s: hidden floating", look, path) << preview << dialog << floating << false;
        QTest::addRow("%s, %s: visible floating", look, path) << preview << dialog << floating << true;
        QTest::addRow("%s, %s: hidden docked", look, path) << preview << dialog << overlay << false;
    }
}

void checkDuplicateIsShownBesideTheOriginal()
{
    QFETCH(bool, preview);
    QFETCH(bool, fromDialog);
    QFETCH(int, mode);
    QFETCH(bool, visible);
    constexpr int kOffset = 24;   // ContainerManager::kDuplicateOffsetPx
    PreviewSwitch look(preview);
    LiveFixture f;
    QVERIFY(f.build());
    const QString id = QStringLiteral("solo");
    const QRect placed = DockMode(mode) == DockMode::Floating ? QRect(100, 100, 300, 200) : QRect(40, 60, 320, 220);
    QVERIFY(f.edit([&](WorkspaceDocument& d) {
        ContainerDocument solo;
        solo.id = id;
        solo.name = QStringLiteral("Solo");
        solo.layout = ContentLayout::VerticalStack;
        solo.dockMode = DockMode(mode);
        solo.visible = visible;
        solo.geometry = placed;
        ContentEntry entry = f.registry->makeEntry(QStringLiteral("meter.customBar"));
        entry.context["sliceId"] = 0;
        solo.contents.append(entry);
        d.containers.append(solo);
    }));
    ContainerWidget* original = f.manager->container(id);
    QVERIFY(original);
    const QRect originalOnScreen = DockMode(mode) == DockMode::Floating
        ? f.document(id)->geometry
        : QRect(original->mapToGlobal(QPoint(0, 0)), original->size());
    const ContainerDocument before = *f.document(id);
    QSet<QString> ids;
    for (const auto& d : f.store->snapshot().containers) {
        ids.insert(d.id);
    }

    if (fromDialog) {
        auto dialog = std::make_unique<ContainerSettingsDialog>(original, f.window.get(), f.manager);
        QPushButton* duplicate = nullptr;
        QPushButton* apply = nullptr;
        for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Duplicate")) {
                duplicate = button;
            } else if (button->text() == QStringLiteral("Apply")) {
                apply = button;
            }
        }
        QVERIFY(duplicate && apply);
        duplicate->click();
        apply->click();
        dialog.reset();
    } else {
        QVERIFY(f.manager->duplicateContainer(id));
    }
    settle();

    QCOMPARE(*f.document(id), before);
    QString copyId;
    for (const auto& d : f.store->snapshot().containers) {
        if (!ids.contains(d.id)) {
            QVERIFY2(copyId.isEmpty(), "more than one new container");
            copyId = d.id;
        }
    }
    QVERIFY(!copyId.isEmpty());
    const ContainerDocument* copy = f.document(copyId);
    QVERIFY(copy->visible);
    QVERIFY(copy->config.value("enabled").toBool(true));
    QCOMPARE(copy->dockMode, DockMode::Floating);
    ContainerWidget* widget = f.manager->container(copyId);
    QVERIFY(widget && widget->isFloating());
    QWidget* form = widget->window();
    QVERIFY(form != f.window.get() && form->isVisible());
    bool onScreen = false;
    for (QScreen* screen : QGuiApplication::screens()) {
        onScreen = onScreen || screen->availableGeometry().contains(form->geometry());
    }
    QVERIFY2(onScreen, qPrintable(describeRect(form->geometry())));
    QVERIFY(form->geometry().topLeft() != originalOnScreen.topLeft());
    if (DockMode(mode) == DockMode::Floating) {
        QCOMPARE(copy->geometry, placed.translated(kOffset, kOffset));
        QCOMPARE(form->geometry(), placed.translated(kOffset, kOffset));
    }
}

} // namespace

class TstWorkspaceLiveInput : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules("nereus.*.debug=false\nnereus.*.info=false\nnereussdr.*.info=false");
        g_previousHandler = qInstallMessageHandler(filterPlatformNoise);
        WorkspacePreview::setEnabledForTest(true);
        AppSettings::setProfileOverride(QStringLiteral("workspace-live-input-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
        // No audio first-run dialog: on Linux the VAX first-run dialog opens
        // over the window and takes the activation the profile rail's
        // keyboard cases give the main window.
        Test::markAudioFirstRunDone();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes(5));
    }
    void cleanupTestCase()
    {
        QFile::remove(AppSettings::instance().filePath());
        RadioDiscovery::clearHoldOffForTest();
        WorkspacePreview::setEnabledForTest(false);
    }

    // The proxy for macOS: every visible native view that draws over others
    // and claims to be transparent for the mouse must say so to the window
    // system, or AppKit gives it every click under it.

    void nativeOverlaysLetClicksThrough()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        // The premise: the container is a native child view, as JJ's log shows
        // ("ContainerWidgetClassWindow must be a top level window"). That is
        // so where the meter and spectrum leaves ask for a native window
        // (macOS, Windows; MeterWidget.cpp, SpectrumWidget.cpp). Elsewhere
        // the window is one native view, Qt routes every click itself and
        // honours WA_TransparentForMouseEvents, and there is no native
        // overlay to check.
        bool nativeLeaf = false;
        for (const MeterWidget* meter : f.window->findChildren<MeterWidget*>()) {
            nativeLeaf = nativeLeaf || meter->testAttribute(Qt::WA_NativeWindow);
        }
        if (!nativeLeaf) {
            QVERIFY2(f.mainContainer()->internalWinId() == 0,
                     "the container is a native view with no native meter under it");
            QSKIP("No native child views on this platform: the meters ask for no native window, "
                  "so the main window is one native view and Qt routes clicks itself.");
        }
        QVERIFY(f.mainContainer()->internalWinId() != 0);
        QStringList blocking;
        for (QWidget* widget : f.window->findChildren<QWidget*>()) {
            if (!widget->isWindow() && widget->internalWinId() && widget->isVisible()
                && widget->testAttribute(Qt::WA_TransparentForMouseEvents) && !passesInput(widget)) {
                blocking.append(f.describe(widget) + QStringLiteral(" in ")
                                + f.describe(widget->parentWidget()));
            }
        }
        QVERIFY2(blocking.isEmpty(), qPrintable(QStringLiteral("native views that swallow clicks: ") + blocking.join(", ")));
    }

    void groupHeaderOptionsOpensTheMenuInArrange()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceGroupHeader* header = f.groupHeader();
        QVERIFY(header && header->isVisible());
        NativeMouse mouse(f.window.get());
        const QPoint at = centreOf(header->optionsButton());
        QWidget* hit = mouse.hitFor(header, at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        MenuProbe probe;
        probe.arm();
        mouse.click(at);
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        QVERIFY(probe.actions.contains(QStringLiteral("Move to Edge")));
        QVERIFY(probe.actions.contains(QStringLiteral("Pop Out")));
        // The popup's transient parent is the main window, a top-level.
        QVERIFY(probe.transientIsTopLevel);
    }

    void entryHeaderOptionsOpensTheMenuInArrange()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceEntryHeader* header = f.entryHeader(f.rxEntryId);
        QVERIFY(header);
        f.reveal(header);
        QVERIFY(header->isVisible());
        NativeMouse mouse(f.window.get());
        const QPoint at = centreOf(header->optionsButton());
        QWidget* hit = mouse.hitFor(header, at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        MenuProbe probe;
        probe.arm();
        mouse.click(at);
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        QVERIFY(probe.actions.contains(QStringLiteral("Pop Out")));
        QVERIFY(probe.enabled.value(QStringLiteral("Pop Out")));
        QVERIFY(probe.transientIsTopLevel);
    }

    void headerDragMovesTheContainerToTheLeftEdge()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceGroupHeader* header = f.groupHeader();
        auto* grip = header->findChild<QLabel*>(QStringLiteral("workspaceGrip"));
        QVERIFY(grip);
        NativeMouse mouse(f.window.get());
        const QPoint from = centreOf(grip);
        QWidget* hit = mouse.hitFor(header, from);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the press lands on ") + f.describe(hit)));
        EdgeDockArea* area = f.manager->edgeDockArea();
        const QPoint to = area->mapToGlobal(area->targetBandRect(DockEdge::Left).center());
        mouse.press(from);
        mouse.move(from + QPoint(-20, 0));
        mouse.move(to + QPoint(10, 0));
        mouse.move(to);
        mouse.release(to);
        settle();
        QCOMPARE(f.document(f.mainId)->edge, DockEdge::Left);
        QCOMPARE(f.document(f.mainId)->dockMode, DockMode::PanelDocked);
    }

    void entryHeaderDragReordersTheEntry()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        // Mod Monitor (third) above Display (second): both in view at once,
        // as the strip shows them.
        QCOMPARE(f.entryIndex(f.mainId, f.modMonitorEntryId), 2);
        WorkspaceEntryHeader* header = f.entryHeader(f.modMonitorEntryId);
        WorkspaceEntryHeader* display = f.entryHeader(f.displayEntryId);
        QVERIFY(header && display);
        f.reveal(display);
        QVERIFY(header->isVisible() && display->isVisible());
        auto* grip = header->findChild<QLabel*>(QStringLiteral("workspaceGrip"));
        QVERIFY(grip);
        NativeMouse mouse(f.window.get());
        const QPoint from = centreOf(grip);
        QWidget* hit = mouse.hitFor(header, from);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the press lands on ") + f.describe(hit)));
        // The press and the move past the drag distance reach the header and
        // the host takes the move as its drag start. The offscreen platform's
        // drag returns at once (it has no drag-and-drop), so the drop half is
        // delivered below, to the view AppKit would give it.
        DragStartSpy started(header, f.host());
        mouse.press(from);
        mouse.move(from + QPoint(0, QApplication::startDragDistance() + 4));
        mouse.release(from + QPoint(0, QApplication::startDragDistance() + 4));
        QVERIFY(started.headerMovesPastDistance > 0);
        QCOMPARE(started.propagated, 0);
        // AppKit delivers the drop to the native view under the cursor. Just
        // below Display's header, above its centre: Mod Monitor goes first.
        const QPoint to = display->mapToGlobal(QPoint(display->width() / 2, display->height() + 4));
        QWidget* dropView = mouse.targetAt(to);
        QWidget* receiver = dropReceiver(dropView, to);
        QVERIFY2(receiver != nullptr,
                 qPrintable(QStringLiteral("no view accepts the drop under the cursor; it lands on ")
                            + f.describe(dropView)));
        QMimeData mime;
        mime.setData(ContainerArrangeController::kMimeType,
                     f.manager->arrangeController()->mimeData(f.modMonitorEntryId));
        const QPointF local = receiver->mapFromGlobal(QPointF(to));
        QDragEnterEvent enter(local.toPoint(), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(receiver, &enter);
        QVERIFY(enter.isAccepted());
        QDragMoveEvent over(local.toPoint(), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(receiver, &over);
        QVERIFY(over.isAccepted());
        QDropEvent drop(local, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(receiver, &drop);
        QVERIFY(drop.isAccepted());
        settle();
        QCOMPARE(f.entryIndex(f.mainId, f.modMonitorEntryId), 1);
        QCOMPARE(f.entryIndex(f.mainId, f.displayEntryId), 2);
    }

    void moveToEdgeFromTheMenu()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        NativeMouse mouse(f.window.get());
        MenuProbe probe;
        bool submenuOpened = false;
        probe.arm([&](QMenu* menu) {
            probe.onSubmenu(menu, [&](QMenu* sub) {
                submenuOpened = true;
                clickItem(sub, QStringLiteral("Left"));
            });
            clickItem(menu, QStringLiteral("Move to Edge"));
        });
        mouse.click(centreOf(f.groupHeader()->optionsButton()));
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(submenuOpened, kMenuWaitMs);
        settle();
        QCOMPARE(f.document(f.mainId)->edge, DockEdge::Left);
    }

    void popOutAnEntryFromItsMenu()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceEntryHeader* header = f.entryHeader(f.rxEntryId);
        QVERIFY(header);
        f.reveal(header);
        NativeMouse mouse(f.window.get());
        MenuProbe probe;
        probe.arm([](QMenu* menu) { clickItem(menu, QStringLiteral("Pop Out")); });
        mouse.click(centreOf(header->optionsButton()));
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        settle();
        QCOMPARE(f.entryIndex(f.mainId, f.rxEntryId), -1);
        bool shell = false;
        for (const auto& d : f.store->snapshot().containers) {
            if (d.popOutShell && d.contents.size() == 1 && d.contents[0].id == f.rxEntryId) {
                shell = d.dockMode == DockMode::Floating;
            }
        }
        QVERIFY(shell);
        // Put RX back before the window goes: the offscreen platform's
        // backing store keeps the moved native view and faults on teardown.
        QVERIFY(f.manager->arrangeController()->returnEntry(f.rxEntryId).ok);
        settle();
    }

    // What made it look locked: under the cursor the header's open hand never
    // showed (the view on top has the arrow), and nothing answered. There is
    // no lock glyph (the look has none) and no disabled item.
    void arrangeDoesNotLookLocked()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceGroupHeader* header = f.groupHeader();
        auto* grip = header->findChild<QLabel*>(QStringLiteral("workspaceGrip"));
        QVERIFY(grip);
        for (const QWidget* child : header->findChildren<QWidget*>()) {
            QVERIFY2(!QString::fromLatin1(child->metaObject()->className()).contains(QStringLiteral("Lock")),
                     child->metaObject()->className());
        }
        NativeMouse mouse(f.window.get());
        QWidget* view = mouse.targetAt(centreOf(grip));
        QWidget* under = view->childAt(view->mapFromGlobal(centreOf(grip)));
        QWidget* shown = under ? under : view;
        QVERIFY2(shown->cursor().shape() == Qt::OpenHandCursor,
                 qPrintable(QStringLiteral("cursor over the grip comes from ") + f.describe(shown)));
        WorkspaceEntryHeader* entry = f.entryHeader(f.rxEntryId);
        QVERIFY(entry);
        f.reveal(entry);
        QWidget* entryView = mouse.hitFor(entry, centreOf(entry));
        QVERIFY2(entryView == entry || entry->isAncestorOf(entryView),
                 qPrintable(QStringLiteral("the entry header is under ") + f.describe(entryView)));
        QCOMPARE(entry->cursor().shape(), Qt::OpenHandCursor);
    }

    // The entry header's ↗: left of ⋯, the entry button size, and one real
    // click pops the entry out into a new floating container.
    void entryPopOutButtonPopsTheEntryOut()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceEntryHeader* header = f.entryHeader(f.rxEntryId);
        QVERIFY(header);
        f.reveal(header);
        QPushButton* popOut = header->popOutButton();
        QVERIFY(popOut && popOut->isVisible());
        QCOMPARE(popOut->objectName(), QStringLiteral("workspaceEntryPopOut"));
        QCOMPARE(popOut->text(), QStringLiteral("↗"));
        QCOMPARE(popOut->toolTip(), QStringLiteral("Pop out"));
        QCOMPARE(popOut->size(), header->optionsButton()->size());
        // Immediately left of ⋯: nothing between them but the row spacing.
        QVERIFY(popOut->geometry().right() < header->optionsButton()->geometry().left());
        for (QWidget* child : header->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            if (child != popOut && child != header->optionsButton() && child != header->hideButton()
                && child->isVisible()) {
                QVERIFY(child->geometry().right() < popOut->geometry().left());
            }
        }
        QVERIFY(popOut->isEnabled());
        NativeMouse mouse(f.window.get());
        const QPoint at = centreOf(popOut);
        QWidget* hit = mouse.hitFor(header, at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        mouse.click(at);
        settle();
        QCOMPARE(f.entryIndex(f.mainId, f.rxEntryId), -1);
        const QString shell = f.shellHolding(f.rxEntryId);
        QVERIFY(!shell.isEmpty());
        QCOMPARE(f.document(shell)->dockMode, DockMode::Floating);
        QVERIFY(f.manager->arrangeController()->returnEntry(f.rxEntryId).ok);
        settle();
    }

    // Disabled, never hidden, when the menu's Pop Out is disabled: no
    // arrange controller (the menu is then empty). The look has no Lock
    // setting, so a container committed locked disables nothing.
    void entryPopOutButtonIsDisabledWhenPopOutIs()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.edit([&](WorkspaceDocument& d) {
            for (auto& c : d.containers) {
                if (c.id == f.mainId) {
                    c.locked = true;
                }
            }
        }));
        QVERIFY(f.enterArrange());
        WorkspaceEntryHeader* header = f.entryHeader(f.rxEntryId);
        QVERIFY(header);
        f.reveal(header);
        QVERIFY(!f.document(f.mainId)->locked);
        QVERIFY(header->popOutButton()->isVisible());
        QVERIFY(header->popOutButton()->isEnabled());
        QVERIFY(header->hideButton()->isVisible());
        QVERIFY(header->hideButton()->isEnabled());
        MenuProbe probe;
        probe.arm();
        NativeMouse mouse(f.window.get());
        mouse.click(centreOf(header->optionsButton()));
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        QVERIFY(probe.actions.contains(QStringLiteral("Pop Out")));
        QVERIFY(probe.enabled.value(QStringLiteral("Pop Out"), false));
        QCOMPARE(f.entryIndex(f.mainId, f.rxEntryId), 3);
        QVERIFY(f.entry(f.mainId, f.rxEntryId).visible);
        ContainerContentHost* host = f.host();
        host->setArrangeController(nullptr);
        settle();
        QVERIFY(header->popOutButton()->isVisible());
        QVERIFY(!header->popOutButton()->isEnabled());
        QVERIFY(header->hideButton()->isVisible());
        QVERIFY(!header->hideButton()->isEnabled());
        host->setArrangeController(f.manager->arrangeController());
        settle();
        QVERIFY(header->popOutButton()->isEnabled());
        QVERIFY(header->hideButton()->isEnabled());
    }

    // A free canvas leaf's header carries the same ↗.
    void freeCanvasEntryPopOutButtonPopsTheEntryOut()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QString leafId;
        QVERIFY(f.edit([&](WorkspaceDocument& d) {
            ContainerDocument canvas;
            canvas.id = QStringLiteral("canvas");
            canvas.name = QStringLiteral("Canvas");
            canvas.layout = ContentLayout::FreeCanvas;
            canvas.dockMode = DockMode::PanelDocked;
            canvas.edge = DockEdge::Left;
            for (int i = 0; i < 2; ++i) {
                ContentEntry entry = f.registry->makeEntry(QStringLiteral("meter.customBar"));
                entry.context["sliceId"] = 0;
                entry.setFreeCanvasRect(QRectF(0, i * 120, 300, 100));
                canvas.contents.append(entry);
            }
            leafId = canvas.contents[0].id;
            d.containers.append(canvas);
            d.arrangement.strips[DockEdge::Left].thickness = 360;
        }));
        QVERIFY(f.enterArrange());
        ContainerContentHost* host = f.manager->contentHost(QStringLiteral("canvas"));
        QVERIFY(host);
        auto* surface = host->findChild<FreeCanvasSurface*>();
        QVERIFY(surface);
        // A leaf's header shows while it is selected or hovered.
        surface->selectEntry(leafId);
        settle();
        auto* header = surface->findChild<WorkspaceEntryHeader*>(QStringLiteral("freeCanvasGrip_") + leafId);
        QVERIFY(header && header->isVisible());
        QPushButton* popOut = header->popOutButton();
        QCOMPARE(popOut->objectName(), QStringLiteral("workspaceEntryPopOut"));
        QVERIFY(popOut->isEnabled());
        NativeMouse mouse(f.window.get());
        const QPoint at = centreOf(popOut);
        QWidget* hit = mouse.hitFor(header, at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        mouse.click(at);
        settle();
        QCOMPARE(f.entryIndex(QStringLiteral("canvas"), leafId), -1);
        const QString shell = f.shellHolding(leafId);
        QVERIFY(!shell.isEmpty());
        QCOMPARE(f.document(shell)->dockMode, DockMode::Floating);
        QVERIFY(f.manager->arrangeController()->returnEntry(leafId).ok);
        settle();
    }

    // JJ's bottom strip: one container "RX" holding Tuner and Amplifier,
    // stacked; he wants them side by side, left to right. The path: the
    // entry's ↗ (or ⋯ > Pop Out), then the new container's ⋯ > Move to Edge >
    // Bottom. Move to Edge appends at the strip's end, so the popped entry
    // lands to the right of what stays: pop the one that belongs on the right.
    // The Tuner and Amplifier applets need a 4O3A device setting, which
    // starts discovery, so they never materialize here (the gallery has the
    // same limit); VAX stands in for Tuner and TCI for Amplifier. The strip
    // and the commands do not look at the entry's type.
    void bottomStripSideBySide_data()
    {
        QTest::addColumn<QString>("popType");
        QTest::addColumn<QString>("leftType");
        QTest::newRow("pop Amplifier: Tuner left, Amplifier right") << QStringLiteral("applet:tci") << QStringLiteral("applet:vax");
        QTest::newRow("pop Tuner: Amplifier left, Tuner right") << QStringLiteral("applet:vax") << QStringLiteral("applet:tci");
    }
    void bottomStripSideBySide()
    {
        QFETCH(QString, popType);
        QFETCH(QString, leftType);
        LiveFixture f;
        QVERIFY(f.build());
        QString popId;
        QVERIFY(f.edit([&](WorkspaceDocument& d) {
            ContainerDocument rx;
            rx.id = QStringLiteral("rx-bottom");
            rx.name = QStringLiteral("RX");
            rx.layout = ContentLayout::VerticalStack;
            rx.dockMode = DockMode::PanelDocked;
            rx.edge = DockEdge::Bottom;
            for (auto& c : d.containers) {
                if (c.id != f.mainId) {
                    continue;
                }
                for (int i = c.contents.size() - 1; i >= 0; --i) {
                    if (c.contents[i].typeId == QStringLiteral("applet:vax")
                        || c.contents[i].typeId == QStringLiteral("applet:tci")) {
                        c.contents.removeAt(i);
                    }
                }
            }
            for (const QString& type : {QStringLiteral("applet:vax"), QStringLiteral("applet:tci")}) {
                ContentEntry entry = f.registry->makeEntry(type);
                entry.context["sliceId"] = 0;
                if (type == popType) {
                    popId = entry.id;
                }
                rx.contents.append(entry);
            }
            d.containers.append(rx);
        }));
        QVERIFY(f.enterArrange());
        ContainerContentHost* host = f.manager->contentHost(QStringLiteral("rx-bottom"));
        QVERIFY(host);
        auto* header = host->findChild<WorkspaceEntryHeader*>(QStringLiteral("workspaceEntryHeader_") + popId);
        QVERIFY(header && header->isVisible());
        NativeMouse mouse(f.window.get());
        const QPoint at = centreOf(header->popOutButton());
        QWidget* hit = mouse.hitFor(header, at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        mouse.click(at);
        settle();
        QTRY_VERIFY_WITH_TIMEOUT(!f.shellHolding(popId).isEmpty(), kMenuWaitMs);
        const QString shell = f.shellHolding(popId);
        ContainerWidget* shellWidget = f.manager->container(shell);
        QVERIFY(shellWidget);
        WorkspaceGroupHeader* shellHeader = shellWidget->workspaceHeader();
        QVERIFY(shellHeader && shellHeader->isVisible());
        // The floating container is its own window: the click goes to it.
        NativeMouse shellMouse(shellWidget->window());
        MenuProbe probe;
        bool submenuOpened = false;
        probe.arm([&](QMenu* menu) {
            probe.onSubmenu(menu, [&](QMenu* sub) {
                submenuOpened = true;
                clickItem(sub, QStringLiteral("Bottom"));
            });
            clickItem(menu, QStringLiteral("Move to Edge"));
        });
        shellMouse.click(centreOf(shellHeader->optionsButton()));
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(submenuOpened, kMenuWaitMs);
        settle();
        QCOMPARE(f.document(shell)->dockMode, DockMode::PanelDocked);
        QCOMPARE(f.document(shell)->edge, DockEdge::Bottom);
        // The bottom strip holds the two containers, one entry each.
        QStringList bottom;
        for (const auto& d : f.store->snapshot().containers) {
            if (d.dockMode == DockMode::PanelDocked && d.edge == DockEdge::Bottom) {
                bottom.append(d.id);
            }
        }
        QCOMPARE(bottom, (QStringList{QStringLiteral("rx-bottom"), shell}));
        QCOMPARE(f.document(QStringLiteral("rx-bottom"))->contents.size(), 1);
        QCOMPARE(f.document(QStringLiteral("rx-bottom"))->contents[0].typeId, leftType);
        // Side by side, left to right, as the strip lays them out.
        ContainerWidget* left = f.manager->container(QStringLiteral("rx-bottom"));
        QVERIFY(left && left->isVisible() && shellWidget->isVisible());
        QWidget* stripWidget = f.manager->edgeDockArea()->strip(DockEdge::Bottom);
        const QRect leftRect(left->mapTo(stripWidget, QPoint()), left->size());
        const QRect rightRect(shellWidget->mapTo(stripWidget, QPoint()), shellWidget->size());
        QVERIFY2(leftRect.right() < rightRect.left(),
                 qPrintable(QStringLiteral("left %1,%2 %3x%4 right %5,%6 %7x%8")
                                .arg(leftRect.x()).arg(leftRect.y()).arg(leftRect.width()).arg(leftRect.height())
                                .arg(rightRect.x()).arg(rightRect.y()).arg(rightRect.width()).arg(rightRect.height())));
        QCOMPARE(leftRect.top(), rightRect.top());
    }

    // JJ, on his Mac: after adding a workspace profile, hovering the badge
    // fans the rail out, and moving to another profile's button and clicking
    // it never switches; the rail collapses back to one button first. The
    // rail is rebuilt while the popover is hidden, and a hidden rail's layout
    // never runs, so the popover kept the rail's old size: on the next hover
    // it opened at the pre-add width with the new badges crammed into it and
    // overlapping, then grew under the pointer once the title bar laid out
    // again. The click landed on the active badge (which closes the rail and
    // switches nothing) or on a badge's remove cross.
    void profileRailSwitchesAfterAddingAProfile_data()
    {
        QTest::addColumn<QStringList>("newItems");
        QTest::addColumn<int>("targetIndex");
        // Two profiles: the one JJ started with and the one he added; he
        // goes back to the first.
        QTest::newRow("added empty") << QStringList{QStringLiteral("Empty")} << 0;
        QTest::newRow("added from current") << QStringList{QStringLiteral("From Current Arrangement")} << 0;
        // Three profiles: the second one.
        QTest::newRow("three profiles")
            << QStringList{QStringLiteral("Empty"), QStringLiteral("From Current Arrangement")} << 1;
    }

    void profileRailSwitchesAfterAddingAProfile()
    {
        QFETCH(QStringList, newItems);
        QFETCH(int, targetIndex);
        LiveFixture f;
        QVERIFY(f.build());
        ProfileRailDock* dock = f.window->profileRailDock();
        auto* profiles = f.window->findChild<WorkspaceProfiles*>();
        QVERIFY(dock && profiles);
        QVERIFY(f.resetProfiles(profiles));

        CocoaPointer pointer(f.window.get());
        NativeMouse mouse(f.window.get());
        ModalWatch modals;
        QPushButton* badge = dock->collapsedBadge();
        const QPoint rest = centreOf(badge) + QPoint(80, 0);
        pointer.move(rest);

        // Add the profiles the way JJ did: hover, the dashed +, an item, the
        // name prompt.
        for (const QString& item : newItems) {
            const int before = profiles->ids().size();
            QVERIFY(f.hoverOpen(pointer, dock, rest));
            const QPoint plus = centreOf(dock->rail()->plusButton());
            pointer.glide(centreOf(badge), plus, 4, 80);
            modals.acceptNamePrompt = true;
            MenuProbe probe;
            probe.arm([item](QMenu* menu) { clickItem(menu, item); });
            mouse.click(plus);
            QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
            QTRY_COMPARE_WITH_TIMEOUT(profiles->ids().size(), before + 1, kMenuWaitMs);
            modals.acceptNamePrompt = false;
            settle();
            // Picking the new profile made it active and closed the rail.
            QVERIFY(!dock->isOpen());
            pointer.glide(plus, rest, 3, 60);
            QTest::qWait(ProfileRailDock::kHideDelayMs + 100);
            QVERIFY(!dock->isOpen());
        }
        const QStringList ids = profiles->ids();
        QCOMPARE(ids.size(), newItems.size() + 1);
        const QString target = ids.at(targetIndex);
        QVERIFY(profiles->activeId() != target);

        // Hover the badge until the rail fans out.
        QVERIFY(f.hoverOpen(pointer, dock, rest));
        settle();
        const QString drawn = railLayoutProblem(dock);
        QVERIFY2(drawn.isEmpty(), qPrintable(drawn));

        // Move to the target's button as it is drawn now, in 12 steps over
        // about 240 ms, along the straight line, and click it.
        QPushButton* targetButton = dock->rail()->badgeButton(target);
        QVERIFY(targetButton);
        const QRect seen = globalRect(targetButton);
        const QPoint from = centreOf(badge);
        const QPoint to = seen.center();
        pointer.glide(from, to, 12, 240);
        QVERIFY2(dock->isOpen(), "the rail closed before the pointer reached the button");
        QVERIFY2(globalRect(targetButton) == seen,
                 qPrintable(QStringLiteral("the button moved under the pointer from %1 to %2")
                                .arg(describeRect(seen), describeRect(globalRect(targetButton)))));
        // The popover's own native view where it has one (macOS); where the
        // window is one native view, a widget inside the popover.
        QWidget* hit = mouse.hitFor(dock->popover(), to);
        QVERIFY2(hit == dock->popover() || (dock->popover()->internalWinId() == 0 && dock->popover()->isAncestorOf(hit)),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        mouse.click(to);
        settle();
        QVERIFY2(modals.seen.isEmpty(), qPrintable(QStringLiteral("the click opened ") + modals.seen.join(", ")));
        QCOMPARE(profiles->activeId(), target);
        QVERIFY(!dock->isOpen());
        QCOMPARE(badge->text(), ProfileRail::initialFor(profiles->name(target)));
    }

    // JJ's decision: every entry header ends with a hide button (Longpath's
    // close cross), and Containers > Hidden brings the entry back where it
    // was. An applet hidden this way is unticked under Containers > Applets.
    void entryHideButtonHidesAndHiddenBringsItBack()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceEntryHeader* header = f.entryHeader(f.rxEntryId);
        QVERIFY(header);
        f.reveal(header);
        QPushButton* hide = header->hideButton();
        QVERIFY(hide && hide->isVisible() && hide->isEnabled());
        QCOMPARE(hide->objectName(), QStringLiteral("workspaceEntryHide"));
        QCOMPARE(hide->text(), QStringLiteral("\u2715"));
        QCOMPARE(hide->toolTip(), QStringLiteral("Hide. Bring it back from the Containers menu."));
        QCOMPARE(hide->size(), header->optionsButton()->size());
        // Last and rightmost: right of ⋯, nothing right of it.
        QVERIFY(header->optionsButton()->geometry().right() < hide->geometry().left());
        for (QWidget* child : header->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            if (child != hide && child->isVisible()) {
                QVERIFY(child->geometry().right() < hide->geometry().left());
            }
        }
        QAction* rxApplet = f.appletAction(QStringLiteral("RX"));
        QVERIFY(rxApplet && rxApplet->isChecked());
        // Before: only the fixture's hidden floating container.
        QCOMPARE(f.openHidden(), QStringList{QStringLiteral("Meter Bars")});

        QWidget* view = nullptr;
        for (const auto& row : f.host()->entryRows()) {
            if (row.entryId == f.rxEntryId) {
                view = row.widget;
            }
        }
        QVERIFY(view && view->isVisible());
        NativeMouse mouse(f.window.get());
        const QPoint at = centreOf(hide);
        QWidget* hit = mouse.hitFor(header, at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        mouse.click(at);
        settle();
        QVERIFY(!f.entry(f.mainId, f.rxEntryId).visible);
        QCOMPARE(f.entryIndex(f.mainId, f.rxEntryId), 3);   // hidden, never moved or deleted
        QVERIFY(!view->isVisible());
        QVERIFY(!rxApplet->isChecked());
        QVERIFY(f.mainContainer()->isVisible());

        const QString label = QStringLiteral("RX (Main Panel)");
        QCOMPARE(f.openHidden(label), (QStringList{label, QStringLiteral("Meter Bars")}));
        QVERIFY(f.entry(f.mainId, f.rxEntryId).visible);
        QCOMPARE(f.entryIndex(f.mainId, f.rxEntryId), 3);
        QVERIFY(rxApplet->isChecked());
        QCOMPARE(f.openHidden(), QStringList{QStringLiteral("Meter Bars")});

        // Both ways: unticking the applet lists it under Hidden.
        rxApplet->trigger();
        settle();
        QVERIFY(!f.entry(f.mainId, f.rxEntryId).visible);
        QCOMPARE(f.openHidden(label), (QStringList{label, QStringLiteral("Meter Bars")}));
        QVERIFY(rxApplet->isChecked());
    }

    // A hidden container is listed by name and comes back where it was: the
    // fixture's floating Meter Bars, and a copy saved hidden as Duplicate
    // made JJ's "Container2 copy" (Floating, visible false). Then the empty
    // state.
    void hiddenContainersComeBackWhereTheyWere()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.edit([&](WorkspaceDocument& d) {
            ContainerDocument copy;
            copy.id = QStringLiteral("container2-copy");
            copy.name = QStringLiteral("Container2 copy");
            copy.layout = ContentLayout::VerticalStack;
            copy.dockMode = DockMode::Floating;
            copy.visible = false;
            copy.geometry = QRect(530, 295, 450, 404);
            ContentEntry entry = f.registry->makeEntry(QStringLiteral("meter.customBar"));
            entry.context["sliceId"] = 0;
            copy.contents.append(entry);
            d.containers.append(copy);
        }));
        QCOMPARE(f.openHidden(), (QStringList{QStringLiteral("Meter Bars"), QStringLiteral("Container2 copy")}));
        QCOMPARE(f.openHidden(QStringLiteral("Container2 copy")), (QStringList{QStringLiteral("Meter Bars"), QStringLiteral("Container2 copy")}));
        const ContainerDocument* copy = f.document(QStringLiteral("container2-copy"));
        QVERIFY(copy->visible);
        QCOMPARE(copy->dockMode, DockMode::Floating);
        QCOMPARE(copy->geometry, QRect(530, 295, 450, 404));
        ContainerWidget* widget = f.manager->container(QStringLiteral("container2-copy"));
        QVERIFY(widget && widget->window()->isVisible());

        QCOMPARE(f.openHidden(QStringLiteral("Meter Bars")), QStringList{QStringLiteral("Meter Bars")});
        QVERIFY(f.document(QStringLiteral("meter-bars"))->visible);
        QVERIFY(f.manager->container(QStringLiteral("meter-bars"))->window()->isVisible());
        QCOMPARE(f.openHidden(), QStringList{QStringLiteral("!Nothing hidden")});
    }

    // The group header's hide button hides the container (the menu's "Hide
    // container"). The look has no Lock setting: a container committed
    // locked keeps it, and the pop-out button, enabled.
    void groupHideButtonHidesTheContainer()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceGroupHeader* header = f.groupHeader();
        QVERIFY(header && header->isVisible());
        QPushButton* hide = header->hideButton();
        QVERIFY(hide && hide->isVisible() && hide->isEnabled());
        QCOMPARE(hide->objectName(), QStringLiteral("workspaceHide"));
        QCOMPARE(hide->text(), QStringLiteral("\u2715"));
        QCOMPARE(hide->toolTip(), QStringLiteral("Hide. Bring it back from the Containers menu."));
        QCOMPARE(hide->size(), header->optionsButton()->size());
        QVERIFY(header->optionsButton()->geometry().right() < hide->geometry().left());
        for (QWidget* child : header->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            if (child != hide && child->isVisible()) {
                QVERIFY(child->geometry().right() < hide->geometry().left());
            }
        }

        QVERIFY(f.edit([&](WorkspaceDocument& d) {
            for (auto& c : d.containers) {
                if (c.id == f.mainId) {
                    c.locked = true;
                }
            }
        }));
        NativeMouse mouse(f.window.get());
        QVERIFY(!f.document(f.mainId)->locked);
        QVERIFY(!f.mainContainer()->isLocked());
        QVERIFY(header->popOutButton()->isEnabled());
        QVERIFY(hide->isVisible() && hide->isEnabled());
        QVERIFY(f.document(f.mainId)->visible);

        const QPoint at = centreOf(hide);
        QWidget* hit = mouse.hitFor(header, at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        mouse.click(at);
        settle();
        QVERIFY(!f.document(f.mainId)->visible);
        QVERIFY(!f.mainContainer()->isVisible());
        QCOMPARE(f.openHidden(QStringLiteral("Main Panel")),
                 (QStringList{QStringLiteral("Main Panel"), QStringLiteral("Meter Bars")}));
        QVERIFY(f.document(f.mainId)->visible);
        QVERIFY(f.mainContainer()->isVisible());
        QCOMPARE(f.document(f.mainId)->edge, DockEdge::Right);
    }

    // A free canvas leaf's header has the same hide button. Hiding the last
    // visible widget takes the container off the screen with it; showing any
    // of them brings it back.
    void hidingTheLastWidgetHidesItsContainer()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QStringList leaves;
        QVERIFY(f.edit([&](WorkspaceDocument& d) {
            ContainerDocument canvas;
            canvas.id = QStringLiteral("canvas");
            canvas.name = QStringLiteral("Canvas");
            canvas.layout = ContentLayout::FreeCanvas;
            canvas.dockMode = DockMode::PanelDocked;
            canvas.edge = DockEdge::Left;
            for (int i = 0; i < 2; ++i) {
                ContentEntry entry = f.registry->makeEntry(QStringLiteral("meter.customBar"));
                entry.context["sliceId"] = 0;
                entry.name = QStringLiteral("Bar %1").arg(i + 1);
                entry.setFreeCanvasRect(QRectF(0, i * 120, 300, 100));
                canvas.contents.append(entry);
                leaves.append(entry.id);
            }
            d.containers.append(canvas);
            d.arrangement.strips[DockEdge::Left].thickness = 360;
        }));
        QVERIFY(f.enterArrange());
        ContainerWidget* canvasWidget = f.manager->container(QStringLiteral("canvas"));
        QVERIFY(canvasWidget && canvasWidget->isVisible());
        NativeMouse mouse(f.window.get());
        for (const QString& leaf : leaves) {
            auto* surface = f.manager->contentHost(QStringLiteral("canvas"))->findChild<FreeCanvasSurface*>();
            QVERIFY(surface);
            surface->selectEntry(leaf);
            settle();
            auto* header = surface->findChild<WorkspaceEntryHeader*>(QStringLiteral("freeCanvasGrip_") + leaf);
            QVERIFY(header && header->isVisible());
            QPushButton* hide = header->hideButton();
            QCOMPARE(hide->objectName(), QStringLiteral("workspaceEntryHide"));
            QVERIFY(hide->isEnabled());
            const QPoint at = centreOf(hide);
            QWidget* hit = mouse.hitFor(header, at);
            QVERIFY2(hit == header || header->isAncestorOf(hit),
                     qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
            mouse.click(at);
            settle();
            QVERIFY(!f.entry(QStringLiteral("canvas"), leaf).visible);
        }
        // Both hidden: the container leaves the screen, its own saved
        // visibility untouched.
        QVERIFY(f.document(QStringLiteral("canvas"))->visible);
        QVERIFY(!canvasWidget->isVisible());
        QCOMPARE(f.openHidden(QStringLiteral("Bar 2 (Canvas)")),
                 (QStringList{QStringLiteral("Meter Bars"), QStringLiteral("Bar 1 (Canvas)"),
                              QStringLiteral("Bar 2 (Canvas)")}));
        QVERIFY(f.entry(QStringLiteral("canvas"), leaves[1]).visible);
        QVERIFY(canvasWidget->isVisible());
    }

    // Containers > Hidden is in both looks, last in the containers group,
    // just above the separator before the Applets section.
    void hiddenMenuSitsInTheSamePlaceInBothLooks_data()
    {
        QTest::addColumn<bool>("preview");
        QTest::addColumn<QString>("place");
        QTest::newRow("preview off") << false << QStringLiteral("&Reset Default Layout | (separator)");
        QTest::newRow("preview on") << true << QStringLiteral("Manage Workspace &Profiles… | (separator)");
    }
    void hiddenMenuSitsInTheSamePlaceInBothLooks()
    {
        QFETCH(bool, preview);
        QFETCH(QString, place);
        PreviewSwitch look(preview);
        LiveFixture f;
        QVERIFY(f.build());
        QCOMPARE(f.hiddenMenuPlace(), place);
        QCOMPARE(f.hiddenMenu()->title(), QStringLiteral("&Hidden"));
    }

    // Preview off: "Hide container (retain placement)" hides a container,
    // Containers > Hidden lists it, and picking it shows it again where it
    // was. Then the empty state.
    void previewOffHiddenBringsBackAHiddenContainer()
    {
        PreviewSwitch look(false);
        LiveFixture f;
        QVERIFY(f.build());
        const QRect placed(40, 60, 320, 220);
        QVERIFY(f.edit([&](WorkspaceDocument& d) {
            ContainerDocument overlay;
            overlay.id = QStringLiteral("overlay");
            overlay.name = QStringLiteral("Overlay Meters");
            overlay.layout = ContentLayout::VerticalStack;
            overlay.dockMode = DockMode::OverlayDocked;
            overlay.geometry = placed;
            ContentEntry entry = f.registry->makeEntry(QStringLiteral("meter.customBar"));
            entry.context["sliceId"] = 0;
            overlay.contents.append(entry);
            d.containers.append(overlay);
        }));
        ContainerWidget* overlay = f.manager->container(QStringLiteral("overlay"));
        QVERIFY(overlay && overlay->isVisible());
        const QRect before = overlay->geometry();
        QCOMPARE(f.openHidden(), QStringList{QStringLiteral("Meter Bars")});

        QVERIFY(f.pickFromContainerMenu(overlay, QStringLiteral("Hide container (retain placement)")));
        QVERIFY(!f.document(QStringLiteral("overlay"))->visible);
        QVERIFY(!overlay->isVisible());

        const QString label = QStringLiteral("Overlay Meters");
        QCOMPARE(f.openHidden(label), (QStringList{QStringLiteral("Meter Bars"), label}));
        const ContainerDocument* shown = f.document(QStringLiteral("overlay"));
        QVERIFY(shown->visible);
        QCOMPARE(shown->dockMode, DockMode::OverlayDocked);
        QCOMPARE(shown->geometry, placed);
        overlay = f.manager->container(QStringLiteral("overlay"));
        QVERIFY(overlay && overlay->isVisible());
        QCOMPARE(overlay->geometry(), before);

        // The main panel, the same way: back in its splitter at its width.
        ContainerWidget* main = f.mainContainer();
        QVERIFY(main && main->isVisible());
        const int width = main->width();
        QVERIFY(f.pickFromContainerMenu(main, QStringLiteral("Hide container (retain placement)")));
        QVERIFY(!f.document(f.mainId)->visible);
        QVERIFY(!main->isVisible());
        QCOMPARE(f.openHidden(QStringLiteral("Main Panel")),
                 (QStringList{QStringLiteral("Main Panel"), QStringLiteral("Meter Bars")}));
        QVERIFY(f.document(f.mainId)->visible);
        QCOMPARE(f.document(f.mainId)->dockMode, DockMode::PanelDocked);
        QVERIFY(f.mainContainer()->isVisible());
        QCOMPARE(f.mainContainer()->width(), width);

        QCOMPARE(f.openHidden(QStringLiteral("Meter Bars")), QStringList{QStringLiteral("Meter Bars")});
        QCOMPARE(f.openHidden(), QStringList{QStringLiteral("!Nothing hidden")});
    }

    // Preview off: a floating container closed by its window comes back
    // floating, at its geometry.
    void previewOffHiddenBringsBackAClosedFloatingContainer()
    {
        PreviewSwitch look(false);
        LiveFixture f;
        QVERIFY(f.build());
        const QString id = QStringLiteral("meter-bars");
        QCOMPARE(f.openHidden(QStringLiteral("Meter Bars")), QStringList{QStringLiteral("Meter Bars")});
        ContainerWidget* bars = f.manager->container(id);
        QVERIFY(bars && bars->isFloating());
        QWidget* form = bars->window();
        QVERIFY(form != f.window.get() && form->isVisible());
        const QRect frame = form->geometry();
        const QRect saved = f.document(id)->geometry;

        // The window's close button: the form asks the workspace to hide it.
        form->close();
        settle();
        QVERIFY(!f.document(id)->visible);
        QVERIFY(!f.manager->container(id)->window()->isVisible());

        QCOMPARE(f.openHidden(QStringLiteral("Meter Bars")), QStringList{QStringLiteral("Meter Bars")});
        const ContainerDocument* shown = f.document(id);
        QVERIFY(shown->visible);
        QCOMPARE(shown->dockMode, DockMode::Floating);
        QCOMPARE(shown->geometry, saved);
        bars = f.manager->container(id);
        QVERIFY(bars && bars->isFloating());
        QVERIFY(bars->window() != f.window.get() && bars->window()->isVisible());
        QCOMPARE(bars->window()->geometry(), frame);
        QCOMPARE(f.openHidden(), QStringList{QStringLiteral("!Nothing hidden")});
    }

    // Preview off: an applet hidden from Containers > Applets is listed too,
    // as in the preview, and comes back in its place.
    void previewOffHiddenListsAHiddenApplet()
    {
        PreviewSwitch look(false);
        LiveFixture f;
        QVERIFY(f.build());
        QAction* rxApplet = f.appletAction(QStringLiteral("RX"));
        QVERIFY(rxApplet && rxApplet->isChecked());
        rxApplet->trigger();
        settle();
        QVERIFY(!f.entry(f.mainId, f.rxEntryId).visible);
        const QString label = QStringLiteral("RX (Main Panel)");
        QCOMPARE(f.openHidden(label), (QStringList{label, QStringLiteral("Meter Bars")}));
        QVERIFY(f.entry(f.mainId, f.rxEntryId).visible);
        QCOMPARE(f.entryIndex(f.mainId, f.rxEntryId), 3);
        QVERIFY(rxApplet->isChecked());
    }

    // A duplicate always appears: visible, floating, beside the original
    // (its floating geometry, or where it sits on screen when docked) and
    // offset so it does not cover it, and on screen. The original is
    // unchanged. Both looks; the manager path and Container Settings.
    void duplicateIsShownBesideTheOriginal_data() { addDuplicateRows(false); }
    void duplicateIsShownBesideTheOriginal() { checkDuplicateIsShownBesideTheOriginal(); }
    // The same in the workspace look: its own function, so the two looks
    // run as separate ctest entries (main()).
    void duplicateIsShownBesideTheOriginalInTheWorkspaceLook_data() { addDuplicateRows(true); }
    void duplicateIsShownBesideTheOriginalInTheWorkspaceLook() { checkDuplicateIsShownBesideTheOriginal(); }

    // JJ on his Mac after 391a09306: the rail "closes when i try to move
    // mouse". On macOS each badge in the popover is a native view and cocoa
    // routes Enter/Leave per view, so the move from the popover onto a badge
    // can bring the popover a Leave and no Enter, which clears WA_UnderMouse.
    // Here no Enter or Leave reaches the badge or the popover while the
    // cursor travels to the other profile, then the popover gets that lone
    // Leave. The rail must stay open past the hide delay and the click must
    // switch.
    void profileRailStaysOpenWhenTheEnterNeverArrives()
    {
        LiveFixture f;
        QVERIFY(f.build());
        ProfileRailDock* dock = f.window->profileRailDock();
        auto* profiles = f.window->findChild<WorkspaceProfiles*>();
        QVERIFY(dock && profiles);
        QVERIFY(f.resetProfiles(profiles));
        QVERIFY(profiles->createEmpty(QStringLiteral("DX")).ok());
        settle();
        const QString target = profiles->ids().first();
        QVERIFY(profiles->activeId() != target);

        CocoaPointer pointer(f.window.get());
        NativeMouse mouse(f.window.get());
        ModalWatch modals;
        QPushButton* badge = dock->collapsedBadge();
        const QPoint rest = centreOf(badge) + QPoint(80, 0);
        pointer.move(rest);
        QVERIFY(f.hoverOpen(pointer, dock, rest));
        settle();

        QPushButton* targetButton = dock->rail()->badgeButton(target);
        QVERIFY(targetButton);
        const QPoint to = centreOf(targetButton);
        {
            DroppedEnterLeave dropped({badge, dock->popover()});
            glideCursor(centreOf(badge), to, 12, 240);
            settle();
            dropped.clearUnderMouse();
            dropped.deliverLeave(dock->popover());
            QTest::qWait(ProfileRailDock::kHideDelayMs + 300);
            QVERIFY2(dock->isOpen(), "the rail closed with the cursor on the other profile");
        }
        mouse.click(to);
        settle();
        QVERIFY2(modals.seen.isEmpty(), qPrintable(QStringLiteral("the click opened ") + modals.seen.join(", ")));
        QCOMPARE(profiles->activeId(), target);
        QVERIFY(!dock->isOpen());
    }

    // The other half: with no Leave at all, the cursor leaving both the badge
    // and the popover still closes the rail after the hide delay, and not
    // before. A rail opened from the keyboard with the cursor elsewhere
    // stays open.
    void profileRailClosesWhenTheCursorLeavesWithoutALeave()
    {
        LiveFixture f;
        QVERIFY(f.build());
        ProfileRailDock* dock = f.window->profileRailDock();
        auto* profiles = f.window->findChild<WorkspaceProfiles*>();
        QVERIFY(dock && profiles);
        QVERIFY(f.resetProfiles(profiles));
        QVERIFY(profiles->createEmpty(QStringLiteral("DX")).ok());
        settle();

        CocoaPointer pointer(f.window.get());
        QPushButton* badge = dock->collapsedBadge();
        const QPoint rest = centreOf(badge) + QPoint(80, 0);
        pointer.move(rest);
        QVERIFY(f.hoverOpen(pointer, dock, rest));
        settle();
        const QRect popover = globalRect(dock->popover());
        const QPoint outside(popover.left() - 40, popover.bottom() + 60);
        {
            DroppedEnterLeave dropped({badge, dock->popover()});
            // Off the rail in one move, so the delay is timed from it.
            QElapsedTimer left;
            QuietCursor::setPos(outside);
            left.start();
            QVERIFY(dock->isOpen());
            QTRY_VERIFY_WITH_TIMEOUT(!dock->isOpen(), ProfileRailDock::kHideDelayMs + 400);
            // The hide delay, less the sampling interval and this check's
            // own granularity.
            QVERIFY2(left.elapsed() >= ProfileRailDock::kHideDelayMs - ProfileRailDock::kPointerSampleMs,
                     qPrintable(QStringLiteral("closed %1 ms after the cursor left").arg(left.elapsed())));
        }

        f.window->activateWindow();
        badge->setFocus(Qt::TabFocusReason);
        settle();
        QVERIFY(dock->isOpen());
        QTest::qWait(ProfileRailDock::kHideDelayMs + 300);
        QVERIFY2(dock->isOpen(), "a rail opened from the keyboard closed with the cursor elsewhere");
    }

    // The remove cross on a profile badge is Longpath's 14x14 disc
    // (setFixedSize(14, 14), border-radius 7px). In the top bar the badge's
    // pill style (min-height = max-height = the pill height) cascaded to the
    // cross and drew it 14x27, the full badge height, where a click aimed at
    // a profile could land on it.
    void profileRailRemoveCrossIsFourteenSquare()
    {
        LiveFixture f;
        QVERIFY(f.build());
        ProfileRailDock* dock = f.window->profileRailDock();
        auto* profiles = f.window->findChild<WorkspaceProfiles*>();
        QVERIFY(dock && profiles);
        QVERIFY(f.resetProfiles(profiles));
        QVERIFY(profiles->createEmpty(QStringLiteral("DX")).ok());
        settle();
        dock->open(false);
        settle();
        for (const QString& id : dock->rail()->badges()) {
            QPushButton* badge = dock->rail()->badgeButton(id);
            QVERIFY(badge);
            QPushButton* cross = nullptr;
            for (QPushButton* child : badge->findChildren<QPushButton*>(Qt::FindDirectChildrenOnly)) {
                if (child->toolTip().startsWith(QStringLiteral("Remove profile"))) {
                    cross = child;
                }
            }
            QVERIFY(cross);
            QEvent enter(QEvent::Enter);
            QCoreApplication::sendEvent(badge, &enter);
            settle();
            QVERIFY(cross->isVisible());
            QCOMPARE(cross->size(), QSize(14, 14));
            QVERIFY(badge->rect().contains(cross->geometry()));
            // Beside the letter, never on the badge's centre.
            QVERIFY(!cross->geometry().contains(badge->rect().center()));
            QEvent leave(QEvent::Leave);
            QCoreApplication::sendEvent(badge, &leave);
            settle();
            QVERIFY(!cross->isVisible());
        }
        dock->close();
    }

    // What the fix must keep: the rail closes when the pointer really leaves
    // both the badge and the rail, Escape closes it, and Tab opens it.
    void profileRailClosesOnLeaveAndEscapeAndOpensOnTab()
    {
        LiveFixture f;
        QVERIFY(f.build());
        ProfileRailDock* dock = f.window->profileRailDock();
        auto* profiles = f.window->findChild<WorkspaceProfiles*>();
        QVERIFY(dock && profiles);
        QVERIFY(f.resetProfiles(profiles));
        QVERIFY(profiles->createEmpty(QStringLiteral("DX")).ok());
        settle();

        CocoaPointer pointer(f.window.get());
        QPushButton* badge = dock->collapsedBadge();
        const QPoint rest = centreOf(badge) + QPoint(80, 0);
        pointer.move(rest);
        QVERIFY(f.hoverOpen(pointer, dock, rest));
        // Out through the popover's left edge and down into the workspace.
        const QRect popover = globalRect(dock->popover());
        const QPoint outside(popover.left() - 40, popover.bottom() + 60);
        pointer.glide(centreOf(badge), outside, 6, 120);
        QVERIFY(dock->isOpen());
        QTRY_VERIFY_WITH_TIMEOUT(!dock->isOpen(), ProfileRailDock::kHideDelayMs + 400);

        QVERIFY(f.hoverOpen(pointer, dock, outside));
        QTest::keyClick(f.window.get(), Qt::Key_Escape);
        settle();
        QVERIFY(!dock->isOpen());

        pointer.glide(centreOf(badge), outside, 4, 40);
        QTest::qWait(ProfileRailDock::kHideDelayMs + 100);
        f.window->activateWindow();
        badge->setFocus(Qt::TabFocusReason);
        settle();
        QVERIFY(dock->isOpen());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstWorkspaceLiveInput test;
    QTEST_SET_MAIN_SOURCE_PATH
    // Each case builds a MainWindow, and most of a case is that build: about
    // 5 to 10 s at load 14 to 20 on a Mac, most of it the workspace
    // reconciled again for every content registration (ContainerManager's
    // runtimeChanged handler). The fixed waits come to well under 2 s a
    // case. The whole binary took 328 s, and each of the four earlier
    // entries 63 to 116 s, so under CI load every one ran past ctest's 120 s.
    // The limit is not raised: the ten groups below run as
    // tst_workspace_live_input_<group> (tests/CMakeLists.txt) and the header
    // drags (and any case no group names) as tst_workspace_live_input,
    // eleven entries of at most about 45 s at that load.
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_WORKSPACE_LIVE_INPUT_GROUP",
        {
         {QStringLiteral("hidden"),
          {
           QStringLiteral("entryHideButtonHidesAndHiddenBringsItBack"),
           QStringLiteral("hiddenContainersComeBackWhereTheyWere"),
           QStringLiteral("groupHideButtonHidesTheContainer"),
           QStringLiteral("hidingTheLastWidgetHidesItsContainer")}},
         {QStringLiteral("hiddenmenu"),
          {
           QStringLiteral("hiddenMenuSitsInTheSamePlaceInBothLooks")}},
         {QStringLiteral("previewoff"),
          {
           QStringLiteral("previewOffHiddenBringsBackAHiddenContainer"),
           QStringLiteral("previewOffHiddenBringsBackAClosedFloatingContainer"),
           QStringLiteral("previewOffHiddenListsAHiddenApplet")}},
         {QStringLiteral("duplicate"),
          {
           QStringLiteral("duplicateIsShownBesideTheOriginal")}},
         {QStringLiteral("duplicatelook"),
          {
           QStringLiteral("duplicateIsShownBesideTheOriginalInTheWorkspaceLook")}},
         {QStringLiteral("menus"),
          {
           QStringLiteral("groupHeaderOptionsOpensTheMenuInArrange"),
           QStringLiteral("entryHeaderOptionsOpensTheMenuInArrange"),
           QStringLiteral("moveToEdgeFromTheMenu"),
           QStringLiteral("popOutAnEntryFromItsMenu"),
           QStringLiteral("arrangeDoesNotLookLocked")}},
         {QStringLiteral("popout"),
          {
           QStringLiteral("entryPopOutButtonPopsTheEntryOut"),
           QStringLiteral("entryPopOutButtonIsDisabledWhenPopOutIs"),
           QStringLiteral("freeCanvasEntryPopOutButtonPopsTheEntryOut")}},
         {QStringLiteral("strip"),
          {
           QStringLiteral("bottomStripSideBySide"),
           QStringLiteral("nativeOverlaysLetClicksThrough")}},
         {QStringLiteral("profiles"),
          {
           QStringLiteral("profileRailStaysOpenWhenTheEnterNeverArrives"),
           QStringLiteral("profileRailClosesWhenTheCursorLeavesWithoutALeave"),
           QStringLiteral("profileRailRemoveCrossIsFourteenSquare"),
           QStringLiteral("profileRailClosesOnLeaveAndEscapeAndOpensOnTab")}},
         {QStringLiteral("profileswitch"),
          {
           QStringLiteral("profileRailSwitchesAfterAddingAProfile")}}});
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}
#include "tst_workspace_live_input.moc"
