// no-port-check: NereusSDR-original test of the slow noise-reduction call
// accounting (dsplock.c WdspNoteNrCall / TakeChannelSlowNrCalls) and the
// Core log lines RadioModel builds from it, 2026-10-22.
#include <QtTest/QtTest>

#include "core/wdsp_api.h"
#include "models/RadioModel.h"

using NereusSDR::RadioModel;

namespace {

constexpr int kChannel = 5;
constexpr long long kMsNs = 1'000'000LL;
constexpr int kPremium = WDSP_NR_STAGE_NNR_PREMIUM;
constexpr int kStandard = WDSP_NR_STAGE_NNR_STANDARD;
// The call in which WDSP's worker applied the standard-only limit to a
// running Premium model (xrxa's rxa_nr_limit_bits).
constexpr int kPremiumLimitedToStandard = WDSP_NR_STAGE_NNR_PREMIUM | WDSP_NR_NNR_LIMIT_APPLIED
                                          | WDSP_NR_NNR_FROM_PREMIUM | WDSP_NR_NNR_TO_STANDARD;

WdspSlowNrCalls take(int channel)
{
    WdspSlowNrCalls calls{};
    calls.count = -1;
    if (TakeChannelSlowNrCalls(channel, &calls) != 0) {
        calls.count = -2;
    }
    return calls;
}

} // namespace

