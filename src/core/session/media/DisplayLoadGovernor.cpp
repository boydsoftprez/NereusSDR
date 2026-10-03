// =================================================================
// src/core/session/media/DisplayLoadGovernor.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See DisplayLoadGovernor.h.
//
// Modification history (NereusSDR):
//   2026-09-23 - The calm test no longer requires zero late blocks. By J.J.
//                 Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code (R-R3-37, R-R3-40).
//   2026-09-25 - floorPanCharge, the floor pan alone, for the display
//                 budget split (several-devices fix wave 2). J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code (R-IOS-31, R-MC-17).
//   2026-09-25 - floorCharge(pans): one floor pan per device sharing the
//                 budget; a cut in force is raised to it (several-devices
//                 fix wave 3). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code (R-IOS-31,
//                 R-MC-17).
//
// =================================================================

#include "core/session/media/DisplayLoadGovernor.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace NereusSDR {
namespace {

quint64 scaled(quint64 value, double scale)
{
    return static_cast<quint64>(std::floor(static_cast<double>(value) * scale));
}

} // namespace

DisplayBudgetCharge DisplayLoadGovernor::floorPanCharge()
{
    const auto pan = spectrumDisplayCost(kFloorPixels, kFloorFps, true);
    return pan ? pan->charge : DisplayBudgetCharge{};
}

DisplayBudgetCharge DisplayLoadGovernor::floorCharge(int pans)
{
    QList<DisplayBudgetCharge> charges{ps3DisplayCharge()};
    const DisplayBudgetCharge pan = floorPanCharge();
    for (int i = 0; i < std::max(1, pans); ++i) {
        charges.append(pan);
    }
    return sumDisplayCharges(charges).value_or(DisplayBudgetCharge{});
}

DisplayBudgetLimits DisplayLoadGovernor::floored(const DisplayBudgetLimits& limits) const
{
    const DisplayBudgetCharge minimum = floorCharge(m_floorPans);
    return DisplayBudgetLimits{
        std::max(limits.applicationBytesPerSecond,
                 std::min(minimum.applicationBytesPerSecond, m_ceiling.applicationBytesPerSecond)),
        std::max(limits.spectrumSampleUnitsPerSecond,
                 std::min(minimum.spectrumSampleUnitsPerSecond,
                          m_ceiling.spectrumSampleUnitsPerSecond)),
        limits.generation};
}

DisplayBudgetLimits DisplayLoadGovernor::computedCeiling()
{
    const auto pan = spectrumDisplayCost(DisplayCodecEncoder::kMaxSamplesPerPlane,
                                         static_cast<int>(kMaximumSpectrumDisplayFramesPerSecond),
                                         true);
    QList<DisplayBudgetCharge> charges{ps3DisplayCharge()};
    for (int i = 0; i < kCeilingPans; ++i) {
        charges.append(pan ? pan->charge : DisplayBudgetCharge{});
    }
    const DisplayBudgetCharge total = sumDisplayCharges(charges).value_or(DisplayBudgetCharge{});
    return DisplayBudgetLimits{total.applicationBytesPerSecond,
                               total.spectrumSampleUnitsPerSecond, 1};
}

std::optional<double> DisplayLoadGovernor::pressure(const DisplayLoadReading& reading)
{
    std::optional<double> result;
    if (reading.highestReceiverLoad && std::isfinite(*reading.highestReceiverLoad)) {
        result = *reading.highestReceiverLoad / kBusyReceiverLoad;
    }
    if (reading.systemCpuPercent && std::isfinite(*reading.systemCpuPercent)) {
        result = std::max(result.value_or(0.0), *reading.systemCpuPercent / kBusySystemCpuPercent);
    }
    return result;
}

DisplayLoadGovernor::DisplayLoadGovernor(DisplayBudgetLimits ceiling)
    : m_ceiling(ceiling)
{
    m_state.limits = ceiling;
}

quint32 DisplayLoadGovernor::nextGeneration(const DisplayBudgetLimits& limits)
{
    const quint32 next = limits.generation + 1;
    return next == 0 ? 1 : next;
}

