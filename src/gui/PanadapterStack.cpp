// no-port-check: AetherSDR-derived NereusSDR file. Pan layout manager
// (5-template QSplitter tree, active-pan tracking, float-pan signal) is
// adapted structurally from AetherSDR src/gui/PanadapterStack.{h,cpp}
// [@0cd4559]. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterStack.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterStack.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanadapterStack.h for full Modification history (NereusSDR).
// =================================================================

#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanFloatingWindow.h"
#include "gui/SpectrumWidget.h"
#include "core/AppSettings.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QSplitterHandle>
#include <QPainter>
#include <QEvent>
#include <QTimer>
#include <QWindow>
#include <QSet>
#include <QStringList>

namespace NereusSDR {

namespace {

// A splitter handle that is thin to look at and thick to grab.
//
// QSplitter gives the handle rect both jobs at once: whatever width the
// handle has is both what gets painted and what the mouse must hit. The pan
// handles were left at the style metric, so the operator was aiming at a
// couple of physical millimetres of unpainted gap. Widening the handle alone
// would have traded that for a fat empty channel between pans, so the extra
// width is spent on hit area and the paint stays a hairline.
//
// Colours are the existing divider tokens (StyleConstants kBorderSubtle
// "#203040" is what MainWindow's own splitter already uses,
// MainWindow.cpp:2587), so a docked stack looks the same as before this
// change at rest. Hover is the only new visual state, and it exists to
// answer the other half of the bench complaint -- not just "hard to hit"
// but hard to find, since an unpainted gap gives no feedback that the
// pointer is on target.
class PanSplitterHandle : public QSplitterHandle {
public:
    PanSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
        setAttribute(Qt::WA_Hover);
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        m_hovered = true;
        update();
        QSplitterHandle::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = false;
        update();
        QSplitterHandle::leaveEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QPainter p(this);
        const QRect r = rect();
        const QColor line = m_hovered ? QColor(0x3d, 0x6f, 0xa0)
                                      : QColor(0x20, 0x30, 0x40);
        // Hairline at rest, three logical px under the pointer. Centred in
        // the grab rect so the painted divider sits where the operator's eye
        // says the boundary is, whatever the surrounding hit area costs.
        const int thickness = m_hovered ? 3 : 1;
        if (orientation() == Qt::Horizontal) {
            // Horizontal splitter -> vertical handle -> vertical line.
            const int x = r.center().x() - thickness / 2;
            p.fillRect(QRect(x, r.top(), thickness, r.height()), line);
        } else {
            const int y = r.center().y() - thickness / 2;
            p.fillRect(QRect(r.left(), y, r.width(), thickness), line);
        }
        Q_UNUSED(event);
    }

private:
    bool m_hovered {false};
};

// QSplitter only exposes handle construction through a virtual, so the
// custom handle needs a QSplitter subclass to hand it out.
class PanSplitter : public QSplitter {
public:
    PanSplitter(Qt::Orientation orientation, QWidget* parent)
        : QSplitter(orientation, parent) {}

protected:
    QSplitterHandle* createHandle() override
    {
        return new PanSplitterHandle(orientation(), this);
    }
};

// Re-realize a SpectrumWidget's render context against whatever window it
// now lives in.
//
// From AetherSDR src/gui/PanadapterStack.cpp:22-43 [@1e0718ad].
//
// Must run AFTER the reparent and AFTER the new window is mapped, and must
// NOT re-send WindowAboutToChangeInternal -- that goes out once, before the
// move (SpectrumWidget::prepareForTopLevelChange).
//
// Off macOS the pipelines alone are enough; on macOS the widget owns a native
// NSView whose CAMetalLayer belongs to the old window, so the QWindow is
// destroyed and the native leaf re-realized with its ancestor isolation
// intact. The trailing update() is a belt-and-braces repaint for the case
// where the first frame lands before the layer is fully attached.
// ONLY for a widget whose TOP-LEVEL WINDOW changed -- the pan being floated,
// or the pan coming back from a floating window. Never for a pan that merely
// got reparented inside the same window by a splitter rebuild, and never for
// a pan that did not move at all.
//
// Bench 2026-08-08, second report: "the pop out renders now but the main
// window stops when popped out; when popped back in the top pan renders again
// and the bottom one stops". The pattern is that whichever pan did NOT move
// is the one that dies -- because an earlier revision ran this over every
// docked pan. Destroying the QWindow of a native child whose parent window
// never changed tears down a live Metal layer that nothing re-creates: the
// pan keeps its render context by every measure Qt reports (no "No QRhi" is
// ever logged) and simply stops producing frames.
//
// AetherSDR applies it to exactly the same set -- the `rebound` list of
// widgets that came out of a floating window, PanadapterStack.cpp:503-515
// [@1e0718ad] -- and to nothing else.
//
// `wantVisible` is passed in rather than read off isVisible(): the float and
// dock paths hide the widget before the reparent, and a widget hidden
// explicitly is NOT un-hidden by showing its parent, so reading its own
// visibility would leave it hidden forever.
static void refreshAfterTopLevelChange(NereusSDR::SpectrumWidget* sw,
                                       bool wantVisible)
{
    if (!sw) { return; }
#if defined(Q_OS_MAC) && defined(NEREUS_GPU_SPECTRUM)
    sw->hide();
    sw->resetGpuResources();
    if (QWindow* windowHandle = sw->windowHandle()) {
        windowHandle->destroy();
    }
    sw->applyNativeWindowIsolationPolicy();
    if (wantVisible) {
        sw->show();
    }
    QTimer::singleShot(50, sw, [sw]() { sw->update(); });
#else
    sw->resetGpuResources();
    if (wantVisible) { sw->show(); }
#endif
}

}  // namespace

