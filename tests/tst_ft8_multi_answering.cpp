// no-port-check: Longpath-eigener Pruefstand. Das VERFAHREN stammt aus
// MSHV (Herkunft im Kopf von core/Ft8Tx.h und in
// docs/attribution/MSHV-PROVENANCE.md); hier steht kein fremder Code.
// =================================================================
// tests/tst_ft8_multi_answering.cpp  (Longpath)
// =================================================================
//
// Mehrere Stationen in EINEM FT8-Fenster — MSHVs „Multi Answering".
//
// Betreiber am 2026-10-22, nach dem Vergleich der drei FT8-Programme:
// „mshv wäre meine erste wahl, sofern möglich."
//
// ── Warum dieser Prueflauf gegen den eigenen Dekoder faehrt ─────────
//
// Ein Sender laesst sich nicht daran pruefen, dass er Proben liefert —
// die liefert auch Rauschen. Der Rundlauf fasst dagegen Packung,
// Tonlage, Summierung und Pegel in einem Lauf: was hineingegeben
// wurde, muss wieder herauskommen. Dieselbe Begruendung wie beim
// RTTY-Rundlauf (PR #257).
//
// ── Was festgenagelt wird ───────────────────────────────────────────
//
//   * aus DREI Nachrichten in einem Fenster findet der eigene Dekoder
//     alle drei wieder — das ist der eigentliche Beweis
//   * die Tonleiter hat MSHVs 60 Hz Abstand und dessen Grenzen
//   * das Fenster wird NICHT laenger, nur voller
//   * der Pegel sinkt mit der Zahl der Plaetze (sonst uebersteuert es)
//   * eine unpackbare Nachricht laesst KEIN halbes Fenster stehen
//
// Modification history (Longpath):
//   2026-10-22 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
//   2026-11-17 — Fall „mehrere angeklickt": drei Doppelklicks gehen im
//                selben Fenster hinaus. Martin Fischer (OE5SOS),
//                KI-gestuetzt mit Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QSignalSpy>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QPlainTextEdit>
#include <QDateTime>

#include "core/Ft8Decoder.h"
#include "gui/applets/Ft8Applet.h"
#include "models/RadioModel.h"
#include "models/LogEntry.h"
#include "core/AppSettings.h"
#include "core/Ft8Tx.h"
#include "core/MoxController.h"
#include <QLabel>

using namespace Longpath;

class TstFt8MultiAnswering : public QObject
{
    Q_OBJECT

private:
    static constexpr auto kFt8 = Ft8Decoder::Protocol::Ft8;

    /// Das 12-kHz-Fenster aus N Nachrichten, so wie
    /// vorbereitenMehrere() es intern bildet — summiert und geteilt.
    static QVector<float> summiere(const QVector<QPair<QString, float>>& n)
    {
        QVector<float> summe;
        for (const auto& [text, ton] : n) {
            QVector<float> eine;
            if (!Ft8Decoder::encodeBlock(text, ton, kFt8, &eine)) { return {}; }
            if (summe.size() < eine.size()) { summe.resize(eine.size()); }
            for (int i = 0; i < eine.size(); ++i) { summe[i] += eine[i]; }
        }
        for (float& v : summe) { v /= static_cast<float>(n.size()); }
        return summe;
    }

    static float spitze(const QVector<float>& v)
    {
        float m = 0.0f;
        for (float x : v) { m = std::max(m, std::abs(x)); }
        return m;
    }

private slots:

    void dieTonleiterFolgtMshv()
    {
        // MSHV gen_ft8.cpp:262 — 60 Hz je Platz.
        const QVector<float> l = Ft8TxSource::tonleiter(1500.0f, 4);
        QCOMPARE(l.size(), 4);
        QCOMPARE(l.at(0), 1500.0f);
        QCOMPARE(l.at(1), 1560.0f);
        QCOMPARE(l.at(2), 1620.0f);
        QCOMPARE(l.at(3), 1680.0f);
    }

    void dieTonleiterHaeltSichAnDieGrenzen()
    {
        // Unten 100, oben 5000 — ebenda. Ohne die Klammer liefe ein
        // Platz aus dem Durchlassbereich und waere still.
        const QVector<float> tief = Ft8TxSource::tonleiter(50.0f, 2);
        QCOMPARE(tief.at(0), 100.0f);

        const QVector<float> hoch = Ft8TxSource::tonleiter(4950.0f, 3);
        QCOMPARE(hoch.at(0), 4950.0f);
        QCOMPARE(hoch.at(1), 5000.0f);
        QCOMPARE(hoch.at(2), 5000.0f);
    }

    void dreiStationenKommenAlleWiederHeraus()
    {
        // DER Beweis: drei Nachrichten hinein, drei heraus.
        const QVector<QPair<QString, float>> plaetze = {
            {QStringLiteral("DL1ABC OE5SOS JN67"), 1500.0f},
            {QStringLiteral("DL2DEF OE5SOS JN67"), 1560.0f},
            {QStringLiteral("DL3GHI OE5SOS JN67"), 1620.0f},
        };

        const QVector<float> fenster = summiere(plaetze);
        QVERIFY2(!fenster.isEmpty(), "das Fenster liess sich nicht bilden");

        const QVector<Ft8Decode> funde =
            Ft8Decoder::decodeBlock(fenster.constData(), fenster.size(), kFt8);

        QStringList gefunden;
        for (const Ft8Decode& f : funde) { gefunden << f.text.trimmed(); }

        for (const auto& [text, ton] : plaetze) {
            QVERIFY2(gefunden.contains(text),
                     qPrintable(QStringLiteral("fehlt: %1 — gefunden: %2")
                                    .arg(text, gefunden.join(QStringLiteral(" | ")))));
        }
    }

