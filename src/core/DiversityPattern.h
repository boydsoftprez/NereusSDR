// =================================================================
// src/core/DiversityPattern.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/DiversityForm.cs [v2.10.3.15],
//   original licence from Thetis source is included below.
//
// Scope of port: the sensitivity-vs-angle math from
// DiversityForm.CalcVrms (DiversityForm.cs:2398-2440 [v2.10.3.15]),
// moved out of DiversityRadarWidget so the Core sends the pattern the
// Diversity dialog draws (the phone draws the Core's samples and never
// ports the formula). The inputs, their defaults and the sampling are
// the desktop radar's own (NereusSDR-original: a 5.5 m spacing and the
// radar's gain term, which upstream CalcVrms does not have).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28 - Moved from DiversityRadarWidget (Phase 3F Sub-Epic G
//                 Task 5) into the Core with the sampling and the wire
//                 value, for the phone's Diversity page. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include <QList>
#include <QString>

namespace NereusSDR::DiversityPattern {

/// The antenna spacing the desktop radar uses. It has no setting (Thetis's
/// udAntSpacing is not ported), so it is a constant. NereusSDR-original.
constexpr double kDefaultSpacingMeters = 5.5;
/// The radar's frequency before a slice sets it (MHz). NereusSDR-original.
constexpr double kDefaultVfoMhz = 14.225;
/// Samples round the circle: 360 / 3 degrees, bearing 0 north, clockwise.
constexpr int kSamples = 120;
/// Places each wire sample keeps (a thousandth of the peak).
constexpr int kWireDecimals = 3;

/// Everything CalcVrms reads.
struct Inputs {
    double vfoMhz = kDefaultVfoMhz;
    /// Steering angle (the diversity phase), radians.
    double phaseRad = 0.0;
    /// Second receiver's gain as a linear ratio (1.0 = unity).
    double gainLinear = 1.0;
    /// Thetis's cross_fire (pi when on). The dialog has no switch: off.
    bool crossFire = false;
    double spacingMeters = kDefaultSpacingMeters;
};

/// The inputs the Diversity dialog gives its radar for a slice: its
/// frequency (Hz), diversity phase (degrees) and gain (dB).
Inputs inputsForSlice(double frequencyHz, double phaseDeg, double gainDb);

/// Relative sensitivity at azimuth `thetaRad` (CalcVrms).
double sensitivity(const Inputs& inputs, double thetaRad);

/// kSamples values, sample i at bearing i * 3 degrees clockwise from
/// north, each divided by the largest (so the peak is 1). When every
/// value is below 1e-9 they are returned undivided. The radar draws
/// sample i at radius value * 0.85 of its circle.
QList<double> normalizedSamples(const Inputs& inputs);

/// The Core's wire value for a slice (the station link document, "The
/// diversity pattern"): compact JSON with the inputs the slice does not
/// already carry (spacingMeters, crossFire), stepDeg and the samples, each
/// rounded to kWireDecimals places. The frequency, phase and gain are the
/// slice's own frequency, diversityPhaseDeg and diversityGainDb.
QString wireJson(double frequencyHz, double phaseDeg, double gainDb);

} // namespace NereusSDR::DiversityPattern
