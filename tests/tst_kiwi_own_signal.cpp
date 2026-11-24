// SPDX-License-Identifier: GPL-3.0-or-later
//
// „Mein Signal am Kiwi“ (Entwurf C, 2026-11-24) -- die Teile, die ohne
// Hauptfenster pruefbar sind:
//   * Verzeichnis-Vorauswahl nach Entfernung und Band (ext_api bleibt
//     Bedingung),
//   * KiwiHearMeMeter: hoert / hoert nichts / unbekannt / wartet, mit
//     Gegenproben, die fehlende Daten NICHT als "hoert nichts" lesen,
//   * der Tonhalt beim Senden (kiwiKeepsAudioDuringTx),
//   * die Aufnahme (WAV + PNG),
//   * der Streifen als Bild, mit Marke auf der Sendefrequenz.
//
// Longpath-eigener Pruefstand, kein Port.
#include <QtTest>
#include <QImage>
#include <QTemporaryDir>

#include "core/KiwiOwnSignal.h"
#include "core/KiwiRecording.h"
#include "gui/KiwiOwnSignalStrip.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

namespace {

KiwiPublicReceiver rx(const QString& name, const QString& grid, int extApi = 4,
                      int users = 1, int usersMax = 4, bool offline = false,
                      const QString& bands = QStringLiteral("0-30000000"))
{
    KiwiPublicReceiver r;
    r.url = QStringLiteral("http://%1.example:8073").arg(name);
    r.name = name;
    r.grid = grid;
    r.extApi = extApi;
    r.users = users;
    r.usersMax = usersMax;
    r.offline = offline;
    r.bands = bands;
    return r;
}

// Eine Wasserfallzeile: Grundrauschen mit etwas Zittern, optional ein Buckel.
QVector<float> row(int n, double lowHz, double highHz, double noiseDbm,
                   double bumpHz = 0.0, double bumpDb = 0.0, int seed = 0)
{
    QVector<float> v(n);
    const double binHz = (highHz - lowHz) / n;
    for (int i = 0; i < n; ++i) {
        const double jitter = ((i * 37 + seed * 11) % 7 - 3) * 0.4;
        double d = noiseDbm + jitter;
        const double f = lowHz + (i + 0.5) * binHz;
        if (bumpDb > 0.0 && std::abs(f - bumpHz) <= 1200.0) { d += bumpDb; }
        v[i] = float(d);
    }
    return v;
}

constexpr double kLo = 14185000.0;
constexpr double kHi = 14225000.0;
constexpr double kMark = 14205000.0;

// Fuettert Zeilen von t0 bis t1 im Takt step; bump nur ab bumpFrom.
void feed(KiwiHearMeMeter& m, qint64 t0, qint64 t1, qint64 step,
          double bumpDb, qint64 bumpFrom, double lo = kLo, double hi = kHi)
{
    int seed = 0;
    for (qint64 t = t0; t <= t1; t += step) {
        m.feedRow(row(1024, lo, hi, -110.0, kMark,
                      t >= bumpFrom ? bumpDb : 0.0, seed++), lo, hi, t);
    }
}

} // namespace

class TstKiwiOwnSignal : public QObject
{
    Q_OBJECT

private slots:

    // ── Vorauswahl ───────────────────────────────────────────────────
    void vorauswahlSortiertNachEntfernungUndFiltert()
    {
        QVector<KiwiPublicReceiver> all;
        all << rx("weit", "FN31")                        // New York
            << rx("nah", "JN78")                         // bei Linz
            << rx("nurweb", "JN77", /*extApi=*/0)        // API aus
            << rx("unbekannt", "JN76", /*extApi=*/-1)    // Regel unbekannt
            << rx("aus", "JN79", 4, 1, 4, /*offline=*/true)
            << rx("voll", "JN88", 4, /*users=*/4, 4)
            << rx("nurLW", "JO60", 4, 1, 4, false,
                  QStringLiteral("0-1000000"))           // deckt 14 MHz nicht
            << rx("ohneOrt", QString());                 // kein Standort
        const QVector<KiwiCandidate> sel =
            kiwiPreselect(all, QStringLiteral("JN78dk"), 14.205e6);

        QStringList names;
        for (const KiwiCandidate& c : sel) { names << c.receiver.name; }
        QCOMPARE(names, (QStringList{"nah", "weit", "ohneOrt"}));
        QVERIFY(sel[0].km >= 0.0 && sel[0].km < 120.0);
        QVERIFY(sel[1].km > 5000.0);
        QCOMPARE(sel[2].km, -1.0);                       // ohne Ort ans Ende

        // Jeder Ausgeschlossene ist wirklich nicht dabei (Gegenprobe
        // gegen "alles durchreichen").
        for (const char* weg : {"nurweb", "unbekannt", "aus", "voll", "nurLW"}) {
            QVERIFY2(!names.contains(QLatin1String(weg)), weg);
        }

        QCOMPARE(kiwiPreselect(all, QStringLiteral("JN78dk"), 14.205e6, 1).size(), 1);
        // Ohne Frequenz kein Bandfilter: der Langwellen-Empfaenger bleibt.
        const QVector<KiwiCandidate> ohneBand =
            kiwiPreselect(all, QStringLiteral("JN78dk"), 0.0);
        QVERIFY(ohneBand.size() > sel.size());
    }

    void ohneLocatorBleibtDieReihenfolgeDesVerzeichnisses()
    {
        QVector<KiwiPublicReceiver> all;
        all << rx("b", "FN31") << rx("a", "JN78");
        const auto sel = kiwiPreselect(all, QString(), 14.2e6);
        QCOMPARE(sel.size(), 2);
        QCOMPARE(sel[0].receiver.name, QStringLiteral("b"));
        QCOMPARE(sel[0].km, -1.0);
    }

    void entfernungErsatzweiseAusGps()
    {
        KiwiPublicReceiver r = rx("gps", QString());
        r.gps = QStringLiteral("(48.30, 14.29)");   // Linz
        const double km = kiwiReceiverDistanceKm(r, QStringLiteral("JN77"));
        QVERIFY2(km >= 0.0 && km < 150.0, qPrintable(QString::number(km)));
        r.gps = QStringLiteral("kaputt");
        QCOMPARE(kiwiReceiverDistanceKm(r, QStringLiteral("JN77")), -1.0);
    }

    // ── Tonhalt ──────────────────────────────────────────────────────
    void tonBleibtImModusBeimSenden()
    {
        KiwiSdrAntennaProfile p;
        QVERIFY(!kiwiKeepsAudioDuringTx(p, false));   // alt: stumm
        QVERIFY(kiwiKeepsAudioDuringTx(p, true));     // Modus: bleibt
        p.keepAudioDuringTx = true;
        QVERIFY(kiwiKeepsAudioDuringTx(p, false));    // Profil verlangt es
    }

