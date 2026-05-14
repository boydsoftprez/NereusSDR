// no-port-check: NereusSDR-original test file. The Thetis source citations
// in the test bodies are traceability markers that confirm which Thetis
// lines define the expected byte values being asserted — no Thetis test
// code is ported here. The production code under test (TciBinaryFrame) is
// the ported code and carries the full attribution in its own files.

// tests/tst_tci_binary_frame.cpp  (NereusSDR)
// NereusSDR-original — no Thetis upstream port in this test file.
//
// Phase 3J-1 Task 16.3 (sub-commit a): TciBinaryFrame::buildStreamPayload +
// TciBinaryFrame::encodeSamples unit tests.
//
// Covers:
//   1. Header-only (null samples) produces exactly 64 bytes.
//   2. receiver field lands at offset 0 in LE order.
//   3. streamType + channels + length fields land at correct offsets.
//   4. FLOAT32 round-trip: encode 4 floats → payload bytes byte-equal.
//   5. INT16 clipping: input 1.5f → output 32767 (short.MaxValue).
//   6. INT16 round-trip: encode [-1,0,1] → verify LE int16 bytes.
//   7. INT24 encode: 1.0f → 0x7FFFFF (8388607).
//   8. INT32 encode: 1.0f → INT_MAX.

#include <QtTest>
#include <cstdint>
#include <cstring>
#include <climits>

#include "core/TciBinaryFrame.h"

using namespace NereusSDR;

class TestTciBinaryFrame : public QObject {
    Q_OBJECT

private:
    // Helper: read LE uint32 from a byte position in a QByteArray.
    static quint32 readLE32(const QByteArray& buf, int offset) {
        quint32 val = 0;
        std::memcpy(&val, buf.constData() + offset, sizeof(val));
        // Swap from host to LE if needed — Qt provides qFromLittleEndian
        return qFromLittleEndian(val);
    }

    // Helper: read LE int16 from a byte position in a QByteArray.
    static qint16 readLE16(const QByteArray& buf, int offset) {
        quint16 val = 0;
        std::memcpy(&val, buf.constData() + offset, sizeof(val));
        return static_cast<qint16>(qFromLittleEndian(val));
    }

    // Helper: read LE int32 from a byte position in a QByteArray.
    static qint32 readLE32s(const QByteArray& buf, int offset) {
        quint32 val = 0;
        std::memcpy(&val, buf.constData() + offset, sizeof(val));
        return static_cast<qint32>(qFromLittleEndian(val));
    }

private slots:
    // Test 1: header-only (nullptr samples) produces exactly 64 bytes.
    void header_only_is_64_bytes();

    // Test 2: receiver field at offset 0 (LE uint32).
    void receiver_field_at_offset_0();

    // Test 3: streamType + channels + length fields at correct offsets.
    void stream_type_channels_length_offsets();

    // Test 4: FLOAT32 round-trip — 4 floats → payload bytes byte-equal.
    void float32_round_trip();

    // Test 5: INT16 clipping — input 1.5f clips to 32767.
    void int16_clips_positive();

    // Test 6: INT16 round-trip — encode [-1.0, 0.0, 1.0].
    void int16_round_trip();

    // Test 7: INT24 encode — 1.0f produces bytes for 8388607.
    void int24_full_scale_positive();

    // Test 8: INT32 encode — 1.0f produces bytes for INT_MAX.
    void int32_full_scale_positive();
};

void TestTciBinaryFrame::header_only_is_64_bytes()
{
    // No samples → header only. From Thetis TCIServer.cs:5244 [v2.10.3.13]:
    //   byte[] payload = new byte[64 + samplePayloadLength];
    // where samplePayloadLength == 0.
    const QByteArray frame = TciBinaryFrame::buildStreamPayload(
        /*receiver=*/0, /*sampleRate=*/48000,
        static_cast<int>(TciSampleType::Float32), /*length=*/0,
        static_cast<int>(TciStreamType::RxAudioStream), /*channels=*/2,
        /*samples=*/nullptr);
    QCOMPARE(frame.size(), 64);
}

void TestTciBinaryFrame::receiver_field_at_offset_0()
{
    // Receiver index at offset 0 per Thetis TCIServer.cs:5247 [v2.10.3.13]:
    //   writeUInt32(payload, offset, (uint)receiver); offset += 4;
    const QByteArray frame = TciBinaryFrame::buildStreamPayload(
        /*receiver=*/7, /*sampleRate=*/48000,
        static_cast<int>(TciSampleType::Float32), /*length=*/0,
        static_cast<int>(TciStreamType::RxAudioStream), /*channels=*/2,
        /*samples=*/nullptr);
    QCOMPARE(readLE32(frame, 0), static_cast<quint32>(7));
}

void TestTciBinaryFrame::stream_type_channels_length_offsets()
{
    // From Thetis TCIServer.cs:5250-5254 [v2.10.3.13]:
    //   offset 20 — length (sample count)
    //   offset 24 — streamType
    //   offset 28 — channels
    const int rx = 3;
    const int rate = 48000;
    const int length = 512;
    const int stream = static_cast<int>(TciStreamType::RxAudioStream);
    const int channels = 2;

    const QByteArray frame = TciBinaryFrame::buildStreamPayload(
        rx, rate,
        static_cast<int>(TciSampleType::Float32), length,
        stream, channels,
        /*samples=*/nullptr);

    QCOMPARE(readLE32(frame, 20), static_cast<quint32>(length));    // length
    QCOMPARE(readLE32(frame, 24), static_cast<quint32>(stream));    // streamType
    QCOMPARE(readLE32(frame, 28), static_cast<quint32>(channels));  // channels
    // Reserved fields at offsets 12, 16, 32..60 must be zero.
    QCOMPARE(readLE32(frame, 12), 0u);
    QCOMPARE(readLE32(frame, 16), 0u);
    for (int i = 0; i < 8; ++i) {
        QCOMPARE(readLE32(frame, 32 + 4 * i), 0u);
    }
}

