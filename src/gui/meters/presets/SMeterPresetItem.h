// =================================================================
// src/gui/meters/presets/SMeterPresetItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//   (AddSMeterBarSignal / addSMeterBar, line 21499 / 21523)
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
//                collapsing the S-Meter bar composite
//                (backdrop + bar + scale + readout text) into one
//                entity. Non-linear 3-point calibration curve
//                (-133 dBm -> 0.0, -73 dBm -> 0.5, -13 dBm -> 0.99)
//                is byte-for-byte from Thetis addSMeterBar.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QMap>
#include <QString>

namespace NereusSDR {

// SMeterPresetItem — first-class S-Meter composite preset.
//
// Collapses the dark-grey backdrop + calibrated Line-style bar +
// ShowType-titled scale + numeric readout emitted by
// `ItemGroup::createSMeterPreset()` / `createSMeterBarPreset()`
// into one MeterItem. Matches Thetis AddSMeterBarSignal (line 21499)
// calibration and colours.
class SMeterPresetItem : public MeterItem
{
    Q_OBJECT

public:
    explicit SMeterPresetItem(QObject* parent = nullptr);

    // --- MeterItem overrides ---
    Layer renderLayer() const override { return Layer::Geometry; }
    void  paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool    deserialize(const QString& data) override;

    // --- Composition introspection (used by tests) ---
    bool hasBar() const { return true; }
    bool hasScale() const { return true; }
    bool hasReadout() const { return m_showReadout; }

    // --- Readout toggle ---
    void setShowReadout(bool on) { m_showReadout = on; }
    bool showReadout() const { return m_showReadout; }

    // --- Calibration introspection (3-point curve) ---
    int  calibrationSize() const { return m_calibration.size(); }
    bool hasCalibrationPoint(double dbm) const;

private:
    // Piecewise-linear normalized-X mapping for a dBm value.
    float valueToNormalizedX(double dbm) const;

    QColor           m_backdropColor{32, 32, 32};  // From Thetis MeterManager.cs:21571 [@501e3f5]
    QColor           m_barColor{0x5f, 0x9e, 0xa0};  // CadetBlue
    QColor           m_titleColor{Qt::red};
    QColor           m_readoutColor{Qt::yellow};
    QColor           m_historyColor{255, 0, 0, 128};
    bool             m_showReadout{true};
    // 3-point non-linear calibration (value -> normalized X on bar).
    // Byte-for-byte from Thetis addSMeterBar — see .cpp for cites.
    QMap<double, float> m_calibration;
};

} // namespace NereusSDR
