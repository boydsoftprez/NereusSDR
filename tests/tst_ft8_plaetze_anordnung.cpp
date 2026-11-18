// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_ft8_plaetze_anordnung.cpp  (Longpath)
// =================================================================
//
// „FT8 Slots" ordnet sich nach der Groesse des Fensters -- geprueft
// am ECHTEN Fenster und am BILD.
//
// ── Warum es diese Pruefung gibt ────────────────────────────────────
//
// Betreiber am 2026-11-18, am Morgen, an dem zwei Stationen zum
// ersten Mal gleichzeitig hinausgingen: „FT8 SLOTS sollte man auch
// nebeneinander anordnen können und soll sich automatsich an die
// Größe des Fenster anpassen!"
//
// Die Kacheln standen immer untereinander. Zog er das Fenster breit
// und niedrig, blieb rechts alles leer -- und die dritte Kachel lief
// unten hinaus, OHNE Rollbalken: die Mindesthoehe des Applets stand
// fest auf 150, und eine von Hand gesetzte Mindesthoehe schlaegt die
// der Anordnung. Der Rollbereich des Fensters sah also 150 und fand
// nichts zu rollen.
//
// ── Warum am Bild und nicht an den Rechtecken ───────────────────────
//
// `tst_ft8_plaetze_panel` fragt `anordnungFuer()` nach den
// Rechtecken. Das prueft die Rechnung -- nicht, ob das Fenster sie
// auch zeigt. Zwischen beiden liegen der Rollbereich, die
// Mindestmasze und der Rollbalken, der dem Inhalt Breite nimmt; alle
// drei reden bei der Spaltenzahl mit, und keiner davon steht in der
// Rechnung.
//
// Darum sucht dieser Lauf die Kacheln dort, wo der Betreiber sie
// sieht: im Abgriff des Fensters, an ihrer Mulde. Er braucht dazu
// NICHTS von dem, was am 2026-11-18 dazukam -- und laesst sich
// deshalb auch gegen die Fassung davor bauen. Das ist die Gegenprobe,
// gefahren am 2026-11-18 (Quelltext des Panels per Patch zurueck, nur
// dieser Lauf gebaut): dort sind `breitUndNiedrig…`, `einerVonVier…`,
// `zuKlein…`, `raster…` und `beimZiehen…` rot. Die uebrigen drei
// (`schmalUndHoch…`, `inSeinerGroesse…`, `derLeereHinweis…`) sind in
// beiden Fassungen gruen -- sie halten fest, was bleiben muss.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-18 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QImage>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>

#include <memory>

#include "gui/StyleConstants.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/Ft8PlaetzeApplet.h"
#include "gui/applets/Ft8PlatzStand.h"
#include "gui/styles/AppTheme.h"
#include "models/RadioModel.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

/// Die Kopfzeile der Flaeche („on air", „2 of 3").
constexpr int kKopf = 22;

/// `belegt` von `gesamt` Plaetzen, mit dem, was eine Kachel wirklich
/// traegt: Rufzeichen, Land, Tonlage, Zug.
Ft8PlaetzeStand standMit(int belegt, int gesamt)
{
    static const struct { const char* ruf; const char* land; int ton; }
        quellen[] = {
            { "R6FMM",  "European Russia",      2310 },
            { "UN6LN",  "Kazakhstan",           2370 },
            { "JS6TWW", "Japan",                2430 },
            { "HB9ZHK", "Switzerland",          2490 },
            { "DL1ABC", "Fed. Rep. of Germany", 2550 },
            { "9A6T",   "Croatia",              2610 } };
    Ft8PlaetzeStand s;
    s.plaetzeGesamt = gesamt;
    s.sendet = belegt > 0;
    for (int i = 0; i < belegt; ++i) {
        Ft8PlatzStand z;
        z.partner = QString::fromLatin1(quellen[i % 6].ruf);
        z.land    = QString::fromLatin1(quellen[i % 6].land);
        z.tonHz   = quellen[i % 6].ton;
        z.zug     = QStringLiteral("Tx%1").arg(1 + i % 3);
        z.stufe   = 1 + i % 4;
        s.plaetze.append(z);
    }
    return s;
}

