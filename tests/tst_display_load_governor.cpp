// =================================================================
// tests/tst_display_load_governor.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. R-R3-08, R-R3-37, R-R3-40: the Core
// lowers the display budget when its computer is busy, never below the
// floor, and restores it one step at a time. Pure: an injected clock and
// injected readings, no radio, no WDSP, no host files.
// =================================================================

#include <QtTest/QtTest>

#include <algorithm>
#include <functional>

#include "core/daemon/DisplayLoadInputs.h"
#include "core/session/media/DisplayBudgetSplit.h"
#include "core/session/media/DisplayLoadGovernor.h"

using namespace NereusSDR;

namespace {

DisplayBudgetCharge panCharge(int pixels, int fps, bool wide = false)
{
    return spectrumDisplayCost(pixels, fps, wide)->charge;
}

// Four pans at 1024 px and 30 fps: what the app has accepted.
DisplayBudgetCharge fourPans()
{
    const DisplayBudgetCharge pan = panCharge(1024, 30);
    return *sumDisplayCharges({pan, pan, pan, pan});
}

DisplayLoadReading loadReading(qint64 nowMs, double load,
                               DisplayBudgetCharge accepted = fourPans())
{
    DisplayLoadReading reading;
    reading.nowMs = nowMs;
    reading.highestReceiverLoad = load;
    reading.acceptedCharge = accepted;
    return reading;
}

DisplayLoadReading cpuReading(qint64 nowMs, double cpu,
                              DisplayBudgetCharge accepted = fourPans())
{
    DisplayLoadReading reading;
    reading.nowMs = nowMs;
    reading.systemCpuPercent = cpu;
    reading.acceptedCharge = accepted;
    return reading;
}

// One step down from `base`: half of it, never below the floor. The floor
// reserves PureSignal's display bytes, which are larger than four ordinary
// pans' bytes, so for these pans a step lowers spectrum samples while bytes
// stay at the floor.
DisplayBudgetLimits stepFrom(const DisplayBudgetCharge& base, quint32 generation)
{
    const DisplayBudgetCharge floor = DisplayLoadGovernor::floorCharge();
    return {std::max(floor.applicationBytesPerSecond, base.applicationBytesPerSecond / 2),
            std::max(floor.spectrumSampleUnitsPerSecond, base.spectrumSampleUnitsPerSecond / 2),
            generation};
}

// Proposes and, when there is a decision, accepts it (a publish the
// StationServer accepted).
std::optional<DisplayLoadDecision> step(DisplayLoadGovernor& governor,
                                        const DisplayLoadReading& reading)
{
    const auto decision = governor.update(reading);
    if (decision) {
        governor.accept(*decision);
    }
    return decision;
}

// A load that each step relieves: `start` with no step in force, less
// `perStep` for every step. The app's accepted charge follows the limits.
std::function<DisplayLoadReading(qint64)> responsiveLoad(const DisplayLoadGovernor& governor,
                                                          double start, double perStep)
{
    return [&governor, start, perStep](qint64 t) {
        const DisplayBudgetLimits limits = governor.limits();
        const DisplayBudgetCharge four = fourPans();
        const DisplayBudgetCharge accepted{
            std::min(limits.applicationBytesPerSecond, four.applicationBytesPerSecond),
            std::min(limits.spectrumSampleUnitsPerSecond, four.spectrumSampleUnitsPerSecond),
            four.messagesPerSecond};
        return loadReading(t, start - perStep * governor.steps(), accepted);
    };
}

// Feeds one reading every 500 ms (the load sampler's period) from `fromMs`
// to `toMs` inclusive; returns every decision made.
QList<DisplayLoadDecision> feed(DisplayLoadGovernor& governor, qint64 fromMs, qint64 toMs,
                                const std::function<DisplayLoadReading(qint64)>& make)
{
    QList<DisplayLoadDecision> decisions;
    for (qint64 t = fromMs; t <= toMs; t += 500) {
        if (const auto decision = governor.update(make(t))) {
            governor.accept(*decision); // Published and accepted.
            decisions.append(*decision);
        }
    }
    return decisions;
}

} // namespace

