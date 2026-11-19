#pragma once
// =================================================================
// src/gui/applets/Ft8PlaetzeApplet.h  (Longpath)
// =================================================================
//
// Was auf den Plaetzen laeuft — Multi Answering zum Zusehen.
//
// ── Warum es das geben muss ─────────────────────────────────────────
//
// `Ft8Mehrfach::belegung()` liefert je Platz Partner, Zustand und
// Tonlage. Am 2026-10-30 nachgesehen: NIEMAND liest es. Mit
// `slots 3` gehen drei Signale gleichzeitig hinaus, und der Betreiber
// sieht keines davon. Dieselbe Art Luecke wie Multi Answering selbst,
// das Monate lang gebaut und unerreichbar dalag.
//
// Betreiber: „ich will sehen, sobald ein qso oder mehrere vielleicht
// seitlich im eigenen fenster zu sehen sind." Aus drei
// Entwurfsblaettern (werkzeug/ft8-plaetze-2026-10-30) hat er A
// gewaehlt: eine Kachel je Platz, darunter die Warteschlange.
//
// ── Warum ein eigenes Applet ────────────────────────────────────────
//
// Im FT8-Fenster ist kein Platz: die Arbeitsleiste unten traegt genau
// eine Zeile, und sechs davon wuerden die Fundliste erdruecken. Als
// eigenes Panel laesst es sich danebenstellen, in der Hoehe ziehen und
// bei `slots 1` einfach zumachen.
//
// ── Was hier NICHT passiert ─────────────────────────────────────────
//
// Es fuehrt keinen Ablauf, kennt kein Protokoll und uebersetzt keine
// Zustaende. Es bekommt einen fertigen `Ft8PlaetzeStand` und malt ihn.
// Zwei Fenster, die beide in `Ft8Mehrfach` hineingreifen, haetten
// jedes seine eigene Meinung darueber, welcher Zug gerade dran ist.
//
// ── Die Anordnung folgt der Groesse (2026-11-18) ───────────────────
//
// Betreiber, am Morgen, an dem zwei Stationen zum ersten Mal
// gleichzeitig hinausgingen: „FT8 SLOTS sollte man auch nebeneinander
// anordnen können und soll sich automatsich an die Größe des Fenster
// anpassen!"
//
// Bis dahin standen die Kacheln immer untereinander. Zog er das
// Fenster breit und niedrig, blieb rechts alles leer und die dritte
// Kachel verschwand unten -- ohne Rollbalken, weil die Mindesthoehe
// des Applets fest auf 150 stand und die der Flaeche darin unterging.
//
// Jetzt ist die Anordnung eine reine Rechnung aus Stand, Breite und
// Hoehe (`Ft8PlaetzeAnordnung`): so viele Spalten, wie Kacheln in
// ihrer Mindestbreite nebeneinander passen. Kein Schalter. Die
// Kacheln selbst -- Inhalt, Schrift, Farben -- sind dieselben.
//
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-11-18 — Die Kacheln ordnen sich nach der Groesse des Panels
//                 (nebeneinander / Raster / untereinander). Martin
//                 Fischer (OE5SOS), KI-gestuetzt ueber Anthropic
//                 Claude (Claude Code).
//   2026-11-19 — Drei Dichten (gross / kompakt / Zeile), die nach dem
//                 Platz von selbst wechseln. Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include "AppletWidget.h"
#include "Ft8PlatzStand.h"

#include <QPointer>
#include <QRect>
#include <QVector>

class QLabel;
class QScrollArea;

