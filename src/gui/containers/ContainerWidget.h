#pragma once

// =================================================================
// src/gui/containers/ContainerWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/ucMeter.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - dragActive, and the press size a dragged floating window
//                 is held at. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: operatorSizesHeight() and
//                 sizingHeightNow(). A popped-out object's window keeps its
//                 resize corner while its height follows its content; a
//                 resize in height makes the height the operator's (JJ,
//                 2026-10-11). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: pinOnTopRequested, from the
//                 header's pin and the Options menu. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: a header is shown or hidden. The
//                 look's hover timers, fade, focus rule and preview seam
//                 are gone; the overlay opacity and shadow stay for Arrange.
//                 With the look off the three header modes are unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: lockRequested is gone with the
//                 Options menu's Lock item. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-08 - setContentMinimumWidth: the container is never narrower
//                 than its content needs. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the header source chip opens the
//                 source menu (slices A to D, Active slice) and emits
//                 sourceRequested. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the source menu ticks the slice
//                 the chip shows (setSourceMenuRx). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - publishUserBackground(): the user's background colour as a
//                 property its meters read (workspace preview). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-02 — Effective contextual draft properties and portable settings by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-21: the receiver source names slices A to D (saved
//                 1..4, so a saved RX1 / RX2 reads as slice A / B), and an
//                 unavailable button's reason is relayed. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21 fix wave: kFirstRxSource / kLastRxSource
//                 (slices A to D). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - vfoFilterContextRequested: the VFO display's filter
//                 right-click, apart from a filter button's. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview: the compact group header (Always in
//                 the layout, Reveal as a zero-space overlay, Hidden),
//                 Arrange presentation and inside resize handles. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - contentBackground() and userBackground(): the content
//                 background a container paints, shared with the Container
//                 Settings preview (workspace preview). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

