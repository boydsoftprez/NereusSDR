// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RotorHeading.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/core/RotorPeilung.h [@551576e], original header from Longpath
//   source is included below.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE). Upstream source has no
// top-of-file GPL header; project-level LICENSE applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted transformation via Anthropic
//               Claude Code. Namespace Longpath::RotorPeilung becomes
//               NereusSDR::RotorHeading, kHoechstens becomes kMaxDeg and
//               lies() becomes parse(); accept() applies the same checks
//               to a number that arrives as a number (a session command),
//               not as text. Rotor control plan, Task 3a.
// =================================================================
//
// --- From RotorPeilung.h ---
//
// Eine Peilung vom Netz annehmen — oder ablehnen.
//
// Longpath-original, kopflastig fuer vier Zeilen Rechnung. Der Grund steht
// am anderen Ende der Leitung: ein Mast mit einer Antenne darauf, der sich
// auf Zuruf dreht. Was hier durchrutscht, dreht echtes Metall.
//
// Darum nimmt diese Stelle NICHT einfach `toDouble()`:
//
//   * `toDouble()` macht aus "" eine 0 und aus "abc" eine 0. Null Grad ist
//     Nord — eine leere Zeile wuerde die Antenne nach Norden drehen.
//   * NaN und Unendlich rutschen durch jeden Bereichsvergleich: `nan < 0`
//     ist falsch, `nan > 360` ist auch falsch.
//   * 360 ist dieselbe Richtung wie 0 und muss angenommen werden; 361 ist
//     ein Tippfehler und darf es nicht.
//
// Negative Werte und Werte ueber 360 werden ABGELEHNT statt umgerechnet.
// Ein Umrechnen waere bequem und falsch: wer -90 schickt, hat sich vertan,
// und 270 Grad sind eine andere Antwort als "ich habe mich vertan".

#pragma once

#include <QString>

#include <cmath>

namespace NereusSDR::RotorHeading {

// From Longpath src/core/RotorPeilung.h:29-30 [@551576e]
/// Groesster erlaubter Wert. 360 gilt und bedeutet Nord.
constexpr double kMaxDeg = 360.0;

// From Longpath src/core/RotorPeilung.h:44-49 [@551576e], the checks of
// lies() after the text has become a number.
//
// Accepts a compass heading that arrived as a number. Returns false for
// not-a-number, infinity, below 0 or above 360; `deg` is then left as it
// was. Out-of-range values are refused, never wrapped. 360 is north and is
// written to `deg` as 0.
inline bool accept(double value, double* deg)
{
    // Reihenfolge wichtig: zuerst auf endlich pruefen. NaN besteht jeden
    // Bereichsvergleich, weil jeder Vergleich mit NaN falsch ist.
    if (!std::isfinite(value)) { return false; }
    if (value < 0.0 || value > kMaxDeg) { return false; }
    if (deg) { *deg = (value == kMaxDeg) ? 0.0 : value; }
    return true;
}

// From Longpath src/core/RotorPeilung.h:32-50 [@551576e]
/// Liest eine Peilung aus dem Befehlstext.
///
/// Gibt `false` zurueck, wenn nichts Brauchbares dasteht; `grad` bleibt dann
/// unberuehrt. 360 wird auf 0 gelegt, weil der Rotor dieselbe Richtung
/// meint und manche Steuerungen 360 nicht annehmen.
//
// An empty or blank box is refused, never north; text that is not a number
// is refused, never 0. `deg` is the upstream `grad`.
inline bool parse(const QString& text, double* deg)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) { return false; }
    bool ok = false;
    const double w = t.toDouble(&ok);
    if (!ok) { return false; }
    return accept(w, deg);
}

} // namespace NereusSDR::RotorHeading
