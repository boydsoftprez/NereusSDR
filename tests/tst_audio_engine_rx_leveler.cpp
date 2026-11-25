// no-port-check: NereusSDR-original test coverage.
// =================================================================
// tests/tst_audio_engine_rx_leveler.cpp  (NereusSDR)
// =================================================================
//
// Where the receive leveler sits in AudioEngine::rxBlockReady: per
// receiver, after VAX and the receiver taps have taken their copy, before
// the speakers mix and the AF level. So:
//   * VAX and the receiver taps get the plain audio with RXL on;
//   * with RXL off the speakers get exactly what they got before;
//   * the AF slider stays 1:1 with RXL on, and the leveler's gain does
//     not depend on where the slider is.
// TCI reads the receive channel (TciServer's audio taps on RxChannel),
// ahead of AudioEngine, so it is plain by construction.
//
// And RadioModel's half (this class is its friend): what publishRxLevelers
// sends for Auto, Custom, a mode RXL leaves alone and a remote window; the
// reset count after a retune and on a reused slice id; the transmitting
// slice's leveler starting over; a block larger than the scratch; two
// receivers kept apart.
//
// Modification history (NereusSDR):
//   2026-11-22 - The RadioModel half, the MOX reset, the reset count, the
//                large block and two receivers. J.J. Boyd (KG4VCF),
//                AI-assisted via Claude Code.
//   2026-11-22 - Re-review: hiss after an over stays unlifted with a
//                station still said present; a start over (retune, mode,
//                filter, reused id, a filter drag) lets the gain go at the
//                release rate. J.J. Boyd (KG4VCF), AI-assisted via Claude
//                Code.
//   2026-11-14 — New. J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/dsp/RxLeveler.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <numbers>
#include <random>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 1024;
constexpr int kBlocks = 240;      // about five seconds
constexpr int kVaxChannel = 1;
constexpr double kPi = std::numbers::pi;

// A quiet, speech-like signal: bursts of a few tones, on and off, well
// under the listening target, so the leveler lifts it.
std::vector<float> quietVoice()
{
    std::vector<float> x(static_cast<size_t>(kFrames) * kBlocks * 2);
    std::mt19937 rng(7);
    std::normal_distribution<double> noise(0.0, 0.0005);
    for (int i = 0; i < kFrames * kBlocks; ++i) {
        const double t = i / 48000.0;
        const bool syllable = std::fmod(t, 0.4) < 0.25;
        const double v = (syllable ? 0.02 : 0.0)
            * (std::sin(2.0 * kPi * 400.0 * t) + 0.5 * std::sin(2.0 * kPi * 1100.0 * t))
            + noise(rng);
        x[static_cast<size_t>(2 * i)] = x[static_cast<size_t>(2 * i + 1)] = static_cast<float>(v);
    }
    return x;
}

// The hiss of quietVoice() alone: nobody talking.
std::vector<float> hissOnly()
{
    std::vector<float> x(static_cast<size_t>(kFrames) * kBlocks * 2);
    std::mt19937 rng(11);
    std::normal_distribution<double> noise(0.0, 0.0005);
    for (int i = 0; i < kFrames * kBlocks; ++i) {
        const float v = static_cast<float>(noise(rng));
        x[static_cast<size_t>(2 * i)] = x[static_cast<size_t>(2 * i + 1)] = v;
    }
    return x;
}

RxLevelerParams levelerOn()
{
    RxLevelerParams p;
    p.on = true;
    p.stationPresent = true;
    p.targetDb = RxLeveler::kAutoTargetDb;
    p.maxBoostDb = RxLeveler::kAutoMaxBoostDb;
    p.maxCutDb = RxLeveler::kAutoMaxCutDb;
    p.attackMs = RxLeveler::kAutoAttackMs;
    p.releaseMs = RxLeveler::kAutoReleaseMs;
    return p;
}

class RecordingSliceTap final : public SliceAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
    }
    void skip(int, int) noexcept override {}
    std::vector<float> received;
};

std::unique_ptr<FakeAudioBus> openBus(const QString& name)
{
    auto bus = std::make_unique<FakeAudioBus>(name);
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    bus->open(format);
    return bus;
}

struct Result {
    QByteArray speakers;
    QByteArray vax;
    std::vector<float> tap;
    double gainDb{0.0};
};

