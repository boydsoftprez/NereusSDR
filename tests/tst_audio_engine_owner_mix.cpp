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
//   2026-10-10 - Headless Core speaker (JJ's ruling, R-AUD-27): a Core
//                with no window plays every receiver on its speaker at
//                the slice's AF level and mute; a desktop host keeps
//                ruling 9.2; the master tap, the headphones tap, every
//                owner mix and the VAX mask are the same either way.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <cstring>
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
        // A slice's route is saved (Slice<N>/OutputRoute) and the settings
        // outlive a Harness, so a route an earlier test or run chose would
        // come back here. Every Harness starts on the speakers, unmuted.
        for (int slice : {sliceA, sliceB, sliceC}) {
            radio->sliceById(slice)->setOutputRoute(SliceModel::OutputRoute::Speakers);
            radio->sliceById(slice)->setMuted(false);
        }
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

    // The left and right of the last frame the speakers bus was handed.
    float speakersLast(int channel) const
    {
        const QByteArray& played = speakers->buffer();
        if (played.size() < static_cast<qsizetype>(2 * sizeof(float))) {
            return -1.0f;
        }
        float value = 0.0f;
        std::memcpy(&value,
                    played.constData() + played.size()
                        - static_cast<qsizetype>((2 - channel) * sizeof(float)),
                    sizeof(float));
        return value;
    }

    // Two remote windows control A and B; nobody controls C.
    void remoteWindowsTakeAAndB()
    {
        radio->sliceOwnership()->setOwner(sliceA, QByteArrayLiteral("deviceA"));
        radio->sliceOwnership()->setOwner(sliceB, QByteArrayLiteral("deviceB"));
    }
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

    // JJ's ruling 2026-10-10 (R-AUD-27): a headless Core's speaker plays
    // every receiver, whichever device controls it, at the slice's own AF
    // level and mute, then the Core speaker's volume and mute. The masks
    // are ruling 9.2's still.
    void aHeadlessCoresSpeakerPlaysEveryReceiver()
    {
        Harness h;
        h.engine->setSpeakersPlayEverySlice(true); // as DaemonApp does
        h.remoteWindowsTakeAAndB();
        const quint32 remote = h.bit(h.sliceA) | h.bit(h.sliceB);
        QCOMPARE(h.engine->localOutputSliceMask() & remote, 0u);
        QCOMPARE(h.engine->vaxSliceMask() & remote, 0u);
        QCOMPARE(h.engine->localListenMask(), 0u);

        // A 0.5, B 0.25, C 0.125, all at full AF.
        h.feed(3);
        QCOMPARE(h.speakersLast(0), 0.875f);
        QCOMPARE(h.speakersLast(1), 0.875f);

        // A's controller turns its AF to half: 0.25 + 0.25 + 0.125.
        h.radio->sliceById(h.sliceA)->setAfGain(50);
        h.feed(2);
        QCOMPARE(h.speakersLast(0), 0.625f);

        // B's controller mutes it: 0.25 + 0.125.
        h.radio->sliceById(h.sliceB)->setMuted(true);
        h.feed(2);
        QCOMPARE(h.speakersLast(0), 0.375f);
        QCOMPARE(h.speakersLast(1), 0.375f);

        // The Core speaker's own volume on top, and its mute.
        h.engine->setVolume(0.5f);
        h.feed(2);
        QCOMPARE(h.speakersLast(0), 0.1875f);
        h.engine->setMasterMuted(true);
        const int pushes = h.speakers->pushCount();
        h.feed(2);
        QCOMPARE(h.speakers->pushCount(), pushes);
    }

    // A slice its controller routed to the headphones is not on a headless
    // Core's speaker, as it is on no speakers.
    void aHeadlessCoresSpeakerLeavesOutTheHeadphonesRoute()
    {
        Harness h;
        h.engine->setSpeakersPlayEverySlice(true);
        h.remoteWindowsTakeAAndB();
        h.radio->sliceById(h.sliceA)->setOutputRoute(SliceModel::OutputRoute::Headphones);
        h.feed(3);
        QCOMPARE(h.speakersLast(0), 0.375f);
    }

    // A desktop that hosts a station (not headless) keeps ruling 9.2: the
    // same ownership leaves the remote windows' slices off its speakers.
    void aDesktopHostsSpeakersLeaveOutOtherDevicesSlices()
    {
        Harness h;
        QVERIFY(!h.engine->speakersPlayEverySlice());
        h.remoteWindowsTakeAAndB();
        h.feed(3);
        QCOMPARE(h.speakersLast(0), 0.125f);
        QCOMPARE(h.speakersLast(1), 0.125f);
    }

    // The headless rule changes the speakers bus and nothing else: the
    // station's program (the master tap), the headphones tap, a
    // controlling device's mix, a listening device's mix and the two
    // slice masks are identical for the same input.
    void theHeadlessRuleChangesOnlyTheSpeakersBus()
    {
        struct Heard {
            std::vector<float> master;
            std::vector<float> headphones;
            std::vector<float> controller;
            std::vector<float> controllerHeadphones;
            std::vector<float> listener;
            quint32 localMask = 0;
            quint32 vaxMask = 0;
            quint32 remoteBits = 0;
            QByteArray speakers;
        };
        const auto run = [](bool headless) {
            Harness h;
            h.engine->setSpeakersPlayEverySlice(headless);
            h.remoteWindowsTakeAAndB();
            h.radio->sliceById(h.sliceA)->setAfGain(50);
            h.engine->masterMixForTest().setSliceGain(h.sliceB, 0.5f, 0.25f);
            RecordingMixTap master;
            RecordingMixTap headphones;
            RecordingMixTap controller;
            RecordingMixTap controllerHeadphones;
            RecordingMixTap listener;
            h.engine->setMasterMixAudioTap(&master);
            h.engine->setHeadphonesMixAudioTap(&headphones);
            const int x = h.engine->acquireOwnerMix();
            const int y = h.engine->acquireOwnerMix();
            h.engine->setOwnerMixSliceMask(x, h.bit(h.sliceA) | h.bit(h.sliceB));
            h.engine->setOwnerMixAudioTap(x, &controller);
            h.engine->setOwnerHeadphonesMixAudioTap(x, &controllerHeadphones);
            h.engine->setOwnerMixListen(y, h.sliceA, 0.25f, /*muted=*/false);
            h.engine->setOwnerMixAudioTap(y, &listener);
            h.feed(3);
            // Mid-run changes: a route to the headphones, a mute.
            h.radio->sliceById(h.sliceC)->setOutputRoute(SliceModel::OutputRoute::Headphones);
            h.radio->sliceById(h.sliceB)->setMuted(true);
            h.feed(3);
            Heard heard;
            heard.master = master.received;
            heard.headphones = headphones.received;
            heard.controller = controller.received;
            heard.controllerHeadphones = controllerHeadphones.received;
            heard.listener = listener.received;
            heard.localMask = h.engine->localOutputSliceMask();
            heard.vaxMask = h.engine->vaxSliceMask();
            heard.remoteBits = h.bit(h.sliceA) | h.bit(h.sliceB);
            heard.speakers = h.speakers->buffer();
            h.engine->clearMasterMixAudioTap(&master);
            h.engine->clearHeadphonesMixAudioTap(&headphones);
            h.engine->releaseOwnerMix(x);
            h.engine->releaseOwnerMix(y);
            return heard;
        };
        const Heard desktop = run(false);
        const Heard core = run(true);
        // Something was heard on each, so equal is not empty against empty.
        QCOMPARE(desktop.master.size(), size_t{6 * kFrames * 2});
        QCOMPARE(desktop.master[static_cast<size_t>(2 * kFrames * 2)], 0.125f);
        QCOMPARE(desktop.listener[static_cast<size_t>(2 * kFrames * 2)], 0.125f);
        QVERIFY(desktop.controller.back() != 0.0f);
        QVERIFY(desktop.headphones.back() != 0.0f);

        QCOMPARE(core.master, desktop.master);
        QCOMPARE(core.headphones, desktop.headphones);
        QCOMPARE(core.controller, desktop.controller);
        QCOMPARE(core.controllerHeadphones, desktop.controllerHeadphones);
        QCOMPARE(core.listener, desktop.listener);
        QCOMPARE(core.localMask, desktop.localMask);
        QCOMPARE(core.vaxMask, desktop.vaxMask);
        QCOMPARE(core.vaxMask, ~core.remoteBits);
        // And the speakers bus is what did change.
        QVERIFY(core.speakers != desktop.speakers);
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
