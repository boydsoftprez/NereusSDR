// src/gui/StyleConstants.h

// =================================================================
// src/gui/StyleConstants.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Theme palette imported from AetherSDR `src/gui/ComboStyle.h` /
//                 `HGauge.h` / `SliceColors.h` and inline QColor calls
//                 in `MainWindow.cpp` / `VfoWidget.cpp`.
// =================================================================

#pragma once
#include <algorithm>

#include <QColor>
#include <QString>
#include <QWidget>

#include "styles/ThemeQss.h"

#include <QFont>
#include <QPainter>

namespace Longpath::Style {

/// Dieselbe Schriftwahl wie monoFont(), aber fuer Stilvorlagen (QSS).
///
/// Warum es das geben muss: am 2026-10-06 stand im Protokoll
///
///     Populating font family aliases took N ms. Replace uses of missing
///     font family "SF Mono" with one that exists.
///
/// Nachgezaehlt gab es dann ACHT verschiedene Schreibweisen fuer
/// dieselbe Absicht, und auf dem Mac des Betreibers kamen dabei VIER
/// verschiedene Schriften heraus:
///
///     'SF Mono', Menlo, monospace        12x  -> Menlo
///     monospace                          12x  -> Qt-Vorgabe
///     'ui-monospace','Menlo','Consolas'   3x  -> Menlo
///     Menlo                               2x  -> Menlo
///     Menlo, monospace                    1x  -> Menlo
///     Menlo, Consolas, monospace          1x  -> Menlo
///     Consolas, 'Courier New', monospace  1x  -> COURIER NEW
///     'Monaco','Menlo',monospace          1x  -> MONACO
///     QFont("SF Mono") ohne Rueckfall     1x  -> gar nicht Monospace
///     QFont("Consolas") ohne Rueckfall    1x  -> gar nicht Monospace
///
/// SF Mono ist auf dem Mac NICHT als Familie ansprechbar (geprueft:
/// 0 Treffer in system_profiler, Menlo 26). Consolas ist Windows. Wer
/// sie ohne Rueckfall nennt, bekommt die proportionale Vorgabeschrift --
/// und genau das sah der Betreiber.
///
/// Als Makro, damit es sich an Zeichenkettenliterale anfuegen laesst:
///     "QLabel { font-family: " LP_MONO_QSS "; }"
#define LP_MONO_QSS "Menlo, 'DejaVu Sans Mono', monospace"


// ── Entblaut, 2026-08-15 ──────────────────────────────────────────────
//
// „nach [der Vorlage] sieht das aber nicht aus" — OE5SOS, mit einem Screenshot,
// auf dem dreißig Knöpfe einen leuchtend blauen Rahmen tragen: 5K, 4.4K,
// SQL, RIT, XIT, TUNE, MOX, VOX, MON, LEV, EQ, CFC, 2-Tone, PS-A. In
// der Vorlage ist ein inaktiver Knopf fast unsichtbar; sichtbar ist der eine,
// der an ist.
//
// Die Ursache war kBorder = #205070 — ein kräftiges Blau als Rahmen für
// ALLES. Dazu der durchgehende Blaustich in den Flächen: #0f0f1a,
// #0a0a18, #203040, #1a2a3a haben alle mehr Blau als Rot.
//
// Die alten Werte stehen daneben, weil das der Umbauzustand ist: was
// noch als Hex-Literal in einem Widget steht, folgt hier NICHT mit und
// bleibt alt. Diese Ungleichheit ist beabsichtigt und nützlich — sie
// zeigt genau, welche Stelle noch Arbeit braucht. Siehe
// docs/design/HAUSSTIL.md.
constexpr auto kAppBg           = "#08080a";   // war #0f0f1a
constexpr auto kPanelBg         = "#0c0c0e";   // war #0a0a18
// Hintergrund einer aktiv ausgewaehlten Kachel (z.B. VFO-Kachel) --
// eine Stufe heller als kPanelBg, aber kein eigenes Knopf-Hover.
constexpr auto kTileActiveBg    = "#16161c";
// ── Die Textleiter, 2026-08-20 angehoben ─────────────────────────────
//
// Der Betreiber: „generell ist die schrift sehr dunkel. heller waere
// besser. zahlen und zeiger aufhellen."
//
// Angehoben wurde die GANZE Leiter, nicht einzelne Werte. Die Abstaende
// zwischen den Stufen tragen die Bedeutung (Fliesstext vor Nebentext
// vor Skala vor Inaktiv); haette man nur die dunkelste angehoben, waere
// sie ihrer Nachbarin zu nah gekommen und der Unterschied zwischen
// „leise" und „aus" verloren.
//
//   text          #c4c4c9 -> #dcdce1
//   title-text    #a8a8ae -> #c4c4ca
//   secondary     #8e8e93 -> #a6a6ac
//   tertiary      #76767a -> #909096
//   scale         #5c5c60 -> #7e7e85     Skalen und Gradzahlen
//   inactive      #3d3d41 -> #58585e
//   measured      #c2924f -> #d8a55f     Zeiger und Messwerte
//
// Dieselben Werte stehen ein zweites Mal in themes/oe5sos.json: die
// Palette bedient den Malcode, die Themendatei die Stylesheets. Wer
// eine der beiden aendert, muss die andere mitziehen.
constexpr auto kTextPrimary     = "#dcdce1";   // war #c8d8e8, dann #c4c4c9
constexpr auto kTextSecondary   = "#a6a6ac";
constexpr auto kTextTertiary    = "#909096";
// ── Sechs Schriftstufen ─────────────────────────────────────────────
//
// OE5SOS, 2026-08-18: „sechs Stufen, QFont::setLetterSpacing, benannte
// Konstanten, vorher zaehlen was es schon gibt."
//
// GEZAEHLT am 2026-08-18: 641 Vorkommen in 18 verschiedenen Groessen
// (60 x setPixelSize, 581 x font-size im Stylesheet). Die haeufigsten
// sind 11 (232), 10 (186), 9 (69), 12 (72) — vier Stufen, die kaum
// auseinanderliegen und offensichtlich nicht entschieden, sondern
// gewachsen sind.
//
// Die Leiter unten deckte am 2026-08-18 zunaechst nur die drei
// Instrumente ab, absichtlich NICHT den ganzen Baum: 641 Stellen blind
// zu ersetzen waere derselbe Fehler wie die Palettenumstellung ohne
// Abbildungstabelle. Wer sie ausweitet, tut es Datei fuer Datei.
//
// UPDATE 2026-08-21: genau diese Datei-fuer-Datei-Ausweitung ist seither
// passiert — der ganze Baum wurde auf die Leiter zurueckgeholt (zwoelf
// abweichende Schriftgroessen auf null). scripts/verify-style-drift.py
// haelt seitdem den GANZEN Baum auf 0 abweichende Groessen fest (Decke
// in docs/design/style-drift-baseline.json), nicht nur die drei
// Instrumente. Wachsen dort neue Abweichungen (z.B. 2026-09-02: 20 neue
// Stellen in KiwiSDR-/Chrome-/Applet-Dateien, seither wieder auf die
// Leiter geholt), sind das keine neuen "erlaubten" Stellen ausserhalb
// der Leiter, sondern Drift, die dieselbe Datei-fuer-Datei-Pruefung
// braucht wie die urspruengliche Migration — kein blindes Ersetzen.
//
// Die Zahlen sind ein Vorschlag mit einer Stelle, an der man ihn
// aendert. Herkunft:
//   Micro    7  — die kleinsten Panadapter-Beschriftungen (dBm-Skala,
//                 Wasserfall-"LIVE"-Chip, Wasserfall-Zeitachse):
//                 setPointSize(7) am 2026-09-24 auf setPixelSize(7)
//                 umgestellt (Betreiber: die Schrift des Panadapters
//                 soll pixelfest sein, nicht DPI-abhaengig), damit
//                 sichtbar auf die Leiter genommen statt als
//                 Ausnahme durchgerutscht
//   Caption  9  — HAUSSTIL.md §Die acht Regeln, Regel 1: 8-9 px
//   Small   11  — die haeufigste Groesse im Baum
//   Body    13  — die VFO-Zeile des Frequenz-Widgets
//   Sub     16  — bisher duenn belegt (8 Vorkommen), fuellt die Luecke
//   Reading 22  — der abgelesene Wert in der Instrumenten-Fusszeile
//   Display 38  — die Frequenz selbst
//   Alarm   48  — die "HIGH SWR"-Warnung auf dem Panadapter: dieselbe
//                 2026-09-24-Umstellung, derselbe Grund
constexpr int kFontMicro   = 7;
constexpr int kFontCaption = 9;
constexpr int kFontSmall   = 11;
constexpr int kFontBody    = 13;
constexpr int kFontSub     = 16;
constexpr int kFontReading = 22;
constexpr int kFontDisplay = 38;
constexpr int kFontAlarm   = 48;

/// Laufweite der Versalzeile, als Anteil der Schriftgroesse.
/// HAUSSTIL.md §Die acht Regeln: „.18em".
///
/// Sie MUSS ueber QFont::setLetterSpacing gesetzt werden.
/// Qt-Stylesheets kennen kein letter-spacing und verwerfen es
/// kommentarlos — das steht so in der Uebergabe vom 2026-08-15 und hat
/// dort schon einmal eine Stunde gekostet.
constexpr double kCapsTracking = 0.18;

constexpr auto kTextScale       = "#7e7e85";
constexpr auto kTextInactive    = "#58585e";
// Longpath-original — used in 5+ places for AGC-T / pan / similar labels;
// sits between kTextSecondary (#8090a0) and kTextScale (#607080).
constexpr auto kLabelMid        = "#828288";
// Versalzeilen-Text ueber Gruppen (z.B. Spektrum-Overlay-Ueberschriften)
// und Fallback fuer unbekannte Zustaende -- dunkler als kTextTertiary,
// heller als kTextInactive.
constexpr auto kTextGroupHead   = "#5c5c60";
constexpr auto kAccent          = "#4a7ba8";
constexpr auto kTitleText       = "#c4c4ca";

// Borders & Surfaces
//
// kBorder ist der wichtigste Wert hier: er umrandet jeden Knopf im
// Programm. #205070 machte daraus dreißig blaue Rechtecke. #2c2c31 ist
// gerade noch da und tritt nicht mehr nach vorne — der ausgewählte
// Knopf soll auffallen, nicht alle anderen.
constexpr auto kButtonBg        = "#1a1a1e";   // war #1a2a3a
constexpr auto kButtonHover     = "#26262b";   // war #203040
constexpr auto kButtonAltHover  = "#2f3138";   // war #204060
constexpr auto kBorder          = "#2c2c31";   // war #205070
constexpr auto kBorderSubtle    = "#1f1f23";   // war #203040
// Gedaempft/aus -- eine Stufe dunkler als kBorder. Gegenstueck zu
// kGreenBorder bei LED-artigen An/Aus-Zustaenden, auch fuer blassen
// Knopftext auf dunklem Grund.
constexpr auto kBorderMuted     = "#3d3d41";
// Grundflaeche des Panadapters, unter Bild und Raster.
//
// Grau-Blau statt des fast schwarzen #0a0a12: der Betreiber hat am
// 2026-08-20 gesagt, der Standardhintergrund solle genau so aussehen
// wie auf den Logo-Vorschlaegen. Dort steht der Verlauf
// #1a2733 -> #0e151b; dies ist sein Mittelwert als Flaeche.
//
// Eine gespeicherte eigene Farbe (PanadapterBackgroundFill) hat
// Vorrang — dieser Wert ist nur die Vorgabe fuer alle, die keine
// gewaehlt haben.
constexpr auto kPanadapterBg    = "#141e27";
// Tiefsee der Blue-Marble-Kugel: die Farbe, die eine FEHLENDE Kachel
// bekommt, damit das Loch wie Ozean aussieht und nicht wie ein
// Schwarzbild. Aus dem GIBS-Foto gemessen (Pazifik, Level 3), nicht
// aus der Hauspalette — die Kugel ist ein Foto, kein Chrom.
constexpr auto kMapOceanDeep    = "#081024";
// ── Azimutalkarte des Logbuchs (2026-11-10) ─────────────────────────
//
// Die Bandfarben des Entwurfsblatts vom 09.11. (Betreiber: „absoluter
// wahnsinn"): gedaempft, damit tausende Strahlen uebereinander nicht
// zu einem Farbbrei werden. 20 m traegt Bernstein (kAmberText), 6 m
// Kupfer statt Rot, nirgends Auswahl-Blau. Eine Sprache fuer Strahl,
// Punkt und Legende -- darum hier und nicht im Widget.
constexpr auto kAzBand160m      = "#7f78a8";
constexpr auto kAzBand80m       = "#9a7aa0";
constexpr auto kAzBand60m       = "#8a8a8a";
constexpr auto kAzBand40m       = "#5f9aa8";
constexpr auto kAzBand30m       = "#6fa58a";
constexpr auto kAzBand17m       = "#c4b35a";
constexpr auto kAzBand15m       = "#a8c46a";
constexpr auto kAzBand12m       = "#7fbf9a";
constexpr auto kAzBand10m       = "#e0c890";
constexpr auto kAzBand6m        = "#c98a6a";
constexpr auto kAzBand4m        = "#b89a7a";
constexpr auto kAzBand2m        = "#efe6cf";
constexpr auto kAzBand70cm      = "#b0a090";
constexpr auto kAzBand23cm      = "#a09888";
// Entfernungsringe und kurze Gradstriche (GRID des Blatts).
constexpr auto kAzRing          = "#2a2a30";
// Die nachgeschlagene Station: warmes Weiss, heller als jedes Band.
constexpr auto kAzZiel          = "#f2e6c8";
// Die Nachtseite, als Lasur ueber dem Weltbild (Deckung im Widget).
constexpr auto kAzNacht         = "#04060e";
// ── Eigener Wert, nicht derselbe wie der Seitengrund ────────────────
//
// kInsetBg trug bis zum 2026-08-21 EXAKT denselben Wert wie kAppBg
// (#08080a). Das ist nicht nur Geschmack, es macht die Rolle
// unbrauchbar: die Themen-Umsetzung schluesselt ueber den FARBWERT,
// und wer denselben Wert zweimal vergibt, bekommt nur die erste Rolle
// — app-bg steht frueher in der Tabelle. Ein Thema konnte die
// Eingabefelder deshalb nicht anfassen, ohne den ganzen Seitengrund
// mitzunehmen.
//
// Dieselbe Krankheit wie „#203040 war vier Konstanten auf einmal"
// weiter unten in ThemeQss.cpp, nur unbemerkt geblieben, weil beide
// Werte gleich AUSSEHEN.
//
// Gefunden von tst_theme_form_knobs: die Pruefung verlangte, dass ein
// Thema die Feldfarbe aendert, und das ging nicht.
//
// Der neue Wert ist zwei Stufen tiefer als der Seitengrund — was fuer
// eine VERSENKTE Flaeche ohnehin richtig ist: ein Feld liegt im Grund,
// es liegt nicht darauf.
constexpr auto kInsetBg         = "#050507";   // war #08080a (= kAppBg!)
// Verlaufsanfang einer Mulde -- bewusst eine Stufe heller als kInsetBg,
// nicht derselbe Wert: der Verlauf braucht beide Enden.
constexpr auto kInsetBgTop      = "#101014";
constexpr auto kInsetBorder     = "#232329";   // war #1e2e3e
constexpr auto kGroove          = "#1f1f23";   // war #203040

// Title Bar Gradient
constexpr auto kTitleGradTop    = "#26262b";
constexpr auto kTitleGradMid    = "#1c1c20";
constexpr auto kTitleGradBot    = "#141417";
constexpr auto kTitleBorder     = "#0d0d0f";

// Active/Checked Button States
constexpr auto kGreenBg         = "#1c3a2a";
constexpr auto kGreenText       = "#6fa384";
constexpr auto kGreenBorder     = "#2c5c44";
// "Verbunden/live" -- kraeftiger und gesaettigter als das gedaempfte
// kGreenText, fuer einen echten Live-Zustand statt einer ruhigen
// Erfolgsmeldung (z.B. Spracherkennung "hoert", KiwiSDR verbunden).
constexpr auto kLiveGreen       = "#4caf6a";
// Dunkle Phase eines langsamen Verbindungs-Pulses (TitleBar Probing/
// Connecting), abwechselnd mit kAccent. Bewusst NICHT kBlueBg: der
// Puls braucht einen deutlich gedaempfteren Gegenpart, kBlueBg wurde
// 2026-08-21 genau umgekehrt (gesaettigter, damit der aktive Knopf
// auffaellt) gerichtet.
constexpr auto kBluePulseDim    = "#254a72";
// Unterste Stufe einer mehrstufigen Zustandsskala ("still,
// unauffaellig" -- z.B. Paketverlust nahe null, vor Gruen/Amber/Rot).
constexpr auto kQuietTone       = "#4a5a52";
// ── Der aktive Zustand ───────────────────────────────────────────────
//
// Der Betreiber, 2026-08-21, zur Vorlage: „das design hat teilweise mehr
// stil ... ich glaube die farblichen verlaeufe machen es."
//
// Die Verlaeufe waren es nicht — die stehen seit zwei Tagen drin und
// sind nachgemessen. Gemessen wurde stattdessen die SAETTIGUNG des
// aktiven Knopfes: bei uns 65 Prozent, in der Vorlage 100. Das alte #254a72
// hatte 39 Prozent Saettigung im Farbwert selbst; es sagte nicht „das
// hier ist an", sondern „das hier ist irgendwie anders".
//
// Der neue Wert ist gesaettigt und liegt im selben Farbton (rund
// 215 Grad), damit nichts anderes in der Palette dagegen steht.
//
// Fuer helle Themen aendert sich dadurch NICHTS: „Kreide" fuehrt
// sel-bg und sel-border selbst (#e0d6ee / #a889cc). Ein gesaettigtes
// Blau auf hellem Grund waere laut — das ist der Grund, warum die
// Rolle dort ueberschrieben wird und nicht hier.
constexpr auto kBlueBg          = "#3576e0";   // war #254a72 (S 39 %)
constexpr auto kBlueText        = "#ffffff";   // war #cfe2f5
constexpr auto kBlueBorder      = "#2a5fbe";   // war #2f5c86
constexpr auto kBlueHover       = "#2d5885";
constexpr auto kAmberBg         = "#33280f";
constexpr auto kAmberText       = "#d8a55f";
// Die gedaempfte Messwertstufe. Sie stand bis 2026-08-17 dreimal als
// nacktes #6b5630 im Baum -- dateilokal in HGauge.cpp:13, noch einmal
// in InstrumentPainter.cpp und als Literal in VfoWidget. Eine Farbe mit
// drei Wohnorten und keinem Namen ist genau die, die beim naechsten
// Feinschliff an zwei Stellen mitgezogen wird und an einer nicht.
constexpr auto kAmberDim        = "#6b5630";
constexpr auto kAmberBorder     = "#6b5426";
constexpr auto kAmberWarn       = "#a8853f";
// "Beschaeftigt/wartet" -- ein heller Bernstein fuer Uebergangs-
// zustaende (PureSignal "wartet", KiwiSDR Connecting/Waiting/Busy),
// unterscheidbar von kAmberWarn (Warnung) und kSwrProtActive (Schutz).
constexpr auto kBusyAmber       = "#d4a23c";
// ── Abzeichen: Paare aus Grund und Text ─────────────────────────────
//
// OE5SOS, 2026-08-17: „keine errechneten Deckkraftwerte, sondern
// benannte Paare aus Grund und Text — wie danger-bg mit Rot-Text."
//
// StatusBadge legte bis dahin eine Deckkraft von 10-20 % ueber den
// Untergrund: rgba(255,96,96,51) und Geschwister. Zwei Gruende, warum
// das weg ist:
//
//   Eine Deckkraft ueber einem UNBEKANNTEN Grund ergibt je nach
//   Untergrund etwas anderes. Dasselbe Abzeichen sah in der
//   Statuszeile anders aus als im RxDashboard, ohne dass irgendwo
//   stand, welcher Ton herauskommen sollte.
//
//   Ein Paar haelt Grund und Text zusammen. Genau das war beim
//   Abzeichen auseinandergelaufen (siehe StatusBadge::applyStyle).
//
// ── Woher die Werte kommen ──────────────────────────────────────────
//
// Ausgerechnet, nicht geschaetzt: fuer jede Variante der sichtbare
// Abstand ihrer bisherigen Wasche vom Panelgrund (#0c0c0e), gemessen
// in CIE-Lab wie in tools/colour_audit.py, und dann die Deckkraft
// gesucht, die mit der HEUTIGEN Schriftfarbe denselben Abstand ergibt.
//
// Der Unterschied ist noetig, weil die alten Waschen aus den alten
// HELLEN Farben gerechnet waren -- Gold #ffd700 unter einer Schrift,
// die seit dem Hausstil Bernstein #c2924f ist, und Signalrot #ff6060
// unter #c25a5c. Dieselbe Deckkraft mit einer gedaempften Farbe
// verschwindet; derselbe ABSTAND bleibt sichtbar.
//
//   Info  #141c27 -> #161e27   (dE 10,0)
//   On    #14251b -> #212b27   (dE 14,9)
//   Off   #15171b -> #18181a   (dE  5,0)
//   Warn  #29240c -> #372c1d   (dE 19,7)
//   Tx    #3d1d1e -> #3f2224   (dE 20,5)
//
// Eigene Abstufung neben kGreenBg/kAmberBg/kRedBg: die sind die
// Fuellung eines Knopfes (TgxlAdvancedPage:648-664), ein Abzeichen ist
// eine Andeutung. Zwei Staerken, zwei Namensfamilien.
constexpr auto kBadgeInfoBg     = "#161e27";
constexpr auto kBadgeOkBg       = "#212b27";
constexpr auto kBadgeOffBg      = "#18181a";
constexpr auto kBadgeWarnBg     = "#372c1d";
constexpr auto kBadgeTxBg       = "#3f2224";

// Das kraeftigere Sende-Rot. HAUSSTIL.md §Bedeutung: „Sendet / Gefahr
// #a86b6d — nur MOX/TX darf kraeftiger, #c25a5c". Stand als Literal in
// StatusBadge und hatte keinen Namen.
constexpr auto kTxRed           = "#c25a5c";

constexpr auto kRedBg           = "#7a2c2e";
constexpr auto kRedText         = "#f0dcdc";
constexpr auto kRedBorder       = "#a86b6d";

// ── Die Sendetaste ────────────────────────────────────────────────────
//
// Bis zum 2026-09-02 trug MOX kTxRed — dieselbe Farbfamilie wie die
// SWR-Warnung zwei Zeilen darueber. Zwei verschiedene Dinge in einer
// Farbe: „ich sende" und „es stimmt etwas nicht".
//
// OE5SOS am Entwurfsblatt, 2026-09-02: „entwurf B, aber das rot finde
// ich nicht sehr schoen" — gewaehlt wurde daraufhin der Ton „Messing":
// der Panelakzent des Hausstils (linear-gradient #a87a3c → #8a6a30) als
// FLAECHE, mit dunkler Schrift darauf. Eine gravierte Taste, kein
// Leuchtschild. Damit ist Rot im TX-Feld wieder eindeutig die Warnung.
//
// TUNE ist dieselbe Art Handlung und bekommt denselben Ton eine Stufe
// leiser — sonst stehen zwei gleich laute Tasten nebeneinander und
// keine ist mehr die wichtigere.
constexpr auto kTxKeyTop        = "#a87a3c";
constexpr auto kTxKeyBottom     = "#8a6a30";
constexpr auto kTxKeyBorder     = "#c2924f";
constexpr auto kTxKeyText       = "#17130a";
constexpr auto kTxKeyDimTop     = "#7a5a2c";
constexpr auto kTxKeyDimBottom  = "#634c24";
constexpr auto kTxKeyDimBorder  = "#9a763c";

// Eine Marke IM versenkten Feld, die nur andeuten soll, wo etwas
// anfaengt und aufhoert — im FT8-Fensterbalken sind das Signalanfang
// und -ende. Dunkles Messing: dieselbe Familie wie kTxKeyBottom, aber
// so weit heruntergezogen, dass sie nicht mit dem Fortschrittsbalken
// darueber streitet. Eine gefuellte Flaeche an dieser Stelle wurde im
// Entwurfsblatt vom 2026-10-30 ausprobiert und verworfen: sie las
// sich wie ein zweiter Balken.
constexpr auto kMarkeImGlas     = "#2a2520";

// Der SWR-Schutz, wenn er greift. Stand als nacktes „#c2924f" in
// TxApplet::wireControls.
constexpr auto kSwrProtActive   = "#c2924f";

// ── Instrumente ───────────────────────────────────────────────────────
//
// S-Meter, SWR-Zifferblatt, Gain Reduction. Ein Instrument bekommt seine
// Wirkung aus Licht, nicht aus Farbe.
//
// Vorher war der S-Meter-Bogen zur Hälfte blau und zur Hälfte rot, und
// die Teilung kodierte damit nichts — es war eine Skala, die zufällig
// zweifarbig war. Rot bedeutet in diesem Programm „Achtung"; es bedeutete
// oben rechts im Bild dauerhaft „Skalenteil".
//
// Jetzt: ein cremeweißer Bogen über die ganze Länge, darunter ein
// entsättigtes Glimmen, und die Grenze als kurze Marke statt als halber
// Bogen. Die Information bleibt, die Fläche verschwindet.
//
// Der Glimmton ist absichtlich keiner, den man benennen würde. Sobald
// man ihn „olivgrün" nennen kann, ist er zu kräftig.
// ── Heller, auf Wunsch des Betreibers (2026-08-22, +40 %) ───────────
//
// "mir kommt auch vor, dass beim testen die swr und stehellenanzeigen
// mehr bleuctet sind, als dann später in der app" — und nach einem
// Vergleichsblatt mit vier Stufen: "plus 40 % probieren wir mal bei
// swr und stehwelle".
//
// Gerechnet wurde auf der Helligkeit (HSL), nicht multiplikativ auf
// RGB — dieselbe Lehre wie bei den Flaechenverlaeufen: auf dunklem
// Grund tut lighter() fast nichts.
//
// EHRLICH ZUM BOGEN: #c8c8c0 lag schon bei 76 % Helligkeit, +40 %
// laeuft also in reines Weiss. Das ist genau das, was im
// Vergleichsblatt zu sehen war und ausgewaehlt wurde — aber es nimmt
// dem Bogen den warmen Stich. Deshalb steht er hier auf #f2f2ec statt
// #ffffff: praktisch die volle Aufhellung, mit einem Rest Farbe. Wer
// das reine Weiss will, setzt die Zeile auf #ffffff.
//
// Das GLIMMEN traegt den groesseren Teil des Eindrucks; dort greift
// die Aufhellung voll.
constexpr auto kInstrumentFace    = "#f2f2ec";  // Bogen, Zeiger, Teilung
constexpr auto kInstrumentGlowHi  = "#817b5c";  // Mitte des Glimmens
constexpr auto kInstrumentGlowLo  = "#47463b";  // Rand des Glimmens
constexpr auto kInstrumentLimit   = "#a86b6d";  // wo es zu viel wird

// ── Spektrum und Gitter ───────────────────────────────────────────────
//
// Die zwei größten Flächen im Fenster. Solange die Kurve neonzyan und
// die Frequenzskala knallgelb ist, nützt eine dezente Palette drumherum
// nichts — zwei Drittel des Bildes schreien weiter.
//
// Die Kurve gehört zur Akzentfamilie (kAccent #4a7ba8), ist aber eine
// Spur heller: sie ist das, worauf man den ganzen Abend schaut, und
// muss vor dem Rauschteppich stehen, ohne zu leuchten.
//
// Das Gitter kommt ohne Alpha aus dieser Datei — die Deckung setzt der
// Malcode, weil fein und grob dieselbe Farbe bei anderer Deckung sind.
// Eine Farbe hier zu verdoppeln, nur um zwei Alphawerte zu tragen, wäre
// ein Wert zu viel in der Theme-Datei.
//
// Die Bandkante bleibt rot und wird hier NICHT gedämpft: sie sagt, wo
// das Senden aufhört, erlaubt zu sein. Eine Sicherheitsmarke leiser zu
// drehen, weil sie auffällt, hieße das Merkmal abzuschaffen.
// Warm, nicht blau. Der Hausstil trennt nach Aufgabe, nicht nach
// Geschmack: „Blau = anfassbar. Warm = gemessen." Die Spektrumkurve ist
// das Gemessene schlechthin, und sie stand als einziger Messwert im
// Programm auf einem Blauton. HAUSSTIL.md führt beide in einer Zeile:
// „Messwert / Kurve #c2924f".
//
// Die Rolle heißt weiter „trace" und nicht „measured", damit eine
// Theme-Datei die Kurve getrennt vom übrigen Bernstein setzen kann —
// nur ihr Ausgangston ist jetzt der Messwert-Ton.
constexpr auto kSpectrumTrace     = "#c2924f";  // Kurve und Füllung
constexpr auto kSpectrumGrid      = "#8a8f96";  // Gitterlinien (Alpha im Code)
constexpr auto kSpectrumGridText  = "#9aa0a8";  // Frequenz- und dBm-Skala

// Die Kurve im Bandfilter-Fenster (BandwidthFilterPane, Rolle „trace").
// Ein eigener, gedämpfterer Bernstein als kSpectrumTrace, am 2026-08-23
// gegen OpenHPSDR-Bilder eingestellt und im Malcode noch aufgehellt und
// gesättigt. Bis zur Stildrift-Prüfung vom 2026-09-18 stand der Wert
// namenlos als Vorgabe hinter dem Rollennamen; ob er in kSpectrumTrace
// aufgeht, ist eine Gestaltungsentscheidung, keine hier.
constexpr auto kFilterPaneTrace   = "#c8a06a";

// Die Vorgabe des Beobachtungslisten-Lichts im Spot-Hub
// (AppSettings "SpotWatchlistColor"). Noch das abgeschaffte Türkis aus
// HAUSSTIL.md („Blau = anfassbar. Warm = gemessen."); als Vorgabewert
// benannt, damit die Stildrift-Prüfung ihn sieht — welcher Ton es
// stattdessen wird, entscheidet der Betreiber beim nächsten Aufräumen.
constexpr auto kSpotWatchlightDefault = "#00b4d8";

// Die Vorgabe der Roundtable-Spots am Panadapter (AppSettings
// "Roundtable/SpotColor", 2026-11-14). Ein gedämpftes Lavendel, weil die
// Nachbarn schon belegt sind: Cluster lohfarben, RBN kAccent, FT8
// kGreenText, die Beobachtungsliste Türkis. Vorläufig — welcher Ton es
// wird, entscheidet der Betreiber.
constexpr auto kSpotRoundtableDefault = "#9c8fc4";

// Gauge Fill Zones
// ── Telling one equaliser band from another ──────────────────────────
//
// Eight hues so that overlapping bands on one curve can be told apart.
// This is not decoration: with eight filters on top of each other, hue
// is the only thing that says which handle belongs to which curve.
//
// Grey at both ends on purpose — those slots hold the high-pass and
// low-pass slopes, which are shapes rather than bumps and should not
// compete for attention with the shaping bands in the middle.
//
// Four of the eight are Longpath's existing accents. Three are new
// (2026-08-11): the palette needs six distinguishable hues between the
// greys and Longpath only had four, so a coral, a blue and a violet
// were added, spaced to stay apart on #0a0a18 and chosen to sit with
// the accents rather than beside them.
constexpr auto kEqBand0 = "#8090a0";   // grey — kTextSecondary, HP slot
constexpr auto kEqBand1 = "#ff8850";   // coral, new
constexpr auto kEqBand2 = "#ffb800";   // amber — kAmberText
constexpr auto kEqBand3 = "#00ff88";   // green — kGreenText
constexpr auto kEqBand4 = "#4a7ba8";   // teal  — kAccent
constexpr auto kEqBand5 = "#4a80ff";   // blue, new
constexpr auto kEqBand6 = "#a070e0";   // violet, new
constexpr auto kEqBand7 = "#8090a0";   // grey — kTextSecondary, LP slot

constexpr auto kGaugeNormal     = "#4a7ba8";
constexpr auto kGaugeWarning    = "#a8853f";
constexpr auto kGaugeDanger     = "#a86b6d";
constexpr auto kGaugePeak       = "#c4c4c9";

// ── Glas & Tiefe ─────────────────────────────────────────────────────
//
// Die Richtung, die der Betreiber am 2026-09-17 aus vier Stilblaettern
// gewaehlt hat (docs/design/2026-09-17-design-durchsicht.md, Blatt 3):
// Panels mit Verlauf und Schatten, Zeichenflaechen VERSENKT (Schwarz,
// Innenschatten oben, Lichtkante), Knoepfe ERHABEN (Verlauf, Lichtkante,
// dunkle Unterkante), Zahlen in schwarzen Glaschips, Kurven mit Hof.
// Die Grundregeln des Hausstils bleiben (zwei Prozent Farbe, Blau ist
// anfassbar, Warm ist gemessen); das hier ist ihre Ausformung.
//
// "Kurven mit Hof" steht NICHT hier, sondern in `gui/KurvenStreifen.h`:
// es ist Geometrie fuer die GPU, keine Farbe. Dort auch die Deckkraft
// des Hofs -- 22 %, derselbe Wert, mit dem docs/design/HAUSSTIL.md
// §Weiche Uebergaenge die Fuellung unter der Kurve auslaufen laesst,
// damit nicht zwei Deckkraft-Vokabulare nebeneinander stehen.
//
// Licht und Schatten sind ALPHA-Werte auf Weiss bzw. Schwarz, keine
// Farben: dieselbe Kante liegt auf jedem Grund richtig.
constexpr auto kGlassPanelTop   = "#17171b";   // Panelverlauf oben
constexpr auto kGlassPanelBot   = "#0e0e10";   // Panelverlauf unten
constexpr auto kGlassBtnTop     = "#222227";   // erhabener Knopf, oben
constexpr auto kGlassBtnBot     = "#141417";   // erhabener Knopf, unten
// Eine Auswahl ist eine MULDE, keine Farbflaeche (2026-11-02).
//
// Betreiber, zum wiederholten Mal und zuletzt deutlich: "ich will
// diesen blauen hintergrund ueberall weg, er kommt immer wieder und
// es ist aergerlich, dass ich das immer wieder sagen muss."
//
// Er hat recht, und es war nie Geschmack: der Hausstil sagt, Farbe
// bedeckt hoechstens zwei Prozent. Ein gefuellter blauer Knopf ist
// eine FLAECHE -- und bei zwanzig Bandtasten, Betriebsarten und
// Filtern sind es zwanzig davon.
//
// Erkennbar bleibt die Auswahl ohne einen Tropfen Farbe, in
// derselben Sprache, die das Haus ueberall spricht: Licht kommt von
// oben, also liegt das Gewaehlte TIEFER als die ungewaehlten
// (aufgesetzten), seine Schrift ist heller und sein Rahmen auch.
//
// Diese vier Werte sind die EINE Stelle. Wer sie aendert, aendert
// jeden gewaehlten Knopf im Programm -- darum kam das Blau nach
// jeder Einzelbehebung zurueck.
constexpr auto kGlassSelTop     = "#131317";   // Auswahl, oben (Mulde)
constexpr auto kGlassSelBot     = "#0d0d10";   // Auswahl, unten
constexpr auto kGlassSelBorder  = "#55555e";   // heller als kBorder
constexpr auto kGlassSelText    = "#f0f0f4";   // heller als kTextPrimary
constexpr int  kGlassLightAlpha  = 18;   // Lichtkante: Weiss mit diesem Alpha
constexpr int  kGlassShadeAlpha  = 110;  // Innenschatten: Schwarz, erste Zeile
constexpr auto kGlassLight       = "rgba(255, 255, 255, 18)";   // dieselbe Kante fuer Stylesheets
constexpr auto kGlassShade       = "rgba(0, 0, 0, 160)";
constexpr int  kGlassChipRadius  = 7;
constexpr int  kGlassPanelRadius = 10;

// ── Mal-Helfer fuer Zeichenflaechen (QPainter) ───────────────────────
//
// Dieselben Bauformen wie die Stylesheets, fuer Widgets, die selbst
// malen (Bandfilter, Panadapter-Chrom, Instrumente). Alphas sind
// Zahlen, keine Farben; ein echter Weichzeichner kostet bei zwanzig
// Bildern je Sekunde zu viel, vier Linien geben denselben Eindruck.

/// Innenschatten oben plus Lichtkante — fuer eine VERSENKTE Flaeche.
inline void paintInsetTop(QPainter& p, const QRect& r)
{
    static const int kShade[] = {kGlassShadeAlpha, 70, 40, 18};
    for (int i = 0; i < 4; ++i) {
        p.setPen(QColor(0, 0, 0, kShade[i]));
        p.drawLine(r.left(), r.top() + 1 + i, r.right(), r.top() + 1 + i);
    }
    p.setPen(QColor(255, 255, 255, kGlassLightAlpha));
    p.drawLine(r.left(), r.top(), r.right(), r.top());
}

/// Die Platte eines schwebenden Fensters (leichtes Rendering,
/// 2026-09-26): derselbe Verlauf wie eine gedockte Zelle
/// (glassPanelFill), feiner Rahmen und die Lichtkante oben innen aus
/// HAUSSTIL.md "Weiche Uebergaenge". Bis dahin malten ToolWindow,
/// AppletFloatingWindow und SideAreaWindow je fuer sich Verlauf und
/// Rahmen, ohne Lichtkante.
inline void paintWindowPlate(QPainter& p, const QRect& r)
{
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(hexRole(kGlassPanelTop)));
    g.setColorAt(1.0, QColor(hexRole(kGlassPanelBot)));
    p.fillRect(r, g);
    p.setPen(QColor(hexRole(kBorderSubtle)));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.setPen(QColor(255, 255, 255, kGlassLightAlpha));
    p.drawLine(r.left() + 1, r.top() + 1, r.right() - 1, r.top() + 1);
}

