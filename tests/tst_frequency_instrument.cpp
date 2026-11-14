// =================================================================
// tests/tst_frequency_instrument.cpp  (Longpath)
// =================================================================
//
// Das Frequenz-Widget BEDIENT, es zeigt nicht nur.
//
// Auflage des Betreibers, 2026-08-17: „Das Frequenz-Widget muss Rad
// und Klick-Eingabe tragen, nicht nur anzeigen — sonst nimmt Punkt 3
// das Abstimmen weg." Punkt 3 ist das Ausblenden der VFO-Flagge; wenn
// dieses Widget nur anzeigte, waere danach das Abstimmen mit der Maus
// verschwunden.
//
// Geprueft wird deshalb genau das: dass das Rad ueber einer Ziffer
// DIESE Dekade dreht, und dass die Aenderung als Wunsch nach aussen
// geht statt hier geschrieben zu werden.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QLabel>
#include <QSignalSpy>
#include <QWheelEvent>

#include "gui/instruments/FrequencyInstrument.h"
#include "core/RitXitAnzeige.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

/// Die Ziffernschilder in Anzeigefolge: die Trennpunkte und die
/// Einheit tragen keinen Mauszeiger fuer senkrechtes Ziehen.
QList<QLabel*> digitsOf(FrequencyInstrument& w)
{
    QList<QLabel*> out;
    for (QLabel* l : w.findChildren<QLabel*>()) {
        if (l->cursor().shape() == Qt::SizeVerCursor) { out.append(l); }
    }
    // findChildren gibt keine Reihenfolge zu — nach x sortieren.
    std::sort(out.begin(), out.end(), [](QLabel* a, QLabel* b) {
        return a->mapToGlobal(QPoint(0, 0)).x()
             < b->mapToGlobal(QPoint(0, 0)).x();
    });
    return out;
}

void wheelOn(QWidget* w, int notches)
{
    QWheelEvent ev(QPointF(4, 4), w->mapToGlobal(QPointF(4, 4)),
                   QPoint(0, 0), QPoint(0, notches * 120),
                   Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(w, &ev);
}

} // namespace

class TestFrequencyInstrument : public QObject
{
    Q_OBJECT

private slots:

    // ── Die Gruppierung, nicht nur der Wert ──────────────────────────
    //
    // Befund des Betreibers, 2026-08-18: oben stand 0.713.9700 MHz,
    // in der VFO-Zeile darunter richtig 7.139.700. Der WERT stimmte
    // die ganze Zeit — nur die Trennpunkte sassen an festen Stellen
    // von links, waehrend die Gruppierung vom kleinsten Hertz her
    // zaehlt. Ein Test auf den Wert allein waere gruen geblieben.
    void groupingCountsFromTheRight()
    {
        FrequencyInstrument w;
        w.resize(400, 120);

        w.setFrequency(7'139'700.0);
        QCOMPARE(w.groupedText(), QStringLiteral("7.139.700"));

        w.setFrequency(14'225'000.0);
        QCOMPARE(w.groupedText(), QStringLiteral("14.225.000"));
    }

    // Unter 10 MHz bleibt die vorderste Stelle leer. Eine fuehrende
    // Null liest sich als Teil der Zahl — genau so ist der Fehler oben
    // ueberhaupt entstanden.
    void belowTenMegahertzThereIsNoLeadingZero()
    {
        FrequencyInstrument w;
        w.resize(400, 120);

        w.setFrequency(7'139'700.0);
        QVERIFY2(!w.groupedText().startsWith(QLatin1Char('0')),
                 qPrintable(QStringLiteral("fuehrende Null: %1")
                                .arg(w.groupedText())));

        w.setFrequency(3'650'000.0);
        QCOMPARE(w.groupedText(), QStringLiteral("3.650.000"));

        // Ab 10 MHz steht die Stelle wieder da.
        w.setFrequency(14'225'000.0);
        QVERIFY(w.groupedText().startsWith(QStringLiteral("14")));
    }

