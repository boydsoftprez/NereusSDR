#pragma once

// =================================================================
// src/gui/meters/SpacerItem.h  (NereusSDR)
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
#include <QColor>

namespace NereusSDR {

class SpacerItem : public MeterItem {
    Q_OBJECT
public:
    explicit SpacerItem(QObject* parent = nullptr);

    // From Thetis MeterManager.cs:16143 — Colour1 / Colour2 / Padding
    void setColour1(const QColor& c) { m_colour1 = c; }
    QColor colour1() const { return m_colour1; }

    void setColour2(const QColor& c) { m_colour2 = c; }
    QColor colour2() const { return m_colour2; }

    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // From Thetis MeterManager.cs:16123 — default Color.FromArgb(32, 32, 32)
    QColor m_colour1{0x20, 0x20, 0x20};
    QColor m_colour2{0x20, 0x20, 0x20};
    // From Thetis MeterManager.cs:16126 — default 0.1f
    float  m_padding{0.1f};
};

} // namespace NereusSDR
