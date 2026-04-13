// tst_meter_item_bar.cpp — BarItem API parity with Thetis clsBarItem.
//
// Covers the meter-parity-audit gaps in BarItem (Phase A of the port plan):
//   A1: ShowValue / ShowPeakValue / FontColour + peak tracking
//   A2: ShowHistory / HistoryColour / HistoryDuration
//   A3: ShowMarker / MarkerColour / PeakHoldMarkerColour
//   A4: BarStyle::Line + non-linear ScaleCalibration
//
// Thetis source of truth:
//   MeterManager.cs:19917-20278 (clsBarItem class definition)
//   MeterManager.cs:35950-36140 (renderHBar SolidFilled/Line branches)
//   MeterManager.cs:21499-21616 (addSMeterBar — canonical Line-style caller)

#include <QtTest/QtTest>
#include <QColor>
#include <QImage>
#include <QPainter>

#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding namespace

using namespace NereusSDR;

class TstMeterItemBar : public QObject
{
    Q_OBJECT

private slots:
    // ---- Phase A1: ShowValue / ShowPeakValue / FontColour ----

    void showValue_default_is_false()
    {
        BarItem b;
        QCOMPARE(b.showValue(), false);
        QCOMPARE(b.showPeakValue(), false);
    }

    void showValue_and_showPeakValue_roundtrip()
    {
        BarItem b;
        b.setShowValue(true);
        b.setShowPeakValue(true);
        QCOMPARE(b.showValue(), true);
        QCOMPARE(b.showPeakValue(), true);
    }

    void fontColour_roundtrip()
    {
        BarItem b;
        const QColor yellow(0xff, 0xff, 0x00);
        b.setFontColour(yellow);
        QCOMPARE(b.fontColour(), yellow);
    }

    void setValue_tracks_peak_high_water_mark()
    {
        // From Thetis clsBarItem — peak is the highest value seen,
        // used by ShowPeakValue text and the peak-hold marker.
        BarItem b;
        b.setRange(-140.0, 0.0);
        b.setValue(-80.0);
        b.setValue(-50.0);   // new peak
        b.setValue(-70.0);   // lower — peak should hold
        QVERIFY(b.peakValue() >= -50.0);
    }

    // ---- Phase A2: ShowHistory / HistoryColour / HistoryDuration ----

    void showHistory_default_is_false()
    {
        BarItem b;
        QCOMPARE(b.showHistory(), false);
    }

    void showHistory_and_historyColour_roundtrip()
    {
        // Thetis addSMeterBar default: HistoryColour = Color.FromArgb(128, Red)
        // (MeterManager.cs:21545).
        BarItem b;
        b.setShowHistory(true);
        const QColor red128(255, 0, 0, 128);
        b.setHistoryColour(red128);
        QCOMPARE(b.showHistory(), true);
        QCOMPARE(b.historyColour(), red128);
    }

    void historyDuration_default_matches_thetis_4000ms()
    {
        // Thetis addSMeterBar / AddADCMaxMag both set HistoryDuration = 4000
        // (MeterManager.cs:21539, 21633). Use that as the NereusSDR default.
        BarItem b;
        QCOMPARE(b.historyDurationMs(), 4000);
    }

    void historyDuration_roundtrip()
    {
        BarItem b;
        b.setHistoryDurationMs(2500);
        QCOMPARE(b.historyDurationMs(), 2500);
    }

    void history_samples_accumulate_when_enabled()
    {
        // With ShowHistory=true, setValue() should push samples into a
        // ring buffer so the render pass can draw the trailing trace.
        // The buffer length is bounded by HistoryDuration + update rate —
        // we only assert non-zero on first few pushes here.
        BarItem b;
        b.setRange(-140.0, 0.0);
        b.setShowHistory(true);
        b.setValue(-90.0);
        b.setValue(-80.0);
        b.setValue(-70.0);
        QVERIFY(b.historySampleCount() >= 3);
    }

    void history_samples_dont_accumulate_when_disabled()
    {
        BarItem b;
        b.setRange(-140.0, 0.0);
        // ShowHistory defaults to false
        b.setValue(-90.0);
        b.setValue(-80.0);
        QCOMPARE(b.historySampleCount(), 0);
    }

    // ---- Phase A3: ShowMarker / MarkerColour / PeakHoldMarkerColour ----

    void showMarker_default_is_false()
    {
        BarItem b;
        QCOMPARE(b.showMarker(), false);
    }

    void showMarker_roundtrip()
    {
        // addSMeterBar sets ShowMarker = true (MeterManager.cs:21543).
        BarItem b;
        b.setShowMarker(true);
        QCOMPARE(b.showMarker(), true);
    }

