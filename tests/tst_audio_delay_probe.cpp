// =================================================================
// tests/tst_audio_delay_probe.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.  The audio delay probe
// (V-HW-8): the speakers click, the input detector, the click and hit
// matcher, the capture time a PortAudio input callback reports, and the
// engine wiring (the click on the speakers push only, a Test Mic lease
// while the probe runs).
//
// No real device is opened: the speakers and VAX outputs are FakeAudioBus,
// and the microphone helper is the scripted fake, run by re-executing this
// binary with --fake-capture-child probe.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 1 (V-HW-8). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/CaptureSupervisor.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"
#include "fakes/FakeCaptureChild.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kRate = 48000;
constexpr std::int64_t kSecondNs = 1'000'000'000;
constexpr double kPi = 3.14159265358979323846;

std::int64_t framesToNs(int frames)
{
    return static_cast<std::int64_t>(std::llround(double(frames) * 1e9 / kRate));
}

// Deterministic uniform noise at the given RMS (uniform in [-a, a] has RMS
// a / sqrt(3)).
class Noise {
public:
    explicit Noise(double rms) : m_amplitude(rms * std::sqrt(3.0)) {}
    float next()
    {
        m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;
        const double unit = double(m_state >> 11) / double(1ULL << 53);
        return float((unit * 2.0 - 1.0) * m_amplitude);
    }

private:
    double m_amplitude;
    std::uint64_t m_state = 0x2545F4914F6CDD1DULL;
};

// `frames` of background with a 48-frame pulse of `level` at `pulseAt`
// (none when negative).
template <typename Background>
std::vector<float> buffer(int frames, Background&& background, int pulseAt, float level)
{
    std::vector<float> out(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        out[size_t(i)] = background(i);
    }
    if (pulseAt >= 0) {
        for (int i = pulseAt; i < std::min(frames, pulseAt + AudioDelayProbeClicker::kClickFrames); ++i) {
            out[size_t(i)] += level;
        }
    }
    return out;
}

CaptureSupervisor::Options fakeOptions(const QString& scenario)
{
    CaptureSupervisor::Options options;
    options.program = QCoreApplication::applicationFilePath();
    options.arguments = {QStringLiteral("--fake-capture-child"), scenario};
    return options;
}

// One engine with fake speakers and a fake VAX 1, one slice at AF 100 on
// VAX 1, and the scripted fake helper in place of the real one.
struct Harness {
    std::unique_ptr<RadioModel> radio;
    AudioEngine* engine = nullptr;
    FakeAudioBus* speakers = nullptr;
    FakeAudioBus* vax = nullptr;
    int slice = -1;
};

std::unique_ptr<Harness> makeHarness()
{
    auto h = std::make_unique<Harness>();
    h->radio = std::make_unique<RadioModel>();
    h->engine = h->radio->audioEngine();
    h->engine->setCaptureSupervisorOptionsForTest(fakeOptions(QStringLiteral("probe")));
    AudioFormat format;
    format.sampleRate = kRate;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
    speakers->open(format);
    h->speakers = speakers.get();
    h->engine->setSpeakersBusForTest(std::move(speakers));
    auto vax = std::make_unique<FakeAudioBus>(QStringLiteral("FakeVax1"));
    vax->open(format);
    h->vax = vax.get();
    h->engine->setVaxBusForTest(1, std::move(vax));
    h->engine->setVolume(1.0f);
    h->radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
    h->slice = h->radio->addSlice();
    h->radio->sliceById(h->slice)->setAfGain(100);
    h->radio->sliceById(h->slice)->setVaxChannel(1);
    return h;
}

// Feeds `seconds` of a 440 Hz stereo tone in 64-frame blocks.
void feed(Harness& h, double seconds)
{
    constexpr int kBlock = 64;
    std::vector<float> block(kBlock * 2);
    const int total = int(seconds * kRate);
    for (int sent = 0; sent < total; sent += kBlock) {
        for (int i = 0; i < kBlock; ++i) {
            const double t = double(sent + i) / kRate;
            block[size_t(2 * i)] = float(0.2 * std::sin(2.0 * kPi * 440.0 * t));
            block[size_t(2 * i + 1)] = float(0.1 * std::sin(2.0 * kPi * 440.0 * t));
        }
        h.engine->rxBlockReady(h.slice, block.data(), kBlock);
    }
}

