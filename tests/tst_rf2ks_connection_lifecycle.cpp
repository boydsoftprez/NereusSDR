// =================================================================
// tests/tst_rf2ks_connection_lifecycle.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream source file ported.
//
// Modification history (NereusSDR):
//   2026-07-27 -- Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Covers the Codex review findings on PR #291 that the
//                 existing RF-Kit fakes could not reach: they model a
//                 server that is either up for the whole test or never
//                 started, so nothing could exercise a session ending
//                 while a request is still in flight, or an amp that
//                 stops answering mid-session.
//   2026-09-24 -- R-R3-47: identity admission (only an amp whose /info
//                 names an RF2K-S counts as connected, and another device
//                 is refused and never retried), the local default
//                 unchanged, and the retry and failure reports. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "core/Rf2ksConnection.h"

using namespace NereusSDR;

// In-process HTTP/1.0 fake with two modes the older fakes lack:
//
//   setResponseDelayMs(n) -- hold each request n ms before answering, so
//       a test can retire the session while a GET is genuinely in flight.
//   setAnswering(false)   -- accept the TCP connection then drop it
//       without a reply, so the connection racks up real poll failures
//       mid-session instead of never connecting at all.
class ControllableAmpServer : public QTcpServer {
    Q_OBJECT
public:
    ControllableAmpServer() {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto* sock = nextPendingConnection();
            connect(sock, &QTcpSocket::readyRead, this, [this, sock] {
                const QByteArray req = sock->readAll();
                if (!m_answering) {
                    // Close without a reply: QNAM surfaces this as an
                    // error, which is what drives markPollFailure().
                    sock->abort();
                    sock->deleteLater();
                    return;
                }
                const QString path = parsePath(req);
                ++m_requestCount;
                if (m_delayMs > 0) {
                    QTimer::singleShot(m_delayMs, sock, [this, sock, path] {
                        respond(sock, path);
                    });
                } else {
                    respond(sock, path);
                }
            });
        });
    }

    quint16 port() const { return serverPort(); }
    void setResponseDelayMs(int ms) { m_delayMs = ms; }
    void setAnswering(bool on) { m_answering = on; }
    int  requestCount() const { return m_requestCount; }
    void resetRequestCount() { m_requestCount = 0; }

private:
    void respond(QTcpSocket* sock, const QString& path) {
        if (!sock || sock->state() != QAbstractSocket::ConnectedState) {
            return;
        }
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

public:
    // R-R3-47: what /info answers.
    QByteArray infoBody =
        R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":"KG4VCF"})";
private:
    QByteArray bodyFor(const QString& path) const {
        if (path == "/info") {
            return infoBody;
        }
        if (path == "/operate-mode") { return R"({"operate_mode":"OPERATE"})"; }
        return "{}";
    }

    int  m_delayMs = 0;
    bool m_answering = true;
    int  m_requestCount = 0;
};

class Rf2ksConnectionLifecycleTest : public QObject {
    Q_OBJECT
private slots:
    void lateReplyAfterDisconnectDoesNotRevive();
    void lateReplyFromPreviousHostDoesNotApply();
    void pollingStopsWhenTheLinkIsDeclaredDown();
    // R-R3-47: identity admission (the Core's), and the local default.
    void identityAdmissionAdmitsAnRf2ks();
    void identityAdmissionRefusesAnotherDevice();
    void deviceChangingMidSessionIsRefused();
    void localConnectionStillConnectsOnAnyInfo();
    void unansweredProbeSaysWhyWhenRetryIsOff();
};

