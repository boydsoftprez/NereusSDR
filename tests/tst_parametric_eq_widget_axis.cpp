// =================================================================
// tests/tst_parametric_eq_widget_axis.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.  Cites for the math
// under test live in ParametricEqWidget.cpp.
// =================================================================
//
// Phase 3M-3a-ii follow-up Batch 2 -- axis math + ordering
// verification.  Pins linear AND custom-log axis conversions,
// hit-test, ordering, default-band layout, and reset behavior
// against hand-computed values traced directly from
// ucParametricEq.cs [v2.10.3.13].
//
// =================================================================

#include "../src/gui/widgets/ParametricEqWidget.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPoint>
#include <QRect>
#include <QTest>

// Tester shim: friended in ParametricEqWidget.h so this subclass can
// reach private axis/ordering helpers and member state.  The widget
// itself remains encapsulated; only this test sees through.
class ParametricEqAxisTester : public NereusSDR::ParametricEqWidget {
public:
    using NereusSDR::ParametricEqWidget::ParametricEqWidget;

    // Promote the math + ordering helpers into the tester's public API.
    using NereusSDR::ParametricEqWidget::computePlotRect;
    using NereusSDR::ParametricEqWidget::xFromFreq;
    using NereusSDR::ParametricEqWidget::yFromDb;
    using NereusSDR::ParametricEqWidget::freqFromX;
    using NereusSDR::ParametricEqWidget::dbFromY;
    using NereusSDR::ParametricEqWidget::getNormalizedFrequencyPosition;
    using NereusSDR::ParametricEqWidget::frequencyFromNormalizedPosition;
    using NereusSDR::ParametricEqWidget::getLogFrequencyCentreHz;
    using NereusSDR::ParametricEqWidget::getLogFrequencyShape;
    using NereusSDR::ParametricEqWidget::getLogFrequencyTicks;
    using NereusSDR::ParametricEqWidget::chooseFrequencyStep;
    using NereusSDR::ParametricEqWidget::chooseDbStep;
    using NereusSDR::ParametricEqWidget::getYAxisStepDb;
    using NereusSDR::ParametricEqWidget::hitTestPoint;
    using NereusSDR::ParametricEqWidget::hitTestGlobalGainHandle;
    using NereusSDR::ParametricEqWidget::resetPointsDefault;
    using NereusSDR::ParametricEqWidget::rescaleFrequencies;
    using NereusSDR::ParametricEqWidget::enforceOrdering;
    using NereusSDR::ParametricEqWidget::clampAllGains;
    using NereusSDR::ParametricEqWidget::clampAllQ;
    using NereusSDR::ParametricEqWidget::isFrequencyLockedIndex;
    using NereusSDR::ParametricEqWidget::getLockedFrequencyForIndex;
    using NereusSDR::ParametricEqWidget::indexFromBandId;

    // Direct member access for pin-tight ordering tests.
    QVector<EqPoint>& pointsMut()                  { return m_points; }
    const QVector<EqPoint>& pointsConst()  const   { return m_points; }
    int  bandCount()                       const   { return m_bandCount; }
    void setLogScale(bool on)                      { m_logScale = on; }
    void setSelectedIndex(int idx)                 { m_selectedIndex = idx; }
    int  selectedIndex()                   const   { return m_selectedIndex; }
};

class TestParametricEqAxis : public QObject {
    Q_OBJECT
private slots:
    void editorPresentationContract();
    // Linear axis -- 0..4000 Hz.
    void linearXFromFreqMidpoint();
    void linearFreqFromXRoundtrip();

    // dB axis -- -24..+24.
    void dbAxisRoundtrip();

    // Custom log scale (centreRatio = 0.125 by default).
    void logShapeAtDefaultCentre();
    void logFrequencyAtNormalisedHalf();

    // chooseDbStep / chooseFrequencyStep deterministic switches.
    void dbStepSwitches();
    void freqStepSwitches();

    // resetPointsDefault produces N evenly-spaced bands with q=4, gain=0.
    void resetProducesTenEvenBands();
    void resetEndpointsLockedToRange();

