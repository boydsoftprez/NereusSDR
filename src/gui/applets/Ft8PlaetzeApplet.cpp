// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/applets/Ft8PlaetzeApplet.cpp  (Longpath)
// =================================================================
// Siehe Ft8PlaetzeApplet.h fuer den Anlass und die Wahl des Blattes.
//
// Gemalt wird nach Entwurf A vom 2026-10-30
// (werkzeug/ft8-plaetze-2026-10-30/runde3-A-kacheln.png). Die eine
// warme Stelle im ganzen Panel ist der Punkt „on air" im Kopf: bei
// Multi Answering senden ALLE Plaetze gleichzeitig, „sendet" ist
// darum kein Unterschied zwischen ihnen, sondern ein Zustand des
// Ganzen. Zwoelf warme Marken waeren zwoelfmal dieselbe Aussage und
// ein klarer Bruch der Zwei-Prozent-Regel des Hausstils.
// =================================================================

#include "Ft8PlaetzeApplet.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPalette>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace Longpath {

namespace {

/// Jede Farbe geht durch `hexRole()`. Das ist der zweite Weg der
/// Themes neben der QSS-Tabelle: wer im Malcode die Konstante direkt
/// nimmt, baut ein Panel, das in jedem Thema gleich aussieht --
/// und darum in den meisten falsch.
QColor farbe(const char* token)
{
    return QColor(Style::hexRole(token));
}

/// Der Rapport, wie er geschrieben wird: immer mit Vorzeichen und
/// zwei Stellen. Dieselbe Form wie in der Nachricht selbst -- so
/// stehen die Zahlen untereinander, auch wenn das Vorzeichen
/// wechselt.
QString rapport(int db)
{
    if (db == -99) { return {}; }
    return QStringLiteral("%1%2")
        .arg(db < 0 ? QLatin1Char('-') : QLatin1Char('+'))
        .arg(qAbs(db), 2, 10, QLatin1Char('0'));
}

// ── Die Masze der Anordnung (2026-11-18) ────────────────────────────

/// Die Warteschlange, unveraendert aus `maleWartende`: 26 von der
/// Trennlinie bis zur ersten Zeile, je Wartendem 22, die Schrift
/// darin 16 hoch.
constexpr int kWarteKopfH   = 26;
constexpr int kWarteZeileH  = 22;
constexpr int kWarteSchrift = 16;
/// Was der Groessenhinweis ueber das Noetige hinaus an Luft laesst --
/// genau so viel, dass `noetigeHoehe()` dieselben Zahlen liefert wie
/// seit dem 2026-10-30 (unter der letzten Kachel 8 + 10 statt
/// `kFussH`, unter dem letzten Wartenden 8 + 10 + 10).
constexpr int kHinweisLuftKacheln  = 14;
constexpr int kHinweisLuftWartende = 24;
/// Zwischen zwei Spalten der Warteschlange. Breiter als zwischen zwei
/// Kacheln: dort trennt der Rahmen, hier nur der Abstand.
constexpr int kWarteLuft   = 16;

// ── Woran die Mindestbreite einer Kachel haengt ─────────────────────
//
// `maleKachel` schreibt Rufzeichen und Land in Rechtecke der Breite
// (Kachelbreite - 100): 28 links fuer die Platznummer, 72 rechts fuer
// Tonlage und Pille. Die Rapportzeile bekommt (Kachelbreite - 40).
// Daraus folgt die Mindestbreite, nicht aus einer geschaetzten Zahl.
constexpr int kKachelNebenText    = 100;
constexpr int kKachelNebenRapport = 40;
/// Der Streifen eines freien Platzes: 28 links, 12 rechts.
constexpr int kFreiNebenText      = 40;
/// Ein Wartender: 100 bis zum Land, 60 rechts davon fuer den Rapport.
constexpr int kWarteNebenLand     = 160;

/// Die Untergrenze, damit das Raster nicht mit jedem Partner
/// umspringt: ein gewoehnliches Rufzeichen in voller Laenge (zwei
/// Buchstaben, Ziffer, drei Buchstaben -- hier aus den breitesten, die
/// vorkommen) und der Landesname, den ein Betreiber in OE am
/// haeufigsten liest und der zu den laengeren in cty.dat gehoert.
/// Erst was DARUEBER hinausgeht (EA8/DL1ABC, „Bosnia-Herzegovina"),
/// macht die Kacheln breiter.
const QLatin1String kMassRuf("MW0MWW");
const QLatin1String kMassLand("Fed. Rep. of Germany");

/// So viele gleich breite Spalten, wie in `innen` passen -- mindestens
/// eine, hoechstens so viele, wie es etwas zu verteilen gibt.
int spaltenFuer(int innen, int mindest, int luft, int hoechstens)
{
    if (hoechstens <= 1) { return 1; }
    const int n = (innen + luft) / (qMax(1, mindest) + luft);
    return qBound(1, n, hoechstens);
}

/// Die k-te von n gleich breiten Zellen. Die Zellen teilen sich den
/// Rundungsrest, statt dass rechts ein Punkt uebrig bleibt.
QRect zelle(int x0, int innen, int n, int luft, int k, int y, int h)
{
    const int nutz   = qMax(0, innen - (n - 1) * luft);
    const int links  = x0 + k * luft + (nutz * k) / n;
    const int rechts = x0 + k * luft + (nutz * (k + 1)) / n;
    return QRect(links, y, rechts - links, h);
}

int zeilenFuer(int anzahl, int spalten)
{
    return anzahl <= 0 ? 0 : (anzahl + spalten - 1) / spalten;
}

}  // namespace

