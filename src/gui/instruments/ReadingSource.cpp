// src/gui/instruments/ReadingSource.cpp

// --- From src/gui/instruments/ReadingSource.h ---
// no-port-check: nennt MeterManager.cs nur als Fundort des woertlichen
// readingName()-Ports, der in MeterItem.cpp lebt und dort registriert
// ist. Diese Datei uebernimmt nichts aus Thetis -- die Beschriftungen
// stammen aus BaseItemEditor.cpp (Longpath-original), die Teilung aus
// den Entwuerfen des Betreibers, und die Skalen aus dem Longpath-Baum
// mit Herkunftsangabe je Zahl. Die Erwaehnung steht da, damit niemand
// hier eine zweite Namensliste anlegt.

// =================================================================
// src/gui/instruments/ReadingSource.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Eine Liste, nicht eine zweite ────────────────────────────────────
//
// OE5SOS, 2026-08-17: „die Instrumente sollen sich aus derselben Liste
// bedienen wie die vorhandenen Gauges, nicht aus einer zweiten."
//
// Die Liste ist MeterBinding (gui/meters/MeterPoller.h). Sie stand beim
// Nachsehen VIERMAL im Baum:
//
//   1. als Konstanten            — MeterPoller.h:82-123
//   2. als beschrifteter Kombi   — BaseItemEditor.cpp (27 Einträge)
//   3. noch einmal als Kombi     — HistoryGraphItemEditor.cpp (15)
//   4. als Thetis-Namen          — readingName(), MeterItem.cpp:128
//
// Diese Datei ist NICHT die fünfte. Sie ist die Stelle, aus der 2 und 3
// abgeleitet werden; 1 bleibt der Schlüssel, und 4 bleibt unangetastet,
// weil es ein wörtlicher Port aus MeterManager.cs:2258-2318 ist —
// label() ruft dorthin, statt danebenzuschreiben.
//
// ── Was die Liste bisher NICHT trug: die Skala ───────────────────────
//
// MeterBinding kennt nur Kennung und Name. Bereich, Schwelle und
// Teilung standen an jeder Aufrufstelle einzeln — und stimmten nicht
// überein. SWR war 1,0-5,0 in ItemGroup.cpp:730 und 1,0-3,0 mit Rot ab
// 2,5 in TxApplet.h:175. Drei Skalen für eine Größe, wenn man den
// Entwurf mitzählt.
//
// Ein Zeigerinstrument braucht Bereich, Schwelle und Teilung. Also
// stehen sie hier, EINMAL, mit Herkunftsangabe je Zahl.
//
// ── Was hier bewusst FEHLT ───────────────────────────────────────────
//
// Wo keine belastbare Skala im Baum steht, trägt der Eintrag keine.
// hasScale() ist dann false, und das Instrument bietet die Größe nicht
// an. Eine erfundene Skala wäre schlimmer als eine fehlende: sie sähe
// aus wie eine Messung.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// --- From src/gui/instruments/ReadingSource.cpp ---
// no-port-check: die einzige Thetis-Erwaehnung hier ist ein
// NEGATIVBEFUND. enums.cs:498 wird genannt, um festzuhalten, dass
// Thetis die Kuehlkoerpertemperatur als Fehlerbit fuehrt und eben
// KEINE Skala hat, die man portieren koennte. Uebernommen wird aus
// Thetis in dieser Datei nichts; die Skalen stammen aus dem eigenen
// Baum, die Teilung aus den Entwuerfen des Betreibers. Die Erwaehnung
// steht da, damit die Suche nicht ein zweites Mal gemacht wird.

// =================================================================
// src/gui/instruments/ReadingSource.cpp  (Longpath)
// =================================================================
// Siehe ReadingSource.h — eine Liste, nicht eine zweite.
// =================================================================

// --- From Project Files/Source/Console/MeterManager.cs (Thetis) ---
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
// src/gui/instruments/ReadingSource.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/instruments/ReadingSource.h
//   src/gui/instruments/ReadingSource.cpp
//   at revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
//   Namespace Longpath becomes NereusSDR.
//   MeterBinding::NoiseFloor (9) and MeterBinding::RadeSnr (10) do
//   not exist in NereusSDR's MeterPoller.h and no MeterPoller producer
//   feeds them. The two ids are kept here as local constants (the
//   numbers are free in MeterBinding; nothing is added there) and
//   their descriptors are marked unavailable (ReadingDescriptor::
//   available), so an instrument bound to them shows its offline
//   state.
//
//   Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs [v2.10.3.15], original
//   licence from Thetis source is included above. The scales of the
//   readings upstream lists without one come from it (each bar's or
//   needle's ScaleCalibration and HighPoint, renderScale's generalScale
//   ticks, ReadingUnits), a NereusSDR addition: without a scale no
//   instrument could show them. TX: ALC was the first.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - TX: ALC scale from Thetis MeterManager.cs (range
//                 -30..+12 dB, 0 dB at 66.5 %, red above 0 dB). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - English labels for the noise floor and RADE SNR readings;
//                 the TX: ALC cite points at the average ALC bar.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Every MeterPoller reading Thetis draws gets the scale of
//                 the Thetis bar or needle that draws it (ScaleCalibration,
//                 HighPoint, renderScale ticks): ADC, AGC, AGC gain, PB SNR,
//                 reverse power, the TX stage readings and their peaks, the
//                 gains, ALC group, volts and amps. The six peak readings
//                 become rows. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include "gui/instruments/ReadingSource.h"

