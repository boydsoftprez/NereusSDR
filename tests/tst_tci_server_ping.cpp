// no-port-check: NereusSDR-original test file. Contains the string "Thetis"
// only as the literal WebSocket ping payload per TCI wire spec, not as a
// Thetis source port. No logic ported from TCIServer.cs or any upstream file.

// tests/tst_tci_server_ping.cpp  (NereusSDR)
// NereusSDR-original — no Thetis upstream port in this file.
//
// Phase 3J-1 Task 2.2: server-driven 20s ping with payload "Thetis".
// Verifies that after setPingIntervalMs(200), at least 2 pongs arrive
// from the client within 3 seconds, and each pong echoes the "Thetis"
// payload per RFC 6455.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include "core/TciServer.h"

using namespace NereusSDR;

class TestTciServerPing : public QObject {
    Q_OBJECT
private slots:
    void server_ping_drives_client_pong();
};

void TestTciServerPing::server_ping_drives_client_pong()
{
    // QWebSocket* must be registered so QSignalSpy can extract the pointer
    // from the queued signal parameter.
    qRegisterMetaType<QWebSocket*>("QWebSocket*");

    TciServer server(nullptr);
    QSignalSpy connectedSpy(&server, &TciServer::clientConnected);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(clientConnectedSpy.wait(2000));
    // connectedSpy may have already fired before we reach this call
    if (connectedSpy.count() < 1) {
        QVERIFY(connectedSpy.wait(500));
    }
    QVERIFY(connectedSpy.count() >= 1);

    auto* serverSideWs = connectedSpy.takeFirst().at(0).value<QWebSocket*>();
    QVERIFY(serverSideWs != nullptr);

    QSignalSpy pongSpy(serverSideWs, &QWebSocket::pong);
    server.setPingIntervalMs(200);  // 20000ms → 200ms for fast test

    // Process events for ~3s; expect ≥ 2 pongs (200ms interval, 3s window → ~15)
    QTest::qWait(3000);

    QVERIFY2(pongSpy.count() >= 2,
             qPrintable(QStringLiteral("Expected >=2 pongs in 3s, got %1").arg(pongSpy.count())));

    // Verify payload echoes "Thetis"
    for (int i = 0; i < pongSpy.count(); ++i) {
        const QByteArray payload = pongSpy.at(i).at(1).toByteArray();
        QCOMPARE(payload, QByteArrayLiteral("Thetis"));
    }

    client.close();
    server.stop();
}

// QTEST_GUILESS_MAIN provides QCoreApplication which is sufficient for
// QWebSocket full-duplex over a localhost socket (no GUI event loop needed).
// Using QTEST_GUILESS_MAIN to match the peer test tst_tci_server_lifecycle
// and avoid pulling in Qt6::Widgets.
QTEST_GUILESS_MAIN(TestTciServerPing)
#include "tst_tci_server_ping.moc"

#else
// WebSockets not available — test file must still compile and produce a
// no-op binary so CTest doesn't report a missing executable.
int main() { return 0; }
#endif // HAVE_WEBSOCKETS