    void markerColour_roundtrip()
    {
        BarItem b;
        const QColor orange(0xff, 0xa5, 0x00);
        b.setMarkerColour(orange);
        QCOMPARE(b.markerColour(), orange);
    }

    void peakHoldMarkerColour_roundtrip()
    {
        // addSMeterBar sets PeakHoldMarkerColour = Red
        // (MeterManager.cs:21544).
        BarItem b;
        const QColor red(0xff, 0x00, 0x00);
        b.setPeakHoldMarkerColour(red);
        QCOMPARE(b.peakHoldMarkerColour(), red);
    }

    void peakHold_decays_when_rate_is_nonzero()
    {
        // Thetis clsBarItem applies an independent decay to the peak-hold
        // marker value so it slowly drops back toward the live value.
        // With a decay ratio > 0, a subsequent lower setValue() should
        // nudge peakValue() downward from the prior high.
        BarItem b;
        b.setRange(-140.0, 0.0);
        b.setPeakHoldDecayRatio(0.5f);
        b.setValue(-50.0);           // peak = -50
        const double peakAfterHigh = b.peakValue();
        b.setValue(-80.0);           // decay kicks in
        QVERIFY2(b.peakValue() < peakAfterHigh,
                 "peakValue should decay toward live value when "
                 "PeakHoldDecayRatio > 0");
        QVERIFY2(b.peakValue() > -80.0,
                 "peak should not collapse all the way to the live value "
                 "in one step");
    }

    void peakHold_holds_when_rate_is_zero()
    {
        // Default behavior (A1) — no decay. Rate 0 means the marker sticks
        // at the highest value forever, matching the A1 test.
        BarItem b;
        b.setRange(-140.0, 0.0);
        b.setPeakHoldDecayRatio(0.0f);
        b.setValue(-40.0);
        b.setValue(-70.0);
        QCOMPARE(b.peakValue(), -40.0);
    }

    // ---- Phase A4: BarStyle::Line + ScaleCalibration non-linear map ----

    void barStyle_Line_roundtrip()
    {
        // Thetis clsBarItem.BarStyle enum (MeterManager.cs:19927-19934)
        // has None, Line, SolidFilled, GradientFilled, Segments. addSMeterBar
        // uses BarStyle.Line (MeterManager.cs:21546). NereusSDR already has
        // Filled + Edge; A4 adds Line.
        BarItem b;
        b.setBarStyle(BarItem::BarStyle::Line);
        QCOMPARE(b.barStyle(), BarItem::BarStyle::Line);
    }

    void scaleCalibration_defaults_empty()
    {
        BarItem b;
        QCOMPARE(b.scaleCalibrationSize(), 0);
    }

    void scaleCalibration_add_and_size()
    {
        BarItem b;
        b.addScaleCalibration(-133.0, 0.0f);
        b.addScaleCalibration(-73.0, 0.5f);
        b.addScaleCalibration(-13.0, 0.99f);
        QCOMPARE(b.scaleCalibrationSize(), 3);
    }

    void scaleCalibration_interpolates_thetis_SMeter_curve()
    {
        // From Thetis addSMeterBar (MeterManager.cs:21547-21549):
        //   -133 dBm -> x = 0.0    (S0 / bottom of scale)
        //   -73 dBm  -> x = 0.5    (S9)
        //   -13 dBm  -> x = 0.99   (S9 + 60 dB)
        // These are NON-LINEAR waypoints. A calibrated BarItem must
        // interpolate through them rather than doing linear min..max.
        BarItem b;
        b.addScaleCalibration(-133.0, 0.0f);
        b.addScaleCalibration(-73.0, 0.5f);
        b.addScaleCalibration(-13.0, 0.99f);

        // Exact waypoints
        QCOMPARE(b.valueToNormalizedX(-133.0), 0.0f);
        QCOMPARE(b.valueToNormalizedX(-73.0), 0.5f);
        QCOMPARE(b.valueToNormalizedX(-13.0), 0.99f);

        // Midpoint of first segment: -133 .. -73, halfway is -103 dBm -> 0.25
        const float midSeg1 = b.valueToNormalizedX(-103.0);
        QVERIFY2(std::abs(midSeg1 - 0.25f) < 1e-3f,
                 "midpoint of first calibration segment should lie at x=0.25");

        // Midpoint of second segment: -73 .. -13, halfway is -43 dBm
        // -> midway between 0.5 and 0.99 = 0.745
        const float midSeg2 = b.valueToNormalizedX(-43.0);
        QVERIFY2(std::abs(midSeg2 - 0.745f) < 1e-3f,
                 "midpoint of second calibration segment should lie at x=0.745");

        // Below-range clamp
        QCOMPARE(b.valueToNormalizedX(-200.0), 0.0f);
        // Above-range clamp (use the last waypoint, 0.99)
        QCOMPARE(b.valueToNormalizedX(100.0), 0.99f);
    }

