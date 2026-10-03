// no-port-check: NereusSDR-original test coverage.
// =================================================================
// tests/tst_slice_listen_audio.cpp  (NereusSDR)
// =================================================================
//
// Slice control plan Task 6 (JJ's ruling on listener audio and AF): the
// WDSP panel gain stays at unity and AudioEngine's mixer applies each
// slice's AF level to its controller's audio, while every device that
// listens to the slice hears it at its own level. VAX and the receiver taps
// carry the slice without the AF level, so VAX stays audible at AF 0.
//
// Coverage:
//   twoListenersAtDifferentLevelsHearDifferentGains
//   afZeroOnTheOperatorLeavesVaxAndListenersAudible
//   aControllerChangeLeavesTheListenerAlone
//   aListenerMuteLeavesTheControllerAndLocalAlone
//   handOffIsContinuous
//   fourDevicesListeningToFiveSlicesTakeNothingNew
//   theHostListensLocallyAtItsOwnLevel
//   anOwnerThatStopsControllingSendsSilence  (stale buffer)
//   theDrainTakesNoLock                  (source scan)
//
// Modification history (NereusSDR):
//   2026-09-29 - Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via Anthropic
//                Claude Code (slice control plan Task 6).
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <QFile>
#include <QRegularExpression>

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 64;

std::vector<float> tone(float value)
{
    return std::vector<float>(static_cast<size_t>(kFrames) * 2, value);
}

class RecordingMixTap final : public MasterMixAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
        ++calls;
    }
    float lastLeft() const { return received.empty() ? -1.0f : received[received.size() - 2]; }
    void clear() { received.clear(); }
    std::vector<float> received;
    int calls = 0;
};

struct Harness {
    std::unique_ptr<RadioModel> radio = std::make_unique<RadioModel>();
    AudioEngine* engine = nullptr;
    std::vector<int> slices;

    explicit Harness(int sliceCount = 3, int rampFrames = 1)
    {
        engine = radio->audioEngine();
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat format;
        format.sampleRate = 48000;
        format.channels = 2;
        format.sample = AudioFormat::Sample::Float32;
        bus->open(format);
        engine->setSpeakersBusForTest(std::move(bus));
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(rampFrames);
        engine->masterMixForTest().setSlewUpFrames(0);
        engine->setVolume(1.0f);
        for (int i = 0; i < sliceCount; ++i) {
            const int id = radio->addSlice();
            slices.push_back(id);
            engine->setSliceStreaming(id, true);
            radio->sliceById(id)->setAfGain(100);
        }
    }

    SliceModel* slice(int index) const { return radio->sliceById(slices[static_cast<size_t>(index)]); }
    quint32 bit(int index) const { return 1u << slices[static_cast<size_t>(index)]; }
    int id(int index) const { return slices[static_cast<size_t>(index)]; }

    // Slice i plays values[i]; one block each per period.
    void feed(const std::vector<float>& values, int periods = 1)
    {
        std::vector<std::vector<float>> blocks;
        for (float v : values) {
            blocks.push_back(tone(v));
        }
        for (int period = 0; period < periods; ++period) {
            for (size_t i = 0; i < blocks.size(); ++i) {
                engine->rxBlockReady(slices[i], blocks[i].data(), kFrames);
            }
        }
    }
};

FakeAudioBus* injectFakeVax(AudioEngine* engine, int channel)
{
    auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeVax%1").arg(channel));
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    bus->open(format);
    FakeAudioBus* view = bus.get();
    engine->setVaxBusForTest(channel, std::move(bus));
    return view;
}

float lastVaxLeft(const FakeAudioBus* bus)
{
    const QByteArray& buf = bus->buffer();
    if (buf.size() < static_cast<int>(sizeof(float) * 2)) {
        return -1.0f;
    }
    float left = 0.0f;
    std::memcpy(&left, buf.constData() + buf.size() - static_cast<int>(sizeof(float) * 2),
                sizeof(float));
    return left;
}

bool near(float a, float b)
{
    return std::fabs(a - b) <= 1e-6f;
}

} // namespace