QSplitter* PanadapterStack::makeSplitter(Qt::Orientation orientation,
                                         QWidget* parent)
{
    auto* s = new PanSplitter(orientation, parent);
    s->setHandleWidth(kSplitterHandleWidth);
    // A collapsible child lets a drag past the end swallow a pan whole,
    // leaving nothing to grab to bring it back. AetherSDR sets the same on
    // every pan splitter it builds (PanadapterStack.cpp:87 [@1e0718ad]).
    s->setChildrenCollapsible(false);
    return s;
}

PanadapterStack::PanadapterStack(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_rootSplitter = makeSplitter(Qt::Vertical, this);
    layout->addWidget(m_rootSplitter);

    addPanadapter(QStringLiteral("pan-0"));
}

PanadapterStack::~PanadapterStack()
{
    dockAllFloatingPans();
}

PanadapterApplet* PanadapterStack::addPanadapter(const QString& panId)
{
    if (m_pans.contains(panId)) {
        return m_pans[panId];
    }
    auto* applet = new PanadapterApplet(panId, this);
    m_pans[panId] = applet;
    if (m_activePanId.isEmpty()) { setActivePan(panId); }
    if (m_pans.size() == 1) {
        m_rootSplitter->addWidget(applet);
    }
    emit countChanged(m_pans.size());
    return applet;
}

void PanadapterStack::removePanadapter(const QString& panId)
{
    auto* applet = m_pans.take(panId);
    if (!applet) { return; }
    applet->deleteLater();

    // Re-seat the active id if it named the pan just destroyed. activePanId()
    // feeds "Add slice on active pan" (Ctrl+R), "Float active pan..." and
    // rebuildFftRouting's last-resort pan resolution, so a stale id points all
    // three at a pan that no longer exists. Nothing would ever repair it:
    // setActivePan's only caller is guarded on m_activePanId.isEmpty(), so a
    // stale NON-empty id is permanent.
    //
    // Clearing on the last removal is what re-arms that guard, so the next
    // pan created becomes active. Untouched when some other pan was removed --
    // re-seating on every removal would move the operator's working pan out
    // from under them.
    //
    // m_pans is a QMap, so constBegin() is the lowest surviving pan id rather
    // than an arbitrary one -- "pan-0" before "pan-1". Deterministic on
    // purpose: which pan Ctrl+R targets after a close should not depend on
    // hash ordering.
    if (m_activePanId == panId) {
        setActivePan(m_pans.isEmpty() ? QString() : m_pans.constBegin().key());
    }

    emit countChanged(m_pans.size());
}

