// =================================================================
// src/core/session/media/DisplayLoadGovernor.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. When the Core computer runs short of
// processing time, lower the remote display budget before receive
// processing has to give way (R-R3-08, R-R3-37, R-R3-40). No upstream logic
// is involved; the thresholds are first estimates to revisit with
// measurements on the Core hardware.
//
// Modification history (NereusSDR):
//   2026-09-23 - Late blocks no longer keep a reading from counting as
//                 calm: frame-based noise reduction makes them as a normal
//                 part of its work. By J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code (R-R3-37,
//                 R-R3-40).
//   2026-09-25 - The floor keeps one useful pan for each device sharing
//                 the display budget, and a cut in force is raised to it
//                 when devices join (several-devices fix wave 3). J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code (R-IOS-31, R-MC-17).
//
// =================================================================

#pragma once

#include "core/session/media/DisplayBudget.h"

#include <QList>

#include <optional>

namespace NereusSDR {

/// One observation of the Core's load, gathered by the caller from caches
/// that were already sampled (RadioModel::receiverDspLoad and the shared
/// host sampler). Nothing here is read under a DSP lock.
struct DisplayLoadReading {
    qint64 nowMs = 0;
    /// Highest load among receivers that processed input in their latest
    /// interval (the share of wall time the worker spent inside blocks,
    /// ReceiverDspLoad::load). Idle receivers are left out: idle is not
    /// proof of no load. nullopt: no receiver measured.
    std::optional<double> highestReceiverLoad;
    /// Late blocks summed over the measured receivers' latest intervals.
    /// A diagnostic only: frame-based noise reduction makes late blocks as
    /// a normal part of its work, so they neither make a reading busy nor
    /// keep it from being calm.
    qint64 lateBlocks = 0;
    /// Longest input wait among the measured receivers' latest batches.
    qint64 highestInputDelayMs = 0;
    /// The Core computer's CPU use since the previous host sample, 0..100.
    /// nullopt where the host cannot be measured (macOS, Windows).
    std::optional<double> systemCpuPercent;
    /// When the host sample behind systemCpuPercent began, on nowMs's
    /// clock. A settling step is never judged on a sample that began before
    /// the point it waits for; without a start the sample counts as begun
    /// too early.
    std::optional<qint64> systemCpuSampleStartMs;
    /// Display traffic the Core has accepted now: every spectrum endpoint
    /// plus the PureSignal display when it is subscribed.
    DisplayBudgetCharge acceptedCharge;
    /// Fix wave 3 after the several-devices re-review (ruling 9.3, the
    /// governor's floor): how many admitted network devices share the
    /// display budget (StationServer::displayBudgetSharingCount). The floor
    /// keeps one useful pan for each, so a cut never pauses every device's
    /// display. Fewer than one counts as one.
    int floorPans = 1;
};

struct DisplayLoadDecision {
    DisplayBudgetLimits limits;
    DisplayBudgetReason reason = DisplayBudgetReason::None;
};

/// Turns load readings into display-budget limits with hysteresis.
///
/// Busy (highest receiver load >= kBusyReceiverLoad or system CPU >=
/// kBusySystemCpuPercent) held for kBusyHoldMs steps the limits down to
/// kStepDownScale of what is accepted now, never below
/// floorCharge(reading.floorPans): PureSignal plus one floor pan per device
/// sharing the budget. While a step is in force and more devices join, the
/// limits are raised to that floor at once (the step count kept). Calm
/// (every measurement under the calm thresholds, input wait under
/// kCalmInputDelayMs; late blocks do not count) held for kCalmHoldMs undoes
/// one step. Readings
/// in between hold. Every change carries the next limits generation; the
/// reason is CoreBusy while any step is in force.
///
/// Settling: after each step down nothing more is cut until the step has
/// taken effect and been measured. The Core has no direct acknowledgement,
/// so the step counts as acknowledged at the first reading whose accepted
/// charge fits the lowered limits in every field (displayChargeFits). The
/// step is then judged at the later of kLoadIntervalMs after that and the
/// end of one full host sample begun after it (a reading without CPU has
/// no host sample to wait for). Without an acknowledgement it is judged at
/// kSettleMs after the step, and never on a host sample that began before
/// the step: such a CPU value is left out of the judgement. The step is
/// judged against the reading that caused it:
///  - relief of at least kReliefMargin, still busy: step again at once;
///  - relief of at least kReliefMargin, no longer busy: keep the step;
///  - less relief: the step saved nothing, so it is undone (the reason
///    clears when no other step remains) and the governor holds, cutting
///    nothing more, until the load falls to calm or rises kClearRiseMargin
///    above where it stood when the step was undone.
/// Loads are compared as a fraction of their busy threshold (pressure()),
/// so a receiver load and a CPU percentage share one scale.
///
/// No measurement at all (macOS, no receivers, every receiver idle) holds
/// and restarts both holds. Once such a gap has lasted kCalmHoldMs,
/// absence counts as calm for restoring only: a cut never outlives the
/// measurements that justified it, and absence never causes one.
///
/// Two-phase: update() only proposes a decision. The caller publishes it
/// and calls accept() once the publication is accepted; a proposal that is
/// not accepted changes nothing, and syncGeneration() lets the next one
/// follow whatever generation is published.
///
/// The busy threshold sits below the NNR step-back's 0.90 (held 2 s), so
/// spectrum yields first.
class DisplayLoadGovernor {
public:
    static constexpr double kBusyReceiverLoad = 0.75;
    static constexpr double kBusySystemCpuPercent = 85.0;
    static constexpr qint64 kBusyHoldMs = 2'000;
    static constexpr double kCalmReceiverLoad = 0.60;
    static constexpr double kCalmSystemCpuPercent = 70.0;
    static constexpr qint64 kCalmInputDelayMs = 100;
    static constexpr qint64 kCalmHoldMs = 10'000;
    static constexpr double kStepDownScale = 0.5;
    static constexpr int kMaximumSteps = 32;
    /// The Core's load readings refresh this often
    /// (ReceiverDspLoadSampler::kSampleIntervalMs; DaemonApp checks they
    /// agree).
    static constexpr qint64 kLoadIntervalMs = 500;
    /// Hardware-pending tuning value: the longest a step down waits to be
    /// judged when the app never acknowledges it, the app's allocation
    /// acknowledgement timeout plus one load interval. An acknowledged step
    /// is judged sooner (see Settling above).
    static constexpr qint64 kSettleMs = kDisplayAllocationAckTimeoutMs + kLoadIntervalMs;
    /// Hardware-pending tuning value: the least drop in pressure, as a
    /// fraction of the busy threshold, that counts as relief from a step.
    static constexpr double kReliefMargin = 0.05;
    /// Hardware-pending tuning value: how far pressure must rise above its
    /// level at an undone step before the governor cuts again.
    static constexpr double kClearRiseMargin = 0.10;
    /// The floor keeps one active pan useful: the app's own reduction floors
    /// (RemoteDisplayAllocator.cpp kUsefulPixels, kUsefulFps).
    static constexpr int kFloorPixels = 256;
    static constexpr int kFloorFps = 10;
    /// The app plans at most eight remote pans (RemoteDisplayAllocator.cpp
    /// kMaximumPans).
    static constexpr int kCeilingPans = 8;