// One receiver, its VAX channel and a receiver tap, fed `input` block by
// block. `leveler` is what RadioModel would publish: set after the slice's
// own settings, so nothing republishes over it.
// `switchOffAt` turns the leveler off from that block on.
Result run(const std::vector<float>& input, int afGain, RxLevelerMode mode,
           const RxLevelerParams& leveler, int switchOffAt = -1)
{
    auto radio = std::make_unique<RadioModel>();
    AudioEngine* engine = radio->audioEngine();
    auto speakers = openBus(QStringLiteral("FakeSpeakers"));
    FakeAudioBus* speakersView = speakers.get();
    engine->setSpeakersBusForTest(std::move(speakers));
    auto vax = openBus(QStringLiteral("FakeVax"));
    FakeAudioBus* vaxView = vax.get();
    engine->setVaxBusForTest(kVaxChannel, std::move(vax));
    radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                               /*defaultRateHz=*/192000);
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    const int id = radio->addSlice();
    SliceModel* slice = radio->sliceById(id);
    slice->setVaxChannel(kVaxChannel);
    slice->setAfGain(afGain);
    slice->setRxLevelerMode(mode);
    engine->setSliceStreaming(id, true);
    engine->setSliceRxLeveler(id, leveler);
    RecordingSliceTap tap;
    engine->setSliceAudioTap(id, &tap);

    for (int b = 0; b < kBlocks; ++b) {
        if (b == switchOffAt) {
            engine->setSliceRxLeveler(id, RxLevelerParams{});
        }
        engine->rxBlockReady(id, input.data() + static_cast<size_t>(b) * kFrames * 2, kFrames);
    }
    Result r;
    r.gainDb = engine->sliceRxLevelerGainDb(id);
    engine->clearSliceAudioTap(&tap);
    r.speakers = speakersView->buffer();
    r.vax = vaxView->buffer();
    r.tap = tap.received;
    return r;
}

// One receiver on a RadioModel with its speakers, as run() makes it, kept
// for a test to drive block by block.
struct Rig {
    std::unique_ptr<RadioModel> radio;
    AudioEngine* engine{nullptr};
    FakeAudioBus* speakers{nullptr};
    int id{-1};

    Rig()
    {
        radio = std::make_unique<RadioModel>();
        engine = radio->audioEngine();
        auto bus = openBus(QStringLiteral("FakeSpeakers"));
        speakers = bus.get();
        engine->setSpeakersBusForTest(std::move(bus));
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        id = addReceiver();
    }

    int addReceiver()
    {
        const int added = radio->addSlice();
        SliceModel* slice = radio->sliceById(added);
        slice->setDspMode(DSPMode::USB);
        slice->setRxLevelerMode(RxLevelerMode::Auto);
        engine->setSliceStreaming(added, true);
        return added;
    }

    // What RadioModel published for the slice, with a station present: the
    // reset count stays RadioModel's.
    void publishStation(int slice, bool present = true)
    {
        RxLevelerParams p = levelerOn();
        p.stationPresent = present;
        p.resetCount = engine->sliceRxLeveler(slice).resetCount;
        engine->setSliceRxLeveler(slice, p);
    }

    // After a reset count change, with a station still said present and
    // only hiss: the gain goes back to unity at the release rate, never in
    // one block, and gets well down within a second (the leveler, had it
    // not started over, would hold the gain through that pause).
    void expectLetsGo(int slice, double from)
    {
        const std::vector<float> hiss = hissOnly();
        constexpr double kDt = double(kFrames) / 48000.0;
        double last = from;
        for (int b = 0; b < 47; ++b) {
            feed(slice, hiss, b, 1);
            const double g = engine->sliceRxLevelerGainDb(slice);
            const double releaseStep =
                std::abs(last) * (1.0 - std::exp(-kDt * 1000.0 / RxLeveler::kAutoReleaseMs));
            QVERIFY2(std::abs(last - g) <= releaseStep + 1e-4,
                     qPrintable(QStringLiteral("block %1 stepped from %2 to %3").arg(b).arg(last).arg(g)));
            last = g;
        }
        QVERIFY2(last < 0.5 * from,
                 qPrintable(QStringLiteral("%1 dB after a second, from %2").arg(last).arg(from)));
    }

    void feed(int slice, const std::vector<float>& input, int fromBlock, int blocks)
    {
        for (int b = fromBlock; b < fromBlock + blocks; ++b) {
            engine->rxBlockReady(slice, input.data() + static_cast<size_t>(b % kBlocks) * kFrames * 2,
                                 kFrames);
        }
    }
};

}  // namespace