#include "gui/meters/MeterItem.h"     // readingName()
#include "gui/meters/MeterPoller.h"   // MeterBinding

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace NereusSDR {

namespace {

// NereusSDR: Longpath's MeterBinding::NoiseFloor and MeterBinding::RadeSnr.
// NereusSDR's MeterPoller.h has neither and no MeterPoller producer feeds
// them. The numbers are free there (MeterBinding uses 0-8, 100-118,
// 200-202 and 300-301); they stay local so MeterBinding is unchanged.
namespace LongpathBinding {
constexpr int NoiseFloor = 9;    // From Longpath src/gui/meters/MeterPoller.h:108 [@551576e3]
constexpr int RadeSnr    = 10;   // From Longpath src/gui/meters/MeterPoller.h:109 [@551576e3]
} // namespace LongpathBinding

// ── Die Empfangsskala ────────────────────────────────────────────────

double sigFractionFromDbm(double dbm) { return signalFraction(dbm); }

// NereusSDR: the TX: ALC scale, Thetis's own ALC bar. TxAlc is the
// average reading (TXA_ALC_AV), the bar Thetis binds to Reading.ALC.
// From Thetis MeterManager.cs:24683, 24694-24696 [v2.10.3.15]
//   cb.ReadingSource = Reading.ALC;
//   cb.ScaleCalibration.Add(-30, new PointF(0, 0));
//   cb.ScaleCalibration.Add(0, new PointF(0.665f, 0));
//   cb.ScaleCalibration.Add(12, new PointF(0.99f, 0));
constexpr double kAlcMinDb = -30.0;
constexpr double kAlcCentreDb = 0.0;
constexpr double kAlcMaxDb = 12.0;
constexpr double kAlcCentreFraction = 0.665;
constexpr double kAlcMaxFraction = 0.99;

// Piecewise linear between the three calibration points, as Thetis's
// bar item interpolates its ScaleCalibration.
double alcFraction(double db)
{
    if (db <= kAlcCentreDb) {
        return (db - kAlcMinDb) / (kAlcCentreDb - kAlcMinDb) * kAlcCentreFraction;
    }
    return kAlcCentreFraction
        + (db - kAlcCentreDb) / (kAlcMaxDb - kAlcCentreDb) * (kAlcMaxFraction - kAlcCentreFraction);
}

// NereusSDR: one Thetis ScaleCalibration entry, the reading value (key)
// and the bar position it sits at (PointF.X). Thetis's bar item
// interpolates linearly between neighbouring entries and holds the end
// positions outside them, as alcFraction does for TX: ALC.
struct CalibrationPoint {
    double key;
    double position;
};

template <std::size_t N>
double calibratedFraction(const std::array<CalibrationPoint, N>& points, double value)
{
    if (value <= points.front().key) { return points.front().position; }
    for (std::size_t i = 1; i < N; ++i) {
        if (value <= points[i].key) {
            const CalibrationPoint& low = points[i - 1];
            const CalibrationPoint& high = points[i];
            return low.position
                + (value - low.key) / (high.key - low.key) * (high.position - low.position);
        }
    }
    return points.back().position;
}

// ReadingDescriptor::fractionOf is a plain function pointer, so each
// calibration gets its own instantiation.
template <const auto& Points>
double fractionOn(double value)
{
    return calibratedFraction(Points, value);
}

// From Thetis MeterManager.cs:23084-23086 [v2.10.3.15] (Reading.ADC_PK;
// Reading.ADC_AV :23109-23111 is the same)
//   cb.ScaleCalibration.Add(-120, new PointF(0, 0));
//   cb.ScaleCalibration.Add(-20, new PointF(0.8333f, 0));
//   cb.ScaleCalibration.Add(0, new PointF(0.99f, 0));
constexpr std::array<CalibrationPoint, 3> kAdcCalibration{{
    {-120.0, 0.0}, {-20.0, 0.8333}, {0.0, 0.99}}};

// From Thetis MeterManager.cs:23306-23308 [v2.10.3.15] (Reading.AGC_PK;
// Reading.AGC_AV :23332-23334 is the same)
//   cb.ScaleCalibration.Add(-125, new PointF(0, 0));
//   cb.ScaleCalibration.Add(0, new PointF(0.5f, 0));
//   cb.ScaleCalibration.Add(125, new PointF(0.99f, 0));
constexpr std::array<CalibrationPoint, 3> kAgcCalibration{{
    {-125.0, 0.0}, {0.0, 0.5}, {125.0, 0.99}}};

// From Thetis MeterManager.cs:23244-23246 [v2.10.3.15] (Reading.AGC_GAIN)
//   cb.ScaleCalibration.Add(-50, new PointF(0, 0));
//   cb.ScaleCalibration.Add(100, new PointF(0.857f, 0));
//   cb.ScaleCalibration.Add(125, new PointF(0.99f, 0));
constexpr std::array<CalibrationPoint, 3> kAgcGainCalibration{{
    {-50.0, 0.0}, {100.0, 0.857}, {125.0, 0.99}}};

// From Thetis MeterManager.cs:23173-23175 [v2.10.3.15] (Reading.ESTIMATED_PBSNR)
//   cb.ScaleCalibration.Add(0, new PointF(0, 0));
//   cb.ScaleCalibration.Add(50, new PointF(0.8333f, 0));
//   cb.ScaleCalibration.Add(60, new PointF(0.99f, 0));
constexpr std::array<CalibrationPoint, 3> kPbSnrCalibration{{
    {0.0, 0.0}, {50.0, 0.8333}, {60.0, 0.99}}};

// From Thetis MeterManager.cs:25181-25184, 25231-25236 [v2.10.3.15]
// (AddREVPWRBar calls AddPWRBar with Reading.REVERSE_PWR; the 100 W case,
// the only one not commented out)
//   cb.ScaleCalibration.Add(0, new PointF(0, 0));
//   cb.ScaleCalibration.Add(5, new PointF(0.1875f, 0));
//   cb.ScaleCalibration.Add(10, new PointF(0.375f, 0));
//   cb.ScaleCalibration.Add(50, new PointF(0.5625f, 0));
//   cb.ScaleCalibration.Add(100, new PointF(0.75f, 0));
//   cb.ScaleCalibration.Add(120, new PointF(0.99f, 0));
constexpr std::array<CalibrationPoint, 6> kPowerCalibration{{
    {0.0, 0.0}, {5.0, 0.1875}, {10.0, 0.375}, {50.0, 0.5625}, {100.0, 0.75}, {120.0, 0.99}}};
// From Thetis MeterManager.cs:25276 [v2.10.3.15]
//   cb.HighPoint = cb.ScaleCalibration.OrderBy(p => p.Key).ElementAt(4).Value;
constexpr std::size_t kPowerHighPointIndex = 4;

// From Thetis MeterManager.cs [v2.10.3.15], the same three points on
// every TX stage bar: MIC_PK :24348-24350, MIC :24372-24374,
// EQ_PK :24435-24437, EQ :24460-24462, LEVELER_PK :24523-24525,
// LEVELER :24547-24549, ALC_PK :24670-24672, CFC_PK :24878-24880,
// CFC_AV :24902-24904, COMP_PK :25025-25027, COMP :25049-25051
//   cb.ScaleCalibration.Add(-30, new PointF(0, 0));
//   cb.ScaleCalibration.Add(0, new PointF(0.665f, 0));
//   cb.ScaleCalibration.Add(12, new PointF(0.99f, 0));
constexpr std::array<CalibrationPoint, 3> kTxStageCalibration{{
    {-30.0, 0.0}, {0.0, 0.665}, {12.0, 0.99}}};

// From Thetis MeterManager.cs [v2.10.3.15]: LVL_G :24609-24611,
// ALC_G :24756-24758, CFC_G :24964-24966
//   cb.ScaleCalibration.Add(0, new PointF(0, 0));
//   cb.ScaleCalibration.Add(20, new PointF(0.8f, 0));
//   cb.ScaleCalibration.Add(25, new PointF(0.99f, 0));
constexpr std::array<CalibrationPoint, 3> kTxGainCalibration{{
    {0.0, 0.0}, {20.0, 0.8}, {25.0, 0.99}}};

// From Thetis MeterManager.cs:24817-24819 [v2.10.3.15] (Reading.ALC_GROUP)
//   cb.ScaleCalibration.Add(-30, new PointF(0, 0));
//   cb.ScaleCalibration.Add(0, new PointF(0.5f, 0));
//   cb.ScaleCalibration.Add(25, new PointF(0.99f, 0));
constexpr std::array<CalibrationPoint, 3> kAlcGroupCalibration{{
    {-30.0, 0.0}, {0.0, 0.5}, {25.0, 0.99}}};

// The bars above set HighPoint to the second calibration point, the
// value the scale's high (red) colour starts from.
// From Thetis MeterManager.cs:24700 [v2.10.3.15]
//   cb.HighPoint = cb.ScaleCalibration.OrderBy(p => p.Key).ElementAt(1).Value;
constexpr std::size_t kBarHighPointIndex = 1;

// From Thetis MeterManager.cs:23855-23857 [v2.10.3.15] (ANANMM, Reading.VOLTS).
// The keys give the range only: the positions are points on the needle
// face's arc, so the NereusSDR scale is linear between them.
//   ni2.ScaleCalibration.Add(10f, new PointF(0.559f, 0.756f));
//   ni2.ScaleCalibration.Add(12.5f, new PointF(0.605f, 0.772f));
//   ni2.ScaleCalibration.Add(15f, new PointF(0.665f, 0.784f));
constexpr double kVoltsMin = 10.0;
constexpr double kVoltsMid = 12.5;
constexpr double kVoltsMax = 15.0;

// From Thetis MeterManager.cs:23884-23894 [v2.10.3.15] (ANANMM, Reading.AMPS).
// The keys give the range only (0f to 20f in 2 A steps on the needle
// face's arc); the NereusSDR scale is linear between them.
//   ni3.ScaleCalibration.Add(0f, new PointF(0.199f, 0.576f));
//   ...
//   ni3.ScaleCalibration.Add(20f, new PointF(0.799f, 0.576f));
constexpr double kAmpsMin = 0.0;
constexpr double kAmpsMax = 20.0;
constexpr double kAmpsStep = 2.0;

// Ticks as Thetis's generalScale draws them (MeterManager.cs:33872-33950
// [v2.10.3.15]): long ticks with labels at lowStart + i * lowIncrement
// (i = 1 .. lowLongTicks - 1) and (highEnd - highLongTicks * highIncrement)
// + i * highIncrement (i = 1 .. highLongTicks); a short tick halfway
// before each long one.
struct GeneralScale {
    int lowLongTicks;
    int highLongTicks;
    int lowStart;
    int highEnd;
    int lowIncrement;
    int highIncrement;
    bool trailingPlus;
};

void addGeneralScaleTicks(ReadingDescriptor& d, const GeneralScale& g)
{
    double previous = g.lowStart;
    const auto add = [&d, &previous](double at, const QString& label) {
        d.ticks.append({at, label});
        d.minorTicks.append((previous + at) / 2.0);
        previous = at;
    };
    for (int i = 1; i < g.lowLongTicks; ++i) {
        const int at = g.lowStart + i * g.lowIncrement;
        add(at, QString::number(at));
    }
    for (int i = 1; i <= g.highLongTicks; ++i) {
        const int at = (g.highEnd - g.highLongTicks * g.highIncrement) + i * g.highIncrement;
        QString label = QString::number(at);
        if (g.trailingPlus && i == g.highLongTicks) { label += QLatin1Char('+'); }
        add(at, label);
    }
}

// From Thetis MeterManager.cs:33455 [v2.10.3.15] (ADC_PK, ADC_AV)
//   generalScale(x, y, w, h, scale, 6, 1, -120, 0, 20, 20, fLineBaseY, fontSizeEmScaled, 255);
constexpr GeneralScale kAdcTicks{6, 1, -120, 0, 20, 20, false};
// From Thetis MeterManager.cs:33442 [v2.10.3.15] (AGC_AV, AGC_PK)
//   generalScale(x, y, w, h, scale, 6, 5, -125, 125, 25, 25, fLineBaseY, fontSizeEmScaled, 255);
constexpr GeneralScale kAgcTicks{6, 5, -125, 125, 25, 25, false};
// From Thetis MeterManager.cs:33436 [v2.10.3.15] (AGC_GAIN)
//   generalScale(x, y, w, h, scale, 7, 1, -50, 125, 25, 25, fLineBaseY, fontSizeEmScaled, 255);
constexpr GeneralScale kAgcGainTicks{7, 1, -50, 125, 25, 25, false};
// From Thetis MeterManager.cs:33460 [v2.10.3.15] (ESTIMATED_PBSNR)
//   generalScale(x, y, w, h, scale, 6, 1, 0, 60, 10, 10, fLineBaseY, fontSizeEmScaled, 255);
constexpr GeneralScale kPbSnrTicks{6, 1, 0, 60, 10, 10, false};
// From Thetis MeterManager.cs:33479 [v2.10.3.15] (MIC .. ALC_PK)
//   generalScale(x, y, w, h, scale, 4, 3, -30, 12, 10, 4, fLineBaseY, fontSizeEmScaled, 255, 0.665f);
constexpr GeneralScale kTxStageTicks{4, 3, -30, 12, 10, 4, false};
// From Thetis MeterManager.cs:33484 [v2.10.3.15] (ALC_GROUP)
//   generalScale(x, y, w, h, scale, 4, 5, -30, 25, 10, 5, fLineBaseY, fontSizeEmScaled, 255, 0.5f, true);
constexpr GeneralScale kAlcGroupTicks{4, 5, -30, 25, 10, 5, true};
// From Thetis MeterManager.cs:33689 [v2.10.3.15] (ALC_G, LVL_G, CFC_G)
//   generalScale(x, y, w, h, scale, 5, 1, 0, 25, 5, 5, fLineBaseY, fontSizeEmScaled, 255, -1, true);
constexpr GeneralScale kTxGainTicks{5, 1, 0, 25, 5, 5, true};

// A reading on a Thetis bar's calibrated scale: range from the first and
// last keys, red from HighPoint's key.
template <const auto& Points>
ReadingDescriptor calibratedReading(int id, const char* label, const char* unit,
                                    std::size_t highPointIndex, const GeneralScale& ticks)
{
    ReadingDescriptor d;
    d.bindingId  = id;
    d.label      = QString::fromLatin1(label);
    d.unit       = QString::fromLatin1(unit);
    d.hasScale   = true;
    d.min        = Points.front().key;
    d.max        = Points.back().key;
    d.threshold  = Points[highPointIndex].key;
    d.decimals   = 1;
    d.fractionOf = &fractionOn<Points>;
    addGeneralScaleTicks(d, ticks);
    return d;
}

// From Longpath src/gui/instruments/ReadingSource.cpp:32 [@551576e3]
QString sigFormat(double dbm)
{
    const double s = sUnitsFromDbm(dbm);
    if (s <= 9.0) {
        return QStringLiteral("S%1").arg(qRound(s));
    }
    return QStringLiteral("S9+%1").arg(qRound((s - 9.0) * 10.0));
}

// ── Die Tabelle ──────────────────────────────────────────────────────
//
// Beschriftungen wörtlich aus BaseItemEditor.cpp:252-283 übernommen,
// damit die Umstellung dort keinen einzigen sichtbaren Text ändert.
//
// Skalen nur, wo eine belastbare Zahl im Baum steht. Herkunft je Zahl
// im Kommentar; wo keine steht, trägt der Eintrag keine Skala.

// From Longpath src/gui/instruments/ReadingSource.cpp:49 [@551576e3]
QList<ReadingDescriptor> buildTable()
{
    QList<ReadingDescriptor> t;

    auto plain = [&t](int id, const char* label) {
        ReadingDescriptor d;
        d.bindingId = id;
        d.label = QString::fromLatin1(label);
        t.append(d);
    };

    // ── RX ───────────────────────────────────────────────────────────
    //
    // Die drei Grössen, die auf einer S-Skala stehen. Bereich und
    // Kennlinie aus Thetis MeterManager.cs:22870-22872
    // [@852bf0e] — S0 = -127 dBm, S9 = -73 dBm,
    // S9+60 = -13 dBm; siehe ReadingSource.h fuer den Wortlaut und die
    // zwei gewollten Abweichungen. KEINE Schwelle: „Wo es keine
    // Schwelle gibt — Empfangsskala —, gibt es keinen roten Abschnitt"
    // (OE5SOS, 2026-08-17).
    //
    // Max Bin gehört dazu, weil es dieselbe Grösse ist: Thetis rechnet
    // ihm denselben Kalibrieroffset an wie den beiden anderen
    // (console.cs:46881 [v2.10.3.13] gegen :46824 und :46828). Es stand
    // vorher als `plain` ohne Skala hier und war damit für die
    // Instrumente unsichtbar — wählbar war es nur über den
    // Rechtsklick der analogen S-Meter-Anzeige, also über eine zweite
    // Auswahlmechanik neben dieser Liste.
    for (const auto& sig : {
             std::pair<int, const char*>{MeterBinding::SignalPeak,   "RX: Signal Peak"},
             std::pair<int, const char*>{MeterBinding::SignalAvg,    "RX: Signal Avg"},
             std::pair<int, const char*>{MeterBinding::SignalMaxBin, "RX: Signal Max Bin"}}) {
        ReadingDescriptor d;
        d.bindingId  = sig.first;
        d.label      = QString::fromLatin1(sig.second);
        d.unit       = QStringLiteral("dBm");
        d.hasScale   = true;
        d.min        = kS0Dbm;
        d.max        = kSMaxDbm;
        d.threshold  = std::nullopt;
        d.decimals   = 0;
        d.fractionOf = &sigFractionFromDbm;
        d.format     = &sigFormat;
        // Beschriftet jede zweite S-Stufe, feine Striche bei jeder —
        // aus dem Entwurf (zeiger-verfeinert.html, sigCfg).
        d.ticks = {
            {kS0Dbm + 1 * 6.0, QStringLiteral("1")},
            {kS0Dbm + 3 * 6.0, QStringLiteral("3")},
            {kS0Dbm + 5 * 6.0, QStringLiteral("5")},
            {kS0Dbm + 7 * 6.0, QStringLiteral("7")},
            {kS9Dbm,           QStringLiteral("9")},
            {kS9Dbm + 20.0,    QStringLiteral("+20")},
            {kS9Dbm + 40.0,    QStringLiteral("+40")},
            {kS9Dbm + 60.0,    QStringLiteral("+60")},
        };
        d.minorTicks = {
            kS0Dbm + 2 * 6.0, kS0Dbm + 4 * 6.0, kS0Dbm + 6 * 6.0,
            kS0Dbm + 8 * 6.0, kS9Dbm + 10.0, kS9Dbm + 30.0, kS9Dbm + 50.0,
        };
        t.append(d);
    }

    // NereusSDR: upstream's lines were
    //   plain(MeterBinding::AdcPeak,      "RX: ADC Peak");
    //   plain(MeterBinding::AdcAvg,       "RX: ADC Avg");
    //   plain(MeterBinding::AgcGain,      "RX: AGC Gain");
    //   plain(MeterBinding::AgcPeak,      "RX: AGC Peak");
    //   plain(MeterBinding::AgcAvg,       "RX: AGC Avg");
    //   plain(MeterBinding::PbSnr,        "RX: PB SNR");
    // Each gets the scale of the Thetis bar that draws it; units from
    // Thetis MeterManager.cs:2326-2335 (ReadingUnits) [v2.10.3.15].
    t.append(calibratedReading<kAdcCalibration>(MeterBinding::AdcPeak, "RX: ADC Peak", "dBFS",
                                                kBarHighPointIndex, kAdcTicks));
    t.append(calibratedReading<kAdcCalibration>(MeterBinding::AdcAvg, "RX: ADC Avg", "dBFS",
                                                kBarHighPointIndex, kAdcTicks));
    t.append(calibratedReading<kAgcGainCalibration>(MeterBinding::AgcGain, "RX: AGC Gain", "dB",
                                                    kBarHighPointIndex, kAgcGainTicks));
    t.append(calibratedReading<kAgcCalibration>(MeterBinding::AgcPeak, "RX: AGC Peak", "dB",
                                                kBarHighPointIndex, kAgcTicks));
    t.append(calibratedReading<kAgcCalibration>(MeterBinding::AgcAvg, "RX: AGC Avg", "dB",
                                                kBarHighPointIndex, kAgcTicks));
    t.append(calibratedReading<kPbSnrCalibration>(MeterBinding::PbSnr, "RX: PB SNR", "dB",
                                                  kBarHighPointIndex, kPbSnrTicks));

    // ── Rauschflur ───────────────────────────────────────────────────
    //
    // OE5SOS, 2026-08-18: „gehört zum Empfang, also ins Instrument als
    // Quelle." Er stand vorher nur als Beschriftung an der analogen
    // Anzeige im Panelkopf.
    //
    // Auf der dBm-ACHSE, nicht auf der S-Skala: ein Rauschflur ist kein
    // Signal, und die S-Stauchung ueber S9 saehe hier aus wie eine
    // Eigenschaft des Rauschens. Ausdruecklich so gewuenscht.
    //
    // Bereich = der Anzeigebereich des Panadapters,
    // PanadapterModel.h:186-187 (Vorgaben aus Thetis): Boden -140 dBm,
    // Decke -40 dBm. Dieselben Grenzen, in denen der Wert entsteht —
    // eine eigene Zahl hier waere eine zweite Behauptung ueber
    // denselben Bereich.
    {
        ReadingDescriptor d;
        d.bindingId = LongpathBinding::NoiseFloor;
        d.available = false;   // NereusSDR: no producer, see LongpathBinding
        d.label     = QStringLiteral("RX: Noise Floor");   // NereusSDR: English label (upstream "RX: Rauschflur")
        d.unit      = QStringLiteral("dBm");
        d.hasScale  = true;
        d.min       = -140.0;
        d.max       =  -40.0;
        // Keine Schwelle. Ein hoher Rauschflur ist ein schlechter
        // Standort, keine Gefahr — und rot heisst hier Gefahr.
        d.threshold = std::nullopt;
        d.decimals  = 0;
        d.ticks = {
            {-140.0, QStringLiteral("-140")},
            {-120.0, QStringLiteral("-120")},
            {-100.0, QStringLiteral("-100")},
            { -80.0, QStringLiteral("-80")},
            { -60.0, QStringLiteral("-60")},
            { -40.0, QStringLiteral("-40")},
        };
        d.minorTicks = {-130.0, -110.0, -90.0, -70.0, -50.0};
        t.append(d);
    }

    // ── RADE-SNR ─────────────────────────────────────────────────────
    //
    // Der Bereich ist AUS DEM RADE-QUELLTEXT GERECHNET, nicht geraten.
    //
    // third_party/rade/src/rade_ofdm.c:412-424 [@b289102]:
    //     float snr_est = S1 / (2.0f * S2) - 1.0f;
    //     if (snr_est <= 0.0f) snr_est = 0.1f;          // Zeile 413
    //     float snrdB_est = 10.0f * log10f(snr_est);
    //     snrdB_est = (snrdB_est - c_corr) / m_corr;    // 4.1343 / 0.7650
    //     snrdB_3k = snrdB_est + 10log10(Rs*Nc/3000) + 10log10((M+Ncp)/M);
    //
    // mit RADE_FS 8000, RADE_NC 30, RADE_M 160, RADE_NCP 32
    // (rade_dsp.h:59-65 [@b289102]). Die beiden Korrekturterme sind
    // -3.01 dB und +0.79 dB.
    //
    // BODEN: die Klemme bei 0.1 in Zeile 413 ist der kleinste Wert, den
    // der Schaetzer ueberhaupt melden kann. Eingesetzt ergibt das
    // -20.69 dB; abgerundet -21 dB. Ein Instrument, das darunter noch
    // Skala zeigte, zeigte Bereich, den die Quelle nie erreicht.
    //
    // DECKE: +30 dB. Der lineare Schaetzwert +30 dB landet bei
    // +31.6 dB(3k), die Decke liegt also innerhalb dessen, was die
    // Rechnung hergibt.
    //
    // KEINE SCHWELLE, obwohl der Baum eine Grenze kennt: VfoWidget.cpp:930
    // faerbt unter 5 dB gedaempft. Die ist aber UMGEKEHRT — klein ist
    // schlecht —, und `threshold` heisst „darueber ist es eine Warnung".
    // Eine umgekehrte Schwelle waere ein zweiter Begriff mit demselben
    // Namen; sie gehoert in die Beschreibung, nicht in dieses Feld.
    {
        ReadingDescriptor d;
        d.bindingId = LongpathBinding::RadeSnr;
        d.available = false;   // NereusSDR: no producer, see LongpathBinding
        d.label     = QStringLiteral("RX: RADE SNR");   // NereusSDR: English label (upstream "RX: RADE-SNR")
        d.unit      = QStringLiteral("dB");
        d.hasScale  = true;
        d.min       = -21.0;
        d.max       =  30.0;
        d.threshold = std::nullopt;
        d.decimals  = 0;
        d.ticks = {
            {-20.0, QStringLiteral("-20")},
            {-10.0, QStringLiteral("-10")},
            {  0.0, QStringLiteral("0")},
            { 10.0, QStringLiteral("10")},
            { 20.0, QStringLiteral("20")},
            { 30.0, QStringLiteral("30")},
        };
        d.minorTicks = {-15.0, -5.0, 5.0, 15.0, 25.0};
        t.append(d);
    }

    // ── TX: Vorlaufleistung ──────────────────────────────────────────
    //
    // Bereich 0-120 W aus ItemGroup.cpp:680 (pwrBar->setRange(0,120)),
    // Schwelle 100 W aus TxApplet.h:174 („HGauge 0-120 W, red > 100 W").
    //
    // TxApplet skaliert die Anzeige je SKU nach (TxApplet.h:225). Diese
    // Tabelle trägt die Grundskala; wer sie je SKU nachziehen will,
    // setzt sie am Instrument, nicht hier.
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::TxPower;
        d.label     = QStringLiteral("TX: Forward Power");
        d.unit      = QStringLiteral("W");
        d.hasScale  = true;
        d.min       = 0.0;
        d.max       = 120.0;
        d.threshold = 100.0;
        d.decimals  = 0;
        d.ticks = {{0, QStringLiteral("0")},   {30, QStringLiteral("30")},
                   {60, QStringLiteral("60")}, {90, QStringLiteral("90")},
                   {120, QStringLiteral("120")}};
        d.minorTicks = {15, 45, 75, 105};
        t.append(d);
    }

    // NereusSDR: upstream's line was
    //   plain(MeterBinding::TxReversePower, "TX: Reverse Power");
    // Thetis draws reverse power on the forward power bar's scale
    // (AddREVPWRBar calls AddPWRBar). With a radio, LongpathContent gives
    // the instrument the per-model power scale, as for forward power.
    {
        ReadingDescriptor d;
        d.bindingId  = MeterBinding::TxReversePower;
        d.label      = QStringLiteral("TX: Reverse Power");
        d.unit       = QStringLiteral("W");
        d.hasScale   = true;
        d.min        = kPowerCalibration.front().key;
        d.max        = kPowerCalibration.back().key;
        d.threshold  = kPowerCalibration[kPowerHighPointIndex].key;
        d.decimals   = 0;
        d.fractionOf = &fractionOn<kPowerCalibration>;
        // From Thetis MeterManager.cs:33531-33535 [v2.10.3.15] (PWR,
        // REVERSE_PWR, scaled to 100 W): long ticks 5, 10, 50, 100, 120+
        // at the calibration positions, short ticks halfway.
        d.ticks = {{5.0, QStringLiteral("5")},     {10.0, QStringLiteral("10")},
                   {50.0, QStringLiteral("50")},   {100.0, QStringLiteral("100")},
                   {120.0, QStringLiteral("120+")}};
        d.minorTicks = {2.5, 7.5, 30.0, 75.0, 110.0};
        t.append(d);
    }

    // ── TX: SWR ──────────────────────────────────────────────────────
    //
    // 1,0-3,0 mit Grenze 2,5 — aus TxApplet.h:175 („HGauge 1.0-3.0,
    // red > 2.5") und deckungsgleich mit dem Entwurf des Betreibers
    // (zeiger-verfeinert.html, swrCfg: min 1, max 3, threshold 2.5).
    //
    // ABWEICHUNG, bewusst nicht stillschweigend übernommen:
    // ItemGroup.cpp:730 setzt denselben Messwert auf 1,0-5,0. Welche
    // der beiden für die Meter-Items gilt, ist hier nicht entschieden —
    // diese Tabelle beschreibt die Skala der INSTRUMENTE, und für die
    // ist der Entwurf die Vorlage.
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::TxSwr;
        d.label     = QStringLiteral("TX: SWR");
        d.hasScale  = true;
        d.min       = 1.0;
        d.max       = 3.0;
        d.threshold = 2.5;
        d.decimals  = 2;
        d.ticks = {{1.0, QStringLiteral("1")},   {1.5, QStringLiteral("1.5")},
                   {2.0, QStringLiteral("2")},   {2.5, QStringLiteral("2.5")},
                   {3.0, QStringLiteral("3")}};
        d.minorTicks = {1.25, 1.75, 2.25, 2.75};
        t.append(d);
    }

    // NereusSDR: upstream's lines were
    //   plain(MeterBinding::TxMic,          "TX: Mic");
    //   plain(MeterBinding::TxComp,         "TX: Compressor");
    // Each TX stage reading and its peak (a NereusSDR row) get the scale
    // of the Thetis bar that draws them, the TX: ALC scale.
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxMic, "TX: Mic", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxMicPeak, "TX: Mic Peak", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxComp, "TX: Compressor", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxCompPeak, "TX: Compressor Peak", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    // NereusSDR: upstream's line was
    //   plain(MeterBinding::TxAlc,          "TX: ALC");
    // which gives TX: ALC no scale. NereusSDR's MeterPoller hands out a
    // TxAlc reading (MeterPoller.cpp handOutTxReading), so it gets
    // Thetis's own ALC scale rather than an invented one.
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::TxAlc;
        d.label     = QStringLiteral("TX: ALC");
        // From Thetis MeterManager.cs:2332 [v2.10.3.15]
        d.unit      = QStringLiteral("dB");
        d.hasScale  = true;
        d.min       = kAlcMinDb;
        d.max       = kAlcMaxDb;
        // From Thetis MeterManager.cs:24700 [v2.10.3.15]: HighPoint is
        // the second calibration point, 0 dB; the scale's high (red)
        // colour starts there (generalScale, MeterManager.cs:33894-33898).
        d.threshold = kAlcCentreDb;
        d.decimals  = 1;
        d.fractionOf = alcFraction;
        // From Thetis MeterManager.cs:33479 [v2.10.3.15]
        //   generalScale(x, y, w, h, scale, 4, 3, -30, 12, 10, 4, fLineBaseY, fontSizeEmScaled, 255, 0.665f);
        // Long ticks -20, -10, 0 (low, from i = 1) and 4, 8, 12 (high);
        // short ticks halfway between (MeterManager.cs:33902-33950).
        d.ticks = {{-20.0, QStringLiteral("-20")}, {-10.0, QStringLiteral("-10")},
                   {  0.0, QStringLiteral("0")},   {  4.0, QStringLiteral("4")},
                   {  8.0, QStringLiteral("8")},   { 12.0, QStringLiteral("12")}};
        d.minorTicks = {-25.0, -15.0, -5.0, 2.0, 6.0, 10.0};
        t.append(d);
    }
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxAlcPeak, "TX: ALC Peak", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    // NereusSDR: upstream's lines were
    //   plain(MeterBinding::TxEq,           "TX: EQ");
    //   plain(MeterBinding::TxLeveler,      "TX: Leveler");
    //   plain(MeterBinding::TxLevelerGain,  "TX: Leveler Gain");
    //   plain(MeterBinding::TxAlcGain,      "TX: ALC Gain");
    //   plain(MeterBinding::TxAlcGroup,     "TX: ALC Group");
    //   plain(MeterBinding::TxCfc,          "TX: CFC");
    //   plain(MeterBinding::TxCfcGain,      "TX: CFC Gain");
    //
    //   plain(MeterBinding::HwVolts,        "HW: Volts");
    //   plain(MeterBinding::HwAmps,         "HW: Amps");
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxEq, "TX: EQ", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxEqPeak, "TX: EQ Peak", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxLeveler, "TX: Leveler", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxLevelerPeak, "TX: Leveler Peak", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxGainCalibration>(MeterBinding::TxLevelerGain, "TX: Leveler Gain", "dB",
                                                   kBarHighPointIndex, kTxGainTicks));
    t.append(calibratedReading<kTxGainCalibration>(MeterBinding::TxAlcGain, "TX: ALC Gain", "dB",
                                                   kBarHighPointIndex, kTxGainTicks));
    t.append(calibratedReading<kAlcGroupCalibration>(MeterBinding::TxAlcGroup, "TX: ALC Group", "dB",
                                                     kBarHighPointIndex, kAlcGroupTicks));
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxCfc, "TX: CFC", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxStageCalibration>(MeterBinding::TxCfcPeak, "TX: CFC Peak", "dB",
                                                    kBarHighPointIndex, kTxStageTicks));
    t.append(calibratedReading<kTxGainCalibration>(MeterBinding::TxCfcGain, "TX: CFC Gain", "dB",
                                                   kBarHighPointIndex, kTxGainTicks));

    // Volts and amps: the range of ANANMM's needles, linear, no red zone
    // (Thetis's needles carry no HighPoint). Units from Thetis
    // MeterManager.cs:2337, 2366 (ReadingUnits) [v2.10.3.15].
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::HwVolts;
        d.label     = QStringLiteral("HW: Volts");
        d.unit      = QStringLiteral("V");
        d.hasScale  = true;
        d.min       = kVoltsMin;
        d.max       = kVoltsMax;
        d.threshold = std::nullopt;
        d.decimals  = 1;
        // The calibration keys, labelled.
        d.ticks = {{kVoltsMin, QStringLiteral("10")}, {kVoltsMid, QStringLiteral("12.5")},
                   {kVoltsMax, QStringLiteral("15")}};
        d.minorTicks = {(kVoltsMin + kVoltsMid) / 2.0, (kVoltsMid + kVoltsMax) / 2.0};
        t.append(d);
    }
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::HwAmps;
        d.label     = QStringLiteral("HW: Amps");
        d.unit      = QStringLiteral("A");
        d.hasScale  = true;
        d.min       = kAmpsMin;
        d.max       = kAmpsMax;
        d.threshold = std::nullopt;
        d.decimals  = 1;
        // The calibration keys: every second one labelled, the rest short.
        for (double at = kAmpsMin; at <= kAmpsMax; at += kAmpsStep) {
            if (std::fmod(at, 2.0 * kAmpsStep) == 0.0) {
                d.ticks.append({at, QString::number(at, 'f', 0)});
            } else {
                d.minorTicks.append(at);
            }
        }
        t.append(d);
    }

    // ── HW: Temperatur — ohne Skala, und das mit Absicht ─────────────
    //
    // Der Betreiber hat sie als eine der vier Quellen für die
    // Instrumente genannt. Sie bekommt trotzdem keine Skala, und das
    // ist nachgesehen und nicht dahingestellt:
    //
    //   Im eigenen Baum steht weder Bereich noch Grenze. RadioStatus
    //   reicht die Zahl durch (setPaTemperature), SystemTile zeigt sie
    //   ohne Zone.
    //
    //   In Thetis auch nicht — nachgesehen am 2026-08-17 gegen
    //   ../Thetis [@852bf0e]. Thetis kennt die Kühlkörpertemperatur
    //   NICHT als Messwert mit Bereich, sondern als FEHLERBIT:
    //   PAstatusIndicatorState.HeatsinkTemperature (enums.cs:498),
    //   ein Flag im PA-Statuswort, das der Verstärker meldet. Es gibt
    //   dort keine Skala zu portieren, weil dort keine gezeichnet
    //   wird.
    //
    // Damit ist die Frage keine Portierungsfrage mehr, sondern eine
    // Angabe: ab welcher Temperatur soll das Instrument warnen. Das
    // steht in keinem Quelltext, sondern im Datenblatt des jeweiligen
    // Geräts — und die Antwort gehört dem Betreiber.
    //
    // Eine erfundene Skala sähe aus wie eine Messung. Sobald Bereich
    // und Grenze belegt sind, sind es drei Zeilen hier, und die Größe
    // steht sofort in der Auswahl der Instrumente. Die Prüfung in
    // tst_reading_source fällt dann um und erinnert daran, die
    // Herkunft dazuzuschreiben.
    plain(MeterBinding::HwTemperature,  "HW: Temperature");

    plain(MeterBinding::RotatorAz,      "Rotator: Azimuth");
    plain(MeterBinding::RotatorEle,     "Rotator: Elevation");

    return t;
}

} // namespace

