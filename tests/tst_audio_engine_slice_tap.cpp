// no-port-check: NereusSDR-original test coverage.
// =================================================================
// tests/tst_audio_engine_slice_tap.cpp  (NereusSDR)
// =================================================================
//
// R-R3-43: AudioEngine's per-slice receiver audio taps. A tap installed for
// slice B receives exactly slice B's own audio at the point the local VAX
// tee reads it: after the MOX gate, before slice mute, slice gain, pan, the
// mix and master volume. Like local VAX it carries no AF level: AudioEngine's
// mixer applies AF (slice control plan Task 6), so nothing is undone here.
// The master-mix tap and the speakers stay byte-for-byte what they were.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <QTemporaryDir>

#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 64;

// Every sample distinct and exactly representable after * 2 and * 4.
std::vector<float> ramp(float base)
{
    std::vector<float> samples(kFrames * 2);
    for (int i = 0; i < kFrames * 2; ++i) {
        samples[static_cast<size_t>(i)] = base + static_cast<float>(i) / 1024.0f;
    }
    return samples;
}

class RecordingSliceTap final : public SliceAudioTap {
public:
    void consume(const float* samples, int frames, int sampleRateHz) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
        ++calls;
        rate = sampleRateHz;
    }
    void skip(int frames, int sampleRateHz) noexcept override
    {
        skipped += frames;
        rate = sampleRateHz;
    }
    std::vector<float> received;
    int calls = 0;
    int skipped = 0;
    int rate = 0;
};

class RecordingMasterTap final : public MasterMixAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
    }
    std::vector<float> received;
};

struct Harness {
    std::unique_ptr<RadioModel> radio = std::make_unique<RadioModel>();
    AudioEngine* engine = nullptr;
    FakeAudioBus* speakers = nullptr;
    int sliceA = -1;
    int sliceB = -1;

    Harness()
    {
        engine = radio->audioEngine();
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat format;
        format.sampleRate = 48000;
        format.channels = 2;
        format.sample = AudioFormat::Sample::Float32;
        bus->open(format);
        speakers = bus.get();
        engine->setSpeakersBusForTest(std::move(bus));
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        sliceA = radio->addSlice();
        sliceB = radio->addSlice();
        engine->setSliceStreaming(sliceA, true);
        engine->setSliceStreaming(sliceB, true);
    }

    void feedBoth(const std::vector<float>& a, const std::vector<float>& b, int periods)
    {
        for (int period = 0; period < periods; ++period) {
            engine->rxBlockReady(sliceA, a.data(), kFrames);
            engine->rxBlockReady(sliceB, b.data(), kFrames);
        }
    }
};

std::vector<float> repeated(const std::vector<float>& block, int times)
{
    std::vector<float> out;
    for (int i = 0; i < times; ++i) {
        out.insert(out.end(), block.begin(), block.end());
    }
    return out;
}

} // namespace

class TstAudioEngineSliceTap : public QObject {
    Q_OBJECT

private slots:
    // Slice B's tap gets B's samples untouched, whatever B's mute, gain and
    // pan and the master volume and mute say, and nothing of slice A.
    void tapCarriesOnlyItsSliceBeforeMuteGainPanAndVolume()
    {
        Harness h;
        const std::vector<float> a = ramp(0.25f);
        const std::vector<float> b = ramp(-0.5f);
        RecordingSliceTap tap;
        QVERIFY(h.engine->setSliceAudioTap(h.sliceB, &tap));
        QCOMPARE(h.engine->sliceAudioTapCount(), 1);

        h.radio->sliceById(h.sliceB)->setMuted(true);
        h.engine->masterMixForTest().setSliceGain(h.sliceB, 0.125f, -0.75f);
        h.engine->setVolume(0.1f);
        h.engine->setMasterMuted(true);
        h.feedBoth(a, b, 3);

        QCOMPARE(tap.calls, 3);
        QCOMPARE(tap.rate, AudioEngine::kMasterMixSampleRateHz);
        QCOMPARE(tap.skipped, 0);
        QVERIFY(tap.received == repeated(b, 3));
        h.engine->clearSliceAudioTap(&tap);
        QCOMPARE(h.engine->sliceAudioTapCount(), 0);
    }

