// =================================================================
// src/core/DiversityPattern.cpp  (NereusSDR)
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

#include "core/DiversityPattern.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace NereusSDR::DiversityPattern {

namespace {

constexpr double kSpeedOfLight = 299792458.0;          // m/s
constexpr double kTwoPi        = 2.0 * M_PI;

double rounded(double value)
{
    const double scale = std::pow(10.0, kWireDecimals);
    return std::round(value * scale) / scale;
}

} // namespace

Inputs inputsForSlice(double frequencyHz, double phaseDeg, double gainDb)
{
    // As DiversityDialog::refreshFromSlice feeds its radar.
    Inputs in;
    in.phaseRad = phaseDeg * M_PI / 180.0;
    in.gainLinear = std::pow(10.0, gainDb / 20.0);
    in.vfoMhz = frequencyHz / 1e6;
    return in;
}

// From Thetis DiversityForm.cs:2398-2440 [v2.10.3.15] (CalcVrms)
//
// Upstream signature: double CalcVrms(double a, double b) where
//   a = theta (azimuth angle, radians)
//   b = steering_angle (radians; this is the user-set phase)
//
// Upstream reads console.VFOAFreq, cross_fire and d_lambda from the form;
// they arrive here in `inputs`. The gain term on v2 is NereusSDR's radar
// (upstream has none).
double sensitivity(const Inputs& inputs, double thetaRad)
{
    const double freq         = 1.0e6 * inputs.vfoMhz;              // Hz
    const double lambda       = kSpeedOfLight / freq;               // m
    const double d_lambda     = inputs.spacingMeters / lambda;      // dimensionless
    const double dt           = 1.0 / (20.0 * freq);                //calc time step; 1/20th of a full cycle of RF
    const double angular_freq = kTwoPi * freq * dt;
    const double cross_fire   = inputs.crossFire ? M_PI : 0.0;
    const double steering     = inputs.phaseRad;

    // phi = cross_fire + cos(theta + steering_angle)
    //
    // Upstream picks between radioButtonMerc1 / Merc2 branches but both
    // branches contain the same expression, so we collapse.
    const double phi = cross_fire + std::cos(thetaRad + steering);

    // generate 20 time points within one cycle of the freq and calculate antenna voltages
    // then calculate rms value for these voltages for the specified theta
    double rms = 0.0;
    for (int i = 0; i < 20; ++i) {
        const double v1  = std::sin(static_cast<double>(i) * angular_freq);
        const double v2  = std::sin(static_cast<double>(i) * angular_freq
                                    + phi - kTwoPi * d_lambda) * inputs.gainLinear;
        const double sum = v1 + v2;
        rms += sum * sum;  //no need for sqrt for rms since sensitivity ~ power ~ v^2
    }
    return (rms / 20.0) * 5.5;          //normalize to 1.0 max
}

QList<double> normalizedSamples(const Inputs& inputs)
{
    QList<double> samples;
    samples.reserve(kSamples);
    double peak = 0.0;
    for (int i = 0; i < kSamples; ++i) {
        const double theta = (kTwoPi * i) / static_cast<double>(kSamples);
        samples.append(sensitivity(inputs, theta));
        peak = std::max(peak, samples.last());
    }
    // Upstream multiplies by 5.5 to "normalise to 1.0 max" but with gain
    // and wide phase swings the result can exceed unity, so the radar
    // rescales to its peak.
    if (peak > 1.0e-9) {
        for (double& value : samples) {
            value /= peak;
        }
    }
    return samples;
}

QString wireJson(double frequencyHz, double phaseDeg, double gainDb)
{
    const Inputs in = inputsForSlice(frequencyHz, phaseDeg, gainDb);
    QJsonArray points;
    for (double value : normalizedSamples(in)) {
        points.append(rounded(value));
    }
    // The slice's frequency, phase and gain travel as its own properties
    // (in the same delta when they move the pattern), so a tuning step that
    // leaves every rounded sample as it was sends no pattern at all.
    const QJsonObject object{
        {QStringLiteral("spacingMeters"), in.spacingMeters},
        {QStringLiteral("crossFire"), in.crossFire},
        {QStringLiteral("stepDeg"), 360.0 / kSamples},
        {QStringLiteral("points"), points},
    };
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

} // namespace NereusSDR::DiversityPattern
