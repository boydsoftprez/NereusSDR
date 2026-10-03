// no-port-check: NereusSDR-original test coverage.
// =================================================================
// tests/tst_audio_engine_owner_mix.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 76 (R-IOS-31; the several-devices design, ruling
// 9.2): AudioEngine's mixer builds one mix per owner. Distinct tones per
// slice: each owner's speakers (or whole program) tap and headphones tap
// carry that owner's slices alone, each at its gain, pan, mute and route;
// the local output (speakers bus, master tap) plays only the station
// device's slices, which RadioModel keeps in step with slice ownership;
// owner mixes are bounded and a released one sends nothing more.
//
// Modification history (NereusSDR):
//   2026-09-25 - Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via Anthropic
//                Claude Code (R-IOS-31).
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 64;

// A constant "tone" per slice, exactly representable after the gains used.
std::vector<float> level(float left, float right)
{
    std::vector<float> samples(kFrames * 2);
    for (int i = 0; i < kFrames; ++i) {
        samples[static_cast<size_t>(i) * 2] = left;
        samples[static_cast<size_t>(i) * 2 + 1] = right;
    }
    return samples;
}

class RecordingMixTap final : public MasterMixAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
        ++calls;
    }
    // The last frame's left and right.
    float lastLeft() const { return received.empty() ? 0.0f : received[received.size() - 2]; }
    float lastRight() const { return received.empty() ? 0.0f : received.back(); }
    std::vector<float> received;
    int calls = 0;
};

struct Harness {
    std::unique_ptr<RadioModel> radio = std::make_unique<RadioModel>();
    AudioEngine* engine = nullptr;
    FakeAudioBus* speakers = nullptr;
    int sliceA = -1;
    int sliceB = -1;
    int sliceC = -1;

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
        // No ramps: every block carries its full level at once.
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        engine->setVolume(1.0f);
        sliceA = radio->addSlice();
        sliceB = radio->addSlice();
        sliceC = radio->addSlice();
        engine->setSliceStreaming(sliceA, true);
        engine->setSliceStreaming(sliceB, true);
        engine->setSliceStreaming(sliceC, true);
        // The mixer applies each slice's AF level (slice control plan
        // Task 6); full AF keeps the tones exact.
        radio->sliceById(sliceA)->setAfGain(100);
        radio->sliceById(sliceB)->setAfGain(100);
        radio->sliceById(sliceC)->setAfGain(100);
    }

    // Slice A plays 0.5, B 0.25, C 0.125 on both channels.
    void feed(int periods)
    {
        const std::vector<float> a = level(0.5f, 0.5f);
        const std::vector<float> b = level(0.25f, 0.25f);
        const std::vector<float> c = level(0.125f, 0.125f);
        for (int period = 0; period < periods; ++period) {
            engine->rxBlockReady(sliceA, a.data(), kFrames);
            engine->rxBlockReady(sliceB, b.data(), kFrames);
            engine->rxBlockReady(sliceC, c.data(), kFrames);
        }
    }

    quint32 bit(int slice) const { return 1u << slice; }
};

} // namespace

