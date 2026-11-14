// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_roundtable_e2e.cpp  (Longpath)
// =================================================================
//
// Der ganze Weg gegen den ECHTEN Roundtable-Server: Upstreams
// Referenzserver (roundtable/server, byte fuer byte uebernommen) laeuft
// auf 127.0.0.1, zwei Stationen melden sich an — eine nackt ueber
// RoundtableClient, eine ueber RoundtableHub mit SpotModel. Die zweite
// muss die erste als Spot der Quelle "Roundtable" sehen, sich selbst
// nicht, und den Spot wieder verlieren, wenn die erste geht.
//
// Damit ist bewiesen, was die Schein-Server der anderen Pruefstaende
// nicht beweisen koennen: dass der Server unsere Signatur (mbedTLS)
// annimmt, unser hello und presence versteht und uns die Frequenz-
// Erweiterung mit stations[] gibt.
//
// Braucht Python ab 3.9 mit `websockets` und `cryptography`
// (roundtable/README.md). Ohne sie wird uebersprungen. Den Pfad gibt
// LONGPATH_ROUNDTABLE_PYTHON vor, sonst `python3`. Nichts verlaesst den
// Rechner.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableKey.h"
#include "models/RoundtableHub.h"
#include "models/SpotModel.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace Longpath;

namespace {

constexpr int kStartMs = 20000;
// Die Raumliste baut der Server alle 2 s (Protokoll 6.11).
constexpr int kSeeMs = 20000;

quint16 freePort()
{
    QTcpServer s;
    s.listen(QHostAddress::LocalHost, 0);
    return s.serverPort();
}

QList<SpotData> roundtableSpots(const SpotModel& model)
{
    QList<SpotData> out;
    for (const SpotData& s : model.spots()) {
        if (s.source == QLatin1String("Roundtable")) {
            out.append(s);
        }
    }
    return out;
}

} // namespace