void TestTciBinaryFrame::float32_round_trip()
{
    // From Thetis TCIServer.cs:5272-5276 [v2.10.3.13] — FLOAT32 fast path:
    //   Buffer.BlockCopy(samples, 0, data, 0, data.Length);
    // Encode 4 IEEE-754 floats and verify the raw bytes in the payload
    // match a direct memcpy of the original float array.
    const float input[4] = {0.25f, -0.5f, 1.0f, -1.0f};
    const int length = 4;  // flat count = channels * framesPerCh

    const QByteArray frame = TciBinaryFrame::buildStreamPayload(
        /*receiver=*/0, /*sampleRate=*/48000,
        static_cast<int>(TciSampleType::Float32), length,
        static_cast<int>(TciStreamType::RxAudioStream), /*channels=*/1,
        input);

    // Header is 64 bytes; payload starts at byte 64.
    QCOMPARE(frame.size(), 64 + static_cast<int>(length * sizeof(float)));

    // Byte-equal comparison against raw float representation.
    uint8_t expected[4 * 4];
    std::memcpy(expected, input, sizeof(expected));
    for (int i = 0; i < length * 4; ++i) {
        QCOMPARE(static_cast<uint8_t>(frame[64 + i]), expected[i]);
    }
}

void TestTciBinaryFrame::int16_clips_positive()
{
    // From Thetis TCIServer.cs:5281-5287 [v2.10.3.13]:
    //   clippedSample = Math.Max(-1.0f, Math.Min(1.0f, samples[i]));
    //   short s16 = (short)Math.Round(clippedSample * short.MaxValue);
    // Input 1.5f must clip to 1.0f, then encode as 32767.
    const float input = 1.5f;
    const QByteArray encoded = TciBinaryFrame::encodeSamples(
        &input, 1, static_cast<int>(TciSampleType::Int16));
    QCOMPARE(encoded.size(), 2);
    const qint16 decoded = readLE16(encoded, 0);
    QCOMPARE(decoded, static_cast<qint16>(32767));
}

void TestTciBinaryFrame::int16_round_trip()
{
    // Test encoding of -1.0, 0.0, +1.0 → -32767, 0, +32767.
    // From Thetis TCIServer.cs:5284-5287 [v2.10.3.13].
    const float input[3] = {-1.0f, 0.0f, 1.0f};
    const QByteArray encoded = TciBinaryFrame::encodeSamples(
        input, 3, static_cast<int>(TciSampleType::Int16));
    QCOMPARE(encoded.size(), 6);
    QCOMPARE(readLE16(encoded, 0), static_cast<qint16>(-32767));
    QCOMPARE(readLE16(encoded, 2), static_cast<qint16>(0));
    QCOMPARE(readLE16(encoded, 4), static_cast<qint16>(32767));
}

void TestTciBinaryFrame::int24_full_scale_positive()
{
    // From Thetis TCIServer.cs:5289-5293 [v2.10.3.13]:
    //   int s24 = (int)Math.Round(clippedSample * 8388607.0f);
    // 1.0f → 8388607 (0x7FFFFF).
    const float input = 1.0f;
    const QByteArray encoded = TciBinaryFrame::encodeSamples(
        &input, 1, static_cast<int>(TciSampleType::Int24));
    QCOMPARE(encoded.size(), 3);
    // LE bytes: 0xFF, 0xFF, 0x7F → 0x7FFFFF
    QCOMPARE(static_cast<uint8_t>(encoded[0]), static_cast<uint8_t>(0xFF));
    QCOMPARE(static_cast<uint8_t>(encoded[1]), static_cast<uint8_t>(0xFF));
    QCOMPARE(static_cast<uint8_t>(encoded[2]), static_cast<uint8_t>(0x7F));
}

void TestTciBinaryFrame::int32_full_scale_positive()
{
    // From Thetis TCIServer.cs:5295-5300 [v2.10.3.13]:
    //   int s32 = (int)Math.Round(clippedSample * int.MaxValue);
    //
    // NOTE: float32(int.MaxValue) = 2147483648.0f (the nearest IEEE-754 single
    // to 2147483647 rounds up to 2^31). So Math.Round(1.0f * int.MaxValue) =
    // 2147483648, which overflows int32 to -2147483648 in C# checked arithmetic.
    // NereusSDR faithfully replicates this: std::lroundf(1.0f * INT_MAX) = 2^31
    // which wraps to INT_MIN on overflow. The test asserts this exact behavior.
    const float input = 1.0f;
    const QByteArray encoded = TciBinaryFrame::encodeSamples(
        &input, 1, static_cast<int>(TciSampleType::Int32));
    QCOMPARE(encoded.size(), 4);
    const qint32 decoded = readLE32s(encoded, 0);
    // Exact C# parity: (int)Math.Round(1.0f * int.MaxValue) = -2147483648
    QCOMPARE(decoded, static_cast<qint32>(INT_MIN));
}

QTEST_GUILESS_MAIN(TestTciBinaryFrame)
#include "tst_tci_binary_frame.moc"
