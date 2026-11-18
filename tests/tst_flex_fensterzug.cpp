// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wer gibt beim FLEX-8400 die Mitte vor: das Fenster oder der
// Panadapter der Maschine?
//
// Dies ist der Prueflauf zu den 80 kHz, die seit dem 2026-10-29 offen
// waren. Die Frequenzspur (PR #351) hat sie nicht mehr erklaeren
// muessen — das Protokoll des laufenden Betriebs vom 2026-10-30 tut
// es selbst:
//
//     41.632  Panadapter auf die Scheibe gezogen — 7.074 MHz
//     41.890  Panadapter folgt dem Bild — 7.154 MHz (war 7.074)
//
// Longpath zog den Panadapter auf die Frequenz der Maschine, und
// 76 ms spaeter zog der Nachzug ihn auf das Fenster zurueck. Das
// Fenster stand auf 7.154, weil es beim Verbinden die zweite
// Ausschnittsmeldung der Maschine uebernommen hatte — 80 kHz ueber
// ihrer eigenen Scheibe.
//
// Beide Regeln sind fuer sich richtig (Zoom nicht wegwerfen; das Bild
// nicht schief ueber der FFT stehen lassen). Was fehlte, ist der
// Unterschied zwischen einem fremden und einem eigenen Zug. Genau das
// wird hier geprueft, in beide Richtungen: der eigene Zug muss
// stehen, und der Zoom des Betreibers muss ueberleben.

#include <QtTest>

#include "core/FlexFensterZug.h"

using namespace Longpath;
using namespace Longpath::FlexFensterZug;

namespace {
// Die Maze aus dem Protokoll vom 2026-10-30, 13:47.
constexpr double kBreite   = 200000.0;   // 200 kHz Ausschnitt
constexpr double kFenster  = 7154000.0;  // Fenster nach dem Verbinden
constexpr double kMaschine = 7074000.0;  // Scheibe der Maschine
} // namespace

class TstFlexFensterzug : public QObject
{
    Q_OBJECT

private slots:
    void derGemesseneRuecklaufIstGestoppt()
    {
        // So war es: das Fenster blieb stehen, weil nur die Breite
        // zaehlte — und der Nachzug zog den eigenen Zug zurueck.
        QVERIFY(!fensterFolgt(kBreite, kBreite, true, false));
        QVERIFY(nachzugZieht(kFenster, kMaschine, kBreite));

        // So ist es: der eigene Zug gilt, das Fenster folgt ihm mit
        // der Mitte — und danach hat der Nachzug nichts mehr zu tun.
        QVERIFY(fensterFolgt(kBreite, kBreite, true, true));
        QVERIFY(!fensterNimmtBreite(kBreite, kBreite, true, true));
        QVERIFY(!nachzugZieht(kMaschine, kMaschine, kBreite));
    }

    void derNachzugLaeuftBeiEigenemNachscrollenGarNichtErstAn()
    {
        // Gemessen am 2026-10-30, 16:06, nach dem Wechsel auf FT8:
        //   04.041  Panadapter folgt dem Bild — 7.154 (war 7.1948)
        //   04.066  Ausschnitt gesetzt — 7.074   (die Zentrierung)
        //   04.154  Ausschnitt gesetzt — 7.154   (der Nachzug gewinnt)
        // Der Nachzug hatte seinen Befehl schon gesendet; ihn danach
        // zu stoppen kommt zu spaet. Er darf nicht anlaufen.
        QVERIFY(!nachzugWirdAngeworfen(true));
        // Zieht der Betreiber selbst, laeuft er wie bisher.
        QVERIFY(nachzugWirdAngeworfen(false));
    }

    void derZoomDesBetreibersUeberlebt()
    {
        // Der Fall vom 2026-10-23: der Betreiber hat hineingezoomt,
        // die Maschine verschiebt nur ihre Mitte. Das Fenster bleibt.
        QVERIFY(!fensterFolgt(kBreite, kBreite, true, false));
        // Und auch bei unserem eigenen Zug wird seine Breite nicht
        // angetastet — nur die Mitte wandert mit.
        QVERIFY(!fensterNimmtBreite(kBreite, kBreite, true, true));
    }