/// Ein Glaschip: schwarz, feiner Rahmen, Lichtkante oben innen, ein
/// Hauch Schatten darunter. Fuer Zahlen, die man liest, nicht drueckt.
inline void paintGlassChip(QPainter& p, const QRect& box, int radius)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(role("border", kBorder)));
    p.setBrush(QColor(0, 0, 0));
    p.drawRoundedRect(box, radius, radius);
    p.setClipRect(box.adjusted(1, 1, -1, -1));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QColor(255, 255, 255, kGlassLightAlpha));
    p.drawLine(box.left() + 1, box.top() + 1, box.right() - 1, box.top() + 1);
    p.setPen(QColor(0, 0, 0, 90));
    p.drawLine(box.left() + 1, box.top() + 2, box.right() - 1, box.top() + 2);
    p.restore();
}

/// Ein ERHABENER Chip wie ein Knopf: Verlauf, Lichtkante oben, dunkle
/// Kante unten. `pressed` kippt den Verlauf (eingerastet/gedrueckt).
inline void paintRaisedChip(QPainter& p, const QRect& box, int radius,
                            bool pressed = false)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient g(box.topLeft(), box.bottomLeft());
    const QColor top(role("glass-btn-top", kGlassBtnTop));
    const QColor bot(role("glass-btn-bot", kGlassBtnBot));
    g.setColorAt(0.0, pressed ? bot : top);
    g.setColorAt(1.0, pressed ? top : bot);
    p.setBrush(g);
    p.setPen(QColor(role("border", kBorder)));
    p.drawRoundedRect(box, radius, radius);
    p.setClipRect(box.adjusted(1, 1, -1, -1));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(pressed ? QColor(0, 0, 0, kGlassShadeAlpha)
                     : QColor(255, 255, 255, kGlassLightAlpha));
    p.drawLine(box.left() + 1, box.top() + 1, box.right() - 1, box.top() + 1);
    p.setPen(pressed ? QColor(255, 255, 255, kGlassLightAlpha)
                     : QColor(0, 0, 0, kGlassShadeAlpha));
    p.drawLine(box.left() + 1, box.bottom() - 1, box.right() - 1, box.bottom() - 1);
    p.restore();
}

