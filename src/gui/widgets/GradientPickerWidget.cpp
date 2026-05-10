// =================================================================
// src/gui/widgets/GradientPickerWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucLGPicker.cs (full UserControl,
//   1009 lines), original licence from Thetis source is included
//   below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7 (1M context).
//                Phase 3M-5c (Custom Gradient Picker widget).
//                STEP 2.2 STUB: this file contains compile-only
//                method bodies so the TDD red state shows the test
//                cases failing rather than failing to link. Step 2.5
//                / 2.6 fill in the real serialization + interpolation
//                logic; Steps 2.8-2.9 land paint + mouse handling.
// =================================================================
//
// --- From ucLGPicker.cs ---
/*  ucLGPicker.cs

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

#include "gui/widgets/GradientPickerWidget.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>

namespace NereusSDR {

// Step 2.2 (TDD red): all method bodies below are stubs sufficient to
// compile + link the test executable. Real implementations land in Steps
// 2.3-2.9. Stubs return defaults / empty / -1 / false so each test fails
// at its first assertion rather than failing to link.

GradientPickerWidget::GradientPickerWidget(QWidget* parent)
    : QWidget(parent)
{
}

GradientPickerWidget::~GradientPickerWidget() = default;

void GradientPickerWidget::setShowAsPercent(bool /*on*/)
{
}

void GradientPickerWidget::setIncludeAlphaInPreview(bool /*on*/)
{
}

QColor GradientPickerWidget::colorForSelectedStop() const
{
    return QColor();
}

void GradientPickerWidget::setColorForSelectedStop(const QColor& /*c*/)
{
}

QString GradientPickerWidget::text() const
{
    return QString();
}

void GradientPickerWidget::setText(const QString& /*s*/)
{
}

QString GradientPickerWidget::encodedText() const
{
    return QString();
}

void GradientPickerWidget::setEncodedText(const QString& /*s*/)
{
}

QColor GradientPickerWidget::colorAtPercent(float /*perc*/) const
{
    return QColor();
}

QColor GradientPickerWidget::colorForDbm(float /*dbm*/) const
{
    return QColor();
}

QVector<QColor> GradientPickerWidget::colorTable(int /*count*/) const
{
    return QVector<QColor>();
}

int GradientPickerWidget::addStop(float /*perc*/)
{
    return -1;
}

bool GradientPickerWidget::removeSelectedStop()
{
    return false;
}

void GradientPickerWidget::highlightFirstStop()
{
}

QVector<GradientStop> GradientPickerWidget::gradientStops() const
{
    return QVector<GradientStop>();
}

#ifdef NEREUS_BUILD_TESTS
QVector<GradientStop> GradientPickerWidget::stopsForTest() const
{
    return m_stops;
}

void GradientPickerWidget::setStopsForTest(const QVector<GradientStop>& stops)
{
    m_stops = stops;
}

void GradientPickerWidget::setSelectedStopForTest(int index)
{
    m_selectedIndex = index;
}

int GradientPickerWidget::selectedStopForTest() const
{
    return m_selectedIndex;
}
#endif

// ---- Helpers (Step 2.5+) ----
int  GradientPickerWidget::findStopIndexAtPixel(int /*x*/, int /*y*/) const { return -1; }
int  GradientPickerWidget::indexAtPercent(float /*perc*/) const             { return -1; }
int  GradientPickerWidget::indexClosestLeft(float /*perc*/) const           { return -1; }
int  GradientPickerWidget::indexClosestLeft(int /*index*/) const            { return -1; }
int  GradientPickerWidget::indexClosestRight(float /*perc*/) const          { return -1; }
int  GradientPickerWidget::indexClosestRight(int /*index*/) const           { return -1; }
int  GradientPickerWidget::findFreeDisabledSlot() const                     { return -1; }
int  GradientPickerWidget::actualWidth() const                              { return std::max(0, width() - kPaddingPx * 2); }
float GradientPickerWidget::percFromPixel(int /*x*/) const                  { return 0.0f; }
int  GradientPickerWidget::percentToPixel(float /*perc*/) const             { return 0; }
int  GradientPickerWidget::pixelToDbm(int /*x*/) const                      { return 0; }
int  GradientPickerWidget::percentToDbm(float /*perc*/) const               { return 0; }

void GradientPickerWidget::rebuildSortedStops()                              {}
void GradientPickerWidget::emitChangedAndUpdate()                            {}
void GradientPickerWidget::drawScales(QPainter& /*painter*/) const           {}
void GradientPickerWidget::drawTextCentered(QPainter& /*painter*/,
                                             const QString& /*s*/,
                                             int /*x*/) const                {}

// ---- Event handlers (Step 2.8 / 2.9) ----
void GradientPickerWidget::paintEvent(QPaintEvent* /*event*/)               {}
void GradientPickerWidget::mousePressEvent(QMouseEvent* /*event*/)          {}
void GradientPickerWidget::mouseMoveEvent(QMouseEvent* /*event*/)           {}
void GradientPickerWidget::mouseReleaseEvent(QMouseEvent* /*event*/)        {}
void GradientPickerWidget::mouseDoubleClickEvent(QMouseEvent* /*event*/)    {}
void GradientPickerWidget::contextMenuEvent(QContextMenuEvent* /*event*/)   {}
void GradientPickerWidget::leaveEvent(QEvent* /*event*/)                    {}

} // namespace NereusSDR
