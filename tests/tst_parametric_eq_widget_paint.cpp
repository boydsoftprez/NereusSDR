// =================================================================
// tests/tst_parametric_eq_widget_paint.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.  Cites for the math
// and paint behaviour under test live in ParametricEqWidget.cpp.
// =================================================================
//
// Phase 3M-3a-ii follow-up Batch 3 -- paintEvent + draw helpers +
// responseDbAtFrequency + bar-chart timer/decay verification.
//
// Tests render the widget to a QImage so we can assert pixel-level
// behaviour (background colour, zero-dB grid line presence) without
// flakiness from font rendering.  The math tests (responseDb...,
// bar chart clamp/decay) drive the helpers directly via friend-class
// access for precise hand-computed expecteds.
//
// =================================================================

#include "../src/gui/widgets/ParametricEqWidget.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <QVector>

#include <cmath>

// Tester shim: friended in ParametricEqWidget.h so this subclass can
// reach private paint helpers, math, bar-chart state, and member fields.
class ParametricEqPaintTester : public NereusSDR::ParametricEqWidget {
public:
    using NereusSDR::ParametricEqWidget::ParametricEqWidget;

    // Math under test.
    using NereusSDR::ParametricEqWidget::responseDbAtFrequency;
    using NereusSDR::ParametricEqWidget::computePlotRect;
    using NereusSDR::ParametricEqWidget::yFromDb;

    // Bar-chart helpers we want to drive directly.
    using NereusSDR::ParametricEqWidget::applyBarChartPeakDecay;

    // Member access for hand-computing expecteds.
    QVector<EqPoint>& pointsMut()                       { return m_points; }
    const QVector<EqPoint>& pointsConst()       const   { return m_points; }
    const QVector<double>& barChartData()       const   { return m_barChartData; }
    const QVector<double>& barChartPeakData()   const   { return m_barChartPeakData; }
    QVector<qint64>& barChartPeakHoldUntilMs()          { return m_barChartPeakHoldUntilMs; }
    qint64& barChartPeakLastUpdateMs()                  { return m_barChartPeakLastUpdateMs; }

    int    barChartPeakHoldMs()             const   { return m_barChartPeakHoldMs; }
    void   setBarChartPeakHoldMs(int ms)            { m_barChartPeakHoldMs = ms; }
    void   setBarChartPeakHoldEnabled(bool on)      { m_barChartPeakHoldEnabled = on; }

    void   setParametric(bool on)                   { m_parametricEq = on; }
    void   setFrequencyRange(double lo, double hi)  { m_frequencyMinHz = lo; m_frequencyMaxHz = hi; }
    void   setDbRange(double lo, double hi)         { m_dbMin = lo; m_dbMax = hi; }
    double dbMin()                          const   { return m_dbMin; }
    double dbMax()                          const   { return m_dbMax; }
    double frequencyMinHz()                 const   { return m_frequencyMinHz; }
    double frequencyMaxHz()                 const   { return m_frequencyMaxHz; }
    double qMin()                           const   { return m_qMin; }
    double qMax()                           const   { return m_qMax; }

    // Replace the m_points list with a single point at (f, gain, q).
    // Used by the responseDb* tests.
    void setSinglePoint(double f, double gain, double q) {
        m_points.clear();
        EqPoint p;
        p.bandId = 1;
        p.frequencyHz = f;
        p.gainDb = gain;
        p.q = q;
        m_points.append(p);
    }

    // Render the widget to a QImage and return it.  Caller owns nothing;
    // the QImage is returned by value.
    QImage paintToImage(int w = 460, int h = 320) {
        resize(w, h);
        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(Qt::magenta);   // sentinel: anything left untouched is loud.
        render(&img);
        return img;
    }
};

class TestParametricEqPaint : public QObject {
    Q_OBJECT
private slots:
    void editorPresentationContract();
    // Paint smoke + background colour.
    void paintsWithoutCrash();
    void backgroundColorIsThetisDarkGrey();
    void zeroDbLineRenderedAtCorrectY();

    // responseDbAtFrequency math (parametric branch).
    void responseDbAtFrequencyZeroAtCenterWith0Gain();
    void responseDbAtFrequencyMatchesGaussianAt100Hz();
    void responseDbAtFrequencyAtCenterEqualsGain();

    // responseDbAtFrequency math (graphic-EQ branch).
    void responseDbGraphicEqLinearInterpolation();

    // Bar chart data flow.
    void barChartDataIsClampedToDbRange();
    void peakHoldDecaysOverTime();
};