    // Ordering: shove a band's frequency, sort kicks in, selection re-resolves.
    void enforceOrderingSortsByFreq();
};

// -----------------------------------------------------------------
// Linear axis tests
// -----------------------------------------------------------------

void TestParametricEqAxis::linearXFromFreqMidpoint() {
    ParametricEqAxisTester w;
    w.resize(460, 540);                 // plot width ~ 400 after margins
    QRect plot = w.computePlotRect();

    // Linear mode by default.  Formula at line cs:2951-2955:
    //   xFromFreq = left + (float)(t * width),  t = 0.5 at midpoint.
    // At midpoint, x must equal left + 0.5*width to within 0.5 px (the
    // float -> int rounding tolerance when width is odd).
    float x = w.xFromFreq(plot, 2000.0);
    double expected = double(plot.left()) + 0.5 * double(plot.width());
    QVERIFY2(qAbs(double(x) - expected) < 0.5,
             qPrintable(QString("x=%1 expected=%2 (left=%3 w=%4)")
                            .arg(double(x)).arg(expected)
                            .arg(plot.left()).arg(plot.width())));

    // Endpoint pin checks.
    QCOMPARE(qRound(w.xFromFreq(plot, 0.0)),    plot.left());
    QCOMPARE(qRound(w.xFromFreq(plot, 4000.0)), plot.left() + plot.width());
}

void TestParametricEqAxis::linearFreqFromXRoundtrip() {
    ParametricEqAxisTester w;
    w.resize(460, 540);
    QRect plot = w.computePlotRect();

    // Pick a non-trivial freq inside the range and round-trip.
    constexpr double kFreq = 1234.0;
    float  x   = w.xFromFreq(plot, kFreq);
    double ret = w.freqFromX(plot, qRound(x));

    // Tolerance: 1 px of plot resolution maps to span/width Hz, so for
    // a ~400 px plot over 4000 Hz that's ~10 Hz/px.  Allow 12 Hz slack
    // to absorb the qRound() on x without making the test brittle.
    QVERIFY2(qAbs(ret - kFreq) < 12.0,
             qPrintable(QString("ret=%1 expected~%2 (plot.w=%3)")
                            .arg(ret).arg(kFreq).arg(plot.width())));
}

// -----------------------------------------------------------------
// dB axis tests
// -----------------------------------------------------------------

void TestParametricEqAxis::dbAxisRoundtrip() {
    ParametricEqAxisTester w;
    w.resize(460, 540);
    QRect plot = w.computePlotRect();

    // Pick dB values inside default [-24, +24] range and round-trip.
    for (double db : {-23.0, -10.0, 0.0, 6.5, 18.0, 23.5}) {
        float  y    = w.yFromDb(plot, db);
        double back = w.dbFromY(plot, qRound(y));
        // 1 px of plot height maps to span/height dB.  For span=48 dB
        // over ~478 px that's ~0.1 dB/px; allow 0.2 dB to absorb qRound.
        QVERIFY2(qAbs(back - db) < 0.2,
                 qPrintable(QString("db=%1 back=%2 plot.h=%3")
                                .arg(db).arg(back).arg(plot.height())));
    }
}

// -----------------------------------------------------------------
// Custom log scale tests (centreRatio = 0.125 verbatim from C#).
// -----------------------------------------------------------------

void TestParametricEqAxis::logShapeAtDefaultCentre() {
    ParametricEqAxisTester w;

    // centreHz = 0 + 4000 * 0.125 = 500 Hz.
    QCOMPARE(w.getLogFrequencyCentreHz(0.0, 4000.0), 500.0);

    // shape = (1 - 2*0.125) / (0.125 * 0.125) = 0.75 / 0.015625 = 48.0.
    QCOMPARE(w.getLogFrequencyShape(0.125), 48.0);

    // Degenerate guards: r at endpoints or at exactly 0.5 -> linear (0).
    QCOMPARE(w.getLogFrequencyShape(0.0), 0.0);
    QCOMPARE(w.getLogFrequencyShape(1.0), 0.0);
    QCOMPARE(w.getLogFrequencyShape(0.5), 0.0);

    // r > 0.5 -> formula is negative, clipped to 0 (drop to linear).
    QCOMPARE(w.getLogFrequencyShape(0.75), 0.0);
}

