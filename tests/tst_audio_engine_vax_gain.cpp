// =================================================================
// tests/tst_audio_engine_vax_gain.cpp  (NereusSDR)
// =================================================================
//
// Exercises AudioEngine's per-channel VAX rx-gain / mute / tx-gain
// setters and their effect on the rxBlockReady tee — Phase 3O
// Sub-Phase 9 Task 9.2a.
//
// Coverage: see private-slot test methods below — each verifies one
// gain/mute or setter/signal contract scenario.
//
// Uses FakeAudioBus injected via the NEREUS_BUILD_TESTS-only
// AudioEngine::setVaxBusForTest / setSpeakersBusForTest seam, so the
// test doesn't need a real CoreAudioHalBus / LinuxPipeBus / PortAudio
// backend. Cross-platform.
// =================================================================

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <array>
#include <cstring>
#include <memory>

using namespace NereusSDR;

namespace {

// Standard test block: 2 frames of stereo float (4 floats / 16 bytes).
constexpr int kTestFrames = 2;
constexpr int kTestStereoFloats = kTestFrames * 2;
constexpr qint64 kExpectedPushBytes =
    static_cast<qint64>(kTestStereoFloats) * sizeof(float);

// Chosen so that * 0.5 produces exact representable floats — keeps
// the direct byte-compare in gainHalfHalvesRms deterministic.
const std::array<float, kTestStereoFloats> kTestSamples = {
    0.4f,  0.6f,   // frame 0  L,R
    -0.2f, -0.8f,  // frame 1  L,R
};

} // namespace

class TstAudioEngineVaxGain : public QObject {
    Q_OBJECT

private:
    // Same harness pattern as tst_audio_engine_vax_tee — duplicated here
    // so each test binary stays independently runnable.
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        AudioEngine* engine;        // non-owning view
        FakeAudioBus* speakers;     // non-owning view (engine owns it)

