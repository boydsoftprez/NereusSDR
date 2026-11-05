// no-port-check: test-only. Exercises SsbModulationAnalyzer.
#include <QtTest>
#include "core/SsbModulationAnalyzer.h"

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
        QVERIFY2(std::fabs(snap.crestDb) < 0.5, qPrintable(QString::number(snap.crestDb)));
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
    }
};

QTEST_MAIN(TestSsbModulationAnalyzer)
#include "tst_ssb_modulation_analyzer.moc"
