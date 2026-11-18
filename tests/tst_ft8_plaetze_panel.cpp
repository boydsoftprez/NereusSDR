// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_ft8_plaetze_panel.cpp  (Longpath)
// =================================================================
//
// Das Plaetze-Panel: was es zeigt, und DASS es etwas zu zeigen
// bekommt.
//
// Der zweite Teil ist der wichtigere. `Ft8Mehrfach::belegung()`
// rechnete die Belegung seit dem 2026-10-24 aus, und niemand las sie
// -- mit `slots 3` gingen drei Signale hinaus und der Betreiber sah
// keines davon. Eine Pruefung, die nur `baueStand()` aufruft, haette
// das nie gefunden; sie muss durch das SIGNAL gehen.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/applets/Ft8Applet.h"
#include "gui/applets/Ft8PlaetzeApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstFt8PlaetzePanel : public QObject
{
    Q_OBJECT

private:
    static QVector<Ft8Decode> einFund(const QString& text, float snr,
                                      float hz)
    {
        Ft8Decode d;
        d.snrDb = snr; d.dtSeconds = 0.2f; d.freqHz = hz;
        d.score = 40;  d.text = text;
        return {d};
    }

private slots:
    void initTestCase()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                         QStringLiteral("JN67"));
    }

    // ── Die Zuege heiszen, wie sie im Fenster heiszen ─────────────
    //
    // NICHT `zustandName()` -- das sind die technischen Namen
    // („SendeRReport"), gut fuer Prueflaeufe und zu lang fuer eine
    // Pille von 48 Punkten.
    void dieZuegeTragenDieNamenAusDemBlatt()
    {
        using Z = Ft8Ablauf::Zustand;
        QCOMPARE(Ft8Applet::zugKurz(Z::RufeCq),       QStringLiteral("CQ"));
        QCOMPARE(Ft8Applet::zugKurz(Z::RufeAn),       QStringLiteral("Tx1"));
        QCOMPARE(Ft8Applet::zugKurz(Z::SendeReport),  QStringLiteral("Tx2"));
        QCOMPARE(Ft8Applet::zugKurz(Z::SendeRReport), QStringLiteral("Tx3"));
        QCOMPARE(Ft8Applet::zugKurz(Z::SendeRR73),    QStringLiteral("RR73"));
        QCOMPARE(Ft8Applet::zugKurz(Z::Sende73),      QStringLiteral("73"));
        QVERIFY(Ft8Applet::zugKurz(Z::Ruht).isEmpty());
    }

    // Die fuenf Striche: ein QSO hat Stufen, keinen Verlauf.
    void dieStufeWaechstMitDemQso()
    {
        using Z = Ft8Ablauf::Zustand;
        QCOMPARE(Ft8Applet::stufeFuer(Z::RufeCq),       0);
        QCOMPARE(Ft8Applet::stufeFuer(Z::RufeAn),       1);
        QCOMPARE(Ft8Applet::stufeFuer(Z::SendeReport),  2);
        QCOMPARE(Ft8Applet::stufeFuer(Z::SendeRReport), 3);
        QCOMPARE(Ft8Applet::stufeFuer(Z::SendeRR73),    4);
        QCOMPARE(Ft8Applet::stufeFuer(Z::Fertig),       5);
    }

    // ── Der ANSCHLUSS: vom Fund bis ins Panel ─────────────────────
    //
    // Die gemessene Szene vom 2026-10-30: HB9ZHK im QSO, PD4RJ und
    // 9A6T rufen dazwischen.
    void einLaufendesQsoUndZweiWartendeKommenImPanelAn()
    {
        RadioModel radio;
        Ft8Applet quelle(&radio);
        Ft8PlaetzeApplet panel(&radio);
        QObject::connect(&quelle, &Ft8Applet::plaetzeGeaendert,
                         &panel,  &Ft8PlaetzeApplet::zeigeStand);
        QSignalSpy spion(&quelle, &Ft8Applet::plaetzeGeaendert);

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(7074000.0);
        quelle.setSlice(slice);

        quelle.ablaufForTestSchreibend().starteMitAnrufer(
            QStringLiteral("HB9ZHK"), QStringLiteral("OE5SOS"),
            QStringLiteral("JN67"), 18, QStringLiteral("JN47"));

        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);
        for (const QString& t : {QStringLiteral("OE5SOS PD4RJ JO33"),
                                 QStringLiteral("OE5SOS 9A6T")}) {
            QMetaObject::invokeMethod(
                &quelle, "onDecoded", Qt::DirectConnection,
                Q_ARG(QVector<Longpath::Ft8Decode>, einFund(t, 12.0f, 1338.0f)),
                Q_ARG(QDateTime, fenster));
        }
        quelle.zeigeAblaufForTest();

        QVERIFY2(spion.count() > 0,
                 "das Panel bekommt nichts -- genau so lag "
                 "Ft8Mehrfach::belegung() monatelang unerreichbar da");

        const Ft8PlaetzeStand& st = panel.flaecheForTest()->stand();
        QCOMPARE(st.plaetze.size(), 1);
        QCOMPARE(st.plaetze.first().partner, QStringLiteral("HB9ZHK"));
        QCOMPARE(st.plaetze.first().zug,     QStringLiteral("Tx2"));
        QCOMPARE(st.plaetze.first().stufe,   2);
        QCOMPARE(st.plaetze.first().gesendet, 18);

        QCOMPARE(st.wartende.size(), 2);
        QCOMPARE(st.wartende.at(0).partner, QStringLiteral("PD4RJ"));
        QCOMPARE(st.wartende.at(1).partner, QStringLiteral("9A6T"));

        // ── Ein Blatt davon, wenn eines verlangt wird ────────────
        //
        // Der Betreiber hat dieses Panel GEFUELLT noch nie gesehen
        // (sieben Gruende, alle am 2026-10-30). Hier steht der
        // Zustand, den er sehen will, also kann hier auch das Bild
        // entstehen.
        //
        // NUR bei gesetztem `LONGPATH_GRAB_OUT`. Ohne die Variable
        // passiert nichts -- am 2026-10-15 ist ein Abgriff-Lauf in
        // der CI rot geworden, weil er ohne sie trotzdem malen
        // wollte. Der Fall oben bleibt davon unberuehrt.
        const QByteArray ziel = qgetenv("LONGPATH_GRAB_OUT");
        if (!ziel.isEmpty()) {
            panel.resize(360, panel.sizeHint().height());
            panel.show();
            QVERIFY(QTest::qWaitForWindowExposed(&panel, 20000));
            const QPixmap bild = panel.grab();
            const QString weg = QString::fromLocal8Bit(ziel);
            QVERIFY2(bild.save(weg), qPrintable(weg));
            qInfo().noquote() << "Blatt abgelegt:" << weg
                              << bild.width() << "x" << bild.height();
        }
    }

    // Leer heisst leer: ohne Plaetze und ohne Wartende steht der
    // Hinweis da, nicht eine leere Flaeche.
    void ohneAllesStehtDerHinweisStattEinerLeerenFlaeche()
    {
        RadioModel radio;
        Ft8PlaetzeApplet panel(&radio);
        panel.zeigeStand(Ft8PlaetzeStand{});
        QVERIFY(!panel.flaecheForTest()->isVisible());

        Ft8PlaetzeStand st;
        Ft8PlatzStand z;
        z.partner = QStringLiteral("DL1ABC");
        st.plaetze.append(z);
        panel.zeigeStand(st);
        QVERIFY(panel.flaecheForTest()->stand().plaetze.size() == 1);
    }

    // ── Wann es sich von selbst zeigen soll ───────────────────────
    //
    // Betreiber am 2026-10-30: „waere schoen, wenn man das immer
    // siehst, sobald ...". `hatInhalt()` ist die eine Regel dafuer --
    // das Panel fragt sie (Hinweis oder Kacheln) und MainWindow
    // fragt sie (aufmachen oder nicht). Zwei Stellen mit derselben
    // Regel laufen auseinander.
    void hatInhaltIstDieEineRegel()
    {
        Ft8PlaetzeStand leer;
        QVERIFY2(!hatInhalt(leer), "ohne alles hat es nichts zu zeigen");

        // Ein laufendes QSO reicht -- auch mit einem einzigen Platz.
        Ft8PlaetzeStand mitQso;
        mitQso.plaetzeGesamt = 1;
        Ft8PlatzStand z;
        z.partner = QStringLiteral("HB9ZHK");
        mitQso.plaetze.append(z);
        QVERIFY(hatInhalt(mitQso));

        // Und ein Wartender allein reicht AUCH -- genau der Fall vom
        // 2026-10-30, als F5PZT und OE3TVJ im Protokoll standen und
        // nirgends sonst.
        Ft8PlaetzeStand nurWartend;
        nurWartend.wartende.append(
            Ft8WartenderStand{QStringLiteral("F5PZT"), QString(), 9});
        QVERIFY2(hatInhalt(nurWartend),
                 "ein Wartender ohne laufendes QSO ist auch Inhalt");
    }

    // ── Das Fenster darf nicht als Streifen aufgehen ──────────────
    //
    // `AppletFloatingWindow::applyDefaultSize()` fragt `sizeHint()`,
    // NICHT `minimumHeight()`. Und der Vorgabe-sizeHint eines
    // Applets ist die Summe seiner Anordnung, in der ein VERSTECKTES
    // Widget gar nicht mitzaehlt -- im leeren Zustand ist die
    // Flaeche versteckt.
    //
    // Der Betreiber bekam darum am 2026-10-30 eine Titelleiste von
    // zwanzig Punkten ohne Inhalt: „ein eigenes fenster erscheint
    // hier aber nicht", und spaeter „die einzelnen qso fenster habe
    // ich bis dato nie gesehen".
    void derSizeHintTraegtZweiKacheln()
    {
        RadioModel radio;
        Ft8PlaetzeApplet panel(&radio);

        // Leer -- und trotzdem gross genug fuer ein Fenster.
        panel.zeigeStand(Ft8PlaetzeStand{});
        QVERIFY2(panel.sizeHint().height()
                     >= 2 * Ft8PlaetzeFlaeche::kKachelH,
                 qPrintable(QStringLiteral("leer: %1 Punkte")
                                .arg(panel.sizeHint().height())));
        QVERIFY(panel.sizeHint().width() >= 300);

        // Mit drei Kacheln waechst er.
        Ft8PlaetzeStand st;
        for (int i = 0; i < 3; ++i) { st.plaetze.append(Ft8PlatzStand{}); }
        panel.zeigeStand(st);
        QVERIFY2(panel.sizeHint().height()
                     >= 3 * Ft8PlaetzeFlaeche::kKachelH,
                 qPrintable(QStringLiteral("drei Plaetze: %1 Punkte")
                                .arg(panel.sizeHint().height())));
    }

    // Das Panel waechst mit der Zahl der Plaetze, statt eine feste
    // Hoehe zu behaupten -- bei sechs Plaetzen waere die Haelfte
    // abgeschnitten.
    void diePanelhoeheWaechstMitDenPlaetzen()
    {
        Ft8PlaetzeFlaeche f;
        Ft8PlaetzeStand st;
        f.setzeStand(st);
        const int leer = f.noetigeHoehe();

        for (int i = 0; i < 3; ++i) { st.plaetze.append(Ft8PlatzStand{}); }
        f.setzeStand(st);
        const int drei = f.noetigeHoehe();
        QVERIFY2(drei > leer, "drei Plaetze brauchen mehr Hoehe als keiner");

        for (int i = 0; i < 3; ++i) { st.plaetze.append(Ft8PlatzStand{}); }
        f.setzeStand(st);
        QVERIFY2(f.noetigeHoehe() > drei,
                 "sechs Plaetze brauchen mehr Hoehe als drei");
        // Und zwar genau drei Kacheln mehr.
        QCOMPARE(f.noetigeHoehe() - drei,
                 3 * (Ft8PlaetzeFlaeche::kKachelH
                      + Ft8PlaetzeFlaeche::kKachelLuft));
    }

    // ── Freie Plaetze stehen auch da (Entwurf B, 2026-11-13) ─────────
    //
    // Betreiber mit `slots 3` vor einem Fenster, das „1 of 3" und EINE
    // Kachel zeigte: „schön wäre es, wenn man dann auch alle 3 sieht,
    // sprich 3 fenster und ein fenster durch 3 geteilt."
    //
    // Aus drei Blaettern waehlte er B: belegte Plaetze bleiben die
    // volle Kachel, freie kosten nur einen Streifen. Der Unterschied
    // zu A faellt bei `slots 6` ins Gewicht -- dort stuenden sonst
    // fuenf leere Kacheln zu je 92 Punkten.
    void freiePlaetzeStehenAlsStreifenDa()
    {
        Ft8PlaetzeFlaeche f;
        Ft8PlaetzeStand st;
        // ZWEI belegte, nicht einer: mit einem liegt die Hoehe noch
        // unter der Untergrenze von 150 (dem leeren Hinweistext in
        // lesbarer Breite), `qMax` greift, und gemessen waere dann die
        // Grenze statt der Streifen. Erst beim zweiten Platz rechnet
        // die Funktion wirklich.
        st.plaetze.append(Ft8PlatzStand{});
        st.plaetze.append(Ft8PlatzStand{});
        st.plaetzeGesamt = 2;
        f.setzeStand(st);
        const int zweiOhneFreie = f.noetigeHoehe();
        QVERIFY2(zweiOhneFreie > 150, "die Untergrenze greift noch — "
                                      "der Vergleich misst sie statt der Streifen");

        // Dieselben zwei Belegten, aber vier eingestellt: zwei
        // Streifen kommen dazu.
        st.plaetzeGesamt = 4;
        f.setzeStand(st);
        const int vierEingestellt = f.noetigeHoehe();
        QVERIFY2(vierEingestellt > zweiOhneFreie,
                 "bei slots 4 muessen die zwei freien Plaetze zu sehen sein");
        QCOMPARE(vierEingestellt - zweiOhneFreie,
                 2 * (Ft8PlaetzeFlaeche::kFreiH
                      + Ft8PlaetzeFlaeche::kKachelLuft));

        // Und ein Streifen ist DEUTLICH schmaler als eine Kachel --
        // das ist der ganze Unterschied zwischen Entwurf B und A.
        QVERIFY2(Ft8PlaetzeFlaeche::kFreiH * 2 < Ft8PlaetzeFlaeche::kKachelH,
                 "ein freier Streifen soll nicht fast so hoch sein wie "
                 "eine belegte Kachel");
    }

    // GEGENPROBE: ein leeres Panel geht deswegen nicht von selbst auf.
    //
    // `hatInhalt()` bleibt unberuehrt. Ein Fenster, das nur „3x free"
    // zeigt, hat nichts zu sagen -- die Streifen erscheinen erst,
    // wenn ohnehin etwas laeuft.
    void leereePlaetzeOeffnenDasPanelNicht()
    {
        Ft8PlaetzeStand st;
        st.plaetzeGesamt = 6;        // sechs eingestellt ...
        QVERIFY2(!hatInhalt(st), "sechs FREIE Plaetze sind kein Inhalt");

        st.plaetze.append(Ft8PlatzStand{});   // ... einer belegt
        QVERIFY(hatInhalt(st));
    }

    // ══ Die Anordnung folgt der Groesse (2026-11-18) ══════════════
    //
    // Betreiber: „FT8 SLOTS sollte man auch nebeneinander anordnen
    // können und soll sich automatsich an die Größe des Fenster
    // anpassen!"
    //
    // Diese Faelle rechnen -- sie fragen `anordnungFuer()` nach den
    // Rechtecken, in die gemalt wird. Dass im BILD dasselbe steht,
    // prueft `tst_ft8_plaetze_anordnung` am echten Fenster.
    //
    // Alle Breiten haengen an `kachelMindestbreite()`: die kommt aus
    // der Schrift, und die ist auf dem Mac, unter Linux und unter
    // Windows je eine andere. Eine feste Zahl waere auf einem davon
    // falsch.

    // So viele Spalten, wie Kacheln in ihrer Mindestbreite passen --
    // und auf den Punkt genau: ein Punkt weniger, eine Spalte weniger.
    void dieSpaltenFolgenDerBreite()
    {
        Ft8PlaetzeFlaeche f;
        f.setzeStand(standMit(3, 3));
        const int k = f.kachelMindestbreite();

        for (int n = 1; n <= 3; ++n) {
            QCOMPARE(f.anordnungFuer(breiteFuer(k, n), 2000).spalten, n);
            QCOMPARE(f.anordnungFuer(breiteFuer(k, n) - 1, 2000).spalten,
                     qMax(1, n - 1));
        }
        // Mehr Spalten als Plaetze gibt es nicht, so breit das Fenster
        // auch ist -- die Kacheln teilen sich dann die Breite.
        const Ft8PlaetzeAnordnung breit =
            f.anordnungFuer(breiteFuer(k, 8), 2000);
        QCOMPARE(breit.spalten, 3);
        QCOMPARE(breit.kacheln.size(), 3);
        QCOMPARE(breit.kacheln.at(0).top(), breit.kacheln.at(2).top());
        QVERIFY2(breit.kacheln.at(2).right()
                     <= breiteFuer(k, 8) - Ft8PlaetzeFlaeche::kRandX,
                 "die letzte Kachel ragt ueber den rechten Rand");

        // Schmal: eine Spalte, untereinander -- wie bis zum 2026-11-18.
        const Ft8PlaetzeAnordnung schmal =
            f.anordnungFuer(breiteFuer(k, 1), 2000);
        QCOMPARE(schmal.spalten, 1);
        QCOMPARE(schmal.kacheln.at(1).top() - schmal.kacheln.at(0).top(),
                 Ft8PlaetzeFlaeche::kKachelH + Ft8PlaetzeFlaeche::kKachelLuft);
        QCOMPARE(schmal.kacheln.at(0).left(), schmal.kacheln.at(2).left());
    }

    // Breit und niedrig braucht EINE Zeile Hoehe, nicht drei. Das war
    // der Fall, um den es ging: rechts alles leer, die dritte Kachel
    // unten hinaus.
    void breitUndNiedrigBrauchtNurEineZeile()
    {
        Ft8PlaetzeFlaeche f;
        f.setzeStand(standMit(3, 3));
        const int k = f.kachelMindestbreite();

        // Knapp: Kopf, Kachel, vier Punkt darunter. Die Luft, die der
        // Groessenhinweis laesst, zaehlt hier nicht -- nach dieser Zahl
        // entscheidet sich, ob ein Rollbalken kommt.
        const int eineZeile = Ft8PlaetzeFlaeche::kKopfH
                              + Ft8PlaetzeFlaeche::kKachelH
                              + Ft8PlaetzeFlaeche::kFussH;
        QCOMPARE(f.pflichtHoehe(breiteFuer(k, 3)), eineZeile);
        QCOMPARE(f.pflichtHoehe(breiteFuer(k, 1)),
                 eineZeile + 2 * (Ft8PlaetzeFlaeche::kKachelH
                                  + Ft8PlaetzeFlaeche::kKachelLuft));
    }

    // ── Das Raster gehoert den PLAETZEN ───────────────────────────
    //
    // Bei `slots 3` und zwei Partnern sind es drei Zellen, nicht
    // zwei: der dritte Platz steht als Streifen in seiner. Ruft der
    // dritte, kommt seine Kachel genau dorthin -- und die beiden
    // anderen bewegen sich nicht. Hinge die Spaltenzahl an den
    // BELEGTEN, spraengen beide von der halben auf ein Drittel der
    // Breite, mitten im QSO.
    void derDrittePlatzKommtDorthinWoFreeStand()
    {
        Ft8PlaetzeFlaeche f;
        f.setzeStand(standMit(2, 3));
        const int b = breiteFuer(f.kachelMindestbreite(), 3);

        const Ft8PlaetzeAnordnung zwei = f.anordnungFuer(b, 2000);
        QCOMPARE(zwei.spalten, 3);
        QCOMPARE(zwei.kacheln.size(), 2);
        QCOMPARE(zwei.freie.size(), 1);
        QCOMPARE(zwei.freie.first().top(), zwei.kacheln.first().top());
        QVERIFY(zwei.freie.first().left() > zwei.kacheln.at(1).right());
        QCOMPARE(zwei.freie.first().height(), Ft8PlaetzeFlaeche::kFreiH);

        // Ein freier Platz NEBEN einer Kachel kostet keine Hoehe --
        // also weicht er auch nicht, wenn es eng wird.
        QCOMPARE(f.pflichtHoehe(b), f.wunschHoehe(b));
        QCOMPARE(f.anordnungFuer(b, 0).freie.size(), 1);

        f.setzeStand(standMit(3, 3));
        const Ft8PlaetzeAnordnung drei = f.anordnungFuer(b, 2000);
        QCOMPARE(drei.kacheln.at(0), zwei.kacheln.at(0));
        QCOMPARE(drei.kacheln.at(1), zwei.kacheln.at(1));
        QCOMPARE(drei.kacheln.at(2).left(),  zwei.freie.first().left());
        QCOMPARE(drei.kacheln.at(2).width(), zwei.freie.first().width());
        QVERIFY(drei.freie.isEmpty());
    }

    // Sein Bild vom 2026-11-18, rund 1040 x 340 mit „1 of 4": eine
    // Kachel ueber die ganze Breite, drei „free" darunter. „beim in
    // die länge ziehen nach rechts sollen dann auch die fenster sich
    // nebeneinander setzen." Vier Plaetze, vier Zellen, eine Zeile.
    void einerVonVierStehtMitDreiFreienInEinerZeile()
    {
        Ft8PlaetzeFlaeche f;
        f.setzeStand(standMit(1, 4));
        const int b = breiteFuer(f.kachelMindestbreite(), 4);

        const Ft8PlaetzeAnordnung a = f.anordnungFuer(b, 2000);
        QCOMPARE(a.spalten, 4);
        QCOMPARE(a.kacheln.size(), 1);
        QCOMPARE(a.freie.size(), 3);
        for (const QRect& r : a.freie) {
            QCOMPARE(r.top(), a.kacheln.first().top());
            QCOMPARE(r.width(), a.kacheln.first().width());
        }
        // Eine Zeile Hoehe -- auch wenn es eng wird, bleiben die drei.
        QCOMPARE(f.pflichtHoehe(b), f.wunschHoehe(b));
        QCOMPARE(f.anordnungFuer(b, 0).freie.size(), 3);
    }

    // ── Belegte vor freien ────────────────────────────────────────
    //
    // Reicht die Hoehe nicht fuer alles, weichen die Zeilen aus
    // lauter freien Plaetzen: erst ruecken sie zusammen, dann sind sie
    // weg. Die Kacheln stehen in allen drei Faellen an derselben
    // Stelle.
    void freieWeichenVorDenBelegten()
    {
        using Art = Ft8PlaetzeAnordnung::Freie;
        Ft8PlaetzeFlaeche f;
        f.setzeStand(standMit(2, 6));
        // Eine Spalte: zwei Kacheln, darunter vier Streifen.
        const int b       = breiteFuer(f.kachelMindestbreite(), 1);
        const int wunsch  = f.wunschHoehe(b);
        const int pflicht = f.pflichtHoehe(b);
        const int streifen = Ft8PlaetzeFlaeche::kFreiH
                             + Ft8PlaetzeFlaeche::kKachelLuft;
        QCOMPARE(wunsch - pflicht, 4 * streifen);

        const Ft8PlaetzeAnordnung voll = f.anordnungFuer(b, wunsch);
        QCOMPARE(voll.freieArt, Art::Voll);
        QCOMPARE(voll.freie.size(), 4);
        QCOMPARE(voll.hoehe, wunsch);
        // Untereinander, in voller Breite -- Entwurf B vom 2026-11-13.
        QCOMPARE(voll.freie.at(0).left(), voll.freie.at(3).left());
        QCOMPARE(voll.freie.at(0).width(), voll.kacheln.at(0).width());

        // Ein Punkt zu wenig: sie ruecken zusammen. Alle vier sind
        // noch da, und es kostet weniger Hoehe.
        const Ft8PlaetzeAnordnung dicht = f.anordnungFuer(b, wunsch - 1);
        QCOMPARE(dicht.freieArt, Art::Dicht);
        QCOMPARE(dicht.freie.size(), 4);
        QVERIFY2(dicht.hoehe < wunsch,
                 "zusammengerueckt muss es weniger Hoehe kosten");
        QVERIFY2(dicht.hoehe <= wunsch - 1, "und in die Hoehe passen");
        QCOMPARE(dicht.freie.at(0).top(), dicht.freie.at(1).top());
        QVERIFY(dicht.freie.at(1).left() > dicht.freie.at(0).right());

        // Auch dafuer zu wenig: die Streifen sind weg, keine Kachel.
        const Ft8PlaetzeAnordnung weg = f.anordnungFuer(b, dicht.hoehe - 1);
        QCOMPARE(weg.freieArt, Art::Gewichen);
        QVERIFY(weg.freie.isEmpty());
        QCOMPARE(weg.hoehe, pflicht);

        QCOMPARE(dicht.kacheln, voll.kacheln);
        QCOMPARE(weg.kacheln,   voll.kacheln);
        QCOMPARE(weg.kacheln.size(), 2);

        // Und NOCH weniger Hoehe aendert nichts mehr: die Kacheln
        // bleiben ganz, der Gastgeber rollt.
        QCOMPARE(f.anordnungFuer(b, 40).kacheln, voll.kacheln);
    }

    // ── Nichts ueberlappt, nichts ragt hinaus ─────────────────────
    //
    // Quer durch alle Belegungen von einem bis sechs Plaetzen, viele
    // Breiten und drei Hoehen. Kacheln sind IMMER vollzaehlig und nie
    // schmaler als ihre Mindestbreite (solange das Panel ueberhaupt
    // eine Kachel fasst).
    void nichtsUeberlapptUndNichtsRagtHinaus()
    {
        Ft8PlaetzeFlaeche f;
        for (int gesamt = 1; gesamt <= 6; ++gesamt) {
            for (int belegt = 0; belegt <= gesamt; ++belegt) {
                for (int wartende : {0, 3}) {
                    if (belegt == 0 && wartende == 0) { continue; }
                    f.setzeStand(standMit(belegt, gesamt, wartende));
                    const int k = f.kachelMindestbreite();
                    for (int breite = f.mindestBreite(); breite <= 1400;
                         breite += 41) {
                        for (int hoehe : {0, f.pflichtHoehe(breite) + 45,
                                          4000}) {
                            pruefeAnordnung(f, breite, hoehe, belegt,
                                            gesamt, k);
                            if (QTest::currentTestFailed()) { return; }
                        }
                    }
                }
            }
        }
    }

    // ── Die Mindestbreite kommt aus dem Inhalt ────────────────────
    //
    // Ein gewoehnliches Rufzeichen aendert sie nicht (sonst spraenge
    // das Raster mit jedem Partner um); ein zusammengesetztes macht
    // die Kachel breiter, statt abgeschnitten zu werden.
    void einLangesRufzeichenMachtDieKachelBreiter()
    {
        Ft8PlaetzeFlaeche f;
        f.setzeStand(Ft8PlaetzeStand{});
        const int grund = f.kachelMindestbreite();

        Ft8PlaetzeStand st;
        st.plaetzeGesamt = 2;
        Ft8PlatzStand z;
        z.partner = QStringLiteral("K1AB");
        z.land    = QStringLiteral("Japan");
        z.tonHz   = 2710;
        st.plaetze.append(z);
        f.setzeStand(st);
        QCOMPARE(f.kachelMindestbreite(), grund);
        QCOMPARE(f.anordnungFuer(breiteFuer(grund, 2), 2000).spalten, 2);

        st.plaetze[0].partner = QStringLiteral("VP2V/DL1ABC/QRP");
        f.setzeStand(st);
        const int lang = f.kachelMindestbreite();
        QVERIFY2(lang > grund,
                 qPrintable(QStringLiteral("lang %1, grund %2")
                                .arg(lang).arg(grund)));
        // Bei der Breite, in der eben noch zwei passten, steht jetzt
        // eine Spalte -- und die Kachel ist breit genug fuer den Ruf.
        const Ft8PlaetzeAnordnung a =
            f.anordnungFuer(breiteFuer(grund, 2), 2000);
        QCOMPARE(a.spalten, 1);
        QVERIFY(a.kacheln.first().width() >= lang);

        // Der Rapport zaehlt auch: „sent -05   rcvd +12" steht in
        // einer Zeile und darf nicht am Rand enden.
        st.plaetze[0].partner = QStringLiteral("K1AB");
        st.plaetze[0].gesendet = -5;
        st.plaetze[0].empfangen = 12;
        f.setzeStand(st);
        QVERIFY(f.kachelMindestbreite() >= grund);
    }

    // ── Das Applet verlangt, was nie abgeschnitten werden darf ────
    //
    // Bis zum 2026-11-18 stand seine Mindesthoehe fest auf 150, und
    // eine von Hand gesetzte Mindesthoehe schlaegt die der Anordnung.
    // Der Rollbereich des Fensters sah also 150 und rollte nie -- die
    // dritte Kachel lief unten hinaus.
    void dasAppletVerlangtDiePflichthoehe()
    {
        RadioModel radio;
        Ft8PlaetzeApplet panel(&radio);
        panel.zeigeStand(standMit(3, 3));
        Ft8PlaetzeFlaeche* f = panel.flaecheForTest();
        const int k = f->kachelMindestbreite();

        // Schmal: drei untereinander.
        panel.resize(breiteFuer(k, 1), 600);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel, 20000));
        QTRY_COMPARE_WITH_TIMEOUT(panel.minimumHeight(),
                                  f->pflichtHoehe(panel.width()), 3000);
        QCOMPARE(f->anordnung().spalten, 1);
        QVERIFY2(panel.minimumHeight()
                     >= 3 * Ft8PlaetzeFlaeche::kKachelH,
                 "drei Kacheln untereinander brauchen ihre Hoehe");

        // Breit: eine Zeile -- die Untergrenze von 150 traegt.
        panel.resize(breiteFuer(k, 3), 600);
        QTRY_COMPARE_WITH_TIMEOUT(panel.minimumHeight(),
                                  int(Ft8PlaetzeApplet::kMindestH), 3000);

        // Die Flaeche bekommt die ganze Hoehe des Applets: nur so
        // weiss sie, ob die freien Plaetze hineinpassen.
        QTRY_COMPARE_WITH_TIMEOUT(f->height(), panel.height(), 3000);
        QCOMPARE(f->anordnung().kacheln.size(), 3);
        QCOMPARE(f->anordnung().spalten, 3);
    }

