// =================================================================
// src/gui/widgets/ParametricEqWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucParametricEq.cs [v2.10.3.13],
//   original licence from Thetis source is included below.
//   Sole author: Richard Samphire (MW0LGE) — GPLv2-or-later with
//   Samphire dual-licensing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Reimplemented in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.  Phase 3M-3a-ii follow-up
//                 sub-PR Batch 1: skeleton + EqPoint/EqJsonState classes
//                 + default 18-color palette + ctor with verbatim
//                 ucParametricEq.cs:360-447 defaults.
// =================================================================

/*  ucParametricEq.cs

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

#include "ParametricEqWidget.h"

#include <QtGlobal>

namespace NereusSDR {

// From Thetis ucParametricEq.cs:254-274 [v2.10.3.13] -- _default_band_palette.
// 18 RGB triples, verbatim.  Ports byte-for-byte; do not reorder, recolor, or trim.
const QVector<QColor>& ParametricEqWidget::defaultBandPalette() {
    static const QVector<QColor> kPalette = {
        QColor(  0, 190, 255),
        QColor(  0, 220, 130),
        QColor(255, 210,   0),
        QColor(255, 140,   0),
        QColor(255,  80,  80),
        QColor(255,   0, 180),
        QColor(170,  90, 255),
        QColor( 70, 120, 255),
        QColor(  0, 200, 200),
        QColor(180, 255,  90),
        QColor(255, 105, 180),
        QColor(255, 215, 120),
        QColor(120, 255, 255),
        QColor(140, 200, 255),
        QColor(220, 160, 255),
        QColor(255, 120,  40),
        QColor(120, 255, 160),
        QColor(255,  60, 120),
    };
    return kPalette;
}

int ParametricEqWidget::defaultBandPaletteSize() {
    return defaultBandPalette().size();
}

QColor ParametricEqWidget::defaultBandPaletteAt(int index) {
    const auto& p = defaultBandPalette();
    if (index < 0 || index >= p.size()) {
        return QColor();
    }
    return p.at(index);
}

// From Thetis ucParametricEq.cs:360-447 [v2.10.3.13] -- public ucParametricEq() ctor.
// Skeleton implementation only -- Task 2 will add the resetPointsDefault() call once axis math
// + ordering helpers exist.
ParametricEqWidget::ParametricEqWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Bar chart peak-hold timer -- 33 ms cadence (~30 fps), matches WinForms Timer { Interval = 33 }
    // at ucParametricEq.cs:428-430.  Slot wired in Task 3 (painting batch).
    m_barChartPeakTimer = new QTimer(this);
    m_barChartPeakTimer->setInterval(33);

    // Skeleton leaves m_points empty until Task 2 implements resetPointsDefault().
}

ParametricEqWidget::~ParametricEqWidget() = default;

} // namespace NereusSDR