// -----------------------------------------------------------------
// Paint smoke + background colour.
// -----------------------------------------------------------------

void TestParametricEqPaint::paintsWithoutCrash() {
    ParametricEqPaintTester w;
    QImage img = w.paintToImage(460, 320);
    QVERIFY(!img.isNull());
    QCOMPARE(img.width(),  460);
    QCOMPARE(img.height(), 320);
}

void TestParametricEqPaint::backgroundColorIsThetisDarkGrey() {
    ParametricEqPaintTester w;
    QImage img = w.paintToImage(460, 320);

    // BackColor at ucParametricEq.cs:443 is RGB(25,25,25).  The very top-left
    // pixel of the client rect sits in the margin above the plot rect, so
    // it must show the widget background (not the slightly-darker plot
    // interior at RGB(18,18,18)).
    QColor px = img.pixelColor(0, 0);
    QCOMPARE(px, QColor(25, 25, 25, 255));

    // Same for the bottom-right corner -- below the plot, in the readout
    // strip, also widget background.
    QColor cornerBR = img.pixelColor(img.width() - 1, img.height() - 1);
    QCOMPARE(cornerBR, QColor(25, 25, 25, 255));
}

void TestParametricEqPaint::zeroDbLineRenderedAtCorrectY() {
    ParametricEqPaintTester w;

    // Default reset gives 10 points all at gain=0, so the white response
    // curve sits exactly on the zero-dB line and obscures the grey grid
    // pixel.  Push a single-point setup with high gain so the curve
    // bows away from zero across most of the X axis, leaving the grey
    // zero-dB line visible at the plot edges and middle.
    w.setSinglePoint(/*f=*/2000.0, /*gain=*/20.0, /*q=*/8.0);

    QImage img = w.paintToImage(460, 320);

    QRect plot = w.computePlotRect();
    int y0 = qRound(double(w.yFromDb(plot, 0.0)));
    QVERIFY2(y0 > plot.top() && y0 < plot.bottom(),
             qPrintable(QString("y0=%1 not in plot [%2..%3]")
                            .arg(y0).arg(plot.top()).arg(plot.bottom())));

    // The zero-dB grid line is RGB(75,75,75) at 1.5 px width drawn from
    // plot.left() to plot.right().  Sample a column near the *left edge*
    // of the plot, which is far from the centred curve peak (at f=2000),
    // so the curve has decayed back close to zero.  Wait -- that would
    // put the curve right back on the zero line.  Sample instead at a
    // X-column 30 px in from plot.left(): with q=8 and centre=2000,
    // sigma is small (~141 Hz), so by the time we're 1.7 kHz off-centre
    // the Gaussian weight is essentially zero and the curve has dropped
    // back to ~0 dB.  Use a row of small but discernible *vertical*
    // search instead and look for the brighter grey within the dim plot.
    //
    // Pick X near the right edge instead -- same logic, but on the
    // right side, the curve is also near zero by ~3.5 kHz.
    //
    // Actually, the most reliable approach: scan a wide span of pixels
    // across the row at y0 and confirm AT LEAST one is the grey line
    // colour.  Most columns will be (18,18,18) plot-interior or (45,45,45)
    // regular grid lines (vertical).  The zero-dB horizontal line
    // contributes a brighter (~75,75,75) pixel everywhere it's drawn,
    // EXCEPT where the curve overlaps it.  With curve centred at
    // x(2000Hz), the curve sits ~50-100 px wide; zero-dB pixels exist
    // outside that band.
    bool foundBrightRow = false;
    int midX = plot.left() + plot.width() / 2;
    QString diag;
    for (int dx = -plot.width() / 2 + 5;
              dx <= plot.width() / 2 - 5;
              dx += 4)
    {
        int x = midX + dx;
        for (int dy = -2; dy <= 2; ++dy) {
            int y = y0 + dy;
            if (y < plot.top() || y > plot.bottom()) continue;
            QColor px = img.pixelColor(x, y);
            int r = px.red(), gp = px.green(), b = px.blue();
            if (qAbs(r - gp) > 3 || qAbs(gp - b) > 3) continue;
            // The 1.5px-wide RGB(75,75,75) line antialiases to ~55-95.
            if (r >= 55 && r <= 110) {
                foundBrightRow = true;
                diag = QString("found at x=%1 y=%2 px=(%3,%4,%5)")
                           .arg(x).arg(y).arg(r).arg(gp).arg(b);
                break;
            }
        }
        if (foundBrightRow) break;
    }
    QVERIFY2(foundBrightRow,
             qPrintable(QString("No bright zero-dB grid pixel found at y~=%1 "
                                "across plot width. Diag:%2")
                            .arg(y0).arg(diag)));
}

