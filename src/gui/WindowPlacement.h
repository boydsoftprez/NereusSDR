#pragma once

// =================================================================
// src/gui/WindowPlacement.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Ein Fenster auf einen sichtbaren Schirm holen ────────────────────
//
// Der Rumpf dieser Funktion stand seit 2026-04-17 als
// FloatingContainer::ensureVisiblePosition() im Baum und löste den Fall
// „der Monitor ist beim Start nicht mehr da" bereits vollständig. Er
// hing dort nur an einer Klasse fest, die ein Meter-Fenster ist — er
// fasst nichts von ihr an ausser zwei Grössenkonstanten.
//
// Am 2026-08-16 wurde er hierher gehoben, damit die Applet-Fenster
// dieselbe Lösung BENUTZEN, statt eine zweite halbe zu bekommen.
// Verhalten unverändert; FloatingContainer ruft jetzt hierher.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Ursprung als FloatingContainer::ensureVisiblePosition,
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   2026-08-16 — Als freie Funktion herausgezogen, Verhalten
//                 unverändert. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
//   2026-09-25 — coveredFraction(): eingeschaltete Applets nach vorne.
//                 Martin Fischer, AI-assisted via Anthropic Claude.
//   2026-09-27 — fitFrameIntoArea(): Werkzeugfenster nie groesser als
//                 der Schirm. Martin Fischer, AI-assisted via Anthropic
//                 Claude.
//   2026-10-21 — mitMindestabstand(): zwei Punkte Luft zwischen
//                 schwebenden Fenstern, aber nur beim Ziehen. Martin
//                 Fischer, AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/StyleConstants.h"
#include <functional>

#include <QList>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>

class QWidget;

