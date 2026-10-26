#pragma once

// =================================================================
// src/gui/meters/ClickBoxItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-26 — Container item parity Task 47: the mouse button mask and
//                 the go to group, change group and cycle units actions of
//                 Thetis clsClickBox / clsMeter.MouseUp. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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

#include "MeterItem.h"
class QMouseEvent;
class QWheelEvent;

namespace NereusSDR {
// Invisible hit target for mouse interaction overlay.
// From Thetis clsClickBox (MeterManager.cs:7597-7652 [v2.10.3.15]).
//
// A click with a button in buttonMask() emits actionRequested. The
// MeterWidget that holds the box acts on it (MeterWidget::addItem): go to a
// display group, step the display group, or step the units of the topmost
// item under the box that has units. A copy outside a widget, or in another
// widget, acts on nothing else.
class ClickBoxItem : public MeterItem {
    Q_OBJECT
public:
    // Thetis keeps GotoGroup (> 0 wins), ChangesGroup and PerformsIncDec
    // on every box (MeterManager.cs:29814-29848 [v2.10.3.15]); NereusSDR
    // gives a box one of them.
    enum class Action { GoToGroup, ChangeGroup, CycleUnits };
    Q_ENUM(Action)

    explicit ClickBoxItem(QObject* parent = nullptr);
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleWheel(QWheelEvent* event, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

    // The buttons the box answers (Thetis Button).
    void setButtonMask(Qt::MouseButtons mask) { m_buttonMask = mask; }
    Qt::MouseButtons buttonMask() const { return m_buttonMask; }
    void setAction(Action action) { m_action = action; }
    Action action() const { return m_action; }
    // The group GoToGroup shows (Thetis GotoGroup); 0 shows every group.
    void setGoToGroup(int group) { m_goToGroup = qBound(0, group, kMaxDisplayGroup); }
    int goToGroup() const { return m_goToGroup; }

    // The highest group the item editors offer (BaseItemEditor).
    static constexpr int kMaxDisplayGroup = 15;

    // Whether CycleUnits can step `item`'s units: a bar or a signal text,
    // the two Thetis items with HandleIncrement (MeterManager.cs:21539,
    // 21836 [v2.10.3.15]).
    static bool supportsUnits(const MeterItem* item);
    // `unit` stepped once forward (step > 0) or back, in the order of
    // SignalTextItem::Units (dBm, S units, uV), wrapping at either end.
    // A PBSNR reading has no uV.
    static MeterItem::MeterUnit steppedUnit(MeterItem::MeterUnit unit, int step, bool pbSnr);

signals:
    void clicked();
    void wheelIncrement();
    void wheelDecrement();
    // A click in the mask. `value`: the group for GoToGroup; +1 (left
    // button) or -1 (right button) for ChangeGroup and CycleUnits.
    void actionRequested(NereusSDR::ClickBoxItem::Action action, int value);

private:
    Qt::MouseButtons m_buttonMask;
    Action m_action;
    int m_goToGroup{0};
};

// A new click box's settings.
struct ClickBoxSettings {
    Qt::MouseButtons buttonMask;
    ClickBoxItem::Action action;
    int goToGroup;
};
// From Thetis clsClickBox() (MeterManager.cs:7605-7613 [v2.10.3.15]):
// left, right and middle buttons, changes group, no go to group.
inline constexpr ClickBoxSettings kClickBoxThetisDefaults{
    Qt::LeftButton | Qt::RightButton | Qt::MiddleButton,
    ClickBoxItem::Action::ChangeGroup,
    0};
} // namespace NereusSDR