namespace Longpath {

/// Wo was steht. Reine Daten, aus Stand, Breite und Hoehe gerechnet
/// -- die Malflaeche malt danach, und ein Prueflauf liest dieselben
/// Rechtecke, statt Bildpunkte zu zaehlen.
struct Ft8PlaetzeAnordnung {
    /// Wie gross die Karten sind (2026-11-19). Von gross nach klein --
    /// die Reihenfolge wird verglichen.
    enum class Dichte {
        Gross,    ///< die Karte von immer, 92 Punkt
        Kompakt,  ///< zwei Zeilen, 46 Punkt
        Zeile     ///< eine Zeile, 24 Punkt
    };
    Dichte dichte{Dichte::Gross};
    /// Spalten des Platz-Rasters.
    int spalten{1};
    /// Je Kachel ein Rechteck, in der Reihenfolge von
    /// `Ft8PlaetzeStand::plaetze`. IMMER vollzaehlig.
    QVector<QRect> kacheln;
    /// Die Streifen der freien Plaetze, die Platz gefunden haben. Kann
    /// kuerzer sein als die Zahl der freien Plaetze -- sie weichen
    /// zuerst. Der n-te Eintrag gehoert zum Platz `kacheln.size()+n+1`.
    QVector<QRect> freie;
    /// Wie die freien Plaetze dastehen.
    enum class Freie {
        Voll,     ///< jeder in seiner Zelle des Platz-Rasters
        Dicht,    ///< reine Frei-Zeilen enger gepackt, mehrere je Zeile
        Gewichen  ///< reine Frei-Zeilen sind weg (die Kopfzeile zaehlt)
    };
    Freie freieArt{Freie::Voll};
    /// Die Warteschlange: Oberkante der Trennlinie (-1: keine) und je
    /// Wartendem die Zeile.
    int wartendeY{-1};
    QVector<QRect> wartende;
    /// Was diese Anordnung an Hoehe braucht: bis unter das Letzte,
    /// das gemalt wird, und eine Handbreit darunter.
    int hoehe{0};
};

/// Die Malfläche. Getrennt vom Applet, damit ein Prueflauf sie ohne
/// RadioModel aufziehen und abgreifen kann.
class Ft8PlaetzeFlaeche : public QWidget
{
    Q_OBJECT
public:
    explicit Ft8PlaetzeFlaeche(QWidget* parent = nullptr);

    void setzeStand(const Ft8PlaetzeStand& s);
    const Ft8PlaetzeStand& stand() const { return m_stand; }

    /// Die Hoehe, die der aktuelle Stand in der Breite des
    /// Groessenhinweises braucht (eine Spalte, alle freien Plaetze).
    /// Das Panel waechst mit der Zahl der Plaetze statt eine feste
    /// Hoehe zu behaupten.
    int noetigeHoehe() const;

    QSize sizeHint() const override;

    // ── Die Anordnung (2026-11-18) ──────────────────────────────
    //
    // Eine Rechnung, drei Fragen: was gemalt wird (`anordnung()`),
    // was mindestens Platz haben MUSS (`pflichtHoehe`: Kopf, Kacheln,
    // Wartende -- darunter rollt der Gastgeber) und was das Panel
    // gern haette (`wunschHoehe`: dazu jeder freie Platz).

    using Dichte = Ft8PlaetzeAnordnung::Dichte;

    /// Fuer eine gegebene Groesse -- die reine Regel, ohne
    /// Vorgeschichte.
    Ft8PlaetzeAnordnung anordnungFuer(int breite, int hoehe) const;
    /// Dasselbe mit der Dichte, die gerade gilt: eine GROESSERE wird
    /// erst genommen, wenn sie mit `kHysterese` Punkt Luft passt.
    Ft8PlaetzeAnordnung anordnungFuer(int breite, int hoehe,
                                      Dichte bisher) const;
    /// Fuer die Groesse, die die Flaeche gerade hat.
    Ft8PlaetzeAnordnung anordnung() const
    { return anordnungFuer(width(), height(), m_dichte); }
    /// Die Dichte, die gerade gilt.
    Dichte dichte() const { return m_dichte; }

    // ── Die Masze der kleinen Dichten (2026-11-19) ──────────────
    // Aus den Blaettern A und B,
    // werkzeug/entwuerfe-ft8-slots-klein-2026-11-19.
    static constexpr int kKompaktH    = 46;
    static constexpr int kKompaktLuft = 5;
    static constexpr int kZeileH      = 24;
    static constexpr int kZeileLuft   = 4;
    /// Ein freier Platz in den kleinen Dichten: flacher als die 30
    /// der grossen.
    static constexpr int kFreiFlachH  = 18;
    /// Wie viel Luft eine groessere Dichte braucht, bevor sie die
    /// kleinere abloest.
    static constexpr int kHysterese   = 6;