// ══ Die Malflaeche ═══════════════════════════════════════════════

Ft8PlaetzeFlaeche::Ft8PlaetzeFlaeche(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void Ft8PlaetzeFlaeche::setzeStand(const Ft8PlaetzeStand& s)
{
    m_stand = s;
    updateGeometry();
    update();
}

// ══ Die Anordnung (2026-11-18) ════════════════════════════════════
//
// Betreiber: „FT8 SLOTS sollte man auch nebeneinander anordnen können
// und soll sich automatsich an die Größe des Fenster anpassen!"
//
// ── Das Raster gehoert den PLAETZEN, nicht den Kacheln ──────────────
//
// Jeder eingestellte Platz hat seine Zelle: belegt steht dort die
// Kachel, frei der Streifen. Die Spaltenzahl haengt darum an der
// Breite und an `plaetzeGesamt` -- nicht daran, wie viele Plaetze
// gerade belegt sind. Sonst waere bei `slots 3` und zwei Partnern
// jede Kachel halb so breit wie das Fenster, und in dem Augenblick,
// in dem der dritte ruft, spraengen beide auf ein Drittel. So kommt
// der dritte dorthin, wo eben noch „3 free" stand, und nichts sonst
// bewegt sich. Es ist auch, was er am 2026-11-13 wollte: „ein fenster
// durch 3 geteilt".
//
// ── Belegte vor freien ──────────────────────────────────────────────
//
// Kopfzeile, Kacheln und Warteschlange sind Pflicht -- reicht die
// Hoehe dafuer nicht, rollt der Gastgeber. Ein freier Streifen neben
// einer Kachel kostet keine Hoehe und bleibt immer. ZEILEN aus lauter
// freien Plaetzen weichen in zwei Schritten:
//
//   Voll      jeder in seiner Zelle;
//   Dicht     enger gepackt, mehrere je Zeile -- derselbe Streifen,
//             nur schmaler;
//   Gewichen  weg. Dass es sie gibt, sagt weiter die Kopfzeile
//             („2 of 6").
//
// Die Kacheln selbst werden dabei nie schmaler als ihre Mindestbreite
// und nie niedriger.
Ft8PlaetzeAnordnung Ft8PlaetzeFlaeche::rechne(int breite, int hoehe,
                                              int spaltenFest) const
{
    Ft8PlaetzeAnordnung a;
    const int innen  = qMax(0, breite - 2 * kRandX);
    const int belegt = int(m_stand.plaetze.size());
    const int gesamt = qMax(m_stand.plaetzeGesamt, belegt);
    const int frei   = gesamt - belegt;

    a.spalten = spaltenFest > 0
        ? spaltenFest
        : spaltenFuer(innen, kachelMindestbreite(), kKachelLuft, gesamt);

    // ── Die Kacheln ─────────────────────────────────────────────
    int y = kKopfH;
    a.kacheln.reserve(belegt);
    for (int i = 0; i < belegt; ++i) {
        a.kacheln.append(zelle(kRandX, innen, a.spalten, kKachelLuft,
                               i % a.spalten,
                               y + (i / a.spalten) * (kKachelH + kKachelLuft),
                               kKachelH));
    }
    const int kachelZeilen = zeilenFuer(belegt, a.spalten);

    // ── Freie neben der letzten Kachel: kosten keine Hoehe ──────
    const int angebrochen = belegt % a.spalten;
    const int mitlaeufer  = angebrochen > 0
        ? qMin(frei, a.spalten - angebrochen) : 0;
    for (int k = 0; k < mitlaeufer; ++k) {
        a.freie.append(zelle(kRandX, innen, a.spalten, kKachelLuft,
                             angebrochen + k,
                             y + (kachelZeilen - 1) * (kKachelH + kKachelLuft),
                             kFreiH));
    }
    y += kachelZeilen * (kKachelH + kKachelLuft);

    // ── Die Warteschlange: Pflicht, also vorab gerechnet ────────
    const int warten = int(m_stand.wartende.size());
    const int warteSpalten = spaltenFest > 0
        ? spaltenFest
        : spaltenFuer(innen, warteMindestbreite(), kWarteLuft, warten);
    const int warteZeilen = zeilenFuer(warten, warteSpalten);

    // ── Zeilen aus lauter freien Plaetzen ───────────────────────
    const int uebrige = frei - mitlaeufer;
    const int dichtSpalten = spaltenFest > 0
        ? spaltenFest
        : spaltenFuer(innen, freiMindestbreite(), kKachelLuft, uebrige);
    // Was das Ganze braucht, wenn `freiZeilen` Zeilen freier Plaetze
    // dastehen: bis zur Unterkante des Letzten, das gemalt wird, und
    // `kFussH` darunter. KNAPP gerechnet -- danach entscheidet sich,
    // ob ein Rollbalken kommt, und der soll nicht wegen leerer Luft
    // unter der letzten Kachel erscheinen.
    auto braucht = [&](int freiZeilen) {
        const int unterZeilen = y + freiZeilen * (kFreiH + kKachelLuft);
        if (warten > 0) {
            return unterZeilen + kWarteKopfH
                   + (warteZeilen - 1) * kWarteZeileH + kWarteSchrift + kFussH;
        }
        const bool zeilen = kachelZeilen > 0 || freiZeilen > 0;
        return (zeilen ? unterZeilen - kKachelLuft : kKopfH) + kFussH;
    };
    auto mitFreien = [&](int spalten) {
        return braucht(zeilenFuer(uebrige, spalten));
    };

    int freiSpalten = 0;
    if (uebrige == 0 || mitFreien(a.spalten) <= hoehe) {
        a.freieArt  = Ft8PlaetzeAnordnung::Freie::Voll;
        freiSpalten = a.spalten;
    } else if (dichtSpalten > a.spalten && mitFreien(dichtSpalten) <= hoehe) {
        a.freieArt  = Ft8PlaetzeAnordnung::Freie::Dicht;
        freiSpalten = dichtSpalten;
    } else {
        a.freieArt  = Ft8PlaetzeAnordnung::Freie::Gewichen;
    }
    int freiZeilen = 0;
    if (freiSpalten > 0) {
        for (int k = 0; k < uebrige; ++k) {
            a.freie.append(zelle(kRandX, innen, freiSpalten, kKachelLuft,
                                 k % freiSpalten,
                                 y + (k / freiSpalten) * (kFreiH + kKachelLuft),
                                 kFreiH));
        }
        freiZeilen = zeilenFuer(uebrige, freiSpalten);
    }
    a.hoehe = braucht(freiZeilen);
    y += freiZeilen * (kFreiH + kKachelLuft);

    // ── Die Wartenden darunter, in ihren eigenen Spalten ────────
    if (warten > 0) {
        a.wartendeY = y;
        a.wartende.reserve(warten);
        for (int k = 0; k < warten; ++k) {
            a.wartende.append(zelle(kRandX, innen, warteSpalten, kWarteLuft,
                                    k % warteSpalten,
                                    y + kWarteKopfH
                                        + (k / warteSpalten) * kWarteZeileH,
                                    kWarteSchrift));
        }
    }
    return a;
}

Ft8PlaetzeAnordnung Ft8PlaetzeFlaeche::anordnungFuer(int breite,
                                                     int hoehe) const
{
    return rechne(breite, hoehe, 0);
}

int Ft8PlaetzeFlaeche::pflichtHoehe(int breite) const
{
    // Hoehe null: nichts Freiwilliges findet Platz.
    return rechne(breite, 0, 0).hoehe;
}

int Ft8PlaetzeFlaeche::wunschHoehe(int breite) const
{
    return rechne(breite, QWIDGETSIZE_MAX, 0).hoehe;
}

int Ft8PlaetzeFlaeche::kachelMindestbreite() const
{
    // Dieselben Schriften wie in `maleKachel` -- wer dort eine
    // aendert, aendert sie hier.
    QFont gross = font();
    gross.setPointSizeF(16.0);
    QFont cq = font();
    cq.setPointSizeF(14.0);
    QFont klein = font();
    klein.setPointSizeF(9.0);
    const QFontMetrics mGross(gross);
    const QFontMetrics mCq(cq);
    const QFontMetrics mKlein(klein);
    const QFontMetrics mMono(Style::monoFont(font(), 12));

    int text = qMax(mGross.horizontalAdvance(kMassRuf),
                    mKlein.horizontalAdvance(kMassLand));
    int rapp = 0;
    for (const Ft8PlatzStand& z : m_stand.plaetze) {
        if (z.partner.isEmpty()) {
            text = qMax(text, mCq.horizontalAdvance(tr("calling CQ")));
            text = qMax(text,
                        mKlein.horizontalAdvance(tr("waiting for an answer")));
        } else {
            text = qMax(text, mGross.horizontalAdvance(z.partner));
            text = qMax(text, mKlein.horizontalAdvance(z.land));
        }
        rapp = qMax(rapp, mMono.horizontalAdvance(rapportZeile(z)));
    }
    // Zwei Punkte Luft: `horizontalAdvance` rundet, und ein Rechteck,
    // das auf den Punkt passt, schneidet den letzten Buchstaben an.
    return qMax(text + kKachelNebenText, rapp + kKachelNebenRapport) + 2;
}

int Ft8PlaetzeFlaeche::freiMindestbreite() const
{
    QFont klein = font();
    klein.setPointSizeF(9.0);
    return QFontMetrics(klein).horizontalAdvance(tr("free"))
           + kFreiNebenText + 2;
}

int Ft8PlaetzeFlaeche::warteMindestbreite() const
{
    QFont klein = font();
    klein.setPointSizeF(9.0);
    return QFontMetrics(klein).horizontalAdvance(kMassLand)
           + kWarteNebenLand + 2;
}

QString Ft8PlaetzeFlaeche::rapportZeile(const Ft8PlatzStand& z)
{
    QStringList teile;
    if (z.gesendet  != -99) { teile << tr("sent %1").arg(rapport(z.gesendet)); }
    if (z.empfangen != -99) { teile << tr("rcvd %1").arg(rapport(z.empfangen)); }
    return teile.join(QStringLiteral("   "));
}

int Ft8PlaetzeFlaeche::noetigeHoehe() const
{
    // EINE Spalte, fest -- nicht „was bei 320 Punkt herauskommt". Der
    // Groessenhinweis soll auf jedem Rechner derselbe sein, auch dort,
    // wo die Schrift schmaler laeuft und zwei Kacheln in 320 passten.
    // Die Rechnung ist dieselbe wie vor dem 2026-11-18: Kopf, je
    // Kachel 92 + 8, je freiem Platz 30 + 8 (Entwurf B, 2026-11-13),
    // die Wartenden, 10 darunter.
    const int h = rechne(kHinweisBreite, QWIDGETSIZE_MAX, 1).hoehe
                  + (m_stand.wartende.isEmpty() ? kHinweisLuftKacheln
                                                : kHinweisLuftWartende);
    // ── Die Untergrenze ist kein Geschmack (2026-10-30) ─────────
    //
    // `AppletFloatingWindow::applyDefaultSize()` rechnet die
    // Fenstergroesse aus `sizeHint()`. Beim Abloesen ist das Applet
    // noch versteckt, und ein verstecktes Widget verlangt fast
    // nichts -- der Betreiber bekam eine Titelleiste ohne Inhalt:
    // „ein eigenes fenster erscheint hier aber nicht", mit einem
    // 20 Punkte hohen Streifen im Bild.
    //
    // 150 ist der leere Hinweistext in lesbarer Breite. Eine Kachel
    // braucht 100, und darunter faengt das Fenster an zu luegen.
    return qMax(h, 150);
}

QSize Ft8PlaetzeFlaeche::sizeHint() const
{
    return QSize(kHinweisBreite, noetigeHoehe());
}

void Ft8PlaetzeFlaeche::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Den Grund selbst malen. Eingebettet in die Spalte erbt das
    // Widget ihn vom Panel und man merkt nichts; als ABGEDOCKTES
    // Fenster -- und genau darum ging die Bitte („seitlich im eigenen
    // fenster") -- stuende sonst die Vorgabe des Systems dahinter,
    // und das ist auf diesem Rechner Weisz. Im Abgriff vom
    // 2026-10-30 war genau das zu sehen: dunkle Schrift auf hellem
    // Grund, die Haelfte unlesbar.
    p.fillRect(rect(), farbe(Style::kPanelBg));

    // Wo was steht, rechnet `anordnungFuer` -- aus der Groesse, die
    // die Flaeche in DIESEM Augenblick hat. Kein gemerkter Stand, der
    // beim Ziehen am Rand einen Zug hinterherhinken koennte.
    const Ft8PlaetzeAnordnung a = anordnung();

    maleKopfzeile(p);
    for (int i = 0; i < a.kacheln.size(); ++i) {
        maleKachel(p, a.kacheln.at(i), i + 1, m_stand.plaetze.at(i));
    }
    // Und die freien (Entwurf B, 2026-11-13): „schön wäre es, wenn
    // man dann auch alle 3 sieht." Sie stehen da, solange die Hoehe
    // reicht -- eine belegte Kachel verdraengen sie nie.
    for (int k = 0; k < a.freie.size(); ++k) {
        maleFreien(p, a.freie.at(k), int(a.kacheln.size()) + k + 1);
    }
    if (a.wartendeY >= 0) {
        maleWartende(p, a);
    }
}

