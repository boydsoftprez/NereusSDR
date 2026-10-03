// =================================================================
// src/core/dsp/NnrLoadGovernor.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Runtime policy for the R3 DSP overload
// work (R-R3-40); no Thetis counterpart. See NnrLoadGovernor.h.
//
// Modification history (NereusSDR):
//   2026-09-23 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "core/dsp/NnrLoadGovernor.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

std::optional<NnrLimit> NnrLoadGovernor::nextLimit(int savedModelSlot, NnrLimit current)
{
    switch (current) {
    case NnrLimit::None:
        return savedModelSlot == 1 ? NnrLimit::StandardOnly : NnrLimit::Off;
    case NnrLimit::StandardOnly:
        return NnrLimit::Off;
    case NnrLimit::Off:
        break;
    }
    return std::nullopt;
}

std::optional<NnrLimit> NnrLoadGovernor::observe(int sliceId, qint64 nowMs,
                                                  const Receiver& receiver)
{
    State& state = m_state[sliceId];
    const std::optional<NnrLimit> next = nextLimit(receiver.savedModelSlot, receiver.limit);
    // Only a receiver running NNR can step back, and never below off. Its
    // history starts again when NNR runs again.
    if (!receiver.nnrSelected || !next) {
        state.lastCheckMs.reset();
        state.samples.clear();
        return std::nullopt;
    }
    // The first check only sets the time base: the load it carries was
    // measured over an interval that may predate this receiver running NNR.
    if (!state.lastCheckMs) {
        state.lastCheckMs = nowMs;
        return std::nullopt;
    }
    const qint64 startMs = *state.lastCheckMs;
    state.lastCheckMs = nowMs;
    if (nowMs <= startMs) {
        return std::nullopt;
    }
    // Each measured check covers the time since the previous check. A check
    // that was not measured covers nothing, so the average needs a full
    // kNnrStepDownHoldMs of measured load after it.
    if (receiver.load && std::isfinite(*receiver.load) && *receiver.load >= 0.0) {
        state.samples.append({startMs, nowMs, *receiver.load});
    }
    const qint64 windowStartMs = nowMs - kNnrStepDownHoldMs;
    while (!state.samples.isEmpty() && state.samples.first().endMs <= windowStartMs) {
        state.samples.removeFirst();
    }
    if (nowMs < state.settleUntilMs) {
        return std::nullopt;
    }

    qint64 coveredMs = 0;
    double weighted = 0.0;
    for (const Sample& sample : std::as_const(state.samples)) {
        const qint64 overlap = std::min(sample.endMs, nowMs)
            - std::max(sample.startMs, windowStartMs);
        if (overlap > 0) {
            coveredMs += overlap;
            weighted += sample.load * static_cast<double>(overlap);
        }
    }
    if (coveredMs < kNnrStepDownHoldMs
        || weighted / static_cast<double>(coveredMs) < kNnrStepDownLoad) {
        return std::nullopt;
    }
    state.samples.clear();
    state.settleUntilMs = nowMs + kNnrStepSettleMs;
    return next;
}

void NnrLoadGovernor::reset(int sliceId)
{
    m_state.remove(sliceId);
}

} // namespace NereusSDR
