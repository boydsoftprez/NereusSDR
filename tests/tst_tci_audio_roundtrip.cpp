// tests/tst_tci_audio_roundtrip.cpp  (NereusSDR)
// no-port-check: NereusSDR-original integration test for the audio binary
// RX pipeline.  Validates: synthetic audio injection → AudioRingSpsc → drain
// timer assembly → resampler (identity at srcRate=48k) → TciBinaryFrame
// encode → QWebSocket sendBinaryMessage → client receives + decodes.
//
// Phase 3J-1 Task 16.4.  Plan spec: ≥ 1 binary frame with streamType==1
// and decoded payload matches input within 1e-3 (identity-resample case).
//
// R3 receiver audio plan, Task 4 (R-R3-42), 2026-09-23, J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code: two local clients on
// one receiver each receive all of its audio, and a mono client receives
// the left channel (Thetis TCIServer.cs PublishRxAudioSamples), not half
// a block of interleaved stereo.
//
// R3 receiver audio fix wave (R-R3-42), 2026-09-23, J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code: stereo at a rate other than
// 48 kHz keeps left and right apart (one resampler per channel, as Thetis
// TCIServer.cs resampleRxAudioSamples does), not one resampler run over
// interleaved L/R.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/TciServer.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestTciAudioRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void synthetic_1khz_tone_arrives_as_binary_frame();
    void two_clients_on_one_receiver_each_get_all_of_it();
    void mono_client_gets_the_left_channel();
    void stereo_at_12k_keeps_left_and_right_apart();
    void desktop_host_audio_uses_owned_slice();
};

namespace {

struct DecodedBlock {
    quint32 receiver = 0;
    quint32 channels = 0;
    quint32 length = 0;
    QVector<float> samples;
};

quint32 readLe32(const QByteArray& frame, int offset)
{
    const auto* p = reinterpret_cast<const quint8*>(frame.constData() + offset);
    return static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8)
         | (static_cast<quint32>(p[2]) << 16) | (static_cast<quint32>(p[3]) << 24);
}

// Float32 RX audio frames only (the server's default sample type).
QList<DecodedBlock> decodeRxAudio(const QSignalSpy& spy)
{
    QList<DecodedBlock> blocks;
    for (const QList<QVariant>& call : spy) {
        const QByteArray frame = call.at(0).toByteArray();
        if (frame.size() < 64 || readLe32(frame, 24) != 1u) { continue; }
        DecodedBlock block;
        block.receiver = readLe32(frame, 0);
        block.length = readLe32(frame, 20);
        block.channels = readLe32(frame, 28);
        block.samples.resize((frame.size() - 64) / 4);
        std::memcpy(block.samples.data(), frame.constData() + 64,
                    static_cast<size_t>(block.samples.size()) * 4);
        blocks.append(block);
    }
    return blocks;
}

// Left channel k (0-based) of the ramp the two cases inject; the right
// channel is its negative, so a left/right mix-up cannot pass.
float rampLeft(int k) { return 1.0e-4f * static_cast<float>(k + 1); }

void injectRamp(TciServer& server, int totalFrames)
{
    constexpr int kChunk = 1024;
    std::vector<float> left(kChunk), right(kChunk);
    for (int sent = 0; sent < totalFrames; sent += kChunk) {
        const int n = std::min(kChunk, totalFrames - sent);
        for (int i = 0; i < n; ++i) {
            left[static_cast<size_t>(i)] = rampLeft(sent + i);
            right[static_cast<size_t>(i)] = -rampLeft(sent + i);
        }
        server.injectAudioFrameForTest(0, left.data(), right.data(), n, 48000);
    }
}

// A tone's amplitude in one channel of interleaved audio at `rateHz`.
double toneAmplitude(const QVector<float>& samples, int channels, int channel,
                     double hz, int rateHz, int firstFrame)
{
    double cosine = 0.0;
    double sine = 0.0;
    int frames = 0;
    for (int f = firstFrame; f * channels + channel < samples.size(); ++f) {
        const double phase = 2.0 * M_PI * hz * double(f) / double(rateHz);
        const double v = samples.at(f * channels + channel);
        cosine += v * std::cos(phase);
        sine += v * std::sin(phase);
        ++frames;
    }
    return frames > 0 ? 2.0 * std::hypot(cosine, sine) / frames : 0.0;
}

bool connectClient(QWebSocket& client, quint16 port)
{
    QSignalSpy connected(&client, &QWebSocket::connected);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    return connected.wait(2000) && client.state() == QAbstractSocket::ConnectedState;
}

} // namespace