namespace Longpath {

// ── Schwebende Fenster aufs Raster ────────────────────────────────────
//
// Betreiber 2026-09-02: "die schwebenden Fenster sollen zueinander in
// einer Linie sein". Alle vier schwebenden Fensterklassen (ToolWindow,
// PanFloatingWindow, AppletFloatingWindow, FloatingContainer) werden
// per NATIVEM macOS-Fenstergriff gezogen, nicht per eigenem Maus-Code
// -- ein direktes Runden in moveEvent() wuerde also gegen das
// Betriebssystem kaempfen (es zieht dorthin, wo die Maus ist, wir
// zoegen sofort zurueck: sichtbares Ruckeln unter dem Cursor). Der
// gedaempfte Weg schnappt erst, wenn die Bewegung kurz zur Ruhe
// gekommen ist -- ein Rueckstoss durch das Runden selbst loest genau
// eine weitere, dann bereits treffende Runde aus, kein Endlosdreh.
//
// Die Weite: 16 Punkte. Bis zum 2026-10-20 waren es 8, und das war zu
// fein -- zwei Fenster konnten um einen halben Schritt versetzt stehen
// und sahen trotzdem "eingerastet" aus. Betreiber am 2026-10-20, mit
// einem Bild von AetherSDR daneben: "aether hat einen groesseren
// raster, der auch sehr gut passt, bei uns gehoert dieser ein bisschen
// groesser."
//
// Reiner Punkt-Helfer, keine Fensteroperation selbst.
constexpr int kSnapGridPx = 16;
QPoint snappedTopLeft(const QPoint& pos, int grid = kSnapGridPx);

// ── Kanten statt Ecke (2026-09-26) ────────────────────────────────────
//
// Betreiber: "einen minimalen kleinen Raster ueber die ganze Oberflaeche,
// an das jedes Widget andockt ... alle auf gleicher Linie, Hoehe und
// Breite" -- und zum Bestand: "vorhanden, passt aber nicht zu 100 %".
// Gerastert wurde nur die linke obere Ecke; rechte und untere Kante
// lagen, wo die Groesse sie hinstellte. Jetzt rastet jede KANTE fuer
// sich (links, oben, rechts, unten). Getrennt gerundete Lage und Groesse
// wuerden zwei aneinanderliegende Fenster um bis zu ein Raster
// auseinanderreissen; eine gemeinsame Kante rundet so bei beiden gleich.
// Nie kleiner als minSize: dann wandert die rechte/untere Kante ein
// Raster weiter hinaus.
/// `abstand` ist die Luft, die das Raster selbst mitbringt: die nahe
/// Kante (links/oben) liegt auf der Rasterlinie, die ferne (rechts/
/// unten) um so viel davor. Zwei Fenster an derselben Linie haben damit
/// genau `abstand` Luft, und alle Kanten bleiben auf dem Raster — siehe
/// die Messung vom 2026-10-31 im Rumpf. `abstand = 0` gibt das alte
/// Verhalten (buendig).
QRect snappedFrameRect(const QRect& frame, const QSize& minSize = QSize(),
                       int grid = kSnapGridPx,
                       int abstand = Style::kFensterLuftPx);

// ── Zwei Punkte Luft (2026-10-15 / 2026-10-21) ───────────────────────
//
// Betreiber 2026-10-15: „der pixelabstand automatisch zu allen fenstern
// ist nicht da", und auf die Frage nach dem Mass: „ein minimaler
// automatischer Abstand, vielleicht 2 pixel. x und y."
//
// Das Raster allein gibt ihn nicht: es rundet jede Kante fuer sich, und
// zwei Fenster, deren gemeinsame Kante auf derselben Rasterlinie landet,
// stossen danach BUENDIG aneinander -- seit das Raster am 2026-10-20 auf
// 16 Punkte ging (#285), trifft das oefter als vorher.
//
// `wunsch` ist der schon gerasterte Rahmen des bewegten Fensters,
// `nachbarn` sind die Rahmen der anderen sichtbaren Fenster. Heraus
// kommt derselbe Rahmen, hoechstens um `abstand` verschoben (die Groesse
// bleibt). Zwei Regeln halten das klein:
//
//  * Nachbar ist nur, wer sich in der ANDEREN Achse ueberlappt -- ein
//    Fenster schraeg darueber steht nicht daneben.
//  * Korrigiert wird nur rund um die Beruehrung: ein Spalt kleiner als
//    `abstand`, aber hoechstens `maxUeberlappung` tief uebereinander.
//    Was der Bediener absichtlich ueber ein anderes Fenster legt -- oder
//    was im Hauptfenster liegt, das hier als Nachbar mitzaehlt --
//    bleibt, wo es ist.
//
// `maxUeberlappung` ist eine HALBE Rasterzelle, und die Zahl ist an
// Martins eigener Anordnung gemessen (Einstellungsdatei vom
// 2026-10-21, Profil „Neu"): darin liegen „Asr" (x=576, Breite 272,
// also bis 847) und „Frequency" (x=832) um genau 16 Punkte
// uebereinander -- eine ganze Rasterzelle. Eine Schwelle von 16 haette
// dieses Paar beim ersten Zug um 18 Punkte auseinandergerissen. Was
// tiefer als eine halbe Zelle uebereinander liegt, ist gewollt; die
// kleinen Reste darunter (2 Punkte, wenn ein Nachbar schon einmal
// aufgeruckt wurde) sind es nicht.
//
// Bewusste Schwaeche: bei mehreren Nachbarn entscheidet der letzte in
// der Liste, der ueberhaupt zutrifft. Zwischen zwei Fenstern mit weniger
// als zwei Punkten Luft kann das Ergebnis also am einen kleben. Das
// kostet beim Ziehen nichts (der naechste Zug korrigiert) und ein
// Ausgleich ueber alle Nachbarn waere eine Platzsuche, nicht mehr ein
// Abstand.
//
// Reiner Rechteck-Helfer, wie ausSperrstreifenSchieben() und aus
// demselben Grund: am Schreibtisch vollstaendig pruefbar.
QRect mitMindestabstand(const QRect& wunsch, const QList<QRect>& nachbarn,
                        int abstand,
                        int maxUeberlappung = kSnapGridPx / 2);

// Der Bezugspunkt fuer die Lage eines schwebenden Fensters im Profil.
//
// Gemessen 2026-11-07: die Fenster wanderten von Lauf zu Lauf um genau
// 16 Punkte nach unten (Bandbreitenfilter 160, 176, 192). Auf macOS steht
// ein frisch maximiertes Hauptfenster noch kurz an seinem alten Platz
// (Geraetelog: y=48 statt 33), waehrend die Fenster schon gesetzt werden.
// Alle landen damit 15 Punkte zu tief, das Raster macht 16 daraus, und
// beim Beenden wird das als neue Lage gespeichert.
//
// Ein maximiertes Fenster steht aber fest in der linken oberen Ecke der
// verfuegbaren Flaeche, auch solange es noch unterwegs ist. Das ist der
// Bezugspunkt, nicht der noch veraltete Rahmen.
QPoint bezugspunktFuerProfil(Qt::WindowStates zustand, const QRect& rahmen,
                             const QRect& verfuegbar);

// Steht das Hauptfenster schon dort, wo es hingehoert? Solange nicht,
// liegen auch die Bedienleisten noch am falschen Platz, und der
// Schutzrahmen schoebe Fenster, die gar nicht im Weg sind.
bool hauptrahmenSteht(Qt::WindowStates zustand, const QRect& rahmen,
                      const QRect& verfuegbar);

// Von moveEvent() jeder schwebenden Fensterklasse mit einer Zeile
// aufzurufen. Legt bei Bedarf einen einmaligen, an `w` gebundenen
// Debounce-Timer an (kein Member in der aufrufenden Klasse noetig) und
// startet ihn neu; erst wenn er ablaeuft, ohne dass diese Funktion
// zwischenzeitlich erneut aufgerufen wurde, schnappt er `w` aufs
// Raster -- also nach dem Loslassen, nicht waehrend des Ziehens.
//
// Seit 2026-09-26 mit Groesse: der Fensterrahmen (frameGeometry) rastet
// an allen vier Kanten ein, siehe snappedFrameRect(). snapSize = false
// rastet nur die linke obere Ecke (fuer Fenster, deren Breite ein
// anderer festlegt). Maximierte, Vollbild- und minimierte Fenster
// bleiben unberuehrt.
void snapToGridAfterSettle(QWidget* w, int grid = kSnapGridPx,
                           int delayMs = 180, bool snapSize = true);

/// Ist der Sprung von `vorher` nach `jetzt` nur das Nachrasten des
/// Rasters (snapToGridAfterSettle), keine Bewegung fuer sich?
///
/// Lage: unter einer Rasterweite je Achse -- eine Kante rueckt hoechstens
/// um eine halbe Weite, die obere unter der Menueleiste um knapp eine
/// ganze (y = 33 darf nicht auf 32, also auf 48).
/// Groesse: unter ANDERTHALB Rasterweiten -- beide Kanten rasten ein,
/// die obere unter der Menueleiste um bis zu 15, die untere um bis zu 8,
/// zusammen hoechstens 23. Gerade beim Hauptfenster addiert sich das: am
/// Geraet am 2026-11-11 gemessen y 33 -> 48 (+15) und Hoehe 738 -> 717
/// (-21). Mit der alten Schwelle (eine Weite auch fuer die Groesse)
/// galt das als eigene Bewegung, und die schwebenden Fenster blieben
/// bei einem schnellen Groessenzug stehen.
bool istRasterNachhall(const QRect& jetzt, const QRect& vorher, int grid = kSnapGridPx);

// Ragt ein (maximiertes) Hauptfenster ueber den freien Platz des Schirms
// hinaus -- mehr als das Raster? Dann war es mit einem anderen freien
// Platz maximiert (Dock damals weg) und muss neu maximiert werden
// (2026-11-13, Protokoll 12.11.: 1470x923 bei freien 1470x867).
bool ragtUeberDenFreienPlatz(const QRect& rahmen, const QRect& frei, int grid = kSnapGridPx);

/// Dafür sorgen, dass `w` auf einem verbundenen Bildschirm zu sehen
/// ist. Verschoben wird, wenn (a) keine Geometrie gespeichert war,
/// (b) die gespeicherte bei (0,0) liegt, (c) sie ausserhalb jedes
/// verbundenen Schirms liegt, oder (d) sie das `anchor`-Fenster in
/// seiner HEUTIGEN Grösse/Lage um weniger als 80×40 Bildpunkte
/// ueberlappt -- LETZTERES NUR, wenn `anchor` bereits sichtbar ist
/// (Betreiber 2026-08-31: "S-Meter usw. liegen frei am Desktop" --
/// eine Position, gespeichert aus einer breiteren/Vollbild-Sitzung,
/// blieb bislang auch dann unangetastet, wenn das Hauptfenster
/// seither viel kleiner geworden ist; ein erster, grosszuegigerer
/// Anlauf ["beruehrt sich ueberhaupt"] liess noch ein Fenster durch,
/// das nur mit einem schmalen Rand an der Hauptfenster-Kante
/// haengenblieb. Zweiter Fund, selber Abend, "vor allem rotor,
/// bandfilter und panadapter sind immer anders als abgespeichert":
/// Rotor/Bandfilter/Panadapter werden alle waehrend MainWindows
/// eigenem Konstruktor wiederhergestellt, lange vor dem ersten
/// show() -- zu diesem Zeitpunkt hat `anchor` seine Vollbild-/
/// Maximiert-Groesse noch nie erreicht, jede Ueberlapp-Pruefung
/// dagegen verwarf die gespeicherte Position also bei JEDEM Start,
/// nicht nur manchmal). `anchor` gibt den Zielschirm vor (in der
/// Regel das Hauptfenster); das Fenster wird auf dessen Schirm
/// zentriert. Liegt die Geometrie schon richtig, geschieht nichts.
///
/// ── Zwei bekannte Schwächen, bewusst stehen gelassen ────────────────
///
/// Beide stammen aus dem Ursprung. Eine Verhaltensänderung an dieser
/// Stelle betrifft JEDES freistehende Fenster — wer eine angeht, tut
/// es als eigenen Schritt, mit Gegenprobe. Die zweite ist am
/// 2026-10-30 so angegangen worden; die erste steht weiter offen:
///
///  1. `(0,0)` wird als „keine Geometrie" gelesen. Das ist ein gültiger
///     Wert, der als Sonderfall missbraucht wird: auf einem Aufbau mit
///     einem Schirm links oder über dem Hauptschirm ist (0,0) eine
///     ganz normale Position, und ein Fenster, das dorthin gehört,
///     wird bei JEDEM Start zur Mitte gezogen. Richtig wäre ein
///     eigenes Feld „Geometrie vorhanden ja/nein" im gespeicherten
///     Satz, statt einen Wert doppelt zu belegen.
///
///  2. ~~Der Sichtbarkeitstest ist `intersects()`~~ — **behoben am
///     2026-10-30.** Ein Bildpunkt Überlappung zählte als sichtbar,
///     ein Fenster, das zu 99 % über die Kante hing, galt als in
///     Ordnung. Am Gerät gesehen: das FT8-Panel stand links über der
///     Schirmkante, der Titel war auf „T8 DECODER" abgeschnitten.
///     Jetzt gilt die Mindestfläche, die hier schon vorgeschlagen
///     war — 80 × 40 px, dieselben zwei Zahlen, die `nearAnchor` im
///     selben Unterprogramm längst benutzte. Am Fenster gedeckelt:
///     ein Fenster, das selbst schmaler ist, könnte die Bedingung
///     sonst nie erfüllen.
void ensureOnVisibleScreen(QWidget* w, QWidget* anchor, QSize minSize);

// ── Auf den Schirm, an dem gearbeitet wird (2026-11-15) ──────────────
//
// Betreiber, mit drei Bildschirmen: „wichtig! ich arbeite gerade mit 2
// bildschirmen. wenn ich das logbuch aufmache mit dem plus sehe ich es
// nur am macbook, nicht aber auf den beiden bildschirmen! es muss immer
// das neu hinzugefuegte im vordergrund sein!!!!"
//
// Gemessen am 15.11.: seine drei Schirme stehen nebeneinander --
// LEN D32qc-20 (2) bei x 0…2560, das eingebaute Display bei
// x 2560…4270, LEN D32qc-20 (1) bei x −2560…0. Die gespeicherte Lage
// des Logbuchfensters lautet x 2561…4268: genau das MacBook. Er hat
// das Fenster irgendwann einmal dort abgelegt, arbeitet seither an den
// grossen Schirmen -- und das Logbuch geht weiter dort auf, wo er
// nicht hinsieht. Ein Fenster, das auf einem Schirm aufgeht, den man
// nicht ansieht, ist nicht offen.
//
// `ensureOnVisibleScreen()` koennte das nicht: ein Fenster auf einem
// vorhandenen Schirm gilt ihm als in Ordnung, solange es das
// Hauptfenster genug ueberlappt -- und das ist genau die Pruefung, die
// hier nicht greift (das Logbuch ueberlappt nichts, es steht allein auf
// seinem Schirm). Darum eine eigene, engere Regel:
//
//   Liegt das Fenster auf einem ANDEREN Schirm als der Anker, kommt es
//   auf dessen Schirm -- an dieselbe Stelle im Verhaeltnis zur freien
//   Flaeche. Liegt es schon dort, geschieht NICHTS: was der Betreiber
//   auf seinem Arbeitsschirm hingestellt hat, bleibt stehen.
//
// Absichtlich in zwei Teilen. Das Rechnen ist eine reine
// Rechteckfunktion und am Schreibtisch vollstaendig pruefbar; nur das
// Nachsehen, welcher Schirm welcher ist, braucht ein Fenster.

/// `rahmen` von der freien Flaeche `vonFlaeche` auf `nachFlaeche`
/// uebertragen: derselbe Anteil des freien Platzes links und oben, die
/// Groesse nur verkleinert, wenn sie nicht hineinpasst. Die linke Kante
/// bleibt links, die rechte rechts, die Mitte in der Mitte.
///
/// Ungueltige Rechtecke kommen unveraendert zurueck.
QRect uebertrageAufFlaeche(const QRect& rahmen, const QRect& vonFlaeche,
                           const QRect& nachFlaeche);

/// Welche der `flaechen` traegt `rahmen` am meisten? -1, wenn keine ihn
/// beruehrt.
///
/// Die zweite reine Haelfte der Regel: WELCHER Schirm ist es. Damit ist
/// der gemessene Fall des Betreibers am Schreibtisch pruefbar -- drei
/// Schirme, ein Fenster bei x 2561, und die Antwort muss das eingebaute
/// Display sein, nicht der Schirm, auf dem er arbeitet.
int flaecheMitDerGroesstenUeberlappung(const QRect& rahmen,
                                       const QList<QRect>& flaechen);

/// `w` auf den Schirm von `anker` holen, wenn es gerade auf einem
/// anderen liegt. Liefert true, wenn verschoben wurde.
///
/// Der Schirm eines Fensters wird an seiner GEOMETRIE bestimmt (groesste
/// Ueberlappung), nicht an `QWidget::screen()`: ein frisch erzeugtes,
/// noch nie gezeigtes Fenster meldet dort den Hauptschirm, egal wohin
/// seine wiederhergestellte Lage zeigt.
bool aufDenArbeitsschirm(QWidget* w, const QWidget* anker);

// ── Liegt etwas darueber? ────────────────────────────────────────────
//
// Betreiber 2026-09-25: "wenn ich rx einblende ist es versteckt ... ich
// muss es dann hinter meinen anderen Fenstern suchen". Ein Applet, das
// in die Spalte des Hauptfensters kommt, liegt unter jedem schwebenden
// Werkzeugfenster (Qt::Tool = NSPanel auf der Palettenebene, immer ueber
// normalen Fenstern) -- und bei ihm stehen Panadapter, TX, Rotor/Log
// genau ueber dieser Spalte.
//
// Anteil (0..1) von `target`, den die Rechtecke in `covers` zusammen
// decken. Ueberlappungen untereinander werden nicht doppelt gezaehlt:
// gerechnet wird auf einem 16x16-Punkteraster ueber `target`, genau
// genug fuer "liegt es zu einem guten Teil unter etwas". Leeres
// `target` gilt als ganz verdeckt (es ist nirgends zu sehen).
double coveredFraction(const QRect& target, const QList<QRect>& covers);

// ── Nicht groesser als der Schirm ────────────────────────────────────
//
// Betreiber 2026-09-27, Bild vom MacBook Air: der Kanalzug stand
// 980 x 1000 auf 849 px nutzbarer Hoehe -- sein unterer Teil lag hinter
// dem Dock, und er liess sich nicht kleiner ziehen. Ein Fenster, das
// aufgeht, soll ganz auf die freie Flaeche passen.
//
// Der Rahmen `frame` (mit Titelleiste) so klein wie noetig, damit er in
// `area` (availableGeometry()) passt -- aber nicht kleiner als
// `minFrame` --, und dann so wenig wie moeglich verschoben, bis er ganz
// darin liegt. Passt er schon, kommt er unveraendert zurueck. Ist
// `minFrame` selbst groesser als `area`, gewinnt die Mindestgroesse und
// die linke obere Ecke liegt auf der von `area` (Titelleiste bleibt
// greifbar).
QRect fitFrameIntoArea(const QRect& frame, const QRect& area,
                       const QSize& minFrame = QSize());

/// fitFrameIntoArea() am Fenster `w`, auf der freien Flaeche seines
/// Schirms. Erst nach show() aufrufen: vorher kennt macOS die
/// Titelleiste nicht, und frameGeometry() waere zu klein.
void fitIntoAvailableScreen(QWidget* w);

// ── Der Schutzrahmen (2026-10-14) ────────────────────────────────────
//
// Der Betreiber: „ich muss immer auf Vollformat klicken, um alle Fenster
// und Icons zu sehen" — und gleich darauf der Vorschlag, der die Loesung
// ist: „es sollte vielleicht einen grossen Rahmen geben, ueber den man
// nichts ziehen kann und keine Fenster anordnen kann."
//
// Der Befund dahinter, in seiner Einstellungsdatei nachgelesen: beide
// Layoutprofile tragen `"mainWindow":{"fullScreen":true}`, und die
// schwebenden Felder stehen mit ABSOLUTEN Bildschirmkoordinaten darin
// (BwFilter y=128, pan-0 y=304, Rotor x=1184 h=704). Im Vollbild beginnt
// das Hauptfenster oben; ohne Vollbild rutscht es um die Titelleiste
// nach unten — die schwebenden Fenster bleiben stehen und liegen dann
// ueber der Knopfreihe der Kommandoleiste. Die Ueberschriften (BAND,
// MODE, FILTER …) schauen noch heraus, die Knoepfe darunter sind
// verdeckt.
//
// Geprueft wurde bis dahin nur gegen den BILDSCHIRM
// (`availableGeometry()`, MainWindow.cpp:1935) — nie gegen das
// Hauptfenster. Ein schwebendes Fenster durfte also jederzeit auf der
// eigenen Bedienleiste liegen.
//
// Diese Funktion ist das Gegenstueck: gesperrte Streifen hinein, ein
// Rechteck heraus, das keinen davon mehr beruehrt. Absichtlich eine
// FREIE Funktion ueber uebergebenen Rechtecken und nicht eine Methode an
// MainWindow: so ist sie am Schreibtisch vollstaendig pruefbar (ein
// `MainWindow w;` startet echte UDP-Suche im Netz).
//
// Verschoben wird nur SENKRECHT. Die gesperrten Streifen laufen ueber
// die ganze Fensterbreite — waagerecht gibt es nichts auszuweichen, und
// ein seitliches Ausweichen wuerde eine Anordnung zerreissen, die der
// Betreiber nebeneinander gestellt hat.
//
// `platz` ist die Flaeche, in der das Ergebnis liegen muss (die freie
// Bildschirmflaeche). Passt ein Fenster nirgends mehr hin — zu hoch fuer
// den Rest —, kommt es UNVERAENDERT zurueck: lieber verdeckt als
// halb vom Schirm geschoben.
QRect ausSperrstreifenSchieben(const QRect& wunsch,
                               const QList<QRect>& sperren,
                               const QRect& platz);

// ── Jeder Mausdruck im Programm (2026-11-07) ─────────────────────────
//
// Auf der Anwendung, weil ein Druck an das Kind geht, das darunter liegt.
// Teuer ist das nicht: alles ausser dem Druck faellt in der ersten Zeile
// durch. Liefert `melder` wahr, wird der Druck geschluckt.
class KlickHoltNachVorne : public QObject {
public:
    KlickHoltNachVorne(std::function<bool(QWidget*)> melder, QObject* eltern);
protected:
    bool eventFilter(QObject* ziel, QEvent* ereignis) override;
private:
    std::function<bool(QWidget*)> m_melder;
};

// ── Ein freier Platz (2026-11-07) ────────────────────────────────────
//
// Betreiber zum CAT-Feld: „wieder ein window, welches ich nicht oeffnen
// kann." Ein Fenster, das von selbst aufgeht, legt sich nicht ueber die
// Anordnung, wenn daneben Platz ist.
//
// Die erste Lage von `groesse` in `flaeche`, die keines der Rechtecke in
// `belegt` schneidet, zeilenweise von links oben auf dem Raster `schritt`.
// Gibt es keine, kommt ein leeres Rechteck zurueck.
QRect freierPlatz(const QSize& groesse, const QRect& flaeche,
                  const QList<QRect>& belegt, int schritt = 16);

// ── „Ich werde gerade gezogen" (2026-10-14) ──────────────────────────
//
// Der Betreiber zum Schutzrahmen: „es sollte vielleicht einen grossen
// Rahmen geben, ueber den man nichts ziehen kann." Ein Rahmen, den man
// erst merkt, wenn das Fenster zurueckspringt, ist eine Ueberraschung;
// einer, den man beim Ziehen SIEHT, ist eine Regel.
//
// Diese zwei Funktionen sind die Bruecke, und absichtlich eine Bruecke
// und kein Ereignisfilter: ein `qApp->installEventFilter` saehe JEDES
// Ereignis des Programms (Zeichnen, Maus, Zeitgeber) -- in einem
// Programm mit Echtzeit-Tonweg ist das kein Preis, den man fuer einen
// Hinweis zahlt. Stattdessen melden sich die drei schwebenden
// Fensterarten selbst, aus ihrem `moveEvent`, wo sie ohnehin schon
// `snapToGridAfterSettle()` rufen.
//
// WindowPlacement kennt MainWindow nicht und soll es nicht kennen --
// darum ein eingehaengter Melder statt eines Aufrufs.
void setzeZugmelder(std::function<void(QWidget*)> melder);

/// Aus dem `moveEvent` eines schwebenden Fensters. Tut nichts, solange
/// niemand einen Melder eingehaengt hat (Prueffaelle, Werkzeugfenster
/// ohne Hauptfenster), nichts, solange eine `EigeneBewegung` lebt, und
/// nichts fuer ein Fenster, das nicht sichtbar ist.
void meldeZug(QWidget* bewegtes);

/// Solange eines dieser Objekte lebt, zaehlt eine Fensterbewegung NICHT
/// als Ziehen.
///
/// Gefunden am 2026-10-20: der Hinweis auf den Schutzrahmen stand schon
/// da, bevor jemand zog. Nicht das Wiederherstellen des Profils war es,
/// sondern Longpaths EIGENES Raster -- `snapToGridAfterSettle()` schiebt
/// das Fenster nach 180 ms auf die naechste Rasterlinie, das `move()`
/// loest ein `moveEvent` aus, und von dort aus sieht `meldeZug()` eine
/// zweite, veraenderte Lage: genau das Muster, an dem ein Ziehen erkannt
/// wird. Der Bediener hatte die Maus nie angefasst.
///
/// Darum die Unterscheidung an der Quelle statt an der Zeit: wer selbst
/// schiebt, sagt es. Das Raster hier, das Herausholen aus dem
/// Sperrstreifen in MainWindow -- und jede weitere Stelle, die ein
/// schwebendes Fenster von sich aus bewegt.
class EigeneBewegung {
public:
    EigeneBewegung();
    ~EigeneBewegung();
    EigeneBewegung(const EigeneBewegung&) = delete;
    EigeneBewegung& operator=(const EigeneBewegung&) = delete;
};

/// Lebt gerade eine `EigeneBewegung`? Fuer Stellen, die auf eine
/// Bewegung des HAUPTFENSTERS reagieren (die schwebenden Fenster ziehen
/// mit, MainWindow::schwebendeMitziehen) und dabei das Platzieren durch
/// ein Profil vom Zug des Betreibers unterscheiden muessen.
bool eigeneBewegungLebt();

} // namespace Longpath