const float* floatsOf(const FakeAudioBus* bus)
{
    return reinterpret_cast<const float*>(bus->buffer().constData());
}

qsizetype floatCount(const FakeAudioBus* bus)
{
    return bus->buffer().size() / qsizetype(sizeof(float));
}

bool waitForCapture(AudioEngine* engine, CaptureSupervisor::Status::State state, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (engine->captureStatus().state != state) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        QTest::qWait(10);
    }
    return true;
}

} // namespace

class TstAudioDelayProbe : public QObject {
    Q_OBJECT

private slots:
    // ── Clicker ───────────────────────────────────────────────────────────

    void clickerConstants()
    {
        QCOMPARE(AudioDelayProbeClicker::kClickFrames, 48);
        QCOMPARE(AudioDelayProbeClicker::kClickLevel, 0.5f);
        QCOMPARE(AudioDelayProbeClicker::kIntervalFrames, 48000);
    }

    void clickerOnceASecondIn480FrameBlocks()
    {
        AudioDelayProbeClicker clicker;
        constexpr int kFrames = 480;
        std::vector<float> block(kFrames * 2);
        for (int b = 0; b < 250; ++b) {
            for (int f = 0; f < kFrames; ++f) {
                block[size_t(2 * f)] = 0.1f;
                block[size_t(2 * f + 1)] = -0.2f;
            }
            const bool started = clicker.process(block.data(), kFrames, 2);
            const bool expected = (b == 0 || b == 100 || b == 200);
            QVERIFY2(started == expected, qPrintable(QStringLiteral("block %1").arg(b)));
            for (int f = 0; f < kFrames; ++f) {
                const bool clicked = expected && f < 48;
                QCOMPARE(block[size_t(2 * f)], clicked ? 0.1f + 0.5f : 0.1f);
                QCOMPARE(block[size_t(2 * f + 1)], clicked ? -0.2f + 0.5f : -0.2f);
            }
        }
    }

    void clickerContinuesIntoTheNextBlock()
    {
        AudioDelayProbeClicker clicker;
        constexpr int kFrames = 32;
        std::vector<float> block(kFrames * 2, 0.0f);
        QVERIFY(clicker.process(block.data(), kFrames, 2));
        for (float v : block) {
            QCOMPARE(v, 0.5f);
        }
        std::fill(block.begin(), block.end(), 0.25f);
        QVERIFY(!clicker.process(block.data(), kFrames, 2));
        for (int f = 0; f < kFrames; ++f) {
            const float expected = f < 16 ? 0.25f + 0.5f : 0.25f;
            QCOMPARE(block[size_t(2 * f)], expected);
            QCOMPARE(block[size_t(2 * f + 1)], expected);
        }
        // The next click starts at the first block whose start is at least
        // 48000 frames after this one's: block 1500.
        for (int b = 2; b < 1600; ++b) {
            std::fill(block.begin(), block.end(), 0.0f);
            const bool started = clicker.process(block.data(), kFrames, 2);
            QVERIFY2(started == (b == 1500), qPrintable(QStringLiteral("block %1").arg(b)));
        }
    }

    void clickerWaitsForABlockStart()
    {
        // 470-frame blocks: 48000 is not a block start, so the click starts
        // at the next one (block 103 at frame 48410), then block 206.
        AudioDelayProbeClicker clicker;
        constexpr int kFrames = 470;
        std::vector<float> block(kFrames * 2, 0.0f);
        for (int b = 0; b < 250; ++b) {
            const bool started = clicker.process(block.data(), kFrames, 2);
            QVERIFY2(started == (b == 0 || b == 103 || b == 206),
                     qPrintable(QStringLiteral("block %1").arg(b)));
        }
    }

    void clickerIgnoresAnEmptyBlock()
    {
        AudioDelayProbeClicker clicker;
        QVERIFY(!clicker.process(nullptr, 64, 2));
        std::vector<float> block(128, 0.0f);
        QVERIFY(!clicker.process(block.data(), 0, 2));
        QVERIFY(clicker.process(block.data(), 64, 2));       // the first click is still to come
    }

    // ── Detector ──────────────────────────────────────────────────────────

    void detectorConstants()
    {
        QCOMPARE(AudioDelayProbeDetector::kMinThreshold, 0.02f);
        QCOMPARE(AudioDelayProbeDetector::kRmsFactor, 8.0f);
        QCOMPARE(AudioDelayProbeDetector::kRmsWindowMs, 100);
        QCOMPARE(AudioDelayProbeDetector::kHoldOffMs, 500);
    }