    static int kachelHoehe(Dichte d);
    static int freiHoehe(Dichte d);
    static int luftFuer(Dichte d);
    /// Unter dieser Breite wuerde in der Dichte `d` ein Rufzeichen
    /// beschnitten -- sie kommt dann nicht in Frage.
    int hartBreite(Dichte d) const;
    /// Die bequeme Breite einer Karte: so breit, dass auch das Land
    /// steht. Danach richtet sich die Spaltenzahl zuerst.
    int basisBreite(Dichte d) const;

    int pflichtHoehe(int breite) const;
    int wunschHoehe(int breite) const;

    /// Die Mindestbreite EINER Kachel: aus den Rechtecken, in die
    /// `maleKachel` schreibt, und der Breite dessen, was hineinsoll.
    /// Nie kleiner als das, was ein gewoehnliches Rufzeichen und ein
    /// langer Landesname brauchen -- sonst sprange das Raster mit
    /// jedem neuen Partner um.
    int kachelMindestbreite() const;
    /// Die Breite, unter der in JEDER Dichte etwas beschnitten wuerde
    /// -- erst dann rollt das Fenster waagrecht.
    int mindestBreite() const
    {
        return qMin(hartBreite(Dichte::Kompakt), hartBreite(Dichte::Zeile))
               + 2 * kRandX;
    }

    /// Die Breite, fuer die `sizeHint()` und `noetigeHoehe()` rechnen.
    static constexpr int kHinweisBreite = 320;
    static constexpr int kKopfH      = 22;
    /// Unter dem Letzten, das gemalt wird. Knapp: nach dieser Hoehe
    /// entscheidet sich, ob der Gastgeber rollt.
    static constexpr int kFussH      = 4;

    // ── Die Masze aus dem gewaehlten Blatt ─────────────────────
    static constexpr int kKachelH   = 92;
    static constexpr int kKachelLuft = 8;
    static constexpr int kRandX     = 10;
    static constexpr int kStufen    = 5;
    /// Ein FREIER Platz, als Streifen (Entwurf B, 2026-11-13).
    ///
    /// Betreiber mit `slots 3` vor einem Fenster, das „1 of 3" und
    /// eine einzige Kachel zeigte: „schön wäre es, wenn man dann auch
    /// alle 3 sieht, sprich 3 fenster und ein fenster durch 3
    /// geteilt." Aus drei Blaettern waehlte er B: belegte Plaetze
    /// bleiben die volle Kachel, freie kosten nur eine Zeile -- so
    /// sind alle zu sehen, ohne dass das Fenster bei `slots 6` zur
    /// Haelfte aus Luft besteht.
    static constexpr int kFreiH     = 30;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    /// Gibt die Hoehe zurueck, die sie gebraucht hat.
    int  maleKopfzeile(QPainter& p);
    void maleKachel(QPainter& p, const QRect& r, int nummer,
                    const Ft8PlatzStand& z);
    /// Die Karte in zwei Zeilen (Blatt A, 2026-11-19).
    void maleKompakt(QPainter& p, const QRect& r, int nummer,
                     const Ft8PlatzStand& z);
    /// Die Karte in einer Zeile (Blatt B, 2026-11-19).
    void maleZeile(QPainter& p, const QRect& r, int nummer,
                   const Ft8PlatzStand& z);
    /// Ein freier Platz als Streifen (Entwurf B).
    void maleFreien(QPainter& p, const QRect& r, int nummer);
    void maleWartende(QPainter& p, const Ft8PlaetzeAnordnung& a);

    /// Wie breit ein Wartender mindestens stehen muss, damit sein
    /// Land neben Rufzeichen und Rapport noch Platz hat.
    int warteMindestbreite() const;
    /// Wie schmal ein freier Streifen werden darf.
    int freiMindestbreite() const;

