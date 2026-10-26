// =================================================================
// src/gui/containers/ContainerManager.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 - setTransmitting hands the transmit state to every
//                 container meter's fade covers, VFO displays and button
//                 grids (MeterWidget::setItemTxState), and each meter that
//                 is announced later takes the current state at once.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-16 - Bench fix (JJ's Rotor window created at (0,1337) on a
//                 1512 x 982 screen): a floating window is given a place
//                 where its header can be reached every time it is shown,
//                 when a screen comes, goes or changes, and when its applet
//                 is ticked under Containers > Applets
//                 (keepWindowReachable, bringAppletOnScreen). Containers >
//                 Hidden lists containers hidden on receive (or transmit)
//                 and by a macro, as effectiveVisible hides them
//                 (hiddenByOwnFlags), and shows them again. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-12 - A floating window is never wider or taller than all the
//                 screens together: the size a document, a remembered
//                 place or the content's height gives it is fitted to
//                 that, and each window's maximum size is kept at it as
//                 screens come, go and change. A window stretched across
//                 monitors keeps its size, and a remembered size is never
//                 rewritten from the screens present (a monitor that is
//                 away must not cost a window its size); the operator's
//                 next move or resize saves the size shown. The height
//                 follower leaves a
//                 window alone while a drag moves it and runs when the
//                 drag ends. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Both looks: the header rule judges only a place the app
//                 restores (start-up, a monitor change, a layout loaded),
//                 never the place the operator's own drop or resize just
//                 committed, and it reads the screen's usable area. A
//                 commit that changes what sizes a window (auto height,
//                 content, layout, header) sizes it from the document
//                 though its place did not change. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Both looks: a window is at home only where its header can
//                 be reached (headerReachable); one whose rect touches its
//                 screen with the header off it, as after a drop in
//                 resolution, is shown on the screen and keeps its saved
//                 place. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Both looks: reconcile puts a window at its document
//                 place and size only when the commit changed that place
//                 or its monitor (or the place differs from the remembered
//                 one). A window the system moved with no drag is left
//                 where it is by a commit of something else; that move is
//                 still not remembered or saved. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Both looks: with two monitors reporting one serial, a
//                 window is matched to the one it was on (its saved
//                 geometry, then its remembered place), not the first.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: a free home that no longer fits
//                 the pane returns to the nearest place inside it that
//                 covers no other free container (nearestClearFreeRect),
//                 and steps off the others' top-lefts only when no place is
//                 clear. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Workspace preview only: a free home is recorded with the
//                 pane its rect was placed in, and Return restores both, so
//                 a container comes back where it was showing after the
//                 pane changed size. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: the commit that makes a container
//                 a window records where it came from (recordHomes), and
//                 Return puts it back there, with fallbacks when the
//                 neighbours, the strip or the free spot are gone
//                 (returnHome). The pin's choice is committed at once. In
//                 both looks a window made by an operator command (Pop
//                 Out, a drop outside the main window, a new or duplicated
//                 container) is saved with the place and monitor it was
//                 given. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Workspace preview only: while arranging, one drop rule
//                 for every container (finishArrangeDrop). A header drag of
//                 a strip, free, floating or pop-out container docks in an
//                 edge zone at the previewed place, sits free over the
//                 pane, becomes a window outside the main window and is
//                 cancelled anywhere else; a drop commits once and a
//                 cancelled drag commits nothing. Not arranging, a drag is
//                 as before. Each container is told the Arrange switch
//                 apart from the Shift hold. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: arranging is the switch or the
//                 Shift hold (setArrangeHeld, arranging); containers, entry
//                 headers and the edge bands follow arranging(), and
//                 containerDragActive and containerDragFinished report a
//                 header drag or grip resize. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: a header is Shown or Hidden. A
//                 container saved with Reveal loads, runs and commits as
//                 Hidden there (hideRevealHeaders). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: the look has no Lock setting. A
//                 container loads, runs and commits unlocked there
//                 (lockApplies, clearLocks), and a strip divider beside a
//                 container that was saved locked moves. With the look off
//                 Lock is unchanged. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - Windows remember their monitor and no two share a default
//                 spot. Screens are read through a ScreenSource. A floating
//                 window is shown at its remembered place when its monitor
//                 is present (or, with none saved, when its rect is on a
//                 present screen), else on the main window's screen with
//                 its remembered place and monitor left alone; a screen
//                 coming or going follows the same rule and commits
//                 nothing. Every default placement goes through
//                 defaultWindowRect or defaultFreeRect. A window made free
//                 remembers a rect inside the pane. A layout saved with no
//                 pane size takes the pane's settled size as its anchor
//                 baseline. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Containers keep the place the operator gave them: the
//                 Move/Resize filter and reconcile no longer change or
//                 clamp a remembered place, saveState writes remembered
//                 places only, and free containers are shown from them
//                 once the pane has its size and whenever it resizes. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - A pop-out shell whose objects are all hidden leaves the
//                 screen with them in both looks (effectiveVisible). J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Container Settings opens after the menu that asked for it
//                 has returned and belongs to the main window, not to the
//                 container's shell, which Remove deletes while the dialog
//                 is open (JJ's crash removing a popped-out container).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - An edge strip's floor follows a container's content
//                 minimum at once (scheduleEdgeStripContentHints, one
//                 layout per burst), not only on the next reconcile. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - A container's minimum width follows what its content host
//                 needs (the widest visible applet, the margins, the
//                 vertical bar), and today's Main Panel splitter applies it
//                 at once without capturing it as a geometry change. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - A duplicated container is always shown, floating, next
//                 to its original, in both looks (showDuplicate). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: hidden containers and entries are
//                 listed for Containers > Hidden and shown again where they
//                 were; a container whose entries are all hidden leaves the
//                 screen with them. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the right strip has no padding,
//                 so a migrated Main Panel width is the strip's width.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: applyWorkspaceHeader hands the
//                 container the slice its chip shows for the source menu
//                 tick. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: the header source menu commits the
//                 container source as Container Settings does (and Active
//                 slice as rxSourceMode followSelectedRx); a following
//                 container's chip reads Active. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview review fixes: profile strips drop
//                 carried thickness and extents, stale edge drags end,
//                 hidden-strip extents prune deleted ids, Move/Resize honour
//                 capture suppression; a container moved into a strip
//                 takes a default share and the saved split stays.
//                 J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: geometry capture suppression for
//                 workspace profile switches by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: PanelDocked containers in the
//                 strip for their edge, Move to Edge, Place Free, the
//                 edge drag preview and commit, the strip arrangement,
//                 grid alignment, strip size hints, lock on strip handles
//                 and the pop-out size by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: app-level Arrange mode, the group
//                 header's title, source chip and lock request, and the
//                 auto-height header extent by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-04 — Follow responsive content height in auto-height containers by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-04 — Preserve pending container placement and initialize overlay
//                 anchor baselines by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-04 — Suppress shutdown presentation reconciliation while retaining
//                 persistence by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Effective contextual draft properties and portable settings by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Draft-only edits and inert cached previews by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-11 - Both looks: a deleted container drops only its own
//                 registration. One removed and added again under the same
//                 id before the deferred delete ran took the new container
//                 out of the manager, so shutdown never released its
//                 content and the host then read the deleted registry.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: a popped-out object's window
//                 whose height the operator set keeps it, growing only
//                 when its content no longer fits; the resize that set it
//                 is saved at once with auto height off (JJ, 2026-10-11).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22  J.J. Boyd (KG4VCF): dialogs exec()ed with a parent are
//               built through ScopedChildWidget instead of on the stack, so
//               a parent that dies while one is open no longer frees it
//               twice. With AI-assisted transformation via Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "ContainerManager.h"
#include "gui/ScopedChildWidget.h"
#include "gui/meters/MeterPoller.h"
#include "ContainerArrangeController.h"
#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QScrollArea>
#include <QSplitterHandle>
#include "ContainerWidget.h"
#include "ContainerSettingsDialog.h"
#include "FloatingContainer.h"
#include "ScreenSource.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"

#include <QSplitter>
#include <QScopedValueRollback>
#include <QJsonArray>
#include <QEvent>
#include <QUuid>
#include <algorithm>
#include "ContainerContentHost.h"
#include "ContainerContentRegistry.h"
#include "ContainerWorkspaceStore.h"
#include "gui/GuiApplication.h"
#include "ContainerDocumentCodec.h"
#include "core/ControlRanges.h"
#include "gui/styles/WorkspacePreview.h"
#include "EdgeDockStrip.h"
#include "WorkspaceHeader.h"
#include "ContainerSourceAdapter.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspaceStyle.h"
#include <algorithm>

namespace NereusSDR {

namespace {
// Locate the MeterWidget hosted inside a container's content widget.
// Container shape varies: user-created containers use a bare MeterWidget
// as content; the panel container wraps a MeterWidget inside an
// AppletPanelWidget. findChild<MeterWidget*>() handles both cases — and
// returns the same pointer for the bare case since the search is
// inclusive of the receiver. Returns nullptr for placeholder content.
MeterWidget* innerMeterWidget(QWidget* content)
{
    if (!content) {
        return nullptr;
    }
    if (auto* m = qobject_cast<MeterWidget*>(content)) {
        return m;
    }
    return content->findChild<MeterWidget*>();
}

// Where a free container remembered at `at` shows once its pane has grown
// by `grown` since: the axis rule of updateDockedPositions below.
// From Thetis MeterManager.cs:5812-5865 [v2.10.3.15]
QPoint anchoredTopLeft(const QPoint& at, AxisLock lock, const QSize& grown)
{
    switch (lock) {
    case AxisLock::Right:
    case AxisLock::BottomRight:
    case AxisLock::Bottom:
        return at + QPoint(grown.width(), grown.height());
    case AxisLock::BottomLeft:
    case AxisLock::Left:
        return at + QPoint(0, grown.height());
    case AxisLock::TopLeft:
        return at;
    case AxisLock::Top:
    case AxisLock::TopRight:
        return at + QPoint(grown.width(), 0);
    }
    return at;
}

// How long the pane must go without a resize, once showing, before its
// size counts as settled. NereusSDR-original value: the spectrum pane is
// given a second size a few event-loop turns after its first.
constexpr int kPaneSettleMs = 100;
// A container is never placed smaller than this.
constexpr QSize kPlacementMinimum(260, 24);
// The size a container with no size of its own is placed at.
constexpr QSize kPlacementFallbackSize(360, 300);
constexpr int kMaxCascadeSteps = 256;
// How much of a window's header has to be on a screen for the window to
// count as shown there: a piece wide enough to take hold of, and at least
// half the header's height (a header across the seam between two screens
// has that on one of them). NereusSDR-original values.
constexpr int kHeaderReachWidthPx = 48;
constexpr int kHeaderReachHeightPx = ContainerWidget::kTitleBarHeight / 2;

// `wanted`, held inside `bounds` (when there are any), then moved down and
// right by `step` until its top-left is none of `taken`. A step that would
// leave the bounds wraps to their other side.
QRect cascaded(const QRect& wanted, const QRect& bounds, const QVector<QPoint>& taken, int step)
{
    const bool bounded = bounds.isValid();
    const QRect start = bounded ? FloatingContainer::clampedGeometry(wanted, bounds, kPlacementMinimum) : wanted;
    const int spanX = bounded ? qMax(0, bounds.width() - start.width()) + 1 : 0;
    const int spanY = bounded ? qMax(0, bounds.height() - start.height()) + 1 : 0;
    for (int k = 0; k < kMaxCascadeSteps; ++k) {
        QPoint at = start.topLeft() + QPoint(k * step, k * step);
        if (bounded) {
            at.setX(bounds.left() + (at.x() - bounds.left()) % spanX);
            at.setY(bounds.top() + (at.y() - bounds.top()) % spanY);
        }
        if (!taken.contains(at)) {
            return QRect(at, start.size());
        }
    }
    return start;
}
} // namespace

ContainerManager::ContainerManager(QWidget* dockParent, QSplitter* splitter,
                                   QObject* parent)
    : QObject(parent)
    , m_dockParent(dockParent)
    , m_splitter(splitter)
{
    m_screens = ScreenSource::system();
    // Every meter announced for polling (created, restored, rebuilt or
    // moved) starts in the current transmit state, so an item loaded or
    // added while transmitting does not wait for the next change.
    connect(this, &ContainerManager::meterReadyForPolling, this, [this](MeterWidget* meter) {
        if (meter) {
            meter->setItemTxState(m_transmitting);
        }
    });
    m_geometryCommit.setSingleShot(true);
    m_geometryCommit.setInterval(200);
    connect(&m_geometryCommit, &QTimer::timeout, this, &ContainerManager::saveState);
    m_paneSettle.setSingleShot(true);
    m_paneSettle.setInterval(kPaneSettleMs);
    connect(&m_paneSettle, &QTimer::timeout, this, [this] {
        // Only a pane that is showing has been through its window's first
        // layout; a hidden one is asked again when it shows or resizes.
        if (m_dockParent && m_dockParent->isVisible()) {
            m_paneSettled = true;
            m_provisionalPane.clear();
        }
    });
    // A top-level pane is laid out at once.
    m_paneSettled = m_dockParent && m_dockParent->isWindow();
    if (m_dockParent) {
        // Free containers are shown again whenever the pane resizes, and a
        // child pane has no size of its own before its first resize.
        m_dockParent->installEventFilter(this);
        connect(m_dockParent, &QObject::destroyed, this, [this] { m_dockParent = nullptr; });
    }
    // Queued: the screen list is read once the change has been applied.
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) { screensChanged(); },
            Qt::QueuedConnection);
    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *) { screensChanged(); },
            Qt::QueuedConnection);
    // A floating window's maximum size is all the screens together; it is
    // kept current as they come, go (screensChanged) and change.
    connect(qApp, &QGuiApplication::screenAdded, this, &ContainerManager::watchScreen);
    const QList<QScreen*> present = QGuiApplication::screens();
    for (QScreen* screen : present) {
        watchScreen(screen);
    }
    qCDebug(lcContainer) << "ContainerManager created";
}

void ContainerManager::setPreviewPoller(MeterPoller* poller) { m_previewPoller=poller; }
MeterPoller* ContainerManager::previewPoller() const { return m_previewPoller; }

ContainerManager::~ContainerManager()
{
    m_geometryCommit.stop();
    if (m_store) { disconnect(m_store, nullptr, this, nullptr); }
    if (m_registry) { disconnect(m_registry, nullptr, this, nullptr); }
    for (auto* c : m_containers) { if (auto* host = contentHost(c->id())) { host->releaseViews(); } }
    const auto forms=m_floatingForms.values();
    for (auto* form : forms) { delete form; }
    qCDebug(lcContainer) << "ContainerManager destroyed —" << m_containers.size() << "containers";
}