// ── Empfangsskala ────────────────────────────────────────────────────

// From Longpath src/gui/instruments/ReadingSource.cpp:316 [@551576e3]
double sUnitsFromDbm(double dbm)
{
    if (dbm <= kS9Dbm) {
        return (dbm - kS0Dbm) / 6.0;      // 6 dB je S-Stufe
    }
    return 9.0 + (dbm - kS9Dbm) / 10.0;   // darüber 10 dB je Stufe
}

// From Longpath src/gui/instruments/ReadingSource.cpp:324 [@551576e3]
double signalFraction(double dbm)
{
    const double s = sUnitsFromDbm(dbm);
    const double f = (s <= 9.0)
        ? (s / 9.0) * kS9Fraction
        : kS9Fraction + ((s - 9.0) / 6.0) * (1.0 - kS9Fraction);
    return qBound(0.0, f, 1.0);
}

// ── ReadingDescriptor ────────────────────────────────────────────────

// From Longpath src/gui/instruments/ReadingSource.cpp:335 [@551576e3]
double ReadingDescriptor::fraction(double value) const
{
    if (fractionOf) {
        return qBound(0.0, fractionOf(value), 1.0);
    }
    if (max <= min) { return 0.0; }
    return qBound(0.0, (value - min) / (max - min), 1.0);
}