    void scaleCalibration_empty_falls_back_to_linear_range()
    {
        // With no calibration waypoints, BarItem should keep its
        // legacy linear min..max behavior so existing Filled presets
        // don't regress.
        BarItem b;
        b.setRange(-140.0, 0.0);
        QCOMPARE(b.valueToNormalizedX(-140.0), 0.0f);
        QCOMPARE(b.valueToNormalizedX(0.0), 1.0f);
        const float mid = b.valueToNormalizedX(-70.0);
        QVERIFY2(std::abs(mid - 0.5f) < 1e-3f,
                 "linear fallback midpoint should be 0.5");
    }

    // ---- Phase C: full tail-tolerant round-trip sweep ----

    void full_roundtrip_preserves_every_A_phase_field()
    {
        // Populate every field added in Phases A1-A4, serialize, parse
        // the result back, assert value equality. If any field is
        // dropped by serialize or misread by deserialize, this test
        // pinpoints which one.
        BarItem a;
        a.setRect(0.1f, 0.2f, 0.5f, 0.3f);
        a.setBindingId(MeterBinding::TxAlc);
        a.setZOrder(2);
        a.setOrientation(BarItem::Orientation::Horizontal);
        a.setRange(-30.0, 0.0);
        a.setBarColor(QColor(0x11, 0x22, 0x33));
        a.setBarRedColor(QColor(0xaa, 0xbb, 0xcc));
        a.setRedThreshold(-5.0);
        a.setAttackRatio(0.7f);
        a.setDecayRatio(0.15f);
        // Phase A1
        a.setShowValue(true);
        a.setShowPeakValue(true);
        a.setFontColour(QColor(0xff, 0xff, 0x00));
        // Phase A2
        a.setShowHistory(true);
        a.setHistoryColour(QColor(255, 0, 0, 128));
        a.setHistoryDurationMs(2500);
        // Phase A3
        a.setShowMarker(true);
        a.setMarkerColour(QColor(Qt::cyan));
        a.setPeakHoldMarkerColour(QColor(Qt::red));
        a.setPeakHoldDecayRatio(0.25f);
        // Phase A4
        a.setBarStyle(BarItem::BarStyle::Line);
        a.addScaleCalibration(-133.0, 0.0f);
        a.addScaleCalibration(-73.0, 0.5f);
        a.addScaleCalibration(-13.0, 0.99f);

        const QString blob = a.serialize();

        BarItem b;
        QVERIFY(b.deserialize(blob));

        QCOMPARE(b.showValue(),          true);
        QCOMPARE(b.showPeakValue(),      true);
        QCOMPARE(b.fontColour(),         QColor(0xff, 0xff, 0x00));
        QCOMPARE(b.showHistory(),        true);
        QCOMPARE(b.historyColour(),      QColor(255, 0, 0, 128));
        QCOMPARE(b.historyDurationMs(),  2500);
        QCOMPARE(b.showMarker(),         true);
        QCOMPARE(b.markerColour(),       QColor(Qt::cyan));
        QCOMPARE(b.peakHoldMarkerColour(), QColor(Qt::red));
        QCOMPARE(b.peakHoldDecayRatio(), 0.25f);
        QCOMPARE(b.barStyle(),           BarItem::BarStyle::Line);
        QCOMPARE(b.scaleCalibrationSize(), 3);
        QCOMPARE(b.valueToNormalizedX(-73.0), 0.5f);
    }

    void deserialize_pre_A1_legacy_BAR_payload()
    {
        // 20-field legacy BAR (through the A0 Edge mode fields) — the
        // shape every saved layout in the wild has today. Must load
        // cleanly and leave every A1-A4 field at its default.
        const QString legacy = QStringLiteral(
            "BAR|0.1|0.2|0.5|0.3|105|2|H|-30|0"
            "|#ff11bbff|#ffff4444|1000|0.8|0.2"
            "|Filled|#ff000000|#ffffffff|#ffff0000|#ffffff00");
        BarItem b;
        QVERIFY(b.deserialize(legacy));
        // Core fields parsed correctly
        QCOMPARE(b.bindingId(), MeterBinding::TxAlc);
        QCOMPARE(b.barStyle(),  BarItem::BarStyle::Filled);
        // A1-A4 defaults intact
        QCOMPARE(b.showValue(),          false);
        QCOMPARE(b.showPeakValue(),      false);
        QCOMPARE(b.showHistory(),        false);
        QCOMPARE(b.historyDurationMs(),  4000);
        QCOMPARE(b.showMarker(),         false);
        QCOMPARE(b.peakHoldDecayRatio(), 0.0f);
        QCOMPARE(b.scaleCalibrationSize(), 0);
    }