// Disabled
constexpr auto kDisabledBg      = "#141417";
constexpr auto kDisabledText    = "#4e4e53";
constexpr auto kDisabledBorder  = "#232327";

// Overlay
constexpr auto kOverlayBtnBg    = "rgba(20, 30, 45, 240)";
constexpr auto kOverlayPanelBg  = "rgba(15, 15, 26, 220)";
constexpr auto kOverlayBtnHover = "rgba(0, 112, 192, 180)";
constexpr auto kOverlayBorder   = "#2c2c31";

// Sizes
constexpr int kTitleBarH        = 16;
constexpr int kButtonH          = 22;
constexpr int kOverlayBtnW      = 68;
constexpr int kOverlayBtnH      = 22;
constexpr int kSliderGrooveH    = 4;
constexpr int kSliderHandleW    = 10;
constexpr int kSliderHandleH    = 10;
constexpr int kAppletPanelW     = 260;

// Status Bar
constexpr int kStatusBarH       = 46;
constexpr auto kStatusBarBg     = "#0a0a0c";
constexpr auto kStatusBarBorder = "#1f1f23";
constexpr auto kStatusSep       = "#2c2c31";

// Shared Stylesheet Fragments
// ── Flaechen mit Verlauf statt flacher Fuellung ──────────────────────
//
// Der Betreiber, 2026-08-21, mit einem Bildschirmfoto der Vorlage: „die
// uebergaenge im hintergrund bei den widget wirken sehr gut" — und
// nach dem Entwurfsblatt: „sehe keinen unterschied beim den pdf.
// mache es wie bei [der Vorlage]."
//
// Der Unterschied ist benennbar: in der Vorlage ist keine Flaeche eine
// Farbe, sondern ein Verlauf von oben nach unten, mit einer helleren
// Oberkante. Das Auge liest das als Licht von oben und damit als
// Flaeche mit DICKE — ein Stueck Blech statt eines Rechtecks. Bei uns
// war alles flach.
//
// Zwei Regeln, mehr steckt nicht dahinter:
//
//   AUFGESETZT (Platte, Kopfleiste, Knopf): oben heller, unten dunkler,
//   eine helle Linie an der Oberkante.
//
//   VERSENKT (Mulde, Eingabefeld, Rille): genau ANDERSHERUM — oben
//   dunkel, unten heller. Dasselbe Mittel, umgedreht, und die Flaeche
//   liegt drin statt drauf.
//
// Kein neuer Farbton: alle Stufen kommen aus der Familie, die die
// Palette ohnehin fuehrt. Und alles an EINER Stelle — wer hier etwas
// aendert, aendert das ganze Programm mit.
//
// Ein senkrechter Verlauf in Qt-Stylesheets:
//   qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 …, stop:1 …)
// Die Stufen werden aus der Grundfarbe GERECHNET, nicht getippt.
// Grund: die Themenverwaltung tauscht Farben ueber ihren Hexwert aus
// (ThemeQss). Ein von Hand gesetztes "#22222a" kennt sie nicht — im
// hellen Thema „Kreide" waere der Knopf dunkel geblieben. Also: die
// Grundfarbe durch hexRole() durchs Thema schicken und die helle und
// dunkle Stufe daraus ableiten. Dann stimmt jedes Thema von selbst,
// auch die, die es noch nicht gibt.
// Die Stufen werden AUS der Grundfarbe gerechnet, nicht getippt.
//
// Zwei Gruende. Erstens die Themenverwaltung: sie tauscht Farben ueber
// ihren Hexwert aus (ThemeQss). Ein von Hand gesetztes "#22222a" kennt
// sie nicht — im hellen Thema „Kreide" waere der Knopf dunkel
// geblieben. Also geht die Grundfarbe durch hexRole() und die Stufen
// werden daraus abgeleitet; dann stimmt jedes Thema von selbst, auch
// die, die es noch nicht gibt.
//
// Zweitens die Rechenart. Der erste Versuch nahm QColor::lighter(118),
// und das ist eine MULTIPLIKATION der Helligkeit. Auf unserem fast
// schwarzen Grund (#1a1a1e, Helligkeit 26) sind 18 Prozent davon
// gerade fuenf Stufen — der Testknopf kam auf einen Abstand von 6,
// und genau das war der Fehler, den der Betreiber am Entwurfsblatt
// gesehen hat: „sehe keinen unterschied". Auf Dunkel muss ADDIERT
// werden. shiftL() legt einen festen Betrag drauf, unabhaengig davon,
// wie dunkel der Grund ist.
inline QString shiftL(const QColor& base, int deltaL)
{
    QColor c = base.toHsl();
    const int l = std::clamp(c.lightness() + deltaL, 0, 255);
    c.setHsl(c.hslHue(), c.hslSaturation(), l, c.alpha());
    return c.name();
}

