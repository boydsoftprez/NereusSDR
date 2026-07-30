#include <QtTest/QtTest>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <utility>
#include "core/Rf2ksConnection.h"

using namespace NereusSDR;

class ControlledInfoServer : public QTcpServer {
    Q_OBJECT
public:
    explicit ControlledInfoServer(QByteArray deviceName, bool autoRespond = false)
        : m_deviceName(std::move(deviceName))
        , m_autoRespond(autoRespond)
    {
        QVERIFY(listen(QHostAddress::LocalHost, 0));
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto* socket = nextPendingConnection();
            m_sockets.append(socket);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                const QByteArray request = socket->readAll();
                if (!request.startsWith("GET /info ")) {
                    return;
                }
                ++m_infoRequests;
                emit infoRequestObserved();
                if (m_autoRespond) {
                    respond(socket);
                } else {
                    m_pendingInfoSockets.append(socket);
                }
            });
        });
    }

    quint16 port() const { return serverPort(); }
    int infoRequests() const { return m_infoRequests; }

    void releasePendingInfo()
    {
        const auto pending = std::exchange(m_pendingInfoSockets, {});
        for (const QPointer<QTcpSocket>& socket : pending) {
            if (socket) {
                respond(socket);
            }
        }
    }

signals:
    void infoRequestObserved();

private:
    void respond(QTcpSocket* socket)
    {
        const QByteArray body =
            QByteArrayLiteral(R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":")")
            + m_deviceName
            + QByteArrayLiteral(R"("})");
        QByteArray response = QByteArrayLiteral(
            "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: ");
        response += QByteArray::number(body.size());
        response += QByteArrayLiteral("\r\n\r\n");
        response += body;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }

    QByteArray m_deviceName;
    bool m_autoRespond = false;
    int m_infoRequests = 0;
    QList<QPointer<QTcpSocket>> m_sockets;
    QList<QPointer<QTcpSocket>> m_pendingInfoSockets;
};

class ProbeScriptServer : public QTcpServer {
    Q_OBJECT
public:
    explicit ProbeScriptServer(QList<int> infoStatuses)
        : m_infoStatuses(std::move(infoStatuses))
    {
        QVERIFY(listen(QHostAddress::LocalHost, 0));
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto* socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                const QByteArray request = socket->readAll();
                const int pathStart = request.indexOf(' ') + 1;
                const int pathEnd = request.indexOf(' ', pathStart);
                const QString path =
                    QString::fromUtf8(request.mid(pathStart, pathEnd - pathStart));
                m_paths.append(path);
                emit requestObserved(path);

                if (path != QStringLiteral("/info")) {
                    respond(socket, 200);
                    return;
                }

                const int status = m_infoStatuses.isEmpty()
                    ? 0
                    : m_infoStatuses.takeFirst();
                if (status == 0) {
                    m_heldInfoSockets.append(socket);
                } else {
                    respond(socket, status);
                }
            });
        });
    }

    quint16 port() const { return serverPort(); }
    QStringList paths() const { return m_paths; }
    int infoRequestCount() const
    {
        return m_paths.count(QStringLiteral("/info"));
    }

    void releaseHeldInfo(int status)
    {
        const auto held = std::exchange(m_heldInfoSockets, {});
        for (const QPointer<QTcpSocket>& socket : held) {
            if (socket) {
                respond(socket, status);
            }
        }
    }

signals:
    void requestObserved(const QString& path);

private:
    static void respond(QTcpSocket* socket, int status)
    {
        const QByteArray body = status == 200
            ? QByteArrayLiteral(R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":"probe"})")
            : QByteArray();
        const QByteArray reason = status == 200
            ? QByteArrayLiteral("OK")
            : QByteArrayLiteral("Service Unavailable");
        QByteArray response = QByteArrayLiteral("HTTP/1.0 ");
        response += QByteArray::number(status);
        response += ' ';
        response += reason;
        response += QByteArrayLiteral(
            "\r\nContent-Type: application/json\r\nContent-Length: ");
        response += QByteArray::number(body.size());
        response += QByteArrayLiteral("\r\n\r\n");
        response += body;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }

    QList<int> m_infoStatuses;
    QStringList m_paths;
    QList<QPointer<QTcpSocket>> m_heldInfoSockets;
};

