// tst_step_attenuator_controller.cpp
//
// StepAttenuatorController unit tests — ADC overload hysteresis counter
// state machine. Verifies the per-ADC counter increment/decrement logic,
// level capping, severity transitions (None→Yellow→Red and back), and
// multi-ADC independence.
//
// The tick() method is public for testability; tests call it directly
// rather than waiting on QTimer, giving deterministic cycle control.
//
// Porting from Thetis console.cs:21359-21382 per-ADC overload level logic.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/StepAttenuatorController.h"

using namespace NereusSDR;

class TestStepAttenuatorController : public QObject {
    Q_OBJECT
private slots:

    void singleOverflow_emitsYellow()
    {
        // A single ADC overflow event followed by one tick should raise
        // the counter from 0→1, which is above 0 → Yellow severity.
        // From Thetis console.cs:21378: level > 0 triggers warning text.
        StepAttenuatorController ctrl;
        // Stop the internal timer so only manual tick() calls drive state.
        ctrl.setTickTimerEnabled(false);

        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);
        qRegisterMetaType<NereusSDR::OverloadLevel>();

        ctrl.onAdcOverflow(0);
        ctrl.tick();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0);  // ADC index
        QCOMPARE(spy.first().at(1).value<OverloadLevel>(), OverloadLevel::Yellow);
        QCOMPARE(ctrl.overloadCounter(0), 1);
    }

    void noOverflow_levelDecays()
    {
        // After reaching Yellow, if no further overflow events arrive,
        // each tick decrements the counter by 1 until it reaches 0 (None).
        // From Thetis console.cs:21373-21375.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // Raise to Yellow.
        ctrl.onAdcOverflow(0);
        ctrl.tick();
        QCOMPARE(ctrl.overloadCounter(0), 1);
        QCOMPARE(spy.count(), 1);  // None→Yellow

        // One tick with no overflow → counter 1→0 → back to None.
        ctrl.tick();
        QCOMPARE(ctrl.overloadCounter(0), 0);
        QCOMPARE(spy.count(), 2);  // Yellow→None
        QCOMPARE(spy.last().at(1).value<OverloadLevel>(), OverloadLevel::None);
    }

    void sustainedOverflow_escalatesToRed()
    {
        // Sustained overflows across >3 ticks push the level past the
        // red threshold (>3). From Thetis console.cs:21369: red when > 3.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // 4 ticks with overflow: level goes 0→1→2→3→4.
        // Yellow emitted at tick 1 (level 1), Red at tick 4 (level 4 > 3).
        for (int i = 0; i < 4; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        QCOMPARE(ctrl.overloadCounter(0), 4);
        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Red);

        // Should have emitted Yellow on first tick, Red on fourth.
        QVERIFY(spy.count() >= 2);
        QCOMPARE(spy.first().at(1).value<OverloadLevel>(), OverloadLevel::Yellow);
        QCOMPARE(spy.last().at(1).value<OverloadLevel>(), OverloadLevel::Red);
    }

    void levelCapsAtFive()
    {
        // From Thetis console.cs:21366 — counter caps at 5 (despite the
        // comment saying 10). Verify it doesn't exceed kMaxOverloadLevel.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();

        // 10 ticks with continuous overflow.
        for (int i = 0; i < 10; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        QCOMPARE(ctrl.overloadCounter(0), 5);
    }

    void redDowngradesToYellow()
    {
        // After reaching Red (level > 3), decay without new overflows
        // should transition Red→Yellow when level drops to 3 (which is
        // not > 3, so Yellow), then Yellow→None when level reaches 0.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // Pump to Red (level 5 = capped).
        for (int i = 0; i < 6; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }
        QCOMPARE(ctrl.overloadCounter(0), 5);
        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Red);
        spy.clear();

        // Decay: 5→4→3. At level 3, severity is Yellow (not > 3).
        ctrl.tick();  // 5→4, still Red
        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Red);

        ctrl.tick();  // 4→3, now Yellow
        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Yellow);
        QVERIFY(spy.count() >= 1);

        // Find the Yellow transition.
        bool foundYellow = false;
        for (const auto& call : spy) {
            if (call.at(1).value<OverloadLevel>() == OverloadLevel::Yellow) {
                foundYellow = true;
                break;
            }
        }
        QVERIFY2(foundYellow, "Expected Red→Yellow transition during decay");
    }

    void multipleAdcsIndependent()
    {
        // Each ADC has its own independent counter. Overflow on ADC 0
        // should not affect ADC 1 or ADC 2.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // Only ADC 1 overflows.
        ctrl.onAdcOverflow(1);
        ctrl.tick();

        QCOMPARE(ctrl.overloadCounter(0), 0);
        QCOMPARE(ctrl.overloadCounter(1), 1);
        QCOMPARE(ctrl.overloadCounter(2), 0);

        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::None);
        QCOMPARE(ctrl.overloadLevel(1), OverloadLevel::Yellow);
        QCOMPARE(ctrl.overloadLevel(2), OverloadLevel::None);

        // Signal should only reference ADC 1.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 1);
    }

    void classicAutoAtt_bumpsOnRed()
    {
        // Classic auto-att bumps the step attenuator by the ADC overload
        // level value when Red threshold is exceeded.
        // From Thetis console.cs:21548-21567.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        ctrl.setAutoAttMode(AutoAttMode::Classic);
        ctrl.setAutoAttUndo(false);

        QSignalSpy spy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // Push ADC 0 to Red: 4 consecutive overflow+tick cycles.
        // Level goes 0→1→2→3→4. Red fires at level 4 (> kRedThreshold=3).
        for (int i = 0; i < 4; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Red);
        QVERIFY2(spy.count() >= 1, "attenuationChanged should fire on Red");
        // Classic bumps by the level value (4 at Red), so ATT > 0.
        int att = spy.last().at(0).toInt();
        QVERIFY2(att > 0, "ATT should be bumped above 0 on Red");
    }

    void adaptiveAttack_rampsGradually()
    {
        // Adaptive mode ramps ATT by 1 dB per tick while Red overload
        // persists. Verify gradual ramp over multiple attack ticks.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        ctrl.setAutoAttMode(AutoAttMode::Adaptive);

        QSignalSpy spy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // 7 ticks with sustained overflow: first 4 reach Red,
        // ticks 5-7 are 3 attack cycles at +1 dB each.
        for (int i = 0; i < 7; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        // Should have at least 3 attenuationChanged emissions (from the
        // 3 attack ticks after reaching Red on tick 4).
        QVERIFY2(spy.count() >= 3, "Expected >= 3 attack emissions");

        // Verify gradual ramp: each emission should be +1 dB from prior.
        for (int i = 1; i < spy.count(); ++i) {
            int prev = spy.at(i - 1).at(0).toInt();
            int curr = spy.at(i).at(0).toInt();
            QCOMPARE(curr, prev + 1);
        }

        // Final ATT should be >= 3 (at least 3 attack ticks after Red).
        int finalAtt = spy.last().at(0).toInt();
        QVERIFY2(finalAtt >= 3, "ATT should be >= 3 after 3+ attack ticks");
    }

    void adaptiveDecay_relaxesAfterHold()
    {
        // Adaptive mode decays ATT by 1 dB per decay interval after the
        // hold period elapses with no further overload. The decay path
        // uses wall-clock time, so we set very short hold/decay values
        // and use QTest::qWait() to advance past them.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        ctrl.setAutoAttMode(AutoAttMode::Adaptive);
        ctrl.setAutoAttUndo(true);
        // Short hold (50ms) and fast decay (1ms per step) for test speed.
        ctrl.setAutoAttHoldSeconds(0.05);   // 50ms hold
        ctrl.setAdaptiveDecayMs(1);         // 1ms decay rate

        QSignalSpy spy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // Push to Red (4 ticks) + 2 more attack ticks = 6 overflow+tick
        // cycles. Level caps at 5. Attack fires on ticks 4-6 (+1 dB each).
        for (int i = 0; i < 6; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        // After stopping overflow, the hysteresis counter decays naturally:
        // 5→4 (still Red, attack continues), 4→3 (Yellow, no more attack).
        // So we tick without overflow to let the counter drain below Red.
        // These ticks may produce additional attack emissions while Red.
        for (int i = 0; i < 3; ++i) {
            ctrl.tick();
        }

        // Record peak ATT value — includes any extra attack ticks during
        // counter drain from Red.
        QVERIFY2(spy.count() >= 1, "Should have attack emissions");
        int peakAtt = spy.last().at(0).toInt();
        QVERIFY2(peakAtt >= 2, "Peak ATT should be >= 2 after attack ticks");

        spy.clear();

        // Wait past the hold period (50ms + generous margin).
        QTest::qWait(150);

        // Tick without overflow to trigger decay path.
        // Each tick calls applyAdaptiveAutoAtt(-1) which decays 1 dB if
        // hold has elapsed and decay interval has passed.
        for (int i = 0; i < 20; ++i) {
            ctrl.tick();
            QTest::qWait(5);  // Ensure decay rate (1ms) is satisfied.
        }

        // Verify decay happened: at least one emission with value < peak.
        QVERIFY2(spy.count() >= 1, "Should have decay emissions after hold");
        bool decayed = false;
        for (int i = 0; i < spy.count(); ++i) {
            if (spy.at(i).at(0).toInt() < peakAtt) {
                decayed = true;
                break;
            }
        }
        QVERIFY2(decayed, "ATT should decay below peak after hold period");
    }
};

QTEST_MAIN(TestStepAttenuatorController)
#include "tst_step_attenuator_controller.moc"