// Codex review [P1-adjacent P2] on PR #291: a GET still in flight when the
// operator disables RF-Kit completed afterwards, fell through to the
// "not connected yet" branch of onReplyFinished() and set m_connected back
// to true -- reviving a link the operator had just shut down.
void Rf2ksConnectionLifecycleTest::lateReplyAfterDisconnectDoesNotRevive()
{
    ControllableAmpServer server;
    server.setResponseDelayMs(300);

    Rf2ksConnection conn;
    conn.setAutoReconnect(false);   // keep the retry path out of this test
    conn.setPollIntervalMs(5000);   // the /info probe is the only request
    conn.connectToAmp("127.0.0.1", server.port());

    // Kill the session while the /info GET is still on the wire.
    QTest::qWait(50);
    QVERIFY2(!conn.isConnected(), "precondition: reply should not have landed yet");
    conn.disconnect();

    // Let the delayed reply arrive and be processed.
    QTest::qWait(600);

    QVERIFY2(!conn.isConnected(),
             "a reply issued before disconnect() revived the connection "
             "after the operator shut it down");
}

// Same race across an amplifier switch: the previous host's reply must not
// apply its state to the new session.
void Rf2ksConnectionLifecycleTest::lateReplyFromPreviousHostDoesNotApply()
{
    ControllableAmpServer slowAmp;
    slowAmp.setResponseDelayMs(300);
    ControllableAmpServer fastAmp;

    Rf2ksConnection conn;
    conn.setAutoReconnect(false);
    conn.setPollIntervalMs(5000);

    conn.connectToAmp("127.0.0.1", slowAmp.port());
    QTest::qWait(50);

    // Operator repoints at a different amp before the first answers.
    conn.connectToAmp("127.0.0.1", fastAmp.port());
    QSignalSpy connSpy(&conn, &Rf2ksConnection::connected);
    QVERIFY(connSpy.wait(2000));

    const int succeededAfterSwitch = conn.pollsSucceeded();

    // The slow amp's reply now lands, tagged with the retired generation.
    QTest::qWait(600);

    QCOMPARE(conn.pollsSucceeded(), succeededAfterSwitch);
    QCOMPARE(conn.peerAddress(), QStringLiteral("127.0.0.1"));
}

// Codex review [P2] on PR #291: markPollFailure() declared the link down
// after three consecutive failures and scheduled a retry, but left the
// poll timer running -- so the poller kept hammering a dead endpoint every
// few hundred ms while the backoff stretched to 60 s.
void Rf2ksConnectionLifecycleTest::pollingStopsWhenTheLinkIsDeclaredDown()
{
    ControllableAmpServer server;

    Rf2ksConnection conn;
    conn.setAutoReconnect(false);   // isolate the stop from the restart
    conn.setPollIntervalMs(250);    // fastest allowed; timer runs at /6

    QSignalSpy connSpy(&conn, &Rf2ksConnection::connected);
    conn.connectToAmp("127.0.0.1", server.port());
    QVERIFY(connSpy.wait(2000));
    QVERIFY(conn.testPollActive());

    // Amp stops answering mid-session.
    QSignalSpy disSpy(&conn, &Rf2ksConnection::disconnected);
    server.setAnswering(false);

    QVERIFY2(disSpy.wait(3000),
             "connection never declared the link down after the amp "
             "stopped answering");

    QVERIFY2(!conn.testPollActive(),
             "poll timer still running after the link was declared down; "
             "the poller keeps hammering a dead amp for the whole backoff "
             "window");
}

void Rf2ksConnectionLifecycleTest::identityAdmissionAdmitsAnRf2ks()
{
    ControllableAmpServer server;
    Rf2ksConnection conn;
    conn.setIdentityAdmissionRequired(true);
    conn.setPollIntervalMs(5000);
    QSignalSpy connSpy(&conn, &Rf2ksConnection::connected);
    conn.connectToAmp("127.0.0.1", server.port());
    QVERIFY(connSpy.wait(2000));
    QVERIFY(conn.isConnected());
    QCOMPARE(conn.reportedDevice(), QStringLiteral("RF2K-S"));
}

