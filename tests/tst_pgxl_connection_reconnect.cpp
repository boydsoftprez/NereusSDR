// =================================================================
// tests/tst_pgxl_connection_reconnect.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (auto-reconnect
// with exponential backoff is a NereusSDR Tier 2 addition per
// design doc §2 and §6.4).
// Backoff sequence: 1/2/5/10/30/60 s, saturates at 60 s.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Test: backoffSequence verifies 8 calls to
//                 testForceDisconnect() produce the expected
//                 1/2/5/10/30/60/60/60 s delay emissions.
//   2026-09-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code (R-R3-47, R-R3-22): the owned retry timer's
//                 lifecycle, the Tuner Genius connection's cases
//                 (tst_tgxl_connection_reconnect) against the Power Genius:
//                 a replaced or cancelled address is never redialled, in
//                 any phase; a retry gets a fresh socket; a late callback
//                 from a replaced attempt cannot act. Loopback only.
//   2026-10-14  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code: closedLoopbackPort() takes the ports the test is
//                 listening on and never returns one of them. The
//                 replacement test opens its listener before picking the
//                 closed port; the OS could otherwise hand the just-released
//                 port to the replacement, so the "closed" address answered.
//   2026-10-22, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//     test ports come from unusedLoopbackPort() (UnusedLoopbackPort.h), below
//     the kernel's ephemeral range, so a parallel test cannot take one between
//     the pick and the listen.
// =================================================================

#include "UnusedLoopbackPort.h"
#include <QtTest/QtTest>
#include <QPointer>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <algorithm>
#include <initializer_list>
#include "core/PgxlConnection.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

namespace {

constexpr int kBackoffUnitMs = 200;
constexpr int kRetryObservationMs = 500;
// A refused loopback dial reports in milliseconds on Linux and macOS but
// takes about 3.4 s on Windows, which retries the SYN after the reset.
constexpr int kRefusedDialMs = 8000;

// A loopback port nothing listens on, never one of the test's own (`inUse`).
// unusedLoopbackPort() picks below the kernel's ephemeral range, so the
// next listen(0) is never handed it. Returns 0 when no such port is found.
quint16 closedLoopbackPort(std::initializer_list<quint16> inUse = {})
{
    constexpr int kMaxPicks = 16;
    for (int pick = 0; pick < kMaxPicks; ++pick) {
        const quint16 port = unusedLoopbackPort();
        if (port == 0) { return 0; }
        if (std::find(inUse.begin(), inUse.end(), port) == inUse.end()) { return port; }
    }
    return 0;
}

void answerVersion(QTcpServer& server, int& accepted, QList<QPointer<QTcpSocket>>& peers)
{
    while (server.hasPendingConnections()) {
        QTcpSocket* peer = server.nextPendingConnection();
        peers.push_back(peer);
        ++accepted;
        peer->write("V3.8.9\n");
        peer->flush();
    }
}

} // namespace

class PgxlConnectionReconnectTest : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void backoffSequence();
    void endpointReplacementCancelsCapturedRetry();
    void explicitDisconnectCancelsCapturedRetry();
    void disconnectWhileDialQueuedNeverDials();
    void disconnectWhileIdentifyingNeverRedials();
    void replacingPendingAWithBLeavesOnlyB();
    void connectErrorAndDisconnectLeaveOneOwnedRetry();
    void retryRetiresTheFailedQtSocket();
    void sourceBindFailureMakesOneOsFallbackDial();
    void reconnectAttemptConsumerMayDisconnect();
    void autoReconnectTurnedOffDropsPendingRetry();
    void lateFailureFromReplacedAttemptCannotRetireReplacement();
    void closeByTheAmpLogsTheLastFrames();
};

void PgxlConnectionReconnectTest::init()
{
    AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("True"));
}

void PgxlConnectionReconnectTest::cleanup()
{
    AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("True"));
}