// -----------------------------------------------------------------
// responseDbAtFrequency -- parametric Gaussian sum.
// -----------------------------------------------------------------

void TestParametricEqPaint::responseDbAtFrequencyZeroAtCenterWith0Gain() {
    ParametricEqPaintTester w;
    w.setParametric(true);
    w.setSinglePoint(/*f=*/1000.0, /*gain=*/0.0, /*q=*/4.0);

    // With p.gainDb = 0, the Gaussian-weighted sum at the center is
    // 0 * exp(0) = 0.  At any other frequency it's still 0 * w = 0.
    double r = w.responseDbAtFrequency(1000.0);
    QCOMPARE(r, 0.0);
    double r2 = w.responseDbAtFrequency(500.0);
    QCOMPARE(r2, 0.0);
}

void TestParametricEqPaint::responseDbAtFrequencyMatchesGaussianAt100Hz() {
    ParametricEqPaintTester w;
    w.setParametric(true);
    w.setFrequencyRange(0.0, 4000.0);  // span = 4000 (default in ctor too)
    w.setSinglePoint(/*f=*/100.0, /*gain=*/6.0, /*q=*/4.0);

    // Hand-compute Gaussian: span/(q*3) = 4000/12 = 333.333..., min=4000/6000=0.6667
    // -> fwhm = 333.3333..., sigma = fwhm / 2.3548200450309493
    constexpr double kFwhmToSigma = 2.3548200450309493;
    const double span  = 4000.0;
    const double fwhm  = span / (4.0 * 3.0);   // 333.33...
    const double sigma = fwhm / kFwhmToSigma;
    const double f     = 200.0;                // sample point
    const double centerHz = 100.0;
    const double gain     = 6.0;

    const double d = (f - centerHz) / sigma;
    const double w_ = std::exp(-0.5 * d * d);
    const double expected = gain * w_;

    double got = w.responseDbAtFrequency(f);
    QVERIFY2(std::fabs(got - expected) < 1e-9,
             qPrintable(QString("got=%1 expected=%2 sigma=%3")
                            .arg(got, 0, 'g', 17)
                            .arg(expected, 0, 'g', 17)
                            .arg(sigma)));
}

void TestParametricEqPaint::responseDbAtFrequencyAtCenterEqualsGain() {
    ParametricEqPaintTester w;
    w.setParametric(true);
    w.setSinglePoint(/*f=*/1500.0, /*gain=*/-3.5, /*q=*/2.0);

    // At f == p.frequencyHz, d=0 and exp(0)=1.0, so sum = p.gainDb.
    double r = w.responseDbAtFrequency(1500.0);
    QCOMPARE(r, -3.5);
}

// -----------------------------------------------------------------
// responseDbAtFrequency -- graphic-EQ branch.
// -----------------------------------------------------------------

void TestParametricEqPaint::responseDbGraphicEqLinearInterpolation() {
    ParametricEqPaintTester w;
    w.setParametric(false);
    // Two points: 0 Hz @ 0 dB,  4000 Hz @ +12 dB.
    w.pointsMut().clear();
    NereusSDR::ParametricEqWidget::EqPoint p0;
    p0.bandId = 1; p0.frequencyHz = 0.0;    p0.gainDb = 0.0;
    NereusSDR::ParametricEqWidget::EqPoint p1;
    p1.bandId = 2; p1.frequencyHz = 4000.0; p1.gainDb = 12.0;
    w.pointsMut().append(p0);
    w.pointsMut().append(p1);

    // Graphic-EQ branch is linear interp between adjacent points.  At
    // f=1000 (1/4 of the way), gain = 0 + 0.25*12 = 3.0.
    QCOMPARE(w.responseDbAtFrequency(1000.0), 3.0);
    // At f=2000 (1/2): gain = 6.0.
    QCOMPARE(w.responseDbAtFrequency(2000.0), 6.0);
    // Below first point: clamped to first.
    QCOMPARE(w.responseDbAtFrequency(-100.0), 0.0);
    // Above last point: clamped to last.
    QCOMPARE(w.responseDbAtFrequency(5000.0), 12.0);
}

// -----------------------------------------------------------------
// Bar chart data flow.
// -----------------------------------------------------------------

