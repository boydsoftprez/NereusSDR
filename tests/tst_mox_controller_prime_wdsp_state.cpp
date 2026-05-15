// =================================================================
// tests/tst_mox_controller_prime_wdsp_state.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this test
// file. The test exercises:
//   - MoxController::primeWdspState()
//
// Regression for the "VOX needs juggling to prime" bench symptom
// reported 2026-05-14: TxApplet's VOX threshold/hold sliders show
// the correct visual position on app launch (model values restored
// from per-MAC AppSettings via TransmitModel::loadFromSettings), but
// WDSP retains its construction-time defaults until the user moves
// a slider.  Moving the slider then "primes" VOX, after which it
// works correctly.
//
// Root cause: loadFromSettings (RadioModel.cpp:2631) runs early in
// the connect sequence, calling MoxController::setVoxThreshold /
// setVoxHangTime / setAntiVoxGain.  Each setter's NaN-sentinel
// "first-call emit" fires voxThresholdRequested / voxHangTimeRequested
// / antiVoxGainRequested -- but the receiver-side connects
// (MoxController -> TxChannel at RadioModel.cpp:3604/3612/3620) are
// only established LATER in the WDSP-init lambda.  The emits land
// in a void receiver; the NaN sentinels are consumed.  The next
// setter call with the same value short-circuits at the sentinel
// guard, so WDSP never gets re-primed.
//
// Fix: primeWdspState() resets the three NaN sentinels and re-runs
// the three recompute() helpers.  Called from pushTxProcessingChain
// (RadioModel.cpp:3747 lambda) after the MoxController -> TxChannel
// connects are established.
//
// Tests verify that primeWdspState() emits the three signals with
// the current (load-time) values, even when the prior emit consumed
// the NaN sentinel into a void receiver.
// =================================================================

// no-port-check: NereusSDR-original test file -- no upstream Thetis port.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <cmath>

#include "core/MoxController.h"

using namespace NereusSDR;