    void dasFensterWirdNichtLaengerNurVoller()
    {
        // Drei Stationen duerfen nicht die dreifache Sendezeit kosten —
        // sie teilen sich DASSELBE Fenster. Waere es laenger, liefe der
        // Sender in das Empfangsfenster hinein.
        Ft8TxSource eine(48000);
        QVERIFY(eine.vorbereiten(QStringLiteral("DL1ABC OE5SOS JN67"),
                                 1500.0f, kFt8));
        const int allein = eine.probenImFenster();

        Ft8TxSource drei(48000);
        QVERIFY(drei.vorbereitenMehrere(
            {{QStringLiteral("DL1ABC OE5SOS JN67"), 1500.0f},
             {QStringLiteral("DL2DEF OE5SOS JN67"), 1560.0f},
             {QStringLiteral("DL3GHI OE5SOS JN67"), 1620.0f}},
            kFt8));

        QCOMPARE(drei.probenImFenster(), allein);
    }

    void derPegelSinktMitDerZahlDerPlaetze()
    {
        // MSHV teilt durch die Zahl der Plaetze (gen_ft8.cpp:303). Ohne
        // das uebersteuert jedes Fenster mit mehr als einer Station.
        // Also: die Summe aus zweien darf nicht lauter sein als eine.
        QVector<float> eine;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("DL1ABC OE5SOS JN67"),
                                        1500.0f, kFt8, &eine));
        const float spitzeEine = spitze(eine);

        const QVector<float> zwei = summiere(
            {{QStringLiteral("DL1ABC OE5SOS JN67"), 1500.0f},
             {QStringLiteral("DL2DEF OE5SOS JN67"), 1560.0f}});
        QVERIFY(!zwei.isEmpty());

        QVERIFY2(spitze(zwei) <= spitzeEine * 1.01f,
                 qPrintable(QStringLiteral("zwei Plaetze sind lauter als einer: "
                                           "%1 gegen %2")
                                .arg(spitze(zwei)).arg(spitzeEine)));
    }

    void eineUnpackbareNachrichtLaesstKeinHalbesFenster()
    {
        // Die dritte ist Unsinn. Dann darf NICHTS vorbereitet sein --
        // ein Fenster, in dem stillschweigend eine Station fehlt, ist
        // schlimmer als keines.
        Ft8TxSource quelle(48000);
        QVERIFY(quelle.vorbereiten(QStringLiteral("DL1ABC OE5SOS JN67"),
                                   1500.0f, kFt8));
        const int vorher = quelle.probenImFenster();
        QVERIFY(vorher > 0);

        const bool ging = quelle.vorbereitenMehrere(
            {{QStringLiteral("DL1ABC OE5SOS JN67"), 1500.0f},
             {QStringLiteral("!!! kein gueltiges FT8 !!! viel zu lang und krumm"), 1560.0f}},
            kFt8);

        QVERIFY2(!ging, "eine unpackbare Nachricht ging durch");
        QVERIFY2(!quelle.laeuft(), "es laeuft trotz Fehlschlag");
    }

    // ── Und jetzt ist es auch ERREICHBAR (2026-10-30) ───────────────
    //
    // Die Faelle darueber pruefen `Ft8Mehrfach` und
    // `Ft8TxSource::vorbereitenMehrere` fuer sich. Beide waren seit
    // dem Bau (#323/#333/#335) vollstaendig und gruen -- und wurden
    // von NIEMANDEM gerufen: sie kamen nur in ihrer eigenen Datei und
    // in diesen Prueflaeufen vor. In der Bilanz gegen MSHV stand
    // trotzdem „gebaut".
    //
    // Betreiber am 2026-10-30: „wie funktioniert mshv nun bei mir" --
    // und beim Nachsehen war die Antwort: gar nicht. Diese Faelle
    // nageln den Anschluss fest, damit das nicht noch einmal
    // unbemerkt abreiszt.

    void dasAppletHatEinenSchalterDafuer()
    {
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY2(applet.plaetzeSpinForTest(), "der Schalter fehlt");
        QCOMPARE(applet.plaetzeSpinForTest()->minimum(), 1);
        QCOMPARE(applet.plaetzeSpinForTest()->maximum(),
                 Ft8Mehrfach::kMaxPlaetze);
        // Ab Werk AUS: die Sendeleistung wird geteilt, das waehlt
        // niemand versehentlich.
        QCOMPARE(applet.plaetzeSpinForTest()->value(), 1);
    }

    void derSchalterStelltDieZahlDerPlaetzeDurch()
    {
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
        RadioModel radio;
        Ft8Applet applet(&radio);
        QCOMPARE(applet.mehrfachForTest().plaetze(), 1);
        applet.plaetzeSpinForTest()->setValue(3);
        QCOMPARE(applet.mehrfachForTest().plaetze(), 3);
    }

    void zweiAnruferLandenAufZweiPlaetzen()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("3"));
        RadioModel radio;
        Ft8Applet applet(&radio);
        QCOMPARE(applet.mehrfachForTest().plaetze(), 3);

        const auto fund = [&applet](const char* text, double snr) {
            Ft8Decode d;
            d.text   = QString::fromLatin1(text);
            d.snrDb  = snr;
            d.freqHz = 1500.0;
            applet.fundForTest(d);
        };

        fund("OE5SOS DL1ABC JO31", -7.0);
        fund("OE5SOS SP9XYZ JO90", -12.0);
        QCOMPARE(applet.mehrfachForTest().aktive(), 2);
        QVERIFY(applet.mehrfachForTest().kennt(QStringLiteral("DL1ABC")));
        QVERIFY(applet.mehrfachForTest().kennt(QStringLiteral("SP9XYZ")));

        // GEGENPROBE: derselbe Ruf noch einmal legt keinen zweiten
        // Platz an -- sonst antwortete man demselben zweimal in
        // demselben Fenster, auf zwei Toenen.
        fund("OE5SOS DL1ABC JO31", -7.0);
        QCOMPARE(applet.mehrfachForTest().aktive(), 2);
    }

    // Und die Gegenprobe zum Schalter: mit EINEM Platz bleibt alles
    // beim einzelnen Ablauf. Ohne diesen Lauf belegt der Fall
    // darueber nicht, dass der Schalter etwas aendert.
    void mitEinemPlatzLaeuftDerGewoehnlicheWeg()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
        RadioModel radio;
        Ft8Applet applet(&radio);

        Ft8Decode d;
        d.text   = QStringLiteral("OE5SOS DL1ABC JO31");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);

        QCOMPARE(applet.mehrfachForTest().aktive(), 0);
    }


    // ── Vier Fehler aus der Durchsicht vom 2026-10-30 ───────────────
    //
    // Alle vier wurden eine Stunde nach dem Anschluss gefunden, beim
    // ruhigen Durchlesen. Sie haben hier ihren Prueffall, damit
    // keiner davon still zurueckkommt.

    // 1. Der schwerste: der Block in `fensterZuEnde` war NIE
    //    geschrieben worden (eine Ersetzung war still
    //    fehlgeschlagen). Multi Answering sendete, aber kein Zug kam
    //    je voran und kein QSO landete im Logbuch.
    void einPlatzKommtUeberDasFensterVoran()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("3"));
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);

        Ft8Decode d;
        d.text   = QStringLiteral("OE5SOS DL1ABC JO31");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
        QCOMPARE(applet.mehrfachForTest().aktive(), 1);

        // Ein Fenster vergeht, ohne dass etwas kam: der Zaehler des
        // Platzes muss laufen. Ohne den fehlenden Block geschah hier
        // GAR NICHTS.
        const auto vorher = applet.mehrfachForTest().belegung();
        QCOMPARE(vorher.size(), 1);
        for (int i = 0; i < 6; ++i) { applet.fensterZuEndeForTest(); }
        QVERIFY2(applet.mehrfachForTest().aktive() == 0,
                 "nach sechs leeren Fenstern muss der Platz aufgeben");
    }

    void einFertigesQsoAufEinemPlatzLandetImLogbuch()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("3"));
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);

        const auto fund = [&applet](const char* t, double snr) {
            Ft8Decode d;
            d.text   = QString::fromLatin1(t);
            d.snrDb  = snr;
            d.freqHz = 1500.0;
            applet.fundForTest(d);
        };

        fund("OE5SOS DL1ABC JO31", -7.0);     // er ruft mich
        fund("OE5SOS DL1ABC R-15", -7.0);     // er bestaetigt
        fund("OE5SOS DL1ABC RR73", -7.0);     // und ist fertig
        applet.fensterZuEndeForTest();

        QCOMPARE(gemeldet.count(), 1);
        const auto e = gemeldet.first().first().value<LogEntry>();
        QCOMPARE(e.call, QStringLiteral("DL1ABC"));
    }

    // 2. Stop hielt die Plaetze nicht an.
    void stopHaeltAuchDiePlaetzeAn()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("3"));
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);

        Ft8Decode d;
        d.text   = QStringLiteral("OE5SOS DL1ABC JO31");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
        QCOMPARE(applet.mehrfachForTest().aktive(), 1);

        applet.stopButtonForTest()->click();
        QCOMPARE(applet.mehrfachForTest().aktive(), 0);
    }

    // 3. Der Protokollwechsel liess die gehoerten Tonlagen stehen.
    void derProtokollwechselVergisstDieTonlagen()
    {
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
        RadioModel radio;
        Ft8Applet applet(&radio);

        Ft8Decode d;
        d.text   = QStringLiteral("CQ DL1ABC JO31");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
        QVERIFY(!applet.belegteToeneForTest().isEmpty());

        // FT4 liegt auf einer anderen Frequenz -- 1500 Hz dort ist
        // eine andere Stelle im Band.
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        QVERIFY2(applet.belegteToeneForTest().isEmpty(),
                 "die alten Tonlagen gelten auf der neuen Frequenz nicht");
    }


    // 5. Zwei Nachrichten duerfen nie auf derselben Tonlage liegen.
    //
    // Beim Durchdenken am 2026-10-30 gefunden: das CQ bekam
    // `toene[plaetze.size()]`, aber `Ft8Mehrfach` vergibt seine Toene
    // nach dem Index im eigenen Feld. Faellt ein aktiver Ablauf mit
    // leerer Nachricht heraus, stimmen die Zahlen nicht mehr ueberein
    // -- und das CQ laege auf der Tonlage einer laufenden Station.
    // Zwei Nachrichten auf einem Ton ueberlagern sich zu Matsch, und
    // beide sind weg.
    void keineZweiNachrichtenAufDerselbenTonlage()
    {
        const QVector<float> toene = Ft8TxSource::tonleiter(1500.0f, 6);
        QCOMPARE(toene.size(), 6);
        for (int i = 0; i < toene.size(); ++i) {
            for (int j = i + 1; j < toene.size(); ++j) {
                QVERIFY2(!qFuzzyCompare(toene[i] + 1.0f, toene[j] + 1.0f),
                         qPrintable(QStringLiteral("Ton %1 und %2 sind "
                                                   "gleich: %3")
                                        .arg(i).arg(j).arg(toene[i])));
            }
        }
        // Und der Abstand ist der, mit dem MSHV arbeitet.
        QCOMPARE(toene[1] - toene[0], Ft8TxSource::kPlatzAbstandHz);
    }

    // ── Der Einstieg eines Anrufers (2026-10-30) ──────────────────
    //
    // Diese Pruefung gab es nicht, und darum lief der Fehler Monate
    // mit: `nimmAnrufer` nahm `starteAlsAntworter`, und das beginnt
    // bei Tx1. Wer mich gerufen hat, bekam meinen Locator statt
    // seines Rapports -- ein Durchgang mehr je QSO.
    void einAnruferBekommtDenRapportUndNichtMeinenLocator()
    {
        Ft8Mehrfach m;
        m.setEigeneDaten(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        m.setPlaetze(3);

        // Genau die Zeile aus dem Fenster vom 2026-10-30:
        //   124945  12 1.2 1338  OE5SOS PD4RJ JO33
        QVERIFY(m.nimmAnrufer(QStringLiteral("PD4RJ"), 12,
                              QStringLiteral("JO33")));

        const QVector<Ft8TxSource::Platz> zuege = m.naechsteNachrichten();
        QCOMPARE(zuege.size(), 1);
        QCOMPARE(zuege.first().text,
                 QStringLiteral("PD4RJ OE5SOS +12"));

        const QVector<Ft8Mehrfach::Belegung> b = m.belegung();
        QCOMPARE(b.size(), 1);
        QCOMPARE(b.first().partner, QStringLiteral("PD4RJ"));
        QCOMPARE(b.first().zustand, Ft8Ablauf::Zustand::SendeReport);
    }

    // Die Gegenprobe: ein negativer Rapport wird auch so geschrieben,
    // und der Locator der Gegenstation geht nicht verloren (er
    // gehoert ins Logbuch, nicht in die Nachricht).
    void derRapportTraegtSeinVorzeichen()
    {
        Ft8Mehrfach m;
        m.setEigeneDaten(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(m.nimmAnrufer(QStringLiteral("9A6T"), -6, QString()));
        QCOMPARE(m.naechsteNachrichten().first().text,
                 QStringLiteral("9A6T OE5SOS -06"));
    }


    // =================================================================
    // 2026-11-17 — Die Plaetze AUF SENDUNG
    // =================================================================
    //
    // Alles darueber prueft, was die Plaetze RECHNEN. Was davon wirklich
    // hinausgeht, pruefte nichts -- und genau dort lag es: die
    // Nachpruefung kurz vor dem Senden packte bei jeder Sendung den Satz
    // des einzelnen Ablaufs ueber das fertige Paket.
    //
    // Am Geraet (17 m, 3 Plaetze, CQ laeuft), aus dem Sitzungsprotokoll:
    //   09:15:13  Senden vorbereitet (2 Plaetze):
    //             US2YW OE5SOS -15@2430 | CQ OE5SOS JN67@2490
    //   09:15:30  Zug in der Frist gewechselt:
    //             US2YW OE5SOS -15 | CQ OE5SOS JN67 -> CQ OE5SOS JN67
    // 27 Sendungen, 27-mal dasselbe. Betreiber: „die automatischen
    // antworten meines cq call funktionieren irgendwie nicht."
    //
    // Darum fragen diese Laeufe die TONQUELLE, was sie gepackt hat --
    // nachdem der Sendetakt abgelaufen ist, also nach der Nachpruefung.

private:
    static void stelleEin(int plaetze)
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QString::number(plaetze));
    }

    static void fund(Ft8Applet& applet, const char* text, double snr)
    {
        Ft8Decode d;
        d.text   = QString::fromLatin1(text);
        d.snrDb  = snr;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
    }

    /// Den Sendetakt ablaufen lassen, als waere die Fenstergrenze da.
    /// Danach hat die Nachpruefung entschieden, was hinausgeht.
    static bool sendetaktAbgelaufen(Ft8Applet& applet)
    {
        QTimer* t = applet.sendeTimerForTest();
        if (!t || !t->isActive()) { return false; }
        t->stop();
        t->start(0);
        return QTest::qWaitFor([t]() { return !t->isActive(); }, 3000);
    }

    /// Was die Tonquelle gepackt hat: die Saetze, mit „#" getrennt.
    static QString aufSendung(RadioModel& radio)
    {
        return radio.ft8TxSource() ? radio.ft8TxSource()->nachricht()
                                   : QStringLiteral("(keine Tonquelle)");
    }

    static QString letzteZeile(Ft8Applet& applet)
    {
        const QStringList z = applet.listForTest()->toPlainText()
                                  .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        return z.isEmpty() ? QString() : z.last();
    }

