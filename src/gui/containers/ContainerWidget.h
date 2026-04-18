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
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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

#include <QWidget>
#include <QPoint>
#include <QSize>
#include <QColor>
#include <QString>
#include <cstdint>

class QLabel;
class QPushButton;

namespace NereusSDR {

class MeterItem;

// From Thetis ucMeter.cs:49-59 — axis lock positions for docked containers.
enum class AxisLock {
    Left = 0, TopLeft, Top, TopRight,
    Right, BottomRight, Bottom, BottomLeft
};

// Docking mode for a container within the application layout.
enum class DockMode {
    PanelDocked,    // In QSplitter (Container #0 default)
    OverlayDocked,  // Absolute position over central widget (Thetis style)
    Floating        // Separate window
};

class FloatingContainer;

class ContainerWidget : public QWidget {
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
    int rxSource() const { return m_rxSource; }
    void setRxSource(int rx);

    // --- Dock Mode ---
    DockMode dockMode() const { return m_dockMode; }
    void setDockMode(DockMode mode);
    bool isFloating() const { return m_dockMode == DockMode::Floating; }
    bool isPanelDocked() const { return m_dockMode == DockMode::PanelDocked; }
    bool isOverlayDocked() const { return m_dockMode == DockMode::OverlayDocked; }

    // --- Axis Lock ---
    AxisLock axisLock() const { return m_axisLock; }
    void setAxisLock(AxisLock lock);
    void cycleAxisLock(bool reverse = false);

    // --- Pin on Top (floating only) ---
    bool isPinOnTop() const { return m_pinOnTop; }
    void setPinOnTop(bool pin);

    // --- Container Properties ---
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
    int containerHeight() const { return m_containerHeight; }

    // Title-bar visibility toggle — from Thetis chkContainerNoTitle
    // (setup.cs:24447). Persistent.
    bool isTitleBarVisible() const { return m_titleBarVisible; }
    void setTitleBarVisible(bool visible);

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

signals:
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
    void bandClicked(int bandIndex);
    void bandStackRequested(int bandIndex);
    void modeClicked(int modeIndex);
    void filterClicked(int filterIndex);
    void filterContextRequested(int filterIndex);
    void antennaSelected(int index);
    void tuneStepSelected(int stepIndex);
    void otherButtonClicked(int buttonId);
    void macroTriggered(int macroIndex);
    void voiceAction(int action);
    void discordAction(int action);
    void frequencyChangeRequested(int64_t deltaHz);

public:
    // Re-apply pin-on-top window flag (call after reparenting to FloatingContainer)
    void setTopMost();

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUI();
    void updateTitleBar();
    void updateTitle();
    void setupBorder();

    void beginDrag(const QPoint& pos);
    void updateDrag(const QPoint& globalPos);
    void endDrag();

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

    // --- Docked geometry ---
    QPoint m_dockedLocation;
    QSize m_dockedSize;
    QPoint m_delta;

    // --- UI elements ---
    QWidget* m_titleBar{nullptr};
    QWidget* m_contentHolder{nullptr};
    QWidget* m_content{nullptr};
    QWidget* m_resizeGrip{nullptr};
    QLabel* m_titleLabel{nullptr};
    QPushButton* m_btnFloat{nullptr};
    QPushButton* m_btnAxis{nullptr};
    QPushButton* m_btnPin{nullptr};
    QPushButton* m_btnSettings{nullptr};
};

} // namespace NereusSDR