bool DisplayLoadGovernor::isBusy(const DisplayLoadReading& reading)
{
    return (reading.highestReceiverLoad && std::isfinite(*reading.highestReceiverLoad)
            && *reading.highestReceiverLoad >= kBusyReceiverLoad)
        || (reading.systemCpuPercent && std::isfinite(*reading.systemCpuPercent)
            && *reading.systemCpuPercent >= kBusySystemCpuPercent);
}

std::optional<DisplayLoadReading> DisplayLoadGovernor::judgeable(const Settle& settle,
                                                                 const DisplayLoadReading& reading)
{
    const bool hasCpu = reading.systemCpuPercent.has_value()
        && std::isfinite(*reading.systemCpuPercent);
    const auto cpuBeganBy = [&reading](qint64 pointMs) {
        return reading.systemCpuSampleStartMs && *reading.systemCpuSampleStartMs >= pointMs;
    };
    if (settle.acknowledgedMs && reading.nowMs < settle.untilMs) {
        // Acknowledged: one load interval, and one full host sample begun
        // after the acknowledgement, measure the lowered display.
        if (reading.nowMs < *settle.acknowledgedMs + kLoadIntervalMs
            || (hasCpu && !cpuBeganBy(*settle.acknowledgedMs))) {
            return std::nullopt;
        }
        return reading;
    }
    if (reading.nowMs < settle.untilMs) {
        return std::nullopt;
    }
    // The cap: judged now, but never on a host sample that began before
    // the step.
    DisplayLoadReading judged = reading;
    if (hasCpu && !cpuBeganBy(settle.stepMs)) {
        judged.systemCpuPercent.reset();
        judged.systemCpuSampleStartMs.reset();
    }
    return judged;
}

