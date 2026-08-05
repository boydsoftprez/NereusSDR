// =================================================================
// tests/tst_port_audio_capture_resample.cpp  (NereusSDR-native)
// =================================================================
// 2026-07-27  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//
// Regression coverage for the native-rate mic capture path added in
// PR #291 (open the device at its native rate, resample on our clock
// with r8brain to bypass the CoreAudio AUHAL SRC).
//
// Codex review raised two defects in that path that only bite on a
// device whose native rate differs from the requested 48 kHz -- which
// no CI runner has, and which tst_port_audio_bus therefore QSKIPs
// past.  Both live in pure logic that PortAudioBus exposes as static
// helpers precisely so they can be exercised here without a device:
//
//   * downmixToMono() previously wrote through a fixed 1024-float
//     stack array, silently discarding every frame past 1024.  The
//     Audio setup page offers buffer sizes up to 2048, so a stereo
//     capture at 2048 frames lost more than half of every callback.
//
//   * reportedCaptureChannels() -- negotiatedFormat() reported the
//     device's channel count while the callback pushed downmixed mono
//     into the ring.  AudioEngine::pullTxMic() strides by the reported
//     channel count, so it read two samples per frame out of a mono
//     ring: half-rate, choppy TX audio.
// =================================================================
#include <QtTest/QtTest>

#include "core/audio/PortAudioBus.h"

#include <vector>

using namespace NereusSDR;

class TstPortAudioCaptureResample : public QObject {
    Q_OBJECT

private slots:
    // ---- reportedCaptureChannels ----------------------------------

    // Not resampling: report exactly what the stream carries, so
    // pullTxMic keeps striding interleaved frames as it always has.
    void reportsStreamChannelsWhenNotResampling()
    {
        QCOMPARE(PortAudioBus::reportedCaptureChannels(1, false), 1);
        QCOMPARE(PortAudioBus::reportedCaptureChannels(2, false), 2);
        QCOMPARE(PortAudioBus::reportedCaptureChannels(4, false), 4);
    }

    // Resampling: the callback downmixes, so the ring holds one float
    // per frame no matter how many channels the device delivers.
    // Reporting anything but 1 halves the effective rate downstream.
    void reportsMonoWhenResampling()
    {
        QCOMPARE(PortAudioBus::reportedCaptureChannels(1, true), 1);
        QCOMPARE(PortAudioBus::reportedCaptureChannels(2, true), 1);
        QCOMPARE(PortAudioBus::reportedCaptureChannels(8, true), 1);
    }

    void reportedChannelsNeverZero()
    {
        QCOMPARE(PortAudioBus::reportedCaptureChannels(0, false), 1);
        QCOMPARE(PortAudioBus::reportedCaptureChannels(-3, false), 1);
    }

    // ---- downmixToMono --------------------------------------------

    void downmixAveragesStereoPairs()
    {
        const std::vector<float> in{
            0.0f, 1.0f,     // -> 0.5
            -1.0f, 1.0f,    // -> 0.0
            0.4f, 0.6f,     // -> 0.5
        };
        std::vector<float> out(3, -99.0f);

        const int n = PortAudioBus::downmixToMono(in.data(), 3, 2,
                                                  out.data(),
                                                  static_cast<int>(out.size()));
        QCOMPARE(n, 3);
        QVERIFY(qFuzzyCompare(out[0] + 1.0f, 0.5f + 1.0f));
        QVERIFY(qFuzzyCompare(out[1] + 1.0f, 0.0f + 1.0f));
        QVERIFY(qFuzzyCompare(out[2] + 1.0f, 0.5f + 1.0f));
    }

    void downmixHandlesMoreThanTwoChannels()
    {
        const std::vector<float> in{
            0.0f, 0.5f, 1.0f, 0.5f,   // mean 0.5
            1.0f, 1.0f, 1.0f, 1.0f,   // mean 1.0
        };
        std::vector<float> out(2, -99.0f);

        const int n = PortAudioBus::downmixToMono(in.data(), 2, 4,
                                                  out.data(),
                                                  static_cast<int>(out.size()));
        QCOMPARE(n, 2);
        QVERIFY(qFuzzyCompare(out[0] + 1.0f, 0.5f + 1.0f));
        QVERIFY(qFuzzyCompare(out[1] + 1.0f, 1.0f + 1.0f));
    }

    void downmixCopiesMonoUnchanged()
    {
        const std::vector<float> in{0.1f, -0.2f, 0.3f};
        std::vector<float> out(3, -99.0f);

        const int n = PortAudioBus::downmixToMono(in.data(), 3, 1,
                                                  out.data(),
                                                  static_cast<int>(out.size()));
        QCOMPARE(n, 3);
        for (int i = 0; i < 3; ++i) {
            QVERIFY(qFuzzyCompare(out[i] + 2.0f, in[static_cast<size_t>(i)] + 2.0f));
        }
    }

    // The finding itself: a 2048-frame stereo callback must not be
    // truncated to 1024.  With a correctly-sized destination every
    // frame survives, and the helper reports how many it wrote so the
    // caller can account for any shortfall instead of losing audio
    // silently.
    void downmixHandlesBufferSizesAbove1024()
    {
        constexpr int kFrames = 2048;
        std::vector<float> in(static_cast<size_t>(kFrames) * 2);
        for (int i = 0; i < kFrames; ++i) {
            in[static_cast<size_t>(i) * 2]     = 1.0f;
            in[static_cast<size_t>(i) * 2 + 1] = 1.0f;
        }
        std::vector<float> out(kFrames, 0.0f);

        const int n = PortAudioBus::downmixToMono(in.data(), kFrames, 2,
                                                  out.data(),
                                                  static_cast<int>(out.size()));
        QCOMPARE(n, kFrames);
        // Every frame written, including the ones past the old 1024 cap.
        QVERIFY(qFuzzyCompare(out[1024] + 1.0f, 2.0f));
        QVERIFY(qFuzzyCompare(out[kFrames - 1] + 1.0f, 2.0f));
    }

    // Capacity is a hard ceiling -- the helper must never write past
    // the caller's scratch, because in production it is a preallocated
    // vector being written from the real-time audio callback.
    void downmixClampsToOutputCapacity()
    {
        std::vector<float> in(600 * 2, 1.0f);
        std::vector<float> out(256, 0.0f);
        // Guard word: a sentinel just past the region we hand over.
        out.push_back(-42.0f);

        const int n = PortAudioBus::downmixToMono(in.data(), 600, 2,
                                                  out.data(), 256);
        QCOMPARE(n, 256);
        QVERIFY(qFuzzyCompare(out[256] + 100.0f, -42.0f + 100.0f));
    }

    void downmixRejectsDegenerateArguments()
    {
        std::vector<float> in(8, 1.0f);
        std::vector<float> out(8, 0.0f);

        QCOMPARE(PortAudioBus::downmixToMono(nullptr, 4, 2, out.data(), 8), 0);
        QCOMPARE(PortAudioBus::downmixToMono(in.data(), 4, 2, nullptr, 8), 0);
        QCOMPARE(PortAudioBus::downmixToMono(in.data(), 0, 2, out.data(), 8), 0);
        QCOMPARE(PortAudioBus::downmixToMono(in.data(), 4, 2, out.data(), 0), 0);
    }
};

QTEST_MAIN(TstPortAudioCaptureResample)
#include "tst_port_audio_capture_resample.moc"
