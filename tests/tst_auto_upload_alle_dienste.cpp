// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Der ganze Weg (2026-11-17, Betreiber: "wichtig, alle qso die im digi
// modus erledigt werden sollen sofort auf qrz und wavelog"):
//
//   Ft8Applet::qsoFertig -> RotorLogbookPanel::logExternalQso
//       -> logbook.adi (zuerst!) -> QsoUploadQueue
//       -> QrzLogbookUploader + CloudlogUploader -> HTTP
//
// Mit den ECHTEN Uploadern, gegen einen Server auf 127.0.0.1, der
// antwortet wie QRZ (KEY=VALUE&…) und wie Wavelog (JSON, Statuscodes
// aus application/controllers/Api.php). Der Pruefstand geht nie ins
// Netz: QRZ bekommt seinen Endpunkt umgebogen, Wavelog ist ohnehin
// eine Adresse, die der Betreiber eintraegt.
//
// Bis zu diesem Tag ging live NUR QRZ hinaus; die Gegenprobe (alte
// Zeile `m_uploader->upload(e)` statt der Warteschlange) laesst
// einFt8QsoGehtAnQrzUndWavelog an der Wavelog-Zeile rot werden.

#include <QtTest>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUrlQuery>

#include <memory>

#include "core/AdifLog.h"
#include "core/CloudlogUploader.h"
#include "core/QrzLogbookUploader.h"
#include "core/QsoUploadQueue.h"
#include "gui/widgets/RotorLogbookPanel.h"

using namespace Longpath;

namespace {

// Gerade genug HTTP: eine Anfrage je Verbindung, Antwort je Pfad.
class KleinerServer : public QTcpServer {
    Q_OBJECT
public:
    struct Anfrage {
        QString    pfad;
        QByteArray rumpf;
        QByteArray kopf;
    };
    struct Antwort {
        int        status{200};
        QByteArray rumpf;
    };

    KleinerServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* s = nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, this, [this, s]() { lesen(s); });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
    }

    QString basis() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(serverPort());
    }
    QList<Anfrage> an(const QString& pfad) const
    {
        QList<Anfrage> out;
        for (const Anfrage& a : anfragen) {
            if (a.pfad == pfad) { out << a; }
        }
        return out;
    }

    QHash<QString, Antwort> antworten;
    QList<Anfrage> anfragen;

private:
    void lesen(QTcpSocket* s)
    {
        QByteArray& puffer = m_puffer[s];
        puffer += s->readAll();
        const int kopfEnde = puffer.indexOf("\r\n\r\n");
        if (kopfEnde < 0) { return; }
        const QByteArray kopf = puffer.left(kopfEnde);
        int laenge = 0;
        for (const QByteArray& zeile : kopf.split('\n')) {
            if (zeile.toLower().startsWith("content-length:")) {
                laenge = zeile.mid(15).trimmed().toInt();
            }
        }
        if (puffer.size() < kopfEnde + 4 + laenge) { return; }

        Anfrage a;
        a.kopf = kopf;
        a.pfad = QString::fromLatin1(kopf.left(kopf.indexOf('\r')).split(' ').value(1));
        a.rumpf = puffer.mid(kopfEnde + 4, laenge);
        m_puffer.remove(s);
        anfragen << a;

        const Antwort r = antworten.value(a.pfad, Antwort{404, "<html>Not Found</html>"});
        s->write("HTTP/1.1 " + QByteArray::number(r.status) + " X\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: " + QByteArray::number(r.rumpf.size()) + "\r\n"
                 "Connection: close\r\n\r\n" + r.rumpf);
        s->flush();
        s->disconnectFromHost();
    }

    QHash<QTcpSocket*, QByteArray> m_puffer;
};

const QString kQrzPfad = QStringLiteral("/qrz/api");
const QString kWavelogPfad = QStringLiteral("/wl/index.php/api/qso");

const KleinerServer::Antwort kQrzJa{200, "RESULT=OK&LOGID=4711&COUNT=1"};
const KleinerServer::Antwort kWavelogJa{
    201, R"({"status":"created","type":"adif","string":"","adif_count":1,"adif_errors":0,"messages":[""]})"};
const KleinerServer::Antwort kWavelogFalscherSchluessel{
    401, R"({"status":"failed","reason":"missing or wrong api key"})"};

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

QString statusText(const RotorLogbookPanel& panel, const QString& enthaelt)
{
    for (const QLabel* l : panel.findChildren<QLabel*>()) {
        if (l->text().contains(enthaelt)) { return l->text(); }
    }
    return {};
}

} // namespace