void PanadapterStack::removeAll() { /* TODO Task 5 */ }

void PanadapterStack::applyLayout(const QString& layoutId, const QStringList& panIds)
{
    // Pans that were floating have just come back from a DIFFERENT top-level
    // window, so their render contexts have to be rebuilt rather than merely
    // re-shown. Every other applet keeps its top-level across this rebuild
    // and must be left alone.
    const QStringList returnedFromFloat = dockAllFloatingPans();
    clearSplitters();

    // Retire orphan pans not referenced by the new layout. Without this,
    // switching from a layout that uses "pan-0" to one keyed on different ids
    // (e.g. "p0..p3" in 2x2 tests) would leak the prior pans into m_pans and
    // distort count(). NereusSDR-specific addition; AetherSDR's layout swap
    // assumes the caller passes the canonical id set.
    const QSet<QString> wanted(panIds.constBegin(), panIds.constEnd());
    const QList<QString> existing = m_pans.keys();
    for (const QString& id : existing) {
        if (!wanted.contains(id)) {
            removePanadapter(id);
        }
    }

    m_currentLayoutId = layoutId;

    if (layoutId == QStringLiteral("1") && !panIds.isEmpty()) {
        auto* applet = addPanadapter(panIds[0]);
        m_rootSplitter->setOrientation(Qt::Vertical);
        m_rootSplitter->addWidget(applet);
        applet->show();
    }
    else if (layoutId == QStringLiteral("2v") && panIds.size() >= 2) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        auto* a = addPanadapter(panIds[0]);
        auto* b = addPanadapter(panIds[1]);
        m_rootSplitter->addWidget(a);
        m_rootSplitter->addWidget(b);
        a->show();
        b->show();
    }
    else if (layoutId == QStringLiteral("2h") && panIds.size() >= 2) {
        m_rootSplitter->setOrientation(Qt::Horizontal);
        auto* a = addPanadapter(panIds[0]);
        auto* b = addPanadapter(panIds[1]);
        m_rootSplitter->addWidget(a);
        m_rootSplitter->addWidget(b);
        a->show();
        b->show();
    }
    else if (layoutId == QStringLiteral("12h") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        auto* top = addPanadapter(panIds[0]);
        m_rootSplitter->addWidget(top);
        top->show();

        auto* bottomSplitter = makeSplitter(Qt::Horizontal, m_rootSplitter);
        auto* bl = addPanadapter(panIds[1]);
        auto* br = addPanadapter(panIds[2]);
        bottomSplitter->addWidget(bl);
        bottomSplitter->addWidget(br);
        bl->show();
        br->show();
        m_rootSplitter->addWidget(bottomSplitter);

        m_rootSplitter->setStretchFactor(0, 2);  // wide top gets 2x weight
        m_rootSplitter->setStretchFactor(1, 1);
    }
    else if (layoutId == QStringLiteral("2x2") && panIds.size() >= 4) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = makeSplitter(Qt::Horizontal, m_rootSplitter);
        auto* tl = addPanadapter(panIds[0]);
        auto* tr = addPanadapter(panIds[1]);
        topRow->addWidget(tl);
        topRow->addWidget(tr);
        tl->show();
        tr->show();

        auto* bottomRow = makeSplitter(Qt::Horizontal, m_rootSplitter);
        auto* bl = addPanadapter(panIds[2]);
        auto* br = addPanadapter(panIds[3]);
        bottomRow->addWidget(bl);
        bottomRow->addWidget(br);
        bl->show();
        br->show();

        m_rootSplitter->addWidget(topRow);
        m_rootSplitter->addWidget(bottomRow);
    }
    else if (layoutId == QStringLiteral("2h1") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = makeSplitter(Qt::Horizontal, m_rootSplitter);
        auto* tl = addPanadapter(panIds[0]);
        auto* tr = addPanadapter(panIds[1]);
        topRow->addWidget(tl);
        topRow->addWidget(tr);
        tl->show();
        tr->show();
        m_rootSplitter->addWidget(topRow);

        auto* bottom = addPanadapter(panIds[2]);
        m_rootSplitter->addWidget(bottom);
        bottom->show();
    }
    else if (layoutId == QStringLiteral("3v") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < 3; ++i) {
            auto* p = addPanadapter(panIds[i]);
            m_rootSplitter->addWidget(p);
            p->show();
        }
    }
    else if (layoutId == QStringLiteral("4v") && panIds.size() >= 4) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < 4; ++i) {
            auto* p = addPanadapter(panIds[i]);
            m_rootSplitter->addWidget(p);
            p->show();
        }
    }
    else if (layoutId == QStringLiteral("3h2") && panIds.size() >= 5) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = makeSplitter(Qt::Horizontal, m_rootSplitter);
        for (int i = 0; i < 3; ++i) {
            auto* p = addPanadapter(panIds[i]);
            topRow->addWidget(p);
            p->show();
        }
        m_rootSplitter->addWidget(topRow);

        auto* bottomRow = makeSplitter(Qt::Horizontal, m_rootSplitter);
        for (int i = 3; i < 5; ++i) {
            auto* p = addPanadapter(panIds[i]);
            bottomRow->addWidget(p);
            p->show();
        }
        m_rootSplitter->addWidget(bottomRow);
    }

    // Deferred a tick so the rebuilt splitter tree is laid out and mapped
    // before the render context is re-created against it.
    if (!returnedFromFloat.isEmpty()) {
        QTimer::singleShot(0, this, [this, returnedFromFloat]() {
            refreshReturnedFromFloat(returnedFromFloat);
        });
    }
}

