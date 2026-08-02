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

// Mirrors kNotchHandleHalfWidthPx in SpectrumWidget.cpp (AetherSDR
// src/gui/SpectrumWidget.cpp:13547-13548 [@c6481cbf]); the implementation
// constant is file-local there, so the fixture restates it.
constexpr int kNotchHandleHalfWidthPxForTest = 5;

// Mirrors kNotchMinHalfWidthPx in SpectrumWidget.cpp, the std::max(2, ...)
// floor at AetherSDR src/gui/SpectrumWidget.cpp:13523 [@c6481cbf] that
// keeps a sub-2-pixel notch grabbable.
constexpr int kNotchMinHalfWidthPxForTest = 2;

// Mirrors the private SpectrumWidget chrome constants kFreqScaleH (28) and
// kDividerH (4) plus the m_spectrumFrac default (0.40f), which together
// give notchSpecRect() its height.  Restated because all three sit in the
// private section of src/gui/SpectrumWidget.h.
constexpr int   kFreqScaleHForTest   = 28;
constexpr int   kDividerHForTest     = 4;
constexpr float kSpectrumFracForTest = 0.40f;

QRect specRect() { return QRect(0, 0, kPanW, kSpecH); }

// Reproduces SpectrumWidget::hzToX (src/gui/SpectrumWidget.cpp:4048-4053)
// term for term, in the same order and the same types, so expected pixel
// columns are exact rather than tolerance-bounded.
int expectX(double hz)
{
    const double lowHz = kCentreHz - kBandwidthHz / 2.0;
    const double frac  = (hz - lowHz) / kBandwidthHz;
    return 0 + static_cast<int>(frac * kPanW);
}

// The two render paths lay the spectrum row out differently, and
// specHFromHeight (src/gui/SpectrumWidget.cpp:5984-5993) already encodes
// that split.  Restated here so notchSpecRect() is pinned against the
// formula the paint sites use rather than against itself.
int expectSpecH(int widgetH)
{
#ifdef NEREUS_GPU_SPECTRUM
    const int contentH = widgetH - (kFreqScaleHForTest + kDividerHForTest);
    return static_cast<int>(contentH * kSpectrumFracForTest);
#else
    return static_cast<int>(widgetH * kSpectrumFracForTest);
#endif
}

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

