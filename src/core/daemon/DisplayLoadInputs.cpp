// =================================================================
// src/core/daemon/DisplayLoadInputs.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See DisplayLoadInputs.h.
// =================================================================

#include "core/daemon/DisplayLoadInputs.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

bool receiverSharesDisplayCores(const PlacementPlan& placement, int sliceId)
{
    return !placement.active || placement.cpuFor(ThreadRole::RxWorker, sliceId) < 0
        || placement.cpuFor(ThreadRole::DspThread) < 0;
}

DisplayLoadReading displayLoadReadingFrom(const DisplayLoadInputs& inputs, qint64 nowMs,
                                          const DisplayBudgetCharge& acceptedCharge)
{
    DisplayLoadReading reading;
    reading.nowMs = nowMs;
    reading.acceptedCharge = acceptedCharge;
    for (const DisplayLoadInputs::Receiver& receiver : inputs.receivers) {
        // Idle is not proof of no load, so it is not a measurement either.
        if (receiver.load.idle || !std::isfinite(receiver.load.load)
            || !receiverSharesDisplayCores(inputs.placement, receiver.sliceId)) {
            continue;
        }
        reading.highestReceiverLoad = std::max(reading.highestReceiverLoad.value_or(0.0),
                                               receiver.load.load);
        reading.lateBlocks += std::max<qint64>(0, receiver.load.lateBlocks);
        reading.highestInputDelayMs = std::max(reading.highestInputDelayMs,
                                               receiver.load.inputDelayMs);
    }
    reading.systemCpuPercent = inputs.placement.active ? inputs.housekeepingCpuPercent
                                                       : inputs.systemCpuPercent;
    if (reading.systemCpuPercent && inputs.cpuSampleBeganMsAgo) {
        reading.systemCpuSampleStartMs = nowMs - std::max<qint64>(0, *inputs.cpuSampleBeganMsAgo);
    }
    return reading;
}

} // namespace NereusSDR
