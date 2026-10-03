// no-port-check: NereusSDR-original unit test for the per-receiver DSP load
// sampler (R-R3-40, R-R3-37): one periodic sample feeds every reader, the
// load is busy time over wall time between two reads, a worker stuck inside
// a block reads as fully busy and no more, a long block in progress counts
// only for the time it has run, an idle receiver reads as idle, and the
// longest block is reported per interval. The first reading for a slice only
// seeds its baseline, because the WDSP counters are cumulative for a channel
// id over the whole process.
#include <QtTest>

#include "models/ReceiverDspLoadSampler.h"

using namespace NereusSDR;

namespace {

constexpr int kSlice = 3;
// 4096 samples at 48 kHz.
constexpr int kPeriodUs = 85333;
constexpr qint64 kPeriodNs = qint64{kPeriodUs} * 1000;

// A worker that keeps up with its input finishes one block per block
// period, so by default a reading is taken `blocks` periods after the
// counters started: a uniform load then reads mean block / block period.
ReceiverDspLoadSampler::Reading reading(qint64 blocks, qint64 busyNs,
                                        qint64 lateBlocks = 0)
{
    ReceiverDspLoadSampler::Reading r;
    r.blocks = blocks;
    r.busyNs = busyNs;
    r.lateBlocks = lateBlocks;
    r.blockPeriodUs = kPeriodUs;
    r.readNs = blocks * kPeriodNs;
    return r;
}

QHash<int, ReceiverDspLoadSampler::Reading> one(const ReceiverDspLoadSampler::Reading& r)
{
    QHash<int, ReceiverDspLoadSampler::Reading> readings;
    readings.insert(kSlice, r);
    return readings;
}

} // namespace

