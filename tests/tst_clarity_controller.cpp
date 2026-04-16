// tst_clarity_controller.cpp
//
// Phase 3G-9c — ClarityController unit tests. Covers the stateful wrapper
// around NoiseFloorEstimator: enable/disable gating, EWMA smoothing,
// deadband gating, poll-rate throttling, TX pause, manual override,
// threshold margin math, and the 30 dB min-gap clamp.
//
// Strict TDD: every test written first, verified RED, then minimal code
// to green. QSignalSpy is used to capture emissions.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/ClarityController.h"

using namespace NereusSDR;

class TestClarityController : public QObject {
    Q_OBJECT
private slots:

    void enabled_emitsThresholdsOnFirstFrame()
    {
        // Smoke path: enabled controller, fed a flat noise frame,
        // emits one threshold update whose low/high bracket the floor
        // with the configured margins. Default margins (-5, +55) around
        // a -130 floor should give (-135, -75) — 60 dB gap, well above
        // the 30 dB min clamp.
        ClarityController ctrl;
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);
        ctrl.setEnabled(true);

        QVector<float> bins(2048, -130.0f);
        ctrl.feedBins(bins, 0);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        const float low  = args.at(0).toFloat();
        const float high = args.at(1).toFloat();
        QCOMPARE(low,  -135.0f);
        QCOMPARE(high, -75.0f);
    }

    void disabled_doesNotEmit()
    {
        // Master-off means no emission regardless of frame content.
        // Default constructor leaves the controller disabled — the
        // smooth-defaults stay in charge until the user opts in.
        ClarityController ctrl;
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> bins(2048, -130.0f);
        ctrl.feedBins(bins, 0);

        QCOMPARE(spy.count(), 0);
    }

    void secondFrameWithinPollInterval_isSuppressed()
    {
        // Cadence gate: at 2 Hz polling (500 ms), two frames fed within
        // the same poll window collapse to a single emission. Design
        // spec §6.2.2 locks 2 Hz; we also keep the UI thread quiet.
        ClarityController ctrl;
        ctrl.setEnabled(true);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> noise(2048, -130.0f);
        QVector<float> hotter(2048, -100.0f);  // deliberately different

        ctrl.feedBins(noise,  0);
        ctrl.feedBins(hotter, 100);  // 100 ms later — inside 500 ms window

        QCOMPARE(spy.count(), 1);
    }

    void transmitting_suppressesEmission()
    {
        // MOX engaged: the RX path is likely distorted by feedback and
        // the noise floor reading is meaningless. Design spec §6.2.4:
        // "Pause Clarity during TX — we already have the MOX signal".
        ClarityController ctrl;
        ctrl.setEnabled(true);
        ctrl.setTransmitting(true);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> bins(2048, -130.0f);
        ctrl.feedBins(bins, 0);

        QCOMPARE(spy.count(), 0);
    }

    void transmitRelease_resumesEmission()
    {
        // After MOX releases and the first post-TX frame arrives past
        // the poll window, Clarity emits again.
        ClarityController ctrl;
        ctrl.setEnabled(true);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> bins(2048, -130.0f);
        ctrl.setTransmitting(true);
        ctrl.feedBins(bins, 0);        // suppressed (TX)
        ctrl.setTransmitting(false);
        ctrl.feedBins(bins, 1000);     // resumed (post-poll)

        QCOMPARE(spy.count(), 1);
    }

    void manualOverride_suppressesUntilRetune()
    {
        // User dragged the Low/High slider while Clarity was running.
        // Clarity pauses — it would undo the user's tune on the next
        // frame otherwise. Stays paused until retuneNow() is called
        // (or the master toggle cycles).
        ClarityController ctrl;
        ctrl.setEnabled(true);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> bins(2048, -130.0f);
        ctrl.feedBins(bins, 0);           // emits (count=1)
        ctrl.notifyManualOverride();      // slider drag
        QVERIFY(ctrl.isPaused());
        ctrl.feedBins(bins, 1000);        // paused → no emission
        ctrl.feedBins(bins, 2000);        // still paused
        QCOMPARE(spy.count(), 1);

        ctrl.retuneNow();                 // Re-tune button
        QVERIFY(!ctrl.isPaused());
        ctrl.feedBins(bins, 3000);        // resumes
        QCOMPARE(spy.count(), 2);
    }

    void manualOverride_emitsPausedChanged()
    {
        // UI reactivity: the status badge in SpectrumOverlayPanel watches
        // pausedChanged to flip green→amber. Must fire exactly once on
        // state transition, not on every feedBins call.
        ClarityController ctrl;
        QSignalSpy pausedSpy(&ctrl, &ClarityController::pausedChanged);

        ctrl.notifyManualOverride();
        QCOMPARE(pausedSpy.count(), 1);
        QCOMPARE(pausedSpy.first().at(0).toBool(), true);

        // Second override call should be idempotent — already paused.
        ctrl.notifyManualOverride();
        QCOMPARE(pausedSpy.count(), 1);
    }

    void retuneNow_emitsPausedChangedFalse()
    {
        ClarityController ctrl;
        ctrl.notifyManualOverride();

        QSignalSpy pausedSpy(&ctrl, &ClarityController::pausedChanged);
        ctrl.retuneNow();

        QCOMPARE(pausedSpy.count(), 1);
        QCOMPARE(pausedSpy.first().at(0).toBool(), false);
    }

    void stableFloor_deadbandSuppressesAfterFirstEmission()
    {
        // Locked spec (§6.2.2): ±2 dB deadband. On a stationary noise
        // floor, Clarity should emit exactly once then stop talking
        // until the floor actually drifts past the deadband.
        // (τ set to 0 so raw-floor == smoothed for this isolation test.)
        ClarityController ctrl;
        ctrl.setEnabled(true);
        ctrl.setSmoothingTauSec(0.0f);
        ctrl.setDeadbandDb(2.0f);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> bins(2048, -130.0f);
        ctrl.feedBins(bins, 0);      // emits
        ctrl.feedBins(bins, 1000);   // past poll, floor unchanged → suppressed
        ctrl.feedBins(bins, 2000);   // still unchanged → suppressed
        ctrl.feedBins(bins, 3000);
        QCOMPARE(spy.count(), 1);
    }

    void floorDriftBeyondDeadband_emitsAgain()
    {
        // Same setup, but between frames the floor moves 10 dB — well
        // past the deadband — so a second emission must fire.
        ClarityController ctrl;
        ctrl.setEnabled(true);
        ctrl.setSmoothingTauSec(0.0f);
        ctrl.setDeadbandDb(2.0f);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> noise (2048, -130.0f);
        QVector<float> louder(2048, -120.0f);
        ctrl.feedBins(noise,  0);
        ctrl.feedBins(louder, 1000);
        QCOMPARE(spy.count(), 2);
    }

    void ewmaSmoothing_dampensStepChange()
    {
        // Verify EWMA is active and dampens. With τ=3s, a step from
        // -130 to -100 after 1 second should NOT snap instantly:
        //   alpha = 1 - exp(-1/3) ≈ 0.28
        //   smoothed ≈ -121.5 dBm (not -100)
        // So the emitted low should be between the anchored and settled
        // values, proving the exponential filter is in the path.
        ClarityController ctrl;
        ctrl.setEnabled(true);
        ctrl.setSmoothingTauSec(3.0f);
        ctrl.setDeadbandDb(0.0f);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> cold(2048, -130.0f);
        QVector<float> hot (2048, -100.0f);

        ctrl.feedBins(cold, 0);
        ctrl.feedBins(hot,  1000);
        QCOMPARE(spy.count(), 2);

        const float settledLow = -100.0f + (-5.0f);   // -105 if snap
        const float anchoredLow = -130.0f + (-5.0f);   // -135 if stationary
        const float actualLow = spy.last().at(0).toFloat();
        QVERIFY2(actualLow < settledLow,
                 "EWMA must not snap instantaneously");
        QVERIFY2(actualLow > anchoredLow,
                 "EWMA must begin moving toward new floor");
    }

    void minGapClamp_expandsNarrowGap()
    {
        // Pathological margin config that produces < 30 dB gap. The
        // empty-band clamp (spec §6.2.4) must expand the gap to 30 dB.
        ClarityController ctrl;
        ctrl.setEnabled(true);
        ctrl.setSmoothingTauSec(0.0f);
        ctrl.setLowMarginDb(-5.0f);
        ctrl.setHighMarginDb(10.0f);   // gap = 15 dB (< 30 min)
        ctrl.setMinGapDb(30.0f);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> bins(2048, -130.0f);
        ctrl.feedBins(bins, 0);
        QCOMPARE(spy.count(), 1);
        const float low  = spy.first().at(0).toFloat();
        const float high = spy.first().at(1).toFloat();
        QVERIFY2((high - low) >= 30.0f,
                 "Min gap clamp must enforce ≥ 30 dB palette range");
    }

    void emptyBins_doesNotEmitOrCrash()
    {
        // Cold-start before FFTEngine sends the first frame.
        ClarityController ctrl;
        ctrl.setEnabled(true);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        QVector<float> empty;
        ctrl.feedBins(empty, 0);
        QCOMPARE(spy.count(), 0);
    }

    void snapToFloor_emitsImmediately()
    {
        // Band-switch path: PanadapterModel feeds the stored floor for
        // the new band into ClarityController::snapToFloor(). The
        // controller sets its EWMA anchor and emits thresholds right
        // away so the waterfall snaps to the remembered state, then
        // Clarity refines from there (spec §6.2.4 "band switch").
        ClarityController ctrl;
        ctrl.setEnabled(true);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        ctrl.snapToFloor(-100.0f);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toFloat(), -105.0f);  // -100 + (-5)
        QCOMPARE(spy.first().at(1).toFloat(),  -45.0f);  // -100 + 55
    }

    void snapToFloor_nan_isIgnored()
    {
        // Band has no stored Clarity data — snap with NaN does nothing.
        ClarityController ctrl;
        ctrl.setEnabled(true);
        QSignalSpy spy(&ctrl, &ClarityController::waterfallThresholdsChanged);

        ctrl.snapToFloor(qQNaN());
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestClarityController)
#include "tst_clarity_controller.moc"
