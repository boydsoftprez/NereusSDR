// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_fftw_planner_threads.cpp  (NereusSDR)
// =================================================================
//
// FFTW's planner is not thread-safe unless it is made so, and the switch
// is per precision library: fftw_make_planner_thread_safe() covers only
// double-precision libfftw3 (WDSP's plans), while the display FFTs plan
// with single-precision libfftw3f (fftwf_plan_dft_1d in
// FFTEngine::replanFft, fftwf_plan_dft_r2c_1d in the WidebandFftEngine
// constructor). Every pan's FFTEngine replans on its own spectrum thread
// and each radio has its own pool, so two float plans can run at once.
//
//   - FFTEngine replans on four threads at once, cycling FFT sizes, with
//     no WdspEngine in the process: an engine must make the float planner
//     thread-safe itself.
//   - WidebandFftEngine construction on two threads at once, alongside
//     FFTEngine replans on two more.
//
// Before the fix both abort (SIGABRT or SIGSEGV inside libfftw3f's
// planner) within a few runs.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/FFTEngine.h"
#include "core/WidebandFftEngine.h"

#include <QLoggingCategory>
#include <QtTest/QtTest>

#include <atomic>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kReplansPerThread = 600;
constexpr int kWidebandBuildsPerThread = 300;

void replanLoop(int receiverId, int offset, std::atomic<int>* go, std::atomic<int>* done)
{
    FFTEngine engine(receiverId);
    const int sizes[] = {1024, 2048, 4096, 8192, 16384, 32768};
    const QVector<float> iq(64, 0.0f);
    while (go->load() == 0) {
    }
    for (int i = 0; i < kReplansPerThread; ++i) {
        engine.setFftSize(sizes[(i + offset) % 6]);
        engine.feedIQ(iq);  // applies the pending size: replanFft()
        done->fetch_add(1);
    }
}

void widebandLoop(std::atomic<int>* go, std::atomic<int>* done)
{
    while (go->load() == 0) {
    }
    for (int i = 0; i < kWidebandBuildsPerThread; ++i) {
        WidebandFftEngine engine;  // plans in the constructor
        done->fetch_add(1);
    }
}

} // namespace

class TestFftwPlannerThreads : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Thousands of replans each log two info lines; keep the output to
        // the results.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.info=false"));
    }

    void fftEngineReplansOnFourThreads()
    {
        std::atomic<int> go{0};
        std::atomic<int> done{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back(replanLoop, t, t, &go, &done);
        }
        go.store(1);
        for (std::thread& th : threads) {
            th.join();
        }
        QCOMPARE(done.load(), 4 * kReplansPerThread);
    }

    void widebandBuildsBesideReplans()
    {
        std::atomic<int> go{0};
        std::atomic<int> done{0};
        std::vector<std::thread> threads;
        threads.emplace_back(replanLoop, 0, 0, &go, &done);
        threads.emplace_back(replanLoop, 1, 3, &go, &done);
        threads.emplace_back(widebandLoop, &go, &done);
        threads.emplace_back(widebandLoop, &go, &done);
        go.store(1);
        for (std::thread& th : threads) {
            th.join();
        }
        QCOMPARE(done.load(), 2 * kReplansPerThread + 2 * kWidebandBuildsPerThread);
    }
};

QTEST_GUILESS_MAIN(TestFftwPlannerThreads)
#include "tst_fftw_planner_threads.moc"