class TstSliceListenAudio : public QObject {
    Q_OBJECT

private slots:
    // One slice, three devices: the controller at the slice's AF level, two
    // listeners at their own, all different.
    void twoListenersAtDifferentLevelsHearDifferentGains()
    {
        Harness h;
        const int x = h.engine->acquireOwnerMix();
        const int y = h.engine->acquireOwnerMix();
        const int z = h.engine->acquireOwnerMix();
        QVERIFY(x >= 0 && y >= 0 && z >= 0);
        RecordingMixTap tapX;
        RecordingMixTap tapY;
        RecordingMixTap tapZ;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &tapX));
        QVERIFY(h.engine->setOwnerMixAudioTap(y, &tapY));
        QVERIFY(h.engine->setOwnerMixAudioTap(z, &tapZ));
        h.engine->setOwnerMixSliceMask(x, h.bit(0));
        h.slice(0)->setAfGain(50);
        h.engine->setOwnerMixListen(y, h.id(0), 0.25f, false);
        h.engine->setOwnerMixListen(z, h.id(0), 0.75f, false);
        h.feed({0.5f, 0.0f, 0.0f}, 3);
        QVERIFY(near(tapX.lastLeft(), 0.25f));
        QVERIFY(near(tapY.lastLeft(), 0.125f));
        QVERIFY(near(tapZ.lastLeft(), 0.375f));
        h.engine->releaseOwnerMix(x);
        h.engine->releaseOwnerMix(y);
        h.engine->releaseOwnerMix(z);
    }

    // AF 0 on the controller silences the controller only.
    void afZeroOnTheOperatorLeavesVaxAndListenersAudible()
    {
        Harness h;
        FakeAudioBus* vax1 = injectFakeVax(h.engine, 1);
        h.engine->setVaxRxGain(1, 1.0f);
        h.slice(0)->setVaxChannel(1);
        RecordingMixTap master;
        h.engine->setMasterMixAudioTap(&master);
        const int y = h.engine->acquireOwnerMix();
        RecordingMixTap tapY;
        QVERIFY(h.engine->setOwnerMixAudioTap(y, &tapY));
        h.engine->setOwnerMixListen(y, h.id(0), 0.5f, false);

        h.slice(0)->setAfGain(0);
        h.feed({0.5f, 0.0f, 0.0f}, 3);
        QVERIFY(near(master.lastLeft(), 0.0f));
        QVERIFY(near(tapY.lastLeft(), 0.25f));
        QVERIFY(vax1->pushCount() > 0);
        QVERIFY(near(lastVaxLeft(vax1), 0.5f));
        h.engine->setMasterMixAudioTap(nullptr);
        h.engine->releaseOwnerMix(y);
    }

    // A controller's AF change or mute leaves a listener's sum unchanged.
    void aControllerChangeLeavesTheListenerAlone()
    {
        Harness h;
        const int x = h.engine->acquireOwnerMix();
        const int y = h.engine->acquireOwnerMix();
        RecordingMixTap tapX;
        RecordingMixTap tapY;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &tapX));
        QVERIFY(h.engine->setOwnerMixAudioTap(y, &tapY));
        h.engine->setOwnerMixSliceMask(x, h.bit(0));
        h.engine->setOwnerMixListen(y, h.id(0), 0.5f, false);
        h.feed({0.5f, 0.0f, 0.0f}, 2);
        const float before = tapY.lastLeft();
        QVERIFY(near(before, 0.25f));

        h.slice(0)->setAfGain(30);
        h.feed({0.5f, 0.0f, 0.0f}, 2);
        QVERIFY(near(tapX.lastLeft(), 0.15f));
        QVERIFY(near(tapY.lastLeft(), before));

        h.slice(0)->setMuted(true);
        h.feed({0.5f, 0.0f, 0.0f}, 2);
        QVERIFY(near(tapX.lastLeft(), 0.0f));
        QVERIFY(near(tapY.lastLeft(), before));
        h.engine->releaseOwnerMix(x);
        h.engine->releaseOwnerMix(y);
    }

    // A listener's mute leaves the controller's sum and the local output.
    void aListenerMuteLeavesTheControllerAndLocalAlone()
    {
        Harness h;
        RecordingMixTap master;
        h.engine->setMasterMixAudioTap(&master);
        h.engine->setLocalOutputSliceMask(h.bit(0));  // the station controls A
        const int y = h.engine->acquireOwnerMix();
        RecordingMixTap tapY;
        QVERIFY(h.engine->setOwnerMixAudioTap(y, &tapY));
        h.engine->setOwnerMixListen(y, h.id(0), 0.5f, false);
        h.feed({0.5f, 0.0f, 0.0f}, 2);
        QVERIFY(near(master.lastLeft(), 0.5f));
        QVERIFY(near(tapY.lastLeft(), 0.25f));

        h.engine->setOwnerMixListen(y, h.id(0), 0.5f, true);
        h.feed({0.5f, 0.0f, 0.0f}, 2);
        QVERIFY(near(master.lastLeft(), 0.5f));
        QVERIFY(near(tapY.lastLeft(), 0.0f));
        h.engine->setMasterMixAudioTap(nullptr);
        h.engine->releaseOwnerMix(y);
    }

    // Control passes from X to Y. X keeps hearing the slice at the level it
    // was seeded with (the AF it had, Q4) with no gap and no step; Y moves
    // from its listen level to the controller's in no step larger than the
    // difference. No owner mix is taken or released.
    void handOffIsContinuous()
    {
        Harness h(3, /*rampFrames=*/16);
        const int x = h.engine->acquireOwnerMix();
        const int y = h.engine->acquireOwnerMix();
        RecordingMixTap tapX;
        RecordingMixTap tapY;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &tapX));
        QVERIFY(h.engine->setOwnerMixAudioTap(y, &tapY));
        h.slice(0)->setAfGain(60);
        h.engine->setOwnerMixSliceMask(x, h.bit(0));
        h.engine->setOwnerMixListen(y, h.id(0), 0.3f, false);
        h.feed({0.5f, 0.0f, 0.0f}, 4);
        QVERIFY(near(tapX.lastLeft(), 0.3f));
        QVERIFY(near(tapY.lastLeft(), 0.15f));
        const int owners = h.engine->ownerMixCount();

        // The hand-off: X listens at the AF level it had, Y controls.
        h.engine->setOwnerMixListen(x, h.id(0), 0.6f, false);
        h.engine->setOwnerMixSliceMask(x, 0);
        h.engine->setOwnerMixSliceMask(y, h.bit(0));
        h.engine->clearOwnerMixListen(y, h.id(0));
        tapX.clear();
        tapY.clear();
        h.feed({0.5f, 0.0f, 0.0f}, 4);
        QCOMPARE(h.engine->ownerMixCount(), owners);

        const float maxStep = (0.6f - 0.3f) * 0.5f + 1e-6f;
        float prevY = 0.15f;
        for (size_t i = 0; i < tapX.received.size(); i += 2) {
            QVERIFY2(near(tapX.received[i], 0.3f),
                     qPrintable(QStringLiteral("X frame %1: %2").arg(i / 2).arg(tapX.received[i])));
            const float yv = tapY.received[i];
            QVERIFY2(yv > 0.1f, qPrintable(QStringLiteral("Y gap at frame %1").arg(i / 2)));
            QVERIFY2(std::fabs(yv - prevY) <= maxStep,
                     qPrintable(QStringLiteral("Y step at frame %1").arg(i / 2)));
            prevY = yv;
        }
        QVERIFY(near(tapY.lastLeft(), 0.3f));
        h.engine->releaseOwnerMix(x);
        h.engine->releaseOwnerMix(y);
    }

    // Four devices, each controlling one slice and listening to the other
    // four of five: no owner mix and no tap is taken for listening.
    void fourDevicesListeningToFiveSlicesTakeNothingNew()
    {
        Harness h(5);
        std::array<int, 4> owners{};
        std::array<RecordingMixTap, 4> taps;
        for (int k = 0; k < 4; ++k) {
            owners[static_cast<size_t>(k)] = h.engine->acquireOwnerMix();
            QVERIFY(owners[static_cast<size_t>(k)] >= 0);
            QVERIFY(h.engine->setOwnerMixAudioTap(owners[static_cast<size_t>(k)],
                                                  &taps[static_cast<size_t>(k)]));
            h.engine->setOwnerMixSliceMask(owners[static_cast<size_t>(k)], h.bit(k));
        }
        const int ownerCount = h.engine->ownerMixCount();
        const int tapCount = h.engine->sliceAudioTapCount();
        for (int k = 0; k < 4; ++k) {
            for (int s = 0; s < 5; ++s) {
                if (s != k) {
                    h.engine->setOwnerMixListen(owners[static_cast<size_t>(k)], h.id(s), 0.5f,
                                                false);
                }
            }
        }
        QCOMPARE(h.engine->ownerMixCount(), ownerCount);
        QCOMPARE(h.engine->sliceAudioTapCount(), tapCount);
        const std::vector<float> values{0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f};
        h.feed(values, 2);
        float sum = 0.0f;
        for (float v : values) {
            sum += v;
        }
        for (int k = 0; k < 4; ++k) {
            const float own = values[static_cast<size_t>(k)];
            QVERIFY(near(taps[static_cast<size_t>(k)].lastLeft(), own + 0.5f * (sum - own)));
            h.engine->releaseOwnerMix(owners[static_cast<size_t>(k)]);
        }
    }

    // The host listens to a remote device's slice on the Core's own output
    // at its own level; the device muting its slice does not silence it.
    void theHostListensLocallyAtItsOwnLevel()
    {
        Harness h;
        RecordingMixTap master;
        h.engine->setMasterMixAudioTap(&master);
        const int x = h.engine->acquireOwnerMix();
        RecordingMixTap tapX;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &tapX));
        h.engine->setOwnerMixSliceMask(x, h.bit(0));
        h.engine->setLocalOutputSliceMask(h.bit(1) | h.bit(2));
        h.engine->setLocalListen(h.id(0), 0.5f, false);
        h.feed({0.5f, 0.25f, 0.25f}, 2);
        QVERIFY(near(master.lastLeft(), 0.75f));
        QVERIFY(near(tapX.lastLeft(), 0.5f));

        h.slice(0)->setMuted(true);
        h.feed({0.5f, 0.25f, 0.25f}, 2);
        QVERIFY(near(tapX.lastLeft(), 0.0f));
        QVERIFY(near(master.lastLeft(), 0.75f));

        h.engine->setLocalListen(h.id(0), 0.5f, true);
        h.feed({0.5f, 0.25f, 0.25f}, 2);
        QVERIFY(near(master.lastLeft(), 0.5f));
        h.engine->clearLocalListen(h.id(0));
        QCOMPARE(h.engine->localListenMask(), 0u);
        h.engine->setMasterMixAudioTap(nullptr);
        h.engine->releaseOwnerMix(x);
    }

    // An owner mix that stops controlling everything sends silence, not the
    // last sum it built (the drain skipped such an owner's buffers).
    void anOwnerThatStopsControllingSendsSilence()
    {
        Harness h;
        const int x = h.engine->acquireOwnerMix();
        RecordingMixTap tapX;
        QVERIFY(h.engine->setOwnerMixAudioTap(x, &tapX));
        h.engine->setOwnerMixSliceMask(x, h.bit(0));
        h.feed({0.5f, 0.0f, 0.0f}, 2);
        QVERIFY(near(tapX.lastLeft(), 0.5f));
        h.engine->setOwnerMixSliceMask(x, 0);
        h.feed({0.5f, 0.0f, 0.0f}, 2);
        QVERIFY(near(tapX.lastLeft(), 0.0f));
        h.engine->releaseOwnerMix(x);
    }

    // The drain reads the listen state from atomics and never waits on a
    // lock: the only locks in it are the existing non-blocking tries on the
    // speakers and headphones buses (a busy bus skips that push).
    void theDrainTakesNoLock()
    {
        QFile file(QStringLiteral(NEREUS_SOURCE_DIR "/src/core/AudioEngine.cpp"));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(file.readAll());
        const int start = text.indexOf(QStringLiteral("void AudioEngine::drainMixes("));
        QVERIFY(start >= 0);
        const int end = text.indexOf(QStringLiteral("\n}\n"), start);
        QVERIFY(end > start);
        const QString body = text.mid(start, end - start);
        static const QRegularExpression blocking(
            QStringLiteral(R"((lock_guard|scoped_lock|\.lock\(\)|QMutexLocker))"));
        QVERIFY2(!blocking.match(body).hasMatch(), "drainMixes waits on a lock");
        // Every unique_lock is a try (its statement names try_to_lock).
        static const QRegularExpression uniqueLock(QStringLiteral(R"(unique_lock<[^;]*;)"),
                                                   QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator it = uniqueLock.globalMatch(body);
        while (it.hasNext()) {
            const QString statement = it.next().captured(0);
            QVERIFY2(statement.contains(QStringLiteral("try_to_lock")),
                     qPrintable(QStringLiteral("drainMixes waits on: %1").arg(statement)));
        }
    }
};

QTEST_MAIN(TstSliceListenAudio)
#include "tst_slice_listen_audio.moc"
