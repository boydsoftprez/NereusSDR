// =================================================================
// tests/tst_audio_engine_multi_slice_mix.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I closeout, defect C1: secondary slices produced no
// audio.
//
// RxDspWorker demodulated every slice and called
// AudioEngine::rxBlockReady(sliceId, ...) for each, but MasterMixer only
// had an entry for slice id 0 (AudioEngine::start pre-registered exactly
// one), and MasterMixer::accumulate() drops any id it has no entry for.
// Slices B-E were therefore demodulated and then silently discarded.
//
// The fix pre-registers ids [0, maxSlices) at connect via
// AudioEngine::preregisterSlices(), which RadioModel::configureStreamPool
// calls alongside the WDSP channel pool sizing. Enrolling lazily on the
// first block is not an option: the DSP thread reads MasterMixer's map
// lock-free, so a main-thread insert would rehash underneath it
// (MasterMixer.h:52-56).
//
// Harness pattern mirrors tst_audio_engine_master_mute.cpp: a FakeAudioBus
// is injected through the NEREUS_BUILD_TESTS-only setSpeakersBusForTest
// seam, so the test needs no real CoreAudio / PipeWire / PortAudio
// backend. start() is deliberately NOT called (it would construct real
// platform buses); configureStreamPool is the production connect-time
// call site being exercised.
// =================================================================

#include <QtTest/QtTest>
#include <QSemaphore>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/AppSettings.h"
#include "core/audio/MasterMixer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

// 2 frames of stereo float. Deliberately all non-zero so a dropped
// accumulate() is visible as silence in the speakers buffer.
constexpr int kTestFrames = 2;
constexpr int kTestStereoFloats = kTestFrames * 2;

const std::array<float, kTestStereoFloats> kTestSamples = {
    0.10f, 0.20f,   // frame 0  L,R
    -0.30f, -0.40f, // frame 1  L,R
};

// Did anything non-zero reach the speakers bus?
bool bufferHasSignal(const QByteArray& bytes)
{
    const int count = static_cast<int>(bytes.size() / sizeof(float));
    const float* f = reinterpret_cast<const float*>(bytes.constData());
    for (int i = 0; i < count; ++i) {
        if (f[i] != 0.0f) { return true; }
    }
    return false;
}

// R-R3-45: the sign of every sample in a pushed buffer. Slice A feeds +0.5
// and slice B -0.5, so a buffer carrying only A has no negative sample and
// one carrying only B no positive sample, whatever the fades do.
struct SignCount {
    int positive = 0;
    int negative = 0;
};

SignCount signsIn(const QByteArray& bytes)
{
    SignCount c;
    const int count = static_cast<int>(bytes.size() / sizeof(float));
    const float* f = reinterpret_cast<const float*>(bytes.constData());
    for (int i = 0; i < count; ++i) {
        if (f[i] > 0.0f) { ++c.positive; }
        if (f[i] < 0.0f) { ++c.negative; }
    }
    return c;
}

} // namespace

class TstAudioEngineMultiSliceMix : public QObject {
    Q_OBJECT

private:
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        AudioEngine* engine;      // non-owning view
        FakeAudioBus* speakers;   // non-owning view (engine owns it)
    };

    // maxSlices mirrors a 5-slice P2 SKU (BoardCapabilities kSaturn row).
    Harness makeHarness(int maxSlices = 5)
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

        // The production connect-time call: sizes the stream pool AND
        // pre-registers the mixer slots (RadioModel.cpp configureStreamPool).
        h.radio->configureStreamPool(/*userDdcCount*/ 5, maxSlices,
                                     /*defaultRateHz*/ 192000);
        return h;
    }