// Nereus-origin structured route: commit first, project only committed snapshots.
ContainerWorkspaceStore* ContainerManager::workspaceStore() const { return m_store; }
ContainerContentRegistry* ContainerManager::contentRegistry() const { return m_registry; }
void ContainerManager::setWorkspaceAdapter(ContainerWorkspaceStore* store, ContainerContentRegistry* registry)
{
    m_geometryCommit.stop();
    m_pendingGeometry.clear();
    m_documentGeometry.clear();
    m_rememberedGeometry.clear();
    m_rememberedPane.clear();
    m_documentPane.clear();
    m_documentScreen.clear();
    m_documentSizing.clear();
    m_interactionStart.clear();
    m_provisionalPane.clear();
    m_rememberedScreen.clear();
    m_displaced.clear();
    if (m_store) { disconnect(m_store, nullptr, this, nullptr); }
    if (m_registry) { disconnect(m_registry, nullptr, this, nullptr); }
    if (m_arrange) {
        delete m_arrange;
        m_arrange = nullptr;
    }
    m_store = store; m_registry = registry;
    if (!store || !registry) { return; }
    m_arrange = new ContainerArrangeController(*store, this, this);
    m_storageError = store->loadError();
    connect(store, &ContainerWorkspaceStore::committed, this, [this] { reconcileWorkspace(m_store->snapshot()); });
    connect(registry, &ContainerContentRegistry::runtimeChanged, this, [this] {
        if (!m_reconciling && m_store) { reconcileWorkspace(m_store->snapshot()); }
    });
    if (m_splitter) {
        m_splitter->installEventFilter(this);
        connect(m_splitter, &QSplitter::splitterMoved, this, [this] {
            if (!m_reconciling) { m_geometryCommit.start(); }
        });
    }
}
ContainerContentHost* ContainerManager::contentHost(const QString& id) const
{
    auto* c = container(id);
    return c ? qobject_cast<ContainerContentHost*>(c->content()) : nullptr;
}
CommitResult ContainerManager::commitWorkspace(const WorkspaceDocument& document, quint64 expectedRevision)
{
    if (!m_store || !m_registry) { return {CommitStatus::Invalid, 0, QStringLiteral("No workspace adapter")}; }
    // Existing duplicate singleton records remain recoverable; transactions may
    // repair them, but cannot create another supported singleton placement.
    QHash<QString, int> oldCounts, counts;
    for (const auto& c : m_store->snapshot().containers) { for (const auto& e : c.contents) { ++oldCounts[e.typeId]; } }
    for (const auto& c : document.containers) { for (const auto& e : c.contents) { ++counts[e.typeId]; } }
    for (const auto& d : m_registry->descriptors()) {
        if (d.singleton && counts.value(d.typeId) > 1 && counts.value(d.typeId) > oldCounts.value(d.typeId)) {
            return {CommitStatus::Invalid, expectedRevision, QStringLiteral("%1 already has a placement").arg(d.title)};
        }
    }
    WorkspaceDocument saved = document;
    if (!lockApplies()) {
        clearLocks(saved);
    }
    if (!revealApplies()) {
        hideRevealHeaders(saved);
    }
    recordHomes(saved);
    const CommitResult result = m_store->commit(saved, expectedRevision);
    if (result.status != CommitStatus::Saved) { m_storageError = result.error; emit workspaceError(result.error); }
    else { m_storageError.clear(); }
    return result;
}
void ContainerManager::recordHomes(WorkspaceDocument& saved) const
{
    const WorkspaceDocument before = m_store->snapshot();
    for (auto& d : saved.containers) {
        if (d.dockMode != DockMode::Floating) {
            // Only a window has a place to go back to.
            d.home.reset();
            continue;
        }
        if (d.home || d.popOutShell || !WorkspacePreview::enabled()) {
            continue;
        }
        const auto was = std::find_if(before.containers.cbegin(), before.containers.cend(),
            [&d](const ContainerDocument& value) { return value.id == d.id; });
        if (was == before.containers.cend() || was->dockMode == DockMode::Floating) {
            // Created as a window, or already one.
            continue;
        }
        ContainerHome home;
        home.dockMode = was->dockMode;
        if (was->dockMode == DockMode::PanelDocked) {
            home.edge = was->edge;
            // Its neighbours in the strip: document order is strip order.
            QStringList strip;
            for (const auto& other : before.containers) {
                if (other.dockMode == DockMode::PanelDocked && other.edge == was->edge) {
                    strip.append(other.id);
                }
            }
            const int at = strip.indexOf(d.id);
            home.beforeId = strip.value(at - 1);
            home.afterId = strip.value(at + 1);
        } else {
            // The place the operator gave it, not where it is shown, with
            // the pane that place is read against.
            home.geometry = m_rememberedGeometry.value(d.id, was->geometry);
            if (!home.geometry.isValid()) {
                continue;
            }
            if (m_rememberedPane.contains(d.id)) {
                home.geometryPane = m_rememberedPane.value(d.id);
            } else {
                home.geometryPane = was->geometryPane;
            }
        }
        d.home = home;
    }
}

bool ContainerManager::returnHome(const QString& id)
{
    if (!m_store || m_reconciling) {
        return true;
    }
    WorkspaceDocument draft = m_store->snapshot();
    const auto self = std::find_if(draft.containers.begin(), draft.containers.end(),
        [&id](const ContainerDocument& value) { return value.id == id; });
    if (self == draft.containers.end() || self->dockMode != DockMode::Floating) {
        return true;
    }
    const std::optional<ContainerHome> home = self->home;
    if (home && home->dockMode == DockMode::OverlayDocked && home->geometry.isValid()) {
        // Its rect with the pane that rect was placed in: shown from them,
        // it is where it was showing, whatever size the pane has now.
        QRect place = home->geometry;
        std::optional<QSize> placedIn = home->geometryPane;
        if (dockParentLaidOut()) {
            const QSize pane = m_dockParent->size();
            const QRect showing(anchoredTopLeft(place.topLeft(), self->anchor, pane - placedIn.value_or(pane)),
                                place.size());
            if (!QRect(QPoint(0, 0), pane).contains(showing)) {
                // The spot is no longer inside the pane: the nearest place
                // that is and covers no other free container, placed for
                // the pane as it is now.
                place = nearestClearFreeRect(showing, id);
                placedIn.reset();
            }
        }
        self->dockMode = DockMode::OverlayDocked;
        self->geometry = place;
        self->geometryPane = placedIn;
        self->screen.reset();
        self->home.reset();
        commitWorkspace(draft, draft.revision);
        return true;
    }
    if (!home && id == draft.mainContainerId) {
        return false;
    }
    // Its own strip; with no home, the main container's.
    DockEdge edge = DockEdge::Right;
    if (home) {
        edge = home->edge;
    } else {
        for (const auto& d : draft.containers) {
            if (d.id == draft.mainContainerId && d.dockMode == DockMode::PanelDocked) {
                edge = d.edge;
            }
        }
    }
    QStringList strip;
    for (const auto& d : draft.containers) {
        if (d.id != id && d.dockMode == DockMode::PanelDocked && d.edge == edge) {
            strip.append(d.id);
        }
    }
    // After the neighbour it had before it; else before the one it had
    // after it; else at the end. An empty strip opens again with it.
    int index = -1;
    if (home) {
        const int before = home->beforeId.isEmpty() ? -1 : strip.indexOf(home->beforeId);
        const int after = home->afterId.isEmpty() ? -1 : strip.indexOf(home->afterId);
        if (before >= 0) {
            index = before + 1;
        } else if (after >= 0) {
            index = after;
        }
    }
    moveContainerToEdge(id, edge, index);
    return true;
}