private slots:

    // Der Fall vom Geraet, Zug um Zug. Jede Zeile „aufSendung" ist eine,
    // die vor der Behebung „CQ OE5SOS JN67" lautete.
    void dieAntwortGehtHinausUndNichtNurDasCq()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.cqVersucheSpinForTest()->setValue(10);
        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);

        applet.cqRufenButtonForTest()->click();
        QVERIFY2(applet.sendetGerade(), "der CQ-Ruf muss eingeplant sein");
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio), QStringLiteral("CQ OE5SOS JN67"));
        applet.fensterZuEndeForTest();
        QVERIFY2(applet.sendetGerade(), "das naechste CQ muss eingeplant sein");

        // Er antwortet, waehrend das naechste CQ schon gepackt ist. Die
        // Antwort geht in DIESEM Fenster hinaus -- und das CQ daneben.
        fund(applet, "OE5SOS US2YW KN28", -15.0);
        QCOMPARE(applet.mehrfachForTest().aktive(), 1);
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio),
                 QStringLiteral("US2YW OE5SOS -15#CQ OE5SOS JN67"));
        applet.fensterZuEndeForTest();

        // Er bestaetigt. Jetzt ist „US2YW OE5SOS -15 | CQ" gepackt, und
        // die Nachpruefung muss daraus das RR73 machen -- NICHT das CQ
        // allein. Das ist die Zeile aus dem Protokoll.
        fund(applet, "OE5SOS US2YW R-09", -15.0);
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio),
                 QStringLiteral("US2YW OE5SOS RR73#CQ OE5SOS JN67"));
        QCOMPARE(gemeldet.count(), 0);      // das RR73 geht eben erst hinaus

        applet.fensterZuEndeForTest();
        QCOMPARE(gemeldet.count(), 1);
        QCOMPARE(gemeldet.first().first().value<LogEntry>().call,
                 QStringLiteral("US2YW"));

        // Und der Lauf geht weiter: wieder CQ, ohne Klick.
        QVERIFY2(applet.sendetGerade(), "nach dem QSO muss weiter gerufen werden");
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio), QStringLiteral("CQ OE5SOS JN67"));

        applet.stopButtonForTest()->click();
    }

    // Drei rufen zugleich: alle drei bekommen ihren Rapport in EINEM
    // Fenster. Am Geraet riefen RA5CW und 9A2KS zusammen achtmal.
    void dreiAnruferBekommenAlleIhrenRapport()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.cqVersucheSpinForTest()->setValue(10);

        applet.cqRufenButtonForTest()->click();
        fund(applet, "OE5SOS JA2FRO PM84", -21.0);
        fund(applet, "OE5SOS RA5CW MO85", -6.0);
        fund(applet, "OE5SOS 9A2KS JN65", -8.0);
        QCOMPARE(applet.mehrfachForTest().aktive(), 3);

        QVERIFY(sendetaktAbgelaufen(applet));
        // Alle Plaetze belegt: das CQ schweigt, die drei gehen hinaus.
        QCOMPARE(aufSendung(radio),
                 QStringLiteral("JA2FRO OE5SOS -21#RA5CW OE5SOS -06"
                                "#9A2KS OE5SOS -08"));
        applet.stopButtonForTest()->click();
    }

    // Ein Rapport OHNE „R" heisst: meiner ist nicht angekommen. Darauf
    // RR73 zu senden schrieb JA2FRO ins Logbuch, der nie eine
    // Bestaetigung bekommen hatte.
    void einUnbestaetigterRapportIstKeinQso()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);

        fund(applet, "OE5SOS JA2FRO PM84", -21.0);
        fund(applet, "OE5SOS JA2FRO -15", -24.0);
        QCOMPARE(applet.mehrfachForTest().belegung().first().zustand,
                 Ft8Ablauf::Zustand::SendeRReport);

        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio), QStringLiteral("JA2FRO OE5SOS R-21"));
        applet.fensterZuEndeForTest();
        QCOMPARE(gemeldet.count(), 0);

        // Erst sein RR73 macht es zum QSO.
        fund(applet, "OE5SOS JA2FRO RR73", -24.0);
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio), QStringLiteral("JA2FRO OE5SOS 73"));
        applet.fensterZuEndeForTest();
        QCOMPARE(gemeldet.count(), 1);
        applet.stopButtonForTest()->click();
    }

    // Wer sich verabschiedet, ist kein neuer Anrufer -- und wer eben
    // gearbeitet wurde, wird nicht gleich noch einmal gearbeitet. Am
    // Geraet stand EY7AD dreimal in zwei Minuten auf einem Platz.
    void werEbenGearbeitetWurdeBekommtKeinenNeuenPlatz()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);

        fund(applet, "OE5SOS EY7AD MM48", 12.0);
        fund(applet, "OE5SOS EY7AD R+05", 12.0);
        applet.fensterZuEndeForTest();          // das RR73 ist draussen
        QCOMPARE(gemeldet.count(), 1);
        QCOMPARE(applet.mehrfachForTest().aktive(), 0);

        fund(applet, "OE5SOS EY7AD 73", 12.0);  // sein Abschied
        QCOMPARE(applet.mehrfachForTest().aktive(), 0);
        fund(applet, "OE5SOS EY7AD R+05", 12.0);    // er wiederholt
        QCOMPARE(applet.mehrfachForTest().aktive(), 0);
        // Und bloss erwaehnt zu sein ist kein Anruf.
        fund(applet, "K1ABC OE5SOS -10", 3.0);
        QCOMPARE(applet.mehrfachForTest().aktive(), 0);

        QCOMPARE(gemeldet.count(), 1);
        applet.stopButtonForTest()->click();
    }

    // Der Doppelklick mit Plaetzen. Vorher ging er an den einzelnen
    // Ablauf, der im Mehrfachbetrieb keinen Fund zu sehen bekommt: das
    // QSO blieb bei „waiting for his report" stehen, und der laufende
    // CQ-Ruf war weg.
    void derDoppelklickNimmtEinenPlatzUndDasCqLaeuftWeiter()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.cqVersucheSpinForTest()->setValue(10);

        applet.cqRufenButtonForTest()->click();
        fund(applet, "CQ DL1ABC JO31", -7.0);
        const QString zeile = letzteZeile(applet);
        QVERIFY2(zeile.contains(QStringLiteral("CQ DL1ABC JO31")),
                 qPrintable(zeile));

        QCOMPARE(applet.nimmFundAn(zeile), QString());
        QCOMPARE(applet.mehrfachForTest().aktive(), 1);
        QCOMPARE(applet.mehrfachForTest().belegung().first().zustand,
                 Ft8Ablauf::Zustand::RufeAn);
        // Der CQ-Ruf ist NICHT ueberschrieben.
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeCq);

        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio),
                 QStringLiteral("DL1ABC OE5SOS JN67#CQ OE5SOS JN67"));
        applet.fensterZuEndeForTest();

        // Und seine Antwort KOMMT AN -- das war der eigentliche Schaden.
        fund(applet, "OE5SOS DL1ABC -12", -7.0);
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio),
                 QStringLiteral("DL1ABC OE5SOS R-07#CQ OE5SOS JN67"));

        // Ein zweiter Doppelklick auf denselben nimmt keinen zweiten Platz.
        QVERIFY(!applet.nimmFundAn(zeile).isEmpty());
        QCOMPARE(applet.mehrfachForTest().aktive(), 1);
        applet.stopButtonForTest()->click();
    }

    // Mehrere Stationen nacheinander anklicken (Betreiber 2026-11-17,
    // slots 3): „wenn ich mehrere Stationen gleichzeitig anklicke glaube
    // ich nicht, dass alle gleichzeitig beantwortet werden". Sein
    // Protokoll 11:07: R2ZDO vorbereitet, 16 s spaeter „Zug in der Frist
    // gewechselt: R2ZDO … -> TF4CU …" -- der zweite Klick ersetzte den
    // ersten. Ohne laufendes CQ, drei Klicks, drei Plaetze: alle drei
    // muessen im SELBEN Fenster hinausgehen.
    void mehrereAngeklickteGehenAlleGleichzeitigHinaus()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);

        fund(applet, "CQ R2ZDO KO85", -9.0);
        const QString z1 = letzteZeile(applet);
        fund(applet, "CQ TF4CU HP94", -14.0);
        const QString z2 = letzteZeile(applet);
        fund(applet, "CQ T77RN JN63", -11.0);
        const QString z3 = letzteZeile(applet);
        QVERIFY2(z1.contains(QStringLiteral("R2ZDO")), qPrintable(z1));
        QVERIFY2(z2.contains(QStringLiteral("TF4CU")), qPrintable(z2));
        QVERIFY2(z3.contains(QStringLiteral("T77RN")), qPrintable(z3));

        QCOMPARE(applet.nimmFundAn(z1), QString());
        QCOMPARE(applet.nimmFundAn(z2), QString());
        QCOMPARE(applet.nimmFundAn(z3), QString());
        QCOMPARE(applet.mehrfachForTest().aktive(), 3);

        QVERIFY(sendetaktAbgelaufen(applet));
        const QString gesendet = aufSendung(radio);
        QVERIFY2(gesendet.contains(QStringLiteral("R2ZDO OE5SOS JN67"))
                     && gesendet.contains(QStringLiteral("TF4CU OE5SOS JN67"))
                     && gesendet.contains(QStringLiteral("T77RN OE5SOS JN67")),
                 qPrintable(QStringLiteral("auf Sendung: ") + gesendet));
        applet.fensterZuEndeForTest();

        // Und alle drei antworten im naechsten Fenster -- jeder bekommt
        // seinen eigenen naechsten Zug, keiner faellt heraus.
        fund(applet, "OE5SOS R2ZDO -05", -9.0);
        fund(applet, "OE5SOS TF4CU -12", -14.0);
        fund(applet, "OE5SOS T77RN -08", -11.0);
        QVERIFY(sendetaktAbgelaufen(applet));
        const QString zwei = aufSendung(radio);
        QVERIFY2(zwei.contains(QStringLiteral("R2ZDO OE5SOS R-09"))
                     && zwei.contains(QStringLiteral("TF4CU OE5SOS R-14"))
                     && zwei.contains(QStringLiteral("T77RN OE5SOS R-11")),
                 qPrintable(QStringLiteral("zweites Fenster: ") + zwei));
        applet.stopButtonForTest()->click();
    }

    // Wer MICH ruft, bekommt auch auf Doppelklick den Rapport und nicht
    // meinen Locator. Am Geraet: Doppelklick auf „OE5SOS JA2FRO PM84"
    // sendete sechsmal „JA2FRO OE5SOS JN67".
    void derDoppelklickAufEinenAnruferGibtDenRapport()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);

        // Abgewiesen, damit er nicht von selbst auf einen Platz geht --
        // der Doppelklick ist dann die Entscheidung des Betreibers.
        applet.schlangeForTestSchreibend().weiseAb(
            QStringLiteral("JA2FRO"), QDateTime::currentMSecsSinceEpoch());
        fund(applet, "OE5SOS JA2FRO PM84", -22.0);
        QCOMPARE(applet.mehrfachForTest().aktive(), 0);

        const QString zeile = letzteZeile(applet);
        QVERIFY2(zeile.contains(QStringLiteral("OE5SOS JA2FRO PM84")),
                 qPrintable(zeile));
        QCOMPARE(applet.nimmFundAn(zeile), QString());
        QCOMPARE(applet.mehrfachForTest().belegung().first().zustand,
                 Ft8Ablauf::Zustand::SendeReport);
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio), QStringLiteral("JA2FRO OE5SOS -22"));
        applet.stopButtonForTest()->click();
    }

    // War der CQ-Zaehler waehrend eines QSOs abgelaufen, blieb es nach
    // dem RR73 still. Beim einzelnen Ablauf ruft es danach von selbst
    // weiter; mit Plaetzen jetzt auch.
    void nachEinemFertigenQsoZaehltDasCqVonVorn()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.cqVersucheSpinForTest()->setValue(2);

        applet.cqRufenButtonForTest()->click();
        QVERIFY(sendetaktAbgelaufen(applet));
        applet.fensterZuEndeForTest();              // Versuch 1 ist um

        fund(applet, "OE5SOS DL1ABC JO31", -7.0);
        QVERIFY(sendetaktAbgelaufen(applet));
        applet.fensterZuEndeForTest();              // Versuch 2: CQ zu Ende
        QVERIFY2(!applet.ablaufForTest().laeuft(),
                 "die Voraussetzung: der CQ-Ruf ist waehrend des QSOs "
                 "abgelaufen");

        fund(applet, "OE5SOS DL1ABC R-15", -7.0);
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio), QStringLiteral("DL1ABC OE5SOS RR73"));
        applet.fensterZuEndeForTest();              // QSO fertig

        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeCq);
        QVERIFY2(applet.sendetGerade(), "es muss von selbst weitergehen");
        QVERIFY(sendetaktAbgelaufen(applet));
        QCOMPARE(aufSendung(radio), QStringLiteral("CQ OE5SOS JN67"));
        applet.stopButtonForTest()->click();
    }

    // Wer warten musste, weil alle Plaetze belegt waren, bekommt beim
    // Nachruecken DENSELBEN ersten Zug wie einer, der gleich drankam:
    // den Rapport. Vorher bekam er meinen Locator.
    void werNachruecktBekommtDenRapport()
    {
        Ft8Mehrfach m;
        m.setEigeneDaten(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        m.setPlaetze(2);
        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -7));
        QVERIFY(m.nimmAnrufer(QStringLiteral("SP9XYZ"), -12));
        QVERIFY(m.nimmAnrufer(QStringLiteral("PD4RJ"), 3));   // wartet
        QCOMPARE(m.aktive(), 2);
        QCOMPARE(m.wartende(), 1);

        // DL1ABC verabschiedet sich: der Platz wird frei.
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS DL1ABC RR73"), -7), 1);
        m.raeumeAuf();
        QCOMPARE(m.wartende(), 0);
        QCOMPARE(m.nimmFertige().size(), 1);

        QStringList zuege;
        for (const Ft8TxSource::Platz& p : m.naechsteNachrichten()) {
            zuege << p.text;
        }
        QVERIFY2(zuege.contains(QStringLiteral("PD4RJ OE5SOS +03")),
                 qPrintable(zuege.join(QStringLiteral(" | "))));
    }

    // Von Hand genommen (er hat MICH nicht gerufen): mein Ruf mit
    // Locator, kein Rapport an jemanden, der nie gefragt hat.
    void vonHandBeginntMitDemLocator()
    {
        Ft8Mehrfach m;
        m.setEigeneDaten(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        m.setPlaetze(1);
        QVERIFY(m.nimmVonHand(QStringLiteral("DL1ABC"), -7));
        QVERIFY(!m.nimmVonHand(QStringLiteral("DL1ABC"), -7));
        QCOMPARE(m.naechsteNachrichten().first().text,
                 QStringLiteral("DL1ABC OE5SOS JN67"));

        // Und aus der Schlange genauso.
        QVERIFY(m.nimmVonHand(QStringLiteral("SP9XYZ"), -12));
        QCOMPARE(m.wartende(), 1);
        for (int i = 0; i < Ft8Ablauf::kMaxWiederholungen; ++i) {
            m.fensterVerstrichen();
        }
        QCOMPARE(m.naechsteNachrichten().first().text,
                 QStringLiteral("SP9XYZ OE5SOS JN67"));
    }

    // `raeumeAuf` zaehlt KEIN Fenster: sonst gaebe ein Platz, der
    // zwischen zwei Sendungen nur nachgesehen wird, frueher auf.
    void aufraeumenZaehltKeinFenster()
    {
        Ft8Mehrfach m;
        m.setEigeneDaten(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        m.setPlaetze(2);
        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -7));
        for (int i = 0; i < 3 * Ft8Ablauf::kMaxWiederholungen; ++i) {
            m.raeumeAuf();
        }
        QCOMPARE(m.aktive(), 1);
    }

    // =================================================================
    // Was das GERAET verweigert, darf nicht als gesendet dastehen
    // =================================================================
    //
    // Am Geraet (2026-11-17, Flex, 15 m): der Stehwellenriegel brach die
    // erste Sendung nach 0,5 s und tastete danach gar nicht mehr —
    // richtig so. Das Fenster schrieb trotzdem alle 30 s „Tx", zeigte
    // „transmitting" und liess den Ablauf weiterzaehlen. Betreiber:
    // „da steht zwar, dass gesendet wird, aber es geht keine Leistung
    // raus!"

