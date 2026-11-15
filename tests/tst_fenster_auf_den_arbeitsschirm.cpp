// =================================================================
// tests/tst_fenster_auf_den_arbeitsschirm.cpp  (Longpath)
// =================================================================
//
// Longpath-original test.
//
// Betreiber, 2026-11-15, mit drei Bildschirmen:
//
//   „wichtig! ich arbeite gerade mit 2 bildschirmen. wenn ich das
//    logbuch aufmache mit dem plus sehe ich es nur am macbook, nicht
//    aber auf den beiden bildschirmen! es muss immer das neu
//    hinzugefuegte im vordergrund sein!!!!"
//
// Am selben Tag an seinem Aufbau gemessen (NSScreen):
//
//   LEN D32qc-20 (2)         x 0…2560     (Menueleiste)
//   Built-in Retina Display  x 2560…4270
//   LEN D32qc-20 (1)         x −2560…0
//
// und die gespeicherte Lage des Logbuchfensters:
//
//   LogbookGeometryState → frame (2561, 112, 4268, 1158), screenNumber 1
//
// Das ist Punkt fuer Punkt das eingebaute Display. Er hat das Fenster
// irgendwann dort abgelegt, arbeitet seither an den grossen Schirmen --
// und `restoreGeometry()` setzt es treu wieder dorthin, weil der Schirm
// ja noch da ist.
//
// ── Was hier geprueft wird und was nicht ────────────────────────────
//
// `uebertrageAufFlaeche()` ist reine Rechteckrechnung und laesst sich
// vollstaendig pruefen -- das ist der Teil, der entscheidet, WOHIN das
// Fenster auf dem anderen Schirm kommt.
//
// `aufDenArbeitsschirm()` braucht Fenster und Schirme. Unter der
// Offscreen-Plattform (CI) gibt es genau EINEN Schirm; der Fall „liegt
// auf einem anderen" ist dort nicht herstellbar. Geprueft wird darum
// die andere Haelfte, und sie ist die gefaehrlichere: ein Fenster auf
// DEMSELBEN Schirm wie der Anker darf NICHT angefasst werden. Genau das
// waere die Verschlechterung -- eine Regel, die ihm die Fenster von
// seinem Arbeitsplatz wegzieht, waere schlimmer als die, die er heute
// gemeldet hat. Abwesenheit ist keine Aussage: dass der
// Schirmwechsel-Fall hier fehlt, heisst nicht, dass er geht, sondern
// dass er an diesem Platz nicht messbar ist.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-15 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include <QGuiApplication>
#include <QList>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#include "gui/WindowPlacement.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstFensterAufDenArbeitsschirm : public QObject {
    Q_OBJECT