bool ContainerManager::lockApplies()
{
    return !WorkspacePreview::enabled();
}
bool ContainerManager::clearLocks(WorkspaceDocument& document)
{
    bool cleared = false;
    for (auto& container : document.containers) {
        cleared = cleared || container.locked;
        container.locked = false;
    }
    return cleared;
}
bool ContainerManager::revealApplies()
{
    return !WorkspacePreview::enabled();
}
bool ContainerManager::hideRevealHeaders(WorkspaceDocument& document)
{
    bool changed = false;
    for (auto& container : document.containers) {
        if (container.header == HeaderMode::Reveal) {
            container.header = HeaderMode::Hidden;
            changed = true;
        }
    }
    return changed;
}
bool ContainerManager::effectiveVisible(const ContainerDocument &document) const
{
    // Workspace preview: a container whose entries are all hidden (the
    // entry header's hide button, Containers > Applets) leaves the screen
    // with them and comes back when any of them is shown. An empty
    // container stays, as before.
    // A pop-out shell does the same in both looks: a shell whose objects
    // are all hidden is never left open and blank (JJ 2026-10-09).
    if ((WorkspacePreview::enabled() || document.popOutShell) && !document.contents.isEmpty()
        && std::none_of(document.contents.cbegin(), document.contents.cend(),
                        [](const ContentEntry &entry) { return entry.visible; })) {
        return false;
    }
    return !hiddenByOwnFlags(document);
}
bool ContainerManager::hiddenByOwnFlags(const ContainerDocument &document) const
{
    return !document.visible || !document.config.value("enabled").toBool(true) ||
           document.config.value("hiddenByMacro").toBool(false) ||
           !document.config.value(m_transmitting ? "showOnTx" : "showOnRx").toBool(true);
}
void ContainerManager::setTransmitting(bool transmitting)
{
    m_transmitting = transmitting;
    // Items that change on transmit (fade cover, VFO display, button grids)
    // hear it from here; the radio has no other subscription for them.
    for (ContainerWidget* c : std::as_const(m_containers)) {
        if (!c || !c->content()) {
            continue;
        }
        QList<MeterWidget*> meters;
        if (auto* direct = qobject_cast<MeterWidget*>(c->content())) {
            meters.append(direct);
        }
        meters += c->content()->findChildren<MeterWidget*>();
        for (MeterWidget* meter : std::as_const(meters)) {
            meter->setItemTxState(transmitting);
        }
    }
    if (!m_store) {
        return;
    }
    for (const auto &document : m_store->snapshot().containers) {
        if (auto *c = container(document.id)) {
            (c->isFloating() ? c->window() : c)->setVisible(effectiveVisible(document));
        }
    }
}
bool ContainerManager::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        // Bench fix: whatever shows a window (a reconcile, the end of a
        // transmit, Containers > Hidden, the main window restored), it is
        // shown where its header can be taken hold of.
        if (auto *form = qobject_cast<FloatingContainer *>(watched); form && m_floatingForms.value(form->id()) == form) {
            keepWindowReachable(form->id());
        }
    }
    if (watched == m_dockParent && !m_paneSettled
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        // Still on the way to its first settled size.
        m_paneSettle.start();
    }
    if (watched == m_dockParent && event->type() == QEvent::Resize) {
        // The pane has its size: free containers are shown from their
        // remembered places, for display only. Nothing is saved for it.
        m_dockParentResized = true;
        placeFreeContainers();
    }
    if (auto *handle = qobject_cast<QSplitterHandle *>(watched);
        handle && edgeMode() && handle->splitter() != m_splitter &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove ||
         event->type() == QEvent::MouseButtonRelease)) {
        // Workspace preview: a strip's handle, and the handle beside a
        // strip, always resize. The look has no Lock setting.
        return false;
    }
    if (qobject_cast<QSplitterHandle *>(watched) &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove ||
         event->type() == QEvent::MouseButtonRelease)) {
        for (auto *c : m_containers) {
            if (c->isPanelDocked() && c->isLocked()) {
                return true;
            }
        }
    }
    if (auto *c = qobject_cast<ContainerWidget *>(watched); c && c->geometryInteractionActive()) {
        return false;
    }
    if (auto *form = qobject_cast<FloatingContainer *>(watched)) {
        if (auto *c = container(form->id()); c && c->geometryInteractionActive()) {
            return false;
        }
    }
    if (m_store && !m_reconciling && !m_geometryCaptureSuppressed && m_storageError.isEmpty()
        && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        if (qobject_cast<ContainerWidget*>(watched) || qobject_cast<FloatingContainer*>(watched)) {
            // A move or resize outside a drag is the app's own (restore,
            // clamping, layout): no remembered place changes for it. The
            // capture still runs for what it saves besides places (splitter
            // sizes, strip thickness and extents, the container flags).
            m_geometryCommit.start();
        }
    }
    return QObject::eventFilter(watched, event);
}
void ContainerManager::reconcileWorkspace(const WorkspaceDocument& document)
{
    // Final geometry/config commits remain durable without showing closed
    // forms again. This also covers aboutToQuit saves after Qt 6.4's Quit scope.
    if (GuiApplication::applicationQuitInProgress()) { return; }
    if (!m_store || !m_registry || m_reconciling) { return; }
    // Refuse draft/preview state. Dependent controllers commit through the store.
    if (document != m_store->snapshot()) { return; }
    if (!lockApplies() || !revealApplies()) {
        // A document saved locked, or with a Reveal header (by an older
        // build, or seeded from the look-off workspace), is saved unlocked
        // and with that header Hidden before anything is shown. That commit
        // reconciles the corrected document, so this call is done.
        WorkspaceDocument unlocked = document;
        const bool cleared = !lockApplies() && clearLocks(unlocked);
        const bool hidden = !revealApplies() && hideRevealHeaders(unlocked);
        if ((cleared || hidden)
            && m_store->commit(unlocked, unlocked.revision).status == CommitStatus::Saved) {
            return;
        }
    }
    QScopedValueRollback<bool> guard(m_reconciling, true);
    m_geometryCommit.stop();
    QHash<QString, QString> placements;
    QSet<QString> ids;
    for (const auto& d : document.containers) {
        ids.insert(d.id);
        for (const auto& e : d.contents) {
            if (e.typeId.startsWith("applet:") && !placements.contains(e.typeId)) { placements[e.typeId] = e.id; }
        }
    }
    m_registry->setSingletonPlacements(placements);
    if (!m_edgeDragId.isEmpty()) {
        // An edge drag ends with its container: gone, or moved out of the
        // mode it was dragged in, the drag and its target bands stop.
        const auto dragged = std::find_if(document.containers.cbegin(), document.containers.cend(),
            [this](const ContainerDocument& value) { return value.id == m_edgeDragId; });
        if (dragged == document.containers.cend() || dragged->dockMode != m_edgeDragMode) {
            m_edgeDragId.clear();
            if (m_edgeArea) {
                m_edgeArea->clearDropPreview();
                m_edgeArea->setTargetBandsVisible(arranging());
            }
        }
    }
    QSet<QString> changed;
    for (const auto& d : document.containers) {
        if (!contentHost(d.id) || contentHost(d.id)->needsReconcile(d) || (container(d.id) && container(d.id)->dockMode()!=d.dockMode)) { changed.insert(d.id); }
    }
    // Release all singleton trees before removing a host or moving between hosts.
    for (auto* c : m_containers) {
        if (auto* host = contentHost(c->id())) {
            if (changed.contains(c->id()) || !ids.contains(c->id())) { host->releaseViews(); }
        }
    }
    for (const QString& id : m_containers.keys()) {
        if (!ids.contains(id)) { destroyContainer(id); }
    }
    m_pendingGeometry.intersect(ids);
    m_dropSettled.intersect(ids);
    for (const QString& id : m_documentGeometry.keys()) {
        if (!ids.contains(id)) {
            m_documentGeometry.remove(id);
            m_rememberedGeometry.remove(id);
            m_rememberedPane.remove(id);
            m_documentPane.remove(id);
            m_documentScreen.remove(id);
            m_documentSizing.remove(id);
            m_interactionStart.remove(id);
            m_provisionalPane.remove(id);
            m_rememberedScreen.remove(id);
            m_displaced.remove(id);
        }
    }
    m_panelContainerId = document.mainContainerId;
    for (const auto& d : document.containers) {
        auto* c = container(d.id);
        const bool fresh = !c;
        // Runtime content refresh and unrelated document edits must not replay
        // stale placement over an uncommitted move. Explicit placement/mode
        // edits remain authoritative, and no storage commit occurs here.
        const bool preserveGeometry = c && m_storageError.isEmpty() && c->dockMode() == d.dockMode
            && (m_pendingGeometry.contains(d.id) || c->geometryInteractionActive())
            && m_documentGeometry.contains(d.id) && m_documentGeometry.value(d.id) == d.geometry;
        // A window is put at its document place only when this commit
        // places it: the document's place or monitor changed, or the place
        // differs from the one remembered since (an operator move that was
        // not saved). A commit of something else leaves a window where it
        // is, so one the system moved with no drag does not jump back.
        const bool windowPlaced = fresh || c->dockMode() != d.dockMode
            || !m_documentGeometry.contains(d.id) || m_documentGeometry.value(d.id) != d.geometry
            || m_documentScreen.value(d.id) != d.screen
            || m_rememberedGeometry.value(d.id, d.geometry) != d.geometry;
        // Its size comes from the document then too, and when the commit
        // changed something else that sizes a window with its geometry
        // left alone (auto height switched, its content, layout or header).
        const bool windowSized = windowPlaced || !m_documentSizing.contains(d.id)
            || sizesWindowDifferently(m_documentSizing.value(d.id), d);
        // The place the operator's own drop or resize just committed: the
        // window is already there. It is theirs however little of it is on
        // a screen, so it is not judged as a place the app restores.
        const FloatingContainer* shownForm = c && c->isFloating() ? m_floatingForms.value(d.id) : nullptr;
        const bool atDocumentPlace = shownForm && d.dockMode == DockMode::Floating
            && shownForm->pos() == d.geometry.topLeft();
        if (preserveGeometry) {
            m_pendingGeometry.insert(d.id);
        } else {
            m_pendingGeometry.remove(d.id);
            // The document's place is the remembered one. A free container's
            // baseline pane follows the document when its placement changed;
            // an unrelated commit keeps the baseline already in use.
            const bool placementChanged = fresh || c->dockMode() != d.dockMode
                || !m_documentGeometry.contains(d.id) || m_documentGeometry.value(d.id) != d.geometry
                || m_documentPane.value(d.id) != d.geometryPane;
            m_rememberedGeometry[d.id] = d.geometry;
            if (d.dockMode != DockMode::OverlayDocked) {
                m_rememberedPane.remove(d.id);
                m_provisionalPane.remove(d.id);
            } else if (placementChanged) {
                m_provisionalPane.remove(d.id);
                if (d.geometryPane) {
                    m_rememberedPane[d.id] = *d.geometryPane;
                } else {
                    m_rememberedPane.remove(d.id);
                }
            }
            // A window's monitor is remembered with its place.
            if (d.dockMode == DockMode::Floating) {
                m_rememberedScreen[d.id] = d.screen;
            } else {
                m_rememberedScreen.remove(d.id);
            }
        }
        if (d.dockMode != DockMode::Floating) {
            m_displaced.remove(d.id);
        }
        if (!c) {
            c = new ContainerWidget(); c->setId(d.id);
            c->setRxSource(d.config.value("rxSource").toInt(1));
            auto* form = new FloatingContainer(c->rxSource());
            form->setScreenSource(m_screens);
            form->setProperty("structuredWorkspace", true); form->setId(d.id);
            form->installEventFilter(this); c->installEventFilter(this);
            m_containers[d.id] = c; m_floatingForms[d.id] = form;
            wireContainer(c);
            connect(form, &FloatingContainer::aboutToClose, this, [this, id = d.id] {
                const auto result = m_arrange->closeContainer(id);
                if (!result.ok) {
                    emit workspaceError(result.error);
                }
            });
            connect(c, &ContainerWidget::returnContainerRequested, this, [this, id = d.id] {
                const auto result = m_arrange->closeContainer(id);
                if (!result.ok) {
                    emit workspaceError(result.error);
                }
            });
            connect(c, &ContainerWidget::hideContainerRequested, this,
                    [this, id = d.id] { setContainerVisible(id, false); });
            // Workspace preview: the header's pin and the menu's Pin on
            // Top. Saved at once, so no later reconcile undoes the choice.
            connect(c, &ContainerWidget::pinOnTopRequested, this, [this, id = d.id](bool pin) {
                auto draft = m_store->snapshot();
                bool saved = pin;
                for (auto& value : draft.containers) {
                    if (value.id == id) {
                        saved = value.config.value("pinOnTop").toBool(false);
                        value.config["pinOnTop"] = pin;
                    }
                }
                if (saved != pin && commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
                    // Refused: the window shows what is saved.
                    if (ContainerWidget* view = container(id)) {
                        view->setPinOnTop(saved);
                    }
                }
            });
            connect(c, &ContainerWidget::headerModeRequested, this,
                    [this, id = d.id](HeaderMode mode) {
                        auto draft = m_store->snapshot();
                        for (auto &value : draft.containers) {
                            if (value.id == id) {
                                value.header = mode;
                            }
                        }
                        commitWorkspace(draft, draft.revision);
                    });
            // Workspace preview: the header's Arrange toggle.
            connect(c, &ContainerWidget::arrangeWorkspaceRequested, this, &ContainerManager::setArrangeMode);
            // Workspace preview: the header's source menu. The same config
            // write as Container Settings' Slice combo, committed as a draft;
            // nothing tunes.
            c->setSliceAvailability(m_sliceAvailable);
            connect(c, &ContainerWidget::sourceRequested, this, [this, id = d.id](int rxSource) {
                commitContainerSource(id, rxSource);
            });
            // Workspace preview: edges, free placement and the edge drag.
            connect(c, &ContainerWidget::moveToEdgeRequested, this,
                    [this, id = d.id](DockEdge edge) { moveContainerToEdge(id, edge); });
            connect(c, &ContainerWidget::placeFreeRequested, this,
                    [this, id = d.id] { placeContainerFree(id); });
            connect(c, &ContainerWidget::edgeDragStarted, this, [this, id = d.id] {
                if (edgeMode()) {
                    m_edgeDragId = id;
                    m_edgeDragArranging = arranging();
                    if (auto* dragged = container(id)) {
                        m_edgeDragMode = dragged->dockMode();
                    }
                    m_edgeArea->setTargetBandsVisible(true);
                }
            });
            connect(c, &ContainerWidget::edgeDragMoved, this, [this, id = d.id](const QPoint& global) {
                if (!edgeMode() || m_edgeDragId != id) {
                    return;
                }
                const auto target = m_edgeArea->dropTargetAt(global);
                if (target.valid) {
                    m_edgeArea->showDropPreview(target.edge, target.index);
                } else {
                    m_edgeArea->clearDropPreview();
                }
            });
            connect(c, &ContainerWidget::edgeDragFinished, this, [this, id = d.id](const QPoint& global) {
                finishEdgeDrag(id, global);
                emit containerDragFinished();
            });
            connect(c, &ContainerWidget::edgeDragCancelled, this, [this] {
                m_edgeDragId.clear();
                if (m_edgeArea) {
                    m_edgeArea->clearDropPreview();
                    m_edgeArea->setTargetBandsVisible(arranging());
                }
                emit containerDragFinished();
            });
        }
        auto* host = contentHost(d.id);
        const bool reparent = fresh || c->dockMode() != d.dockMode;
        if (host && reparent) { host->releaseViews(); }
        auto* form = m_floatingForms.value(d.id);
        if (reparent) {
            c->hide(); form->hide(); form->setContainerFloating(d.dockMode == DockMode::Floating);
            if (d.dockMode == DockMode::Floating) { form->takeOwner(c); }
            else if (d.dockMode == DockMode::PanelDocked && edgeMode()) {
                // Workspace preview: its edge's strip; layoutEdgeStrips orders it.
                m_edgeArea->strip(d.edge)->splitter()->addWidget(c);
            }
            else if (d.dockMode == DockMode::PanelDocked) { c->setParent(m_splitter); m_splitter->addWidget(c); }
            else { c->setParent(m_dockParent); }
        }
        c->setDockMode(d.dockMode); c->setNotes(d.name); c->setRxSource(d.config.value("rxSource").toInt(1));
        c->setAxisLock(d.anchor); c->setLocked(lockApplies() && d.locked); c->setAutoHeight(d.autoHeight);
        c->setPopOutShell(d.popOutShell);
        c->setHeaderMode(d.header);
        if (WorkspacePreview::enabled()) {
            applyWorkspaceHeader(c, d);
            c->setDockEdge(d.edge);
            // The one drop rule needs the edge area to judge a release.
            c->setArrangeDropRule(edgeMode());
            c->setGridAlignment(document.arrangement.gridAlignment ? kWorkspaceSnapGridPx : 0);
        }
        c->setContainerEnabled(d.config.value("enabled").toBool(true));
        c->setShowOnRx(d.config.value("showOnRx").toBool(true)); c->setShowOnTx(d.config.value("showOnTx").toBool(true));
        c->setContainerMinimises(d.config.value("containerMinimises").toBool(false));
        c->setContainerHidesWhenRxNotUsed(d.config.value("hidesWhenRxNotUsed").toBool(false));
        c->setBackgroundColor(QColor(d.config.value("backgroundColor").toString("#0f0f1a")));
        c->setBorder(d.config.value("border").toBool(true)); c->setNoControls(d.config.value("noControls").toBool(false));
        c->setPinOnTop(d.config.value("pinOnTop").toBool(false));
        if (fresh && d.dockMode==DockMode::Floating && !d.geometry.isValid()) {
            const QByteArray geometry=QByteArray::fromHex(d.config.value("legacyAppletFloatGeometry").toString().toLatin1());
            if (!geometry.isEmpty()) {
                form->QWidget::restoreGeometry(geometry);
                // An imported float window keeps the place it had while that
                // is on a screen; it is not a remembered place.
                if (form->pos() != QPoint(0, 0) && m_screens->onAnyScreen(form->geometry())) {
                    m_displaced.insert(d.id);
                }
            }
        }
        if (!preserveGeometry) {
            // A window takes its size here and its place below, once its
            // height is known; a free or docked container takes its rect.
            if (d.geometry.isValid()) {
                if (d.dockMode == DockMode::Floating) {
                    if (windowSized) {
                        form->resize(form->fittedSize(d.geometry.size()));
                    }
                } else {
                    c->setGeometry(d.geometry);
                }
            }
        }
        if (!host) {
            host = new ContainerContentHost(*m_registry, c);
            connect(host, &ContainerContentHost::meterSurfaceReady, this, [this, c](MeterWidget* meter, const QJsonObject& context) {
                // Structured items route through Main's per-entry wiring; never
                // also install the legacy container-source forwarding signals.
                Q_UNUSED(c);
                emit meterContextReady(meter, context);
                emit meterReadyForPolling(meter);
            });
            connect(host, &ContainerContentHost::reconciled, this, &ContainerManager::workspaceReconciled);
            // The container is never narrower than its content (JJ,
            // 2026-10-08). Queued: a split set while the window is built
            // (MainWindow's first-run setSizes({1024, 256}), a restored
            // size) is set against the 260 px container floor as it always
            // was, and only then raised, so the Main Panel keeps today's
            // width wherever that already fits and takes its content's
            // minimum where it does not. The raise is the content's need,
            // not a user resize: the splitter applies it now, inside the
            // reconcile guard, and no geometry is captured for it.
            connect(host, &ContainerContentHost::contentMinimumWidthChanged, this,
                    [this, view = QPointer<ContainerWidget>(c), content = QPointer<ContainerContentHost>(host)] {
                if (!view || !content || view->content() != content.data()) {
                    return;
                }
                QScopedValueRollback<bool> guard(m_reconciling, true);
                view->setContentMinimumWidth(content->contentMinimumWidth());
                if (m_splitter && view->parentWidget() == m_splitter) {
                    m_splitter->refresh();
                }
                if (view->isPanelDocked()) {
                    // Workspace preview: the edge strip holding it.
                    scheduleEdgeStripContentHints();
                }
            }, Qt::QueuedConnection);
            connect(host, &ContainerContentHost::preferredContentHeightChanged, this,
                    [this, id = d.id, content = QPointer<ContainerContentHost>(host),
                     view = QPointer<ContainerWidget>(c)] {
                if (!m_store || m_reconciling || GuiApplication::applicationQuitInProgress()
                    || !content || !view || container(id) != view.data()
                    || contentHost(id) != content.data()) {
                    return;
                }
                followContentHeight(id);
            }, Qt::QueuedConnection);
            c->setContent(host);
        }
        host->setArrangeController(m_arrange);
        // Workspace preview: entry headers follow the container's header mode
        // and the app-level Arrange (both inert with the switch off).
        host->setContainerHeaderMode(d.header);
        host->setArrangeActive(arranging());
        host->reconcile(d);
        if (d.autoHeight && d.dockMode != DockMode::PanelDocked) {
            QWidget *target = d.dockMode == DockMode::Floating ? static_cast<QWidget *>(form) : c;
            int height=host->preferredContentHeight()+c->headerExtent();
            if (const int available = availableHeightFor(target); d.layout == ContentLayout::FreeCanvas && available > 0) {
                height = qMin(height, available);
            }
            QSize wanted(qMax(target->width(), c->minimumSizeHint().width()), height);
            const bool window = d.dockMode == DockMode::Floating;
            if (window) {
                // Never larger than all the screens together.
                wanted = form->fittedSize(wanted);
            }
            // A drag moves a window and never resizes it; the end of the
            // drag gives it its content's height (followContentHeight).
            if (!window || !c->dragActive()) {
                target->resize(wanted);
            }
        }
        if (!d.autoHeight && d.dockMode == DockMode::Floating && c->operatorSizesHeight()
            && form->height() < c->naturalHeight() && !c->dragActive()) {
            // Workspace preview: the operator's height, but never less than
            // the content needs (nor more than all the screens have).
            form->resize(form->fittedSize(QSize(form->width(), c->naturalHeight())));
        }
        if (d.dockMode == DockMode::Floating && !preserveGeometry) {
            // At its remembered place when its monitor is here; otherwise on
            // the main window's screen, and nothing is remembered for that.
            // A window already shown away from its place stays judged as
            // one the app is restoring.
            const bool restoring = m_displaced.contains(d.id) || (windowPlaced && !atDocumentPlace);
            placeFloatingWindow(d.id, d.geometry, d.screen, form, windowPlaced, restoring);
        }
        // A free container is shown by placeFreeContainers() below, from its
        // remembered place; nothing is clamped into that place here.
        if (d.dockMode == DockMode::Floating) {
            c->show();
            form->setVisible(effectiveVisible(d));
        } else {
            c->setVisible(effectiveVisible(d));
        }
        if (fresh) { emit containerAdded(d.id); }
        m_documentGeometry[d.id] = d.geometry;
        m_documentPane[d.id] = d.geometryPane;
        m_documentScreen[d.id] = d.screen;
        m_documentSizing[d.id] = d;
    }
    if (edgeMode()) {
        layoutEdgeStrips(document);
    }
    placeFreeContainers();
    if (m_splitter) {
        for (auto *handle : m_splitter->findChildren<QSplitterHandle *>()) {
            handle->installEventFilter(this);
        }
    }
    if (edgeMode()) {
        // installEventFilter ignores a filter that is already installed.
        for (auto *handle : m_edgeArea->findChildren<QSplitterHandle *>()) {
            handle->installEventFilter(this);
        }
    }
    const bool interactionActive = std::any_of(m_containers.cbegin(), m_containers.cend(),
        [](ContainerWidget* c) { return c->geometryInteractionActive(); });
    if (!m_pendingGeometry.isEmpty() && !interactionActive && m_storageError.isEmpty()) {
        m_geometryCommit.start();
    }
    emit workspaceReconciled();
}
void ContainerManager::setArrangeMode(bool on)
{
    if (m_arrangeMode == on) {
        return;
    }
    // Arrange is runtime only, never saved.
    m_arrangeMode = on;
    for (ContainerWidget* c : std::as_const(m_containers)) {
        if (c) {
            c->setArrangeSwitchOn(on);
        }
    }
    applyArranging();
    emit arrangeModeChanged(on);
}

void ContainerManager::setArrangeHeld(bool held)
{
    if (m_arrangeHeld == held) {
        return;
    }
    // The Shift hold: independent of the switch, and never saved.
    m_arrangeHeld = held;
    applyArranging();
}

void ContainerManager::applyArranging()
{
    const bool on = arranging();
    if (m_arrangingApplied == on) {
        return;
    }
    m_arrangingApplied = on;
    for (auto it = m_containers.cbegin(); it != m_containers.cend(); ++it) {
        if (!it.value()) {
            continue;
        }
        it.value()->setArrangeActive(on);
        if (auto *host = contentHost(it.key())) {
            host->setArrangeActive(on);
        }
    }
    // The edge target bands show through Arrange and while dragging.
    if (edgeMode()) {
        m_edgeArea->setTargetBandsVisible(on || !m_edgeDragId.isEmpty());
    }
}

bool ContainerManager::containerDragActive() const
{
    if (!m_edgeDragId.isEmpty()) {
        return true;
    }
    return std::any_of(m_containers.cbegin(), m_containers.cend(), [](ContainerWidget* c) {
        return c && (c->geometryInteractionActive() || c->edgeDragActive());
    });
}

// ---- Workspace preview: four-edge strips ----------------------------------

bool ContainerManager::edgeMode() const
{
    return m_edgeArea && WorkspacePreview::enabled();
}

