// =================================================================
// tests/tst_ft8_flaeche.cpp  (Longpath)
// =================================================================
//
// Die Flaeche zum nativen FT8/FT4-Dekoder — und der Weg vom Ton bis
// in die Liste.
//
// Warum dieser Lauf noetig ist: Ft8Decoder (reine Funktion) und
// Ft8Stream (Uhr und Fenster) waren beide gruen und TROTZDEM im
// Programm unerreichbar — kein Abgriff am Ton, keine Flaeche, kein
// Menueeintrag. Genau der Fehler vom 2026-10-14 („vier Wuensche, vier
// Mal laengst gebaut und nur unerreichbar"). Dieser Lauf prueft
// darum nicht das Dekodieren, sondern die VERDRAHTUNG:
//
//   * ein Fund landet als Zeile in der Liste, Spalten wie in WSJT-X
//   * und gleichzeitig als Eintrag in RxDecodeModel, mit Rufzeichen
//     und mit der Frequenz der Scheibe PLUS der Tonlage
//   * das leere Fenster wird gemeldet, nicht verschwiegen
//   * FT8/FT4 schaltet das Fenster um und die beiden Knoepfe mit
//   * eine entfernte Scheibe loest den Abgriff
//   * der sechste Abgriff am AudioEngine traegt Proben
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeZone>

#include "core/AudioEngine.h"
#include "core/Ft8Stream.h"
#include "core/audio/AudioTapRing.h"
#include "core/Ft8Ablauf.h"
#include "gui/applets/Ft8Applet.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/RxDecodeModel.h"
#include "models/SpotModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstFt8Flaeche : public QObject {
    Q_OBJECT

private:
    static QVector<Ft8Decode> einFund(const QString& text,
                                      float snr = -12.0f,
                                      float hz = 1234.0f)
    {
        Ft8Decode d;
        d.snrDb = snr;
        d.dtSeconds = 0.2f;
        d.freqHz = hz;
        d.score = 40;
        d.text = text;
        return {d};
    }

