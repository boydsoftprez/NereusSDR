// =================================================================
// tests/tst_ft8_uhrversatz.cpp  (Longpath)
// =================================================================
//
// Faehrt `Ft8Uhrversatz` gegen die Faelle, um derentwillen sie
// gebaut wurde -- und gegen die, bei denen sie SCHWEIGEN muss.
//
// Der teuerste Fehler waere nicht eine verpasste Warnung, sondern
// eine falsche: wer einmal gewarnt wird und seine Uhr in Ordnung
// findet, sieht beim naechsten Mal weg. Darum stehen hier mehr
// Gegenproben als Nachweise.
//
// Modification history (Longpath):
//   2026-11-13 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QDir>
#include <QLabel>

#include "core/Ft8Decoder.h"
#include "core/Ft8Uhrversatz.h"
#include "gui/applets/Ft8Applet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using Longpath::DSPMode;
using Longpath::Ft8Uhrversatz;
using Longpath::RadioModel;
using Longpath::SliceModel;
using Lage = Ft8Uhrversatz::Lage;

namespace {

/// Eine Periode mit den angegebenen Funden fuettern und schliessen.
void periode(Ft8Uhrversatz& u, std::initializer_list<double> dt)
{
    for (double d : dt) { u.merke(d); }
    u.periodeFertig();
}

/// n gleiche Perioden -- der Normalfall eines gleichmaessigen Versatzes.
void perioden(Ft8Uhrversatz& u, int n, std::initializer_list<double> dt)
{
    for (int i = 0; i < n; ++i) { periode(u, dt); }
}

/// Eine Periode, wie sie aus dem Dekoder kaeme.
QVector<Longpath::Ft8Decode> funde(std::initializer_list<double> dt)
{
    QVector<Longpath::Ft8Decode> v;
    int hz = 800;
    for (double d : dt) {
        Longpath::Ft8Decode f;
        f.dtSeconds = float(d);
        f.freqHz    = float(hz);
        f.snrDb     = -5.0f;
        f.text      = QStringLiteral("CQ DL%1AA JO40").arg(hz % 10);
        v.push_back(f);
        hz += 60;
    }
    return v;
}

}  // namespace

class TstFt8Uhrversatz : public QObject
{
    Q_OBJECT

private slots:

    // ── Erst schweigen ──────────────────────────────────────────

    void ohneFundeKeinUrteil()
    {
        Ft8Uhrversatz u;
        QVERIFY(!u.hatUrteil());
        QCOMPARE(u.lage(), Lage::Unbekannt);
        QCOMPARE(u.versatzSek(), 0.0);
    }

    void zweiPeriodenReichenNochNicht()
    {
        Ft8Uhrversatz u;
        perioden(u, 2, {2.0, 2.1, 2.0});
        // Der Versatz waere katastrophal -- aber zwei Perioden sind
        // keine Grundlage. Lieber eine Viertelminute laenger hoeren.
        QVERIFY(!u.hatUrteil());
        QCOMPARE(u.lage(), Lage::Unbekannt);
    }

    void einzelneFundeZaehlenNicht()
    {
        Ft8Uhrversatz u;
        // Zehn Perioden mit je EINEM Fund, alle weit daneben: die
        // Anzeige darf trotzdem stumm bleiben. Genau so sieht ein
        // totes Band mit einer einzigen schlecht gehenden Gegenstation
        // aus.
        for (int i = 0; i < 10; ++i) { periode(u, {2.4}); }
        QVERIFY(!u.hatUrteil());
        QCOMPARE(u.periodenGezaehlt(), 0);
    }

    // ── Dann urteilen ───────────────────────────────────────────

    void einesauberUhrIstOk()
    {
        Ft8Uhrversatz u;
        // Die uebliche Mischung aus Laufzeit und Puffer: ein paar
        // Zehntel ins Plus. Das ist der Normalfall, nicht der Fehler.
        perioden(u, 4, {0.1, 0.3, 0.2, 0.4});
        QVERIFY(u.hatUrteil());
        QCOMPARE(u.lage(), Lage::Ok);
        QVERIFY(std::abs(u.versatzSek() - 0.25) < 0.001);
    }