void PgxlConnectionReconnectTest::backoffSequence() {
    PgxlConnection conn;
    QSignalSpy spy(&conn, &NereusSDR::PgxlConnection::reconnectAttempt);
    NereusSDR::AppSettings::instance().setValue("PGXL_AutoReconnect", "True");
    // Seed a last host so scheduleReconnect does not bail on empty host.
    // testForceDisconnect() calls scheduleReconnect() directly, which
    // checks m_lastHost. We set it via the public connectToPgxl path;
    // use a non-routable address so no real connect fires.
    // NOTE: connectToPgxl() clears m_reconnectAttempts - set after the
    // call so the counter starts at 0.
    // The testForceDisconnect() call does NOT reset m_reconnectAttempts,
    // so 8 consecutive calls accumulate attempts 0..7 producing the
    // expected delay table.

    // Directly prime the private m_lastHost via connectToPgxl (which
    // will not block since the socket is not event-loop-driven here).
    conn.connectToPgxl("192.0.2.1", 9008);  // non-routable TEST-NET-1 per RFC 5737

    // m_reconnectAttempts was reset to 0 by onConnected in connectToPgxl.
    // onConnected only fires on real TCP connect which doesn't happen here,
    // so m_reconnectAttempts stays at whatever value it was (0 after ctor).
    // The QTimer::singleShot calls in scheduleReconnect() do NOT fire here
    // because the test runs without a running event loop (QTEST_GUILESS_MAIN).
    // We just verify the reconnectAttempt signal emissions (the increments to
    // m_reconnectAttempts inside the singleShot lambdas never execute).
    for (int i = 0; i < 8; ++i) {
        conn.testForceDisconnect();
    }

    // Collect all emitted delays.
    QVector<int> delays;
    while (spy.count()) {
        delays << spy.takeFirst().at(1).toInt();
    }
    QCOMPARE(delays.count(), 8);
    QCOMPARE(delays.value(0), 1000);
    QCOMPARE(delays.value(1), 2000);
    QCOMPARE(delays.value(2), 5000);
    QCOMPARE(delays.value(3), 10000);
    QCOMPARE(delays.value(4), 30000);
    QCOMPARE(delays.value(5), 60000);
    QCOMPARE(delays.value(6), 60000);
    QCOMPARE(delays.value(7), 60000);
}

// Replacing a retrying address with a new one: only the new one is dialled.
void PgxlConnectionReconnectTest::endpointReplacementCancelsCapturedRetry()
{
    QTcpServer replacement;
    QVERIFY(replacement.listen(QHostAddress::LocalHost, 0));
    const quint16 oldPort = closedLoopbackPort({replacement.serverPort()});
    QVERIFY(oldPort != 0);
    QVERIFY(oldPort != replacement.serverPort());
    int replacementConnections = 0;
    QList<QPointer<QTcpSocket>> peers;
    connect(&replacement, &QTcpServer::newConnection, this, [&] {
        answerVersion(replacement, replacementConnections, peers);
    });

    PgxlConnection conn;
    conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
    QSignalSpy attempts(&conn, &PgxlConnection::reconnectAttempt);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), oldPort);
    QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, kRefusedDialMs);
    QTcpServer oldEndpoint;
    QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, oldPort));
    QSignalSpy staleConnections(&oldEndpoint, &QTcpServer::newConnection);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), replacement.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
    QTest::qWait(kRetryObservationMs);
    QCOMPARE(staleConnections.size(), 0);
    QCOMPARE(replacementConnections, 1);
    QCOMPARE(conn.peerPort(), replacement.serverPort());
    QVERIFY(!conn.testReconnectPending());
}

// Disconnect during a pending retry: the old address is never dialled again.
void PgxlConnectionReconnectTest::explicitDisconnectCancelsCapturedRetry()
{
    const quint16 oldPort = closedLoopbackPort();
    QVERIFY(oldPort != 0);
    PgxlConnection conn;
    conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
    QSignalSpy attempts(&conn, &PgxlConnection::reconnectAttempt);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), oldPort);
    QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, kRefusedDialMs);
    QVERIFY(conn.testReconnectPending());
    conn.disconnect();
    QVERIFY(!conn.testReconnectPending());

    QTcpServer oldEndpoint;
    QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, oldPort));
    QSignalSpy staleConnections(&oldEndpoint, &QTcpServer::newConnection);
    QTest::qWait(kRetryObservationMs + 3 * kBackoffUnitMs);
    QCOMPARE(staleConnections.size(), 0);
    QVERIFY(!conn.testReconnectPending());
}

// Disconnect before the queued dial runs: nothing is dialled at all.
void PgxlConnectionReconnectTest::disconnectWhileDialQueuedNeverDials()
{
    QTcpServer endpoint;
    QVERIFY(endpoint.listen(QHostAddress::LocalHost, 0));
    QSignalSpy connections(&endpoint, &QTcpServer::newConnection);
    PgxlConnection conn;
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), endpoint.serverPort());
    conn.disconnect();
    QTest::qWait(kRetryObservationMs);
    QCOMPARE(connections.size(), 0);
    QCOMPARE(conn.testActiveSocketAttemptGeneration(), quint64(0));
    QVERIFY(!conn.testReconnectPending());
}