void ContainerManager::setEdgeDockArea(EdgeDockArea* area)
{
    if (m_edgeArea) {
        disconnect(m_edgeArea, nullptr, this, nullptr);
    }
    m_edgeArea = area;
    if (!area) {
        return;
    }
    // A dragged strip edge or extent is a geometry edit like any other.
    connect(area, &EdgeDockArea::thicknessChanged, this, [this] {
        if (m_store && !m_reconciling) {
            m_geometryCommit.start();
        }
    });
}

EdgeDockArea* ContainerManager::edgeDockArea() const
{
    return m_edgeArea;
}

void ContainerManager::applyEdgeStripContentHints(bool sideStripsOnly)
{
    if (!m_edgeArea) {
        return;
    }
    const DockEdge edges[] = {DockEdge::Left, DockEdge::Right, DockEdge::Top, DockEdge::Bottom};
    // What each strip's containers need across it. The header counts only
    // where it takes room (Shown); Reveal and Hidden reserve none.
    for (DockEdge edge : edges) {
        if (sideStripsOnly && EdgeDockStrip::orientationFor(edge) != Qt::Vertical) {
            // An end strip's preferred height follows its own signal
            // (preferredContentHeightChanged).
            continue;
        }
        int minimumCross = 0;
        int preferredCross = 0;
        for (QWidget* w : m_edgeArea->strip(edge)->containers()) {
            auto* c = qobject_cast<ContainerWidget*>(w);
            if (c == nullptr || c->isHidden()) {
                continue;
            }
            if (EdgeDockStrip::orientationFor(edge) == Qt::Vertical) {
                // The container's own minimum, and its scrolling content's:
                // a scroll area reports a small minimum of its own, so the
                // content's width is read from the scrolled body.
                int widest = qMax(c->minimumWidth(), c->minimumSizeHint().width());
                if (auto* scroll = c->contentHolder()->findChild<QScrollArea*>();
                    scroll != nullptr && scroll->widget() != nullptr) {
                    widest = qMax(widest, scroll->widget()->minimumSizeHint().width());
                }
                minimumCross = qMax(minimumCross, widest);
            } else if (auto* host = contentHost(c->id())) {
                preferredCross = qMax(preferredCross,
                                      host->preferredContentHeight() + c->headerExtent());
            }
        }
        m_edgeArea->setStripContentHint(edge, minimumCross, preferredCross);
    }
}

void ContainerManager::scheduleEdgeStripContentHints()
{
    // A container's content minimum changed with no workspace change (an
    // applet showed wider controls): the side strip's floor follows at once.
    // Coalesced, so a burst of changes lays the strips out once.
    if (!m_edgeArea || m_edgeHintsPending) {
        return;
    }
    m_edgeHintsPending = true;
    QTimer::singleShot(0, this, [this] {
        m_edgeHintsPending = false;
        if (!m_edgeArea) {
            return;
        }
        // The content's need, not a user resize: nothing is captured.
        QScopedValueRollback<bool> guard(m_reconciling, true);
        applyEdgeStripContentHints(true);
        m_edgeArea->refreshLayout();
    });
}

void ContainerManager::layoutEdgeStrips(const WorkspaceDocument& document)
{
    // Document order is strip order.
    QMap<DockEdge, QList<QWidget*>> widgets;
    QMap<DockEdge, QStringList> ids;
    for (const auto& d : document.containers) {
        if (d.dockMode != DockMode::PanelDocked) {
            continue;
        }
        if (auto* c = container(d.id)) {
            widgets[d.edge].append(c);
            ids[d.edge].append(d.id);
        }
    }
    const DockEdge edges[] = {DockEdge::Left, DockEdge::Right, DockEdge::Top, DockEdge::Bottom};
    for (DockEdge edge : edges) {
        m_edgeArea->strip(edge)->setContainers(widgets.value(edge));
        const int thickness = document.arrangement.strips.value(edge).thickness;
        if (thickness > 0) {
            m_edgeArea->setThickness(edge, thickness);
        } else if (!m_geometryCommit.isActive()) {
            // A document with no thickness is the default rule, so another
            // profile's dragged thickness is not carried in. A drag still
            // waiting for its geometry commit is kept.
            m_edgeArea->clearThickness(edge);
        }
    }
    // Migration: the right strip takes the Main Panel's saved width, plus
    // the strip's padding (none on the right) so the panel keeps that width.
    if (!m_edgeArea->hasThickness(DockEdge::Right)) {
        const QJsonArray saved = document.extensions.value("splitterSizes").toArray();
        if (saved.size() == 2 && saved.at(1).toInt() > 0) {
            m_edgeArea->setThickness(DockEdge::Right,
                                     saved.at(1).toInt() + 2 * EdgeDockStrip::paddingFor(DockEdge::Right));
        }
    }
    applyEdgeStripContentHints(false);
    m_edgeArea->refreshLayout();
    for (DockEdge edge : edges) {
        const QStringList strip = ids.value(edge);
        const auto saved = document.arrangement.strips.value(edge).extents;
        if (strip.size() < 2) {
            continue;
        }
        // Saved sizes for the containers the strip knows; a container new
        // to the strip (moved in since the last capture) gets the mean
        // known share, so the known split keeps its ratio.
        QList<int> extents;
        int knownTotal = 0;
        int knownCount = 0;
        bool anyMissing = false;
        for (const QString& id : strip) {
            if (saved.contains(id)) {
                const int size = saved.value(id);
                extents.append(size);
                if (size > 0) {
                    knownTotal += size;
                    ++knownCount;
                }
            } else {
                extents.append(-1);
                anyMissing = true;
            }
        }
        if (anyMissing && knownCount > 0) {
            const int share = knownTotal / knownCount;
            for (int& size : extents) {
                if (size < 0) {
                    size = share;
                }
            }
        }
        if (!anyMissing || knownCount > 0) {
            m_edgeArea->strip(edge)->setExtents(extents);
        } else if (!m_geometryCommit.isActive()) {
            // No saved extents for these containers: the default split,
            // not the previous arrangement's.
            m_edgeArea->strip(edge)->resetExtents();
        }
    }
}

void ContainerManager::captureArrangement(WorkspaceDocument& document) const
{
    WorkspaceArrangement& arrangement = document.arrangement;
    const DockEdge edges[] = {DockEdge::Left, DockEdge::Right, DockEdge::Top, DockEdge::Bottom};
    for (DockEdge edge : edges) {
        EdgeStripLayout layout = arrangement.strips.value(edge);
        if (m_edgeArea->hasThickness(edge)) {
            layout.thickness = m_edgeArea->thickness(edge);
        }
        EdgeDockStrip* strip = m_edgeArea->strip(edge);
        const QList<int> sizes = strip->extents();
        const QList<QWidget*> widgets = strip->containers();
        int total = 0;
        for (int size : sizes) {
            total += size;
        }
        // Extents are read only from a laid-out strip.
        if (strip->isVisible() && total > 0) {
            layout.extents.clear();
            for (int i = 0; i < widgets.size() && i < sizes.size(); ++i) {
                auto* c = qobject_cast<ContainerWidget*>(widgets[i]);
                if (c && !c->isHidden()) {
                    layout.extents.insert(c->id(), sizes[i]);
                }
            }
        }
        // A hidden strip keeps its saved extents; ids of containers that
        // no longer exist are dropped from them.
        for (auto it = layout.extents.begin(); it != layout.extents.end();) {
            const bool exists = std::any_of(document.containers.cbegin(), document.containers.cend(),
                [&it](const ContainerDocument& value) { return value.id == it.key(); });
            it = exists ? std::next(it) : layout.extents.erase(it);
        }
        if (layout.thickness > 0 || !layout.extents.isEmpty() || !layout.extensions.isEmpty()) {
            arrangement.strips.insert(edge, layout);
        } else {
            arrangement.strips.remove(edge);
        }
    }
}

void ContainerManager::moveContainerToEdge(const QString& id, DockEdge edge, int index)
{
    if (!m_store || m_reconciling) {
        return;
    }
    WorkspaceDocument draft = m_store->snapshot();
    const auto indexOfId = [&draft](const QString& value) {
        for (int i = 0; i < draft.containers.size(); ++i) {
            if (draft.containers[i].id == value) {
                return i;
            }
        }
        return -1;
    };
    const int position = indexOfId(id);
    if (position < 0) {
        return;
    }
    if (lockApplies() && draft.containers[position].locked) {
        emit workspaceError(tr("Arrangement is locked"));
        return;
    }
    QStringList order;
    for (const auto& d : draft.containers) {
        if (d.dockMode == DockMode::PanelDocked && d.edge == edge) {
            order.append(d.id);
        }
    }
    int target = index < 0 ? order.size() : qBound(0, index, int(order.size()));
    const int self = order.indexOf(id);
    if (self >= 0) {
        if (self < target) {
            --target;
        }
        order.removeAt(self);
    }
    ContainerDocument moved = draft.containers.takeAt(position);
    moved.dockMode = DockMode::PanelDocked;
    moved.edge = edge;
    int insertAt = draft.containers.size();
    if (target < order.size()) {
        insertAt = indexOfId(order[target]);
    } else if (!order.isEmpty()) {
        insertAt = indexOfId(order.last()) + 1;
    }
    draft.containers.insert(insertAt, moved);
    if (draft != m_store->snapshot()) {
        commitWorkspace(draft, draft.revision);
    }
}

void ContainerManager::placeContainerFree(const QString& id)
{
    if (!m_store || m_reconciling) {
        return;
    }
    WorkspaceDocument draft = m_store->snapshot();
    for (auto& d : draft.containers) {
        if (d.id != id) {
            continue;
        }
        if (lockApplies() && d.locked) {
            emit workspaceError(tr("Arrangement is locked"));
            return;
        }
        // Centred over the spectrum pane, at its strip width and its
        // content's height (a strip stretches it), where it fits.
        QSize size = freePlacementSize(id, d);
        if (m_dockParent) {
            size = size.boundedTo(m_dockParent->size());
        }
        const QPoint origin = m_dockParent
            ? QPoint(qMax(0, (m_dockParent->width() - size.width()) / 2),
                     qMax(0, (m_dockParent->height() - size.height()) / 2))
            : QPoint();
        d.dockMode = DockMode::OverlayDocked;
        // Off any spot another free container already has.
        d.geometry = defaultFreeRect(QRect(origin, size), id);
        // Placed for the pane as it is now.
        d.geometryPane.reset();
        d.screen.reset();
        commitWorkspace(draft, draft.revision);
        return;
    }
}

QSize ContainerManager::freePlacementSize(const QString& id, const ContainerDocument& d) const
{
    constexpr QSize kFallback(360, 300);
    constexpr int kMinimumHeight = 120;
    ContainerWidget* c = container(id);
    if (c == nullptr || c->width() <= 0) {
        return d.geometry.isValid() ? d.geometry.size() : kFallback;
    }
    if (d.dockMode != DockMode::PanelDocked) {
        return c->size();
    }
    int height = c->height();
    if (auto* host = contentHost(id)) {
        height = qMin(height, qMax(kMinimumHeight,
                                   host->preferredContentHeight() + WorkspaceGroupHeader::kHeight));
    }
    // A top or bottom strip spans the window; free, it takes a usual width.
    constexpr int kEndStripFreeWidth = 640;
    const bool endStrip = d.edge == DockEdge::Top || d.edge == DockEdge::Bottom;
    return QSize(endStrip ? qMin(c->width(), kEndStripFreeWidth) : c->width(), height);
}

void ContainerManager::setGridAlignment(bool on)
{
    if (!m_store || m_reconciling || gridAlignment() == on) {
        return;
    }
    WorkspaceDocument draft = m_store->snapshot();
    draft.arrangement.gridAlignment = on;
    if (commitWorkspace(draft, draft.revision).status == CommitStatus::Saved) {
        emit gridAlignmentChanged(on);
    }
}

bool ContainerManager::gridAlignment() const
{
    return m_store && m_store->snapshot().arrangement.gridAlignment;
}

void ContainerManager::showDropPreviewForTest(DockEdge edge, int index)
{
    if (!m_edgeArea) {
        return;
    }
    m_edgeArea->setTargetBandsVisible(true);
    m_edgeArea->showDropPreview(edge, index);
}

void ContainerManager::clearDropPreviewForTest()
{
    if (!m_edgeArea) {
        return;
    }
    m_edgeArea->clearDropPreview();
    m_edgeArea->setTargetBandsVisible(arranging());
}

void ContainerManager::finishEdgeDrag(const QString& id, const QPoint& globalPos)
{
    if (!edgeMode() || m_edgeDragId != id) {
        return;
    }
    const auto target = m_edgeArea->dropTargetAt(globalPos);
    // Only a strip container reports an edge drag when not arranging.
    const bool oneRule = m_edgeDragArranging || m_edgeDragMode != DockMode::PanelDocked;
    m_edgeDragId.clear();
    m_edgeArea->clearDropPreview();
    m_edgeArea->setTargetBandsVisible(arranging());
    if (oneRule) {
        finishArrangeDrop(id, globalPos);
        return;
    }
    if (target.valid) {
        moveContainerToEdge(id, target.edge, target.index);
        return;
    }
    if (!target.detach || !m_store) {
        // An invalid release restores: the container never left its strip.
        return;
    }
    // Pulled out of the main window: a floating shell under the pointer.
    WorkspaceDocument draft = m_store->snapshot();
    for (auto& d : draft.containers) {
        if (d.id != id) {
            continue;
        }
        const QSize size = freePlacementSize(id, d);
        d.dockMode = DockMode::Floating;
        d.geometry = QRect(globalPos - QPoint(size.width() / 2, WorkspaceGroupHeader::kHeight / 2), size);
        d.screen = screenForNewWindow(d.geometry);
        if (commitWorkspace(draft, draft.revision).status == CommitStatus::Saved) {
            keepNewWindowPlace(id);
        }
        return;
    }
}
QRect ContainerManager::paneRectWithout(const QString& id) const
{
    QRect pane(m_dockParent->mapToGlobal(QPoint(0, 0)), m_dockParent->size());
    const ContainerWidget* c = container(id);
    if (!edgeMode() || !c || !c->isPanelDocked()) {
        return pane;
    }
    const DockEdge edge = c->dockEdge();
    for (const auto& d : m_store->snapshot().containers) {
        if (d.id != id && d.dockMode == DockMode::PanelDocked && d.edge == edge && effectiveVisible(d)) {
            // The strip stays open for its other containers.
            return pane;
        }
    }
    const QRect area(m_edgeArea->mapToGlobal(QPoint(0, 0)), m_edgeArea->size());
    switch (edge) {
    case DockEdge::Left:
        pane.setLeft(area.left());
        break;
    case DockEdge::Right:
        pane.setRight(area.right());
        break;
    case DockEdge::Top:
        pane.setTop(area.top());
        break;
    case DockEdge::Bottom:
        pane.setBottom(area.bottom());
        break;
    }
    return pane;
}

QRect ContainerManager::freeDropRect(const QRect& wanted, const QSize& pane) const
{
    QRect rect(wanted.topLeft(), wanted.size().boundedTo(pane));
    const auto inside = [&pane, &rect](const QPoint& topLeft) {
        return QPoint(std::clamp(topLeft.x(), 0, std::max(0, pane.width() - rect.width())),
                      std::clamp(topLeft.y(), 0, std::max(0, pane.height() - rect.height())));
    };
    rect.moveTopLeft(inside(rect.topLeft()));
    if (gridAlignment() && !(QApplication::keyboardModifiers() & Qt::AltModifier)) {
        rect.moveTopLeft(inside(workspaceSnappedTopLeft(rect.topLeft(), kWorkspaceSnapGridPx)));
    }
    return rect;
}

