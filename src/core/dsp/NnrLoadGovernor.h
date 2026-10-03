// =================================================================
// src/core/dsp/NnrLoadGovernor.h  (NereusSDR)
// =================================================================
// Decides when a receiver that cannot keep up with neural noise reduction
// (NNR) steps back: Premium to Standard, Standard to off. The decision is a
// runtime limit; it never changes the operator's saved choice, and it never
// raises a level by itself (the operator clears it).
//
// no-port-check: NereusSDR-original. Runtime policy for the R3 DSP overload
// work (R-R3-40); no Thetis counterpart.
//
// Plan: docs/architecture/2026-09-23-r3-dsp-overload-plan.md, Task 7.
//
// Modification history (NereusSDR):
//   2026-09-23 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//                 Later the same day: the load it judges is busy time over
//                 wall time (R-R3-40); thresholds and timings unchanged.
// =================================================================

#pragma once

#include "core/dsp/NnrSettings.h"

#include <QHash>
#include <QList>
#include <QtGlobal>

#include <optional>

namespace NereusSDR {

class NnrLoadGovernor {
public:
    // The governor judges every receiver this often (the load sampler's
    // interval, ReceiverDspLoadSampler::kSampleIntervalMs).
    static constexpr int kNnrCheckIntervalMs = 500;
    // A receiver running NNR steps back one level when its DSP load (the
    // share of wall time its worker spent inside blocks,
    // ReceiverDspLoad::load), averaged over kNnrStepDownHoldMs, is at least
    // this. 1.0 is real time; 0.90 leaves the worker 10% headroom.
    static constexpr double kNnrStepDownLoad = 0.90;
    static constexpr qint64 kNnrStepDownHoldMs = 2000;
    // After a step the receiver is not judged again for this long.
    static constexpr qint64 kNnrStepSettleMs = 5000;

    // One receiver as the governor sees it at a check.
    struct Receiver {
        // NNR is the operator's active noise reduction on this receiver and
        // is running there (a receiver with NNR selected but not running is
        // overloaded by something else).
        bool nnrSelected{false};
        // The operator's saved model (0 Standard, 1 Premium).
        int savedModelSlot{0};
        // The limit in force now.
        NnrLimit limit{NnrLimit::None};
        // The DSP load over the latest interval. nullopt when it was not
        // measured: no snapshot, or the receiver read as idle. Idle is not
        // proof the receiver is unloaded (its worker may have waited for the
        // DSP lock), so it neither counts as load nor as relief.
        std::optional<double> load;
    };

    // The limit one step below `current` for this saved model, or nullopt
    // when there is nothing lower (NNR already off).
    static std::optional<NnrLimit> nextLimit(int savedModelSlot, NnrLimit current);

    // Takes one check of one receiver at nowMs (a monotonic clock). Returns
    // the new limit when the receiver must step back now, otherwise nullopt.
    std::optional<NnrLimit> observe(int sliceId, qint64 nowMs, const Receiver& receiver);

    // The limit was cleared (the operator tried again, chose a model or
    // turned NNR off): forget the history, including any settle time.
    void reset(int sliceId);
    // The receiver is gone.
    void forget(int sliceId) { m_state.remove(sliceId); }

private:
    struct Sample {
        qint64 startMs{0};
        qint64 endMs{0};
        double load{0.0};
    };
    struct State {
        std::optional<qint64> lastCheckMs;
        qint64 settleUntilMs{0};
        QList<Sample> samples;
    };
    QHash<int, State> m_state;
};

} // namespace NereusSDR
