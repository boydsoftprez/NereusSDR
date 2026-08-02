// =================================================================
// tests/tst_notch_hit_test.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Tunable Notch Filter (TNF).
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 8.1 (push API), section 8.2 (rendering).
//
// Build order (design section 12) puts the push API and the marker
// render in step 6 and the pixel hit test in step 7; both live in this
// one executable so the suite gains a single new binary, not two.
// Task 6 creates and registers this file; Task 7 appends slots and
// registers nothing.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>

#include <algorithm>

#include "gui/SpectrumWidget.h"
#include "gui/PanadapterStack.h"
#include "models/NotchModel.h"

using namespace NereusSDR;

namespace {

// Pan geometry shared by every render test.  8 kHz across 800 px is
// 10 Hz per pixel, so a 200 Hz notch is 20 px wide and both of its edge
// columns land on distinct, assertable pixels.
constexpr double kCentreHz    = 14'250'000.0;
constexpr double kBandwidthHz = 8'000.0;
constexpr int    kPanW        = 800;
constexpr int    kPanH        = 400;
constexpr int    kSpecH       = 200;

SpectrumWidget::NotchMarker makeNotch(int id, double freqHz, double widthHz,
                                      bool active = true)
{
    SpectrumWidget::NotchMarker n;
    n.id      = id;
    n.freqMhz = freqHz / 1.0e6;
    n.widthHz = widthHz;
    n.active  = active;
    return n;
}

} // namespace

class TestNotchHitTest : public QObject {
    Q_OBJECT
private slots:
    // -- section 8.1 push API --------------------------------------------
    void marker_list_is_empty_by_default()
    {
        SpectrumWidget sw;
        QCOMPARE(sw.notchMarkersForTest().size(), 0);
    }

    void marker_push_round_trips()
    {
        SpectrumWidget sw;
        sw.setNotchMarkers({makeNotch(7, kCentreHz, 200.0)});

        QCOMPARE(sw.notchMarkersForTest().size(), 1);
        QCOMPARE(sw.notchMarkersForTest().first().id, 7);
        QCOMPARE(sw.notchMarkersForTest().first().widthHz, 200.0);
        QCOMPARE(sw.notchMarkersForTest().first().active, true);
    }

    // Plan decision D-a (RESOLVED, JJ 2026-07-29): the master enable ships
    // OFF, matching Thetis (chkTNF unchecked) and WDSP (create_notchdb
    // master run 0, third_party/wdsp/src/RXA.c:87).  The widget mirror has
    // to agree with NotchModel::globalEnabled(), which is also false, or
    // the first frame after construction paints in the wrong colour.
    void global_enabled_defaults_false_and_round_trips()
    {
        SpectrumWidget sw;
        QCOMPARE(sw.notchGlobalEnabledForTest(), false);
        QCOMPARE(sw.notchGlobalEnabledForTest(), NotchModel().globalEnabled());
        sw.setNotchGlobalEnabled(true);
        QCOMPARE(sw.notchGlobalEnabledForTest(), true);
    }

    // 100 Hz is what wintype-0 min_notch_width yields on this tree:
    // 1600 / (4096 / 256) * (48000 / 48000), third_party/wdsp/src/nbp.c:88.
    void min_notch_width_defaults_to_100_and_round_trips()
    {
        SpectrumWidget sw;
        QCOMPARE(sw.notchMinWidthHzForTest(), 100.0);
        sw.setNotchMinWidthHz(400.0);
        QCOMPARE(sw.notchMinWidthHzForTest(), 400.0);
    }

    // Section 8.2: notch chrome lives in the cached GPU static-overlay
    // texture, so every mutator must invalidate it.  A bare update()
    // (which is all the spot push does) leaves a dragged marker frozen on
    // the shipping path, where NEREUS_GPU_SPECTRUM is ON by default.
    void every_notch_mutator_invalidates_the_static_overlay()
    {
#ifdef NEREUS_GPU_SPECTRUM
        SpectrumWidget sw;

        sw.clearOverlayStaticDirtyForTest();
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0)});
        QVERIFY2(sw.overlayStaticDirtyForTest(),
                 "setNotchMarkers did not call markOverlayDirty()");

        sw.clearOverlayStaticDirtyForTest();
        sw.setNotchGlobalEnabled(true);
        QVERIFY2(sw.overlayStaticDirtyForTest(),
                 "setNotchGlobalEnabled did not call markOverlayDirty()");

        sw.clearOverlayStaticDirtyForTest();
        sw.setNotchMinWidthHz(400.0);
        QVERIFY2(sw.overlayStaticDirtyForTest(),
                 "setNotchMinWidthHz did not call markOverlayDirty()");
#else
        QSKIP("no cached overlay texture on the CPU-only spectrum path");
#endif
    }
};

QTEST_MAIN(TestNotchHitTest)
#include "tst_notch_hit_test.moc"
