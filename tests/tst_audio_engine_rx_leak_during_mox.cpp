// =================================================================
// tests/tst_audio_engine_rx_leak_during_mox.cpp  (NereusSDR)
// =================================================================
//
// Exercises AudioEngine E.4: RX-leak-during-MOX fold via activeSlice
// gate in rxBlockReady.
//
// When MOX is on and the incoming slice is the active (TX) slice,
// rxBlockReady must drop the block before it reaches the MasterMixer.
// Non-active slices must NOT be gated — they keep playing.
//
// Coverage:
//   moxState_default_isFalse          — m_moxActive defaults false
//   setMoxState_atomic_round_trip     — store/load round-trip via setMoxState
//   rxBlockReady_moxOff_blockFlows    — MOX off → speakers push occurs
//   rxBlockReady_moxOn_activeSlice_blockDropped
//                                     — MOX on + active slice → push dropped
//   rxBlockReady_moxOn_nonActiveSlice_blockFlows
//                                     — MOX on + non-active slice → push flows
//   rxBlockReady_moxOnToOff_blockResumes
//                                     — MOX off again → push resumes
//
// Test seam: setMoxStateForTest() (NEREUS_BUILD_TESTS) drives the gate
// without a full RadioModel/MoxController fixture.
//
// Observation method: FakeAudioBus injected via setSpeakersBusForTest().
// Each time rxBlockReady flushed the master mix to speakers, pushCount()
// increments. This mirrors the approach in tst_audio_engine_master_mute.cpp.
//
// Note: rxBlockReady calls MasterMixer::accumulate + mixInto internally
// (the mix is flushed to speakers within the same call). So speakers
// pushCount() is the correct observable — not masterMixForTest().mixInto()
// which would drain an already-flushed mixer.
//
// SliceModel::setActive(bool) drives isActiveSlice(). RadioModel::addSlice()
// sets m_active=true on the first slice automatically (E.4 RadioModel fix).
// For the non-active-slice case a second slice is added without setActive.
//
// Plan: 3M-1b E.4. Pre-code review §10.3 + §10.4.
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
// Mirrors tst_audio_engine_master_mute.cpp.
constexpr int kTestFrames = 2;
constexpr int kTestStereoFloats = kTestFrames * 2;

const std::array<float, kTestStereoFloats> kTestSamples = {
    0.10f, 0.20f,   // frame 0  L,R
   -0.30f, -0.40f, // frame 1  L,R
};

} // namespace

class TstAudioEngineRxLeakDuringMox : public QObject {
    Q_OBJECT

private:

    // Harness: RadioModel + AudioEngine with FakeSpeakers injected.
    // Mirrors the Harness in tst_audio_engine_master_mute.cpp.
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        AudioEngine* engine;        // non-owning view (RadioModel owns it)
        FakeAudioBus* speakers;     // non-owning view (engine owns it)

