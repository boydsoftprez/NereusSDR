// =================================================================
// tests/tst_device_rate_matcher.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original deterministic acceptance test for the
// already-attributed DeviceRateMatcher (R-AUD-15, V-SW-5).  It drives the
// matcher's writer and reader on two simulated clocks, with no timer and no
// device, and compares its ratio with WDSP's own rmatchV fed the same
// clocks.  Each simulation is single-threaded and deterministic; the four
// independent drift simulations (two offsets, ours and rmatchV) run on
// worker threads, and the threads only run them side by side to keep the
// wall time down.  No simulation shares state with another.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 2 (R-AUD-15, V-SW-5). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/audio/AudioDelayParts.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <future>
#include <limits>
#include <memory>
#include <numbers>
#include <vector>

#ifdef HAVE_WDSP
extern "C" {
// Exact declarations from Thetis Project Files/Source/wdsp/rmatch.h:117-135
// (third_party/wdsp/src/rmatch.h), as RemoteAudioRateMatcher.cpp declares them.
void* create_rmatchV(int in_size, int out_size, int nom_inrate, int nom_outrate,
                     int ringsize, double var);
void destroy_rmatchV(void* ptr);
void xrmatchOUT(void* b, double* out);
void xrmatchIN(void* b, double* in);
void getRMatchDiags(void* b, int* underflows, int* overflows, double* var,
                    int* ringsize, int* nring);
}
#endif

using namespace NereusSDR;

namespace {

constexpr int kRate = 48000;
constexpr int kWriteFrames = 64;
constexpr int kReadFrames = 128;
constexpr double kSineAmplitude = 0.5;
constexpr int kSinePeriodFrames = 48;   // 1 kHz at 48 kHz
// Twice the 1 kHz sine's own largest sample-to-sample step.
const double kMaxAllowedStep = 2.0 * kSineAmplitude * 2.0 * std::numbers::pi * 1000.0 / kRate;

struct SineSource {
    std::array<float, kSinePeriodFrames> sinTable{};
    std::uint64_t frame = 0;

    SineSource()
    {
        for (int i = 0; i < kSinePeriodFrames; ++i) {
            const double phase = 2.0 * std::numbers::pi * i / kSinePeriodFrames;
            sinTable[static_cast<std::size_t>(i)] = static_cast<float>(kSineAmplitude * std::sin(phase));
        }
    }

    void fill(float* interleaved, int frames)
    {
        for (int f = 0; f < frames; ++f) {
            const std::size_t i = static_cast<std::size_t>((frame + static_cast<std::uint64_t>(f))
                                                           % kSinePeriodFrames);
            interleaved[2 * f + 0] = sinTable[i];
            interleaved[2 * f + 1] = -sinTable[i];
        }
        frame += static_cast<std::uint64_t>(frames);
    }
};

// Two clocks on one integer time line.  In units of
// 1 / (48000 * (1e6 + ppm)) s the writer's n-th 64-frame event is at
// n * 64 * (1e6 + ppm) and the reader's m-th 128-frame event, 48 kHz times
// (1 + ppm / 1e6), at m * 128 * 1e6.  A tie runs the writer first.
class ClockSim {
public:
    struct Options {
        int ppm = 0;
        bool reference = false;          // feed WDSP's rmatchV instead of ours
        double readerSkipAtSeconds = -1;  // the reader skips 50 ms of calls here
        double writerBurstAtSeconds = -1; // the writer delivers 100 ms at once here
    };