void TestTciAudioRoundtrip::desktop_host_audio_uses_owned_slice()
{
    RadioModel radio;
    const int foreign = radio.addSlice(QStringLiteral("pan-0"));
    const int owned = radio.addSlice(QStringLiteral("pan-0"));
    QCOMPARE(foreign, 0);
    QCOMPARE(owned, 1);
    radio.sliceOwnership()->setOwner(foreign, QByteArray("phone"));
    radio.sliceOwnership()->setOwner(owned, SliceOwnership::stationDevice());
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    QWebSocket client;
    QSignalSpy binary(&client, &QWebSocket::binaryMessageReceived);
    QVERIFY(connectClient(client, server.port()));
    client.sendTextMessage(QStringLiteral("audio_start:0;"));
    QTest::qWait(100);
    std::vector<float> foreignSamples(2048, 0.2f);
    std::vector<float> ownedSamples(2048, 0.4f);
    server.injectAudioFrameForTest(foreign, foreignSamples.data(), foreignSamples.data(),
                                   2048, 48000);
    QTest::qWait(100);
    QCOMPARE(decodeRxAudio(binary).size(), 0);
    server.injectAudioFrameForTest(owned, ownedSamples.data(), ownedSamples.data(),
                                   2048, 48000);
    QTRY_COMPARE_WITH_TIMEOUT(decodeRxAudio(binary).size(), 1, 3000);
    const DecodedBlock block = decodeRxAudio(binary).first();
    QCOMPARE(block.receiver, 0u);
    QVERIFY(std::abs(block.samples.at(0) - 0.4f) < 1e-3f);
    // Leave a partial old-owner block in the ring, then change ownership.
    // The next full receiver block must contain only the new owner's PCM.
    server.injectAudioFrameForTest(owned, ownedSamples.data(), ownedSamples.data(),
                                   1024, 48000);
    radio.sliceOwnership()->setOwner(owned, QByteArray("phone"));
    radio.sliceOwnership()->setOwner(foreign, SliceOwnership::stationDevice());
    binary.clear();
    server.injectAudioFrameForTest(owned, ownedSamples.data(), ownedSamples.data(),
                                   2048, 48000);
    QTest::qWait(100);
    QCOMPARE(decodeRxAudio(binary).size(), 0);
    std::vector<float> newOwnedSamples(2048, 0.6f);
    server.injectAudioFrameForTest(foreign, newOwnedSamples.data(), newOwnedSamples.data(),
                                   2048, 48000);
    QTRY_COMPARE_WITH_TIMEOUT(decodeRxAudio(binary).size(), 1, 3000);
    const DecodedBlock remapped = decodeRxAudio(binary).first();
    QCOMPARE(remapped.receiver, 0u);
    for (float sample : remapped.samples) {
        QVERIFY(std::abs(sample - 0.6f) < 1e-3f);
    }
    client.close();
    server.stop();
}

// Two apps on receiver 0 used to pop the one shared ring, so each got about
// half of the blocks. Each must now get every block, in order.
void TestTciAudioRoundtrip::two_clients_on_one_receiver_each_get_all_of_it()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QWebSocket clientA;
    QWebSocket clientB;
    QSignalSpy binaryA(&clientA, &QWebSocket::binaryMessageReceived);
    QSignalSpy binaryB(&clientB, &QWebSocket::binaryMessageReceived);
    QVERIFY(connectClient(clientA, server.port()));
    QVERIFY(connectClient(clientB, server.port()));
    clientA.sendTextMessage(QStringLiteral("audio_start:0;"));
    clientB.sendTextMessage(QStringLiteral("audio_start:0;"));
    QTest::qWait(100);

    constexpr int kBlockFrames = 2048;   // default audio_stream_samples
    constexpr int kBlocks = 3;
    injectRamp(server, kBlockFrames * kBlocks);
    QTest::qWait(300);

    for (QSignalSpy* spy : {&binaryA, &binaryB}) {
        const QList<DecodedBlock> blocks = decodeRxAudio(*spy);
        QCOMPARE(blocks.size(), kBlocks);
        int frame = 0;
        for (const DecodedBlock& block : blocks) {
            QCOMPARE(block.receiver, 0u);
            QCOMPARE(block.channels, 2u);
            QCOMPARE(block.length, quint32(kBlockFrames * 2));
            for (int i = 0; i < kBlockFrames; ++i, ++frame) {
                QVERIFY2(std::fabs(block.samples.at(2 * i) - rampLeft(frame)) < 1e-7f
                             && std::fabs(block.samples.at(2 * i + 1) + rampLeft(frame)) < 1e-7f,
                         qPrintable(QStringLiteral("frame %1 is %2,%3; expected %4,%5")
                                        .arg(frame)
                                        .arg(double(block.samples.at(2 * i)))
                                        .arg(double(block.samples.at(2 * i + 1)))
                                        .arg(double(rampLeft(frame)))
                                        .arg(double(-rampLeft(frame)))));
            }
        }
    }
    clientA.close();
    clientB.close();
    server.stop();
}

