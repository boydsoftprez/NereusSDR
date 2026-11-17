// SPDX-License-Identifier: GPL-3.0-or-later
//
// Intel macOS presents the spectrum without waiting for the rest of the
// window. Qt 6.12 rewrites QWidget::repaint() from UpdateNow to a posted
// UpdateRequest while a texture child has composed within one refresh
// (QWidgetRepaintManager::sendUpdateRequest). The posted sync then paints
// every dirty widget in the top-level, and that pass is longer than a
// waterfall row, so the scroll steps.
//
// Holding those widgets out of one sync did not help: restoring them posted
// the same full-window flush. The spectrum is therefore its own window, and
// this path syncs that window once. markDirty(UpdateLater) also posts an
// UpdateRequest, and sync() does not take that event back. Delivering it
// flushed the waterfall a second time, took another Metal drawable, and
// stretched one row across two frames. The hold below remains only for the
// frames before the window split.

#include "gui/PanadapterApplet.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QPointer>
#include <QWidget>
#include <QWindow>

#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_X86_64)
#ifdef slots
#undef slots
#endif
#ifdef signals
#undef signals
#endif
#import <AppKit/AppKit.h>
#endif

#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_X86_64) && defined(NEREUS_HAVE_WIDGETS_PRIVATE)
#include <QtWidgets/private/qwidget_p.h>
#include <QtWidgets/private/qwidgetrepaintmanager_p.h>
#define NEREUS_INTEL_SPECTRUM_PRESENT 1
#endif

