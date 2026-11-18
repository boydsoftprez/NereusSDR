// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Sendeseite — geschlossene Schleife, ohne Funkgeraet.
//
// Was hier geprueft wird, ist der Weg, den eine FT8-Nachricht bis zum
// Sender nimmt: Text -> ft8_lib-Modulation (12 kHz) -> Umtastung auf
// die Mikrofonrate (48 kHz) -> `pullSamples()` in Haeppchen, so wie
// der Tonfaden sie abholt. Was dabei herauskommt, wird wieder auf
// 12 kHz gebracht und DEKODIERT: steht dieselbe Nachricht da, hat der
// ganze Weg getragen.
//
// Ohne diese Schleife waere „es sendet" eine Behauptung ueber Code,
// den niemand gehoert hat.
//
// ── Was hier bewusst NICHT steht ────────────────────────────────────
//
// Das Tasten. Ft8TxSource weiss, WAS gesendet wuerde und wie lang das
// Fenster ist; ob der Sender angeht, entscheidet jemand anderes.
// Dieselbe Trennung wie beim Sprachspeicher (audio/VoiceKeyer.h).
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QSpinBox>
#include <QTimer>
#include <QDateTime>

#include "core/Ft8Decoder.h"
#include "core/Ft8Tx.h"
#include "core/Ft8Ablauf.h"
#include "models/LogEntry.h"
#include "core/AppSettings.h"
#include <QMenu>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QMouseEvent>
#include "core/MoxController.h"
#include "core/safety/BandPlanGuard.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/applets/Ft8Applet.h"
#include "core/Resampler.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/PcMicSource.h"

using namespace Longpath;

