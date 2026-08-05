// =================================================================
// src/gui/widgets/DiversityRadarWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/DiversityForm.cs [v2.10.3.15],
//   original licence from Thetis source is included below.
//
// Scope of port: the sensitivity-vs-angle math from
// DiversityForm.CalcVrms (DiversityForm.cs:2398-2440 [v2.10.3.15])
// plus the picRadar_Paint background/axes structure
// (DiversityForm.cs:1489-1530 [v2.10.3.15]) and the
// getControlHandlePoint placement helper
// (DiversityForm.cs:1610-1632 [v2.10.3.15]).  Upstream Thetis
// defines CalcVrms but its callsite in picRadar_Paint is commented
// out (DiversityForm.cs:1564-1580 [v2.10.3.15]); NereusSDR
// uncomments and uses it to render the antenna lobe.  Visual
// chrome (radial gradient backdrop, compass labels, dashed range
// rings, translucent cyan lobe fill, centre dot, yellow steering
// handle) is NereusSDR-original Qt6 paint.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 — Reimplemented in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.  Phase 3F Sub-Epic G
//                 Task 5: polar QPainter widget with mouse-drag to
//                 emit phaseAdjusted; consumer (Sub-Epic G Task 12)
//                 wires the signal to SliceModel::diversityPhaseDeg.
// =================================================================

//=================================================================
// DiversityForm.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
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

#pragma once

#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace NereusSDR {

/// Polar paint widget rendering the diversity sensitivity lobe.
///
/// Geometry: square, centred origin, range = unit (clamped to 1.0).
/// Phase is supplied in radians (0..2 PI), gain as a linear ratio
/// (1.0 = unity).  Frequency drives the antenna-spacing-in-wavelengths
/// term used by the lobe math; default 14.225 MHz, default 5.5 m
/// element spacing.
class DiversityRadarWidget : public QWidget {
    Q_OBJECT
public:
    explicit DiversityRadarWidget(QWidget* parent = nullptr);

    void setPhase(double radians);
    void setGain(double ratio);
    void setCrossFire(bool on);
    void setVfoFreqMhz(double mhz);
    void setAntennaSpacingMeters(double m);

signals:
    void phaseAdjusted(double newRadians);
    void gainAdjusted(double newGain);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    double m_phase {0.0};
    double m_gain {1.0};
    bool   m_crossFire {false};
    double m_vfoMhz {14.225};
    double m_antSpacingM {5.5};
    bool   m_dragging {false};

    // Sensitivity (relative power) at the given azimuth angle (radians).
    // Ported from Thetis DiversityForm.CalcVrms.
    double sensitivityAtAngle(double angleRad) const;
};

} // namespace NereusSDR
