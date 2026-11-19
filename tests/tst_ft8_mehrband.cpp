// =================================================================
// tests/tst_ft8_mehrband.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// FT8 auf zwei Baendern gleichzeitig (2026-11-19).
//
// Betreiber: „ich will 2 bänder, mehr macht derzeit mein trx nicht,
// gleichzeitig empfangen umd ggf. eine dxcc station zu hören."
// Gestaltung: 2B (vom zweiten Band nur neue Laender, graue
// Bandmarke), 3A (Doppelklick tauscht die Baender) und „aber nicht
// automatisch senden … erst dann nach tunen."
//
// Drei Teile, drei Fragen:
//
//  1. Die Lauschscheibe am Draht (core/FlexLauschscheibe): welche
//     Befehle gehen an die Maschine, in welcher Reihenfolge -- und
//     dass KEINER davon sendet. Und: ihre Meldungen halten nie die
//     Hauptscheibe zum Narren (sonst stuende der VFO auf 28,074).
//  2. Die Abgriffe (AudioEngine): Hauptband und Lauschband laufen in
//     getrennten Ringen, nichts spricht ueber, und der Ton des
//     Lauschbandes kommt nicht in den Lautsprecher.
//  3. Das FT8-Fenster: vom Lauschband kommt nur ein neues Land in die
//     Liste, mit Bandmarke; ein Doppelklick tauscht die Baender und
//     sendet nichts.
// =================================================================

#include <QtTest/QtTest>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTimer>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/DxccColorProvider.h"
#include "core/FlexLauschscheibe.h"
#include "core/IAudioBus.h"
#include "core/audio/AudioTapRing.h"
#include "gui/applets/Ft8Applet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <vector>

using namespace Longpath;

namespace {

/// Ein Mitschreiber fuer den Befehlskanal: merkt sich jeden Befehl
/// und haelt die Antworten zurueck, bis der Fall sie gibt.
struct Draht {
    QStringList befehle;
    QVector<FlexLauschscheibe::Antwort> offen;   // gleiche Reihenfolge
    QVector<QString> offenFuer;

    FlexLauschscheibe::Sende sende()
    {
        return [this](const QString& b, FlexLauschscheibe::Antwort a) {
            befehle << b;
            if (a) { offen << a; offenFuer << b; }
        };
    }
    /// Die Antwort auf den ersten offenen Befehl, der mit `anfang`
    /// beginnt.
    void antworte(const QString& anfang, quint32 code, const QString& body)
    {
        for (int i = 0; i < offen.size(); ++i) {
            if (offenFuer.at(i).startsWith(anfang)) {
                auto a = offen.takeAt(i);
                offenFuer.removeAt(i);
                a(code, body);
                return;
            }
        }
        QFAIL(qPrintable(QStringLiteral("keine offene Antwort fuer ") + anfang));
    }
};

QMap<QString, QString> kv(std::initializer_list<std::pair<QString, QString>> l)
{
    QMap<QString, QString> m;
    for (const auto& p : l) { m.insert(p.first, p.second); }
    return m;
}

/// Ein Lauscher, der bis „laeuft" gekommen ist: Scheibe 1, Strom 0x4000009.
void bisLaeuft(FlexLauschscheibe& l, Draht& d)
{
    l.setHauptscheibe(QStringLiteral("0"));
    l.setPanadapter(QStringLiteral("0x40000000"));
    l.setHauptfrequenzHz(21074000.0);
    l.setFrequenzHz(28074000.0);
    d.antworte(QStringLiteral("slice create"), 0, QStringLiteral("1"));
    d.antworte(QStringLiteral("stream create"), 0, QStringLiteral("4000009"));
}

std::vector<float> stereo(int rahmen, float wert)
{
    return std::vector<float>(static_cast<size_t>(rahmen) * 2, wert);
}

bool traegtSignal(const QByteArray& bytes)
{
    const int n = static_cast<int>(bytes.size() / sizeof(float));
    const float* f = reinterpret_cast<const float*>(bytes.constData());
    for (int i = 0; i < n; ++i) {
        if (f[i] != 0.0f) { return true; }
    }
    return false;
}

Ft8Decode fund(const QString& text, float hz, float snr = -12.0f)
{
    Ft8Decode d;
    d.snrDb = snr;
    d.dtSeconds = 0.2f;
    d.freqHz = hz;
    d.score = 40;
    d.text = text;
    return d;
}

/// Ein Logbuch mit einem einzigen Land: Deutschland auf 20 m FT8.
/// Danach ist DL „gearbeitet", 3B8 (Mauritius) neu.
QString schreibeLogbuch(const QTemporaryDir& dir)
{
    const QString pfad = dir.filePath(QStringLiteral("mehrband.adi"));
    QFile f(pfad);
    if (!f.open(QIODevice::WriteOnly)) { return {}; }
    f.write("Longpath test\n<EOH>\n"
            "<CALL:6>DL1ABC<BAND:3>20m<MODE:3>FT8<QSO_DATE:8>20261101"
            "<TIME_ON:4>1200<EOR>\n");
    return pfad;
}

} // namespace

