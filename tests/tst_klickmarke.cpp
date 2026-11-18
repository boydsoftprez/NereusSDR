// =================================================================
// tests/tst_klickmarke.cpp  (Longpath)
// =================================================================
//
// Longpath-original test.
//
// Betreiber, 2026-11-17: „Das klicken auf Buttons sollte besser farblich
// markiert sein, verwende das Orange wie beim PLUS Zeichen."
//
// Was hier festgehalten wird:
//   * ein Druck auf einen Knopf legt die Marke darueber, und sie ist
//     wirklich ORANGE (am Bild gezaehlt, nicht an einer Flagge);
//   * nach dem Loslassen klingt sie aus und verschwindet ganz;
//   * die Marke sieht zu, sie greift nicht ein: `clicked` kommt weiter;
//   * Ankreuzfelder, Wahlknoepfe, gesperrte Knoepfe und wer sich
//     abmeldet (das Plus) bekommen keine;
//   * ein Druck auf ein KIND im Knopf markiert den Knopf.
//
// Mit LONGPATH_BILD_DIR schreibt der Lauf zwei Bilder (Ruhe / gedrueckt)
// -- Gruen heisst, dass die Marke kommt, nicht, dass sie gut aussieht.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QToolButton>

#include "gui/StyleConstants.h"
#include "gui/widgets/KlickMarke.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

// Wie viele Bildpunkte liegen im Orange des Plus? Warm und hell: Rot
// deutlich ueber Blau, Gruen dazwischen -- so sieht auf dem dunklen
// Grund des Hausstils nichts anderes aus.
int orangePunkte(const QImage& bild)
{
    int n = 0;
    for (int y = 0; y < bild.height(); ++y) {
        for (int x = 0; x < bild.width(); ++x) {
            const QColor c = bild.pixelColor(x, y);
            if (c.red() > 110 && c.red() > c.blue() + 50 && c.green() > c.blue() + 20
                && c.red() >= c.green()) {
                ++n;
            }
        }
    }
    return n;
}

void schreibeBild(const QImage& bild, const QString& name)
{
    const QByteArray ziel = qgetenv("LONGPATH_BILD_DIR");
    if (ziel.isEmpty()) { return; }
    QDir().mkpath(QString::fromLocal8Bit(ziel));
    bild.save(QString::fromLocal8Bit(ziel) + QLatin1Char('/') + name);
}

} // namespace