void Rf2ksConnectionLifecycleTest::identityAdmissionRefusesAnotherDevice()
{
    ControllableAmpServer server;
    server.infoBody = R"({"device":"SPE Expert","software_version":{"GUI":1,"controller":1}})";
    Rf2ksConnection conn;
    conn.setIdentityAdmissionRequired(true);
    conn.setPollIntervalMs(5000);
    QSignalSpy connSpy(&conn, &Rf2ksConnection::connected);
    QSignalSpy failSpy(&conn, &Rf2ksConnection::connectionFailed);
    conn.connectToAmp("127.0.0.1", server.port());
    QVERIFY(failSpy.wait(2000));
    QCOMPARE(connSpy.count(), 0);
    QVERIFY(!conn.isConnected());
    QVERIFY(!conn.reconnectPending());
    QVERIFY(!conn.testPollActive());
    QCOMPARE(failSpy.first().at(0).toString(),
             QStringLiteral("The device at this address is not an RF-Kit RF2K-S amplifier. "
                            "It reports itself as SPE Expert."));
    const int asked = server.requestCount();
    QTest::qWait(300);
    QCOMPARE(server.requestCount(), asked);
}

void Rf2ksConnectionLifecycleTest::deviceChangingMidSessionIsRefused()
{
    ControllableAmpServer server;
    Rf2ksConnection conn;
    conn.setIdentityAdmissionRequired(true);
    conn.setPollIntervalMs(5000);
    QSignalSpy connSpy(&conn, &Rf2ksConnection::connected);
    conn.connectToAmp("127.0.0.1", server.port());
    QVERIFY(connSpy.wait(2000));
    QSignalSpy disSpy(&conn, &Rf2ksConnection::disconnected);
    QSignalSpy failSpy(&conn, &Rf2ksConnection::connectionFailed);
    // The /info refresh (every ten poll cycles) finds another product at
    // the address. Ask for /info now rather than wait for the poller. (A
    // reply naming no device is retried, not refused: M2, R-R3-47;
    // tst_station_rfkit_controller answerThatNamesNoDeviceIsRetried.)
    server.infoBody = R"({"device":"SPE Expert","custom_device_name":"not an amp"})";
    QVERIFY(QMetaObject::invokeMethod(&conn, "onReconnectTimeout", Qt::DirectConnection));
    QVERIFY(failSpy.wait(2000));
    QCOMPARE(disSpy.count(), 1);
    QVERIFY(!conn.isConnected());
}

void Rf2ksConnectionLifecycleTest::localConnectionStillConnectsOnAnyInfo()
{
    ControllableAmpServer server;
    server.infoBody = R"({"custom_device_name":"KG4VCF"})";
    Rf2ksConnection conn;   // a local window: identity admission off
    conn.setPollIntervalMs(5000);
    QSignalSpy connSpy(&conn, &Rf2ksConnection::connected);
    conn.connectToAmp("127.0.0.1", server.port());
    QVERIFY(connSpy.wait(2000));
    QVERIFY(conn.isConnected());
}

void Rf2ksConnectionLifecycleTest::unansweredProbeSaysWhyWhenRetryIsOff()
{
    ControllableAmpServer server;
    server.setAnswering(false);
    Rf2ksConnection conn;
    conn.setAutoReconnect(false);
    QSignalSpy failSpy(&conn, &Rf2ksConnection::connectionFailed);
    QSignalSpy retrySpy(&conn, &Rf2ksConnection::reconnectScheduled);
    conn.connectToAmp("127.0.0.1", server.port());
    QVERIFY(failSpy.wait(2000));
    QCOMPARE(failSpy.first().at(0).toString(),
             QStringLiteral("The RF-Kit amplifier did not answer at this address."));
    QCOMPARE(retrySpy.count(), 0);

    Rf2ksConnection retrying;
    QSignalSpy retrying2(&retrying, &Rf2ksConnection::reconnectScheduled);
    retrying.connectToAmp("127.0.0.1", server.port());
    QVERIFY(retrying2.wait(2000));
    QCOMPARE(retrying2.first().at(0).toInt(), 1);
    QCOMPARE(retrying2.first().at(1).toInt(), 1000);
    retrying.disconnect();
    QVERIFY(!retrying.reconnectPending());
}

QTEST_MAIN(Rf2ksConnectionLifecycleTest)
#include "tst_rf2ks_connection_lifecycle.moc"