// ── Die EINE warme Stelle ────────────────────────────────────────────
//
// Die Titelleiste gehoert dem Container, nicht uns; „on air" und
// „2 of 3" aus dem Blatt haetten dort keinen Platz. Also traegt die
// Flaeche eine eigene schmale Zeile.
//
// Warum der Punkt nicht je Kachel sitzt: bei Multi Answering senden
// ALLE Plaetze gleichzeitig -- in derselben Tastung, 60 Hz
// auseinander. „Sendet" unterscheidet sie also nicht, es ist ein
// Zustand des Ganzen. Zwoelf warme Marken waeren zwoelfmal dieselbe
// Aussage und ein klarer Bruch der Zwei-Prozent-Regel.
int Ft8PlaetzeFlaeche::maleKopfzeile(QPainter& p)
{
    const int w = width() - 2 * kRandX;
    if (w < 80) { return 2; }

    QFont klein = font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);

    const int belegt = int(std::count_if(
        m_stand.plaetze.cbegin(), m_stand.plaetze.cend(),
        [](const Ft8PlatzStand& z) { return !z.partner.isEmpty(); }));
    p.setPen(farbe(Style::kTextScale));
    p.drawText(QRect(kRandX + w - 60, 4, 60, 14),
               Qt::AlignRight | Qt::AlignVCenter,
               tr("%1 of %2").arg(belegt).arg(m_stand.plaetzeGesamt));

    if (m_stand.sendet) {
        p.setPen(Qt::NoPen);
        p.setBrush(farbe(Style::kTxKeyBorder));
        p.drawEllipse(QRectF(kRandX + w - 112, 7.5, 6, 6));
        p.setPen(farbe(Style::kTxKeyBorder));
        p.drawText(QRect(kRandX + w - 102, 4, 46, 14),
                   Qt::AlignLeft | Qt::AlignVCenter, tr("on air"));
    }
    return 22;
}