    void eineWirklichAndereBreiteGiltImmer()
    {
        // Zoom an der Maschine: 50 kHz statt 200. Das ist eine
        // Aussage ueber die Breite und gilt, von welcher Seite auch
        // immer der Zug kommt.
        QVERIFY(fensterFolgt(50000.0, kBreite, true, false));
        QVERIFY(fensterNimmtBreite(50000.0, kBreite, true, false));
        QVERIFY(fensterFolgt(50000.0, kBreite, true, true));
        QVERIFY(fensterNimmtBreite(50000.0, kBreite, true, true));
    }

    void dasErsteMalGiltImmer()
    {
        // Ohne vorherigen Ausschnitt gibt es keinen Zoom zu schuetzen.
        QVERIFY(fensterFolgt(kBreite, 0.0, false, false));
        QVERIFY(fensterNimmtBreite(kBreite, 0.0, false, false));
        // Und auch dann, wenn schon einer stand, aber ohne Breite.
        QVERIFY(fensterFolgt(kBreite, 0.0, true, false));
        QVERIFY(fensterNimmtBreite(kBreite, 0.0, true, false));
    }

    void dasTotbandLaesstRundungenDurch()
    {
        // Ein Prozent von 200 kHz sind 2 kHz. Die Grenze genau
        // treffen: 1,9 kHz schweigt, 2,1 kHz zieht.
        QVERIFY(!nachzugZieht(7076000.0 - 100.0, 7074000.0, kBreite));
        QVERIFY(nachzugZieht(7076100.0, 7074000.0, kBreite));
        // Beide Richtungen.
        QVERIFY(nachzugZieht(7071900.0, 7074000.0, kBreite));
    }

    void ohneAusschnittKeinUrteil()
    {
        // Keine Breite heiszt: es gibt gar keinen Ausschnitt, den man
        // ziehen koennte. Abwesenheit ist keine Aussage.
        QVERIFY(!nachzugZieht(kFenster, kMaschine, 0.0));
        QVERIFY(!nachzugZieht(kFenster, kMaschine, -1.0));
    }

    void dasTotbandLaesstSichSetzen()
    {
        // Zehn Prozent — der Stand vor dem 2026-10-27, der die
        // Dehnung durchliesz. Hier nur als Beleg, dass der Anteil
        // wirklich wirkt.
        QVERIFY(!nachzugZieht(7090000.0, 7074000.0, kBreite, 0.10));
        QVERIFY(nachzugZieht(7090000.0, 7074000.0, kBreite, 0.01));
    }

    // ── Das FT8-Bild bleibt auf dem Durchlassbereich (2026-11-18) ────
    //
    // Am Geraet nach einem Bandwechsel: das Digi-Bild stand richtig
    // auf 21.0755 MHz / 3,5 kHz, 255 ms spaeter zog das Folgen die
    // Mitte auf die Skala (21.074). Betreiber: „signal ft8 im
    // panadapter sollte standarmäßig in der mitte sein."
    void dasDigiBildBehaeltSeineMitte()
    {
        using FlexFensterZug::fensterMitteBeimFolgen;
        const double skala = 21074000.0;
        const double digiMitte = 21075550.0, digiBreite = 3500.0;
        // Auf der Anrufstelle, Fenster hat die Digi-Breite: Digi-Mitte.
        QCOMPARE(fensterMitteBeimFolgen(skala, 3500.0, digiMitte,
                                        digiBreite, true), digiMitte);
        // Der Betreiber hat selbst gezoomt: die Maschine gilt.
        QCOMPARE(fensterMitteBeimFolgen(skala, 20000.0, digiMitte,
                                        digiBreite, true), skala);
        // Nicht auf einer Anrufstelle: die Maschine gilt.
        QCOMPARE(fensterMitteBeimFolgen(21200000.0, 3500.0, 21201550.0,
                                        digiBreite, false), 21200000.0);
        // Kein Digi-Bild bekannt: die Maschine gilt.
        QCOMPARE(fensterMitteBeimFolgen(skala, 3500.0, 0.0, 0.0, true),
                 skala);
    }
};

QTEST_APPLESS_MAIN(TstFlexFensterzug)
#include "tst_flex_fensterzug.moc"