private slots:

    // ── Der Dekoder folgt der Skala (2026-11-15) ────────────────────
    //
    // Gemessen am 15.11.: die Sitzung davor endete in FT4 auf 14,080
    // MHz, beim Neustart behielt die Flex ihre Frequenz, der Dekoder
    // kam in FT8 hoch -- 102 Fenster mit 0 Funden. Steht die Skala auf
    // einer Anrufstelle, gilt deren Verfahren; daneben die Wahl des
    // Betreibers.
    void derDekoderFolgtDerSkala()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setDspMode(DSPMode::DIGU);
        // Schon VOR dem Anschluss auf der FT4-Stelle: so kommt die
        // Flex nach dem Start herein.
        slice->setFrequency(14080000.0);
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft4);

        slice->setFrequency(14074000.0);
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft8);
        slice->setFrequency(14080300.0);              // Spielraum
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft4);
        // Irgendwo im Band: die Wahl bleibt, wie sie ist.
        slice->setFrequency(14100000.0);
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft4);
        slice->setFrequency(7074000.0);
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft8);
        // DIGL: die Tafel gilt fuer oberes Seitenband.
        slice->setDspMode(DSPMode::DIGL);
        slice->setFrequency(7047500.0);
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft8);
    }

    // ── Die Warteschlange ist auch ANGESCHLOSSEN ────────────────────
    //
    // Nicht „gibt es", sondern „kommt dort an". Genau diese Pruefung
    // fehlte bei Multi Answering, und darum lag es Monate fertig und
    // unerreichbar da (2026-10-30).
    //
    // Gefahren wird der gemessene Fall vom 2026-10-30: waehrend des
    // QSOs mit HB9ZHK rief PD4RJ dreimal und wurde nie beantwortet.
    void einAnrufWaehrendDesQsosLandetInDerSchlange()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                         QStringLiteral("JN67"));

        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(7074000.0);
        applet.setSlice(slice);

        // Ein QSO laeuft: HB9ZHK hat auf mein CQ geantwortet.
        applet.ablaufForTestSchreibend().starteMitAnrufer(
            QStringLiteral("HB9ZHK"), QStringLiteral("OE5SOS"),
            QStringLiteral("JN67"), 18, QStringLiteral("JN47"));
        QCOMPARE(applet.ablaufForTest().partner(),
                 QStringLiteral("HB9ZHK"));

        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);

        // Jetzt ruft PD4RJ dazwischen -- dreimal, wie gemessen.
        for (int i = 0; i < 3; ++i) {
            QMetaObject::invokeMethod(
                &applet, "onDecoded", Qt::DirectConnection,
                Q_ARG(QVector<Longpath::Ft8Decode>,
                      einFund(QStringLiteral("OE5SOS PD4RJ JO33"), 12.0f)),
                Q_ARG(QDateTime, fenster));
        }
        // Und 9A6T, ohne Locator.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("OE5SOS 9A6T"), 11.0f)),
            Q_ARG(QDateTime, fenster));

        const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
        QCOMPARE(applet.schlangeForTest().wartende(jetzt), 2);

        // Der Partner des laufenden QSOs steht NICHT in der Schlange.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("OE5SOS HB9ZHK R-14"), 16.0f)),
            Q_ARG(QDateTime, fenster));
        QCOMPARE(applet.schlangeForTest().wartende(jetzt), 2);

        // Und ein Abschied legt niemanden an.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("OE5SOS DL9XYZ 73"), 5.0f)),
            Q_ARG(QDateTime, fenster));
        QCOMPARE(applet.schlangeForTest().wartende(jetzt), 2);

        // Jetzt der zweite Teil: der naechste Wartende wird gerufen --
        // und zwar mit dem RAPPORT, nicht mit meinem Locator.
        QVERIFY(applet.rufeNaechstenWartendenForTest());
        QCOMPARE(applet.ablaufForTest().partner(), QStringLiteral("PD4RJ"));
        QCOMPARE(applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::SendeReport);
        QCOMPARE(applet.ablaufForTest().naechsteNachricht(),
                 QStringLiteral("PD4RJ OE5SOS +12"));
        QCOMPARE(applet.schlangeForTest().wartende(jetzt), 1);
    }

    // ── Und man SIEHT, dass jemand wartet ─────────────────────────
    //
    // Am 2026-10-30 im Betrieb aufgefallen, eine Minute nachdem die
    // Schlange zum ersten Mal lief: F5PZT und OE3TVJ warteten, und
    // beide standen im Protokoll und nirgends sonst. Das
    // Plaetze-Panel zeigt sie, ist aber ab Werk aus. Eine
    // Warteschlange, die niemand sieht, ist keine.
    void dieZahlDerWartendenStehtImKopfDerListe()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(7074000.0);
        applet.setSlice(slice);

        QVERIFY(applet.lastLabelForTest());
        QVERIFY2(!applet.lastLabelForTest()->text()
                      .contains(QStringLiteral("waiting")),
                 "ohne Wartende steht dort nichts von Wartenden");

        applet.ablaufForTestSchreibend().starteMitAnrufer(
            QStringLiteral("HB9ZHK"), QStringLiteral("OE5SOS"),
            QStringLiteral("JN67"), 18, QString());

        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);
        // Die beiden echten Rufzeichen aus dem Protokoll von 16:09.
        for (const QString& t : {QStringLiteral("OE5SOS F5PZT JN12"),
                                 QStringLiteral("OE5SOS OE3TVJ JN78")}) {
            QMetaObject::invokeMethod(
                &applet, "onDecoded", Qt::DirectConnection,
                Q_ARG(QVector<Longpath::Ft8Decode>, einFund(t)),
                Q_ARG(QDateTime, fenster));
        }

        const QString kopf = applet.lastLabelForTest()->text();
        QVERIFY2(kopf.contains(QStringLiteral("2 waiting")),
                 qPrintable(kopf));
    }

    // ── Die Spaltenkoepfe (2026-10-30) ───────────────────────────
    //
    // Betreiber: „die staerke des signals sollte auch in einer
    // eigenen spalten sein." Sie IST eine -- es fehlte die
    // Ueberschrift. Entscheidend ist, dass Kopf und Zeile DIESELBEN
    // Breiten benutzen: zwei Stellen mit eigenen Zahlen liefen beim
    // ersten Spaltenwechsel auseinander.
    void derSpaltenkopfStehtUeberDenSpalten()
    {
        const QString kopf = Ft8Applet::spaltenkopf();
        QVERIFY2(kopf.contains(QStringLiteral("dB")), qPrintable(kopf));
        QVERIFY(kopf.contains(QStringLiteral("DT")));
        QVERIFY(kopf.contains(QStringLiteral("Freq")));

        // Die Zahl steht unter ihrer Ueberschrift. Gemessen am ENDE
        // der Spalte, weil beide rechtsbuendig stehen -- und mit
        // einem Rauschabstand, der in der Zeile sonst nirgends
        // vorkommt. Mein erster Versuch suchte „8" und fand die Acht
        // in „191845".
        const QString zeile = Ft8Applet::zeilentext(
            QStringLiteral("191845"), -7, 1.0, 2406,
            QStringLiteral("LZ1ZF SP4DIS KO13"), QString());
        QCOMPARE(kopf.indexOf(QStringLiteral("dB")) + 2,
                 zeile.indexOf(QStringLiteral("-7")) + 2);
        QCOMPARE(kopf.indexOf(QStringLiteral("Freq")) + 4,
                 zeile.indexOf(QStringLiteral("2406")) + 4);
        QCOMPARE(kopf.indexOf(QStringLiteral("DT")) + 2,
                 zeile.indexOf(QStringLiteral("1.0")) + 3);
    }

    // ── Die Landspalte ───────────────────────────────────────────────
    //
    // Betreiber am 2026-10-21: „vielleicht kann man auch das land
    // hinzufuegen." Der Auflaeser kannte es laengst; das Fenster
    // benutzte den Namen nur fuer eine Farbe und warf ihn weg.
    //
    // Reine Funktion, darum ohne Fenster, ohne Logbuch und ohne
    // cty.dat pruefbar.
    void dieLandspalteStehtHinterDerNachricht()
    {
        const QString z = Ft8Applet::zeilentext(
            QStringLiteral("121645"), 17, 1.0, 1506,
            QStringLiteral("CQ PA3DJV JO33"), QStringLiteral("Netherlands"));
        QVERIFY2(z.endsWith(QStringLiteral("Netherlands")), qPrintable(z));
        QVERIFY2(z.contains(QStringLiteral("CQ PA3DJV JO33")), qPrintable(z));
        // Die Zahlenspalten bleiben, wie sie waren.
        QVERIFY2(z.startsWith(QStringLiteral("121645")), qPrintable(z));
        QVERIFY2(z.contains(QStringLiteral("1506")), qPrintable(z));
    }

    void ohneLandBleibtDieZeileWieVorher()
    {
        // Leer heisst „nicht aufloesbar", nicht „kein Land". Dann darf
        // die Zeile keinen Leerraum anhaengen, der wie eine leere
        // Spalte aussieht.
        const QString z = Ft8Applet::zeilentext(
            QStringLiteral("121645"), 17, 1.0, 1506,
            QStringLiteral("CQ PA3DJV JO33"), QString());
        QCOMPARE(z, z.trimmed());
        QVERIFY2(z.endsWith(QStringLiteral("CQ PA3DJV JO33")), qPrintable(z));
    }

    void dieLandspalteFluchtet()
    {
        // Der ganze Zweck einer Spalte: zwei Zeilen mit verschieden
        // langen Nachrichten muessen das Land an derselben Stelle
        // zeigen.
        const QString kurz = Ft8Applet::zeilentext(
            QStringLiteral("121645"), 17, 1.0, 1506,
            QStringLiteral("TA2LA OH3OJ RR73"), QStringLiteral("Turkey"));
        const QString lang = Ft8Applet::zeilentext(
            QStringLiteral("121645"), 16, 1.2, 509,
            QStringLiteral("VK4MDP EV1R"), QStringLiteral("Australia"));
        QCOMPARE(kurz.indexOf(QStringLiteral("Turkey")),
                 lang.indexOf(QStringLiteral("Australia")));
    }

    void eineUEBERLANGENachrichtWirdNichtAbgeschnitten()
    {
        // Lieber eine verrutschte Landspalte als eine halbe Nachricht.
        const QString sehrLang = QStringLiteral("ON6LEO F5OYA/P JN13XX");
        const QString z = Ft8Applet::zeilentext(
            QStringLiteral("121645"), 17, 1.0, 1506, sehrLang,
            QStringLiteral("Belgium"));
        QVERIFY2(z.contains(sehrLang), qPrintable(z));
        QVERIFY2(z.endsWith(QStringLiteral("Belgium")), qPrintable(z));
    }

    // ── Die Reihenfolge der Zeilenarten ─────────────────────────────
    //
    // Vier Sorten, und die REIHENFOLGE ist die Aussage. Sie laesst
    // sich nicht ansehen: eine Zeile kann gleichzeitig „an mich",
    // „neues Land" und „CQ" sein, und dann entscheidet diese Regel,
    // was der Betreiber sieht.
    //
    // Der Fall, auf den es ankommt, ist die dritte Zeile unten: ein
    // neues Land, das gerade KEIN CQ ruft. Es muss trotzdem
    // hervorgehoben werden -- man kann die Station im Auge behalten,
    // bis sie frei ist. Wuerde „CQ" darueber stehen, ginge genau das
    // verloren, was man am wenigsten uebersehen will.
    void dieReihenfolgeDerZeilenartenStimmt()
    {
        using Art = Ft8Applet::Zeilenart;

        // an mich schlaegt alles
        QCOMPARE(Ft8Applet::zeilenart(true, true, true, false, true), Art::AnMich);
        QCOMPARE(Ft8Applet::zeilenart(true, false, false, false, false),
                 Art::AnMich);
        // neues Land OHNE CQ -> trotzdem hervorgehoben
        QCOMPARE(Ft8Applet::zeilenart(false, true, false, false, false),
                 Art::NeuesLand);
        // neues Land MIT CQ -> das Land schlaegt das CQ
        QCOMPARE(Ft8Applet::zeilenart(false, true, false, false, true),
                 Art::NeuesLand);
        // ── Das neue Grossfeld (2026-10-30) ───────────────────
        //
        // Zwischen Land und CQ. Ein fehlendes LAND ist mehr wert als
        // ein fehlendes Feld -- wer beides hat, will das Land sehen.
        QCOMPARE(Ft8Applet::zeilenart(false, true, true, false, false),
                 Art::NeuesLand);
        QCOMPARE(Ft8Applet::zeilenart(false, false, true, false, false),
                 Art::NeuesGrid);
        // ... und es schlaegt das CQ, aus demselben Grund wie das Land.
        QCOMPARE(Ft8Applet::zeilenart(false, false, true, false, true),
                 Art::NeuesGrid);
        // ── Die zweite Stufe: fehlt nur auf DIESEM Band (2026-11-12) ──
        //
        // Betreiber heute: die Locatorjagd „sollte optimiert werden und
        // eher am programm ft8 und nicht im logprogramm zu finden sein."
        // Bis dahin kannte das FT8-Fenster nur „steht nirgends im
        // Logbuch" -- die Rechnung je Band stand allein hinter der
        // Karte im Logbuch.
        //
        // Unter dem ganz fehlenden Feld, denn ein Feld, das man noch
        // NIE hatte, ist mehr wert als eines, das nur auf diesem Band
        // fehlt:
        QCOMPARE(Ft8Applet::zeilenart(false, false, true, true, false),
                 Art::NeuesGrid);
        QCOMPARE(Ft8Applet::zeilenart(false, false, false, true, false),
                 Art::NeuesGridBand);
        // ... aber ueber dem CQ: ein CQ ist ein Angebot, ein fehlendes
        // Band ist ein Grund.
        QCOMPARE(Ft8Applet::zeilenart(false, false, false, true, true),
                 Art::NeuesGridBand);
        // Und unter dem Land, wie das ganz fehlende Feld auch.
        QCOMPARE(Ft8Applet::zeilenart(false, true, false, true, false),
                 Art::NeuesLand);
        // nur CQ
        QCOMPARE(Ft8Applet::zeilenart(false, false, false, false, true), Art::Cq);
        // ein fremdes QSO
        QCOMPARE(Ft8Applet::zeilenart(false, false, false, false, false),
                 Art::Gewoehnlich);
    }

    // ── Die Sendestelle ist waehlbar und bleibt ─────────────────────
    //
    // Betreiber 2026-10-18: von den Luecken gegenueber WSJT-X war das
    // die einzige mit Folgen auf dem BAND statt am Schirm -- die
    // Tonlage stand fest auf 1500 Hz, also sendete jeder Longpath-
    // Betreiber mit „Auto" immer auf dieselbe Stelle.
    //
    // Der Lauf haelt beide Haelften fest: das Feld setzt die Tonlage,
    // UND sie ueberlebt das Schliessen des Fensters. Ohne die zweite
    // waere die Wahl bei jedem Start wieder weg.
    void dieSendestelleIstWaehlbarUndBleibt()
    {
        RadioModel radio;
        {
            Ft8Applet applet(&radio);
            QVERIFY2(applet.sendeTonFeldForTest(), "kein Feld fuer die Tonlage");
            QCOMPARE(applet.sendeTonHzForTest(), 1500);   // Vorgabe wie WSJT-X

            applet.sendeTonFeldForTest()->setValue(1725);
            QCOMPARE(applet.sendeTonHzForTest(), 1725);
        }
        // Zweites Fenster, frisch gebaut: die Wahl muss wieder da sein.
        {
            Ft8Applet wieder(&radio);
            QCOMPARE(wieder.sendeTonHzForTest(), 1725);
            QCOMPARE(wieder.sendeTonFeldForTest()->value(), 1725);
        }
    }

    // ── „CQ only" filtert die ANSICHT, nicht den Betrieb ────────────
    //
    // Betreiber 2026-10-18: „dachte das sind dann mehr optionen usw.
    // wie ein richtiges ft8 programm". In WSJT-X ist das der haeufigste
    // Griff -- man sucht jemanden, den man rufen KANN.
    //
    // Der Lauf haelt beide Haelften fest: die fremde QSO-Zeile
    // verschwindet aus der Liste, steht aber weiter im Modell. Faellt
    // die zweite Haelfte weg, waere der Filter kein Filter mehr,
    // sondern ein Taubstellen des Dekoders.
    void nurCqFiltertDieAnsichtAberNichtDasModell()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);

        QVERIFY2(applet.nurCqButtonForTest(), "der Knopf fehlt");
        applet.nurCqButtonForTest()->click();
        QVERIFY(applet.nurCqButtonForTest()->isChecked());

        QVERIFY(radio.rxDecodeModel());
        const int vorher = radio.rxDecodeModel()->decodes().size();

        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);

        // 1. Ein fremdes QSO -- kein CQ.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("DL1ABC DK0BO -12"))),
            Q_ARG(QDateTime, fenster));

        QVERIFY2(!applet.listForTest()->toPlainText()
                     .contains(QStringLiteral("DL1ABC DK0BO")),
                 qPrintable(applet.listForTest()->toPlainText()));
        QVERIFY2(radio.rxDecodeModel()->decodes().size() == vorher + 1,
                 "der Filter hat den Fund auch aus dem Modell gehalten");

        // 2. Ein CQ -- der kommt durch.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ DL1ABC JN49"))),
            Q_ARG(QDateTime, fenster));

        QVERIFY2(applet.listForTest()->toPlainText()
                     .contains(QStringLiteral("CQ DL1ABC JN49")),
                 qPrintable(applet.listForTest()->toPlainText()));
    }

    // ── „New DXCC": ein Ruf an MICH kommt durch jeden Filter ────────
    //
    // Das ist die Falle, die einen Filter von einem Fehler trennt. Wer
    // gerufen wird und es nicht sieht, weil ein Haken sitzt, verliert
    // ein QSO.
    //
    // Ohne cty.dat und ohne Logbuch sagt der Aufloeser zu NICHTS „neu"
    // -- genau deshalb ist dieser Lauf aussagekraeftig: er faehrt den
    // haertesten Fall, in dem der Filter alles wegnimmt, was nicht an
    // mich geht.
    void neueLaenderFiltertAberNichtDenRufAnMich()
    {
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        RadioModel radio;
        Ft8Applet applet(&radio);

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);

        QVERIFY2(applet.nurNeueLaenderButtonForTest(), "der Knopf fehlt");
        applet.nurNeueLaenderButtonForTest()->click();
        QVERIFY(applet.nurNeueLaenderButtonForTest()->isChecked());

        const int vorher = radio.rxDecodeModel()->decodes().size();
        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);

        // 1. Ein fremdes CQ -- ohne Logbuch kein „neues Land", also weg.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ DL1ABC JN49"))),
            Q_ARG(QDateTime, fenster));
        QVERIFY2(!applet.listForTest()->toPlainText()
                     .contains(QStringLiteral("CQ DL1ABC JN49")),
                 qPrintable(applet.listForTest()->toPlainText()));

        // ... aber im Modell steht er. Der Filter ist ANSICHT.
        QCOMPARE(radio.rxDecodeModel()->decodes().size(), vorher + 1);

        // 2. Ein Ruf an mich -- der kommt IMMER durch.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("OE5SOS DL1ABC -12"))),
            Q_ARG(QDateTime, fenster));
        QVERIFY2(applet.listForTest()->toPlainText()
                     .contains(QStringLiteral("OE5SOS DL1ABC")),
                 qPrintable(applet.listForTest()->toPlainText()));
    }

    // ── Der Fund kommt an zwei Stellen an ───────────────────────────

    void einFundStehtInDerListeUndImModell()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY(applet.streamForTest());

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY2(slice, "ohne Scheibe sagt der Lauf nichts");
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);

        QVERIFY(radio.rxDecodeModel());
        const int vorher = radio.rxDecodeModel()->decodes().size();

        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, fenster));

        // 1. Die Liste.
        const QString zeile = applet.listForTest()->toPlainText();
        qInfo().noquote() << "Zeile:" << zeile;
        QVERIFY2(zeile.contains(QStringLiteral("CQ OE5SOS JN78")),
                 qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("-12")), qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("1234")), qPrintable(zeile));
        // Die Uhrzeit des Fensters, nicht die Uhrzeit von jetzt.
        QVERIFY2(zeile.startsWith(fenster.toString(QStringLiteral("HHmmss"))),
                 qPrintable(zeile));

        // 2. Das Modell (RxDecodeModel, „was mein Geraet gerade
        //    gehoert hat"). Ohne diesen Schritt bleibt der Fund in
        //    einem Textfeld. Die Ueberlagerung am Panadapter liest
        //    heute NICHT aus diesem Modell, sondern aus SpotModel —
        //    die Bruecke dorthin ist bewusst noch nicht gebaut (sie
        //    braucht Lebensdauer, Farbe und einen eigenen Reiter im
        //    Spot Hub, nicht eine Zeile).
        const auto decodes = radio.rxDecodeModel()->decodes();
        QCOMPARE(decodes.size(), vorher + 1);
        const RxDecode& e = decodes.last();
        QCOMPARE(e.callsign, QStringLiteral("OE5SOS"));
        QCOMPARE(e.mode, QStringLiteral("FT8"));
        QCOMPARE(e.snr, -12);
        // Der Nutztext traegt den Modus vorn — dasselbe Muster wie bei
        // den WSJT-X-Funden, aus denen SpotTableModel::extractMode den
        // Modus liest.
        QVERIFY2(e.payload.endsWith(QStringLiteral("CQ OE5SOS JN78")),
                 qPrintable(e.payload));
        QVERIFY2(e.payload.startsWith(QStringLiteral("FT8")),
                 qPrintable(e.payload));
        // Die UHRZEIT des Fensters, nicht die von jetzt. Nur die
        // Uhrzeit: DxSpot traegt kein Datum (QTime), das Datum kommt
        // auf dem Weg von heute — im Betrieb ist ein Fenster Sekunden
        // alt, nicht Tage.
        QCOMPARE(e.utcTime.time(), fenster.time());
        // Wahlfrequenz PLUS Tonlage: 14 074 000 + 1234 Hz.
        QVERIFY2(qAbs(e.freqMhz - 14.075234) < 1e-6,
                 qPrintable(QString::number(e.freqMhz, 'f', 6)));
        // Nicht „WSJT-X": das ist eine andere Quelle.
        QVERIFY2(e.source != QStringLiteral("WSJT-X"), qPrintable(e.source));

        // 3. Und der Spot — ohne den erreicht der Fund die
        //    Ueberlagerung am Panadapter nie. Genau das war bis heute
        //    der Fall: der Dekoder schrieb nur ins RxDecodeModel, und
        //    die Ueberlagerung liest aus SpotModel.
        QVERIFY(radio.spotModel());
        bool gefunden = false;
        for (const SpotData& sp : radio.spotModel()->spots()) {
            if (sp.callsign != QStringLiteral("OE5SOS")) { continue; }
            gefunden = true;
            qInfo() << "Spot:" << sp.callsign << sp.rxFreqMhz << sp.source;
            QVERIFY2(qAbs(sp.rxFreqMhz - 14.075234) < 1e-4,
                     qPrintable(QString::number(sp.rxFreqMhz, 'f', 6)));
            QCOMPARE(sp.source, QStringLiteral("Longpath"));
        }
        QVERIFY2(gefunden, "der Fund steht nicht im SpotModel");
    }

    // ── Das Seitenband entscheidet das Vorzeichen ───────────────────
    //
    // In DIGU liegt der Ton oberhalb der Wahlfrequenz, in DIGL
    // darunter. Ohne die Unterscheidung stuende ein DIGL-Fund um bis
    // zu 3 kHz daneben — und zwar lautlos.

    void dasSeitenbandEntscheidetDasVorzeichen()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGL);
        applet.setSlice(slice);

        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));

        QVERIFY(radio.rxDecodeModel());
        QVERIFY(!radio.rxDecodeModel()->decodes().isEmpty());
        const double mhz = radio.rxDecodeModel()->decodes().last().freqMhz;
        qInfo() << "DIGL:" << mhz << "MHz";
        // 14 074 000 MINUS 1234 Hz.
        QVERIFY2(qAbs(mhz - 14.072766) < 1e-6,
                 qPrintable(QString::number(mhz, 'f', 6)));
    }

    // ── Das leere Fenster ist eine Aussage ──────────────────────────
    //
    // „Abwesenheit ist keine Aussage" gilt umgekehrt genauso: ein
    // Fenster ohne Fund und ein toter Dekoder duerfen nicht gleich
    // aussehen.

    void dasLeereFensterWirdGemeldet()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QCOMPARE(applet.lastPeriodCountForTest(), -1);   // noch nichts

        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>, QVector<Ft8Decode>{}),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));

        QCOMPARE(applet.lastPeriodCountForTest(), 0);
        // Die Zahl steht jetzt neben den Pillen, nicht mehr in einer
        // eigenen Fusszeile am anderen Ende des Panels. Was geprueft
        // wird, ist unveraendert: eine leere Periode meldet NULL
        // Funde, nicht „noch nichts".
        QVERIFY2(applet.lastLabelForTest()->text()
                     .contains(QStringLiteral("0 last")),
                 qPrintable(applet.lastLabelForTest()->text()));
        QVERIFY(applet.listForTest()->toPlainText().isEmpty());
    }

    // ── FT8 / FT4 ───────────────────────────────────────────────────

    void ft4SchaltetFensterUndKnoepfe()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QCOMPARE(applet.streamForTest()->slotMs(), 15000);
        QVERIFY(applet.ft8ButtonForTest()->isChecked());
        QVERIFY(!applet.ft4ButtonForTest()->isChecked());

        applet.ft4ButtonForTest()->click();
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft4);
        QCOMPARE(applet.streamForTest()->slotMs(), 7500);
        QVERIFY(applet.ft4ButtonForTest()->isChecked());
        QVERIFY2(!applet.ft8ButtonForTest()->isChecked(),
                 "beide Knoepfe gedrueckt — dann sagt keiner etwas");

        applet.ft8ButtonForTest()->click();
        QCOMPARE(applet.streamForTest()->slotMs(), 15000);
        QVERIFY(applet.ft8ButtonForTest()->isChecked());
        QVERIFY(!applet.ft4ButtonForTest()->isChecked());

        // Und der Modus steht in der Zeile, sonst liest man FT4-Funde
        // als FT8.
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));
        QVERIFY(radio.rxDecodeModel());
        QVERIFY(!radio.rxDecodeModel()->decodes().isEmpty());
        QCOMPARE(radio.rxDecodeModel()->decodes().last().mode,
                 QStringLiteral("FT4"));
    }

    // ── Rufzeichen aus der Nachricht ────────────────────────────────

    void derAbsenderWirdErkannt()
    {
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ OE5SOS JN78")),
                 QStringLiteral("OE5SOS"));
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ DX OE5SOS JN78")),
                 QStringLiteral("OE5SOS"));
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("OE5SOS DL1ABC -12")),
                 QStringLiteral("DL1ABC"));
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("OE5SOS DL1ABC RR73")),
                 QStringLiteral("DL1ABC"));
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ OE5SOS")),
                 QStringLiteral("OE5SOS"));
        // Der Feldkenner darf NICHT als Rufzeichen durchgehen — das war
        // der Fehler, auf den die „letzte Stelle keine Ziffer"-Regel
        // antwortet.
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ DX JN78")),
                 QString());
        QCOMPARE(Ft8Applet::senderCallsign(QString()), QString());
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("TU 73")), QString());
    }

    // ── Der Contest schreibt die Rufzeichen in spitzen Klammern ─────
    //
    // Im EU-VHF-Contest (Typ 5) stehen BEIDE Rufzeichen nur als Hash
    // im Paket; ft8_lib schreibt sie aufgeloest in spitzen Klammern
    // heraus, wie WSJT-X. Ohne Abstreifen nahm diese Funktion
    // stattdessen den LOCATOR fuer das Rufzeichen -- `IO91NP` hat
    // Buchstaben, Ziffern und endet auf einen Buchstaben, die Pruefung
    // passt leider. Die DXCC-Einfaerbung schlug dann auf ein Land, das
    // niemand gerufen hatte.
    //
    // Gegenprobe: ohne `ohneSpitzeKlammern` in Ft8Applet.cpp liefert
    // der erste Fall hier „IO91NP".
    void derAbsenderWirdAuchInSpitzenKlammernErkannt()
    {
        QCOMPARE(Ft8Applet::senderCallsign(
                     QStringLiteral("<OE1ABC> <OE5SOS> 590003 IO91NP")),
                 QStringLiteral("OE5SOS"));
        QCOMPARE(Ft8Applet::senderCallsign(
                     QStringLiteral("<OE1ABC> <OE5SOS> R 590003 IO91NP")),
                 QStringLiteral("OE5SOS"));
        // Unaufgeloest bleibt unaufgeloest: „<...>" ist kein
        // Rufzeichen, und etwas zu erfinden waere schlimmer als nichts.
        QCOMPARE(Ft8Applet::senderCallsign(
                     QStringLiteral("<...> <...> 590003 IO91NP")),
                 QString());
        // Und der gewoehnliche Weg bleibt, wie er war.
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ OE5SOS JN78")),
                 QStringLiteral("OE5SOS"));
    }

    // ── Der Abgriff ─────────────────────────────────────────────────

    void derSechsteAbgriffTraegtProben()
    {
        // Wie tst_audio_engine_tap_teardown_race: der Abgriff haengt am
        // AudioEngine DES Modells und an einer echten Scheibenkennung.
        RadioModel radio;
        AudioEngine* engine = radio.audioEngine();
        QVERIFY(engine);
        const int sliceId = radio.addSlice();

        AudioTapRing ring(4096);
        engine->setFt8Tap(&ring, sliceId);

        QVector<float> block(512);
        for (int i = 0; i < block.size(); ++i) {
            block[i] = (i % 2 == 0) ? 0.25f : -0.25f;
        }
        engine->rxBlockReady(sliceId, block.constData(), block.size() / 2);

        QVector<float> gelesen(4096);
        const int got = ring.read(gelesen.data(), gelesen.size());
        qInfo() << "aus dem Ring:" << got << "Werte";
        QVERIFY2(got > 0, "der Abgriff liefert nichts");

        // Gegenprobe: eine FREMDE Scheibe darf hier nicht hereinlaufen.
        engine->setFt8Tap(&ring, sliceId + 99);
        engine->rxBlockReady(sliceId, block.constData(), block.size() / 2);
        QCOMPARE(ring.read(gelesen.data(), gelesen.size()), 0);

        engine->setFt8Tap(nullptr, -1);
    }

    void eineEntfernteScheibeLoestDenAbgriff()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        // ZWEI Scheiben: RadioModel entfernt die letzte nie
        // (Phase-3F-Zusage), ein Lauf mit nur einer prueft also
        // versehentlich gar nichts.
        radio.addSlice();
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        applet.setSlice(slice);

        radio.removeSlice(id);
        QVERIFY2(radio.sliceById(id) == nullptr, "die Scheibe ist noch da");
        // Kein Absturz, und die Flaeche haelt keine Scheibe mehr: der
        // naechste Empfaenger mit diesem Index laeuft nicht hier
        // herein.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));
        QVERIFY(radio.rxDecodeModel());
        // .last() auf eine leere Liste ist undefiniert — ohne diese
        // Zeile bestand die Gegenprobe mit ausgehaengtem Modell
        // scheinbar.
        QVERIFY2(!radio.rxDecodeModel()->decodes().isEmpty(),
                 "kein Eintrag im Modell");
        const double mhz = radio.rxDecodeModel()->decodes().last().freqMhz;
        qInfo() << "nach dem Entfernen:" << mhz << "MHz";
        // Ohne Scheibe ist die Wahlfrequenz 0 — der Fund steht dann
        // nur mit seiner Tonlage da, nicht mit einer falschen
        // Bandfrequenz.
        QVERIFY2(mhz < 0.01, qPrintable(QString::number(mhz, 'f', 6)));
    }

    // ── Senden ──────────────────────────────────────────────────────

    /// Ein Druck auf „Send" bereitet die Nachricht vor und wartet auf
    /// das eigene Fenster — er tastet NICHT sofort.
    ///
    /// Das ist der Punkt, an dem ein Fehler teuer wird: wer hier
    /// sofort tastet, sendet mitten in die Empfangsperiode.
    void derSendeknopfTastetNichtSofort()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY(applet.cqButtonForTest());
        QVERIFY(applet.stopButtonForTest());
        QVERIFY(applet.textFeldForTest());

        // Ohne Text passiert nichts — und zwar ohne Absturz.
        applet.cqButtonForTest()->click();
        QVERIFY(!applet.sendetGerade());

        applet.textFeldForTest()->setText(QStringLiteral("CQ OE5SOS JN78"));
        applet.cqButtonForTest()->click();

        // Ohne Funkgeraet gibt es keinen Sendeweg: das Applet sagt es
        // und faengt NICHT an.
        if (!radio.ft8TxSource()) {
            QVERIFY2(!applet.sendetGerade(),
                     "ohne Sendeweg darf nichts anlaufen");
            qInfo() << "kein Sendeweg (nicht verbunden) —"
                    << applet.statusForTest()->text();
            return;
        }

        QVERIFY(applet.sendetGerade());
        QVERIFY2(!applet.cqButtonForTest()->isEnabled(),
                 "waehrend des Wartens darf man nicht zweimal druecken");
        QVERIFY(applet.stopButtonForTest()->isEnabled());

        applet.abbrechen();
        QVERIFY(!applet.sendetGerade());
        QVERIFY(applet.cqButtonForTest()->isEnabled());
        QVERIFY(!applet.stopButtonForTest()->isEnabled());
    }

    // ── Hound zieht die Tonlage hoch, und gibt sie zurueck ──────────
    //
    // „Hounds transmitting below f0+1000 Hz will not be answered."
    // Lieber hochziehen als warnen: eine Warnung, die man wegklickt,
    // laesst einen stundenlang ins Leere rufen.
    //
    // Der zweite Teil kommt vom laufenden Geraet (2026-10-30): der
    // Betreiber hat Hound ein- und wieder ausgeschaltet, und seine
    // 837 Hz waren weg. Eine Einstellung, die ich fuer eine
    // Betriebsart hochziehe, gehoert ihm zurueck.
    void houndZiehtDieTonlageHochUndGibtSieZurueck()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY(applet.sendeTonFeldForTest());
        QVERIFY(applet.houndButtonForTest());

        applet.sendeTonFeldForTest()->setValue(837);
        applet.houndButtonForTest()->click();
        QVERIFY2(applet.sendeTonFeldForTest()->value()
                     >= Ft8Ablauf::kHoundMinTonHz,
                 qPrintable(QStringLiteral("blieb bei %1 Hz")
                                .arg(applet.sendeTonFeldForTest()->value())));

        applet.houndButtonForTest()->click();
        QCOMPARE(applet.sendeTonFeldForTest()->value(), 837);
    }

    // GEGENPROBE: wer schon hoch genug steht, wird nicht angefasst --
    // und bekommt beim Ausschalten auch nichts „zurueckgegeben".
    void eineHoheTonlageBleibtUnberuehrt()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(2100);
        applet.houndButtonForTest()->click();
        QCOMPARE(applet.sendeTonFeldForTest()->value(), 2100);
        applet.houndButtonForTest()->click();
        QCOMPARE(applet.sendeTonFeldForTest()->value(), 2100);
    }

    // ── Die freie Sendestelle (2026-10-30) ──────────────────────────
    //
    // Longpath-eigen: weder WSJT-X noch MSHV suchen eine freie Stelle.
    // Diese Faelle pruefen den ANSCHLUSS -- die Rechnung selbst liegt
    // in tst_ft8_tonwahl. Genau dieser Anschluss hat bei Multi
    // Answering monatelang gefehlt, ohne dass es auffiel.

    void derKnopfFuerDieFreieStelleIstDa()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY2(applet.freieStelleButtonForTest(), "der Knopf fehlt");
        QVERIFY(!applet.freieStelleButtonForTest()->isCheckable());
    }

    void gehoerteStationenLandenInDerBelegungsliste()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY(applet.belegteToeneForTest().isEmpty());

        QVector<Longpath::Ft8Decode> funde;
        for (const int hz : {500, 900, 1800}) {
            Ft8Decode d;
            d.text   = QStringLiteral("CQ DL1ABC JO31");
            d.snrDb  = -10.0;
            d.freqHz = double(hz);
            funde.append(d);
        }
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>, funde),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));

        const QVector<int> belegt = applet.belegteToeneForTest();
        QCOMPARE(belegt.size(), 3);
        QVERIFY(belegt.contains(900));
    }

    void aufEinerBelegtenStelleWirdAusgewichen()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);

        Ft8Decode d;
        d.text   = QStringLiteral("CQ DL1ABC JO31");
        d.snrDb  = -10.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);

        QVERIFY2(applet.sucheFreieStelleForTest(true),
                 "auf 1500 lag jemand, es haette ausweichen muessen");
        QVERIFY(applet.sendeTonFeldForTest()->value() != 1500);
    }

    // GEGENPROBE: auf einer freien Stelle wird NICHT gewechselt. Wer
    // einen ruft, sucht ihn dort, wo er zuletzt war.
    void aufEinerFreienStelleBleibtEs()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);

        Ft8Decode d;
        d.text   = QStringLiteral("CQ DL1ABC JO31");
        d.snrDb  = -10.0;
        d.freqHz = 700.0;        // weit weg von 1500
        applet.fundForTest(d);

        QVERIFY(!applet.sucheFreieStelleForTest(true));
        QCOMPARE(applet.sendeTonFeldForTest()->value(), 1500);
    }

    // Und ohne einen einzigen Fund wird gar nichts verschoben --
    // Abwesenheit ist keine Aussage.
    void ohneFundeWirdNichtsVerschoben()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.sendeTonFeldForTest()->setValue(1500);
        QVERIFY(!applet.sucheFreieStelleForTest(false));
        QCOMPARE(applet.sendeTonFeldForTest()->value(), 1500);
    }

    // ── Der Contest ruft mich — und ich sehe es (2026-11-04) ───────
    //
    // Im EU-VHF-Contest stehen die Rufzeichen in spitzen Klammern.
    // `zeigeZeile` verglich bis heute `<OE5SOS>` mit `OE5SOS` und sagte
    // nie ja: die Zeile stand vor dem Betreiber, aber die Sicht
    // „An mich" blieb leer, und die Zeile war nicht hervorgehoben.
    // Im Contest heisst das: man wird gerufen und merkt es nicht.
    //
    // Gegenprobe: ohne `ohneSpitzeKlammern` in `zeigeZeile` bleibt die
    // Sicht „An mich" hier leer.
    void einContestrufAnMichWirdErkannt()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));

        Ft8Decode d;
        d.text   = QStringLiteral("<OE5SOS> <DL1ABC> 590003 JO31AB");
        d.snrDb  = -10.0;
        d.freqHz = 1200.0;
        applet.fundForTest(d);

        // Auf „An mich" schalten: dort darf nur stehen, was mich meint.
        applet.sichtAnMichButtonForTest()->click();
        const QString sichtbar = applet.listForTest()->toPlainText();
        QVERIFY2(sichtbar.contains(QStringLiteral("590003")),
                 qPrintable(QStringLiteral("Sicht „An mich\" ist leer: [%1]")
                                .arg(sichtbar)));
    }

    // GEGENPROBE: ein Contestruf an JEMAND ANDEREN darf dort NICHT
    // stehen. Ein Abstreifen, das jede Zeile durchlaesst, waere
    // schlimmer als keines.
    void einContestrufAnEinenAnderenStehtNichtDort()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));

        Ft8Decode d;
        d.text   = QStringLiteral("<DL9XYZ> <DL1ABC> 590007 JO31AB");
        d.snrDb  = -10.0;
        d.freqHz = 1300.0;
        applet.fundForTest(d);

        applet.sichtAnMichButtonForTest()->click();
        QVERIFY2(!applet.listForTest()->toPlainText()
                     .contains(QStringLiteral("590007")),
                 qPrintable(applet.listForTest()->toPlainText()));
    }

    void dieFlaecheLaesstSichAbbilden()
    {
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (grabDir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }
        RadioModel radio;
        Ft8Applet applet(&radio);
        // Groesze aus der Umgebung, damit dasselbe Werkzeug fuer
        // Gestaltungsblaetter taugt: dort zaehlt die WIRKLICHE Groesze
        // des Panels, nicht ein Briefmarkenabzug.
        const int breite = qEnvironmentVariableIntValue("LONGPATH_GRAB_W");
        const int hoehe  = qEnvironmentVariableIntValue("LONGPATH_GRAB_H");
        applet.resize(breite > 0 ? breite : 320, hoehe > 0 ? hoehe : 260);

        // Mehrere Funde, damit die Liste aussieht wie im Betrieb --
        // eine Zeile zeigt nicht, wo die Spalten klemmen. Die drei
        // Sorten, die das Applet unterschiedlich faerbt, sind dabei:
        // an mich gerichtet, ein CQ, und ein fremdes QSO.
        const char* zeilen[] = {
            "CQ OE5SOS JN78",
            "CQ DL1ABC JO31",
            "OE5SOS DL1ABC -07",
            "JA1XYZ W1AW RR73",
            "CQ DX IK6CGO JN63",
            "OE5SOS PD8O R-15",
            "CQ RA3TKS KO85",
            "F6HUK EA3IZF 73",
        };
        int i = 0;
        for (const char* z : zeilen) {
            QVector<Longpath::Ft8Decode> f =
                einFund(QString::fromLatin1(z));
            if (!f.isEmpty()) {
                f[0].snrDb  = -3.0 - double(i) * 2.5;
                f[0].freqHz = 500.0 + double(i) * 237.0;
                f[0].dtSeconds = 0.1 + double(i % 3) * 0.2;
            }
            QMetaObject::invokeMethod(
                &applet, "onDecoded", Qt::DirectConnection,
                Q_ARG(QVector<Longpath::Ft8Decode>, f),
                Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));
            ++i;
        }
        // Mit LONGPATH_GRAB_QSO=1 laeuft ein QSO: die Arbeitsleiste
        // zeigt erst dann, wofuer sie da ist. Ein Blatt im Leerlauf
        // beurteilt die leere Haelfte.
        if (qEnvironmentVariableIntValue("LONGPATH_GRAB_QSO") != 0) {
            applet.ablaufForTestSchreibend().starteAlsAntworter(
                QStringLiteral("PD8O"), QStringLiteral("OE5SOS"),
                QStringLiteral("JN67"), -15,
                QStringLiteral("JO22"));
            applet.ablaufForTestSchreibend().verarbeite(
                QStringLiteral("OE5SOS PD8O R-15"), -15);
            applet.zeigeAblaufForTest();
        }
        const QPixmap pm = applet.grab();
        QVERIFY(!pm.isNull());
        QVERIFY(pm.save(grabDir + QStringLiteral("/longpath-grab-Ft8Applet.png")));
    }
};

QTEST_MAIN(TstFt8Flaeche)
#include "tst_ft8_flaeche.moc"