class TstFt8Senden : public QObject
{
    Q_OBJECT

private:
    /// Holt das ganze Fenster so ab, wie der Tonfaden es tut: in
    /// gleich grossen Haeppchen.
    static QVector<float> abholen(Ft8TxSource& quelle, int haeppchen,
                                  int hoechstens)
    {
        QVector<float> alles;
        QVector<float> puffer(haeppchen);
        while (alles.size() < hoechstens) {
            const int n = quelle.pullSamples(puffer.data(), haeppchen);
            if (n != haeppchen) { break; }
            for (int i = 0; i < n; ++i) { alles.append(puffer[i]); }
            if (!quelle.laeuft()) { break; }
        }
        return alles;
    }

private:
    /// Ein Doppelklick auf die erste Zeile der Fundliste. Per
    /// sendEvent, weil QTest-Mausklicks ohne Schirm verpuffen.
    static void doppelklickInDieListe(Ft8Applet& applet)
    {
        auto* vp = applet.listForTest()->viewport();
        QMouseEvent ev(QEvent::MouseButtonDblClick, QPointF(10, 5),
                       vp->mapToGlobal(QPointF(10, 5)),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(vp, &ev);
    }

private slots:

    // ── Nur so lange tasten, wie das Signal dauert ───────────────────
    //
    // Betreiber am 2026-10-22, nach dem ersten Sendeversuch: „hat zu
    // lange gedauert." Gemessen im Protokoll des Geraets: `mox=true`
    // von 06:17:30,295 bis 06:17:45,170 = 14,9 s, obwohl ein
    // FT8-Signal 12,64 s dauert. Die zwei Sekunden danach gehen nicht
    // nur verloren -- in ihnen laeuft der Empfang nicht, und das
    // naechste Fenster beginnt unmittelbar.
    void dieSendedauerIstDasSignalNichtDasFenster()
    {
        QCOMPARE(Ft8Tx::signalMs(Ft8Decoder::Protocol::Ft8), 12640);
        QCOMPARE(Ft8Tx::signalMs(Ft8Decoder::Protocol::Ft4), 5040);

        // Und sie ist KUERZER als das Fenster -- sonst waere nichts
        // gewonnen.
        QVERIFY(Ft8Tx::signalMs(Ft8Decoder::Protocol::Ft8)
                < Ft8Tx::fensterMs(Ft8Decoder::Protocol::Ft8));
        QVERIFY(Ft8Tx::signalMs(Ft8Decoder::Protocol::Ft4)
                < Ft8Tx::fensterMs(Ft8Decoder::Protocol::Ft4));

        // Zwei Sekunden Luft bleiben bei FT8 mindestens -- genug, um
        // rechtzeitig auf Empfang zurueck zu sein.
        QVERIFY2(Ft8Tx::fensterMs(Ft8Decoder::Protocol::Ft8)
                     - Ft8Tx::signalMs(Ft8Decoder::Protocol::Ft8) >= 2000,
                 "weniger als zwei Sekunden Luft vor dem naechsten Fenster");
    }

    // ── Tx1..Tx6 ────────────────────────────────────────────────────
    //
    // Die sechs Standardsaetze sind PROTOKOLL -- an jedem Buchstaben
    // haengt, ob die Gegenstation den Zug versteht. Darum wird hier
    // der ganze Satz verglichen und nicht nur „ist nicht leer".
    //
    // Die beiden letzten Faelle sind die wichtigen: ohne Gegenueber
    // duerfen Tx1..Tx5 NICHTS liefern. Ein Platzhalter im Feld ginge
    // auf den naechsten Druck auf „Send" hinaus.
    void dieSechsStandardsaetzeStimmenWoertlich()
    {
        const QString dx   = QStringLiteral("DL1ABC");
        const QString ich  = QStringLiteral("OE5SOS");
        const QString grid = QStringLiteral("JN67");

        QCOMPARE(Ft8Applet::txKnopfNachricht(1, dx, ich, grid, -12),
                 QStringLiteral("DL1ABC OE5SOS JN67"));
        QCOMPARE(Ft8Applet::txKnopfNachricht(2, dx, ich, grid, -12),
                 QStringLiteral("DL1ABC OE5SOS -12"));
        QCOMPARE(Ft8Applet::txKnopfNachricht(3, dx, ich, grid, -12),
                 QStringLiteral("DL1ABC OE5SOS R-12"));
        QCOMPARE(Ft8Applet::txKnopfNachricht(4, dx, ich, grid, -12),
                 QStringLiteral("DL1ABC OE5SOS RR73"));
        QCOMPARE(Ft8Applet::txKnopfNachricht(5, dx, ich, grid, -12),
                 QStringLiteral("DL1ABC OE5SOS 73"));
        QCOMPARE(Ft8Applet::txKnopfNachricht(6, dx, ich, grid, -12),
                 QStringLiteral("CQ OE5SOS JN67"));
    }

    void ohneGegenueberLiefernTx1BisTx5NICHTS()
    {
        const QString ich  = QStringLiteral("OE5SOS");
        const QString grid = QStringLiteral("JN67");
        for (int n = 1; n <= 5; ++n) {
            QVERIFY2(Ft8Applet::txKnopfNachricht(n, QString{}, ich, grid, 0)
                         .isEmpty(),
                     qPrintable(QStringLiteral("Tx%1 hat ohne Gegenueber "
                                               "etwas geliefert").arg(n)));
        }
        // Tx6 ruft CQ -- das geht immer.
        QCOMPARE(Ft8Applet::txKnopfNachricht(6, QString{}, ich, grid, 0),
                 QStringLiteral("CQ OE5SOS JN67"));
        // Und ohne eigenes Rufzeichen geht gar nichts.
        QVERIFY(Ft8Applet::txKnopfNachricht(6, QString{}, QString{}, grid, 0)
                    .isEmpty());
    }


    // ── Die Schleife ────────────────────────────────────────────────

    void wasGesendetWuerdeLaesstSichZurueckLesen()
    {
        const QString text = QStringLiteral("CQ OE5SOS JN78");
        Ft8TxSource quelle(48000);
        QVERIFY2(quelle.vorbereiten(text, 1500.0f,
                                    Ft8Decoder::Protocol::Ft8),
                 "ft8_lib konnte die Nachricht nicht packen");
        QCOMPARE(quelle.nachricht(), text);

        // Ein ganzes FT8-Fenster bei 48 kHz: 15 s = 720 000 Proben.
        qInfo() << "Fenster am Sendeweg:" << quelle.probenImFenster()
                << "Proben";
        QVERIFY2(quelle.probenImFenster() > 700000,
                 qPrintable(QString::number(quelle.probenImFenster())));

        // Vor dem Start ist Stille — und zwar ohne Ausnahme.
        QVector<float> stille(1024);
        QCOMPARE(quelle.pullSamples(stille.data(), 1024), 1024);
        for (float v : stille) { QCOMPARE(v, 0.0f); }
        QVERIFY(!quelle.laeuft());

        quelle.starten();
        QVERIFY(quelle.laeuft());

        QVector<float> alles;
        QVector<float> puffer(1024);
        int runden = 0;
        while (quelle.laeuft() && runden < 2000) {
            const int n = quelle.pullSamples(puffer.data(), 1024);
            QCOMPARE(n, 1024);
            for (int i = 0; i < n; ++i) { alles.append(puffer[i]); }
            ++runden;
        }
        qInfo() << "abgeholt:" << alles.size() << "Proben in" << runden
                << "Haeppchen";
        QVERIFY2(!quelle.laeuft(), "die Quelle hoert nicht von selbst auf");

        // Zurueck auf 12 kHz und dekodieren.
        Resampler zurueck(48000.0, double(Ft8Decoder::kSampleRate), 4096);
        QVector<float> bei12k;
        QVector<float> aus(4096 + 64);
        for (int pos = 0; pos < alles.size(); pos += 4096) {
            const int n = qMin(4096, alles.size() - pos);
            const int g = zurueck.processInto(alles.constData() + pos, n,
                                              aus.data(), aus.size());
            for (int i = 0; i < g; ++i) { bei12k.append(aus[i]); }
        }
        qInfo() << "zurueck bei 12 kHz:" << bei12k.size() << "Proben";

        const auto funde = Ft8Decoder::decodeBlock(bei12k.constData(),
                                                   bei12k.size());
        QStringList wortlaute;
        for (const Ft8Decode& d : funde) { wortlaute << d.text; }
        qInfo().noquote() << "zurueckgelesen:" << wortlaute.join(QLatin1String(" | "));
        QVERIFY2(wortlaute.contains(text),
                 qPrintable(QStringLiteral("gesendet „%1\", gelesen: %2")
                                .arg(text, wortlaute.join(QLatin1String(" | ")))));
    }

    /// Abbrechen heisst sofort still — nicht am Ende des Fensters.
    void abbrechenWirktSofort()
    {
        Ft8TxSource quelle(48000);
        QVERIFY(quelle.vorbereiten(QStringLiteral("CQ OE5SOS JN78"),
                                   1500.0f, Ft8Decoder::Protocol::Ft8));
        quelle.starten();

        // ── Ein Fenster beginnt mit STILLE ──────────────────────
        //
        // Gemessen am 2026-10-15: encodeBlock legt die Daten mittig
        // ins Fenster, davor liegen rund 1,18 s Ruhe (ft8_lib
        // demo/gen_ft8.c rechnet `stille = (ganz - nutz) / 2`). Die
        // ersten 1024 Proben — 21 ms — sind darum still, und der
        // erste Entwurf dieses Laufs erklaerte das zum Fehler.
        //
        // Fuer das Tasten heisst das: der Sender muss zu
        // FENSTERBEGINN an, nicht erst, wenn man etwas hoert.
        QVector<float> puffer(1024);
        bool etwasGehoert = false;
        int gelesen = 0;
        while (gelesen < 48000 * 3 && !etwasGehoert) {
            QCOMPARE(quelle.pullSamples(puffer.data(), 1024), 1024);
            gelesen += 1024;
            for (float v : puffer) {
                if (v != 0.0f) { etwasGehoert = true; break; }
            }
        }
        qInfo() << "erster Ton nach" << gelesen << "Proben ="
                << QString::number(gelesen / 48000.0, 'f', 2) << "s";
        QVERIFY2(etwasGehoert, "in den ersten drei Sekunden kein Ton");

        quelle.abbrechen();
        QVERIFY(!quelle.laeuft());
        QCOMPARE(quelle.pullSamples(puffer.data(), 1024), 1024);
        for (float v : puffer) { QCOMPARE(v, 0.0f); }
    }

    // ── Die Gegenperiode ────────────────────────────────────────────

    void dieGegenperiodeLiegtRichtig()
    {
        // 1 000 000 000 000 ms ist nicht durch 60 000 teilbar; die
        // Rechnung geht von der vollen Minute aus, also wird hier eine
        // genommen: 999 999 960 000 ms.
        const qint64 minute = 999999960000LL;
        QCOMPARE(minute % 60000, 0LL);

        const auto ft8 = Ft8Decoder::Protocol::Ft8;
        QCOMPARE(Ft8Tx::fensterMs(ft8), 15000);
        QCOMPARE(Ft8Tx::fensterNummer(minute, ft8), 0);
        QCOMPARE(Ft8Tx::fensterNummer(minute + 15000, ft8), 1);
        QCOMPARE(Ft8Tx::fensterNummer(minute + 45000, ft8), 3);

        QVERIFY(Ft8Tx::istSendefenster(minute, ft8, Ft8Periode::Gerade));
        QVERIFY(!Ft8Tx::istSendefenster(minute, ft8, Ft8Periode::Ungerade));
        QVERIFY(Ft8Tx::istSendefenster(minute + 15000, ft8,
                                       Ft8Periode::Ungerade));

        // FT4: halb so lange Fenster, also acht in der Minute.
        const auto ft4 = Ft8Decoder::Protocol::Ft4;
        QCOMPARE(Ft8Tx::fensterMs(ft4), 7500);
        QCOMPARE(Ft8Tx::fensterNummer(minute + 7500, ft4), 1);

        // Und der Weg bis zum naechsten eigenen Fenster.
        QCOMPARE(Ft8Tx::bisZumNaechsten(minute, ft8, Ft8Periode::Gerade), 0LL);
        QCOMPARE(Ft8Tx::bisZumNaechsten(minute, ft8, Ft8Periode::Ungerade),
                 15000LL);
        QCOMPARE(Ft8Tx::bisZumNaechsten(minute + 1000, ft8,
                                        Ft8Periode::Gerade), 29000LL);
    }

    /// FT4 geht denselben Weg — kuerzeres Fenster, gleiche Schleife.
    void ft4SendetAuch()
    {
        Ft8TxSource quelle(48000);
        QVERIFY(quelle.vorbereiten(QStringLiteral("CQ OE5SOS JN78"),
                                   1500.0f, Ft8Decoder::Protocol::Ft4));
        // 7,5 s bei 48 kHz = 360 000 Proben.
        qInfo() << "FT4-Fenster:" << quelle.probenImFenster() << "Proben";
        QVERIFY2(quelle.probenImFenster() > 340000,
                 qPrintable(QString::number(quelle.probenImFenster())));
        QVERIFY2(quelle.probenImFenster() < 400000,
                 "ein FT4-Fenster ist halb so lang wie ein FT8-Fenster");
    }

    /// Eine Nachricht, die ft8_lib nicht packen kann, darf NICHT
    /// stillschweigend ein leeres Fenster senden.
    void einUnmoeglicherTextSendetNichts()
    {
        Ft8TxSource quelle(48000);
        const bool ok = quelle.vorbereiten(
            QStringLiteral("DIES IST VIEL ZU LANG FUER EINE FT8 NACHRICHT 123"),
            1500.0f, Ft8Decoder::Protocol::Ft8);
        qInfo() << "zu langer Text angenommen:" << ok;
        if (!ok) {
            QCOMPARE(quelle.probenImFenster(), 0);
            quelle.starten();
            QVERIFY2(!quelle.laeuft(),
                     "ohne vorbereitete Nachricht darf nichts starten");
        }
    }

    // ── Der Weg in den Sender ───────────────────────────────────────

    /// Am Mikrofon-Router angemeldet — und ohne laufende Quelle still.
    ///
    /// Das ist die Stelle, an der FT8 bisher endete: gebaut, aber
    /// nirgends angeschlossen. Der Sprachspeicher steht bis heute so
    /// da (WavTxSource, niemand ruft es).
    // ── Sagt der Bandplan nein, wird nicht gesendet ─────────────────
    //
    // MoxController::setMox(true) darf abgelehnt werden (falsche
    // Betriebsart, Frequenz ausserhalb des Bandes, Cross-Band-TX). Die
    // Ablehnung kommt NUR als moxRejected(reason) — setMox() gibt
    // nichts zurueck.
    //
    // Bis zum 2026-10-16 hoerte im Ft8Applet niemand darauf: der Ton
    // lief danach das ganze Fenster weiter, der Sendeweg blieb auf
    // FT8 (das Mikrofon also abgeklemmt), und in der Zeile stand
    // „transmitting", waehrend nichts hinausging.
    //
    // FT4 statt FT8, damit der Lauf hoechstens ein 7,5-s-Fenster
    // abwartet statt eines von 15 s.
    void einAbgelehntesTastenBrichtDasSendenAb()
    {
        RadioModel radio;
        radio.installFt8TxForTest();   // sonst: „no transmit path"
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);

        auto* mox = radio.moxController();
        QVERIFY2(mox, "ohne MoxController hat dieser Lauf keinen Gegenstand");

        // Eine Pruefung, die immer ablehnt — wie der Bandplan an einer
        // Stelle, an der nicht gesendet werden darf. MoxCheckFn nimmt
        // nichts entgegen und wird nur vor dem EINschalten gefragt —
        // siehe MoxController::setMoxCheck.
        //
        // ── Und der Grund im Augenblick der Ablehnung ───────────────
        //
        // Nicht nach dem Warten unten: Ft8Applet haengt an einem Takt
        // von 200 ms, und updateStatus() schreibt bei jedem Takt den
        // Fensterstand („0.0 / 7.5 s") ueber die Zeile. Wer den Grund
        // danach liest, liest den Takt — am 2026-10-16 genau so
        // gemessen, erst mit einer gereihten Zustellung (die dem
        // ueberfaelligen Taktgeber nicht zuverlaessig zuvorkommt) und
        // dann ohne.
        //
        // Der eine Augenblick, der sicher traegt, ist die Pruefung
        // selbst: MoxController::setMox() ruft m_moxCheck() auf und
        // sendet moxRejected ERST danach aus (MoxController.cpp:501),
        // und das Applet hat seinen Handler da schon angemeldet (es
        // verbindet unmittelbar vor setMox(true)). Eine Verbindung,
        // die hier entsteht, steht also HINTER der des Applets und
        // laeuft im selben Aufruf direkt nach ihr — vor jedem Takt.
        int gefragt = 0;
        QString grundInDerZeile;
        QMetaObject::Connection beobachter;
        mox->setMoxCheck([&]() {
            ++gefragt;
            if (!beobachter) {
                beobachter = QObject::connect(
                    mox, &MoxController::moxRejected, &applet,
                    [&applet, &grundInDerZeile](const QString&) {
                        if (auto* l = applet.statusForTest()) {
                            grundInDerZeile = l->text();
                        }
                    },
                    Qt::DirectConnection);
            }
            return safety::BandPlanGuard::MoxCheckResult{
                false, QStringLiteral("ausserhalb des Bandes")};
        });

        applet.sendeNachricht(QStringLiteral("CQ OE5SOS JN78"));
        QVERIFY2(applet.sendetGerade(),
                 "der Versuch muss erst einmal anlaufen — er wartet auf "
                 "sein Fenster");

        // Sobald das Fenster beginnt, wird getastet, abgelehnt und
        // abgebrochen.
        //
        // Die Frist nicht raten, sondern rechnen: gesendet wird in der
        // GEGENperiode, die kommt nur jedes zweite Fenster. Die
        // Wartezeit ist also bis zu 2 x fensterMs — bei FT4 bis zu
        // 15 s, nicht 7,5. Mit geschaetzten 12 s fielen am 2026-10-16
        // zwei von drei Laeufen um (13100 und 13150 ms haetten
        // gereicht), waehrend der erste gruen war: genau die Art
        // Wette, die dieser Pruefstand heute an vier Stellen
        // losgeworden ist.
        const int maxWarten = 2 * Ft8Tx::fensterMs(Ft8Decoder::Protocol::Ft4);
        QTRY_VERIFY_WITH_TIMEOUT(!applet.sendetGerade(), maxWarten + 5000);

        QVERIFY2(gefragt > 0, "es wurde nie zu tasten versucht");
        QVERIFY2(!mox->isMox(), "abgelehnt heisst: der Sender bleibt aus");

        // Und der Sendeweg liegt wieder, wo er war — sonst waere das
        // Mikrofon nach einem abgelehnten Versuch stumm.
        if (auto* router = radio.micRouter()) {
            QVERIFY2(router->activeSource() != MicSource::Ft8,
                     "nach dem Abbruch darf der Sendeweg nicht auf FT8 "
                     "stehenbleiben");
        }

        // ── Nicht auf „transmitting" pruefen (2026-10-16) ───────────
        //
        // Hier stand !zeile.contains("transmitting"). Das war gruen,
        // solange der Ablehnungstext deutsch war („sendet nicht: %1");
        // seit die Zeile englisch spricht, steht dort „NOT
        // transmitting: ausserhalb des Bandes" — und die enthaelt das
        // gesuchte Wort. Der Lauf fiel also um, WEIL das Programm sich
        // richtig verhielt.
        //
        // Ein Teilwort trennt die beiden Zustaende nicht. Geprueft
        // wird darum der oben abgegriffene Stand genau: er ist nicht
        // der Sendezustand (tr("transmitting") / tr("sending in %1 s")),
        // und er nennt den Grund der Ablehnung.
        QVERIFY2(grundInDerZeile != QStringLiteral("transmitting")
                     && !grundInDerZeile.startsWith(
                            QStringLiteral("sending in")),
                 qPrintable(QStringLiteral("die Zeile behauptet zu senden: %1")
                                .arg(grundInDerZeile)));
        QVERIFY2(grundInDerZeile.contains(
                     QStringLiteral("ausserhalb des Bandes")),
                 qPrintable(QStringLiteral(
                     "die Zeile nannte den Grund der Ablehnung nie: %1")
                                .arg(grundInDerZeile)));

        // Und zuletzt der Stand, den der Betreiber wirklich sieht:
        // nach dem Abbruch laeuft wieder die Fensteranzeige des
        // Takts, nicht eine eingefrorene Sendemeldung.
        const QString zeile = applet.statusForTest()
                                  ? applet.statusForTest()->text() : QString();
        QVERIFY2(zeile != QStringLiteral("transmitting"),
                 qPrintable(QStringLiteral(
                     "die Zeile ist auf der Sendemeldung "
                     "stehengeblieben: %1")
                                .arg(zeile)));
    }