// From Longpath src/gui/instruments/ReadingSource.cpp:344 [@551576e3]
QString ReadingDescriptor::text(double value) const
{
    if (format) { return format(value); }
    return QString::number(value, 'f', decimals);
}

// From Longpath src/gui/instruments/ReadingSource.cpp:350 [@551576e3]
QString ReadingDescriptor::textWithUnit(double value) const
{
    if (format || unit.isEmpty()) { return text(value); }
    return QStringLiteral("%1 %2").arg(text(value), unit);
}

// From Longpath src/gui/instruments/ReadingSource.cpp:356 [@551576e3]
QString ReadingDescriptor::thetisName() const
{
    return readingName(bindingId);
}

// ── Zugriff ──────────────────────────────────────────────────────────

// From Longpath src/gui/instruments/ReadingSource.cpp:363 [@551576e3]
const QList<ReadingDescriptor>& allReadings()
{
    static const QList<ReadingDescriptor> kTable = buildTable();
    return kTable;
}

// From Longpath src/gui/instruments/ReadingSource.cpp:369 [@551576e3]
const ReadingDescriptor* readingFor(int bindingId)
{
    const auto& t = allReadings();
    auto it = std::find_if(t.constBegin(), t.constEnd(),
                           [bindingId](const ReadingDescriptor& d) {
                               return d.bindingId == bindingId;
                           });
    return (it == t.constEnd()) ? nullptr : &(*it);
}

// From Longpath src/gui/instruments/ReadingSource.cpp:379 [@551576e3]
QList<const ReadingDescriptor*> readingsWithScale()
{
    QList<const ReadingDescriptor*> out;
    for (const ReadingDescriptor& d : allReadings()) {
        if (d.hasScale) { out.append(&d); }
    }
    return out;
}

} // namespace NereusSDR
