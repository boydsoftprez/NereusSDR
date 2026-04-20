// =================================================================
// tests/tst_audio_engine_vax_tee.cpp  (NereusSDR)
// =================================================================
//
// Exercises AudioEngine's RX → VAX bus tee logic — Phase 3O Sub-Phase 8.5.
//
// Coverage: see private-slot test methods below — each verifies one
// routing-contract scenario.
//
// Uses FakeAudioBus injected via the NEREUS_BUILD_TESTS-only
// AudioEngine::setVaxBusForTest / setSpeakersBusForTest seam, so the
// test doesn't need a real CoreAudioHalBus / LinuxPipeBus / PortAudio
// backend. Cross-platform.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <array>
#include <memory>

using namespace NereusSDR;

namespace {

// Standard test block: 2 frames of stereo float (4 floats / 16 bytes).
// Matches what rxBlockReady will forward to push() byte-for-byte.
constexpr int kTestFrames = 2;
constexpr int kTestStereoFloats = kTestFrames * 2;
constexpr qint64 kExpectedPushBytes = static_cast<qint64>(kTestStereoFloats) * sizeof(float);

const std::array<float, kTestStereoFloats> kTestSamples = {
    0.10f, 0.20f,   // frame 0  L,R
    -0.30f, -0.40f, // frame 1  L,R
};

} // namespace

class TstAudioEngineVaxTee : public QObject {
    Q_OBJECT

private:
    // Build a RadioModel + AudioEngine pair, register slice 0 with the
    // master mixer (mirrors AudioEngine::start() pre-registration), and
    // hand the engine a fake speakers bus so the speaker push() in
    // rxBlockReady is observable. RadioModel owns AudioEngine so we just
    // hold a unique_ptr to RadioModel and reach into it.
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        AudioEngine* engine;        // non-owning view
        FakeAudioBus* speakers;     // non-owning view (engine owns it)

        // Add a slice with the given vaxChannel value; returns its index.
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

        // Inject the fake speakers bus before any rxBlockReady call. We
        // do NOT call engine->start() — that would try to construct
        // platform-native VAX buses, and on Mac CI without the HAL
        // plugin installed CoreAudioHalBus::open could noisily fail.
        // The test only cares about the tee logic, which runs
        // independent of the m_running flag (rxBlockReady gates on
        // m_radio + samples + frames, not m_running).
        auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        speakers->open(fmt);
        h.speakers = speakers.get();
        h.engine->setSpeakersBusForTest(std::move(speakers));

        return h;
    }

    // Inject a fresh fake bus at the given VAX channel; returns a
    // non-owning view for inspection.
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

private slots:

    // ── 1. Fan-in: two slices on the same VAX channel both push ─────────────

    void fanIn() {
        Harness h = makeHarness();
        FakeAudioBus* vax2 = injectFakeVax(h.engine, 2);

        const int s1 = h.addSlice(/*vaxChannel=*/2);
        const int s2 = h.addSlice(/*vaxChannel=*/2);

        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax2->pushCount(), 2);
        QCOMPARE(static_cast<qint64>(vax2->buffer().size()),
                 kExpectedPushBytes * 2);
    }

    // ── 2. vaxChannel == 0 → no VAX push ────────────────────────────────────

    void vaxOffSkipsBus() {
        Harness h = makeHarness();
        // Inject fakes on every channel so we can prove none of them
        // received a push.
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);
        FakeAudioBus* vax2 = injectFakeVax(h.engine, 2);
        FakeAudioBus* vax3 = injectFakeVax(h.engine, 3);
        FakeAudioBus* vax4 = injectFakeVax(h.engine, 4);

        const int s = h.addSlice(/*vaxChannel=*/0);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 0);
        QCOMPARE(vax2->pushCount(), 0);
        QCOMPARE(vax3->pushCount(), 0);
        QCOMPARE(vax4->pushCount(), 0);
    }

    // ── 3. Speakers tee runs regardless of vaxChannel ──────────────────────

    void vaxDoesNotGateSpeakers() {
        Harness h = makeHarness();
        FakeAudioBus* vax3 = injectFakeVax(h.engine, 3);

        const int s = h.addSlice(/*vaxChannel=*/3);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        // VAX3 saw the push.
        QCOMPARE(vax3->pushCount(), 1);
        QCOMPARE(static_cast<qint64>(vax3->lastPushBytes()), kExpectedPushBytes);

        // Speakers also saw the push (the master-mix flush at the end of
        // rxBlockReady). The size matches because frames is identical and
        // the mixer is stereo.
        QCOMPARE(h.speakers->pushCount(), 1);
        QCOMPARE(static_cast<qint64>(h.speakers->lastPushBytes()),
                 kExpectedPushBytes);
    }

    // Repeat (3) but with vaxChannel == 0: speakers still get the audio.
    void vaxOffStillFeedsSpeakers() {
        Harness h = makeHarness();
        const int s = h.addSlice(/*vaxChannel=*/0);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(h.speakers->pushCount(), 1);
        QCOMPARE(static_cast<qint64>(h.speakers->lastPushBytes()),
                 kExpectedPushBytes);
    }

    // ── 4. Null VAX slot is a safe no-op ───────────────────────────────────

    void nullVaxSlotIsSafeNoOp() {
        Harness h = makeHarness();
        // Deliberately do NOT inject a VAX bus on channel 4.
        const int s = h.addSlice(/*vaxChannel=*/4);

        // Should not crash, should not throw, should still flush the
        // mix to speakers.
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(h.speakers->pushCount(), 1);
    }

    // ── 5. Closed bus is gated by isOpen() guard ───────────────────────────

    void closedVaxBusSkipsPush() {
        Harness h = makeHarness();
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);
        vax1->setForceOpen(false);  // bus exists, but isOpen() → false

        const int s = h.addSlice(/*vaxChannel=*/1);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 0);
        // Speakers tee still runs — VAX failure must not break the
        // primary RX audio path.
        QCOMPARE(h.speakers->pushCount(), 1);
    }

    // ── 6. Out-of-range vaxChannel values are ignored (defensive) ──────────

    void outOfRangeVaxChannelIgnored() {
        Harness h = makeHarness();
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);
        FakeAudioBus* vax4 = injectFakeVax(h.engine, 4);

        // vaxChannel = 5 is invalid (> 4); rxBlockReady's guard rejects.
        const int s = h.addSlice(/*vaxChannel=*/5);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(vax1->pushCount(), 0);
        QCOMPARE(vax4->pushCount(), 0);
        // Speakers still gets the audio (vaxChannel is a tap, not a gate).
        QCOMPARE(h.speakers->pushCount(), 1);
    }
};

QTEST_MAIN(TstAudioEngineVaxTee)
#include "tst_audio_engine_vax_tee.moc"