private:
    // ── Was die Faelle oben gemeinsam brauchen ────────────────────

    /// `belegt` von `gesamt` Plaetzen, dazu `wartende` in der Schlange.
    static Ft8PlaetzeStand standMit(int belegt, int gesamt, int wartende = 0)
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
        for (int i = 0; i < wartende; ++i) {
            s.wartende.append(Ft8WartenderStand{
                QString::fromLatin1(quellen[5 - i % 6].ruf),
                QString::fromLatin1(quellen[5 - i % 6].land), 3 * i - 7});
        }
        return s;
    }

    /// Die schmalste Flaeche, in der `n` Kacheln der Mindestbreite `k`
    /// nebeneinander stehen.
    static int breiteFuer(int k, int n)
    {
        return 2 * Ft8PlaetzeFlaeche::kRandX + n * k
               + (n - 1) * Ft8PlaetzeFlaeche::kKachelLuft;
    }

    static void pruefeAnordnung(const Ft8PlaetzeFlaeche& f, int breite,
                                int hoehe, int belegt, int gesamt, int k)
    {
        const Ft8PlaetzeAnordnung a = f.anordnungFuer(breite, hoehe);
        const QByteArray wo = QStringLiteral("%1 von %2 bei %3 x %4")
            .arg(belegt).arg(gesamt).arg(breite).arg(hoehe).toUtf8();

        QVERIFY2(a.kacheln.size() == belegt, wo.constData());
        QVERIFY2(a.freie.size() <= gesamt - belegt, wo.constData());
        QVERIFY2(a.spalten >= 1 && a.spalten <= qMax(1, gesamt),
                 wo.constData());

        QVector<QRect> alle = a.kacheln;
        alle += a.freie;
        alle += a.wartende;
        const int links  = Ft8PlaetzeFlaeche::kRandX;
        const int rechts = breite - Ft8PlaetzeFlaeche::kRandX;
        for (int i = 0; i < alle.size(); ++i) {
            const QRect& r = alle.at(i);
            QVERIFY2(r.left() >= links && r.right() < rechts,
                     wo.constData());
            QVERIFY2(r.top() >= Ft8PlaetzeFlaeche::kKopfH
                         && r.bottom() < a.hoehe, wo.constData());
            for (int j = i + 1; j < alle.size(); ++j) {
                QVERIFY2(!r.intersects(alle.at(j)), wo.constData());
            }
        }
        for (const QRect& r : a.kacheln) {
            QVERIFY2(r.width() >= k, wo.constData());
            QVERIFY2(r.height() == Ft8PlaetzeFlaeche::kKachelH,
                     wo.constData());
        }
        // Die Wartenden stehen unter allem anderen.
        if (a.wartendeY >= 0) {
            for (const QRect& r : a.kacheln + a.freie) {
                QVERIFY2(r.bottom() < a.wartendeY, wo.constData());
            }
        }
        // Passt alles in die Hoehe, ist auch alles da.
        if (hoehe >= f.wunschHoehe(breite)) {
            QVERIFY2(a.freie.size() == gesamt - belegt, wo.constData());
        }
        // Und nie mehr Hoehe als die Pflicht, wenn die Hoehe fehlt.
        if (hoehe < f.pflichtHoehe(breite)) {
            QVERIFY2(a.hoehe == f.pflichtHoehe(breite), wo.constData());
        }
    }
};

QTEST_MAIN(TstFt8PlaetzePanel)
#include "tst_ft8_plaetze_panel.moc"