    // The master tap and the speakers see exactly what they saw before a
    // receiver tap existed.
    void masterTapAndSpeakersAreUnchangedByteForByte()
    {
        const std::vector<float> a = ramp(0.25f);
        const std::vector<float> b = ramp(-0.5f);
        const auto run = [&](bool withSliceTaps, std::vector<float>* master,
                             QByteArray* speakers) {
            Harness h;
            h.engine->masterMixForTest().setSliceGain(h.sliceB, 0.5f, 0.25f);
            h.engine->setVolume(0.5f);
            RecordingMasterTap masterTap;
            h.engine->setMasterMixAudioTap(&masterTap);
            RecordingSliceTap tapA;
            RecordingSliceTap tapB;
            if (withSliceTaps) {
                QVERIFY(h.engine->setSliceAudioTap(h.sliceA, &tapA));
                QVERIFY(h.engine->setSliceAudioTap(h.sliceB, &tapB));
            }
            h.feedBoth(a, b, 4);
            h.engine->clearMasterMixAudioTap(&masterTap);
            h.engine->clearSliceAudioTap(&tapA);
            h.engine->clearSliceAudioTap(&tapB);
            *master = masterTap.received;
            *speakers = h.speakers->buffer();
        };
        std::vector<float> masterWithout;
        std::vector<float> masterWith;
        QByteArray speakersWithout;
        QByteArray speakersWith;
        run(false, &masterWithout, &speakersWithout);
        run(true, &masterWith, &speakersWith);
        QVERIFY(!masterWithout.empty());
        QVERIFY(!speakersWithout.isEmpty());
        QVERIFY(masterWith == masterWithout);
        QCOMPARE(speakersWith, speakersWithout);
    }

    // kMaxSliceAudioTaps slots (eight from iPhone app Task 76, so every
    // slice a board can have fits whichever devices own them). One more tap
    // is refused; clearing one frees its slot; installing a tap already in
    // a slot moves it to the new slice; clearing one tap leaves the others
    // fed.
    void everySlotMovesAndClearsIndependently()
    {
        Harness h;
        const int sliceC = h.radio->addSlice();
        QVERIFY(sliceC >= 0);
        constexpr int kSlots = AudioEngine::kMaxSliceAudioTaps;
        QVERIFY(kSlots >= 5);
        RecordingSliceTap taps[kSlots + 1];
        for (int i = 0; i < kSlots; ++i) {
            QVERIFY(h.engine->setSliceAudioTap(i % 2 == 0 ? h.sliceA : h.sliceB, &taps[i]));
        }
        QCOMPARE(h.engine->sliceAudioTapCount(), kSlots);
        QVERIFY(!h.engine->setSliceAudioTap(h.sliceA, &taps[kSlots]));
        QVERIFY(!h.engine->setSliceAudioTap(-1, &taps[0]));
        QVERIFY(!h.engine->setSliceAudioTap(h.sliceA, nullptr));

        // Moving taps[1] to slice A keeps the count.
        QVERIFY(h.engine->setSliceAudioTap(h.sliceA, &taps[1]));
        QCOMPARE(h.engine->sliceAudioTapCount(), kSlots);
        h.engine->clearSliceAudioTap(&taps[0]);
        QCOMPARE(h.engine->sliceAudioTapCount(), kSlots - 1);
        QVERIFY(h.engine->setSliceAudioTap(sliceC, &taps[kSlots]));

        const std::vector<float> a = ramp(0.25f);
        const std::vector<float> b = ramp(-0.5f);
        h.feedBoth(a, b, 2);
        QCOMPARE(taps[0].calls, 0);
        QVERIFY(taps[1].received == repeated(a, 2));
        for (int i = 2; i < kSlots; ++i) {
            QVERIFY(taps[i].received == repeated(i % 2 == 0 ? a : b, 2));
        }
        QCOMPARE(taps[kSlots].calls, 0);
        for (RecordingSliceTap& tap : taps) {
            h.engine->clearSliceAudioTap(&tap);
        }
        QCOMPARE(h.engine->sliceAudioTapCount(), 0);
    }

