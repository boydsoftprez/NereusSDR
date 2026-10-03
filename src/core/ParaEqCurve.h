// =================================================================
// src/core/ParaEqCurve.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucParametricEq.cs (the response curve,
//   PointsFromJson, GetDefaults and enforceOrdering) and
//   Project Files/Source/Console/eqform.cs (the TX EQ panel's widget
//   limits, ParaEQTXData's setter, sendTXDspUpdate and setTXEQProfile),
//   original licences from Thetis source are included below.
//   Sole author of ucParametricEq.cs: Richard Samphire (MW0LGE).
//
// Declarations; the implementation is in ParaEqCurve.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - R-R3-49 (parity Task 4): the TX EQ parametric curve
//                 moved out of the GUI (ParametricEqWidget's response
//                 curve, TxEqDialog's sampling onto the TX channel's ten
//                 bands) into src/core, so the Core applies the curve
//                 saved in txEqParaEqData to its own TX channel. Same
//                 numbers as the dialog (tst_para_eq_curve).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-25 - R-R3-49 (group A fix wave): the TX EQ reaches WDSP as
//                 Thetis sends it. The Core decodes a saved curve as
//                 Thetis's transmit path does (PointsFromJson, GetDefaults
//                 for a blank or broken value) and builds the arrays of
//                 sendTXDspUpdate (every point's F and G, Q when the panel
//                 uses Q factors) and of setTXEQProfile for the legacy EQ.
//                 The ten-point sampling, the widget-load port and the
//                 point ordering it needed are gone. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-IOS-13 / R-R3-49: the panel's point ordering
//                 (ucParametricEq enforceOrdering with the TX panel's
//                 reorder and 5 Hz spacing) and txEqCurveJson, the
//                 NereusSDR-owned, read-only form of the saved curve the
//                 Core sends as transmit.txEqCurve (station link document,
//                 "The TX EQ curve"). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - R-IOS-13 / R-R3-49 (follow-up): readCurveJson, the
//                 parser the Core and ParametricEqWidget share. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-IOS-13 / R-R3-49 (txEqCurveVersion 2): an app's curve
//                 checked against the TX panel's choices
//                 (txEqPointsFromCurveJson), SaveToJsonFromPoints as the
//                 ParaEQTXData getter saves it, and the panel's Reset
//                 (ResetPoints), for txEq.setCurve and txEq.resetCurve.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
// =================================================================

// --- From ucParametricEq.cs ---
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

// --- From eqform.cs ---
//=================================================================
// eqform.cs
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

#include <QString>

#include <array>
#include <cmath>
#include <vector>