class TstDisplayLoadGovernor : public QObject {
    Q_OBJECT

private slots:
    void thresholdsSitBelowTheNoiseReductionStepBack()
    {
        // The NNR step-back acts at a receiver load of 0.90 held for 2 s;
        // spectrum must yield first.
        QVERIFY(DisplayLoadGovernor::kBusyReceiverLoad < 0.90);
        QVERIFY(DisplayLoadGovernor::kBusyHoldMs <= 2'000);
        QVERIFY(DisplayLoadGovernor::kCalmReceiverLoad < DisplayLoadGovernor::kBusyReceiverLoad);
        QVERIFY(DisplayLoadGovernor::kCalmSystemCpuPercent
                < DisplayLoadGovernor::kBusySystemCpuPercent);
    }

    void ceilingAndFloorAreTheDocumentedCharges()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        QVERIFY(ceiling.isValid());
        QCOMPARE(ceiling.generation, quint32{1});
        const DisplayBudgetCharge widest = panCharge(DisplayCodecEncoder::kMaxSamplesPerPlane,
                                                     60, true);
        QCOMPARE(ceiling.applicationBytesPerSecond,
                 8 * widest.applicationBytesPerSecond
                     + ps3DisplayCharge().applicationBytesPerSecond);
        QCOMPARE(ceiling.spectrumSampleUnitsPerSecond, 8 * widest.spectrumSampleUnitsPerSecond);

        const DisplayBudgetCharge floor = DisplayLoadGovernor::floorCharge();
        const DisplayBudgetCharge onePan = panCharge(256, 10, true);
        QCOMPARE(floor.applicationBytesPerSecond,
                 onePan.applicationBytesPerSecond + ps3DisplayCharge().applicationBytesPerSecond);
        QCOMPARE(floor.spectrumSampleUnitsPerSecond, onePan.spectrumSampleUnitsPerSecond);
        QVERIFY(displayChargeFits(ceiling, floor));
    }

    void noMeasurementChangesNothing()
    {
        DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
        // macOS with no receivers, or every receiver idle: nothing measured.
        const auto decisions = feed(governor, 0, 60'000, [](qint64 t) {
            DisplayLoadReading reading;
            reading.nowMs = t;
            reading.acceptedCharge = fourPans();
            return reading;
        });
        QVERIFY(decisions.isEmpty());
        QCOMPARE(governor.steps(), 0);
        QCOMPARE(governor.reason(), DisplayBudgetReason::None);