std::optional<DisplayLoadDecision> DisplayLoadGovernor::update(const DisplayLoadReading& reading)
{
    m_proposal.reset();
    m_floorPans = std::max(1, reading.floorPans);
    const qint64 nowMs = reading.nowMs;
    State next = m_state;
    // The Core has no direct acknowledgement of a step: the app has acted
    // on it once what it has accepted fits the lowered limits.
    if (next.settle && !next.settle->acknowledgedMs
        && displayChargeFits(next.limits, reading.acceptedCharge)) {
        next.settle->acknowledgedMs = nowMs;
    }
    // Fix wave 3 (ruling 9.3): a cut in force never leaves less than one
    // useful pan for each device sharing the budget; when devices join,
    // the limits rise to the new floor at once, the step kept.
    if (!next.previous.isEmpty()) {
        const DisplayBudgetLimits raised = floored(next.limits);
        if (raised.applicationBytesPerSecond != next.limits.applicationBytesPerSecond
            || raised.spectrumSampleUnitsPerSecond != next.limits.spectrumSampleUnitsPerSecond) {
            next.limits = DisplayBudgetLimits{raised.applicationBytesPerSecond,
                                              raised.spectrumSampleUnitsPerSecond,
                                              nextGeneration(next.limits)};
            return propose(std::move(next));
        }
    }
    const std::optional<double> pressureNow = pressure(reading);
    if (!pressureNow) {
        // No evidence of load: never a reason to cut, and a gap does not
        // count toward the busy hold.
        m_busySinceMs.reset();
        if (!m_gapSinceMs || nowMs < *m_gapSinceMs) {
            m_gapSinceMs = nowMs;
            m_calmSinceMs.reset();
        }
        if (nowMs - *m_gapSinceMs < kCalmHoldMs) {
            m_state = std::move(next);
            return std::nullopt;
        }
        // A sustained gap counts as calm for restoring only, so a cut
        // cannot outlive the measurements that justified it.
        if (!m_calmSinceMs) {
            m_calmSinceMs = *m_gapSinceMs;
        }
        // A step that settled with nothing to judge it by is simply over.
        if (next.settle && nowMs >= next.settle->untilMs) {
            next.settle.reset();
        }
        return calmReading(std::move(next), nowMs, *m_calmSinceMs);
    }
    m_gapSinceMs.reset();

    const bool hasLoad = reading.highestReceiverLoad.has_value()
        && std::isfinite(*reading.highestReceiverLoad);
    const bool hasCpu = reading.systemCpuPercent.has_value()
        && std::isfinite(*reading.systemCpuPercent);
    const bool busy = isBusy(reading);
    // Late blocks are not in the test: frame-based noise reduction makes one
    // long block in several as a normal part of its work, and a receiver
    // really falling behind shows in its input wait.
    const bool calm = (!hasLoad || *reading.highestReceiverLoad < kCalmReceiverLoad)
        && (!hasCpu || *reading.systemCpuPercent < kCalmSystemCpuPercent)
        && reading.highestInputDelayMs < kCalmInputDelayMs;

    if (next.settle) {
        const std::optional<DisplayLoadReading> judged = judgeable(*next.settle, reading);
        const std::optional<double> judgedPressure = judged ? pressure(*judged)
                                                            : std::optional<double>{};
        if (!judgedPressure) {
            // The app is still acting on the last step, or its effect is not
            // measured yet: judge nothing.
            m_busySinceMs.reset();
            m_calmSinceMs.reset();
            m_state = std::move(next);
            return std::nullopt;
        }
        const Settle settled = *next.settle;
        next.settle.reset();
        m_busySinceMs.reset();
        m_calmSinceMs.reset();
        if (settled.pressureAtStep - *judgedPressure < kReliefMargin
            && !next.previous.isEmpty()) {
            // The step saved nothing: give the quality back and cut no more
            // until the load changes clearly.
            next.heldPressure = *judgedPressure;
            return restoreStep(std::move(next));
        }
        if (isBusy(*judged)) {
            m_busySinceMs = nowMs;
            return stepDown(std::move(next), reading, *judgedPressure);
        }
    }

    if (next.heldPressure) {
        if (calm) {
            next.heldPressure.reset();
        } else if (*pressureNow >= *next.heldPressure + kClearRiseMargin) {
            // A clearly heavier load: cutting may help again, after a full
            // busy hold of its own.
            next.heldPressure.reset();
            m_busySinceMs.reset();
        }
    }

    if (busy) {
        m_calmSinceMs.reset();
        if (next.heldPressure) {
            m_busySinceMs.reset();
            m_state = std::move(next);
            return std::nullopt;
        }
        if (!m_busySinceMs || nowMs < *m_busySinceMs) {
            m_busySinceMs = nowMs;
        }
        if (nowMs - *m_busySinceMs < kBusyHoldMs) {
            m_state = std::move(next);
            return std::nullopt;
        }
        m_busySinceMs = nowMs;
        return stepDown(std::move(next), reading, *pressureNow);
    }
    m_busySinceMs.reset();

    if (!calm) {
        m_calmSinceMs.reset();
        m_state = std::move(next);
        return std::nullopt;
    }
    if (!m_calmSinceMs || nowMs < *m_calmSinceMs) {
        m_calmSinceMs = nowMs;
    }
    return calmReading(std::move(next), nowMs, *m_calmSinceMs);
}

std::optional<DisplayLoadDecision> DisplayLoadGovernor::calmReading(State next, qint64 nowMs,
                                                                    qint64 calmStartMs)
{
    if (next.previous.isEmpty() || next.settle || nowMs - calmStartMs < kCalmHoldMs) {
        if (next.previous.isEmpty()) {
            m_calmSinceMs.reset();
        }
        m_state = std::move(next);
        return std::nullopt;
    }
    m_calmSinceMs = nowMs;
    return restoreStep(std::move(next));
}

std::optional<DisplayLoadDecision> DisplayLoadGovernor::propose(State next)
{
    const DisplayLoadDecision decision{next.limits, reasonFor(next)};
    m_proposal = Proposal{decision, std::move(next)};
    return decision;
}

