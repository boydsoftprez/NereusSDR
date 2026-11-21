// no-port-check: Longpath-eigener Prüfstand, keine portierte Logik.
// =================================================================
// tests/tst_mikrofon_waechter.cpp  (Longpath)
// =================================================================
//
// Das PC-Mikrofon fällt aus — merkt Longpath es, und NUR dann?
//
// Nachgespielt wird der 2026-11-20: USB-Mikrofon ausgesteckt, der
// Aufnahmestrom bleibt offen, CoreAudio ruft nicht mehr zurück. Dazu die
// Gegenfälle, in denen KEINE Warnung erscheinen darf: normaler Jitter
// (gemessen ≤ 2 ms Lücke), Mikrofon an der Maschine, Ton aus.
//
// Die Uhr ist künstlich: jeder Takt sind 250 ms, wie im Programm.
// =================================================================
// Modification history (Longpath):
//   2026-11-20 — Angelegt. KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <atomic>
#include <memory>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/audio/MikrofonWaechter.h"

using Longpath::AudioEngine;
using Longpath::AudioFormat;
using Longpath::IAudioBus;
using Longpath::Audio::MikrofonWaechter;
using Z = MikrofonWaechter::Zustand;

namespace {

constexpr qint64 kTaktMs = 250;
constexpr qint64 kMs = 1000000;

/// Ein gesundes Gerät: der letzte Rückruf ist `lueckeMs` her.
MikrofonWaechter::Probe pc(qint64 lueckeMs, bool offen = true)
{
    MikrofonWaechter::Probe p;
    p.pcMikrofonGewaehlt = true;
    p.tonLaeuft = true;
    p.eingangOffen = offen;
    p.nsSeitRueckruf = lueckeMs < 0 ? -1 : lueckeMs * kMs;
    return p;
}

/// Ein Aufnahmebus, dessen "letzter Rueckruf" der Pruefstand stellt —
/// so laesst sich ein ausgestecktes Mikrofon ohne Tongeraet nachspielen.
class GestellterBus : public IAudioBus {
public:
    explicit GestellterBus(std::shared_ptr<std::atomic<qint64>> ns) : m_ns(std::move(ns)) {}
    bool open(const AudioFormat&) override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    qint64 push(const char*, qint64 bytes) override { return bytes; }
    qint64 pull(char*, qint64) override { return 0; }
    float rxLevel() const override { return 0.0F; }
    float txLevel() const override { return 0.0F; }
    QString backendName() const override { return QStringLiteral("GestellterBus"); }
    AudioFormat negotiatedFormat() const override { return {}; }
    qint64 nsSinceLastCallback() const override { return m_ns->load(); }

private:
    std::shared_ptr<std::atomic<qint64>> m_ns;
};

}  // namespace

class TstMikrofonWaechter : public QObject
{
    Q_OBJECT
private slots:
    // Eine Stunde Normalbetrieb mit dem gemessenen Jitter: nie eine Warnung.
    void normalerJitterWarntNie()
    {
        MikrofonWaechter w;
        qint64 t = 0;
        for (int i = 0; i < 4 * 3600; ++i, t += kTaktMs) {
            // 0..53 ms: irgendwo innerhalb eines Puffers, dazu ab und zu
            // ein spaeter Rueckruf wie nach einem Schlummer des Threads.
            const qint64 luecke = (i % 97 == 0) ? 180 : (i * 7) % 54;
            QCOMPARE(w.pruefe(pc(luecke), t), Z::Liefert);
        }
    }