    // ── Auf eine gehoerte Station antworten ─────────────────────────
    //
    // Ein Doppelklick in die Fundliste soll den Standardanruf ins
    // Eingabefeld legen. Geprueft wird die REGEL, nicht die Maus:
    // antwortAuf() ist statisch und braucht weder Fenster noch
    // Zeigegeraet.
    void einKlickAufEineZeileWirdZurAntwort_data()
    {
        QTest::addColumn<QString>("zeile");
        QTest::addColumn<QString>("erwartet");

        // So, wie onDecoded() die Zeile baut: HHMMSS, dB auf 4,
        // DT auf 5, Hz auf 5, dann zwei Leerzeichen und die Nachricht.
        QTest::newRow("CQ mit Locator")
            << QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31")
            << QStringLiteral("DL1ABC OE5SOS JN67");
        QTest::newRow("CQ DX")
            << QStringLiteral("120000  -14  -0.3   780  CQ DX VK3ABC QF22")
            << QStringLiteral("VK3ABC OE5SOS JN67");
        // Ein laufendes QSO zweier anderer: geantwortet wird dem, der
        // sendet — senderCallsign() liest das zweite Rufzeichen.
        QTest::newRow("fremdes QSO")
            << QStringLiteral("120000   -3   0.0  2100  OE1XYZ DL1ABC -12")
            << QStringLiteral("DL1ABC OE5SOS JN67");
        // Sich selbst anrufen ergibt nichts.
        QTest::newRow("das bin ich")
            << QStringLiteral("120000   -3   0.0  2100  DL1ABC OE5SOS -12")
            << QString();
        // Kein Rufzeichen in der Zeile.
        QTest::newRow("nur Grusswort")
            << QStringLiteral("120000   -3   0.0  2100  73")
            << QString();
        // Eine Zeile ohne Nachrichtenspalte.
        QTest::newRow("abgeschnitten")
            << QStringLiteral("120000   -3")
            << QString();
    }

    void einKlickAufEineZeileWirdZurAntwort()
    {
        QFETCH(QString, zeile);
        QFETCH(QString, erwartet);
        QCOMPARE(Ft8Applet::antwortAuf(zeile, QStringLiteral("OE5SOS"),
                                       QStringLiteral("JN67VV")),
                 erwartet);
    }

    // Ohne eigenes Rufzeichen gibt es keine Antwort — sonst stuende
    // eine halbe Nachricht im Feld, die beim Senden nicht packbar ist.
    void ohneEigenesRufzeichenKeineAntwort()
    {
        const QString zeile =
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31");
        QVERIFY(Ft8Applet::antwortAuf(zeile, QString(),
                                      QStringLiteral("JN67VV")).isEmpty());
        QVERIFY(Ft8Applet::antwortAuf(zeile, QStringLiteral("   "),
                                      QStringLiteral("JN67VV")).isEmpty());

        // Ohne Locator aber schon: der ist in einer FT8-Antwort
        // ueblich, aber nicht zwingend.
        QCOMPARE(Ft8Applet::antwortAuf(zeile, QStringLiteral("OE5SOS"),
                                       QString()),
                 QStringLiteral("DL1ABC OE5SOS"));
    }