/*  ucMeter.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

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

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "ContainerTypes.h"
#include "gui/NereusGuiExport.h"

#include <QWidget>
#include <QPointer>
#include <QTimer>
#include <QPoint>
#include <QSize>
#include <QColor>
#include <QString>
#include <cstdint>
#include <functional>

class QLabel;
class QPushButton;
class QGraphicsOpacityEffect;
class QMenu;

namespace NereusSDR {

struct BoardCapabilities;
class MeterItem;

class FloatingContainer;
class WorkspaceGroupHeader;

class NEREUS_GUI_EXPORT ContainerWidget : public QWidget {
    Q_OBJECT

public:
    static constexpr int kMinContainerWidth = 260;
    static constexpr int kMinContainerHeight = 24;
    static constexpr int kTitleBarHeight = 22;
    static constexpr int kTitleHoverZone = 40;  // larger target for hover reveal

    explicit ContainerWidget(QWidget* parent = nullptr);
    ~ContainerWidget() override;

    // --- Identity ---
    QString id() const { return m_id; }
    void setId(const QString& id);
    // R-R3-21: the slice this container's controls act on, saved as 1..4
    // for slices A to D (Thetis's RX 1 / RX 2 field, so a layout saved as
    // RX1 / RX2 reads as slice A / B). The slice id is rxSource() - 1.
    int rxSource() const { return m_rxSource; }
    void setRxSource(int rx);
    static int sliceIdForRxSource(int rx) { return rx - 1; }
    static QString sliceNameForRxSource(int rx);
    // The receiver sources a container offers: slices A (1) to D (4).
    static constexpr int kFirstRxSource = 1;
    static constexpr int kLastRxSource = 4;

    // --- Dock Mode ---
    DockMode dockMode() const { return m_dockMode; }
    void setDockMode(DockMode mode);
    bool isFloating() const { return m_dockMode == DockMode::Floating; }
    bool isPanelDocked() const { return m_dockMode == DockMode::PanelDocked; }
    bool isOverlayDocked() const { return m_dockMode == DockMode::OverlayDocked; }
    // The width the content needs (ContainerContentHost::contentMinimumWidth).
    // The container's minimum width is the larger of this and
    // kMinContainerWidth, in every dock mode.
    void setContentMinimumWidth(int width);
    int contentMinimumWidth() const { return m_contentMinimumWidth; }

    // --- Axis Lock ---
    AxisLock axisLock() const { return m_axisLock; }
    void setAxisLock(AxisLock lock);
    void cycleAxisLock(bool reverse = false);

    // --- Pin on Top (floating only) ---
    bool isPinOnTop() const { return m_pinOnTop; }
    void setPinOnTop(bool pin);

    // --- Container Properties ---
    QColor backgroundColor() const {return m_backgroundColor;}
    void setBackgroundColor(const QColor& color);
    // The colour a container configured with `configured` paints behind
    // its content: `configured`, or with the workspace preview on the
    // panel role for the default background. The Container Settings
    // preview paints the same.
    static QColor contentBackground(const QColor& configured);
    // The background the user chose (kUserBackgroundProperty): `configured`,
    // or an invalid QColor for the default background.
    static QColor userBackground(const QColor& configured);
    bool hasBorder() const { return m_border; }
    void setBorder(bool border);
    bool isLocked() const { return m_locked; }
    void setLocked(bool locked);
    bool isContainerEnabled() const { return m_enabled; }
    void setContainerEnabled(bool enabled);
    bool showOnRx() const { return m_showOnRx; }
    void setShowOnRx(bool show);
    bool showOnTx() const { return m_showOnTx; }
    void setShowOnTx(bool show);
    bool isHiddenByMacro() const { return m_hiddenByMacro; }
    void setHiddenByMacro(bool hidden);
    bool containerMinimises() const { return m_containerMinimises; }
    void setContainerMinimises(bool minimises);
    bool containerHidesWhenRxNotUsed() const { return m_containerHidesWhenRxNotUsed; }
    void setContainerHidesWhenRxNotUsed(bool hides);
    QString notes() const { return m_notes; }
    void setNotes(const QString& notes);
    bool noControls() const { return m_noControls; }
    void setNoControls(bool noControls);
    bool autoHeight() const { return m_autoHeight; }
    void setAutoHeight(bool autoHeight);
    // Workspace preview only: the window of one popped-out object. Its
    // height follows its content until the operator resizes it in height
    // (corner, or the bottom edge while arranging); that resize turns auto
    // height off, and the height is then theirs. Never less than the
    // content needs. Always false with the look off.
    bool operatorSizesHeight() const;
    // True while a corner or bottom edge drag of such a window is under way.
    bool sizingHeightNow() const;
    // What its content and header need at the width it has now.
    int naturalHeight() const;
    int containerHeight() const { return m_containerHeight; }

    // Title-bar visibility toggle — from Thetis chkContainerNoTitle
    // (setup.cs:24447). Persistent.
    bool geometryInteractionActive() const { return m_dragging || m_resizing; }
    // True while a header or title-bar drag moves the container.
    bool dragActive() const { return m_dragging; }
    void setPopOutShell(bool shell);
    void setHeaderMode(HeaderMode mode);
    HeaderMode headerMode() const { return m_headerMode; }
    bool chromeVisible() const;
    void recoverChrome();
    bool isTitleBarVisible() const { return m_titleBarVisible; }
    void setTitleBarVisible(bool visible);

    // --- Workspace preview header (WorkspacePreview::enabled() only) ---
    // A hidden header drawn over the content while arranging: its opacity
    // and the depth of the shadow under it.
    static constexpr double kOverlayOpacity = 0.96;
    static constexpr int kOverlayShadowPx = 6;
    static constexpr int kArrangeEdgePx = 4;
    // The header extent the layout reserves: the legacy 22 px slot with
    // the preview off; with it on, 18 px when the header is shown (and
    // while minimised) and 0 when it is hidden. A hidden header drawn
    // while arranging lies over the content and reserves nothing.
    int headerExtent() const;
    // App-level Arrange (ContainerManager::setArrangeMode): every header
    // shows, the outline and the inside resize handles appear. Not saved.
    void setArrangeActive(bool active);
    bool arrangeActive() const { return m_arrangeActive; }
    // The Arrange switch (the menu item and its shortcut), apart from the
    // Shift hold: what the menus' "Arrange Workspace" ticks and toggles.
    void setArrangeSwitchOn(bool on) { m_arrangeSwitchOn = on; }
    bool arrangeSwitchOn() const { return m_arrangeSwitchOn; }
    // Workspace preview: with the rule on, a header drag made while
    // arranging reports through the edge drag signals whatever the
    // container's dock mode, and the manager decides the drop at release.
    // A free or floating container still follows the pointer, and is not
    // held inside the pane meanwhile.
    void setArrangeDropRule(bool on) { m_arrangeDropRule = on; }
    bool arrangeDropRule() const { return m_arrangeDropRule; }
    // Where the header was pressed, from the container's top-left.
    QPoint dragGrabOffset() const { return m_grabOffset; }
    // The title the group header shows (a single-content container passes
    // its entry's name) and its source chip ("RX A"); empty hides the chip.
    void setWorkspaceTitle(const QString& title);
    void setWorkspaceSource(const QString& text, const QColor& colour);
    // The source menu the chip opens: slices A to D (a slice that does not
    // exist is disabled, not hidden) and Active slice. A choice emits
    // sourceRequested; the manager commits it as Container Settings does.
    // kActiveSliceSource is the Active slice choice (follow the selected
    // receiver); 1..4 are slices A to D.
    static constexpr int kActiveSliceSource = 0;
    // Whether a slice id (0 for A) exists; unset, every slice counts.
    void setSliceAvailability(std::function<bool(int sliceId)> available);
    // The saved choice the menu checks: Active slice, or rxSource().
    void setFollowsSelectedRx(bool follows) { m_followsSelectedRx = follows; }
    bool followsSelectedRx() const { return m_followsSelectedRx; }
    // Workspace preview: the receiver source (1 to 4) the chip shows, which
    // the source menu ticks; 0 falls back to rxSource().
    void setSourceMenuRx(int rxSource) { m_sourceMenuRx = rxSource; }
    void populateSourceMenu(QMenu& menu);
    WorkspaceGroupHeader* workspaceHeader() const { return m_groupHeader; }
    QWidget* contentHolder() const { return m_contentHolder; }
    // Workspace preview: the strip a panel-docked container sits in, and
    // the alignment grid for free and floating placement (0 is off; Alt
    // at release bypasses it).
    void setDockEdge(DockEdge edge) { m_dockEdge = edge; }
    DockEdge dockEdge() const { return m_dockEdge; }
    void setGridAlignment(int gridPx) { m_gridPx = gridPx; }
    int gridAlignment() const { return m_gridPx; }
    // True while a header drag is reporting through the edge drag signals
    // (ContainerManager shows the destination preview): a panel-docked
    // container's, or any container's while arranging.
    bool edgeDragActive() const { return m_edgeDragging; }

    // Minimised collapse state (runtime only; not persisted). Collapses
    // a floating container to just its title bar. From Thetis
    // ContainerMinimised runtime flag (MeterManager.cs usage). Whether a
    // container *can* be minimised is controlled by m_containerMinimises
    // above; whether it *is* currently minimised lives here.
    bool isMinimised() const { return m_minimised; }
    void setMinimised(bool minimised);

    // Highlight-for-setup runtime flag (not persisted). Paints a 2px
    // accent outline around the container frame while the settings
    // dialog is editing it. From Thetis chkContainerHighlight
    // (setup.cs:24443). Cleared on dialog close.
    bool isHighlighted() const { return m_highlighted; }
    void setHighlighted(bool highlighted);

    // --- Docked Position (overlay mode only) ---
    QPoint dockedLocation() const { return m_dockedLocation; }
    void setDockedLocation(const QPoint& loc);
    QSize dockedSize() const { return m_dockedSize; }
    void setDockedSize(const QSize& size);
    void storeLocation();
    void restoreLocation();

    // --- Delta (console resize offset for axis-lock) ---
    QPoint delta() const { return m_delta; }
    void setDelta(const QPoint& delta);

    // --- Content Slot ---
    QWidget* content() const { return m_content; }
    void setContent(QWidget* widget);

    // --- Serialization ---
    QString serialize() const;
    bool deserialize(const QString& data);

    // Wire interactive MeterItem signals to this container's forwarding signals.
    // Call after installing items into the container's MeterWidget.
    void wireInteractiveItem(MeterItem* item);

public slots:
    // Phase 3P-I-a T17 — push current board caps into every
    // AntennaButtonItem hosted inside this container. Called by
    // MainWindow on connect / currentRadioChanged. Stored locally so
    // items added later (via wireInteractiveItem) inherit the flag.
    void setBoardCapabilities(const NereusSDR::BoardCapabilities& caps);

signals:
    void geometryInteractionStarted();
    void geometryInteractionFinished();
    void returnContainerRequested();
    void hideContainerRequested();
    void headerModeRequested(HeaderMode mode);
    // Workspace preview: the operator asked for the window to be kept on
    // top, or not, from the header's pin or the Options menu. The widget
    // has already taken the state; the owner saves it.
    void pinOnTopRequested(bool pin);
    // Workspace preview: the options menu's Arrange workspace.
    void arrangeWorkspaceRequested(bool active);
    // Workspace preview: a source menu choice, kActiveSliceSource or 1..4.
    void sourceRequested(int rxSource);
    // Workspace preview: the options menu's Move to Edge and Place Free,
    // and a header drag (global pointer positions): a panel-docked
    // container's, or any container's while arranging. Only a release on
    // a valid destination commits; Escape or a lost mouse grab cancels.
    void moveToEdgeRequested(DockEdge edge);
    void placeFreeRequested();
    void edgeDragStarted();
    void edgeDragMoved(const QPoint& globalPos);
    void edgeDragFinished(const QPoint& globalPos);
    void edgeDragCancelled();
    void floatRequested();
    void dockRequested();
    void settingsRequested();
    void dockedMoved();
    void dockModeChanged(DockMode mode);
    void titleBarVisibilityChanged(bool visible);
    void minimisedChanged(bool minimised);
    void notesChanged(const QString& notes);
    // Emitted from setContent() after the new content widget is
    // installed. ContainerManager listens so it can scan for an inner
    // MeterWidget and announce it for poller registration.
    void contentChanged(QWidget* widget);

    // --- Phase 3G-5 interactive item signals ---
    // The band's number (static_cast<int>(Band)), from a band grid.
    void bandClicked(int band);
    void bandStackRequested(int band);
    void modeClicked(int modeIndex);
    void filterClicked(int filterIndex);
    void filterContextRequested(int filterIndex);
    /// The VFO display's filter right-click (it names no filter button).
    void vfoFilterContextRequested();
    void antennaSelected(int index);
    void tuneStepSelected(int stepIndex);
    void otherButtonClicked(int buttonId);
    void macroTriggered(int macroIndex);
    // R-R3-21: a click on an unavailable button; nothing changed.
    void unavailableButtonClicked(const QString& reason);
    void rxSourceChanged(int rx);
    void voiceAction(int action);
    void frequencyChangeRequested(int64_t deltaHz);

public:
    // Re-apply pin-on-top window flag (call after reparenting to FloatingContainer)
    void setTopMost();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // Sets WorkspacePreview::kUserBackgroundProperty from m_backgroundColor:
    // the colour when the user chose one, an invalid colour for the default.
    void publishUserBackground();
    void updateChrome();
    void updateWorkspaceChrome();
    void buildWorkspaceHeader();
    void placeWorkspaceOverlays();
    void showWorkspaceOptions();
    void showSourceMenu();
    void requestPinOnTop(bool pin);
    void populateWorkspaceMenu(QMenu& menu);
    void buildUI();
    void updateTitleBar();
    void updateTitle();
    void setupBorder();

    void beginDrag(const QPoint& pos);
    void updateDrag(const QPoint& globalPos);
    void endDrag();
    void cancelDrag();
    // Ends a drag made under the arrange drop rule: cancelled, the
    // container is back where it started.
    void finishArrangeDrag(bool cancelled, const QPoint& globalPos);
    // Puts a dragged floating window back at its press size; true when it
    // had to. Never called from inside a resize event.
    bool holdDragSize();
    void endDragSizeHold();
    void endEdgeDrag(bool cancelled, const QPoint& globalPos);

    void beginResize(const QPoint& globalPos);
    void updateResize(const QPoint& globalPos);
    void endResize();
    void doResize(int w, int h);

    static int roundToNearestTen(int value);
    static QString axisLockToString(AxisLock lock);
    static AxisLock axisLockFromString(const QString& str);
    static QString dockModeToString(DockMode mode);
    static DockMode dockModeFromString(const QString& str);

    // --- Identity ---
    QString m_id;
    int m_rxSource{1};
    int m_contentMinimumWidth{0};

    // --- State ---
    DockMode m_dockMode{DockMode::OverlayDocked};
    bool m_dragging{false};
    bool m_resizing{false};
    AxisLock m_axisLock{AxisLock::TopLeft};
    bool m_pinOnTop{false};

    // --- Properties (defaults from Thetis ucMeter.cs:95-103) ---
    bool m_border{true};
    bool m_noControls{false};
    bool m_locked{false};
    bool m_enabled{true};
    bool m_showOnRx{true};
    bool m_showOnTx{true};
    bool m_hiddenByMacro{false};
    bool m_containerMinimises{true};
    bool m_containerHidesWhenRxNotUsed{true};
    QString m_notes;
    int m_containerHeight{kMinContainerHeight};
    bool m_autoHeight{false};
    bool m_titleBarVisible{true};
    bool m_minimised{false};          // runtime only, not persisted
    bool m_highlighted{false};        // runtime only, not persisted
    QColor m_backgroundColor{0x0f, 0x0f, 0x1a};

    // --- Drag/Resize state ---
    QPoint m_dragStartPos;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;
    // A drag moves a floating window and never resizes it: the window's
    // size at the press, put back whenever it is found different while the
    // drag lasts (holdDragSize). Not valid outside a floating drag.
    QSize m_dragHoldSize;
    // One warning per drag.
    bool m_dragSizeWarned{false};
    // The resize that puts the size back is under way; nothing answers it.
    bool m_restoringDragSize{false};
    // A resize arrived mid-drag: one check is queued, and at most one such
    // check restores the size between two pointer moves, so a platform
    // that answers every restore with another resize cannot keep a loop
    // going through the event queue.
    bool m_dragSizeCheckQueued{false};
    bool m_dragSizeQueuedRestoreDone{false};

    // --- Docked geometry ---
    QPoint m_dockedLocation;
    QSize m_dockedSize;
    QPoint m_delta;

    // --- UI elements ---
    QWidget* m_headerSlot{nullptr};
    HeaderMode m_headerMode{HeaderMode::Shown};
    bool m_structuredChrome{false};
    bool m_popOutShell{false};
    bool m_hoverChrome{false};
    bool m_recoverChrome{false};
    bool m_shiftChrome{false};
    QWidget* m_titleBar{nullptr};
    QWidget* m_contentHolder{nullptr};
    QWidget* m_content{nullptr};
    QWidget* m_resizeGrip{nullptr};
    QLabel* m_titleLabel{nullptr};
    QPushButton* m_btnFloat{nullptr};
    QPushButton* m_btnAxis{nullptr};
    QPushButton* m_btnPin{nullptr};
    QPushButton* m_btnSettings{nullptr};

    // --- Workspace preview header ---
    WorkspaceGroupHeader* m_groupHeader{nullptr};
    QWidget* m_overlayShadow{nullptr};
    QWidget* m_arrangeFrame{nullptr};
    QWidget* m_edgeRight{nullptr};
    QWidget* m_edgeBottom{nullptr};
    QGraphicsOpacityEffect* m_headerOpacity{nullptr};
    bool m_arrangeActive{false};
    bool m_arrangeSwitchOn{false};
    bool m_arrangeDropRule{false};
    // The drag in progress follows the arrange drop rule.
    bool m_arrangeDrag{false};
    QPoint m_grabOffset;
    bool m_followsSelectedRx{false};
    int m_sourceMenuRx{0};
    std::function<bool(int)> m_sliceAvailable;
    Qt::Orientations m_resizeAxes{Qt::Horizontal | Qt::Vertical};
    DockEdge m_dockEdge{DockEdge::Right};
    int m_gridPx{0};
    bool m_edgePressed{false};
    bool m_edgeDragging{false};
    QPoint m_edgePressGlobal;
    QPoint m_dragOrigin;

    // Phase 3P-I-a T17 — cached hasAlex flag so items added via
    // wireInteractiveItem() after setBoardCapabilities() still inherit
    // the current gating. Defaults to true (no gating) until set.
    bool m_hasAlex{true};
};

} // namespace NereusSDR
