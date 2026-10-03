// From AetherSDR SpectrumWidget.cpp:14216-14366 [@1872028c] (appendShadow /
// writeShadowSlot -- the shadow-descriptor assembly consumed by
// dss_mesh.frag's applySliceShadow()) and SpectrumWidget.cpp:9975-9979
// [@1872028c] (the "3D Slice Shadow" context-menu toggle). Task 12 of the
// 3D stacked-trace spectrum plan (design doc docs/architecture/2026-08-08-
// 3d-stacked-trace-spectrum-design.md, plan docs/architecture/2026-08-08-3d-
// stacked-trace-spectrum-plan.md).
//
// Pins: buildDssShadowBands() maps each visible slice's passband onto the
// same [0,1] viewport-unit space hzToX() uses, gated entirely off when
// threeDSliceDepth() is off, truncated to the shader's fixed 8-descriptor
// budget, and dropping (not clamping) a slice whose passband never touches
// the visible viewport. centreUnit is pinned to the slice's own CARRIER
// frequency (matching drawSliceMarker()'s separate VFO-centre-line pixel,
// and upstream's own unitForShadowMhz(so.freqMhz)) rather than the passband
// MIDPOINT -- the two coincide for a symmetric filter, so an asymmetric
// USB-shaped filter is required to actually distinguish them.

#include <QTest>
#include <cmath>

#include "gui/SpectrumWidget.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

// Give the widget a real geometry and view before asking for slice markers.
void placePan(SpectrumWidget& w)
{
    w.resize(800, 400);
    w.setFrequencyRange(14200000.0, 96000.0);
}

VfoWidget* addSlice(SpectrumWidget& w, int index, double hz, int lo, int hi)
{
    VfoWidget* flag = w.addVfoWidget(index);
    if (flag) {
        flag->setFrequency(hz);
        flag->setFilter(lo, hi);
    }
    return flag;
}

}  // namespace

class TestDssSliceShadow : public QObject {
    Q_OBJECT

private slots:
    void disabled_producesNoBands() {
        SpectrumWidget w;
        placePan(w);
        QVERIFY(addSlice(w, 0, 14200000.0, -3000, 3000));
        w.setThreeDSliceDepth(false);
        QVERIFY(w.buildDssShadowBands().isEmpty());
    }

    void enabled_mapsPassbandToViewportUnits() {
        SpectrumWidget w;
        placePan(w);
        QVERIFY(addSlice(w, 0, 14200000.0, -3000, 3000));
        w.setThreeDSliceDepth(true);
        const auto bands = w.buildDssShadowBands();
        QCOMPARE(bands.size(), 1);
        // A slice on the view centre sits at 0.5 across the viewport.
        QVERIFY(std::abs(bands[0].centreUnit - 0.5f) < 1e-4f);
        // 6 kHz of a 96 kHz view is 1/16 of the width.
        QVERIFY(std::abs((bands[0].highUnit - bands[0].lowUnit) - 0.0625f)
                < 1e-4f);
        QVERIFY(bands[0].lowUnit < bands[0].highUnit);
    }

    // The shader's shadowBands array is fixed at 8, so more slices than that
    // must be truncated by the builder rather than overrunning the UBO.
    void moreThanEightSlices_areTruncated() {
        SpectrumWidget w;
        w.resize(800, 400);
        w.setFrequencyRange(14200000.0, 500000.0);
        for (int i = 0; i < 12; ++i) {
            addSlice(w, i, 14100000.0 + i * 10000.0, -1500, 1500);
        }
        w.setThreeDSliceDepth(true);
        QVERIFY(w.sliceMarkerGeometry().size() > 8);   // precondition
        QCOMPARE(w.buildDssShadowBands().size(), 8);
    }

    void offscreenSlice_isDropped() {
        SpectrumWidget w;
        placePan(w);
        QVERIFY(addSlice(w, 0, 21000000.0, -3000, 3000));
        w.setThreeDSliceDepth(true);
        QVERIFY(w.buildDssShadowBands().isEmpty());
    }

    // centreUnit must track the slice's CARRIER frequency, not the passband
    // midpoint -- a symmetric filter (as used by every other test in this
    // file) cannot tell the two apart, since they coincide exactly. A
    // USB-shaped filter (150..2850 Hz, matching tst_pan_flag_positions.cpp's
    // kUsbLowHz/kUsbHighHz) offsets the midpoint 1500 Hz above the carrier;
    // at this test's 96 kHz view that is a 0.015625 unit gap, three orders
    // of magnitude past the 1e-4 tolerance below, so a builder that
    // (wrongly) averaged lowUnit/highUnit instead of reading the carrier
    // frequency directly is caught, not just plausible.
    void centreUnit_tracksCarrierNotPassbandMidpoint() {
        SpectrumWidget w;
        placePan(w);
        QVERIFY(addSlice(w, 0, 14200000.0, 150, 2850));
        w.setThreeDSliceDepth(true);
        const auto bands = w.buildDssShadowBands();
        QCOMPARE(bands.size(), 1);
        QVERIFY(std::abs(bands[0].centreUnit - 0.5f) < 1e-4f);
    }
};

QTEST_MAIN(TestDssSliceShadow)
#include "tst_dss_slice_shadow.moc"