class TstAudioEngineRxLeveler : public QObject {
    Q_OBJECT

private slots:
    // VAX and the receiver taps get the plain audio while the speakers get
    // the leveled audio.
    void vaxAndTapsStayPlain()
    {
        const std::vector<float> input = quietVoice();
        const Result plain = run(input, 100, RxLevelerMode::Off, RxLevelerParams{});
        const Result leveled = run(input, 100, RxLevelerMode::Auto, levelerOn());
        QVERIFY(leveled.gainDb > 3.0);                 // the leveler is lifting
        QVERIFY(!plain.speakers.isEmpty());
        QVERIFY(leveled.speakers != plain.speakers);   // and the speakers hear it
        QVERIFY(!plain.vax.isEmpty());
        QCOMPARE(leveled.vax, plain.vax);
        QVERIFY(leveled.tap == input);
        QVERIFY(plain.tap == input);
    }

    // RXL off is the audio as it was: the same bytes to the speakers as a
    // receiver that never had the leveler, also after it was on.
    void offIsBitIdentical()
    {
        const std::vector<float> input = quietVoice();
        const Result never = run(input, 100, RxLevelerMode::Off, RxLevelerParams{});
        RxLevelerParams offButPresent = levelerOn();
        offButPresent.on = false;
        const Result off = run(input, 100, RxLevelerMode::Off, offButPresent);
        QCOMPARE(off.speakers, never.speakers);
        QCOMPARE(off.gainDb, 0.0);

        // On for the first half, off for the second: the second half is the
        // plain audio again.
        const int half = kBlocks / 2;
        const Result switched = run(input, 100, RxLevelerMode::Auto, levelerOn(), half);
        QCOMPARE(switched.speakers.size(), never.speakers.size());
        const qsizetype from = never.speakers.size() / 2 + kFrames * 2 * qsizetype(sizeof(float));
        QVERIFY(switched.speakers.first(from) != never.speakers.first(from));
        QCOMPARE(switched.speakers.sliced(from), never.speakers.sliced(from));
        QCOMPARE(switched.gainDb, 0.0);
    }

    // The AF slider is the volume: with RXL on, half the slider is half the
    // speakers, sample for sample, and the leveler's gain is the same
    // wherever the slider is.
    void afSliderStaysOneToOne()
    {
        const std::vector<float> input = quietVoice();
        const Result full = run(input, 100, RxLevelerMode::Auto, levelerOn());
        const Result half = run(input, 50, RxLevelerMode::Auto, levelerOn());
        const Result quarter = run(input, 25, RxLevelerMode::Auto, levelerOn());
        QVERIFY(full.gainDb > 3.0);
        QCOMPARE(half.gainDb, full.gainDb);
        QCOMPARE(quarter.gainDb, full.gainDb);
        QCOMPARE(half.speakers.size(), full.speakers.size());
        QCOMPARE(quarter.speakers.size(), full.speakers.size());
        const auto* f = reinterpret_cast<const float*>(full.speakers.constData());
        const auto* h = reinterpret_cast<const float*>(half.speakers.constData());
        const auto* q = reinterpret_cast<const float*>(quarter.speakers.constData());
        const qsizetype n = full.speakers.size() / qsizetype(sizeof(float));
        double loudest = 0.0;
        for (qsizetype i = 0; i < n; ++i) {
            loudest = std::max(loudest, std::abs(static_cast<double>(f[i])));
            QVERIFY2(std::abs(h[i] - 0.5f * f[i]) <= 1e-6f * std::max(1.0f, std::abs(f[i])),
                     qPrintable(QStringLiteral("sample %1: %2 vs %3").arg(i).arg(h[i]).arg(f[i])));
            QVERIFY2(std::abs(q[i] - 0.25f * f[i]) <= 1e-6f * std::max(1.0f, std::abs(f[i])),
                     qPrintable(QStringLiteral("sample %1: %2 vs %3").arg(i).arg(q[i]).arg(f[i])));
        }
        QVERIFY(loudest > 0.01);
    }

    // ── RadioModel's half ─────────────────────────────────────────────────