/// Das Panel in seinem echten Fenster.
struct Aufbau {
    RadioModel                            radio;
    Ft8PlaetzeApplet*                     panel{nullptr};
    std::unique_ptr<AppletFloatingWindow> fenster;
    QScrollArea*                          roll{nullptr};

    explicit Aufbau(const Ft8PlaetzeStand& st)
    {
        // Die Reihenfolge wie im Programm: der Stand kommt, solange
        // das Applet noch in der Spalte sitzt; erst danach loest die
        // Automatik es ab.
        panel = new Ft8PlaetzeApplet(&radio, nullptr);
        panel->zeigeStand(st);
        fenster = std::make_unique<AppletFloatingWindow>(
            panel, QStringLiteral("Ft8Slots"), 0, nullptr);
        roll = fenster->findChild<QScrollArea*>();
    }

    /// Bis die Anordnung steht. Eine Groessenaenderung laeuft ueber
    /// mehrere gepostete Ereignisse (Mindesthoehe -> Rollbereich ->
    /// Rollbalken -> Breite -> Mindesthoehe); eine feste, kleine Zahl
    /// von Runden statt einer Schleife ohne Ende.
    static void beruhige()
    {
        for (int runde = 0; runde < 12; ++runde) {
            QCoreApplication::sendPostedEvents();
            QCoreApplication::processEvents();
        }
    }

    void zieheAuf(int breite, int hoehe)
    {
        fenster->resize(breite, hoehe);
        beruhige();
    }

    bool rolltSenkrecht() const
    {
        const QScrollBar* b = roll->verticalScrollBar();
        return b->isVisible() && b->maximum() > b->minimum();
    }
};

/// Was im Sichtbereich an Kacheln zu SEHEN ist.
struct Gesehen {
    /// In voller Hoehe sichtbar, von links oben nach rechts unten.
    QVector<QRect> ganze;
    /// Vom Rand des Sichtbereichs angeschnitten.
    QVector<QRect> angeschnittene;
};

/// Sucht die Kacheln im Abgriff des Fensters -- an ihrer Mulde
/// (`kTileActiveBg`), der einzigen Flaeche dieser Farbe im Panel.
///
/// Zusammenhaengende Flaechen der Farbe, je mit ihrem umschlieszenden
/// Rechteck. Schrift und Pille stanzen Loecher hinein, trennen die
/// Flaeche aber nicht; was kleiner ist als eine halbe Kachelbreite
/// (das Innere einer „0"), faellt weg.
Gesehen sucheKacheln(const Aufbau& a)
{
    Gesehen g;
    const QImage bild = a.fenster->grab().toImage()
                            .convertToFormat(QImage::Format_RGB32);
    const qreal dpr = bild.devicePixelRatio();
    const QRgb mulde = QColor(Style::hexRole(Style::kTileActiveBg)).rgb();

    const QWidget* sicht = a.roll->viewport();
    const QRect ausschnitt(sicht->mapTo(a.fenster.get(), QPoint(0, 0)),
                           sicht->size());
    const int x0 = qRound(ausschnitt.left() * dpr);
    const int y0 = qRound(ausschnitt.top() * dpr);
    const int w  = qMin(qRound(ausschnitt.width() * dpr),
                        bild.width() - x0);
    const int h  = qMin(qRound(ausschnitt.height() * dpr),
                        bild.height() - y0);
    if (w <= 0 || h <= 0) { return g; }

    QVector<char> besucht(w * h, 0);
    QVector<int>  stapel;
    for (int sy = 0; sy < h; ++sy) {
        for (int sx = 0; sx < w; ++sx) {
            if (besucht[sy * w + sx]
                || bild.pixel(x0 + sx, y0 + sy) != mulde) {
                continue;
            }
            int links = sx, rechts = sx, oben = sy, unten = sy;
            stapel.clear();
            stapel.append(sy * w + sx);
            besucht[sy * w + sx] = 1;
            while (!stapel.isEmpty()) {
                const int i = stapel.takeLast();
                const int px = i % w;
                const int py = i / w;
                links = qMin(links, px);  rechts = qMax(rechts, px);
                oben  = qMin(oben, py);   unten  = qMax(unten, py);
                const int nx[4] = {px - 1, px + 1, px, px};
                const int ny[4] = {py, py, py - 1, py + 1};
                for (int k = 0; k < 4; ++k) {
                    if (nx[k] < 0 || nx[k] >= w || ny[k] < 0 || ny[k] >= h) {
                        continue;
                    }
                    const int j = ny[k] * w + nx[k];
                    if (besucht[j]
                        || bild.pixel(x0 + nx[k], y0 + ny[k]) != mulde) {
                        continue;
                    }
                    besucht[j] = 1;
                    stapel.append(j);
                }
            }
            // Zurueck in Punkte, bezogen auf den Sichtbereich.
            const QRect r(qRound(links / dpr), qRound(oben / dpr),
                          qRound((rechts - links + 1) / dpr),
                          qRound((unten - oben + 1) / dpr));
            if (r.width() < 60) { continue; }
            // Die Mulde liegt einen Punkt innerhalb des Rahmens.
            if (r.height() >= Ft8PlaetzeFlaeche::kKachelH - 4) {
                g.ganze.append(r);
            } else if (r.height() >= 3) {
                g.angeschnittene.append(r);
            }
        }
    }
    std::sort(g.ganze.begin(), g.ganze.end(),
              [](const QRect& l, const QRect& r) {
                  return l.top() != r.top() ? l.top() < r.top()
                                            : l.left() < r.left();
              });
    return g;
}

