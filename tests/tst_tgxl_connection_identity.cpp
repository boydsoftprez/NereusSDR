// =================================================================
// tests/tst_tgxl_connection_identity.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native identity-admission regression. Native `info` grammar is
// grounded in captures/flex-tgxl-direct-NOTES.md:67-93. No RF command or
// hardware is used; loopback peers exercise the production socket callbacks.
// =================================================================

#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPointer>

#include "core/AppSettings.h"
#include "core/TgxlConnection.h"

using namespace NereusSDR;

namespace {

quint32 sequenceFor(const QSignalSpy& frames, const QString& command)
{
    const QRegularExpression expression(
        QStringLiteral("^C(\\d+)\\|%1$").arg(QRegularExpression::escape(command)));
    for (const auto& arguments : frames) {
        const auto match = expression.match(arguments.first().toString());
        if (match.hasMatch()) {
            return match.captured(1).toUInt();
        }
    }
    return 0;
}

QPointer<QTcpSocket> acceptPeer(QTcpServer& server)
{
    QPointer<QTcpSocket> peer;
    QElapsedTimer wait;
    wait.start();
    while (peer.isNull() && wait.elapsed() < 2000) {
        if (server.waitForNewConnection(10)) {
            peer = server.nextPendingConnection();
        }
        QCoreApplication::processEvents();
    }
    return peer;
}

void writeLine(QTcpSocket* peer, const QString& line)
{
    QVERIFY(peer);
    QCOMPARE(peer->write((line + QLatin1Char('\n')).toUtf8()), line.size() + 1LL);
    QVERIFY(peer->flush());
}

} // namespace

