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
#include <QApplication>
#include <QComboBox>
#include <QSplitter>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
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

#include <cmath>
#include <limits>
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
            // die Scheibe des Betreibers bleibt die aktive
            QStringLiteral("slice set 0 active=1"),
            QStringLiteral("stream create type=dax_rx dax_channel=1"),
        };
        QCOMPARE(d.befehle, erwartet);
        QCOMPARE(l.zustand(), FlexLauschscheibe::Zustand::Laeuft);
        QCOMPARE(l.scheibe(), 1);
        QCOMPARE(l.strom(), quint32(0x4000009));
        QVERIFY(l.istLauschstrom(0x4000009));
        QVERIFY(!l.istLauschstrom(0x4000008));   // der Mix-Strom
    }

    void derBandwechselStimmtDieLauschscheibe()
    {
        // Am Geraet, 2026-11-20 09:15: „Lauschband 40m" -- und im
        // Protokoll keine Zeile, dass die Scheibe dorthin ging. Der
        // Befehl muss hinausgehen, und seine Bestaetigung muss sichtbar
        // sein (die Maschine echot eigene Abstimmbefehle nicht).
        Draht d;
        FlexLauschscheibe l(d.sende());
        bisLaeuft(l, d);                      // Scheibe 1 auf 28.074
        d.befehle.clear();
        l.setFrequenzHz(0, 7074000.0);
        QCOMPARE(d.befehle,
                 QStringList({QStringLiteral("slice tune 1 7.074000 autopan=0")}));
        // Mit Rueckruf, damit die Bestaetigung ins Protokoll kommt.
        d.antworte(QStringLiteral("slice tune 1 7.074000"), 0, QString());
        QCOMPARE(l.gewuenschteFrequenzHz(0), 7074000.0);
        // Und beim Anlegen geht die Lauschfrequenz gleich nach dem
        // Anlegen hinaus -- angelegt wird in der Panadapter-Mitte, damit
        // die Maschine den Panadapter nicht mitzieht.
        Draht e;
        FlexLauschscheibe m(e.sende());
        m.setHauptscheibe(QStringLiteral("0"));
        m.setPanadapter(QStringLiteral("0x40000000"));
        m.setHauptfrequenzHz(14074000.0);
        m.setFrequenzHz(0, 21074000.0);
        e.antworte(QStringLiteral("slice create"), 0, QStringLiteral("1"));
        QVERIFY2(e.befehle.contains(QStringLiteral("slice tune 1 21.074000 autopan=0")),
                 qPrintable(e.befehle.join('\n')));
    }

    void sieEntstehtInDerMitteDesPanadapters()
    {
        // Am Geraet, 2026-11-19 18:29: Longpath hatte die Hauptscheibe
        // auf 14.074 gestellt, die Maschine meldete noch 21.074 -- die
        // Lauschscheibe entstand dort, ausserhalb des Panadapters.
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setHauptscheibe(QStringLiteral("0"));
        l.setPanadapter(QStringLiteral("0x40000000"));
        l.setHauptfrequenzHz(21074000.0);           // veraltet
        l.setPanMitteQuelle([]() { return 14074000.0; });
        l.setFrequenzHz(28074000.0);
        QCOMPARE(d.befehle.value(0),
                 QStringLiteral("slice create pan=0x40000000 freq=14.074000 mode=DIGU"));
    }

    void unbrauchbareProbenWerdenVerworfen()
    {
        // Am Geraet, 2026-11-20 10:32: „Spitze 708.3 dBFS" in der ersten
        // Minute nach dem Anlegen -- einzelne Werte um 1e35.
        QVector<float> ton = {0.1f, -0.2f, 1.0e35f,
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),
                              -0.9f, 7.9f, -12.0f};
        float groesster = 0.0f;
        QCOMPARE(FlexLauschscheibe::bereinigeTon(ton, &groesster), 4);
        QVERIFY(std::isinf(groesster));
        const QVector<float> erwartet = {0.1f, -0.2f, 0.0f, 0.0f, 0.0f,
                                         -0.9f, 7.9f, 0.0f};
        QCOMPARE(ton, erwartet);

        // Und auf dem Weg durch die Lauschscheibe: was danach gemessen und
        // weitergereicht wird, ist sauber.
        Draht d;
        FlexLauschscheibe l(d.sende());
        bisLaeuft(l, d);
        QVector<float> paket(256, 0.25f);
        paket[17] = 1.0e35f;
        l.tonAngekommen(0, paket);
        QCOMPARE(paket[17], 0.0f);
        for (float w : paket) { QVERIFY(std::isfinite(w) && std::fabs(w) <= 1.0f); }
    }

    void derPegelWirdGemessen()
    {
        Draht d;
        FlexLauschscheibe l(d.sende());
        bisLaeuft(l, d);
        // Kein Absturz, kein Befehl -- nur Protokoll.
        QVector<float> laut(512, 0.1f), still(512, 0.0f);
        l.tonAngekommen(laut);
        l.tonAngekommen(still);
        QVERIFY(!d.befehle.join('\n').contains(QStringLiteral("tx")));
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

    // ── 1b. Die Maschine merkt sich die Lauschscheibe (am Geraet) ────
    //
    // 2026-11-19, nach einem Neustart der Flex: beim Anmelden kam
    //   slice 0 … RF_frequency=14.074 tx=1 client_handle=0x08587180
    //   slice 1 … RF_frequency=21.074 tx=0 audio_mute=1 client_handle=0x08587180
    // und Longpath zog VFO und Panadapter auf 21,074 MHz, waehrend die
    // Sendescheibe auf 14,074 stand.

    static QMap<QString, QString> scheibe(const char* rf, const char* tx,
                                          const char* handle = "0x08587180")
    {
        return kv({{QStringLiteral("in_use"), QStringLiteral("1")},
                   {QStringLiteral("RF_frequency"), QString::fromLatin1(rf)},
                   {QStringLiteral("tx"), QString::fromLatin1(tx)},
                   {QStringLiteral("audio_level"), QStringLiteral("50")},
                   {QStringLiteral("client_handle"), QString::fromLatin1(handle)}});
    }

    void dieUebrigeKommtNieAnDenVfo()
    {
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setEigenerHandle(0x08587180);
        // Vor der Anmeldung: Hauptscheibe sendet, die uebrige nicht.
        QVERIFY(!l.nimmStatus(QStringLiteral("slice 0"), scheibe("14.074000", "1")));
        QVERIFY(l.nimmStatus(QStringLiteral("slice 1"), scheibe("21.074000", "0")));
        QVERIFY(l.darfAnDenVfo(0));
        QVERIFY(!l.darfAnDenVfo(1));
        QVERIFY(l.istNichtHaupt(1));
        QVERIFY(!l.istNichtHaupt(0));
        // Entfernt wird erst, wenn die Hauptscheibe feststeht.
        QVERIFY(d.befehle.isEmpty());
        l.setHauptscheibe(QStringLiteral("0"));
        QCOMPARE(d.befehle, QStringList({QStringLiteral("slice remove 1")}));
        QVERIFY(!l.darfAnDenVfo(1));
        QVERIFY(l.istFremd(1));
        // Weitere Meldungen: kein zweites Entfernen, weiter geschluckt.
        QVERIFY(l.nimmStatus(QStringLiteral("slice 1"), scheibe("21.074000", "0")));
        QCOMPARE(d.befehle.size(), 1);
        // Und wenn sie weg ist, ist sie weg.
        QVERIFY(l.nimmStatus(QStringLiteral("slice 1"),
                             kv({{QStringLiteral("in_use"), QStringLiteral("0")}})));
        QVERIFY(!l.istFremd(1));
    }

    void dieUebrigeWirdUebernommenWennGelauschtWird()
    {
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setEigenerHandle(0x08587180);
        l.setPanadapter(QStringLiteral("0x40000000"));
        l.setHauptfrequenzHz(14074000.0);
        l.setFrequenzHz(21074000.0);           // gewuenscht, noch nicht bereit
        l.nimmStatus(QStringLiteral("slice 0"), scheibe("14.074000", "1"));
        l.nimmStatus(QStringLiteral("slice 1"), scheibe("21.074000", "0"));
        QVERIFY(d.befehle.isEmpty());
        l.setHauptscheibe(QStringLiteral("0"));
        // Uebernommen statt neu angelegt -- keine dritte Scheibe.
        QVERIFY2(!d.befehle.join('\n').contains(QStringLiteral("slice create")),
                 qPrintable(d.befehle.join('\n')));
        QVERIFY(!d.befehle.contains(QStringLiteral("slice remove 1")));
        QVERIFY(d.befehle.contains(QStringLiteral("slice set 1 dax=1")));
        QVERIFY(d.befehle.contains(QStringLiteral("slice tune 1 21.074000 autopan=0")));
        QCOMPARE(l.scheibe(), 1);
    }

    void eineHauptscheibeOhneTxWirdNieEntfernt()
    {
        // Eine einzige Scheibe, die (noch) nicht sendet: das ist die des
        // Betreibers, nicht eine uebrige.
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setEigenerHandle(0x08587180);
        QVERIFY(!l.nimmStatus(QStringLiteral("slice 0"), scheibe("14.074000", "0")));
        QVERIFY(!l.istNichtHaupt(0));
        QVERIFY(l.darfAnDenVfo(0));           // die einzige -- sie darf
        l.setHauptscheibe(QStringLiteral("0"));
        QVERIFY(l.darfAnDenVfo(0));
        QVERIFY(d.befehle.isEmpty());
    }

    void fremdeClientsBleibenUnberuehrt()
    {
        // Die Scheibe eines anderen Clients (Maestro, SmartSDR) ist nicht
        // unsere Sache -- nie entfernen.
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setEigenerHandle(0x08587180);
        l.nimmStatus(QStringLiteral("slice 0"), scheibe("14.074000", "1"));
        QVERIFY(!l.nimmStatus(QStringLiteral("slice 1"),
                              scheibe("7.074000", "0", "0x11111111")));
        l.setHauptscheibe(QStringLiteral("0"));
        QVERIFY(d.befehle.isEmpty());
        QVERIFY(!l.istFremd(1));
        QVERIFY(!l.darfAnDenVfo(1));          // an den VFO aber trotzdem nicht
    }

    void nurDerEigenePanadapterWirdGezeichnet()
    {
        // Am Geraet nach dem Neustart: ein zweiter Panadapter
        // (0x40000001, 100 Bins) mischte sich ins Bild.
        RadioModel radio;
        QSignalSpy bild(&radio, &RadioModel::flexSpectrumReady);
        const QVector<float> rahmen(100, -120.0f);
        radio.meldeFlexSpektrum(0x40000001u, rahmen);   // Pan noch unbekannt:
        QCOMPARE(bild.size(), 1);                       // wie bisher durch
        radio.setzeFlexPanIdForTest(QStringLiteral("0x40000000"));
        radio.meldeFlexSpektrum(0x40000001u, rahmen);
        QCOMPARE(bild.size(), 1);                       // fremd: nicht gezeichnet
        radio.meldeFlexSpektrum(0x40000000u, QVector<float>(4096, -120.0f));
        QCOMPARE(bild.size(), 2);
        QCOMPARE(bild.last().at(0).toUInt(), 0x40000000u);
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

    void dieWahlFuerDasZweiteBand()
    {
        // „sprich was man noch vom anderen band sehen will! zum bespiel
        // nur CQ oder nur grids, die man noch nie gearbeitet hat"
        using A = Ft8Applet;
        const QStringList land{QStringLiteral("NeuesLand")};
        // Vorgabe: nur neue Laender.
        QVERIFY(A::lauschZeileGewuenscht(land, true, false, false, false));
        QVERIFY(!A::lauschZeileGewuenscht(land, false, true, true, true));
        // Neu auf diesem Band schliesst neue Laender ein.
        const QStringList band{QStringLiteral("NeuesBand")};
        QVERIFY(A::lauschZeileGewuenscht(band, false, true, false, false));
        QVERIFY(A::lauschZeileGewuenscht(band, true, false, false, false));
        QVERIFY(!A::lauschZeileGewuenscht(band, false, false, true, false));
        // Nur neue Felder.
        const QStringList grid{QStringLiteral("NeuesGrid")};
        QVERIFY(A::lauschZeileGewuenscht(grid, false, false, true, false));
        QVERIFY(!A::lauschZeileGewuenscht(grid, true, false, false, true));
        // Neue Felder, aber nur CQ.
        const QStringList gridCq{QStringLiteral("NeuesGrid"), QStringLiteral("NurCq")};
        QVERIFY(A::lauschZeileGewuenscht(gridCq, false, false, true, true));
        QVERIFY(!A::lauschZeileGewuenscht(gridCq, false, false, true, false));
        // Nur CQ, ohne Grund: jedes CQ.
        const QStringList nurCq{QStringLiteral("NurCq")};
        QVERIFY(A::lauschZeileGewuenscht(nurCq, false, false, false, true));
        QVERIFY(!A::lauschZeileGewuenscht(nurCq, true, false, false, false));
        // Nichts angekreuzt: alles.
        QVERIFY(A::lauschZeileGewuenscht({}, false, false, false, false));

        // Gespeichert und gelesen. Nie gesetzt: wie das Hauptband
        // (Vorgabe seit 2026-11-20: „wenn ich hauptband ändere, soll sich
        // auch das nebenband genauso mitändern").
        auto& s = AppSettings::instance();
        s.remove(QStringLiteral("Ft8/NebenbandFilter"));
        QVERIFY(A::nebenbandWieHauptband());
        QVERIFY(A::lauschbandZeigt().isEmpty());
        QVERIFY(A::nebenbandFilterText().isEmpty());
        s.setValue(QStringLiteral("Ft8/NebenbandFilter"), QStringLiteral("NeuesGrid,NurCq"));
        QCOMPARE(A::lauschbandZeigt(), gridCq);
        s.setValue(QStringLiteral("Ft8/NebenbandFilter"), QStringLiteral("Alles"));
        QVERIFY(A::lauschbandZeigt().isEmpty());
        s.setValue(QStringLiteral("Ft8/NebenbandFilter"), QStringLiteral("Unsinn"));
        QCOMPARE(A::lauschbandZeigt(), land);
        s.remove(QStringLiteral("Ft8/NebenbandFilter"));
    }

    void imFensterWirktDieWahl()
    {
        QTemporaryDir dir;
        RadioModel radio;
        auto* dxcc = radio.dxccColorProvider();
        QSignalSpy fertig(dxcc, &DxccColorProvider::importFinished);
        dxcc->importAdifFile(schreibeLogbuch(dir));     // DL auf 20 m
        QVERIFY(fertig.wait(5000));
        dxcc->setEnabled(true);

        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(21074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschfrequenzHz(28074000.0);

        auto& s = AppSettings::instance();
        // „Neu auf diesem Band": DL steht nur auf 20 m im Log, also ist
        // DL auf 10 m neu auf diesem Band.
        s.setValue(QStringLiteral("Ft8/NebenbandFilter"), QStringLiteral("NeuesBand"));
        applet.lauschPeriodeForTest({fund(QStringLiteral("CQ DL2XYZ JN58"), 1300.0f)});
        QVERIFY2(applet.lauschListeForTest()->toPlainText().contains(QStringLiteral("DL2XYZ")),
                 qPrintable(applet.lauschListeForTest()->toPlainText()));
        // „CQ only": ein Rapport vom zweiten Band kommt nicht, ein CQ schon.
        s.setValue(QStringLiteral("Ft8/NebenbandFilter"), QStringLiteral("NurCq"));
        applet.lauschPeriodeForTest({fund(QStringLiteral("DL4QQ DL5RR RR73"), 1840.0f),
                                     fund(QStringLiteral("CQ DL7ABC JO62"), 900.0f)});
        const QString l = applet.lauschListeForTest()->toPlainText();
        QVERIFY2(!l.contains(QStringLiteral("DL5RR")), qPrintable(l));
        QVERIFY2(l.contains(QStringLiteral("DL7ABC")), qPrintable(l));
        s.remove(QStringLiteral("Ft8/NebenbandFilter"));
    }

    void dasNebenbandFolgtDenPillen()
    {
        // „grundversion, beide gleich, bzw. wenn ich hauptband ändere,
        // soll sich auch das nebenband genauso mitändern"
        auto& s = AppSettings::instance();
        s.remove(QStringLiteral("Ft8/NebenbandFilter"));
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschfrequenzHz(0, 7074000.0);
        applet.lauschPeriodeForTest(0, {fund(QStringLiteral("CQ AA1AAA FN42"), 900.0f),
                                        fund(QStringLiteral("K1ABC W9XYZ EN52"), 1300.0f),
                                        fund(QStringLiteral("OE5SOS DL7ZZ JO62"), 1500.0f)});
        QPlainTextEdit* neben = applet.lauschListeForTest(0);
        // Ab Werk alles, wie die Hauptliste.
        QVERIFY(neben->toPlainText().contains(QStringLiteral("W9XYZ")));
        QVERIFY(neben->toPlainText().contains(QStringLiteral("AA1AAA")));
        QVERIFY(applet.lauschTitelForTest()->text() == QStringLiteral("40M · 3 HEARD"));
        // Pille CQ im Hauptband: das Nebenband filtert genauso.
        applet.nurCqButtonForTest()->click();
        QVERIFY2(!neben->toPlainText().contains(QStringLiteral("W9XYZ")),
                 qPrintable(neben->toPlainText()));
        QVERIFY(neben->toPlainText().contains(QStringLiteral("AA1AAA")));
        // ... und wer mich ruft, bleibt sichtbar (wie im Hauptband).
        QVERIFY(neben->toPlainText().contains(QStringLiteral("DL7ZZ")));
        applet.sichtAllesButtonForTest()->click();
        QVERIFY(neben->toPlainText().contains(QStringLiteral("W9XYZ")));

        // Eigener Filter: die Pillen wirken NICHT mehr, und der Filter
        // steht im Titel.
        s.setValue(QStringLiteral("Ft8/NebenbandFilter"), QStringLiteral("NurCq"));
        applet.sichtAllesButtonForTest()->click();   // neu zeichnen
        QVERIFY(!neben->toPlainText().contains(QStringLiteral("W9XYZ")));
        QVERIFY(neben->toPlainText().contains(QStringLiteral("AA1AAA")));
        QVERIFY2(applet.lauschTitelForTest()->text().endsWith(QStringLiteral("· CQ ONLY")),
                 qPrintable(applet.lauschTitelForTest()->text()));
        applet.nurNeueLaenderButtonForTest()->click();   // Hauptband „New DXCC"
        QVERIFY2(neben->toPlainText().contains(QStringLiteral("AA1AAA")),
                 "eigener Filter: die Pille des Hauptbands wirkt nicht");
        s.remove(QStringLiteral("Ft8/NebenbandFilter"));
    }

    void dieZahlFaelltNichtNachDerSendung()
    {
        // Am Geraet, 2026-11-20 10:46: „15M · 0 HEARD" -- im eigenen
        // Sendedurchgang hoert die Flex (eine Empfangseinheit) auf keinem
        // Band, und die Zahl sprang auf 0.
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschfrequenzHz(0, 21074000.0);
        QVector<Ft8Decode> neun;
        for (int i = 0; i < 9; ++i) {
            neun << fund(QStringLiteral("CQ AA%1AA FN42").arg(i), 500.0f + 100.0f * i);
        }
        applet.lauschPeriodeForTest(0, neun);
        QCOMPARE(applet.lauschTitelForTest()->text(), QStringLiteral("15M · 9 HEARD"));
        applet.lauschPeriodeForTest(0, {});            // eigener Sendedurchgang
        QCOMPARE(applet.lauschTitelForTest()->text(), QStringLiteral("15M · 9 HEARD"));
        applet.lauschPeriodeForTest(0, {});            // und noch einer: wirklich still
        QCOMPARE(applet.lauschTitelForTest()->text(), QStringLiteral("15M · 0 HEARD"));
    }

    void dieZeileSiehtAusWieImHauptband()
    {
        // „ich denke, es soll alles so wie im hauptband aussehen": dieselbe
        // Zeile, dieselbe Art (an mich / CQ / neues Land).
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschfrequenzHz(0, 7074000.0);
        const QVector<Ft8Decode> periode = {fund(QStringLiteral("CQ AA1AAA FN42"), 900.0f)};
        applet.periodeForTest(periode);
        applet.lauschPeriodeForTest(0, periode);
        const QString haupt = applet.listForTest()->toPlainText()
                                  .split('\n').filter(QStringLiteral("AA1AAA")).value(0);
        const QString neben = applet.lauschListeForTest(0)->toPlainText()
                                  .split('\n').filter(QStringLiteral("AA1AAA")).value(0);
        QVERIFY(!haupt.isEmpty());
        // Bis auf die Uhrzeit (zwei Aufrufe, zwei Sekunden) gleich.
        QCOMPARE(neben.mid(6), haupt.mid(6));
    }

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
        QCOMPARE(Ft8Applet::bandmarkeAusZeile(mitMarke), QStringLiteral("10m"));
        QCOMPARE(Ft8Applet::bandmarkeAusZeile(haupt), QString());
        QCOMPARE(Ft8Applet::ohneBandmarke(mitMarke).simplified(),
                 haupt.simplified());
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
        AppSettings::instance().setValue(QStringLiteral("Ft8/NebenbandFilter"),
                                         QStringLiteral("NeuesLand"));

        applet.lauschPeriodeForTest({
            fund(QStringLiteral("CQ 3B8XX LG89"), 902.0f),      // neu
            fund(QStringLiteral("CQ DL2XYZ JN58"), 1300.0f),    // DL gearbeitet
            fund(QStringLiteral("DL4QQ DL5RR RR73"), 1840.0f),  // auch DL
        });

        // Gestaltung D (2026-11-19): das zweite Band hat seine eigene
        // Spalte; die Hauptliste bleibt dem Hauptband.
        QVERIFY(applet.lauschSpalteForTest()->isVisibleTo(&applet));
        const QString liste = applet.lauschListeForTest()->toPlainText();
        QVERIFY2(liste.contains(QStringLiteral("3B8XX")), qPrintable(liste));
        QVERIFY2(!liste.contains(QStringLiteral("DL2XYZ")), qPrintable(liste));
        QVERIFY2(!liste.contains(QStringLiteral("DL5RR")), qPrintable(liste));
        QVERIFY2(!applet.listForTest()->toPlainText().contains(QStringLiteral("3B8XX")),
                 "das zweite Band gehoert nicht in die Hauptliste");
        QCOMPARE(applet.lauschFundeZuletzt(), 3);
        // Eigener Filter: er steht im Titel.
        QCOMPARE(applet.lauschTitelForTest()->text(),
                 QStringLiteral("10M · 3 HEARD · NEW DXCC"));

        // Und umgekehrt: das Hauptband kommt nicht in die Spalte.
        applet.periodeForTest({fund(QStringLiteral("CQ DL3AAA JN48"), 1500.0f)});
        QVERIFY(applet.listForTest()->toPlainText().contains(QStringLiteral("DL3AAA")));
        QVERIFY(!applet.lauschListeForTest()->toPlainText().contains(QStringLiteral("DL3AAA")));
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

        const QString zeile = applet.lauschListeForTest()->toPlainText()
                                  .split('\n').filter(QStringLiteral("3B8XX")).value(0);
        QVERIFY(!zeile.isEmpty());
        applet.lauschDoppelklickForTest(0, zeile);

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

    void dreiPlaetzeWieAnDer8600()
    {
        // „wichtig ist, dass bei 8600 flex es 4 fenster werden können"
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);
        applet.setLauschOhneGeraetForTest(true);
        applet.setLauschPlaetzeForTest(3);
        for (int k = 0; k < 3; ++k) {
            QVERIFY(applet.lauschFeldForTest(k)->isVisibleTo(&applet));
        }
        applet.setLauschfrequenzHz(0, 7074000.0);
        applet.setLauschfrequenzHz(1, 21074000.0);
        applet.setLauschfrequenzHz(2, 28074000.0);
        QCOMPARE(applet.lauschband(0), QStringLiteral("40m"));
        QCOMPARE(applet.lauschband(1), QStringLiteral("15m"));
        QCOMPARE(applet.lauschband(2), QStringLiteral("10m"));
        // Ein Band nie auf zwei Plaetzen.
        applet.setLauschfrequenzHz(2, 7074000.0);
        QCOMPARE(applet.lauschband(2), QStringLiteral("10m"));
        // Jedes Feld bietet die Baender der anderen Plaetze nicht an.
        QComboBox* feld0 = applet.lauschFeldForTest(0);
        for (int i = 0; i < feld0->count(); ++i) {
            QVERIFY(!feld0->itemText(i).startsWith(QStringLiteral("15m")));
            QVERIFY(!feld0->itemText(i).startsWith(QStringLiteral("10m")));
            QVERIFY(!feld0->itemText(i).startsWith(QStringLiteral("20m")));
        }
        // Vier Spalten im Teiler, jede mit ihrem Band, kein Uebersprechen.
        QCOMPARE(applet.listenTeilerForTest()->count(), 4);
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Ft8/NebenbandFilter"), QStringLiteral("Alles"));
        applet.lauschPeriodeForTest(0, {fund(QStringLiteral("CQ AA1AAA FN42"), 900.0f)});
        applet.lauschPeriodeForTest(1, {fund(QStringLiteral("CQ BB1BBB FN42"), 900.0f)});
        applet.lauschPeriodeForTest(2, {fund(QStringLiteral("CQ CC1CCC FN42"), 900.0f)});
        s.setValue(QStringLiteral("Ft8/NebenbandFilter"), QStringLiteral("NeuesLand"));
        const QStringList ruf = {QStringLiteral("AA1AAA"), QStringLiteral("BB1BBB"),
                                 QStringLiteral("CC1CCC")};
        for (int k = 0; k < 3; ++k) {
            const QString l = applet.lauschListeForTest(k)->toPlainText();
            for (int j = 0; j < 3; ++j) {
                QCOMPARE(l.contains(ruf.at(j)), j == k);
            }
            QVERIFY(applet.lauschSpalteForTest(k)->isVisibleTo(&applet));
        }
        // Abschalten blendet die Spalte aus.
        applet.setLauschfrequenzHz(1, 0.0);
        QVERIFY(!applet.lauschSpalteForTest(1)->isVisibleTo(&applet));
    }

    void ander8400NurEinFeld()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.setLauschPlaetzeForTest(1);
        QVERIFY(applet.lauschFeldForTest(0)->isVisibleTo(&applet));
        QVERIFY(!applet.lauschFeldForTest(1)->isVisibleTo(&applet));
        QVERIFY(!applet.lauschFeldForTest(2)->isVisibleTo(&applet));
    }

    void zweiPlaetzeAmDraht()
    {
        // Angelegt wird immer nur EINE zur Zeit -- waehrend des Anlegens
        // ist die neue Scheibe nur daran zu erkennen, dass sie keine der
        // bekannten ist. Jeder Platz hat seinen DAX-Kanal.
        Draht d;
        FlexLauschscheibe l(d.sende());
        l.setPlaetze(3);
        l.setHauptscheibe(QStringLiteral("0"));
        l.setPanadapter(QStringLiteral("0x40000000"));
        l.setHauptfrequenzHz(14074000.0);
        l.setFrequenzHz(0, 7074000.0);
        l.setFrequenzHz(1, 21074000.0);
        QCOMPARE(d.befehle.filter(QStringLiteral("slice create")).size(), 1);
        d.antworte(QStringLiteral("slice create"), 0, QStringLiteral("1"));
        QCOMPARE(d.befehle.filter(QStringLiteral("slice create")).size(), 2);
        d.antworte(QStringLiteral("stream create"), 0, QStringLiteral("4000009"));
        d.antworte(QStringLiteral("slice create"), 0, QStringLiteral("2"));
        d.antworte(QStringLiteral("stream create"), 0, QStringLiteral("400000a"));
        QVERIFY(d.befehle.contains(QStringLiteral("slice set 1 dax=1")));
        QVERIFY(d.befehle.contains(QStringLiteral("slice set 2 dax=2")));
        QVERIFY(d.befehle.contains(QStringLiteral("stream create type=dax_rx dax_channel=2")));
        QVERIFY(d.befehle.contains(QStringLiteral("slice tune 2 21.074000 autopan=0")));
        QCOMPARE(l.platzFuerStrom(0x4000009), 0);
        QCOMPARE(l.platzFuerStrom(0x400000a), 1);
        QCOMPARE(l.platzFuerStrom(0x4000008), -1);   // der Mix
        QVERIFY(l.istLauschscheibe(1) && l.istLauschscheibe(2));
        QVERIFY(!l.darfAnDenVfo(1) && !l.darfAnDenVfo(2));
        // Nur so viele Plaetze, wie das Geraet hat.
        l.setPlaetze(1);
        d.befehle.clear();
        l.setFrequenzHz(2, 28074000.0);
        QVERIFY(d.befehle.filter(QStringLiteral("slice create")).isEmpty());
        // Abbau raeumt beide.
        l.abbauen();
        QVERIFY(d.befehle.contains(QStringLiteral("slice remove 1")));
        QVERIFY(d.befehle.contains(QStringLiteral("slice remove 2")));
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

        // Und so saehe es an einer FLEX-8600 aus: drei mitgehoerte Baender.
        applet.setLauschPlaetzeForTest(3);
        applet.setLauschfrequenzHz(1, 7074000.0);
        applet.setLauschfrequenzHz(2, 14074000.0);
        applet.lauschPeriodeForTest(1, {fund(QStringLiteral("CQ 9K2MP LL49"), 1180.0f, -15.0f)});
        applet.resize(1400, 600);
        QTest::qWait(300);
        const QImage vier = applet.grab().toImage();
        const QString vierPfad = dir + QStringLiteral("/ft8-vier-baender-8600.png");
        QVERIFY2(vier.save(vierPfad), qPrintable(vierPfad));
        qInfo() << "Blatt:" << vierPfad << vier.size();
    }

    void blattOptionen()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()
            || qEnvironmentVariable("LONGPATH_ENTWURF") != QStringLiteral("1")) {
            QSKIP("braucht LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1");
        }
        RadioModel radio;
        Ft8Applet applet(&radio);
        QString gespeichert;
        QTimer::singleShot(400, [&]() {
            QWidget* blatt = QApplication::activeModalWidget();
            if (!blatt) { return; }
            const QString pfad = dir + QStringLiteral("/ft8-optionen.png");
            blatt->grab().toImage().save(pfad);
            gespeichert = pfad;
            if (auto* d = qobject_cast<QDialog*>(blatt)) { d->reject(); }
        });
        applet.zeigeOptionenForTest();
        QVERIFY2(!gespeichert.isEmpty(), "kein Optionsblatt gesehen");
        qInfo() << "Blatt:" << gespeichert;
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