    // ── Ein Doppelklick nimmt das QSO an ────────────────────────────
    //
    // UMGEKEHRT am 2026-10-22. Hier stand bis dahin
    // `ohneAutoFuelltDerDoppelklickNurDasFeld`, und der hielt fest:
    // ohne den Schalter „Auto" legt ein Doppelklick die Antwort nur
    // ins Feld, gesendet wird nichts.
    //
    // Betreiber: „wichtig ist auch, dass nach der annahme des qso das
    // qso automatisch anlaeuft bis 73." Damit ist die Grenze, die der
    // alte Prueflauf huetete, keine mehr — ein Doppelklick IST die
    // Annahme. Der Schalter ist weg, angehalten wird mit „Stop".
    //
    // Der Doppelklick geht per sendEvent an den viewport: QTest-Klicks
    // verpuffen ohne Schirm, und dieser Lauf hat keinen.
    void derDoppelklickNimmtDasQsoAn()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        auto* liste = applet.listForTest();
        QVERIFY(liste);
        liste->setPlainText(
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31"));

        doppelklickInDieListe(applet);

        QCOMPARE(applet.textFeldForTest()->text(),
                 QStringLiteral("DL1ABC OE5SOS JN67"));
        QVERIFY2(applet.ablaufForTest().laeuft(),
                 "ein Doppelklick muss den Ablauf starten");
        QCOMPARE(applet.ablaufForTest().partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::RufeAn);
        QVERIFY2(applet.sendetGerade(),
                 "der erste Zug muss auf sein Fenster warten");
        applet.abbrechen();
    }

