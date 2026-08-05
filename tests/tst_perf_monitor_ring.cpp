// =================================================================
// tests/tst_perf_monitor_ring.cpp  (NereusSDR-native)
// =================================================================
// 2026-07-27  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//
// Coverage for PerfMonitor's rolling-stats ring.
//
// The ring was rewritten from a QMutex-guarded array to a lock-free
// one (per-slot std::atomic<double>, relaxed indices) because
// recordAudioFillMs() is called from PortAudioBus::paCallback -- the
// PortAudio real-time audio callback -- where CLAUDE.md forbids
// holding a mutex.  PerfMonitor shipped with no tests at all, so that
// rewrite touched avg / max / min arithmetic with nothing guarding it.
// These cases pin the behaviour through the public API.
//
// PerfMonitor is a process-wide singleton and snapshotAndClearDeltas()
// resets delta counters but NOT the rolling rings, so each case seeds
// a full window (kRingSize == 120) of its own values.  That makes every
// assertion independent of whatever ran before it.
// =================================================================
#include <QtTest/QtTest>

#include "core/PerfMonitor.h"

#include <thread>

using namespace NereusSDR;

namespace {
// Matches PerfMonitor::kRingSize.  Kept local because the constant is
// private; if it ever changes, overfilling here still works -- the ring
// only ever retains the most recent kRingSize samples.
constexpr int kWindow = 120;
}  // namespace

class TstPerfMonitorRing : public QObject {
    Q_OBJECT

private slots:
    void avgMaxMinOverAFullWindow()
    {
        PerfMonitor& pm = PerfMonitor::instance();
        // Fill the whole window with a known constant, then a single
        // outlier at each end.
        for (int i = 0; i < kWindow; ++i) {
            pm.recordAudioFillMs(10.0);
        }
        pm.recordAudioFillMs(40.0);   // max
        pm.recordAudioFillMs(1.0);    // min

        const PerfMonitor::Snapshot s = pm.snapshotAndClearDeltas();
        QCOMPARE(s.audioFillSamples, kWindow);
        QVERIFY(qFuzzyCompare(s.audioFillMaxMs, 40.0));
        QVERIFY(qFuzzyCompare(s.audioFillMinMs, 1.0));
        // 118 x 10 + 40 + 1 over 120 samples.
        const double expectedAvg = (118.0 * 10.0 + 40.0 + 1.0) / kWindow;
        QVERIFY(qAbs(s.audioFillAvgMs - expectedAvg) < 1e-9);
    }

    // The ring must retain only the most recent window: values pushed
    // more than kRingSize samples ago cannot influence the stats.
    void oldSamplesFallOutOfTheWindow()
    {
        PerfMonitor& pm = PerfMonitor::instance();
        pm.recordAudioFillMs(999.0);            // should be evicted
        for (int i = 0; i < kWindow; ++i) {
            pm.recordAudioFillMs(5.0);
        }

        const PerfMonitor::Snapshot s = pm.snapshotAndClearDeltas();
        QCOMPARE(s.audioFillSamples, kWindow);
        QVERIFY(qFuzzyCompare(s.audioFillMaxMs, 5.0));
        QVERIFY(qFuzzyCompare(s.audioFillMinMs, 5.0));
        QVERIFY(qFuzzyCompare(s.audioFillAvgMs, 5.0));
    }

    // Concurrent pushes are legal by design -- the speakers bus and the
    // VAX bus can both be live -- so this must not trip TSan/ASan or
    // produce an out-of-range index.  Values may be lost or interleaved;
    // what must hold is that the stats stay inside the range of what was
    // actually pushed.
    void concurrentPushesStayInRange()
    {
        PerfMonitor& pm = PerfMonitor::instance();
        for (int i = 0; i < kWindow; ++i) {
            pm.recordAudioFillMs(20.0);
        }
        (void)pm.snapshotAndClearDeltas();

        constexpr int kPerThread = 500;
        std::thread a([&pm]() {
            for (int i = 0; i < kPerThread; ++i) { pm.recordAudioFillMs(20.0); }
        });
        std::thread b([&pm]() {
            for (int i = 0; i < kPerThread; ++i) { pm.recordAudioFillMs(30.0); }
        });
        a.join();
        b.join();

        const PerfMonitor::Snapshot s = pm.snapshotAndClearDeltas();
        QVERIFY(s.audioFillSamples > 0);
        QVERIFY(s.audioFillSamples <= kWindow);
        QVERIFY(s.audioFillMinMs >= 20.0);
        QVERIFY(s.audioFillMaxMs <= 30.0);
        QVERIFY(s.audioFillAvgMs >= 20.0 && s.audioFillAvgMs <= 30.0);
    }

    // TX I/Q counters are the other half of the perf overlay this PR
    // adds; totals accumulate, deltas reset on each snapshot read.
    void txIqCountersAccumulateAndDeltasReset()
    {
        PerfMonitor& pm = PerfMonitor::instance();
        (void)pm.snapshotAndClearDeltas();   // clear whatever preceded us

        pm.incTxIqUnderrun(7);
        pm.incTxIqProduced(240);
        const PerfMonitor::Snapshot first = pm.snapshotAndClearDeltas();
        QCOMPARE(first.txIqUnderrunsDelta, static_cast<uint64_t>(7));
        QCOMPARE(first.txIqProducedDelta, static_cast<uint64_t>(240));

        const PerfMonitor::Snapshot second = pm.snapshotAndClearDeltas();
        QCOMPARE(second.txIqUnderrunsDelta, static_cast<uint64_t>(0));
        QCOMPARE(second.txIqProducedDelta, static_cast<uint64_t>(0));
        // Totals keep climbing across snapshots.
        QVERIFY(second.txIqUnderrunsTotal >= first.txIqUnderrunsTotal);
        QCOMPARE(second.txIqUnderrunsTotal, first.txIqUnderrunsTotal);
    }

    // Non-positive sample counts are ignored rather than corrupting the
    // counters (P2RadioConnection guards its call sites, but the API
    // documents the contract).
    void txIqUnderrunIgnoresNonPositive()
    {
        PerfMonitor& pm = PerfMonitor::instance();
        (void)pm.snapshotAndClearDeltas();

        pm.incTxIqUnderrun(0);
        pm.incTxIqUnderrun(-5);

        const PerfMonitor::Snapshot s = pm.snapshotAndClearDeltas();
        QCOMPARE(s.txIqUnderrunsDelta, static_cast<uint64_t>(0));
    }
};

QTEST_MAIN(TstPerfMonitorRing)
#include "tst_perf_monitor_ring.moc"
