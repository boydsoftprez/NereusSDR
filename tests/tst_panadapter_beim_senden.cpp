// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Panadapter beim Senden: das Empfangsbild bleibt stehen (2026-11-20) ─
//
// Betreiber an der FLEX-8400 bei FT8: „wenn ich sende ist alles orange und
// keine peaks ersichtlich", „beim emfang in gruen viel besser". Gewaehlt:
// Entwurf 1 „wie Empfang" (werkzeug/entwuerfe-pan-beim-senden-2026-11-20).
//
// Dieser Pruefstand nagelt fest:
//   1. ab Werk an, Abwahl ueberlebt den Neustart;
//   2. beim Senden bleibt die Spur das letzte Empfangsbild, 400 ms danach
//      auch noch, dann laeuft sie wieder;
//   3. der Wasserfall schreibt beim Senden keine Zeilen -- weder die
//      fertige Kachel der Flex noch die aus den Spektren abgeleitete;
//   4. statt der orangen TX-Flaeche nur ein Streifen oben;
//   5. Zweiton-Test und abgewaehlter Schalter: live wie bisher;
//   6. „TX passband in RX colour" ist ein eigener Schalter (Betreiber:
//      „als option die gruene grafik auch in der bandwith"), beide stehen
//      im Zahnrad des Panadapters.
//
// Gegenprobe gefahren (2026-11-20): die drei Tore im Datenweg
// (updateSpectrumLinear, zeigeFremdeWasserfallzeile, pushWaterfallRow) und
// der Streifen in drawTxFilterOverlay per Patch herausgenommen -> die Faelle
// 2, 3a, 3b und 4 fallen; Patch zurueckgenommen -> gruen.

#include <QtTest/QtTest>
#include <QApplication>
#include <QImage>
#include <QPainter>

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"

#include <QMenu>

using namespace Longpath;

namespace {

constexpr double kMitteHz  = 14075000.0;
constexpr double kSpanneHz = 3200.0;
constexpr int    kBins     = 1024;

// Flacher Boden mit einem Traeger bei `toneBin` -- das Empfangsbild.
QVector<float> empfang(int toneBin)
{
    QVector<float> bins(kBins, 1e-12f);   // ~ -120 dBm
    bins[toneBin] = 1e-8f;                // ~ -80 dBm
    return bins;
}

// Die Flaeche, die die Flex beim Senden liefert: alles hoch.
QVector<float> sendeflaeche()
{
    return QVector<float>(kBins, 1e-6f);  // ~ -60 dBm ueberall
}

void einrichten(SpectrumWidget& w)
{
    w.setDisplayWidthOverrideForTest(800);
    w.setFrequencyRange(kMitteHz, kSpanneHz);
    w.setDdcCenterFrequency(kMitteHz);
    w.setSampleRate(kSpanneHz);
    w.setVfoFrequency(kMitteHz - 1500.0);
    w.setSpectrumDetector(SpectrumDetector::Peak);
    w.setSpectrumAveraging(SpectrumAveraging::None);
    w.setWaterfallDetector(SpectrumDetector::Peak);
    w.setWaterfallAveraging(SpectrumAveraging::None);
}

void fuettern(SpectrumWidget& w, const QVector<float>& bins)
{
    w.updateSpectrumLinear(0, bins, 2.0, 0.0);
}

}  // namespace