    // ── Messung ──────────────────────────────────────────────────────
    void vorDemSendenIstNichtsZuMessen()
    {
        KiwiHearMeMeter m;
        m.setMarkHz(kMark);
        feed(m, 0, 3000, 200, 25.0, 0);   // laute Marke, aber nie gesendet
        QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Waiting);
    }

    void erkenntDieAussendungUndMisstDenVerzug()
    {
        KiwiHearMeMeter m;
        m.setMarkHz(kMark);
        feed(m, 0, 900, 100, 0.0, 0);                 // Ruhe vor dem Tasten
        m.txChanged(true, 1000);
        feed(m, 1100, 2300, 100, 25.0, 2400);         // Verzug: noch Ruhe
        feed(m, 2400, 6000, 100, 25.0, 2400);         // dann das Signal
        m.txChanged(false, 6000);
        feed(m, 6100, 8000, 100, 0.0, 99999);
        const auto r = m.result();
        QCOMPARE(r.verdict, KiwiHearMeMeter::Verdict::Hears);
        QVERIFY(r.delayMeasured);
        QVERIFY2(r.delayMs >= 1300 && r.delayMs <= 1500,
                 qPrintable(QString::number(r.delayMs)));
        QVERIFY(r.snrDb >= 20.0);
        // -110 + 25 = -85 dBm -> S7
        QCOMPARE(r.sUnits, 7);
        QCOMPARE(kiwiSUnitsText(r.sUnits, r.overS9Db), QStringLiteral("S7"));
    }

    void hoertNichts_nurMitBeweislast()
    {
        KiwiHearMeMeter m;
        m.setMarkHz(kMark);
        feed(m, 0, 900, 100, 0.0, 0);
        m.txChanged(true, 1000);
        // Mitten in der Sendung, noch im Einschwingfenster: unbekannt, nicht
        // gleich "hoert nichts".
        feed(m, 1100, 3000, 100, 0.0, 99999);
        QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Unknown);
        feed(m, 3100, 6000, 100, 0.0, 99999);
        m.txChanged(false, 6000);
        feed(m, 6100, 8000, 100, 0.0, 99999);
        const auto r = m.result();
        QCOMPARE(r.verdict, KiwiHearMeMeter::Verdict::Nothing);
        QVERIFY(!r.delayMeasured);   // nie ein Anstieg gesehen -> angenommen
        QCOMPARE(r.delayMs, int(KiwiHearMeMeter::kAssumedDelayMs));
    }

    void keinBildIstUnbekanntNichtHoertNichts()
    {
        // Gegenprobe zur Hauptsache dieser Messung: Abwesenheit von Daten
        // ist kein Messergebnis.
        {
            KiwiHearMeMeter m;
            m.setMarkHz(kMark);
            m.txChanged(true, 1000);
            m.txChanged(false, 6000);
            QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Unknown);
        }
        {   // zu wenige Zeilen im Fenster
            KiwiHearMeMeter m;
            m.setMarkHz(kMark);
            m.txChanged(true, 1000);
            m.feedRow(row(1024, kLo, kHi, -110.0), kLo, kHi, 2500);
            m.feedRow(row(1024, kLo, kHi, -110.0), kLo, kHi, 3500);
            m.txChanged(false, 6000);
            QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Unknown);
        }
        {   // viele Zeilen, aber die Marke liegt nicht im Bild des Kiwi
            KiwiHearMeMeter m;
            m.setMarkHz(kMark);
            m.txChanged(true, 1000);
            feed(m, 1100, 6000, 100, 0.0, 99999, 7000000.0, 7040000.0);
            m.txChanged(false, 6000);
            QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Unknown);
        }
        {   // leere/zu kurze Zeilen
            KiwiHearMeMeter m;
            m.setMarkHz(kMark);
            m.txChanged(true, 1000);
            for (qint64 t = 2500; t < 6000; t += 100) {
                m.feedRow(QVector<float>(), kLo, kHi, t);
            }
            m.txChanged(false, 6000);
            QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Unknown);
        }
    }

    void einDauertraegerVorDemTastenGibtKeinenVerzug()
    {
        // Jemand anderes steht schon auf der Frequenz: ein Ausschlag nach dem
        // Tasten sagt nichts ueber den Verzug.
        KiwiHearMeMeter m;
        m.setMarkHz(kMark);
        feed(m, 0, 900, 100, 25.0, 0);
        m.txChanged(true, 1000);
        feed(m, 1100, 6000, 100, 25.0, 0);
        m.txChanged(false, 6000);
        feed(m, 6100, 8000, 100, 25.0, 0);
        QVERIFY(!m.result().delayMeasured);
    }

    void eineNeueSendungBeginntEineNeueMessung()
    {
        KiwiHearMeMeter m;
        m.setMarkHz(kMark);
        m.txChanged(true, 1000);
        feed(m, 1100, 6000, 100, 25.0, 1100);
        m.txChanged(false, 6000);
        feed(m, 6100, 8000, 100, 25.0, 1100);
        QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Hears);
        m.txChanged(true, 20000);                 // zweite Sendung, noch ohne Bild
        QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Unknown);
        m.reset();
        QCOMPARE(m.result().verdict, KiwiHearMeMeter::Verdict::Waiting);
    }

    void sWerteStimmen()
    {
        int s = 0, over = 0;
        KiwiHearMeMeter::sUnitsFromDbm(-73.0, s, over);
        QCOMPARE(s, 9); QCOMPARE(over, 0);
        KiwiHearMeMeter::sUnitsFromDbm(-61.0, s, over);
        QCOMPARE(s, 9); QCOMPARE(over, 12);
        QCOMPARE(kiwiSUnitsText(s, over), QStringLiteral("S9+12"));
        KiwiHearMeMeter::sUnitsFromDbm(-109.0, s, over);
        QCOMPARE(s, 3);
        KiwiHearMeMeter::sUnitsFromDbm(-140.0, s, over);
        QCOMPARE(s, 0);
    }

    // ── Aufnahme ─────────────────────────────────────────────────────
    void aufnahmeSchreibtWavUndBild()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KiwiRecording rec;
        QString err;
        const QDateTime when(QDate(2026, 11, 24), QTime(15, 30, 12));
        QVERIFY2(rec.start(dir.filePath("a/b"), QStringLiteral("Linz / Nord"), when, &err),
                 qPrintable(err));
        QVERIFY(rec.isRecording());
        QVERIFY(rec.wavPath().endsWith(
            QStringLiteral("Mein-Signal_Linz-Nord_2026-11-24_153012.wav")));
        // Zweiter Start waehrend der Aufnahme: abgelehnt.
        QVERIFY(!rec.start(dir.filePath("a/b"), QStringLiteral("x"), when, &err));

        QByteArray pcm(24000 * 2 * int(sizeof(float)), 0);
        auto* f = reinterpret_cast<float*>(pcm.data());
        for (int i = 0; i < 24000 * 2; ++i) { f[i] = 0.25f; }
        rec.feedAudio(pcm);
        rec.feedAudio(pcm);
        QCOMPARE(rec.elapsedMs(), qint64(2000));

        QImage img(100, 60, QImage::Format_RGB32);
        img.fill(Qt::gray);
        const QString wav = rec.stop(img);
        QVERIFY(!rec.isRecording());
        QVERIFY(QFileInfo::exists(wav));
        QVERIFY(QFileInfo::exists(rec.imagePath()));
        QVERIFY(rec.imagePath().endsWith(".png"));
        // 44 Byte Kopf + 2 s * 24000 * 2 Kanaele * 4 Byte
        QVERIFY2(QFileInfo(wav).size() >= 44 + 2 * 24000 * 8,
                 qPrintable(QString::number(QFileInfo(wav).size())));
        QCOMPARE(rec.stop(img), QString());   // zweites stop: nichts mehr
    }

    void aufnahmeOhneOrdnerFehlerStattAbsturz()
    {
        KiwiRecording rec;
        QString err;
        QVERIFY(!rec.start(QStringLiteral("/dev/null/geht-nicht"),
                           QStringLiteral("x"), QDateTime::currentDateTime(), &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(!rec.isRecording());
    }

    // ── Streifen als Bild ────────────────────────────────────────────
    void streifenZeigtMarkeKopfUndStatus()
    {
        KiwiOwnSignalStrip strip;
        strip.resize(960, 190);
        strip.setKiwiName(QStringLiteral("Linz"));
        strip.setDistanceKm(38.0);
        strip.setDelay(1400, true);
        strip.setView(kMark, 40000.0);
        strip.setMarkHz(kMark);

        // vor dem ersten Bild
        QCOMPARE(strip.statusText(), QStringLiteral("wartet auf Sendung"));
        QVERIFY(strip.waterfallImage().isNull());

        for (int i = 0; i < 80; ++i) {
            strip.pushRow(row(1024, kLo, kHi, -110.0, kMark, 25.0, i), kLo, kHi);
        }
        KiwiHearMeMeter::Result hears;
        hears.verdict = KiwiHearMeMeter::Verdict::Hears;
        hears.sUnits = 7;
        strip.setVerdict(hears);
        strip.setRecording(true, 42000, QStringLiteral("Mein-Signal_2026-11-24.wav"));

        QCOMPARE(strip.headerText(), QStringLiteral("KIWI LINZ · 38 km · 1,4 s")
                     .replace(',', QLocale::system().decimalPoint()));
        QCOMPARE(strip.statusText(), QStringLiteral("hört mich · S7"));
        QCOMPARE(strip.statusColor(), QColor(Style::kGreenText));
        QCOMPARE(strip.footerText(),
                 QStringLiteral("● Aufnahme 00:42 → Mein-Signal_2026-11-24.wav"));

        // Marke genau in der Mitte der Achse.
        const int mx = strip.markX();
        QVERIFY(mx >= 0);
        QVERIFY(std::abs(mx - strip.width() / 2) <= 2);

        const QImage shot = strip.grab().toImage();
        const QString out = qEnvironmentVariable("LONGPATH_STRIP_PNG");
        if (!out.isEmpty()) { QVERIFY(shot.save(out + "-hoert.png")); }

        // In der Bildmitte, mitten im Wasserfall, steht die Markenfarbe...
        const QColor rot(Style::kRedBorder);
        const qreal dpr = shot.devicePixelRatio();
        const int y = int(100 * dpr);
        const QColor am(shot.pixelColor(int(mx * dpr), y));
        QVERIFY2(std::abs(am.red() - rot.red()) < 6 && std::abs(am.green() - rot.green()) < 6,
                 qPrintable(am.name()));
        // ...und links davon Grau (R=G=B): Farbe nur auf der Marke.
        const QColor daneben(shot.pixelColor(int(mx * dpr) - int(60 * dpr), y));
        QCOMPARE(daneben.red(), daneben.green());
        QCOMPARE(daneben.green(), daneben.blue());

        // Marke ausserhalb der Achse: nicht gezeichnet.
        strip.setMarkHz(kMark + 500000.0);
        QCOMPARE(strip.markX(), -1);
    }

    void streifenSagtHoertNichtsUndUnbekanntInAnderenFarben()
    {
        KiwiOwnSignalStrip strip;
        strip.resize(960, 190);
        strip.setKiwiName(QStringLiteral("Linz"));
        strip.setView(kMark, 40000.0);
        strip.setMarkHz(kMark);
        for (int i = 0; i < 80; ++i) {
            strip.pushRow(row(1024, kLo, kHi, -110.0, 0.0, 0.0, i), kLo, kHi);
        }
        KiwiHearMeMeter::Result r;
        r.verdict = KiwiHearMeMeter::Verdict::Nothing;
        strip.setVerdict(r);
        QCOMPARE(strip.statusText(), QStringLiteral("hört nichts"));
        QCOMPARE(strip.statusColor(), QColor(Style::kAmberWarn));
        const QString out = qEnvironmentVariable("LONGPATH_STRIP_PNG");
        if (!out.isEmpty()) { QVERIFY(strip.grab().toImage().save(out + "-nichts.png")); }

        r.verdict = KiwiHearMeMeter::Verdict::Unknown;
        strip.setVerdict(r);
        QCOMPARE(strip.statusText(), QStringLiteral("unbekannt"));
        QVERIFY(strip.statusColor() != QColor(Style::kAmberWarn));
        QVERIFY(strip.statusColor() != QColor(Style::kGreenText));
        strip.setRecording(false, 0, QString());
        QCOMPARE(strip.footerText(), QStringLiteral("Aufnahme aus"));
    }

    void streifenBildAufAndererAchseWirdUmgerechnet()
    {
        // Der Kiwi trifft die verlangte Breite nicht genau. Ein Bild, das nur
        // die Haelfte der Achse abdeckt, laesst die andere Haelfte leer --
        // und der Buckel liegt dort, wo seine Frequenz ist.
        KiwiOwnSignalStrip strip;
        strip.resize(800, 190);
        strip.setView(14205000.0, 40000.0);   // 14185..14225
        strip.setMarkHz(14205000.0);
        // Bild nur 14205..14225, Buckel bei 14215
        for (int i = 0; i < 60; ++i) {
            strip.pushRow(row(1024, 14205000.0, 14225000.0, -110.0, 14215000.0, 30.0, i),
                          14205000.0, 14225000.0);
        }
        const QImage img = strip.waterfallImage(800);
        QVERIFY(!img.isNull());
        // x fuer 14215: Mitte der rechten Haelfte = 3/4 der Breite.
        const int xBump = 600;
        const int xBlankLeft = 100;            // 14190: ausserhalb des Bildes
        const QColor bump(img.pixelColor(xBump, 5));
        const QColor leer(img.pixelColor(xBlankLeft, 5));
        QVERIFY2(bump.red() > 150, qPrintable(bump.name()));
        QVERIFY2(leer.red() < 10, qPrintable(leer.name()));
    }
};

QTEST_MAIN(TstKiwiOwnSignal)
#include "tst_kiwi_own_signal.moc"
