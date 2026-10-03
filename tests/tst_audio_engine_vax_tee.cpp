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

#include <QSemaphore>

#include <array>
#include <atomic>
#include <memory>
#include <thread>

using namespace NereusSDR;

namespace {

// A VAX output whose pacing query holds until the test lets it go, so a
// thread can sit inside AudioEngine's bus lock (vaxOutputPacing) while the
// DSP thread's tee runs. Counts any push made while that thread is inside.
class PacingHoldBus : public FakeAudioBus {
public:
    PacingHoldBus() : FakeAudioBus(QStringLiteral("PacingHoldVax")) {}
    std::optional<OutputPacing> outputPacing() const override
    {
        m_inside.store(true);
        entered.release();
        release.acquire();
        m_inside.store(false);
        return std::nullopt;
    }
    qint64 push(const char* data, qint64 bytes) override
    {
        if (m_inside.load()) {
            pushesWhileHeld.fetch_add(1);
        }
        return FakeAudioBus::push(data, bytes);
    }
    mutable QSemaphore entered;
    mutable QSemaphore release;
    std::atomic<int> pushesWhileHeld{0};

private:
    mutable std::atomic<bool> m_inside{false};
};

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
            SliceModel* slice = radio->sliceById(idx);
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

        // Register the mixer slots, exactly as connecting to a radio does
        // (RadioModel::configureStreamPool -> AudioEngine::preregisterSlices).
        //
        // MasterMixer::accumulate() drops any slice id it has no entry for,
        // so without this the block never reaches the mix at all. That went
        // unnoticed for as long as the speakers push was unconditional: an
        // empty push still incremented pushCount(), so these tests read as
        // passing while the audio was being discarded. Phase 3F gates the
        // push on the mixer actually producing a block, which turns the same
        // condition into pushCount() == 0.
        h.radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                     /*defaultRateHz=*/192000);

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

    // ── 1. Fan-in: two slices on one VAX channel make ONE summed block ────
    //
    // R-R3-44 (fix wave): each slice used to push its own block into the
    // channel's ring, two periods of audio per real period, so the app
    // reading the channel heard the ring overrun and garbled audio. The
    // channel now waits for every slice on it and pushes their sum once.

    void fanIn() {
        Harness h = makeHarness();
        FakeAudioBus* vax2 = injectFakeVax(h.engine, 2);

        const int s1 = h.addSlice(/*vaxChannel=*/2);
        const int s2 = h.addSlice(/*vaxChannel=*/2);

        // A slice joins a channel's mix with its first block, as it joins
        // the speakers mix: the first period carries slice 1 alone while
        // slice 2 joins, and slice 2's first block waits for the next.
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 1);

        // From then on, one summed block per period. The first slice alone
        // does not make it.
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 2);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 2);
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 3);

        QCOMPARE(static_cast<qint64>(vax2->buffer().size()), kExpectedPushBytes * 3);
        const auto* pushed = reinterpret_cast<const float*>(vax2->buffer().constData());
        for (int block = 1; block < 3; ++block) {
            for (int i = 0; i < kTestStereoFloats; ++i) {
                QCOMPARE(pushed[block * kTestStereoFloats + i],
                         2.0f * kTestSamples[static_cast<size_t>(i)]);
            }
        }
    }

    // One summed block per period, whichever slice delivers first.
    void fanInPushesOncePerPeriod() {
        Harness h = makeHarness();
        FakeAudioBus* vax2 = injectFakeVax(h.engine, 2);
        const int s1 = h.addSlice(/*vaxChannel=*/2);
        const int s2 = h.addSlice(/*vaxChannel=*/2);

        const std::array<float, kTestStereoFloats> other = {0.01f, 0.02f, 0.03f, 0.04f};
        for (int period = 0; period < 10; ++period) {
            if (period % 3 == 0) {
                h.engine->rxBlockReady(s2, other.data(), kTestFrames);
                h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
            } else {
                h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
                h.engine->rxBlockReady(s2, other.data(), kTestFrames);
            }
            QCOMPARE(vax2->pushCount(), period + 1);
        }
        QCOMPARE(static_cast<qint64>(vax2->buffer().size()), kExpectedPushBytes * 10);
        const auto* pushed = reinterpret_cast<const float*>(vax2->buffer().constData());
        // Period 0 is the join (slice 2 alone); every later block is the sum.
        for (int period = 1; period < 10; ++period) {
            for (int i = 0; i < kTestStereoFloats; ++i) {
                QCOMPARE(pushed[period * kTestStereoFloats + i],
                         kTestSamples[static_cast<size_t>(i)] + other[static_cast<size_t>(i)]);
            }
        }
    }

    // A slice moved to another channel stops holding its old channel.
    void aSliceMovedAwayLeavesTheChannel() {
        Harness h = makeHarness();
        FakeAudioBus* vax2 = injectFakeVax(h.engine, 2);
        FakeAudioBus* vax3 = injectFakeVax(h.engine, 3);
        const int s1 = h.addSlice(/*vaxChannel=*/2);
        const int s2 = h.addSlice(/*vaxChannel=*/2);
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 1);

        h.radio->sliceById(s2)->setVaxChannel(3);
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 3);
        QCOMPARE(vax3->pushCount(), 2);
        const auto* last = reinterpret_cast<const float*>(vax2->buffer().constData())
                           + 2 * kTestStereoFloats;
        for (int i = 0; i < kTestStereoFloats; ++i) {
            QCOMPARE(last[i], kTestSamples[static_cast<size_t>(i)]);
        }
    }

    // A slice withdrawn from the mix (a closed pan, the slice keyed for
    // transmit) stops holding its channel, as it stops holding the speakers.
    void aWithdrawnSliceDoesNotHoldTheChannel() {
        Harness h = makeHarness();
        FakeAudioBus* vax2 = injectFakeVax(h.engine, 2);
        const int s1 = h.addSlice(/*vaxChannel=*/2);
        const int s2 = h.addSlice(/*vaxChannel=*/2);
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 1);

        h.engine->setSliceStreaming(s2, false);
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 3);

        // Back in: it joins again with its next block.
        h.engine->setSliceStreaming(s2, true);
        h.engine->rxBlockReady(s2, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 3);
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax2->pushCount(), 4);
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

    // ── 5b. The tee keeps out of a bus another thread holds ─────────────────
    //
    // RD-I10 (fix wave 2026-09-30): the owner thread replaces a VAX output
    // and a remote window's feeder writes it, both under m_vaxBusMutex. The
    // tee on the DSP thread read the bus without the lock, so a replace
    // could free the bus under its push. It now try-locks the bus, as the
    // speakers push does, and drops the block while someone else holds it.
    void teeSkipsABusHeldByAnotherThread() {
        Harness h = makeHarness();
        auto bus = std::make_unique<PacingHoldBus>();
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        bus->open(fmt);
        PacingHoldBus* vax1 = bus.get();
        h.engine->setVaxBusForTest(1, std::move(bus));
        const int s = h.addSlice(/*vaxChannel=*/1);

        // Another thread inside the bus lock.
        std::thread holder([&h]() { (void)h.engine->vaxOutputPacing(1); });
        vax1->entered.acquire();
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);
        const int pushesWhileHeld = vax1->pushesWhileHeld.load();
        const int pushes = vax1->pushCount();
        vax1->release.release();
        holder.join();

        QCOMPARE(pushesWhileHeld, 0);
        QCOMPARE(pushes, 0);
        // The speakers still play the block.
        QCOMPARE(h.speakers->pushCount(), 1);

        // Free again: the next block reaches the bus.
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);
        QCOMPARE(vax1->pushCount(), 1);
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
