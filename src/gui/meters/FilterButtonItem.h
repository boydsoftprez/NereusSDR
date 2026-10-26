#pragma once

// =================================================================
// src/gui/meters/FilterButtonItem.h  (NereusSDR)
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
//   2026-10-26 - Every slice's grid has the 12 buttons Thetis gives RX1
//                 (kFilterCount public, with the Var buttons' indices).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "ButtonBoxItem.h"

namespace NereusSDR {

// Filter preset buttons: F1-F10, Var1, Var2.
// Ported from Thetis clsFilterButtonBox (MeterManager.cs:7674+).
class FilterButtonItem : public ButtonBoxItem {
    Q_OBJECT

public:
    // From Thetis MeterManager.cs:7718-7727 [v2.10.3.15]: RX1's grid has
    // Buttons = 12 and RX2's Buttons = 9. Every NereusSDR slice has all 12
    // (F1-F10, Var1, Var2), so a grid on Slice B shows what Slice A's does.
    static constexpr int kFilterCount = 12;
    // Var1 and Var2 (Thetis Filter.VAR1 / VAR2) come after F1-F10.
    static constexpr int kVar1Button = 10;
    static constexpr int kVar2Button = 11;

    explicit FilterButtonItem(QObject* parent = nullptr);

    void setActiveFilter(int index);
    int activeFilter() const { return m_activeFilter; }

    void setFilterLabel(int index, const QString& label);
    QString filterLabel(int index) const
    {
        return (index >= 0 && index < buttonCount()) ? button(index).text : QString();
    }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    QString serialize() const override;
    bool deserialize(const QString& data) override;

    // The column counts this grid offers (Thetis setup.cs).
    std::pair<int, int> columnRange() const override;

signals:
    void filterClicked(int filterIndex);
    // From Thetis PopupFilterContextMenu (MeterManager.cs:7917)
    void filterContextRequested(int filterIndex);

private:
    void onButtonClicked(int index, Qt::MouseButton button);
    int m_activeFilter{-1};
};

} // namespace NereusSDR
