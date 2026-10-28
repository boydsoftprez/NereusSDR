// =================================================================
// tests/tst_meter_item_bar.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (meters M1): the red transition
//                 takes the calibration only. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-25 - BAR building block: Segments and Solid geometry, peak hold
//                 marker on its own, peak value colour, power limit, ignore
//                 history, a 32-field saved record and the new-item
//                 defaults. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include <QtTest/QtTest>
#include <QColor>
#include <QImage>
#include <QPainter>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding namespace

#include <memory>

using namespace NereusSDR;

namespace {

// A BAR record as serialize() wrote it before the BAR options were
// appended: 32 fields (0-31), live marker on, Line style.
const QString kSavedBar32 = QStringLiteral(
    "BAR|0|0|1|1|-1|0|H|-140|0"
    "|#ff00b4d8|#ffff4444|1000|0.5|0.25"
    "|Line|#ff000000|#ffffffff|#ffff0000|#ffffff00"
    "|1|1|#ffffff00"
    "|0|#80ff0000|4000"
    "|1|#ff00ff00|#ffff00ff|0"
    "|"
    "|");

} // namespace

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

    void peakFontColour_roundtrip_preserves_explicit_override()
    {
        // Phase E2 tail field — explicit override must survive
        // serialize/deserialize. Invalid (default) -> blank slot ->
        // falls back to m_fontColour at render time.
        BarItem a;
        a.setRange(-140.0, 0.0);
        a.setFontColour(QColor(Qt::yellow));
        a.setPeakFontColour(QColor(0xff, 0x00, 0x00));  // Thetis red
        const QString blob = a.serialize();

        BarItem b;
        QVERIFY(b.deserialize(blob));
        QCOMPARE(b.peakFontColour(), QColor(0xff, 0x00, 0x00));

        // Unset override round-trips as invalid -> fallback to fontColour.
        BarItem c;
        c.setFontColour(QColor(Qt::yellow));
        BarItem d;
        QVERIFY(d.deserialize(c.serialize()));
        QCOMPARE(d.peakFontColour(), QColor(Qt::yellow));
    }

    // ---- BAR building block: styles, peak hold, power limit, defaults ----

    void saved32FieldRecord_keepsLegacyValues()
    {
        QCOMPARE(kSavedBar32.split(QLatin1Char('|')).size(), 32);
        BarItem b;
        QVERIFY(b.deserialize(kSavedBar32));
        // The literal is today's format: it serializes back unchanged.
        QCOMPARE(b.serialize().section(QLatin1Char('|'), 0, 31), kSavedBar32);
        QCOMPARE(b.barStyle(), BarItem::BarStyle::Line);
        QCOMPARE(b.showMarker(), true);
        QCOMPARE(b.peakHold(), true);   // followed showMarker before
        QCOMPARE(b.ignoreHistoryMs(), 0);
        QCOMPARE(b.powerLimit(), 100.0);
        QCOMPARE(b.strokeWidth(), 2.0f);
        QCOMPARE(b.peakValueColour(), QColor(0xff, 0xff, 0x00));  // falls back to font colour

        // Live marker off in a saved record: no peak marker either.
        QString noMarker = kSavedBar32;
        QStringList parts = noMarker.split(QLatin1Char('|'));
        parts[26] = QStringLiteral("0");
        BarItem c;
        QVERIFY(c.deserialize(parts.join(QLatin1Char('|'))));
        QCOMPARE(c.peakHold(), false);

        // An unrecognised field 32 is not a peak hold choice.
        BarItem d;
        QVERIFY(d.deserialize(kSavedBar32 + QStringLiteral("|future")));
        QCOMPARE(d.peakHold(), true);
    }

    void saved32FieldRecord_paintsBothMarkers()
    {
        QImage img(200, 40, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            QVERIFY(b.deserialize(kSavedBar32));
            b.setShowValue(false);
            b.setShowPeakValue(false);
            for (int i = 0; i < 20; ++i) { b.setValue(-20.0); }
            for (int i = 0; i < 20; ++i) { b.setValue(-100.0); }
            b.paint(p, img.width(), img.height());
        }
        QVERIFY(countMatching(img, img.rect(), QColor(Qt::green), 60) > 0);
        QVERIFY(countMatching(img, img.rect(), QColor(Qt::magenta), 60) > 0);
        // 2 px markers: the live marker covers two columns.
        int greenColumns = 0;
        for (int x = 0; x < img.width(); ++x) {
            if (countMatching(img, QRect(x, 20, 1, 1), QColor(Qt::green), 60) > 0) { ++greenColumns; }
        }
        QCOMPARE(greenColumns, 2);
    }

    void segmentGeometry_matchesThetisOn200x20()
    {
        const BarItem::SegmentGeometry g = BarItem::segmentGeometry(200.0f, 20.0f);
        QCOMPARE(g.blockSize, 7);     // max(7, int(200 * 0.02) = 4)
        QCOMPARE(g.gapSize, 1);       // int(7 * 0.2)
        QCOMPARE(g.step, 8);
        QCOMPARE(g.barHeight, 17.0f); // 20 * 0.85
        const BarItem::SegmentGeometry wide = BarItem::segmentGeometry(1000.0f, 40.0f);
        QCOMPARE(wide.blockSize, 20);
        QCOMPARE(wide.gapSize, 4);
        QCOMPARE(wide.step, 24);
    }

    static BarItem* fullBar(BarItem& b, BarItem::BarStyle style)
    {
        b.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        b.setRange(0.0, 100.0);
        b.setBarStyle(style);
        b.setBarColor(QColor(Qt::cyan));
        b.setBarRedColor(QColor(Qt::magenta));
        b.setAttackRatio(1.0f);
        b.setValue(100.0);
        return &b;
    }

    void segments_paintBlocksGapsAndHeight()
    {
        QImage img(200, 20, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            fullBar(b, BarItem::BarStyle::Segments)->paint(p, img.width(), img.height());
        }
        QCOMPARE(img.pixelColor(0, 5), QColor(Qt::cyan));
        QCOMPARE(img.pixelColor(6, 5), QColor(Qt::cyan));
        QCOMPARE(img.pixelColor(7, 5), QColor(Qt::black));   // gap
        QCOMPARE(img.pixelColor(8, 5), QColor(Qt::cyan));    // next block
        QCOMPARE(img.pixelColor(15, 5), QColor(Qt::black));  // gap
        QCOMPARE(img.pixelColor(3, 16), QColor(Qt::cyan));   // last bar row
        QCOMPARE(img.pixelColor(3, 17), QColor(Qt::black));  // bar is 17 high
        QCOMPARE(countMatching(img, img.rect(), QColor(Qt::magenta), 10), 0);
    }

    void solid_fillsLowBelowAndHighAboveTransition()
    {
        QImage img(200, 20, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setRedThreshold(50.0);
            fullBar(b, BarItem::BarStyle::Solid)->paint(p, img.width(), img.height());
        }
        QCOMPARE(img.pixelColor(7, 5), QColor(Qt::cyan));     // no gaps
        QCOMPARE(img.pixelColor(99, 5), QColor(Qt::cyan));
        QCOMPARE(img.pixelColor(100, 5), QColor(Qt::magenta));
        QCOMPARE(img.pixelColor(199, 5), QColor(Qt::magenta));
        QCOMPARE(img.pixelColor(50, 17), QColor(Qt::black));
    }

    // The red transition is the threshold's place on the calibration
    // (Thetis HighPoint, MeterManager.cs:37482 [v2.10.3.15]); a power bar's
    // limit scales the reading, not the transition.
    void solid_redTransitionUsesTheCalibrationOnly()
    {
        QImage img(200, 20, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setBindingId(MeterBinding::TxPower);
            b.addScaleCalibration(0.0, 0.0f);
            b.addScaleCalibration(100.0, 1.0f);
            b.setPowerLimit(50.0);
            b.setRedThreshold(50.0);
            fullBar(b, BarItem::BarStyle::Solid)->paint(p, img.width(), img.height());
        }
        QCOMPARE(img.pixelColor(50, 5), QColor(Qt::cyan));
        QCOMPARE(img.pixelColor(150, 5), QColor(Qt::magenta));
    }

    void segments_switchToHighColourAboveTransition()
    {
        QImage img(200, 20, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setRedThreshold(50.0);
            fullBar(b, BarItem::BarStyle::Segments)->paint(p, img.width(), img.height());
        }
        QCOMPARE(img.pixelColor(0, 5), QColor(Qt::cyan));
        QCOMPARE(img.pixelColor(160, 5), QColor(Qt::magenta));
        QCOMPARE(img.pixelColor(167, 5), QColor(Qt::black));  // gap stays in the high range
    }

    void newStyles_roundTrip()
    {
        for (BarItem::BarStyle style : {BarItem::BarStyle::Segments, BarItem::BarStyle::Solid}) {
            BarItem a;
            a.setBarStyle(style);
            BarItem b;
            QVERIFY(b.deserialize(a.serialize()));
            QCOMPARE(b.barStyle(), style);
        }
        // Existing values keep their numbers.
        QCOMPARE(static_cast<int>(BarItem::BarStyle::Filled), 0);
        QCOMPARE(static_cast<int>(BarItem::BarStyle::Edge), 1);
        QCOMPARE(static_cast<int>(BarItem::BarStyle::Line), 2);
    }

    void peakHoldMarker_drawsWithLiveMarkerOff()
    {
        QImage img(200, 40, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            b.setBarStyle(BarItem::BarStyle::Line);
            b.setBarColor(QColor(Qt::darkGray));
            b.setRange(-140.0, 0.0);
            b.setShowMarker(false);
            b.setPeakHold(true);
            b.setMarkerColour(QColor(Qt::green));
            b.setPeakHoldMarkerColour(QColor(Qt::magenta));
            QCOMPARE(b.showMarker(), false);
            QCOMPARE(b.peakHold(), true);
            for (int i = 0; i < 20; ++i) { b.setValue(-20.0); }
            for (int i = 0; i < 20; ++i) { b.setValue(-100.0); }
            b.paint(p, img.width(), img.height());
        }
        QVERIFY(countMatching(img, img.rect(), QColor(Qt::magenta), 60) > 0);
        QCOMPARE(countMatching(img, img.rect(), QColor(Qt::green), 60), 0);
    }

    void liveMarker_withoutPeakHold()
    {
        QImage img(200, 40, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            b.setBarStyle(BarItem::BarStyle::Line);
            b.setBarColor(QColor(Qt::darkGray));
            b.setRange(-140.0, 0.0);
            b.setShowMarker(true);
            b.setPeakHold(false);
            b.setMarkerColour(QColor(Qt::green));
            b.setPeakHoldMarkerColour(QColor(Qt::magenta));
            for (int i = 0; i < 20; ++i) { b.setValue(-20.0); }
            for (int i = 0; i < 20; ++i) { b.setValue(-100.0); }
            b.paint(p, img.width(), img.height());
        }
        QVERIFY(countMatching(img, img.rect(), QColor(Qt::green), 60) > 0);
        QCOMPARE(countMatching(img, img.rect(), QColor(Qt::magenta), 60), 0);
    }

    void peakText_usesPeakValueColour()
    {
        QImage img(240, 60, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            BarItem b;
            b.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            b.setBarStyle(BarItem::BarStyle::Line);
            b.setBarColor(QColor(Qt::darkGray));
            b.setRange(-140.0, 0.0);
            b.setShowValue(false);
            b.setShowPeakValue(true);
            b.setFontColour(QColor(Qt::yellow));
            b.setPeakValueColour(QColor(Qt::green));
            QCOMPARE(b.peakValueColour(), QColor(Qt::green));
            QCOMPARE(b.peakFontColour(), QColor(Qt::green));
            for (int i = 0; i < 20; ++i) { b.setValue(-50.0); }
            b.paint(p, img.width(), img.height());
        }
        const QRect topRight(img.width() / 2, 0, img.width() / 2, img.height() / 2);
        QVERIFY(countMatching(img, topRight, QColor(Qt::green), 50) > 0);
        QCOMPARE(countMatching(img, img.rect(), QColor(Qt::yellow), 30), 0);
    }

    void powerLimit_normalisesCalibratedPowerBar()
    {
        BarItem b;
        b.setBindingId(MeterBinding::TxPower);
        b.addScaleCalibration(0.0, 0.0f);
        b.addScaleCalibration(50.0, 0.5f);
        b.addScaleCalibration(100.0, 1.0f);
        QCOMPARE(b.powerLimit(), 100.0);
        QCOMPARE(b.valueToNormalizedX(25.0), 0.25f);
        b.setPowerLimit(50.0);
        QCOMPARE(b.valueToNormalizedX(25.0), 0.5f);   // the 50% calibration point

        // Reverse power too; other readings are not scaled.
        b.setBindingId(MeterBinding::TxReversePower);
        QCOMPARE(b.valueToNormalizedX(25.0), 0.5f);
        b.setBindingId(MeterBinding::TxAlc);
        QCOMPARE(b.valueToNormalizedX(25.0), 0.25f);
    }

    void powerLimit_clampsToSetupRange()
    {
        BarItem b;
        b.setPowerLimit(9000.0);
        QCOMPARE(b.powerLimit(), 5000.0);
        b.setPowerLimit(0.1);
        QCOMPARE(b.powerLimit(), 0.1);
        b.setPowerLimit(0.0);
        QCOMPARE(b.powerLimit(), 100.0);  // 0 recovers
    }

    void ignoreHistory_skipsReadingsAfterClearHistory()
    {
        BarItem b;
        b.setRange(-140.0, 0.0);
        b.setShowHistory(true);
        b.setUpdateIntervalMs(100);
        b.setIgnoreHistoryMs(5000);
        QCOMPARE(b.ignoreHistoryMs(), 4000);
        b.setIgnoreHistoryMs(-1);
        QCOMPARE(b.ignoreHistoryMs(), 0);
        b.setIgnoreHistoryMs(300);
        b.setValue(-80.0);
        QCOMPARE(b.historySampleCount(), 1);

        b.clearHistory();
        QCOMPARE(b.historySampleCount(), 0);
        QCOMPARE(b.ignoringReadings(), 3);
        for (int i = 0; i < 3; ++i) { b.setValue(-10.0); }
        QCOMPARE(b.historySampleCount(), 0);
        QVERIFY(b.peakValue() < -10.0);          // the peak waited too
        b.setValue(-10.0);
        QCOMPARE(b.historySampleCount(), 1);
        QCOMPARE(b.peakValue(), -10.0);
    }

    void newFields_roundTrip()
    {
        BarItem a;
        a.setShowMarker(true);
        a.setPeakHold(false);
        a.setIgnoreHistoryMs(1500);
        a.setPowerLimit(400.0);
        a.setStrokeWidth(3.0f);
        a.setPeakValueColour(QColor(0x12, 0x34, 0x56));
        BarItem b;
        QVERIFY(b.deserialize(a.serialize()));
        QCOMPARE(a.serialize().split(QLatin1Char('|')).size(), 36);
        QCOMPARE(b.peakHold(), false);
        QCOMPARE(b.ignoreHistoryMs(), 1500);
        QCOMPARE(b.powerLimit(), 400.0);
        QCOMPARE(b.strokeWidth(), 3.0f);
        QCOMPARE(b.peakValueColour(), QColor(0x12, 0x34, 0x56));
    }

    void makeEntryBar_takesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("BAR"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* bar = qobject_cast<BarItem*>(item.get());
        QVERIFY(bar != nullptr);
        QCOMPARE(bar->historyDurationMs(), 2000);
        QCOMPARE(bar->ignoreHistoryMs(), 2000);
        QCOMPARE(bar->showHistory(), false);
        QCOMPARE(bar->showValue(), true);
        QCOMPARE(bar->showPeakValue(), true);
        QCOMPARE(bar->barStyle(), BarItem::BarStyle::Line);
        QCOMPARE(bar->barColor(), QColor(Qt::red));
        QCOMPARE(bar->barRedColor(), QColor(0xff, 0xa5, 0x00));   // Orange
        QCOMPARE(bar->markerColour(), QColor(Qt::yellow));
        QCOMPARE(bar->peakHoldMarkerColour(), QColor(Qt::red));
        QCOMPARE(bar->peakValueColour(), QColor(Qt::red));
        QCOMPARE(bar->strokeWidth(), 3.0f);
        QCOMPARE(bar->peakHold(), false);
        QCOMPARE(bar->showMarker(), true);
        QCOMPARE(bar->updateIntervalMs(), 100);
        // The table itself.
        QCOMPARE(kBarItemThetisDefaults.historyDurationMs, 2000);
        QCOMPARE(kBarItemThetisDefaults.ignoreHistoryMs, 2000);
        QCOMPARE(QColor::fromRgb(kBarItemThetisDefaults.colourHigh), QColor(0xff, 0xa5, 0x00));
    }

    void plainBar_keepsLegacyLook()
    {
        // A bar built in code (presets) keeps NereusSDR's constructor
        // values; only makeEntry applies the Thetis defaults.
        BarItem b;
        QCOMPARE(b.barStyle(), BarItem::BarStyle::Filled);
        QCOMPARE(b.ignoreHistoryMs(), 0);
        QCOMPARE(b.strokeWidth(), 2.0f);
        QCOMPARE(b.powerLimit(), 100.0);
        b.setShowMarker(true);
        QCOMPARE(b.peakHold(), true);
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