// Disconnect while the Core is identifying the amp (V seen, info pending):
// no redial, and the attempt's late V cannot admit anything.
void PgxlConnectionReconnectTest::disconnectWhileIdentifyingNeverRedials()
{
    QTcpServer endpoint;
    QVERIFY(endpoint.listen(QHostAddress::LocalHost, 0));
    int accepted = 0;
    QList<QPointer<QTcpSocket>> peers;
    connect(&endpoint, &QTcpServer::newConnection, this, [&] {
        answerVersion(endpoint, accepted, peers);
    });
    PgxlConnection conn;
    conn.setIdentityAdmissionRequired(true);
    conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
    QSignalSpy progress(&conn, &PgxlConnection::identityProtocolProgress);
    QSignalSpy retries(&conn, &PgxlConnection::reconnectAttempt);
    QSignalSpy connected(&conn, &PgxlConnection::connected);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), endpoint.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(progress.size(), 1, 2000);
    const quint64 attempt = conn.testActiveSocketAttemptGeneration();
    conn.disconnect();
    conn.testInjectLineForSocketAttempt(QStringLiteral("R1|0|serial=X version=3.8.9"), attempt);
    QTest::qWait(kRetryObservationMs + 3 * kBackoffUnitMs);
    QCOMPARE(accepted, 1);
    QCOMPARE(retries.size(), 0);
    QCOMPARE(connected.size(), 0);
    QVERIFY(!conn.isConnected());
    QVERIFY(!conn.testReconnectPending());
}

// A is still connecting (its TCP accepted, no V yet) when B replaces it:
// only B completes, A's late V cannot act, and A is not dialled again.
void PgxlConnectionReconnectTest::replacingPendingAWithBLeavesOnlyB()
{
    QTcpServer endpointA;
    QTcpServer endpointB;
    QVERIFY(endpointA.listen(QHostAddress::LocalHost, 0));
    QVERIFY(endpointB.listen(QHostAddress::LocalHost, 0));
    QPointer<QTcpSocket> heldA;
    int connectionsA = 0;
    connect(&endpointA, &QTcpServer::newConnection, this, [&] {
        ++connectionsA;
        heldA = endpointA.nextPendingConnection();
    });
    int connectionsB = 0;
    QList<QPointer<QTcpSocket>> peersB;
    connect(&endpointB, &QTcpServer::newConnection, this, [&] {
        answerVersion(endpointB, connectionsB, peersB);
    });

    PgxlConnection conn;
    conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
    QSignalSpy connected(&conn, &PgxlConnection::connected);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), endpointA.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(!heldA.isNull(), 2000);
    const quint64 attemptA = conn.testActiveSocketAttemptGeneration();
    QVERIFY(attemptA != 0);

    conn.connectToPgxl(QStringLiteral("127.0.0.1"), endpointB.serverPort());
    conn.testInjectLineForSocketAttempt(QStringLiteral("V3.8.9"), attemptA);
    QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
    heldA->write("V3.8.9\n");
    heldA->flush();
    heldA->disconnectFromHost();
    QTest::qWait(kRetryObservationMs);
    QCOMPARE(connected.size(), 1);
    QCOMPARE(connectionsA, 1);
    QCOMPARE(connectionsB, 1);
    QCOMPARE(conn.peerPort(), endpointB.serverPort());
    QCOMPARE(conn.configuredPort(), endpointB.serverPort());
    QVERIFY(!conn.testReconnectPending());
}

// Qt reports one failed handshake twice (error, then disconnected): one retry.
void PgxlConnectionReconnectTest::connectErrorAndDisconnectLeaveOneOwnedRetry()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    int accepted = 0;
    connect(&server, &QTcpServer::newConnection, this, [&] {
        while (server.hasPendingConnections()) {
            QTcpSocket* peer = server.nextPendingConnection();
            ++accepted;
            peer->disconnectFromHost();
            peer->deleteLater();
        }
    });
    PgxlConnection conn;
    conn.testSetReconnectBackoffUnitMs(1000);
    QSignalSpy attempts(&conn, &PgxlConnection::reconnectAttempt);
    QSignalSpy failures(&conn, &PgxlConnection::connectionFailed);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), server.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(accepted, 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, 2000);
    QTest::qWait(100);
    QCOMPARE(attempts.size(), 1);
    QVERIFY(conn.testReconnectPending());
}