class TstRoundtableE2e : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString root = QString::fromLocal8Bit(qgetenv("LONGPATH_SOURCE_DIR"));
        if (root.isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt (ueber ctest starten)");
        }
        m_python = QString::fromLocal8Bit(qgetenv("LONGPATH_ROUNDTABLE_PYTHON"));
        if (m_python.isEmpty()) {
            m_python = QStringLiteral("python3");
        }
        QProcess probe;
        probe.start(m_python, {QStringLiteral("-c"),
                               QStringLiteral("import sys, websockets, cryptography; "
                                              "sys.exit(0 if sys.version_info >= (3, 9) else 1)")});
        if (!probe.waitForFinished(15000) || probe.exitStatus() != QProcess::NormalExit
            || probe.exitCode() != 0) {
            QSKIP("Kein Python mit websockets und cryptography (LONGPATH_ROUNDTABLE_PYTHON)");
        }

        QVERIFY(m_dir.isValid());
        m_wsPort = freePort();
        quint16 http = freePort();
        while (http == m_wsPort) {
            http = freePort();
        }
        QDir().mkpath(m_dir.filePath(QStringLiteral("static")));
        QDir().mkpath(m_dir.filePath(QStringLiteral("backup")));
        QFile conf(m_dir.filePath(QStringLiteral("roundtable.conf")));
        QVERIFY(conf.open(QIODevice::WriteOnly));
        conf.write(QStringLiteral("[roundtable]\n"
                                  "ws_listen = 127.0.0.1:%1\n"
                                  "http_listen = 127.0.0.1:%2\n"
                                  "public_host = 127.0.0.1\n"
                                  "database = %3\n"
                                  "backup_dir = %4\n"
                                  "static_dir = %5\n"
                                  "log_level = info\n")
                       .arg(m_wsPort)
                       .arg(http)
                       .arg(m_dir.filePath(QStringLiteral("roundtable.db")),
                            m_dir.filePath(QStringLiteral("backup")),
                            m_dir.filePath(QStringLiteral("static")))
                       .toUtf8());
        conf.close();

        m_server = std::make_unique<QProcess>();
        m_server->setWorkingDirectory(root + QStringLiteral("/roundtable/server"));
        m_server->setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PYTHONDONTWRITEBYTECODE"), QStringLiteral("1"));
        m_server->setProcessEnvironment(env);
        m_server->start(m_python, {QStringLiteral("-m"), QStringLiteral("nereus_roundtable"),
                                   QStringLiteral("--config"), conf.fileName(),
                                   QStringLiteral("--stop-on-stdin-eof")});
        QVERIFY2(m_server->waitForStarted(kStartMs), qPrintable(m_server->errorString()));
        const quint16 port = m_wsPort;
        const bool up = QTest::qWaitFor([port] {
            QTcpSocket s;
            s.connectToHost(QHostAddress::LocalHost, port);
            return s.waitForConnected(200);
        }, kStartMs);
        QVERIFY2(up, qPrintable(QString::fromUtf8(m_server->readAll())));
    }

    void cleanupTestCase()
    {
        if (m_server) {
            m_server->closeWriteChannel(); // --stop-on-stdin-eof
            if (!m_server->waitForFinished(10000)) {
                m_server->kill();
                m_server->waitForFinished(5000);
            }
            qInfo().noquote() << "Server-Protokoll:\n" << QString::fromUtf8(m_server->readAll());
        }
    }

    void zweiStationenSehenEinander()
    {
        const QUrl url(QStringLiteral("ws://127.0.0.1:%1/v1/station").arg(m_wsPort));

        // B: nackter Client. Getastet, damit sein Raum sofort aufgeht
        // (Protokoll 6.3: sonst erst nach 10 s Mitgliedschaft).
        QString err;
        RoundtableKey keyB = RoundtableKey::generate(&err);
        QVERIFY2(keyB.isValid(), qPrintable(err));
        auto b = std::make_unique<RoundtableClient>();
        b->setServerUrl(url);
        b->setIdentity(&keyB);
        b->setStationInfo(QStringLiteral("DL1ABC"), QStringLiteral("JN58td"));
        RoundtableSliceState sb;
        sb.id = 0;
        sb.hz = 14212000;
        sb.mode = QStringLiteral("USB");
        sb.filterHz = 2700;
        sb.keyed = true;
        b->setSlices({sb});
        b->setFirstRunAnswered(true);
        b->start();
        QTRY_COMPARE_WITH_TIMEOUT(b->linkState(), RoundtableClient::LinkState::Live, kStartMs);
        QCOMPARE(b->stationId(), keyB.roundtableId());

        // A: ueber den Hub, so wie Longpath es tut.
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("OE5SOS"));
        s.setValue(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("JN78"));
        s.setValue(QStringLiteral("Roundtable/ServerUrl"), url.toString());
        s.setValue(QStringLiteral("Roundtable/AutoStart"), QStringLiteral("True"));
        s.setValue(QStringLiteral("Roundtable/FirstRunAnswer"), QStringLiteral("Live"));
        s.setValue(QStringLiteral("Roundtable/Hidden"), QStringLiteral("False"));
        RoundtableHub hub;
        hub.setKeyPathForTest(m_dir.filePath(QStringLiteral("a/roundtable-identity.pem")));
        SpotModel spots;
        hub.attachSpots(&spots);
        RoundtableSliceState sa;
        sa.id = 0;
        sa.hz = 14212100;
        sa.mode = QStringLiteral("USB");
        sa.filterHz = 2700;
        hub.client()->setSlices({sa});
        hub.reloadSettings();
        QTRY_COMPARE_WITH_TIMEOUT(hub.client()->linkState(), RoundtableClient::LinkState::Live,
                                  kStartMs);
        QCOMPARE(hub.statusText(), QStringLiteral("Live as OE5SOS"));
        // Der Schluessel liegt da, wo wir ihn hinlegen, und der Server
        // kennt uns unter seiner Id.
        const RoundtableKey keyA = RoundtableKey::loadOrCreate(
            m_dir.filePath(QStringLiteral("a/roundtable-identity.pem")));
        QCOMPARE(hub.client()->stationId(), keyA.roundtableId());

        // B erscheint als Spot, A nicht.
        QTRY_VERIFY_WITH_TIMEOUT(!roundtableSpots(spots).isEmpty(), kSeeMs);
        QList<SpotData> list = roundtableSpots(spots);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().callsign, QStringLiteral("DL1ABC"));
        QCOMPARE(list.first().rxFreqMhz, 14.212);
        QCOMPARE(list.first().mode, QStringLiteral("USB"));
        QVERIFY(list.first().comment.contains(QStringLiteral("on the air")));
        qInfo() << "Spot:" << list.first().callsign << list.first().rxFreqMhz
                << list.first().comment;

        // B geht: der Spot geht mit.
        b->stop();
        b.reset();
        QTRY_VERIFY_WITH_TIMEOUT(roundtableSpots(spots).isEmpty(), kSeeMs);

        hub.client()->stop();
    }

private:
    QTemporaryDir m_dir;
    QString m_python;
    quint16 m_wsPort = 0;
    std::unique_ptr<QProcess> m_server;
};

QTEST_GUILESS_MAIN(TstRoundtableE2e)
#include "tst_roundtable_e2e.moc"