        // A gap without measurements restarts the busy hold.
        QVERIFY(feed(governor, 100'000, 101'500,
                     [](qint64 t) { return loadReading(t, 0.9); }).isEmpty());
        DisplayLoadReading gap;
        gap.nowMs = 102'000;
        gap.acceptedCharge = fourPans();
        QVERIFY(!step(governor, gap));
        QVERIFY(feed(governor, 102'500, 104'000,
                     [](qint64 t) { return loadReading(t, 0.9); }).isEmpty());
        QCOMPARE(governor.steps(), 0);
    }

    void busyForTwoSecondsStepsDownTheAcceptedCharge()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        QVERIFY(feed(governor, 0, 1'500,
                     [](qint64 t) { return loadReading(t, 0.75); }).isEmpty());
        const auto decision = step(governor, loadReading(2'000, 0.75));
        QVERIFY(decision.has_value());
        QCOMPARE(decision->reason, DisplayBudgetReason::CoreBusy);
        QCOMPARE(decision->limits.generation, quint32{2});
        const DisplayBudgetCharge accepted = fourPans();
        const DisplayBudgetLimits firstStep = stepFrom(accepted, 2);
        QCOMPARE(decision->limits, firstStep);
        QVERIFY(decision->limits.spectrumSampleUnitsPerSecond
                < accepted.spectrumSampleUnitsPerSecond);
        QCOMPARE(governor.limits(), decision->limits);
        QCOMPARE(governor.steps(), 1);

        QVERIFY(governor.settling());

        // Nothing more is cut until the step has settled, however busy.
        QVERIFY(feed(governor, 2'500, 2'000 + DisplayLoadGovernor::kSettleMs - 500,
                     [](qint64 t) { return loadReading(t, 0.95); }).isEmpty());
        // The step relieved the load (0.75 to 0.70 is 0.067 of the busy
        // threshold) and it is no longer busy: the step stays, nothing more.
        const auto second = step(governor,
                                 loadReading(2'000 + DisplayLoadGovernor::kSettleMs, 0.70));
        QVERIFY(!second.has_value());
        QCOMPARE(governor.steps(), 1);
        QVERIFY(!governor.settling());
    }

    void settleIsTheAllocationTimeoutPlusOneLoadInterval()
    {
        QCOMPARE(DisplayLoadGovernor::kSettleMs,
                 qint64{kDisplayAllocationAckTimeoutMs} + DisplayLoadGovernor::kLoadIntervalMs);
        QCOMPARE(DisplayLoadGovernor::kSettleMs, qint64{10'500});
        QVERIFY(DisplayLoadGovernor::kReliefMargin > 0.0);
        QVERIFY(DisplayLoadGovernor::kClearRiseMargin > DisplayLoadGovernor::kReliefMargin);
    }

    void aResponsiveLoadIsSteppedOncePerSettle()
    {
        DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
        // Each step takes 0.06 off the receiver load (0.08 of the busy
        // threshold): relief every time, busy until the third step.
        const auto decisions = feed(governor, 0, 60'000, responsiveLoad(governor, 0.92, 0.06));
        QCOMPARE(decisions.size(), 3);
        QList<qint64> times;
        // Replay to find when each decision fell: the first after the busy
        // hold, each later one exactly one settle after the one before.
        DisplayLoadGovernor replay(DisplayLoadGovernor::computedCeiling());
        const auto make = responsiveLoad(replay, 0.92, 0.06);
        for (qint64 t = 0; t <= 60'000; t += 500) {
            if (step(replay, make(t))) {
                times.append(t);
            }
        }
        QCOMPARE(times.size(), 3);
        QCOMPARE(times.at(0), DisplayLoadGovernor::kBusyHoldMs);
        // The app acts on each step at once, so each settles one reading
        // later (the acknowledgement) plus one load interval.
        const qint64 settled = 500 + DisplayLoadGovernor::kLoadIntervalMs;
        QCOMPARE(times.at(1) - times.at(0), settled);
        QCOMPARE(times.at(2) - times.at(1), settled);
        for (int i = 0; i < decisions.size(); ++i) {
            QCOMPARE(decisions.at(i).reason, DisplayBudgetReason::CoreBusy);
            QCOMPARE(decisions.at(i).limits.generation, quint32(2 + i));
        }
        QCOMPARE(governor.steps(), 3);
        // 0.92 - 3 x 0.06 = 0.74: no longer busy, not calm. Holds there.
        QVERIFY(feed(governor, 60'500, 180'000,
                     responsiveLoad(governor, 0.92, 0.06)).isEmpty());
        QCOMPARE(governor.steps(), 3);
    }

    // A step is acknowledged once the accepted charge fits the lowered
    // limits; it is then judged one load interval later, long before the
    // cap.
    void aQuickAcknowledgementJudgesTheStepEarly()
    {
        DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
        QCOMPARE(feed(governor, 0, 2'000,
                      [](qint64 t) { return loadReading(t, 0.9); }).size(), 1);
        const DisplayBudgetLimits lowered = governor.limits();
        const DisplayBudgetCharge four = fourPans();
        const DisplayBudgetCharge fitted{
            std::min(lowered.applicationBytesPerSecond, four.applicationBytesPerSecond),
            std::min(lowered.spectrumSampleUnitsPerSecond, four.spectrumSampleUnitsPerSecond),
            four.messagesPerSecond};
        QVERIFY(displayChargeFits(lowered, fitted));
        QVERIFY(!displayChargeFits(lowered, four));
        // Not yet acted on: nothing judged, however busy.
        QVERIFY(!step(governor, loadReading(2'500, 0.95)));
        QVERIFY(governor.settling());
        // Acknowledged at 3 s; one load interval measures the result.
        QVERIFY(!step(governor, loadReading(3'000, 0.85, fitted)));
        QVERIFY(governor.settling());
        // Relieved (0.9 to 0.85) and still busy: the next step at 3.5 s.
        const auto next = step(governor, loadReading(3'500, 0.85, fitted));
        QVERIFY(next.has_value());
        QCOMPARE(next->reason, DisplayBudgetReason::CoreBusy);
        QCOMPARE(governor.steps(), 2);
        QVERIFY(3'500 < 2'000 + DisplayLoadGovernor::kSettleMs);
    }

    // The app never acts on the step: it is judged at the cap, not before.
    void aMissingAcknowledgementWaitsForTheCap()
    {
        DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
        QCOMPARE(feed(governor, 0, 2'000,
                      [](qint64 t) { return loadReading(t, 0.9); }).size(), 1);
        // fourPans() stays above the lowered limits the whole time.
        QVERIFY(feed(governor, 2'500, 2'000 + DisplayLoadGovernor::kSettleMs - 500,
                     [](qint64 t) { return loadReading(t, 0.85); }).isEmpty());
        QVERIFY(governor.settling());
        const auto next = step(governor,
                               loadReading(2'000 + DisplayLoadGovernor::kSettleMs, 0.85));
        QVERIFY(next.has_value());
        QCOMPARE(governor.steps(), 2);
    }

    // A host sample that began before the step (or its acknowledgement)
    // measured the display before the step; the judgement never uses it.
    void aJudgementNeverUsesAHostSampleBegunBeforeTheReduction()
    {
        // Host samples every 900 ms: a reading at t uses the last sample
        // finished by t, which began 900 ms before that.
        const auto startOfSample = [](qint64 t) { return (t / 900) * 900 - 900; };
        DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
        const auto busy = [&](qint64 t) {
            DisplayLoadReading reading = cpuReading(t, 95.0);
            reading.systemCpuSampleStartMs = startOfSample(t);
            return reading;
        };
        QCOMPARE(feed(governor, 0, 2'000, busy).size(), 1);
        const DisplayBudgetLimits lowered = governor.limits();
        const DisplayBudgetCharge four = fourPans();
        const DisplayBudgetCharge fitted{
            std::min(lowered.applicationBytesPerSecond, four.applicationBytesPerSecond),
            std::min(lowered.spectrumSampleUnitsPerSecond, four.spectrumSampleUnitsPerSecond),
            four.messagesPerSecond};
        // Acknowledged at 2.5 s. A sample begun before then still reads
        // 95 % (no relief): judged on it, the step would be undone.
        const auto sample = [&](qint64 t) {
            DisplayLoadReading reading = cpuReading(t, startOfSample(t) >= 2'500 ? 80.0 : 95.0,
                                                    fitted);
            reading.systemCpuSampleStartMs = startOfSample(t);
            return reading;
        };
        QVERIFY(feed(governor, 2'500, 3'500, sample).isEmpty());
        QVERIFY(governor.settling()); // 3.5 s: the latest sample began at 1.8 s.
        // 4 s: the sample that began at 2.7 s reads 80 %, relieved and not
        // busy, so the step stands.
        QVERIFY(!step(governor, sample(4'000)));
        QVERIFY(!governor.settling());
        QCOMPARE(governor.steps(), 1);

        // At the cap without an acknowledgement, a CPU value from a sample
        // begun before the step is left out; the receiver load judges.
        DisplayLoadGovernor capped(DisplayLoadGovernor::computedCeiling());
        const auto both = [](qint64 t, double load, double cpu, qint64 startMs) {
            DisplayLoadReading reading = loadReading(t, load);
            reading.systemCpuPercent = cpu;
            reading.systemCpuSampleStartMs = startMs;
            return reading;
        };
        QCOMPARE(feed(capped, 0, 2'000,
                      [&](qint64 t) { return both(t, 0.9, 95.0, t - 900); }).size(), 1);
        QVERIFY(feed(capped, 2'500, 2'000 + DisplayLoadGovernor::kSettleMs - 500,
                     [&](qint64 t) { return both(t, 0.7, 99.0, 1'000); }).isEmpty());
        // Judged on 99 % the step would bring no relief and be undone; on
        // the receiver load (0.9 to 0.7) it relieved and stands.
        QVERIFY(!step(capped, both(2'000 + DisplayLoadGovernor::kSettleMs, 0.7, 99.0, 1'000)));
        QVERIFY(!capped.settling());
        QCOMPARE(capped.steps(), 1);
    }

    void aStepThatBringsNoReliefIsUndoneAndCutsStop()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        // The display is not what loads this receiver: a step changes
        // nothing. One step, then its undo, then nothing for five minutes.
        const auto decisions = feed(governor, 0, 300'000,
                                    [](qint64 t) { return loadReading(t, 0.9); });
        QCOMPARE(decisions.size(), 2);
        QCOMPARE(decisions.at(0).reason, DisplayBudgetReason::CoreBusy);
        QCOMPARE(decisions.at(0).limits.generation, quint32{2});
        QCOMPARE(decisions.at(1).reason, DisplayBudgetReason::None);
        QCOMPARE(decisions.at(1).limits.applicationBytesPerSecond,
                 ceiling.applicationBytesPerSecond);
        QCOMPARE(decisions.at(1).limits.spectrumSampleUnitsPerSecond,
                 ceiling.spectrumSampleUnitsPerSecond);
        QCOMPARE(decisions.at(1).limits.generation, quint32{3});
        QCOMPARE(governor.steps(), 0);
        QCOMPARE(governor.reason(), DisplayBudgetReason::None);
        QVERIFY(governor.holding());

        // A small rise is not a clear change: still no cut.
        QVERIFY(feed(governor, 300'500, 320'000,
                     [](qint64 t) { return loadReading(t, 0.9 + 0.05); }).isEmpty());
        // A clear rise (more than kClearRiseMargin of the threshold) lets
        // the governor try again, after a full busy hold.
        const double clearRise = 0.9 + DisplayLoadGovernor::kClearRiseMargin
            * DisplayLoadGovernor::kBusyReceiverLoad + 0.01;
        QVERIFY(feed(governor, 320'500, 322'000,
                     [clearRise](qint64 t) { return loadReading(t, clearRise); }).isEmpty());
        const auto again = step(governor, loadReading(322'500, clearRise));
        QVERIFY(again.has_value());
        QCOMPARE(again->reason, DisplayBudgetReason::CoreBusy);
        QVERIFY(!governor.holding());
    }

    void aHoldEndsWhenTheLoadFallsToCalm()
    {
        DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
        QCOMPARE(feed(governor, 0, 12'500,
                      [](qint64 t) { return loadReading(t, 0.9); }).size(), 2);
        QVERIFY(governor.holding());
        QVERIFY(!step(governor, loadReading(13'000, 0.5)));
        QVERIFY(!governor.holding());
        // Busy again: an ordinary busy hold, then a step.
        QVERIFY(feed(governor, 13'500, 15'000,
                     [](qint64 t) { return loadReading(t, 0.9); }).isEmpty());
        QVERIFY(step(governor, loadReading(15'500, 0.9)).has_value());
    }

    void anUnacceptedProposalChangesNothing()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        for (qint64 t = 0; t < 2'000; t += 500) {
            QVERIFY(!governor.update(loadReading(t, 0.9)));
        }
        const auto proposed = governor.update(loadReading(2'000, 0.9));
        QVERIFY(proposed.has_value());
        QCOMPARE(proposed->limits.generation, quint32{2});
        // Not accepted (the publish was refused): nothing is in force.
        QCOMPARE(governor.limits(), ceiling);
        QCOMPARE(governor.steps(), 0);
        QVERIFY(!governor.settling());
        // A later publish at generation 6 was accepted elsewhere: the next
        // proposal follows it.
        governor.syncGeneration(6);
        const auto retried = governor.update(loadReading(4'000, 0.9));
        QVERIFY(retried.has_value());
        QCOMPARE(retried->limits.generation, quint32{7});
        // Accepting a decision that is not the latest proposal does nothing.
        governor.accept(*proposed);
        QCOMPARE(governor.steps(), 0);
        governor.accept(*retried);
        QCOMPARE(governor.steps(), 1);
        QCOMPARE(governor.limits(), retried->limits);
        // An older generation never moves the governor backwards.
        governor.syncGeneration(3);
        QCOMPARE(governor.limits().generation, quint32{7});
    }

    void aSustainedGapRestoresButNeverCuts()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        QCOMPARE(feed(governor, 0, 2'000,
                      [](qint64 t) { return loadReading(t, 0.9); }).size(), 1);
        const auto absent = [](qint64 t) {
            DisplayLoadReading reading;
            reading.nowMs = t;
            reading.acceptedCharge = fourPans();
            return reading;
        };
        // Measurements stop (every receiver idle). Shorter than the calm
        // hold: the cut stays.
        QVERIFY(feed(governor, 2'500, 12'000, absent).isEmpty());
        QCOMPARE(governor.steps(), 1);
        // From kCalmHoldMs of gap on, absence counts as calm for restoring.
        const auto restored = feed(governor, 12'500, 12'500, absent);
        QCOMPARE(restored.size(), 1);
        QCOMPARE(restored.first().reason, DisplayBudgetReason::None);
        QCOMPARE(restored.first().limits.spectrumSampleUnitsPerSecond,
                 ceiling.spectrumSampleUnitsPerSecond);
        // And never as busy: nothing more for ten minutes.
        QVERIFY(feed(governor, 13'000, 600'000, absent).isEmpty());
        QCOMPARE(governor.limits().spectrumSampleUnitsPerSecond,
                 ceiling.spectrumSampleUnitsPerSecond);
    }

    void systemCpuAloneStepsDown()
    {
        DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
        QVERIFY(feed(governor, 0, 10'000,
                     [](qint64 t) { return cpuReading(t, 84.9); }).isEmpty());
        const auto decisions = feed(governor, 20'000, 22'000,
                                    [](qint64 t) { return cpuReading(t, 85.0); });
        QCOMPARE(decisions.size(), 1);
        QCOMPARE(decisions.first().reason, DisplayBudgetReason::CoreBusy);
    }

    void betweenTheThresholdsHolds()
    {
        DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
        QCOMPARE(feed(governor, 0, 2'000,
                      [](qint64 t) { return loadReading(t, 0.8); }).size(), 1);
        const DisplayBudgetLimits lowered = governor.limits();
        // 0.70 is neither busy nor calm: nothing moves, however long.
        QVERIFY(feed(governor, 2'500, 60'000,
                     [](qint64 t) { return loadReading(t, 0.70); }).isEmpty());
        QCOMPARE(governor.limits(), lowered);
        QCOMPARE(governor.reason(), DisplayBudgetReason::CoreBusy);
    }

    void neverBelowTheFloor()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        // Each step relieves a little but the Core stays busy.
        const auto decisions = feed(governor, 0, 600'000, [&governor](qint64 t) {
            return loadReading(t, 2.0 - 0.04 * governor.steps());
        });
        QVERIFY(decisions.size() > 1);
        const DisplayBudgetCharge floor = DisplayLoadGovernor::floorCharge();
        QCOMPARE(governor.limits().applicationBytesPerSecond, floor.applicationBytesPerSecond);
        QCOMPARE(governor.limits().spectrumSampleUnitsPerSecond,
                 floor.spectrumSampleUnitsPerSecond);
        for (const DisplayLoadDecision& decision : decisions) {
            QVERIFY(decision.limits.applicationBytesPerSecond >= floor.applicationBytesPerSecond);
            QVERIFY(decision.limits.spectrumSampleUnitsPerSecond
                    >= floor.spectrumSampleUnitsPerSecond);
        }
        // At the floor nothing more is published.
        QVERIFY(feed(governor, 600'500, 700'000, [&governor](qint64 t) {
            return loadReading(t, 2.0 - 0.04 * governor.steps());
        }).isEmpty());

        // Nothing above the floor being sent: nothing to lower.
        DisplayLoadGovernor quiet(ceiling);
        QVERIFY(feed(quiet, 0, 20'000, [&floor](qint64 t) {
            return loadReading(t, 0.95, floor);
        }).isEmpty());
        QCOMPARE(quiet.limits(), ceiling);
    }

    // Fix wave 3 (the re-review's first out-of-scope item, ruling 9.3): the
    // floor keeps one useful pan for each device sharing the budget, so a
    // cut never pauses every device's display.
    void theFloorKeepsOneUsefulPanForEachSharingDevice()
    {
        const DisplayBudgetCharge onePan = DisplayLoadGovernor::floorPanCharge();
        const DisplayBudgetCharge two = DisplayLoadGovernor::floorCharge(2);
        QCOMPARE(two.applicationBytesPerSecond,
                 ps3DisplayCharge().applicationBytesPerSecond
                     + 2 * onePan.applicationBytesPerSecond);
        QCOMPARE(two.spectrumSampleUnitsPerSecond, 2 * onePan.spectrumSampleUnitsPerSecond);
        QCOMPARE(DisplayLoadGovernor::floorCharge(0).spectrumSampleUnitsPerSecond,
                 onePan.spectrumSampleUnitsPerSecond);

        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        const auto busyForTwo = [&governor](qint64 t) {
            DisplayLoadReading reading = loadReading(t, 2.0 - 0.04 * governor.steps());
            reading.floorPans = 2;
            return reading;
        };
        QVERIFY(feed(governor, 0, 600'000, busyForTwo).size() > 1);
        QCOMPARE(governor.limits().applicationBytesPerSecond, two.applicationBytesPerSecond);
        QCOMPARE(governor.limits().spectrumSampleUnitsPerSecond, two.spectrumSampleUnitsPerSecond);

        // Split between the two devices, each asking for four pans, with
        // nobody subscribed to PureSignal's display: each keeps one pan.
        DisplayBudgetSplitInput in;
        in.total = governor.limits();
        in.governorCut = true;
        in.minimumRequest = onePan;
        for (const char* id : {"a", "b"}) {
            DisplayBudgetSplitDevice device;
            device.id = id;
            device.request = fourPans();
            in.devices.append(device);
        }
        for (const DisplayBudgetShare& share : DisplayBudgetSplit::split(in)) {
            QVERIFY(share.limits.applicationBytesPerSecond >= onePan.applicationBytesPerSecond);
            QVERIFY(share.limits.spectrumSampleUnitsPerSecond
                    >= onePan.spectrumSampleUnitsPerSecond);
        }
    }

    // Fix wave 3: a cut in force at one device's floor rises to two floor
    // pans at once when a second device shares the budget, the step kept.
    void aCutInForceRisesToTheFloorWhenADeviceJoins()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        QVERIFY(feed(governor, 0, 600'000, [&governor](qint64 t) {
            return loadReading(t, 2.0 - 0.04 * governor.steps());
        }).size() > 1);
        const DisplayBudgetCharge one = DisplayLoadGovernor::floorCharge();
        QCOMPARE(governor.limits().spectrumSampleUnitsPerSecond, one.spectrumSampleUnitsPerSecond);
        const int steps = governor.steps();
        const quint32 generation = governor.limits().generation;

        DisplayLoadReading joined = loadReading(600'500, 2.0 - 0.04 * steps);
        joined.floorPans = 2;
        const auto decision = step(governor, joined);
        QVERIFY(decision.has_value());
        const DisplayBudgetCharge two = DisplayLoadGovernor::floorCharge(2);
        QCOMPARE(decision->limits.applicationBytesPerSecond, two.applicationBytesPerSecond);
        QCOMPARE(decision->limits.spectrumSampleUnitsPerSecond, two.spectrumSampleUnitsPerSecond);
        QCOMPARE(decision->limits.generation, generation + 1);
        QCOMPARE(decision->reason, DisplayBudgetReason::CoreBusy);
        QCOMPARE(governor.steps(), steps);
        // Held there: nothing more while two devices share it.
        QVERIFY(!step(governor, joined));
    }

    void calmForTenSecondsRestoresOneStepAtATime()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        // Two relieving steps: at 2 s and one settle later.
        const qint64 secondStepMs = 2'000 + DisplayLoadGovernor::kSettleMs;
        QCOMPARE(feed(governor, 0, secondStepMs, [&governor](qint64 t) {
            return loadReading(t, 0.9 - 0.06 * governor.steps());
        }).size(), 2);
        QCOMPARE(governor.steps(), 2);
        const DisplayBudgetCharge accepted = fourPans();
        // The second step settles on a calm load, which keeps it; the calm
        // hold starts once it has settled.
        QVERIFY(feed(governor, secondStepMs + 500,
                     secondStepMs + DisplayLoadGovernor::kSettleMs - 500,
                     [](qint64 t) { return loadReading(t, 0.5); }).isEmpty());
        QCOMPARE(governor.steps(), 2);
        const qint64 base = secondStepMs + DisplayLoadGovernor::kSettleMs - 4'500;

        // Calm: 9.5 s is not enough.
        QVERIFY(feed(governor, base + 4'500, base + 14'000, [](qint64 t) {
            DisplayLoadReading reading = loadReading(t, 0.5);
            reading.systemCpuPercent = 60.0;
            return reading;
        }).isEmpty());
        const auto first = step(governor, loadReading(base + 14'500, 0.5));
        QVERIFY(first.has_value());
        QCOMPARE(first->reason, DisplayBudgetReason::CoreBusy);
        QCOMPARE(first->limits, stepFrom(accepted, 4));

        // A long input wait restarts the calm hold.
        QVERIFY(feed(governor, base + 15'000, base + 20'500,
                     [](qint64 t) { return loadReading(t, 0.5); }).isEmpty());
        DisplayLoadReading waiting = loadReading(base + 21'000, 0.5);
        waiting.highestInputDelayMs = DisplayLoadGovernor::kCalmInputDelayMs;
        QVERIFY(!step(governor, waiting));
        QVERIFY(feed(governor, base + 21'500, base + 31'000,
                     [](qint64 t) { return loadReading(t, 0.5); }).isEmpty());
        const auto restored = step(governor, loadReading(base + 31'500, 0.5));
        QVERIFY(restored.has_value());
        QCOMPARE(restored->reason, DisplayBudgetReason::None);
        QCOMPARE(restored->limits.applicationBytesPerSecond, ceiling.applicationBytesPerSecond);
        QCOMPARE(restored->limits.spectrumSampleUnitsPerSecond,
                 ceiling.spectrumSampleUnitsPerSecond);
        QCOMPARE(restored->limits.generation, quint32{5});
        QCOMPARE(governor.steps(), 0);

        // At the ceiling a calm Core publishes nothing.
        QVERIFY(feed(governor, base + 32'000, base + 80'000,
                     [](qint64 t) { return loadReading(t, 0.1); }).isEmpty());
    }

    // Late blocks are not overload: frame-based noise reduction makes one
    // long block in twelve at the default buffer as a normal part of its
    // work, so a calm load with late blocks in every interval must still
    // give the display back.
    void lateBlocksWithACalmLoadLetTheDisplayRecover()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        // One relieving step at 2 s, kept once it settles on a calm load.
        QCOMPARE(feed(governor, 0, 2'000, [](qint64 t) { return loadReading(t, 0.9); }).size(),
                 1);
        QCOMPARE(governor.steps(), 1);
        const qint64 settled = 2'000 + DisplayLoadGovernor::kSettleMs;
        const auto frames = [](qint64 t) {
            DisplayLoadReading reading = loadReading(t, 0.5);
            reading.lateBlocks = 31;
            return reading;
        };
        QVERIFY(feed(governor, 2'500, settled, frames).isEmpty());
        QVERIFY(!governor.settling());
        QCOMPARE(governor.steps(), 1);

        // Calm, late blocks and all, for the calm hold: the step is undone.
        const QList<DisplayLoadDecision> restored =
            feed(governor, settled + 500, settled + 500 + DisplayLoadGovernor::kCalmHoldMs, frames);
        QCOMPARE(restored.size(), 1);
        QCOMPARE(restored.first().reason, DisplayBudgetReason::None);
        QCOMPARE(restored.first().limits.applicationBytesPerSecond,
                 ceiling.applicationBytesPerSecond);
        QCOMPARE(restored.first().limits.spectrumSampleUnitsPerSecond,
                 ceiling.spectrumSampleUnitsPerSecond);
        QCOMPARE(governor.steps(), 0);
    }

    void resetReturnsToTheCeilingOnce()
    {
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DisplayLoadGovernor governor(ceiling);
        QVERIFY(!governor.reset());
        QCOMPARE(feed(governor, 0, 2'000,
                      [](qint64 t) { return loadReading(t, 0.9); }).size(), 1);
        const auto reset = governor.reset();
        QVERIFY(reset.has_value());
        QCOMPARE(reset->reason, DisplayBudgetReason::None);
        QCOMPARE(reset->limits.applicationBytesPerSecond, ceiling.applicationBytesPerSecond);
        QCOMPARE(reset->limits.generation, quint32{3});
        QVERIFY(!governor.reset());
    }

    // R-R3-40/41 (final review I1): with thread placement giving a
    // receiver's worker and the DSP thread cores of their own, the display
    // runs elsewhere and lowering it cannot relieve that receiver.
    void receiverLoadCountsOnlyWhereTheDisplayCanRelieveIt()
    {
        PlacementPlan placed;
        placed.active = true;
        placed.signalPool = {4, 5, 6, 7};
        placed.housekeeping = {0, 1, 2, 3};
        placed.assignments = {{ThreadRole::RxWorker, 0, 4}, {ThreadRole::DspThread, -1, 5}};
        QVERIFY(!receiverSharesDisplayCores(placed, 0));
        QVERIFY(receiverSharesDisplayCores(placed, 1)); // no core of its own
        PlacementPlan noDspCore = placed;
        noDspCore.assignments = {{ThreadRole::RxWorker, 0, 4}};
        QVERIFY(receiverSharesDisplayCores(noDspCore, 0));
        QVERIFY(receiverSharesDisplayCores(PlacementPlan{}, 0)); // not placed

        const auto inputs = [](const PlacementPlan& plan, QList<int> slices) {
            DisplayLoadInputs in;
            in.placement = plan;
            for (int slice : slices) {
                ReceiverDspLoad load;
                load.load = 0.85;
                load.lateBlocks = 2;
                load.inputDelayMs = 150;
                in.receivers.append({slice, load});
            }
            return in;
        };
        const DisplayBudgetCharge accepted = fourPans();

        // Placed: 0.85 held for a minute changes nothing.
        DisplayLoadGovernor placedGovernor(DisplayLoadGovernor::computedCeiling());
        const DisplayLoadReading placedReading
            = displayLoadReadingFrom(inputs(placed, {0}), 0, accepted);
        QVERIFY(!placedReading.highestReceiverLoad.has_value());
        QCOMPARE(placedReading.lateBlocks, qint64{0});
        QVERIFY(feed(placedGovernor, 0, 60'000, [&](qint64 t) {
            return displayLoadReadingFrom(inputs(placed, {0}), t, accepted);
        }).isEmpty());
        QCOMPARE(placedGovernor.steps(), 0);

        // Placement off: the same load steps down after the busy hold.
        DisplayLoadGovernor offGovernor(DisplayLoadGovernor::computedCeiling());
        const DisplayLoadReading offReading
            = displayLoadReadingFrom(inputs(PlacementPlan{}, {0}), 0, accepted);
        QCOMPARE(offReading.highestReceiverLoad, std::optional<double>(0.85));
        QCOMPARE(offReading.lateBlocks, qint64{2});
        QCOMPARE(offReading.highestInputDelayMs, qint64{150});
        const auto offDecisions = feed(offGovernor, 0, 2'000, [&](qint64 t) {
            return displayLoadReadingFrom(inputs(PlacementPlan{}, {0}), t, accepted);
        });
        QCOMPARE(offDecisions.size(), 1);
        QCOMPARE(offDecisions.first().reason, DisplayBudgetReason::CoreBusy);

        // Placed, but a second receiver shares the housekeeping cores: its
        // load counts.
        const DisplayLoadReading mixed = displayLoadReadingFrom(inputs(placed, {0, 1}), 0, accepted);
        QCOMPARE(mixed.highestReceiverLoad, std::optional<double>(0.85));
        QCOMPARE(mixed.lateBlocks, qint64{2});

        // Idle receivers are never a measurement.
        DisplayLoadInputs idle = inputs(PlacementPlan{}, {0});
        idle.receivers[0].load.idle = true;
        QVERIFY(!displayLoadReadingFrom(idle, 0, accepted).highestReceiverLoad.has_value());
    }

    // R-R3-40/41 (final review I2): while placing, the governor's CPU is the
    // housekeeping cores' share, where spectrum, encoding, Opus and sending
    // run; otherwise all cores. Telemetry's systemCpuPercent is untouched.
    void cpuIsTheHousekeepingShareWhilePlacing()
    {
        PlacementPlan placed;
        placed.active = true;
        placed.housekeeping = {0, 1, 2, 3};
        DisplayLoadInputs in;
        in.systemCpuPercent = 80.0;
        in.housekeepingCpuPercent = 100.0;
        in.placement = placed;
        QCOMPARE(displayLoadReadingFrom(in, 0, fourPans()).systemCpuPercent,
                 std::optional<double>(100.0));
        in.placement = PlacementPlan{};
        QCOMPARE(displayLoadReadingFrom(in, 0, fourPans()).systemCpuPercent,
                 std::optional<double>(80.0));
        // Placing without a housekeeping reading yet: no CPU measurement,
        // not the aggregate.
        in.placement = placed;
        in.housekeepingCpuPercent.reset();
        QVERIFY(!displayLoadReadingFrom(in, 0, fourPans()).systemCpuPercent.has_value());
    }

    void aConfiguredCeilingBelowTheFloorIsNeverRaised()
    {
        const DisplayBudgetCharge small = panCharge(128, 5);
        const DisplayBudgetLimits ceiling{small.applicationBytesPerSecond,
                                          small.spectrumSampleUnitsPerSecond, 7};
        DisplayLoadGovernor governor(ceiling);
        QVERIFY(feed(governor, 0, 20'000, [&small](qint64 t) {
            return loadReading(t, 0.95, small);
        }).isEmpty());
        QCOMPARE(governor.limits(), ceiling);
    }
};

QTEST_MAIN(TstDisplayLoadGovernor)
#include "tst_display_load_governor.moc"