private:
    /// Wie das Hauptfenster am Flex: beim Umlegen der Hardware wird
    /// `flexTasten(true)` gerufen. Ohne Verbindung sagt es nein — das
    /// erste der Tore, und das einzige, das ohne Geraet zu erreichen
    /// ist. Der Weg des Signals ist fuer alle Tore derselbe.
    static QMetaObject::Connection tasteWieAmFlex(RadioModel& radio)
    {
        return QObject::connect(
            radio.moxController(), &MoxController::hardwareFlipped,
            &radio, [&radio](bool isTx) {
                if (isTx) { radio.flexTasten(true); }
            });
    }

    static int txZeilen(Ft8Applet& applet)
    {
        int n = 0;
        const QStringList z = applet.listForTest()->toPlainText()
                                  .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString& zeile : z) {
            if (zeile.contains(QStringLiteral(" Tx "))) { ++n; }
        }
        return n;
    }

private slots:

    void verweigertDasGeraetDasTastenHaeltDerLaufAn()
    {
        stelleEin(1);
        RadioModel radio;
        radio.installFt8TxForTest();
        QVERIFY(radio.moxController());
        tasteWieAmFlex(radio);
        Ft8Applet applet(&radio);
        applet.cqVersucheSpinForTest()->setValue(10);

        applet.cqRufenButtonForTest()->click();
        QVERIFY(applet.sendetGerade());
        QVERIFY(sendetaktAbgelaufen(applet));

        // Nichts ging hinaus — also steht auch nichts da.
        QVERIFY2(!applet.sendetGerade(), "das Fenster haelt sich noch fuer sendend");
        QVERIFY2(!applet.ablaufForTest().laeuft(),
                 "der Ablauf zaehlt weiter, obwohl nie getastet wurde");
        QCOMPARE(txZeilen(applet), 0);
        const QString grund = applet.statusForTest()->text();
        QVERIFY2(grund.contains(QStringLiteral("not transmitting")),
                 qPrintable(grund));
        QVERIFY2(grund.contains(QStringLiteral("no connection")),
                 qPrintable(grund));
        // Kein neuer Versuch im naechsten Fenster.
        QVERIFY(!applet.sendeTimerForTest()->isActive());
        QTRY_VERIFY(!radio.moxController()->isMox());

        // Und der Grund BLEIBT stehen: der Sekundentakt schrieb sonst
        // „listening" darueber.
        QTest::qWait(1300);
        QCOMPARE(applet.statusForTest()->text(), grund);

        // Ein neuer Ruf raeumt ihn ab.
        applet.cqRufenButtonForTest()->click();
        QVERIFY(applet.statusForTest()->text() != grund);
        applet.stopButtonForTest()->click();
    }

    void brichtDasGeraetDieSendungHaeltDerLaufAn()
    {
        stelleEin(3);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.cqVersucheSpinForTest()->setValue(10);

        applet.cqRufenButtonForTest()->click();
        fund(applet, "OE5SOS DL1ABC JO31", -7.0);
        QVERIFY(sendetaktAbgelaufen(applet));
        // Getastet, die Sendung laeuft.
        QVERIFY(applet.sendetGerade());
        QCOMPARE(txZeilen(applet), 1);
        QCOMPARE(applet.mehrfachForTest().aktive(), 1);

        // Der Riegel greift mitten in der Sendung.
        emit radio.flexNichtGetastet(QStringLiteral(
            "SWR 2.52 is above the 2.50 limit on 15m — transmission stopped"));
        QTRY_VERIFY(!applet.sendetGerade());

        QVERIFY(!applet.ablaufForTest().laeuft());
        QCOMPARE(applet.mehrfachForTest().aktive(), 0);
        QVERIFY(!radio.moxController()->isMox());
        const QString grund = applet.statusForTest()->text();
        QVERIFY2(grund.contains(QStringLiteral("SWR 2.52")), qPrintable(grund));
        QTest::qWait(1300);
        QCOMPARE(applet.statusForTest()->text(), grund);
    }

    // Wer am Mikrofon spricht oder abstimmt, waehrend FT8 nur zuhoert,
    // hat mit diesem Fenster nichts zu tun.
    void eineFremdeSperreLaesstDasFensterInRuhe()
    {
        stelleEin(1);
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);

        emit radio.flexNichtGetastet(QStringLiteral("SWR 3.00 is above the 2.50 limit"));
        QTest::qWait(1300);
        QVERIFY2(!applet.statusForTest()->text().contains(
                     QStringLiteral("not transmitting")),
                 qPrintable(applet.statusForTest()->text()));
    }
};

QTEST_MAIN(TstFt8MultiAnswering)
#include "tst_ft8_multi_answering.moc"