    // Auto is published as its preset whatever the knobs say; Custom
    // passes the knobs; a mode RXL leaves alone turns it off.
    void publishResolvesAutoAndPassesCustom()
    {
        Rig rig;
        SliceModel* slice = rig.radio->sliceById(rig.id);
        slice->setRxLevelerTargetDb(-18);
        slice->setRxLevelerMaxBoostDb(14);
        slice->setRxLevelerAttackMs(2500);
        slice->setRxLevelerReleaseMs(900);
        slice->setRxLevelerMaxCutDb(9);
        rig.radio->publishRxLevelers();
        RxLevelerParams p = rig.engine->sliceRxLeveler(rig.id);
        QVERIFY(p.on);
        QCOMPARE(p.targetDb, RxLeveler::kAutoTargetDb);
        QCOMPARE(p.maxBoostDb, RxLeveler::kAutoMaxBoostDb);
        QCOMPARE(p.attackMs, RxLeveler::kAutoAttackMs);
        QCOMPARE(p.releaseMs, RxLeveler::kAutoReleaseMs);
        QCOMPARE(p.maxCutDb, RxLeveler::kAutoMaxCutDb);

        slice->setRxLevelerMode(RxLevelerMode::Custom);
        rig.radio->publishRxLevelers();
        p = rig.engine->sliceRxLeveler(rig.id);
        QVERIFY(p.on);
        QCOMPARE(p.targetDb, -18);
        QCOMPARE(p.maxBoostDb, 14);
        QCOMPARE(p.attackMs, 2500);
        QCOMPARE(p.releaseMs, 900);
        QCOMPARE(p.maxCutDb, 9);

        for (DSPMode mode : {DSPMode::CWU, DSPMode::FM, DSPMode::DIGU}) {
            slice->setDspMode(mode);
            rig.radio->publishRxLevelers();
            QVERIFY2(!rig.engine->sliceRxLeveler(rig.id).on, qPrintable(QString::number(int(mode))));
        }
        slice->setDspMode(DSPMode::LSB);
        rig.radio->publishRxLevelers();
        QVERIFY(rig.engine->sliceRxLeveler(rig.id).on);
        slice->setRxLevelerMode(RxLevelerMode::Off);
        rig.radio->publishRxLevelers();
        QVERIFY(!rig.engine->sliceRxLeveler(rig.id).on);
    }

    // A remote window's audio arrives leveled by the Core: it publishes off.
    void aRemoteWindowPublishesOff()
    {
        RadioModel remote{RadioModel::Role::Remote};
        SliceModel* slice = remote.sliceById(remote.addSliceWithStationId(0));
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setRxLevelerMode(RxLevelerMode::Auto);
        remote.publishRxLevelers();
        QVERIFY(!remote.audioEngine()->sliceRxLeveler(0).on);
    }

    // A retune starts the leveler over: the gain it had for the last
    // frequency lets go at the release rate. A meter tick with no retune
    // leaves it alone.
    void aRetuneStartsTheLevelerOver()
    {
        const std::vector<float> input = quietVoice();
        Rig rig;
        SliceModel* slice = rig.radio->sliceById(rig.id);
        rig.radio->updateRxLevelers();      // the station check knows the frequency
        const int before = rig.engine->sliceRxLeveler(rig.id).resetCount;
        rig.radio->updateRxLevelers();
        QCOMPARE(rig.engine->sliceRxLeveler(rig.id).resetCount, before);
        rig.publishStation(rig.id);
        rig.feed(rig.id, input, 0, 200);
        QVERIFY(rig.engine->sliceRxLevelerGainDb(rig.id) > 3.0);

        slice->setFrequency(slice->frequency() + 5000.0);
        rig.radio->updateRxLevelers();
        QCOMPARE_NE(rig.engine->sliceRxLeveler(rig.id).resetCount, before);
        rig.publishStation(rig.id);         // a station there too, for this test
        rig.expectLetsGo(rig.id, rig.engine->sliceRxLevelerGainDb(rig.id));
    }

    // A mode or a filter change starts it over too.
    void aModeOrFilterChangeStartsTheLevelerOver()
    {
        const std::vector<float> input = quietVoice();
        Rig rig;
        SliceModel* slice = rig.radio->sliceById(rig.id);
        int count = rig.engine->sliceRxLeveler(rig.id).resetCount;
        slice->setFilter(200, 2600);
        QCOMPARE_NE(rig.engine->sliceRxLeveler(rig.id).resetCount, count);
        count = rig.engine->sliceRxLeveler(rig.id).resetCount;
        rig.publishStation(rig.id);
        rig.feed(rig.id, input, 0, 200);
        QVERIFY(rig.engine->sliceRxLevelerGainDb(rig.id) > 3.0);
        slice->setDspMode(DSPMode::LSB);
        QCOMPARE_NE(rig.engine->sliceRxLeveler(rig.id).resetCount, count);
        rig.publishStation(rig.id);
        rig.expectLetsGo(rig.id, rig.engine->sliceRxLevelerGainDb(rig.id));
    }

