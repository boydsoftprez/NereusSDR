// no-port-check: test-only. Exercises SsbModulationAnalyzer.
#include <QtTest>
#include "core/SsbModulationAnalyzer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kFs = 48000;

std::vector<float> constantIq(double seconds, double i, double q)
{
    const int n = static_cast<int>(seconds * kFs);
    std::vector<float> iq(static_cast<std::size_t>(n) * 2);
    for (int s = 0; s < n; ++s) {
        iq[static_cast<std::size_t>(2 * s)] = static_cast<float>(i);
        iq[static_cast<std::size_t>(2 * s + 1)] = static_cast<float>(q);
    }
    return iq;
}

void feed(SsbModulationAnalyzer& analyzer, const std::vector<float>& iq, int block = 64)
{
    const int frames = static_cast<int>(iq.size() / 2);
    for (int off = 0; off + block <= frames; off += block) {
        analyzer.pushIq(iq.data() + static_cast<std::size_t>(2 * off), block);
    }
}

// One block is 1 ms of the analyzer's own clock, so these tests run on the
// clock seam and never wait.
constexpr int kBlockFrames = kFs / 1000;

// Calls each(ms) after each block.
template <typename Each>
void feedClocked(SsbModulationAnalyzer& analyzer, const std::vector<float>& iq, Each each)
{
    const int blocks = static_cast<int>(iq.size() / 2) / kBlockFrames;
    for (int b = 0; b < blocks; ++b) {
        analyzer.pushIq(iq.data() + static_cast<std::size_t>(2 * b * kBlockFrames), kBlockFrames);
        analyzer.advanceClockForTest(std::chrono::milliseconds(1));
        each(b);
    }
}

void feedClocked(SsbModulationAnalyzer& analyzer, const std::vector<float>& iq)
{
    feedClocked(analyzer, iq, [](int) {});
}

// Two equal tones, complex as the TX I/Q is. The envelope beats at 1200 Hz,
// 40 samples a period at 48 kHz, so a sample lands on each crest.
std::vector<float> twoToneIq(double seconds, double each = 0.25)
{
    const int n = static_cast<int>(seconds * kFs);
    std::vector<float> iq(static_cast<std::size_t>(n) * 2);
    for (int s = 0; s < n; ++s) {
        const double t = static_cast<double>(s) / kFs;
        const double a = 2.0 * M_PI * 700.0 * t;
        const double b = 2.0 * M_PI * 1900.0 * t;
        iq[static_cast<std::size_t>(2 * s)] = static_cast<float>(each * (std::cos(a) + std::cos(b)));
        iq[static_cast<std::size_t>(2 * s + 1)] = static_cast<float>(each * (std::sin(a) + std::sin(b)));
    }
    return iq;
}

} // namespace