void ContainerManager::finishArrangeDrop(const QString& id, const QPoint& globalPos)
{
    ContainerWidget* c = container(id);
    FloatingContainer* form = m_floatingForms.value(id);
    if (!c || !m_store || !m_dockParent) {
        return;
    }
    const EdgeDockArea::DropTarget target = m_edgeArea->dropTargetAt(globalPos);
    // A free or floating container followed the pointer; a strip container
    // stayed in its strip.
    const bool followed = !c->isPanelDocked();
    const QRect start = m_interactionStart.value(id);
    QWidget* shown = c->isFloating() && form ? static_cast<QWidget*>(form) : c;
    if (followed) {
        m_dropSettled.insert(id);
    }
    const quint64 revision = m_store->snapshot().revision;
    // The header sits under the pointer where it was grabbed.
    const QPoint grab = c->dragGrabOffset();
    const QRect pane(m_dockParent->mapToGlobal(QPoint(0, 0)), m_dockParent->size());
    const QWidget* mainWindow = m_edgeArea->window();
    const bool outside = target.detach && !mainWindow->frameGeometry().contains(globalPos);

    const auto commitPlace = [this, &id](DockMode mode, const QRect& geometry, const QSize& paneSize = QSize()) {
        WorkspaceDocument draft = m_store->snapshot();
        for (auto& d : draft.containers) {
            if (d.id != id) {
                continue;
            }
            d.dockMode = mode;
            d.geometry = geometry;
            d.geometryPane.reset();
            d.screen.reset();
            if (mode == DockMode::OverlayDocked && dockParentLaidOut()) {
                // Placed for the pane it lands in.
                d.geometryPane = paneSize.isValid() ? paneSize : m_dockParent->size();
            } else if (mode == DockMode::Floating) {
                if (const std::optional<ScreenInfo> on = m_screens->screenHolding(geometry)) {
                    d.screen = savedScreenOf(*on);
                }
            }
            commitWorkspace(draft, draft.revision);
            return;
        }
    };
    // Same dock mode as before: the operator's move, remembered and saved
    // now. A drop back on the start changes nothing.
    const auto keepMoved = [this, &id, &start, shown](const QPoint& topLeft) {
        if (shown->pos() != topLeft) {
            QScopedValueRollback<bool> guard(m_reconciling, true);
            shown->move(topLeft);
        }
        if (liveGeometry(id) != start) {
            rememberOperatorGeometry(id);
            saveState();
        }
    };
    const auto documentOf = [this, &id]() -> ContainerDocument {
        for (const auto& d : m_store->snapshot().containers) {
            if (d.id == id) {
                return d;
            }
        }
        return {};
    };

    if (target.valid) {
        moveContainerToEdge(id, target.edge, target.index);
    } else if (pane.contains(globalPos)) {
        if (c->isOverlayDocked()) {
            keepMoved(freeDropRect(c->geometry(), pane.size()).topLeft());
        } else if (c->isFloating()) {
            commitPlace(DockMode::OverlayDocked,
                        freeDropRect(QRect(m_dockParent->mapFromGlobal(shown->pos()), shown->size()), pane.size()));
        } else {
            // Its strip may close behind it: the place is for the pane as
            // it will be, so the container stays under the pointer.
            const QRect landing = paneRectWithout(id);
            const QSize size = freePlacementSize(id, documentOf());
            const QPoint offset(std::min(grab.x(), size.width() - 1), grab.y());
            commitPlace(DockMode::OverlayDocked,
                        freeDropRect(QRect(globalPos - offset - landing.topLeft(), size), landing.size()),
                        landing.size());
        }
    } else if (outside) {
        if (c->isFloating()) {
            // A window is only moved; the grid applies as it does to any
            // window move.
            QPoint topLeft = shown->pos();
            if (gridAlignment() && !(QApplication::keyboardModifiers() & Qt::AltModifier)) {
                topLeft = workspaceSnappedTopLeft(topLeft, kWorkspaceSnapGridPx);
            }
            keepMoved(topLeft);
        } else if (c->isOverlayDocked()) {
            commitPlace(DockMode::Floating, QRect(c->mapToGlobal(QPoint(0, 0)), c->size()));
        } else {
            const QSize size = freePlacementSize(id, documentOf());
            const QPoint offset(std::min(grab.x(), size.width() - 1), grab.y());
            commitPlace(DockMode::Floating, QRect(globalPos - offset, size));
        }
    }
    // Anywhere else inside the main window (its menu bar, status bar or
    // title bar) nothing can take the container: the drag is cancelled.
    if (followed && m_store->snapshot().revision == revision && liveGeometry(id) != start && start.isValid()) {
        // Cancelled, or nothing was saved: back where it started.
        QScopedValueRollback<bool> guard(m_reconciling, true);
        (c->isFloating() && form ? static_cast<QWidget*>(form) : c)->move(start.topLeft());
    }
}

void ContainerManager::applyWorkspaceHeader(ContainerWidget *c, const ContainerDocument &d)
{
    // A single entry's title is the header, so there is never a second bar.
    QString title = d.name;
    if (d.contents.size() == 1) {
        const auto &entry = d.contents.constFirst();
        title = entry.name;
        if (title.isEmpty() && m_registry) {
            for (const auto &descriptor : m_registry->descriptors()) {
                if (descriptor.typeId == entry.typeId) {
                    title = descriptor.title;
                }
            }
        }
        if (title.isEmpty()) {
            title = d.name;
        }
    }
    c->setWorkspaceTitle(title.isEmpty() ? d.id : title);
    // The source chip names a fixed slice; a container following the
    // selected receiver shows none.
    QString source;
    QColor colour;
    int menuRx = 0;
    if (d.config.value("rxSourceMode").toString() != QStringLiteral("followSelectedRx")
        && (d.config.contains("sliceId") || d.config.contains("rxSource"))) {
        const int slice = d.config.contains("sliceId") ? d.config.value("sliceId").toInt()
                                                       : d.config.value("rxSource").toInt(1) - 1;
        if (slice >= 0 && slice < 26) {
            source = QStringLiteral("RX %1").arg(QChar('A' + slice));
            colour = QColor::fromRgb(ControlRanges::sliceColour(slice));
            menuRx = slice + 1;
        }
    }
    // Workspace preview: a following container's chip reads Active, so the
    // source menu stays reachable.
    const bool follows = d.config.value("rxSourceMode").toString() == QStringLiteral("followSelectedRx");
    if (follows) {
        source = tr("Active");
        colour = QColor(Workspace::role("label-mid", Workspace::kLabelMid));
    }
    c->setFollowsSelectedRx(follows);
    c->setSourceMenuRx(menuRx);
    c->setWorkspaceSource(source, colour);
    c->setArrangeActive(arranging());
    c->setArrangeSwitchOn(m_arrangeMode);
}
void ContainerManager::setSliceAvailability(std::function<bool(int sliceId)> available)
{
    m_sliceAvailable = std::move(available);
    for (auto* c : std::as_const(m_containers)) {
        c->setSliceAvailability(m_sliceAvailable);
    }
}
bool ContainerManager::commitContainerSource(const QString& id, int rxSource)
{
    // No lock check: the source is not geometry, and the Container Settings
    // Source combo does not check the lock either.
    if (!m_store || m_reconciling) {
        return false;
    }
    WorkspaceDocument draft = m_store->snapshot();
    for (auto& d : draft.containers) {
        if (d.id != id) {
            continue;
        }
        if (rxSource == ContainerWidget::kActiveSliceSource) {
            d.config["rxSourceMode"] = QStringLiteral("followSelectedRx");
        } else {
            d.config.remove("rxSourceMode");
            ContainerSourceAdapter::writeContainerSource(d.config, rxSource);
        }
        return commitWorkspace(draft, draft.revision).status == CommitStatus::Saved;
    }
    return false;
}
bool ContainerManager::commitDockMode(const QString& id, DockMode mode)
{
    if (!m_store || m_reconciling) { return false; }
    WorkspaceDocument document = m_store->snapshot();
    for (auto &d : document.containers) {
        if (d.id == id) {
            if (lockApplies() && d.locked) {
                emit workspaceError(tr("Arrangement is locked"));
                return true;
            }
            if (mode == DockMode::Floating && d.dockMode == DockMode::PanelDocked && edgeMode()) {
                // Workspace preview: a strip container popped out keeps its
                // own size (its width, its content's height plus the
                // header), centred on the main window. The app has no
                // pop-out default size to use instead.
                const QSize size = freePlacementSize(id, d);
                const QRect anchor = m_edgeArea->window()->geometry();
                d.geometry = defaultWindowRect(
                    QRect(anchor.center() - QPoint(size.width() / 2, size.height() / 2), size));
            } else if (d.dockMode == DockMode::PanelDocked && mode != DockMode::PanelDocked) {
                // A docked container's rect is laid out, not remembered: it
                // leaves the panel at the size it has now, as it always did.
                if (ContainerWidget* docked = container(id); docked && docked->geometry().isValid()) {
                    d.geometry = docked->geometry();
                }
            }
            if (mode == DockMode::OverlayDocked && d.dockMode == DockMode::Floating && dockParentLaidOut()) {
                // A window's rect is in screen coordinates. Made free, the
                // container's place is the rect it lands at inside the pane.
                const QSize size = d.geometry.isValid() ? d.geometry.size() : kPlacementFallbackSize;
                const QRect landing = d.geometry.isValid()
                    ? d.geometry
                    : QRect(QPoint((m_dockParent->width() - size.width()) / 2,
                                   (m_dockParent->height() - size.height()) / 2), size);
                d.geometry = defaultFreeRect(landing, id);
            }
            if (d.dockMode != mode) {
                // Its place is for the pane as it is now.
                d.geometryPane.reset();
                d.screen.reset();
                if (mode == DockMode::Floating) {
                    // The place it is given now is its remembered place.
                    d.screen = screenForNewWindow(d.geometry);
                }
            }
            const bool becomesWindow = mode == DockMode::Floating && d.dockMode != mode;
            d.dockMode = mode;
            if (commitWorkspace(document, document.revision).status == CommitStatus::Saved && becomesWindow) {
                keepNewWindowPlace(id);
            }
            return true;
        }
    }
    return true;
}

void ContainerManager::followContentHeight(const QString& id)
{
    ContainerWidget* view = container(id);
    ContainerContentHost* content = contentHost(id);
    FloatingContainer* shell = m_floatingForms.value(id);
    if (!m_store || m_reconciling || GuiApplication::applicationQuitInProgress() || !view || !content) {
        return;
    }
    const WorkspaceDocument latest = m_store->snapshot();
    const auto placement = std::find_if(latest.containers.cbegin(), latest.containers.cend(),
        [&id](const ContainerDocument& value) { return value.id == id; });
    if (placement == latest.containers.cend() || placement->dockMode == DockMode::PanelDocked) {
        return;
    }
    // Workspace preview: a popped-out object's window whose
    // height the operator set keeps it and only grows to fit.
    const bool operatorHeight = !placement->autoHeight && view->operatorSizesHeight();
    if ((!placement->autoHeight && !operatorHeight) || view->sizingHeightNow()) {
        return;
    }
    const bool window = placement->dockMode == DockMode::Floating;
    if (window && view->dragActive()) {
        // A drag moves a window and never resizes it. The end of the drag
        // runs this again (wireContainer).
        return;
    }
    QWidget* target = window ? static_cast<QWidget*>(shell) : view;
    if (!target) {
        return;
    }
    int height = content->preferredContentHeight() + view->headerExtent();
    if (const int available = availableHeightFor(target);
        placement->layout == ContentLayout::FreeCanvas && available > 0) {
        height = qMin(height, available);
    }
    if (operatorHeight) {
        height = qMax(height, target->height());
    }
    QSize wanted = QSize(qMax(target->width(), view->minimumSizeHint().width()), height)
        .expandedTo(target->minimumSize()).boundedTo(target->maximumSize());
    if (window && shell) {
        // Never larger than all the screens together.
        wanted = shell->fittedSize(wanted);
    }
    if (target->size() != wanted) {
        // Width changes settle through the host's viewport layout;
        // use live placement rather than replaying saved geometry.
        target->resize(wanted);
    }
}

void ContainerManager::wireContainer(ContainerWidget* container)
{
    connect(container, &ContainerWidget::geometryInteractionStarted, this, [this, container] {
        m_geometryCommit.stop();
        m_interactionStart[container->id()] = liveGeometry(container->id());
    });
    connect(container, &ContainerWidget::geometryInteractionFinished, this, [this, container] {
        if (m_store && !m_reconciling) {
            // Only a drag or grip resize that moved or resized the container
            // is the operator's: a press and release with no move, or a
            // cancelled drag, re-bases nothing.
            const QString id = container->id();
            const bool known = m_interactionStart.contains(id);
            const QRect start = m_interactionStart.take(id);
            if (known && liveGeometry(id).size() == start.size()) {
                // A move, not a resize. The height follower left a window
                // alone while the drag moved it; whatever its content asked
                // for meanwhile is given now.
                followContentHeight(id);
            }
            if (m_dropSettled.remove(id)) {
                // The drop rule placed it, saved it and reported the end of
                // the drag (finishArrangeDrop).
                return;
            }
            if (!known || liveGeometry(id) != start) {
                rememberOperatorGeometry(id);
            }
            if (container->operatorSizesHeight() && !container->autoHeight()) {
                // Workspace preview: a resize in height just made this
                // window's height the operator's. Saved now, so nothing
                // puts the content's height back before the delayed commit.
                saveState();
            } else {
                m_geometryCommit.start();
            }
        }
        emit containerDragFinished();
    });
    // A removed container is deleted later; by then the same id may name
    // a new container (removed and added again in one turn of the event
    // loop), which stays registered: dropped here, it was never released
    // at shutdown and its content host read the deleted registry.
    connect(container, &QObject::destroyed, this, [this, id=container->id(), container] {
        if (m_containers.value(id) == container) { m_containers.remove(id); }
    });
    connect(container, &ContainerWidget::floatRequested, this, [this, container]() {
        floatContainer(container->id());
    });
    connect(container, &ContainerWidget::dockRequested, this, [this, container]() {
        dockContainer(container->id());
    });
    connect(container, &ContainerWidget::settingsRequested, this,
            [this, container]() {
        // The dialog opens once the menu that asked for it has returned,
        // and belongs to the main window, never to the container's shell:
        // Remove and Apply delete the container and its shell while the
        // dialog is still open, and a dialog on the stack owned by the
        // shell was freed with it (JJ 2026-10-09, "pointer being freed
        // was not allocated").
        QMetaObject::invokeMethod(this, [this, view = QPointer<ContainerWidget>(container)] {
            if (!view || this->container(view->id()) != view.data()) {
                return;
            }
            QWidget* shell = view->window();
            QWidget* owner = m_dockParent ? m_dockParent->window() : nullptr;
            // Owned by ScopedChildWidget: freed once even if the parent dies in exec().
            ScopedChildWidget<ContainerSettingsDialog> dialogOwner(view.data(), owner, this);
            ContainerSettingsDialog& dialog = *dialogOwner.get();
            if (shell && shell != owner) {
                // Over the shell, where it opened when the shell owned it.
                if (shell->windowFlags().testFlag(Qt::WindowStaysOnTopHint)) {
                    dialog.setWindowFlag(Qt::WindowStaysOnTopHint);
                }
                QRect frame(QPoint(), dialog.size());
                frame.moveCenter(shell->geometry().center());
                dialog.move(frame.topLeft());
            }
            dialog.exec();
        }, Qt::QueuedConnection);
    });
    connect(container, &ContainerWidget::notesChanged, this,
            [this, container](const QString& notes) {
        emit containerTitleChanged(container->id(), notes);
    });
    // Announce any MeterWidget that becomes a (descendant of) the
    // container's content. Listens for both the create path
    // (createContainer + caller setContent) and the restore path
    // (ContainerManager itself calls setContent after wireContainer).
    // During restoreState() m_suppressMeterAnnouncements is set so
    // the placeholder → setMeterFloating → fresh-meter cascade
    // doesn't emit twice; restoreState emits one manual announcement
    // after the final content is in place.
    connect(container, &ContainerWidget::contentChanged, this,
            [this](QWidget* content) {
        if (m_suppressMeterAnnouncements) { return; }
        if (auto* meter = innerMeterWidget(content)) {
            emit meterReadyForPolling(meter);
        }
    });
}

