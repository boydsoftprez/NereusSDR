// no-port-check: NereusSDR-original unit-test file.  RADE audio
// routing is NereusSDR-native; no Thetis equivalent.
// =================================================================
// tests/tst_audio_engine_rade.cpp  (NereusSDR)
// =================================================================
//
// Phase 3R Task J4 unit tests: RadeChannel::rxSpeechReady is routed
// into AudioEngine via the same speakers bus path as the WDSP
// RxChannel.  The connection is set up by
// RadioModel::wireRadeChannel(sliceId, channel, slice) (which now
// adds the audio connection on top of the I5 signal graph) and
// emits float32 stereo PCM into AudioEngine::rxBlockReady through
// a per-slice adapter slot.
//
// RadeChannel emits QByteArray of interleaved float32 stereo at
// 24 kHz; AudioEngine::rxBlockReady takes (const float*, int frames)
// for interleaved stereo float.  The adapter slot reinterprets the
// byte buffer as a float pointer and calls through.
//
// Tests use a small RadeChannel test subclass exposing an
// emit-helper for rxSpeechReady so we can drive the audio path
// without standing up the full I2 RX pipeline (which would require
// a live RADE codec + I/Q feed).
//
// Test case (1):
//   1. switchToRadeRoutesAudioToAudioEngine - emitting rxSpeechReady
//      with a small PCM block after wireRadeChannel must push to the
//      FakeAudioBus speakers stand-in.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11 - New test file for Phase 3R Task J4.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/RadeChannel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>

using namespace NereusSDR;

namespace {

// Test seam: RadeChannel subclass exposing a thin wrapper around
// emit rxSpeechReady so the test can drive the audio path without
// the I2 RX pipeline standing up a live librade decoder.
class TestableRadeChannel : public RadeChannel {
    Q_OBJECT
public:
    using RadeChannel::RadeChannel;

    void emitRxSpeechReadyForTest(const QByteArray& pcm) {
        emit rxSpeechReady(pcm);
    }
};

// Build an N-frame interleaved-stereo float32 PCM block at 24 kHz
// (the rate RadeChannel emits on rxSpeechReady).  Bench-fix path in
// RadioModel::wireRadeChannel upsamples 24 -> 48 kHz before pushing
// to AudioEngine; the lazy-built per-leg Resampler has a 4096-sample
// internal window, so the first push of a tiny block produces zero
// output frames ("resampler warmup") and never reaches the speakers
// bus.  Sizing the test block to 1024 frames (~42 ms) clears the
// warmup on the first call and gives the test something to count.
QByteArray makeStereoFloatPcm(int frames) {
    QByteArray buf;
    buf.resize(frames * 2 * static_cast<int>(sizeof(float)));
    auto* dst = reinterpret_cast<float*>(buf.data());
    for (int i = 0; i < frames; ++i) {
        // Cheap deterministic content: small sine-ish ramp.  Magnitude
        // stays inside the speakers-bus float32 range.
        const float v = 0.1f * static_cast<float>((i % 17) - 8) / 8.0f;
        dst[2 * i + 0] = v;
        dst[2 * i + 1] = v;
    }
    return buf;
}

} // namespace

class TstAudioEngineRade : public QObject {
    Q_OBJECT

private slots:

    // ── switchToRadeRoutesAudioToAudioEngine ───────────────────────────
    //
    // Wiring contract for J4: after RadioModel::wireRadeChannel(0,
    // channel, slice), emitting RadeChannel::rxSpeechReady with a
    // valid float32 stereo block must reach the speakers bus through
    // AudioEngine::rxBlockReady.  Verified by FakeAudioBus push count.
    void switchToRadeRoutesAudioToAudioEngine() {
        // Standard harness mirrors tst_audio_engine_rx_leak_during_mox:
        // RadioModel + audioEngine + FakeSpeakers injected via
        // setSpeakersBusForTest.
        auto radio = std::make_unique<RadioModel>();
        AudioEngine* engine = radio->audioEngine();

        auto speakers = std::make_unique<FakeAudioBus>(
            QStringLiteral("FakeSpeakers"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 2;
        fmt.sample     = AudioFormat::Sample::Float32;
        speakers->open(fmt);
        FakeAudioBus* speakersRaw = speakers.get();
        engine->setSpeakersBusForTest(std::move(speakers));

        // Register the mixer slots, as connecting to a radio does
        // (RadioModel::configureStreamPool -> AudioEngine::preregisterSlices).
        // MasterMixer::accumulate() drops any slice id it has no entry for,
        // so without this the RADE speech block never reaches the mix. It
        // used to look like it did, because the speakers push was
        // unconditional and an empty push still incremented pushCount().
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);

        const int sliceId = radio->addSlice();
        SliceModel* slice = radio->sliceById(sliceId);
        QVERIFY(slice != nullptr);

        // Wire the RADE channel.  J4's audio connection is added inside
        // RadioModel::wireRadeChannel alongside the I5 signal graph.
        TestableRadeChannel channel;
        radio->wireRadeChannel(sliceId, &channel, slice);

        // Baseline: speakers bus has not been pushed yet.
        const int baselinePushes = speakersRaw->pushCount();
        QCOMPARE(baselinePushes, 0);

        // Drive the audio path.  r8brain CDSPResampler24 ships with
        // ReqAtten=206.91 dB by default — a very long filter with
        // latency in the thousands of input samples (same caveat
        // tst_rade_tx_pump:170-178 documents).  Pump 4 emissions of
        // 1024 stereo frames at 24 kHz to fully clear the warmup
        // window and guarantee at least one 48 kHz output block
        // reaches the speakers bus on subsequent calls.
        const QByteArray pcm = makeStereoFloatPcm(1024);
        for (int rep = 0; rep < 4; ++rep) {
            channel.emitRxSpeechReadyForTest(pcm);
        }

        QVERIFY2(speakersRaw->pushCount() > baselinePushes,
                 "wireRadeChannel must connect RadeChannel::rxSpeechReady "
                 "through AudioEngine::rxBlockReady so emitting a valid "
                 "PCM block reaches the speakers bus");
    }
};

QTEST_GUILESS_MAIN(TstAudioEngineRade)
#include "tst_audio_engine_rade.moc"