    explicit ClockSim(const Options& options)
        : m_options(options)
        , m_matcher(DeviceRateMatcher::Config{})
        , m_writeBuffer(static_cast<std::size_t>(kWriteFrames) * 2)
        , m_burstBuffer(static_cast<std::size_t>(kRate / 10) * 2)
        , m_readBuffer(static_cast<std::size_t>(kReadFrames) * 2)
    {
        m_reader = m_matcher.makeReader();
#ifdef HAVE_WDSP
        if (options.reference) {
            m_reference = create_rmatchV(kWriteFrames, kReadFrames, kRate, kRate, 0, 1.0);
            m_referenceIn.assign(static_cast<std::size_t>(kWriteFrames) * 2, 0.0);
            m_referenceOut.assign(static_cast<std::size_t>(kReadFrames) * 2, 0.0);
        }
#endif
        const std::int64_t unitsPerSecond = static_cast<std::int64_t>(kRate) * (1'000'000 + options.ppm);
        m_unitsPerSecond = unitsPerSecond;
        if (options.readerSkipAtSeconds >= 0.0) {
            m_skipStart = static_cast<std::int64_t>(options.readerSkipAtSeconds * unitsPerSecond);
            m_skipEnd = m_skipStart + unitsPerSecond / 20;   // 50 ms
        }
        if (options.writerBurstAtSeconds >= 0.0) {
            m_burstAt = static_cast<std::int64_t>(options.writerBurstAtSeconds * unitsPerSecond);
        }
    }

    ~ClockSim()
    {
#ifdef HAVE_WDSP
        if (m_reference != nullptr) {
            destroy_rmatchV(m_reference);
        }
#endif
    }

    ClockSim(const ClockSim&) = delete;
    ClockSim& operator=(const ClockSim&) = delete;

    DeviceRateMatcher& matcher() { return m_matcher; }
    double maxStep() const { return m_maxStep; }
    std::uint64_t reads() const { return m_readerEvent; }

    // Runs every event before `seconds`.  When `sampleFromSeconds` is not
    // negative, the ratios are averaged from then on (per write).
    void runUntil(double seconds, double sampleFromSeconds = -1.0)
    {
        const std::int64_t end = static_cast<std::int64_t>(seconds * m_unitsPerSecond);
        const std::int64_t sampleFrom = sampleFromSeconds < 0.0
            ? std::numeric_limits<std::int64_t>::max()
            : static_cast<std::int64_t>(sampleFromSeconds * m_unitsPerSecond);
        const std::int64_t writerPeriod = static_cast<std::int64_t>(kWriteFrames) * (1'000'000 + m_options.ppm);
        const std::int64_t readerPeriod = static_cast<std::int64_t>(kReadFrames) * 1'000'000;
        while (true) {
            const std::int64_t nextWriter = m_writerEvent * writerPeriod;
            std::int64_t nextReader = static_cast<std::int64_t>(m_readerEvent) * readerPeriod;
            if (nextReader >= m_skipStart && nextReader < m_skipEnd) {
                nextReader = m_skipEnd;   // skipped, then made up at once
            }
            if (nextWriter >= end && nextReader >= end) {
                break;
            }
            if (nextWriter <= nextReader) {
                writeEvent(nextWriter, nextWriter >= sampleFrom);
            } else {
                readEvent();
            }
        }
    }

    double meanRatio() const { return m_ratioSamples ? m_ratioSum / m_ratioSamples : 0.0; }

private:
    bool feedsOurs() const { return !m_options.reference; }

    void writeEvent(std::int64_t time, bool sample)
    {
        const std::int64_t nowNs = m_writerEvent * kWriteFrames * 1'000'000'000LL / kRate;
        if (m_burstAt >= 0 && time >= m_burstAt) {
            m_burstAt = -1;
            const int frames = kRate / 10;
            m_source.fill(m_burstBuffer.data(), frames);
            m_matcher.write(m_burstBuffer.data(), frames, nowNs);
        }
        m_source.fill(m_writeBuffer.data(), kWriteFrames);
        if (feedsOurs()) {
            m_matcher.write(m_writeBuffer.data(), kWriteFrames, nowNs);
        }
#ifdef HAVE_WDSP
        if (m_reference != nullptr) {
            for (int i = 0; i < kWriteFrames * 2; ++i) {
                m_referenceIn[static_cast<std::size_t>(i)] = m_writeBuffer[static_cast<std::size_t>(i)];
            }
            xrmatchIN(m_reference, m_referenceIn.data());
        }
#endif
        if (sample) {
            if (feedsOurs()) {
                m_ratioSum += m_matcher.ratio();
            }
#ifdef HAVE_WDSP
            if (m_reference != nullptr) {
                int underflows = 0;
                int overflows = 0;
                int ringsize = 0;
                int nring = 0;
                double var = 0.0;
                getRMatchDiags(m_reference, &underflows, &overflows, &var, &ringsize, &nring);
                m_ratioSum += var;
            }
#endif
            ++m_ratioSamples;
        }
        ++m_writerEvent;
    }

