// tests/tst_tci_iq_roundtrip.cpp  (NereusSDR)
// no-port-check: NereusSDR-original integration test for the IQ binary
// stream pipeline.  Validates: synthetic I/Q injection → wantsIQStream
// per-client subscription gating → IQSwap flag honored → TciBinaryFrame
// encode → QWebSocket sendBinaryMessage → client decodes streamType=0.
//
// Phase 3J-1 Task 18.1.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QUrl>
#include <QVector>

#include <cstring>

#include "core/TciServer.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestTciIqRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void iq_start_subscribes_then_frames_arrive();
    void iq_stop_early_out_no_frames();
    void iq_swap_flag_swaps_i_q_pairs();
    void always_stream_iq_overrides_subscription();
};

// ── iq_start_subscribes_then_frames_arrive() ─────────────────────────────────
//
// Wire path exercised:
//   client sends "iq_start:0;" → onTextMessageReceived intercepts →
//   session->iqStreamEnabled.insert(0)
//   test calls injectRawIqForTest(iqData) → onRawIqDataReceived:
//     IQSwap=True (default) swaps each (I,Q) pair
//     wantsIQStream(0) → session subscribed → encode + sendBinaryMessage
//   client binaryMessageReceived fires
//   test asserts header: receiver=0, sampleType=Float32(3), streamType=IqStream(0),
//   channels=2.
//
// From Thetis TCIServer.cs:5397-5434 [v2.10.3.13] — wantsIQStream +
// PublishIQSamples.

void TestTciIqRoundtrip::iq_start_subscribes_then_frames_arrive()
{
    // Ensure default IQSwap=True is set (sandbox AppSettings).
    AppSettings::instance().setValue(QStringLiteral("TciIqSwap"), QStringLiteral("True"));
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));

    // Spin up TciServer on an ephemeral port (RadioModel* not needed — test-injection path).
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QVERIFY(server.isRunning());

    // Connect a real QWebSocket client.
    QWebSocket client;
    QSignalSpy clientConnected(&client, &QWebSocket::connected);
    QSignalSpy binarySpy(&client, &QWebSocket::binaryMessageReceived);

    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(clientConnected.wait(2000));
    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);

    // Subscribe to slice 0 IQ.
    // onTextMessageReceived intercepts "iq_start:0;" before TciProtocol dispatch
    // and inserts 0 into session->iqStreamEnabled.
    client.sendTextMessage(QStringLiteral("iq_start:0;"));

    // Allow subscription to register across the loopback socket + event loop.
    QTest::qWait(50);

    // Inject synthetic IQ: 1024 complex samples (I=0.5, Q=0.3).
    // With IQSwap=True the wire will carry (Q=0.3, I=0.5).
    QVector<float> iqData;
    iqData.reserve(1024 * 2);
    for (int i = 0; i < 1024; ++i) {
        iqData.append(0.5f);   // I
        iqData.append(0.3f);   // Q
    }
    server.injectRawIqForTest(iqData);

    // Give the slot time to run and sendBinaryMessage to fire.
    QTest::qWait(100);

    // Assert: at least one binary frame arrived.
    QVERIFY2(binarySpy.count() >= 1,
             qPrintable(QStringLiteral("Expected ≥1 IQ binary frame, got %1")
                            .arg(binarySpy.count())));

    // Decode and verify the first frame header.
    //
    // Header layout (each field is uint32 LE, offsets in bytes):
    //   0  : receiver index
    //   4  : sample rate
    //   8  : sample type    (Float32 = 3)
    //   12 : reserved
    //   16 : reserved
    //   20 : length         (complexSamples * 2 = total floats)
    //   24 : stream type    (IqStream = 0)
    //   28 : channels       (always 2 for IQ)
    //
    // From TciBinaryFrame.h + Thetis TCIServer.cs:5240-5262 [v2.10.3.13].
    const QByteArray frame = binarySpy.at(0).at(0).toByteArray();
    QVERIFY2(frame.size() > 64,
             qPrintable(QStringLiteral("IQ frame too small: %1 bytes").arg(frame.size())));

    auto readU32 = [&](int offset) -> quint32 {
        const auto* p = reinterpret_cast<const quint8*>(frame.constData() + offset);
        return static_cast<quint32>(p[0])
             | (static_cast<quint32>(p[1]) << 8)
             | (static_cast<quint32>(p[2]) << 16)
             | (static_cast<quint32>(p[3]) << 24);
    };

    QCOMPARE(readU32(0),  0u);    // receiver = 0
    QCOMPARE(readU32(8),  3u);    // sampleType = Float32
    QCOMPARE(readU32(24), 0u);    // streamType = IqStream (= 0)
    QCOMPARE(readU32(28), 2u);    // channels = 2

    // `length` field = complexSamples * 2 (total floats in the frame).
    // From Thetis TCIServer.cs:5434 [v2.10.3.13] — bug-for-bug parity.
    const quint32 lengthField = readU32(20);
    QVERIFY(lengthField > 0u);

    // Payload must be length * sizeof(float) bytes after the 64-byte header.
    const int payloadBytes = frame.size() - 64;
    QCOMPARE(payloadBytes, static_cast<int>(lengthField) * 4);

    client.close();
    server.stop();
}