    void eineSekundeIstAuffaellig()
    {
        Ft8Uhrversatz u;
        perioden(u, 4, {1.0, 1.1, 0.9, 1.0});
        QCOMPARE(u.lage(), Lage::Auffaellig);
    }

    void zweiSekundenSindSchlecht()
    {
        Ft8Uhrversatz u;
        perioden(u, 4, {2.0, 2.1, 1.9, 2.0});
        QCOMPARE(u.lage(), Lage::Schlecht);
    }

    void auchNachHintenWirdGewarnt()
    {
        Ft8Uhrversatz u;
        // Eine nachgehende Uhr ist genauso schlimm -- und bei FT8
        // sogar frueher toedlich, weil das Suchfenster nach unten nur
        // 1,60 s hat (nach oben 3,04 s).
        perioden(u, 4, {-1.8, -1.7, -1.9, -1.8});
        QCOMPARE(u.lage(), Lage::Schlecht);
        QVERIFY(u.versatzSek() < 0.0);
    }

    // ── Der Grund fuer den Median ───────────────────────────────

    void eineSchlechteGegenstationKipptNichts()
    {
        Ft8Uhrversatz u;
        // Vier Stationen sauber, eine bei +2,6 s. Ein Mittelwert
        // laege bei 0,66 s und faerbte die Anzeige; der Median nicht.
        perioden(u, 4, {0.2, 0.3, 0.2, 0.3, 2.6});
        QCOMPARE(u.lage(), Lage::Ok);
        QVERIFY(std::abs(u.versatzSek() - 0.3) < 0.001);
    }

    void eineAusreisserPeriodeKipptNichts()
    {
        Ft8Uhrversatz u;
        perioden(u, 3, {0.2, 0.2, 0.3});
        periode(u, {2.5, 2.6, 2.4});   // eine verrueckte Periode
        perioden(u, 3, {0.2, 0.2, 0.3});
        QCOMPARE(u.lage(), Lage::Ok);
    }

    void unsinnigeWerteWerdenNichtGezaehlt()
    {
        Ft8Uhrversatz u;
        for (int i = 0; i < 4; ++i) {
            u.merke(0.2);
            u.merke(std::numeric_limits<double>::quiet_NaN());
            u.merke(1e9);
            u.merke(0.3);
            u.periodeFertig();
        }
        QCOMPARE(u.lage(), Lage::Ok);
        QVERIFY(std::abs(u.versatzSek() - 0.25) < 0.001);
    }

    // ── FT4 hat weniger Luft ────────────────────────────────────

    void ft4WarntFrueher()
    {
        Ft8Uhrversatz ft8;
        perioden(ft8, 4, {0.6, 0.6, 0.6});
        QCOMPARE(ft8.lage(), Lage::Ok);

        Ft8Uhrversatz ft4;
        ft4.setzeFt4(true);
        perioden(ft4, 4, {0.6, 0.6, 0.6});
        // Derselbe Versatz, der bei FT8 niemanden stoert, kostet bei
        // FT4 schon Funde: 0,048 s je Symbol statt 0,16 s.
        QCOMPARE(ft4.lage(), Lage::Schlecht);
    }

    void protokollwechselVergisstDasAlte()
    {
        Ft8Uhrversatz u;
        perioden(u, 5, {0.2, 0.2, 0.2});
        QVERIFY(u.hatUrteil());
        u.setzeFt4(true);
        // Die alten Perioden gehoeren zu einem anderen Lauf -- und
        // zu einer anderen Fensterlaenge.
        QVERIFY(!u.hatUrteil());
        QCOMPARE(u.lage(), Lage::Unbekannt);
    }