QString ContainerManager::extractMeterItems(ContainerWidget* container)
{
    if (!container) { return {}; }
    QWidget* content = container->content();
    MeterWidget* meter = innerMeterWidget(content);
    if (!meter) { return {}; }

    const QString payload = meter->serializeItems();

    if (qobject_cast<MeterWidget*>(content) == meter) {
        // Bare MeterWidget as content — clear via setContent(nullptr) so
        // the content holder layout is empty during the upcoming reparent.
        container->setContent(nullptr);
    } else if (auto* panel = qobject_cast<AppletPanelWidget*>(content)) {
        // AppletPanelWidget header — detach the MeterWidget so the panel
        // can reparent with no native-window child.
        panel->clearHeaderWidget();
    }
    qCDebug(lcContainer) << "Extracted meter items from" << container->id()
                          << "bytes:" << payload.size();
    return payload;
}

void ContainerManager::installFreshMeter(ContainerWidget* container, const QString& payload)
{
    if (!container || payload.isEmpty()) { return; }

    auto* fresh = new MeterWidget();
    fresh->deserializeItems(payload);
    fresh->inferStackFromGeometry();
    for (MeterItem* item : fresh->items()) {
        container->wireInteractiveItem(item);
    }

    QWidget* content = container->content();
    if (auto* panel = qobject_cast<AppletPanelWidget*>(content)) {
        panel->setHeaderWidget(fresh, QStringLiteral("Meters"), 1.3f);
        // Same gate as the wireContainer contentChanged listener below
        // — restoreState emits one manual announcement per container
        // once the final content is in place.
        if (!m_suppressMeterAnnouncements) {
            emit meterReadyForPolling(fresh);
        }
    } else {
        // Bare content path (user-created containers) — setContent emits
        // contentChanged which routes into meterReadyForPolling.
        container->setContent(fresh);
    }
    qCDebug(lcContainer) << "Installed fresh meter for" << container->id()
                          << "items:" << fresh->items().size();
}

ContainerWidget* ContainerManager::duplicateContainer(const QString& sourceId)
{
    if (m_store && !m_reconciling) {
        WorkspaceDocument document=m_store->snapshot();
        for (const auto& source : document.containers) {
            if (source.id!=sourceId) { continue; }
            for (const auto& entry : source.contents) { if(entry.typeId.startsWith("applet:")) {
                m_storageError=QStringLiteral("A singleton applet already has a placement; move it instead of duplicating it"); emit workspaceError(m_storageError); return nullptr;
            } }
            auto copy=source; copy.id=QUuid::createUuid().toString(QUuid::WithoutBraces); copy.dockMode=DockMode::Floating;
            for (auto& entry : copy.contents) { entry.id=QUuid::createUuid().toString(QUuid::WithoutBraces); entry.returnLocation.reset(); }
            showDuplicate(copy, source);
            document.containers.append(copy);
            if(commitWorkspace(document,document.revision).status!=CommitStatus::Saved) { return nullptr; }
            return container(copy.id);
        }
        return nullptr;
    }
    ContainerWidget* src = container(sourceId);
    if (!src) {
        qCWarning(lcContainer) << "duplicateContainer: unknown id:" << sourceId;
        return nullptr;
    }

    ContainerWidget* dup = createContainer(src->rxSource(), DockMode::Floating);
    if (!dup) { return nullptr; }

    // Copy user-editable state. The auto-generated ID on the new
    // container is deliberately preserved; everything else mirrors
    // the source.
    dup->setNotes(src->notes());
    dup->setBorder(src->hasBorder());
    dup->setLocked(src->isLocked());
    dup->setContainerEnabled(src->isContainerEnabled());
    dup->setShowOnRx(src->showOnRx());
    dup->setShowOnTx(src->showOnTx());
    dup->setContainerMinimises(src->containerMinimises());
    dup->setContainerHidesWhenRxNotUsed(src->containerHidesWhenRxNotUsed());
    dup->setAutoHeight(src->autoHeight());
    dup->setTitleBarVisible(src->isTitleBarVisible());
    dup->setPinOnTop(src->isPinOnTop());
    dup->setAxisLock(src->axisLock());
    dup->setDockedSize(src->dockedSize());

    qCDebug(lcContainer) << "Duplicated container:" << sourceId << "->" << dup->id();
    return dup;
}

void ContainerManager::showDuplicate(ContainerDocument& copy, const ContainerDocument& original) const
{
    // NereusSDR differs from Thetis on purpose here. Thetis's duplicate
    // (setup.cs:35197-35225 through MeterManager.ContainerToString /
    // ContainerFromString, MeterManager.cs:6147, 6217 [v2.10.3.15]) keeps
    // the original's state, so a copy of a hidden or docked container
    // arrives hidden or under the original, and the command looks like it
    // did nothing. Ours always shows the copy, floating, beside the
    // original.
    copy.visible = true;
    copy.config["enabled"] = true;
    copy.config.remove(QStringLiteral("hiddenByMacro"));
    if (!copy.contents.isEmpty()
        && std::none_of(copy.contents.cbegin(), copy.contents.cend(),
                        [](const ContentEntry& entry) { return entry.visible; })) {
        // Workspace preview: a container whose widgets are all hidden
        // leaves the screen (effectiveVisible), so the copy shows them.
        for (auto& entry : copy.contents) {
            entry.visible = true;
        }
    }
    QRect anchor;
    if (original.dockMode == DockMode::Floating) {
        anchor = original.geometry;
    } else if (ContainerWidget* c = container(original.id);
               c && c->width() >= ContainerWidget::kMinContainerWidth
               && c->height() >= ContainerWidget::kMinContainerHeight) {
        // Docked: the document holds dock-local geometry, so the anchor is
        // where the original is (or was last) on screen.
        anchor = QRect(c->mapToGlobal(QPoint(0, 0)), c->size());
    }
    if (!anchor.isValid()) {
        constexpr QSize kFallback(360, 300);
        const QSize size = original.geometry.isValid() ? original.geometry.size() : kFallback;
        const QRect window = m_dockParent ? m_dockParent->window()->geometry() : QRect(QPoint(100, 100), size);
        anchor = QRect(window.center() - QPoint(size.width() / 2, size.height() / 2), size);
    }
    copy.dockMode = DockMode::Floating;
    // Beside the original, and off any spot another container already has
    // (a second copy of the same original would land on the first).
    copy.geometry = defaultWindowRect(anchor.translated(kDuplicateOffsetPx, kDuplicateOffsetPx));
    copy.screen = screenForNewWindow(copy.geometry);
}

ContainerWidget* ContainerManager::createContainer(int rxSource, DockMode mode)
{
    if (m_store && !m_reconciling) {
        WorkspaceDocument document = m_store->snapshot();
        ContainerDocument d; d.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        d.name = QStringLiteral("Container"); d.layout = ContentLayout::VerticalStack; d.dockMode = mode; d.config["rxSource"] = rxSource;
        document.containers.append(d);
        if (document.mainContainerId.isEmpty()) { document.mainContainerId = d.id; }
        if (commitWorkspace(document, document.revision).status != CommitStatus::Saved) { return nullptr; }
        if (mode == DockMode::Floating) {
            // A new window has no place of its own: the one it is given is
            // saved, so the next start puts it there again.
            keepNewWindowPlace(d.id);
        }
        return container(d.id);
    }
    // From Thetis MeterManager.cs:5613-5673
    auto* container = new ContainerWidget(nullptr);
    container->setRxSource(rxSource);
    container->setDockMode(mode);

    auto* floatingForm = new FloatingContainer(rxSource);
    floatingForm->setScreenSource(m_screens);
    floatingForm->setId(container->id());

    wireContainer(container);

    m_containers.insert(container->id(), container);
    m_floatingForms.insert(container->id(), floatingForm);

    // Place container according to dock mode
    switch (mode) {
    case DockMode::PanelDocked:
        container->setParent(m_splitter);
        m_splitter->addWidget(container);
        m_panelContainerId = container->id();
        container->show();
        break;
    case DockMode::OverlayDocked:
        container->setParent(m_dockParent);
        container->show();
        container->raise();
        break;
    case DockMode::Floating:
        setMeterFloating(container, floatingForm);
        break;
    }

    qCDebug(lcContainer) << "Created container:" << container->id()
                          << "rx:" << rxSource << "mode:" << static_cast<int>(mode);
    emit containerAdded(container->id());
    return container;
}

void ContainerManager::destroyContainer(const QString& id)
{
    if (m_store && !m_reconciling) {
        const auto result = m_arrange->removeContainer(id);
        if (!result.ok) {
            emit workspaceError(result.error);
        }
        return;
    }
    // From Thetis MeterManager.cs:6533-6579
    // Upstream inline attribution preserved verbatim (MeterManager.cs:6563):
    //   f.Dispose();//[2.10.3.7]MW0LGE // we have to dispose it because close() prevent this being freed up
    if (!m_containers.contains(id)) {
        qCWarning(lcContainer) << "destroyContainer: unknown id:" << id;
        return;
    }

    if (m_floatingForms.contains(id)) {
        FloatingContainer* form = m_floatingForms.take(id);
        form->hide();
        form->deleteLater();
    }

    ContainerWidget* container = m_containers.take(id);
    container->hide();
    container->setParent(nullptr);
    container->deleteLater();

    if (m_panelContainerId == id) {
        m_panelContainerId.clear();
    }

    qCDebug(lcContainer) << "Destroyed container:" << id;
    emit containerRemoved(id);
}

void ContainerManager::floatContainer(const QString& id)
{
    if (commitDockMode(id, DockMode::Floating)) { return; }
    if (!m_containers.contains(id) || !m_floatingForms.contains(id)) {
        return;
    }
    setMeterFloating(m_containers[id], m_floatingForms[id]);
}

void ContainerManager::dockContainer(const QString& id)
{
    if (!m_containers.contains(id) || !m_floatingForms.contains(id)) {
        return;
    }
    if (m_store && WorkspacePreview::enabled() && returnHome(id)) {
        // Workspace preview: back where it was popped out of.
        return;
    }
    // Return to previous dock mode
    if (id == m_panelContainerId) {
        panelDockContainer(id);
    } else {
        overlayDockContainer(id);
    }
}

void ContainerManager::panelDockContainer(const QString& id)
{
    if (commitDockMode(id, DockMode::PanelDocked)) { return; }
    if (!m_containers.contains(id) || !m_floatingForms.contains(id)) {
        return;
    }
    ContainerWidget* container = m_containers[id];
    FloatingContainer* form = m_floatingForms[id];

    form->setContainerFloating(false);
    form->hide();
    container->hide();
    const QString payload = extractMeterItems(container);
    container->setParent(m_splitter);
    m_splitter->addWidget(container);
    container->setDockMode(DockMode::PanelDocked);
    container->show();
    installFreshMeter(container, payload);
    m_panelContainerId = id;

    qCDebug(lcContainer) << "Panel-docked container:" << id;
}

void ContainerManager::overlayDockContainer(const QString& id)
{
    if (commitDockMode(id, DockMode::OverlayDocked)) { return; }
    if (!m_containers.contains(id) || !m_floatingForms.contains(id)) {
        return;
    }
    ContainerWidget* container = m_containers[id];
    FloatingContainer* form = m_floatingForms[id];

    // From Thetis MeterManager.cs:5867-5893
    form->setContainerFloating(false);
    form->hide();
    container->hide();
    const QString payload = extractMeterItems(container);
    container->setParent(m_dockParent);
    container->setDockMode(DockMode::OverlayDocked);
    container->restoreLocation();
    container->show();
    container->raise();
    installFreshMeter(container, payload);

    qCDebug(lcContainer) << "Overlay-docked container:" << id;
}

void ContainerManager::setMeterFloating(ContainerWidget* container, FloatingContainer* form)
{
    // From Thetis MeterManager.cs:5894-5918
    container->hide();
    const QString payload = extractMeterItems(container);
    form->takeOwner(container);
    form->setContainerFloating(true);
    container->setDockMode(DockMode::Floating);
    container->setTopMost();  // Re-apply pin-on-top now that parent is set
    if (form->ensureVisiblePosition(m_dockParent, m_screens)) {
        // A place the app chose: not one another window already has.
        form->setGeometry(defaultWindowRect(form->geometry(), container->id()));
    }
    form->show();
    installFreshMeter(container, payload);
    qCDebug(lcContainer) << "Floated container:" << container->id();
}

void ContainerManager::returnMeterFromFloating(ContainerWidget* container, FloatingContainer* form)
{
    // From Thetis MeterManager.cs:5867-5893
    form->setContainerFloating(false);
    form->hide();
    container->hide();
    const QString payload = extractMeterItems(container);
    container->setParent(m_dockParent);
    container->setDockMode(DockMode::OverlayDocked);
    container->restoreLocation();
    container->show();
    container->raise();
    installFreshMeter(container, payload);
    qCDebug(lcContainer) << "Docked container:" << container->id();
}

void ContainerManager::recoverContainer(const QString& id)
{
    if (m_store && !m_reconciling) {
        WorkspaceDocument document=m_store->snapshot();
        for (auto& d : document.containers) { if(d.id==id) {
            d.visible=true; d.config["enabled"]=true;
            if(d.dockMode==DockMode::Floating) { d.dockMode=DockMode::OverlayDocked; }
            if(m_dockParent) { const QSize size=d.geometry.isValid()?d.geometry.size():QSize(360,300); d.geometry=defaultFreeRect(QRect(QPoint(qMax(0,(m_dockParent->width()-size.width())/2),qMax(0,(m_dockParent->height()-size.height())/2)),size), id); }
            // Centred for the pane as it is now.
            d.geometryPane.reset();
            d.screen.reset();
            commitWorkspace(document,document.revision); return;
        } }
        return;
    }
    // From Thetis MeterManager.cs:6514-6531
    if (!m_containers.contains(id)) {
        return;
    }
    ContainerWidget* container = m_containers[id];

    if (container->isFloating()) {
        overlayDockContainer(id);
    }
    container->setContainerEnabled(true);
    container->show();

    if (m_dockParent) {
        int cx = (m_dockParent->width() / 2) - (container->width() / 2);
        int cy = (m_dockParent->height() / 2) - (container->height() / 2);
        container->move(cx, cy);
        container->storeLocation();
    }
    qCDebug(lcContainer) << "Recovered container:" << id;
}

// ---- Windows and their monitors -------------------------------------------

void ContainerManager::setScreenSourceForTest(ScreenSource* source)
{
    m_screens = source ? source : ScreenSource::system();
    for (FloatingContainer* form : std::as_const(m_floatingForms)) {
        if (form) {
            form->setScreenSource(m_screens);
        }
    }
}

ScreenInfo ContainerManager::mainWindowScreen() const
{
    if (m_dockParent && m_dockParent->window()) {
        return m_screens->screenFor(m_dockParent->window()->geometry());
    }
    return m_screens->primary();
}

int ContainerManager::availableHeightFor(const QWidget* target) const
{
    const QRect onScreen(target->mapToGlobal(QPoint(0, 0)), target->size());
    return m_screens->screenFor(onScreen).available.height();
}

void ContainerManager::refreshWindowSizeLimits()
{
    for (FloatingContainer* form : std::as_const(m_floatingForms)) {
        if (form) {
            form->refreshDesktopLimit();
        }
    }
}