// The Rock's invalid-descriptor errors: a retry must own a fresh socket.
void PgxlConnectionReconnectTest::retryRetiresTheFailedQtSocket()
{
    const quint16 deadPort = closedLoopbackPort();
    QVERIFY(deadPort != 0);
    PgxlConnection conn;
    conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
    QSignalSpy attempts(&conn, &PgxlConnection::reconnectAttempt);
    QSignalSpy failures(&conn, &PgxlConnection::connectionFailed);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), deadPort);
    QTRY_VERIFY_WITH_TIMEOUT(conn.testActiveSocketAttemptGeneration() != 0, 2000);
    const quint64 failedAttempt = conn.testActiveSocketAttemptGeneration();
    QPointer<QTcpSocket> failedSocket = conn.testSocketForTesting();
    QVERIFY(!failedSocket.isNull());
    QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, kRefusedDialMs);
    QTRY_VERIFY_WITH_TIMEOUT(conn.testActiveSocketAttemptGeneration() != failedAttempt, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(failedSocket.isNull(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(failures.size() >= 2, kRefusedDialMs);
    for (const QList<QVariant>& failure : failures) {
        QVERIFY2(!failure.at(0).toString().contains(QStringLiteral("Invalid socket descriptor")),
                 qPrintable(failure.at(0).toString()));
    }
    conn.disconnect();
}

void PgxlConnectionReconnectTest::sourceBindFailureMakesOneOsFallbackDial()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    int accepted = 0;
    QList<QPointer<QTcpSocket>> peers;
    connect(&server, &QTcpServer::newConnection, this, [&] {
        answerVersion(server, accepted, peers);
    });
    PgxlConnection conn;
    conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
    conn.testForceSourceBindFailureOnce();
    QSignalSpy attempts(&conn, &PgxlConnection::reconnectAttempt);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), server.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
    QCOMPARE(accepted, 1);
    QCOMPARE(attempts.size(), 0);
    QVERIFY(!conn.testReconnectPending());
}

void PgxlConnectionReconnectTest::reconnectAttemptConsumerMayDisconnect()
{
    const quint16 deadPort = closedLoopbackPort();
    QVERIFY(deadPort != 0);
    PgxlConnection conn;
    conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
    QSignalSpy attempts(&conn, &PgxlConnection::reconnectAttempt);
    connect(&conn, &PgxlConnection::reconnectAttempt, &conn,
            [&conn](int, int) { conn.disconnect(); }, Qt::DirectConnection);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), deadPort);
    QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, kRefusedDialMs);
    QTcpServer oldEndpoint;
    QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, deadPort));
    QSignalSpy staleConnections(&oldEndpoint, &QTcpServer::newConnection);
    QTest::qWait(kRetryObservationMs);
    QCOMPARE(staleConnections.size(), 0);
    QVERIFY(!conn.testReconnectPending());
}

// R-R3-47: turning automatic retry off drops a retry already pending.
void PgxlConnectionReconnectTest::autoReconnectTurnedOffDropsPendingRetry()
{
    const quint16 deadPort = closedLoopbackPort();
    QVERIFY(deadPort != 0);
    PgxlConnection conn;
    conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
    QSignalSpy attempts(&conn, &PgxlConnection::reconnectAttempt);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), deadPort);
    QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, kRefusedDialMs);
    AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("False"));
    conn.applyConnectionSettings();
    QVERIFY(!conn.testReconnectPending());
    QTcpServer oldEndpoint;
    QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, deadPort));
    QSignalSpy staleConnections(&oldEndpoint, &QTcpServer::newConnection);
    QTest::qWait(kRetryObservationMs + 3 * kBackoffUnitMs);
    QCOMPARE(staleConnections.size(), 0);
    QCOMPARE(attempts.size(), 1);
}

void PgxlConnectionReconnectTest::lateFailureFromReplacedAttemptCannotRetireReplacement()
{
    QTcpServer oldEndpoint;
    QTcpServer replacement;
    QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, 0));
    QVERIFY(replacement.listen(QHostAddress::LocalHost, 0));
    QPointer<QTcpSocket> heldOldPeer;
    connect(&oldEndpoint, &QTcpServer::newConnection, this, [&] {
        heldOldPeer = oldEndpoint.nextPendingConnection();
    });
    int replacementConnections = 0;
    QList<QPointer<QTcpSocket>> peers;
    connect(&replacement, &QTcpServer::newConnection, this, [&] {
        answerVersion(replacement, replacementConnections, peers);
    });

    PgxlConnection conn;
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), oldEndpoint.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(!heldOldPeer.isNull(), 2000);
    const quint64 oldAttempt = conn.testActiveSocketAttemptGeneration();
    QVERIFY(oldAttempt != 0);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), replacement.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);

    QSignalSpy disconnected(&conn, &PgxlConnection::disconnected);
    QSignalSpy retries(&conn, &PgxlConnection::reconnectAttempt);
    conn.testInjectFailureForSocketAttempt(oldAttempt);
    QCoreApplication::processEvents();
    QCOMPARE(disconnected.size(), 0);
    QCOMPARE(retries.size(), 0);
    QVERIFY(conn.isConnected());
    QCOMPARE(conn.peerPort(), replacement.serverPort());
}

