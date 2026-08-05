#include <QtTest/QtTest>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <utility>
#include "core/Rf2ksConnection.h"

using namespace NereusSDR;

// Minimal in-process HTTP/1.0 server that answers fixture JSON for the 8
// documented endpoints.  Listens on an ephemeral port; tests connect to
// it instead of a real amp.
class FakeAmpServer : public QTcpServer {
    Q_OBJECT
public:
    explicit FakeAmpServer(bool delayInfo = false)
        : m_delayInfo(delayInfo)
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this]{
            auto* sock = nextPendingConnection();
            connect(sock, &QTcpSocket::readyRead, this, [this, sock]{
                const QByteArray req = sock->readAll();
                const auto path = parsePath(req);
                m_paths.append(path);
                emit requestObserved(path);
                if (m_delayInfo && path == QStringLiteral("/info")) {
                    m_pendingInfoSockets.append(sock);
                    return;
                }
                respond(sock, path);
            });
        });
    }
    quint16 port() const { return serverPort(); }
    QStringList paths() const { return m_paths; }
    int requestCount(const QString& path) const { return m_paths.count(path); }

    void releasePendingInfo()
    {
        const auto pending = std::exchange(m_pendingInfoSockets, {});
        for (const QPointer<QTcpSocket>& socket : pending) {
            if (socket) {
                respond(socket, QStringLiteral("/info"));
            }
        }
    }

signals:
    void requestObserved(const QString& path);

private:
    void respond(QTcpSocket* sock, const QString& path) {
                const QByteArray body = bodyFor(path);
                QByteArray reply;
                reply  = "HTTP/1.0 200 OK\r\n";
                reply += "Content-Type: application/json\r\n";
                reply += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
                reply += body;
                sock->write(reply);
                sock->flush();
                sock->disconnectFromHost();
    }

    static QString parsePath(const QByteArray& req) {
        const int sp = req.indexOf(' ') + 1;
        const int end = req.indexOf(' ', sp);
        return QString::fromUtf8(req.mid(sp, end - sp));
    }
    static QByteArray bodyFor(const QString& path) {
        if (path == "/info")            return R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":"KG4VCF"})";
        if (path == "/power")           return R"({"temperature":{"value":27.0,"unit":"C"},"voltage":{"value":52.7,"unit":"V"},"current":{"value":0.0,"unit":"A"},"forward":{"value":0,"max_value":56,"unit":"W"},"reflected":{"value":0,"max_value":0,"unit":"W"},"swr":{"value":1.0,"max_value":1.11,"unit":""}})";
        if (path == "/tuner")           return R"({"mode":"AUTO","setup":"LC","L":{"value":1200,"unit":"nH"},"C":{"value":345,"unit":"pF"},"tuned_frequency":{"value":3891,"unit":"kHz"},"segment_size":{"value":9,"unit":"kHz"}})";
        if (path == "/antennas")        return R"({"antennas":[{"type":"INTERNAL","number":1,"state":"ACTIVE"},{"type":"INTERNAL","number":2,"state":"AVAILABLE"},{"type":"INTERNAL","number":3,"state":"AVAILABLE"},{"type":"INTERNAL","number":4,"state":"AVAILABLE"}]})";
        if (path == "/antennas/active") return R"({"type":"INTERNAL","number":1})";
        if (path == "/operate-mode")    return R"({"operate_mode":"STANDBY"})";
        if (path == "/operational-interface") return R"({"operational_interface":"TCI"})";
        if (path == "/data")            return R"({"band":{"value":80,"unit":"m"},"frequency":{"value":3895.0,"unit":"kHz"},"status":""})";
        return "{}";
    }

    bool m_delayInfo = false;
    QStringList m_paths;
    QList<QPointer<QTcpSocket>> m_pendingInfoSockets;
};

class Rf2ksConnectionPollTest : public QObject {
    Q_OBJECT
private slots:
    void connectsAndPollsOnce();
    void pollIntervalConfigurable();
    void pollingStartsOnlyAfterSuccessfulInfoProbe();
};

void Rf2ksConnectionPollTest::connectsAndPollsOnce() {
    FakeAmpServer server;
    Rf2ksConnection conn;
    conn.setPollIntervalMs(120);  // short for test

    QSignalSpy connSpy(&conn, &Rf2ksConnection::connected);
    QSignalSpy powerSpy(&conn, &Rf2ksConnection::powerUpdated);
    QSignalSpy opModeSpy(&conn, &Rf2ksConnection::operateModeUpdated);

    conn.connectToAmp("127.0.0.1", server.port());
    QVERIFY(connSpy.wait(2000));
    QVERIFY(conn.isConnected());

    QVERIFY(powerSpy.wait(2000));
    QVERIFY(opModeSpy.wait(2000));
    QVERIFY(conn.pollsSucceeded() >= 1);
}

void Rf2ksConnectionPollTest::pollIntervalConfigurable() {
    FakeAmpServer server;
    Rf2ksConnection conn;
    conn.setPollIntervalMs(200);
    conn.connectToAmp("127.0.0.1", server.port());

    QSignalSpy powerSpy(&conn, &Rf2ksConnection::powerUpdated);
    QVERIFY(powerSpy.wait(2000));
    const int firstPolls = conn.pollsSucceeded();

    QTest::qWait(500);   // expect ~2 more polls
    QVERIFY(conn.pollsSucceeded() >= firstPolls + 1);
}

void Rf2ksConnectionPollTest::pollingStartsOnlyAfterSuccessfulInfoProbe()
{
    FakeAmpServer server(/*delayInfo=*/true);
    Rf2ksConnection conn;
    conn.setPollIntervalMs(250);
    QSignalSpy requestSpy(&server, &FakeAmpServer::requestObserved);
    QSignalSpy connectedSpy(&conn, &Rf2ksConnection::connected);

    conn.connectToAmp(QStringLiteral("127.0.0.1"), server.port());
    QTRY_COMPARE(server.requestCount(QStringLiteral("/info")), 1);
    requestSpy.clear();
    QVERIFY2(!requestSpy.wait(200),
             "hot-path polling started before the initial /info succeeded");
    QCOMPARE(server.paths(), QStringList{QStringLiteral("/info")});
    QVERIFY(!conn.isConnected());

    server.releasePendingInfo();
    QVERIFY(connectedSpy.wait(1000));
    QTRY_VERIFY(server.paths().size() >= 2);
    QVERIFY2(server.paths().at(1) != QStringLiteral("/info"),
             "successful /info did not start staggered hot-path polling");
}

QTEST_MAIN(Rf2ksConnectionPollTest)
#include "tst_rf2ks_connection_poll.moc"