class Rf2ksConnectionReconnectTest : public QObject {
    Q_OBJECT
private slots:
    void backoffSequenceFollowsSchedule();
    void successResetsBackoff();
    void disconnectCancelsPendingReconnect();
    void autoReconnectOffSuppressesRetry();
    void disconnectInvalidatesDelayedReplyAcrossGenerations();
    void initialInfoFailureBacksOffAndReconnectProbesOnly();
    void manualDisconnectSuppressesInitialRetry();
    void initialFailureDoesNotRetryWhenDisabled();
};

void Rf2ksConnectionReconnectTest::backoffSequenceFollowsSchedule() {
    Rf2ksConnection conn;
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 1000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 2000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 4000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 8000);
    for (int i = 0; i < 5; ++i) { conn.testForceBackoffSequence(); }
    QVERIFY(conn.testCurrentBackoffMs() <= 60000);
}

void Rf2ksConnectionReconnectTest::successResetsBackoff() {
    Rf2ksConnection conn;
    conn.testForceBackoffSequence();
    conn.testForceBackoffSequence();
    conn.testForceBackoffSequence();
    QVERIFY(conn.testCurrentBackoffMs() > 1000);
    conn.testForceBackoffReset();
    QCOMPARE(conn.testCurrentBackoffMs(), 1000);
}

// Review blocker [P1] on PR #291: scheduleReconnect() used
// m_reconnectTimer.singleShot(...), which is the STATIC QTimer::singleShot
// invoked through an instance.  It compiles, but it does not arm
// m_reconnectTimer, so disconnect()'s m_reconnectTimer.stop() could never
// cancel it.  A pending reconnect could therefore fire after the user
// disabled RF-Kit or after RadioModel::teardownPeripherals(), re-issue
// /info and restart polling against a device the operator had turned off.
void Rf2ksConnectionReconnectTest::disconnectCancelsPendingReconnect() {
    Rf2ksConnection conn;

    // Arming a reconnect must arm the owned timer, not a detached one.
    conn.testScheduleReconnect();
    QVERIFY2(conn.testReconnectPending(),
             "scheduleReconnect() did not arm the owned m_reconnectTimer, "
             "so disconnect() cannot cancel it");

    // ...and disconnect() must cancel it.
    conn.disconnect();
    QVERIFY2(!conn.testReconnectPending(),
             "disconnect() left a reconnect pending");
}

// Review blocker [P2] on PR #291: RfKitPage saved an "Auto-reconnect"
// checkbox to AppSettings that nothing ever read back, so scheduleReconnect()
// retried unconditionally whatever the operator chose.
void Rf2ksConnectionReconnectTest::autoReconnectOffSuppressesRetry() {
    Rf2ksConnection conn;
    QVERIFY2(conn.autoReconnect(), "default must stay on (prior behaviour)");

    conn.setAutoReconnect(false);
    conn.testScheduleReconnect();
    QVERIFY2(!conn.testReconnectPending(),
             "auto-reconnect is off but a retry was still armed");

    // And back on again, so the gate is not a one-way latch.
    conn.setAutoReconnect(true);
    conn.testScheduleReconnect();
    QVERIFY(conn.testReconnectPending());
    conn.disconnect();
}