/// AUFGESETZT: oben heller, unten dunkler — Licht von oben, die
/// Flaeche bekommt Dicke. Fuer Platten, Kopfleisten, Knoepfe.
inline QString raisedFill(const char* baseHex, int lift = -1, int drop = -1)
{
    // Tiefe aus dem Thema, sofern es eine Meinung hat. Die Aufrufer
    // duerfen weiter eigene Werte setzen — der aktive Knopf etwa ist
    // absichtlich etwas kraeftiger als eine ruhende Platte.
    if (lift < 0) { lift = formInt("relief", 16); }
    if (drop < 0) { drop = lift * 3 / 4; }
    const QColor base(hexRole(baseHex));
    return QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %1, stop:0.55 %2, stop:1 %3)")
        .arg(shiftL(base, lift), base.name(), shiftL(base, -drop));
}

/// VERSENKT: genau andersherum — oben dunkel, unten heller. Dasselbe
/// Mittel, umgedreht, und die Flaeche liegt drin statt drauf.
inline QString sunkenFill(const char* baseHex, int deepen = -1, int lift = -1)
{
    if (deepen < 0) { deepen = formInt("mulde", 10); }
    if (lift < 0)   { lift   = deepen * 6 / 5; }
    const QColor base(hexRole(baseHex));
    return QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %1, stop:1 %2)")
        .arg(shiftL(base, -deepen), shiftL(base, lift));
}

