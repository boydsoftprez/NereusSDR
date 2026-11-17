// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// QsoUploadQueue — jedes geloggte QSO an jeden eingerichteten Dienst,
// und eine Warteliste fuer das, was nicht durchging (2026-11-17,
// Betreiber: "ich moechte, dass meine logs automatisch auf qrz geladen
// werden, aber auch parallel auf wavelog").
//
// Hier die Warteschlange allein, gegen Attrappen statt gegen Dienste:
// Reihenfolge, Unabhaengigkeit der Dienste, Dauerhaftigkeit, Nachholen,
// ein Dienst, der schweigt. Der Weg vom Loggen durch die echten
// Uploader steht in tst_auto_upload_alle_dienste. Kein Netz.

#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>

#include <functional>

#include "core/AdifLog.h"
#include "core/QsoUploadQueue.h"
#include "core/QsoUploader.h"

using namespace Longpath;

namespace {

struct Answer {
    bool    ok{true};
    bool    duplicate{false};
    QString message{QStringLiteral("accepted")};
};

// Ein Dienst, der tut, was der Pruefstand ihm sagt.
class FakeService : public QsoUploader {
    Q_OBJECT
public:
    FakeService(const QString& id, const QString& shortName, QObject* parent = nullptr)
        : QsoUploader(parent), m_id(id), m_short(shortName) {}

    QString serviceName() const override { return m_short + QStringLiteral(" (long name)"); }
    QString shortName() const override { return m_short; }
    QString serviceId() const override { return m_id; }
    bool isConfigured() const override { return configured; }

    void upload(const LogEntry& entry) override
    {
        received << entry.call;
        ++inFlight;
        maxInFlight = qMax(maxInFlight, inFlight);
        if (hold) { held << entry.call; return; }
        const Answer a = answerFor ? answerFor(entry) : Answer{};
        if (synchronous) { finish(entry.call, a); return; }
        QMetaObject::invokeMethod(this, [this, call = entry.call, a]() {
            finish(call, a);
        }, Qt::QueuedConnection);
    }

    void finish(const QString& call, const Answer& a)
    {
        --inFlight;
        emit uploadFinished(call, a.ok, a.duplicate, a.message);
    }
    // Die zurueckgehaltene Antwort jetzt geben.
    void release(const Answer& a = Answer{})
    {
        QVERIFY(!held.isEmpty());
        finish(held.takeFirst(), a);
    }

    bool configured{true};
    bool synchronous{false};
    bool hold{false};
    std::function<Answer(const LogEntry&)> answerFor;
    QStringList received;
    QStringList held;
    int inFlight{0};
    int maxInFlight{0};

private:
    QString m_id;
    QString m_short;
};

Answer refuse(const QString& why)
{
    Answer a;
    a.ok = false;
    a.message = why;
    return a;
}

LogEntry ft8(const QString& call, int minute)
{
    LogEntry e;
    e.call = call;
    e.timeOn = QDateTime(QDate(2026, 11, 17), QTime(14, minute, 15), QTimeZone::UTC);
    e.band = QStringLiteral("20m");
    e.mode = QStringLiteral("FT8");
    e.freqMHz = 14.074;
    e.rstSent = QStringLiteral("-07");
    e.rstRcvd = QStringLiteral("-12");
    e.gridSquare = QStringLiteral("JO62");
    e.myGridSquare = QStringLiteral("JN68");
    return e;
}

struct Ergebnis {
    QList<QPair<QString, QVector<QsoUploadQueue::Outcome>>> fertig;
    QStringList hochgeladen;        // "dienst:CALL"
    QStringList nachgeholt;         // "Dienst sent/waiting"
};

void mitschreiben(QsoUploadQueue& q, Ergebnis& e)
{
    QObject::connect(&q, &QsoUploadQueue::qsoFinished, &q,
        [&e](const LogEntry& entry, const QVector<QsoUploadQueue::Outcome>& o) {
            e.fertig.append(qMakePair(entry.call, o));
        });
    QObject::connect(&q, &QsoUploadQueue::uploaded, &q,
        [&e](const QString& id, const LogEntry& entry, bool) {
            e.hochgeladen << id + QLatin1Char(':') + entry.call;
        });
    QObject::connect(&q, &QsoUploadQueue::backlogSent, &q,
        [&e](const QString& name, int sent, int waiting, const QString&) {
            e.nachgeholt << QStringLiteral("%1 %2/%3").arg(name).arg(sent).arg(waiting);
        });
}

QJsonObject datei(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    return QJsonDocument::fromJson(f.readAll()).object();
}

} // namespace

