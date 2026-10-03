// no-port-check: NereusSDR test of the port of Thetis's grid noise-floor
// tracking. Thetis moves the grid minimum to the display's own noise floor
// every 500 ms on tmrAutoAGC, not while transmitting, and only when the
// floor is "good" (not in fast attack):
//   console.cs:46136-46167 [v2.10.3.15] tmrAutoAGC_Tick
//   display.cs:5398-5404, 4670-4677, 925-934 [v2.10.3.15]
// The pan's own noise floor (NoiseFloorFollower) drives it, so it works with
// Clarity off and in a remote window; while Clarity feeds a pan its
// estimate, Clarity keeps driving that pan's grid (the add-on is kept).
//
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"

#include <cmath>

using namespace NereusSDR;

class TestGridFollowsDisplayNoiseFloor : public QObject
{
    Q_OBJECT

private:
    static constexpr double kPanCentreHz = 14200000.0;
    static constexpr double kPanSpanHz   = 800.0;

    static void configure(SpectrumWidget& w)
    {
        w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
        w.setDdcCenterFrequency(kPanCentreHz);
        w.setSampleRate(kPanSpanHz);
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::None);
        w.setDbmRange(-150.0f, -40.0f);
        w.setNFOffsetGridFollow(-5);
        w.setMaintainNFAdjustDelta(false);
        w.setAdjustGridMinToNoiseFloor(true);
        // Out of fast attack, at a known floor.
        w.setNoiseFloorFastAttack(false);
        w.setMeasuredNoiseFloorForTest(-120.0f);
    }

    static void feed(SpectrumWidget& w)
    {
        const QVector<float> bins(4096, 1e-12f);   // a flat floor
        w.updateSpectrumLinear(0, bins, 2.0, -10.0);
    }

private slots:
    void gridMinFollowsTheDisplayNoiseFloorWithoutClarity()
    {
        SpectrumWidget w;
        configure(w);
        feed(w);
        const int expected = qRound(w.nfLerpAverageForTest() + w.nfShiftDbm() - 5.0f);
        QVERIFY(std::abs(expected - w.gridMin()) >= 2);
        QTRY_COMPARE_WITH_TIMEOUT(w.gridMin(), expected, 2000);
        QCOMPARE(w.gridMax(), -40);   // Maintain grid range is off
    }

    void maintainGridRangeMovesTheMaxToo()
    {
        SpectrumWidget w;
        configure(w);
        w.setMaintainNFAdjustDelta(true);
        feed(w);
        const int expected = qRound(w.nfLerpAverageForTest() + w.nfShiftDbm() - 5.0f);
        QTRY_COMPARE_WITH_TIMEOUT(w.gridMin(), expected, 2000);
        QCOMPARE(w.gridMax() - w.gridMin(), 110);
    }

    void holdsWhileTransmittingAndInFastAttack()
    {
        SpectrumWidget keyed;
        configure(keyed);
        keyed.setMoxOverlay(true);
        feed(keyed);
        SpectrumWidget attacking;
        configure(attacking);
        attacking.setNoiseFloorFastAttack(true);
        feed(attacking);
        QTest::qWait(1200);
        QCOMPARE(attacking.gridMin(), -150);
        keyed.setMoxOverlay(false);
        // The receive grid the pan keeps while keyed.
        QCOMPARE(keyed.gridMin(), -150);
    }

    void aPanClarityFeedsKeepsClaritysGrid()
    {
        SpectrumWidget w;
        configure(w);
        w.testApplyNoiseFloor(-100.0f);   // Clarity's estimate
        QCOMPARE(w.gridMin(), -105);
        feed(w);
        QTest::qWait(700);
        QCOMPARE(w.gridMin(), -105);
    }

    void offDoesNothing()
    {
        SpectrumWidget w;
        configure(w);
        w.setAdjustGridMinToNoiseFloor(false);
        feed(w);
        QTest::qWait(1200);
        QCOMPARE(w.gridMin(), -150);
    }
};

QTEST_MAIN(TestGridFollowsDisplayNoiseFloor)
#include "tst_grid_follows_display_noise_floor.moc"
