// =================================================================
// src/gui/meters/presets/CrossNeedleItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddCrossNeedle, line 22817)
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
//                collapsing the Cross Needle preset (forward +
//                reflected power with dual calibration) into a single
//                entity. Binding IDs map to NereusSDR MeterBinding
//                (TxPower / TxReversePower) rather than Thetis's
//                MMIOVariableIndex slot ordinal.
//   2026-04-19 — Arc-fix: needle pivot/tip anchored to letterboxed
//                background image rect via bgRect() helper, so the
//                arc stays glued to the meter face at non-default
//                container aspect ratios. Shared geometry helper in
//                PresetGeometry.h.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QImage>
#include <QMap>
#include <QPointF>
#include <QRect>
#include <QString>
#include <array>

namespace NereusSDR {

// CrossNeedleItem — first-class Cross Needle preset.
//
// Encapsulates the forward/reflected power composite (background
// cross-needle.png image + optional band overlay cross-needle-bg.png
// + forward needle bound to TxPower + reflected needle bound to
// TxReversePower, each with its own multi-point calibration table) as
// a single MeterItem. Collapses the 6-child output of
// `ItemGroup::createCrossNeedlePreset()` into one row / one editor /
// one persistence blob.
class CrossNeedleItem : public MeterItem
{
    Q_OBJECT

public:
    static constexpr int kNeedleCount = 2;

    explicit CrossNeedleItem(QObject* parent = nullptr);

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

    // --- Needle introspection ---
    int                 needleCount() const { return kNeedleCount; }
    QString             needleName(int i) const;
    QMap<float, QPointF> needleCalibration(int i) const;
    QColor              needleColor(int i) const;
    bool                needleVisible(int i) const;
    void                setNeedleVisible(int i, bool v);

    // --- Band overlay toggle ---
    // Thetis AddCrossNeedle draws two images: the main cross-needle.png
    // behind the needles (z=1) and a small cross-needle-bg.png strip
    // along the bottom (z=5). Expose the bottom-strip toggle so tests
    // and the property pane can suppress it independently.
    bool showBandOverlay() const { return m_showBandOverlay; }
    void setShowBandOverlay(bool v) { m_showBandOverlay = v; }

    // --- Arc-fix: anchor needle geometry to background image rect ---
    // When true (default), pivot/tip math is computed against the
    // letterboxed bgRect() rather than the container's stretched item
    // rect — same fix AnanMultiMeterItem uses to keep the arc glued
    // to the meter face at non-default aspect ratios.
    bool anchorToBgRect() const { return m_anchorToBgRect; }
    void setAnchorToBgRect(bool v) { m_anchorToBgRect = v; }

private:
    struct Needle {
        QString              name;
        int                  bindingId{-1};
        QMap<float, QPointF> calibration;
        QColor               color;
        bool                 visible{true};
        // Per-needle pivot (Thetis `NeedleOffset`); negative X mirrors
        // the reflected-power needle onto the right-hand side of the
        // cross-needle face.
        QPointF              needleOffset{0.0, 0.0};
        // Per-needle reach scaler — Thetis `clsNeedleItem.LengthFactor`.
        float                lengthFactor{1.0f};
    };

    void   initialiseNeedles();
    QRect  bgRect(int widgetW, int widgetH) const;
    QPointF calibratedPosition(const Needle& n, float value) const;
    void   paintNeedle(QPainter& p, const Needle& n, const QRect& rect) const;

    QImage                             m_background;
    QImage                             m_bandOverlay;
    bool                               m_showBandOverlay{true};
    // Arc-fix: when true, needle geometry anchors to the letterboxed
    // background image rect rather than the raw container rect. Same
    // default / toggle semantics as AnanMultiMeterItem::m_anchorToBgRect.
    bool                               m_anchorToBgRect{true};
    std::array<Needle, kNeedleCount>   m_needles;
};

} // namespace NereusSDR