QImage renderNotches(SpectrumWidget& sw)
{
    QImage img(specRect().size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, false);
    sw.drawNotchMarkersForTest(p, specRect());
    p.end();
    return img;
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

    // -- section 8.2 render geometry -------------------------------------
    // notchSpecRect() is the SINGLE geometry source: both paint sites and
    // (Task 7) the pixel hit test call it, so hit boxes cannot drift from
    // drawn markers.  Pinned against the specHFromHeight formula the paint
    // sites use, not against itself.
    void notch_spec_rect_matches_the_paint_site_geometry()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);

        const QRect r = sw.notchSpecRectForTest();
        QCOMPARE(r.left(), 0);
        QCOMPARE(r.top(), 0);
        QCOMPARE(r.width(), kPanW - sw.reservedRightEdgeWidth());
        QCOMPARE(r.height(), expectSpecH(kPanH));
    }

    // Height is a fraction of the widget, so it has to track a resize
    // rather than latch whatever the first paint saw.
    void notch_spec_rect_tracks_a_resize()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        const int firstH = sw.notchSpecRectForTest().height();

        sw.resize(kPanW, kPanH * 2);
        QCOMPARE(sw.notchSpecRectForTest().height(), expectSpecH(kPanH * 2));
        QVERIFY(sw.notchSpecRectForTest().height() != firstH);
    }

    void empty_marker_list_paints_nothing()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({});

        const QImage img = renderNotches(sw);
        for (int x = 0; x < kPanW; x += 17) {
            QCOMPARE(img.pixelColor(x, kSpecH / 2), QColor(Qt::black));
        }
    }

    // AetherSDR src/gui/SpectrumWidget.cpp:13522-13525, :13538-13542
    // [@c6481cbf]: halfW comes from the UPPER edge only and is mirrored,
    // so left and right are symmetric about cx by construction.
    void edge_lines_land_on_both_notch_boundaries()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0)});

        const int cx    = expectX(kCentreHz);
        const int halfW = std::max(2, expectX(kCentreHz + 100.0) - cx);
        const int left  = cx - halfW;
        const int right = cx + halfW;
        QVERIFY2(halfW > kNotchHandleHalfWidthPxForTest,
                 "fixture notch must be wider than the grab handle");

        const QImage img = renderNotches(sw);
        const QColor yellow = QColor::fromRgb(qRgb(0xFF, 0xFF, 0x00));

        QCOMPARE(img.pixelColor(left,  kSpecH / 2), yellow);
        QCOMPARE(img.pixelColor(right, kSpecH / 2), yellow);
        QCOMPARE(img.pixelColor(left  - 2, kSpecH / 2), QColor(Qt::black));
        QCOMPARE(img.pixelColor(right + 2, kSpecH / 2), QColor(Qt::black));
    }

    // AetherSDR :13534 -- fillRect spans specRect.height(), top to bottom.
    void fill_spans_the_full_spectrum_height()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0)});

        const int cx = expectX(kCentreHz);
        const QImage img = renderNotches(sw);

        QVERIFY(img.pixelColor(cx, kSpecH - 1) != QColor(Qt::black));
        QVERIFY(img.pixelColor(cx, kSpecH / 2) != QColor(Qt::black));
        QVERIFY(img.pixelColor(cx, 30)         != QColor(Qt::black));
    }

    // AetherSDR :13546-13549 -- the handle is +/-5 px wide regardless of
    // how narrow the notch is, and it lives at the TOP of the spectrum.
    void grab_handle_is_wider_than_a_narrow_notch_and_only_at_the_top()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        // 20 Hz across 10 Hz/px is 1 px of half-width, which the
        // std::max(2, ...) floor at AetherSDR :13523 lifts to 2.
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 20.0)});

        const int cx = expectX(kCentreHz);
        const QImage img = renderNotches(sw);
        const QColor black(Qt::black);

        // Body: exactly 2 px each side of centre.
        QVERIFY(img.pixelColor(cx - 2, kSpecH / 2) != black);
        QVERIFY(img.pixelColor(cx + 2, kSpecH / 2) != black);
        QCOMPARE(img.pixelColor(cx - 4, kSpecH / 2), black);
        QCOMPARE(img.pixelColor(cx + 4, kSpecH / 2), black);

        // Handle: +/-5 px wide at the top row, far wider than the 2 px
        // body.  The polygon is symmetric about the integer column cx
        // while Qt samples at pixel centres (x + 0.5), so the rasterised
        // top span is [cx-5, cx+4] rather than [cx-5, cx+5].  Upstream
        // draws the identical polygon (AetherSDR :13546-13549
        // [@c6481cbf]), so the off-by-one is faithful, not a defect.
        QVERIFY(img.pixelColor(cx - 5, 0) != black);
        QVERIFY(img.pixelColor(cx + 4, 0) != black);
        QCOMPARE(img.pixelColor(cx - 7, 0), black);
        QCOMPARE(img.pixelColor(cx + 7, 0), black);

        // ... and only at the top: gone again 40 px down.
        QCOMPARE(img.pixelColor(cx - 4, 40), black);
        QCOMPARE(img.pixelColor(cx + 4, 40), black);

        // Handle HEIGHT is our fixed 10 px, replacing AetherSDR's
        // depth-derived 8 + depthDb * 2 (:13545 [@c6481cbf]).  The
        // triangle's half-width falls inside the 2 px body between
        // scanline 4 and scanline 5 for a height of 10; at upstream's
        // depthDb-0 height of 8 it would already be inside at scanline 4,
        // so this pair pins the divergence rather than merely observing
        // that a handle exists.
        QVERIFY(img.pixelColor(cx - 3, 4) != black);
        QCOMPARE(img.pixelColor(cx - 3, 5), black);
    }

    // The other forced divergence: hatch spacing is a fixed 8 px, not
    // AetherSDR's depth-derived (depthDb <= 1) ? 12 : (depthDb == 2 ? 8 : 5)
    // at :13535 [@c6481cbf].  The hatch strokes paint at full alpha over
    // the alpha-92 fill, so they are the only interior columns that read
    // back as the undimmed base colour.
    void hatch_spacing_is_a_fixed_eight_pixels()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0)});

        const int cx    = expectX(kCentreHz);
        const int halfW = std::max(2, expectX(kCentreHz + 100.0) - cx);
        const QImage img = renderNotches(sw);
        const QColor yellow = QColor::fromRgb(qRgb(0xFF, 0xFF, 0x00));

        QList<int> hatchColumns;
        for (int x = cx - halfW + 1; x < cx + halfW; ++x) {
            if (img.pixelColor(x, kSpecH / 2) == yellow) {
                hatchColumns.append(x);
            }
        }

        QVERIFY2(hatchColumns.size() >= 2,
                 "a 19 px notch body must carry at least two hatch strokes "
                 "at 8 px spacing");
        for (int i = 1; i < hatchColumns.size(); ++i) {
            QCOMPARE(hatchColumns.at(i) - hatchColumns.at(i - 1), 8);
        }
    }

    // AetherSDR :13527-13528 -- "Skip if fully off-screen".
    void off_screen_markers_are_skipped()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz - 100'000.0, 200.0),
                            makeNotch(2, kCentreHz + 100'000.0, 200.0)});

        const QImage img = renderNotches(sw);
        for (int x = 0; x < kPanW; x += 13) {
            QCOMPARE(img.pixelColor(x, kSpecH / 2), QColor(Qt::black));
        }
    }

    // The MHz/Hz boundary. NotchMarker::freqMhz is the ONLY MHz quantity
    // in the TNF stack; widthHz beside it is Hz. A marker whose freqMhz
    // was fed raw Hz lands 1e6 times off and paints nothing on screen,
    // which is exactly what a silent unit drift looks like.
    void marker_freq_is_mhz_while_width_is_hz()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);

        SpectrumWidget::NotchMarker correct;
        correct.id      = 1;
        correct.freqMhz = kCentreHz / 1.0e6;  // 14.25
        correct.widthHz = 200.0;              // Hz, NOT 0.0002 MHz
        sw.setNotchMarkers({correct});
        QCOMPARE(sw.notchMarkersForTest().first().freqMhz, 14.25);

        const int cx    = expectX(kCentreHz);
        const int halfW = std::max(2, expectX(kCentreHz + 100.0) - cx);
        // 200 Hz at 10 Hz/px is a 9 px half-width: hzToX truncates, and
        // 4100/8000 * 800 lands a hair under 410 in double, so the upper
        // edge is column cx+9 rather than cx+10.  Derived from the same
        // helper the implementation uses rather than hardcoded.
        QVERIFY2(halfW > kNotchMinHalfWidthPxForTest,
                 "fixture must exceed the std::max floor or the width unit "
                 "is not being exercised");

        const QImage okImg = renderNotches(sw);
        QVERIFY(okImg.pixelColor(cx, kSpecH / 2) != QColor(Qt::black));
        QVERIFY(okImg.pixelColor(cx - halfW, kSpecH / 2) != QColor(Qt::black));
        QVERIFY(okImg.pixelColor(cx + halfW, kSpecH / 2) != QColor(Qt::black));
        QCOMPARE(okImg.pixelColor(cx - halfW - 1, kSpecH / 2), QColor(Qt::black));
        QCOMPARE(okImg.pixelColor(cx + halfW + 1, kSpecH / 2), QColor(Qt::black));

        // Same notch with freqMhz mistakenly given Hz: off screen.
        SpectrumWidget::NotchMarker wrong = correct;
        wrong.freqMhz = kCentreHz;
        sw.setNotchMarkers({wrong});
        const QImage img = renderNotches(sw);
        for (int x = 0; x < kPanW; x += 13) {
            QCOMPARE(img.pixelColor(x, kSpecH / 2), QColor(Qt::black));
        }
    }
};

QTEST_MAIN(TestNotchHitTest)
#include "tst_notch_hit_test.moc"
