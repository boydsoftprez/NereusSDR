// tst_noise_floor_estimator.cpp
//
// Phase 3G-9c — Clarity noise-floor estimator unit tests. Pure math over
// NoiseFloorEstimator::estimate(). Strict TDD: each test was written before
// the corresponding code path exists (RED), verified failing, then the
// minimal code to pass lands.

#include <QtTest/QtTest>
#include <QtNumeric>

#include "core/NoiseFloorEstimator.h"

using namespace NereusSDR;

class TestNoiseFloorEstimator : public QObject {
    Q_OBJECT
private slots:

    void flatNoise_returnsNoiseFloor()
    {
        // A band of uniform -130 dBm noise returns -130 at any percentile —
        // every bin has the same value so the result is unambiguous.
        QVector<float> bins(2048, -130.0f);
        NoiseFloorEstimator est(0.30f);
        QCOMPARE(est.estimate(bins), -130.0f);
    }

    void singleCarrier_doesNotMoveEstimate()
    {
        // A single strong carrier at bins[0] must not dominate — the 30th
        // percentile still sits at the noise floor. This is the whole
        // point of percentile over mean: one outlier can't pull the
        // estimate up. Contrast with Thetis processNoiseFloor which
        // requires upstream filtering to skip carriers.
        QVector<float> bins(2048, -130.0f);
        bins[0] = -40.0f;  // loud carrier
        NoiseFloorEstimator est(0.30f);
        QCOMPARE(est.estimate(bins), -130.0f);
    }

    void emptyInput_returnsNaN()
    {
        // Cold start (no FFT frame yet) passes an empty bin vector.
        // The estimator must return a well-defined sentinel — NaN —
        // so downstream (ClarityController) can detect "no data yet"
        // and fall back to static thresholds during the bootstrap
        // window described in design spec §6.2.4.
        QVector<float> bins;
        NoiseFloorEstimator est;
        QVERIFY(qIsNaN(est.estimate(bins)));
    }

    void multiSsbScene_returnsNoiseFloor()
    {
        // Half the band occupied by SSB envelopes sitting 70 dB above
        // the noise floor. The 30th percentile lands squarely in the
        // noise region since signal bins are a minority.
        QVector<float> bins;
        bins.reserve(2048);
        for (int i = 0; i < 1024; ++i) { bins.append(-130.0f); }
        for (int i = 0; i < 1024; ++i) { bins.append(-60.0f);  }
        NoiseFloorEstimator est(0.30f);
        QCOMPARE(est.estimate(bins), -130.0f);
    }

    void allBinsZero_returnsZero()
    {
        // Degenerate "FFT returned all zeros" case — no crash, no NaN,
        // just 0. Downstream will handle by clamping the floor via
        // ClarityController's min-gap logic.
        QVector<float> bins(2048, 0.0f);
        NoiseFloorEstimator est;
        QCOMPARE(est.estimate(bins), 0.0f);
    }

    void callerVectorIsNotMutated()
    {
        // The estimator copies bins into an internal work buffer so
        // callers can reuse their own vectors without observing side
        // effects. Breaking this invariant breaks FFTEngine — it would
        // hand the same buffer to the waterfall AND Clarity each frame.
        QVector<float> bins = { -130.0f, -120.0f, -110.0f, -40.0f };
        const QVector<float> before = bins;
        NoiseFloorEstimator est(0.30f);
        (void) est.estimate(bins);
        QCOMPARE(bins, before);
    }
};

QTEST_MAIN(TestNoiseFloorEstimator)
#include "tst_noise_floor_estimator.moc"
