// =================================================================
// tests/tst_dexp_peak_meter.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original unit-test file.  Cites for the
// paint behavior under test live in DexpPeakMeter.cpp.
//
// Phase 3M-3a-iii Task 13 — Unit tests for DexpPeakMeter widget.
// Covers:
//   1. Default-construct + render does not crash.
//   2. Out-of-range signal levels clamp to [0, 1] (verified via
//      the "1.5 -> full-width green" pixel inspection).
//   3. Green peak fill paints from left edge to signal_x.
//   4. Red threshold marker (1 px vertical line) paints at
//      threshold_x even when there is no signal.
//   5. Red above-threshold overlay paints from threshold+1 to
//      signal when signal > threshold (mirrors picVOX_Paint:
//      28958-28959 [v2.10.3.13]).
//
// Tests use QImage pixel inspection at known x positions.  Uses
// QTEST_MAIN (not QTEST_APPLESS_MAIN) because QWidget rendering
// requires a live QApplication.
//
// =================================================================

#include "gui/widgets/DexpPeakMeter.h"

#include <QImage>
#include <QPainter>
#include <QtTest/QtTest>

using namespace NereusSDR;

class TstDexpPeakMeter : public QObject {
    Q_OBJECT
private slots:

    // Default state must be paint-safe — caller must be able to
    // construct the widget and immediately paint without setting
    // either signal or threshold.
    void defaultState_signalAndThresholdAreSafe()
    {
        DexpPeakMeter meter;
        meter.resize(meter.sizeHint());
        QImage img(meter.size(), QImage::Format_RGB32);
        img.fill(Qt::black);
        QPainter p(&img);
        meter.render(&p);
        // Reaching this point without a crash IS the assertion.
        QVERIFY(true);
    }

    // setSignalLevel(-0.5) -> clamps to 0.0 -> no green fill.
    // setSignalLevel( 1.5) -> clamps to 1.0 -> full-width green.
    void setSignalLevel_clampsToRange()
    {
        DexpPeakMeter meter;
        meter.resize(100, 4);

        // Negative input: should clamp to 0 -> column at x=10 stays
        // background (no green).
        meter.setSignalLevel(-0.5);
        meter.setThresholdMarker(1.0);   // park threshold off-screen-right
        QImage low(meter.size(), QImage::Format_RGB32);
        low.fill(Qt::black);
        {
            QPainter p(&low);
            meter.render(&p);
        }
        const QRgb pxLow = low.pixel(10, 2);
        QVERIFY2(qGreen(pxLow) < 100,
                 "negative signal must clamp to 0 -> column should not be green");

        // Above-1 input: should clamp to 1.0 -> column at x=90 is
        // green (well inside the full-width green fill).
        meter.setSignalLevel(1.5);
        QImage high(meter.size(), QImage::Format_RGB32);
        high.fill(Qt::black);
        {
            QPainter p(&high);
            meter.render(&p);
        }
        const QRgb pxHigh = high.pixel(90, 2);
        QVERIFY2(qGreen(pxHigh) > 200 && qRed(pxHigh) < 100,
                 ">1.0 signal must clamp to 1.0 -> column should be green");
    }

    // Green fill — signal=0.5, threshold=0.8 -> threshold sits past
    // signal, so no red overlay paints.  Pixel at x=10 must be green.
    void paintShowsGreenPeakBar()
    {
        DexpPeakMeter meter;
        meter.resize(100, 4);
        meter.setSignalLevel(0.5);
        meter.setThresholdMarker(0.8);
        QImage img(meter.size(), QImage::Format_RGB32);
        img.fill(Qt::black);
        QPainter p(&img);
        meter.render(&p);
        const QRgb px = img.pixel(10, 2);
        QVERIFY2(qGreen(px) > 200,
                 "x=10 should be in the green peak fill (signal=0.5)");
        QVERIFY2(qRed(px)   < 100,
                 "x=10 should NOT be red (threshold sits past signal)");
    }

    // Red threshold marker — even with signal=0, the 1 px red
    // vertical line must render at threshold_x=50.
    void paintShowsRedThresholdMarker()
    {
        DexpPeakMeter meter;
        meter.resize(100, 4);
        meter.setSignalLevel(0.0);     // no peak fill at all
        meter.setThresholdMarker(0.5);
        QImage img(meter.size(), QImage::Format_RGB32);
        img.fill(Qt::black);
        QPainter p(&img);
        meter.render(&p);
        const QRgb px = img.pixel(50, 1);
        QVERIFY2(qRed(px) > 200,
                 "x=50 should be the red threshold marker line");
    }

    // Red above-threshold overlay — when signal>threshold, the
    // segment from threshold+1 to signal repaints in red.
    // Mirrors picVOX_Paint:28958-28959 [v2.10.3.13]:
    //   if (vox_x < peak_x)
    //       FillRectangle(Red, vox_x+1, 0, peak_x - vox_x - 1, H);
    void paintShowsRedOverlayAboveThreshold()
    {
        DexpPeakMeter meter;
        meter.resize(100, 4);
        meter.setSignalLevel(0.8);
        meter.setThresholdMarker(0.4);
        QImage img(meter.size(), QImage::Format_RGB32);
        img.fill(Qt::black);
        QPainter p(&img);
        meter.render(&p);
        // x=60 sits between threshold(40) and signal(80) -> red.
        const QRgb px = img.pixel(60, 1);
        QVERIFY2(qRed(px) > 200,
                 "x=60 should be in the red above-threshold overlay");
        QVERIFY2(qGreen(px) < 100,
                 "x=60 should NOT be green (above the threshold)");
    }
};

QTEST_MAIN(TstDexpPeakMeter)
#include "tst_dexp_peak_meter.moc"
