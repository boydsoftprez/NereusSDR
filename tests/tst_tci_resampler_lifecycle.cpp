// tests/tst_tci_resampler_lifecycle.cpp  (NereusSDR)
// NereusSDR-original — no Thetis upstream port in this test file.
//
// Phase 3J-1 Task 16.3 (sub-commit b): per-(client, slice) WDSP resampler
// lifecycle test.  Exercises TciServer::handleAudioSubscribe /
// handleAudioUnsubscribe and the totalResamplerInstances() accessor.
//
// Lifecycle matrix:
//   1. start server, connect client 1  → 0 resamplers
//   2. client 1 sends audio_start:0;  → 1 resampler
//   3. connect client 2, sends audio_start:0; → 2 resamplers
//   4. client 1 sends audio_stop:0;   → 1 resampler
//   5. server stop                    → 0 resamplers
//
// Uses QWebSocket in loopback (same pattern as tst_tci_server_lifecycle.cpp).

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>

#include "core/TciServer.h"

using namespace NereusSDR;

class TestTciResamplerLifecycle : public QObject {
    Q_OBJECT

private:
    // Helper: connect a QWebSocket to the server and wait for it to connect.
    static QWebSocket* connectClient(TciServer& server) {
        auto* ws = new QWebSocket();
        QEventLoop loop;
        QObject::connect(ws, &QWebSocket::connected, &loop, &QEventLoop::quit);
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);  // 2s timeout
        ws->open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        loop.exec();
        return ws;
    }

    // Helper: send a text message and spin the event loop briefly to let the
    // server process it.
    static void sendAndProcess(QWebSocket* ws, const QString& msg) {
        ws->sendTextMessage(msg);
        // Give Qt event loop a chance to deliver the textMessageReceived signal
        // to TciServer's onTextMessageReceived slot.
        QTest::qWait(30);
    }

private slots:
    // Lifecycle test: subscribe → 1; second client subscribes → 2;
    // first unsubscribes → 1; server stop → 0.
    void resampler_lifecycle();

    // Idempotency: sending audio_start for the same rx twice does not
    // create a second resampler.
    void audio_start_idempotent();
};

void TestTciResamplerLifecycle::resampler_lifecycle()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));

    // Step 1: no clients yet → 0 resamplers.
    QCOMPARE(server.totalResamplerInstances(), 0);

    // Step 2: client 1 subscribes to slice 0.
    auto* ws1 = connectClient(server);
    QVERIFY(ws1->state() == QAbstractSocket::ConnectedState);
    sendAndProcess(ws1, QStringLiteral("audio_start:0;"));
    QCOMPARE(server.totalResamplerInstances(), 1);

    // Step 3: client 2 subscribes to the same slice 0.
    auto* ws2 = connectClient(server);
    QVERIFY(ws2->state() == QAbstractSocket::ConnectedState);
    sendAndProcess(ws2, QStringLiteral("audio_start:0;"));
    QCOMPARE(server.totalResamplerInstances(), 2);

    // Step 4: client 1 unsubscribes.
    sendAndProcess(ws1, QStringLiteral("audio_stop:0;"));
    QCOMPARE(server.totalResamplerInstances(), 1);

    // Step 5: server stop → 0 resamplers.
    ws1->deleteLater();
    ws2->deleteLater();
    server.stop();
    QCOMPARE(server.totalResamplerInstances(), 0);
}

void TestTciResamplerLifecycle::audio_start_idempotent()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));

    auto* ws = connectClient(server);
    QVERIFY(ws->state() == QAbstractSocket::ConnectedState);

    sendAndProcess(ws, QStringLiteral("audio_start:0;"));
    QCOMPARE(server.totalResamplerInstances(), 1);

    // Second audio_start for the same rx must not create another resampler.
    sendAndProcess(ws, QStringLiteral("audio_start:0;"));
    QCOMPARE(server.totalResamplerInstances(), 1);

    ws->deleteLater();
    server.stop();
    QCOMPARE(server.totalResamplerInstances(), 0);
}

QTEST_GUILESS_MAIN(TestTciResamplerLifecycle)
#include "tst_tci_resampler_lifecycle.moc"

#else
int main() { return 0; }
#endif // HAVE_WEBSOCKETS
