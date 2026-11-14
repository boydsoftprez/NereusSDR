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
// Modification history (NereusSDR):
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
#include <random>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 1024;
constexpr int kBlocks = 240;      // about five seconds
constexpr int kVaxChannel = 1;
constexpr double kPi = 3.14159265358979323846;

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
};

QTEST_GUILESS_MAIN(TstAudioEngineRxLeveler)
#include "tst_audio_engine_rx_leveler.moc"
