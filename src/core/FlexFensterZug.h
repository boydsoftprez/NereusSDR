#pragma once

// =================================================================
// src/core/FlexFensterZug.h  (Longpath)
// =================================================================
//
// Wer gibt beim FLEX-8400 die Mitte vor: das Fenster oder der
// Panadapter der Maschine? Zwei Urteile, und sie muessen zusammen
// passen, sonst ziehen sie gegeneinander.
//
// Der Fall, gemessen im laufenden Betrieb am 2026-10-30 (13:47:40
// bis 13:47:42, Protokoll longpath-20261030-134732.log):
//
//     40.094  Maschine meldet Ausschnitt 7.075–7.275 (Mitte 7.175)
//     40.094  Maschine meldet Ausschnitt 7.054–7.254 (Mitte 7.154)
//     40.119  Maschine meldet Scheibe 0 auf 7.154 MHz
//     40.119  Maschine meldet Scheibe 0 auf 7.074 MHz
//     41.625  Profil stellte 14.225 her, Maschine steht auf 7.074
//     41.632  Scheibe und Maschine gehen auseinander — es gilt 7.074
//     41.632  Panadapter auf die Scheibe gezogen — 7.074 MHz
//     41.815  Ausschnitt selbst nachgefuehrt — Mitte 7.074
//     41.890  Panadapter folgt dem Bild — 7.154 MHz (war 7.074)
//     42.035  Ausschnitt selbst nachgefuehrt — Mitte 7.154
//
// Die letzten vier Zeilen sind die Schleife: Longpath zieht den
// Panadapter der Maschine auf die Frequenz der Maschine (7.074), und
// 76 ms spaeter zieht der Nachzug ihn wieder auf das Fenster zurueck
// (7.154). Das Fenster steht dort, weil es beim Verbinden die ZWEITE
// Ausschnittsmeldung der Maschine uebernommen hat — und die stand
// 80 kHz ueber ihrer eigenen Scheibe.
//
// Das sind die seit dem 2026-10-29 gesuchten 80 kHz. Sie sind kein
// gerechneter Versatz und kein wiederhergestelltes Bandgedaechtnis:
// sie sind der Abstand zwischen dem Panadapter der Maschine und ihrer
// eigenen Scheibe im Augenblick des Verbindens, festgehalten im
// Fenster und von dort aus wieder hinausgeschickt.
//
// Beide Regeln, die hier aufeinandertreffen, sind fuer sich richtig:
//
//   * Das Fenster folgt einer reinen MITTENverschiebung der Maschine
//     NICHT (2026-10-23). Sonst wirft jeder Doppelklick im Panadapter
//     den Zoom des Betreibers weg — er hat genau das gemeldet.
//   * Der Nachzug schiebt den Panadapter der Maschine auf das Fenster,
//     sonst steht das Bild schief ueber der FFT und wird gedehnt
//     (2026-10-27).
//
// Was fehlte, ist der Unterschied zwischen einem FREMDEN und einem
// EIGENEN Zug. Zieht die Maschine, darf das Fenster stehen bleiben.
// Ziehen WIR, ist unser Zug die juengere Entscheidung — dann folgt das
// Fenster, und zwar nur mit der Mitte: die Breite bleibt die des
// Betreibers.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <cmath>