void Ft8PlaetzeFlaeche::maleKachel(QPainter& p, const QRect& r, int nummer,
                                   const Ft8PlatzStand& z)
{
    const int w = r.width();
    if (w < 80) { return; }

    // Panel-Radius 8 (HAUSSTIL: nie Radius 3).
    p.setPen(farbe(Style::kBorderSubtle));
    p.setBrush(farbe(Style::kTileActiveBg));
    p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    // Die Platznummer, klein und links -- eine Spurnummer, kein Titel.
    QFont klein = font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextInactive));
    p.drawText(QRect(r.left() + 8, r.top() + 10, 14, 14),
               Qt::AlignLeft | Qt::AlignVCenter, QString::number(nummer));

    const int tx = r.left() + 28;

    // Rufzeichen -- oder der CQ-Ruf, der keines hat.
    QFont gross = font();
    gross.setPointSizeF(z.partner.isEmpty() ? 14.0 : 16.0);
    p.setFont(gross);
    p.setPen(farbe(z.partner.isEmpty() ? Style::kTextScale
                                       : Style::kTextPrimary));
    p.drawText(QRect(tx, r.top() + 6, w - 100, 24),
               Qt::AlignLeft | Qt::AlignVCenter,
               z.partner.isEmpty() ? tr("calling CQ") : z.partner);

    // Land, oder was der CQ-Ruf stattdessen sagt.
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(z.partner.isEmpty() ? Style::kTextInactive
                                       : Style::kTextScale));
    p.drawText(QRect(tx, r.top() + 30, w - 100, 14),
               Qt::AlignLeft | Qt::AlignVCenter,
               z.partner.isEmpty() ? tr("waiting for an answer") : z.land);

    // Die Tonlage rechts oben. Grau, nicht warm: sie ist eine
    // Einstellung, keine Messung.
    // `Style::monoFont` statt `setFamily("Menlo")`: Menlo gibt es nur
    // auf dem Mac. Qt nimmt anderswo klaglos die naechstbeste Schrift,
    // und das ist nicht zwingend eine dicktengleiche -- dann stehen
    // die Zahlen nicht mehr untereinander, und genau dafuer ist die
    // Mono-Schrift hier da. `tst_schriftfamilien` liest den Quelltext
    // und faengt es (2026-10-30, auf dem Linux-Shard).
    p.setFont(Style::monoFont(font(), 13));
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(QRect(r.right() - 76, r.top() + 8, 68, 14),
               Qt::AlignRight | Qt::AlignVCenter,
               tr("%1 Hz").arg(z.tonHz));

    // Der naechste Zug, als Pille (Radius 6).
    if (!z.zug.isEmpty()) {
        const QRect pille(r.right() - 56, r.top() + 28, 48, 17);
        p.setPen(farbe(Style::kBorder));
        p.setBrush(farbe(Style::kPanelBg));
        p.drawRoundedRect(QRectF(pille).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        p.setFont(klein);
        p.setPen(farbe(Style::kTextPrimary));
        p.drawText(pille, Qt::AlignCenter, z.zug);
    }

    // Die beiden Rapporte in einer Zeile, in Mono -- damit sie
    // untereinander stehen, auch wenn das Vorzeichen wechselt.
    const QString rapporte = rapportZeile(z);
    if (!rapporte.isEmpty()) {
        p.setFont(Style::monoFont(font(), 12));
        p.setPen(farbe(Style::kLabelMid));
        p.drawText(QRect(tx, r.top() + 50, w - 40, 14),
                   Qt::AlignLeft | Qt::AlignVCenter, rapporte);
    }

    // Der Fortschritt: fuenf Striche, einer je Zug im QSO. Kein
    // durchgehender Balken -- ein QSO hat Stufen, keinen Verlauf.
    const int sy = r.top() + 76;
    for (int k = 0; k < kStufen; ++k) {
        const int x0 = tx + k * 13;
        // Erreicht gegen offen: im ersten Abgriff lagen beide so dicht
        // beieinander, dass die Stufe nicht abzulesen war. Jetzt
        // Zweitfarbe gegen die gedaempfte Rahmenfarbe.
        p.setPen(QPen(farbe(k < z.stufe ? Style::kTextSecondary
                                        : Style::kBorder), 2));
        p.drawLine(x0, sy, x0 + 9, sy);
    }
}