class TstAutoUploadAlleDienste : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    QString m_store;
    QString m_log;

    // Der Aufbau, wie MainWindow ihn macht: dieselben Uploader im
    // Panel (QRZ auch im Konstruktor, fuer den QRZ-Abgleich) und in der
    // Warteschlange.
    struct Station {
        QrzLogbookUploader qrz;
        CloudlogUploader   wavelog;
        std::unique_ptr<QsoUploadQueue> queue;
        std::unique_ptr<RotorLogbookPanel> panel;
    };
    void aufbauen(Station& st, const KleinerServer& server)
    {
        st.qrz.setApiKey(QStringLiteral("TEST-KEY-QRZ"));
        st.qrz.setEndpointForTest(QUrl(server.basis() + kQrzPfad));
        st.wavelog.setBaseUrl(server.basis() + QStringLiteral("/wl"));
        st.wavelog.setApiKey(QStringLiteral("test-key-wavelog"));
        st.wavelog.setStationProfileId(QStringLiteral("3"));
        st.queue = std::make_unique<QsoUploadQueue>(m_store);
        st.queue->setUploaders({&st.qrz, &st.wavelog});
        st.panel = std::make_unique<RotorLogbookPanel>(nullptr, nullptr, &st.qrz);
        st.panel->setUploadTargets({&st.qrz, &st.wavelog});
        st.panel->setUploadQueue(st.queue.get());
    }

    QVector<LogEntry> lokal() const { return AdifLog::read(m_log); }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_store = m_dir.filePath(QStringLiteral("qso-upload-queue.json"));
        m_log = RotorLogbookPanel::logbookPath();
        QVERIFY2(!m_log.startsWith(QDir::homePath() + QStringLiteral("/Library/Preferences/Longpath/"))
                 && !m_log.startsWith(QDir::homePath() + QStringLiteral("/.config/Longpath/")),
                 qPrintable(QStringLiteral("Testpfad zeigt auf echte Daten: ") + m_log));
    }

    void init()
    {
        QFile::remove(m_store);
        QFile::remove(m_log);
    }

    // (a) Ein FT8-QSO loest Uploads an BEIDE eingerichteten Dienste aus.
    void einFt8QsoGehtAnQrzUndWavelog()
    {
        KleinerServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.antworten.insert(kQrzPfad, kQrzJa);
        server.antworten.insert(kWavelogPfad, kWavelogJa);
        Station st;
        aufbauen(st, server);

        st.panel->logExternalQso(ft8(QStringLiteral("DL1ABC"), 1));

        // Erst lokal — das steht schon da, bevor irgendetwas hinausging.
        QCOMPARE(lokal().size(), 1);
        QVERIFY(server.anfragen.isEmpty());
        QVERIFY2(!statusText(*st.panel, QStringLiteral("uploading…")).isEmpty(),
                 "die Statuszeile sagt, dass hochgeladen wird");

        QTRY_COMPARE(server.an(kQrzPfad).size(), 1);
        QTRY_COMPARE(server.an(kWavelogPfad).size(), 1);   // <- Gegenprobe wird hier rot

        // QRZ: Formular mit Schluessel, INSERT und dem ADIF-Satz.
        const QUrlQuery form(QString::fromUtf8(server.an(kQrzPfad).first().rumpf));
        QCOMPARE(form.queryItemValue(QStringLiteral("KEY")), QStringLiteral("TEST-KEY-QRZ"));
        QCOMPARE(form.queryItemValue(QStringLiteral("ACTION")), QStringLiteral("INSERT"));
        QVERIFY(form.queryItemValue(QStringLiteral("ADIF"), QUrl::FullyDecoded)
                    .contains(QStringLiteral("<CALL:6>DL1ABC")));
        QVERIFY2(server.an(kQrzPfad).first().kopf.toLower().contains("user-agent: longpath/"),
                 "QRZ verlangt einen erkennbaren User-Agent");

        // Wavelog: JSON mit key, station_profile_id, type=adif, string.
        const QJsonObject json =
            QJsonDocument::fromJson(server.an(kWavelogPfad).first().rumpf).object();
        QCOMPARE(json.value(QStringLiteral("key")).toString(), QStringLiteral("test-key-wavelog"));
        QCOMPARE(json.value(QStringLiteral("station_profile_id")).toString(), QStringLiteral("3"));
        QCOMPARE(json.value(QStringLiteral("type")).toString(), QStringLiteral("adif"));
        const QString satz = json.value(QStringLiteral("string")).toString();
        QVERIFY(satz.contains(QStringLiteral("<CALL:6>DL1ABC")));
        QVERIFY(satz.contains(QStringLiteral("<MODE:3>FT8")));
        QVERIFY(satz.endsWith(QStringLiteral("<EOR>")));

        // Die Statuszeile nennt beide Dienste beim Namen.
        QTRY_VERIFY2(!statusText(*st.panel, QStringLiteral("DL1ABC — uploaded to QRZ, Wavelog")).isEmpty(),
                     qPrintable(statusText(*st.panel, QStringLiteral("DL1ABC"))));
        QCOMPARE(st.queue->waitingCount(), 0);
        QVERIFY(!QFile::exists(m_store));

        // Der Vermerk im Log gilt QRZ, und er steht am richtigen QSO.
        QTRY_VERIFY(lokal().first().uploadedToQrz);
    }

    // (b) Wavelog lehnt ab, QRZ geht durch, das QSO steht lokal.
    void einDienstScheitertDerAndereGehtDurch()
    {
        KleinerServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.antworten.insert(kQrzPfad, kQrzJa);
        server.antworten.insert(kWavelogPfad, kWavelogFalscherSchluessel);
        Station st;
        aufbauen(st, server);

        st.panel->logExternalQso(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_COMPARE(server.an(kQrzPfad).size(), 1);
        QTRY_COMPARE(server.an(kWavelogPfad).size(), 1);

        const QString erwartet = QStringLiteral(
            "DL1ABC — uploaded to QRZ; logged locally, "
            "Wavelog upload failed: API key rejected — will retry");
        QTRY_VERIFY2(!statusText(*st.panel, erwartet).isEmpty(),
                     qPrintable(statusText(*st.panel, QStringLiteral("DL1ABC"))));

        // Lokal steht es, vollstaendig, und nur einmal.
        const QVector<LogEntry> log = lokal();
        QCOMPARE(log.size(), 1);
        QCOMPARE(log.first().call, QStringLiteral("DL1ABC"));
        QCOMPARE(log.first().mode, QStringLiteral("FT8"));
        QTRY_VERIFY(lokal().first().uploadedToQrz);

        // Und es wartet — fuer Wavelog, nicht fuer QRZ.
        QCOMPARE(st.queue->waitingCount(QStringLiteral("qrz")), 0);
        QCOMPARE(st.queue->waitingCount(QStringLiteral("cloudlog")), 1);
        QCOMPARE(QsoUploadQueue::storedCount(m_store), 1);
        QFile f(m_store);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray inhalt = f.readAll();
        QVERIFY(inhalt.contains("DL1ABC"));
        QVERIFY2(!inhalt.contains("test-key-wavelog") && !inhalt.contains("TEST-KEY-QRZ"),
                 "kein Schluessel in der Warteliste");
    }

    // (c) Der gescheiterte Upload wird nachgeholt — beim naechsten Start.
    void dasGescheiterteWirdBeimNaechstenStartNachgeholt()
    {
        KleinerServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.antworten.insert(kQrzPfad, kQrzJa);
        server.antworten.insert(kWavelogPfad, kWavelogFalscherSchluessel);
        {
            Station st;
            aufbauen(st, server);
            st.panel->logExternalQso(ft8(QStringLiteral("DL1ABC"), 1));
            QTRY_COMPARE(st.queue->waitingCount(QStringLiteral("cloudlog")), 1);
            QTRY_COMPARE(server.an(kQrzPfad).size(), 1);
            QTRY_VERIFY(lokal().first().uploadedToQrz);
        }

        // Programm neu gestartet, der Schluessel inzwischen berichtigt.
        server.antworten.insert(kWavelogPfad, kWavelogJa);
        server.anfragen.clear();
        Station st;
        aufbauen(st, server);
        QCOMPARE(st.queue->waitingCount(QStringLiteral("cloudlog")), 1);

        st.queue->retryPending();
        QTRY_COMPARE(server.an(kWavelogPfad).size(), 1);
        QVERIFY(QJsonDocument::fromJson(server.an(kWavelogPfad).first().rumpf).object()
                    .value(QStringLiteral("string")).toString()
                    .contains(QStringLiteral("<CALL:6>DL1ABC")));
        QTRY_COMPARE(st.queue->waitingCount(), 0);
        QVERIFY(!QFile::exists(m_store));
        QTRY_VERIFY2(!statusText(*st.panel, QStringLiteral("Wavelog: 1 earlier contact uploaded")).isEmpty(),
                     qPrintable(statusText(*st.panel, QStringLiteral("Wavelog"))));

        // QRZ hatte es schon: kein zweites Mal.
        QTest::qWait(50);
        QCOMPARE(server.an(kQrzPfad).size(), 0);
        QCOMPARE(lokal().size(), 1);
    }

    // (c') …oder ohne Neustart, sobald das naechste QSO durchgeht.
    void dasGescheiterteWirdBeimNaechstenErfolgNachgeholt()
    {
        KleinerServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.antworten.insert(kQrzPfad, kQrzJa);
        server.antworten.insert(kWavelogPfad, KleinerServer::Antwort{502, "<html>Bad Gateway</html>"});
        Station st;
        aufbauen(st, server);

        st.panel->logExternalQso(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_COMPARE(st.queue->waitingCount(QStringLiteral("cloudlog")), 1);
        QTRY_VERIFY2(!statusText(*st.panel, QStringLiteral("Wavelog upload failed: HTTP 502")).isEmpty(),
                     qPrintable(statusText(*st.panel, QStringLiteral("DL1ABC"))));

        server.antworten.insert(kWavelogPfad, kWavelogJa);
        st.panel->logExternalQso(ft8(QStringLiteral("SP2XYZ"), 2));
        QTRY_COMPARE(st.queue->waitingCount(), 0);

        QStringList anWavelog;
        for (const auto& a : server.an(kWavelogPfad)) {
            anWavelog << (a.rumpf.contains("DL1ABC") ? QStringLiteral("DL1ABC")
                                                     : QStringLiteral("SP2XYZ"));
        }
        // DL1ABC abgelehnt, SP2XYZ angenommen, DL1ABC nachgeholt.
        QCOMPARE(anWavelog, (QStringList{QStringLiteral("DL1ABC"), QStringLiteral("SP2XYZ"),
                                         QStringLiteral("DL1ABC")}));
        QCOMPARE(lokal().size(), 2);
    }

    // Wavelog meldet ein Duplikat als HTTP 400 "abort". Das ist kein
    // Fehlschlag — sonst wuerde es ewig nachgeholt.
    void einWavelogDuplikatIstErledigt()
    {
        KleinerServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.antworten.insert(kQrzPfad, KleinerServer::Antwort{
            200, "RESULT=FAIL&REASON=Unable to add QSO to database: duplicate&COUNT=0"});
        // Eine deutsche Instanz: die Worte sind uebersetzt, der Rahmen nicht.
        server.antworten.insert(kWavelogPfad, KleinerServer::Antwort{
            400, R"({"status":"abort","type":"adif","string":"","adif_count":1,"adif_errors":1,)"
                 R"("messages":["","Date\/Time: 2026-11-17 14:01:15 Callsign: DL1ABC Band: 20M Doublette zu OE5SOS<br>"]})"});
        Station st;
        aufbauen(st, server);

        st.panel->logExternalQso(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_VERIFY2(!statusText(*st.panel, QStringLiteral("DL1ABC — already in QRZ, Wavelog")).isEmpty(),
                     qPrintable(statusText(*st.panel, QStringLiteral("DL1ABC"))));
        QCOMPARE(st.queue->waitingCount(), 0);
        QTRY_VERIFY(lokal().first().uploadedToQrz);
    }

    // Eine Adresse, die mit 200 und einer Webseite antwortet, ist keine
    // API. Frueher galt das als "hochgeladen".
    void eineWebseiteIstKeinErfolg()
    {
        KleinerServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.antworten.insert(kQrzPfad, kQrzJa);
        server.antworten.insert(kWavelogPfad, KleinerServer::Antwort{
            200, "<!DOCTYPE html><html><body>Login</body></html>"});
        Station st;
        aufbauen(st, server);

        st.panel->logExternalQso(ft8(QStringLiteral("DL1ABC"), 1));
        QTRY_VERIFY2(!statusText(*st.panel, QStringLiteral("check the instance URL")).isEmpty(),
                     qPrintable(statusText(*st.panel, QStringLiteral("DL1ABC"))));
        QCOMPARE(st.queue->waitingCount(QStringLiteral("cloudlog")), 1);
    }

    // Ohne eingerichtete Dienste: geloggt wird trotzdem, gemeldet wird
    // kein Upload.
    void ohneDiensteWirdNurGeloggt()
    {
        QrzLogbookUploader qrz;        // kein Schluessel
        CloudlogUploader wavelog;      // keine Adresse
        QsoUploadQueue queue(m_store);
        queue.setUploaders({&qrz, &wavelog});
        RotorLogbookPanel panel(nullptr, nullptr, &qrz);
        panel.setUploadQueue(&queue);

        panel.logExternalQso(ft8(QStringLiteral("DL1ABC"), 1));
        QCOMPARE(lokal().size(), 1);
        QVERIFY(statusText(panel, QStringLiteral("uploading")).isEmpty());
        QVERIFY(!statusText(panel, QStringLiteral("Logged DL1ABC")).isEmpty());
        QCOMPARE(queue.waitingCount(), 0);
    }
};

QTEST_MAIN(TstAutoUploadAlleDienste)
#include "tst_auto_upload_alle_dienste.moc"