    void detectorFindsAStepInNoiseWithHoldOff()
    {
        AudioDelayProbeDetector detector(kRate);
        Noise noise(0.001);
        const auto background = [&noise](int) { return noise.next(); };
        constexpr int kFrames = 9600;                           // 200 ms buffers
        const std::int64_t t0 = 1'000'000'000;

        // 200 ms of noise first: nothing.
        auto lead = buffer(kFrames, background, -1, 0.0f);
        QVERIFY(!detector.process(lead.data(), kFrames, t0 - framesToNs(kFrames)));

        // A 0.5 step at frame 1000 of the buffer whose frame 0 is at t0.
        auto a = buffer(kFrames, background, 1000, 0.5f);
        const auto hit = detector.process(a.data(), kFrames, t0);
        QVERIFY(hit);
        const std::int64_t expected = t0 + 1000LL * kSecondNs / kRate;
        QVERIFY2(std::llabs(*hit - expected) <= framesToNs(1),
                 qPrintable(QStringLiteral("hit %1, expected %2").arg(*hit).arg(expected)));

        // 200 ms later: inside the hold-off.
        auto b = buffer(kFrames, background, 1000, 0.5f);
        QVERIFY(!detector.process(b.data(), kFrames, t0 + framesToNs(kFrames)));
        auto c = buffer(kFrames, background, -1, 0.0f);
        QVERIFY(!detector.process(c.data(), kFrames, t0 + framesToNs(2 * kFrames)));

        // 600 ms after the first: a hit.
        auto d = buffer(kFrames, background, 1000, 0.5f);
        const auto second = detector.process(d.data(), kFrames, t0 + framesToNs(3 * kFrames));
        QVERIFY(second);
        QVERIFY(std::llabs(*second - (expected + 600'000'000)) <= framesToNs(1));
    }

    void detectorThresholdFollowsTheBackground()
    {
        // A tone at RMS 0.1 makes the threshold 0.8: a 0.5 click is not
        // found, a 1.0 click is.
        AudioDelayProbeDetector detector(kRate);
        const auto tone = [](int i) {
            return float(0.1 * std::sqrt(2.0) * std::sin(2.0 * kPi * 1000.0 * i / kRate));
        };
        constexpr int kFrames = 48000;
        const std::int64_t t0 = 5 * kSecondNs;
        auto lead = buffer(9600, tone, -1, 0.0f);
        QVERIFY(!detector.process(lead.data(), 9600, t0 - framesToNs(9600)));
        auto soft = buffer(kFrames, tone, 1000, 0.5f);
        QVERIFY(!detector.process(soft.data(), kFrames, t0));
        auto loud = buffer(kFrames, tone, 1000, 1.0f);
        const auto hit = detector.process(loud.data(), kFrames, t0 + kSecondNs);
        QVERIFY(hit);
        QVERIFY(std::llabs(*hit - (t0 + kSecondNs + framesToNs(1000))) <= framesToNs(1));
    }

    void detectorMinimumThresholdInSilence()
    {
        AudioDelayProbeDetector detector(kRate);
        const auto silence = [](int) { return 0.0f; };
        auto lead = buffer(9600, silence, -1, 0.0f);
        QVERIFY(!detector.process(lead.data(), 9600, 0));
        auto quiet = buffer(9600, silence, 100, 0.015f);
        QVERIFY(!detector.process(quiet.data(), 9600, framesToNs(9600)));
        auto audible = buffer(9600, silence, 100, 0.03f);
        QVERIFY(detector.process(audible.data(), 9600, framesToNs(2 * 9600)));
    }

    void detectorAtAnotherRate()
    {
        constexpr int kRate44 = 44100;
        AudioDelayProbeDetector detector(kRate44);
        const auto silence = [](int) { return 0.0f; };
        auto lead = buffer(kRate44 / 5, silence, -1, 0.0f);
        QVERIFY(!detector.process(lead.data(), kRate44 / 5, 0));
        auto a = buffer(kRate44, silence, 441, 0.5f);
        const auto hit = detector.process(a.data(), kRate44, kSecondNs);
        QVERIFY(hit);
        QCOMPARE(*hit, kSecondNs + 10'000'000);                // frame 441 at 44.1 kHz
    }