void ContainerManager::watchScreen(QScreen* screen)
{
    if (screen) {
        // A monitor whose resolution or place changed: the limit only.
        connect(screen, &QScreen::geometryChanged, this, &ContainerManager::refreshWindowSizeLimits,
                Qt::UniqueConnection);
        // And its windows as for a screen that came or went: a window left
        // where its header cannot be reached is brought back. Queued, as
        // those are, so the change has been applied.
        connect(screen, &QScreen::geometryChanged, this, &ContainerManager::screensChanged,
                static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
        connect(screen, &QScreen::availableGeometryChanged, this, &ContainerManager::screensChanged,
                static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
    }
}

std::optional<SavedScreen> ContainerManager::screenForNewWindow(const QRect& geometry) const
{
    if (!windowIsHome(geometry, std::nullopt, false)) {
        return std::nullopt;
    }
    const std::optional<ScreenInfo> on = m_screens->screenHolding(geometry);
    return on ? std::optional<SavedScreen>(savedScreenOf(*on)) : std::nullopt;
}

void ContainerManager::keepNewWindowPlace(const QString& id)
{
    const ContainerWidget* c = container(id);
    if (!m_store || m_reconciling || !c || !c->isFloating() || !m_displaced.contains(id)) {
        return;
    }
    // Its document place could not be used, so the app chose one. The
    // operator asked for this window: that place is its own from now on.
    rememberOperatorGeometry(id);
    saveState();
}

QRect ContainerManager::defaultWindowRect(const QRect& wanted) const
{
    return defaultWindowRect(wanted, QString());
}

QRect ContainerManager::defaultWindowRect(const QRect& wanted, const QString& exceptId) const
{
    const std::optional<ScreenInfo> holding = m_screens->screenHolding(wanted);
    return windowRectOn(wanted, holding ? *holding : mainWindowScreen(), exceptId);
}

QRect ContainerManager::windowRectOn(const QRect& wanted, const ScreenInfo& screen, const QString& exceptId) const
{
    // Every visible container's top-left, in screen coordinates.
    QVector<QPoint> taken;
    for (auto it = m_containers.constBegin(); it != m_containers.constEnd(); ++it) {
        ContainerWidget* c = it.value();
        if (!c || it.key() == exceptId) {
            continue;
        }
        if (c->isFloating()) {
            const FloatingContainer* form = m_floatingForms.value(it.key());
            if (form && form->isVisible()) {
                taken.append(form->geometry().topLeft());
            }
        } else if (c->isOverlayDocked() && !c->isHidden() && c->parentWidget()) {
            taken.append(c->mapToGlobal(QPoint(0, 0)));
        }
    }
    return cascaded(wanted, screen.available, taken, kCascadeOffsetPx);
}

QRect ContainerManager::defaultFreeRect(const QRect& wanted, const QString& exceptId) const
{
    QVector<QPoint> taken;
    for (auto it = m_containers.constBegin(); it != m_containers.constEnd(); ++it) {
        const ContainerWidget* c = it.value();
        if (c && it.key() != exceptId && c->isOverlayDocked() && !c->isHidden()
            && c->parentWidget() == m_dockParent) {
            taken.append(c->geometry().topLeft());
        }
    }
    const QRect pane = dockParentLaidOut() ? QRect(QPoint(0, 0), m_dockParent->size()) : QRect();
    return cascaded(wanted, pane, taken, kCascadeOffsetPx);
}

QRect ContainerManager::nearestClearFreeRect(const QRect& wanted, const QString& exceptId) const
{
    if (!dockParentLaidOut()) {
        return defaultFreeRect(wanted, exceptId);
    }
    const QRect pane(QPoint(0, 0), m_dockParent->size());
    const QRect start = FloatingContainer::clampedGeometry(wanted, pane, kPlacementMinimum);
    QVector<QRect> others;
    for (auto it = m_containers.constBegin(); it != m_containers.constEnd(); ++it) {
        const ContainerWidget* c = it.value();
        if (c && it.key() != exceptId && c->isOverlayDocked() && !c->isHidden()
            && c->parentWidget() == m_dockParent) {
            others.append(c->geometry());
        }
    }
    // The least move ends with each coordinate either where it was wanted
    // (held inside the pane) or flush against a side of the pane or of
    // another container, so those are the only places to try.
    QVector<int> lefts{start.x(), pane.left(), pane.right() + 1 - start.width()};
    QVector<int> tops{start.y(), pane.top(), pane.bottom() + 1 - start.height()};
    for (const QRect& other : others) {
        lefts.append(other.left() - start.width());
        lefts.append(other.right() + 1);
        tops.append(other.top() - start.height());
        tops.append(other.bottom() + 1);
    }
    std::optional<QRect> best;
    qint64 bestMoved = 0;
    for (const int left : lefts) {
        for (const int top : tops) {
            const QRect at(QPoint(left, top), start.size());
            if (!pane.contains(at)) {
                continue;
            }
            const bool covers = std::any_of(others.cbegin(), others.cend(),
                [&at](const QRect& other) { return other.intersects(at); });
            if (covers) {
                continue;
            }
            const QPoint moved = at.topLeft() - wanted.topLeft();
            const qint64 distance = qint64(moved.x()) * moved.x() + qint64(moved.y()) * moved.y();
            if (!best || distance < bestMoved) {
                best = at;
                bestMoved = distance;
            }
        }
    }
    // Nowhere in the pane is clear: off the others' top-lefts, as any
    // default placement is.
    return best ? *best : defaultFreeRect(wanted, exceptId);
}

bool ContainerManager::windowIsHome(const QRect& remembered, const std::optional<SavedScreen>& screen,
                                    bool restoring) const
{
    if (!remembered.isValid()) {
        return false;
    }
    if (screen) {
        // Its monitor is here, and the place is still on it (a monitor
        // known by its serial may have been moved in the arrangement).
        const std::optional<ScreenInfo> present = m_screens->matching(*screen, remembered);
        return present && remembered.intersects(present->geometry) && (!restoring || headerReachable(remembered));
    }
    // An older layout names no monitor: any present screen the rect is on
    // will do. A rect at the origin is the place a window never given one
    // lands at, where the main window hides it (see
    // FloatingContainer::ensureVisiblePosition); it was never a chosen place.
    return remembered.topLeft() != QPoint(0, 0) && m_screens->onAnyScreen(remembered)
        && (!restoring || headerReachable(remembered));
}

bool ContainerManager::sizesWindowDifferently(const ContainerDocument& before, const ContainerDocument& now)
{
    // What a window's size follows besides its geometry: whether its
    // height is its content's, and what that content and its header are.
    static const QStringList kChromeKeys{QStringLiteral("border"), QStringLiteral("noControls")};
    const bool chrome = std::any_of(kChromeKeys.cbegin(), kChromeKeys.cend(), [&before, &now](const QString& key) {
        return before.config.value(key) != now.config.value(key);
    });
    return chrome || before.autoHeight != now.autoHeight || before.contents != now.contents
        || before.layout != now.layout || before.header != now.header || before.popOutShell != now.popOutShell;
}

bool ContainerManager::headerReachable(const QRect& remembered) const
{
    // The window's own header is the top strip of its rect. A piece of it
    // under the system's own bars cannot be taken hold of, so it is the
    // screen's usable area that counts.
    const QRect header(remembered.topLeft(), QSize(remembered.width(), ContainerWidget::kTitleBarHeight));
    const int wide = qMin(kHeaderReachWidthPx, header.width());
    const QVector<ScreenInfo> present = m_screens->screens();
    return std::any_of(present.cbegin(), present.cend(), [&header, wide](const ScreenInfo& screen) {
        const QRect usable = screen.available.isValid() ? screen.available : screen.geometry;
        const QRect shared = usable.intersected(header);
        return shared.width() >= wide && shared.height() >= kHeaderReachHeightPx;
    });
}

QRect ContainerManager::displacedWindowRect(const QString& id, const QRect& remembered,
                                            const std::optional<SavedScreen>& screen, const QSize& size) const
{
    const std::optional<ScreenInfo> own = screen ? m_screens->matching(*screen, remembered) : std::nullopt;
    const ScreenInfo target = own ? *own : mainWindowScreen();
    QRect wanted;
    if (remembered.isValid() && screen && target.geometry.isValid()) {
        // Where it was on its own monitor, on this one, so windows that
        // were apart stay apart.
        wanted = QRect(remembered.topLeft() - screen->geometry.topLeft() + target.geometry.topLeft(), size);
    } else {
        // Over the main window, as a window with no place always was.
        const QRect over = m_dockParent && m_dockParent->window() ? m_dockParent->window()->geometry()
                                                                  : target.available;
        wanted = QRect(QPoint(over.center().x() - size.width() / 2, over.center().y() - size.height() / 2), size);
    }
    return windowRectOn(wanted, target, id);
}

void ContainerManager::placeFloatingWindow(const QString& id, const QRect& remembered,
                                           const std::optional<SavedScreen>& screen, FloatingContainer* form,
                                           bool fromDocument, bool restoring)
{
    if (windowIsHome(remembered, screen, restoring)) {
        const bool wasAway = m_displaced.remove(id);
        if ((fromDocument || wasAway) && form->pos() != remembered.topLeft()) {
            form->move(remembered.topLeft());
        }
        const ContainerWidget* c = container(id);
        if (wasAway && c && !c->autoHeight() && form->size() != form->fittedSize(remembered.size())) {
            // Back at its own size too, had the other screen been smaller.
            form->resize(form->fittedSize(remembered.size()));
        }
        return;
    }
    if (m_displaced.contains(id) && m_screens->onAnyScreen(form->geometry())) {
        // Already shown somewhere the operator can reach.
        return;
    }
    form->setGeometry(displacedWindowRect(id, remembered, screen, form->size()));
    m_displaced.insert(id);
}

void ContainerManager::bringAppletOnScreen(const QString& typeId)
{
    if (!m_store || typeId.isEmpty()) {
        return;
    }
    const WorkspaceDocument document = m_store->snapshot();
    for (const auto& d : document.containers) {
        const bool holds = std::any_of(d.contents.cbegin(), d.contents.cend(),
                                       [&typeId](const ContentEntry& entry) { return entry.typeId == typeId; });
        if (!holds) {
            continue;
        }
        FloatingContainer* form = m_floatingForms.value(d.id);
        if (const ContainerWidget* c = container(d.id); c && c->isFloating() && form && form->isVisible()) {
            keepWindowReachable(d.id);
            form->raise();
        }
        return;
    }
}

void ContainerManager::keepWindowReachable(const QString& id)
{
    ContainerWidget* c = container(id);
    FloatingContainer* form = m_floatingForms.value(id);
    if (!c || !form || !c->isFloating() || c->geometryInteractionActive()
        || GuiApplication::applicationQuitInProgress() || m_screens->screens().isEmpty()
        || headerReachable(form->geometry())) {
        return;
    }
    // The app's own move: nothing is remembered or committed for it.
    QScopedValueRollback<bool> guard(m_reconciling, true);
    const QRect remembered = m_rememberedGeometry.value(id, form->geometry());
    const std::optional<SavedScreen> screen = m_rememberedScreen.value(id);
    if (windowIsHome(remembered, screen, true)) {
        // Its own place is on a screen: back there.
        m_displaced.remove(id);
        form->move(remembered.topLeft());
    } else {
        form->setGeometry(displacedWindowRect(id, remembered, screen, form->size()));
        m_displaced.insert(id);
    }
    qCDebug(lcContainer) << "Window" << id << "brought where its header can be reached:" << form->geometry();
}

void ContainerManager::keepWindowsReachable()
{
    const QStringList ids = m_floatingForms.keys();
    for (const QString& id : ids) {
        if (FloatingContainer* form = m_floatingForms.value(id); form && form->isVisible()) {
            keepWindowReachable(id);
        }
    }
}

void ContainerManager::screensChanged()
{
    refreshWindowSizeLimits();
    if (!m_store || m_reconciling || GuiApplication::applicationQuitInProgress()) {
        return;
    }
    // The app's own moves: nothing is remembered or committed for them.
    QScopedValueRollback<bool> guard(m_reconciling, true);
    const WorkspaceDocument document = m_store->snapshot();
    for (const auto& d : document.containers) {
        ContainerWidget* c = container(d.id);
        FloatingContainer* form = m_floatingForms.value(d.id);
        if (d.dockMode != DockMode::Floating || !c || !form || !c->isFloating()
            || c->geometryInteractionActive()) {
            continue;
        }
        placeFloatingWindow(d.id, m_rememberedGeometry.value(d.id, d.geometry),
                            m_rememberedScreen.value(d.id, d.screen), form, false, true);
    }
    // A window at its own place is left there only while its header can be
    // reached (one the system moved off the screens comes back).
    keepWindowsReachable();
}


bool ContainerManager::dockParentLaidOut() const
{
    return m_dockParent && (m_dockParent->isWindow() || m_dockParentResized);
}

QRect ContainerManager::liveGeometry(const QString& id) const
{
    ContainerWidget* c = container(id);
    if (!c) {
        return {};
    }
    FloatingContainer* form = m_floatingForms.value(id);
    return c->isFloating() && form ? form->geometry() : c->geometry();
}

void ContainerManager::rememberOperatorGeometry(const QString& id)
{
    ContainerWidget* c = container(id);
    if (!m_store || !c || m_geometryCaptureSuppressed) {
        return;
    }
    const QRect place = liveGeometry(id);
    m_rememberedGeometry[id] = place;
    if (c->isOverlayDocked()) {
        if (dockParentLaidOut()) {
            m_rememberedPane[id] = m_dockParent->size();
            c->setDelta(QPoint(m_dockParent->width(), m_dockParent->height()));
        } else {
            m_rememberedPane.remove(id);
        }
        c->setDockedLocation(place.topLeft());
        c->setDockedSize(place.size());
    } else {
        m_rememberedPane.remove(id);
        if (c->isFloating()) {
            // The monitor it was put on goes with the place, and the window
            // is where the operator wants it wherever it was shown before.
            const std::optional<ScreenInfo> on = m_screens->screenHolding(place);
            m_rememberedScreen[id] = on ? std::optional<SavedScreen>(savedScreenOf(*on)) : std::nullopt;
            m_displaced.remove(id);
        }
    }
    m_provisionalPane.remove(id);
    m_pendingGeometry.insert(id);
    if (!m_reconciling) {
        m_geometryCommit.start();
    }
}

void ContainerManager::placeFreeContainers()
{
    if (m_dockParent) {
        placeFreeContainersIn(m_dockParent->size());
    }
}

void ContainerManager::placeFreeContainersIn(const QSize& pane)
{
    if (!m_store || !dockParentLaidOut()) {
        // Before the pane has a size of its own the containers sit at their
        // remembered rects, as reconcile left them.
        return;
    }
    // The app's own moves: nothing is captured for them.
    QScopedValueRollback<bool> guard(m_reconciling, true);
    for (auto it = m_containers.constBegin(); it != m_containers.constEnd(); ++it) {
        ContainerWidget* c = it.value();
        if (!c || !c->isOverlayDocked() || c->parentWidget() != m_dockParent || c->geometryInteractionActive()) {
            continue;
        }
        const QRect place = m_rememberedGeometry.value(it.key());
        if (!place.isValid()) {
            continue;
        }
        if (!m_rememberedPane.contains(it.key()) || m_provisionalPane.contains(it.key())) {
            // No pane size came with the place (an older document, a menu
            // command): it is for the pane as it is now. While the pane is
            // still on the way to its first settled size that baseline
            // follows it, so the container is shown at its saved place for
            // the size the pane ends at, not for one it passed through.
            m_rememberedPane[it.key()] = pane;
            if (!m_paneSettled) {
                m_provisionalPane.insert(it.key());
            }
        }
        const QSize base = m_rememberedPane.value(it.key());
        // The anchor values the widget carries mirror the remembered place.
        c->setDockedLocation(place.topLeft());
        c->setDockedSize(place.size());
        c->setDelta(QPoint(base.width(), base.height()));
        QRect shown(anchoredTopLeft(place.topLeft(), c->axisLock(), pane - base), place.size());
        if (c->autoHeight()) {
            // Derived from the content on reconcile, not remembered.
            shown.setHeight(c->height());
        }
        shown = FloatingContainer::clampedGeometry(shown, QRect(QPoint(0, 0), pane),
                                                   c->minimumSizeHint().expandedTo(QSize(260, 24)));
        if (shown != c->geometry()) {
            c->setGeometry(shown);
        }
    }
}

void ContainerManager::updateDockedPositions(int hDelta, int vDelta)
{
    if (m_store) {
        placeFreeContainersIn(QSize(hDelta, vDelta));
        return;
    }
    // From Thetis MeterManager.cs:5812-5865 — overlay-docked only
    for (auto it = m_containers.constBegin(); it != m_containers.constEnd(); ++it) {
        ContainerWidget* c = it.value();
        if (!c->isOverlayDocked()) {
            continue;
        }

        QPoint dockedLoc = c->dockedLocation();
        QPoint delta = c->delta();
        QPoint newLocation;

        switch (c->axisLock()) {
        case AxisLock::Right:
        case AxisLock::BottomRight:
            newLocation = QPoint(dockedLoc.x() - delta.x() + hDelta,
                                 dockedLoc.y() - delta.y() + vDelta);
            break;
        case AxisLock::BottomLeft:
        case AxisLock::Left:
            newLocation = QPoint(dockedLoc.x(),
                                 dockedLoc.y() - delta.y() + vDelta);
            break;
        case AxisLock::TopLeft:
            newLocation = QPoint(dockedLoc.x(), dockedLoc.y());
            break;
        case AxisLock::Top:
        case AxisLock::TopRight:
            newLocation = QPoint(dockedLoc.x() - delta.x() + hDelta,
                                 dockedLoc.y());
            break;
        case AxisLock::Bottom:
            newLocation = QPoint(dockedLoc.x() - delta.x() + hDelta,
                                 dockedLoc.y() - delta.y() + vDelta);
            break;
        }

        if (m_dockParent) {
            int maxX = m_dockParent->width() - c->width();
            int maxY = m_dockParent->height() - c->height();
            newLocation.setX(std::clamp(newLocation.x(), 0, std::max(0, maxX)));
            newLocation.setY(std::clamp(newLocation.y(), 0, std::max(0, maxY)));
        }

        if (newLocation != c->pos()) {
            c->move(newLocation);
        }
    }
}

void ContainerManager::saveSplitterState()
{
    if (m_store) { saveState(); return; }
    if (!m_splitter) {
        return;
    }
    QList<int> sizes = m_splitter->sizes();
    QStringList parts;
    for (int s : sizes) {
        parts << QString::number(s);
    }
    AppSettings::instance().setValue(QStringLiteral("MainSplitterSizes"),
                                    parts.join(QLatin1Char(',')));
}

void ContainerManager::restoreSplitterState()
{
    if (m_store) {
        if (!m_splitter || edgeMode()) { return; }
        const QJsonArray saved = m_store->snapshot().extensions.value("splitterSizes").toArray();
        QList<int> sizes; for (const auto& value : saved) { sizes.append(value.toInt()); }
        if (sizes.size() == m_splitter->count()) { QScopedValueRollback<bool> guard(m_reconciling, true); m_splitter->setSizes(sizes); }
        return;
    }
    if (!m_splitter) {
        return;
    }
    QString val = AppSettings::instance().value(QStringLiteral("MainSplitterSizes")).toString();
    if (val.isEmpty()) {
        return;
    }
    QStringList parts = val.split(QLatin1Char(','));
    QList<int> sizes;
    for (const QString& p : parts) {
        bool ok;
        int s = p.toInt(&ok);
        if (ok) {
            sizes << s;
        }
    }
    if (sizes.size() == m_splitter->count()) {
        m_splitter->setSizes(sizes);
    }
}

QList<ContainerWidget*> ContainerManager::allContainers() const
{
    return m_containers.values();
}

ContainerWidget* ContainerManager::container(const QString& id) const
{
    return m_containers.value(id, nullptr);
}

ContainerWidget* ContainerManager::panelContainer() const
{
    return m_containers.value(m_panelContainerId, nullptr);
}

int ContainerManager::containerCount() const
{
    return m_containers.size();
}

QString ContainerManager::entryTitle(const ContentEntry& entry) const
{
    if (!entry.name.isEmpty()) {
        return entry.name;
    }
    if (m_registry) {
        for (const auto& descriptor : m_registry->descriptors()) {
            if (descriptor.typeId == entry.typeId) {
                return descriptor.title;
            }
        }
    }
    return entry.id;
}

QVector<ContainerManager::HiddenItem> ContainerManager::hiddenItems() const
{
    QVector<HiddenItem> items;
    if (!m_store) {
        return items;
    }
    const WorkspaceDocument document = m_store->snapshot();
    for (const auto& d : document.containers) {
        const QString name = d.name.isEmpty() ? d.id : d.name;
        // Every flag that takes the container off the screen, as
        // effectiveVisible reads them (hidden on receive, or on transmit
        // while transmitting, and hidden by a macro included).
        if (hiddenByOwnFlags(d)) {
            items.append({d.id, {}, name});
        }
        for (const auto& entry : d.contents) {
            if (!entry.visible) {
                items.append({d.id, entry.id, QStringLiteral("%1 (%2)").arg(entryTitle(entry), name)});
            }
        }
    }
    return items;
}

bool ContainerManager::showHiddenContainer(const QString& id)
{
    if (!m_store) {
        return false;
    }
    WorkspaceDocument document = m_store->snapshot();
    bool found = false;
    for (auto& d : document.containers) {
        if (d.id == id) {
            d.visible = true;
            d.config["enabled"] = true;
            // Only a flag that hides it is written.
            if (d.config.value("hiddenByMacro").toBool(false)) {
                d.config["hiddenByMacro"] = false;
            }
            const char *shownNow = m_transmitting ? "showOnTx" : "showOnRx";
            if (!d.config.value(shownNow).toBool(true)) {
                d.config[shownNow] = true;
            }
            found = true;
        }
    }
    return found && commitWorkspace(document, document.revision).status == CommitStatus::Saved;
}

bool ContainerManager::showHiddenEntry(const QString& entryId)
{
    if (!m_arrange) {
        return false;
    }
    const ArrangeResult result = m_arrange->setEntryVisible(entryId, true);
    if (!result.ok) {
        qCWarning(lcContainer) << "showHiddenEntry:" << result.error;
    }
    return result.ok;
}

void ContainerManager::setContainerVisible(const QString& id, bool visible)
{
    if (m_store && !m_reconciling) {
        WorkspaceDocument document = m_store->snapshot();
        for (auto& d : document.containers) { if (d.id == id) { d.visible = visible; } }
        commitWorkspace(document, document.revision); return;
    }
    if (!m_containers.contains(id)) {
        return;
    }
    ContainerWidget* c = m_containers[id];
    if (c->isFloating() && m_floatingForms.contains(id)) {
        m_floatingForms[id]->setVisible(visible);
    } else {
        c->setVisible(visible);
    }
}

// ---------------------------------------------------------------------------
// forEachMeterItem — Task 3.2 unit-mode fan-out helper
//
// Walks every container, extracts its MeterWidget via innerMeterWidget(),
// and invokes fn on each MeterItem in that widget.  Used by MultimeterPage
// to broadcast setUnitMode / setShowDecimal to all live items without the
// items having to poll AppSettings on every paint redraw.
// ---------------------------------------------------------------------------
void ContainerManager::forEachMeterItem(std::function<void(MeterItem*)> fn)
{
    for (ContainerWidget* container : m_containers) {
        QList<MeterWidget*> meters;
        if (auto* direct = qobject_cast<MeterWidget*>(container->content())) { meters.append(direct); }
        meters += container->content()->findChildren<MeterWidget*>();
        for (MeterWidget* meter : meters) {
        if (!meter) {
            continue;
        }
        for (MeterItem* item : meter->items()) {
            fn(item);
        }
        }
    }
}

void ContainerManager::setContentFactory(ContainerContentFactory factory)
{
    m_contentFactory = std::move(factory);
}

void ContainerManager::setGeometryCaptureSuppressed(bool on)
{
    m_geometryCaptureSuppressed = on;
    m_geometryCommit.stop();
    if (!on) {
        m_pendingGeometry.clear();
    }
}

void ContainerManager::saveState()
{
    if (m_store) {
        if (m_reconciling || m_geometryCaptureSuppressed || !m_store->loadError().isEmpty()) { return; }
        // Publication projects the final captured geometry; rejection restores
        // committed placement rather than preserving the rejected live draft.
        m_geometryCommit.stop();
        const QSet<QString> moved = m_pendingGeometry;
        m_pendingGeometry.clear();
        WorkspaceDocument document = m_store->snapshot();
        for (auto& d : document.containers) {
            auto* c = container(d.id); if (!c) { continue; }
            // Geometry commits retain the committed content verbatim; live readings
            // and stack projection are presentation state, not document edits.
            d.name = c->notes(); d.dockMode = c->dockMode(); d.locked = c->isLocked(); d.autoHeight = c->autoHeight(); d.anchor = c->axisLock();
            // The remembered place, never the widget's rect: what is shown
            // may differ for a while (a small pane, a missing screen).
            d.geometry = m_rememberedGeometry.value(d.id, d.geometry);
            // A window's monitor goes with its place; nothing else has one.
            if (d.dockMode != DockMode::Floating) {
                d.screen.reset();
            } else if (m_rememberedScreen.contains(d.id)) {
                d.screen = m_rememberedScreen.value(d.id);
            }
            if (d.dockMode != DockMode::OverlayDocked) {
                d.geometryPane.reset();
            } else if (moved.contains(d.id)) {
                d.geometryPane = m_rememberedPane.contains(d.id)
                    ? std::optional<QSize>(m_rememberedPane.value(d.id)) : std::nullopt;
            }
            if (c->rxSource() != d.config.value("rxSource").toInt(1)) { d.config["rxSource"] = c->rxSource(); }
            d.config["enabled"] = c->isContainerEnabled();
            d.config["showOnRx"] = c->showOnRx(); d.config["showOnTx"] = c->showOnTx();
            d.config["border"] = c->hasBorder(); d.config["pinOnTop"] = c->isPinOnTop();
        }
        if (m_splitter && !edgeMode()) { QJsonArray sizes; for (int size : m_splitter->sizes()) { sizes.append(size); } document.extensions["splitterSizes"] = sizes; }
        // Workspace preview: strip thickness and extents, not the splitter.
        if (edgeMode()) { captureArrangement(document); }
        if (document == m_store->snapshot()) { return; }
        if (commitWorkspace(document, document.revision).status != CommitStatus::Saved) { reconcileWorkspace(m_store->snapshot()); }
        return;
    }
    // From Thetis MeterManager.cs:6391-6447
    // Upstream inline attribution preserved verbatim (MeterManager.cs:6433):
    //   //a.Add("meterIGSettings_" + ig.Value.ID, igs.ToString()); //[2.10.3.6]MW0LGE not used
    auto& s = AppSettings::instance();

    // Clear old data
    QString oldIdList = s.value(QStringLiteral("ContainerIdList")).toString();
    if (!oldIdList.isEmpty()) {
        for (const QString& oldId : oldIdList.split(QLatin1Char(','))) {
            s.remove(QStringLiteral("ContainerData_%1").arg(oldId));
            s.remove(QStringLiteral("ContainerItems_%1").arg(oldId));
            s.remove(QStringLiteral("MeterDisplay_%1_Geometry").arg(oldId));
        }
    }

    // Save current containers
    QStringList idList;
    for (auto it = m_containers.constBegin(); it != m_containers.constEnd(); ++it) {
        ContainerWidget* c = it.value();
        if (c->isOverlayDocked()) {
            c->storeLocation();
        }
        s.setValue(QStringLiteral("ContainerData_%1").arg(c->id()), c->serialize());
        // Persist the meter items hosted inside the container, if any.
        // Bare-MeterWidget and AppletPanelWidget content shapes both
        // resolve via innerMeterWidget(); placeholder content (the
        // QLabel installed by the constructor) returns nullptr and is
        // skipped. Stored in a parallel key — items payload contains
        // both '|' and '\n' separators that would corrupt the
        // field-tolerant container metadata format if appended.
        if (auto* meter = innerMeterWidget(c->content())) {
            s.setValue(QStringLiteral("ContainerItems_%1").arg(c->id()),
                       meter->serializeItems());
        }
        idList << c->id();
        if (m_floatingForms.contains(c->id())) {
            m_floatingForms[c->id()]->saveGeometry();
        }
    }

    s.setValue(QStringLiteral("ContainerIdList"), idList.join(QLatin1Char(',')));
    s.setValue(QStringLiteral("ContainerCount"), QString::number(m_containers.size()));
    saveSplitterState();

    qCDebug(lcContainer) << "Saved" << m_containers.size() << "container(s)";
}

void ContainerManager::restoreState()
{
    if (m_store) { reconcileWorkspace(m_store->snapshot()); restoreSplitterState(); return; }
    // From Thetis MeterManager.cs:6012-6105
    auto& s = AppSettings::instance();

    QString idList = s.value(QStringLiteral("ContainerIdList")).toString();
    if (idList.isEmpty()) {
        qCDebug(lcContainer) << "No saved containers found";
        return;
    }

    QStringList ids = idList.split(QLatin1Char(','), Qt::SkipEmptyParts);
    int restored = 0;

    for (const QString& id : ids) {
        QString data = s.value(QStringLiteral("ContainerData_%1").arg(id)).toString();
        if (data.isEmpty()) {
            continue;
        }

        auto* container = new ContainerWidget(nullptr);
        if (!container->deserialize(data)) {
            qCWarning(lcContainer) << "Failed to deserialize:" << id;
            delete container;
            continue;
        }

        // wireContainer BEFORE setContent so the contentChanged
        // listener catches the meterReadyForPolling emit on the
        // restore path. (Originally wireContainer ran after setContent
        // and listeners missed the announcement.)
        wireContainer(container);

        // Suppress interim meterReadyForPolling emissions. The
        // setContent below + setMeterFloating's extract/install cycle
        // would otherwise emit twice for a Floating container (once for
        // the placeholder meter, once for the fresh meter after
        // reparent); a single manual emit is sent at the end of this
        // iteration once the final content is in place.
        m_suppressMeterAnnouncements = true;

        // Materialize the inner content widget. MainWindow registers a
        // factory so the panel container gets an AppletPanelWidget;
        // when no factory is set (tests, headless tools) we default to
        // a bare MeterWidget which matches the user-created shape.
        QWidget* content = m_contentFactory
            ? m_contentFactory(container->id(), container->rxSource())
            : new MeterWidget();
        if (content) {
            container->setContent(content);
        }

        // Restore meter items into whichever MeterWidget the content
        // shape exposes (bare or wrapped). Empty payload → leave the
        // fresh meter empty so caller-side seeding can decide what to
        // do (Container #0's default presets, etc.).
        if (auto* meter = innerMeterWidget(content)) {
            const QString itemsPayload =
                s.value(QStringLiteral("ContainerItems_%1").arg(id)).toString();
            if (!itemsPayload.isEmpty()) {
                meter->deserializeItems(itemsPayload);
            }
        }

        auto* floatingForm = new FloatingContainer(container->rxSource());
        floatingForm->setScreenSource(m_screens);
        floatingForm->setId(container->id());

        m_containers.insert(container->id(), container);
        m_floatingForms.insert(container->id(), floatingForm);

        // Track the first restored container as the panel container, regardless
        // of its current dock mode. This ensures panelContainer() returns it so
        // MainWindow can populate its content (meters + applets).
        if (m_panelContainerId.isEmpty()) {
            m_panelContainerId = container->id();
        }

        switch (container->dockMode()) {
        case DockMode::PanelDocked:
            container->setParent(m_splitter);
            m_splitter->addWidget(container);
            m_panelContainerId = container->id();
            container->show();
            break;
        case DockMode::OverlayDocked:
            container->setParent(m_dockParent);
            container->restoreLocation();
            if (container->isContainerEnabled() && !container->isHiddenByMacro()) {
                container->show();
                container->raise();
            }
            break;
        case DockMode::Floating:
            container->resize(floatingForm->size());
            setMeterFloating(container, floatingForm);
            if (container->isContainerEnabled() && !container->isHiddenByMacro()) {
                floatingForm->show();
            }
            break;
        }

        // Interim emissions are done. Announce the final meter (if the
        // container has one) exactly once, then clear the guard so
        // post-restore user interactions announce normally.
        m_suppressMeterAnnouncements = false;
        if (auto* meter = innerMeterWidget(container->content())) {
            emit meterReadyForPolling(meter);
        }

        restored++;
        emit containerAdded(container->id());
    }

    restoreSplitterState();
    qCDebug(lcContainer) << "Restored" << restored << "container(s)";
}

} // namespace NereusSDR