PanadapterApplet* PanadapterStack::panadapter(const QString& id) const { return m_pans.value(id, nullptr); }
QList<PanadapterApplet*> PanadapterStack::allApplets() const { return m_pans.values(); }
SpectrumWidget* PanadapterStack::spectrum(const QString& panId) const
{
    PanadapterApplet* applet = m_pans.value(panId, nullptr);
    return applet ? applet->spectrumWidget() : nullptr;
}
void PanadapterStack::setActivePan(const QString& id) { if (m_activePanId != id) { m_activePanId = id; emit activePanChanged(id); } }

// The writer PanadapterApplet::activeSliceIndex() never had after its one-shot
// seed in addSlice. See the header for the bench defect this closes.
//
// The scan is over single-digit pan counts on a user action, and it asks each
// pan the one question that matters -- do you host this slice -- rather than
// resolving through SliceModel::panKey(). panKey is the authoritative binding
// for WHERE a slice belongs, but associatedSlices() is what
// MainWindow::sliceForPan actually reads back, so keying off the same set is
// what makes the pan's answer and this function's answer agree. Every pan that
// lists the slice is updated rather than the first one found: if two ever
// disagree, moving both is the state the operator can act on, where stopping
// at the first would leave a pan silently tuning something else.
void PanadapterStack::setActiveSliceOnHostingPan(int sliceId)
{
    const QList<PanadapterApplet*> pans = allApplets();
    for (PanadapterApplet* applet : pans) {
        if (!applet) { continue; }
        if (!applet->associatedSlices().contains(sliceId)) { continue; }
        applet->setActiveSliceIndex(sliceId);
    }
}

// Keeps associatedSlices() honest across a pan change. See the header.
void PanadapterStack::moveSliceToPan(int sliceId, const QString& destPanId)
{
    PanadapterApplet* dest = m_pans.value(destPanId, nullptr);
    if (!dest) { return; }

    const QList<PanadapterApplet*> pans = allApplets();
    for (PanadapterApplet* applet : pans) {
        if (!applet || applet == dest) { continue; }
        applet->removeSlice(sliceId);
    }
    dest->addSlice(sliceId);
}