/// Wie viele verschiedene Werte (auf zwei Punkte genau).
int verschiedene(QVector<int> werte)
{
    std::sort(werte.begin(), werte.end());
    int n = 0;
    for (int i = 0; i < werte.size(); ++i) {
        if (i == 0 || werte.at(i) - werte.at(i - 1) > 2) { ++n; }
    }
    return n;
}

QByteArray beschreibe(const Gesehen& g)
{
    QString s = QStringLiteral("%1 ganze, %2 angeschnittene:")
                    .arg(g.ganze.size()).arg(g.angeschnittene.size());
    for (const QRect& r : g.ganze) {
        s += QStringLiteral(" [%1,%2 %3x%4]")
                 .arg(r.left()).arg(r.top()).arg(r.width()).arg(r.height());
    }
    for (const QRect& r : g.angeschnittene) {
        s += QStringLiteral(" (%1,%2 %3x%4)")
                 .arg(r.left()).arg(r.top()).arg(r.width()).arg(r.height());
    }
    return s.toUtf8();
}

}  // namespace

class TstFt8PlaetzeAnordnung : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Das Thema des Programms -- vor allem seine Rollbalken. Die
        // des Systems schweben auf dem Mac ueber dem Inhalt und nehmen
        // ihm keine Breite; die des Programms tun es (10 Punkt), und
        // genau daran haengt der Fall `beimZiehen…` unten.
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);

        QVERIFY2(QColor(Style::hexRole(Style::kTileActiveBg))
                     != QColor(Style::hexRole(Style::kPanelBg)),
                 "Mulde und Grund haben dieselbe Farbe -- dann findet "
                 "dieser Lauf keine Kachel und prueft nichts");
    }

    // ── Der Fall, um den es ging ──────────────────────────────────
    //
    // Breit und niedrig, drei belegt: eine Zeile. Vorher standen sie
    // untereinander, eine war zu sehen, die zweite angeschnitten, die
    // dritte gar nicht.
    void breitUndNiedrigStehenDreiNebeneinander()
    {
        Aufbau a(standMit(3, 3));
        a.fenster->resize(1000, 200);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        const Gesehen g = sucheKacheln(a);
        QVERIFY2(g.ganze.size() == 3, beschreibe(g).constData());
        QVERIFY2(g.angeschnittene.isEmpty(), beschreibe(g).constData());
        QVERIFY2(!a.rolltSenkrecht(),
                 "eine Zeile Kacheln braucht keinen Rollbalken");

        // Nebeneinander: dieselbe Oberkante, gleich breit, und keine
        // beruehrt die naechste.
        QVERIFY2(qAbs(g.ganze.at(0).top() - g.ganze.at(2).top()) <= 1,
                 beschreibe(g).constData());
        QVERIFY2(qAbs(g.ganze.at(0).width() - g.ganze.at(2).width()) <= 1,
                 beschreibe(g).constData());
        QVERIFY2(g.ganze.at(1).left() > g.ganze.at(0).right()
                     && g.ganze.at(2).left() > g.ganze.at(1).right(),
                 beschreibe(g).constData());
        // Und sie fuellen die Breite, statt links zu kleben.
        QVERIFY2(g.ganze.at(2).right() > a.roll->viewport()->width() - 20,
                 beschreibe(g).constData());
    }

    // ── Sein Bild vom selben Tag ──────────────────────────────────
    //
    // „beim in die länge ziehen nach rechts sollen dann auch die
    // fenster sich nebeneinander setzen." Das Fenster rund 1040 x 340,
    // „1 of 4": EINE Kachel ueber die ganze Breite, darunter drei
    // „free" untereinander, rechts alles leer.
    //
    // Jetzt hat jeder der vier Plaetze seine Zelle. Die eine Kachel
    // ist darum hoechstens halb so breit wie das Fenster (auf jedem
    // Rechner; auf dem Mac ein Viertel), und kommen zwei Partner dazu,
    // stehen alle drei in EINER Zeile.
    void einerVonVierFuelltNichtMehrDieGanzeBreite()
    {
        Aufbau a(standMit(1, 4));
        a.fenster->resize(1040, 340);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        const Gesehen einer = sucheKacheln(a);
        QVERIFY2(einer.ganze.size() == 1, beschreibe(einer).constData());
        QVERIFY2(einer.ganze.first().width()
                     <= a.roll->viewport()->width() / 2,
                 beschreibe(einer).constData());
        QVERIFY(!a.rolltSenkrecht());
        const QRect erste = einer.ganze.first();

        a.panel->zeigeStand(standMit(3, 4));
        Aufbau::beruhige();
        const Gesehen drei = sucheKacheln(a);
        QVERIFY2(drei.ganze.size() == 3, beschreibe(drei).constData());
        QVERIFY2(qAbs(drei.ganze.at(0).top() - drei.ganze.at(2).top()) <= 1,
                 beschreibe(drei).constData());
        // Und die erste steht, wo sie stand: wer mitten im QSO ist,
        // dem springt die Kachel nicht weg, weil ein zweiter ruft.
        QCOMPARE(drei.ganze.first(), erste);
    }

    // Schmal und hoch: eine Spalte, wie bis zum 2026-11-18.
    void schmalUndHochStehenSieUntereinander()
    {
        Aufbau a(standMit(3, 3));
        a.fenster->resize(300, 560);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        const Gesehen g = sucheKacheln(a);
        QVERIFY2(g.ganze.size() == 3, beschreibe(g).constData());
        QVERIFY2(g.angeschnittene.isEmpty(), beschreibe(g).constData());
        QVERIFY2(qAbs(g.ganze.at(0).left() - g.ganze.at(2).left()) <= 1,
                 beschreibe(g).constData());
        QCOMPARE(g.ganze.at(1).top() - g.ganze.at(0).top(),
                 Ft8PlaetzeFlaeche::kKachelH + Ft8PlaetzeFlaeche::kKachelLuft);
        QCOMPARE(g.ganze.at(2).top() - g.ganze.at(1).top(),
                 Ft8PlaetzeFlaeche::kKachelH + Ft8PlaetzeFlaeche::kKachelLuft);
    }

    // Die Groesse, in der das Fenster beim Betreiber steht, mit dem,
    // was er an dem Morgen hatte: zwei von drei. Das muss bleiben,
    // wie es ist -- beide ganz zu sehen, kein Rollbalken.
    void inSeinerGroesseStehenZweiVonDreiWieBisher()
    {
        Aufbau a(standMit(2, 3));
        a.fenster->resize(365, 340);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        const Gesehen g = sucheKacheln(a);
        QVERIFY2(g.ganze.size() == 2, beschreibe(g).constData());
        QVERIFY2(g.angeschnittene.isEmpty(), beschreibe(g).constData());
        QVERIFY(!a.rolltSenkrecht());
    }

    // ── Zu klein: ein Rollbalken, keine verlorene Kachel ──────────
    //
    // Vorher gab es keinen -- das Applet behauptete 150 Punkt
    // Mindesthoehe, der Rollbereich glaubte es, und die dritte Kachel
    // war nicht zu erreichen.
    void zuKleinKommtEinRollbalkenStattEinerVerlorenenKachel()
    {
        Aufbau a(standMit(3, 3));
        a.fenster->resize(300, 220);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        QVERIFY2(a.rolltSenkrecht(),
                 "drei Kacheln untereinander passen nicht in 220 Punkt -- "
                 "ohne Rollbalken ist die dritte nicht zu erreichen");
        const int alleDrei = kKopf + 3 * Ft8PlaetzeFlaeche::kKachelH
                             + 2 * Ft8PlaetzeFlaeche::kKachelLuft;
        QVERIFY2(a.roll->widget()->height() >= alleDrei,
                 qPrintable(QStringLiteral("das Applet ist %1 hoch, drei "
                                           "Kacheln brauchen %2")
                                .arg(a.roll->widget()->height())
                                .arg(alleDrei)));

        // Oben: die erste ganz. Unten: die letzte ganz.
        const Gesehen oben = sucheKacheln(a);
        QVERIFY2(!oben.ganze.isEmpty(), beschreibe(oben).constData());

        a.roll->verticalScrollBar()->setValue(
            a.roll->verticalScrollBar()->maximum());
        Aufbau::beruhige();
        const Gesehen unten = sucheKacheln(a);
        QVERIFY2(!unten.ganze.isEmpty(), beschreibe(unten).constData());
        QVERIFY2(unten.ganze.last().bottom()
                     < a.roll->viewport()->height(),
                 beschreibe(unten).constData());
    }

    // ── Etwa quadratisch: ein Raster ──────────────────────────────
    //
    // Sechs Plaetze, vier belegt. Mehr als eine Spalte UND mehr als
    // eine Zeile, alle vier ganz zu sehen. (Ob zwei oder drei Spalten,
    // haengt an der Schrift des Rechners -- die genaue Zahl prueft
    // `tst_ft8_plaetze_panel::dieSpaltenFolgenDerBreite`.)
    void rasterBeiMittlererGroesse()
    {
        Aufbau a(standMit(4, 6));
        a.fenster->resize(600, 400);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        const Gesehen g = sucheKacheln(a);
        QVERIFY2(g.ganze.size() == 4, beschreibe(g).constData());
        QVERIFY2(g.angeschnittene.isEmpty(), beschreibe(g).constData());
        QVERIFY(!a.rolltSenkrecht());

        QVector<int> linke, obere;
        for (const QRect& r : g.ganze) {
            linke.append(r.left());
            obere.append(r.top());
        }
        QVERIFY2(verschiedene(linke) >= 2, beschreibe(g).constData());
        QVERIFY2(verschiedene(obere) >= 2, beschreibe(g).constData());
        for (int i = 0; i < g.ganze.size(); ++i) {
            for (int j = i + 1; j < g.ganze.size(); ++j) {
                QVERIFY2(!g.ganze.at(i).intersects(g.ganze.at(j)),
                         beschreibe(g).constData());
            }
        }
    }

    // ── Beim Ziehen am Rand ───────────────────────────────────────
    //
    // Von schmal nach breit und wieder zurueck, in kleinen Schritten.
    // Bei JEDER Breite gilt: entweder sind alle drei Kacheln ganz zu
    // sehen, oder es steht ein Rollbalken da. Nie eine angeschnittene
    // Kachel ohne Weg zu ihr.
    //
    // Und der Rueckweg sieht aus wie der Hinweg. Das ist nicht
    // selbstverstaendlich: der Rollbalken nimmt dem Inhalt zehn Punkt
    // Breite, und in einem Band von genau dieser Breite gibt es zwei
    // stimmige Zustaende -- drei nebeneinander ohne Balken, oder
    // weniger Spalten mit. Wer nur mit der Breite rechnet, die er
    // gerade hat, bleibt im zweiten haengen, sobald er von der
    // schmalen Seite kommt.
    void beimZiehenOrdnetEsSichNeuUndBleibtNichtHaengen()
    {
        Aufbau a(standMit(3, 3));
        a.fenster->resize(300, 210);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        constexpr int kVon = 300;
        constexpr int kBis = 1000;
        constexpr int kSchritt = 7;

        QVector<int> hinweg;   // Zahl der ganz sichtbaren Kacheln
        QVector<bool> hinwegRollt;
        for (int b = kVon; b <= kBis; b += kSchritt) {
            a.zieheAuf(b, 210);
            const Gesehen g = sucheKacheln(a);
            const QByteArray wo = QStringLiteral("Breite %1: ").arg(b)
                                      .toUtf8() + beschreibe(g);
            QVERIFY2(a.rolltSenkrecht()
                         || (g.ganze.size() == 3
                             && g.angeschnittene.isEmpty()),
                     wo.constData());
            hinweg.append(int(g.ganze.size()));
            hinwegRollt.append(a.rolltSenkrecht());
        }
        // Am breiten Ende stehen alle drei da, ohne Balken.
        QCOMPARE(hinweg.last(), 3);
        QVERIFY(!hinwegRollt.last());
        // Und am schmalen rollt es.
        QVERIFY(hinwegRollt.first());

        int i = hinweg.size() - 1;
        for (int b = kVon + i * kSchritt; b >= kVon; b -= kSchritt, --i) {
            a.zieheAuf(b, 210);
            const Gesehen g = sucheKacheln(a);
            const QByteArray wo =
                QStringLiteral("Rueckweg, Breite %1 (Hinweg: %2 ganze, "
                               "Balken %3): ")
                    .arg(b).arg(hinweg.at(i)).arg(hinwegRollt.at(i))
                    .toUtf8() + beschreibe(g);
            QVERIFY2(a.rolltSenkrecht() == hinwegRollt.at(i), wo.constData());
            QVERIFY2(int(g.ganze.size()) == hinweg.at(i), wo.constData());
        }
    }

    // ── Der leere Hinweis bricht um ───────────────────────────────
    //
    // „Nothing running. …" ist ein Absatz. In einem schmalen Fenster
    // braucht er mehr Zeilen -- und wenn die nicht hineinpassen, einen
    // Rollbalken, statt unten abgeschnitten zu enden.
    void derLeereHinweisBrichtUmUndGehtNichtVerloren()
    {
        Aufbau a{Ft8PlaetzeStand{}};
        a.fenster->resize(150, 150);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        QLabel* hinweis = a.panel->findChild<QLabel*>();
        QVERIFY(hinweis);
        QVERIFY(hinweis->isVisible());
        QVERIFY(hinweis->wordWrap());
        const int braucht = hinweis->heightForWidth(hinweis->width());
        // Der Fall muss ueberhaupt etwas pruefen: der Absatz braucht in
        // dieser Breite mehr Hoehe, als das Fenster zeigt.
        QVERIFY2(braucht > a.roll->viewport()->height(),
                 qPrintable(QStringLiteral("der Hinweis braucht %1, der "
                                           "Sichtbereich hat %2 -- so "
                                           "prueft dieser Fall nichts")
                                .arg(braucht)
                                .arg(a.roll->viewport()->height())));
        QVERIFY2(a.rolltSenkrecht(),
                 "der Absatz passt nicht hinein, also muss es rollen");
        QVERIFY2(hinweis->height() >= braucht,
                 qPrintable(QStringLiteral("der Hinweis ist %1 hoch und "
                                           "braucht %2 -- der Rest ist "
                                           "abgeschnitten")
                                .arg(hinweis->height()).arg(braucht)));

        // In seiner gewohnten Groesse passt er ohne Rollbalken.
        a.zieheAuf(365, 340);
        QVERIFY(!a.rolltSenkrecht());
        QVERIFY(hinweis->height()
                >= hinweis->heightForWidth(hinweis->width()));
    }
};

QTEST_MAIN(TstFt8PlaetzeAnordnung)
#include "tst_ft8_plaetze_anordnung.moc"