    void readEvent()
    {
        if (feedsOurs()) {
            m_reader.read(m_readBuffer.data(), kReadFrames);
            for (int f = 0; f < kReadFrames; ++f) {
                for (int c = 0; c < 2; ++c) {
                    const float value = m_readBuffer[static_cast<std::size_t>(2 * f + c)];
                    m_maxStep = std::max(m_maxStep, std::abs(static_cast<double>(value) - m_last[c]));
                    m_last[c] = value;
                }
            }
        }
#ifdef HAVE_WDSP
        if (m_reference != nullptr) {
            xrmatchOUT(m_reference, m_referenceOut.data());
        }
#endif
        ++m_readerEvent;
    }

    Options m_options;
    DeviceRateMatcher m_matcher;
    MatcherReader m_reader;
    SineSource m_source;
    std::vector<float> m_writeBuffer;
    std::vector<float> m_burstBuffer;
    std::vector<float> m_readBuffer;
#ifdef HAVE_WDSP
    void* m_reference = nullptr;
    std::vector<double> m_referenceIn;
    std::vector<double> m_referenceOut;
#endif
    std::int64_t m_unitsPerSecond = 0;
    std::int64_t m_writerEvent = 0;
    std::uint64_t m_readerEvent = 0;
    std::int64_t m_skipStart = std::numeric_limits<std::int64_t>::max();
    std::int64_t m_skipEnd = std::numeric_limits<std::int64_t>::max();
    std::int64_t m_burstAt = -1;
    double m_last[2] = {0.0, 0.0};
    double m_maxStep = 0.0;
    double m_ratioSum = 0.0;
    std::uint64_t m_ratioSamples = 0;
};

} // namespace

class TestDeviceRateMatcher : public QObject {
    Q_OBJECT

private slots:
    void ringLayoutAndAttach();
    void sizesAndReadout();
    void manualSizeAndRates();
    void callerMemory();
    void driftMatchesRmatch();
    void forcedDryRunStepsUpOnce();
    void forcedOverrunStaysSmooth();
    void flushRestartAndFade();
};

void TestDeviceRateMatcher::ringLayoutAndAttach()
{
    QCOMPARE(kMatcherRingMagic, 0x4E524D52u);
    QCOMPARE(kMatcherNoSkip, std::numeric_limits<std::uint64_t>::max());
    const std::size_t bytes = matcherRingBytes(256, 2);
    QCOMPARE(bytes, sizeof(MatcherRingHeader) + 256 * 2 * sizeof(float));

    auto memory = std::make_unique<std::byte[]>(bytes);
    QVERIFY(constructMatcherRing(memory.get(), 255, 2, 10) == nullptr);   // not a power of two
    MatcherRingHeader* ring = constructMatcherRing(memory.get(), 256, 2, 10);
    QVERIFY(ring != nullptr);
    QCOMPARE(ring->magic, kMatcherRingMagic);
    QCOMPARE(ring->capacityFrames, 256u);
    QCOMPARE(ring->channels, 2u);
    QCOMPARE(ring->slewFrames, 10u);
    QCOMPARE(ring->skipTo.load(), kMatcherNoSkip);
    QCOMPARE(ring->written.load(), std::uint64_t{0});

    QCOMPARE(attachMatcherRing(memory.get(), bytes), ring);
    QVERIFY(attachMatcherRing(memory.get(), bytes - 4) == nullptr);
    QVERIFY(attachMatcherRing(nullptr, bytes) == nullptr);
    ring->channels = 1;
    QVERIFY(attachMatcherRing(memory.get(), bytes) == nullptr);
    ring->channels = 2;
    ring->magic = 0;
    QVERIFY(attachMatcherRing(memory.get(), bytes) == nullptr);
}

