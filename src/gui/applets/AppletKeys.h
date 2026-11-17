#pragma once

// =================================================================
// src/gui/applets/AppletKeys.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── EINE Kennung, nicht zwei ─────────────────────────────────────────
//
// Bis 2026-08-18 trug jedes Applet zwei Namen:
//
//   Panelkennung   „Rx"  — m_appletsById, AppletVisibilityController,
//                          der Auswaehler, das Profilfeld „visible"
//   Eigenkennung   „rx"  — AppletWidget::appletId(), die Profilfelder
//                          „order" und „floatingApplets",
//                          AppSettings „AppletStackOrder"
//
// Sie stimmen bei vier von neunzehn Applets ueberein. Bei den uebrigen
// laufen sie auseinander: rx/Rx, TX/Tx, PHCW/PhoneCw, vax/Vax, amp/Amp,
// tuner/Tuner, tci/Tci, pure_signal/PureSignal, RADE/Rade,
// tci_clients/ClientChain.
//
// Jede Stelle, an der ein Schluessel der einen Sorte in einer Karte der
// anderen nachgeschlagen wurde, gab still nullptr zurueck. Das war kein
// Absturz und keine Warnung — nur ein Nichts, das wie ein Ergebnis
// aussieht. Zwei Fehler kamen daher:
//
//   1. „Als Fenster abloesen" nahm das RX-Panel aus der Spalte, fragte
//      die Sichtbarkeit unter „rx" ab, bekam false und zeigte das
//      fertig gebaute Fenster nie. Das Applet war weg, und der Zustand
//      wurde so gespeichert.
//   2. Die gespeicherte Reihenfolge loeste beim Start nur vier von
//      vierzehn Eintraegen auf; der Rest fiel weg.
//
// Diese Datei ist die Bruecke, und sie ist absichtlich eine FREIE
// Funktion ueber einer uebergebenen Karte statt einer Methode auf
// MainWindow: MainWindow laesst sich im Pruefstand nicht bauen (ein
// blosses `MainWindow w;` startet echte UDP-Suche im Netz), und eine
// Aufloesung, die niemand pruefen kann, ist genau die Art Code, in der
// dieser Fehler entstanden ist.
//
// ── Welche Kennung gewinnt ───────────────────────────────────────────
//
// Die PANELKENNUNG, ueberall. Sie steht im Auswaehler, im Menue und im
// Profil und ist die einzige, die ein Mensch je zu Gesicht bekommt.
// appletId() bleibt, wo es hingehoert: als Eigenname des Widgets.
//
// Aufnahmen von vor dem 2026-08-18 nennen Eigenkennungen. appletFor()
// nimmt beide Sorten an, damit eine bestehende Anordnung nicht beim
// ersten Start nach diesem Update verlorengeht — dieselbe Ruecksicht
// wie bei Migration v9.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QHash>
#include <QString>
#include <QStringList>

namespace Longpath {

class AppletWidget;

/// Die Karte Panelkennung → Applet, wie MainWindow sie haelt.
using AppletMap = QHash<QString, AppletWidget*>;

namespace AppletKeys {

/// Die Panelkennung eines Applets. Steht es nicht in der Karte, ist
/// seine Eigenkennung das Beste, was es gibt — ein solches Applet steht
/// dann auch nicht im Auswaehler und wird nicht ins Profil gehalten.
QString panelIdFor(const AppletMap& map, const AppletWidget* applet);

/// Das Applet zu einer Kennung, gleich welcher Sorte.
AppletWidget* appletFor(const AppletMap& map, const QString& key);

/// Eine Kennung beliebiger Sorte auf die Panelkennung bringen. Eine
/// unbekannte Kennung bleibt unveraendert: eine Aufnahme von vor einem
/// Update kann Widgets nennen, die es nicht mehr gibt, und die sollen
/// nicht stillschweigend zu etwas anderem werden.
QString canonical(const AppletMap& map, const QString& key);

// ── Ein Panel liegt an EINEM Ort (2026-11-17) ───────────────────────
//
// Betreiber: „Das TX Fenster verschwindet immer bei FT8 nach neustart,
// aber auch wenn ich ein Profil kurz aendere und danach wieder auf FT8
// zurueckkehre."
//
// Gemessen in seiner Einstellungsdatei vom 17.11.: das Profil „FT8"
// fuehrt TX ZWEIMAL --
//
//     floatingApplets: { Tx: x 0, y 592, 397x253 }   (eigenes Fenster)
//     kombiInhalt:     [ RotorLog, Tx, Rx ]          (im Kombi-Feld)
//
// -- und das Kombi-Feld schwebt in FT8 gar nicht; es steckt in der
// Spalte, die dort niemand sieht. Beim Anwenden entsteht in Schritt 3
// das TX-Fenster, und am Ende zieht `holeKombiInhaltZurueck()` TX
// wieder heraus und in das unsichtbare Feld. Im Protokoll desselben
// Tages steht es bei JEDEM Wechsel nach FT8: „Kombi: 3 von 3 gemerkten
// Panels zurueckgeholt", und zwanzig Sekunden spaeter sein Griff
// danach: `bringAppletToFront("Tx"): "detached" ... hidden=true`.
//
// Hineingekommen ist der Widerspruch ueber den Rueckfall auf die
// GLOBALE Liste (`Kombi/Inhalt`): ein Profil ohne eigenen Eintrag bekam
// den Inhalt des zuletzt benutzten Feldes -- und die naechste Aufnahme
// schrieb ihn als den eigenen fest. Seitdem haelt er sich selbst.
//
// Die Regel, die das aufloest, ist keine Heuristik: ein Eintrag mit
// Lage und Groesse ist eine Ansage des Betreibers (er hat das Fenster
// dorthin gestellt), ein Eintrag in einer Liste ist es nicht. Fuehrt
// ein Profil ein Panel als eigenes Fenster, kommt es nicht ins Feld.
//
// Reine Listenfunktion, wie alles in dieser Datei und aus demselben
// Grund: am Schreibtisch vollstaendig pruefbar.

/// Was aus `gewuenscht` (der `kombiInhalt`-Liste eines Profils) wirklich
/// ins Feld kommt: alles, was das Profil nicht zugleich in `schwebend`
/// (den Schluesseln seiner `floatingApplets`) als eigenes Fenster
/// fuehrt. Reihenfolge bleibt, Doppelte fallen weg.
QStringList kombiInhaltOhneEigeneFenster(const QStringList& gewuenscht,
                                         const QStringList& schwebend);

} // namespace AppletKeys
} // namespace Longpath
