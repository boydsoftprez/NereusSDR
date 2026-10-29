// src/gui/instruments/ReadingSource.h

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

// =================================================================
// src/gui/instruments/ReadingSource.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/instruments/ReadingSource.h
//   at revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
//   Namespace Longpath becomes NereusSDR.
//   ReadingDescriptor gains `available` (NereusSDR): false for a
//   reading NereusSDR has no producer for. See ReadingSource.cpp.
//
//   Independently implemented from Thetis interface: this header takes
//   nothing from Thetis. The TX: ALC scale constants from Thetis
//   MeterManager.cs live in ReadingSource.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - The known-gap note on the VHF S9 shift says the shift is
//                 now applied by the needle and bar factories. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace NereusSDR {

// From Longpath src/gui/instruments/ReadingSource.h:67 [@551576e3]
/// Ein beschrifteter Teilstrich.
struct ReadingTick {
    double  value;
    QString label;
};

// From Longpath src/gui/instruments/ReadingSource.h:74 [@551576e3]
/// Beschreibung einer Messgröße: woher der Wert kommt (bindingId), wie
/// er heißt, und — wenn bekannt — auf welcher Skala er steht.
struct ReadingDescriptor {
    int     bindingId{-1};
    QString label;          ///< Für Auswahllisten, z. B. „TX: SWR".
    QString unit;           ///< Leer, wenn einheitenlos.

    // ── Skala. Nur gültig, wenn hasScale true ist. ───────────────────
    bool    hasScale{false};
    double  min{0.0};
    double  max{1.0};

    /// Ab hier ist es keine Messung mehr, sondern eine Warnung. Ohne
    /// Wert gibt es keinen roten Abschnitt — auf der Empfangsskala gibt
    /// es keine Schwelle, und ein Instrument soll dort auch keine
    /// zeichnen.
    std::optional<double> threshold;

    QList<ReadingTick> ticks;       ///< beschriftet
    QList<double>      minorTicks;  ///< fein, ohne Beschriftung
    int                decimals{1};

    /// Wert → Anteil 0..1. Nullzeiger heißt linear zwischen min und
    /// max. Nichtlinear ist die Empfangsskala: bis S9 gleichmäßig,
    /// darüber gestaucht.
    double (*fractionOf)(double) {nullptr};

    /// Wert → Anzeigetext. Nullzeiger heißt „mit `decimals` Stellen".
    QString (*format)(double) {nullptr};

    // NereusSDR: false for a reading NereusSDR has no producer for
    // (NoiseFloor and RadeSnr, see ReadingSource.cpp). The descriptor and
    // its scale stay; an instrument bound to it shows its offline state.
    bool available{true};

    /// Anteil für einen Wert, mit Klemmung auf 0..1.
    double fraction(double value) const;

    /// Anzeigetext für einen Wert.
    QString text(double value) const;

    /// Anzeigetext MIT Einheit — aber nur, wo die Einheit zum Text
    /// passt: ein eigenes `format` (die S-Skala: "S8", "S9+20") traegt
    /// seine Einheit schon im Wort, und "S8 dBm" war auf dem Foto vom
    /// 2026-09-17 eine S-Stufe mit einer Einheit, die nicht ihre ist.
    /// Ohne eigenes Format haengt die Einheit an ("-97 dBm", "3.4 W").
    QString textWithUnit(double value) const;

    /// Der Thetis-wörtliche Name aus readingName(). Eigene Funktion,
    /// damit hier keine zweite Namensliste entsteht.
    QString thetisName() const;
};

// From Longpath src/gui/instruments/ReadingSource.h:125 [@551576e3]
/// Alle beschriebenen Größen. Weitgehend in der Reihenfolge von
/// MeterPoller.h — Ausnahme: Signal Max Bin steht bei den beiden
/// anderen S-Skalen, weil diese Liste auch die Auswahl im
/// Rechtsklickmenü ist und dort die drei Empfangsquellen
/// zusammengehören.
const QList<ReadingDescriptor>& allReadings();

// From Longpath src/gui/instruments/ReadingSource.h:128 [@551576e3]
/// Die Beschreibung zu einer Kennung, oder nullptr.
const ReadingDescriptor* readingFor(int bindingId);