private slots:
    // Die linke Kante bleibt links, die rechte rechts, die Mitte in der
    // Mitte. Ein blosses Umrechnen der Koordinate
    // (x * neueBreite / alteBreite) kann das nicht: ein rechts
    // angelegtes Fenster landete damit ueber der Kante.
    void dieKantenBleibenKanten()
    {
        const QRect gross(0, 0, 2560, 1336);
        const QRect klein(2560, 280, 1710, 1074);

        // Links oben angelegt -> links oben.
        const QRect l = uebertrageAufFlaeche(QRect(0, 0, 400, 300),
                                             gross, klein);
        QCOMPARE(l.topLeft(), klein.topLeft());
        QCOMPARE(l.size(), QSize(400, 300));

        // Rechts unten angelegt -> rechts unten, und ganz drin.
        const QRect r = uebertrageAufFlaeche(
            QRect(gross.right() - 399, gross.bottom() - 299, 400, 300),
            gross, klein);
        QCOMPARE(r.right(),  klein.right());
        QCOMPARE(r.bottom(), klein.bottom());
        QVERIFY(klein.contains(r));

        // Mittig -> mittig (auf einen Punkt genau, Rundung erlaubt).
        const QRect m = uebertrageAufFlaeche(
            QRect(gross.x() + (gross.width() - 400) / 2,
                  gross.y() + (gross.height() - 300) / 2, 400, 300),
            gross, klein);
        QVERIFY(qAbs(m.center().x() - klein.center().x()) <= 1);
        QVERIFY(qAbs(m.center().y() - klein.center().y()) <= 1);
    }

    // Ein Fenster vom 2560er Schirm passt nicht unveraendert auf den
    // 1710er. Es wird kleiner -- und bleibt ganz auf der Flaeche.
    void zuGrossWirdKleinerUndBleibtDrin()
    {
        const QRect gross(0, 0, 2560, 1336);
        const QRect klein(2560, 280, 1710, 1074);

        const QRect weit = uebertrageAufFlaeche(QRect(100, 100, 2000, 1200),
                                                gross, klein);
        QCOMPARE(weit.width(),  klein.width());
        QCOMPARE(weit.height(), klein.height());
        QVERIFY(klein.contains(weit));
    }

    // Ein ungueltiges Rechteck kommt unveraendert zurueck, statt
    // irgendwohin gerechnet zu werden.
    void ungueltigesBleibtUnberuehrt()
    {
        const QRect flaeche(0, 0, 1000, 800);
        const QRect kaputt;
        QCOMPARE(uebertrageAufFlaeche(kaputt, flaeche, flaeche), kaputt);
        QCOMPARE(uebertrageAufFlaeche(QRect(10, 10, 100, 100), QRect(), flaeche),
                 QRect(10, 10, 100, 100));
        QCOMPARE(uebertrageAufFlaeche(QRect(10, 10, 100, 100), flaeche, QRect()),
                 QRect(10, 10, 100, 100));
    }

    // Zwischen zwei gleich grossen Flaechen ist die Uebertragung nur
    // eine Verschiebung -- dieselbe Stelle, nur auf dem anderen Schirm.
    void gleichGrosseFlaecheIstNurEineVerschiebung()
    {
        const QRect links(-2560, 0, 2560, 1440);
        const QRect rechts(0, 0, 2560, 1440);
        const QRect fenster(-2000, 300, 500, 400);
        const QRect hin = uebertrageAufFlaeche(fenster, links, rechts);
        QCOMPARE(hin.size(), fenster.size());
        QCOMPARE(hin.topLeft() - fenster.topLeft(),
                 rechts.topLeft() - links.topLeft());
    }

    // ── Sein Aufbau, Punkt fuer Punkt (15.11. gemessen) ─────────────
    //
    // Drei Schirme, und das gespeicherte Logbuchfenster bei x 2561.
    // Genau das ist der Fall, den er gemeldet hat: er arbeitet auf dem
    // grossen Schirm (Index 0, die Menueleiste), das Fenster liegt auf
    // dem eingebauten Display (Index 1).
    void seinAufbauVom15November()
    {
        const QList<QRect> schirme = {
            QRect(0, 0, 2560, 1440),        // LEN D32qc-20 (2)
            QRect(2560, 280, 1710, 1112),   // Built-in Retina Display
            QRect(-2560, 0, 2560, 1440),    // LEN D32qc-20 (1)
        };

        // Das Logbuch, wie es in der Einstellungsdatei steht.
        const QRect logbuch(2561, 112, 4268 - 2561, 1158 - 112);
        QCOMPARE(flaecheMitDerGroesstenUeberlappung(logbuch, schirme), 1);

        // Das Hauptfenster, wie es im Protokoll seines Laufs steht.
        const QRect haupt(0, 33, 1470, 865);
        QCOMPARE(flaecheMitDerGroesstenUeberlappung(haupt, schirme), 0);

        // Also: zwei verschiedene Schirme -- das Fenster muss hinueber.
        QVERIFY(flaecheMitDerGroesstenUeberlappung(logbuch, schirme)
                != flaecheMitDerGroesstenUeberlappung(haupt, schirme));

        // Und dort landet es im sichtbaren Bereich des Arbeitsschirms.
        const QRect hin = uebertrageAufFlaeche(logbuch, schirme.at(1),
                                               schirme.at(0));
        QVERIFY2(schirme.at(0).contains(hin),
                 "das Logbuch muss ganz auf dem Arbeitsschirm liegen");
        QVERIFY2(!schirme.at(1).intersects(hin),
                 "und nicht mehr auf dem MacBook");
    }

    // Ein Fenster, das keinen Schirm beruehrt, hat keinen -- statt
    // stillschweigend den ersten zu bekommen.
    void ganzAusserhalbIstKeinSchirm()
    {
        const QList<QRect> schirme = { QRect(0, 0, 2560, 1440) };
        QCOMPARE(flaecheMitDerGroesstenUeberlappung(
                     QRect(9000, 9000, 300, 200), schirme), -1);
        QCOMPARE(flaecheMitDerGroesstenUeberlappung(QRect(10, 10, 100, 100),
                                                     {}), -1);
    }

    // Halb auf dem einen, ein Viertel auf dem anderen: es gewinnt, wo
    // mehr davon liegt.
    void derSchirmMitMehrFlaecheGewinnt()
    {
        const QList<QRect> schirme = { QRect(0, 0, 1000, 1000),
                                       QRect(1000, 0, 1000, 1000) };
        // 700 Punkte links, 300 rechts.
        QCOMPARE(flaecheMitDerGroesstenUeberlappung(
                     QRect(300, 100, 1000, 200), schirme), 0);
        // 200 links, 800 rechts.
        QCOMPARE(flaecheMitDerGroesstenUeberlappung(
                     QRect(800, 100, 1000, 200), schirme), 1);
    }

    // ── Die gefaehrlichere Haelfte ──────────────────────────────────
    //
    // Auf demselben Schirm wie der Anker wird NICHTS angefasst. Was der
    // Betreiber auf seinem Arbeitsschirm hingestellt hat, bleibt dort
    // stehen -- auch wenn es das Hauptfenster gar nicht beruehrt (genau
    // darin unterscheidet sich diese Regel von
    // `ensureOnVisibleScreen()`, das dafuer eine Mindestueberlappung
    // verlangt).
    void aufDemselbenSchirmBleibtAllesStehen()
    {
        QWidget haupt;
        haupt.setGeometry(50, 50, 500, 400);
        haupt.show();
        QVERIFY(QTest::qWaitForWindowExposed(&haupt, kFensterFrist));

        QScreen* s = haupt.windowHandle() ? haupt.windowHandle()->screen()
                                          : QGuiApplication::primaryScreen();
        QVERIFY(s);
        const QRect frei = s->availableGeometry();

        // Weit weg vom Hauptfenster, aber auf demselben Schirm.
        QWidget neben(&haupt, Qt::Tool);
        const QRect wo(frei.right() - 260, frei.bottom() - 200, 250, 190);
        neben.setGeometry(wo);
        neben.show();
        QVERIFY(QTest::qWaitForWindowExposed(&neben, kFensterFrist));
        const QRect vorher = neben.geometry();

        QVERIFY2(!aufDenArbeitsschirm(&neben, &haupt),
                 "ein Fenster auf dem Arbeitsschirm darf nicht verschoben "
                 "werden");
        QCOMPARE(neben.geometry(), vorher);
    }

    // Ohne Anker (oder ohne Fenster) tut die Funktion nichts, statt zu
    // raten. Der Pruefmodus-Fall: ein Werkzeugfenster ohne Hauptfenster.
    void ohneAnkerGeschiehtNichts()
    {
        QWidget allein;
        allein.setGeometry(100, 100, 300, 200);
        QVERIFY(!aufDenArbeitsschirm(&allein, nullptr));
        QVERIFY(!aufDenArbeitsschirm(nullptr, &allein));
        QCOMPARE(allein.geometry(), QRect(100, 100, 300, 200));
    }
};

QTEST_MAIN(TstFensterAufDenArbeitsschirm)
#include "tst_fenster_auf_den_arbeitsschirm.moc"