class TstFt8Mehrband : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                         QStringLiteral("JN67"));
    }

    // ── 1. Die Lauschscheibe am Draht ───────────────────────────────

    void dieBefehlsfolgeStimmt()
    {
        Draht d;
        FlexLauschscheibe l(d.sende());
        bisLaeuft(l, d);

        const QStringList erwartet = {
            // im Panadapter, auf der Frequenz der Hauptscheibe
            QStringLiteral("slice create pan=0x40000000 freq=21.074000 mode=DIGU"),
            // erst stumm (nicht in den Lautsprecher), dann der DAX-Kanal
            QStringLiteral("slice set 1 audio_mute=1"),
            QStringLiteral("slice set 1 dax=1"),
            // weg auf das Lauschband, OHNE den Panadapter mitzuziehen
            QStringLiteral("slice tune 1 28.074000 autopan=0"),
            QStringLiteral("stream create type=dax_rx dax_channel=1"),
        };
        QCOMPARE(d.befehle, erwartet);
        QCOMPARE(l.zustand(), FlexLauschscheibe::Zustand::Laeuft);
        QCOMPARE(l.scheibe(), 1);
        QCOMPARE(l.strom(), quint32(0x4000009));
        QVERIFY(l.istLauschstrom(0x4000009));
        QVERIFY(!l.istLauschstrom(0x4000008));   // der Mix-Strom
    }

    void keinBefehlSendet()
    {
        // „aber nicht automatisch senden": nichts, was die Lauschscheibe
        // je schickt, darf den Sender beruehren.
        Draht d;
        FlexLauschscheibe l(d.sende());
        bisLaeuft(l, d);
        l.setFrequenzHz(24915000.0);
        l.setFrequenzHz(0.0);
        for (const QString& b : d.befehle) {
            QVERIFY2(!b.contains(QStringLiteral("tx=1"))
                         && !b.startsWith(QStringLiteral("xmit"))
                         && !b.startsWith(QStringLiteral("transmit"))
                         && !b.contains(QStringLiteral("dax_tx")),
                     qPrintable(b));
        }
    }

    void ohneHauptscheibeWirdNichtsAngelegt()
    {
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setFrequenzHz(28074000.0);
        QVERIFY2(d.befehle.isEmpty(), qPrintable(d.befehle.join('\n')));

        // Sobald Scheibe, Panadapter und Frequenz da sind: nachholen.
        l.setHauptfrequenzHz(21074000.0);
        l.setHauptscheibe(QStringLiteral("0"));
        QVERIFY(d.befehle.isEmpty());
        l.setPanadapter(QStringLiteral("0x40000000"));
        QCOMPARE(d.befehle.size(), 1);
        QVERIFY(d.befehle.first().startsWith(QStringLiteral("slice create")));
    }

    void meldungenDerLauschscheibeBleibenBeiIhr()
    {
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setHauptscheibe(QStringLiteral("0"));
        l.setPanadapter(QStringLiteral("0x40000000"));
        l.setHauptfrequenzHz(21074000.0);
        QSignalSpy gemeldet(&l, &FlexLauschscheibe::frequenzGemeldet);
        l.setFrequenzHz(28074000.0);

        // Die Maschine meldet die neue Scheibe VOR der Antwort.
        QVERIFY(l.nimmStatus(QStringLiteral("slice 1"),
                             kv({{QStringLiteral("RF_frequency"),
                                  QStringLiteral("21.074000")}})));
        // Die Hauptscheibe ist nie die Lauschscheibe.
        QVERIFY(!l.nimmStatus(QStringLiteral("slice 0"),
                              kv({{QStringLiteral("RF_frequency"),
                                   QStringLiteral("21.074000")}})));
        QVERIFY(!l.istLauschscheibe(0));
        // Anderes als Scheiben auch nicht.
        QVERIFY(!l.nimmStatus(QStringLiteral("display pan 0x40000000"),
                              kv({{QStringLiteral("center"),
                                   QStringLiteral("21.1")}})));

        d.antworte(QStringLiteral("slice create"), 0, QStringLiteral("1"));
        QVERIFY(l.istLauschscheibe(1));
        QVERIFY(!l.istLauschscheibe(0));
        QVERIFY(l.nimmStatus(QStringLiteral("slice 1"),
                             kv({{QStringLiteral("RF_frequency"),
                                  QStringLiteral("28.074000")}})));
        QCOMPARE(l.gemeldeteFrequenzHz(), 28074000.0);
        QVERIFY(!gemeldet.isEmpty());
    }

    void abbauenRaeumtDieMaschineAuf()
    {
        Draht d;
        FlexLauschscheibe l(d.sende());
        bisLaeuft(l, d);
        d.befehle.clear();
        l.setFrequenzHz(0.0);
        QCOMPARE(d.befehle,
                 QStringList({QStringLiteral("stream remove 0x4000009"),
                              QStringLiteral("slice remove 1")}));
        QCOMPARE(l.zustand(), FlexLauschscheibe::Zustand::Aus);
        QVERIFY(!l.istLauschscheibe(1));
    }

    void abbauenWaehrendDesAnlegens()
    {
        // Ausgeschaltet, bevor die Nummer zurueck ist: sobald sie kommt,
        // wird die Scheibe entfernt -- nicht eingerichtet.
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setHauptscheibe(QStringLiteral("0"));
        l.setPanadapter(QStringLiteral("0x40000000"));
        l.setHauptfrequenzHz(21074000.0);
        l.setFrequenzHz(28074000.0);
        l.setFrequenzHz(0.0);
        d.befehle.clear();
        d.antworte(QStringLiteral("slice create"), 0, QStringLiteral("1"));
        QCOMPARE(d.befehle, QStringList({QStringLiteral("slice remove 1")}));
        QCOMPARE(l.zustand(), FlexLauschscheibe::Zustand::Aus);
    }

    void dieMaschineVerweigert()
    {
        // 0x50000003: „all slice receivers in use".
        Draht d;
        FlexLauschscheibe l(d.sende());
        QSignalSpy abgelehnt(&l, &FlexLauschscheibe::abgelehnt);
        l.setHauptscheibe(QStringLiteral("0"));
        l.setPanadapter(QStringLiteral("0x40000000"));
        l.setHauptfrequenzHz(21074000.0);
        l.setFrequenzHz(28074000.0);
        d.befehle.clear();
        d.antworte(QStringLiteral("slice create"), 0x50000003u, QString());
        QCOMPARE(abgelehnt.size(), 1);
        QVERIFY(d.befehle.isEmpty());
        QCOMPARE(l.zustand(), FlexLauschscheibe::Zustand::Aus);
        QVERIFY(!l.istLauschscheibe(1));
    }

    void vonAussenEntferntWirdVergessen()
    {
        Draht d;
        FlexLauschscheibe l(d.sende());
        bisLaeuft(l, d);
        d.befehle.clear();
        QVERIFY(l.nimmStatus(QStringLiteral("slice 1"),
                             kv({{QStringLiteral("in_use"), QStringLiteral("0")}})));
        QCOMPARE(l.zustand(), FlexLauschscheibe::Zustand::Aus);
        // Nicht wieder anlegen -- das waere ein Kampf mit dem, der sie
        // entfernt hat.
        QVERIFY(d.befehle.isEmpty());
    }

    // ── 2. Die Abgriffe ──────────────────────────────────────────────

    void dieAbgriffeSprechenNichtUeber()
    {
        RadioModel radio;
        AudioEngine* motor = radio.audioEngine();
        const int haupt = radio.addSlice();

        AudioTapRing hauptRing(48000 * 2);
        AudioTapRing lauschRing(48000 * 2);
        motor->setFt8Tap(&hauptRing, haupt);
        motor->setFt8LauschTap(0, &lauschRing);

        // Der Lauschton (24 kHz, wie die Flex ihn schickt) landet nur
        // im Lauschring.
        const auto laut = stereo(2400, 0.25f);
        for (int i = 0; i < 10; ++i) {
            motor->feedFt8LauschAudio(0, laut.data(), 2400, 24000);
        }
        QVERIFY2(lauschRing.available() > 0, "der Lauschring bleibt leer");
        QCOMPARE(hauptRing.available(), 0);

        // Und der Ton der Hauptscheibe nur im Hauptring.
        std::vector<float> rest(static_cast<size_t>(lauschRing.available()));
        lauschRing.read(rest.data(), static_cast<int>(rest.size()));
        const auto haupton = stereo(480, 0.5f);
        motor->rxBlockReady(haupt, haupton.data(), 480);
        QVERIFY(hauptRing.available() > 0);
        QCOMPARE(lauschRing.available(), 0);

        motor->setFt8Tap(nullptr, -1);
        motor->setFt8LauschTap(0, nullptr);
    }

    void derLauschtonKommtNichtInDenLautsprecher()
    {
        RadioModel radio;
        AudioEngine* motor = radio.audioEngine();
        auto lautsprecher =
            std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 2;
        fmt.sample     = AudioFormat::Sample::Float32;
        lautsprecher->open(fmt);
        FakeAudioBus* bus = lautsprecher.get();
        motor->setSpeakersBusForTest(std::move(lautsprecher));

        AudioTapRing lauschRing(48000 * 2);
        motor->setFt8LauschTap(0, &lauschRing);
        const auto laut = stereo(2400, 0.25f);
        for (int i = 0; i < 10; ++i) {
            motor->feedFt8LauschAudio(0, laut.data(), 2400, 24000);
        }
        QVERIFY(lauschRing.available() > 0);
        QVERIFY2(!traegtSignal(bus->buffer()),
                 "Der Ton des zweiten Bandes gehoert nicht in den Lautsprecher.");
        motor->setFt8LauschTap(0, nullptr);
    }

    void ohneRingWirdNichtsGeschrieben()
    {
        RadioModel radio;
        AudioEngine* motor = radio.audioEngine();
        AudioTapRing ring(4800);
        motor->setFt8LauschTap(0, &ring);
        motor->setFt8LauschTap(0, nullptr);
        const auto laut = stereo(2400, 0.25f);
        motor->feedFt8LauschAudio(0, laut.data(), 2400, 24000);
        QCOMPARE(ring.available(), 0);
        // Und ein Platz ausserhalb des Bereichs tut nichts (und stuerzt nicht).
        motor->feedFt8LauschAudio(AudioEngine::kFt8LauschPlaetze,
                                  laut.data(), 2400, 24000);
    }

    // ── 3. Das FT8-Fenster ───────────────────────────────────────────

    void naheliegendIst10Und15()
    {
        QCOMPARE(Ft8Applet::naheliegendesLauschband(Band::Band15m),
                 std::optional<Band>(Band::Band10m));
        QCOMPARE(Ft8Applet::naheliegendesLauschband(Band::Band10m),
                 std::optional<Band>(Band::Band15m));
        QVERIFY(!Ft8Applet::naheliegendesLauschband(Band::Band40m).has_value());
    }

    void derParserUeberspringtDieBandmarke()
    {
        const QString haupt = Ft8Applet::zeilentext(
            QStringLiteral("014630"), -12, 0.4, 902,
            QStringLiteral("CQ 3B8XX LG89"), QStringLiteral("Mauritius"));
        QString mitMarke = haupt;
        mitMarke.insert(6, QStringLiteral(" 10m "));
        QCOMPARE(Ft8Applet::nachrichtAusZeile(haupt),
                 QStringLiteral("CQ 3B8XX LG89"));
        QCOMPARE(Ft8Applet::nachrichtAusZeile(mitMarke),
                 QStringLiteral("CQ 3B8XX LG89"));
        // Eine Zeile ohne Marke, deren Nachricht mit einer Zahl
        // anfaengt, bleibt wie sie ist.
        QString leer = haupt;
        leer.insert(6, QStringLiteral("     "));
        QCOMPARE(Ft8Applet::nachrichtAusZeile(leer),
                 QStringLiteral("CQ 3B8XX LG89"));
    }

    void vomLauschbandNurNeueLaenderMitMarke()
    {
        QTemporaryDir dir;
        RadioModel radio;
        auto* dxcc = radio.dxccColorProvider();
        QVERIFY(dxcc);
        QSignalSpy fertig(dxcc, &DxccColorProvider::importFinished);
        dxcc->importAdifFile(schreibeLogbuch(dir));
        QVERIFY(fertig.wait(5000));
        dxcc->setEnabled(true);

        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(21074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschfrequenzHz(28074000.0);
        QCOMPARE(applet.lauschband(), QStringLiteral("10m"));

        applet.lauschPeriodeForTest({
            fund(QStringLiteral("CQ 3B8XX LG89"), 902.0f),      // neu
            fund(QStringLiteral("CQ DL2XYZ JN58"), 1300.0f),    // DL gearbeitet
            fund(QStringLiteral("DL4QQ DL5RR RR73"), 1840.0f),  // auch DL
        });

        const QString liste = applet.listForTest()->toPlainText();
        QVERIFY2(liste.contains(QStringLiteral("3B8XX")), qPrintable(liste));
        QVERIFY2(!liste.contains(QStringLiteral("DL2XYZ")), qPrintable(liste));
        QVERIFY2(!liste.contains(QStringLiteral("DL5RR")), qPrintable(liste));
        // Die Marke steht direkt nach der Uhrzeit.
        const QString zeile = liste.split('\n').filter(QStringLiteral("3B8XX")).value(0);
        QVERIFY2(zeile.mid(6).trimmed().startsWith(QStringLiteral("10m")),
                 qPrintable(zeile));
        QCOMPARE(applet.lauschFundeZuletzt(), 3);
        QVERIFY2(applet.lastLabelForTest()->text().contains(QStringLiteral("10m: 3 heard")),
                 qPrintable(applet.lastLabelForTest()->text()));

        // Die Zeilen des Hauptbandes bekommen dieselbe Spalte, leer.
        applet.periodeForTest({fund(QStringLiteral("CQ DL3AAA JN48"), 1500.0f)});
        const QString danach = applet.listForTest()->toPlainText();
        const QString hz = danach.split('\n').filter(QStringLiteral("DL3AAA")).value(0);
        QVERIFY2(hz.mid(6, 5).trimmed().isEmpty(), qPrintable(hz));
    }

    void doppelklickTauschtUndSendetNicht()
    {
        QTemporaryDir dir;
        RadioModel radio;
        auto* dxcc = radio.dxccColorProvider();
        QSignalSpy fertig(dxcc, &DxccColorProvider::importFinished);
        dxcc->importAdifFile(schreibeLogbuch(dir));
        QVERIFY(fertig.wait(5000));
        dxcc->setEnabled(true);

        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(21074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschfrequenzHz(28074000.0);
        applet.lauschPeriodeForTest({fund(QStringLiteral("CQ 3B8XX LG89"), 902.0f)});

        const QString zeile = applet.listForTest()->toPlainText()
                                  .split('\n').filter(QStringLiteral("3B8XX")).value(0);
        QVERIFY(!zeile.isEmpty());
        applet.doppelklickForTest(zeile);

        // Das Hauptband ist jetzt 10 m, auf der Frequenz des Lauschbandes ...
        QCOMPARE(slice->frequency(), 28074000.0);
        // ... und gelauscht wird auf dem alten Hauptband, auf seiner
        // FT8-Anrufstelle.
        QCOMPARE(applet.lauschband(), QStringLiteral("15m"));
        QCOMPARE(applet.lauschfrequenzHz(), 21074000.0);

        // Nichts gesendet, nichts vorbereitet zum Senden.
        QVERIFY(!applet.ablaufForTest().laeuft());
        QVERIFY(!applet.sendeTimerForTest() || !applet.sendeTimerForTest()->isActive());
        // Die Antwort steht im Feld -- gesendet wird erst auf Druck.
        QVERIFY2(applet.textFeldForTest()->text().startsWith(QStringLiteral("3B8XX OE5SOS")),
                 qPrintable(applet.textFeldForTest()->text()));
        QVERIFY2(applet.statusForTest()->text().contains(QStringLiteral("nothing sent")),
                 qPrintable(applet.statusForTest()->text()));
    }

    void derVfoAufsLauschbandTauschtAuch()
    {
        // 3A gilt nicht nur fuer den Doppelklick: wer das Hauptband
        // von Hand auf das Lauschband dreht, lauscht danach auf dem
        // alten Hauptband -- nie zweimal auf demselben.
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(21074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschfrequenzHz(28074000.0);

        slice->setFrequency(28074000.0);
        QCOMPARE(applet.lauschband(), QStringLiteral("15m"));

        // Und auf dem Hauptband selbst laesst es sich nicht lauschen.
        applet.setLauschfrequenzHz(28074000.0);
        QCOMPARE(applet.lauschband(), QStringLiteral("15m"));
    }

    void dasFeldAlsoZeigtDieWahl()
    {
        // 1A: „Off", dann das naheliegende Band, dann der Rest -- ohne
        // das Hauptband, jedes auf seiner FT8-Anrufstelle.
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(21074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);

        QComboBox* feld = applet.lauschFeldForTest();
        QVERIFY(feld);
        QCOMPARE(feld->itemText(0), QStringLiteral("Off"));
        QVERIFY2(feld->itemText(1).startsWith(QStringLiteral("10m")),
                 qPrintable(feld->itemText(1)));
        QCOMPARE(feld->itemData(1).toDouble(), 28074000.0);
        for (int i = 0; i < feld->count(); ++i) {
            QVERIFY2(!feld->itemText(i).startsWith(QStringLiteral("15m")),
                     "das Hauptband gehoert nicht in die Auswahl");
        }
        QCOMPARE(feld->currentIndex(), 0);

        // Waehlen wie mit der Maus.
        feld->setCurrentIndex(1);
        emit feld->activated(1);
        QCOMPARE(applet.lauschband(), QStringLiteral("10m"));
        QCOMPARE(feld->currentIndex(), 1);

        // Nach dem Tausch steht 15m gewaehlt, und 10m fehlt.
        slice->setFrequency(28074000.0);
        QVERIFY2(feld->currentText().startsWith(QStringLiteral("15m")),
                 qPrintable(feld->currentText()));
        for (int i = 0; i < feld->count(); ++i) {
            QVERIFY(!feld->itemText(i).startsWith(QStringLiteral("10m")));
        }

        // Und „Off" schaltet ab.
        emit feld->activated(0);
        QCOMPARE(applet.lauschfrequenzHz(), 0.0);
    }

    void blattFuerDenBetreiber()
    {
        // Das echte Fenster mit Lauschband, als Bild -- nur auf Wunsch.
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()
            || qEnvironmentVariable("LONGPATH_ENTWURF") != QStringLiteral("1")) {
            QSKIP("braucht LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1");
        }
        QTemporaryDir tmp;
        RadioModel radio;
        auto* dxcc = radio.dxccColorProvider();
        QSignalSpy fertig(dxcc, &DxccColorProvider::importFinished);
        // Ein Logbuch wie im Betrieb: alles gearbeitet ausser
        // Kasachstan (Hauptband) und Mauritius (Lauschband).
        const QString pfad = tmp.filePath(QStringLiteral("blatt.adi"));
        {
            QFile f(pfad);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QByteArray adi("Longpath test\n<EOH>\n");
            for (const char* ruf : {"DL1ABC", "PD1AA", "R6AA", "OE3AA", "F5AA"}) {
                adi += QByteArray("<CALL:") + QByteArray::number(int(qstrlen(ruf)))
                       + ">" + ruf + "<BAND:3>20m<MODE:3>FT8<QSO_DATE:8>20261101"
                       "<TIME_ON:4>1200<EOR>\n";
            }
            f.write(adi);
        }
        dxcc->importAdifFile(pfad);
        QVERIFY(fertig.wait(5000));
        dxcc->setEnabled(true);

        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(21074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschfrequenzHz(28074000.0);
        applet.periodeForTest({
            fund(QStringLiteral("CQ UN0GY MO52"), 1337.0f, -3.0f),
            fund(QStringLiteral("OE5SOS PD4RJ JO33"), 1512.0f, -9.0f),
            fund(QStringLiteral("DL1ABC R6FMM KN95"), 1790.0f, -8.0f),
        });
        applet.lauschPeriodeForTest({
            fund(QStringLiteral("CQ 3B8XX LG89"), 902.0f, -12.0f),
            fund(QStringLiteral("CQ DL2XYZ JN58"), 1300.0f),
        });
        applet.periodeForTest({
            fund(QStringLiteral("F5PZT OE3TVJ RR73"), 611.0f, 4.0f),
        });
        applet.resize(1000, 600);
        applet.show();
        QTest::qWait(400);
        const QImage bild = applet.grab().toImage();
        const QString bildPfad = dir + QStringLiteral("/ft8-zwei-baender.png");
        QVERIFY2(bild.save(bildPfad), qPrintable(bildPfad));
        qInfo() << "Blatt:" << bildPfad << bild.size();
    }

    void ohneZweiteScheibeKeinLauschband()
    {
        // Ein Geraet ohne Lauschscheibe (hier: gar keines) -- dann
        // stuende sonst „10m: 0 heard" da, und niemand hoerte zu.
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.setLauschfrequenzHz(28074000.0);
        QCOMPARE(applet.lauschfrequenzHz(), 0.0);
        QVERIFY(applet.lauschband().isEmpty());
    }
};

QTEST_MAIN(TstFt8Mehrband)
#include "tst_ft8_mehrband.moc"
