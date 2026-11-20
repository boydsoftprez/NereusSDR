// =================================================================
// tests/tst_flex_leistung_je_band.cpp  (Longpath)
// =================================================================
//
// Longpath-original test.
//
// Betreiber, 2026-11-19, mit Bild: Regler 56, gemessen 36,3 W, „zur info
// bezüglich ausgangsleistung" -- und auf die Frage danach: „Ja,
// nachgehen".
//
// Aus allen Protokollen des Novembers gezaehlt (FWDPWR gegen die Zeile
// „Leistung vor dem Tasten"): die Flex liefert je Band einen festen
// Anteil der Reglerzahl -- 15 m 0,64, 20 m 0,75, 10/17/40 m um 0,9 --,
// gleich bei 5 % wie bei 56 %, unabhaengig von SWR und Platzzahl.
//
// Die Maschine fuehrt je Band eine eigene Sendeleistung
// (`transmit band N band_name=15 rfpower=...`). Sie kam mit `sub tx all`
// schon immer an und wurde nirgends gelesen. Dieser Pruefstand haelt
// fest, dass sie jetzt festgehalten wird -- durch denselben Weg wie
// der Zustandsschub der Maschine.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-19 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include "models/RadioModel.h"

using namespace Longpath;

namespace {

/// Zerlegt eine Statuszeile wie `parseLine`: Objektgrenze vor dem
/// ersten '=' (wie in tst_flex_zweiton_tor).
void speise(RadioModel& rm, const QString& rumpf)
{
    const int firstEq = rumpf.indexOf(QLatin1Char('='));
    const int kvStart = rumpf.lastIndexOf(QLatin1Char(' '), firstEq);
    QMap<QString, QString> kvs;
    const QStringList paare =
        rumpf.mid(kvStart + 1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& paar : paare) {
        const int gleich = paar.indexOf(QLatin1Char('='));
        if (gleich > 0) { kvs.insert(paar.left(gleich), paar.mid(gleich + 1)); }
    }
    rm.flexApdStatusForTest(rumpf.left(kvStart).trimmed(), kvs);
}

}  // namespace

class TstFlexLeistungJeBand : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    // Ohne Meldung der Maschine: nichts erfunden.
    void ohneMeldungKeinWert()
    {
        RadioModel rm;
        QCOMPARE(rm.flexBandLeistung(QStringLiteral("15")), -1);
    }

    // Die Zeile der Maschine, je Band -- und eine spaetere Aenderung.
    void jeBandFestgehalten()
    {
        RadioModel rm;
        speise(rm, QStringLiteral("transmit band 7 band_name=15 rfpower=64 tunepower=10 "
                                  "inhibit=0 hwalc_enabled=0"));
        speise(rm, QStringLiteral("transmit band 9 band_name=20 rfpower=75 tunepower=10 "
                                  "inhibit=0 hwalc_enabled=0"));
        QCOMPARE(rm.flexBandLeistung(QStringLiteral("15")), 64);
        QCOMPARE(rm.flexBandLeistung(QStringLiteral("20")), 75);
        QCOMPARE(rm.flexBandLeistung(QStringLiteral("40")), -1);

        speise(rm, QStringLiteral("transmit band 7 band_name=15 rfpower=100"));
        QCOMPARE(rm.flexBandLeistung(QStringLiteral("15")), 100);
    }

    // Eine Teilmeldung ohne rfpower aendert nichts.
    void teilmeldungLaesstStehen()
    {
        RadioModel rm;
        speise(rm, QStringLiteral("transmit band 7 band_name=15 rfpower=64"));
        speise(rm, QStringLiteral("transmit band 7 band_name=15 inhibit=1"));
        QCOMPARE(rm.flexBandLeistung(QStringLiteral("15")), 64);
    }
};

QTEST_MAIN(TstFlexLeistungJeBand)
#include "tst_flex_leistung_je_band.moc"
