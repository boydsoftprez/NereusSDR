// =================================================================
// tests/tst_mox_controller_vox_threshold.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this test
// file. The test exercises:
//   - MoxController::setVoxThreshold(int dB)     — H.2 Phase 3M-1b
//   - MoxController::onMicBoostChanged(bool)     — H.2 Phase 3M-1b
//   - MoxController::setVoxGainScalar(float)     — H.2 Phase 3M-1b
//   - MoxController::voxThresholdRequested(double) — H.2 Phase 3M-1b signal
//
// Source references (for traceability):
//   Thetis Project Files/Source/Console/cmaster.cs:1054-1059 [v2.10.3.13]
//     — CMSetTXAVoxThresh: original scaling logic + SetDEXPAttackThreshold.
//   Thetis Project Files/Source/Console/setup.cs:18908-18912 [v2.10.3.13]
//     — udDEXPThreshold_ValueChanged: dB→linear conversion before calling
//       CMSetTXAVoxThresh; establishes that thresh parameter is linear amplitude.
//   Pre-code review §3.3 + §8.3 — mic-boost-aware threshold analysis.
//
// Two-step Thetis formula (verified against upstream):
//   Step 1 (setup.cs:18911): thresh = pow(10.0, dB / 20.0)
//   Step 2 (cmaster.cs:1057): if (MicBoost) thresh *= VOXGain
//
// Default member values (from Thetis source):
//   m_voxThresholdDb  = -40  (TransmitModel default)
//   m_micBoost        = true (console.cs:13237 [v2.10.3.13]: mic_boost = true)
//   m_voxGainScalar   = 1.0  (audio.cs:194 [v2.10.3.13]: vox_gain = 1.0f)
// =================================================================

// no-port-check: NereusSDR-original test file — no upstream Thetis port.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <cmath>
#include <limits>

#include "core/MoxController.h"

using namespace NereusSDR;

// Helper: compute the expected threshold for cross-check tests.
// Mirrors computeScaledThreshold() exactly.
static double expectedThreshold(int dB, bool micBoost, float gainScalar)
{
    double thresh = std::pow(10.0, static_cast<double>(dB) / 20.0);
    if (micBoost) {
        thresh *= static_cast<double>(gainScalar);
    }
    return thresh;
}