    /// One pan at kFloorPixels and kFloorFps with its wide plane: one
    /// useful pan. Fix wave 2 after the several-devices re-review
    /// (Critical 1, ruling 9.3): the least any admitted network device is
    /// counted as asking for when the display budget is split.
    static DisplayBudgetCharge floorPanCharge();
    /// PureSignal's display plus `pans` floorPanCharge()s (at least one):
    /// one useful pan for each device sharing the budget (fix wave 3).
    static DisplayBudgetCharge floorCharge(int pans = 1);
    /// Eight pans at the codec's largest plane, highest frame rate and a
    /// wide plane, plus PureSignal's display, at generation 1. What the Core
    /// advertises when adaptation is on and no limits are configured, so
    /// apps plan in budget mode from the start.
    static DisplayBudgetLimits computedCeiling();
    /// The reading's load as a fraction of its busy threshold: the larger of
    /// receiver load over kBusyReceiverLoad and CPU over
    /// kBusySystemCpuPercent. nullopt without a measurement.
    static std::optional<double> pressure(const DisplayLoadReading& reading);

    explicit DisplayLoadGovernor(DisplayBudgetLimits ceiling);

    /// A proposed decision when the limits should change, otherwise
    /// nullopt. Nothing it proposes is in force until accept().
    std::optional<DisplayLoadDecision> update(const DisplayLoadReading& reading);
    /// The proposal from the latest update() was published: put it in
    /// force. A decision that is not that proposal is ignored.
    void accept(const DisplayLoadDecision& decision);
    /// Publication refused because `generation` is already published:
    /// the next proposal follows it.
    void syncGeneration(quint32 generation);
    /// Back to the ceiling (the session ended), in force at once. A
    /// decision only when a step was in force.
    std::optional<DisplayLoadDecision> reset();

