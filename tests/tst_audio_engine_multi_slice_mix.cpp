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

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/audio/MasterMixer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <array>
#include <memory>

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

    // ── Every id in [0, maxSlices) is registered up front, so no slice ever
    //    needs a mid-stream insert into the mixer's map. ───────────────────

    void everySliceIdInTheCapIsRegistered()
    {
        Harness h = makeHarness(/*maxSlices*/ 4);
        MasterMixer& mix = h.engine->masterMixForTest();

        for (int id = 0; id < 4; ++id) {
            const std::array<float, 2> in = {0.5f, 0.5f};  // 1 frame stereo
            mix.accumulate(id, in.data(), 1);
            std::array<float, 2> out{};
            mix.mixInto(out.data(), 1);
            QCOMPARE(out[0], 0.5f);
            QCOMPARE(out[1], 0.5f);
        }
    }

    // ── Reconnecting to a wider SKU tops the map up rather than skipping
    //    it: the pre-registration is monotonic, not a one-shot bool. ───────

    void widerRadioOnReconnectTopsTheMapUp()
    {
        Harness h = makeHarness(/*maxSlices*/ 1);   // e.g. Hermes Lite 2
        MasterMixer& mix = h.engine->masterMixForTest();

        h.radio->configureStreamPool(5, /*maxSlices*/ 5, 192000);  // e.g. ANAN-G2

        const std::array<float, 2> in = {0.5f, 0.5f};
        mix.accumulate(4, in.data(), 1);
        std::array<float, 2> out{};
        mix.mixInto(out.data(), 1);
        QCOMPARE(out[0], 0.5f);
    }

    // ── A count below 1 must still leave slice A registered. ──────────────

    void slotZeroSurvivesADegenerateCount()
    {
        AudioEngine engine;
        engine.preregisterSlices(0);

        MasterMixer& mix = engine.masterMixForTest();
        const std::array<float, 2> in = {0.5f, 0.5f};
        mix.accumulate(0, in.data(), 1);
        std::array<float, 2> out{};
        mix.mixInto(out.data(), 1);
        QCOMPARE(out[0], 0.5f);
    }
};

QTEST_MAIN(TstAudioEngineMultiSliceMix)
#include "tst_audio_engine_multi_slice_mix.moc"