namespace NereusSDR {

namespace ParaEqCurve {

// The TX EQ panel's widget limits a saved curve is clamped to. From Thetis
// eqform.cs:946-970 [v2.10.3.15] (ucParametricEq1's property block);
// TxEqDialog applies the same values to its widget.
inline constexpr double kTxEqDbMin = -24.0;   // cs:960
inline constexpr double kTxEqDbMax =  24.0;   // cs:959
inline constexpr double kTxEqQMin  =   0.2;   // cs:970
inline constexpr double kTxEqQMax  =  20.0;   // cs:969
// ucParametricEq1.MinPointSpacingHz = 5D and AllowPointReorder = true.
// From Thetis eqform.cs:966, 946 [v2.10.3.15].
inline constexpr double kTxEqMinPointSpacingHz = 5.0;

// ucParametricEq.GetDefaults' default arguments, as eqform's
// ParaEQTXData setter calls it for a value it cannot load. From Thetis
// ucParametricEq.cs:1107-1131 [v2.10.3.15].
inline constexpr int    kTxEqDefaultBandCount = 10;
inline constexpr double kTxEqDefaultMinHz     = 0.0;
inline constexpr double kTxEqDefaultMaxHz     = 4000.0;
inline constexpr double kTxEqDefaultQ         = 4.0;

// From Thetis ucParametricEq.cs:2983-2988 [v2.10.3.15].
inline double clamp(double v, double lo, double hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

// The response curve in dB at `frequencyHz`. From Thetis
// ucParametricEq.cs:2694-2748 [v2.10.3.15]. Two branches:
//   - graphic EQ (!parametricEq): straight lines between adjacent points,
//     clamped to the first and last gain at the edges;
//   - parametric: a Gaussian per point, FWHM = span / (q*3) (at least
//     span/6000), sigma = FWHM / 2.3548200450309493, summed unweighted.
// A template over the point type: ParametricEqWidget's EqPoint (the
// panel's display) uses it.
template <typename PointList>
double responseDb(const PointList& points, bool parametricEq,
                  double frequencyMinHz, double frequencyMaxHz,
                  double qMin, double qMax, double frequencyHz)
{
    if (!parametricEq) {
        if (points.isEmpty()) { return 0.0; }
        const double f = frequencyHz;
        if (f <= points.first().frequencyHz) { return points.first().gainDb; }
        if (f >= points.last().frequencyHz)  { return points.last().gainDb; }

        for (int i = 1; i < points.size(); ++i) {
            const auto& left  = points.at(i - 1);
            const auto& right = points.at(i);
            if (f <= right.frequencyHz) {
                const double denom = right.frequencyHz - left.frequencyHz;
                if (denom <= 0.0000001) { return right.gainDb; }
                double t = (f - left.frequencyHz) / denom;
                if (t < 0.0) { t = 0.0; }
                if (t > 1.0) { t = 1.0; }
                return left.gainDb + ((right.gainDb - left.gainDb) * t);
            }
        }
        return points.last().gainDb;
    }

    double span = frequencyMaxHz - frequencyMinHz;
    if (span <= 0.0) { span = 1.0; }

    double sum = 0.0;
    for (const auto& p : points) {
        const double q = clamp(p.q, qMin, qMax);
        double fwhm = span / (q * 3.0);
        const double minFwhm = span / 6000.0;
        if (fwhm < minFwhm) { fwhm = minFwhm; }
        const double sigma = fwhm / 2.3548200450309493;
        const double d = (frequencyHz - p.frequencyHz) / sigma;
        const double w = std::exp(-0.5 * d * d);
        sum += p.gainDb * w;
    }
    return sum;
}

/// The TX EQ's parametric points as Thetis's EQ form holds them for WDSP
/// (eqform.cs ParaEQState's TX_F, TX_G, TX_Q, TX_Preamp, TX_minHz,
/// TX_maxHz, TX_ParametricEQ and TX_BandCount).
struct TxEqPoints {
    std::vector<double> f;
    std::vector<double> g;
    std::vector<double> q;
    double preampDb     = 0.0;
    double minHz        = kTxEqDefaultMinHz;
    double maxHz        = kTxEqDefaultMaxHz;
    bool   parametricEq = true;
    int    bandCount    = kTxEqDefaultBandCount;
};

/// A saved curve's JSON as Thetis's EqJsonState holds it after
/// JsonConvert.DeserializeObject: unclamped, unrounded, missing fields at
/// their C# defaults (0, false).
struct CurveJson {
    int    bandCount      = 0;
    bool   parametricEq   = false;
    double globalGainDb   = 0.0;
    double frequencyMinHz = 0.0;
    double frequencyMaxHz = 0.0;
    std::vector<double> f;
    std::vector<double> g;
    std::vector<double> q;
};

/// The deserialise-and-check block ucParametricEq's PointsFromJson and
/// LoadFromJson share: false where both refuse the value. The one parser
/// the Core (pointsFromJson) and ParametricEqWidget::loadFromJson use.
bool readCurveJson(const QString& json, CurveJson& out);

/// ucParametricEq.PointsFromJson with the TX panel's limits: the points of
/// a saved curve's JSON, each clamped and rounded as Thetis does, in the
/// saved order, the first and last locked to the range's ends. False,
/// leaving `out` alone, when Thetis would not load it.
bool pointsFromJson(const QString& json, TxEqPoints& out);

/// ucParametricEq.GetDefaults with its default arguments: ten flat points
/// from 0 to 4000 Hz, Q 4, parametric, no preamp.
TxEqPoints defaultTxEqPoints();

/// The points a saved txEqParaEqData value holds (the gzip and base64url
/// envelope Thetis saves, or raw JSON from an early NereusSDR build).
/// False when it holds none Thetis would load.
bool loadTxEqPoints(const QString& paraEqData, TxEqPoints& out);

/// eqform.cs ParaEQTXData's setter: the saved value's points, or
/// GetDefaults' when it holds none (a blank or broken value).
TxEqPoints txEqPointsFromParaEqData(const QString& paraEqData);

/// The points as the TX EQ panel draws them: ucParametricEq's
/// enforceOrdering(true) with the panel's reorder on and its 5 Hz point
/// spacing. Sorted by frequency (a tie keeps the saved order), every point
/// inside the range, the first at minHz and the last at maxHz, and the
/// points between at least the spacing apart (less when the range is too
/// narrow for it). Thetis runs this when the panel loads a curve
/// (eqform.cs setParaEQData, SetPointsData), so the panel shows these even
/// where the saved order differs.
TxEqPoints txEqDisplayPoints(const TxEqPoints& points);

/// What transmit.txEqCurve says for a txEqParaEqData value, as compact
/// JSON (NereusSDR-owned; the station link document's "The TX EQ curve"):
///   {"state":"saved"|"default", "parametric":bool, "preampDb":f,
///    "minHz":f, "maxHz":f, "points":[{"frequencyHz":f,"gainDb":f,"q":f}]}
/// with the points as the panel draws them (txEqDisplayPoints), or
/// {"state":"unavailable"} for a non-empty value the Core cannot read.
/// "default" is an empty value: the flat curve Thetis puts in its place.
QString txEqCurveJson(const QString& paraEqData);

// ── Editing the curve from an app (txEq.setCurve, txEqCurveVersion 2) ──
//
// What the TX EQ panel lets an operator choose, from Thetis eqform.cs
// [v2.10.3.15]: the 5-band, 10-band and 18-band buttons (radParaEQ_5/10/18,
// cs:517-541, applied by radParaEQ_CheckedChanged cs:3127-3139); Low and
// High from 0 to 20000 Hz (udParaEQ_low/high, cs:567-606) kept at least
// 1000 Hz apart (nudParaEQ_low/high_ValueChanged, cs:3543-3546,
// 3563-3566); a point's frequency from 0 to 20000 Hz (nudParaEQ_f,
// cs:803-812), its gain from -24 to 24 dB (nudParaEQ_gain, cs:883-892),
// its Q from 0.2 to 20 (nudParaEQ_q, cs:843-852); the preamp from -24 to
// 24 dB (nudParaEQ_preamp, cs:696-705).
inline constexpr int    kTxEqBandCounts[] = {5, 10, 18};
inline constexpr double kTxEqRangeLowestHz  = 0.0;
inline constexpr double kTxEqRangeHighestHz = 20000.0;
inline constexpr double kTxEqMinRangeSpreadHz = 1000.0;
inline constexpr double kTxEqPreampMinDb = -24.0;
inline constexpr double kTxEqPreampMaxDb =  24.0;

/// A curve an app sent in the txEqCurve shape ({"parametric", "preampDb",
/// "minHz", "maxHz", "points":[{"frequencyHz", "gainDb", "q"}]}; any other
/// key, "state" included, is ignored), checked against the panel's
/// choices above and read as the Core keeps it: each value rounded as
/// Thetis's PointsFromJson rounds (frequency 0.001 Hz, gain and preamp
/// 0.1 dB, Q 0.01) and the points ordered as the panel orders them
/// (txEqDisplayPoints). False, with the reason in plain words and `out`
/// left alone, for a curve the panel could not hold.
bool txEqPointsFromCurveJson(const QString& curveJson, TxEqPoints& out, QString* refusal);

/// ucParametricEq.SaveToJsonFromPoints with the TX panel's limits: the JSON
/// eqform's ParaEQTXData getter saves for these points (each clamped and
/// rounded, the first and last at the range's ends). Empty where Thetis
/// returns null (fewer than two points, mismatched arrays, a range that is
/// not finite or not increasing).
QString saveToJsonFromPoints(const TxEqPoints& points);

/// What txEqParaEqData holds for these points, as the ParaEQTXData getter
/// saves them: saveToJsonFromPoints in the gzip and base64url envelope.
/// Empty where saveToJsonFromPoints is.
QString txEqParaEqDataFromPoints(const TxEqPoints& points);

/// The panel's Reset button (eqform.cs btnParaEQReset_Click): the preamp
/// to 0 and the points reset as ucParametricEq.ResetPoints does, flat at
/// 0 dB with Q 4, evenly spread over the panel's current range, keeping its
/// band count and Use Q Factors. `current` is what the panel holds
/// (txEqPointsFromParaEqData). Not GetDefaults: the range and band count
/// stay the operator's.
TxEqPoints resetTxEqPoints(const TxEqPoints& current);

/// The arrays Thetis hands WDSP's SetTXAEQProfile(channel, nfreqs, F, G, Q):
/// nfreqs = F.size() - 1; F[0] = 0 and G[0] = the preamp; Q[0] = 0, and
/// `q` empty when Thetis passes no Q.
struct TxEqProfile {
    std::vector<double> f;
    std::vector<double> g;
    std::vector<double> q;
};

/// eqform.cs sendTXDspUpdate: every point's F and G, and Q when the panel
/// uses Q factors.
TxEqProfile txEqProfileFromPoints(const TxEqPoints& points);

/// eqform.cs setTXEQProfile, the legacy ten-band EQ: the band centres and
/// gains and the preamp, no Q.
TxEqProfile legacyTxEqProfile(int preampDb, const std::array<int, 10>& bandGainsDb,
                              const std::array<int, 10>& bandFreqsHz);

} // namespace ParaEqCurve

} // namespace NereusSDR