void TestDeviceRateMatcher::sizesAndReadout()
{
    DeviceRateMatcher matcher(DeviceRateMatcher::Config{});
    QVERIFY(matcher.valid());
    // Automatic: callback 128 + write 64 frames is 4 ms, so 5 ms.
    QCOMPARE(matcher.delayStepMs(), 5);
    const DeviceRateMatcherStats stats = matcher.stats();
    QCOMPARE(stats.rsizeFrames, 480);       // 2 * 240
    QCOMPARE(stats.capacityFrames, 4096);   // 40 ms rsize 3840 + 68
    QCOMPARE(stats.delayStepMs, 5);
    QVERIFY(!stats.controlActive);
    QCOMPARE(stats.ratio, 1.0);
    QCOMPARE(matcher.ring()->slewFrames, 127u);   // (int)(0.003 * 48000) capped at 256 / 2 - 1
    QCOMPARE(matcher.fillFrames(), 240.0);        // rsize / 2 of silence
    QCOMPARE(matcher.resamplerDelayFrames(), 69);
    QCOMPARE(DeviceRateMatcher::ringBytes(DeviceRateMatcher::Config{}), matcherRingBytes(4096, 2));

    const AudioDelayParts parts = matcher.delayParts(10.0, 3.0);
    QCOMPARE(parts.matcherFillMs, 5.0);
    QCOMPARE(parts.resamplerMs, 69.0 / 48.0);
    QCOMPARE(parts.deviceBufferMs, 10.0);
    QCOMPARE(parts.deviceLatencyMs, 3.0);
    QCOMPARE(parts.totalMs(), parts.matcherFillMs + parts.resamplerMs + parts.deviceBufferMs
                                  + parts.deviceLatencyMs);
    QCOMPARE(AudioDelayParts{}.totalMs(), -1.0);

    // The first read plays the silence.
    MatcherReader reader = matcher.makeReader();
    QVERIFY(reader.valid());
    std::vector<float> out(kReadFrames * 2, 1.0f);
    reader.read(out.data(), kReadFrames);
    QVERIFY(std::all_of(out.begin(), out.end(), [](float v) { return v == 0.0f; }));
    QCOMPARE(matcher.fillFrames(), 112.0);
    QCOMPARE(matcher.stats().dryRuns, std::uint64_t{0});
    QVERIFY(!MatcherReader().valid());
}

void TestDeviceRateMatcher::manualSizeAndRates()
{
    DeviceRateMatcher::Config config;
    config.delayMs = 20;
    DeviceRateMatcher twenty(config);
    QVERIFY(twenty.valid());
    QCOMPARE(twenty.delayStepMs(), 20);
    QCOMPARE(twenty.stats().rsizeFrames, 1920);

    config.delayMs = 7;   // not a step: the next one up
    DeviceRateMatcher seven(config);
    QCOMPARE(seven.delayStepMs(), 10);

    DeviceRateMatcher::Config converting;
    converting.inRate = 48000;
    converting.outRate = 44100;
    QVERIFY(DeviceRateMatcher::ringBytes(converting) > 0);
    DeviceRateMatcher matcher(converting);
#ifdef HAVE_WDSP
    QVERIFY(matcher.valid());
    // varsamp.c: out below in, so min_rate 44100, norm_rate 48000.
    QCOMPARE(matcher.resamplerDelayFrames(), static_cast<int>(140.0 * 48000.0 / 44100.0) / 2 - 1);
#else
    QVERIFY(!matcher.valid());   // copies only: no rate conversion without WDSP
#endif

    DeviceRateMatcher::Config bad;
    bad.outRate = 0;
    QCOMPARE(DeviceRateMatcher::ringBytes(bad), std::size_t{0});
    DeviceRateMatcher invalid(bad);
    QVERIFY(!invalid.valid());
    QVERIFY(invalid.ring() == nullptr);
    QVERIFY(!invalid.makeReader().valid());
    QCOMPARE(invalid.delayParts(1.0, 1.0).totalMs(), -1.0);
}