class TstKlickMarke : public QObject {
    Q_OBJECT

private slots:
    void derDruckLeuchtetOrangeUndKlingtAus()
    {
        KlickMarke marke;

        QWidget fenster;
        fenster.setStyleSheet(QStringLiteral("background: %1;")
                                  .arg(QLatin1String(Style::kPanelBg)));
        auto* zeile = new QHBoxLayout(&fenster);
        auto* knopf = new QPushButton(QStringLiteral("20 m"), &fenster);
        knopf->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
        knopf->setFixedSize(96, 30);
        zeile->addWidget(knopf);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));

        const QImage ruhe = knopf->grab().toImage();
        // In Ruhe so gut wie nichts -- aber nicht „genau null": unter
        // Linux glaettet Qt die Schrift ueber die Farbkanaele, und die
        // Raender der Buchstaben haben dann einzelne warme Bildpunkte
        // (CI 17.11., Linux x64: dieser Vergleich fiel auf `== 0`). Die
        // Marke selbst deckt die halbe Flaeche; fuenf Prozent sind weit
        // darunter und weit ueber jedem Schriftrand.
        const int ruhePunkte = orangePunkte(ruhe);
        QVERIFY2(ruhePunkte < ruhe.width() * ruhe.height() / 20,
                 qPrintable(QStringLiteral("schon in Ruhe %1 orange Bildpunkte")
                                .arg(ruhePunkte)));
        QVERIFY(!marke.markeFuerTest(knopf));
        schreibeBild(fenster.grab().toImage(), QStringLiteral("klickmarke-1-ruhe.png"));

        QSignalSpy geklickt(knopf, &QPushButton::clicked);
        QTest::mousePress(knopf, Qt::LeftButton);
        QVERIFY2(marke.markeFuerTest(knopf), "beim Druck liegt keine Marke ueber dem Knopf");
        const QImage gedrueckt = knopf->grab().toImage();
        schreibeBild(fenster.grab().toImage(), QStringLiteral("klickmarke-2-gedrueckt.png"));
        // Gut die halbe Flaeche: ein Rand allein waere zu wenig, um
        // „besser farblich markiert" zu sein.
        const int flaeche = gedrueckt.width() * gedrueckt.height();
        QVERIFY2(orangePunkte(gedrueckt) > flaeche / 2,
                 qPrintable(QStringLiteral("nur %1 von %2 Bildpunkten sind orange")
                                .arg(orangePunkte(gedrueckt)).arg(flaeche)));

        QTest::mouseRelease(knopf, Qt::LeftButton);
        // Die Marke sieht zu, sie greift nicht ein.
        QCOMPARE(geklickt.count(), 1);

        // Ausklang: danach ist sie ganz weg, auch aus dem Bild.
        QTRY_VERIFY_WITH_TIMEOUT(!marke.markeFuerTest(knopf),
                                 KlickMarke::kAusklangMs * 10);
        // Wieder genau wie vorher -- dieselben Schriftraender, nicht mehr.
        QCOMPARE(orangePunkte(knopf->grab().toImage()), ruhePunkte);
    }

    void werKeineBekommt()
    {
        KlickMarke marke;
        QWidget fenster;
        auto* zeile = new QHBoxLayout(&fenster);
        auto* haken = new QCheckBox(QStringLiteral("Split"), &fenster);
        auto* wahl = new QRadioButton(QStringLiteral("USB"), &fenster);
        auto* gesperrt = new QPushButton(QStringLiteral("TUNE"), &fenster);
        gesperrt->setEnabled(false);
        auto* plus = new QPushButton(QStringLiteral("+"), &fenster);
        plus->setProperty("klickmarke", false);
        auto* werkzeug = new QToolButton(&fenster);
        for (QWidget* w : {static_cast<QWidget*>(haken), static_cast<QWidget*>(wahl),
                           static_cast<QWidget*>(gesperrt), static_cast<QWidget*>(plus),
                           static_cast<QWidget*>(werkzeug)}) {
            zeile->addWidget(w);
        }
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));

        // Der Haken IST die Antwort.
        QVERIFY(!KlickMarke::bekommtMarke(haken));
        QVERIFY(!KlickMarke::bekommtMarke(wahl));
        QVERIFY(!KlickMarke::bekommtMarke(gesperrt));
        QVERIFY(!KlickMarke::bekommtMarke(plus));
        // Ein Werkzeugknopf ist ein Druckknopf wie jeder andere.
        QVERIFY(KlickMarke::bekommtMarke(werkzeug));

        QTest::mousePress(haken, Qt::LeftButton);
        QVERIFY(!marke.markeFuerTest(haken));
        QTest::mouseRelease(haken, Qt::LeftButton);
        QTest::mousePress(plus, Qt::LeftButton);
        QVERIFY(!marke.markeFuerTest(plus));
        QTest::mouseRelease(plus, Qt::LeftButton);
    }

    // Ein Druck landet oft auf einem Kind im Knopf (Symbol, Text).
    void einDruckAufEinKindMarkiertDenKnopf()
    {
        KlickMarke marke;
        QWidget fenster;
        auto* knopf = new QPushButton(&fenster);
        knopf->setFixedSize(120, 34);
        auto* innen = new QHBoxLayout(knopf);
        auto* schild = new QLabel(QStringLiteral("FT8"), knopf);
        innen->addWidget(schild);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));

        QCOMPARE(KlickMarke::knopfZu(schild), static_cast<QAbstractButton*>(knopf));
        QVERIFY(!KlickMarke::knopfZu(&fenster));

        QTest::mousePress(schild, Qt::LeftButton);
        QVERIFY(marke.markeFuerTest(knopf));
        QTest::mouseRelease(schild, Qt::LeftButton);
    }

    // Vom Knopf herunterziehen und draussen loslassen: nichts ausgeloest,
    // aber die Marke muss trotzdem gehen -- sonst bleibt sie stehen.
    void draussenLoslassenRaeumtAuf()
    {
        KlickMarke marke;
        QWidget fenster;
        fenster.resize(300, 120);
        auto* knopf = new QPushButton(QStringLiteral("MOX"), &fenster);
        knopf->setGeometry(10, 10, 80, 30);
        auto* daneben = new QLabel(QStringLiteral("daneben"), &fenster);
        daneben->setGeometry(150, 60, 100, 30);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));

        QTest::mousePress(knopf, Qt::LeftButton);
        QVERIFY(marke.markeFuerTest(knopf));
        QTest::mouseRelease(daneben, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!marke.markeFuerTest(knopf),
                                 KlickMarke::kAusklangMs * 10);
    }

    // Der Knopf verschwindet, waehrend er gedrueckt ist (ein Dialog
    // schliesst sich im Druck): kein Absturz, kein haengender Eintrag.
    void derKnopfVerschwindetImDruck()
    {
        KlickMarke marke;
        auto* fenster = new QWidget;
        auto* knopf = new QPushButton(QStringLiteral("OK"), fenster);
        fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(fenster, kFensterFrist));
        QTest::mousePress(knopf, Qt::LeftButton);
        QVERIFY(marke.markeFuerTest(knopf));
        delete fenster;
        QVERIFY(!marke.markeFuerTest(knopf));
        // Ein spaeteres Loslassen irgendwo darf nichts mehr anfassen.
        QWidget anderes;
        anderes.show();
        QVERIFY(QTest::qWaitForWindowExposed(&anderes, kFensterFrist));
        QTest::mouseRelease(&anderes, Qt::LeftButton);
    }
};

QTEST_MAIN(TstKlickMarke)
#include "tst_klickmarke.moc"