class TstAudioEngineOwnerMix : public QObject {
    Q_OBJECT

private slots:
    // Two devices, each with its own slices: each owner mix carries only its
    // own slices' tones, both as the program and as the speakers' mix.
    void eachOwnerHearsOnlyItsOwnSlices()
    {
        Harness h;
        const int x = h.engine->acquireOwnerMix();
        const int y = h.engine->acquireOwnerMix();
        QVERIFY(x >= 0 && y >= 0 && x != y);
        h.engine->setOwnerMixSliceMask(x, h.bit(h.sliceA));
        h.engine->setOwnerMixSliceMask(y, h.bit(h.sliceB) | h.bit(h.sliceC));
        RecordingMixTap programX;
        RecordingMixTap programY;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &programX));
        QVERIFY(h.engine->setOwnerMixAudioTap(y, &programY, /*speakersOnly=*/true));
        h.feed(3);
        QCOMPARE(programX.calls, 3);
        QCOMPARE(programY.calls, 3);
        QCOMPARE(programX.lastLeft(), 0.5f);
        QCOMPARE(programX.lastRight(), 0.5f);
        QCOMPARE(programY.lastLeft(), 0.375f);
        QCOMPARE(programY.lastRight(), 0.375f);
        h.engine->releaseOwnerMix(x);
        h.engine->releaseOwnerMix(y);
    }

    // Each owner's slices keep their own gain, pan and mute in its mix.
    void anOwnersMixFollowsItsSlicesGainPanAndMute()
    {
        Harness h;
        const int x = h.engine->acquireOwnerMix();
        h.engine->setOwnerMixSliceMask(x, h.bit(h.sliceA) | h.bit(h.sliceB));
        RecordingMixTap program;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &program));
        // A at half gain panned hard left; B muted.
        h.engine->masterMixForTest().setSliceGain(h.sliceA, 0.5f, -1.0f);
        h.radio->sliceById(h.sliceB)->setMuted(true);
        h.feed(3);
        QCOMPARE(program.lastLeft(), 0.25f);
        QCOMPARE(program.lastRight(), 0.0f);
        h.engine->releaseOwnerMix(x);
    }

    // The headphones: a slice routed there reaches its own owner's
    // headphones tap and program, and never another owner's.
    void headphonesStayWithTheirOwner()
    {
        Harness h;
        h.radio->sliceById(h.sliceB)->setOutputRoute(SliceModel::OutputRoute::Headphones);
        const int x = h.engine->acquireOwnerMix();
        const int y = h.engine->acquireOwnerMix();
        h.engine->setOwnerMixSliceMask(x, h.bit(h.sliceA));
        h.engine->setOwnerMixSliceMask(y, h.bit(h.sliceB));
        RecordingMixTap speakersX;
        RecordingMixTap phonesX;
        RecordingMixTap speakersY;
        RecordingMixTap phonesY;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &speakersX, /*speakersOnly=*/true));
        QVERIFY(h.engine->setOwnerHeadphonesMixAudioTap(x, &phonesX));
        QVERIFY(h.engine->setOwnerMixAudioTap(y, &speakersY, /*speakersOnly=*/true));
        QVERIFY(h.engine->setOwnerHeadphonesMixAudioTap(y, &phonesY));
        h.feed(3);
        QCOMPARE(speakersX.lastLeft(), 0.5f);
        QCOMPARE(phonesX.lastLeft(), 0.0f);
        QCOMPARE(speakersY.lastLeft(), 0.0f);
        QCOMPARE(phonesY.lastLeft(), 0.25f);
        h.engine->releaseOwnerMix(x);
        h.engine->releaseOwnerMix(y);
    }

    // The Core's local output (the master tap, and the speakers bus from the
    // same sum) plays only the slices of its local mask; with every bit,
    // every slice, as before.
    void theLocalOutputPlaysOnlyItsMask()
    {
        Harness h;
        RecordingMixTap master;
        h.engine->setMasterMixAudioTap(&master);
        h.feed(3);
        QCOMPARE(master.lastLeft(), 0.875f);

        h.engine->setLocalOutputSliceMask(~h.bit(h.sliceA));
        h.feed(2);
        QCOMPARE(master.lastLeft(), 0.375f);
        h.engine->clearMasterMixAudioTap(&master);
    }

    // RadioModel keeps the local mask on the station device's slices: a
    // slice a device owns leaves the Core's speakers, one held for a device
    // (owned by the station device) plays.
    void theCoresOutputPlaysOnlyTheStationDevicesSlices()
    {
        Harness h;
        SliceOwnership* ownership = h.radio->sliceOwnership();
        QVERIFY(ownership != nullptr);
        ownership->setOwner(h.sliceA, QByteArrayLiteral("deviceA"));
        ownership->setOwner(h.sliceB, QByteArrayLiteral("deviceB"));
        ownership->hold(h.sliceC, QByteArrayLiteral("deviceB"));
        QCOMPARE(h.engine->localOutputSliceMask() & (h.bit(h.sliceA) | h.bit(h.sliceB)), 0u);
        QVERIFY((h.engine->localOutputSliceMask() & h.bit(h.sliceC)) != 0);
        RecordingMixTap master;
        h.engine->setMasterMixAudioTap(&master);
        h.feed(3);
        QCOMPARE(master.lastLeft(), 0.125f);
        h.engine->clearMasterMixAudioTap(&master);
    }

    // At most kMaxOwnerMixes; a released one frees its slot and its taps
    // hear nothing more; a slot not taken refuses a tap.
    void ownerMixesAreBoundedAndReleaseStopsTheirTaps()
    {
        Harness h;
        QList<int> taken;
        for (int i = 0; i < AudioEngine::kMaxOwnerMixes; ++i) {
            const int slot = h.engine->acquireOwnerMix();
            QVERIFY(slot >= 0);
            taken.append(slot);
        }
        QCOMPARE(h.engine->ownerMixCount(), AudioEngine::kMaxOwnerMixes);
        QCOMPARE(h.engine->acquireOwnerMix(), -1);

        RecordingMixTap tap;
        h.engine->setOwnerMixSliceMask(taken.first(), h.bit(h.sliceA));
        QVERIFY(h.engine->setOwnerMixAudioTap(taken.first(), &tap));
        h.feed(1);
        QCOMPARE(tap.calls, 1);
        h.engine->releaseOwnerMix(taken.first());
        QCOMPARE(h.engine->ownerMixCount(), AudioEngine::kMaxOwnerMixes - 1);
        QCOMPARE(h.engine->ownerMixSliceMask(taken.first()), 0u);
        h.feed(2);
        QCOMPARE(tap.calls, 1);
        QVERIFY(!h.engine->setOwnerMixAudioTap(taken.first(), &tap));
        QVERIFY(!h.engine->setOwnerMixAudioTap(-1, &tap));
        QCOMPARE(h.engine->acquireOwnerMix(), taken.first());
        for (int slot : taken) {
            h.engine->releaseOwnerMix(slot);
        }
        QCOMPARE(h.engine->ownerMixCount(), 0);
    }

    // An owner mix leaves the local output exactly as it was.
    void ownerMixesLeaveTheLocalOutputUnchanged()
    {
        const auto run = [](bool withOwners) {
            Harness h;
            h.engine->masterMixForTest().setSliceGain(h.sliceB, 0.5f, 0.25f);
            RecordingMixTap x;
            if (withOwners) {
                const int slot = h.engine->acquireOwnerMix();
                h.engine->setOwnerMixSliceMask(slot, h.bit(h.sliceB));
                h.engine->setOwnerMixAudioTap(slot, &x);
            }
            h.feed(4);
            return h.speakers->buffer();
        };
        const QByteArray without = run(false);
        QVERIFY(!without.isEmpty());
        QCOMPARE(run(true), without);
    }
};

QTEST_MAIN(TstAudioEngineOwnerMix)
#include "tst_audio_engine_owner_mix.moc"