    // Die leere Stelle behaelt ihre Trefferflaeche: 7 MHz muss sich auf
    // 17 MHz drehen lassen, auch wenn dort nichts steht.
    void theBlankLeadingDigitStillTunes()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'139'700.0);
        QSignalSpy spy(&w, &FrequencyInstrument::frequencyEdited);
        wheelOn(digitsOf(w).at(0), +1);
        QCOMPARE(spy.count(), 1);
        QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 17'139'700.0) < 1e-6);
    }

    void everyDigitIsItsOwnTarget()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        QCOMPARE(digitsOf(w).size(), 8);
    }

    // ── Drei Bloecke, nicht acht Zeichen ─────────────────────────────
    //
    // Befund des Betreibers, 2026-08-18: „Der Trenner steht genauso
    // weit ab wie die Ziffern untereinander, damit sieht das Auge acht
    // Einzelzeichen statt drei Bloecke — und die Gruppierung, die wir
    // gerade repariert haben, wird optisch wieder aufgehoben."
    //
    // Ursache: Monospace gibt dem Punkt dieselbe Zelle wie einer
    // Ziffer. Der Test misst die WIRKLICHEN Abstaende auf dem Schirm,
    // nicht die Konstanten — waere er gegen kGroupGapOfCell gerechnet,
    // pruefte er nur, dass eine Zahl gleich sich selbst ist.
    void theGapBetweenGroupsBeatsTheGapWithin()
    {
        FrequencyInstrument w;
        w.resize(520, 140);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        const QList<QLabel*> d = digitsOf(w);
        QCOMPARE(d.size(), 8);

        auto gapAfter = [&d](int i) {
            const QRect a = d.at(i)->geometry();
            const QRect b = d.at(i + 1)->geometry();
            return b.left() - a.right() - 1;
        };

        // Innerhalb einer Gruppe: 2-3, 3-4 und 5-6, 6-7.
        const int within = qMax(qMax(gapAfter(2), gapAfter(3)),
                                qMax(gapAfter(5), gapAfter(6)));
        // Zwischen zwei Gruppen: hinter Stelle 1 und hinter Stelle 4.
        const int between = qMin(gapAfter(1), gapAfter(4));

        // Beide Enden festnageln, nicht nur ihr Verhaeltnis: „innen
        // eng" ist eine eigene Zusicherung, sonst waere die Zeile auch
        // dann gruen, wenn beide Abstaende gross sind und nur der eine
        // groesser. Bezug ist die Zeichenzelle, damit die Pruefung
        // ueber die Schriftstufen haelt.
        const int cell = d.at(0)->width();
        QVERIFY2(within <= cell / 6,
                 qPrintable(QStringLiteral(
                     "innerhalb der Gruppe %1 px bei Zelle %2 — nicht eng")
                     .arg(within).arg(cell)));
        QVERIFY2(between >= cell / 2,
                 qPrintable(QStringLiteral(
                     "zwischen den Gruppen %1 px bei Zelle %2 — das Auge "
                     "sieht acht Zeichen statt drei Bloecke")
                     .arg(between).arg(cell)));
    }

    // Der Kern der Auflage: das Rad ueber der Kilohertz-Stelle dreht
    // Kilohertz, nicht die eingestellte Schrittweite.
    void theWheelTurnsThatDigitsDecade()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'139'700.0);
        const QList<QLabel*> d = digitsOf(w);
        QCOMPARE(d.size(), 8);

        QSignalSpy spy(&w, &FrequencyInstrument::frequencyEdited);

        // Stelle 0 = 10 MHz, 4 = 1 kHz, 7 = 1 Hz.
        const QList<QPair<int, double>> cases = {
            {0, 1e7}, {1, 1e6}, {2, 1e5}, {3, 1e4},
            {4, 1e3}, {5, 1e2}, {6, 1e1}, {7, 1e0},
        };
        for (const auto& c : cases) {
            spy.clear();
            wheelOn(d.at(c.first), +1);
            QCOMPARE(spy.count(), 1);
            const double got = spy.at(0).at(0).toDouble();
            QVERIFY2(qAbs((got - 7'139'700.0) - c.second) < 1e-6,
                     qPrintable(QStringLiteral(
                         "Stelle %1 muss %2 Hz drehen, drehte %3")
                             .arg(c.first).arg(c.second)
                             .arg(got - 7'139'700.0)));
        }
    }

    void theWheelTurnsBothWays()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'139'700.0);
        const QList<QLabel*> d = digitsOf(w);
        QSignalSpy spy(&w, &FrequencyInstrument::frequencyEdited);
        wheelOn(d.at(4), -1);
        QCOMPARE(spy.count(), 1);
        QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 7'138'700.0) < 1e-6);
    }

    // Das Widget schreibt NICHT selbst. Es meldet den Wunsch; wer ihn
    // ausfuehrt, entscheidet der Empfaenger — so laeuft die Aenderung
    // ueber denselben Weg wie jede andere Abstimmung, samt allem, was
    // daran haengt.
    void theWidgetDoesNotMoveItself()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'139'700.0);
        wheelOn(digitsOf(w).at(4), +1);
        QCOMPARE(w.frequency(), 7'139'700.0);
    }

    void negativeFrequenciesAreNotOffered()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(5.0);
        QSignalSpy spy(&w, &FrequencyInstrument::frequencyEdited);
        // Zehn MHz abwaerts von 5 Hz waere negativ.
        wheelOn(digitsOf(w).at(0), -1);
        QCOMPARE(spy.count(), 0);
    }

    void theThreeFormsAreSelectable()
    {
        FrequencyInstrument w;
        // War Form::BandStrip. Betreiber 2026-08-30: "MHZ Balken
        // benätige ich nie, löschen!" -- NumberOnly ist seither die
        // Vorgabe.
        QCOMPARE(w.form(), FrequencyInstrument::Form::NumberOnly);
        w.setForm(FrequencyInstrument::Form::FlatArc);
        QCOMPARE(w.form(), FrequencyInstrument::Form::FlatArc);
        w.setForm(FrequencyInstrument::Form::NumberOnly);
        QCOMPARE(w.form(), FrequencyInstrument::Form::NumberOnly);
        // Die blosse Zahl braucht weniger Hoehe als der Bogen — sonst
        // stuende in der knappsten Fassung die meiste leere Flaeche.
        const int hNumber = w.sizeHint().height();
        w.setForm(FrequencyInstrument::Form::FlatArc);
        QVERIFY(w.sizeHint().height() > hNumber);
    }

    // ── RIT/XIT gehoeren dorthin, wo die Frequenz steht (2026-11-12) ──
    //
    // STELLEN kann der Betreiber beide laengst: RxApplet hat seit S2.8
    // je eine Reihe aus Ein/Aus, "0", Minus/Plus und einem "+0 Hz"-
    // Schild. ABLESEN konnte er sie nur dort. Wer das RX-Applet nicht
    // offen hat -- und am 2026-11-12 hatte er es nicht --, sieht einen
    // eingeschalteten Versatz NIRGENDS, waehrend
    // SliceModel::effectiveRxFrequency ihn still addiert: der Empfang
    // liegt daneben, die grossen Ziffern zeigen weiter die VFO-Zahl.
    void offsetsStayInvisibleUntilSwitchedOn()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'164'781.0);
        w.setOtherFrequency(14'074'000.0);

        // Aus: die Zeile ist genau so lang wie eh und je. Ein Versatz,
        // der ausgeschaltet dastuende, kostet Breite und sagt nichts.
        QCOMPARE(w.vfoRowText(),
                 QStringLiteral("A 7.164.781  B 14.074.000"));

        // Ein Wert OHNE eingeschaltet zu sein aendert nichts -- genau
        // so rechnet effectiveRxFrequency() auch.
        w.setRitXit(false, 1200, false, 0);
        QCOMPARE(w.vfoRowText(),
                 QStringLiteral("A 7.164.781  B 14.074.000"));
    }

    void switchedOnOffsetsAreReadableWithTheirSign()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'164'781.0);
        w.setOtherFrequency(14'074'000.0);

        w.setRitXit(true, 1200, false, 0);
        QCOMPARE(w.vfoRowText(),
                 QStringLiteral("A 7.164.781  B 14.074.000  RIT +1200"));

        // Minus als echtes Minuszeichen (U+2212), nicht als Bindestrich
        // -- dieselbe Schreibweise wie die Striche der leeren Anzeige.
        w.setRitXit(true, -250, false, 0);
        QCOMPARE(w.vfoRowText(),
                 QStringLiteral("A 7.164.781  B 14.074.000  RIT \u2212250"));

        // Null MIT Vorzeichen: "RIT +0" heisst eingeschaltet und steht
        // gerade auf null. Ein nacktes "0" heisst das nicht, und ein
        // verschwundener Eintrag erst recht nicht -- das ist die ganze
        // Falle, gegen die dieser Test steht.
        w.setRitXit(true, 0, false, 0);
        QCOMPARE(w.vfoRowText(),
                 QStringLiteral("A 7.164.781  B 14.074.000  RIT +0"));
    }

    void ritAndXitAppearTogetherAndInThatOrder()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'164'781.0);
        w.setOtherFrequency(14'074'000.0);

        w.setRitXit(true, 1200, true, -500);
        QCOMPARE(w.vfoRowText(),
                 QStringLiteral("A 7.164.781  B 14.074.000"
                                "  RIT +1200  XIT \u2212500"));

        // Nur XIT: RIT faellt weg, XIT rutscht NICHT an dessen Platz
        // mit RIT-Beschriftung.
        w.setRitXit(false, 1200, true, -500);
        QCOMPARE(w.vfoRowText(),
                 QStringLiteral("A 7.164.781  B 14.074.000  XIT \u2212500"));
    }

    // Die gefaerbte Zeile ist dieselbe Zeile. Sonst koennte der
    // Klartext oben gruen sein, waehrend am Bildschirm etwas anderes
    // steht -- genau die Spaltung, gegen die groupedText() seit dem
    // 2026-08-18 steht.
    void thePaintedRowCarriesTheSameOffset()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'164'781.0);
        w.setOtherFrequency(14'074'000.0);
        w.setRitXit(true, 1200, false, 0);

        QLabel* vfoRow = nullptr;
        for (QLabel* l : w.findChildren<QLabel*>()) {
            if (l->text().contains(QStringLiteral("RIT"))) { vfoRow = l; break; }
        }
        QVERIFY2(vfoRow, "Die VFO-Zeile zeigt den Versatz nicht an");

        // Das HTML traegt den Wert und die helle Farbe des aktiven
        // Zustands (kAmberText), nicht die matte der stillen Scheibe.
        QVERIFY(vfoRow->text().contains(QStringLiteral("+1200")));
        QVERIFY(vfoRow->text().contains(QStringLiteral("#d8a55f")));
        // Und ausgeschaltet verschwindet er auch aus dem HTML.
        w.setRitXit(false, 1200, false, 0);
        QVERIFY(!vfoRow->text().contains(QStringLiteral("RIT")));
    }

    // ── Welche der ZWEI Quellen die Zeile zeigt ───────────────────────
    //
    // Longpath kennt RIT/XIT an zwei Stellen: der eigenen Scheibe und
    // dem fremden Geraet (TS-590). Die Zeile kann nur eine zeigen --
    // addieren waere falsch, das sind zwei verschiedene Empfaenger.
    // Die Regel sitzt in core/RitXitAnzeige.h; hier wird sie genagelt.
    void withoutACatPathTheSliceOwnsTheRow()
    {
        Ts590Cat::Zustand geraet;          // nichts gesagt: alles -1
        const RitXitAnzeige a =
            ritXitFuerDieZeile(false, geraet, true, 1200, false, 0);
        QCOMPARE(a.ritAn, true);
        QCOMPARE(a.ritHz, 1200);
        QCOMPARE(a.xitAn, false);
    }

    void aLiveCatPathOwnsTheRow()
    {
        // Ein Versatzregister fuer BEIDE, wie im Geraet: steht XIT an,
        // traegt es denselben Betrag wie RIT.
        Ts590Cat::Zustand geraet;
        geraet.ritAn = 0;
        geraet.ritHz = -250;
        geraet.xitAn = 1;

        // Die Scheibe meldet etwas voellig anderes -- es darf nicht
        // durchschlagen, sonst zeigt die Zeile einen Versatz, den der
        // Empfaenger, den der Betreiber hoert, gar nicht hat.
        const RitXitAnzeige a =
            ritXitFuerDieZeile(true, geraet, true, 1200, false, 0);
        QCOMPARE(a.ritAn, false);
        QCOMPARE(a.xitAn, true);
        QCOMPARE(a.xitHz, -250);
    }

    // Ein eingestellter, aber TOTER CAT-Weg ist keine Quelle. Sonst
    // stuende nach dem Ausschalten des Funkgeraets dessen letzter
    // Versatz weiter in der Zeile.
    void aDeadCatPathDoesNotTakeOver()
    {
        Ts590Cat::Zustand geraet;
        geraet.ritAn = 1;
        geraet.ritHz = 900;
        const RitXitAnzeige a =
            ritXitFuerDieZeile(false, geraet, false, 0, false, 0);
        QCOMPARE(a.ritAn, false);
        QCOMPARE(a.ritHz, 0);
    }

    // Abwesenheit ist keine Aussage: solange das Geraet zu KEINER der
    // beiden Stellen geantwortet hat (beide -1), ist es als Quelle
    // nicht da -- auch bei offener Verbindung.
    void aSilentRadioIsNotYetASource()
    {
        Ts590Cat::Zustand stumm;           // ritAn = xitAn = -1
        const RitXitAnzeige a =
            ritXitFuerDieZeile(true, stumm, true, 1200, false, 0);
        QCOMPARE(a.ritAn, true);
        QCOMPARE(a.ritHz, 1200);
    }

    // Und die halbe Antwort: der TS-590 laesst xitAn auf -1, wenn die
    // IF-Antwort zu kurz war (Ts590Cat.h). Dann gilt RIT vom Geraet,
    // und zu XIT behauptet die Zeile NICHTS -- weder an noch aus.
    void anUnknownXitIsNotClaimedToBeOff()
    {
        Ts590Cat::Zustand halb;
        halb.ritAn = 1;
        halb.ritHz = 600;
        halb.xitAn = -1;
        const RitXitAnzeige a =
            ritXitFuerDieZeile(true, halb, false, 0, true, 3000);
        QCOMPARE(a.ritAn, true);
        QCOMPARE(a.ritHz, 600);
        QCOMPARE(a.xitAn, false);   // nicht gezeigt, nicht behauptet
    }
};

QTEST_MAIN(TestFrequencyInstrument)
#include "tst_frequency_instrument.moc"
