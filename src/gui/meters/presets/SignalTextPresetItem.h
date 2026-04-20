// =================================================================
// src/gui/meters/presets/SignalTextPresetItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddSMeterBarText, line 21678)
//   verbatim upstream header reproduced below; the .cpp carries the
//   same block in front of the formatting logic.
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
//                collapsing the large signal text readout
//                (backdrop + big S-unit + dBm text) into one entity.
//                Default colour (Yellow), font size (56pt) and
//                binding (AVG_SIGNAL_STRENGTH) are byte-for-byte
//                from Thetis AddSMeterBarText. The paint-time
//                fit-to-rect clamp on m_fontPoint is a runtime
//                safeguard against overflow in narrow NereusSDR
//                containers; it does not override the Thetis-exact
//                default value.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QString>

namespace NereusSDR {

// SignalTextPresetItem — first-class large signal-text preset.
//
// Collapses the backdrop + SignalTextItem composition emitted by
// `ItemGroup::createSignalTextPreset()` / Thetis AddSMeterBarText
// into one MeterItem. Renders "S<n> [+NN]  -NN dBm" in a single
// large font.
class SignalTextPresetItem : public MeterItem
{
    Q_OBJECT

public:
    explicit SignalTextPresetItem(QObject* parent = nullptr);

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

    bool hasText() const { return true; }

    // --- Formatting helpers (exposed for test) ---
    QString formatReadout(double dbm) const;

    // --- Colour ---
    QColor textColor() const { return m_textColor; }
    void   setTextColor(const QColor& c) { m_textColor = c; }

    // --- Font size (Thetis-vs-NereusSDR override, driven by Task 11 editor) ---
    float fontPoint() const { return m_fontPoint; }
    void  setFontPoint(float pts) { m_fontPoint = pts; }

private:
    QColor  m_backdropColor{32, 32, 32};  // From Thetis MeterManager.cs:21689 [@501e3f5]
    QColor  m_textColor{Qt::yellow};      // From Thetis MeterManager.cs:21708 [@501e3f5]
    // Thetis-exact default — FontSize = 56f at MeterManager.cs:21709.
    // The fit-to-rect clamp in paint() caps the *effective* font size
    // to container height, but never changes this stored default.
    float   m_fontPoint{56.0f};  // From Thetis MeterManager.cs:21709 [@501e3f5]
};

} // namespace NereusSDR