// ── Griffleisten zwischen den Feldern ────────────────────────────────
//
// Der Betreiber, 2026-08-21: „der pandapter muss jetzt endlich mal
// funktionieren, das ist das kernstueck" — und davor: er loese ihn nur
// ab, weil das „der einzige Weg" sei, die Groesse zu aendern.
//
// Es war nie der einzige Weg. Die Splitter waren immer da, ihre Griffe
// aber DREI PIXEL breit (MainWindow.cpp: setHandleWidth(3)), und die
// Splitter im Panadapter-Stapel setzten gar nichts, blieben also auf
// Qts Vorgabe. Ein Ziehgriff von drei Pixeln ist auf einem Retina-
// Schirm mit der Maus nicht zu treffen. Vorhanden und unerreichbar —
// dieselbe Fehlerklasse wie beim Erreichbarkeits-Durchgang, nur in
// Pixeln statt in Signalen.
//
// Sechs Pixel sind greifbar, und ein sichtbarer Strich in der Mitte
// sagt, DASS man ziehen kann. Ohne den weiss es niemand.
constexpr int kSplitterHandlePx = 6;

// ── Der eine Abstand zwischen zwei Feldern (2026-10-15) ─────────────
//
// Der Betreiber: „es sollte auch immer automatisch zwischen windows
// und windows ein minimaler automatischer Abstand sein, vielleicht
// 2 pixel. x und y, dann sieht es besser aus und gleich."
//
// „Gleich" ist das Stichwort. Gemessen am laufenden Programm stand
// zwischen zwei Applet-Feldern 6 px, links vom Fensterrand 6 px,
// rechts 8 px (Rinne fuer den Rollbalken) und oben 6 px — vier Werte
// fuer dieselbe Sache. Im ganzen Quelltext sind es fuenfzehn
// verschiedene setSpacing-Werte (0,1,2,3,4,5,6,7,8,9,10,12,14,16,22).
//
// Dieser eine Wert gilt ab jetzt fuer jeden Abstand ZWISCHEN Feldern,
// in beiden Achsen, und fuer den Rand des Fensters.
//
// NICHT fuer kSplitterHandlePx darueber: ein Ziehgriff ist kein
// Abstand, sondern ein Bedienteil. Der Betreiber hat am 2026-08-21
// selbst gemeldet, dass drei Pixel nicht zu treffen sind; sechs
// bleiben sechs.
constexpr int kPanelGapPx = 2;

// ── Die Luft ZWISCHEN zwei schwebenden Fenstern (2026-10-31) ─────────
//
// Betreiber: „ev groeszerer pixel abstand von min 3 pixel."
//
// Eine EIGENE Zahl, nicht `kPanelGapPx`: der sitzt auch in den
// Innenraendern und Abstaenden INNERHALB der Panels
// (`MainWindow.cpp`, `AppletPanelWidget.cpp`). Wer dort an der Zahl
// dreht, verschiebt den Inhalt der Felder mit — gemeint war der
// Abstand der Fenster zueinander.
//
// Das Raster rechnet sie mit ein (`snappedFrameRect`): die nahe Kante
// auf der Rasterlinie, die ferne so viel davor. Darum ist sie der
// GARANTIERTE Abstand, nicht nur ein Mindestwert, den etwas spaeter
// noch aufschlaegt.
constexpr int kFensterLuftPx = 3;

// ── Der Griff ist UNSICHTBAR, bis man ihn beruehrt (2026-10-14) ──────
//
// Der Betreiber, mit einem Bild: „Ich moechte im Hintergrund keine
// Linien haben, das muss ein schwarzer Bildschirm sein. Keinen Rahmen
// hier setzen, mit keinen Begrenzungen und so weiter."
//
// Bis heute trug jeder Griff einen HAARSTRICH (`border-left`/
// `border-top: 1px solid kBorder`) und einen eigenen Grund
// (`kPanelBg`). Das waren die senkrechten und waagerechten Linien in
// seinem Bild -- sie liefen quer durch die leere, schwarze Flaeche.
//
// Der Strich hatte seinen Grund, und der steht oben: ein Griff von drei
// Pixeln war nicht zu treffen, und „ein sichtbarer Strich in der Mitte
// sagt, DASS man ziehen kann". **Die Greifbarkeit bleibt** -- sechs
// Pixel sind sechs Pixel, ob man sie sieht oder nicht. Nur das
// DAUERHAFTE Zeigen faellt weg: beim Darueberfahren leuchtet der Griff
// in der Akzentfarbe auf, und das sagt dasselbe, ohne im Weg zu stehen.
inline QString splitterStyle()
{
    return QStringLiteral(
        // `transparent`, nicht kPanelBg: der Griff nimmt die Farbe
        // dessen an, worueber er liegt -- auf schwarzem Grund ist er
        // schwarz.
        "QSplitter::handle { background: transparent; border: none; }"
        "QSplitter::handle:horizontal { width: %1px; image: none; }"
        "QSplitter::handle:vertical { height: %1px; image: none; }"
        "QSplitter::handle:hover { background: %2; }")
        .arg(kSplitterHandlePx)
        .arg(hexRole(kAccent));
}

inline QString buttonBaseStyle()
{
    return QStringLiteral(
        "QPushButton {"
        // Polsterung 4/10 statt 2/4 und 7 statt 6 Punkt Eckenradius.
        // Klingt nach nichts und ist der halbe Eindruck von Ruhe: die
        // Knopfreihen in der Vorlage wirken nicht ruhiger, weil dort weniger
        // steht, sondern weil um jedes Ding mehr Luft ist.
        "  background: %1; border: 1px solid %2; border-radius: %6px;"
        // Glas & Tiefe (2026-09-17): ein erhabener Knopf hat oben eine
        // Lichtkante und unten eine dunkle Kante — der Rahmen ist
        // nicht rundum gleich, sondern das Licht kommt von oben.
        "  border-top-color: %9; border-bottom-color: %10;"
        "  color: %3; font-size: 11px; font-weight: bold;"
        "  padding: %7px %8px;"
        "}"
        "QPushButton:hover { background: %4; }"
        // Gedrueckt: der Verlauf kippt. Ein Knopf, der sich beim
        // Druecken nicht bewegt, fuehlt sich tot an — und das kostet
        // hier kein einziges Pixel Verschiebung.
        "QPushButton:pressed { background: %5; border-top-color: %10; border-bottom-color: %9; }"
    ).arg(raisedFill(kButtonBg),
          QLatin1String(kBorder),
          QLatin1String(kTextPrimary),
          raisedFill(kButtonAltHover, 20, 10),
          sunkenFill(kButtonBg))
     .arg(formInt("radius", 7))
     .arg(formInt("luft-v", 4))
     .arg(formInt("luft-h", 10))
     .arg(shiftL(QColor(hexRole(kBorder)), 12),
          shiftL(QColor(hexRole(kBorder)), -14));
}

inline QString greenCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kGreenBg, kGreenText, kGreenBorder);
}

inline QString blueCheckedStyle()
{
    // raisedFill statt einer flachen Fuellung: der aktive Knopf ist
    // dieselbe aufgesetzte Flaeche wie jeder andere, nur in der
    // Akzentfarbe. Zwei verschiedene Bauarten fuer denselben Knopf
    // waeren genau die Art Unstimmigkeit, die man als „unfertig" liest.
    // Glas & Tiefe (2026-09-17): der gedeckte Auswahlverlauf der
    // Kopfleisten-Pillen (kGlassSel*) statt des flachen kBlueBg — auf
    // den Applet-Blaettern leuchteten "Stereo", "Phone", "5.0k", "ON/RX/
    // TX" und die Scheibenmarke "A" in einem Blau, das die Kopfleiste
    // laengst nicht mehr trug. Eine Auswahl, eine Farbe.
    return QStringLiteral(
        "QPushButton:checked {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);"
        "  color: %3; border: 1px solid %4; border-top-color: %5; }"
    ).arg(hexRole(kGlassSelTop), hexRole(kGlassSelBot),
          hexRole(kGlassSelText), hexRole(kGlassSelBorder),
          shiftL(QColor(hexRole(kGlassSelBorder)), 16));
}

// Eingerastet, aber leise: dunkler Grund, Akzentrand, heller Text.
//
// War bis zum 2026-09-02 ein Einzelstueck am MON-Knopf der TxApplet
// ("Longpath-original one-off — do NOT snap to kBlueBg"). Seit dem
// Umbau des TX-Feldes ist es die Bauform fuer ALLE Zustandsschalter
// dort: VOX, MON, LEV, EQ, CFC, PS-A. Der Grund steht im Hausstil —
// Farbe bedeckt hoechstens zwei Prozent, also darf im Feld genau eine
// Stelle kraeftig sein, und das ist die Sendetaste (txKeyStyle).
inline QString quietCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2;"
        " border: 1px solid %3; }"
    ).arg(QLatin1String(kBadgeInfoBg),
          QLatin1String(kTextPrimary),
          QLatin1String(kAccent));
}

inline QString amberCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kAmberBg, kAmberText, kAmberBorder);
}

// Die Sendetaste, eingerastet: Messing als Flaeche, dunkle Schrift.
// `leise` ist die Stufe fuer TUNE.  Wer MOX faerbt, muss auch die
// untere Leiste faerben (TxSwitchBar) — sonst leuchtet unten weiter
// Rot, waehrend oben Messing steht.
inline QString txKeyStyle(bool leise = false)
{
    // Glas & Tiefe (2026-09-17): auch die Messingtaste ist erhaben —
    // Lichtkante oben, dunkle Unterkante, wie jeder andere Knopf.
    return QStringLiteral(
        "QPushButton:checked {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %1, stop:1 %2);"
        "  color: %3; border: 1px solid %4;"
        "  border-top-color: %5; border-bottom-color: %6;"
        "}"
    ).arg(QLatin1String(leise ? kTxKeyDimTop : kTxKeyTop),
          QLatin1String(leise ? kTxKeyDimBottom : kTxKeyBottom),
          QLatin1String(kTxKeyText),
          QLatin1String(leise ? kTxKeyDimBorder : kTxKeyBorder),
          shiftL(QColor(leise ? kTxKeyDimBorder : kTxKeyBorder), 16),
          shiftL(QColor(leise ? kTxKeyDimBottom : kTxKeyBottom), -12));
}

inline QString redCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kRedBg, kRedText, kRedBorder);
}

// DSP toggle — brighter green than the kGreenBg action buttons.
// Used by VfoWidget DSP tab and SpectrumOverlayPanel DSP toggles.
// Source: Longpath-original. Distinct semantic from action-button
// "checked" state because DSP toggles communicate "feature on" not
// "action engaged."
constexpr auto kDspToggleBg     = "#1b3527";
constexpr auto kDspToggleBorder = "#33684c";
constexpr auto kDspToggleText   = "#7fae91";