class TestMoxControllerPrimeWdspState : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // §1 -- primeWdspState() re-emits voxThresholdRequested after first-call
    //
    // Simulates the bench scenario: setVoxThreshold consumes the NaN sentinel
    // during loadFromSettings (when no TxChannel listener is wired), then
    // primeWdspState() runs after TxChannel is wired and must re-emit.
    // ════════════════════════════════════════════════════════════════════════

    void primeWdspState_reEmitsVoxThreshold()
    {
        MoxController ctrl;
        // Simulate loadFromSettings: setter fires NaN-sentinel first emit
        // (which in the bench scenario lands in a void TxChannel receiver).
        ctrl.setVoxThreshold(-25);

        // Attach spy AFTER the sentinel-consuming first emit.
        QSignalSpy spy(&ctrl, &MoxController::voxThresholdRequested);

        // Simulate post-wiring prime: TxChannel listener is now connected.
        ctrl.primeWdspState();

        QCOMPARE(spy.count(), 1);
        // Computed threshold = pow(10, -25/20) * micBoost*scalar
        //                    = pow(10, -1.25) * 1.0 (default boost=true, scalar=1.0)
        //                    ~= 0.05623
        const double expected = std::pow(10.0, -25.0 / 20.0);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), expected));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §2 -- primeWdspState() re-emits voxHangTimeRequested after first-call
    //
    // Same pattern: setVoxHangTime consumes NaN sentinel, primeWdspState()
    // must force a re-emit so the late-wired TxChannel receives the load
    // value (ms -> seconds conversion preserved).
    // ════════════════════════════════════════════════════════════════════════

    void primeWdspState_reEmitsVoxHangTime()
    {
        MoxController ctrl;
        ctrl.setVoxHangTime(800);  // NaN sentinel consumed by first emit

        QSignalSpy spy(&ctrl, &MoxController::voxHangTimeRequested);

        ctrl.primeWdspState();

        QCOMPARE(spy.count(), 1);
        // ms -> seconds conversion: 800 / 1000.0 = 0.8
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 0.8));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §3 -- primeWdspState() re-emits antiVoxGainRequested after first-call
    //
    // Same pattern for the third NaN-sentinel signal (dB -> linear via /20.0).
    // ════════════════════════════════════════════════════════════════════════

    void primeWdspState_reEmitsAntiVoxGain()
    {
        MoxController ctrl;
        ctrl.setAntiVoxGain(-20);  // NaN sentinel consumed; gain = pow(10,-1) = 0.1

        QSignalSpy spy(&ctrl, &MoxController::antiVoxGainRequested);

        ctrl.primeWdspState();

        QCOMPARE(spy.count(), 1);
        // pow(10, -20/20) = pow(10, -1) = 0.1
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 0.1));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §4 -- primeWdspState() fires all three signals in one call
    //
    // Integration: a single primeWdspState() call after loadFromSettings has
    // touched all three setters must re-emit all three signals.  This is the
    // production call site shape inside pushTxProcessingChain.
    // ════════════════════════════════════════════════════════════════════════

    void primeWdspState_emitsAllThreeSignalsOnce()
    {
        MoxController ctrl;
        ctrl.setVoxThreshold(-30);   // consumes vox threshold NaN sentinel
        ctrl.setVoxHangTime(1500);   // consumes vox hang time NaN sentinel
        ctrl.setAntiVoxGain(6);      // consumes anti-vox gain NaN sentinel

        QSignalSpy spyThr(&ctrl, &MoxController::voxThresholdRequested);
        QSignalSpy spyHng(&ctrl, &MoxController::voxHangTimeRequested);
        QSignalSpy spyAvg(&ctrl, &MoxController::antiVoxGainRequested);

        ctrl.primeWdspState();

        QCOMPARE(spyThr.count(), 1);
        QCOMPARE(spyHng.count(), 1);
        QCOMPARE(spyAvg.count(), 1);

        // Cross-check the computed doubles match the same recompute formulas:
        const double expThr = std::pow(10.0, -30.0 / 20.0);   // ~0.03162
        const double expHng = 1500.0 / 1000.0;                // 1.5 s
        const double expAvg = std::pow(10.0,   6.0 / 20.0);   // ~1.9953
        QVERIFY(qFuzzyCompare(spyThr.at(0).at(0).toDouble(), expThr));
        QVERIFY(qFuzzyCompare(spyHng.at(0).at(0).toDouble(), expHng));
        QVERIFY(qFuzzyCompare(spyAvg.at(0).at(0).toDouble(), expAvg));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §5 -- primeWdspState() at default state still emits
    //
    // Edge case: if the user's persisted VOX values happen to match the
    // MoxController member defaults (vox threshold = -40, vox hang = 500 ms,
    // anti-vox gain = 0), the bench symptom still manifests because the
    // first emit during loadFromSettings is consumed before TxChannel is
    // wired.  primeWdspState() must re-emit even when nothing was "changed"
    // from the construction-time default.
    // ════════════════════════════════════════════════════════════════════════

    void primeWdspState_emitsEvenAtDefaultState()
    {
        MoxController ctrl;
        // Touch each setter with the default value to consume the NaN
        // sentinels (mirrors loadFromSettings reading the defaults from
        // disk and calling the setters).
        ctrl.setVoxThreshold(-40);
        ctrl.setVoxHangTime(500);
        ctrl.setAntiVoxGain(0);

        QSignalSpy spyThr(&ctrl, &MoxController::voxThresholdRequested);
        QSignalSpy spyHng(&ctrl, &MoxController::voxHangTimeRequested);
        QSignalSpy spyAvg(&ctrl, &MoxController::antiVoxGainRequested);

        ctrl.primeWdspState();

        QCOMPARE(spyThr.count(), 1);
        QCOMPARE(spyHng.count(), 1);
        QCOMPARE(spyAvg.count(), 1);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §6 -- primeWdspState() repeated calls each re-emit
    //
    // Subsequent connects (disconnect / reconnect to the same radio) need
    // to re-prime a freshly-constructed TxChannel.  primeWdspState() must
    // not "remember" that it already primed once.
    // ════════════════════════════════════════════════════════════════════════

    void primeWdspState_repeatedCallsEachReEmit()
    {
        MoxController ctrl;
        ctrl.setVoxThreshold(-25);
        ctrl.setVoxHangTime(800);
        ctrl.setAntiVoxGain(-20);

        QSignalSpy spyThr(&ctrl, &MoxController::voxThresholdRequested);
        QSignalSpy spyHng(&ctrl, &MoxController::voxHangTimeRequested);
        QSignalSpy spyAvg(&ctrl, &MoxController::antiVoxGainRequested);

        ctrl.primeWdspState();  // first prime
        ctrl.primeWdspState();  // second prime (simulates reconnect)

        QCOMPARE(spyThr.count(), 2);
        QCOMPARE(spyHng.count(), 2);
        QCOMPARE(spyAvg.count(), 2);
    }
};

QTEST_MAIN(TestMoxControllerPrimeWdspState)
#include "tst_mox_controller_prime_wdsp_state.moc"
