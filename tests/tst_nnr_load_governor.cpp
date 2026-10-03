// =================================================================
// tests/tst_nnr_load_governor.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original unit tests for the NNR step-back policy
// (R-R3-40). Pure logic: synthetic loads on a synthetic clock, no WDSP.
//
// Modification history (NereusSDR):
//   2026-09-23 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/dsp/NnrLoadGovernor.h"

using namespace NereusSDR;

namespace {

constexpr qint64 kTick = NnrLoadGovernor::kNnrCheckIntervalMs;

NnrLoadGovernor::Receiver premium(std::optional<double> load,
                                  NnrLimit limit = NnrLimit::None)
{
    NnrLoadGovernor::Receiver receiver;
    receiver.nnrSelected = true;
    receiver.savedModelSlot = 1;
    receiver.limit = limit;
    receiver.load = load;
    return receiver;
}

} // namespace

class TestNnrLoadGovernor : public QObject {
    Q_OBJECT

private slots:
    void thresholdsAreThePlannedValues()
    {
        QCOMPARE(NnrLoadGovernor::kNnrStepDownLoad, 0.90);
        QCOMPARE(NnrLoadGovernor::kNnrStepDownHoldMs, qint64(2000));
        QCOMPARE(NnrLoadGovernor::kNnrStepSettleMs, qint64(5000));
        QCOMPARE(NnrLoadGovernor::kNnrCheckIntervalMs, 500);
    }

    void levelsGoDownOneAtATimeAndNeverUp()
    {
        QCOMPARE(NnrLoadGovernor::nextLimit(1, NnrLimit::None), NnrLimit::StandardOnly);
        QCOMPARE(NnrLoadGovernor::nextLimit(0, NnrLimit::None), NnrLimit::Off);
        QCOMPARE(NnrLoadGovernor::nextLimit(1, NnrLimit::StandardOnly), NnrLimit::Off);
        QCOMPARE(NnrLoadGovernor::nextLimit(0, NnrLimit::StandardOnly), NnrLimit::Off);
        QVERIFY(!NnrLoadGovernor::nextLimit(1, NnrLimit::Off));
        QVERIFY(!NnrLoadGovernor::nextLimit(0, NnrLimit::Off));
    }

    // Two seconds of measured load at the threshold steps Premium down to
    // Standard: the first check sets the time base, the next four cover 2 s.
    void twoSecondsAtTheThresholdStepsPremiumToStandard()
    {
        NnrLoadGovernor governor;
        qint64 now = 0;
        QVERIFY(!governor.observe(0, now, premium(0.90)));
        for (int i = 0; i < 3; ++i) {
            now += kTick;
            QVERIFY2(!governor.observe(0, now, premium(0.90)), "stepped before 2 s");
        }
        now += kTick;
        QCOMPARE(governor.observe(0, now, premium(0.90)), NnrLimit::StandardOnly);
    }

    // The average decides, not single samples: 2 s averaging just under the
    // threshold does not step, even with one very heavy interval in it.
    void theTwoSecondAverageDecides()
    {
        NnrLoadGovernor governor;
        qint64 now = 0;
        governor.observe(0, now, premium(0.5));
        // The first four average 0.875: just short, despite the 2.9.
        const double loads[] = {0.5, 2.9, 0.1, 0.0, 0.2, 0.3};
        for (double load : loads) {
            now += kTick;
            QVERIFY(!governor.observe(0, now, premium(load)));
        }
        // Window now holds 0.1, 0.0, 0.2, 0.3; a heavy run must fill it.
        for (int i = 0; i < 3; ++i) {
            now += kTick;
            QVERIFY(!governor.observe(0, now, premium(1.0)));
        }
        now += kTick;
        // 1.0, 1.0, 1.0, 1.0 over the last 2 s.
        QCOMPARE(governor.observe(0, now, premium(1.0)), NnrLimit::StandardOnly);
    }

    void justUnderTheThresholdNeverSteps()
    {
        NnrLoadGovernor governor;
        qint64 now = 0;
        for (int i = 0; i < 40; ++i, now += kTick) {
            QVERIFY(!governor.observe(0, now, premium(0.8999)));
        }
    }

