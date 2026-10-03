// tests/tst_tci_iq_roundtrip.cpp  (NereusSDR)
// no-port-check: NereusSDR-original integration test for the IQ binary
// stream pipeline.  Validates: synthetic I/Q injection → wantsIQStream
// per-client subscription gating → IQSwap flag honored → TciBinaryFrame
// encode → QWebSocket sendBinaryMessage → client decodes streamType=0.
//
// Phase 3J-1 Task 18.1.
//
// R3 receiver audio plan, Task 4 (R-R3-42), 2026-09-23, J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code: a remote window refuses
// iq_start with no subscription and no echo, and tells the operator why,
// never the app.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QUrl>
#include <QVector>

#include <cstring>

#include "core/TciServer.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestTciIqRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void iq_start_subscribes_then_frames_arrive();
    void iq_stop_early_out_no_frames();
    void iq_swap_flag_swaps_i_q_pairs();
    void always_stream_iq_overrides_subscription();
    void remote_window_refuses_iq_start();
    void receiver_and_rate_are_reported();
    void tagged_local_stream_maps_receiver_and_accepted_rate();
    void remote_first_last_and_always_demand();
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

void TestTciIqRoundtrip::remote_window_refuses_iq_start()
{
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));
    RadioModel remote(RadioModel::Role::Remote);
    TciServer server(&remote);
    QSignalSpy notices(&server, &TciServer::operatorNotice);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy connected(&client, &QWebSocket::connected);
    QSignalSpy text(&client, &QWebSocket::textMessageReceived);
    QSignalSpy binary(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    const auto lines = [&text] {
        QStringList out;
        for (const auto& call : text) { out << call.at(0).toString(); }
        return out;
    };
    QTRY_VERIFY_WITH_TIMEOUT(lines().contains(QStringLiteral("ready;")), 3000);
    const int linesBefore = int(text.count());

    client.sendTextMessage(QStringLiteral("iq_start:0;"));
    QTRY_COMPARE_WITH_TIMEOUT(notices.count(), 1, 3000);
    QTest::qWait(100);
    QCOMPARE(server.activeIqSubscriberCount(0), 0);
    QVERIFY(!lines().mid(linesBefore).contains(QStringLiteral("iq_start:0;")));
    const QString reason = notices.constFirst().at(1).toString();
    QCOMPARE(reason, QString::fromLatin1(TciServer::kRemoteIqRefusedReason));
    for (const QString& line : lines()) {
        QVERIFY2(!line.contains(reason), qPrintable(line));
    }
    // Even pushed at the server, I/Q reaches no app of a remote window.
    server.injectRawIqForTest(QVector<float>(2048, 0.25f));
    QTest::qWait(100);
    QCOMPARE(binary.count(), 0);

    client.close();
    server.stop();
}