class TestReceiverDspLoadSampler : public QObject {
    Q_OBJECT

private slots:
    // R-R3-40 fix wave: dsplock.c gives up after 64 attempts at reading
    // busyNs and the block in progress at rest, and that last pair may be
    // torn (a block counted twice or not at all). Such a read measures
    // nothing: the slice keeps its load, and the next read measures from
    // the last good baseline.
    void aReadThatMayBeTornKeepsTheSlicesLoad()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(0, 0)));
        sampler.update(one(reading(10, 10 * 40'000'000LL)));
        const auto before = sampler.snapshot(kSlice);
        QVERIFY(before);

        // Torn: the block just finished is in busyNs and in the block in
        // progress too.
        ReceiverDspLoadSampler::Reading torn = reading(20, 20 * 40'000'000LL);
        torn.currentBlockNs = 40'000'000LL;
        torn.consistent = false;
        sampler.update(one(torn));
        const auto kept = sampler.snapshot(kSlice);
        QVERIFY(kept);
        QCOMPARE(kept->load, before->load);

        // The next good read covers both intervals from the kept baseline.
        sampler.update(one(reading(30, 10 * 40'000'000LL + 20 * 20'000'000LL)));
        const auto next = sampler.snapshot(kSlice);
        QVERIFY(next);
        QVERIFY(qAbs(next->load - 20000.0 / kPeriodUs) < 1e-9);
    }

    void severalReadersSeeTheSameSnapshot()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(0, 0)));
        sampler.update(one(reading(10, 10 * 40'000'000LL)));

        // Telemetry and the step-back governor both read between samples;
        // neither may start a new interval for the other.
        const auto telemetry = sampler.snapshot(kSlice);
        const auto governor = sampler.snapshot(kSlice);
        const auto again = sampler.snapshot(kSlice);
        QVERIFY(telemetry && governor && again);
        QCOMPARE(governor->load, telemetry->load);
        QCOMPARE(again->load, telemetry->load);
        QCOMPARE(again->lateBlocks, telemetry->lateBlocks);
        QVERIFY(qAbs(telemetry->load - 40000.0 / kPeriodUs) < 1e-9);
        QVERIFY(!telemetry->idle);

        // The next sample covers only the blocks since the previous one.
        sampler.update(one(reading(20, 10 * 40'000'000LL + 10 * 80'000'000LL)));
        const auto next = sampler.snapshot(kSlice);
        QVERIFY(next);
        QVERIFY(qAbs(next->load - 80000.0 / kPeriodUs) < 1e-9);
    }

    void aWorkerStuckInsideABlockReadsAsFullyBusy()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(0, 0)));
        const ReceiverDspLoadSampler::Reading before = reading(10, 10 * 20'000'000LL);
        sampler.update(one(before));

        // No block finished in the next 500 ms; the worker has been inside
        // its current block for 450 ms of them, over five block periods.
        ReceiverDspLoadSampler::Reading stuck = before;
        stuck.currentBlockNs = 450'000'000LL;
        stuck.readNs = before.readNs + 500'000'000LL;
        sampler.update(one(stuck));
        const auto load = sampler.snapshot(kSlice);
        QVERIFY(load);
        QVERIFY(!load->idle);
        QVERIFY2(load->load >= 0.9 - 1e-9,
                 "a stuck block must read as busy for the time it has run");
        QVERIFY2(load->load <= 1.0 + 1e-9,
                 "a stuck block reads about 1.0, never more");
        QCOMPARE(load->maxBlockUs, 450000LL);

        // Still inside it 500 ms later: the whole interval was busy.
        ReceiverDspLoadSampler::Reading still = stuck;
        still.currentBlockNs = 950'000'000LL;
        still.readNs = stuck.readNs + 500'000'000LL;
        sampler.update(one(still));
        const auto after = sampler.snapshot(kSlice);
        QVERIFY(after);
        QVERIFY(qAbs(after->load - 1.0) < 1e-9);
        QCOMPARE(after->maxBlockUs, 950000LL);
    }

    // The defect found on the Rock 5C: frame-based noise reduction makes one
    // long block in twelve, and a sample that landed inside one read its
    // time so far over one block period. A block in progress counts only for
    // the time it has run inside the interval.
    void aLongBlockInProgressCountsOnlyItsTimeSoFar()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(0, 0)));

        // Over 500 ms: two quick blocks finished, then one has run 300 ms so
        // far. 320 ms busy of 500 ms, not 300 ms over one block period.
        ReceiverDspLoadSampler::Reading r = reading(2, 2 * 10'000'000LL);
        r.currentBlockNs = 300'000'000LL;
        r.readNs = 500'000'000LL;
        sampler.update(one(r));
        const auto load = sampler.snapshot(kSlice);
        QVERIFY(load);
        QVERIFY(!load->idle);
        QVERIFY(qAbs(load->load - 0.64) < 1e-9);
        QCOMPARE(load->maxBlockUs, 300000LL);

        // The next 500 ms: that block ends at 400 ms (100 ms more), then two
        // more quick blocks. Only the 100 ms not yet counted is new.
        ReceiverDspLoadSampler::Reading next = reading(5, 2 * 10'000'000LL + 400'000'000LL
                                                              + 2 * 10'000'000LL);
        next.readNs = 1'000'000'000LL;
        sampler.update(one(next));
        const auto steady = sampler.snapshot(kSlice);
        QVERIFY(steady);
        QVERIFY(qAbs(steady->load - 0.24) < 1e-9);
    }

    void anIdleReceiverReadsAsIdle()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(10, 10 * 20'000'000LL)));
        sampler.update(one(reading(10, 10 * 20'000'000LL)));
        const auto load = sampler.snapshot(kSlice);
        QVERIFY(load);
        QVERIFY(load->idle);
        QCOMPARE(load->load, 0.0);
        QCOMPARE(load->lateBlocks, 0LL);
    }

    void theLongestBlockIsPerInterval()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(0, 0)));
        ReceiverDspLoadSampler::Reading first = reading(5, 5 * 100'000'000LL, 5);
        first.intervalMaxBlockUs = 120000;
        first.lifetimeMaxBlockUs = 120000;
        sampler.update(one(first));
        QCOMPARE(sampler.snapshot(kSlice)->maxBlockUs, 120000LL);
        QCOMPARE(sampler.snapshot(kSlice)->lateBlocks, 5LL);

        ReceiverDspLoadSampler::Reading second = reading(10, 5 * 100'000'000LL + 5 * 20'000'000LL, 5);
        second.intervalMaxBlockUs = 21000;
        second.lifetimeMaxBlockUs = 120000;
        sampler.update(one(second));
        const auto load = sampler.snapshot(kSlice);
        QVERIFY(load);
        QCOMPARE(load->maxBlockUs, 21000LL);
        QCOMPARE(load->lifetimeMaxBlockUs, 120000LL);
        QCOMPARE(load->lateBlocks, 0LL);
    }

    void aReceiverWithoutAReadingHasNoSnapshot()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(0, 0)));
        sampler.update(one(reading(1, 1'000'000)));
        QVERIFY(sampler.snapshot(kSlice));
        QVERIFY(!sampler.snapshot(kSlice + 1));
        sampler.update({});
        QVERIFY(!sampler.snapshot(kSlice));
    }

    // Review finding: a WDSP channel id keeps its counters for the whole
    // process, so a new slice on a reused id starts with its predecessor's
    // history. The first reading must publish nothing rather than report
    // that history as one interval's load.
    void theFirstReadingOnlySeedsTheBaseline()
    {
        ReceiverDspLoadSampler sampler;
        // An earlier slice on this channel id ran 1000 blocks, 900 of them
        // late, at twice the block period.
        ReceiverDspLoadSampler::Reading reused =
            reading(1000, 1000 * 2LL * kPeriodUs * 1000LL, 900);
        reused.intervalMaxBlockUs = 2 * kPeriodUs;
        sampler.update(one(reused));
        QVERIFY(!sampler.snapshot(kSlice));

        // The next reading covers only this slice's own blocks.
        ReceiverDspLoadSampler::Reading next = reading(
            1004, 1000 * 2LL * kPeriodUs * 1000LL + 4 * 40'000'000LL, 900);
        next.intervalMaxBlockUs = 40000;
        sampler.update(one(next));
        const auto load = sampler.snapshot(kSlice);
        QVERIFY(load);
        QVERIFY(qAbs(load->load - 40000.0 / kPeriodUs) < 1e-9);
        QCOMPARE(load->lateBlocks, 0LL);
        QCOMPARE(load->maxBlockUs, 40000LL);
    }

    // A forgotten slice (removed from the radio) starts again from a new
    // baseline: a slice created later with the same ID never inherits the
    // removed one's snapshot.
    void aForgottenSliceStartsOver()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(0, 0)));
        sampler.update(one(reading(10, 10 * 80'000'000LL)));
        QVERIFY(sampler.snapshot(kSlice));

        sampler.forget(kSlice);
        QVERIFY(!sampler.snapshot(kSlice));
        sampler.update(one(reading(12, 10 * 80'000'000LL + 2 * 10'000'000LL)));
        QVERIFY(!sampler.snapshot(kSlice));
        sampler.update(one(reading(14, 10 * 80'000'000LL + 4 * 10'000'000LL)));
        const auto load = sampler.snapshot(kSlice);
        QVERIFY(load);
        QVERIFY(qAbs(load->load - 10000.0 / kPeriodUs) < 1e-9);
    }

    // dsplock.c never resets a channel id's counters, so they do not go
    // backwards in production. If one ever did, the interval is unknown:
    // the sampler publishes nothing and measures from that reading on.
    void countersThatGoBackwardsStartOver()
    {
        ReceiverDspLoadSampler sampler;
        sampler.update(one(reading(0, 0)));
        sampler.update(one(reading(100, 100 * 20'000'000LL)));
        QVERIFY(sampler.snapshot(kSlice));
        sampler.update(one(reading(4, 4 * 40'000'000LL)));
        QVERIFY(!sampler.snapshot(kSlice));
        sampler.update(one(reading(8, 8 * 40'000'000LL)));
        const auto load = sampler.snapshot(kSlice);
        QVERIFY(load);
        QVERIFY(qAbs(load->load - 40000.0 / kPeriodUs) < 1e-9);
    }
};

QTEST_GUILESS_MAIN(TestReceiverDspLoadSampler)
#include "tst_receiver_dsp_load_sampler.moc"