    // ---- Phase D1b: BarStyle::Line render path ----

    static int countMatching(const QImage& img, const QRect& region,
                             const QColor& wanted, int tol = 50)
    {
        int hits = 0;
        for (int y = region.top(); y <= region.bottom(); ++y) {
            for (int x = region.left(); x <= region.right(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (std::abs(c.red()   - wanted.red())   <= tol &&
                    std::abs(c.green() - wanted.green()) <= tol &&
                    std::abs(c.blue()  - wanted.blue())  <= tol) {
                    ++hits;
                }
            }
        }
        return hits;
    }

    void Line_style_paints_bar_using_scaleCalibration()
    {
        // A Line-style BarItem with the Thetis S-meter calibration and
        // value = -73 (S9, calibration midpoint = 0.5) should paint bar
        // pixels across the left half of the widget and leave the right
        // half unfilled.
        QImage img(200, 40, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            b.setBarStyle(BarItem::BarStyle::Line);
            b.setBarColor(QColor(Qt::cyan));
            b.addScaleCalibration(-133.0, 0.0f);
            b.addScaleCalibration( -73.0, 0.5f);
            b.addScaleCalibration( -13.0, 0.99f);
            // setValue applies attack smoothing — drive it hard so
            // smoothedValue reaches -73 quickly.
            for (int i = 0; i < 20; ++i) { b.setValue(-73.0); }
            b.paint(p, img.width(), img.height());
        }
        const QRect leftHalf(0, 0, img.width() / 2 - 5, img.height());
        const QRect rightQuarter(img.width() * 3 / 4, 0,
                                 img.width() / 4, img.height());
        const int cyanLeft  = countMatching(img, leftHalf, QColor(Qt::cyan), 60);
        const int cyanRight = countMatching(img, rightQuarter, QColor(Qt::cyan), 60);
        QVERIFY2(cyanLeft  > 0,   "Line bar should fill the left half at S9");
        QCOMPARE(cyanRight, 0);
    }

    void Line_style_ShowValue_draws_yellow_text_top_left()
    {
        QImage img(240, 60, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            b.setBarStyle(BarItem::BarStyle::Line);
            b.setBarColor(QColor(Qt::darkGray));  // avoid confusion with yellow
            b.setShowValue(true);
            b.setFontColour(QColor(Qt::yellow));
            b.setRange(-140.0, 0.0);
            for (int i = 0; i < 20; ++i) { b.setValue(-50.0); }
            b.paint(p, img.width(), img.height());
        }
        // Yellow text should land in the top-left quadrant
        const QRect topLeft(0, 0, img.width() / 2, img.height() / 2);
        const int hits = countMatching(img, topLeft, QColor(Qt::yellow), 50);
        QVERIFY2(hits > 0, "ShowValue should draw yellow text in top-left");
    }

    void Line_style_ShowMarker_draws_peak_and_live_lines()
    {
        QImage img(240, 60, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            b.setBarStyle(BarItem::BarStyle::Line);
            b.setRange(-140.0, 0.0);
            b.setShowMarker(true);
            b.setMarkerColour(QColor(Qt::green));
            b.setPeakHoldMarkerColour(QColor(Qt::magenta));
            // push a high peak, then settle lower so live < peak
            for (int i = 0; i < 20; ++i) { b.setValue(-20.0); }
            for (int i = 0; i < 20; ++i) { b.setValue(-80.0); }
            b.paint(p, img.width(), img.height());
        }
        const int greenHits   = countMatching(img, img.rect(), QColor(Qt::green),   60);
        const int magentaHits = countMatching(img, img.rect(), QColor(Qt::magenta), 60);
        QVERIFY2(greenHits > 0,   "live marker should paint in green");
        QVERIFY2(magentaHits > 0, "peak-hold marker should paint in magenta");
    }

    void deserialize_garbled_calibration_field_is_tolerated()
    {
        // Malformed calibration pairs are silently dropped — prevents
        // user-hand-edited AppSettings files from breaking BarItem
        // load entirely.
        BarItem a;
        a.setRange(-140.0, 0.0);
        const QString blob = a.serialize()
            .section(QLatin1Char('|'), 0, 29)
            + QLatin1String("|garbage;not=valid;=;=0.5;-50=bad;-20=0.9");
        BarItem b;
        QVERIFY(b.deserialize(blob));
        // -20 -> 0.9 is the only well-formed pair, so the size is 1
        QCOMPARE(b.scaleCalibrationSize(), 1);
        QCOMPARE(b.valueToNormalizedX(-20.0), 0.9f);
    }
};

QTEST_MAIN(TstMeterItemBar)
#include "tst_meter_item_bar.moc"