    void derselbeProtokollwertVergisstNichts()
    {
        Ft8Uhrversatz u;
        perioden(u, 5, {0.2, 0.2, 0.2});
        u.setzeFt4(false);           // war schon false
        QVERIFY(u.hatUrteil());
    }

    // ── Das Fenster wandert mit ─────────────────────────────────

    void alteePeriodenFallenHeraus()
    {
        Ft8Uhrversatz u;
        // Erst lange schlecht ...
        perioden(u, 8, {2.0, 2.0, 2.0});
        QCOMPARE(u.lage(), Lage::Schlecht);
        // ... dann die Uhr gestellt. Nach kPerioden sauberen Perioden
        // muss die Warnung weg sein, ohne dass jemand etwas leert.
        perioden(u, int(Ft8Uhrversatz::kPerioden), {0.2, 0.2, 0.2});
        QCOMPARE(u.lage(), Lage::Ok);
        QCOMPARE(u.periodenGezaehlt(), int(Ft8Uhrversatz::kPerioden));
    }

    void leerenVergisstAlles()
    {
        Ft8Uhrversatz u;
        perioden(u, 5, {2.0, 2.0, 2.0});
        u.leere();
        QVERIFY(!u.hatUrteil());
        QCOMPARE(u.versatzSek(), 0.0);
    }

    // ── Der Median selbst ───────────────────────────────────────

    void geradeAnzahlNimmtDieMitteDerBeiden()
    {
        Ft8Uhrversatz u;
        // Vier Werte: 0,0 / 0,2 / 0,4 / 0,6 -> Median 0,3.
        // Eine `nth_element`-Loesung, die nur das obere der beiden
        // mittleren nimmt, lieferte 0,4 und waere hier rot.
        perioden(u, 4, {0.0, 0.2, 0.4, 0.6});
        QVERIFY(std::abs(u.versatzSek() - 0.3) < 0.001);
    }

    // ── Und jetzt im Panel selbst ───────────────────────────────
    //
    // Die Rechnung oben kann stimmen und das Schild trotzdem nie
    // erscheinen: zwischen beiden liegen die Fuetterung in
    // `zeigeFunde` und ein `setVisible`. Genau diese Strecke faehrt
    // der Rest dieser Datei -- ueber `periodeForTest`, also denselben
    // Weg, den ein echter Fund nimmt.

    void dasPanelSchweigtBeiEinerGutenUhr()
    {
        RadioModel radio;
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGU);

        Longpath::Ft8Applet applet(&radio);
        applet.setProtocol(Longpath::Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);

        QLabel* schild = applet.uhrWarnungForTest();
        QVERIFY2(schild, "das Schild muss es ueberhaupt geben");
        QVERIFY(!schild->isVisible());