// ── Ein freier Platz (Entwurf B, 2026-11-13) ─────────────────────────
//
// Betreiber mit `slots 3` vor einem Fenster, das „1 of 3" und eine
// einzige Kachel zeigte: „schön wäre es, wenn man dann auch alle 3
// sieht, sprich 3 fenster und ein fenster durch 3 geteilt."
//
// Aus drei Blaettern waehlte er B. Der Unterschied zu A (jeder Platz
// dieselbe volle Kachel) ist die Hoehe: bei `slots 6` und einem
// Rufer stuenden dort fuenf leere Kacheln zu je 92 Punkten -- ein
// Fenster, das zu fuenf Sechsteln aus Luft besteht. Als Streifen
// kostet ein freier Platz 30 statt 92, und alle sind trotzdem da.
//
// Kein gestrichelter Rand wie im Blatt: durchgezogen, nur ohne
// Fuellung. Die Mulde unterscheidet belegt von frei schon, und eine
// zweite Unterscheidung an derselben Stelle sagt nichts dazu.
void Ft8PlaetzeFlaeche::maleFreien(QPainter& p, const QRect& r, int nummer)
{
    // Kein „unter 80 malt nichts" wie bei der Kachel: dicht gepackt
    // ist ein Streifen mit Absicht schmal, und `freiMindestbreite`
    // sorgt dafuer, dass „free" hineinpasst.
    const int w = r.width();
    if (w < 40) { return; }

    p.setPen(farbe(Style::kBorderSubtle));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    QFont klein = font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextInactive));
    p.drawText(QRect(r.left() + 8, r.top(), 14, kFreiH),
               Qt::AlignLeft | Qt::AlignVCenter, QString::number(nummer));
    p.drawText(QRect(r.left() + 28, r.top(), w - 40, kFreiH),
               Qt::AlignLeft | Qt::AlignVCenter, tr("free"));
}