    // ── Was mir noch fehlt, steht blau (2026-10-30) ─────────────────
    //
    // Betreiber: „interessant waere zu sehen, welche grids ich noch
    // nicht habe und diese dann zb blau zu markieren ... somit sehe
    // ich dann, was ich noch benoetige." Danach meldete er: „keine
    // blauen rufzeichen...."
    //
    // Behoben war es; belegt nicht. `tst_ft8_flaeche` prueft die
    // RANGFOLGE der Zeilenarten (`zeilenart(...)`), `tst_grids_-
    // gearbeitet` die Einheit -- den WEG dazwischen fuhr niemand:
    //
    //   Logbuch -> GridsGearbeitet::istNeu -> z.neuesGrid
    //   -> Sichtfilter -> was in der Liste steht
    //
    // Genau die Form, in der heute acht Fehler steckten.
    void nurDieFehlendenGrossfelderStehenInDerSicht()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));

        // Ein Logbuch mit genau einem Grossfeld. Ohne das greift
        // `leer()` und es wird NICHTS markiert -- auch Absicht, siehe
        // den naechsten Fall.
        applet.gridsForTest().merke(QStringLiteral("JO31"));
        QVERIFY(!applet.gridsForTest().leer());

        auto* knopf = applet.sichtNeuesGridButtonForTest();
        QVERIFY2(knopf, "die Sicht „New grid“ hat keinen Knopf");
        knopf->click();

        const auto fund = [&applet](const char* text) {
            Ft8Decode d;
            d.text   = QString::fromLatin1(text);
            d.snrDb  = -10;
            d.freqHz = 1500.0;
            applet.fundForTest(d);
        };
        fund("CQ DL1ABC JO31");    // habe ich schon
        fund("CQ SP9XYZ KO02");    // fehlt mir
        fund("CQ G4MHJ IO83");     // fehlt mir

        const QString sicht = applet.listForTest()->toPlainText();
        QVERIFY2(!sicht.contains(QStringLiteral("DL1ABC")),
                 qPrintable(QStringLiteral("JO31 ist gearbeitet und darf "
                                           "nicht in der Sicht stehen:\n")
                            + sicht));
        QVERIFY2(sicht.contains(QStringLiteral("SP9XYZ")),
                 qPrintable(QStringLiteral("KO02 fehlt und muss stehen:\n")
                            + sicht));
        QVERIFY2(sicht.contains(QStringLiteral("G4MHJ")),
                 qPrintable(QStringLiteral("IO83 fehlt und muss stehen:\n")
                            + sicht));
    }

    // ── Dasselbe Feld, anderes Band (2026-11-12) ────────────────────
    //
    // Betreiber heute: die Locatorjagd „sollte optimiert werden und
    // eher am programm ft8 und nicht im logprogramm zu finden sein."
    //
    // Bis dahin kannte das FT8-Fenster nur `GridsGearbeitet`, und die
    // kennt kein Band: ein Feld, das irgendwann auf 80 m im Logbuch
    // landete, galt auf 20 m als erledigt. Die Rechnung je Band
    // (`LocatorMosaik`) gab es laengst -- aber nur hinter der Karte im
    // Logbuchfenster, also dort, wo niemand FT8 faehrt.
    //
    // Derselbe WEG wie oben, eine Stufe tiefer:
    //
    //   Logbuch -> LocatorMosaik::stand(feld, band) -> z.neuesGridBand
    //   -> zeilenart -> Farbe
    //
    // Das Band kommt aus `bandFuerLog`, nicht aus einer eigenen
    // Rechnung -- sonst stuende am 2-m-Konverter das ZF-Band da.
    void einFeldAufAnderemBandIstDieZweiteStufe()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);          // 20 m
        applet.setSlice(slice);

        // Das Feld STEHT im Logbuch -- also nicht „ganz neu" ...
        applet.gridsForTest().merke(QStringLiteral("JO31"));
        // ... aber nur auf 80 m.
        LogEntry e;
        e.call       = QStringLiteral("DL1ABC");
        e.band       = QStringLiteral("80m");
        e.mode       = QStringLiteral("FT8");
        e.gridSquare = QStringLiteral("JO31");
        applet.mosaikForTest().baue(QVector<LogEntry>{e}, false);

        Ft8Decode d;
        d.text   = QStringLiteral("CQ DL1ABC JO31");
        d.snrDb  = -10;
        d.freqHz = 1500.0;
        applet.fundForTest(d);

        QCOMPARE(applet.letzteZeilenartForTest(),
                 Ft8Applet::Zeilenart::NeuesGridBand);

        // Gegenprobe, ohne die das Gruene nichts belegt: dasselbe Feld
        // auf DIESEM Band gearbeitet -- dann ist nichts mehr zu holen
        // und es bleibt ein gewoehnliches CQ.
        e.band = QStringLiteral("20m");
        applet.mosaikForTest().baue(QVector<LogEntry>{e}, false);
        applet.fundForTest(d);
        QCOMPARE(applet.letzteZeilenartForTest(),
                 Ft8Applet::Zeilenart::Cq);
    }

    // Ohne gelesenes Logbuch waere JEDES Feld neu, und die Markierung
    // sagte nichts mehr. Dann lieber keine -- `leer()` schuetzt davor,
    // und das ist eine Entscheidung, nicht ein Versehen.
    void ohneLogbuchWirdNichtsMarkiert()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));

        applet.gridsForTest().leeren();
        applet.sichtNeuesGridButtonForTest()->click();

        Ft8Decode d;
        d.text   = QStringLiteral("CQ SP9XYZ KO02");
        d.snrDb  = -10;
        d.freqHz = 1500.0;
        applet.fundForTest(d);

        QVERIFY2(!applet.listForTest()->toPlainText()
                     .contains(QStringLiteral("SP9XYZ")),
                 "ohne Logbuch ist kein Feld „neu“ -- sonst waere alles neu");
    }

    // ── Auch waehrend einer laufenden Sendung (2026-10-30) ──────────
    //
    // Betreiber: „doppelklick loest kein automatisches qso aus."
    //
    // Hier stand `if (!m_sendet)`, und das verschluckte jeden
    // Doppelklick, der in eine laufende Sendung fiel. Bei FT8 ist das
    // die HAELFTE der Zeit: 13,2 s von 15 wird getastet, und wer in
    // der Liste etwas sieht, klickt genau dann. Der Druck war weg,
    // ohne dass irgendwo etwas stand.
    //
    // Der Fall darueber endet bereits mit `sendetGerade() == true` --
    // ein zweiter Doppelklick ist also genau seine Lage, und mit der
    // alten Fassung tat er nichts.
    void einDoppelklickWaehrendDerSendungZaehltAuch()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        auto* liste = applet.listForTest();
        QVERIFY(liste);
        liste->setPlainText(
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31"));
        doppelklickInDieListe(applet);
        QCOMPARE(applet.ablaufForTest().partner(), QStringLiteral("DL1ABC"));
        QVERIFY2(applet.sendetGerade(), "Voraussetzung: es geht etwas hinaus");

        // Jetzt faellt der Klick MITTEN in die Sendung.
        liste->setPlainText(
            QStringLiteral("120015  -14   0.1  1740  CQ SP9XYZ KO02"));
        doppelklickInDieListe(applet);

        QCOMPARE(applet.textFeldForTest()->text(),
                 QStringLiteral("SP9XYZ OE5SOS JN67"));
        QVERIFY2(applet.ablaufForTest().laeuft(),
                 "der Ablauf muss auch waehrend der Sendung anlaufen");
        QCOMPARE(applet.ablaufForTest().partner(), QStringLiteral("SP9XYZ"));
        applet.abbrechen();
    }

    // ── Das eigene Gespraech steht rechts ───────────────────────────
    //
    // Betreiber am 2026-10-22: „sehe ich auch rechts, wenn ein qso zu
    // stande kommt." Die Antworten AN ihn standen dort schon (jede
    // Zeile mit dem eigenen Rufzeichen geht nach rechts). Gefehlt hat
    // die eigene Haelfte -- und seit das QSO von selbst laeuft,
    // stuende rechts sonst nur, was der andere sagt.
    void dieEigeneSendungStehtRechts()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        QVERIFY(applet.listForTest());
        QVERIFY(applet.listForTest()->toPlainText().isEmpty());

        applet.zeigeEigeneSendung(QStringLiteral("DL1ABC OE5SOS JN67"));

        // ── Die eigene Sendung steht in JEDER Sicht (2026-10-30) ──
        //
        // Bis dahin stand sie in der rechten Spalte und links NICHT:
        // „dort steht, was auf dem Band los ist, und das eigene Signal
        // gehoert nicht dazu." Mit einer Liste ist das anders herum
        // richtig: die eigene Sendung IST der Zusammenhang, in dem die
        // Antwort gelesen wird. Wer sie ausblendet, sieht ein QSO als
        // Folge fremder Saetze.
        const QString liste = applet.listForTest()->toPlainText();
        QVERIFY2(liste.contains(QStringLiteral("DL1ABC OE5SOS JN67")),
                 "die eigene Sendung muss in der Liste stehen");
        QVERIFY2(liste.contains(QStringLiteral("Tx")),
                 "sie muss als eigene Sendung erkennbar sein");

        // Auch in einer eingeschraenkten Sicht — sie kommt durch jede.
        applet.sichtAnMichButtonForTest()->click();
        QVERIFY2(applet.listForTest()->toPlainText()
                     .contains(QStringLiteral("DL1ABC OE5SOS JN67")),
                 "die eigene Sendung darf von keiner Sicht wegfallen");
        applet.sichtAllesButtonForTest()->click();

        // Leeres bleibt leer -- sonst stuende nach jedem Fehlversuch
        // eine Zeile mit nichts drin.
        const QString vorher = applet.listForTest()->toPlainText();
        applet.zeigeEigeneSendung(QStringLiteral("   "));
        QCOMPARE(applet.listForTest()->toPlainText(), vorher);
    }

    // ── Und der Sendeweg ruft sie wirklich ──────────────────────────
    //
    // Der Lauf darueber ruft zeigeEigeneSendung() SELBST — er belegt
    // damit, was die Methode tut, aber nicht, dass jemand sie ruft.
    // Entfernt man den Aufruf aus dem Sendeweg, bleibt er gruen. Also
    // hier derselbe Weg wie im Betrieb: senden, den Zeitgeber auf 0
    // stellen und ablaufen lassen.
    void derSendewegSchreibtDieZeileWirklich()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        applet.sendeNachricht(QStringLiteral("CQ OE5SOS JN67"));
        QVERIFY2(applet.sendetGerade(), "die Sendung muss eingeplant sein");

        QTimer* t = applet.sendeTimerForTest();
        QVERIFY(t);
        QVERIFY2(t->isActive(), "der Zeitgeber muss auf das Fenster warten");
        t->stop();
        t->start(0);

        QTRY_VERIFY_WITH_TIMEOUT(
            applet.listForTest()->toPlainText()
                .contains(QStringLiteral("CQ OE5SOS JN67")), 3000);

        applet.abbrechen();
    }

    // ── Der Sendetaktgeber muss GENAU sein ──────────────────────────
    //
    // Qt gibt jedem Taktgeber ab 20 ms von sich aus `Qt::CoarseTimer`
    // und erlaubt sich dabei, bis zu fuenf Prozent des Intervalls zu
    // frueh zu wecken. Bei FT8 wird hier auf die naechste
    // Fenstergrenze gewartet — bis zu 17,8 s, also bis zu 0,89 s
    // Vorlauf.
    //
    // Am Geraet gemessen (2026-10-29, 07:02 bis 07:04, fuenf
    // Sendungen): MOX ging bei :29,21 an statt bei :30,00 — 0,779 /
    // 0,787 / 0,786 / 0,786 / 0,786 s zu frueh, jedes Mal dieselbe
    // Groesze. Die Rechnung in `Ft8Tx::bisZumNaechsten` war dabei
    // richtig (der Lauf `derZeitplanStimmt` darueber belegt sie); der
    // Taktgeber hat sie verworfen. Die Systemuhr war es nicht, `sntp`
    // meldete im selben Augenblick +37 ms.
    //
    // Die Gegenprobe steht im Lauf selbst: ein frischer Taktgeber
    // bekommt Qts Vorgabe, und die ist NICHT genau. Ohne die Zeile im
    // Sendeweg stuende hier dasselbe.
    void derSendetaktgeberIstGenau()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        // Gegenprobe zuerst: so waere es ohne die Einstellung.
        QTimer vorgabe;
        vorgabe.setSingleShot(true);
        QCOMPARE(vorgabe.timerType(), Qt::CoarseTimer);

        applet.sendeNachricht(QStringLiteral("CQ OE5SOS JN67"));
        QVERIFY2(applet.sendetGerade(), "die Sendung muss eingeplant sein");

        QTimer* t = applet.sendeTimerForTest();
        QVERIFY(t);
        QCOMPARE(t->timerType(), Qt::PreciseTimer);

        // Und die bestellte Wartezeit muss auf einer Fenstergrenze
        // enden -- plus der Dekoderfrist. 15 s teilen die Minute, also
        // liegt jede Grenze auf :00, :15, :30 oder :45.
        //
        // ── Die Frist gehoert dazu (2026-10-30) ──────────────────────
        //
        // Bis dahin stand hier „der Rest gegen 15 000 ms ist null".
        // Seit dem Sendeplan wird 250 ms NACH der Grenze getastet: der
        // Dekoder ist erst ~40 ms nach dem Fensterende fertig, und
        // ohne diese Frist kam die Antwort der Gegenstation immer ein
        // ganzes Fenster zu spaet. Die Frist wird aus der fuehrenden
        // Stille der Wellenform genommen, nicht hinten angehaengt --
        // der Ton geht zur selben Zeit hinaus wie vorher. Siehe
        // `Ft8Tx::sendeplan` und tst_ft8_sendeplan.
        const Ft8Tx::Sendeplan plan =
            Ft8Tx::sendeplan(Ft8Decoder::Protocol::Ft8, 250, 500, 300);
        const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
        const qint64 ziel  = jetzt + t->remainingTime() - plan.tastenNachMs;
        const qint64 rest  = ziel % 15000;
        // Zwischen dem Einplanen und dieser Zeile liegt Rechenzeit;
        // 150 ms Spiel sind dafuer, nicht fuer den Taktgeber.
        QVERIFY2(rest < 150 || rest > 14850,
                 qPrintable(QStringLiteral("Ziel liegt %1 ms neben der "
                                           "Fenstergrenze").arg(rest)));

        applet.abbrechen();
    }

    // ── „Stop" heisst Stop ──────────────────────────────────────────
    //
    // Seit der Doppelklick den Ablauf immer startet, ist dieser Knopf
    // der einzige Weg hinaus. Er rief bis zum 2026-10-22 nur
    // abbrechen() — das nimmt das laufende Fenster zurueck, laesst
    // den Ablauf aber stehen, und der naechste passende Fund haette
    // ihn von selbst weiterlaufen lassen.
    // ── Der CQ-Knopf zeigt, dass er laeuft ──────────────────────────
    //
    // Betreiber am 2026-10-29: „der button sollte sich auch farblich
    // aendern, wenn er geklickt ist." Im Hausstil ist das die
    // Blaufuellung des eingerasteten Knopfes — also `isChecked`.
    //
    // Gegenprobe zur alten Fassung: dort setzte niemand den Zustand.
    // Ein einrastbarer Knopf kippte beim Klick zwar um, blieb danach
    // aber stehen, auch wenn der Ruf laengst aufgegeben hatte.
    void derCqKnopfZeigtDenLaufendenRuf()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        QPushButton* cq = applet.cqRufenButtonForTest();
        QVERIFY(cq);
        QVERIFY2(!cq->isChecked(), "vor dem Klick darf nichts leuchten");

        // Einrasten allein genuegt nicht: ohne die Blaufuellung aus
        // `blueToggle` sieht man den Zustand nicht. Das war der
        // eigentliche Befund — der Knopf hing an `styledButton`.
        QVERIFY2(cq->styleSheet().contains(QStringLiteral(":checked")),
                 "der CQ-Knopf hat keine Farbe fuer den eingerasteten "
                 "Zustand");

        cq->click();
        QVERIFY2(cq->isChecked(),
                 "waehrend der Ruf laeuft, muss der Knopf leuchten");

        // Und er geht wieder aus, wenn der Betreiber Stop drueckt.
        QPushButton* stop = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("Stop")) { stop = b; break; }
        }
        QVERIFY(stop);
        stop->click();
        QVERIFY2(!cq->isChecked(),
                 "nach Stop darf der Knopf nicht weiterleuchten");
    }

    // Tx1-Tx6 sitzen seit dem 2026-10-30 hinter „Manual" (Entwurf C):
    // sechs gleich aussehende Knoepfe in einer Reihe sind Handbetrieb,
    // und Handbetrieb ist beim automatischen Durchlauf die Ausnahme.
    //
    // Was dabei gleich bleiben MUSS: sie legen einen Satz ins Feld und
    // sind fertig. Ein Menueeintrag kann gar nicht einrasten -- aber
    // die sechs muessen vollzaehlig da sein, sonst waeren sie beim
    // Umbau still verschwunden.
    void dieSechsStandardsaetzeStehenUnterManual()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);

        QPushButton* hand = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("Manual")) { hand = b; break; }
        }
        QVERIFY2(hand, "der Knopf „Manual\" fehlt");
        QVERIFY2(!hand->isCheckable(), "Manual rastet ein");
        QVERIFY2(hand->menu(), "Manual hat kein Menue");

        int geprueft = 0;
        for (QAction* a : hand->menu()->actions()) {
            if (!a->text().startsWith(QStringLiteral("Tx"))) { continue; }
            QVERIFY2(!a->isCheckable(),
                     qPrintable(QStringLiteral("%1 rastet ein").arg(a->text())));
            ++geprueft;
        }
        QCOMPARE(geprueft, 6);

        // Und in der Reihe steht keiner mehr -- sonst gaebe es sie
        // zweimal, und die zweite waere die, die niemand pflegt.
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            QVERIFY2(!b->text().startsWith(QStringLiteral("Tx"))
                         || b->text() == QStringLiteral("TX Hz"),
                     qPrintable(QStringLiteral("%1 steht noch als Knopf da")
                                    .arg(b->text())));
        }
    }

    void derStopKnopfHaeltDenAblaufAn()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        applet.listForTest()->setPlainText(
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31"));
        doppelklickInDieListe(applet);
        QVERIFY(applet.ablaufForTest().laeuft());

        QVERIFY(applet.stopButtonForTest());
        applet.stopButtonForTest()->click();

        QVERIFY2(!applet.ablaufForTest().laeuft(),
                 "nach Stop darf kein Ablauf mehr laufen");
        QVERIFY2(!applet.sendetGerade(), "nach Stop darf nichts senden");

        // Gegenprobe gegen das stille Weiterlaufen: ein Fund, der den
        // Ablauf vorher weitergebracht haette, darf nichts mehr tun.
        Ft8Decode d;
        d.text   = QStringLiteral("OE5SOS DL1ABC -12");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
        QVERIFY2(!applet.sendetGerade(),
                 "ein Fund nach Stop darf den Sender nicht wieder tasten");
    }

    // ── Die Landspalte verdirbt den Locator nicht ───────────────────
    //
    // Seit #311 steht hinter der Nachricht das Land. Der Doppelklick
    // las den Locator als LETZTES WORT der Zeile — und das war ab da
    // „Hungary", nicht „JN86". Derselbe Griff holte auch die
    // Nachricht selbst, also stand das Land mit drin.
    void dieLandspalteVerdirbtDenLocatorNicht()
    {
        // Genau die Zeile, die zeilentext() baut: Nachricht auf 19
        // aufgefuellt, dann zwei Leerzeichen, dann das Land.
        const QString mitLand = Ft8Applet::zeilentext(
            QStringLiteral("084200"), 13, 1.2, 1297,
            QStringLiteral("CQ HA1BF JN86"), QStringLiteral("Hungary"));

        QCOMPARE(Ft8Applet::nachrichtAusZeile(mitLand),
                 QStringLiteral("CQ HA1BF JN86"));
        QCOMPARE(Ft8Applet::locatorAusNachricht(
                     Ft8Applet::nachrichtAusZeile(mitLand)),
                 QStringLiteral("JN86"));

        // Ohne Landspalte unveraendert.
        const QString ohneLand = Ft8Applet::zeilentext(
            QStringLiteral("084200"), 13, 1.2, 1297,
            QStringLiteral("CQ HA1BF JN86"), QString());
        QCOMPARE(Ft8Applet::nachrichtAusZeile(ohneLand),
                 QStringLiteral("CQ HA1BF JN86"));

        // Und ein Rapport ist kein Locator.
        QVERIFY(Ft8Applet::locatorAusNachricht(
                    QStringLiteral("OE5SOS DL1ABC R-09")).isEmpty());
        QVERIFY(Ft8Applet::locatorAusNachricht(
                    QStringLiteral("OE5SOS DL1ABC -12")).isEmpty());

        // Auch die Antwort darf das Land nicht mitnehmen.
        QCOMPARE(Ft8Applet::antwortAuf(mitLand, QStringLiteral("OE5SOS"),
                                       QStringLiteral("JN67VV")),
                 QStringLiteral("HA1BF OE5SOS JN67"));
    }

    // ── Der Logbucheintrag eines fertigen QSOs ──────────────────────
    //
    // Geprueft werden die FELDER, nicht die Zustellung: logEintrag()
    // ist statisch, das Signal ist nur der Weg nach draussen
    // (MainWindow -> RotorLogbookPanel::logExternalQso, derselbe wie
    // bei einem QSO aus WSJT-X).
    void derLogbucheintragStimmt()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67VV"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC R-15"), -7));
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::Fertig);

        // 14,074 MHz Wahlfrequenz, oberes Seitenband: der Sendeton
        // liegt darueber.
        const LogEntry e = Ft8Applet::logEintrag(
            a, 14074000.0, /*unteresSeitenband=*/false,
            Ft8Decoder::Protocol::Ft8, QStringLiteral("JN67VV"), 1500);

        QCOMPARE(e.call, QStringLiteral("DL1ABC"));
        QCOMPARE(e.mode, QStringLiteral("FT8"));
        QCOMPARE(e.band, QStringLiteral("20m"));
        QCOMPARE(e.rstSent, QStringLiteral("-07"));
        QCOMPARE(e.rstRcvd, QStringLiteral("-15"));
        QCOMPARE(e.gridSquare, QStringLiteral("JO31"));
        QCOMPARE(e.myGridSquare, QStringLiteral("JN67VV"));
        QVERIFY(e.timeOn.isValid());
        QCOMPARE(e.timeOn.timeSpec(), Qt::UTC);
        // 14,074 MHz + 1500 Hz Sendeton.
        QVERIFY2(qAbs(e.freqMHz - 14.0755) < 1e-6,
                 qPrintable(QStringLiteral("Frequenz %1").arg(e.freqMHz, 0, 'f', 6)));
    }

    // In DIGL liegt der Ton UNTER der Wahlfrequenz — ohne das stuende
    // die Frequenz im Logbuch um bis zu 3 kHz daneben.
    void inDiglLiegtDerTonDarunter()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));

        const LogEntry oben = Ft8Applet::logEintrag(
            a, 14074000.0, false, Ft8Decoder::Protocol::Ft8, QStringLiteral("JN67"), 1500);
        const LogEntry unten = Ft8Applet::logEintrag(
            a, 14074000.0, true, Ft8Decoder::Protocol::Ft8, QStringLiteral("JN67"), 1500);
        QVERIFY2(unten.freqMHz < oben.freqMHz,
                 "in DIGL muss die Frequenz unter der in DIGU liegen");
        QVERIFY(qAbs((oben.freqMHz - unten.freqMHz) - 0.003) < 1e-6);
    }

    // FT4 ist eine eigene ADIF-Betriebsart, kein SUBMODE von FT8.
    void ft4StehtAlsEigeneBetriebsart()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
        const LogEntry e = Ft8Applet::logEintrag(
            a, 7074000.0, false, Ft8Decoder::Protocol::Ft4, QStringLiteral("JN67"), 1500);
        QCOMPARE(e.mode, QStringLiteral("FT4"));
        QVERIFY2(e.submode.isEmpty(), "FT4 ist kein SUBMODE");
        QCOMPARE(e.band, QStringLiteral("40m"));
    }

    // Kam nie ein Feldkenner, bleibt das Feld leer statt geraten.
    void ohneFeldkennerBleibtDasFeldLeer()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC"), -7));
        const LogEntry e = Ft8Applet::logEintrag(
            a, 14074000.0, false, Ft8Decoder::Protocol::Ft8, QStringLiteral("JN67"), 1500);
        QVERIFY(e.gridSquare.isEmpty());
    }

    // ── Der ganze Weg: Doppelklick bis Logbucheintrag ───────────────
    //
    // Und die Wache dabei: zeigeAblauf() laeuft bei jedem Schritt, und
    // der Zustand bleibt auf Fertig stehen. Ohne m_qsoGemeldet stuende
    // dasselbe QSO mehrfach im Logbuch — darum wird hier GEZAEHLT,
    // nicht nur geschaut, ob ueberhaupt etwas kommt.
    void einQsoWirdGenauEinmalGemeldet()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);

        applet.listForTest()->setPlainText(
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31"));
        doppelklickInDieListe(applet);
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeAn);
        QCOMPARE(applet.ablaufForTest().partnerLocator(),
                 QStringLiteral("JO31"));

        const auto fund = [&applet](const char* text, double snr) {
            Ft8Decode d;
            d.text    = QString::fromLatin1(text);
            d.snrDb   = snr;
            d.freqHz  = 1500.0;
            applet.fundForTest(d);
        };

        fund("OE5SOS DL1ABC -09", -12.0);
        QCOMPARE(applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::SendeRReport);

        fund("OE5SOS DL1ABC RR73", -12.0);
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::Sende73);
        QCOMPARE(gemeldet.count(), 0);   // noch nicht fertig

        fund("OE5SOS DL1ABC 73", -12.0);
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::Fertig);
        QCOMPARE(gemeldet.count(), 1);

        // Weitere Funde derselben Station aendern nichts mehr — und
        // melden vor allem nicht noch einmal.
        fund("OE5SOS DL1ABC 73", -12.0);
        fund("OE5SOS DL1ABC RR73", -12.0);
        QCOMPARE(gemeldet.count(), 1);

        // Und der Weg, auf dem die zweite Meldung wirklich entstuende:
        // das zuletzt gesendete Fenster laeuft ab, fensterZuEnde()
        // sieht einen Ablauf, der nicht mehr laeuft, und zeigt ihn an.
        // Ohne die Wache stuende das QSO hier ein zweites Mal im
        // Logbuch — im Betrieb bei JEDEM Durchgang.
        applet.fensterZuEndeForTest();
        applet.fensterZuEndeForTest();
        QCOMPARE(gemeldet.count(), 1);

        const auto e = gemeldet.first().first().value<LogEntry>();
        QCOMPARE(e.call, QStringLiteral("DL1ABC"));
        QCOMPARE(e.mode, QStringLiteral("FT4"));
        QCOMPARE(e.gridSquare, QStringLiteral("JO31"));
        QCOMPARE(e.rstSent, QStringLiteral("-08"));   // SNR der CQ-Zeile
        QCOMPARE(e.rstRcvd, QStringLiteral("-09"));

        applet.abbrechen();
    }

    // ── CQ rufen ────────────────────────────────────────────────────
    //
    // Ft8Ablauf kann die ANRUFER-Rolle seit dem QSO-Ablauf
    // (starteAlsAnrufer, dreizehn Faelle in tst_ft8_ablauf) — nur rief
    // sie bis zum 2026-10-16 niemand auf. Antworten ging per
    // Doppelklick, selbst CQ rufen gar nicht.

    void derCqRufStimmt_data()
    {
        QTest::addColumn<QString>("ruf");
        QTest::addColumn<QString>("locator");
        QTest::addColumn<QString>("erwartet");

        QTest::newRow("mit Locator")
            << QStringLiteral("OE5SOS") << QStringLiteral("JN67VV")
            << QStringLiteral("CQ OE5SOS JN67");
        // Ohne Feldkenner ist „CQ OE5SOS" zulaessig und kommt auf dem
        // Band vor.
        QTest::newRow("ohne Locator")
            << QStringLiteral("OE5SOS") << QString()
            << QStringLiteral("CQ OE5SOS");
        QTest::newRow("klein geschrieben")
            << QStringLiteral("oe5sos") << QStringLiteral("jn67vv")
            << QStringLiteral("CQ OE5SOS JN67");
        // Ohne Rufzeichen gar nichts — sonst stuende „CQ  JN67" auf
        // dem Band.
        QTest::newRow("ohne Ruf")
            << QString() << QStringLiteral("JN67")
            << QString();
        QTest::newRow("nur Leerzeichen")
            << QStringLiteral("   ") << QStringLiteral("JN67")
            << QString();
    }

    void derCqRufStimmt()
    {
        QFETCH(QString, ruf);
        QFETCH(QString, locator);
        QFETCH(QString, erwartet);
        QCOMPARE(Ft8Applet::cqRuf(ruf, locator), erwartet);
    }

    // Ohne „Auto" legt der Knopf den Ruf nur ins Feld — dieselbe
    // Grenze wie beim Doppelklick.
    // ── Der CQ-Knopf ruft CQ ─────────────────────────────────────────
    //
    // Hier stand bis zum 2026-10-22 das Gegenteil: `ohneAutoFuellt
    // DerCqKnopfNurDasFeld` hielt fest, dass ein Druck auf CQ NUR den
    // Text ins Feld schreibt und nur bei eingeschaltetem „Auto" etwas
    // sendet.
    //
    // Der Betreiber hat das am Geraet verworfen: „der cq ruf
    // funktioniert nicht." Und er hat recht -- ein Knopf mit der
    // Aufschrift CQ, der nicht ruft, ist kein Knopf, sondern eine
    // Textschablone. Der alte Lauf wird darum nicht geloescht, sondern
    // umgedreht, damit die Umkehr in der Geschichte steht.
    void derCqKnopfRuftWirklichCq()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        QVERIFY(applet.cqRufenButtonForTest());
        applet.cqRufenButtonForTest()->click();

        QCOMPARE(applet.textFeldForTest()->text(),
                 QStringLiteral("CQ OE5SOS JN67"));
        QVERIFY2(applet.ablaufForTest().laeuft(),
                 "CQ gedrueckt, und es laeuft kein Ruf");
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeCq);
    }

    void dieZahlDerVersucheIstWaehlbarUndBleibt()
    {
        // Betreiber am 2026-10-22: „man soll auch entscheiden koennen,
        // wie oft er das automatisch probiert."
        RadioModel radio;
        radio.installFt8TxForTest();
        {
            Ft8Applet applet(&radio);
            QVERIFY(applet.cqVersucheSpinForTest());
            applet.cqVersucheSpinForTest()->setValue(4);
        }
        // Ueber einen Neustart des Fensters hinweg -- eine Einstellung,
        // die man jedes Mal neu machen muss, ist keine.
        Ft8Applet wieder(&radio);
        QVERIFY(wieder.cqVersucheSpinForTest());
        QCOMPARE(wieder.cqVersucheSpinForTest()->value(), 4);
    }

    // Der CQ-Knopf faengt den Ablauf als ANRUFER an und wartet auf
    // sein Fenster.
    void derCqKnopfWartetAufSeinFenster()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        applet.cqRufenButtonForTest()->click();

        QVERIFY2(applet.ablaufForTest().laeuft(), "der Ablauf muss anlaufen");
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeCq);
        QVERIFY2(applet.ablaufForTest().partner().isEmpty(),
                 "beim CQ-Rufen gibt es noch keinen Partner");
        QVERIFY2(applet.sendetGerade(),
                 "der Ruf muss auf sein Fenster warten");

        applet.abbrechen();
    }

    // Und der ganze Weg aus der Anrufer-Rolle: jemand antwortet, der
    // Rapport geht hinaus, er bestaetigt, RR73 — ein Logbucheintrag.
    void einQsoAusDerAnruferRolle()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);
        applet.cqRufenButtonForTest()->click();
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeCq);

        const auto fund = [&applet](const char* text, double snr) {
            Ft8Decode d;
            d.text   = QString::fromLatin1(text);
            d.snrDb  = snr;
            d.freqHz = 1500.0;
            applet.fundForTest(d);
        };

        fund("OE5SOS DL1ABC JO31", -7.0);
        QCOMPARE(applet.ablaufForTest().partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::SendeReport);

        fund("OE5SOS DL1ABC R-15", -7.0);
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::SendeRR73);
        QCOMPARE(gemeldet.count(), 0);

        // Das gesendete RR73 schliesst das QSO ab -- und weil der Ruf
        // vom CQ-Knopf kam, geht es ohne Klick weiter.
        //
        // Betreiber am 2026-10-30: „Der Durchlauf muss automatisch
        // ablaufen, so schnell wie moeglich ohne mein klicken bei
        // einem QSO. Danach automatisch ins Logbuch." Bis dahin blieb
        // der Ablauf in `Fertig` stehen und es war still, bis jemand
        // wieder drueckte.
        applet.fensterZuEndeForTest();
        QCOMPARE(gemeldet.count(), 1);          // erst ins Logbuch
        QCOMPARE(applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::RufeCq);   // dann weiter rufen
        QVERIFY(applet.ablaufForTest().partner().isEmpty());

        const auto e = gemeldet.first().first().value<LogEntry>();
        QCOMPARE(e.call, QStringLiteral("DL1ABC"));
        QCOMPARE(e.gridSquare, QStringLiteral("JO31"));
        QCOMPARE(e.rstSent, QStringLiteral("-07"));   // was ich von ihm mass
        QCOMPARE(e.rstRcvd, QStringLiteral("-15"));   // was er mir gab

        applet.abbrechen();
    }

    void derRouterLiefertDieFt8QuelleDurch()
    {
        PcMicSource* keinPc = nullptr;
        CompositeTxMicRouter router(keinPc, nullptr, false);

        Ft8TxSource quelle(48000);
        router.setFt8Source(&quelle);
        QCOMPARE(router.ft8Source(), static_cast<TxMicRouter*>(&quelle));

        router.setActiveSource(MicSource::Ft8);

        // Ohne gestartete Quelle: Stille, kein Rauschen, kein Muell.
        QVector<float> puffer(512);
        QCOMPARE(router.pullSamples(puffer.data(), 512), 512);
        for (float v : puffer) { QCOMPARE(v, 0.0f); }

        // Mit gestarteter Quelle kommt das Fenster durch den Router —
        // dieselben Proben, die die Quelle selbst liefert.
        QVERIFY(quelle.vorbereiten(QStringLiteral("CQ OE5SOS JN78"),
                                   1500.0f, Ft8Decoder::Protocol::Ft8));
        quelle.starten();

        bool etwas = false;
        int gelesen = 0;
        while (gelesen < 48000 * 3 && !etwas) {
            QCOMPARE(router.pullSamples(puffer.data(), 512), 512);
            gelesen += 512;
            for (float v : puffer) { if (v != 0.0f) { etwas = true; break; } }
        }
        QVERIFY2(etwas, "durch den Router kommt kein Ton");

        // Und die Gegenprobe: ohne angemeldete Quelle faellt der
        // Router auf Pc zurueck (hier null) und liefert Stille statt
        // zu stuerzen.
        router.setFt8Source(nullptr);
        QCOMPARE(router.pullSamples(puffer.data(), 512), 512);
        for (float v : puffer) { QCOMPARE(v, 0.0f); }
    }

    // =================================================================
    // Neuer Ort, leere Liste (2026-11-17)
    // =================================================================
    //
    // Betreiber: „Generell sollte bei einem Bandwechsel von FT4 auf FT8
    // … alle calls vom vorigen band gelöscht werden." Bis heute blieb
    // die Liste ueber jeden Wechsel stehen; ein Doppelklick auf eine
    // Zeile von 15 m rief auf 17 m ins Leere.
    void einBandwechselLeertDieListe()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(21074000.0);          // 15 m
        applet.setSlice(slice);

        const auto fund = [&applet](const QString& text) {
            Ft8Decode d;
            d.text   = text;
            d.snrDb  = -10;
            d.freqHz = 1500.0;
            applet.fundForTest(d);
        };
        const auto liste = [&applet]() {
            return applet.listForTest()->toPlainText();
        };

        fund(QStringLiteral("CQ DL1ABC JO31"));
        fund(QStringLiteral("OE5SOS F5XYZ JN18"));   // ruft mich -> wartet
        QVERIFY2(liste().contains(QStringLiteral("DL1ABC")), qPrintable(liste()));
        const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();

        // Innerhalb des Bandes: nichts geschieht. Gegenprobe, ohne die
        // das Leeren darunter auch ein „leert bei jedem Dreh" sein
        // koennte.
        slice->setFrequency(21076000.0);
        QVERIFY2(liste().contains(QStringLiteral("DL1ABC")),
                 "ein Dreh innerhalb des Bandes hat die Liste geleert");

        const int wartendVorher = applet.schlangeForTest().wartende(jetzt);

        // 15 m -> 17 m.
        slice->setFrequency(18100000.0);
        QVERIFY2(!liste().contains(QStringLiteral("DL1ABC")),
                 qPrintable(QStringLiteral("die Zeilen von 15 m stehen noch da: %1")
                                .arg(liste())));
        QVERIFY2(!liste().contains(QStringLiteral("F5XYZ")), qPrintable(liste()));
        QCOMPARE(applet.schlangeForTest().wartende(jetzt), 0);
        Q_UNUSED(wartendVorher);

        // Und was auf dem neuen Band kommt, steht wieder da.
        fund(QStringLiteral("CQ EA4IPN IN80"));
        QVERIFY2(liste().contains(QStringLiteral("EA4IPN")), qPrintable(liste()));
    }

    void derWechselZwischenFt8UndFt4LeertDieListe()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);

        Ft8Decode d;
        d.text   = QStringLiteral("CQ DL1ABC JO31");
        d.snrDb  = -10;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
        QVERIFY(applet.listForTest()->toPlainText().contains(QStringLiteral("DL1ABC")));

        // Dasselbe Verfahren noch einmal gesetzt (so kommt es beim
        // Aufbau und aus der Skalenfolge): die Liste bleibt.
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);
        QVERIFY2(applet.listForTest()->toPlainText().contains(QStringLiteral("DL1ABC")),
                 "dasselbe Verfahren noch einmal gesetzt hat die Liste geleert");

        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        QVERIFY2(!applet.listForTest()->toPlainText().contains(QStringLiteral("DL1ABC")),
                 "die FT8-Zeilen stehen nach dem Wechsel auf FT4 noch da");
    }

    // ── Ein Bandwechsel haelt das Senden an (2026-11-18) ──────────────
    //
    // Betreiber: „sobald man einen bandwechsel durchführt dann nicht
    // mehr automatisch weiter gesendet werden!!!!" Am Geraet lief der
    // CQ-Ruf nach 20 m -> 30 m einfach weiter, in eine Antenne mit
    // Stehwelle 2,96.
    void einBandwechselHaeltDasSendenAn()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67"));
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);          // 20 m
        applet.setSlice(slice);

        applet.cqRufenButtonForTest()->click();
        QVERIFY2(applet.sendetGerade(), "der CQ-Ruf ist gar nicht angelaufen");
        QVERIFY(applet.ablaufForTest().laeuft());

        // Innerhalb des Bandes laeuft er weiter (Gegenprobe).
        slice->setFrequency(14076000.0);
        QVERIFY2(applet.ablaufForTest().laeuft(),
                 "ein Dreh innerhalb des Bandes hat den Ruf angehalten");

        // 20 m -> 30 m.
        slice->setFrequency(10136000.0);
        QVERIFY2(!applet.ablaufForTest().laeuft(),
                 "der Ruf laeuft nach dem Bandwechsel weiter");
        QVERIFY2(!applet.sendetGerade(),
                 "nach dem Bandwechsel steht noch eine Sendung an");
        QVERIFY(!applet.sendeTimerForTest()->isActive());
        QVERIFY(!radio.moxController()->isMox());
        const QString zeile = applet.statusForTest()->text();
        QVERIFY2(zeile.contains(QStringLiteral("band changed")),
                 qPrintable(zeile));
    }
};

QTEST_MAIN(TstFt8Senden)
#include "tst_ft8_senden.moc"