        for (int i = 0; i < 6; ++i) {
            applet.periodeForTest(funde({0.2, 0.3, 0.2}));
        }
        QCOMPARE(applet.uhrversatzForTest().lage(), Lage::Ok);
        QVERIFY2(schild->text().isEmpty(),
                 "eine Zeile, die immer dasteht, liest niemand mehr");
    }

    void dasPanelWarntBeiEinerSchlechtenUhr()
    {
        RadioModel radio;
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGU);

        Longpath::Ft8Applet applet(&radio);
        applet.setProtocol(Longpath::Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);
        QLabel* schild = applet.uhrWarnungForTest();
        QVERIFY(schild);

        for (int i = 0; i < 6; ++i) {
            applet.periodeForTest(funde({2.0, 2.1, 1.9}));
        }
        QCOMPARE(applet.uhrversatzForTest().lage(), Lage::Schlecht);

        // Die ZAHL muss dastehen, nicht nur ein Urteil: wer seine Uhr
        // fuer richtig haelt, soll nachrechnen koennen.
        QVERIFY2(schild->text().contains(QStringLiteral("2.0")),
                 qPrintable(QStringLiteral("Schild sagt: '%1'")
                                .arg(schild->text())));
        QVERIFY(!schild->toolTip().isEmpty());
    }

    void dasPanelNimmtDieWarnungZurueck()
    {
        RadioModel radio;
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGU);

        Longpath::Ft8Applet applet(&radio);
        applet.setProtocol(Longpath::Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);
        QLabel* schild = applet.uhrWarnungForTest();
        QVERIFY(schild);

        for (int i = 0; i < 6; ++i) {
            applet.periodeForTest(funde({2.0, 2.1, 1.9}));
        }
        QVERIFY(!schild->text().isEmpty());

        // Uhr gestellt -- das Schild muss von selbst verschwinden,
        // ohne dass jemand etwas zuruecksetzt.
        for (int i = 0; i < int(Ft8Uhrversatz::kPerioden); ++i) {
            applet.periodeForTest(funde({0.2, 0.3, 0.2}));
        }
        QCOMPARE(applet.uhrversatzForTest().lage(), Lage::Ok);
        QVERIFY2(schild->text().isEmpty(), "die Warnung blieb stehen");
    }

    void einePeriodeOhneFundeStoertNicht()
    {
        RadioModel radio;
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        Longpath::Ft8Applet applet(&radio);
        applet.setProtocol(Longpath::Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);

        // Der haeufigste Fall ueberhaupt: nichts gehoert. Das darf
        // weder zaehlen noch stoeren -- und vor allem darf die
        // Fuetterung nicht hinter dem fruehen Ausstieg fuer leere
        // Perioden liegen.
        for (int i = 0; i < 4; ++i) {
            applet.periodeForTest({});
            applet.periodeForTest(funde({2.0, 2.1, 1.9}));
        }
        QCOMPARE(applet.uhrversatzForTest().periodenGezaehlt(), 4);
        QCOMPARE(applet.uhrversatzForTest().lage(), Lage::Schlecht);
    }

    /// Ein Blatt zum Ansehen -- nur mit LONGPATH_GRAB_DIR.
    ///
    /// Gestaltung beurteilt der Betreiber am Bild, nicht an einer
    /// Beschreibung. Das Schild erscheint nur im Fehlerfall, und den
    /// herzustellen hiesse sonst, die Systemuhr zu verstellen.
    void blattZumAnsehen()
    {
        const QByteArray ordner = qgetenv("LONGPATH_GRAB_DIR");
        if (ordner.isEmpty()) {
            QSKIP("ohne LONGPATH_GRAB_DIR kein Blatt");
        }
        RadioModel radio;
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGU);

        Longpath::Ft8Applet applet(&radio);
        applet.setProtocol(Longpath::Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);
        applet.resize(520, 420);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));

        for (int i = 0; i < 6; ++i) {
            applet.periodeForTest(funde({1.9, 2.0, 2.1}));
        }
        QCoreApplication::processEvents();

        const QString ziel = QString::fromUtf8(ordner);
        QDir().mkpath(ziel);
        QVERIFY(applet.grab().save(ziel + QStringLiteral("/ft8-uhrwarnung.png")));
        qInfo().noquote() << "Blatt:" << ziel + QStringLiteral("/ft8-uhrwarnung.png")
                          << "Schild:" << applet.uhrWarnungForTest()->text();
    }

    /// Drueckt das Schild die Fundzahl weg?
    ///
    /// Die Sichtreihe traegt sechs Pillen, die Fundzahl und jetzt das
    /// Schild. Das ist die Stelle, an der bei schmalem Fenster etwas
    /// verschwindet -- und das Erste waere die Zahl, auf die man sieht.
    ///
    /// Gemessen am 2026-11-13: das Panel laesst sich gar nicht unter
    /// rund 700 Punkte druecken (die Pillenreihe darueber setzt die
    /// Untergrenze), und bei dieser Breite stehen Fundzahl und Schild
    /// GENAU auf ihrem sizeHint -- 80 und 156. Der Fall haelt das
    /// fest, damit eine kuenftige Verengung hier auffaellt statt im
    /// Bild des Betreibers.
    ///
    /// 2026-11-18: die Verengung ist gekommen -- die Reihen brechen
    /// um, und das Panel ist jetzt wirklich 380 Punkt breit. Der Fall
    /// hat angeschlagen, wie er sollte, und haelt weiter: Zaehler und
    /// Schild rutschen zusammen in eine eigene Zeile und stehen dort
    /// in voller Breite.
    void beiSchmalemPanelBleibtDieFundzahlLesbar()
    {
        RadioModel radio;
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGU);

        Longpath::Ft8Applet applet(&radio);
        applet.setProtocol(Longpath::Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);
        applet.resize(380, 420);   // so schmal, wie es sich druecken laesst
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));

        for (int i = 0; i < 6; ++i) {
            applet.periodeForTest(funde({1.9, 2.0, 2.1}));
        }
        // Mehrere Runden, eine feste Zahl (2026-11-18): Zaehler, Schild
        // und `Clear` stehen seit dem Umbruch der Reihen zusammen in
        // einem eigenen Stueck. Ein neuer Text laeuft darum ueber ZWEI
        // Anordnungen nach oben, und jede meldet sich mit einem eigenen
        // geposteten Ereignis -- nach einer Runde stand das Schild noch
        // in der Breite des vorigen Textes (gemessen: 152 statt 156).
        for (int runde = 0; runde < 6; ++runde) {
            QCoreApplication::sendPostedEvents();
            QCoreApplication::processEvents();
        }

        QLabel* zahl   = applet.lastLabelForTest();
        QLabel* schild = applet.uhrWarnungForTest();
        QVERIFY(zahl && schild);
        QVERIFY2(schild->isVisible(), "das Schild muss zu sehen sein");
        // Die Fundzahl darf nicht auf null gedrueckt werden.
        QVERIFY2(zahl->width() > 0 && zahl->isVisible(),
                 qPrintable(QStringLiteral("Fundzahl %1 breit, Schild %2")
                                .arg(zahl->width()).arg(schild->width())));
        // Und keines der beiden darf aus dem Panel hinausragen.
        const int rand = applet.width();
        QVERIFY2(schild->geometry().right() <= rand,
                 qPrintable(QStringLiteral("Schild endet bei %1, Panel ist %2")
                                .arg(schild->geometry().right()).arg(rand)));
        qInfo().noquote() << "schmal:" << applet.width()
                          << "| Fundzahl" << zahl->width()
                          << "braucht" << zahl->sizeHint().width()
                          << QStringLiteral("'%1'").arg(zahl->text())
                          << "| Schild" << schild->width()
                          << "braucht" << schild->sizeHint().width();
        // Gedrueckt heisst abgeschnitten: ein QLabel kuerzt nicht, es
        // schneidet. Was hier zu schmal steht, fehlt im Bild.
        QVERIFY2(zahl->width() >= zahl->sizeHint().width(),
                 qPrintable(QStringLiteral("Fundzahl abgeschnitten: %1 statt %2 fuer '%3'")
                                .arg(zahl->width()).arg(zahl->sizeHint().width())
                                .arg(zahl->text())));
        QVERIFY2(schild->width() >= schild->sizeHint().width(),
                 qPrintable(QStringLiteral("Schild abgeschnitten: %1 statt %2")
                                .arg(schild->width()).arg(schild->sizeHint().width())));
    }

    void dieSchwelleSelbstWarntSchon()
    {
        Ft8Uhrversatz u;
        perioden(u, 4, {Ft8Uhrversatz::kAuffaelligFt8,
                        Ft8Uhrversatz::kAuffaelligFt8});
        // Genau auf der Schwelle gilt sie als erreicht -- sonst
        // haengt das Urteil an der letzten Nachkommastelle.
        QCOMPARE(u.lage(), Lage::Auffaellig);
    }
};

QTEST_MAIN(TstFt8Uhrversatz)
#include "tst_ft8_uhrversatz.moc"