class TgxlConnectionIdentityTest final : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().setValue(QStringLiteral("TGXL_AutoReconnect"),
                                         QStringLiteral("True"));
    }

    void defaultLocalDirectStillAdmitsVersionBanner()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        TgxlConnection connection;
        QSignalSpy connected(&connection, &TgxlConnection::connected);
        connection.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        const auto peer = acceptPeer(server);
        QVERIFY(peer);
        writeLine(peer, QStringLiteral("V1.2.17"));
        QTRY_COMPARE_WITH_TIMEOUT(connected.size(), 1, 2000);
        QVERIFY(connection.isConnected());
    }

    void changingIdentityPolicyRetiresLiveLegacyConnection()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        TgxlConnection connection;
        QSignalSpy disconnected(&connection, &TgxlConnection::disconnected);
        connection.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        const auto peer = acceptPeer(server);
        QVERIFY(peer);
        writeLine(peer, QStringLiteral("V1.2.17"));
        QTRY_VERIFY_WITH_TIMEOUT(connection.isConnected(), 2000);

        connection.setIdentityAdmissionRequired(true);
        QCOMPARE(disconnected.size(), 1);
        QVERIFY(!connection.isConnected());
        QCOMPARE(connection.socketAttemptToken(), quint64(0));
        QVERIFY(connection.identityAdmissionRequired());
    }

    void optInCorrelatesInfoAndWaitsForCurrentApproval()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        TgxlConnection connection;
        connection.setIdentityAdmissionRequired(true);
        QSignalSpy connected(&connection, &TgxlConnection::connected);
        QSignalSpy progress(&connection, &TgxlConnection::identityProtocolProgress);
        QSignalSpy info(&connection, &TgxlConnection::nativeInfoReceived);
        QSignalSpy state(&connection, &TgxlConnection::stateUpdated);
        QSignalSpy status(&connection, &TgxlConnection::statusUpdated);
        QSignalSpy frames(&connection, &TgxlConnection::testFrameWrittenForTesting);

        connection.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        const auto peer = acceptPeer(server);
        QVERIFY(peer);
        writeLine(peer, QStringLiteral("V1.2.17"));
        QTRY_COMPARE_WITH_TIMEOUT(progress.size(), 1, 2000);
        QVERIFY(!connection.isConnected());
        QCOMPARE(connected.size(), 0);
        const quint64 token = progress.first().at(0).toULongLong();
        QVERIFY(token != 0);
        QCOMPARE(progress.first().at(1).toString(), QStringLiteral("127.0.0.1"));
        QCOMPARE(progress.first().at(2).toUInt(), uint(server.serverPort()));
        QCOMPARE(progress.first().at(3).toString(), QStringLiteral("1.2.17"));

        QTRY_VERIFY_WITH_TIMEOUT(sequenceFor(frames, QStringLiteral("info")) != 0, 2000);
        const quint32 infoSequence = sequenceFor(frames, QStringLiteral("info"));
        QCOMPARE(sequenceFor(frames, QStringLiteral("status")), quint32(0));
        writeLine(peer, QStringLiteral("S0|state tuning=1 relayC1=48"));
        QCoreApplication::processEvents();
        QCOMPARE(state.size(), 0);
        writeLine(peer, QStringLiteral("R%1|0|info serial=wrong-sequence version=0 nickname=no")
                            .arg(infoSequence + 1));
        QTest::qWait(20);
        QCOMPARE(info.size(), 0);

        writeLine(peer, QStringLiteral(
            "R%1|0|info serial=241288-1 version=1.2.17 nickname=Tuner_Genius_XL 3way=1")
                            .arg(infoSequence));
        QTRY_COMPARE_WITH_TIMEOUT(info.size(), 1, 2000);
        const TgxlIdentityInfo result = qvariant_cast<TgxlIdentityInfo>(
            info.first().first());
        QCOMPARE(result.socketAttemptToken, token);
        QCOMPARE(result.peerAddress, QStringLiteral("127.0.0.1"));
        QCOMPARE(result.peerPort, server.serverPort());
        QCOMPARE(result.serial, QStringLiteral("241288-1"));
        QCOMPARE(result.version, QStringLiteral("1.2.17"));
        QCOMPARE(result.nickname, QStringLiteral("Tuner_Genius_XL"));
        QVERIFY(!connection.isConnected());
        QCOMPARE(status.size(), 0);

        QVERIFY(connection.admitIdentity(token, QStringLiteral("241288-1")));
        QTRY_COMPARE_WITH_TIMEOUT(connected.size(), 1, 2000);
        QVERIFY(connection.isConnected());
        QCOMPARE(status.size(), 1);
        const auto admitted = status.first().first()
                                  .value<QMap<QString, QString>>();
        QCOMPARE(admitted.value(QStringLiteral("serial_num")),
                 QStringLiteral("241288-1"));
        QCOMPARE(admitted.value(QStringLiteral("3way")), QStringLiteral("1"));
        QTRY_VERIFY_WITH_TIMEOUT(sequenceFor(frames, QStringLiteral("status")) != 0, 2000);
    }

    void mismatchedSerialCannotAdmitOrSendActuatorCommand()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        TgxlConnection connection;
        connection.setIdentityAdmissionRequired(true);
        QSignalSpy failures(&connection, &TgxlConnection::identityAdmissionFailed);
        QSignalSpy frames(&connection, &TgxlConnection::testFrameWrittenForTesting);
        connection.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        const auto peer = acceptPeer(server);
        QVERIFY(peer);
        writeLine(peer, QStringLiteral("V1.2.17"));
        QTRY_VERIFY_WITH_TIMEOUT(sequenceFor(frames, QStringLiteral("info")) != 0, 2000);
        const quint64 token = connection.socketAttemptToken();
        writeLine(peer, QStringLiteral(
            "R%1|0|info serial=observed version=1.2.17 nickname=Tuner_Genius_XL")
                            .arg(sequenceFor(frames, QStringLiteral("info"))));
        QTRY_COMPARE_WITH_TIMEOUT(connection.identityInfo().serial,
                                  QStringLiteral("observed"), 2000);

        const int provisionalFrames = frames.size();
        connection.adjustRelay(1, 1);
        QCOMPARE(connection.sendCommand(QStringLiteral("autotune")), quint32(0));
        QCOMPARE(connection.enableKeepalive(), quint32(0));
        QCOMPARE(connection.ping(), quint32(0));
        QCOMPARE(connection.readSetup(), quint32(0));
        QCOMPARE(connection.writeSetup({{QStringLiteral("operate"),
                                         QStringLiteral("1")}}), quint32(0));
        QCOMPARE(connection.readIfconf(), quint32(0));
        QCOMPARE(connection.writeIfconf(QStringLiteral("192.0.2.1"),
                                         QStringLiteral("255.255.255.0"),
                                         QStringLiteral("192.0.2.254"), false), quint32(0));
        QCOMPARE(connection.save(), quint32(0));
        QCOMPARE(frames.size(), provisionalFrames);
        QVERIFY(!connection.admitIdentity(token, QStringLiteral("expected")));
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 2000);
        QVERIFY(!connection.isConnected());
    }

    void replacementAndDisconnectRetireInfoApproval()
    {
        QTcpServer oldServer;
        QTcpServer replacement;
        QVERIFY(oldServer.listen(QHostAddress::LocalHost, 0));
        QVERIFY(replacement.listen(QHostAddress::LocalHost, 0));
        TgxlConnection connection;
        connection.setIdentityAdmissionRequired(true);
        QSignalSpy progress(&connection, &TgxlConnection::identityProtocolProgress);
        QSignalSpy info(&connection, &TgxlConnection::nativeInfoReceived);
        QSignalSpy frames(&connection, &TgxlConnection::testFrameWrittenForTesting);

        connection.connectToTgxl(QStringLiteral("127.0.0.1"), oldServer.serverPort());
        const auto oldPeer = acceptPeer(oldServer);
        QVERIFY(oldPeer);
        writeLine(oldPeer, QStringLiteral("V1.2.17"));
        QTRY_COMPARE_WITH_TIMEOUT(progress.size(), 1, 2000);
        const quint64 oldToken = progress.first().first().toULongLong();
        QTRY_VERIFY_WITH_TIMEOUT(sequenceFor(frames, QStringLiteral("info")) != 0, 2000);
        const quint32 oldInfoSequence = sequenceFor(frames, QStringLiteral("info"));

        frames.clear();
        connection.connectToTgxl(QStringLiteral("127.0.0.1"), replacement.serverPort());
        const auto replacementPeer = acceptPeer(replacement);
        QVERIFY(replacementPeer);
        writeLine(replacementPeer, QStringLiteral("V1.2.18"));
        QTRY_COMPARE_WITH_TIMEOUT(progress.size(), 2, 2000);
        const quint64 replacementToken = progress.last().first().toULongLong();
        QVERIFY(replacementToken != oldToken);
        connection.testInjectLineForSocketAttempt(
            QStringLiteral("R%1|0|info serial=old version=1 nickname=old")
                .arg(oldInfoSequence), oldToken);
        QCoreApplication::processEvents();
        QCOMPARE(info.size(), 0);
        QVERIFY(!connection.admitIdentity(oldToken, QStringLiteral("old")));

        QTRY_VERIFY_WITH_TIMEOUT(sequenceFor(frames, QStringLiteral("info")) != 0, 2000);
        writeLine(replacementPeer, QStringLiteral(
            "R%1|0|info serial=new version=1.2.18 nickname=current")
                .arg(sequenceFor(frames, QStringLiteral("info"))));
        QTRY_COMPARE_WITH_TIMEOUT(info.size(), 1, 2000);
        connection.disconnect();
        QVERIFY(!connection.admitIdentity(replacementToken, QStringLiteral("new")));
        QVERIFY(!connection.isConnected());
    }

    void automaticRetryUsesFreshAttemptAndApproval()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        TgxlConnection connection;
        connection.setIdentityAdmissionRequired(true);
        connection.testSetReconnectBackoffUnitMs(20);
        QSignalSpy progress(&connection, &TgxlConnection::identityProtocolProgress);
        QSignalSpy frames(&connection, &TgxlConnection::testFrameWrittenForTesting);
        connection.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        auto firstPeer = acceptPeer(server);
        QVERIFY(firstPeer);
        writeLine(firstPeer, QStringLiteral("V1.2.17"));
        QTRY_COMPARE_WITH_TIMEOUT(progress.size(), 1, 2000);
        const quint64 firstToken = progress.first().first().toULongLong();
        firstPeer->disconnectFromHost();
        QTRY_VERIFY_WITH_TIMEOUT(firstPeer->state() == QAbstractSocket::UnconnectedState,
                                 2000);

        auto secondPeer = acceptPeer(server);
        QVERIFY(secondPeer);
        frames.clear();
        writeLine(secondPeer, QStringLiteral("V1.2.17"));
        QTRY_COMPARE_WITH_TIMEOUT(progress.size(), 2, 2000);
        const quint64 secondToken = progress.last().first().toULongLong();
        QVERIFY(secondToken != firstToken);
        QVERIFY(!connection.admitIdentity(firstToken, QStringLiteral("241288-1")));
        QVERIFY(!connection.isConnected());
    }

    void invalidInfoResponseCannotAdmit_data()
    {
        QTest::addColumn<QString>("response");
        QTest::newRow("nonzero response")
            << QStringLiteral("R%1|1|info serial=241288-1");
        QTest::newRow("missing serial")
            << QStringLiteral("R%1|0|info version=1.2.17 nickname=Tuner_Genius_XL");
    }

    void invalidInfoResponseCannotAdmit()
    {
        QFETCH(QString, response);
        AppSettings::instance().setValue(QStringLiteral("TGXL_AutoReconnect"),
                                         QStringLiteral("False"));
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        TgxlConnection connection;
        connection.setIdentityAdmissionRequired(true);
        QSignalSpy failures(&connection, &TgxlConnection::identityAdmissionFailed);
        QSignalSpy frames(&connection, &TgxlConnection::testFrameWrittenForTesting);
        connection.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        const auto peer = acceptPeer(server);
        QVERIFY(peer);
        writeLine(peer, QStringLiteral("V1.2.17"));
        QTRY_VERIFY_WITH_TIMEOUT(sequenceFor(frames, QStringLiteral("info")) != 0, 2000);
        writeLine(peer, response.arg(sequenceFor(frames, QStringLiteral("info"))));
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 1000);
        QVERIFY(!connection.isConnected());
        QVERIFY(connection.identityInfo().serial.isEmpty());
    }

    void identityTimeoutDoesNotAdmit_data()
    {
        QTest::addColumn<bool>("sendVersion");
        QTest::newRow("TCP without V") << false;
        QTest::newRow("V without info") << true;
    }

    void identityTimeoutDoesNotAdmit()
    {
        QFETCH(bool, sendVersion);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        TgxlConnection connection;
        connection.setIdentityAdmissionRequired(true);
        connection.testSetIdentityTimeoutMs(30);
        QSignalSpy failures(&connection, &TgxlConnection::identityAdmissionFailed);
        connection.connectToTgxl(QStringLiteral("127.0.0.1"), server.serverPort());
        const auto peer = acceptPeer(server);
        QVERIFY(peer);
        if (sendVersion) {
            writeLine(peer, QStringLiteral("V1.2.17"));
        }
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 1000);
        QVERIFY(!connection.isConnected());
    }
};

QTEST_GUILESS_MAIN(TgxlConnectionIdentityTest)
#include "tst_tgxl_connection_identity.moc"