inline QString dspToggleStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kDspToggleBg, kDspToggleText, kDspToggleBorder);
}

inline QString sliderHStyle()
{
    // Glas & Tiefe (2026-09-17): die Rinne ist VERSENKT (schwarz, oben
    // die dunkle Kante, unten Licht), der Griff ERHABEN — eine Pille im
    // Knopfverlauf mit Akzentrand, nicht mehr die blaue Kugel von Qt.
    // Blau bleibt am Rand: der Griff ist anfassbar.
    return QStringLiteral(
        "QSlider::groove:horizontal {"
        "  height: 4px; background: %1; border: 1px solid %2;"
        "  border-top-color: %3; border-bottom-color: %4; border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 10px; height: 14px; margin: -6px 0;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %5, stop:1 %6);"
        "  border: 1px solid %7; border-top-color: %8; border-radius: 5px;"
        "}"
        "QSlider::handle:horizontal:hover { background: %9; }"
    ).arg(hexRole(kInsetBg), hexRole(kBorder),
          QLatin1String(kGlassShade), QLatin1String(kGlassLight),
          hexRole(kGlassBtnTop), hexRole(kGlassBtnBot),
          hexRole(kAccent), shiftL(QColor(hexRole(kAccent)), 18),
          hexRole(kGlassBtnTop));
}

inline QString sliderVStyle()
{
    // Wie sliderHStyle(), nur stehend: Rinne versenkt (Licht kommt von
    // oben, also Schattenkante oben), Griff eine liegende Pille.
    return QStringLiteral(
        "QSlider::groove:vertical {"
        "  width: 4px; background: %1; border: 1px solid %2;"
        "  border-top-color: %3; border-bottom-color: %4; border-radius: 3px;"
        "}"
        "QSlider::handle:vertical {"
        "  height: 10px; width: 14px; margin: 0 -6px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %5, stop:1 %6);"
        "  border: 1px solid %7; border-top-color: %8; border-radius: 5px;"
        "}"
        "QSlider::handle:vertical:hover { background: %9; }"
    ).arg(hexRole(kInsetBg), hexRole(kBorder),
          QLatin1String(kGlassShade), QLatin1String(kGlassLight),
          hexRole(kGlassBtnTop), hexRole(kGlassBtnBot),
          hexRole(kAccent), shiftL(QColor(hexRole(kAccent)), 18),
          hexRole(kGlassBtnTop));
}

/// Die Platte eines Panels (Glas & Tiefe, 2026-09-17): ein Verlauf von
/// oben hell nach unten dunkel und ein feiner Rahmen. Fuer schwebende
/// Fenster und gedockte Zellen dasselbe — die Applets darin sind
/// durchsichtig, damit die Platte durchscheint.
inline QString glassPanelFill()
{
    return QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2)")
        .arg(hexRole(kGlassPanelTop), hexRole(kGlassPanelBot));
}

/// Eine Tabelle hinter Glas (Glas & Tiefe, 2026-09-17): versenkt wie
/// ein Zahlenfeld, Kopfzeile ohne Kaesten — nur eine Haarlinie unter
/// den Titeln —, Auswahl im gedeckten Blauverlauf, Gitter leise. Keine
/// Schriftangaben: Groesse und Versalien setzt der Aufrufer per
/// setFont (Tabelle 11 px, Kopf capsFont 9 px), sonst kaskadiert eine
/// font-size aus dem Stylesheet in den Kopf und schlaegt sie tot.
inline QString tableStyle()
{
    return QStringLiteral(
        "QTableView, QTableWidget, QTreeView, QTreeWidget, QListView, QListWidget {"
        "  background: %1; color: %2; border: 1px solid %3;"
        "  border-top-color: %4; border-bottom-color: %5;"
        "  border-radius: %6px; gridline-color: %7; outline: none;"
        "  selection-background-color: %8; selection-color: %9;"
        // Zebra eine Spur heller als der Grund. Ohne diese Zeile nimmt
        // eine Tabelle mit setAlternatingRowColors(true) die Palette —
        // und die ist hell: auf dem Logbuch-Blatt vom 2026-09-17 stand
        // jede zweite Zeile WEISS.
        "  alternate-background-color: %12;"
        "}"
        "QTableView::item, QTableWidget::item, QTreeView::item, QListView::item {"
        "  padding: 2px 3px; border: none; }"
        "QTableView::item:selected, QTableWidget::item:selected,"
        "QTreeView::item:selected, QListView::item:selected {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %10, stop:1 %8);"
        "  color: %9; }"
        "QHeaderView { background: transparent; border: none; }"
        "QHeaderView::section { background: transparent; color: %11;"
        "  border: none; border-bottom: 1px solid %3; padding: 3px 3px; }"
        "QTableCornerButton::section { background: transparent; border: none; }"
    ).arg(sunkenFill(kInsetBg, 8, 4),
          hexRole(kTextPrimary),
          hexRole(kBorder),
          QLatin1String(kGlassShade),
          QLatin1String(kGlassLight),
          QString::number(kGlassChipRadius),
          hexRole(kBorderSubtle),
          hexRole(kGlassSelBot),
          hexRole(kGlassSelText))
     .arg(hexRole(kGlassSelTop),
          hexRole(kTextScale),
          hexRole(kGlassPanelBot));
}

/// Ein Zahlenfeld hinter Glas (Glas & Tiefe, 2026-09-17): schwarz,
/// feiner Rahmen, oben die dunkle Kante des Innenschattens, unten eine
/// Lichtkante — und KEINE Pfeile. Mausrad, Pfeiltasten und Tippen
/// bleiben; die Pfeilknoepfe waren Qt-Optik und nahmen dem Feld die
/// Ruhe. Die Schrift setzt der Aufrufer per setFont (Monospace, Regel
/// 4) — Groesse gehoert nicht ins Stylesheet (HAUSSTIL.md, die Falle).
inline QString glassFieldStyle()
{
    return QStringLiteral(
        "QSpinBox, QDoubleSpinBox, QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-top-color: %3; border-bottom-color: %4;"
        "  border-radius: %5px; color: %6; padding: 3px 8px;"
        "  selection-background-color: %7; selection-color: %8;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button,"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "  width: 0px; border: none; }"
        "QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {"
        "  border: 1px solid %9; }"
    ).arg(sunkenFill(kInsetBg, 8, 4),
          hexRole(kBorder),
          QLatin1String(kGlassShade),
          QLatin1String(kGlassLight),
          QString::number(kGlassChipRadius),
          hexRole(kTextPrimary),
          hexRole(kGlassSelBot),
          hexRole(kGlassSelText),
          hexRole(kAccent));
}

inline QString insetValueStyle()
{
    // VERSENKT: oben dunkel, unten heller — siehe die Notiz bei
    // buttonBaseStyle. Dasselbe Mittel wie bei der Platte, nur
    // umgedreht, und das Feld liegt drin statt drauf.
    // Glas & Tiefe (2026-09-17): ein Glaschip wie im Bandfilter — oben
    // die dunkle Kante, unten Licht, Monospace fuer die Zahl (Regel 4).
    return QStringLiteral(
        "QLabel {"
        "  font-size: 11px; font-family: " LP_MONO_QSS "; background: %1; border: 1px solid %2;"
        "  border-top-color: %4; border-bottom-color: %5;"
        "  border-radius: %6px; padding: 1px 4px; color: %3;"
        "}"
    ).arg(sunkenFill(kInsetBg, 8, 4),
          QLatin1String(kInsetBorder),
          QLatin1String(kTextPrimary),
          QLatin1String(kGlassShade),
          QLatin1String(kGlassLight))
     .arg(kGlassChipRadius);
}

inline QString titleBarStyle()
{
    // Die Kopfleiste soll AUFLIEGEN, nicht eingelassen sein: etwas
    // kraeftiger als die Platte darunter, und eine dunkle Unterkante,
    // die sie von ihr abhebt. Die Toene stehen weiter in der Palette
    // (kTitleGrad*) — hier ist nur der Schluss dunkler geworden.
    // Glas & Tiefe (2026-09-17): dazu eine Lichtkante am oberen Rand,
    // damit der Kopf eine Oberflaeche hat und nicht nur einen Verlauf.
    return QStringLiteral(
        "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 %1, stop:0.5 %2, stop:1 %3);"
        " border-bottom: 1px solid %4; border-top: 1px solid %5;"
    ).arg(kTitleGradTop, kTitleGradMid, kTitleGradBot, kTitleBorder,
          QLatin1String(kGlassLight));
}

// ── Setup Dialog Page Stylesheets ─────────────────────────────────────────────
// Flat string constants used by SetupPage subclasses that build their own
// layouts directly (CatNetwork, Keyboard, Diagnostics pages).

constexpr auto kPageStyle =
    "QWidget { background: #08080a; color: #c8d8e8; }";   // kAppBg -- das alte Blau #0f0f1a ist seit 2026-11-04 verboten (tst_kein_blauer_grund)

constexpr auto kGroupBoxStyle =
    // Eine Gruppe ist eine Platte im Panel: feiner Rahmen mit Licht-
    // kante, Radius 10 wie die Panels, Titel in Zweitfarbe — nicht
    // mehr blau und fett, das war die alte Setup-Palette.
    "QGroupBox { border: 1px solid #2c2c31; border-top-color: #38383e;"
    " border-radius: 10px; margin-top: 8px; padding-top: 12px;"
    " font-weight: 600; color: #a6a6ac; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";

constexpr auto kSecondaryLabelStyle =
    "QLabel { color: #8090a0; font-size: 11px; }";

// ── Der OPERATE/STANDBY-Knopf der Verstaerkerfelder ───────────────────
//
// Stand als Literal zweimal in Rf2ksApplet::setOperateMode (dort seit
// dem Prueflauf vom 2026-05-25: „standby operate button does not change
// the color of the glowing dot on mode change"). Beim SPE-Feld
// (2026-10-09) waere es ein drittes und viertes Mal geworden, und
// scripts/verify-style-drift.py hat genau das gemeldet -- nicht weil
// eine neue FARBE dazukam (alle vier Werte sind Hausfarben), sondern
// weil acht neue Literale dazukamen.
//
// Darum hier ein Name. Beide Felder nehmen ihn; wer einen dritten
// Verstaerker anbindet, nimmt ihn auch.
constexpr auto kAmpOperateActiveBtnStyle =
    "QPushButton { background: #1a6030; border: 1px solid #6fa384; "
    "border-radius: 6px; color: #ffffff; font-size: 11px; font-weight: bold; }"
    "QPushButton:hover { background: #33684c; }";
constexpr auto kAmpNeutralBtnStyle =
    "QPushButton { background: #204060; border: 1px solid #205070; "
    "border-radius: 6px; color: #c8d8e8; font-size: 11px; font-weight: bold; }"
    "QPushButton:hover { background: #204060; }";