void Ft8PlaetzeFlaeche::maleWartende(QPainter& p,
                                     const Ft8PlaetzeAnordnung& a)
{
    const int y = a.wartendeY;
    const int w = width() - 2 * kRandX;
    p.setPen(farbe(Style::kBorderSubtle));
    p.drawLine(kRandX, y, kRandX + w, y);

    QFont klein = font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextScale));
    p.drawText(QRect(kRandX + 4, y + 8, 160, 14),
               Qt::AlignLeft | Qt::AlignVCenter, tr("WAITING"));

    // Je Wartendem eine Zeile in seiner Zelle. In EINER Spalte ist
    // die Zelle die ganze Breite, und alles steht, wo es seit dem
    // 2026-10-30 steht; im breiten Fenster stehen sie nebeneinander,
    // in der Reihenfolge der Schlange von links nach rechts.
    for (int k = 0; k < a.wartende.size(); ++k) {
        const Ft8WartenderStand& wart = m_stand.wartende.at(k);
        const QRect z  = a.wartende.at(k);
        const int   yy = z.top();
        const int   zw = z.width();

        QFont f = font();
        f.setPointSizeF(10.5);
        p.setFont(f);
        p.setPen(farbe(Style::kLabelMid));
        p.drawText(QRect(z.left() + 14, yy, qMin(110, zw - 14), 16),
                   Qt::AlignLeft | Qt::AlignVCenter, wart.partner);

        p.setFont(klein);
        p.setPen(farbe(Style::kTextInactive));
        p.drawText(QRect(z.left() + 100, yy, zw - 160, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, wart.land);

        p.setFont(Style::monoFont(font(), 12));
        p.setPen(farbe(Style::kTextScale));
        p.drawText(QRect(z.left() + zw - 50, yy, 44, 16),
                   Qt::AlignRight | Qt::AlignVCenter, rapport(wart.snr));
    }
}

