#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "core/Rf2ksConnection.h"

using namespace NereusSDR;

class RecordingAmpServer : public QTcpServer {
    Q_OBJECT
public:
    RecordingAmpServer() {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this]{
            auto* s = nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, this, [this, s]{
                m_requests << s->readAll();
                s->write("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
                s->flush();
                s->disconnectFromHost();
            });
        });
    }
    quint16 port() const { return serverPort(); }
    QList<QByteArray> requests() const { return m_requests; }
private:
    QList<QByteArray> m_requests;
};

class Rf2ksConnectionControlTest : public QObject {
    Q_OBJECT
private slots:
    void putAntennasActive();
    void putOperateMode();
    void putOperationalInterface();
    void postErrorReset();
};

// Find the first captured request that starts with the given prefix.
static QByteArray findRequest(const QList<QByteArray>& reqs, const QByteArray& prefix)
{
    for (const QByteArray& r : reqs) {
        if (r.startsWith(prefix)) {
            return r;
        }
    }
    return {};
}

void Rf2ksConnectionControlTest::putAntennasActive() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    // High poll interval - keeps polling noise out of the 200 ms window.
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());
    conn.setActiveAntenna(RfKitAntenna::Type::Internal, 2);
    QTest::qWait(200);
    QVERIFY(!server.requests().isEmpty());
    const QByteArray req = findRequest(server.requests(), "PUT /antennas/active ");
    QVERIFY2(!req.isEmpty(), "No PUT /antennas/active request captured");
    // QJsonObject sorts keys alphabetically: "number" < "type"
    QVERIFY(req.contains(R"("number":2)"));
    QVERIFY(req.contains(R"("type":"INTERNAL")"));
}

void Rf2ksConnectionControlTest::putOperateMode() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());
    conn.setOperateMode("OPERATE");
    QTest::qWait(200);
    QVERIFY(!server.requests().isEmpty());
    const QByteArray req = findRequest(server.requests(), "PUT /operate-mode ");
    QVERIFY2(!req.isEmpty(), "No PUT /operate-mode request captured");
    QVERIFY(req.contains(R"("operate_mode":"OPERATE")"));
}

void Rf2ksConnectionControlTest::putOperationalInterface() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());
    conn.setOperationalInterface("TCI");
    QTest::qWait(200);
    QVERIFY(!server.requests().isEmpty());
    const QByteArray req = findRequest(server.requests(), "PUT /operational-interface ");
    QVERIFY2(!req.isEmpty(), "No PUT /operational-interface request captured");
    QVERIFY(req.contains(R"("operational_interface":"TCI")"));
}

void Rf2ksConnectionControlTest::postErrorReset() {
    RecordingAmpServer server;
    Rf2ksConnection conn;
    conn.setPollIntervalMs(5000);
    conn.connectToAmp("127.0.0.1", server.port());
    conn.resetError();
    QTest::qWait(200);
    QVERIFY(!server.requests().isEmpty());
    const QByteArray req = findRequest(server.requests(), "POST /error/reset ");
    QVERIFY2(!req.isEmpty(), "No POST /error/reset request captured");
}

QTEST_MAIN(Rf2ksConnectionControlTest)
#include "tst_rf2ks_connection_control.moc"