void DisplayLoadGovernor::accept(const DisplayLoadDecision& decision)
{
    if (!m_proposal || m_proposal->decision.limits != decision.limits
        || m_proposal->decision.reason != decision.reason) {
        return;
    }
    m_state = std::move(m_proposal->next);
    m_proposal.reset();
}

void DisplayLoadGovernor::syncGeneration(quint32 generation)
{
    m_proposal.reset();
    const quint32 delta = generation - m_state.limits.generation;
    if (delta != 0 && delta < 0x80000000u) {
        m_state.limits.generation = generation;
    }
}

std::optional<DisplayLoadDecision> DisplayLoadGovernor::stepDown(
    State next, const DisplayLoadReading& reading, double pressureNow)
{
    const DisplayBudgetCharge& accepted = reading.acceptedCharge;
    const DisplayBudgetLimits current = next.limits;
    const DisplayBudgetCharge minimum = floorCharge(m_floorPans);
    const quint64 floorBytes = std::min(minimum.applicationBytesPerSecond,
                                        m_ceiling.applicationBytesPerSecond);
    const quint64 floorSamples = std::min(minimum.spectrumSampleUnitsPerSecond,
                                          m_ceiling.spectrumSampleUnitsPerSecond);
    const quint64 baseBytes = std::min(current.applicationBytesPerSecond,
                                       accepted.applicationBytesPerSecond);
    const quint64 baseSamples = std::min(current.spectrumSampleUnitsPerSecond,
                                         accepted.spectrumSampleUnitsPerSecond);
    const quint64 bytes = std::min(current.applicationBytesPerSecond,
                                   std::max(floorBytes, scaled(baseBytes, kStepDownScale)));
    const quint64 samples = std::min(current.spectrumSampleUnitsPerSecond,
                                     std::max(floorSamples, scaled(baseSamples, kStepDownScale)));
    const DisplayBudgetLimits lowered{bytes, samples, nextGeneration(current)};
    // Nothing above the floor is being sent (lowering the budget would save
    // nothing now and only cap pans that open later), the limits are
    // already at the floor, or the step count is spent.
    const bool nothingToCut = accepted.applicationBytesPerSecond <= floorBytes
        && accepted.spectrumSampleUnitsPerSecond <= floorSamples;
    const bool atFloor = bytes == current.applicationBytesPerSecond
        && samples == current.spectrumSampleUnitsPerSecond;
    if (nothingToCut || atFloor || next.previous.size() >= kMaximumSteps || !lowered.isValid()) {
        m_state = std::move(next);
        return std::nullopt;
    }
    next.previous.append(current);
    next.limits = lowered;
    next.settle = Settle{reading.nowMs, reading.nowMs + kSettleMs, pressureNow, std::nullopt};
    return propose(std::move(next));
}

std::optional<DisplayLoadDecision> DisplayLoadGovernor::restoreStep(State next)
{
    if (next.previous.isEmpty()) {
        m_state = std::move(next);
        return std::nullopt;
    }
    const DisplayBudgetLimits previous = floored(next.previous.takeLast());
    next.limits = DisplayBudgetLimits{previous.applicationBytesPerSecond,
                                      previous.spectrumSampleUnitsPerSecond,
                                      nextGeneration(next.limits)};
    return propose(std::move(next));
}

std::optional<DisplayLoadDecision> DisplayLoadGovernor::reset()
{
    m_proposal.reset();
    m_busySinceMs.reset();
    m_calmSinceMs.reset();
    m_gapSinceMs.reset();
    m_state.settle.reset();
    m_state.heldPressure.reset();
    if (m_state.previous.isEmpty()) {
        return std::nullopt;
    }
    m_state.previous.clear();
    m_state.limits = DisplayBudgetLimits{m_ceiling.applicationBytesPerSecond,
                                         m_ceiling.spectrumSampleUnitsPerSecond,
                                         nextGeneration(m_state.limits)};
    return DisplayLoadDecision{m_state.limits, reason()};
}

} // namespace NereusSDR