    // After a step the receiver is left alone for 5 s, then needs another
    // 2 s over the threshold for the next level; it never raises a level.
    void aStepSettlesFiveSecondsThenStepsAgain()
    {
        NnrLoadGovernor governor;
        qint64 now = 0;
        std::optional<NnrLimit> step;
        for (; !step; now += kTick) {
            step = governor.observe(0, now, premium(2.0));
        }
        const qint64 stepMs = now - kTick;
        QCOMPARE(*step, NnrLimit::StandardOnly);
        for (now = stepMs + kTick; now < stepMs + NnrLoadGovernor::kNnrStepSettleMs; now += kTick) {
            QVERIFY2(!governor.observe(0, now, premium(2.0, NnrLimit::StandardOnly)),
                     "judged during the settle time");
        }
        QCOMPARE(governor.observe(0, now, premium(2.0, NnrLimit::StandardOnly)), NnrLimit::Off);
        // Off is the floor; light load never brings a level back.
        for (int i = 0; i < 40; ++i) {
            now += kTick;
            QVERIFY(!governor.observe(0, now, premium(0.0, NnrLimit::Off)));
        }
    }

    void aSavedStandardChoiceStepsToOff()
    {
        NnrLoadGovernor governor;
        NnrLoadGovernor::Receiver standard = premium(1.5);
        standard.savedModelSlot = 0;
        qint64 now = 0;
        std::optional<NnrLimit> step;
        for (int i = 0; i < 5; ++i, now += kTick) {
            step = governor.observe(0, now, standard);
        }
        QCOMPARE(step, NnrLimit::Off);
    }

    void anOverloadedReceiverWithNnrOffGetsNoStep()
    {
        NnrLoadGovernor governor;
        NnrLoadGovernor::Receiver receiver = premium(5.0);
        receiver.nnrSelected = false;
        for (qint64 now = 0; now < 20000; now += kTick) {
            QVERIFY(!governor.observe(0, now, receiver));
        }
    }

    // Idle (or no snapshot) is not measured: it is neither load nor relief.
    // A window with an unmeasured check in it is short of 2 s.
    void idleIsNeverJudged()
    {
        NnrLoadGovernor governor;
        qint64 now = 0;
        for (int i = 0; i < 40; ++i, now += kTick) {
            QVERIFY2(!governor.observe(0, now, premium(std::nullopt)), "stepped on idle");
        }
        // One unmeasured check in the middle of heavy load delays the step
        // until 2 s of measured load follow it.
        governor.reset(0);
        now = 0;
        governor.observe(0, now, premium(2.0));
        now += kTick; QVERIFY(!governor.observe(0, now, premium(2.0)));
        now += kTick; QVERIFY(!governor.observe(0, now, premium(2.0)));
        now += kTick; QVERIFY(!governor.observe(0, now, premium(std::nullopt)));
        for (int i = 0; i < 3; ++i) {
            now += kTick;
            QVERIFY(!governor.observe(0, now, premium(2.0)));
        }
        now += kTick;
        QCOMPARE(governor.observe(0, now, premium(2.0)), NnrLimit::StandardOnly);
    }

    // The window is time, not a count of checks: late checks still need 2 s.
    void aLateCheckCoversItsWholeInterval()
    {
        NnrLoadGovernor governor;
        governor.observe(0, 0, premium(1.0));
        QVERIFY(!governor.observe(0, 1999, premium(1.0)));
        QCOMPARE(governor.observe(0, 2000, premium(1.0)), NnrLimit::StandardOnly);
    }

    void resetForgetsHistoryAndSettleTime()
    {
        NnrLoadGovernor governor;
        qint64 now = 0;
        std::optional<NnrLimit> step;
        for (; !step; now += kTick) {
            step = governor.observe(0, now, premium(2.0));
        }
        governor.reset(0);   // the operator tried again
        std::optional<NnrLimit> again;
        int checks = 0;
        for (; !again; now += kTick, ++checks) {
            again = governor.observe(0, now, premium(2.0));
        }
        QCOMPARE(*again, NnrLimit::StandardOnly);
        QCOMPARE(checks, 5);   // 2 s again, with no settle time left over
    }

    void receiversAreJudgedSeparately()
    {
        NnrLoadGovernor governor;
        qint64 now = 0;
        std::optional<NnrLimit> a, b;
        for (int i = 0; i < 5; ++i, now += kTick) {
            a = governor.observe(0, now, premium(2.0));
            b = governor.observe(1, now, premium(0.2));
        }
        QCOMPARE(a, NnrLimit::StandardOnly);
        QVERIFY(!b);
    }
};

QTEST_APPLESS_MAIN(TestNnrLoadGovernor)
#include "tst_nnr_load_governor.moc"