void TestParametricEqAxis::logFrequencyAtNormalisedHalf() {
    ParametricEqAxisTester w;
    w.setLogScale(true);

    // At t=0.5 with the default custom log (shape=48 over [0, 4000]),
    // u = (exp(0.5*log(49))-1) / 48 = (sqrt(49)-1)/48 = 6/48 = 0.125
    // -> freq = 0 + 0.125 * 4000 = 500 Hz.  This is the *centre* freq.
    double f = w.frequencyFromNormalizedPosition(0.5, 0.0, 4000.0, /*useLog=*/true);
    QVERIFY2(qAbs(f - 500.0) < 1e-6,
             qPrintable(QString("expected 500, got %1").arg(f, 0, 'g', 17)));

    // Round-trip: at the centre freq, normalised position must be 0.5.
    double t = w.getNormalizedFrequencyPosition(500.0, 0.0, 4000.0, /*useLog=*/true);
    QVERIFY2(qAbs(t - 0.5) < 1e-9,
             qPrintable(QString("expected 0.5, got %1").arg(t, 0, 'g', 17)));

    // useLog=false at t=0.5 -> midpoint freq = 2000 Hz (linear).
    double fLin = w.frequencyFromNormalizedPosition(0.5, 0.0, 4000.0, /*useLog=*/false);
    QCOMPARE(fLin, 2000.0);
}

// -----------------------------------------------------------------
// chooseDbStep / chooseFrequencyStep -- pin every branch.
// -----------------------------------------------------------------

void TestParametricEqAxis::dbStepSwitches() {
    ParametricEqAxisTester w;
    QCOMPARE(w.chooseDbStep(  2.5),  0.5);
    QCOMPARE(w.chooseDbStep(  3.0),  0.5);   // boundary
    QCOMPARE(w.chooseDbStep(  6.0),  1.0);
    QCOMPARE(w.chooseDbStep( 12.0),  2.0);
    QCOMPARE(w.chooseDbStep( 24.0),  3.0);
    QCOMPARE(w.chooseDbStep( 48.0),  6.0);
    QCOMPARE(w.chooseDbStep( 96.0), 12.0);
    QCOMPARE(w.chooseDbStep(200.0), 24.0);
}

void TestParametricEqAxis::freqStepSwitches() {
    ParametricEqAxisTester w;
    QCOMPARE(w.chooseFrequencyStep(   300.0),   25.0);
    QCOMPARE(w.chooseFrequencyStep(   600.0),   50.0);
    QCOMPARE(w.chooseFrequencyStep(  1200.0),  100.0);
    QCOMPARE(w.chooseFrequencyStep(  2500.0),  250.0);
    QCOMPARE(w.chooseFrequencyStep(  6000.0),  500.0);
    QCOMPARE(w.chooseFrequencyStep( 12000.0), 1000.0);
    QCOMPARE(w.chooseFrequencyStep( 24000.0), 2000.0);
    QCOMPARE(w.chooseFrequencyStep( 50000.0), 5000.0);
}

// -----------------------------------------------------------------
// resetPointsDefault: 10 evenly-spaced bands, q=4, gain=0, endpoints pinned.
// -----------------------------------------------------------------

void TestParametricEqAxis::resetProducesTenEvenBands() {
    ParametricEqAxisTester w;       // ctor calls resetPointsDefault()

    QCOMPARE(w.pointsConst().size(), w.bandCount());
    QCOMPARE(w.pointsConst().size(), 10);

    for (int i = 0; i < w.pointsConst().size(); ++i) {
        const auto& p = w.pointsConst().at(i);
        QCOMPARE(p.q, 4.0);
        QCOMPARE(p.gainDb, 0.0);
        QCOMPARE(p.bandId, i + 1);   // bandIds 1..10
    }
}