void TestParametricEqPaint::barChartDataIsClampedToDbRange() {
    ParametricEqPaintTester w;
    // Defaults: dbMin=-24, dbMax=+24.
    QCOMPARE(w.dbMin(), -24.0);
    QCOMPARE(w.dbMax(),  24.0);

    QVector<double> data { 100.0, -100.0, 0.0, 12.0 };
    w.drawBarChartData(data);

    QCOMPARE(w.barChartData().size(), 4);
    QCOMPARE(w.barChartData().at(0),  24.0);   // 100 -> clamp to +24
    QCOMPARE(w.barChartData().at(1), -24.0);   // -100 -> clamp to -24
    QCOMPARE(w.barChartData().at(2),   0.0);
    QCOMPARE(w.barChartData().at(3),  12.0);

    // Peaks initialised on first push: should equal clamped data.
    QCOMPARE(w.barChartPeakData().size(), 4);
    QCOMPARE(w.barChartPeakData().at(0),  24.0);
    QCOMPARE(w.barChartPeakData().at(1), -24.0);
    QCOMPARE(w.barChartPeakData().at(2),   0.0);
    QCOMPARE(w.barChartPeakData().at(3),  12.0);
}

void TestParametricEqPaint::peakHoldDecaysOverTime() {
    ParametricEqPaintTester w;
    w.setBarChartPeakHoldEnabled(true);
    w.setBarChartPeakHoldMs(100);   // short hold so we can decay quickly.

    // Step 1: push high -> bar=peak=10.
    QVector<double> hi { 10.0 };
    w.drawBarChartData(hi);
    QCOMPARE(w.barChartPeakData().at(0), 10.0);

    // Step 2: push low -> bar drops to 2, peak still 10 (hold-until set on
    // previous push; v=2 < peak=10 so neither "v >= peak" nor "peak < v"
    // updates the peak).
    QVector<double> lo { 2.0 };
    w.drawBarChartData(lo);
    QCOMPARE(w.barChartData().at(0), 2.0);
    QCOMPARE(w.barChartPeakData().at(0), 10.0);

    // Step 3: backdate the hold-until (so it has expired) AND the last
    // update timestamp (so elapsed seconds is non-trivial), then call
    // applyBarChartPeakDecay directly.  The decay path uses the *current*
    // bar value as the floor: floor = clamp(m_barChartData[0], -24, +24)
    // = 2.  Peak is currently 10, hold-expired, so:
    //   elapsedSeconds = 0.200
    //   decayDb        = 20 * 0.200 = 4.0
    //   newPeak        = 10 - 4 = 6.0  (floor=2, 6 >= 2 so no clamp)
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    w.barChartPeakHoldUntilMs()[0] = nowMs - 1;        // hold already expired
    w.barChartPeakLastUpdateMs()   = nowMs - 200;      // 0.2 s elapsed -> 4 dB decay
    w.applyBarChartPeakDecay(nowMs);

    QVERIFY2(w.barChartPeakData().at(0) < 10.0,
             qPrintable(QString("peak should have decayed below 10, got %1")
                            .arg(w.barChartPeakData().at(0))));
    QVERIFY2(w.barChartPeakData().at(0) > 2.0,
             qPrintable(QString("peak should still exceed floor (2), got %1")
                            .arg(w.barChartPeakData().at(0))));
    // Hand-pinned: 10 - (20 * 0.2) = 6.0 exact (peakDecayDbPerSecond default = 20.0).
    QVERIFY2(std::fabs(w.barChartPeakData().at(0) - 6.0) < 1e-6,
             qPrintable(QString("expected peak ~= 6.0 after 0.2s decay, got %1")
                            .arg(w.barChartPeakData().at(0), 0, 'g', 17)));
}

void TestParametricEqPaint::editorPresentationContract() {
    ParametricEqPaintTester w;
    w.setEditorPresentationEnabled(true);
    w.setPointData(4, w.points().at(4).frequencyHz, -8.0, 4.0);
    w.setSelectedIndex(4);
    const QImage image = w.paintToImage();
    QCOMPARE(image.pixelColor(0, 0), QColor("#0f0f1a"));
    const QString capture = qEnvironmentVariable("NEREUS_EQ_EDITOR_CAPTURE");
    if (!capture.isEmpty()) { QVERIFY(image.save(capture)); }
    w.setEditorPresentationEnabled(false);
    QCOMPARE(w.paintToImage().pixelColor(0, 0), QColor(25,25,25));
}

QTEST_MAIN(TestParametricEqPaint)
#include "tst_parametric_eq_widget_paint.moc"
