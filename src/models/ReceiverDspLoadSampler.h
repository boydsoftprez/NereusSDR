// =================================================================
// src/models/ReceiverDspLoadSampler.h  (NereusSDR)
// =================================================================
// Turns each receiver's cumulative WDSP load counters into one snapshot per
// sampling interval, so every reader (network telemetry, the noise-reduction
// step-back) sees the same numbers and no reader starts or splits an
// interval. RadioModel owns one sampler and feeds it every
// kSampleIntervalMs; RadioModel::receiverDspLoad returns the cached result.
//
// no-port-check: NereusSDR-original. Measurement bookkeeping for the R3 DSP
// overload work; no Thetis counterpart.
//
// Plan: docs/architecture/2026-09-23-r3-dsp-overload-plan.md, Task 5 and the
// fix wave after it. Requirement R-R3-40.
//
// Modification history (NereusSDR):
//   2026-09-23 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//                 Later the same day: the first reading for a slice only
//                 seeds its baseline, and forget() drops a removed slice.
//                 Later the same day: the load is busy time over wall time
//                 between two reads (readNs), so a long block in progress
//                 counts only for the time it has run (R-R3-40, R-R3-37;
//                 docs/architecture/2026-09-23-r3-receiver-load-hotfix-plan.md).
//                 Later the same day: Reading::consistent; a read that may be
//                 torn keeps the slice's baseline and snapshot (R-R3-40).
// =================================================================

#pragma once

#include <QHash>
#include <QtGlobal>

#include <optional>

namespace NereusSDR {

// One receiver's DSP load over the latest sampling interval.
struct ReceiverDspLoad {
    // The share of wall time the WDSP worker spent inside blocks over the
    // interval: the change in busyNs + currentBlockNs over the change in
    // readNs between two readings. A block still running counts only for
    // the time it has run, so a worker stuck in one long block reads about
    // 1.0 (never unloaded, never more), and frame-based work that makes one
    // long block in several reads its real share, not that block's length
    // over one period. 1.0 = the worker never left its blocks: it cannot
    // keep up. Uniform blocks read mean block time / block period, as
    // before.
    double load{0.0};
    // True when the worker finished no block in the interval and is not
    // inside one. load is 0.0 then.
    //
    // Idle is not proof of no load. The worker publishes a block's start
    // only once it holds the DSP lock, so a worker waiting that whole
    // interval for the lock (the control thread holding it) reads exactly
    // like a receiver with no input. A reader deciding whether processing
    // is too heavy must treat idle as "not measured", never as "unloaded".
    bool idle{false};
    // Worker blocks longer than their block period, finished in the
    // interval. A diagnostic only: frame-based noise reduction makes late
    // blocks as a normal part of its work, so they are not overload.
    qint64 lateBlocks{0};
    // Longest block finished in the interval, or the block still in progress
    // if it has already run longer.
    qint64 maxBlockUs{0};
    // Longest single block on this receiver's WDSP channel id since the
    // process started.
    qint64 lifetimeMaxBlockUs{0};
    // How long this receiver's latest I/Q batch waited before RxDspWorker
    // processed it.
    qint64 inputDelayMs{0};
    // Input RxDspWorker skipped to keep that wait bounded, since the worker
    // was created (cumulative).
    qint64 droppedInputMs{0};
};

class ReceiverDspLoadSampler {
public:
    // RadioModel samples every receiver this often (main thread).
    static constexpr int kSampleIntervalMs = 500;

    // One receiver's raw readings at a sample.
    struct Reading {
        // Cumulative WDSP counters (RxChannel::dspLoad).
        qint64 blocks{0};
        qint64 busyNs{0};
        qint64 lateBlocks{0};
        qint64 lifetimeMaxBlockUs{0};
        int    blockPeriodUs{0};
        // The block in progress so far; 0 between blocks.
        qint64 currentBlockNs{0};
        // When the counters were read (RxChannel::DspLoadCounters::readNs).
        qint64 readNs{0};
        // Longest block finished since the previous sample
        // (RxChannel::takeDspIntervalMaxBlockUs).
        qint64 intervalMaxBlockUs{0};
        // RxDspWorker::inputDelayStats for the receiver's input.
        qint64 inputDelayMs{0};
        qint64 droppedInputMs{0};
        // False for a read whose busy pair may be torn
        // (RxChannel::DspLoadCounters::consistent): the slice keeps its
        // previous baseline and snapshot, and the next read measures from
        // that baseline.
        bool consistent{true};
    };

    // Replaces every snapshot with one computed from these readings, keyed
    // by slice ID. A slice absent from `readings` loses its snapshot and its
    // baseline. The first reading for a slice only seeds its baseline and
    // publishes no snapshot: the WDSP counters are cumulative for a channel
    // id over the whole process (dsplock.c never resets them), so measuring
    // from zero would report every earlier user of that id as this
    // interval's load.
    void update(const QHash<int, Reading>& readings);

    // The latest snapshot for this slice, or nullopt when it had no reading
    // at the latest update or only its first one. Reading never changes
    // anything.
    std::optional<ReceiverDspLoad> snapshot(int sliceId) const;

    // Drops this slice's snapshot and baseline at once, so a slice created
    // later with the same ID starts from a fresh baseline instead of
    // inheriting the removed one's numbers.
    void forget(int sliceId);

    void clear();

private:
    struct Baseline {
        qint64 blocks{0};
        qint64 busyNs{0};
        qint64 lateBlocks{0};
        // busyNs + currentBlockNs at readNs: the worker's time inside
        // blocks up to that read.
        qint64 busyToReadNs{0};
        qint64 readNs{0};
    };

    static ReceiverDspLoad compute(const Reading& now, const Baseline& previous);

    QHash<int, Baseline> m_baselines;
    QHash<int, ReceiverDspLoad> m_snapshots;
};

} // namespace NereusSDR