class TestPanadapterBeimSenden : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }

    // 1 ──────────────────────────────────────────────────────────────────
    void abWerkAnUndAbwahlBleibt()
    {
        {
            SpectrumWidget w;
            w.loadSettings();
            QVERIFY(w.holdReceivePictureOnTx());
            QVERIFY(w.txPassbandLikeRx());
            QVERIFY(!w.haeltEmpfangsbild());       // im Empfang nichts halten
            QVERIFY(!w.zeigtSendestreifen());
            w.setHoldReceivePictureOnTx(false);
            w.setTxPassbandLikeRx(false);
            w.saveSettings();
        }
        SpectrumWidget w2;
        w2.loadSettings();
        QVERIFY(!w2.holdReceivePictureOnTx());
        QVERIFY(!w2.txPassbandLikeRx());
    }

    // 2 ──────────────────────────────────────────────────────────────────
    void spurBleibtBeimSendenStehenUndLaeuftDanachWeiter()
    {
        SpectrumWidget w;
        einrichten(w);
        fuettern(w, empfang(300));
        const QVector<float> empfangsbild = w.renderedPixels();
        QVERIFY(!empfangsbild.isEmpty());

        w.setMoxOverlay(true);
        QVERIFY(w.haeltEmpfangsbild());
        fuettern(w, sendeflaeche());
        QCOMPARE(w.renderedPixels(), empfangsbild);

        // Nachlauf: direkt nach dem Senden noch gehalten ...
        w.setMoxOverlay(false);
        QVERIFY(w.haeltEmpfangsbild());
        fuettern(w, sendeflaeche());
        QCOMPARE(w.renderedPixels(), empfangsbild);

        // ... nach 400 ms wieder Empfang.
        QTRY_VERIFY_WITH_TIMEOUT(!w.haeltEmpfangsbild(), 1500);
        fuettern(w, empfang(700));
        QVERIFY(w.renderedPixels() != empfangsbild);
    }

    // 3a ─────────────────────────────────────────────────────────────────
    void flexKachelnWerdenBeimSendenNichtGeschrieben()
    {
        SpectrumWidget w;
        einrichten(w);
        w.resize(900, 500);
        const QVector<float> zeile(600, 100.0f);
        const double tief = kMitteHz - kSpanneHz / 2.0;
        const double hoch = kMitteHz + kSpanneHz / 2.0;

        w.zeigeFremdeWasserfallzeile(zeile, tief, hoch);
        const quint64 vorher = w.waterfallRowsWrittenForTest();
        QVERIFY(vorher > 0);

        w.setMoxOverlay(true);
        for (int i = 0; i < 5; ++i) {
            w.zeigeFremdeWasserfallzeile(zeile, tief, hoch);
        }
        QCOMPARE(w.waterfallRowsWrittenForTest(), vorher);

        w.setMoxOverlay(false);
        QTRY_VERIFY_WITH_TIMEOUT(!w.haeltEmpfangsbild(), 1500);
        w.zeigeFremdeWasserfallzeile(zeile, tief, hoch);
        QCOMPARE(w.waterfallRowsWrittenForTest(), vorher + 1);
    }

    // 3b ─────────────────────────────────────────────────────────────────
    // Der abgeleitete Wasserfall laeuft an einem Zeitgeber. Ohne das Tor in
    // pushWaterfallRow schoebe er beim Senden die letzte Empfangszeile noch
    // zwei Sekunden lang weiter (bis die Ankunftssperre greift).
    void abgeleiteteZeilenStehenBeimSenden()
    {
        SpectrumWidget w;
        einrichten(w);
        w.resize(900, 500);
        w.setWfUpdatePeriodMs(20);
        for (int i = 0; i < 10; ++i) {
            fuettern(w, empfang(300));
            QTest::qWait(10);
        }
        QTRY_VERIFY_WITH_TIMEOUT(w.waterfallRowsWrittenForTest() > 0, 2000);

        w.setMoxOverlay(true);
        QTest::qWait(30);  // ein Takt, der schon unterwegs war, darf noch landen
        const quint64 beimSenden = w.waterfallRowsWrittenForTest();
        for (int i = 0; i < 10; ++i) {
            fuettern(w, sendeflaeche());
            QTest::qWait(20);
        }
        QCOMPARE(w.waterfallRowsWrittenForTest(), beimSenden);

        w.setMoxOverlay(false);
        QTRY_VERIFY_WITH_TIMEOUT(!w.haeltEmpfangsbild(), 1500);
        for (int i = 0; i < 10; ++i) {
            fuettern(w, empfang(300));
            QTest::qWait(20);
        }
        QTRY_VERIFY_WITH_TIMEOUT(w.waterfallRowsWrittenForTest() > beimSenden, 2000);
    }

    // 4 ──────────────────────────────────────────────────────────────────
    void streifenStattOrangerFlaeche()
    {
        SpectrumWidget w;
        einrichten(w);
        w.resize(800, 400);
        w.setVfoFrequency(14074000.0);
        w.setFrequencyRange(14075750.0, 3500.0);
        w.setTxMode(DSPMode::DIGU);
        w.setTxFilterRange(100, 2900);
        w.setTxFilterVisible(true);
        w.setMoxOverlay(true);
        QVERIFY(w.zeigtSendestreifen());

        const QRect r(0, 0, 800, 300);
        const auto malen = [&]() {
            QImage img(800, 300, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QPainter p(&img);
            w.paintTxOverlayForTest(p, r);
            return img;
        };
        const int xMitte = 400;   // 14.0757 MHz, mitten im TX-Bereich

        const QImage streifen = malen();
        QVERIFY2(qAlpha(streifen.pixel(xMitte, 1)) > 150,
                 "oben ueber dem Sendebereich muss der Streifen stehen");
        QCOMPARE(qAlpha(streifen.pixel(xMitte, 150)), 0);   // keine Flaeche

        // Gegenstueck: abgewaehlt -> die alte Flaeche, auch wenn das
        // Bild weiter gehalten wird (zwei getrennte Schalter).
        w.setTxPassbandLikeRx(false);
        QVERIFY(w.haeltEmpfangsbild());
        QVERIFY(!w.zeigtSendestreifen());
        const QImage flaeche = malen();
        QVERIFY(qAlpha(flaeche.pixel(xMitte, 150)) > 0);

        // Und umgekehrt: Bild live, Durchlass trotzdem gruen.
        w.setTxPassbandLikeRx(true);
        w.setHoldReceivePictureOnTx(false);
        QVERIFY(!w.haeltEmpfangsbild());
        QVERIFY(w.zeigtSendestreifen());
        QCOMPARE(qAlpha(malen().pixel(xMitte, 150)), 0);
    }

    // 6 ──────────────────────────────────────────────────────────────────
    void beideSchalterStehenImZahnrad()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        SpectrumWidget* sw = applet.spectrumWidget();
        QVERIFY(sw);
        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);
        QAction* halten = nullptr;
        QAction* gruen = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text() == QStringLiteral("Hold receive picture while transmitting")) {
                halten = a;
            }
            if (a->text() == QStringLiteral("TX passband in RX colour")) {
                gruen = a;
            }
        }
        QVERIFY2(halten && gruen, "beide Haken stehen im Zahnrad");
        QVERIFY(halten->isCheckable() && halten->isChecked());
        QVERIFY(gruen->isCheckable() && gruen->isChecked());

        halten->trigger();
        QVERIFY(!sw->holdReceivePictureOnTx());
        gruen->trigger();
        QVERIFY(!sw->txPassbandLikeRx());
        delete m;
    }

    // 5 ──────────────────────────────────────────────────────────────────
    void zweitonTestBleibtLive()
    {
        SpectrumWidget w;
        einrichten(w);
        fuettern(w, empfang(300));
        const QVector<float> empfangsbild = w.renderedPixels();

        w.setTestingIMD(true);
        w.setMoxOverlay(true);
        QVERIFY(!w.haeltEmpfangsbild());
        QVERIFY(!w.zeigtSendestreifen());
        fuettern(w, sendeflaeche());
        QVERIFY(w.renderedPixels() != empfangsbild);
    }

    void abgewaehltLaeuftLiveWieBisher()
    {
        SpectrumWidget w;
        einrichten(w);
        w.setHoldReceivePictureOnTx(false);
        fuettern(w, empfang(300));
        const QVector<float> empfangsbild = w.renderedPixels();

        w.setMoxOverlay(true);
        QVERIFY(!w.haeltEmpfangsbild());
        fuettern(w, sendeflaeche());
        QVERIFY(w.renderedPixels() != empfangsbild);
    }
};

QTEST_MAIN(TestPanadapterBeimSenden)
#include "tst_panadapter_beim_senden.moc"