// audio_stream_channels:1 used to take half a block of interleaved L/R
// floats and send them as mono samples. A mono block is the left channel,
// one sample per frame, as Thetis PublishRxAudioSamples sends it.
void TestTciAudioRoundtrip::mono_client_gets_the_left_channel()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QWebSocket client;
    QSignalSpy binary(&client, &QWebSocket::binaryMessageReceived);
    QVERIFY(connectClient(client, server.port()));
    client.sendTextMessage(QStringLiteral("audio_stream_channels:1;"));
    client.sendTextMessage(QStringLiteral("audio_start:0;"));
    QTest::qWait(100);

    constexpr int kBlockFrames = 2048;
    injectRamp(server, kBlockFrames * 2);
    QTest::qWait(300);

    const QList<DecodedBlock> blocks = decodeRxAudio(binary);
    QCOMPARE(blocks.size(), 2);
    int frame = 0;
    for (const DecodedBlock& block : blocks) {
        QCOMPARE(block.channels, 1u);
        QCOMPARE(block.length, quint32(kBlockFrames));
        QCOMPARE(block.samples.size(), kBlockFrames);
        for (int i = 0; i < kBlockFrames; ++i, ++frame) {
            QVERIFY2(std::fabs(block.samples.at(i) - rampLeft(frame)) < 1e-7f,
                     qPrintable(QStringLiteral("mono sample %1 is %2; expected %3")
                                    .arg(frame)
                                    .arg(double(block.samples.at(i)))
                                    .arg(double(rampLeft(frame)))));
        }
    }
    client.close();
    server.stop();
}

// Stereo at 12 kHz: one resampler used to run over interleaved L/R, which
// mixed the channels and read them at the wrong rate. Each channel now has
// its own, so a 440 Hz left and a 1000 Hz right arrive apart, at 12 kHz.
void TestTciAudioRoundtrip::stereo_at_12k_keeps_left_and_right_apart()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QWebSocket client;
    QSignalSpy binary(&client, &QWebSocket::binaryMessageReceived);
    QVERIFY(connectClient(client, server.port()));
    client.sendTextMessage(QStringLiteral("audio_samplerate:12000;"));
    client.sendTextMessage(QStringLiteral("audio_stream_channels:2;"));
    client.sendTextMessage(QStringLiteral("audio_start:0;"));
    QTest::qWait(100);

    constexpr int kChunk = 1024;
    constexpr int kTotal = 48000;   // one second at 48 kHz
    std::vector<float> left(kChunk), right(kChunk);
    for (int sent = 0; sent < kTotal; sent += kChunk) {
        for (int i = 0; i < kChunk; ++i) {
            const double t = double(sent + i) / 48000.0;
            left[size_t(i)] = float(0.3 * std::sin(2.0 * M_PI * 440.0 * t));
            right[size_t(i)] = float(0.2 * std::sin(2.0 * M_PI * 1000.0 * t));
        }
        server.injectAudioFrameForTest(0, left.data(), right.data(), kChunk, 48000);
        QTest::qWait(10);
    }
    QTest::qWait(300);

    QVector<float> received;
    for (const DecodedBlock& block : decodeRxAudio(binary)) {
        QCOMPARE(block.channels, 2u);
        received += block.samples;
    }
    // 2048 frames of 48 kHz audio per block, 512 at 12 kHz.
    QVERIFY2(received.size() >= 2 * 11000, qPrintable(QString::number(received.size())));
    constexpr int kSkip = 600;   // past the resampler's start
    const double leftLow = toneAmplitude(received, 2, 0, 440.0, 12000, kSkip);
    const double leftHigh = toneAmplitude(received, 2, 0, 1000.0, 12000, kSkip);
    const double rightLow = toneAmplitude(received, 2, 1, 440.0, 12000, kSkip);
    const double rightHigh = toneAmplitude(received, 2, 1, 1000.0, 12000, kSkip);
    QVERIFY2(std::abs(leftLow - 0.3) < 0.01 && leftHigh < 0.005,
             qPrintable(QStringLiteral("left: 440 Hz %1, 1000 Hz %2").arg(leftLow).arg(leftHigh)));
    QVERIFY2(std::abs(rightHigh - 0.2) < 0.01 && rightLow < 0.005,
             qPrintable(QStringLiteral("right: 440 Hz %1, 1000 Hz %2").arg(rightLow).arg(rightHigh)));
    client.close();
    server.stop();
}