    // Der 20.11.: ab Sekunde 10 kein Rückruf mehr. Spätestens nach
    // 1,25 s muss die Warnung stehen — und bleiben.
    void ausgesteckt()
    {
        MikrofonWaechter w;
        qint64 t = 0;
        for (; t < 10000; t += kTaktMs) {
            QCOMPARE(w.pruefe(pc(2), t), Z::Liefert);
        }
        const qint64 weg = t;
        qint64 erkannt = -1;
        for (; t < weg + 60000; t += kTaktMs) {
            const Z z = w.pruefe(pc(t - weg), t);
            if (z == Z::Ausgefallen && erkannt < 0) { erkannt = t; }
            if (erkannt >= 0) { QCOMPARE(z, Z::Ausgefallen); }
        }
        QVERIFY(erkannt >= 0);
        QVERIFY2(erkannt - weg <= 1250, qPrintable(QString::number(erkannt - weg)));
        QCOMPARE(w.grund(), MikrofonWaechter::Grund::KeineRueckrufe);
        QCOMPARE(w.ausgefallenSeitMs(), erkannt);
    }

    // Ein Gerät, das alle 4,5 s noch einen einzelnen Block schickt (die
    // Zahl aus dem Protokoll vom 20.11.), darf die Warnung nicht
    // ausschalten — auch nicht kurz.
    void sporadischeRueckrufeFlackernNicht()
    {
        MikrofonWaechter w;
        qint64 t = 0;
        qint64 letzter = 0;
        for (int i = 0; i < 8; ++i, t += kTaktMs) { w.pruefe(pc(2), t); }
        letzter = t;
        bool warAus = false;
        for (; t < 120000; t += kTaktMs) {
            if (t - letzter >= 4500) { letzter = t; }
            const Z z = w.pruefe(pc(t - letzter), t);
            if (z == Z::Ausgefallen) { warAus = true; }
            if (warAus) { QCOMPARE(z, Z::Ausgefallen); }
        }
        QVERIFY(warAus);
    }

    // Gerät wieder da: Warnung geht nach drei Sekunden regelmäßiger
    // Rückrufe, nicht beim ersten.
    void erholtSichNachDreiSekunden()
    {
        MikrofonWaechter w;
        qint64 t = 0;
        for (; t < 5000; t += kTaktMs) { w.pruefe(pc(t), t); }
        QCOMPARE(w.zustand(), Z::Ausgefallen);
        const qint64 zurueck = t;
        for (; t < zurueck + MikrofonWaechter::kErholtNachMs; t += kTaktMs) {
            QCOMPARE(w.pruefe(pc(3), t), Z::Ausgefallen);
        }
        QCOMPARE(w.pruefe(pc(3), t), Z::Liefert);
        QCOMPARE(w.ausgefallenSeitMs(), qint64(-1));
    }

    // Eine Lücke mitten in der Erholung setzt die drei Sekunden zurück.
    void lueckeInDerErholungZaehltNeu()
    {
        MikrofonWaechter w;
        qint64 t = 0;
        for (; t < 3000; t += kTaktMs) { w.pruefe(pc(t), t); }
        for (int i = 0; i < 8; ++i, t += kTaktMs) { w.pruefe(pc(3), t); }  // 2 s gut
        w.pruefe(pc(600), t); t += kTaktMs;                                 // Hänger
        for (int i = 0; i < 8; ++i, t += kTaktMs) {
            QCOMPARE(w.pruefe(pc(3), t), Z::Ausgefallen);                   // erst 2 s
        }
        for (int i = 0; i < 5; ++i, t += kTaktMs) { w.pruefe(pc(3), t); }
        QCOMPARE(w.zustand(), Z::Liefert);
    }

    // PC-Mikrofon gewählt, aber gar kein Eingang offen (Gerät beim Start
    // weg, Berechtigung fehlt): auch das sendet Stille.
    void keinEingangOffen()
    {
        MikrofonWaechter w;
        QCOMPARE(w.pruefe(pc(-1, /*offen=*/false), 0), Z::Ausgefallen);
        QCOMPARE(w.grund(), MikrofonWaechter::Grund::KeinGeraet);
    }

    // Offen, aber noch nie ein Rückruf: eine Sekunde Anlauf, dann Warnung.
    void offenOhneJedenRueckruf()
    {
        MikrofonWaechter w;
        qint64 t = 0;
        for (; t <= MikrofonWaechter::kAusfallMs; t += kTaktMs) {
            QCOMPARE(w.pruefe(pc(-1), t), Z::Liefert);
        }
        QCOMPARE(w.pruefe(pc(-1), t), Z::Ausgefallen);
        QCOMPARE(w.grund(), MikrofonWaechter::Grund::KeineRueckrufe);
    }