void TestDeviceRateMatcher::callerMemory()
{
    const DeviceRateMatcher::Config config;
    const std::size_t bytes = DeviceRateMatcher::ringBytes(config);
    auto memory = std::make_unique<std::uint64_t[]>(bytes / sizeof(std::uint64_t) + 1);

    DeviceRateMatcher tooSmall(config, memory.get(), bytes - 8);
    QVERIFY(!tooSmall.valid());

    DeviceRateMatcher matcher(config, memory.get(), bytes);
    QVERIFY(matcher.valid());
    QCOMPARE(static_cast<void*>(matcher.ring()), static_cast<void*>(memory.get()));
    QCOMPARE(attachMatcherRing(memory.get(), bytes), matcher.ring());
}

namespace {

struct DriftResult {
    double ratio = 0.0;
    double maxStep = 0.0;
    DeviceRateMatcherStats settled;
    DeviceRateMatcherStats end;
};

// Ten simulated minutes on one clock pair; the ratio is averaged over the
// last minute.  `reference` runs WDSP's rmatchV on the same clocks instead.
DriftResult runDrift(int ppm, bool reference)
{
    ClockSim::Options options;
    options.ppm = ppm;
    options.reference = reference;
    ClockSim sim(options);
    DriftResult result;
    sim.runUntil(10.0);
    result.settled = sim.matcher().stats();
    sim.runUntil(540.0);
    sim.runUntil(600.0, 540.0);
    result.end = sim.matcher().stats();
    result.ratio = sim.meanRatio();
    result.maxStep = sim.maxStep();
    return result;
}

} // namespace

void TestDeviceRateMatcher::driftMatchesRmatch()
{
#ifndef HAVE_WDSP
    QSKIP("WDSP's rmatchV is the reference; this build has no WDSP");
#else
    constexpr std::array<int, 2> kOffsets{200, -200};   // reader fast, then slow
    std::array<std::future<DriftResult>, 2> ours;
    std::array<std::future<DriftResult>, 2> references;
    for (std::size_t i = 0; i < kOffsets.size(); ++i) {
        ours[i] = std::async(std::launch::async, runDrift, kOffsets[i], false);
        references[i] = std::async(std::launch::async, runDrift, kOffsets[i], true);
    }
    for (std::size_t i = 0; i < kOffsets.size(); ++i) {
        const int ppm = kOffsets[i];
        const DriftResult result = ours[i].get();
        const double reference = references[i].get().ratio;
        const DeviceRateMatcherStats& end = result.end;
        qInfo("ppm %d: ratio %.9f, rmatchV %.9f, expected %.9f; step %d ms, dry runs %llu, overruns %llu",
              ppm, result.ratio, reference, 1.0 + ppm * 1.0e-6, end.delayStepMs,
              static_cast<unsigned long long>(end.dryRuns),
              static_cast<unsigned long long>(end.overruns));
        QVERIFY(end.controlActive);
        QCOMPARE(end.dryRuns, result.settled.dryRuns);
        QCOMPARE(end.overruns, result.settled.overruns);
        QCOMPARE(end.delayStepMs, result.settled.delayStepMs);
        QVERIFY2(std::abs(result.ratio - reference) <= 10.0e-6,
                 qPrintable(QStringLiteral("ppm %1: ratio %2 vs rmatchV %3")
                                .arg(ppm).arg(result.ratio, 0, 'f', 9).arg(reference, 0, 'f', 9)));
        QVERIFY(result.maxStep <= kMaxAllowedStep);
    }
#endif
}