void TestTciIqRoundtrip::receiver_and_rate_are_reported()
{
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QWebSocket client;
    QSignalSpy connected(&client, &QWebSocket::connected);
    QSignalSpy binary(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    client.sendTextMessage(QStringLiteral("iq_start:1;"));
    QTRY_COMPARE_WITH_TIMEOUT(server.activeIqSubscriberCount(1), 1, 3000);
    const QVector<float> samples(32, 0.25f);
    server.injectRawIqForTest(0, 96000, samples);
    QTest::qWait(50);
    QCOMPARE(binary.size(), 0);
    server.injectRawIqForTest(1, 384000, samples);
    QTRY_COMPARE_WITH_TIMEOUT(binary.size(), 1, 3000);
    const QByteArray frame = binary.at(0).at(0).toByteArray();
    const auto word = [&frame](int offset) {
        return quint32(quint8(frame.at(offset)))
            | (quint32(quint8(frame.at(offset + 1))) << 8)
            | (quint32(quint8(frame.at(offset + 2))) << 16)
            | (quint32(quint8(frame.at(offset + 3))) << 24);
    };
    QCOMPARE(word(0), 1u);
    QCOMPARE(word(4), 384000u);
    server.injectRawIqForTest(1, 768000, samples);
    QTest::qWait(50);
    QCOMPARE(binary.size(), 1);
    server.injectRawIqForTest(1, 96000, samples);
    QTRY_COMPARE_WITH_TIMEOUT(binary.size(), 2, 3000);
    QCOMPARE(word(4), 384000u);
    const QByteArray changed = binary.at(1).at(0).toByteArray();
    const auto* bytes = reinterpret_cast<const quint8*>(changed.constData() + 4);
    QCOMPARE(quint32(bytes[0]) | (quint32(bytes[1]) << 8)
                 | (quint32(bytes[2]) << 16) | (quint32(bytes[3]) << 24), 96000u);
    client.close();
    server.stop();
}

void TestTciIqRoundtrip::tagged_local_stream_maps_receiver_and_accepted_rate()
{
    AppSettings::instance().setValue(QStringLiteral("TciIqSwap"), QStringLiteral("False"));
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));
    RadioModel radio;
    radio.setBoardForTest(HPSDRHW::Saturn);
    radio.configureStreamPool(5, 5, 192000);
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    radio.setLastRadioInfoForTest(info);
    const int first = radio.addSlice();
    const int second = radio.addSlice();
    QCOMPARE(first, 0);
    QCOMPARE(second, 1);
    SliceModel* const slice = radio.sliceById(second);
    QVERIFY(slice);
    slice->setFrequency(18'123'456.0);
    const int stream = slice->streamIndex();
    QVERIFY(stream > 0);
    QVERIFY(radio.setStreamSampleRate(stream, 96000));
    QCOMPARE(radio.streamSampleRateHz(stream), 96000);

    TciServer server(&radio);
    QVERIFY(server.start(0));
    QWebSocket client;
    QSignalSpy connected(&client, &QWebSocket::connected);
    QSignalSpy binary(&client, &QWebSocket::binaryMessageReceived);
    QSignalSpy text(&client, &QWebSocket::textMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    client.sendTextMessage(QStringLiteral("iq_start:1;"));
    QTRY_COMPARE(server.activeIqSubscriberCount(1), 1);
    radio.rawIqDataForStream(stream, QVector<float>(2048, 0.25f));
    QTRY_COMPARE(binary.size(), 1);
    const auto word = [](const QByteArray& frame, int offset) {
        const auto* p = reinterpret_cast<const quint8*>(frame.constData() + offset);
        return quint32(p[0]) | (quint32(p[1]) << 8)
            | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
    };
    QCOMPARE(word(binary.at(0).at(0).toByteArray(), 0), 1u);
    QCOMPARE(word(binary.at(0).at(0).toByteArray(), 4), 96000u);

    QVERIFY(radio.setStreamSampleRate(stream, 384000));
    QCOMPARE(radio.streamSampleRateHz(stream), 384000);
    radio.rawIqDataForStream(stream, QVector<float>(2048, 0.25f));
    QTRY_COMPARE(binary.size(), 2);
    QCOMPARE(word(binary.at(1).at(0).toByteArray(), 0), 1u);
    QCOMPARE(word(binary.at(1).at(0).toByteArray(), 4), 384000u);
    client.sendTextMessage(QStringLiteral("iq_samplerate;"));
    QTRY_VERIFY(([&] {
        for (const auto& call : text) {
            if (call.at(0).toString() == QLatin1String("iq_samplerate:384000;")) { return true; }
        }
        return false;
    })());
    client.close();
    server.stop();
}

void TestTciIqRoundtrip::remote_first_last_and_always_demand()
{
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));
    RadioModel remote(RadioModel::Role::Remote);
    TciServer server(&remote);
    QVector<int> requests, releases;
    server.setRemoteIqSource({[] { return true; },
        [&](int slice) { requests.append(slice); },
        [&](int slice) { releases.append(slice); }});
    QVERIFY(server.start(0));
    QWebSocket first, second;
    QSignalSpy firstConnected(&first, &QWebSocket::connected);
    QSignalSpy secondConnected(&second, &QWebSocket::connected);
    QSignalSpy firstBinary(&first, &QWebSocket::binaryMessageReceived);
    const QUrl url(QStringLiteral("ws://127.0.0.1:%1").arg(server.port()));
    first.open(url);
    second.open(url);
    QVERIFY(firstConnected.wait(2000));
    if (secondConnected.isEmpty()) { QVERIFY(secondConnected.wait(2000)); }
    first.sendTextMessage(QStringLiteral("iq_start:1;"));
    QTRY_COMPARE(requests.size(), 1);
    QCOMPARE(requests.at(0), 1);
    second.sendTextMessage(QStringLiteral("iq_start:1;"));
    QTRY_COMPARE(server.activeIqSubscriberCount(1), 2);
    QCOMPARE(requests.size(), 1);
    server.receiveRemoteIq(1, 96000, QVector<float>(2048, 0.25f));
    QTRY_COMPARE(firstBinary.size(), 1);
    first.sendTextMessage(QStringLiteral("iq_stop:1;"));
    QTRY_COMPARE(server.activeIqSubscriberCount(1), 1);
    QCOMPARE(releases.size(), 0);
    second.close();
    QTRY_COMPARE(releases.size(), 1);
    QCOMPARE(releases.at(0), 1);

    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("True"));
    server.refreshRemoteIqDemand();
    QCOMPARE(requests.size(), 3);
    QCOMPARE(requests.at(1), 0);
    QCOMPARE(requests.at(2), 1);
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False"));
    server.refreshRemoteIqDemand();
    QCOMPARE(releases.size(), 3);
    first.close();
    server.stop();
}

QTEST_GUILESS_MAIN(TestTciIqRoundtrip)
#include "tst_tci_iq_roundtrip.moc"

#else  // !HAVE_WEBSOCKETS

// WebSockets not available — test file must still compile and produce a
// no-op binary so CTest does not report a missing executable.
int main() { return 0; }

#endif // HAVE_WEBSOCKETS
