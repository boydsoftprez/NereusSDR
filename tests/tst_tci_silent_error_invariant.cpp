// no-port-check: NereusSDR-original integration test. The string "Thetis"
// only appears as Sweep B / TCI-protocol context in comments below; no logic
// ported from TCIServer.cs.
//
// Phase 3J-1 Task 3.2: silent-error invariant.
// Verifies that an unknown command produces ZERO outbound traffic from the
// server — no response, no notification, no broadcast. This is the core
// design-doc §4.1 / Sweep B invariant that ESDR3/SunSDR/WSJT-X clients
// rely on to distinguish "command rejected" from "command in flight".

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include "core/TciServer.h"

using namespace NereusSDR;

class TestTciSilentErrorInvariant : public QObject {
    Q_OBJECT
private slots:
    void unknown_command_produces_no_outbound_traffic();
};

void TestTciSilentErrorInvariant::unknown_command_produces_no_outbound_traffic()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy clientTextSpy(&client, &QWebSocket::textMessageReceived);
    QSignalSpy clientBinarySpy(&client, &QWebSocket::binaryMessageReceived);

    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(clientConnectedSpy.wait(2000));

    // Drain the post-connect init burst. Phase 4 Tasks 4.1+4.2 build ~98
    // wire frames via TciProtocol::buildInitBurst(), pushed onto the
    // session send queue by TciServer::onNewConnection. The silent-error
    // invariant applies to UNKNOWN COMMANDS specifically, NOT to the
    // normal post-connect greeting.
    QTest::qWait(200);  // generous for ~98 small text frames over loopback
    const int initBurstCount = clientTextSpy.count();
    QVERIFY2(initBurstCount > 0,
             qPrintable(QStringLiteral("Expected init burst to deliver >=1 line, got %1")
                            .arg(initBurstCount)));
    clientTextSpy.clear();

    // Send an obviously-unknown command. Per design doc §4.1 / Sweep B:
    // unknown commands produce zero outbound traffic. ESDR3/SunSDR/WSJT-X
    // distinguish "rejected" from "in flight" by silence, NOT by an error
    // frame. Adding any response would break those clients.
    client.sendTextMessage(QStringLiteral("nonexistent_command_xyz;"));

    // Wait long enough that any in-flight response would have arrived.
    // 200ms is generous on a localhost TCP socket.
    QTest::qWait(200);

    QCOMPARE(clientTextSpy.count(), 0);
    QCOMPARE(clientBinarySpy.count(), 0);

    client.close();
    server.stop();
}

QTEST_GUILESS_MAIN(TestTciSilentErrorInvariant)
#include "tst_tci_silent_error_invariant.moc"

#endif // HAVE_WEBSOCKETS
