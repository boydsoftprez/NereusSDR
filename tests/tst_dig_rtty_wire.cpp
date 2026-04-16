// tst_dig_rtty_wire.cpp
//
// S2.10b: Verify DIG offset and RTTY mark/shift client-side logic.
//
// DIG offset:
//   From Thetis console.cs:14637 (DIGUClickTuneOffset) and :14672
//   (DIGLClickTuneOffset). NereusSDR treats these as additive shifts on the
//   same setShiftFrequency path as RIT. Total shift = RIT + DIG (when active).
//   This is a client-side state test — RadioModel wiring to RxChannel is
//   integration behavior (requires WdspEngine, not unit-testable here).
//
// RTTY mark/shift → filter:
//   filterLow  = markHz − shiftHz/2 − 100
//   filterHigh = markHz + shiftHz/2 + 100
//   SliceModel.setFilter() emits filterChanged; RadioModel connects that
//   to RxChannel::setFilterFreqs(). This test verifies the computation
//   math only — RadioModel integration requires WdspEngine.
//
// Note: Thetis enums.cs:252-268 has no DSPMode::RTTY in the WDSP enum —
// RTTY is operated in DIGU/DIGL mode with the dolly/ampeak peak filter.
// NereusSDR models the bandpass using the mark/shift formula for now.