class TestSsbModulationAnalyzer : public QObject
{
    Q_OBJECT
private slots:
    void silence_isNotPresent()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feed(analyzer, constantIq(0.5, 0.0, 0.0));
        const auto snap = analyzer.snapshot();
        QVERIFY(!snap.signalPresent);
        QCOMPARE(snap.peakPct, 0.0);
    }

    void halfScaleTone_readsMinusSixDbfs()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        // Envelope 0.5 is 50% of full scale and -6.02 dBFS. A steady
        // tone has no crest.
        feed(analyzer, constantIq(0.5, 0.5, 0.0));
        const auto snap = analyzer.snapshot();
        QVERIFY(snap.signalPresent);
        QVERIFY2(std::fabs(snap.peakPct - 50.0) < 0.5, qPrintable(QString::number(snap.peakPct)));
        QVERIFY2(std::fabs(snap.avgPct - 50.0) < 0.5, qPrintable(QString::number(snap.avgPct)));
        QVERIFY2(std::fabs(snap.peakDbfs - (-6.0206)) < 0.2, qPrintable(QString::number(snap.peakDbfs)));
        QVERIFY(snap.crestDb.has_value());
        QVERIFY2(std::fabs(*snap.crestDb) < 0.5, qPrintable(QString::number(*snap.crestDb)));
        QVERIFY(snap.peakHoldPct >= 49.0);
        QVERIFY(!snap.scope.empty());
        QCOMPARE(snap.scopeRateHz, kFs / 6);
    }

    void fullScale_isZeroDbfs()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feed(analyzer, constantIq(0.5, 1.0, 0.0));
        const auto snap = analyzer.snapshot();
        QVERIFY(snap.signalPresent);
        QVERIFY2(std::fabs(snap.peakPct - 100.0) < 0.5, qPrintable(QString::number(snap.peakPct)));
        QVERIFY2(std::fabs(snap.peakDbfs) < 0.2, qPrintable(QString::number(snap.peakDbfs)));
    }

    void reset_clearsTheHold()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feed(analyzer, constantIq(0.5, 0.8, 0.0));
        QVERIFY(analyzer.snapshot().peakHoldPct > 70.0);
        analyzer.reset();
        const auto snap = analyzer.snapshot();
        QVERIFY(!snap.signalPresent);
        QCOMPARE(snap.peakHoldPct, 0.0);
        QVERIFY(snap.scope.empty());
        QVERIFY(!snap.crestDb.has_value());
    }

    // Review finding 1: a new block peak meets the decayed hold, not the
    // peak that started it.
    void holdSettlesOnALowerSteadyLevel()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feedClocked(analyzer, constantIq(0.001, 1.0, 0.0));
        QCOMPARE(analyzer.snapshot().peakHoldPct, 100.0);
        double lowest = 100.0;
        feedClocked(analyzer, constantIq(4.0, 0.5, 0.0), [&](int) {
            lowest = std::min(lowest, analyzer.snapshot().peakHoldPct);
        });
        QVERIFY2(lowest >= 49.99, qPrintable(QString::number(lowest)));
        const auto snap = analyzer.snapshot();
        QVERIFY2(std::fabs(snap.peakHoldPct - 50.0) < 0.01, qPrintable(QString::number(snap.peakHoldPct)));
        QVERIFY(snap.crestDb.has_value());
        QVERIFY2(std::fabs(*snap.crestDb) < 0.1, qPrintable(QString::number(*snap.crestDb)));
    }

    void holdStaysAboveAnOverThresholdThroughADecay()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feedClocked(analyzer, constantIq(0.001, 1.0, 0.0));
        constexpr double kOverDbfs = -1.0;
        double lowest = 0.0;
        feedClocked(analyzer, constantIq(3.0, 0.95, 0.0), [&](int) {
            lowest = std::min(lowest, analyzer.snapshot().peakHoldDbfs);
        });
        QVERIFY2(lowest >= kOverDbfs, qPrintable(QString::number(lowest)));
    }

    void holdKeepsItsTimeAndDecayRate()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feedClocked(analyzer, constantIq(0.2, 0.8, 0.0));
        analyzer.advanceClockForTest(std::chrono::milliseconds(1400));
        QVERIFY(std::fabs(analyzer.snapshot().peakHoldPct - 80.0) < 0.01);
        // Past the 1.5 s hold by 200 ms: 100 percent per second is 20 down.
        analyzer.advanceClockForTest(std::chrono::milliseconds(301));
        const double held = analyzer.snapshot().peakHoldPct;
        QVERIFY2(std::fabs(held - 60.0) < 0.5, qPrintable(QString::number(held)));
    }

    // Review finding 3: nothing live once the samples stop.
    void samplesStopping_clearsWhatWasLive()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feedClocked(analyzer, constantIq(0.5, 0.5, 0.0));
        auto snap = analyzer.snapshot();
        QVERIFY(snap.signalPresent);
        QVERIFY(snap.scope.size() >= 2);

        // Within the bound it is still the same over.
        analyzer.advanceClockForTest(std::chrono::milliseconds(
            static_cast<int>(SsbModulationAnalyzer::kInputStaleMs) - 5));
        QVERIFY(analyzer.snapshot().signalPresent);

        analyzer.advanceClockForTest(std::chrono::milliseconds(6));
        snap = analyzer.snapshot();
        QVERIFY(!snap.signalPresent);
        QCOMPARE(snap.avgPct, 0.0);
        QCOMPARE(snap.avgDbfs, -120.0);
        QVERIFY(snap.scope.empty());
        QVERIFY(!snap.crestDb.has_value());
        // The hold may finish its decay.
        QVERIFY(snap.peakHoldPct > 49.0);
        analyzer.advanceClockForTest(std::chrono::milliseconds(3000));
        QCOMPARE(analyzer.snapshot().peakHoldPct, 0.0);

        // The next over starts clean.
        feedClocked(analyzer, constantIq(0.5, 0.25, 0.0));
        snap = analyzer.snapshot();
        QVERIFY(snap.signalPresent);
        QVERIFY2(std::fabs(snap.avgPct - 25.0) < 0.5, qPrintable(QString::number(snap.avgPct)));
    }

    // The maintainer's crest: peak over RMS of the envelope, over one stretch.
    void crest_twoToneIsThreeDb()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feedClocked(analyzer, twoToneIq(1.0));
        const auto crest = analyzer.snapshot().crestDb;
        QVERIFY(crest.has_value());
        QVERIFY2(std::fabs(*crest - 3.01) < 0.1, qPrintable(QString::number(*crest)));
    }

    void crest_steadyToneIsZeroDb()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feedClocked(analyzer, constantIq(1.0, 0.5, 0.0));
        const auto crest = analyzer.snapshot().crestDb;
        QVERIFY(crest.has_value());
        QVERIFY2(std::fabs(*crest) < 0.01, qPrintable(QString::number(*crest)));
    }

    void crest_aPauseNeverInflatesItThenShowsNoValue()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feedClocked(analyzer, twoToneIq(1.0));
        const auto tone = analyzer.snapshot().crestDb;
        QVERIFY(tone.has_value());

        // Keyed and silent: the blocks keep coming, below the presence
        // minimum.
        double highest = -100.0;
        bool ended = false;
        bool cameBack = false;
        int endedAtMs = -1;
        feedClocked(analyzer, constantIq(1.0, 0.0, 0.0), [&](int ms) {
            const auto crest = analyzer.snapshot().crestDb;
            if (crest) {
                highest = std::max(highest, *crest);
                cameBack = cameBack || ended;
            } else if (!ended) {
                ended = true;
                endedAtMs = ms;
            }
        });
        QVERIFY2(highest <= *tone + 0.2,
                 qPrintable(QStringLiteral("%1 against %2").arg(highest).arg(*tone)));
        QVERIFY(ended);
        QVERIFY(!cameBack);
        QVERIFY2(endedAtMs <= SsbModulationAnalyzer::kCrestWindowMs + 10,
                 qPrintable(QString::number(endedAtMs)));
    }

    void crest_leavesPausesOutOfASpeechLikeOver()
    {
        // A constant-envelope burst, then silence, over and over: every
        // block counted is the same level, so a pause cannot raise it.
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        for (int i = 0; i < 6; ++i) {
            feedClocked(analyzer, constantIq(0.05, 0.5, 0.0));
            feedClocked(analyzer, constantIq(0.05, 0.0, 0.0));
            const auto crest = analyzer.snapshot().crestDb;
            QVERIFY(crest.has_value());
            QVERIFY2(std::fabs(*crest) < 0.01, qPrintable(QString::number(*crest)));
        }
    }

    void crest_ignoresThePeakOutsideItsWindow()
    {
        SsbModulationAnalyzer analyzer;
        analyzer.setSampleRate(kFs);
        feedClocked(analyzer, constantIq(0.001, 1.0, 0.0));
        feedClocked(analyzer, constantIq(0.1, 0.5, 0.0));
        const auto withSpike = analyzer.snapshot().crestDb;
        QVERIFY(withSpike.has_value());
        QVERIFY(*withSpike > 1.0);
        feedClocked(analyzer, constantIq(SsbModulationAnalyzer::kCrestWindowMs / 1000.0, 0.5, 0.0));
        const auto after = analyzer.snapshot().crestDb;
        QVERIFY(after.has_value());
        QVERIFY2(std::fabs(*after) < 0.01, qPrintable(QString::number(*after)));
    }
};

QTEST_MAIN(TestSsbModulationAnalyzer)
#include "tst_ssb_modulation_analyzer.moc"