void TestParametricEqAxis::resetEndpointsLockedToRange() {
    ParametricEqAxisTester w;

    QVERIFY(w.pointsConst().size() >= 2);
    // Endpoints are pinned by enforceOrdering at the end of reset.
    QCOMPARE(w.pointsConst().front().frequencyHz, 0.0);    // m_frequencyMinHz
    QCOMPARE(w.pointsConst().back().frequencyHz,  4000.0); // m_frequencyMaxHz

    // isFrequencyLockedIndex agrees: index 0 and last are locked.
    QVERIFY( w.isFrequencyLockedIndex(0));
    QVERIFY( w.isFrequencyLockedIndex(w.pointsConst().size() - 1));
    QVERIFY(!w.isFrequencyLockedIndex(1));
    QVERIFY(!w.isFrequencyLockedIndex(w.pointsConst().size() - 2));

    // getLockedFrequencyForIndex returns range endpoints at the ends.
    QCOMPARE(w.getLockedFrequencyForIndex(0), 0.0);
    QCOMPARE(w.getLockedFrequencyForIndex(w.pointsConst().size() - 1), 4000.0);
}

// -----------------------------------------------------------------
// enforceOrdering: shove a band past its neighbour, sort fires, the
// previously-selected band is still selected (re-resolved by bandId).
// -----------------------------------------------------------------

void TestParametricEqAxis::enforceOrderingSortsByFreq() {
    ParametricEqAxisTester w;
    QVERIFY(w.pointsConst().size() == 10);

    // After reset, freqs are: 0, 444.4..., 888.9..., 1333.3..., 1777.8...,
    //                         2222.2..., 2666.7..., 3111.1..., 3555.6..., 4000.
    // Pre-select bandId=4 (currently at index 3, freq ~1333.3).
    w.setSelectedIndex(3);
    QCOMPARE(w.selectedIndex(), 3);
    QCOMPARE(w.pointsConst().at(3).bandId, 4);

    // Shove its freq up to 3500 (past indices 4,5,6,7 -> would land at 7).
    w.pointsMut()[3].frequencyHz = 3500.0;

    // Verify out-of-order before enforceOrdering.
    QVERIFY(w.pointsConst().at(3).frequencyHz > w.pointsConst().at(4).frequencyHz);

    w.enforceOrdering(/*enforceSpacingAll=*/true);

    // Post-sort: ascending by frequency.
    for (int i = 1; i < w.pointsConst().size(); ++i) {
        QVERIFY2(w.pointsConst().at(i - 1).frequencyHz
                 <= w.pointsConst().at(i).frequencyHz,
                 qPrintable(QString("not sorted at i=%1: %2 then %3")
                                .arg(i)
                                .arg(w.pointsConst().at(i - 1).frequencyHz)
                                .arg(w.pointsConst().at(i).frequencyHz)));
    }

    // bandId=4 was re-resolved to its new index by enforceOrdering.
    int newIdx = w.indexFromBandId(4);
    QVERIFY2(newIdx > 0 && newIdx < w.pointsConst().size(),
             qPrintable(QString("bandId 4 not found after sort: idx=%1").arg(newIdx)));
    QCOMPARE(w.selectedIndex(), newIdx);

    // Endpoints still pinned (interior shove must not unpin them).
    QCOMPARE(w.pointsConst().front().frequencyHz, 0.0);
    QCOMPARE(w.pointsConst().back().frequencyHz,  4000.0);
    QCOMPARE(w.pointsConst().front().bandId, 1);
    QCOMPARE(w.pointsConst().back().bandId,  10);
}

void TestParametricEqAxis::editorPresentationContract() {
    ParametricEqAxisTester a, b;
    a.resize(800, 400); b.resize(800, 400);
    a.setDbMin(0); a.setDbMax(16);
    b.setDbMin(-24); b.setDbMax(24);
    a.setMinimumPlotGutters(100, 50); b.setMinimumPlotGutters(100, 50);
    QCOMPARE(a.plotRect().left(), b.plotRect().left());
    QCOMPARE(a.plotRect().right(), b.plotRect().right());
}

QTEST_MAIN(TestParametricEqAxis)
#include "tst_parametric_eq_widget_axis.moc"