    // ── Capture time ──────────────────────────────────────────────────────

    void captureTimeFromTheAdcTime()
    {
        // inputBufferAdcTime 10 ms before currentTime.
        QCOMPARE(audioProbeCaptureNs(5 * kSecondNs, 10.0, 9.99, 480, 48000, 0.005),
                 5 * kSecondNs - 10'000'000);
    }

    void captureTimeWithoutTheAdcTime()
    {
        // 480 frames at 48 kHz (10 ms) plus the stream's 5 ms input latency.
        QCOMPARE(audioProbeCaptureNs(5 * kSecondNs, 10.0, 0.0, 480, 48000, 0.005),
                 5 * kSecondNs - 15'000'000);
        QCOMPARE(audioProbeCaptureNs(5 * kSecondNs, 0.0, 0.0, 441, 44100, 0.0),
                 5 * kSecondNs - 10'000'000);
    }

    void probeClockIsSteady()
    {
        const std::int64_t a = audioProbeNowNs();
        const std::int64_t b = audioProbeNowNs();
        QVERIFY(b >= a);
        const auto steady = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now().time_since_epoch()).count();
        QVERIFY(std::llabs(steady - b) < kSecondNs);
    }

    // ── Matcher ───────────────────────────────────────────────────────────

    void matcherConstants()
    {
        QCOMPARE(AudioDelayProbeMatcher::kPairWindowNs, std::int64_t(500'000'000));
        QCOMPARE(AudioDelayProbeMatcher::kSummaryEvery, 30);
    }

    void matcherPairsAHit23MsLater()
    {
        AudioDelayProbeMatcher matcher;
        for (int k = 1; k <= 30; ++k) {
            matcher.addClick(k * kSecondNs);
            matcher.addHit(k * kSecondNs + 23'000'000);
        }
        QCOMPARE(matcher.takeSummary(),
                 QStringLiteral("Audio delay probe: median 23.0 ms, min 23.0, max 23.0 over 30 "
                                "clicks; readout not available"));
    }

    void matcherDropsALateHit()
    {
        AudioDelayProbeMatcher matcher;
        matcher.addHit(kSecondNs);                              // no click yet
        for (int k = 1; k <= 29; ++k) {
            matcher.addClick(k * kSecondNs);
            matcher.addHit(k * kSecondNs + 20'000'000);
        }
        matcher.addClick(100 * kSecondNs);
        matcher.addHit(100 * kSecondNs + 600'000'000);          // 600 ms after the last click
        matcher.addHit(100 * kSecondNs - 1);                    // before it
        QCOMPARE(matcher.takeSummary(), QString());
        matcher.addClick(200 * kSecondNs);
        matcher.addHit(200 * kSecondNs + 500'000'000);          // the window's edge pairs
        matcher.addHit(200 * kSecondNs + 20'000'000);           // one hit per click
        QCOMPARE(matcher.takeSummary(),
                 QStringLiteral("Audio delay probe: median 20.0 ms, min 20.0, max 500.0 over 30 "
                                "clicks; readout not available"));
    }

    void matcherSummaryEveryThirtyPairs()
    {
        AudioDelayProbeMatcher matcher;
        const int delaysMs[3] = {20, 21, 22};
        for (int k = 0; k < 30; ++k) {
            QCOMPARE(matcher.takeSummary(), QString());
            const std::int64_t click = (k + 1) * kSecondNs;
            matcher.addClick(click);
            matcher.addHit(click + delaysMs[k % 3] * 1'000'000LL);
        }
        const QString summary = matcher.takeSummary();
        QCOMPARE(summary,
                 QStringLiteral("Audio delay probe: median 21.0 ms, min 20.0, max 22.0 over 30 "
                                "clicks; readout not available"));
        QCOMPARE(matcher.takeSummary(), QString());             // taken once

        matcher.setReadoutMs(19.5);
        for (int k = 0; k < 30; ++k) {
            const std::int64_t click = (k + 100) * kSecondNs;
            matcher.addClick(click);
            matcher.addHit(click + delaysMs[k % 3] * 1'000'000LL);
        }
        QCOMPARE(matcher.takeSummary(),
                 QStringLiteral("Audio delay probe: median 21.0 ms, min 20.0, max 22.0 over 30 "
                                "clicks; readout 19.5 ms"));

        matcher.setReadoutMs(std::nullopt);
        for (int k = 0; k < 30; ++k) {
            const std::int64_t click = (k + 200) * kSecondNs;
            matcher.addClick(click);
            matcher.addHit(click + 21'000'000);
        }
        QVERIFY(matcher.takeSummary().endsWith(QStringLiteral("; readout not available")));
    }

    // ── Engine ────────────────────────────────────────────────────────────

    void probeOffLeavesTheSpeakersUntouched()
    {
        auto plain = makeHarness();
        auto toggled = makeHarness();
        toggled->engine->setDelayProbeEnabled(true);
        toggled->engine->setDelayProbeEnabled(false);
        QVERIFY(!toggled->engine->isDelayProbeEnabled());
        feed(*plain, 2.0);
        feed(*toggled, 2.0);
        QVERIFY(floatCount(plain->speakers) > 0);
        QCOMPARE(toggled->speakers->buffer(), plain->speakers->buffer());
        QCOMPARE(toggled->vax->buffer(), plain->vax->buffer());
    }

    void clickGoesToTheSpeakersOnly()
    {
        auto plain = makeHarness();
        auto probed = makeHarness();
        probed->engine->setDelayProbeEnabled(true);
        QVERIFY(probed->engine->isDelayProbeEnabled());
        feed(*plain, 2.0);
        feed(*probed, 2.0);

        // VAX never carries it.
        QVERIFY(floatCount(plain->vax) > 0);
        QCOMPARE(probed->vax->buffer(), plain->vax->buffer());

        // The speakers carry +0.5 on both channels for 48 frames from the
        // first block and from frame 48000; every other sample is untouched.
        QCOMPARE(floatCount(probed->speakers), floatCount(plain->speakers));
        const float* a = floatsOf(plain->speakers);
        const float* b = floatsOf(probed->speakers);
        const qsizetype frames = floatCount(plain->speakers) / 2;
        QVERIFY(frames > 48000 + 48);
        int clicked = 0;
        for (qsizetype f = 0; f < frames; ++f) {
            const bool inClick = (f < 48) || (f >= 48000 && f < 48048);
            for (int c = 0; c < 2; ++c) {
                const float plainValue = a[2 * f + c];
                const float probedValue = b[2 * f + c];
                if (inClick) {
                    QVERIFY2(std::abs((probedValue - plainValue) - 0.5f) < 1e-6f,
                             qPrintable(QStringLiteral("frame %1").arg(f)));
                    ++clicked;
                } else {
                    QVERIFY2(probedValue == plainValue,
                             qPrintable(QStringLiteral("frame %1").arg(f)));
                }
            }
        }
        QCOMPARE(clicked, 2 * 2 * 48);
        probed->engine->setDelayProbeEnabled(false);
    }

    void probeHoldsATestMicLeaseAndTakesHits()
    {
        auto h = makeHarness();
        QCOMPARE(h->engine->captureStatus().state, CaptureSupervisor::Status::State::Closed);
        h->engine->setDelayProbeEnabled(true);
        QVERIFY(waitForCapture(h->engine, CaptureSupervisor::Status::State::Ready, 5000));
        QVERIFY(h->engine->captureHelperProcessIdForTest() > 0);
        // The fake helper exits if ProbeEnable comes before its Ready; its
        // hits reach the engine.
        QTRY_VERIFY_WITH_TIMEOUT(h->engine->delayProbeHitCountForTest() >= 2, 3000);
        QCOMPARE(h->engine->captureStatus().state, CaptureSupervisor::Status::State::Ready);

        h->engine->setDelayProbeEnabled(false);
        const quint64 hits = h->engine->delayProbeHitCountForTest();
        QVERIFY(waitForCapture(h->engine, CaptureSupervisor::Status::State::Closed, 3000));
        QTRY_COMPARE_WITH_TIMEOUT(h->engine->captureHelperProcessIdForTest(), qint64(0), 3000);
        QTest::qWait(150);
        QCOMPARE(h->engine->delayProbeHitCountForTest(), hits);
    }
};

int main(int argc, char* argv[])
{
    if (argc > 2 && std::strcmp(argv[1], "--fake-capture-child") == 0) {
        return NereusSDR::Test::runFakeCaptureChild(QString::fromLocal8Bit(argv[2]));
    }
    QCoreApplication app(argc, argv);
    TstAudioDelayProbe test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_audio_delay_probe.moc"
