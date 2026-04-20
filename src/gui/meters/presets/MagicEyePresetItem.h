// =================================================================
// src/gui/meters/presets/MagicEyePresetItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddMagicEye, line 22249)
//   verbatim upstream header reproduced below; the .cpp carries the
//   same block in front of the calibration tables.
//
// =================================================================

// --- From MeterManager.cs ---
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

// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7. First-class MeterItem subclass
//                collapsing the Magic Eye preset (bullseye image +
//                leaf) into one entity. 3-point calibration
//                (-127 -> 0, -73 -> 0.85, -13 -> 1) is byte-for-byte
//                from Thetis AddMagicEye.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QImage>
#include <QMap>
#include <QPointF>

namespace NereusSDR {

// MagicEyePresetItem — first-class Magic Eye preset.
//
// Collapses the eye-bezel image + MagicEyeItem composition emitted by
// `ItemGroup::createMagicEyePreset()` into one MeterItem. Leaf colour
// and calibration are byte-for-byte from Thetis AddMagicEye
// (MeterManager.cs:22249).
class MagicEyePresetItem : public MeterItem
{
    Q_OBJECT

public:
    explicit MagicEyePresetItem(QObject* parent = nullptr);

    // --- MeterItem overrides ---
    // Background -> MeterWidget calls paintForLayer() -> paint();
    // primitives (BarItem / NeedleItem / etc.) on Geometry /
    // OverlayDynamic paint on top. This matches the user's mental
    // model that a preset is the "background meter" the container
    // shows, with the in-use list's item order driving z-order
    // (top of list = behind, bottom = front). Preset classes
    // override paint() only (not emitVertices()), so Layer::Geometry
    // would route through the empty GPU vertex path.
    Layer renderLayer() const override { return Layer::Background; }
    void  paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool    deserialize(const QString& data) override;

    // --- Composition introspection ---
    bool hasBackground() const { return true; }
    bool hasLeaf() const { return true; }

    // --- Leaf colour ---
    QColor leafColor() const { return m_leafColor; }
    void   setLeafColor(const QColor& c) { m_leafColor = c; }

private:
    // Interpolates the leaf aperture fraction [0..1] from a dBm value
    // using the 3-point Thetis calibration.
    float apertureFraction(double dbm) const;

    QImage               m_background;
    QColor               m_leafColor;
    QMap<double, float>  m_calibration;
};

} // namespace NereusSDR