// ══ Das Applet ═══════════════════════════════════════════════════

Ft8PlaetzeApplet::Ft8PlaetzeApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    // Derselbe Grund wie in der Flaeche -- sonst bleibt unter ihr ein
    // heller Streifen stehen, wo der Dehnraum sitzt. Im Abgriff vom
    // 2026-10-30 war er deutlich zu sehen.
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(Style::hexRole(Style::kPanelBg)));
    setPalette(pal);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // ── Der leere Zustand sagt die Wahrheit (2026-10-30) ───────
    //
    // Er stand zuerst da als „Set slots to 2 or more" -- und das war
    // irrefuehrend. Betreiber: „fuer was benoetige ich den ft8 slot".
    // Das Panel fuellt sich AUCH mit einem Platz: mit dem laufenden
    // Partner und mit der Warteschlange. Leer ist es nur, solange
    // ueberhaupt nichts laeuft.
    m_leer = new QLabel(tr("Nothing running.\n\n"
                           "While a QSO is going, this shows who you are "
                           "working and who is waiting for you.\n\n"
                           "Set “slots” in the FT8 window to 2 or more "
                           "to work several callers at the same time."), this);
    m_leer->setWordWrap(true);
    m_leer->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_leer->setContentsMargins(14, 14, 14, 14);
    m_leer->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                              .arg(QString::fromLatin1(Style::kTextScale)));
    // Mit Dehnung, und unten KEIN Dehnraum mehr (2026-11-18): was
    // gerade zu sehen ist -- Hinweis oder Flaeche --, bekommt die
    // ganze Hoehe des Applets. Die Flaeche braucht sie, um zu wissen,
    // ob die freien Plaetze noch hineinpassen; der Hinweis steht
    // ohnehin oben (`AlignTop`).
    lay->addWidget(m_leer, 1);

    // Auch das Applet, nicht nur die Flaeche: beim Abloesen fragt
    // `applyDefaultSize()` DAS Applet, und das reicht die Frage an
    // seine Anordnung weiter -- die bei verstecktem Inhalt fast
    // nichts verlangt.
    setMinimumHeight(kMindestH);

    m_flaeche = new Ft8PlaetzeFlaeche(this);
    m_flaeche->hide();
    lay->addWidget(m_flaeche, 1);
}

QSize Ft8PlaetzeApplet::sizeHint() const
{
    // Breit genug fuer „HB9ZHK · Switzerland · 1790 Hz · RR73" in
    // einer Zeile, hoch genug fuer zwei Kacheln. Steht mehr darin,
    // waechst die Flaeche von selbst (`noetigeHoehe`), und das
    // Fenster laesst sich ziehen -- die Groesse merkt sich
    // `AppletFensterMasse`.
    //
    // Der Hinweis gilt fuer EINE Spalte in 320 Punkt Breite -- die
    // Groesse, in der das Fenster zum ersten Mal aufgeht. Zieht der
    // Betreiber es breiter, ordnet die Flaeche um; der Hinweis rechnet
    // das nicht mit, sonst haenge die Hoehe des ersten Fensters an
    // einer Breite, die es noch gar nicht hat.
    const int inhalt = m_flaeche ? m_flaeche->noetigeHoehe() : kMindestH;
    return QSize(Ft8PlaetzeFlaeche::kHinweisBreite,
                 qMax(inhalt, 2 * Ft8PlaetzeFlaeche::kKachelH + 40));
}

void Ft8PlaetzeApplet::zeigeStand(const Longpath::Ft8PlaetzeStand& s)
{
    const bool etwas = hatInhalt(s);
    m_flaeche->setzeStand(s);
    m_leer->setVisible(!etwas);
    m_flaeche->setVisible(etwas);
    richteMindestmasseEin();
}

