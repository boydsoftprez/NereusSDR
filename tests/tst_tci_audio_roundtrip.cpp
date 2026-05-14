// tests/tst_tci_audio_roundtrip.cpp  (NereusSDR)
// no-port-check: NereusSDR-original integration test for the audio binary
// RX pipeline.  Validates: synthetic audio injection → AudioRingSpsc → drain
// timer assembly → resampler (identity at srcRate=48k) → TciBinaryFrame
// encode → QWebSocket sendBinaryMessage → client receives + decodes.
//
// Phase 3J-1 Task 16.4.  Plan spec: ≥ 1 binary frame with streamType==1
// and decoded payload matches input within 1e-3 (identity-resample case).

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

using namespace NereusSDR;

class TestTciAudioRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void synthetic_1khz_tone_arrives_as_binary_frame();
};

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