namespace {
QStringList* g_capturedLog = nullptr;
void captureLog(QtMsgType, const QMessageLogContext&, const QString& message)
{
    if (g_capturedLog != nullptr) { g_capturedLog->append(message); }
}
} // namespace

// 2026-10-11: the amp closed its control link on a clock and the log said
// only "The remote host closed the connection". The last frames each way
// are now written with it.
void PgxlConnectionReconnectTest::closeByTheAmpLogsTheLastFrames()
{
    AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("False"));
    QTcpServer amp;
    QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
    PgxlConnection conn;
    QSignalSpy connected(&conn, &PgxlConnection::connected);
    QSignalSpy disconnected(&conn, &PgxlConnection::disconnected);
    conn.connectToPgxl(QStringLiteral("127.0.0.1"), amp.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(amp.hasPendingConnections(), 3000);
    QPointer<QTcpSocket> peer = amp.nextPendingConnection();
    QVERIFY(peer);
    peer->write("V3.9.9\n");
    peer->flush();
    QTRY_COMPARE_WITH_TIMEOUT(connected.count(), 1, 3000);

    // More replies than the trace holds: the oldest fall off the front.
    for (int i = 0; i < 40; ++i) {
        peer->write(QByteArrayLiteral("S0|status state=IDLE n=") + QByteArray::number(i) + '\n');
    }
    peer->flush();
    QTRY_VERIFY_WITH_TIMEOUT(conn.recentWireForTesting().contains(
                                 QStringLiteral("RX S0|status state=IDLE n=39")), 3000);
    const QStringList held = conn.recentWireForTesting();
    QCOMPARE(held.size(), 32);
    QVERIFY(!held.contains(QStringLiteral("RX V3.9.9")));

    // One `status` in flight (2026-10-12): the amp answers the handshake's
    // (C1 is `info`, C2 `status`), and the poll's next one is the TX frame
    // the trace must hold.
    peer->write("R2|0|state=IDLE\n");
    peer->flush();
    QTRY_VERIFY_WITH_TIMEOUT(!conn.recentWireForTesting().filter(QRegularExpression(
                                 QStringLiteral("^TX C\\d+\\|status$"))).isEmpty(), 3000);

    QStringList log;
    g_capturedLog = &log;
    const QtMessageHandler previous = qInstallMessageHandler(captureLog);
    peer->close();   // the amp closes the link
    const bool dropped = disconnected.count() > 0 || disconnected.wait(3000);
    qInstallMessageHandler(previous);
    g_capturedLog = nullptr;
    QVERIFY(dropped);

    const QString all = log.join(QLatin1Char('\n'));
    QVERIFY2(all.contains(QStringLiteral("PGXL link closed")), qPrintable(all));
    QVERIFY2(all.contains(QStringLiteral("after the TCP connect; last 32 frames")), qPrintable(all));
    QVERIFY2(all.contains(QStringLiteral("RX S0|status state=IDLE n=39")), qPrintable(all));
    QVERIFY2(all.contains(QRegularExpression(
                 QStringLiteral("PGXL link trace -\\d+ ms TX C\\d+\\|status"))),
             qPrintable(all));

    // A close the operator asked for is not traced.
    PgxlConnection second;
    QSignalSpy secondConnected(&second, &PgxlConnection::connected);
    second.connectToPgxl(QStringLiteral("127.0.0.1"), amp.serverPort());
    QTRY_VERIFY_WITH_TIMEOUT(amp.hasPendingConnections(), 3000);
    QPointer<QTcpSocket> secondPeer = amp.nextPendingConnection();
    QVERIFY(secondPeer);
    secondPeer->write("V3.9.9\n");
    secondPeer->flush();
    QTRY_COMPARE_WITH_TIMEOUT(secondConnected.count(), 1, 3000);
    QStringList quietLog;
    g_capturedLog = &quietLog;
    qInstallMessageHandler(captureLog);
    second.disconnect();
    QTest::qWait(100);
    qInstallMessageHandler(previous);
    g_capturedLog = nullptr;
    QVERIFY2(!quietLog.join(QLatin1Char('\n')).contains(QStringLiteral("PGXL link trace")),
             qPrintable(quietLog.join(QLatin1Char('\n'))));
}

QTEST_GUILESS_MAIN(PgxlConnectionReconnectTest)
#include "tst_pgxl_connection_reconnect.moc"