    // The MOX gate withholds the transmitting slice's audio from its tap as
    // from everything else, and the tap is told how many frames it missed.
    void moxGateWithholdsAndReportsTheGap()
    {
        Harness h;
        SliceModel* const txSlice = h.radio->txBoundSlice();
        QVERIFY(txSlice != nullptr);
        const int gated = txSlice->sliceIndex();
        const int other = gated == h.sliceA ? h.sliceB : h.sliceA;
        RecordingSliceTap gatedTap;
        RecordingSliceTap otherTap;
        QVERIFY(h.engine->setSliceAudioTap(gated, &gatedTap));
        QVERIFY(h.engine->setSliceAudioTap(other, &otherTap));

        const std::vector<float> block = ramp(0.125f);
        h.engine->setMoxStateForTest(true);
        h.engine->rxBlockReady(gated, block.data(), kFrames);
        h.engine->rxBlockReady(other, block.data(), kFrames);
        h.engine->setMoxStateForTest(false);
        QCOMPARE(gatedTap.calls, 0);
        QCOMPARE(gatedTap.skipped, kFrames);
        QCOMPARE(otherTap.calls, 1);
        QCOMPARE(otherTap.skipped, 0);

        h.engine->rxBlockReady(gated, block.data(), kFrames);
        QCOMPARE(gatedTap.calls, 1);
        QVERIFY(gatedTap.received == block);
        h.engine->clearSliceAudioTap(&gatedTap);
        h.engine->clearSliceAudioTap(&otherTap);
    }

    // Each tap carries its slice's block as it arrives: the receive
    // channel's AF setting does not reach the tap, and neither do the VAX
    // channel gain and mute (slice control plan Task 6; the old tap divided
    // out the channel's AF gain).
    void tapIgnoresTheReceiversAfGain()
    {
        Harness h;
        QTemporaryDir config;
        QVERIFY(config.isValid());
        WdspEngine* const wdsp = h.radio->wdspEngine();
        QVERIFY(wdsp != nullptr);
        wdsp->setSynchronousInitForTest(true);
        QVERIFY(wdsp->initialize(config.path()));
        const auto channelFor = [wdsp](int sliceId) {
            RxChannel* rx = wdsp->rxChannel(sliceId);
            return rx != nullptr ? rx : wdsp->createRxChannel(sliceId, 64, 4096);
        };
        RxChannel* const rxA = channelFor(h.sliceA);
        RxChannel* const rxB = channelFor(h.sliceB);
        QVERIFY(rxA != nullptr && rxB != nullptr);
        rxA->setAfGain(0.5);
        rxB->setAfGain(0.25);
        // A VAX channel gain and mute must not touch the receiver tap.
        h.radio->sliceById(h.sliceB)->setVaxChannel(1);
        h.engine->setVaxRxGain(1, 0.5f);
        h.engine->setVaxMuted(1, true);

        RecordingSliceTap tapA;
        RecordingSliceTap tapB;
        QVERIFY(h.engine->setSliceAudioTap(h.sliceA, &tapA));
        QVERIFY(h.engine->setSliceAudioTap(h.sliceB, &tapB));
        const std::vector<float> a = ramp(0.25f);
        const std::vector<float> b = ramp(-0.5f);
        h.feedBoth(a, b, 1);

        QVERIFY(tapA.received == a);
        QVERIFY(tapB.received == b);

        // AF 0 leaves the tap carrying the block.
        rxB->setAfGain(0.0);
        h.radio->sliceById(h.sliceB)->setAfGain(0);
        tapB.received.clear();
        h.feedBoth(a, b, 1);
        QVERIFY(tapB.received == b);
        h.engine->clearSliceAudioTap(&tapA);
        h.engine->clearSliceAudioTap(&tapB);
    }
};

QTEST_MAIN(TstAudioEngineSliceTap)
#include "tst_audio_engine_slice_tap.moc"