namespace Longpath {
namespace FlexFensterZug {

/// Das Totband des Nachzugs, als Anteil der Ausschnittsbreite. Ein
/// Prozent: bei 200 kHz sind das 2 kHz, und 2 kHz Rest ergeben einen
/// Maszstab von 1,01 — am Bildrand ein Kilohertz. Gemessen am
/// 2026-10-27, als hier ein Zehntel stand und das Bild sichtbar
/// gedehnt wurde.
inline constexpr double kTotbandAnteil = 0.01;

/// Folgt das Fenster dem neuen Ausschnitt der Maschine?
///
/// \param neueBreiteHz     Breite, die die Maschine jetzt meldet
/// \param alteBreiteHz     Breite, die sie zuletzt gemeldet hat
///                         (0 oder kleiner: noch keine)
/// \param schonUebernommen true, sobald einmal ein Ausschnitt stand
/// \param eigenerZug       true, wenn diese Meldung aus UNSEREM
///                         eigenen Zug stammt und nicht von der
///                         Maschine
inline bool fensterFolgt(double neueBreiteHz, double alteBreiteHz,
                         bool schonUebernommen, bool eigenerZug)
{
    if (!schonUebernommen)     { return true; }
    if (eigenerZug)            { return true; }
    if (!(alteBreiteHz > 0.0)) { return true; }
    // Eine wirklich andere Breite ist ein Zoom der Maschine und gilt.
    // Eine reine Mittenverschiebung nicht — siehe oben.
    return std::abs(neueBreiteHz - alteBreiteHz) > 1.0;
}

/// Nimmt das Fenster bei diesem Zug auch die BREITE der Maschine an?
///
/// Nur wenn die Maschine wirklich eine andere schickt. Bei unserem
/// eigenen Zug hat sich nur die Mitte bewegt; die Breite ist die des
/// Betreibers und bleibt.
inline bool fensterNimmtBreite(double neueBreiteHz, double alteBreiteHz,
                               bool schonUebernommen, bool eigenerZug)
{
    if (!schonUebernommen)     { return true; }
    if (!(alteBreiteHz > 0.0)) { return true; }
    if (!eigenerZug)           { return true; }
    return std::abs(neueBreiteHz - alteBreiteHz) > 1.0;
}

/// Wird der Nachzug von dieser Mittenaenderung ueberhaupt angeworfen?
///
/// Nein, wenn das Fenster nur dem VFO an den Rand nachgescrollt ist.
/// Am 2026-10-30 um 16:06 am Geraet gemessen, nach dem Wechsel auf FT8
/// (Scheibe 7.1948 -> 7.074):
///
///     04.041  Panadapter folgt dem Bild — 7.154 MHz (war 7.1948)
///     04.066  Ausschnitt gesetzt — 7.074 MHz   (die Zentrierung)
///     04.154  Ausschnitt gesetzt — 7.154 MHz   (der Nachzug gewinnt)
///
/// Zwei Zuege liefen gegeneinander: `RadioModel` zentriert den
/// Panadapter auf die Scheibe, sobald sie aus dem Bild laeuft — und
/// der Nachzug, den dasselbe Nachscrollen angeworfen hatte, zog ihn
/// auf die Randstellung zurueck. Die spaetere Quittung gewann, und
/// das Signal stand wieder 80 kHz neben der Mitte.
///
/// Den Nachzug danach zu STOPPEN genuegt nicht: er hatte seinen Befehl
/// da schon gesendet. Er darf gar nicht erst anlaufen.
inline bool nachzugWirdAngeworfen(bool mitteFolgtDemVfo)
{
    return !mitteFolgtDemVfo;
}

/// Darf der Nachzug den Panadapter der Maschine auf das Fenster
/// ziehen?
///
/// \param fensterMitteHz  Mitte des Fensters
/// \param panMitteHz      Mitte des Ausschnitts der Maschine
/// \param breiteHz        Breite des Ausschnitts der Maschine
inline bool nachzugZieht(double fensterMitteHz, double panMitteHz,
                         double breiteHz,
                         double totbandAnteil = kTotbandAnteil)
{
    if (!(breiteHz > 0.0)) { return false; }
    return std::abs(fensterMitteHz - panMitteHz) > breiteHz * totbandAnteil;
}

/// Auf welche Mitte das Fenster gestellt wird, wenn es der Maschine
/// FOLGT, ohne ihre Breite zu nehmen.
///
/// ── Das FT8-Bild bleibt auf dem Durchlassbereich (2026-11-18) ──────
///
/// Am Geraet, Bandwechsel 10 m -> 15 m im FT8-Profil:
///   11:37:44.978  FT8 — Bild auf 21.0755 MHz, 3.5 kHz
///   11:37:45.233  Ausschnitt selbst nachgefuehrt 20.974–21.174 MHz
///   11:37:45.233  Ausschnitt gesetzt auf "Pan_pan-0": 21.074 MHz, 3.5 kHz
/// Das Digi-Bild hatte die Mitte richtig auf die Mitte des
/// Durchlassbereichs gestellt (Skala + 1550 Hz, siehe
/// `DigiFrequenzen::bildausschnitt`); 255 ms spaeter zog das Folgen sie
/// auf die Mitte der MASCHINE, und das ist die Skala. Bei 3,5 kHz
/// Breite lag das Band damit von der Bildmitte bis ueber den rechten
/// Rand hinaus -- alles oberhalb 1750 Hz war nicht mehr zu sehen.
/// Betreiber: „signal ft8 im panadapter sollte standarmäßig in der
/// mitte sein."
///
/// Steht die Skala auf einer Digi-Anrufstelle und hat das Fenster noch
/// die Breite des Digi-Bildes (der Betreiber hat also nicht selbst
/// gezoomt), dann ist die Mitte des Digi-Bildes die richtige. Sonst
/// die der Maschine, wie bisher.
///
/// \param maschinenMitteHz   Mitte des Ausschnitts der Maschine
/// \param fensterBreiteHz    Breite, die das Fenster gerade zeigt
/// \param digiMitteHz        Mitte des Digi-Bildes fuer die Skala
/// \param digiBreiteHz       Breite des Digi-Bildes (0 = keines)
/// \param aufDigiAnrufstelle steht die Skala auf FT8/FT4/…?
inline double fensterMitteBeimFolgen(double maschinenMitteHz,
                                     double fensterBreiteHz,
                                     double digiMitteHz,
                                     double digiBreiteHz,
                                     bool aufDigiAnrufstelle)
{
    if (aufDigiAnrufstelle && digiBreiteHz > 0.0
        && std::abs(fensterBreiteHz - digiBreiteHz) < 1.0) {
        return digiMitteHz;
    }
    return maschinenMitteHz;
}

} // namespace FlexFensterZug
} // namespace Longpath