namespace NereusSDR {

static int g_intelSpectrumPaintSerial = 0;
static qint64 g_intelSpectrumPaintMs = 0;

void noteIntelSpectrumPainted(qint64 paintMs)
{
    ++g_intelSpectrumPaintSerial;
    g_intelSpectrumPaintMs += paintMs;
}

void orderIntelDetachedWindow(QWidget* window, QWidget* host, bool restack)
{
#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_X86_64)
    // Qt::Tool is created at NSFloatingWindowLevel, above every normal
    // window of every application. Drop it to the normal level and attach
    // the existing NSWindow once. A click on the host frame orders the host
    // forward; without the child link the host covers the waterfall and the
    // placeholder panel flashes through. Ordering again on that click is
    // the flash. This is not called from the present path.
    Q_UNUSED(restack);
    if (!window || !host || window == host || !window->isWindow()) {
        return;
    }
    QWindow* windowHandle = window->windowHandle();
    QWindow* hostHandle = host->windowHandle();
    if (!windowHandle || !windowHandle->handle() || !hostHandle || !hostHandle->handle()) {
        return;
    }
    NSView* view = reinterpret_cast<NSView*>(windowHandle->winId());
    NSView* hostView = reinterpret_cast<NSView*>(hostHandle->winId());
    NSWindow* nsWindow = view.window;
    NSWindow* hostWindow = hostView.window;
    if (!nsWindow || !hostWindow || nsWindow == hostWindow) {
        return;
    }
    if (nsWindow.animationBehavior != NSWindowAnimationBehaviorNone) {
        nsWindow.animationBehavior = NSWindowAnimationBehaviorNone;
    }
    if (nsWindow.level != NSNormalWindowLevel) {
        nsWindow.level = NSNormalWindowLevel;
        static bool logged = false;
        if (!logged) {
            logged = true;
            qDebug() << "SpectrumWidget: intel window level normal";
        }
    }
    if (nsWindow.parentWindow == hostWindow) {
        return;
    }
    if (nsWindow.parentWindow) {
        [nsWindow.parentWindow removeChildWindow:nsWindow];
    }
    [hostWindow addChildWindow:nsWindow ordered:NSWindowAbove];
#else
    Q_UNUSED(window);
    Q_UNUSED(host);
    Q_UNUSED(restack);
#endif
}

namespace {

struct IntelSurfaceState {
    int visible = 0;
    int mapped = 0;
    int exposed = 0;
    int dirty = 0;
};

IntelSurfaceState intelSurfaceState(QWidget* spectrum)
{
    IntelSurfaceState state;
    if (!spectrum) {
        return state;
    }
    state.visible = spectrum->isVisible() ? 1 : 0;
    state.mapped = spectrum->testAttribute(Qt::WA_Mapped) ? 1 : 0;
    if (QWindow* window = spectrum->windowHandle()) {
        state.exposed = window->isExposed() ? 1 : 0;
    }
#ifdef NEREUS_INTEL_SPECTRUM_PRESENT
    state.dirty = QWidgetPrivate::get(spectrum)->inDirtyList ? 1 : 0;
#endif
    return state;
}

void noteIntelPresent(bool fast, qint64 syncMs, qint64 paintMs, const IntelSurfaceState& state)
{
    static int fastCount = 0;
    static int skippedCount = 0;
    static qint64 worstGapMs = 0;
    static qint64 worstSyncMs = 0;
    static qint64 syncSum = 0;
    static qint64 paintSum = 0;
    static IntelSurfaceState last;
    static QElapsedTimer window;
    static QElapsedTimer gap;
    if (!window.isValid()) {
        window.start();
    }
    last = state;
    if (fast) {
        if (gap.isValid()) {
            worstGapMs = qMax(worstGapMs, gap.elapsed());
        }
        gap.start();
        ++fastCount;
        worstSyncMs = qMax(worstSyncMs, syncMs);
        syncSum += syncMs;
        paintSum += paintMs;
    } else {
        ++skippedCount;
    }
    if (window.elapsed() < 1000) {
        return;
    }
    qDebug() << "SpectrumWidget: intel present" << fastCount
             << "skipped" << skippedCount
             << "worstGapMs" << worstGapMs
             << "worstSyncMs" << worstSyncMs
             << "syncSum" << syncSum
             << "paintSum" << paintSum
             << "visible" << last.visible
             << "mapped" << last.mapped
             << "exposed" << last.exposed
             << "dirty" << last.dirty;
    fastCount = 0;
    skippedCount = 0;
    worstGapMs = 0;
    worstSyncMs = 0;
    syncSum = 0;
    paintSum = 0;
    window.restart();
}

void recoverIntelSpectrumSurface(QWidget* spectrum)
{
    if (!spectrum || !spectrum->isWindow()) {
        return;
    }
    if (spectrum->isVisible() && spectrum->testAttribute(Qt::WA_Mapped)) {
        return;
    }
    auto* applet = qobject_cast<PanadapterApplet*>(spectrum->parentWidget());
    if (!applet) {
        return;
    }
    applet->ensureDetachedSpectrumShown();
}

} // namespace

bool presentIntelSpectrumFrame(QWidget* spectrum)
{
#ifndef NEREUS_INTEL_SPECTRUM_PRESENT
    Q_UNUSED(spectrum);
    return false;
#else
    if (spectrum && spectrum->isWindow()) {
        // A hidden or ordered-out tool window makes sync() return before
        // paint, and the posted retry used to be deleted on that miss.
        recoverIntelSpectrumSurface(spectrum);
    }
    const IntelSurfaceState state = intelSurfaceState(spectrum);
    if (!spectrum || !spectrum->isVisible() || !spectrum->updatesEnabled()
        || spectrum->rect().isEmpty()) {
        noteIntelPresent(false, 0, 0, state);
        return false;
    }
    QWidget* top = spectrum->window();
    if (!top || top->paintingActive() || spectrum->paintingActive()) {
        noteIntelPresent(false, 0, 0, state);
        return false;
    }
    QWidgetPrivate* topPrivate = QWidgetPrivate::get(top);
    QWidgetRepaintManager* manager = topPrivate->maybeRepaintManager();
    const bool ownWindow = spectrum->isWindow();
    // An embedded spectrum shares the main window's backing store, so any
    // other dirty widget copies that whole image. Its own window copies
    // only the spectrum, tags included.
    if (!manager || !QWidgetPrivate::get(spectrum)->renderToTexture
        || (!ownWindow && !manager->dirtyRegion().isEmpty())) {
        noteIntelPresent(false, 0, 0, state);
        return false;
    }

    static bool inPresent = false;
    if (inPresent) {
        noteIntelPresent(false, 0, 0, state);
        return false;
    }
    inPresent = true;

    if (ownWindow) {
        if (QWindow* window = spectrum->windowHandle()) {
            static bool loggedInterval = false;
            if (!loggedInterval) {
                loggedInterval = true;
                qDebug() << "SpectrumWidget: intel swap interval"
                         << window->requestedFormat().swapInterval();
            }
        }
        QWidgetPrivate* spectrumPrivate = QWidgetPrivate::get(spectrum);
        // A render-to-texture widget whose bit is clear is punched out of
        // the backing store and not painted. Child updates in this window
        // were taking that path, so the timer kept "presenting" a texture
        // that no longer moved.
        // A paint can mark the widget dirty again before sync returns.
        // inDirtyList is then set for a frame that did move, so the gap
        // is taken from whether paintEvent ran.
        spectrumPrivate->renderToTextureReallyDirty = 1;
        const int paintsBefore = g_intelSpectrumPaintSerial;
        const qint64 paintMsBefore = g_intelSpectrumPaintMs;
        QElapsedTimer syncTimer;
        syncTimer.start();
        manager->markDirty(spectrum->rect(), spectrum, QWidgetRepaintManager::UpdateLater);
        manager->sync();
        const bool painted = g_intelSpectrumPaintSerial != paintsBefore;
        // The posted UpdateRequest is the retry for a sync that returned
        // before paint. Deleting it, and telling the timer the present
        // succeeded, left the waterfall unmapped for the rest of the session.
        if (painted) {
            QCoreApplication::removePostedEvents(top, QEvent::UpdateRequest);
        }
        const qint64 syncMs = syncTimer.elapsed();
        const qint64 paintMs = g_intelSpectrumPaintMs - paintMsBefore;
        inPresent = false;
        noteIntelPresent(painted, syncMs, paintMs, intelSurfaceState(spectrum));
        return painted;
    }

    struct HeldUpdate {
        QPointer<QWidget> widget;
        QRegion region;
    };
    QList<HeldUpdate> held;
    const QList<QWidget*> dirty = manager->dirtyWidgetList();
    held.reserve(dirty.size());
    for (QWidget* widget : dirty) {
        if (!widget || widget == spectrum) {
            continue;
        }
        held.append(HeldUpdate{widget, QWidgetPrivate::get(widget)->dirty});
    }
    for (const HeldUpdate& heldUpdate : held) {
        if (heldUpdate.widget) {
            manager->removeDirtyWidget(heldUpdate.widget);
        }
    }

    // UpdateLater only queues the texture widget. sync() paints it before
    // that posted request runs, so the request does not fold the spectrum
    // back into the widgets we held out.
    manager->markDirty(spectrum->rect(), spectrum, QWidgetRepaintManager::UpdateLater);
    manager->sync();
    inPresent = false;

    for (const HeldUpdate& heldUpdate : held) {
        QWidget* widget = heldUpdate.widget;
        if (!widget || heldUpdate.region.isEmpty() || widget->window() != top
            || !widget->isVisible() || !widget->updatesEnabled()) {
            continue;
        }
        manager->markDirty(heldUpdate.region, widget, QWidgetRepaintManager::UpdateLater);
    }
    noteIntelPresent(true, 0, 0, intelSurfaceState(spectrum));
    return true;
#endif
}

namespace {

bool embeddedRenderToTexture(QWidget* widget)
{
    if (!widget) {
        return false;
    }
    const QObjectList children = widget->children();
    for (QObject* object : children) {
        auto* child = qobject_cast<QWidget*>(object);
        if (!child || child->isWindow()) {
            continue;
        }
#if defined(NEREUS_INTEL_SPECTRUM_PRESENT)
        if (QWidgetPrivate::get(child)->renderToTexture) {
            return true;
        }
#endif
        if (embeddedRenderToTexture(child)) {
            return true;
        }
    }
    return false;
}

} // namespace

void releaseIntelTopLevelRhiFlushIfIdle(QWidget* top)
{
#ifndef NEREUS_INTEL_SPECTRUM_PRESENT
    Q_UNUSED(top);
#else
    if (!top) {
        return;
    }
    top = top->window();
    if (!top || embeddedRenderToTexture(top)) {
        return;
    }
    // textureChildSeen stays set after the last texture child becomes its
    // own window. Leaving usesRhiFlush set keeps every meter update on the
    // full-window texture copy.
    QWidgetPrivate::get(top)->usesRhiFlush = false;
#endif
}

} // namespace NereusSDR