    // A filter drag changes the filter on every step, each a start over:
    // the gain never steps, it lets go at the release rate.
    void aFilterDragDoesNotStepTheGain()
    {
        const std::vector<float> input = quietVoice();
        Rig rig;
        SliceModel* slice = rig.radio->sliceById(rig.id);
        rig.publishStation(rig.id);
        rig.feed(rig.id, input, 0, 200);
        double last = rig.engine->sliceRxLevelerGainDb(rig.id);
        QVERIFY(last > 3.0);
        constexpr double kDt = double(kFrames) / 48000.0;
        const int count = rig.engine->sliceRxLeveler(rig.id).resetCount;
        for (int step = 0; step < 30; ++step) {
            slice->setFilter(100, 2900 - 20 * step - 20);
            rig.publishStation(rig.id);
            rig.feed(rig.id, input, 200 + step, 1);
            const double g = rig.engine->sliceRxLevelerGainDb(rig.id);
            const double releaseStep =
                std::abs(last) * (1.0 - std::exp(-kDt * 1000.0 / RxLeveler::kAutoReleaseMs));
            QVERIFY2(std::abs(last - g) <= releaseStep + 1e-4,
                     qPrintable(QStringLiteral("drag step %1: %2 to %3 dB").arg(step).arg(last).arg(g)));
            last = g;
        }
        QCOMPARE_NE(rig.engine->sliceRxLeveler(rig.id).resetCount, count);
        QVERIFY(last > 0.0);
    }

    // A receiver made again on a slice id starts with nothing the last one
    // there learned.
    void aReusedSliceIdStartsTheLevelerOver()
    {
        const std::vector<float> input = quietVoice();
        Rig rig;
        const int second = rig.addReceiver();
        rig.publishStation(second);
        rig.feed(second, input, 0, 200);
        QVERIFY(rig.engine->sliceRxLevelerGainDb(second) > 3.0);
        const int before = rig.engine->sliceRxLeveler(second).resetCount;

        rig.radio->removeSlice(second);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(rig.radio->sliceById(second) == nullptr);
        const int again = rig.addReceiver();
        QCOMPARE(again, second);
        QCOMPARE_NE(rig.engine->sliceRxLeveler(again).resetCount, before);
        rig.publishStation(again);
        rig.expectLetsGo(again, rig.engine->sliceRxLevelerGainDb(again));
    }

    // The station check still says a station is there across the unkey
    // (its hold, or the S-meter frozen at key-down): with only hiss after
    // the over, the gain stays at unity. The leveler kept the hiss it
    // learned before the over.
    void hissAfterAnOverStaysUnliftedWithAStationStillSaid()
    {
        const std::vector<float> input = quietVoice();
        const std::vector<float> hiss = hissOnly();
        Rig rig;
        rig.radio->requestTxHandoffToSlice(rig.id);
        QCOMPARE(rig.radio->txBoundSlice()->sliceIndex(), rig.id);
        rig.publishStation(rig.id);
        rig.feed(rig.id, input, 0, 200);
        QVERIFY(rig.engine->sliceRxLevelerGainDb(rig.id) > 3.0);
        rig.engine->setMoxStateForTest(true);
        rig.feed(rig.id, input, 200, 5);
        rig.engine->setMoxStateForTest(false);
        rig.publishStation(rig.id, /*present=*/true);
        double most = 0.0;
        for (int b = 0; b < 141; ++b) {     // three seconds
            rig.feed(rig.id, hiss, b, 1);
            most = std::max(most, rig.engine->sliceRxLevelerGainDb(rig.id));
        }
        QVERIFY2(most < 0.5, qPrintable(QStringLiteral("most lift on the hiss %1 dB").arg(most)));
    }

