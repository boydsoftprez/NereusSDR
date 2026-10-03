// =================================================================
// tests/tst_tgxl_connection_reconnect.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native lifecycle regression. AetherSDR's owned single-shot
// reconnect timer is the structural precedent; endpoint generation,
// exponential backoff, source selection, and OS-route fallback are
// NereusSDR additions. No Thetis TGXL connection manager exists to port.
// =================================================================

#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPointer>

#include "core/AppSettings.h"
#include "core/TgxlConnection.h"
#include "models/TunerModel.h"

using namespace NereusSDR;

namespace {

constexpr int kBackoffUnitMs = 200;
constexpr int kRetryObservationMs = 500;

quint16 closedLoopbackPort()
{
    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost, 0)) { return 0; }
    const quint16 port = reservation.serverPort();
    reservation.close();
    return port;
}

void answerVersion(QTcpServer& server, int& accepted,
                   QList<QPointer<QTcpSocket>>& peers)
{
    while (server.hasPendingConnections()) {
        QTcpSocket* peer = server.nextPendingConnection();
        peers.push_back(peer);
        ++accepted;
        peer->write("V1.2.17\n");
        peer->flush();
    }
}

} // namespace

class TgxlConnectionReconnectTest final : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().setValue(QStringLiteral("TGXL_AutoReconnect"),
                                         QStringLiteral("True"));
    }

    void endpointReplacementCancelsCapturedRetry()
    {
        const quint16 oldPort = closedLoopbackPort();
        QVERIFY(oldPort != 0);
        QTcpServer replacement;
        QVERIFY(replacement.listen(QHostAddress::LocalHost, 0));
        int replacementConnections = 0;
        QList<QPointer<QTcpSocket>> replacementPeers;
        connect(&replacement, &QTcpServer::newConnection, this, [&] {
            answerVersion(replacement, replacementConnections, replacementPeers);
        });

        TgxlConnection conn;
        conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
        QSignalSpy attempts(&conn, &TgxlConnection::reconnectAttempt);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), oldPort);
        QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, 2000);
        QTcpServer oldEndpoint;
        QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, oldPort));
        QSignalSpy staleConnections(&oldEndpoint, &QTcpServer::newConnection);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), replacement.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
        QCOMPARE(replacementConnections, 1);
        QTest::qWait(kRetryObservationMs);
        QCOMPARE(staleConnections.size(), 0);
        QCOMPARE(replacementConnections, 1);
        QCOMPARE(conn.peerPort(), replacement.serverPort());
        QVERIFY(!conn.testReconnectPending());
    }

    void explicitDisconnectCancelsCapturedRetry()
    {
        const quint16 oldPort = closedLoopbackPort();
        QVERIFY(oldPort != 0);
        TgxlConnection conn;
        conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
        QSignalSpy attempts(&conn, &TgxlConnection::reconnectAttempt);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), oldPort);
        QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, 2000);
        conn.disconnect();

        QTcpServer oldEndpoint;
        QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, oldPort));
        QSignalSpy staleConnections(&oldEndpoint, &QTcpServer::newConnection);
        QTest::qWait(kRetryObservationMs);
        QCOMPARE(staleConnections.size(), 0);
        QVERIFY(!conn.testReconnectPending());
    }

    void connectErrorAndDisconnectLeaveOneOwnedRetry()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        int accepted = 0;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            while (server.hasPendingConnections()) {
                QTcpSocket* peer = server.nextPendingConnection();
                ++accepted;
                // Close before sending V so the client receives both the
                // RemoteHostClosed error and disconnected notifications for
                // one failed handshake.
                peer->disconnectFromHost();
                peer->deleteLater();
            }
        });
        TgxlConnection conn;
        conn.testSetReconnectBackoffUnitMs(1000);
        QSignalSpy attempts(&conn, &TgxlConnection::reconnectAttempt);
        QSignalSpy failures(&conn, &TgxlConnection::connectionFailed);
        QSignalSpy disconnected(&conn, &TgxlConnection::disconnected);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(accepted, 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(disconnected.size(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, 2000);
        QCOMPARE(attempts.size(), 1);
        QVERIFY(conn.testReconnectPending());
    }

    void retryRetiresTheFailedQtSocket()
    {
        const quint16 deadPort = closedLoopbackPort();
        QVERIFY(deadPort != 0);

        TgxlConnection conn;
        conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
        QSignalSpy attempts(&conn, &TgxlConnection::reconnectAttempt);
        QSignalSpy failures(&conn, &TgxlConnection::connectionFailed);

        conn.connectToTgxl(QStringLiteral("127.0.0.1"), deadPort);
        QTRY_VERIFY_WITH_TIMEOUT(conn.testActiveSocketAttemptGeneration() != 0,
                                 2000);
        const quint64 failedAttempt =
            conn.testActiveSocketAttemptGeneration();
        QPointer<QTcpSocket> failedSocket = conn.testSocketForTesting();
        QVERIFY(!failedSocket.isNull());

        QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(
            conn.testActiveSocketAttemptGeneration() != failedAttempt, 2000);

        // Qt's asynchronous connect timeout leaves QAbstractSocket in public
        // UnconnectedState without resetting its internal socket engine. A
        // later bind() on that same object can therefore report
        // InvalidSocketError. A physical retry must own a fresh QTcpSocket;
        // localhost refusal keeps this regression bounded while enforcing the
        // lifecycle invariant required by the real timeout path.
        QTRY_VERIFY_WITH_TIMEOUT(failedSocket.isNull(), 2000);
        QVERIFY(conn.testSocketForTesting() != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(failures.size() >= 2, 2000);
        for (const QList<QVariant>& failure : failures) {
            QVERIFY2(!failure.at(0).toString().contains(
                         QStringLiteral("Invalid socket descriptor")),
                     qPrintable(failure.at(0).toString()));
        }
    }

    void sourceBindErrorReentryMakesOneOsFallbackDial()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        int accepted = 0;
        QList<QPointer<QTcpSocket>> peers;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            answerVersion(server, accepted, peers);
        });
        TgxlConnection conn;
        conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
        conn.testForceSourceBindFailureOnce();
        QSignalSpy attempts(&conn, &TgxlConnection::reconnectAttempt);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
        QCOMPARE(accepted, 1);
        QCOMPARE(attempts.size(), 0);
        QVERIFY(!conn.testReconnectPending());
    }

    void successfulHandshakeClearsRetryAndAttemptState()
    {
        const quint16 deadPort = closedLoopbackPort();
        QVERIFY(deadPort != 0);
        TgxlConnection conn;
        conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
        QSignalSpy attempts(&conn, &TgxlConnection::reconnectAttempt);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), deadPort);
        QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, 2000);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        int accepted = 0;
        QList<QPointer<QTcpSocket>> peers;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            answerVersion(server, accepted, peers);
        });
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
        QTest::qWait(kRetryObservationMs);
        QCOMPARE(accepted, 1);
        QVERIFY(conn.isConnected());
        QCOMPARE(conn.peerPort(), server.serverPort());
        QCOMPARE(conn.reconnectCount(), 0);
        QVERIFY(!conn.testReconnectPending());
    }

    void reconnectAttemptConsumerMayReplaceEndpoint()
    {
        const quint16 deadPort = closedLoopbackPort();
        QVERIFY(deadPort != 0);
        QTcpServer replacement;
        QVERIFY(replacement.listen(QHostAddress::LocalHost, 0));
        int accepted = 0;
        QList<QPointer<QTcpSocket>> peers;
        connect(&replacement, &QTcpServer::newConnection, this, [&] {
            answerVersion(replacement, accepted, peers);
        });
        TgxlConnection conn;
        conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
        connect(&conn, &TgxlConnection::reconnectAttempt, &conn,
                [&conn, &replacement](int, int) {
            conn.connectToTgxl(QStringLiteral("127.0.0.1"), replacement.serverPort());
        }, Qt::DirectConnection);

        conn.connectToTgxl(QStringLiteral("127.0.0.1"), deadPort);
        QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
        QTest::qWait(kRetryObservationMs);
        QCOMPARE(accepted, 1);
        QCOMPARE(conn.peerPort(), replacement.serverPort());
        QVERIFY(!conn.testReconnectPending());
    }

    void reconnectAttemptConsumerMayDisconnect()
    {
        const quint16 deadPort = closedLoopbackPort();
        QVERIFY(deadPort != 0);
        TgxlConnection conn;
        conn.testSetReconnectBackoffUnitMs(kBackoffUnitMs);
        QSignalSpy attempts(&conn, &TgxlConnection::reconnectAttempt);
        connect(&conn, &TgxlConnection::reconnectAttempt,
                &conn, [&conn](int, int) { conn.disconnect(); }, Qt::DirectConnection);

        conn.connectToTgxl(QStringLiteral("127.0.0.1"), deadPort);
        QTRY_COMPARE_WITH_TIMEOUT(attempts.size(), 1, 2000);
        QTcpServer oldEndpoint;
        QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, deadPort));
        QSignalSpy staleConnections(&oldEndpoint, &QTcpServer::newConnection);
        QTest::qWait(kRetryObservationMs);
        QCOMPARE(staleConnections.size(), 0);
        QVERIFY(!conn.testReconnectPending());
    }

    void connectionFailureConsumerMayReplaceEndpoint()
    {
        const quint16 deadPort = closedLoopbackPort();
        QVERIFY(deadPort != 0);
        QTcpServer replacement;
        QVERIFY(replacement.listen(QHostAddress::LocalHost, 0));
        int accepted = 0;
        QList<QPointer<QTcpSocket>> peers;
        connect(&replacement, &QTcpServer::newConnection, this, [&] {
            answerVersion(replacement, accepted, peers);
        });

        TgxlConnection conn;
        bool replaced = false;
        connect(&conn, &TgxlConnection::connectionFailed, &conn,
                [&conn, &replacement, &replaced](const QString&) {
            if (replaced) {
                return;
            }
            replaced = true;
            conn.connectToTgxl(QStringLiteral("127.0.0.1"),
                               replacement.serverPort());
        }, Qt::DirectConnection);

        conn.connectToTgxl(QStringLiteral("127.0.0.1"), deadPort);
        QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
        QCOMPARE(accepted, 1);
        QCOMPARE(conn.peerPort(), replacement.serverPort());
        QVERIFY(!conn.testReconnectPending());
    }

    void connectionFailureConsumerMayDisconnect()
    {
        const quint16 deadPort = closedLoopbackPort();
        QVERIFY(deadPort != 0);
        TgxlConnection conn;
        QSignalSpy failures(&conn, &TgxlConnection::connectionFailed);
        connect(&conn, &TgxlConnection::connectionFailed, &conn,
                [&conn](const QString&) { conn.disconnect(); },
                Qt::DirectConnection);

        conn.connectToTgxl(QStringLiteral("127.0.0.1"), deadPort);
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 2000);

        QTcpServer oldEndpoint;
        QVERIFY(oldEndpoint.listen(QHostAddress::LocalHost, deadPort));
        QSignalSpy staleConnections(&oldEndpoint, &QTcpServer::newConnection);
        QTest::qWait(kRetryObservationMs);
        QCOMPARE(staleConnections.size(), 0);
        QVERIFY(!conn.testReconnectPending());
    }

    void replacingConnectedEndpointPublishesPendingDisconnect()
    {
        QTcpServer firstEndpoint;
        QTcpServer replacement;
        QVERIFY(firstEndpoint.listen(QHostAddress::LocalHost, 0));
        QVERIFY(replacement.listen(QHostAddress::LocalHost, 0));
        int accepted = 0;
        QList<QPointer<QTcpSocket>> peers;
        connect(&firstEndpoint, &QTcpServer::newConnection, this, [&] {
            answerVersion(firstEndpoint, accepted, peers);
        });
        QPointer<QTcpSocket> heldReplacement;
        connect(&replacement, &QTcpServer::newConnection, this, [&] {
            heldReplacement = replacement.nextPendingConnection();
        });

        TgxlConnection conn;
        TunerModel tuner;
        tuner.bindConnection(&conn);
        QList<bool> publishedStates;
        connect(&tuner, &TunerModel::directConnectionChanged, this, [&] {
            publishedStates.push_back(tuner.hasDirectConnection());
        });
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), firstEndpoint.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(tuner.hasDirectConnection(), 2000);
        QCOMPARE(publishedStates, QList<bool>{true});

        conn.connectToTgxl(QStringLiteral("127.0.0.1"), replacement.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(!heldReplacement.isNull(), 2000);
        QVERIFY(!tuner.hasDirectConnection());
        QCOMPARE(publishedStates, (QList<bool>{true, false}));

        heldReplacement->write("V1.2.17\n");
        heldReplacement->flush();
        QTRY_VERIFY_WITH_TIMEOUT(tuner.hasDirectConnection(), 2000);
        QCOMPARE(publishedStates, (QList<bool>{true, false, true}));
        QCOMPARE(conn.peerPort(), replacement.serverPort());
    }

    void heldVersionFromReplacedAttemptCannotAdmitOldEndpoint()
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
        QList<QPointer<QTcpSocket>> replacementPeers;
        connect(&replacement, &QTcpServer::newConnection, this, [&] {
            answerVersion(replacement, replacementConnections, replacementPeers);
        });

        TgxlConnection conn;
        QSignalSpy connected(&conn, &TgxlConnection::connected);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), oldEndpoint.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(!heldOldPeer.isNull(), 2000);
        const quint64 oldAttempt = conn.testActiveSocketAttemptGeneration();
        QVERIFY(oldAttempt != 0);

        conn.connectToTgxl(QStringLiteral("127.0.0.1"), replacement.serverPort());
        // Deterministically deliver the V callback that was already queued
        // for A before B's zero-delay dial gets its event-loop turn.
        conn.testInjectLineForSocketAttempt(QStringLiteral("V1.2.17"), oldAttempt);

        QTRY_COMPARE_WITH_TIMEOUT(replacementConnections, 1, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
        QCOMPARE(connected.size(), 1);
        QCOMPARE(conn.peerPort(), replacement.serverPort());
    }

    void disconnectRejectsHeldVersionFromCancelledAttempt()
    {
        QTcpServer endpoint;
        QVERIFY(endpoint.listen(QHostAddress::LocalHost, 0));
        QPointer<QTcpSocket> heldPeer;
        connect(&endpoint, &QTcpServer::newConnection, this, [&] {
            heldPeer = endpoint.nextPendingConnection();
        });

        TgxlConnection conn;
        QSignalSpy connected(&conn, &TgxlConnection::connected);
        QSignalSpy frames(&conn, &TgxlConnection::testFrameWrittenForTesting);
        QSignalSpy retries(&conn, &TgxlConnection::reconnectAttempt);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), endpoint.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(!heldPeer.isNull(), 2000);
        const quint64 cancelledAttempt = conn.testActiveSocketAttemptGeneration();
        QVERIFY(cancelledAttempt != 0);

        conn.disconnect();
        conn.testInjectLineForSocketAttempt(QStringLiteral("V1.2.17"),
                                            cancelledAttempt);
        QCoreApplication::processEvents();
        QCOMPARE(connected.size(), 0);
        QCOMPARE(frames.size(), 0);
        QCOMPARE(retries.size(), 0);
        QVERIFY(!conn.isConnected());
        QVERIFY(!conn.testReconnectPending());
    }

    void lateFailureFromReplacedAttemptCannotRetireReplacement()
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
        QList<QPointer<QTcpSocket>> replacementPeers;
        connect(&replacement, &QTcpServer::newConnection, this, [&] {
            answerVersion(replacement, replacementConnections, replacementPeers);
        });

        TgxlConnection conn;
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), oldEndpoint.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(!heldOldPeer.isNull(), 2000);
        const quint64 oldAttempt = conn.testActiveSocketAttemptGeneration();
        QVERIFY(oldAttempt != 0);
        conn.connectToTgxl(QStringLiteral("127.0.0.1"), replacement.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(conn.isConnected(), 2000);
        QCOMPARE(replacementConnections, 1);

        QSignalSpy disconnected(&conn, &TgxlConnection::disconnected);
        QSignalSpy retries(&conn, &TgxlConnection::reconnectAttempt);
        conn.testInjectFailureForSocketAttempt(oldAttempt);
        QCoreApplication::processEvents();
        QCOMPARE(disconnected.size(), 0);
        QCOMPARE(retries.size(), 0);
        QVERIFY(conn.isConnected());
        QCOMPARE(conn.peerPort(), replacement.serverPort());
        QVERIFY(!conn.testReconnectPending());
    }
};

QTEST_GUILESS_MAIN(TgxlConnectionReconnectTest)
#include "tst_tgxl_connection_reconnect.moc"