// ── Formularstil: Glas & Tiefe (2026-09-18) ───────────────────────────
//
// Roadmap C/F, Betreiber am 17.09.: "kannst ruhig weiter designen".
// Ein Stil fuer alle Formularteile, in Applets wie im Setup-Dialog:
//
//   Auswahlfelder (QComboBox) sind ERHABEN wie ein Knopf — Verlauf,
//   Lichtkante oben, dunkle Kante unten, Radius 7 — denn sie oeffnen
//   ein Menue, das UEBER der Oberflaeche liegt. Die Liste selbst ist
//   ein dunkles Glas mit dem gedeckten Auswahlverlauf der Kopfleiste.
//
//   Haekchen und Wahlpunkte sind VERSENKT (schwarz, Schattenkante
//   oben, Lichtkante unten); angehakt fuellt sie derselbe gedeckte
//   Blauverlauf wie jede andere Auswahl. Radius 4 statt 2 — die
//   Regel "nie 3" gilt auch fuer Kleines.
//
//   Eingabefelder sind Glasfelder (glassFieldStyle), Rinnen versenkt
//   (sliderHStyle/sliderVStyle), Knoepfe erhaben (buttonBaseStyle).
//
// Feste Hexwerte statt hexRole(): die Konstanten muessen constexpr
// bleiben, weil ~200 Aufrufstellen sie als const char* erwarten; die
// Glasfarben (kGlass*) haben ohnehin keine Themenrolle. Werte:
// Kante hell = shiftL(kBorder, +12), Kante dunkel = shiftL(kBorder, -14).
constexpr auto kGlassEdgeLight = "#38383e";   // Lichtkante auf kBorder
constexpr auto kGlassEdgeDark  = "#1f1f23";   // Schattenkante auf kBorder

constexpr auto kComboStyle =
    "QComboBox {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #222227, stop:1 #141417);"
    "  color: #dcdce1; border: 1px solid #2c2c31;"
    "  border-top-color: #38383e; border-bottom-color: #1f1f23;"
    "  border-radius: 7px; padding: 3px 8px; }"
    "QComboBox:hover {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a2a30, stop:1 #1a1a1e); }"
    "QComboBox:on {"   // aufgeklappt: gedrueckt, der Verlauf kippt
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #141417, stop:1 #222227);"
    "  border-top-color: #1f1f23; border-bottom-color: #38383e; }"
    "QComboBox:disabled { color: #6a6a70; border-color: #232327; }"
    "QComboBox::drop-down { border: none; width: 18px; subcontrol-origin: padding; subcontrol-position: center right; }"
    "QComboBox::down-arrow { image: url(:/icons/spin-down.svg); width: 10px; height: 10px; }"
    // Die Liste: dunkles Glas, gedeckte Auswahl (kGlassSelBot/Text) —
    // 2026-09-08 lernte das Haus, dass selection-color ohne
    // selection-background-color den Eintrag unsichtbar macht.
    "QComboBox QAbstractItemView {"
    "  background: #0e0e10; color: #dcdce1; border: 1px solid #2c2c31;"
    "  selection-background-color: #1a1a1e; selection-color: #f0f0f4; outline: none; }"
    "QComboBox QAbstractItemView::item { padding: 3px 6px; min-height: 18px; }"
    "QComboBox QAbstractItemView::item:selected { background: #1a1a1e; color: #f0f0f4; }";

// ── Ein Haken ist ein Haken (2026-11-04) ────────────────────────────
//
// Betreiber, am Update-Dialog: "man sieht nicht, ob das Haekchen an ist
// oder nicht. das sollte ueberall kontrolliert werden." Stimmt: "an"
// war bis dahin nur ein etwas hellerer Rand (#55555e statt #2c2c31) auf
// fast demselben schwarzen Grund -- gemessen im Indikator: Helligkeit
// 17 aus, 26 an. Jetzt traegt der angehakte Zustand ein Haekchen
// (resources/icons/check.svg, Textfarbe) und der Rundknopf einen Punkt;
// abgeschaltet dasselbe in der gedaempften Schrift. Die Mulde selbst
// bleibt. Alle Stellen, die nur Breite/Hoehe des Indikators setzen
// (SpotHub, RX-Applet, ClientChain, Overlay-Menue), erben das von hier;
// AddCustomRadioDialog fuellt "an" mit kAccent und war schon sichtbar.
// Pruefstand: tests/tst_haekchen_sichtbar.cpp.
constexpr auto kCheckBoxStyle =
    "QCheckBox { color: #dcdce1; spacing: 6px; min-height: 20px; }"   // min-height: drei Haekchen untereinander kollidierten
    "QCheckBox:disabled { color: #6a6a70; }"
    "QCheckBox::indicator { width: 14px; height: 14px;"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #000000, stop:1 #08080c);"
    "  border: 1px solid #2c2c31; border-top-color: rgba(0, 0, 0, 160);"
    "  border-bottom-color: rgba(255, 255, 255, 18); border-radius: 4px; }"
    "QCheckBox::indicator:hover { border-color: #4a7ba8; }"
    "QCheckBox::indicator:checked {"
    "  image: url(:/icons/check.svg);"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131317, stop:1 #0d0d10);"
    "  border: 1px solid #55555e; border-top-color: #55555e; }"
    "QCheckBox::indicator:checked:disabled { image: url(:/icons/check-dim.svg);"
    "  background: #232327; border-color: #232327; }";

constexpr auto kRadioButtonStyle =
    "QRadioButton { color: #dcdce1; spacing: 6px; min-height: 20px; }"
    "QRadioButton:disabled { color: #6a6a70; }"
    "QRadioButton::indicator { width: 14px; height: 14px;"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #000000, stop:1 #08080c);"
    "  border: 1px solid #2c2c31; border-top-color: rgba(0, 0, 0, 160);"
    "  border-bottom-color: rgba(255, 255, 255, 18); border-radius: 7px; }"
    "QRadioButton::indicator:hover { border-color: #4a7ba8; }"
    "QRadioButton::indicator:checked {"
    "  image: url(:/icons/radio-dot.svg);"   // ein Punkt, siehe kCheckBoxStyle
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131317, stop:1 #0d0d10);"
    "  border: 1px solid #55555e; border-top-color: #55555e; }"
    "QRadioButton::indicator:checked:disabled { image: url(:/icons/radio-dot-dim.svg);"
    "  background: #232327; border-color: #232327; }";

// ── Eingabefelder sind Mulden, keine Flaechen ────────────────────────
//
// Gemeldet 2026-08-16 auf Startup & Preferences: Callsign, Grid Square,
// Process Priority und FFTW Wisdom waren "fast unsichtbar -- man sieht
// nicht mehr, dass das bedienbare Felder sind".
//
// Ursache war nicht zu viel Daempfung, sondern die falsche RICHTUNG.
// Die Werte #1a2a3a (Grund) und #304050 (Rand) wurden von themed()
// korrekt auf #1a1a1e und #2c2c31 gezogen -- zwei fast gleiche
// Grautoene. Vorher trennte der Rand durch ein kraeftiges Blau von
// selbst; ohne das liegt ein Feld auf dem Panel wie eine Flaeche.
//
// HAUSSTIL beschreibt es als VERSENKT: "Versenkt (Glas) #000000 +
// inset 0 2px 8px #000". Ein Feld gehoert DUNKLER als seine Umgebung,
// nicht heller -- dann traegt der hellere Rand von selbst.
//
// Qt-Stylesheets koennen keinen inneren Schatten. Der Grund liegt
// deshalb auf kInsetBg (#08080a, dunkler als das Panel #0c0c0e) und der
// Rand auf kBorder (#2c2c31), der gegen fast Schwarz deutlich steht.
//
// Ein leeres Feld ist nicht inaktiv. Es wartet auf eine Eingabe.
// ── Eingabefelder sind VERSENKT ──────────────────────────────────────
//
// Der Betreiber, 2026-08-21, auf ein Bildschirmfoto zeigend: „hier
// siehst du gut die uebergaenge: filter, customs. im feld von 150 bis
// 2850." Gemeint sind die Eingabefelder und Rillen — weiche
// Verlaeufe, die eine Flaeche als eingelassen lesen lassen.
//
// Bei uns waren genau die flach: „background: #08080a", fest
// eingetippt. Und schlimmer — dieser Wert stand in KEINER
// Themen-Tabelle, die Felder folgten also gar keinem Thema. Im hellen
// Thema „Kreide" blieben sie schwarz.
//
// Jetzt sind es Funktionen: sunkenFill() ueber die Themenfarbe, Tiefe
// und Eckenradius aus den Form-Reglern. Die alten Konstanten sind
// ENTFERNT, nicht danebengestellt — sonst haette der Uebersetzer die
// vergessenen Aufrufstellen nicht gefunden, und die Haelfte der
// Felder waere flach geblieben.
/// Glasfeld mit Pfeilen: dieselbe versenkte Mulde wie glassFieldStyle(),
/// aber die Pfeilknoepfe bleiben (Setup-Seiten sind Formulare, dort
/// helfen sie; die Wertchips in den Applets kommen ohne aus).
inline QString formFieldStyle()
{
    return QStringLiteral(
        "QLineEdit, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QTextEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-top-color: %3; border-bottom-color: %4;"
        "  border-radius: %5px; color: %6; padding: 3px 8px;"
        "  selection-background-color: %7; selection-color: %8;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus,"
        "QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid %9; }"
        "QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {"
        "  color: %10; border-color: %11; }"
    ).arg(sunkenFill(kInsetBg, 8, 4),
          hexRole(kBorder),
          QLatin1String(kGlassShade),
          QLatin1String(kGlassLight),
          QString::number(kGlassChipRadius),
          hexRole(kTextPrimary),
          hexRole(kGlassSelBot),
          hexRole(kGlassSelText),
          hexRole(kAccent))
     .arg(hexRole(kDisabledText), hexRole(kDisabledBorder));
}

inline QString lineEditStyle()   { return formFieldStyle(); }
inline QString spinBoxStyle()    { return formFieldStyle(); }
inline QString doubleSpinBoxStyle() { return formFieldStyle(); }

constexpr auto kSliderStyle =
    // Dieselbe Rinne wie sliderHStyle(), als Konstante fuer die Setup-
    // Seiten: versenkte Rinne, erhabener Griff mit Akzentrand.
    "QSlider::groove:horizontal { height: 4px; background: #050507;"
    " border: 1px solid #2c2c31; border-top-color: rgba(0, 0, 0, 160);"
    " border-bottom-color: rgba(255, 255, 255, 18); border-radius: 3px; }"
    "QSlider::handle:horizontal { width: 10px; height: 14px; margin: -6px 0;"
    " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #222227, stop:1 #141417);"
    " border: 1px solid #4a7ba8; border-top-color: #5e8db8; border-radius: 5px; }"
    "QSlider::handle:horizontal:hover { background: #222227; }";

constexpr auto kButtonStyle =
    // Erhaben wie buttonBaseStyle(), als Konstante: Verlauf, Licht-
    // kante oben, dunkle Kante unten; gedrueckt kippt der Verlauf.
    "QPushButton {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #29292f, stop:0.55 #1a1a1e, stop:1 #0f0f11);"
    "  border: 1px solid #2c2c31; border-top-color: #38383e; border-bottom-color: #1f1f23;"
    "  border-radius: 7px; color: #dcdce1; padding: 4px 12px; }"
    "QPushButton:hover {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #42454e, stop:0.55 #2f3138, stop:1 #26282e); }"
    "QPushButton:pressed {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0f0f11, stop:1 #29292f);"
    "  border-top-color: #1f1f23; border-bottom-color: #38383e; }"
    "QPushButton:disabled { color: #58585e; border-color: #232327;"
    "  background: #101013; }";   // flach: ein gesperrter Knopf hat keine Dicke