private slots:

    // ── C1 regression: a block pushed for a NON-ZERO slice id has to reach
    //    the mixer. Before the fix MasterMixer::accumulate found no entry
    //    for id 1 and returned, so the block never made it into the mix and
    //    the speakers push carried pure silence. ────────────────────────────

    void secondarySliceAudioReachesTheMixer()
    {
        Harness h = makeHarness();

        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);

        h.engine->rxBlockReady(b, kTestSamples.data(), kTestFrames);

        // The push itself always happens (mixInto writes zeros when nothing
        // accumulated), so counting pushes is not enough — the payload is
        // what proves the block was not dropped.
        QCOMPARE(h.speakers->pushCount(), 1);
        QVERIFY(bufferHasSignal(h.speakers->buffer()));
    }

    // ── Slice A (the single-slice path that shipped) must be untouched. ────

    void sliceAStillMixes()
    {
        Harness h = makeHarness();

        const int a = h.radio->addSlice();
        QCOMPARE(a, 0);

        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);

        QCOMPARE(h.speakers->pushCount(), 1);
        QVERIFY(bufferHasSignal(h.speakers->buffer()));
    }

    // ── Closing a pan must not wedge the mix. ─────────────────────────────
    //
    // The removed slice was a barrier member on its last block. The mixer
    // has no timeout that will notice it is gone (deliberately: that
    // timeout is what made a merely-late slice look stopped and produced
    // the 2026-07-27 G2E scratchy-audio defect). So the slice lifecycle
    // has to withdraw it, exactly as SetAAudioMixState does upstream when
    // a stream leaves the mix. From Thetis aamix.c:522 [v2.10.3.15].
    //
    // Without that withdrawal the drain waits forever on a slice that will
    // never deliver again, and ALL audio stops.

    void closingAPanDoesNotWedgeTheMix()
    {
        Harness h = makeHarness();

        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();

        // Slice A drains alone (B has not delivered yet, so it is not yet a
        // member); B's block then joins it to the barrier and holds it.
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(b, kTestSamples.data(), kTestFrames);
        QCOMPARE(h.speakers->pushCount(), 1);

        // One more A block completes the period and drains BOTH rings, so
        // B has nothing queued when it goes away. Without this the leftover
        // block in B's ring satisfies the barrier on its own and hides the
        // wedge this test exists to catch.
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        QCOMPARE(h.speakers->pushCount(), 2);

        // Operator closes the second pan.
        h.radio->removeSlice(b);

        // Slice A alone must keep feeding the speakers.
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        QCOMPARE(h.speakers->pushCount(), 3);
        QVERIFY(bufferHasSignal(h.speakers->buffer()));
    }

    // ── Keying up must not silence the slices that are still receiving. ───
    //
    // The active TX slice's RX audio is gated during MOX. Because the
    // barrier waits on every member and has no timeout, a gated slice that
    // simply stopped feeding would hold the drain for the entire
    // transmission and take the whole mix down with it.
    //
    // Thetis avoids this by dropping the stream from the mix outright on
    // every MOX transition: console.cs:27650-27771 [v2.10.3.15] calls
    // SetAAudioMixStates, and with PureSignal on it goes all the way down
    // to MON alone. We cannot follow it literally, because our gate is
    // evaluated on the audio thread and setSliceStreaming() takes the
    // slice-map mutex. The gated slice therefore keeps its membership and
    // contributes silence, which lands the same audible result: it adds
    // nothing to the mix, and everyone else keeps playing.

    void aGatedTxSliceDoesNotSilenceTheOtherSlice()
    {
        Harness h = makeHarness();

        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();

        // The gate keys off isActiveSlice(), so the test is only meaningful
        // if A is in fact the active slice.
        QVERIFY(h.radio->sliceById(a)->isActiveSlice());

        // Both enrolled, and one more A block empties both rings.
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(b, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        const int before = h.speakers->pushCount();

        // Key up. Slice A's RX audio is now gated; slice B keeps receiving.
        h.engine->setMoxStateForTest(true);

        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);  // gated
        h.engine->rxBlockReady(b, kTestSamples.data(), kTestFrames);

        QVERIFY(h.speakers->pushCount() > before);
        QVERIFY(bufferHasSignal(h.speakers->buffer()));
    }

    // ── Dropping the stream bindings must release every slice. ────────────
    //
    // teardownConnection calls releaseStreamBindings(), which unbinds every
    // slice from every stream. Whichever slices the next connect does not
    // re-bind can never deliver again, and with no timeout in the barrier a
    // single one of them holds the mix down forever. Reconnecting to a radio
    // with fewer DDCs is enough to reach that.

    void releasingStreamBindingsWithdrawsEverySlice()
    {
        Harness h = makeHarness();

        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();

        // Both enrolled, rings drained.
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(b, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        const int before = h.speakers->pushCount();

        h.radio->releaseStreamBindings();

        // A production reconnect re-admits a rebound slice through
        // RadioModel::activateSliceChannel before its first new-generation
        // block arrives. Model that lifecycle edge explicitly; a block
        // delivered while the slice is still withdrawn is intentionally
        // rejected by MasterMixer.
        h.engine->setSliceStreaming(a, true);

        // Slice A alone resumes. Slice B, which never comes back, must not
        // still be holding the barrier.
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        QVERIFY(h.speakers->pushCount() > before);
    }

    void withdrawalWaitsForAdmittedMixAndPreventsOldPush()
    {
        Harness h = makeHarness();
        const int a = h.radio->addSlice();

        QSemaphore admitted;
        QSemaphore resume;
        QSemaphore withdrawalPublished;
        QSemaphore withdrawalReturned;

        h.engine->masterMixForTest().setDrainAdmissionHookForTest([&] {
            admitted.release();
            resume.acquire();
        });
        h.engine->setWithdrawalPublishedHookForTest([&] {
            withdrawalPublished.release();
        });

        std::thread audio([&] {
            h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        });
        const bool admissionObserved = admitted.tryAcquire(1, 1000);
        if (!admissionObserved) {
            resume.release();
            audio.join();
            QVERIFY2(admissionObserved,
                     "audio callback never reached the drain seam");
            return;
        }

        std::thread control([&] {
            h.engine->setSliceStreaming(a, false);
            withdrawalReturned.release();
        });
        const bool publicationObserved =
            withdrawalPublished.tryAcquire(1, 1000);

        // Once invalidation is published, withdrawal cannot return until the
        // admitted region has observed it and abandoned the old block.
        const bool returnedEarly = withdrawalReturned.tryAcquire(1, 50);
        resume.release();
        const bool returnedAfterResume =
            returnedEarly || withdrawalReturned.tryAcquire(1, 1000);

        audio.join();
        control.join();
        QVERIFY2(publicationObserved,
                 "control path never published mixer invalidation");
        QVERIFY2(!returnedEarly,
                 "withdrawal returned while old output was still admitted");
        QVERIFY(returnedAfterResume);
        QCOMPARE(h.speakers->pushCount(), 0);
    }

    // ── Every id in [0, maxSlices) is registered up front, so no slice ever
    //    needs a mid-stream insert into the mixer's map. ───────────────────

    void everySliceIdInTheCapIsRegistered()
    {
        Harness h = makeHarness(/*maxSlices*/ 4);
        MasterMixer& mix = h.engine->masterMixForTest();
        mix.setRampFrames(1);   // assert steady-state gain, not the fade-in

        // Feed every id, THEN drain once. Draining after each id in turn
        // would deadlock against the readiness barrier by design: an id
        // that has already delivered is a member with an empty ring, so
        // it holds the next drain until the rest catch up.
        const std::array<float, 2> in = {0.5f, 0.5f};  // 1 frame stereo
        for (int id = 0; id < 4; ++id) {
            mix.accumulate(id, in.data(), 1);
        }
        QCOMPARE(mix.producingSliceCount(), 4);   // all four ids registered

        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 4 * 0.5f);               // every id contributed
        QCOMPARE(out[1], 4 * 0.5f);
    }

    // ── Reconnecting to a wider SKU tops the map up rather than skipping
    //    it: the pre-registration is monotonic, not a one-shot bool. ───────

    void widerRadioOnReconnectTopsTheMapUp()
    {
        Harness h = makeHarness(/*maxSlices*/ 1);   // e.g. Hermes Lite 2
        MasterMixer& mix = h.engine->masterMixForTest();
        mix.setRampFrames(1);

        h.radio->configureStreamPool(5, /*maxSlices*/ 5, 192000);  // e.g. ANAN-G2

        const std::array<float, 2> in = {0.5f, 0.5f};
        mix.accumulate(4, in.data(), 1);
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.5f);
    }

    // ── A count below 1 must still leave slice A registered. ──────────────

    void slotZeroSurvivesADegenerateCount()
    {
        AudioEngine engine;
        engine.preregisterSlices(0);

        MasterMixer& mix = engine.masterMixForTest();
        mix.setRampFrames(1);
        const std::array<float, 2> in = {0.5f, 0.5f};
        mix.accumulate(0, in.data(), 1);
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.5f);
    }

    // ── Bench defect, ANAN-G2E 2026-07-26: audio distorted as soon as a
    //    second pan came up. ────────────────────────────────────────────────
    //
    // AudioEngine::rxBlockReady ends with an unconditional
    // MasterMixer::mixInto + speakers push, so it drains once per SLICE
    // rather than once per audio period. One slice: accumulate -> drain ->
    // push, correct. Two slices: A accumulates and is drained and pushed
    // ALONE, then B accumulates and is drained and pushed ALONE.
    //
    // Two consequences, both audible. The sink is handed two 48 kHz blocks
    // per period and can only consume one, and the slices never actually
    // sum -- mixInto zeroes the accumulator, so each block leaves on its own
    // and the output alternates between receivers instead of mixing them.
    void two_slices_in_one_period_produce_one_summed_push()
    {
        Harness h = makeHarness();

        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);

        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(b, kTestSamples.data(), kTestFrames);

        // One audio period in, one block out. Two pushes here means the sink
        // is being overrun at 2x the rate it drains.
        QCOMPARE(h.speakers->pushCount(), 1);

        // And the one block that leaves carries real audio rather than
        // the silence a mis-ordered barrier would emit. That both slices
        // sum linearly is pinned by tst_master_mixer's twoSlicesSumLinearly;
        // asserting an exact 2x here would fight the production fade-in,
        // which deliberately holds frame 0 below full gain.
        QVERIFY(bufferHasSignal(h.speakers->buffer()));
    }

    // ── R-R3-45: speakers or headphones per receiver (VAX design 6.2) ────
    //
    // Slice A on the speakers, slice B on the headphones: the speakers
    // output carries only A and the headphones output only B.
    void speakersCarryOnlyASliceAndHeadphonesOnlyB()
    {
        AppSettings::instance().clear();
        Harness h = makeHarness();
        auto hpBus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeHeadphones"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        hpBus->open(fmt);
        FakeAudioBus* headphones = hpBus.get();
        h.engine->setHeadphonesBusForTest(std::move(hpBus));
        QVERIFY(h.engine->headphonesAvailable());

        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();
        h.radio->sliceById(b)->setOutputRoute(SliceModel::OutputRoute::Headphones);

        const std::array<float, 4> plus = {0.5f, 0.5f, 0.5f, 0.5f};
        const std::array<float, 4> minus = {-0.5f, -0.5f, -0.5f, -0.5f};
        for (int period = 0; period < 400; ++period) {
            h.engine->rxBlockReady(a, plus.data(), 2);
            h.engine->rxBlockReady(b, minus.data(), 2);
        }

        // One push per period on each output, from the one barrier.
        QCOMPARE(h.speakers->pushCount(), headphones->pushCount());
        const SignCount spk = signsIn(h.speakers->buffer());
        const SignCount hp = signsIn(headphones->buffer());
        QVERIFY(spk.positive > 0);
        QCOMPARE(spk.negative, 0);
        QVERIFY(hp.negative > 0);
        QCOMPARE(hp.positive, 0);

        // Muting B silences it on the headphones and leaves A playing.
        h.radio->sliceById(b)->setMuted(true);
        for (int period = 0; period < 400; ++period) {
            h.engine->rxBlockReady(a, plus.data(), 2);
            h.engine->rxBlockReady(b, minus.data(), 2);
        }
        const QByteArray hpTail = headphones->buffer().right(16);
        QVERIFY(!bufferHasSignal(hpTail));
        QVERIFY(bufferHasSignal(h.speakers->buffer().right(16)));

        // Moving B back to the speakers brings it there.
        h.radio->sliceById(b)->setMuted(false);
        h.radio->sliceById(b)->setOutputRoute(SliceModel::OutputRoute::Speakers);
        const int before = static_cast<int>(h.speakers->buffer().size());
        for (int period = 0; period < 400; ++period) {
            h.engine->rxBlockReady(a, plus.data(), 2);
            h.engine->rxBlockReady(b, minus.data(), 2);
        }
        // A + B on the speakers sum to silence once the fades settle.
        QVERIFY(!bufferHasSignal(h.speakers->buffer().right(16)));
        QVERIFY(h.speakers->buffer().size() > before);
        AppSettings::instance().clear();
    }

    // No headphones output: a slice routed there is silent, and it does not
    // leak onto the speakers.
    void headphonesSliceIsSilentWithNoHeadphonesOutput()
    {
        AppSettings::instance().clear();
        Harness h = makeHarness();
        QVERIFY(!h.engine->headphonesAvailable());

        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();
        h.radio->sliceById(b)->setOutputRoute(SliceModel::OutputRoute::Headphones);

        const std::array<float, 4> plus = {0.5f, 0.5f, 0.5f, 0.5f};
        const std::array<float, 4> minus = {-0.5f, -0.5f, -0.5f, -0.5f};
        for (int period = 0; period < 100; ++period) {
            h.engine->rxBlockReady(a, plus.data(), 2);
            h.engine->rxBlockReady(b, minus.data(), 2);
        }
        const SignCount spk = signsIn(h.speakers->buffer());
        QVERIFY(spk.positive > 0);
        QCOMPARE(spk.negative, 0);
        AppSettings::instance().clear();
    }

    // The route is restored when a local slice is added (restart).
    void addedSliceRestoresItsRoute()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("Slice1/OutputRoute"),
                                         QStringLiteral("Headphones"));
        Harness h = makeHarness();
        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();
        QCOMPARE(h.radio->sliceById(a)->outputRoute(), SliceModel::OutputRoute::Speakers);
        QCOMPARE(h.radio->sliceById(b)->outputRoute(), SliceModel::OutputRoute::Headphones);
        AppSettings::instance().clear();
    }

    // R-R3-45 fix wave: the mix scratch is the engine's, sized before any
    // block (never grown on the DSP thread), and grown by setDspBlockSize
    // so a block that size mixes in one push.
    void mixScratchIsSizedOffTheDspThread()
    {
        AppSettings::instance().clear();
        Harness h = makeHarness();
        QCOMPARE(h.engine->mixScratchFrames(), AudioEngine::kMixScratchMinFrames);
        const int a = h.radio->addSlice();

        h.engine->setDspBlockSize(8192);
        QCOMPARE(h.engine->mixScratchFrames(), 8192);
        h.engine->setDspBlockSize(1024);          // never shrinks
        QCOMPARE(h.engine->mixScratchFrames(), 8192);

        const std::vector<float> block(6000 * 2, 0.25f);
        const int before = h.speakers->pushCount();
        const qsizetype bytesBefore = h.speakers->buffer().size();
        h.engine->rxBlockReady(a, block.data(), 6000);
        QCOMPARE(h.speakers->pushCount(), before + 1);
        QCOMPARE(h.speakers->buffer().size() - bytesBefore,
                 qsizetype(6000 * 2 * sizeof(float)));
        AppSettings::instance().clear();
    }

    // R-R3-45 (carried from Task 1): each local slice's VAX channel is
    // restored when it is added, as its output route is. Nothing called
    // SliceModel::loadFromSettings(), so the saved channel was lost on
    // every restart.
    void addedSliceRestoresItsVaxChannel()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("Slice1/VaxChannel"),
                                         QStringLiteral("3"));
        Harness h = makeHarness();
        const int a = h.radio->addSlice();
        const int b = h.radio->addSlice();
        QCOMPARE(h.radio->sliceById(a)->vaxChannel(), 0);
        QCOMPARE(h.radio->sliceById(b)->vaxChannel(), 3);
        AppSettings::instance().clear();
    }
};

QTEST_MAIN(TstAudioEngineMultiSliceMix)
#include "tst_audio_engine_multi_slice_mix.moc"
