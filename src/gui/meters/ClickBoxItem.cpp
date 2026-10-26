// =================================================================
// src/gui/meters/ClickBoxItem.cpp  (NereusSDR)
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
//                 Thetis clsClickBox / clsMeter.MouseUp, saved after the
//                 record's base fields. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
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

#include "ClickBoxItem.h"
#include "SignalTextItem.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

namespace NereusSDR {

namespace {
// The record before the action fields: CLICKBOX|x|y|w|h|binding|z.
constexpr int kBaseFieldCount = 7;
constexpr int kFieldCount = 10;

// SignalTextItem::Units order (Thetis Units: DBM = 0, S_UNTS, U_V,
// MeterManager.cs:21240-21245, 21608-21613 [v2.10.3.15]).
int thetisUnitIndex(MeterItem::MeterUnit unit)
{
    switch (unit) {
        case MeterItem::MeterUnit::S:  return static_cast<int>(SignalTextItem::Units::SUnits);
        case MeterItem::MeterUnit::uV: return static_cast<int>(SignalTextItem::Units::Uv);
        default:                       return static_cast<int>(SignalTextItem::Units::Dbm);
    }
}

MeterItem::MeterUnit meterUnit(int thetisIndex)
{
    switch (static_cast<SignalTextItem::Units>(thetisIndex)) {
        case SignalTextItem::Units::SUnits: return MeterItem::MeterUnit::S;
        case SignalTextItem::Units::Uv:     return MeterItem::MeterUnit::uV;
        default:                            return MeterItem::MeterUnit::dBm;
    }
}
} // namespace

ClickBoxItem::ClickBoxItem(QObject* parent)
    : MeterItem(parent)
    , m_buttonMask(kClickBoxThetisDefaults.buttonMask)
    , m_action(kClickBoxThetisDefaults.action)
    , m_goToGroup(kClickBoxThetisDefaults.goToGroup)
{
}

void ClickBoxItem::paint(QPainter& p, int widgetW, int widgetH)
{
    Q_UNUSED(p); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
}

bool ClickBoxItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    const Qt::MouseButton button = event->button();
    // NereusSDR: Thetis stores Button but MouseUp acts on any button; the
    // box here answers only the buttons in its mask.
    if (button == Qt::NoButton || !m_buttonMask.testFlag(button)) { return false; }
    if (button == Qt::LeftButton) { emit clicked(); }
    // From Thetis MeterManager.cs:29814-29848 [v2.10.3.15] (MouseUp): a go
    // to group answers any button; changing group and inc/dec answer left
    // (increment) and right (decrement) only.
    if (m_action == Action::GoToGroup) {
        emit actionRequested(m_action, m_goToGroup);
        return true;
    }
    int step = 0;
    if (button == Qt::LeftButton) { step = 1; }
    else if (button == Qt::RightButton) { step = -1; }
    if (step == 0) { return false; }
    emit actionRequested(m_action, step);
    return true;
}

bool ClickBoxItem::handleWheel(QWheelEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    if (event->angleDelta().y() > 0) { emit wheelIncrement(); }
    else { emit wheelDecrement(); }
    return true;
}

bool ClickBoxItem::supportsUnits(const MeterItem* item)
{
    return qobject_cast<const SignalTextItem*>(item) != nullptr
        || qobject_cast<const BarItem*>(item) != nullptr;
}

// From Thetis MeterManager.cs:21539-21573 [v2.10.3.15] (clsBarItem
// HandleIncrement / HandleDecrement); clsSignalText's are the same
// (MeterManager.cs:21836-21860), except that its decrement wraps to S units
// for every reading. That skips uV going back from dBm, so NereusSDR's
// signal text wraps as the bar does.
MeterItem::MeterUnit ClickBoxItem::steppedUnit(MeterItem::MeterUnit unit, int step, bool pbSnr)
{
    const int first = static_cast<int>(SignalTextItem::Units::Dbm);
    const int last = static_cast<int>(pbSnr ? SignalTextItem::Units::SUnits : SignalTextItem::Units::Uv);
    int n = thetisUnitIndex(unit);
    if (step > 0) {
        n++;
        if (n > last) { n = first; }
    } else if (step < 0) {
        n--;
        if (n < first) { n = last; }
    }
    return meterUnit(n);
}

QString ClickBoxItem::serialize() const
{
    // NereusSDR: the action fields follow the base fields; a record saved
    // before them loads with Thetis's defaults.
    return QStringLiteral("CLICKBOX|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h).arg(m_bindingId).arg(m_zOrder)
        .arg(m_buttonMask.toInt()).arg(static_cast<int>(m_action)).arg(m_goToGroup);
}

bool ClickBoxItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < kBaseFieldCount || parts[0] != QLatin1String("CLICKBOX")) { return false; }
    Qt::MouseButtons mask = kClickBoxThetisDefaults.buttonMask;
    Action action = kClickBoxThetisDefaults.action;
    int goToGroup = kClickBoxThetisDefaults.goToGroup;
    if (parts.size() >= kFieldCount) {
        bool maskOk = false;
        bool actionOk = false;
        bool groupOk = false;
        const int maskValue = parts[7].toInt(&maskOk);
        const int actionValue = parts[8].toInt(&actionOk);
        const int groupValue = parts[9].toInt(&groupOk);
        const Qt::MouseButtons known = kClickBoxThetisDefaults.buttonMask;
        if (!maskOk || !actionOk || !groupOk || (maskValue & ~known.toInt()) != 0
            || actionValue < static_cast<int>(Action::GoToGroup)
            || actionValue > static_cast<int>(Action::CycleUnits)
            || groupValue < 0 || groupValue > kMaxDisplayGroup) {
            return false;
        }
        mask = Qt::MouseButtons::fromInt(maskValue);
        action = static_cast<Action>(actionValue);
        goToGroup = groupValue;
    }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    m_buttonMask = mask;
    m_action = action;
    m_goToGroup = goToGroup;
    return true;
}

} // namespace NereusSDR