        // Add a slice with the given vaxChannel value; returns its index.
        // The first addSlice() call sets isActiveSlice()=true on that slice
        // (E.4 RadioModel fix to propagate m_active).
        int addSlice(int vaxChannel = 0) {
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

        auto speakers = std::make_unique<FakeAudioBus>(
            QStringLiteral("FakeSpeakers"));
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

private slots:

    void moxGatesTheTxBoundSliceEvenWhenAnotherSliceIsActive()
    {
        Harness h = makeHarness();
        const int a = h.addSlice();
        const int c = h.addSlice();
        QVERIFY(h.radio->requestTxHandoffToSlice(c));
        QCOMPARE(h.radio->activeSlice()->sliceIndex(), a);
        QCOMPARE(h.radio->txBoundSlice()->sliceIndex(), c);

        h.engine->setMoxStateForTest(true);

        h.engine->rxBlockReady(c, kTestSamples.data(), kTestFrames);
        QCOMPARE(h.speakers->pushCount(), 0);

        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        QVERIFY2(h.speakers->pushCount() > 0,
                 "the non-TX listening slice should remain audible during MOX");

        h.radio->setActiveSlice(1);
        const int before = h.speakers->pushCount();
        h.engine->rxBlockReady(c, kTestSamples.data(), kTestFrames);
        QCOMPARE(h.speakers->pushCount(), before);

        h.engine->setMoxStateForTest(false);
        h.engine->rxBlockReady(a, kTestSamples.data(), kTestFrames);
        h.engine->rxBlockReady(c, kTestSamples.data(), kTestFrames);
        QVERIFY(h.speakers->pushCount() > before);
    }

    // ── 1. Default MOX state is false ─────────────────────────────────────

    void moxState_default_isFalse()
    {
        AudioEngine engine;
        QCOMPARE(engine.moxState(), false);
    }

    // ── 2. setMoxState atomic round-trip ──────────────────────────────────

    void setMoxState_atomic_round_trip()
    {
        AudioEngine engine;
        QCOMPARE(engine.moxState(), false);

        engine.setMoxStateForTest(true);
        QCOMPARE(engine.moxState(), true);

        engine.setMoxStateForTest(false);
        QCOMPARE(engine.moxState(), false);
    }

    // ── 3. MOX off → block flows through to speakers ──────────────────────

    void rxBlockReady_moxOff_blockFlows()
    {
        Harness h = makeHarness();
        const int s = h.addSlice();

        // Verify slice 0 is the active slice.
        QVERIFY(h.radio->sliceById(s) != nullptr);
        QVERIFY(h.radio->sliceById(s)->isActiveSlice());

        // MOX off (default).
        QCOMPARE(h.engine->moxState(), false);

        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QVERIFY2(h.speakers->pushCount() > 0,
                 "MOX off: block should reach speakers but pushCount == 0");
    }

    // ── 4. MOX on + active slice → block dropped ─────────────────────────

    void rxBlockReady_moxOn_activeSlice_blockDropped()
    {
        Harness h = makeHarness();
        const int s = h.addSlice();

        // Verify slice 0 is the active slice.
        QVERIFY(h.radio->sliceById(s)->isActiveSlice());

        // Turn MOX on.
        h.engine->setMoxStateForTest(true);

        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);

        QCOMPARE(h.speakers->pushCount(), 0);
    }

    // ── 5. MOX on + non-active slice → block flows ────────────────────────

    void rxBlockReady_moxOn_nonActiveSlice_blockFlows()
    {
        Harness h = makeHarness();
        // addSlice() twice: slice 0 is active, slice 1 is not.
        h.addSlice();           // slice 0 — active
        const int s1 = h.addSlice();  // slice 1 — not active

        QVERIFY(h.radio->sliceById(0)->isActiveSlice());
        QVERIFY(!h.radio->sliceById(s1)->isActiveSlice());

        // Turn MOX on.
        h.engine->setMoxStateForTest(true);

        // Push a block for slice 1 (non-active).
        h.engine->rxBlockReady(s1, kTestSamples.data(), kTestFrames);

        QVERIFY2(h.speakers->pushCount() > 0,
                 "MOX on + non-active slice: block should flow but pushCount == 0");
    }

    // ── 6. MOX on then off → block resumes ───────────────────────────────

    void rxBlockReady_moxOnToOff_blockResumes()
    {
        Harness h = makeHarness();
        const int s = h.addSlice();

        QVERIFY(h.radio->sliceById(s)->isActiveSlice());

        // MOX on — block dropped.
        h.engine->setMoxStateForTest(true);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);
        QCOMPARE(h.speakers->pushCount(), 0);

        // MOX off — block flows.
        h.engine->setMoxStateForTest(false);
        h.engine->rxBlockReady(s, kTestSamples.data(), kTestFrames);
        QVERIFY2(h.speakers->pushCount() > 0,
                 "MOX off: block should reach speakers after resume");
    }
};

QTEST_MAIN(TstAudioEngineRxLeakDuringMox)
#include "tst_audio_engine_rx_leak_during_mox.moc"