void TestDeviceRateMatcher::forcedDryRunStepsUpOnce()
{
    ClockSim::Options options;
    options.ppm = 200;
    options.readerSkipAtSeconds = 15.0;
    ClockSim sim(options);
    sim.runUntil(14.9);
    const DeviceRateMatcherStats before = sim.matcher().stats();
    QVERIFY(before.controlActive);
    sim.runUntil(15.2);
    const DeviceRateMatcherStats after = sim.matcher().stats();
    sim.runUntil(25.0);
    const DeviceRateMatcherStats end = sim.matcher().stats();
    qInfo("dry run: step %d -> %d -> %d ms, dry runs %llu -> %llu -> %llu, max step %.5f (limit %.5f)",
          before.delayStepMs, after.delayStepMs, end.delayStepMs,
          static_cast<unsigned long long>(before.dryRuns),
          static_cast<unsigned long long>(after.dryRuns),
          static_cast<unsigned long long>(end.dryRuns), sim.maxStep(), kMaxAllowedStep);
    QVERIFY(after.dryRuns > before.dryRuns);
    const auto& steps = DeviceRateMatcher::kDelayStepsMs;
    const auto beforeIndex = std::find(steps.begin(), steps.end(), before.delayStepMs) - steps.begin();
    const auto endIndex = std::find(steps.begin(), steps.end(), end.delayStepMs) - steps.begin();
    QCOMPARE(endIndex, beforeIndex + 1);
    QCOMPARE(end.delayStepMs, after.delayStepMs);
    QCOMPARE(end.dryRuns, after.dryRuns);
    QVERIFY(sim.maxStep() <= kMaxAllowedStep);
}

void TestDeviceRateMatcher::forcedOverrunStaysSmooth()
{
    ClockSim::Options options;
    options.ppm = -200;
    options.writerBurstAtSeconds = 15.0;
    ClockSim sim(options);
    sim.runUntil(14.9);
    const DeviceRateMatcherStats before = sim.matcher().stats();
    QVERIFY(before.controlActive);
    sim.runUntil(25.0);
    const DeviceRateMatcherStats end = sim.matcher().stats();
    qInfo("overrun: overruns %llu -> %llu, dry runs %llu -> %llu, max step %.5f (limit %.5f)",
          static_cast<unsigned long long>(before.overruns),
          static_cast<unsigned long long>(end.overruns),
          static_cast<unsigned long long>(before.dryRuns),
          static_cast<unsigned long long>(end.dryRuns), sim.maxStep(), kMaxAllowedStep);
    QVERIFY(end.overruns > before.overruns);
    QVERIFY(sim.maxStep() <= kMaxAllowedStep);
}

void TestDeviceRateMatcher::flushRestartAndFade()
{
    ClockSim::Options options;
    ClockSim sim(options);
    sim.runUntil(4.0);
    QVERIFY(sim.matcher().stats().controlActive);

    // A restart turns the control off until 3 s of reads and writes again.
    sim.matcher().requestRestart();
    sim.runUntil(4.5);
    QVERIFY(!sim.matcher().stats().controlActive);
    QCOMPARE(sim.matcher().ratio(), 1.0);
    sim.runUntil(8.0);
    QVERIFY(sim.matcher().stats().controlActive);

    // A flush drops what is queued and starts again from the target.
    sim.matcher().requestFlush();
    sim.runUntil(8.5);
    QVERIFY(sim.maxStep() <= kMaxAllowedStep);

    // The fade: down to silence over ntslew (127) frames, then silence.
    DeviceRateMatcher matcher(DeviceRateMatcher::Config{});
    MatcherReader reader = matcher.makeReader();
    SineSource source;
    std::vector<float> in(kWriteFrames * 2);
    std::vector<float> out(kReadFrames * 2);
    for (int i = 0; i < 20; ++i) {
        source.fill(in.data(), kWriteFrames);
        matcher.write(in.data(), kWriteFrames, i * 1'333'333LL);
        if (i % 2 == 1) {
            reader.read(out.data(), kReadFrames);
        }
    }
    QVERIFY(!reader.fadedOut());
    reader.requestFadeOut();
    reader.read(out.data(), kReadFrames);
    QVERIFY(reader.fadedOut());
    QCOMPARE(out[2 * 127 + 0], 0.0f);
    QCOMPARE(out[2 * 127 + 1], 0.0f);
    source.fill(in.data(), kWriteFrames);
    matcher.write(in.data(), kWriteFrames, 30 * 1'333'333LL);
    reader.read(out.data(), kReadFrames);
    QVERIFY(std::all_of(out.begin(), out.end(), [](float v) { return v == 0.0f; }));
}

QTEST_GUILESS_MAIN(TestDeviceRateMatcher)
#include "tst_device_rate_matcher.moc"