// ── synthetic_1khz_tone_arrives_as_binary_frame() ───────────────────────────
//
// Wire path exercised:
//   test calls injectAudioFrameForTest(slice=0, L, R, n=1024, srcRate=48000)
//   → onAudioFrameReady interleaves L/R into m_audioRing[0]
//   → 5ms drain timer fires: pops 2048*2 floats (one full audioStreamSamples
//     * channels chunk), no resampler branch (48000 == 48000), encodes via
//     TciBinaryFrame::buildStreamPayload, calls sendBinaryMessage
//   → client's binaryMessageReceived signal fires
//   → test decodes the 64-byte LE header and float payload, asserts fields
//     and sine-wave amplitude.

void TestTciAudioRoundtrip::synthetic_1khz_tone_arrives_as_binary_frame()
{
    // ── 1.  Spin up TciServer on an ephemeral port ────────────────────────────
    TciServer server(nullptr);   // RadioModel* not needed — test-injection path
    QVERIFY(server.start(0));
    QVERIFY(server.isRunning());

    // ── 2.  Connect a real QWebSocket client ─────────────────────────────────
    QWebSocket client;
    QSignalSpy clientConnected(&client, &QWebSocket::connected);
    QSignalSpy binarySpy(&client, &QWebSocket::binaryMessageReceived);

    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(clientConnected.wait(2000));
    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);

    // ── 3.  Subscribe to slice 0 audio ────────────────────────────────────────
    // onTextMessageReceived intercepts "audio_start:0;" before TciProtocol
    // dispatch and calls handleAudioSubscribe which creates the resampler.
    client.sendTextMessage(QStringLiteral("audio_start:0;"));

    // Give the subscription time to register: the message crosses the
    // loopback socket (one round-trip through the OS network stack) and
    // the slot runs synchronously on the Qt event loop.  50ms is generous.
    QTest::qWait(50);

    // ── 4.  Generate ~250 ms of 1 kHz sine at 48 kHz, stereo ─────────────────
    // audioStreamSamples default = 2048 (from TciClientSession defaults).
    // channels default = 2.  The drain pops 2048*2 floats per tick; so we
    // inject enough chunks to fill at least one drain window.
    //
    // 250ms @ 48kHz = 12000 frames.  We inject in 1024-frame chunks (12 chunks).
    // Each chunk pushes 1024*2 floats = 8192 bytes.
    // One drain window needs 2048*2*4 = 16384 bytes → 2 chunks fill it.
    constexpr int kSrcRate     = 48000;
    constexpr int kTotalFrames = kSrcRate / 4;    // 12000 stereo frames = 250ms
    constexpr int kChunkFrames = 1024;
    constexpr double kFreqHz   = 1000.0;

    std::vector<float> L(kChunkFrames), R(kChunkFrames);
    int totalFramesSent = 0;
    while (totalFramesSent < kTotalFrames) {
        const int remaining = kTotalFrames - totalFramesSent;
        const int thisChunk = std::min(kChunkFrames, remaining);
        for (int i = 0; i < thisChunk; ++i) {
            const double phase = 2.0 * M_PI * kFreqHz
                                 * (totalFramesSent + i) / double(kSrcRate);
            const float sample = static_cast<float>(std::sin(phase));
            L[i] = sample;
            R[i] = sample;   // mono content, stereo channel pair
        }
        // Test-only injection: bypasses the real RxChannel→RadioModel chain
        // (which requires hardware + WDSP wisdom) and feeds audio directly into
        // m_audioRing[0] for the drain timer to pick up.
        server.injectAudioFrameForTest(0, L.data(), R.data(), thisChunk, kSrcRate);
        totalFramesSent += thisChunk;
    }

    // ── 5.  Wait for drain ticks to flush binary frames to the client ─────────
    // Drain timer fires every 5ms.  200ms gives ≥40 ticks — well more than
    // needed to drain 250ms of audio at 2048-sample windows.
    // The QTest::qWait spins the event loop so timer events are processed.
    QTest::qWait(200);

    // ── 6.  Assert: at least one binary frame arrived ─────────────────────────
    QVERIFY2(binarySpy.count() >= 1,
             qPrintable(QStringLiteral("Expected ≥1 binary frame, got %1")
                            .arg(binarySpy.count())));

    // ── 7.  Decode the first frame ────────────────────────────────────────────
    //
    // Header layout (each field is uint32 LE, offsets in bytes):
    //   0   receiver
    //   4   sampleRate
    //   8   sampleType
    //   12  reserved
    //   16  reserved
    //   20  length  ← flat count (perChSamples * channels = 2048*2 = 4096)
    //   24  streamType
    //   28  channels
    //   32..60  reserved (8 × uint32, all 0)
    //
    // From TciBinaryFrame.cpp:buildStreamPayload + Thetis TCIServer.cs:5240-5262
    // [v2.10.3.13].  The `length` field carries the flat interleaved count, not
    // the per-channel count — verified by reading buildStreamPayload which calls
    //   encodeSamples(samples, length, sampleType)
    // and passes `outSamples = perChSamples * channels` as `length`.

    const QByteArray firstFrame = binarySpy.at(0).at(0).toByteArray();
    QVERIFY2(firstFrame.size() > 64,
             qPrintable(QStringLiteral("Frame too small: %1 bytes").arg(firstFrame.size())));

    // Little-endian uint32 reader.
    auto readU32 = [&](int offset) -> quint32 {
        const auto* p = reinterpret_cast<const quint8*>(firstFrame.constData() + offset);
        return static_cast<quint32>(p[0])
             | (static_cast<quint32>(p[1]) << 8)
             | (static_cast<quint32>(p[2]) << 16)
             | (static_cast<quint32>(p[3]) << 24);
    };

    const quint32 receiver   = readU32(0);
    const quint32 sampleRate = readU32(4);
    const quint32 sampleType = readU32(8);
    const quint32 length     = readU32(20);   // flat interleaved count
    const quint32 streamType = readU32(24);
    const quint32 channels   = readU32(28);

    // ── 8.  Assert header fields ──────────────────────────────────────────────
    QCOMPARE(receiver,   0u);       // slice 0
    QCOMPARE(sampleRate, 48000u);   // default audioSampleRate (TciClientSession default)
    QCOMPARE(sampleType, 3u);       // Float32 (TciClientSession audioSampleType default)
    QCOMPARE(streamType, 1u);       // RxAudioStream per TciStreamType enum
    QCOMPARE(channels,   2u);       // stereo (TciClientSession audioStreamChannels default)
    QVERIFY(length > 0u);

    // ── 9.  Decode FLOAT32 payload and check amplitude ────────────────────────
    //
    // For Float32/stereo, the payload after the 64-byte header is:
    //   length * sizeof(float) bytes  (length is the flat interleaved count)
    // The number of floats equals `length`, NOT `length * channels`.
    // Verified: encodeSamples(samples, length, sampleType) encodes exactly
    // `length` floats; buildStreamPayload stores that as `length` in the header.

    const int sampleBytes  = firstFrame.size() - 64;
    const int totalFloats  = sampleBytes / 4;

    // Sanity: payload size must be consistent with the flat-count header field.
    QCOMPARE(totalFloats, static_cast<int>(length));

    // All decoded values must be in [-1, 1] (sine is bounded by construction
    // and no clipping or gain stage is applied in the identity-resample path).
    float peak = 0.0f;
    for (int i = 0; i < totalFloats; ++i) {
        float v = 0.0f;
        std::memcpy(&v, firstFrame.constData() + 64 + i * 4, 4);
        QVERIFY2(v >= -1.000001f && v <= 1.000001f,
                 qPrintable(QStringLiteral("Sample %1 out of range: %2").arg(i).arg(double(v))));
        if (std::abs(v) > peak) { peak = std::abs(v); }
    }

    // A 1 kHz sine with amplitude 1.0 must produce a peak ≥ 0.5 within any
    // window of ≥ 2048 stereo samples (≥ 1024 per channel).  The interleaved
    // layout mixes L and R samples; both carry the same sine, so every other
    // float in the payload is a sine sample — more than enough to see peak.
    QVERIFY2(peak > 0.5f,
             qPrintable(QStringLiteral(
                 "Peak amplitude %1 too low — 1kHz sine not flowing through pipeline")
                 .arg(double(peak))));

    // ── Cleanup ───────────────────────────────────────────────────────────────
    client.close();
    server.stop();
}

QTEST_GUILESS_MAIN(TestTciAudioRoundtrip)
#include "tst_tci_audio_roundtrip.moc"

#else  // !HAVE_WEBSOCKETS

// WebSockets not available — test file must still compile and produce a
// no-op binary so CTest does not report a missing executable.
int main() { return 0; }

#endif // HAVE_WEBSOCKETS