        int addSlice(int vaxChannel) {
            const int idx = radio->addSlice();
            SliceModel* slice = radio->sliceAt(idx);
            slice->setVaxChannel(vaxChannel);
            return idx;
        }
    };

    Harness makeHarness() {
        Harness h;
        h.radio = std::make_unique<RadioModel>();
        h.engine = h.radio->audioEngine();

        auto speakers = std::make_unique<FakeAudioBus>(
            QStringLiteral("FakeSpeakers"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        speakers->open(fmt);
        h.speakers = speakers.get();
        h.engine->setSpeakersBusForTest(std::move(speakers));

        return h;
    }

    FakeAudioBus* injectFakeVax(AudioEngine* engine, int channel) {
        auto bus = std::make_unique<FakeAudioBus>(
            QStringLiteral("FakeVax%1").arg(channel));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        bus->open(fmt);
        FakeAudioBus* view = bus.get();
        engine->setVaxBusForTest(channel, std::move(bus));
        return view;
    }

    // Copy the fake's pushed buffer into a stereo-float span for
    // byte-for-byte comparison.
    static std::array<float, kTestStereoFloats>
    bufferAsFloats(const FakeAudioBus* bus)
    {
        std::array<float, kTestStereoFloats> out{};
        const QByteArray& buf = bus->buffer();
        Q_ASSERT(buf.size() >= static_cast<int>(kExpectedPushBytes));
        std::memcpy(out.data(), buf.constData(), kExpectedPushBytes);
        return out;
    }

private slots:

    // ── 1. Unity gain is a byte-for-byte passthrough ────────────────────────

    void gainUnityPassesThrough() {
        Harness h = makeHarness();
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);

        h.engine->setVaxRxGain(1, 1.0f);
        const int s = h.addSlice(/*vaxChannel=*/1);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 1);
        QCOMPARE(static_cast<qint64>(vax1->lastPushBytes()),
                 kExpectedPushBytes);
        const auto got = bufferAsFloats(vax1);
        for (int i = 0; i < kTestStereoFloats; ++i) {
            QCOMPARE(got[i], kTestSamples[i]);
        }
    }

    // ── 2. Gain = 0.5 halves every sample (direct byte compare) ─────────────

    void gainHalfHalvesRms() {
        Harness h = makeHarness();
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);

        h.engine->setVaxRxGain(1, 0.5f);
        const int s = h.addSlice(/*vaxChannel=*/1);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 1);
        const auto got = bufferAsFloats(vax1);
        const std::array<float, kTestStereoFloats> expected = {
            0.2f, 0.3f, -0.1f, -0.4f,
        };
        for (int i = 0; i < kTestStereoFloats; ++i) {
            QCOMPARE(got[i], expected[i]);
        }
    }

    // ── 3. Gain = 0.0 pushes an all-zero block (push still happens) ─────────

    void gainZeroPushesZeros() {
        Harness h = makeHarness();
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);

        h.engine->setVaxRxGain(1, 0.0f);
        const int s = h.addSlice(/*vaxChannel=*/1);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 1);
        const auto got = bufferAsFloats(vax1);
        for (int i = 0; i < kTestStereoFloats; ++i) {
            QCOMPARE(got[i], 0.0f);
        }
    }

    // ── 4. Muted VAX channel skips the push entirely ────────────────────────

    void mutedSkipsPush() {
        Harness h = makeHarness();
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);

        h.engine->setVaxMuted(1, true);
        const int s = h.addSlice(/*vaxChannel=*/1);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 0);
        // Speakers still receive audio — VAX mute must not gate speakers.
        QCOMPARE(h.speakers->pushCount(), 1);
    }

    // ── 5. Mute overrides gain (mute wins regardless of gain value) ─────────

    void mutedOverridesGain() {
        Harness h = makeHarness();
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);

        h.engine->setVaxRxGain(1, 1.0f);
        h.engine->setVaxMuted(1, true);
        const int s = h.addSlice(/*vaxChannel=*/1);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 0);
    }

    // ── 6. Fresh AudioEngine with no setter calls → passthrough ────────────

    void gainDefaultsToUnity() {
        Harness h = makeHarness();
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);

        // No setVaxRxGain / setVaxMuted calls — relying on defaults.
        QCOMPARE(h.engine->vaxRxGain(1), 1.0f);
        QCOMPARE(h.engine->vaxMuted(1), false);

        const int s = h.addSlice(/*vaxChannel=*/1);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 1);
        const auto got = bufferAsFloats(vax1);
        for (int i = 0; i < kTestStereoFloats; ++i) {
            QCOMPARE(got[i], kTestSamples[i]);
        }
    }

    // ── 7. setVaxRxGain emits change-signal with channel + value ───────────

    void setVaxRxGainEmitsSignal() {
        Harness h = makeHarness();
        QSignalSpy spy(h.engine, &AudioEngine::vaxRxGainChanged);

        h.engine->setVaxRxGain(2, 0.25f);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 2);
        QCOMPARE(args.at(1).toFloat(), 0.25f);
    }

    // ── 8. setVaxRxGain does not emit when value is unchanged ──────────────

    void setVaxRxGainNoSignalOnNoChange() {
        Harness h = makeHarness();
        h.engine->setVaxRxGain(1, 0.5f);

        QSignalSpy spy(h.engine, &AudioEngine::vaxRxGainChanged);
        h.engine->setVaxRxGain(1, 0.5f);

        QCOMPARE(spy.count(), 0);
    }

    // ── 9. Out-of-range gain values are clamped to [0,1] ───────────────────

    void setVaxRxGainClampsOutOfRange() {
        Harness h = makeHarness();

        h.engine->setVaxRxGain(1, 2.5f);
        QCOMPARE(h.engine->vaxRxGain(1), 1.0f);

        h.engine->setVaxRxGain(1, -0.3f);
        QCOMPARE(h.engine->vaxRxGain(1), 0.0f);
    }

    // ── 10. setVaxMuted emits signal on change ─────────────────────────────

    void setVaxMutedEmitsSignal() {
        Harness h = makeHarness();
        QSignalSpy spy(h.engine, &AudioEngine::vaxMutedChanged);

        h.engine->setVaxMuted(3, true);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 3);
        QCOMPARE(args.at(1).toBool(), true);

        // No-change is quiet.
        h.engine->setVaxMuted(3, true);
        QCOMPARE(spy.count(), 0);
    }

    // ── 11. setVaxTxGain stores + emits (no consumer path yet) ─────────────

    void setVaxTxGainEmitsSignal() {
        Harness h = makeHarness();
        QSignalSpy spy(h.engine, &AudioEngine::vaxTxGainChanged);

        h.engine->setVaxTxGain(0.75f);
        QCOMPARE(h.engine->vaxTxGain(), 0.75f);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toFloat(), 0.75f);

        // Clamp symmetry.
        h.engine->setVaxTxGain(2.0f);
        QCOMPARE(h.engine->vaxTxGain(), 1.0f);
        h.engine->setVaxTxGain(-1.0f);
        QCOMPARE(h.engine->vaxTxGain(), 0.0f);
    }

    // ── 12. Invalid channel numbers are no-ops (no crash, no persist) ──────

    void invalidChannelIsNoOp() {
        Harness h = makeHarness();
        QSignalSpy spy(h.engine, &AudioEngine::vaxRxGainChanged);

        // Seed channel 1 to a known value so we can prove nothing bled
        // across channels via an OOB index.
        h.engine->setVaxRxGain(1, 0.75f);
        spy.clear();

        h.engine->setVaxRxGain(0, 0.5f);  // below range
        h.engine->setVaxRxGain(5, 0.5f);  // above range

        QCOMPARE(spy.count(), 0);
        QCOMPARE(h.engine->vaxRxGain(1), 0.75f);

        // Prove channel 1 is still writable after the OOB attempts.
        h.engine->setVaxRxGain(1, 0.25f);
        QCOMPARE(h.engine->vaxRxGain(1), 0.25f);

        // Mirror check for muted setter.
        h.engine->setVaxMuted(0, true);
        h.engine->setVaxMuted(5, true);
        QCOMPARE(h.engine->vaxMuted(1), false);
    }
};

QTEST_MAIN(TstAudioEngineVaxGain)
#include "tst_audio_engine_vax_gain.moc"