// A reply issued before an operator disconnect belongs to the old connection
// generation. Even if the server later supplies valid JSON, that reply must
// not parse state, count as a success, emit a connection transition, or affect
// a newer connection generation.
void Rf2ksConnectionReconnectTest::disconnectInvalidatesDelayedReplyAcrossGenerations()
{
    ControlledInfoServer firstServer(QByteArrayLiteral("old-generation"));
    ControlledInfoServer secondServer(QByteArrayLiteral("current-generation"),
                                      /*autoRespond=*/true);
    Rf2ksConnection conn;

    QSignalSpy connectedSpy(&conn, &Rf2ksConnection::connected);
    QSignalSpy infoSpy(&conn, &Rf2ksConnection::infoUpdated);
    QSignalSpy failedSpy(&conn, &Rf2ksConnection::connectionFailed);

    conn.connectToAmp(QStringLiteral("127.0.0.1"), firstServer.port());
    QTRY_COMPARE(firstServer.infoRequests(), 1);

    conn.disconnect();
    QCOMPARE(connectedSpy.count(), 0);
    QCOMPARE(infoSpy.count(), 0);
    QCOMPARE(conn.pollsSucceeded(), 0);
    QVERIFY(!conn.testReconnectPending());

    firstServer.releasePendingInfo();
    QVERIFY2(!infoSpy.wait(500),
             "a valid delayed /info reply crossed the manual-disconnect boundary");
    QCOMPARE(connectedSpy.count(), 0);
    QCOMPARE(conn.pollsSucceeded(), 0);
    QCOMPARE(conn.deviceName(), QString());
    QVERIFY(!conn.testReconnectPending());

    conn.connectToAmp(QStringLiteral("127.0.0.1"), secondServer.port());
    QTRY_COMPARE(connectedSpy.count(), 1);
    QTRY_COMPARE(infoSpy.count(), 1);
    QCOMPARE(conn.deviceName(), QStringLiteral("current-generation"));
    const int successesAfterCurrentGeneration = conn.pollsSucceeded();

    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(infoSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(conn.pollsSucceeded(), successesAfterCurrentGeneration);
    QCOMPARE(conn.deviceName(), QStringLiteral("current-generation"));
    QVERIFY(!conn.testReconnectPending());
}

void Rf2ksConnectionReconnectTest::initialInfoFailureBacksOffAndReconnectProbesOnly()
{
    ProbeScriptServer server({503, 0});
    Rf2ksConnection conn;
    QSignalSpy requestSpy(&server, &ProbeScriptServer::requestObserved);

    conn.connectToAmp(QStringLiteral("127.0.0.1"), server.port());
    QTRY_COMPARE(conn.pollsFailed(), 1);
    QCOMPARE(server.paths(), QStringList{QStringLiteral("/info")});
    QVERIFY2(conn.testReconnectPending(),
             "the first failed /info did not enter reconnect backoff");

    QVERIFY(QMetaObject::invokeMethod(&conn, "onReconnectTimeout",
                                      Qt::DirectConnection));
    QTRY_COMPARE(server.infoRequestCount(), 2);
    QCOMPARE(server.paths(),
             (QStringList{QStringLiteral("/info"), QStringLiteral("/info")}));
    QVERIFY2(!requestSpy.wait(200),
             "hot-path polling ran while the reconnect /info probe was outstanding");

    server.releaseHeldInfo(503);
    QTRY_COMPARE(conn.pollsFailed(), 2);
    QVERIFY2(conn.testReconnectPending(),
             "a failed reconnect probe did not re-arm backoff");
    QCOMPARE(server.paths(),
             (QStringList{QStringLiteral("/info"), QStringLiteral("/info")}));
    conn.disconnect();
}

void Rf2ksConnectionReconnectTest::manualDisconnectSuppressesInitialRetry()
{
    ProbeScriptServer server({503});
    Rf2ksConnection conn;
    QSignalSpy requestSpy(&server, &ProbeScriptServer::requestObserved);

    conn.connectToAmp(QStringLiteral("127.0.0.1"), server.port());
    QTRY_COMPARE(conn.pollsFailed(), 1);
    QVERIFY(conn.testReconnectPending());

    conn.disconnect();
    QVERIFY(!conn.testReconnectPending());
    QVERIFY(QMetaObject::invokeMethod(&conn, "onReconnectTimeout",
                                      Qt::DirectConnection));
    QVERIFY2(!requestSpy.wait(200),
             "manual disconnect allowed a reconnect timeout to issue /info");
    QVERIFY2(server.paths() == QStringList{QStringLiteral("/info")},
             "manual disconnect allowed a reconnect timeout to issue /info");
}

void Rf2ksConnectionReconnectTest::initialFailureDoesNotRetryWhenDisabled()
{
    ProbeScriptServer server({503});
    Rf2ksConnection conn;
    QSignalSpy requestSpy(&server, &ProbeScriptServer::requestObserved);
    conn.setAutoReconnect(false);

    conn.connectToAmp(QStringLiteral("127.0.0.1"), server.port());
    QTRY_COMPARE(conn.pollsFailed(), 1);
    QVERIFY(!conn.testReconnectPending());
    QVERIFY2(!requestSpy.wait(200),
             "normal polling continued after the failed initial /info");
    QCOMPARE(server.paths(), QStringList{QStringLiteral("/info")});
}

QTEST_MAIN(Rf2ksConnectionReconnectTest)
#include "tst_rf2ks_connection_reconnect.moc"