    DisplayBudgetLimits ceiling() const { return m_ceiling; }
    DisplayBudgetLimits limits() const { return m_state.limits; }
    DisplayBudgetReason reason() const { return reasonFor(m_state); }
    int steps() const { return static_cast<int>(m_state.previous.size()); }
    /// A step is waiting to be judged.
    bool settling() const { return m_state.settle.has_value(); }
    /// An undone step is holding further cuts.
    bool holding() const { return m_state.heldPressure.has_value(); }

private:
    struct Settle {
        qint64 stepMs = 0;
        /// The cap: stepMs + kSettleMs.
        qint64 untilMs = 0;
        double pressureAtStep = 0.0;
        /// The first reading whose accepted charge fit the lowered limits.
        std::optional<qint64> acknowledgedMs;
    };
    /// Everything a published decision changes.
    struct State {
        DisplayBudgetLimits limits;
        QList<DisplayBudgetLimits> previous;
        std::optional<Settle> settle;
        std::optional<double> heldPressure;
    };
    struct Proposal {
        DisplayLoadDecision decision;
        State next;
    };

    static DisplayBudgetReason reasonFor(const State& state)
    {
        return state.previous.isEmpty() ? DisplayBudgetReason::None
                                        : DisplayBudgetReason::CoreBusy;
    }
    std::optional<DisplayLoadDecision> propose(State next);
    std::optional<DisplayLoadDecision> stepDown(State next, const DisplayLoadReading& reading,
                                                double pressureNow);
    std::optional<DisplayLoadDecision> restoreStep(State next);
    std::optional<DisplayLoadDecision> calmReading(State next, qint64 nowMs, qint64 calmStartMs);
    static quint32 nextGeneration(const DisplayBudgetLimits& limits);
    /// `limits` raised, field by field, to floorCharge(m_floorPans) capped
    /// at the ceiling; the generation is left as it is.
    DisplayBudgetLimits floored(const DisplayBudgetLimits& limits) const;
    /// The reading a settling step may be judged on now, or nullopt while
    /// it must wait. Leaves out a CPU value from a host sample that began
    /// before the point the judgement waits for.
    static std::optional<DisplayLoadReading> judgeable(const Settle& settle,
                                                       const DisplayLoadReading& reading);
    static bool isBusy(const DisplayLoadReading& reading);

    DisplayBudgetLimits m_ceiling;
    State m_state;
    std::optional<Proposal> m_proposal;
    std::optional<qint64> m_busySinceMs;
    std::optional<qint64> m_calmSinceMs;
    std::optional<qint64> m_gapSinceMs;
    /// The latest reading's floorPans (at least 1).
    int m_floorPans = 1;
};

} // namespace NereusSDR