// Reiter (QTabWidget): die Platte darunter mit Lichtkante, die Reiter
// erhaben wie Knoepfe; der gewaehlte Reiter ist Teil der Platte —
// gleicher Grund, keine Unterkante, Text in Erstfarbe.
constexpr auto kTabStyle =
    "QTabWidget::pane { border: 1px solid #2c2c31; border-top-color: #38383e;"
    "  border-radius: 10px; background: #0e0e10; top: -1px; }"
    "QTabBar::tab {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #222227, stop:1 #141417);"
    "  color: #a6a6ac; border: 1px solid #2c2c31; border-top-color: #38383e; border-bottom: none;"
    "  border-top-left-radius: 7px; border-top-right-radius: 7px;"
    "  padding: 5px 14px; margin-right: 2px; }"
    "QTabBar::tab:hover { color: #dcdce1; }"
    "QTabBar::tab:selected { background: #0e0e10; color: #dcdce1; border-bottom: none;"
    "  margin-bottom: -1px; }";

// Bildlaufleisten: schmal, dunkel, ohne Pfeile — Qt/macOS malt sonst
// eine helle Systemleiste in jede Setup-Seite.
constexpr auto kScrollBarStyle =
    "QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }"
    "QScrollBar::handle:vertical { background: #2c2c31; border-radius: 4px; min-height: 24px; margin: 2px; }"
    "QScrollBar::handle:vertical:hover { background: #3a3a41; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: none; }"
    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    "QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }"
    "QScrollBar::handle:horizontal { background: #2c2c31; border-radius: 4px; min-width: 24px; margin: 2px; }"
    "QScrollBar::handle:horizontal:hover { background: #3a3a41; }"
    "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; border: none; background: none; }"
    "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }";

// Apply the canonical "dark page" stylesheet to a Setup page that lays
// itself out manually (i.e. doesn't inherit the SetupPage::addLabeledX
// helper-based widgets). Replaces the 4 byte-for-byte copies of
// applyDarkStyle() that previously lived in TransmitSetupPages,
// DisplaySetupPages, AppearanceSetupPages, GeneralOptionsPage.
//
// Per docs/architecture/ui-audit-polish-plan.md §A3.
//
// Two intentional drift corrections vs the local copies:
// - QGroupBox border uses kBorder (#205070), not the locals' #203040.
// - QGroupBox padding-top is 12 px, not the locals' 4 px (an 8 px
//   header-position shift, matching the rest of the app).
// All other rules match the locals byte-for-byte so callers see no
// other visual change.
//
// Issue #175 follow-up (2026-05-04): added QRadioButton rules.  Without
// these, calling applyDarkPageStyle() on a SetupPage subclass overwrote
// the QRadioButton::indicator style installed in SetupPage::init() via
// kRadioButtonStyle, leaving radio-button text + indicators in system
// default (black-on-black on dark theme).  Caught on PowerPage's new
// "Tune" group radios.  Mirrors kRadioButtonStyle (lines 241-245 above).
inline void applyDarkPageStyle(QWidget* w)
{
    if (!w) { return; }
    // Glas & Tiefe (2026-09-18): keine eigene Palette mehr, sondern die
    // Bausteine, die auch die Applets tragen — Auswahlfelder erhaben,
    // Haekchen versenkt, Felder als Glas, Rinnen versenkt, Knoepfe
    // erhaben. Eine Seite, die "dunkel" sagt, sagt damit dasselbe wie
    // das Hauptfenster.
    w->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; color: %2; }"
        "QLabel { color: %2; }")
        .arg(hexRole(kAppBg), hexRole(kTextPrimary))
        + QLatin1String(kGroupBoxStyle)
        + QLatin1String(kComboStyle)
        + QLatin1String(kCheckBoxStyle)
        + QLatin1String(kRadioButtonStyle)
        + formFieldStyle()
        + sliderHStyle()
        + sliderVStyle()
        + QLatin1String(kButtonStyle));
}

// ── TX / RX filter overlay palette ────────────────────────────────────────────
// Plan 4 D9 (Cluster E).  Used by SpectrumWidget::drawTxFilterOverlay() and
// SpectrumWidget::drawTxFilterWaterfallColumn().
// Colours are Longpath-original; no Thetis upstream equivalent (Thetis uses
// hard-coded GDI+ brushes without named constants).

// TX filter overlay — translucent orange.
// Customisable via ColorSwatchButton on Setup → Display → TX Display per
// docs/architecture/ui-audit-polish-plan.md §E1.D9b.
constexpr auto kTxFilterOverlayFill   = "rgba(255, 120, 60, 46)";
constexpr auto kTxFilterOverlayBorder = "#a8724a";
constexpr auto kTxFilterOverlayLabel  = "#c2924f";

// RX filter overlay — translucent cyan.
// Border reuses kAccent (#00b4d8); only the fill variant is new.
// Customisable on Setup → Display → Spectrum Defaults.
constexpr auto kRxFilterOverlayFill   = "rgba(0, 180, 216, 80)";


/// Eine Versalzeile: Groesse, Halbfett, Grossbuchstaben und die
/// Laufweite aus kCapsTracking — an einer Stelle, damit sie nicht je
/// Aufrufstelle neu gerechnet wird und dabei auseinanderlaeuft.
inline QFont capsFont(const QFont& base, int px = kFontCaption)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(QFont::DemiBold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::AbsoluteSpacing, px * kCapsTracking);
    return f;
}


/// Die Rueckfallkette auf eine bestehende Schrift legen.
///
/// Fuer die Stellen, die sich die Schrift selbst zusammenbauen und nur
/// die Familie setzen wollen. setFamily() nimmt EINE Familie: fehlt sie,
/// sucht Qt sich klaglos irgendeine -- und die muss nicht dicktengleich
/// sein. Am 2026-10-07 beim Gegenlesen an zehn Stellen gefunden, alle
/// mit setFamily("Menlo"), also genau der Schwaeche, die monoFont() und
/// monoFontPt() einen Tag vorher losgeworden waren.
inline void setzeMonoFamilien(QFont& f)
{
    f.setFamilies({QStringLiteral("Menlo"),
                   QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("monospace")});
    f.setStyleHint(QFont::TypeWriter);
    f.setFixedPitch(true);
}

/// Eine Zahl, die sich aendert: Monospace, damit Stellen untereinander
/// stehen. HAUSSTIL.md §Die acht Regeln, Regel 2.
///
/// Haelt dieselbe Familie wie LP_MONO_QSS. Wer eine aendert, aendert
/// beide -- tst_schriftfamilien faengt das Auseinanderlaufen.
inline QFont monoFont(const QFont& base, int px, QFont::Weight w = QFont::Normal)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(w);
    // setFamilies statt setFamily: die ganze Rueckfallkette, dieselbe wie
    // LP_MONO_QSS. Mit setFamily() stand hier nur "Menlo" -- auf einem
    // Rechner ohne Menlo nahm Qt die naechstbeste Schrift, und das war
    // nicht zwingend eine dicktengleiche.
    f.setFamilies({QStringLiteral("Menlo"),
                   QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("monospace")});
    // Menlo gibt es nur auf dem Mac. Ohne diesen Hinweis nahm Qt unter
    // Windows/Linux eine proportionale Schrift, und die Stellen tanzten
    // doch (2026-09-27).
    f.setStyleHint(QFont::TypeWriter);
    f.setFixedPitch(true);
    return f;
}

/// Dieselbe Entscheidung, aber fuer Aufrufstellen mit PUNKT-Groesse.
///
/// Vier Stellen bauten ihre Schrift selbst zusammen und nannten dabei je
/// EINEN Familiennamen, den es auf der laufenden Plattform nicht gibt:
/// `QFont("SF Mono", 10, DemiBold)` in TitleBar, `QFont("Consolas", 9)`
/// im SupportDialog (eine Windows-Schrift!), dazu zweimal die
/// CSS-Gattungsnamen "monospace" und "sans-serif", die Qt nicht als
/// Familie kennt. Qt sucht sich dann selbst etwas aus — und das ist
/// nicht zwangslaeufig dicktengleich, womit genau das zurueckkommt, was
/// die Schwesterfunktion oben 2026-09-27 abgestellt hat.
///
/// Belegt im Betriebslog vom 2026-10-03, bei jedem Start:
///
///   WRN: Populating font family aliases took 35 ms. Replace uses of
///        missing font family "SF Mono" with one that exists ...
///
/// Punktgroesse statt Pixelgroesse, weil die Aufrufstellen sie so
/// angeben: 10 pt und 10 px sind auf dem Mac dasselbe, unter Windows
/// (96 dpi) nicht — die Groesse umzurechnen waere eine Aenderung am
/// Bild, und die steht hier nicht an. Familie, Hinweis und fixedPitch
/// sind dieselben wie oben.
inline QFont monoFontPt(int pt, QFont::Weight w = QFont::Normal)
{
    QFont f;
    f.setPointSize(pt);
    f.setWeight(w);
    // setFamilies statt setFamily, dieselbe Kette wie monoFont() und
    // LP_MONO_QSS. Mit setFamily stand hier nur "Menlo" -- auf einem
    // Rechner ohne Menlo nahm Qt die naechstbeste Schrift, und die muss
    // keine dicktengleiche sein. Der Pruefstand
    // tst_schrift_dicktengleich misst auf DIESEM Mac, auf dem es Menlo
    // gibt; die Kette ist fuer die anderen.
    f.setFamilies({QStringLiteral("Menlo"),
                   QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("monospace")});
    f.setStyleHint(QFont::TypeWriter);
    f.setFixedPitch(true);
    return f;
}

// ── Hoehenprofil fuer UKW-QSOs (2026-11-11) ─────────────────────────
//
// Aus dem Entwurfsblatt, das der Betreiber gewaehlt hat
// (werkzeug/logbuch-2026-11-09/blaetter/5-hoehenprofil-2m.png, Farben in
// blaetter.py/profil.py). Grund und Bernstein sind schon benannt:
// kPanelBg (#0c0c0e) und kAmberText (#d8a55f).
constexpr auto kProfilGitter    = "#2a2a30";   // Gitterlinien
constexpr auto kProfilText      = "#e6e1d6";   // Sichtlinie, Urteil
constexpr auto kProfilTextLeise = "#8a8578";   // Achsen, Fusszeile
constexpr auto kProfilGelaende  = "#3d372d";   // Gelaendeflaeche
constexpr auto kProfilKante     = "#9a8b70";   // Gelaendekante

// ── Locator-Jagd mit FT8 (2026-11-11) ───────────────────────────────
//
// Mosaik und Live-Punkte auf flacher Karte und Azimutalkarte. Keine
// neue Farbe -- nur Rollen fuer schon benannte Toene, damit beide
// Karten dieselbe Sprache sprechen:
//   * bestaetigt  -- das Gruen der Hausfarben, gedaempft im Widget
//   * gearbeitet  -- warmer Sand (die Gelaendekante des Hoehenprofils)
//   * noch nie    -- keine Fuellung: die Karte selbst
//   * NEU         -- Bernstein (Hausregel: Bernstein fuer „neu"),
//                    ein neues Land zusaetzlich mit warmweissem Rand
//   * schon gearbeitet / gewoehnlich -- Grautoene
// Kein kraeftiges Rot, kein Auswahl-Blau.
constexpr auto kJagdFeldBestaetigt = kGreenText;
constexpr auto kJagdFeldGearbeitet = kProfilKante;
constexpr auto kJagdNeu            = kAmberText;
constexpr auto kJagdNeuesLandRand  = kAzZiel;
constexpr auto kJagdGearbeitet     = kTextScale;
constexpr auto kJagdGewoehnlich    = kTextSecondary;

} // namespace Longpath::Style