    /// Die eine Rechnung hinter `anordnungFuer`, `pflichtHoehe`,
    /// `wunschHoehe` und `noetigeHoehe`: die Rechtecke fuer EINE Dichte
    /// und EINE Spaltenzahl. Welche gilt, waehlt `anordnungFuer`.
    /// `alleEinspaltig` haelt auch Wartende und freie Streifen in einer
    /// Spalte (der Groessenhinweis).
    Ft8PlaetzeAnordnung rechne(int breite, int hoehe, Dichte dichte,
                               int spalten, bool alleEinspaltig) const;
    /// Die Dichte fuer die Groesse nachfuehren, die die Flaeche hat.
    void merkeDichte();
    /// Wie breit das breiteste Rufzeichen in der Dichte `d` steht.
    int rufBreite(Dichte d) const;
    /// Wie breit das laengste Land steht.
    int landBreite() const;
    /// „sent -05   rcvd +12" -- gemalt und gemessen aus EINER Quelle.
    static QString rapportZeile(const Ft8PlatzStand& z);

    Ft8PlaetzeStand m_stand;
    /// Die Dichte, die gerade gilt -- gemerkt nur fuer die kleine
    /// Traegheit beim Ziehen (siehe `anordnungFuer`).
    Dichte          m_dichte{Dichte::Gross};
};

class Ft8PlaetzeApplet : public AppletWidget
{
    Q_OBJECT
public:
    explicit Ft8PlaetzeApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("Ft8Slots"); }
    QString appletTitle() const override { return QStringLiteral("FT8 Slots"); }
    void    syncFromModel() override {}

public slots:
    /// Vom `Ft8Applet` her. Einziger Weg herein.
    void zeigeStand(const Longpath::Ft8PlaetzeStand& s);

    /// Wie hoch das Fenster sein muss, wenn es abgeloest wird.
    ///
    /// `AppletFloatingWindow::applyDefaultSize()` fragt `sizeHint()`
    /// -- NICHT `minimumHeight()`. Und der Vorgabe-sizeHint eines
    /// Applets ist die Summe seiner Anordnung, in der ein
    /// VERSTECKTES Widget gar nicht mitzaehlt: im leeren Zustand ist
    /// die Flaeche versteckt, also blieb nur das Hinweisschild, und
    /// der Betreiber bekam am 2026-10-30 eine Titelleiste von
    /// zwanzig Punkten Hoehe ohne Inhalt.
    ///
    /// Darum die Hoehe hier selbst sagen, statt sie ausrechnen zu
    /// lassen.
    QSize sizeHint() const override;

public:
    Ft8PlaetzeFlaeche* flaecheForTest() const { return m_flaeche; }
    QLabel*            leerForTest() const { return m_leer; }

    /// Unter dieser Hoehe luegt das Fenster (2026-10-30): der leere
    /// Hinweis in lesbarer Breite, und mehr als eine Kachel.
    static constexpr int kMindestH = 150;

protected:
    void resizeEvent(QResizeEvent* ev) override;
    bool event(QEvent* ev) override;
    bool eventFilter(QObject* wer, QEvent* ev) override;

private:
    /// Der Rollbereich, in dem das Applet als Fenster steckt -- oder
    /// nullptr, solange es in der Spalte sitzt.
    QScrollArea* rollbereich() const;

    /// Was das Applet mindestens braucht, damit keine belegte Kachel
    /// abgeschnitten wird. Siehe die Begruendung in der .cpp: die
    /// Mindesthoehe haengt an der BREITE, und die Breite haengt am
    /// Rollbalken, den die Mindesthoehe erst hervorruft.
    void richteMindestmasseEin();

    Ft8PlaetzeFlaeche* m_flaeche{nullptr};
    QLabel*            m_leer{nullptr};
    /// Der Sichtbereich, den wir gerade belauschen (Elternwechsel).
    QPointer<QWidget>  m_belauscht;
    bool               m_richtetEin{false};
};

}  // namespace Longpath
