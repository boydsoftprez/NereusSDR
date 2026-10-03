// =================================================================
// tests/tst_slice_audio_view_race.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. R-R3-49: the audio thread must never walk the slice
// list or touch a SliceModel. AudioEngine::rxBlockReady used to call
// RadioModel::sliceById, iterating m_slices on the DSP thread while
// removeSliceImpl took the slice out of the list (takeAt) and later deleted
// it on the main thread: a use-after-free on the audio thread whenever a
// slice was removed while audio ran (ThreadSanitizer, reconnect-crash
// report, finding 2).
//
// Coverage:
//   aRemovedSliceFeedsNothing        a block for a removed id reaches no mix
//   audioViewFollowsTheSlice         mute / route / VAX changes reach the
//                                    audio side's view of the slice
//   audioViewFollowsALayoutRestore   a restore that sets the slice with its
//                                    signals blocked still reaches the view
//   addingAndRemovingSlicesWhileBlocksFlow
//                                    stress: a feeder thread delivers blocks
//                                    for every id while the main thread adds
//                                    and removes slices (run under TSan)
//   audioViewCarriesTheAfLevel       the slice's AF level reaches the view
//   listenChurnWhileBlocksFlow       stress: a feeder thread delivers blocks
//                                    while the main thread joins, leaves and
//                                    changes listen levels (run under TSan)
//
// Modification history (NereusSDR):
//   2026-09-29 - Slice control plan Task 6: the AF level in the view and the
//                listen churn stress. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - Load finding: both stress cases wait for the feeder's first
//                block before the churn starts, so the churn overlaps real
//                block flow on a busy computer too. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <array>
#include <atomic>
#include <memory>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr int kTestFrames = 2;
const std::array<float, kTestFrames * 2> kTestSamples = {0.10f, 0.20f, -0.30f, -0.40f};

// How long a stress case waits for its feeder thread's first block before
// the churn starts. The test's own bound (no product deadline applies): the
// feeder only has to be scheduled once, which takes microseconds on a quiet
// computer and was not done within the main thread's whole churn at a load
// of 41 to 98.
constexpr int kFeederFirstBlockMs = 10000;

// Stops and joins a stress case's feeder however the case leaves, so a
// failed wait cannot destroy a joinable std::thread.
struct FeederStop {
    std::atomic<bool>& stop;
    std::thread& feeder;
    ~FeederStop()
    {
        stop.store(true, std::memory_order_release);
        if (feeder.joinable()) {
            feeder.join();
        }
    }
};

struct Harness {
    std::unique_ptr<RadioModel> radio;
    AudioEngine* engine{nullptr};
    FakeAudioBus* speakers{nullptr};
};

Harness makeHarness()
{
    Harness h;
    h.radio = std::make_unique<RadioModel>();
    h.engine = h.radio->audioEngine();
    auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
    AudioFormat fmt;
    fmt.sampleRate = 48000;
    fmt.channels = 2;
    fmt.sample = AudioFormat::Sample::Float32;
    speakers->open(fmt);
    h.speakers = speakers.get();
    h.engine->setSpeakersBusForTest(std::move(speakers));
    h.radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                 /*defaultRateHz=*/192000);
    return h;
}

} // namespace

class TstSliceAudioViewRace : public QObject {
    Q_OBJECT

private slots:
    void aRemovedSliceFeedsNothing()
    {
        Harness h = makeHarness();
        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();
        QVERIFY(a >= 0 && b >= 0 && a != b);

        QVERIFY(h.engine->sliceAudioView(b).present);
        h.radio->removeSlice(b);
        QVERIFY(!h.engine->sliceAudioView(b).present);

        // Only `a` is left: its block drains the mix once; `b`'s is dropped
        // before it reaches any mixer, so it adds no push of its own.
        const int pushes = h.speakers->pushCount();
        h.engine->rxBlockReady(b, kTestSamples.data(), kTestFrames);
        QCOMPARE(h.speakers->pushCount(), pushes);
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        QCOMPARE(h.speakers->pushCount(), pushes + 1);
    }

    void audioViewFollowsTheSlice()
    {
        Harness h = makeHarness();
        const int a = h.radio->addSlice();
        SliceModel* slice = h.radio->sliceById(a);
        QVERIFY(slice != nullptr);

        AudioEngine::SliceAudioView v = h.engine->sliceAudioView(a);
        QVERIFY(v.present);
        QCOMPARE(v.muted, slice->muted());

        slice->setMuted(!slice->muted());
        QCOMPARE(h.engine->sliceAudioView(a).muted, slice->muted());

        slice->setVaxChannel(3);
        QCOMPARE(h.engine->sliceAudioView(a).vaxChannel, 3);

        slice->setOutputRoute(SliceModel::OutputRoute::Headphones);
        QVERIFY(h.engine->sliceAudioView(a).headphones);
        slice->setOutputRoute(SliceModel::OutputRoute::Speakers);
        QVERIFY(!h.engine->sliceAudioView(a).headphones);
    }