class TestMoxControllerVoxThreshold : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // §1 — First-call emit at default state (NAN sentinel works)
    //
    // A fresh MoxController has m_lastVoxThresholdEmitted = NAN. Calling
    // setVoxThreshold(-40) (matching the default m_voxThresholdDb) must still
    // emit because the NAN sentinel forces the very first call through.
    //
    // This primes WDSP with the correct threshold at startup regardless of
    // what value the model defaults to.
    // ════════════════════════════════════════════════════════════════════════

    void firstCall_defaultValue_emitsOnce()
    {
        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);

        ctrl.setVoxThreshold(-40);  // matches default; NAN sentinel forces emit

        QCOMPARE(spy.count(), 1);
        // With micBoost=true (default) and gainScalar=1.0 (default):
        //   thresh = pow(10.0, -40/20.0) * 1.0 = 0.01
        const double expected = expectedThreshold(-40, true, 1.0f);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), expected));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §2 — Boost OFF: threshold passes through without gain scaling
    //
    // From Thetis cmaster.cs:1057 [v2.10.3.13]:
    //   if (Audio.console.MicBoost) thresh *= (double)Audio.VOXGain;
    // When MicBoost is false the condition is skipped; thresh is just the
    // dB→linear conversion.
    //
    // Use scalar=2.0 so boost-on (0.02) differs from boost-off (0.01):
    //   boost=on : pow(10,-2) * 2.0 = 0.02
    //   boost=off: pow(10,-2)        = 0.01
    // This makes the boost-toggle detectable regardless of scalar=1.0
    // degenerate case.  Prime NAN sentinel with boost=on first, then switch
    // boost off and verify the new (unscaled) value is emitted.
    // ════════════════════════════════════════════════════════════════════════

    void boostOff_thresholdPassesThrough()
    {
        MoxController ctrl;
        ctrl.setVoxGainScalar(2.0f);    // non-unity so on/off values differ
        ctrl.setVoxThreshold(-40);      // prime NAN sentinel with boost=on, scalar=2.0

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.onMicBoostChanged(false);  // computed: 0.02 → 0.01 → must emit

        QCOMPARE(spy.count(), 1);
        const double expected = expectedThreshold(-40, false, 2.0f);
        // pow(10.0, -40/20.0) = 0.01; no gain scaling (boost is off)
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), expected));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §3 — Boost ON, scalar 1.0: scaled == unscaled (identity multiplication)
    //
    // Default m_voxGainScalar = 1.0 so boost-on with scalar 1.0 produces the
    // same linear value as boost-off. Verifies the code path is taken even
    // when the multiplication is a no-op numerically.
    // ════════════════════════════════════════════════════════════════════════

    void boostOn_scalarOne_sameAsUnscaled()
    {
        MoxController ctrl;
        // micBoost defaults to true; gainScalar defaults to 1.0

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxThreshold(-40);

        QCOMPARE(spy.count(), 1);
        const double expected = expectedThreshold(-40, true, 1.0f);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), expected));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §4 — Boost ON, scalar 0.5: threshold halved
    //
    // From Thetis cmaster.cs:1057 [v2.10.3.13]:
    //   if (Audio.console.MicBoost) thresh *= (double)Audio.VOXGain;
    // With VOXGain = 0.5: thresh = pow(10, -40/20) * 0.5 = 0.005.
    //
    // Note: setVoxGainScalar(0.5f) itself triggers recomputeVoxThreshold() via
    // the NAN sentinel, so the spy is attached AFTER priming the threshold first.
    // ════════════════════════════════════════════════════════════════════════

    void boostOn_halfScalar_thresholdHalved()
    {
        MoxController ctrl;
        ctrl.setVoxThreshold(-40);  // prime NAN sentinel (scalar=1.0, emits once)

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxGainScalar(0.5f);  // computed changes: 0.01 → 0.005 → must emit

        QCOMPARE(spy.count(), 1);
        const double expected = expectedThreshold(-40, true, 0.5f);
        // pow(10, -40/20) = 0.01; * 0.5 = 0.005
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), expected));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §5 — Gain scalar change re-emits
    //
    // Changing setVoxGainScalar from 0.5 → 1.0 while boost is on must
    // emit the new scaled value. Idempotent-on-EMITTED: since the computed
    // double changes, a new emit must occur.
    // ════════════════════════════════════════════════════════════════════════

    void gainScalarChange_reEmits()
    {
        MoxController ctrl;
        ctrl.setVoxThreshold(-40);       // first emit (NAN sentinel)
        ctrl.setVoxGainScalar(0.5f);     // changes computed value → emits

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxGainScalar(1.0f);     // back to 1.0 → computed changes again → emits

        QCOMPARE(spy.count(), 1);
        const double expected = expectedThreshold(-40, true, 1.0f);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), expected));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §6 — Mic-boost toggle re-emits
    //
    // Switching micBoost ON → OFF and OFF → ON must each re-emit because
    // the computed linear threshold changes.
    // ════════════════════════════════════════════════════════════════════════

    void micBoostToggle_reEmitsEachTime()
    {
        MoxController ctrl;
        ctrl.setVoxGainScalar(2.0f);     // non-unity scalar so on/off differ
        ctrl.setVoxThreshold(-20);       // prime NAN sentinel

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);

        ctrl.onMicBoostChanged(false);   // boost off → unscaled
        QCOMPARE(spy.count(), 1);
        const double expectedOff = expectedThreshold(-20, false, 2.0f);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), expectedOff));

        ctrl.onMicBoostChanged(true);    // boost on → scaled
        QCOMPARE(spy.count(), 2);
        const double expectedOn = expectedThreshold(-20, true, 2.0f);
        QVERIFY(qFuzzyCompare(spy.at(1).at(0).toDouble(), expectedOn));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §7 — Idempotent on emitted double: repeat setVoxThreshold → no double emit
    //
    // Idempotent-on-EMITTED: after the first emit, a second call with the
    // same dB (which produces the same computed double) must NOT emit again.
    // ════════════════════════════════════════════════════════════════════════

    void repeatSameThreshold_noDoubleEmit()
    {
        MoxController ctrl;
        ctrl.setVoxThreshold(-40);  // first emit (NAN sentinel)

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxThreshold(-40);  // same value → computed double unchanged → no emit

        QCOMPARE(spy.count(), 0);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §8 — Thetis cross-check: specific dB + gain combinations
    //
    // Hand-computed using the Thetis formula verbatim:
    //   thresh = pow(10.0, dB / 20.0)
    //   if (micBoost) thresh *= gainScalar
    //
    // Combo A: dB=-60, boost=off, gain=1.0 → pow(10,-3) = 0.001
    // Combo B: dB=-20, boost=on,  gain=0.5 → pow(10,-1) * 0.5 = 0.05
    // Combo C: dB=0,   boost=on,  gain=2.0 → pow(10, 0) * 2.0 = 2.0
    //
    // These values must be exact within qFuzzyCompare tolerance.
    // ════════════════════════════════════════════════════════════════════════

    void thetisValueCrossCheck_comboA()
    {
        // dB=-60, boost=off, gain=1.0 → 0.001
        MoxController ctrl;
        ctrl.onMicBoostChanged(false);
        ctrl.setVoxGainScalar(1.0f);

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxThreshold(-60);

        QCOMPARE(spy.count(), 1);
        // pow(10, -60/20) = pow(10, -3) = 0.001
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 0.001));
    }

    void thetisValueCrossCheck_comboB()
    {
        // dB=-20, boost=on, gain=0.5 → 0.05
        MoxController ctrl;
        ctrl.setVoxGainScalar(0.5f);  // micBoost defaults to true

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxThreshold(-20);

        QCOMPARE(spy.count(), 1);
        // pow(10, -20/20) = 0.1; * 0.5 = 0.05
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 0.05));
    }

    void thetisValueCrossCheck_comboC()
    {
        // dB=0, boost=on, gain=2.0 → 2.0
        MoxController ctrl;
        ctrl.setVoxGainScalar(2.0f);  // micBoost defaults to true

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxThreshold(0);

        QCOMPARE(spy.count(), 1);
        // pow(10, 0/20) = 1.0; * 2.0 = 2.0
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 2.0));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §9 — Boundary values: no overflow or NaN at extremes
    //
    // ptbVOX range from console.Designer.cs:6018-6019 [v2.10.3.13]:
    //   Minimum = -80 (ptbVOX.Minimum)
    //   Maximum = 0   (ptbVOX.Maximum)
    //
    // At -80 dB: pow(10, -4) = 0.0001 — small but not zero or denormal.
    // At  0 dB:  pow(10,  0) = 1.0    — unity amplitude.
    // Neither should produce NaN, Inf, or a negative value.
    // ════════════════════════════════════════════════════════════════════════

    void boundary_minus80dB_notNanOrZero()
    {
        MoxController ctrl;
        ctrl.onMicBoostChanged(false);  // simplify: no gain scaling

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxThreshold(-80);

        QCOMPARE(spy.count(), 1);
        const double val = spy.at(0).at(0).toDouble();
        QVERIFY(!std::isnan(val));
        QVERIFY(!std::isinf(val));
        QVERIFY(val > 0.0);
        // pow(10, -80/20) = pow(10, -4) = 1e-4
        QVERIFY(qFuzzyCompare(val, 1e-4));
    }

    void boundary_0dB_unity()
    {
        MoxController ctrl;
        ctrl.onMicBoostChanged(false);  // no gain scaling

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);
        ctrl.setVoxThreshold(0);

        QCOMPARE(spy.count(), 1);
        const double val = spy.at(0).at(0).toDouble();
        QVERIFY(!std::isnan(val));
        QVERIFY(!std::isinf(val));
        // pow(10, 0) = 1.0
        QVERIFY(qFuzzyCompare(val, 1.0));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §10 — NAN sentinel: first call always emits even at default value
    //
    // Verifies the NAN sentinel property independently: construct fresh ctrl,
    // do NOT call setVoxThreshold before the spy, then call it and confirm
    // exactly one emit. The spy must fire even though -40 is the default.
    // ════════════════════════════════════════════════════════════════════════

    void nanSentinel_firstCallAlwaysEmits()
    {
        MoxController ctrl;
        // Do not call setVoxThreshold before attaching spy — fresh NAN state.
        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);

        ctrl.setVoxThreshold(-40);

        // Must emit exactly once even though -40 is the default dB value.
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toDouble() > 0.0);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §11 — Boost off then on: idempotency on EMITTED not on INPUT
    //
    // After boost-on produces value V, switching boost-off (different V),
    // then switching boost-on again must re-emit V (the round-trip must not
    // be swallowed by an input-level guard).
    // ════════════════════════════════════════════════════════════════════════

    void boostRoundTrip_reEmitsCorrectly()
    {
        MoxController ctrl;
        ctrl.setVoxGainScalar(4.0f);   // non-unity so on/off clearly differ
        ctrl.setVoxThreshold(-40);      // prime NAN sentinel; emits with boost=true

        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);

        ctrl.onMicBoostChanged(false);  // new emitted value (unscaled)
        QCOMPARE(spy.count(), 1);

        ctrl.onMicBoostChanged(true);   // back to boost-on; different from last emit → emits
        QCOMPARE(spy.count(), 2);

        const double expectedOn = expectedThreshold(-40, true, 4.0f);
        QVERIFY(qFuzzyCompare(spy.at(1).at(0).toDouble(), expectedOn));
    }
};

QTEST_MAIN(TestMoxControllerVoxThreshold)
#include "tst_mox_controller_vox_threshold.moc"