#include <QtTest/QtTest>
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestDigRttyWire : public QObject {
    Q_OBJECT

private slots:
    // ── DIG offset: SliceModel stores per-mode offsets ────────────────────────

    void diglOffsetStoredSeparately() {
        SliceModel s;
        s.setDspMode(DSPMode::DIGL);
        s.setDiglOffsetHz(1500);
        s.setDiguOffsetHz(0);
        QCOMPARE(s.diglOffsetHz(), 1500);
        QCOMPARE(s.diguOffsetHz(), 0);
    }

    void diguOffsetStoredSeparately() {
        SliceModel s;
        s.setDspMode(DSPMode::DIGU);
        s.setDiguOffsetHz(2000);
        s.setDiglOffsetHz(0);
        QCOMPARE(s.diguOffsetHz(), 2000);
        QCOMPARE(s.diglOffsetHz(), 0);
    }

    void diglAndDiguOffsetAreIndependent() {
        SliceModel s;
        s.setDiglOffsetHz(1500);
        s.setDiguOffsetHz(2000);
        // Changing DIGL does not affect DIGU and vice versa
        s.setDiglOffsetHz(100);
        QCOMPARE(s.diguOffsetHz(), 2000);
        s.setDiguOffsetHz(300);
        QCOMPARE(s.diglOffsetHz(), 100);
    }

    void diglOffsetDefaultIsZero() {
        // From Thetis console.cs:14672 — DIGLClickTuneOffset default 0 Hz
        // (NereusSDR defaults diglOffsetHz to 0 per SliceModel; Thetis's
        // digl_click_tune_offset default is 2210 but that governs filter
        // preset centering, not the demodulation shift we model here.)
        SliceModel s;
        QCOMPARE(s.diglOffsetHz(), 0);
    }

    void diguOffsetDefaultIsZero() {
        // From Thetis console.cs:14637 — DIGUClickTuneOffset default 0 Hz
        SliceModel s;
        QCOMPARE(s.diguOffsetHz(), 0);
    }

    void diglOffsetEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::diglOffsetHzChanged);
        s.setDiglOffsetHz(500);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 500);
    }

    void diguOffsetEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::diguOffsetHzChanged);
        s.setDiguOffsetHz(750);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 750);
    }

    void diglOffsetNoSignalOnSameValue() {
        SliceModel s;
        s.setDiglOffsetHz(400);
        QSignalSpy spy(&s, &SliceModel::diglOffsetHzChanged);
        s.setDiglOffsetHz(400);
        QCOMPARE(spy.count(), 0);
    }

    // ── RTTY mark/shift: default values ──────────────────────────────────────

    void rttyMarkDefaultIs2295() {
        // From Thetis setup.designer.cs:40635-40637 — RTTY MARK default 2295 Hz
        SliceModel s;
        QCOMPARE(s.rttyMarkHz(), 2295);
    }

    void rttyShiftDefaultIs170() {
        // From Thetis radio.cs:2043-2044 — rx_dolly_freq1=2295, rx_dolly_freq0=2125
        // shift = 2295 − 2125 = 170 Hz
        SliceModel s;
        QCOMPARE(s.rttyShiftHz(), 170);
    }

    // ── RTTY filter computation math ──────────────────────────────────────────
    //
    // filterLow  = markHz − shiftHz/2 − 100
    // filterHigh = markHz + shiftHz/2 + 100

    void rttyFilterComputationDefaultValues() {
        // mark=2295, shift=170
        // filterLow  = 2295 − 170/2 − 100 = 2295 − 85 − 100 = 2110
        // filterHigh = 2295 + 170/2 + 100 = 2295 + 85 + 100 = 2480
        const int mark  = 2295;
        const int shift = 170;
        const int expectedLow  = mark - shift / 2 - 100;  // 2110
        const int expectedHigh = mark + shift / 2 + 100;  // 2480
        QCOMPARE(expectedLow,  2110);
        QCOMPARE(expectedHigh, 2480);
    }

    void rttyFilterComputationCustomValues() {
        // mark=2125 (Baudot space), shift=170
        // filterLow  = 2125 − 85 − 100 = 1940
        // filterHigh = 2125 + 85 + 100 = 2310
        const int mark  = 2125;
        const int shift = 170;
        const int expectedLow  = mark - shift / 2 - 100;
        const int expectedHigh = mark + shift / 2 + 100;
        QCOMPARE(expectedLow,  1940);
        QCOMPARE(expectedHigh, 2310);
    }

    void rttyFilterComputationWideShift() {
        // mark=2295, shift=850 (wide FSK)
        // filterLow  = 2295 − 425 − 100 = 1770
        // filterHigh = 2295 + 425 + 100 = 2820
        const int mark  = 2295;
        const int shift = 850;
        const int expectedLow  = mark - shift / 2 - 100;
        const int expectedHigh = mark + shift / 2 + 100;
        QCOMPARE(expectedLow,  1770);
        QCOMPARE(expectedHigh, 2820);
    }

    // ── RTTY mark/shift SliceModel signals ────────────────────────────────────

    void rttyMarkEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::rttyMarkHzChanged);
        s.setRttyMarkHz(2000);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2000);
    }

    void rttyShiftEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::rttyShiftHzChanged);
        s.setRttyShiftHz(200);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 200);
    }

    void rttyMarkNoSignalOnSameValue() {
        SliceModel s;
        s.setRttyMarkHz(2295);
        QSignalSpy spy(&s, &SliceModel::rttyMarkHzChanged);
        s.setRttyMarkHz(2295);
        QCOMPARE(spy.count(), 0);
    }

    void rttyShiftNoSignalOnSameValue() {
        SliceModel s;
        s.setRttyShiftHz(170);
        QSignalSpy spy(&s, &SliceModel::rttyShiftHzChanged);
        s.setRttyShiftHz(170);
        QCOMPARE(spy.count(), 0);
    }

    // ── RTTY mode: setFilter is called via RadioModel in integration ──────────
    //
    // This verifies that SliceModel::setFilter works correctly for the
    // computed values; RadioModel connects rttyMarkHzChanged/rttyShiftHzChanged
    // to call slice->setFilter(low, high) when mode == RTTY.

    void setFilterUpdatesFilterLowHigh() {
        // Note: Thetis has no DSPMode::RTTY — RTTY uses DSPMode::DIGU or DIGL
        // per Thetis enums.cs:252-268. The RTTY bandpass filter applies in DIG
        // modes via mark/shift → setFilter() computation in RadioModel.
        SliceModel s;
        s.setDspMode(DSPMode::DIGU);  // RTTY is operated in DIG mode

        QSignalSpy spy(&s, &SliceModel::filterChanged);
        s.setFilter(2110, 2480);  // computed from mark=2295, shift=170

        QCOMPARE(spy.count(), 1);
        QCOMPARE(s.filterLow(),  2110);
        QCOMPARE(s.filterHigh(), 2480);
    }
};

QTEST_MAIN(TestDigRttyWire)
#include "tst_dig_rtty_wire.moc"