    void audioViewFollowsALayoutRestore()
    {
        // A change made while the slice's signals are blocked (the layout
        // restore does this) must still reach the view once the slice list
        // is republished; the publisher reads the slices, not the signals.
        Harness h = makeHarness();
        const int a = h.radio->addSlice();
        SliceModel* slice = h.radio->sliceById(a);
        const bool before = slice->muted();
        {
            const QSignalBlocker block(slice);
            slice->setMuted(!before);
        }
        QCOMPARE(h.engine->sliceAudioView(a).muted, before);
        h.radio->publishSliceAudioViewForTest();
        QCOMPARE(h.engine->sliceAudioView(a).muted, !before);
    }

    void addingAndRemovingSlicesWhileBlocksFlow()
    {
        Harness h = makeHarness();
        const int anchor = h.radio->addSlice();
        QVERIFY(anchor >= 0);

        std::atomic<bool> stop{false};
        std::atomic<long> blocks{0};
        std::thread feeder([&] {
            while (!stop.load(std::memory_order_acquire)) {
                for (int id = 0; id < 5; ++id) {
                    h.engine->rxBlockReady(id, kTestSamples.data(), kTestFrames);
                    blocks.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
        FeederStop feederStop{stop, feeder};
        // The churn must overlap real block flow; without this the main
        // thread could finish every round before the feeder ran once.
        QTRY_VERIFY_WITH_TIMEOUT(blocks.load() > 0, kFeederFirstBlockMs);

        constexpr int kRounds = 300;
        for (int round = 0; round < kRounds; ++round) {
            const int id = h.radio->addSlice();
            if (id >= 0) {
                if (SliceModel* s = h.radio->sliceById(id)) {
                    s->setMuted((round & 1) != 0);
                    s->setVaxChannel(round % 5);
                }
                h.radio->removeSlice(id);
            }
            // Runs the removed slice's deleteLater, as the event loop would.
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }

        stop.store(true, std::memory_order_release);
        feeder.join();
        QVERIFY(blocks.load() > 0);
        QCOMPARE(h.radio->slices().size(), 1);
    }

    // Slice control plan Task 6: the mixer applies the AF level, so the
    // view carries it, 0..100 as 0..1.
    void audioViewCarriesTheAfLevel()
    {
        Harness h = makeHarness();
        const int a = h.radio->addSlice();
        SliceModel* slice = h.radio->sliceById(a);
        QVERIFY(slice != nullptr);
        slice->setAfGain(25);
        QCOMPARE(h.engine->sliceAudioView(a).afGain, 0.25f);
        slice->setAfGain(0);
        QCOMPARE(h.engine->sliceAudioView(a).afGain, 0.0f);
        slice->setAfGain(100);
        QCOMPARE(h.engine->sliceAudioView(a).afGain, 1.0f);
    }

    // Slice control plan Task 6: joins, leaves and level changes on the
    // control side while the audio thread drains, for ThreadSanitizer.
    void listenChurnWhileBlocksFlow()
    {
        Harness h = makeHarness();
        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();
        QVERIFY(a >= 0 && b >= 0);
        h.engine->setSliceStreaming(a, true);
        h.engine->setSliceStreaming(b, true);
        const int x = h.engine->acquireOwnerMix();
        const int y = h.engine->acquireOwnerMix();
        QVERIFY(x >= 0 && y >= 0);
        struct NullTap final : MasterMixAudioTap {
            void consume(const float*, int, int) noexcept override {}
        };
        NullTap tapX;
        NullTap tapY;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &tapX));
        QVERIFY(h.engine->setOwnerMixAudioTap(y, &tapY));
        h.engine->setOwnerMixSliceMask(x, 1u << a);
        h.engine->setOwnerMixSliceMask(y, 1u << b);

        std::atomic<bool> stop{false};
        std::atomic<long> blocks{0};
        std::thread feeder([&] {
            while (!stop.load(std::memory_order_acquire)) {
                h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
                h.engine->rxBlockReady(b, kTestSamples.data(), kTestFrames);
                blocks.fetch_add(1, std::memory_order_relaxed);
            }
        });
        FeederStop feederStop{stop, feeder};
        // The churn must overlap real block flow; without this the main
        // thread could finish all 2000 rounds before the feeder ran once.
        QTRY_VERIFY_WITH_TIMEOUT(blocks.load() > 0, kFeederFirstBlockMs);

        constexpr int kRounds = 2000;
        for (int round = 0; round < kRounds; ++round) {
            const float level = static_cast<float>(round % 11) / 10.0f;
            h.engine->setOwnerMixListen(x, b, level, (round & 3) == 0);
            h.engine->setOwnerMixListen(y, a, 1.0f - level, false);
            h.engine->setLocalListen(a, level, (round & 1) != 0);
            if ((round % 7) == 0) {
                h.engine->clearOwnerMixListen(x, b);
                h.engine->clearLocalListen(a);
            }
            if ((round % 13) == 0) {
                h.engine->setOwnerMixSliceMask(x, (round & 1) != 0 ? (1u << b) : (1u << a));
            }
        }

        stop.store(true, std::memory_order_release);
        feeder.join();
        QVERIFY(blocks.load() > 0);
        h.engine->releaseOwnerMix(x);
        h.engine->releaseOwnerMix(y);
        QCOMPARE(h.engine->ownerMixListenMask(x), 0u);
        QCOMPARE(h.engine->ownerMixListenMask(y), 0u);
    }
};

QTEST_MAIN(TstSliceAudioViewRace)
#include "tst_slice_audio_view_race.moc"