// Detach a pan into its own top-level window.
//
// Bench report 2026-08-08: the popped-out pan painted exactly one frame and
// then logged "QRhiWidget: No QRhi" at the display-timer rate forever, i.e.
// a live-looking but frozen panadapter. The previous implementation's
// hide/show cycle acted on the APPLET, and hiding a parent does not make a
// QRhiWidget release its render context, so nothing here ever told the
// SpectrumWidget its window had changed. The three-call teardown below is
// what actually does that, ported from AetherSDR, which hit and fixed the
// same defect on the same widget class.
//
// From AetherSDR src/gui/PanadapterStack.cpp:785-831 [@1e0718ad].
void PanadapterStack::floatPanadapter(const QString& panId)
{
    auto* applet = m_pans.value(panId, nullptr);
    if (!applet || m_floating.contains(panId)) { return; }

    // Release the GPU resources BEFORE the reparent, while the widget still
    // belongs to the window that owns them.
    SpectrumWidget* sw = applet->spectrumWidget();
    if (sw) {
        sw->hide();
        sw->prepareForTopLevelChange();
        sw->resetGpuResources();
    }

    // Parented to the main window: Qt::Window still makes this top-level (so
    // it can go to a second monitor), and the parent only buys z-order above
    // the console plus destruction with it.
    auto* floater = new PanFloatingWindow(window());
    floater->adoptApplet(applet);       // straight from splitter to window
    applet->setFloatingState(true);     // title strip + Dock button appear
    m_floating[panId] = floater;

    // Both ways back -- the window's close box and the strip's Dock button --
    // land on the same slot, so they cannot drift apart.
    connect(floater, &PanFloatingWindow::dockRequested,
            this, &PanadapterStack::dockPanadapter);
    connect(applet, &PanadapterApplet::dockRequested,
            this, &PanadapterStack::dockPanadapter, Qt::UniqueConnection);

    floater->restoreWindowGeometry();
    floater->show();
    floater->raise();

    // Deferred a tick so the new window is mapped and Metal is bound to the
    // new NSView before the first render. Showing before the reset can drive
    // render() against a stale surface.
    // Only the floated widget. The pans left behind keep the same top-level
    // and the same splitter -- QSplitter just drops a child and resizes the
    // rest -- so touching them is what used to kill them.
    QTimer::singleShot(0, this, [sw]() {
        refreshAfterTopLevelChange(sw, /*wantVisible=*/true);
    });
}

// Bring a floated pan back into the splitter tree.
//
// From AetherSDR src/gui/PanadapterStack.cpp:833-860 [@1e0718ad] — same
// teardown-before-reparent order as the float path, for the same reason.
void PanadapterStack::dockPanadapter(const QString& panId)
{
    PanFloatingWindow* floater = m_floating.take(panId);
    if (!floater) { return; }

    floater->saveWindowGeometry();

    PanadapterApplet* applet = floater->applet();
    SpectrumWidget* sw = applet ? applet->spectrumWidget() : nullptr;
    if (sw) {
        sw->hide();
        sw->prepareForTopLevelChange();
        sw->resetGpuResources();
    }
    if (applet) { applet->setFloatingState(false); }

    // Drop our connections before the teardown so a close-driven dock cannot
    // re-enter this slot while it is running.
    QObject::disconnect(floater, nullptr, this, nullptr);
    if (applet) { QObject::disconnect(applet, &PanadapterApplet::dockRequested,
                                      this, nullptr); }

    // Reparent straight onto the stack, never through nullptr, then let
    // applyLayout put it back in the splitter tree.
    floater->takeApplet(this);
    floater->hide();
    floater->deleteLater();

    // This pan is already out of m_floating (taken at the top), so
    // applyLayout's own dockAllFloatingPans finds nothing and schedules
    // nothing for it -- the refresh has to be named here. Exactly one pass,
    // over exactly the one widget whose top-level changed.
    applyLayout(m_currentLayoutId, m_pans.keys());

    QTimer::singleShot(0, this, [this, panId]() {
        refreshReturnedFromFloat({panId});
    });
}

// Re-realize only the pans that just came back from a floating window.
//
// Everything else in the tree kept its top-level, so it needs nothing: a
// plain layout swap moved applets between splitters long before any of this
// and never needed a GPU touch.
void PanadapterStack::refreshReturnedFromFloat(const QStringList& panIds)
{
    for (const QString& panId : panIds) {
        PanadapterApplet* applet = m_pans.value(panId, nullptr);
        if (!applet) { continue; }
        refreshAfterTopLevelChange(applet->spectrumWidget(),
                                   /*wantVisible=*/true);
    }
}

