// no-port-check: NereusSDR-original observational transport regression.
#include <QHostAddress>
#include <QSignalSpy>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>
#include "core/session/SessionTransport.h"

using namespace NereusSDR;

class TestSessionTransportTelemetry : public QObject {
    Q_OBJECT
private slots:
    void binaryDeliveryDoesNotCountAsControlPayload()
    {
        QWebSocketServer listener(QStringLiteral("binary-metrics-test"),
                                  QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        auto* socket = new QWebSocket;
        WebSocketTransport client(socket, 16384);
        socket->open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(listener.serverPort())));
        QTRY_VERIFY(listener.hasPendingConnections());
        WebSocketTransport server(listener.nextPendingConnection(), 16384);
        QTRY_VERIFY(client.isOpen());

        QSignalSpy serverBinary(&server, &SessionTransport::binaryReceived);
        QSignalSpy clientBinary(&client, &SessionTransport::binaryReceived);
        const QByteArray outward = QByteArray::fromHex("004E534443FF80");
        const QByteArray inward = QByteArray::fromHex("004E534458FE81");
        QVERIFY(client.sendBinary(outward));
        QTRY_COMPARE(serverBinary.count(), 1);
        QCOMPARE(serverBinary.first().first().toByteArray(), outward);
        QVERIFY(server.sendBinary(inward));
        QTRY_COMPARE(clientBinary.count(), 1);
        QCOMPARE(clientBinary.first().first().toByteArray(), inward);
        QCOMPARE(client.telemetry()->acceptedPayloadBytes, quint64(0));
        QCOMPARE(client.telemetry()->receivedPayloadBytes, quint64(0));
        QCOMPARE(server.telemetry()->acceptedPayloadBytes, quint64(0));
        QCOMPARE(server.telemetry()->receivedPayloadBytes, quint64(0));

        QSignalSpy serverText(&server, &SessionTransport::textReceived);
        QSignalSpy clientText(&client, &SessionTransport::textReceived);
        const QByteArray request = QStringLiteral("request 音声").toUtf8();
        const QByteArray response = QStringLiteral("reply →").toUtf8();
        client.sendText(request);
        QTRY_COMPARE(serverText.count(), 1);
        QCOMPARE(serverText.first().first().toByteArray(), request);
        server.sendText(response);
        QTRY_COMPARE(clientText.count(), 1);
        QCOMPARE(clientText.first().first().toByteArray(), response);
        QCOMPARE(client.telemetry()->acceptedPayloadBytes, static_cast<quint64>(request.size()));
        QCOMPARE(client.telemetry()->receivedPayloadBytes, static_cast<quint64>(response.size()));
        QCOMPARE(server.telemetry()->acceptedPayloadBytes, static_cast<quint64>(response.size()));
        QCOMPARE(server.telemetry()->receivedPayloadBytes, static_cast<quint64>(request.size()));
    }

    void countsActualUtf8PayloadAndObservesExistingPong()
    {
        QWebSocketServer listener(QStringLiteral("metrics-test"),
                                  QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        auto* socket = new QWebSocket;
        WebSocketTransport client(socket, 16384);
        QVERIFY(!client.telemetry());
        socket->open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(listener.serverPort())));
        QTRY_VERIFY(listener.hasPendingConnections());
        WebSocketTransport server(listener.nextPendingConnection(), 16384);
        QTRY_VERIFY(client.isOpen());
        QVERIFY(client.telemetry());
        QVERIFY(!client.telemetry()->pongRttMs);
        const QByteArray outward = QStringLiteral("Core → GUI: 音声").toUtf8();
        QSignalSpy received(&server, &SessionTransport::textReceived);
        client.sendText(outward);
        QTRY_COMPARE(received.count(), 1);
        QCOMPARE(received.first().first().toByteArray(), outward);
        QCOMPARE(client.telemetry()->acceptedPayloadBytes,
                 static_cast<quint64>(outward.size()));
        QCOMPARE(server.telemetry()->receivedPayloadBytes,
                 static_cast<quint64>(outward.size()));
        QCOMPARE(client.telemetry()->receivedPayloadBytes, quint64(0));

        const QByteArray inward = QByteArrayLiteral("response");
        server.sendText(inward);
        QTRY_COMPARE(client.telemetry()->receivedPayloadBytes,
                     static_cast<quint64>(inward.size()));
        QSignalSpy pong(&client, &SessionTransport::pongReceived);
        client.ping();
        QTRY_COMPARE(pong.count(), 1);
        QVERIFY(client.telemetry()->pongRttMs);
        QVERIFY(client.telemetry()->pongAgeMs);
        // Ping/pong frames are not counted as application text payload.
        QCOMPARE(client.telemetry()->acceptedPayloadBytes,
                 static_cast<quint64>(outward.size()));
        QCOMPARE(client.telemetry()->receivedPayloadBytes,
                 static_cast<quint64>(inward.size()));
        client.closeLink(QStringLiteral("test complete"));
        QTRY_VERIFY(!client.isOpen());
        QVERIFY(!client.telemetry());
    }
};

QTEST_GUILESS_MAIN(TestSessionTransportTelemetry)
#include "tst_session_transport_telemetry.moc"