class TstQsoUploadQueue : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    QString m_store;

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_store = m_dir.filePath(QStringLiteral("qso-upload-queue.json"));
        QFile::remove(m_store);
    }

    // Der Kern der Ansage: nicht QRZ allein, sondern alle, die
    // eingerichtet sind — und nur die.
    void einQsoGehtAnJedenEingerichtetenDienst()
    {
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        FakeService lokal(QStringLiteral("adifnet"), QStringLiteral("local logger"));
        lokal.configured = false;

        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz, &wavelog, &lokal});

        QCOMPARE(q.submit(ft8(QStringLiteral("DL1ABC"), 1)), 2);
        // Nichts aus submit() heraus: der Aufrufer schreibt erst noch
        // seine eigene Statuszeile.
        QVERIFY(e.fertig.isEmpty());
        QVERIFY(qrz.received.isEmpty());

        QTRY_COMPARE(e.fertig.size(), 1);
        QCOMPARE(qrz.received, QStringList{QStringLiteral("DL1ABC")});
        QCOMPARE(wavelog.received, QStringList{QStringLiteral("DL1ABC")});
        QVERIFY(lokal.received.isEmpty());

        const auto outcomes = e.fertig.first().second;
        QCOMPARE(outcomes.size(), 2);
        QVERIFY(outcomes.at(0).ok && outcomes.at(1).ok);
        QCOMPARE(QsoUploadQueue::statusLine(QStringLiteral("DL1ABC"), outcomes),
                 QStringLiteral("DL1ABC — uploaded to QRZ, Wavelog"));

        QCOMPARE(q.waitingCount(), 0);
        QVERIFY2(!QFile::exists(m_store), "nichts wartet, also keine Datei");
    }

    // Die Statuszeile nennt die Dienste in fester Reihenfolge, nicht in
    // der, in der sie antworten. Im ersten CI-Lauf dieser Klasse stand
    // "uploaded to Wavelog, QRZ", weil Wavelog schneller war.
    void dieReihenfolgeHaengtNichtAnDerAntwortzeit()
    {
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        qrz.hold = true;                       // QRZ laesst sich Zeit
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz, &wavelog});
        q.submit(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_COMPARE(wavelog.received.size(), 1);
        QTRY_COMPARE(q.waitingCount(QStringLiteral("cloudlog")), 0);   // Wavelog ist fertig
        QTRY_COMPARE(qrz.held.size(), 1);
        QVERIFY(e.fertig.isEmpty());
        qrz.release();
        QTRY_COMPARE(e.fertig.size(), 1);
        QCOMPARE(QsoUploadQueue::statusLine(QStringLiteral("DL1ABC"), e.fertig.first().second),
                 QStringLiteral("DL1ABC — uploaded to QRZ, Wavelog"));
    }

    void nichtsEingerichtetHeisstNichtsGemeldet()
    {
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        qrz.configured = false;
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz});
        QCOMPARE(q.submit(ft8(QStringLiteral("DL1ABC"), 1)), 0);
        QTest::qWait(30);
        QVERIFY(e.fertig.isEmpty());
        QCOMPARE(q.waitingCount(), 0);
    }

    // Faellt einer aus, geht der andere trotzdem durch — und was nicht
    // durchging, steht danach auf der Platte.
    void einerScheitertDerAndereGehtDurch()
    {
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        wavelog.answerFor = [](const LogEntry&) { return refuse(QStringLiteral("API key rejected")); };

        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz, &wavelog});
        QCOMPARE(q.submit(ft8(QStringLiteral("DL1ABC"), 1)), 2);
        QTRY_COMPARE(e.fertig.size(), 1);

        QCOMPARE(e.hochgeladen, QStringList{QStringLiteral("qrz:DL1ABC")});
        QCOMPARE(q.waitingCount(QStringLiteral("qrz")), 0);
        QCOMPARE(q.waitingCount(QStringLiteral("cloudlog")), 1);

        const QString zeile =
            QsoUploadQueue::statusLine(QStringLiteral("DL1ABC"), e.fertig.first().second);
        QCOMPARE(zeile, QStringLiteral(
            "DL1ABC — uploaded to QRZ; logged locally, "
            "Wavelog upload failed: API key rejected — will retry"));

        // Auf der Platte: unter dem Dienst, als ADIF, ohne Schluessel.
        const QJsonObject services = datei(m_store).value(QStringLiteral("services")).toObject();
        QCOMPARE(services.keys(), QStringList{QStringLiteral("cloudlog")});
        const QJsonArray records = services.value(QStringLiteral("cloudlog")).toArray();
        QCOMPARE(records.size(), 1);
        const QVector<LogEntry> zurueck = AdifLog::parse(records.at(0).toString());
        QCOMPARE(zurueck.size(), 1);
        QVERIFY(AdifLog::isSameQso(zurueck.first(), ft8(QStringLiteral("DL1ABC"), 1)));
        QCOMPARE(QsoUploadQueue::storedCount(m_store), 1);
    }

    // Die Warteliste ueberlebt das Beenden, und der naechste Start holt
    // nach.
    void dieWartelisteUeberlebtDenNeustartUndWirdNachgeholt()
    {
        {
            FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
            FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
            wavelog.answerFor = [](const LogEntry&) { return refuse(QStringLiteral("HTTP 502")); };
            QsoUploadQueue q(m_store);
            Ergebnis e;
            mitschreiben(q, e);
            q.setUploaders({&qrz, &wavelog});
            q.submit(ft8(QStringLiteral("DL1ABC"), 1));
            q.submit(ft8(QStringLiteral("SP2XYZ"), 2));
            QTRY_COMPARE(e.fertig.size(), 2);
            QCOMPARE(q.waitingCount(QStringLiteral("cloudlog")), 2);
        }

        // Neuer Lauf, der Dienst ist wieder da.
        QCOMPARE(QsoUploadQueue::storedCount(m_store), 2);
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz, &wavelog});
        QCOMPARE(q.waitingCount(QStringLiteral("cloudlog")), 2);
        QVERIFY2(wavelog.received.isEmpty(), "ohne Anstoss wird nichts gesendet");

        q.retryPending();
        QTRY_COMPARE(e.nachgeholt.size(), 1);
        QCOMPARE(e.nachgeholt.first(), QStringLiteral("Wavelog 2/0"));
        QCOMPARE(wavelog.received.size(), 2);
        QVERIFY(wavelog.received.contains(QStringLiteral("DL1ABC")));
        QVERIFY(wavelog.received.contains(QStringLiteral("SP2XYZ")));
        QVERIFY2(qrz.received.isEmpty(), "QRZ hatte beide schon — kein Doppel-Upload");
        QVERIFY2(e.fertig.isEmpty(), "Nachholen ist kein frisch geloggtes QSO");
        QCOMPARE(q.waitingCount(), 0);
        QVERIFY(!QFile::exists(m_store));
    }

    // Ohne Neustart: das naechste QSO, das durchgeht, zieht das
    // liegengebliebene nach.
    void einFrischerErfolgHoltDasLiegengebliebeneNach()
    {
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        bool netzDa = false;
        wavelog.answerFor = [&netzDa](const LogEntry&) {
            return netzDa ? Answer{} : refuse(QStringLiteral("network unreachable"));
        };
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&wavelog});

        q.submit(ft8(QStringLiteral("DL1ABC"), 1));
        q.submit(ft8(QStringLiteral("SP2XYZ"), 2));
        QTRY_COMPARE(e.fertig.size(), 2);
        QCOMPARE(q.waitingCount(), 2);

        netzDa = true;
        q.submit(ft8(QStringLiteral("OK1DEF"), 3));
        QTRY_COMPARE(q.waitingCount(), 0);
        QTRY_COMPARE(e.nachgeholt.size(), 1);
        QCOMPARE(e.nachgeholt.first(), QStringLiteral("Wavelog 2/0"));
        QCOMPARE(e.hochgeladen.size(), 3);
        // Jedes genau einmal erfolgreich, keines doppelt.
        QCOMPARE(wavelog.received.count(QStringLiteral("DL1ABC")), 2);   // 1x abgelehnt, 1x angenommen
        QCOMPARE(wavelog.received.count(QStringLiteral("OK1DEF")), 1);
    }

    // uploadFinished nennt nur das Rufzeichen. Darum je Dienst immer
    // nur EIN QSO unterwegs — sonst liesse sich die Antwort nicht dem
    // richtigen zuordnen.
    void jeDienstImmerNurEinsUnterwegs()
    {
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        qrz.hold = true;
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz, &wavelog});

        // Zweimal dieselbe Station, zwei Baender — FT8 macht das.
        LogEntry a = ft8(QStringLiteral("DL1ABC"), 1);
        LogEntry b = ft8(QStringLiteral("DL1ABC"), 9);
        b.band = QStringLiteral("40m");
        q.submit(a);
        q.submit(b);
        QTRY_COMPARE(wavelog.received.size(), 2);
        QTRY_COMPARE(qrz.held.size(), 1);
        QTest::qWait(30);
        QCOMPARE(qrz.maxInFlight, 1);
        // Wavelog ist laengst fertig, QRZ haelt: unabhaengig voneinander.
        QCOMPARE(q.waitingCount(QStringLiteral("cloudlog")), 0);
        QVERIFY(e.fertig.isEmpty());

        qrz.release(refuse(QStringLiteral("timeout")));     // das erste scheitert
        QTRY_COMPARE(qrz.held.size(), 1);
        qrz.release();                                      // das zweite geht durch
        QTRY_COMPARE(e.fertig.size(), 2);
        QCOMPARE(qrz.maxInFlight, 1);

        // Das ERSTE ist gescheitert, nicht das zweite — obwohl beide
        // dasselbe Rufzeichen tragen.
        QCOMPARE(e.fertig.at(0).second.size(), 2);
        bool ersteQrzOk = true;
        for (const auto& o : e.fertig.at(0).second) {
            if (o.service == QStringLiteral("QRZ")) { ersteQrzOk = o.ok; }
        }
        QVERIFY(!ersteQrzOk);
        // …und wird gleich danach nachgeholt, weil das zweite durchging.
        QTRY_COMPARE(qrz.held.size(), 1);
        qrz.release();
        QTRY_COMPARE(q.waitingCount(), 0);
    }

    // Ein Duplikat ist ein Erfolg: der Dienst HAT das QSO.
    void einDuplikatGiltAlsErledigt()
    {
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        qrz.answerFor = [](const LogEntry&) {
            Answer a;
            a.duplicate = true;
            a.message = QStringLiteral("Already in your QRZ logbook");
            return a;
        };
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz});
        q.submit(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_COMPARE(e.fertig.size(), 1);
        QCOMPARE(q.waitingCount(), 0);
        QCOMPARE(QsoUploadQueue::statusLine(QStringLiteral("DL1ABC"), e.fertig.first().second),
                 QStringLiteral("DL1ABC — already in QRZ"));
    }

    // Ein Dienst, der am Boden liegt, kostet drei Anfragen je Versuch,
    // nicht eine je wartendem QSO.
    void dreiAbsagenBeendenDieRunde()
    {
        {
            FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
            wavelog.answerFor = [](const LogEntry&) { return refuse(QStringLiteral("HTTP 503")); };
            QsoUploadQueue q(m_store);
            Ergebnis e;
            mitschreiben(q, e);
            q.setUploaders({&wavelog});
            for (int i = 0; i < 6; ++i) { q.submit(ft8(QStringLiteral("DL%1ABC").arg(i), i)); }
            QTRY_COMPARE(e.fertig.size(), 6);
        }
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        wavelog.answerFor = [](const LogEntry&) { return refuse(QStringLiteral("HTTP 503")); };
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&wavelog});
        q.retryPending();
        QTRY_COMPARE(e.nachgeholt.size(), 1);
        QCOMPARE(wavelog.received.size(), QsoUploadQueue::kMaxRetryFailuresInARow);
        QCOMPARE(e.nachgeholt.first(), QStringLiteral("Wavelog 0/6"));
        QCOMPARE(q.waitingCount(), 6);   // und keines ist verloren
        QCOMPARE(QsoUploadQueue::storedCount(m_store), 6);
    }

    // Ein QSO, das der Dienst um seiner selbst willen ablehnt, darf
    // nicht bei jedem Versuch vor den anderen stehen.
    void einAbgelehntesWandertAnsEnde()
    {
        {
            FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
            wavelog.answerFor = [](const LogEntry&) { return refuse(QStringLiteral("HTTP 503")); };
            QsoUploadQueue q(m_store);
            Ergebnis e;
            mitschreiben(q, e);
            q.setUploaders({&wavelog});
            q.submit(ft8(QStringLiteral("BAD1X"), 1));
            q.submit(ft8(QStringLiteral("DL1ABC"), 2));
            q.submit(ft8(QStringLiteral("SP2XYZ"), 3));
            QTRY_COMPARE(e.fertig.size(), 3);
        }
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        wavelog.answerFor = [](const LogEntry& entry) {
            return entry.call == QStringLiteral("BAD1X")
                ? refuse(QStringLiteral("Differing locator")) : Answer{};
        };
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&wavelog});
        q.retryPending();
        QTRY_COMPARE(e.nachgeholt.size(), 1);
        QCOMPARE(e.nachgeholt.first(), QStringLiteral("Wavelog 2/1"));
        QCOMPARE(q.waitingCount(), 1);
        const QJsonArray rest = datei(m_store).value(QStringLiteral("services")).toObject()
                                    .value(QStringLiteral("cloudlog")).toArray();
        QCOMPARE(rest.size(), 1);
        QVERIFY(rest.at(0).toString().contains(QStringLiteral("BAD1X")));
    }

    // Ein Dienst, der gar nicht antwortet, haelt seine Spur nicht fuer
    // immer fest.
    void einSchweigenderDienstBlockiertNicht()
    {
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        wavelog.hold = true;
        QsoUploadQueue q(m_store);
        q.setAnswerTimeoutMs(60);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&wavelog});
        q.submit(ft8(QStringLiteral("DL1ABC"), 1));
        q.submit(ft8(QStringLiteral("SP2XYZ"), 2));
        QTRY_COMPARE(e.fertig.size(), 2);
        QCOMPARE(e.fertig.at(0).second.first().message, QStringLiteral("no answer"));
        QCOMPARE(wavelog.received.size(), 2);
        QCOMPARE(q.waitingCount(), 2);
    }

    // WSJT-X schickt sein Log-Telegramm gern zweimal. Ein QSO, eine
    // Zeile in der Warteliste.
    void dasselbeQsoWartetNurEinmal()
    {
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        wavelog.answerFor = [](const LogEntry&) { return refuse(QStringLiteral("HTTP 503")); };
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&wavelog});
        q.submit(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_COMPARE(e.fertig.size(), 1);
        q.submit(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_COMPARE(e.fertig.size(), 2);
        QCOMPARE(q.waitingCount(), 1);
    }

    // Der lokale Logger antwortet aus upload() heraus. Auch dann nichts
    // aus submit() heraus, und keine Rekursion ueber die Liste.
    void einSofortAntwortenderDienst()
    {
        FakeService lokal(QStringLiteral("adifnet"), QStringLiteral("local logger"));
        lokal.synchronous = true;
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&lokal});
        for (int i = 0; i < 40; ++i) {
            QCOMPARE(q.submit(ft8(QStringLiteral("DL%1ABC").arg(i), i)), 1);
        }
        QVERIFY(e.fertig.isEmpty());
        QTRY_COMPARE(e.fertig.size(), 40);
        QCOMPARE(lokal.maxInFlight, 1);
        QCOMPARE(q.waitingCount(), 0);
    }

    // Die Uploader gehoeren auch dem Upload-Menue des Logbuchfensters.
    // Dessen Antworten sind nicht unsere.
    void fremdeAntwortenWerdenNichtGezaehlt()
    {
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        qrz.hold = true;
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz});

        // Nichts von uns unterwegs: eine Antwort aendert nichts.
        emit qrz.uploadFinished(QStringLiteral("K1ABC"), true, false, QStringLiteral("Uploaded"));
        QVERIFY(e.hochgeladen.isEmpty());

        q.submit(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_COMPARE(qrz.held.size(), 1);
        // Eine Antwort fuer ein anderes Rufzeichen, waehrend unseres laeuft.
        emit qrz.uploadFinished(QStringLiteral("K1ABC"), true, false, QStringLiteral("Uploaded"));
        QVERIFY(e.hochgeladen.isEmpty());
        QCOMPARE(q.waitingCount(), 1);
        qrz.release();
        QTRY_COMPARE(e.hochgeladen, QStringList{QStringLiteral("qrz:DL1ABC")});
    }

    // Wird ein Dienst abbestellt, bleibt seine Liste liegen — verloren
    // geht nichts, gesendet wird nichts.
    void einAbbestellterDienstBehaeltSeineListe()
    {
        {
            FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
            wavelog.answerFor = [](const LogEntry&) { return refuse(QStringLiteral("HTTP 503")); };
            QsoUploadQueue q(m_store);
            Ergebnis e;
            mitschreiben(q, e);
            q.setUploaders({&wavelog});
            q.submit(ft8(QStringLiteral("DL1ABC"), 1));
            QTRY_COMPARE(e.fertig.size(), 1);
        }
        FakeService qrz(QStringLiteral("qrz"), QStringLiteral("QRZ"));
        FakeService wavelog(QStringLiteral("cloudlog"), QStringLiteral("Wavelog"));
        wavelog.configured = false;
        QsoUploadQueue q(m_store);
        Ergebnis e;
        mitschreiben(q, e);
        q.setUploaders({&qrz, &wavelog});
        q.retryPending();
        q.submit(ft8(QStringLiteral("SP2XYZ"), 2));
        QTRY_COMPARE(e.fertig.size(), 1);
        QVERIFY(wavelog.received.isEmpty());
        QCOMPARE(q.waitingCount(QStringLiteral("cloudlog")), 1);
        QCOMPARE(QsoUploadQueue::storedCount(m_store), 1);

        // Wieder eingerichtet: jetzt geht es hinaus.
        wavelog.configured = true;
        q.retryPending();
        QTRY_COMPARE(q.waitingCount(), 0);
        QCOMPARE(wavelog.received, QStringList{QStringLiteral("DL1ABC")});
    }

    void dieStatuszeileNenntDenDienst()
    {
        using O = QsoUploadQueue::Outcome;
        const O qrzOk{QStringLiteral("QRZ"), true, false, QStringLiteral("Uploaded")};
        const O wlOk{QStringLiteral("Wavelog"), true, false, QStringLiteral("accepted")};
        const O wlDup{QStringLiteral("Wavelog"), true, true, QStringLiteral("already in the log")};
        const O qrzBad{QStringLiteral("QRZ"), false, false, QStringLiteral("invalid api key")};
        const O wlBad{QStringLiteral("Wavelog"), false, false, QStringLiteral("API key rejected")};
        const QString c = QStringLiteral("OE1ABC");

        QCOMPARE(QsoUploadQueue::statusLine(c, {qrzOk, wlOk}),
                 QStringLiteral("OE1ABC — uploaded to QRZ, Wavelog"));
        QCOMPARE(QsoUploadQueue::statusLine(c, {qrzOk, wlDup}),
                 QStringLiteral("OE1ABC — uploaded to QRZ, already in Wavelog"));
        QCOMPARE(QsoUploadQueue::statusLine(c, {wlBad}),
                 QStringLiteral("OE1ABC logged locally, Wavelog upload failed: "
                                "API key rejected — will retry"));
        const QString beide = QsoUploadQueue::statusLine(c, {qrzBad, wlBad});
        QVERIFY2(beide.startsWith(QStringLiteral("OE1ABC logged locally, ")), qPrintable(beide));
        QVERIFY(beide.contains(QStringLiteral("QRZ upload failed: invalid api key")));
        QVERIFY(beide.contains(QStringLiteral("Wavelog upload failed: API key rejected")));
    }
};

QTEST_MAIN(TstQsoUploadQueue)
#include "tst_qso_upload_queue.moc"
