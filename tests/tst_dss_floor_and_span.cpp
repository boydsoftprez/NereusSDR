// From AetherSDR SpectrumWidget.cpp:12743-12784 [@1872028c] (dssRowSpanTarget
// shape) and SpectrumWidget.cpp:14195 [@1872028c] (colorRangeDb's std::min
// cap) plus NereusSDR-original floor/span/wide-channel logic. Task 9 of the
// 3D stacked-trace spectrum plan (design doc docs/architecture/2026-08-08-
// 3d-stacked-trace-spectrum-design.md, plan docs/architecture/2026-08-08-3d-
// stacked-trace-spectrum-plan.md).
//
// Pins: the 3D Floor control offsets the measured noise floor downward: the
// dBm span tracks m_refLevel/m_dynamicRange; the colour aperture caps at
// kDssColorSpanDb but narrows further with a tighter dBm range, never
// widening past the cap; buildDssWideRow() windows the off-screen DDC bins
// of a full-frame snapshot, sized for the WIDEST angle so a runtime angle
// change never invalidates retained rows, and returns empty at full DDC
// width (no span available); and dssRowSpanTarget() scales the available
// overhang by the 3D Span percentage.

#include <QTest>
#include <QVector>
#include <cmath>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestDssFloorAndSpan : public QObject {
    Q_OBJECT

private slots:
    // The surface baseline sits 3D Floor dB BELOW the measured noise floor,
    // so raising the control reveals more noise texture, not less.
    void floorDbm_sitsBelowTheMeasuredNoiseFloor() {
        SpectrumWidget w;
        w.setMeasuredNoiseFloorForTest(-120.0f);
        w.setDssFloorDepth(0);
        QVERIFY(std::abs(w.dssFloorDbm() - (-120.0f)) < 0.01f);
        w.setDssFloorDepth(6);
        QVERIFY(std::abs(w.dssFloorDbm() - (-126.0f)) < 0.01f);
        w.setDssFloorDepth(24);
        QVERIFY(std::abs(w.dssFloorDbm() - (-144.0f)) < 0.01f);
    }

    void spanDb_followsTheDbmDisplayRange() {
        SpectrumWidget w;
        w.setDbmRange(-140.0f, -40.0f);
        QVERIFY(std::abs(w.dssSpanDb() - 100.0f) < 0.01f);
    }

    // Colour reads a STABLE aperture (kDssColorSpanDb, 45 dB) independent of
    // the height mapping's own dBm span, but that aperture can only ever be
    // narrowed by a tighter display range, never widened past it -- upstream
    // AetherSDR SpectrumWidget.cpp:14195 [@1872028c] writes
    // std::min(rangeDb, DssRenderer::kColorSpanDb), not the constant alone.
    // A wide range (100 dB) hits the cap; a narrow one (20 dB, below the
    // cap) tracks the span instead, so the colormap never stretches thinner
    // than the real dBm range in front of it.
    void colorRangeDb_capsAtTheStableApertureButNeverExceedsANarrowSpan() {
        SpectrumWidget w;
        w.setDbmRange(-140.0f, -40.0f);   // 100 dB span -- wider than the cap
        QVERIFY(std::abs(w.dssColorRangeDb() - 45.0f) < 0.01f);
        w.setDbmRange(-30.0f, -10.0f);    // 20 dB span -- narrower than the cap
        QVERIFY(std::abs(w.dssColorRangeDb() - 20.0f) < 0.01f);
    }

    // At full DDC width there is nothing outside the view, so the span
    // control correctly reports nothing available and the surface relaxes to
    // the classic clipped trapezoid.
    void atFullDdcWidth_noSpanIsAvailable() {
        SpectrumWidget w;
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 768000.0);   // view == DDC
        double c = 0.0;
        double b = 0.0;
        const QVector<float> wide =
            w.buildDssWideRow(QVector<float>(4096, -130.0f), c, b);
        QVERIFY(wide.isEmpty());
        QCOMPARE(b, 0.0);
    }

    // Zoomed in, the off-screen DDC bins are available and the wide row
    // covers more spectrum than the viewport.
    void zoomedIn_wideRowReachesPastTheViewport() {
        SpectrumWidget w;
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 96000.0);    // 8x zoom
        double c = 0.0;
        double b = 0.0;
        const QVector<float> wide =
            w.buildDssWideRow(QVector<float>(4096, -130.0f), c, b);
        QVERIFY(!wide.isEmpty());
        QVERIFY2(b > 0.096, "wide row must span more than the viewport");
        QVERIFY(std::abs(c - 14.2) < 1e-6);
    }

    // The wide window is sized for the WIDEST angle, not the current one, so
    // moving the angle slider never invalidates rows already in the ring.
    void wideRowWindow_isAngleIndependent() {
        SpectrumWidget w;
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 96000.0);
        double c0 = 0.0, b0 = 0.0, c1 = 0.0, b1 = 0.0;
        w.setDssAngle(0);
        w.buildDssWideRow(QVector<float>(4096, -130.0f), c0, b0);
        w.setDssAngle(100);
        w.buildDssWideRow(QVector<float>(4096, -130.0f), c1, b1);
        QCOMPARE(b0, b1);
        QCOMPARE(c0, c1);
    }

    // The 3D Span slider scales the AVAILABLE overhang, so 0 always gives
    // the classic trapezoid and 100 spends everything on offer.
    //
    // Two additions beyond the brief's literal test, both needed for this
    // test to exercise anything at all rather than pass vacuously:
    //
    // 1. resize()+show()+qWaitForWindowExposed(): pushWaterfallRow()'s very
    //    first guard returns immediately while m_waterfall is null (see
    //    tst_dss_row_tee.cpp's stopOnTx_freezesBothPanesTogether, which
    //    documents the same trap). Without this, pushDssRow() never runs
    //    and dssRowsPushedForTest() stays 0 for the whole test.
    // 2. setFullBinsForTest(): pushDssRow() feeds the wide channel from
    //    m_lastFullBinsDbm, which production code only ever populates from
    //    inside updateSpectrumLinear() (an FFT-arrival callback this test
    //    never drives). Without seeding it directly, buildDssWideRow()
    //    always sees an empty fullBins vector and returns empty every time
    //    regardless of zoom, so newestWideBandwidthMhz() can never see
    //    anything above targetBandwidthMhz and dssRowSpanTarget() would
    //    read 1.0f at BOTH span settings instead of only at 0. The seed
    //    value mirrors zoomedIn_wideRowReachesPastTheViewport's own
    //    buildDssWideRow() probe above (same DDC/view setup), which already
    //    establishes that a 4096-bin -130 dBm snapshot yields a wide row
    //    wider than a 96 kHz viewport at this sample rate/centre.
    void rowSpanTarget_scalesTheAvailableOverhang() {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 96000.0);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setFullBinsForTest(QVector<float>(4096, -130.0f));
        for (int i = 0; i < 3; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(768, -130.0f));
        }
        QCOMPARE(w.dssRowsPushedForTest(), 3);   // precondition: rows landed
        w.setDssRowSpan(0);
        QVERIFY(std::abs(w.dssRowSpanTarget(0.096) - 1.0f) < 1e-6f);
        w.setDssRowSpan(100);
        QVERIFY(w.dssRowSpanTarget(0.096) > 1.0f);
    }
};

QTEST_MAIN(TestDssFloorAndSpan)
#include "tst_dss_floor_and_span.moc"