    // Mikrofon an der Maschine oder DAX, oder der Ton läuft nicht:
    // keine Warnung, auch wenn der PC-Eingang tot ist. Und ein Wechsel
    // dorthin nimmt eine stehende Warnung zurück.
    void nurBeiPcMikrofon()
    {
        MikrofonWaechter w;
        MikrofonWaechter::Probe p = pc(9000);
        p.pcMikrofonGewaehlt = false;
        QCOMPARE(w.pruefe(p, 0), Z::Unbeteiligt);
        p = pc(9000);
        p.tonLaeuft = false;
        QCOMPARE(w.pruefe(p, 250), Z::Unbeteiligt);

        QCOMPARE(w.pruefe(pc(9000), 500), Z::Ausgefallen);
        p = pc(9000);
        p.pcMikrofonGewaehlt = false;
        QCOMPARE(w.pruefe(p, 750), Z::Unbeteiligt);
        QCOMPARE(w.ausgefallenSeitMs(), qint64(-1));
    }

    // ── Die Tonmaschine selbst: ein simulierter Ausfall der Rueckrufe ──
    //
    // Gestellter Bus, PC-Mikrofon gewaehlt, Ton "laeuft". Erst 5 s gesund
    // (kein Signal), dann stehen die Rueckrufe: genau EIN Signal true,
    // und nach drei Sekunden regelmaessiger Rueckrufe genau EIN false.
    void tonmaschineMeldetAusfallUndErholung()
    {
        auto ns = std::make_shared<std::atomic<qint64>>(2 * kMs);
        AudioEngine engine;
        engine.setTxInputBusForTest(std::make_unique<GestellterBus>(ns));
        engine.onMicSourceChanged(true);
        engine.setTonLaeuftFuerTest(true);
        QSignalSpy spy(&engine, &AudioEngine::pcMikrofonAusfallGeaendert);

        qint64 t = 0;
        for (; t < 5000; t += kTaktMs) { engine.pruefePcMikrofon(t); }
        QCOMPARE(spy.count(), 0);
        QVERIFY(!engine.pcMikrofonAusgefallen());

        const qint64 weg = t;
        for (; t < weg + 10000; t += kTaktMs) {
            ns->store((t - weg) * kMs);
            engine.pruefePcMikrofon(t);
        }
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QVERIFY(engine.pcMikrofonAusgefallen());

        ns->store(3 * kMs);
        for (int i = 0; i < 20; ++i, t += kTaktMs) { engine.pruefePcMikrofon(t); }
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
        QVERIFY(!engine.pcMikrofonAusgefallen());
    }

    // Derselbe tote Bus, aber Quelle "Mikrofon an der Maschine": kein
    // Signal. Und ein Wechsel weg vom PC-Mikrofon nimmt die Warnung zurueck.
    void tonmaschineSchweigtBeiAndererQuelle()
    {
        auto ns = std::make_shared<std::atomic<qint64>>(9000 * kMs);
        AudioEngine engine;
        engine.setTxInputBusForTest(std::make_unique<GestellterBus>(ns));
        engine.onMicSourceChanged(false);
        engine.setTonLaeuftFuerTest(true);
        QSignalSpy spy(&engine, &AudioEngine::pcMikrofonAusfallGeaendert);
        for (qint64 t = 0; t < 5000; t += kTaktMs) { engine.pruefePcMikrofon(t); }
        QCOMPARE(spy.count(), 0);

        engine.onMicSourceChanged(true);
        engine.pruefePcMikrofon(5000);
        QCOMPARE(spy.count(), 1);
        engine.onMicSourceChanged(false);
        engine.pruefePcMikrofon(5250);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }
};

QTEST_MAIN(TstMikrofonWaechter)
#include "tst_mikrofon_waechter.moc"