void PanadapterStack::rebuildSplitters(const QString&, const QStringList&) {}

void PanadapterStack::saveFloatingGeometry()
{
    for (auto it = m_floating.cbegin(); it != m_floating.cend(); ++it) {
        if (PanFloatingWindow* floater = it.value()) {
            floater->saveWindowGeometry();
        }
    }
}

QStringList PanadapterStack::dockAllFloatingPans()
{
    QStringList returned;
    while (!m_floating.isEmpty()) {
        auto it = m_floating.begin();
        PanFloatingWindow* floater = it.value();
        const QString panId = it.key();
        m_floating.erase(it);
        if (!floater) { continue; }

        QObject::disconnect(floater, nullptr, this, nullptr);
        // Before anything else, because this path is also the destructor's,
        // and quitting with a pan still floating is the ordinary way to end a
        // session. dockPanadapter and the window's close box both save; this
        // one did not, so the last move or resize of the day was the one
        // guaranteed to be lost. Found by Codex on PR #318.
        floater->saveWindowGeometry();
        if (PanadapterApplet* applet = floater->applet()) {
            QObject::disconnect(applet, &PanadapterApplet::dockRequested,
                                this, nullptr);
            // Same teardown-before-reparent order as dockPanadapter. This
            // path also runs from the destructor, where the applets are about
            // to die anyway, but releasing the GPU resources while the widget
            // still has its owning window is what keeps that orderly.
            if (SpectrumWidget* sw = applet->spectrumWidget()) {
                sw->hide();
                sw->prepareForTopLevelChange();
                sw->resetGpuResources();
            }
            applet->setFloatingState(false);
            floater->takeApplet(this);
            applet->hide();
            returned << panId;
        }
        delete floater;
    }
    return returned;
}

void PanadapterStack::clearSplitters()
{
    // Detach all applets from the current splitter tree but do not delete them
    // (they live in m_pans and may be re-attached by the new layout).
    for (auto* applet : m_pans.values()) {
        applet->setParent(this);
        applet->hide();
    }
    // Tear down the splitter tree and rebuild from scratch.
    if (m_rootSplitter) {
        layout()->removeWidget(m_rootSplitter);
        m_rootSplitter->deleteLater();
    }
    m_rootSplitter = makeSplitter(Qt::Vertical, this);
    layout()->addWidget(m_rootSplitter);
}

// Phase 3F Sub-Epic D Task 6: persist splitter geometry across launches.
// Storage layout: PanLayoutId + PanSplitter0Sizes (root) + PanSplitter1Sizes /
// PanSplitter2Sizes (nested splitters for 12h / 2x2). Sub-Epic D ships only
// root-splitter persistence; nested splitter persistence may be wired in
// Sub-Epic H polish if bench feedback demands it.
void PanadapterStack::saveSplitterState()
{
    if (!m_rootSplitter) { return; }
    auto& s = AppSettings::instance();
    QStringList parts;
    const QList<int> sizes = m_rootSplitter->sizes();
    for (int sz : sizes) {
        parts << QString::number(sz);
    }
    s.setValue(QStringLiteral("PanSplitter0Sizes"), parts.join(QStringLiteral(",")));
    s.setValue(QStringLiteral("PanLayoutId"), m_currentLayoutId);
}

void PanadapterStack::restoreSplitterState()
{
    if (!m_rootSplitter) { return; }
    auto& s = AppSettings::instance();
    const QString raw = s.value(QStringLiteral("PanSplitter0Sizes"), QString()).toString();
    if (raw.isEmpty()) { return; }
    QList<int> sizes;
    const QStringList parts = raw.split(QStringLiteral(","), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        sizes << part.toInt();
    }
    if (!sizes.isEmpty()) {
        m_rootSplitter->setSizes(sizes);
    }
}

QList<int> PanadapterStack::rootSplitterSizes() const
{
    if (!m_rootSplitter) { return {}; }
    return m_rootSplitter->sizes();
}

void PanadapterStack::rootSplitterSetSizesForTest(const QList<int>& sizes)
{
    if (m_rootSplitter) {
        m_rootSplitter->setSizes(sizes);
    }
}

} // namespace NereusSDR