// ── iq_stop_early_out_no_frames() ────────────────────────────────────────────
//
// Validates the wantsIQStream early-out:
//   iq_start:0; → subscribe
//   iq_stop:0;  → unsubscribe
//   inject IQ   → no frame should be delivered
//
// From Thetis TCIServer.cs:5402-5404 [v2.10.3.13] — early return when
// receiver not in m_iqStreamEnabled.

void TestTciIqRoundtrip::iq_stop_early_out_no_frames()
{
    AppSettings::instance().setValue(QStringLiteral("TciIqSwap"), QStringLiteral("True"));
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));

    TciServer server(nullptr);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnected(&client, &QWebSocket::connected);
    QSignalSpy binarySpy(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(clientConnected.wait(2000));

    // Subscribe, then immediately unsubscribe.
    client.sendTextMessage(QStringLiteral("iq_start:0;"));
    QTest::qWait(50);
    client.sendTextMessage(QStringLiteral("iq_stop:0;"));
    QTest::qWait(50);

    // Verify subscription is gone.
    QCOMPARE(server.activeIqSubscriberCount(0), 0);

    // Inject IQ — should be silently discarded (wantsIQStream returns false).
    QVector<float> iqData(2048, 0.1f);
    server.injectRawIqForTest(iqData);
    QTest::qWait(100);

    QCOMPARE(binarySpy.count(), 0);   // unsubscribed; no frames

    client.close();
    server.stop();
}

// ── iq_swap_flag_swaps_i_q_pairs() ───────────────────────────────────────────
//
// Validates IQSwap flag behavior — From Thetis TCIServer.cs:6111 [v2.10.3.13].
//
// With IQSwap=False: inject (I=0.7, Q=0.2) → wire carries (0.7, 0.2).
// With IQSwap=True:  inject (I=0.7, Q=0.2) → wire carries (0.2, 0.7).
//
// This test forces IQSwap=False so that the first payload float is I=0.7
// and the second is Q=0.2 (no swap).

void TestTciIqRoundtrip::iq_swap_flag_swaps_i_q_pairs()
{
    // Force IQSwap off — no swap, wire carries original I,Q order.
    AppSettings::instance().setValue(QStringLiteral("TciIqSwap"), QStringLiteral("False"));
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));

    TciServer server(nullptr);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnected(&client, &QWebSocket::connected);
    QSignalSpy binarySpy(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(clientConnected.wait(2000));
    client.sendTextMessage(QStringLiteral("iq_start:0;"));
    QTest::qWait(50);

    // 16 complex samples with I=0.7, Q=0.2.
    QVector<float> iqData;
    iqData.reserve(32);
    for (int i = 0; i < 16; ++i) {
        iqData.append(0.7f);   // I
        iqData.append(0.2f);   // Q
    }
    server.injectRawIqForTest(iqData);
    QTest::qWait(100);

    QVERIFY2(binarySpy.count() >= 1,
             "IQSwap=False: expected ≥1 binary frame");

    const QByteArray frame = binarySpy.at(0).at(0).toByteArray();
    QVERIFY(frame.size() > 64);

    // Payload starts at byte 64.  With IQSwap=False the first float is I=0.7.
    float first = 0.0f, second = 0.0f;
    std::memcpy(&first,  frame.constData() + 64, 4);
    std::memcpy(&second, frame.constData() + 68, 4);

    QVERIFY2(std::abs(first  - 0.7f) < 1e-6f,
             qPrintable(QStringLiteral("IQSwap=False: first float should be I=0.7, got %1").arg(double(first))));
    QVERIFY2(std::abs(second - 0.2f) < 1e-6f,
             qPrintable(QStringLiteral("IQSwap=False: second float should be Q=0.2, got %1").arg(double(second))));

    // Restore default for subsequent tests.
    AppSettings::instance().setValue(QStringLiteral("TciIqSwap"), QStringLiteral("True"));
    client.close();
    server.stop();
}

// ── always_stream_iq_overrides_subscription() ─────────────────────────────────
//
// Validates AlwaysStreamIQ flag — From Thetis TCIServer.cs:5401 [v2.10.3.13]:
//   if (m_server != null && m_server.AlwaysStreamIQ) return true;
//
// When TciAlwaysStreamIq=True, the client receives IQ frames even without
// sending iq_start.

void TestTciIqRoundtrip::always_stream_iq_overrides_subscription()
{
    AppSettings::instance().setValue(QStringLiteral("TciIqSwap"), QStringLiteral("True"));
    // Enable AlwaysStreamIQ — bypasses per-client subscription check.
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("True"));

    TciServer server(nullptr);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnected(&client, &QWebSocket::connected);
    QSignalSpy binarySpy(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(clientConnected.wait(2000));
    // Deliberately NO iq_start — AlwaysStreamIQ must override.

    QVector<float> iqData(2048, 0.1f);
    server.injectRawIqForTest(iqData);
    QTest::qWait(100);

    QVERIFY2(binarySpy.count() >= 1,
             "AlwaysStreamIQ override failed — client should receive IQ frames "
             "without an explicit iq_start subscription");

    // Restore defaults for cleanliness.
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));
    client.close();
    server.stop();
}

QTEST_GUILESS_MAIN(TestTciIqRoundtrip)
#include "tst_tci_iq_roundtrip.moc"

#else  // !HAVE_WEBSOCKETS

// WebSockets not available — test file must still compile and produce a
// no-op binary so CTest does not report a missing executable.
int main() { return 0; }

#endif // HAVE_WEBSOCKETS