// ── Die Mindestmasze: was nie abgeschnitten werden darf ─────────────
//
// Bis zum 2026-11-18 trug die FLAECHE eine Mindesthoehe und das
// Applet fest 150. Eine von Hand gesetzte Mindesthoehe schlaegt aber
// die der Anordnung (`qSmartMinSize`), also sah jeder Gastgeber 150 --
// und im Fenster des Betreibers (365 x 340, drei belegt) lief die
// dritte Kachel unten hinaus, OHNE dass ein Rollbalken kam.
//
// Jetzt sagt das Applet selbst, was es braucht: die Pflichthoehe in
// der Breite, die es hat. Reicht der Gastgeber die nicht, rollt er.
//
// ── Der Rollbalken und die Breite beissen sich ──────────────────────
//
// Im Fenster haengt das an einem Kreis: die Pflichthoehe haengt an
// der Spaltenzahl, die Spaltenzahl an der Breite, und die Breite
// daran, ob der senkrechte Rollbalken dasteht -- den die Pflichthoehe
// erst hervorruft. In einem Band von der Breite des Balkens gibt es
// zwei stimmige Zustaende: zwei Spalten ohne Balken, oder eine Spalte
// mit. Wer nur mit der Breite rechnet, die er gerade hat, bleibt im
// zweiten haengen, sobald er einmal dort war.
//
// Darum zuerst die Frage: passte alles, WENN der Balken weg waere?
// Dann fuer diese Breite rechnen -- der Gastgeber nimmt den Balken
// darauf selbst weg. Erst wenn nicht, gilt die Breite, die da ist.
void Ft8PlaetzeApplet::richteMindestmasseEin()
{
    if (m_richtetEin || !m_flaeche || !m_leer) { return; }
    // `setMinimumHeight` kann das Applet sofort groesser ziehen, und
    // dessen `resizeEvent` kaeme wieder hierher.
    m_richtetEin = true;

    const bool etwas = hatInhalt(m_flaeche->stand());
    auto hoeheBei = [this, etwas](int breite) {
        const int h = etwas ? m_flaeche->pflichtHoehe(breite)
                            : m_leer->heightForWidth(breite);
        return qMax(kMindestH, h);
    };

    int breite  = width();
    int mindestB = 0;
    if (QScrollArea* r = rollbereich()) {
        // Schmaler als eine Kachel: lieber waagrecht rollen als den
        // Text beschneiden. NUR im Fenster -- in der Spalte wuerde
        // eine Mindestbreite die ganze Spalte aufziehen, sobald ein
        // langes Rufzeichen kommt.
        if (etwas && r->horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
            mindestB = m_flaeche->mindestBreite();
        }
        const QSize frei = r->maximumViewportSize();
        const int ohneBalken = qMax(frei.width(), mindestB);
        if (hoeheBei(ohneBalken) <= frei.height()) {
            breite = ohneBalken;
        }
    }
    const int mindestH = hoeheBei(qMax(breite, mindestB));

    if (minimumWidth() != mindestB)  { setMinimumWidth(mindestB); }
    if (minimumHeight() != mindestH) { setMinimumHeight(mindestH); }
    m_richtetEin = false;
}

QScrollArea* Ft8PlaetzeApplet::rollbereich() const
{
    // `AppletFloatingWindow` und das Kombi-Feld stecken das Applet in
    // einen QScrollArea; sein Elternteil ist dann dessen Sichtbereich.
    QWidget* sicht = parentWidget();
    auto* r = sicht ? qobject_cast<QScrollArea*>(sicht->parentWidget())
                    : nullptr;
    return (r && r->viewport() == sicht) ? r : nullptr;
}

void Ft8PlaetzeApplet::resizeEvent(QResizeEvent* ev)
{
    AppletWidget::resizeEvent(ev);
    richteMindestmasseEin();
}

bool Ft8PlaetzeApplet::event(QEvent* ev)
{
    // Abgeloest, angedockt, ins Kombi-Feld gelegt: jedes Mal ein
    // anderer Gastgeber. Seinen Sichtbereich belauschen wir, weil
    // dessen HOEHE mitredet (siehe oben) und sich aendern kann, ohne
    // dass das Applet selbst eine andere Groesse bekommt -- es ist
    // dann hoeher als der Sichtbereich und bleibt es.
    if (ev->type() == QEvent::ParentChange) {
        if (m_belauscht) { m_belauscht->removeEventFilter(this); }
        m_belauscht = parentWidget();
        if (m_belauscht) { m_belauscht->installEventFilter(this); }
        richteMindestmasseEin();
    }
    return AppletWidget::event(ev);
}

bool Ft8PlaetzeApplet::eventFilter(QObject* wer, QEvent* ev)
{
    if (wer == m_belauscht && ev->type() == QEvent::Resize) {
        richteMindestmasseEin();
    }
    return AppletWidget::eventFilter(wer, ev);
}

}  // namespace Longpath