    // While the slice transmits its leveler starts over, so after the over
    // the hiss is not lifted by the gain it had at key-down.
    void transmittingStartsTheLevelerOver()
    {
        const std::vector<float> input = quietVoice();
        Rig rig;
        rig.radio->requestTxHandoffToSlice(rig.id);
        QVERIFY(rig.radio->txBoundSlice() != nullptr);
        QCOMPARE(rig.radio->txBoundSlice()->sliceIndex(), rig.id);
        rig.publishStation(rig.id);
        rig.feed(rig.id, input, 0, 200);
        QVERIFY(rig.engine->sliceRxLevelerGainDb(rig.id) > 3.0);

        rig.engine->setMoxStateForTest(true);
        rig.feed(rig.id, input, 200, 5);
        QCOMPARE(rig.engine->sliceRxLevelerGainDb(rig.id), 0.0);
        rig.engine->setMoxStateForTest(false);
        // Unkeyed, nobody on the frequency.
        rig.publishStation(rig.id, /*present=*/false);
        rig.feed(rig.id, input, 205, 1);
        QCOMPARE(rig.engine->sliceRxLevelerGainDb(rig.id), 0.0);
    }

    // A block larger than the leveler's scratch is leveled and queued a
    // scratch at a time: every frame goes through the leveler, which ends
    // where one leveler running the same chunks ends, and the speakers
    // hear it.
    void aBlockLargerThanTheScratchIsLeveledWhole()
    {
        constexpr int kChunk = AudioEngine::kMixScratchMinFrames;
        constexpr int kBig = 2 * kChunk + 500;
        constexpr int kBigBlocks = 12;
        const std::vector<float> voice = quietVoice();
        std::vector<float> input(static_cast<size_t>(kBig) * kBigBlocks * 2);
        for (size_t i = 0; i < input.size(); ++i) {
            input[i] = voice[i % voice.size()];
        }
        Rig rig;
        rig.publishStation(rig.id);
        const RxLevelerParams published = rig.engine->sliceRxLeveler(rig.id);
        for (int b = 0; b < kBigBlocks; ++b) {
            rig.engine->rxBlockReady(rig.id, input.data() + static_cast<size_t>(b) * kBig * 2, kBig);
        }
        QVERIFY(rig.engine->sliceRxLevelerGainDb(rig.id) > 3.0);

        // The same chunks through a leveler of our own.
        RxLeveler reference;
        std::vector<float> expected = input;
        for (int b = 0; b < kBigBlocks; ++b) {
            for (int done = 0; done < kBig;) {
                const int n = std::min(kChunk, kBig - done);
                reference.process(expected.data() + (static_cast<size_t>(b) * kBig + done) * 2, n,
                                  48000.0, published);
                done += n;
            }
        }
        QCOMPARE(rig.engine->sliceRxLevelerGainDb(rig.id), reference.gainDb());
        const QByteArray speakers = rig.speakers->buffer();
        QVERIFY(speakers.size() >= qsizetype(kBig * 2 * sizeof(float)));
        QVERIFY(speakers.size() <= qsizetype(expected.size() * sizeof(float)));
    }

    // Two receivers keep their levelers apart: a station on one lifts that
    // one only, and turning one off or starting it over leaves the other.
    void twoReceiversAreKeptApart()
    {
        const std::vector<float> input = quietVoice();
        Rig rig;
        const int other = rig.addReceiver();
        rig.publishStation(rig.id, /*present=*/true);
        rig.publishStation(other, /*present=*/false);
        for (int b = 0; b < 200; ++b) {
            rig.feed(rig.id, input, b, 1);
            rig.feed(other, input, b, 1);
        }
        const double lifted = rig.engine->sliceRxLevelerGainDb(rig.id);
        QVERIFY(lifted > 3.0);
        QCOMPARE(rig.engine->sliceRxLevelerGainDb(other), 0.0);

        // The other turned off, then started over: the first keeps its gain.
        rig.radio->sliceById(other)->setRxLevelerMode(RxLevelerMode::Off);
        rig.radio->sliceById(other)->setFilter(300, 2500);
        rig.radio->publishRxLevelers();
        rig.publishStation(rig.id, /*present=*/true);
        rig.feed(other, input, 200, 2);
        rig.feed(rig.id, input, 200, 1);
        QVERIFY2(rig.engine->sliceRxLevelerGainDb(rig.id) > lifted - 0.5,
                 qPrintable(QStringLiteral("%1, was %2")
                                .arg(rig.engine->sliceRxLevelerGainDb(rig.id)).arg(lifted)));
    }
};

QTEST_GUILESS_MAIN(TstAudioEngineRxLeveler)
#include "tst_audio_engine_rx_leveler.moc"