class TstWdspSlowNrCalls : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QCOMPARE(take(kChannel).count >= 0, true);
        QCOMPARE(take(kChannel + 1).count >= 0, true);
    }

    void countsOnlyCallsOverFiftyMillisecondsWithTheirStages()
    {
        WdspNoteNrCall(kChannel, 2 * kMsNs, kPremium);
        WdspNoteNrCall(kChannel, 50 * kMsNs, kPremium);       // not over 50 ms
        WdspNoteNrCall(kChannel, 50 * kMsNs + 1000, kPremium);
        WdspNoteNrCall(kChannel, 1200 * kMsNs, kPremium);
        WdspNoteNrCall(kChannel, 75 * kMsNs, WDSP_NR_STAGE_MNR);
        const WdspSlowNrCalls calls = take(kChannel);
        QCOMPARE(calls.count, 3LL);
        QCOMPARE(calls.longestUs, 1200000LL);
        QCOMPARE(calls.longestStages, kPremium);
        QCOMPARE(calls.stages, kPremium | WDSP_NR_STAGE_MNR);
    }

    // A group in which no NR stage ran (and no limit was applied) is not an
    // NR call, however long it took (a preempted worker).
    void aCallThatRanNoStageIsNotCounted()
    {
        WdspNoteNrCall(kChannel, 900 * kMsNs, 0);
        const WdspSlowNrCalls calls = take(kChannel);
        QCOMPARE(calls.count, 0LL);
        QCOMPARE(calls.longestUs, 0LL);
        QCOMPARE(calls.stages, 0);
    }

    // The longest call keeps its own stages: a shorter call under another
    // model in the same interval does not relabel it.
    void theLongestKeepsTheStagesItRan()
    {
        WdspNoteNrCall(kChannel, 400 * kMsNs, kPremiumLimitedToStandard);
        WdspNoteNrCall(kChannel, 90 * kMsNs, kStandard);
        const WdspSlowNrCalls calls = take(kChannel);
        QCOMPARE(calls.count, 2LL);
        QCOMPARE(calls.longestUs, 400000LL);
        QCOMPARE(calls.longestStages, kPremiumLimitedToStandard);
        QCOMPARE(calls.stages, kPremiumLimitedToStandard | kStandard);
    }

    void aTakeStartsTheNextInterval()
    {
        WdspNoteNrCall(kChannel, 300 * kMsNs, WDSP_NR_STAGE_EMNR);
        QCOMPARE(take(kChannel).count, 1LL);
        const WdspSlowNrCalls calls = take(kChannel);
        QCOMPARE(calls.count, 0LL);
        QCOMPARE(calls.longestUs, 0LL);
        QCOMPARE(calls.longestStages, 0);
        QCOMPARE(calls.stages, 0);
    }

    void channelsAreCountedApart()
    {
        WdspNoteNrCall(kChannel + 1, 90 * kMsNs, WDSP_NR_STAGE_DFNR);
        QCOMPARE(take(kChannel).count, 0LL);
        const WdspSlowNrCalls calls = take(kChannel + 1);
        QCOMPARE(calls.count, 1LL);
        QCOMPARE(calls.longestUs, 90000LL);
        QCOMPARE(calls.longestStages, WDSP_NR_STAGE_DFNR);
    }

    void rejectsInvalidChannelsAndNullPointers()
    {
        WdspNoteNrCall(-1, 900 * kMsNs, kPremium);   // ignored, no crash
        WdspSlowNrCalls calls{};
        QCOMPARE(TakeChannelSlowNrCalls(-1, &calls), -1);
        QCOMPARE(TakeChannelSlowNrCalls(kChannel, nullptr), -1);
    }

    void stageNamesSayWhatRan()
    {
        QCOMPARE(RadioModel::nrStagesLogName(0), QStringLiteral("no stage recorded"));
        QCOMPARE(RadioModel::nrStagesLogName(kPremium), QStringLiteral("NNR Premium"));
        QCOMPARE(RadioModel::nrStagesLogName(kStandard), QStringLiteral("NNR Standard"));
        QCOMPARE(RadioModel::nrStagesLogName(WDSP_NR_STAGE_ANR | WDSP_NR_STAGE_EMNR
                                             | WDSP_NR_STAGE_RNNR | WDSP_NR_STAGE_SBNR
                                             | WDSP_NR_STAGE_DFNR | WDSP_NR_STAGE_MNR),
                 QStringLiteral("NR1, NR2, NR3, NR4, DFNR, MNR"));
        QCOMPARE(RadioModel::nrStagesLogName(kPremiumLimitedToStandard),
                 QStringLiteral("NNR Premium, the NNR limit switched NNR from Premium to "
                                "Standard inside the call"));
        // The off step: NNR ran Standard at the call's start, then stopped.
        QCOMPARE(RadioModel::nrStagesLogName(kStandard | WDSP_NR_NNR_LIMIT_APPLIED
                                             | WDSP_NR_NNR_FROM_STANDARD),
                 QStringLiteral("NNR Standard, the NNR limit switched NNR from Standard to "
                                "off inside the call"));
    }

    void logLineNamesSliceStagesAndTime()
    {
        QCOMPARE(RadioModel::slowNrCallLogLine(0, 3, 1012345, kPremium, kPremium | kStandard),
                 QStringLiteral("Slice A (index 0): 3 noise reduction call(s) over 50 ms since "
                                "the last report, running NNR Standard, NNR Premium; the longest "
                                "took 1012.3 ms, ran NNR Premium"));
    }

    // A call can land between a take's reads (dsplock.c): the next report
    // has its count but not its length.
    void logLineSaysOverFiftyMillisecondsWithoutALongest()
    {
        QCOMPARE(RadioModel::slowNrCallLogLine(1, 1, 0, 0, kStandard),
                 QStringLiteral("Slice B (index 1): 1 noise reduction call(s) over 50 ms since "
                                "the last report, running NNR Standard; the longest took over "
                                "50 ms"));
    }

    void nonFiniteOutputLineNamesTheSlice()
    {
        QCOMPARE(RadioModel::outputNonFiniteLogLine(0, 2048),
                 QStringLiteral("Slice A (index 0): 2048 non-finite samples in WDSP's output "
                                "since the last report"));
    }
};

QTEST_GUILESS_MAIN(TstWdspSlowNrCalls)
#include "tst_wdsp_slow_nr_calls.moc"
