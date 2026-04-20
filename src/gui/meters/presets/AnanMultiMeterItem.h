// =================================================================
// src/gui/meters/presets/AnanMultiMeterItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddAnanMM, ~line 22461)
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
//                collapsing the 7-needle ANAN Multi Meter preset into
//                a single entity; pivot/radius now anchor to the
//                background image's letterboxed draw rect to fix the
//                arc-drift bug at non-default container aspect ratios.
//
//                Note on binding identifiers: Thetis's AddAnanMM
//                tags each needle with `MMIOVariableIndex = 0..6` —
//                a per-preset slot ordinal used by the Thetis MMIO
//                exporter, NOT a runtime data-source identifier.
//                NereusSDR's runtime needle-to-WDSP routing uses the
//                C++ `MeterBinding::*` constants in
//                `src/gui/meters/MeterPoller.h` (e.g.
//                `MeterBinding::SignalAvg = 1`, `HwVolts = 200`,
//                `HwAmps = 201`, `TxPower = 100`, `TxSwr = 102`,
//                `TxAlcGain = 109`, `TxAlcGroup = 110`). The two
//                numbering schemes are unrelated; the Thetis 0..6
//                slot ordering is preserved only as the array index
//                inside `m_needles` to keep the assignment order
//                visually parallel to AddAnanMM.
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

// AnanMultiMeterItem — first-class ANAN Multi Meter preset.
//
// Encapsulates the 7-needle composite (Signal, Volts, Amps, Power,
// SWR, Compression, ALC) plus the ananMM.png background image as a
// single MeterItem. Replaces the 9+ child items emitted by the old
// `ItemGroup::createAnanMMPreset()` factory; one row in the in-use
// list, one property pane, one persistence blob.
//
// Arc-fix: pivot and radius are anchored to the painted image rect
// (`bgRect()`), not the container's normalized item rect. At
// non-natural aspect ratios the background letterboxes inside the
// item rect; the needles follow the image, keeping the arc glued to
// the meter face. Toggle off via `setAnchorToBgRect(false)` for
// pre-port behaviour.
class AnanMultiMeterItem : public MeterItem
{
    Q_OBJECT

public:
    static constexpr int kNeedleCount = 7;

    explicit AnanMultiMeterItem(QObject* parent = nullptr);

    // --- MeterItem overrides ---
    // Background -> MeterWidget calls paintForLayer() -> paint();
    // primitives (BarItem / NeedleItem / etc.) on Geometry /
    // OverlayDynamic paint on top. This matches the user's mental
    // model that a preset is the "background meter" the container
    // shows, with the in-use list's item order driving z-order
    // (top of list = behind, bottom = front). The 11 preset classes
    // only override paint() (QPainter), not emitVertices();
    // Layer::Geometry would route them through the empty GPU vertex
    // pipeline and the preset would never draw.
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

    // --- Geometry ---
    QPointF pivot() const { return m_pivot; }
    QPointF radiusRatio() const { return m_radiusRatio; }
    void    setPivot(const QPointF& p) { m_pivot = p; }
    void    setRadiusRatio(const QPointF& r) { m_radiusRatio = r; }

    // --- Arc-fix: anchor pivot/radius to background image rect ---
    bool anchorToBgRect() const { return m_anchorToBgRect; }
    void setAnchorToBgRect(bool v) { m_anchorToBgRect = v; }

    // --- Debug overlay: paint coloured dots at every calibration point ---
    bool debugOverlay() const { return m_debugOverlay; }
    void setDebugOverlay(bool v) { m_debugOverlay = v; }

private:
    struct Needle {
        QString              name;
        int                  bindingId{-1};
        QMap<float, QPointF> calibration;
        QColor               color;
        bool                 visible{true};
        // Per-needle reach scaler — Thetis `clsNeedleItem.LengthFactor`.
        // Extends the tip past the calibration point by this factor
        // (1.0 = exact calibration; >1.0 overshoots; <1.0 falls short).
        // Orthogonal to `m_radiusRatio`, which scales the whole arc.
        float                lengthFactor{1.0f};
    };

    void   initialiseNeedles();
    QRect  bgRect(int widgetW, int widgetH) const;
    QPointF calibratedPosition(const Needle& n, float value) const;
    void   paintNeedle(QPainter& p, const Needle& n, const QRect& bg) const;
    void   paintDebugOverlay(QPainter& p, const QRect& bg) const;

    QImage                       m_background;
    QPointF                      m_pivot{0.004, 0.736};
    QPointF                      m_radiusRatio{1.0, 0.58};
    bool                         m_anchorToBgRect{true};
    bool                         m_debugOverlay{false};
    std::array<Needle, kNeedleCount> m_needles;
};

} // namespace NereusSDR