// From Longpath src/gui/instruments/ReadingSource.h:132 [@551576e3]
/// Nur die Größen, die eine Skala tragen — das ist die Auswahl, die
/// ein Zeiger- oder Balkeninstrument anbieten darf.
QList<const ReadingDescriptor*> readingsWithScale();

// From Longpath src/gui/instruments/ReadingSource.h:168 [@551576e3]
// ── Die Empfangsskala ────────────────────────────────────────────────
//
// Die drei Stuetzstellen kommen aus THETIS, nicht aus einer eigenen
// Rechnung. Thetis MeterManager.cs:22870-22872 [@852bf0e]
// legt die Kalibrierpunkte des S-Meter-Balkens fest:
//
//     cb.ScaleCalibration.Add(-133, new PointF(0, 0));      // position for S0 or below
//                                    // -133 is the edge, as S0 (-127) is the first small tick
//     cb.ScaleCalibration.Add(-73, new PointF(0.5f, 0));    // position for S9
//     cb.ScaleCalibration.Add(-13, new PointF(0.99f, 0));   // position for S9+60dB or above
//
// Also S0 = -127 dBm, S9 = -73 dBm, S9+60 = -13 dBm; daraus 6 dB je
// S-Stufe bis S9 und 10 dB darueber.
//
// Bis 2026-08-18 stand hier stattdessen ein Verweis auf
// SMeterWidget.h:62-66 + :318-319 — unsere eigene Zwischenstation, die
// die Zahlen ihrerseits aus AetherSDR hatte. Mit deren Loeschung waere
// der Verweis ins Leere gelaufen; die Quelle ist ohnehin diese hier.
//
// ── Zwei Abweichungen von Thetis, beide gewollt ──────────────────────
//
// 1. Die STELLE von S9 auf der Skala. Thetis setzt sie auf 0,5 der
//    Balkenlaenge, wir auf 0,66 — aus dem Entwurf des Betreibers
//    (~/Downloads/zeiger-verfeinert.html, sigFrac). Das ist eine
//    Gestaltungsentscheidung ueber dieselben dBm-Werte, kein
//    Portierungsfehler. Wer sie „berichtigt", macht ein anderes
//    Instrument daraus.
//
// 2. Thetis verschiebt die Skala oberhalb einer Grenzfrequenz um 20 dB
//    (MeterManager.cs:22874, `if (IsAboveS9Frequency(_rx)) cb.Value -= 20;`
//    mit _s9Frequency = 30.0 MHz aus :358) — auf UKW ist S9 nach IARU
//    -93 dBm statt -73 dBm. Diese Umschaltung haben WIR NICHT, und die
//    geloeschte analoge Anzeige hatte sie auch nicht. Bekannte Luecke,
//    hier vermerkt, damit sie nicht ein drittes Mal gesucht wird.
//
//    NereusSDR: the shift is now applied. A Longpath needle or bar raises
//    Signal Peak and Signal Avg by 20 dB when its entry's slice is at or
//    above 30 MHz (LongpathContent meterFactory, aboveS9Adjusted in
//    AboveS9Frequency.h), so S9 reads at -93 dBm there; this scale and
//    kS9Dbm stay as they are.
constexpr double kS0Dbm   = -127.0;   ///< MeterManager.cs:22870 (Kommentar)
constexpr double kS9Dbm   =  -73.0;   ///< MeterManager.cs:22871
constexpr double kSMaxDbm =  -13.0;   ///< MeterManager.cs:22872

// From Longpath src/gui/instruments/ReadingSource.h:177 [@551576e3]
/// Wo S9 auf der Skala sitzt. Aus dem Entwurf des Betreibers
/// (~/Downloads/zeiger-verfeinert.html, sigFrac): 0.66, nicht 2/3.
/// Der Unterschied ist 0,7 % der Skalenlänge und damit unsichtbar —
/// aber ein Test muss eine Zahl festnageln, und die gezeichnete ist
/// die Vorlage.
constexpr double kS9Fraction = 0.66;

// From Longpath src/gui/instruments/ReadingSource.h:180 [@551576e3]
/// dBm → S-Stufen. Bis S9 sechs dB je Stufe, darüber zehn.
double sUnitsFromDbm(double dbm);

// From Longpath src/gui/instruments/ReadingSource.h:183 [@551576e3]
/// dBm → Anteil 0..1 mit der Stauchung über S9.
double signalFraction(double dbm);

} // namespace NereusSDR
