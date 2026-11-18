// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_pan_options_button.cpp  (Longpath)
// =================================================================
// Der Zahnrad-Knopf am Panadapter — sichtbar und wirksam.
//
// Der Betreiber, 2026-08-20: „weiters fehlt auch die option button um
// beim pandapter gleich etwas zu aendern wie zb bild, groesse usw."
//
// Bild, Deckkraft und Grundfarbe lagen nur unter Setup -> Display,
// drei Klicks und einen Dialog von der Flaeche entfernt, die man
// gerade ansieht.
//
// Geprueft wird der WEG, nicht der Zustand — die Lehre aus dem
// Ablöseknopf, den es gab, den aber niemand erreichen konnte.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QDir>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>

#include "gui/PanadapterApplet.h"
#include "gui/meters/MeterPoller.h"
#include "models/SliceModel.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestPanOptionsButton : public QObject
{
    Q_OBJECT
private slots:
    void theGearIsVisibleInTheHeader()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(800, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));

        QPushButton* gear = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("⚙")) { gear = b; break; }
        }
        QVERIFY2(gear, "in der Kopfleiste MUSS ein Zahnrad stehen");
        QVERIFY2(gear->isVisible(), "und es muss zu sehen sein");
        QVERIFY2(gear->isEnabled(), "und anklickbar");
        QVERIFY2(!gear->toolTip().isEmpty(),
                 "mit einem Hinweis, was es tut");
    }

    // Die Eintraege muessen wirklich etwas ausloesen.
    // 2026-11-06, Betreiber: "1. aber bitte als option zum einblenden im
    // panadapter." Ab Werk aus, keine Zeile; jeder Haken einzeln.
    void dieAnzeigeZeileIstEineOption()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(800, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));

        QWidget* zeile = applet.readoutRowForTest();
        QVERIFY(zeile);
        QVERIFY2(!zeile->isVisible(), "ohne Haken gibt es keine Zeile");
        QVERIFY(!applet.frequencyReadoutShown() && !applet.swrReadoutShown());

        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);
        QAction* frequenz = nullptr;
        QAction* swr = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text() == QStringLiteral("Frequency readout")) { frequenz = a; }
            if (a->text() == QStringLiteral("SWR readout")) { swr = a; }
        }
        QVERIFY2(frequenz && swr, "beide Haken stehen im Zahnrad");
        QVERIFY(frequenz->isCheckable() && !frequenz->isChecked());

        frequenz->trigger();
        QVERIFY(applet.frequencyReadoutShown());
        QVERIFY2(zeile->isVisible(), "ein Haken genuegt fuer die Zeile");
        swr->trigger();
        QVERIFY(applet.swrReadoutShown());
        frequenz->trigger();
        QVERIFY2(zeile->isVisible(), "SWR allein traegt die Zeile auch");
        swr->trigger();
        QVERIFY2(!zeile->isVisible(), "beide aus: Zeile weg");
        delete m;
    }

    // Die Zeile zeigt die Abstimmfrequenz der Scheibe und laeuft mit.
    void dieAnzeigeZeileLaeuftMitDerScheibe()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        SliceModel slice;
        slice.setFrequency(7113065.0);
        applet.updateStatusOverlay(&slice, 0);
        QCOMPARE(applet.frequencyReadoutTextForTest(), QStringLiteral("7.113.065"));
        slice.setFrequency(14225000.0);
        QCOMPARE(applet.frequencyReadoutTextForTest(), QStringLiteral("14.225.000"));

        // Eine andere Scheibe loest die erste ab.
        SliceModel zweite;
        zweite.setFrequency(3650500.0);
        applet.updateStatusOverlay(&zweite, 0);
        QCOMPARE(applet.frequencyReadoutTextForTest(), QStringLiteral("3.650.500"));
        slice.setFrequency(21000000.0);
        QCOMPARE(applet.frequencyReadoutTextForTest(), QStringLiteral("3.650.500"));
    }

    // Ohne Leistung kein SWR: im Empfang steht ein Strich. Beim Senden die
    // Zahl aus derselben Quelle wie das Frequenz-Widget.
    void imEmpfangSteht_einStrich()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        const QString strich = QString::fromUtf8("\xe2\x80\x94");
        QCOMPARE(applet.swrReadoutTextForTest(), strich);
        applet.onReading(MeterBinding::TxSwr, 1.4);
        QCOMPARE(applet.swrReadoutTextForTest(), strich);     // noch keine Leistung
        applet.onReading(MeterBinding::TxPower, 25.0);
        QCOMPARE(applet.swrReadoutTextForTest(), QStringLiteral("1.40"));
        applet.onReading(MeterBinding::TxPower, 0.0);
        QCOMPARE(applet.swrReadoutTextForTest(), strich);     // zurueck im Empfang
    }

    // ── Das S-Meter als dritte Wahl (2026-11-17) ────────────────────
    //
    // Betreiber: „S-Meter soll auch optional im Panadapter zu sehen
    // sein." Derselbe Weg wie Frequenz und SWR: ein Haken im Zahnrad,
    // ab Werk aus, und er traegt die Zeile auch allein.
    void dasSMeterIstDieDritteWahl()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(900, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        QWidget* zeile = applet.readoutRowForTest();
        QVERIFY(zeile);
        QVERIFY2(!applet.sMeterReadoutShown(), "ab Werk aus");
        QVERIFY(!zeile->isVisible());

        QMenu* m = applet.buildDisplayMenuForTesting();
        QAction* sMeter = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text() == QStringLiteral("S-meter readout")) { sMeter = a; }
        }
        QVERIFY2(sMeter, "der Haken steht im Zahnrad");
        QVERIFY(sMeter->isCheckable() && !sMeter->isChecked());

        sMeter->trigger();
        QVERIFY(applet.sMeterReadoutShown());
        QVERIFY2(zeile->isVisible(), "das S-Meter traegt die Zeile auch allein");
        sMeter->trigger();
        QVERIFY2(!zeile->isVisible(), "aus: Zeile weg");
        delete m;
    }

    // Vor dem ersten Wert ein Strich; dann S-Stufe und Balken aus der
    // EINEN Skala (ReadingSource) -- dieselbe wie im S-Meter-Instrument.
    void dasSMeterZeigtSStufen()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        const QString strich = QString::fromUtf8("\xe2\x80\x94");
        QCOMPARE(applet.sMeterReadoutTextForTest(), strich);
        QCOMPARE(applet.sMeterReadoutBarForTest(), 0);

        // S9 = -73 dBm, eine S-Stufe = 6 dB (Kurzwelle).
        applet.onReading(MeterBinding::SignalAvg, -73.0);
        QCOMPARE(applet.sMeterReadoutTextForTest(), QStringLiteral("S9"));
        const int beiS9 = applet.sMeterReadoutBarForTest();
        QVERIFY2(beiS9 > 0 && beiS9 < 1000,
                 qPrintable(QStringLiteral("Balken bei S9: %1").arg(beiS9)));

        applet.onReading(MeterBinding::SignalAvg, -103.0);
        QCOMPARE(applet.sMeterReadoutTextForTest(), QStringLiteral("S4"));
        QVERIFY2(applet.sMeterReadoutBarForTest() < beiS9, "schwaecher: kuerzerer Balken");

        applet.onReading(MeterBinding::SignalAvg, -53.0);
        QCOMPARE(applet.sMeterReadoutTextForTest(), QStringLiteral("S9+20"));
        QVERIFY2(applet.sMeterReadoutBarForTest() > beiS9, "staerker: laengerer Balken");

        // Andere Messgroessen ruehren es nicht an.
        applet.onReading(MeterBinding::TxSwr, 3.0);
        QCOMPARE(applet.sMeterReadoutTextForTest(), QStringLiteral("S9+20"));
    }

    // Ein Bild fuer den Betreiber (LONGPATH_BILD_DIR): alle drei Stuecke
    // der Zeile nebeneinander. Gruen heisst, dass es da ist -- nicht,
    // dass es gut aussieht.
    void dieZeileAlsBild()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(1100, 300);
        SliceModel slice;
        slice.setFrequency(14074000.0);
        applet.updateStatusOverlay(&slice, 0);
        applet.setFrequencyReadoutShown(true);
        applet.setSMeterReadoutShown(true);
        applet.setSwrReadoutShown(true);
        applet.onReading(MeterBinding::SignalAvg, -67.0);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        QWidget* zeile = applet.readoutRowForTest();
        QVERIFY(zeile && zeile->isVisible());
        const QByteArray ziel = qgetenv("LONGPATH_BILD_DIR");
        if (!ziel.isEmpty()) {
            QDir().mkpath(QString::fromLocal8Bit(ziel));
            zeile->grab().save(QString::fromLocal8Bit(ziel)
                               + QStringLiteral("/pan-anzeigezeile-s-meter.png"));
        }
    }

    void itsEntriesActuallyFire()
    {
        PanadapterApplet applet(QStringLiteral("A"));

        QSignalSpy imgSpy(&applet,
                          &PanadapterApplet::backgroundImageRequested);
        QSignalSpy opaSpy(&applet,
                          &PanadapterApplet::backgroundOpacityRequested);
        QSignalSpy colSpy(&applet,
                          &PanadapterApplet::backgroundColourRequested);
        QSignalSpy setSpy(&applet,
                          &PanadapterApplet::displaySetupRequested);

        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);

        auto fire = [m](const QString& text) -> bool {
            for (QAction* a : m->actions()) {
                if (a->text() == text) { a->trigger(); return true; }
                if (a->menu()) {
                    for (QAction* sub : a->menu()->actions()) {
                        if (sub->text() == text) { sub->trigger(); return true; }
                    }
                }
            }
            return false;
        };

        QVERIFY2(fire(PanadapterApplet::tr("Remove background image")),
                 "der Eintrag zum Entfernen MUSS im Menue stehen");
        QCOMPARE(imgSpy.count(), 1);
        QVERIFY2(imgSpy.at(0).at(0).toString().isEmpty(),
                 "Entfernen heisst: leerer Pfad");

        // „Bild sichtbar 75 %" heisst intern „Fuellfarbe sichtbar 25".
        //
        // Der Wert, den SpectrumWidget haelt, ist die Sichtbarkeit der
        // FUELLFARBE (100 = nur Fuellfarbe, 0 = nur Bild). Das Menue
        // sprach zuerst von „Deckkraft" mit demselben Zahlenwert — also
        // genau verkehrt herum, und der Betreiber sah am 2026-08-20
        // sein Foto nicht. Diese Pruefung haelt die Umrechnung fest.
        QVERIFY2(fire(QStringLiteral("75 %")),
                 "die Bildsichtbarkeit MUSS waehlbar sein");
        QCOMPARE(opaSpy.count(), 1);
        QCOMPARE(opaSpy.at(0).at(0).toInt(), 25);

        QVERIFY2(fire(QStringLiteral("Grundfarbe…")), "Grundfarbe fehlt");
        QCOMPARE(colSpy.count(), 1);

        // Dieselbe tr()-Rolle wie im Programm (2026-10-14): seit
        // Englisch die Quellsprache ist, ist ein Menuetext
        // uebersetzbar und taugt nicht als Bezeichner.
        QVERIFY2(fire(PanadapterApplet::tr("All display settings…")),
                 "der Weg in den Setup-Dialog MUSS bleiben — ein Menue, "
                 "das alles kann, waere wieder der Dialog, nur schlechter");
        QCOMPARE(setSpy.count(), 1);

        m->deleteLater();
    }
};
QTEST_MAIN(TestPanOptionsButton)
#include "tst_pan_options_button.moc"
