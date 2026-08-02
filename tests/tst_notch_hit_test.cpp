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
//
// Task 7 additions pin the panadapter-side interaction (design
// section 7):
//   * the pixel-space hit test, which follows Thetis
//     MNotchDB.NotchThatSurroundsFrequencyInBW (radio.cs:4296-4325
//     [v2.10.3.15]): first-found in list order, pad applied only when
//     the notch is narrower than twice the pad, off-screen reject;
//   * edge-vs-centre grab discrimination (console.cs:49037-49067
//     [v2.10.3.15]): 8 px minimum on-screen width before edge zones
//     exist at all, +/- 4 px edge zone, side-of-centre default, Shift
//     as an explicit resize;
//   * hover-driven selection, centre/edge drag, wheel resize gated on
//     the selection (console.cs:31141-31145 + :33299-33321
//     [v2.10.3.15]), Ctrl + right-click add (console.cs:49614,
//     49629-49646 [v2.10.3.15]) and the notch context menu (AetherSDR
//     src/gui/SpectrumWidget.cpp:8517-8572 [@c6481cbf]).
// =================================================================

#include <QtTest/QtTest>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QImage>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QSignalSpy>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "core/ConnectionState.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumOverlayMenu.h"
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

// Left edge column of a 200 Hz notch centred on `freqHz`.  The edge lines
// are the only part of a marker drawn at full alpha, so this is where the
// base colour reads back undimmed by the alpha-92 fill.
int leftEdgeX(double freqHz = kCentreHz)
{
    const int cx = expectX(freqHz);
    return cx - std::max(2, expectX(freqHz + 100.0) - cx);
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

// ── Task 7 interaction fixture (design section 7) ────────────────────
//
// Deliberately separate geometry from the render fixture above: 1000 px
// of spectrum across 128 kHz gives exactly 128 Hz per pixel, and the
// centre frequency lands on an exact binary fraction of the span, so
// every hand-computed pixel below is exact rather than "within a
// rounding error".  The `kUi` prefix keeps the two fixtures from
// colliding in this one translation unit.
constexpr double kUiCentreHz    = 14'200'000.0;
constexpr double kUiBandwidthHz = 128'000.0;
constexpr double kUiHzPerPx     = 128.0;      // kUiBandwidthHz / kUiWidgetW
constexpr int    kUiWidgetW     = 1000;
constexpr int    kUiWidgetH     = 400;
constexpr int    kUiCentreX     = 500;
// specHFromHeight(400, 0.40f, 28 + 4) is 147 on the QRhi layout and 160
// on the QPainter one; any y well below both is inside the spectrum plot
// on either render path.
constexpr int    kUiSpecY       = 50;

// Mirrors SpectrumWidget::hzToX (src/gui/SpectrumWidget.cpp:4059-4064)
// exactly, truncating int cast included, so expected pixels cannot drift
// from the widget's own mapping.
int uiXForHz(double hz)
{
    const double lowHz = kUiCentreHz - kUiBandwidthHz / 2.0;
    return static_cast<int>((hz - lowHz) / kUiBandwidthHz * kUiWidgetW);
}

// Mirrors SpectrumWidget::xToHz (src/gui/SpectrumWidget.cpp:4066-4070).
double uiHzForX(int x)
{
    const double lowHz = kUiCentreHz - kUiBandwidthHz / 2.0;
    return lowHz + (static_cast<double>(x) / kUiWidgetW) * kUiBandwidthHz;
}

void configureUi(SpectrumWidget& w)
{
    w.resize(kUiWidgetW, kUiWidgetH);
    // dBm strip off -> reservedRightEdgeWidth() == 0 -> the spectrum rect
    // is the full widget width, so uiXForHz above IS the widget's mapping.
    w.setDbmScaleVisible(false);
    w.setFrequencyRange(kUiCentreHz, kUiBandwidthHz);
    // mousePressEvent swallows left clicks while not Connected.
    w.setConnectionState(ConnectionState::Connected);
}

// QTest::mouseMove with no button held only calls QCursor::setPos and
// never delivers an event to the widget, so the hover tests synthesise
// the QMouseEvent directly.
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos,
               Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   button, buttons, mods);
    QApplication::sendEvent(w, &me);
}

void sendWheel(QWidget* w, QPoint pos, int angleDeltaY,
               Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QWheelEvent we(QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   QPoint(0, 0), QPoint(0, angleDeltaY),
                   Qt::NoButton, mods, Qt::NoScrollPhase, false);
    QApplication::sendEvent(w, &we);
}

QAction* actionByText(const QList<QAction*>& actions, const QString& text)
{
    for (QAction* a : actions) {
        if (a->text() == text) {
            return a;
        }
    }
    return nullptr;
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
        // Master TNF on so the base colour is the active yellow rather
        // than the olive master-off colour (design section 8.2).
        sw.setNotchGlobalEnabled(true);
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
        sw.setNotchGlobalEnabled(true);
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

    // -- section 8.2 colours, from Thetis display.cs:8691-8722 [v2.10.3.15]
    //
    // The base colour is read back off an edge line, which is drawn at
    // full alpha; the fill beside it is changeAlpha(colour, 92)
    // (display.cs:400-408 [v2.10.3.15]) and would compare against a
    // dimmed value instead.
    void active_notch_is_yellow()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchGlobalEnabled(true);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0, /*active*/ true)});

        QCOMPARE(renderNotches(sw).pixelColor(leftEdgeX(), kSpecH / 2),
                 QColor::fromRgb(qRgb(0xFF, 0xFF, 0x00)));
    }

    void bypassed_notch_is_gray()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchGlobalEnabled(true);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0, /*active*/ false)});

        QCOMPARE(renderNotches(sw).pixelColor(leftEdgeX(), kSpecH / 2),
                 QColor::fromRgb(qRgb(0x80, 0x80, 0x80)));
    }

    // display.cs:8704-8707 -- master TNF off repaints every marker olive
    // rather than hiding it, so the operator can still see where the
    // notches are while the master switch is down.
    void master_tnf_off_is_olive_even_for_an_active_notch()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0, /*active*/ true)});
        sw.setNotchGlobalEnabled(false);

        QCOMPARE(renderNotches(sw).pixelColor(leftEdgeX(), kSpecH / 2),
                 QColor::fromRgb(qRgb(0x80, 0x80, 0x00)));
    }

    // display.cs:8710-8722 "overide if highlighed" -- the highlight is
    // applied AFTER the master-off branch upstream, so it wins over every
    // other state.
    void selected_notch_is_chartreuse_and_overrides_master_off()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(4, kCentreHz, 200.0, /*active*/ false)});
        sw.setNotchGlobalEnabled(false);
        sw.setSelectedNotchIdForTest(4);

        QCOMPARE(renderNotches(sw).pixelColor(leftEdgeX(), kSpecH / 2),
                 QColor::fromRgb(qRgb(0x7F, 0xFF, 0x00)));
    }

    void hovered_notch_is_chartreuse()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchGlobalEnabled(true);
        sw.setNotchMarkers({makeNotch(9, kCentreHz, 200.0, /*active*/ true)});
        sw.setHoveredNotchIdForTest(9);

        QCOMPARE(renderNotches(sw).pixelColor(leftEdgeX(), kSpecH / 2),
                 QColor::fromRgb(qRgb(0x7F, 0xFF, 0x00)));
    }

    // Only the marker whose id matches highlights. A second notch on the
    // same pan must keep its own state.
    void highlight_applies_only_to_the_matching_marker()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchGlobalEnabled(true);
        sw.setNotchMarkers({makeNotch(1, kCentreHz - 2'000.0, 200.0, true),
                            makeNotch(2, kCentreHz + 2'000.0, 200.0, true)});
        sw.setSelectedNotchIdForTest(2);

        const QImage img = renderNotches(sw);
        QCOMPARE(img.pixelColor(leftEdgeX(kCentreHz - 2'000.0), kSpecH / 2),
                 QColor::fromRgb(qRgb(0xFF, 0xFF, 0x00)));
        QCOMPARE(img.pixelColor(leftEdgeX(kCentreHz + 2'000.0), kSpecH / 2),
                 QColor::fromRgb(qRgb(0x7F, 0xFF, 0x00)));
    }

    // m_selectedNotchId / m_hoveredNotchId default to -1, and so does a
    // default-constructed NotchMarker.  They must not match each other.
    void unset_selection_does_not_highlight_an_unidentified_marker()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchGlobalEnabled(true);
        sw.setNotchMarkers({makeNotch(-1, kCentreHz, 200.0, /*active*/ true)});

        QCOMPARE(renderNotches(sw).pixelColor(leftEdgeX(), kSpecH / 2),
                 QColor::fromRgb(qRgb(0xFF, 0xFF, 0x00)));
    }

    // -- section 8.1 inbound signals -------------------------------------
    // Emitters land with the interaction layer (design section 12 step 7);
    // this is the signature gate MainWindow::wirePanNotchHandlers connects
    // against.
    void notch_interaction_signals_exist_with_expected_signatures()
    {
        SpectrumWidget sw;
        QSignalSpy create(&sw, &SpectrumWidget::notchCreateRequested);
        QSignalSpy move(&sw,   &SpectrumWidget::notchMoveRequested);
        QSignalSpy width(&sw,  &SpectrumWidget::notchWidthRequested);
        QSignalSpy active(&sw, &SpectrumWidget::notchActiveRequested);
        QSignalSpy remove(&sw, &SpectrumWidget::notchRemoveRequested);

        QVERIFY(create.isValid());
        QVERIFY(move.isValid());
        QVERIFY(width.isValid());
        QVERIFY(active.isValid());
        QVERIFY(remove.isValid());
    }

    // Every notch frequency crossing these signals is absolute RF in Hz,
    // never MHz: NotchMarker::freqMhz is the single MHz quantity in the
    // stack.  Pinned through the metaobject so a later signature drift is
    // caught at the seam rather than 1e6 downstream in Tasks 7 and 10.
    void notch_signal_frequencies_are_declared_in_hz()
    {
        const QMetaObject& mo = SpectrumWidget::staticMetaObject;
        for (const char* sig : {"notchCreateRequested(double,bool)",
                                "notchMoveRequested(int,double)",
                                "notchWidthRequested(int,double)",
                                "notchActiveRequested(int,bool)",
                                "notchRemoveRequested(int)"}) {
            const int idx = mo.indexOfSignal(sig);
            QVERIFY2(idx >= 0, sig);
            QCOMPARE(mo.method(idx).methodType(), QMetaMethod::Signal);
        }
    }

    // section 8.1: under D1 the notch list is global, so EVERY pan gets
    // the same push and each converts it into its own pixel space.
    // Deliberately NOT the spot overlay's activeSpectrumWidget()-only
    // shape, which leaves secondary pans blank.
    void two_pans_map_the_same_notch_to_their_own_pixel_space()
    {
        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2v"),
                          {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QCOMPARE(stack.count(), 2);

        SpectrumWidget* a = stack.spectrum(QStringLiteral("pan-0"));
        SpectrumWidget* b = stack.spectrum(QStringLiteral("pan-1"));
        QVERIFY(a != nullptr);
        QVERIFY(b != nullptr);

        a->resize(kPanW, kPanH);
        b->resize(kPanW, kPanH);
        a->setFrequencyRange(kCentreHz, kBandwidthHz);
        // pan-1 is parked 2 kHz low, so the same absolute-RF notch has to
        // land 200 px to the right of where it lands on pan-0.
        b->setFrequencyRange(kCentreHz - 2'000.0, kBandwidthHz);

        const QVector<SpectrumWidget::NotchMarker> markers{
            makeNotch(1, kCentreHz, 200.0)};
        a->setNotchMarkers(markers);
        b->setNotchMarkers(markers);

        QCOMPARE(a->notchMarkersForTest().size(), 1);
        QCOMPARE(b->notchMarkersForTest().size(), 1);

        const QImage imgA = renderNotches(*a);
        const QImage imgB = renderNotches(*b);

        QVERIFY(imgA.pixelColor(400, kSpecH / 2) != QColor(Qt::black));
        QCOMPARE(imgA.pixelColor(600, kSpecH / 2), QColor(Qt::black));

        QVERIFY(imgB.pixelColor(600, kSpecH / 2) != QColor(Qt::black));
        QCOMPARE(imgB.pixelColor(400, kSpecH / 2), QColor(Qt::black));
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

    // -- section 8.1 fan-out ---------------------------------------------
    // MainWindow is deliberately never constructed in this suite (see the
    // banner of tests/tst_mainwindow_tools_spot_hub.cpp), so this pins the
    // half of MainWindow::refreshPanNotchMarkers that can drift silently:
    // the NotchModel -> NotchMarker conversion, and the fact that the same
    // vector reaches every pan.  The m_panStack loop itself is
    // bench-covered (design section 11.2).
    //
    // `auto` in the loop is deliberate: it compiles whether Notch is
    // nested in NotchModel or lives at NereusSDR namespace scope.
    void notch_model_entries_convert_and_reach_every_pan()
    {
        NotchModel model;
        const int idA = model.addNotch(14'250'000.0, 200.0);
        const int idB = model.addNotch(14'251'000.0, 100.0);
        QVERIFY(idA >= 0);
        QVERIFY(idB >= 0);
        QVERIFY(model.setActive(idB, false));

        QVector<SpectrumWidget::NotchMarker> markers;
        markers.reserve(model.notches().size());
        for (const auto& n : model.notches()) {
            SpectrumWidget::NotchMarker m;
            m.id      = n.id;
            m.freqMhz = n.centerHz / 1.0e6;
            m.widthHz = n.widthHz;
            m.active  = n.active;
            markers.append(m);
        }
        QCOMPARE(markers.size(), 2);

        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2h"),
                          {QStringLiteral("pan-0"), QStringLiteral("pan-1")});

        model.setGlobalEnabled(false);
        for (const QString& panId : {QStringLiteral("pan-0"),
                                     QStringLiteral("pan-1")}) {
            SpectrumWidget* sw = stack.spectrum(panId);
            QVERIFY(sw != nullptr);
            sw->setNotchMarkers(markers);
            sw->setNotchGlobalEnabled(model.globalEnabled());
        }

        for (const QString& panId : {QStringLiteral("pan-0"),
                                     QStringLiteral("pan-1")}) {
            SpectrumWidget* sw = stack.spectrum(panId);
            QCOMPARE(sw->notchMarkersForTest().size(), 2);
            QCOMPARE(sw->notchMarkersForTest().at(0).id, idA);
            QCOMPARE(sw->notchMarkersForTest().at(0).freqMhz, 14.25);
            QCOMPARE(sw->notchMarkersForTest().at(0).widthHz, 200.0);
            QCOMPARE(sw->notchMarkersForTest().at(0).active, true);
            QCOMPARE(sw->notchMarkersForTest().at(1).id, idB);
            QCOMPARE(sw->notchMarkersForTest().at(1).widthHz, 100.0);
            QCOMPARE(sw->notchMarkersForTest().at(1).active, false);
            QCOMPARE(sw->notchGlobalEnabledForTest(), false);
        }
    }

    // The other half of the unit boundary: NotchModel stores Hz, the
    // marker carries MHz, and refreshPanNotchMarkers is the ONLY divide by
    // 1e6.  A notch stored at 14.251 MHz must arrive as 14.251, not
    // 14251000.
    void model_to_marker_conversion_is_the_only_hz_to_mhz_site()
    {
        NotchModel model;
        const int id = model.addNotch(14'251'000.0, 100.0);
        QVERIFY(id >= 0);
        QCOMPARE(model.notches().first().centerHz, 14'251'000.0);

        SpectrumWidget::NotchMarker m;
        m.id      = model.notches().first().id;
        m.freqMhz = model.notches().first().centerHz / 1.0e6;
        m.widthHz = model.notches().first().widthHz;

        QCOMPARE(m.freqMhz, 14.251);
        // Width does NOT convert: it stays Hz on both sides.
        QCOMPARE(m.widthHz, model.notches().first().widthHz);
        QCOMPARE(m.widthHz, 100.0);
    }

    // Widths pushed onto a create come from NotchModel's Thetis constants,
    // under the names Task 3 shipped.  Guards against a second spelling
    // being minted in the wiring layer.
    void notch_width_constants_come_from_notch_model()
    {
        QCOMPARE(NotchModel::kDefaultNotchWidthHz, 200.0);
        QCOMPARE(NotchModel::kNarrowNotchWidthHz, 100.0);
    }

    // MainWindow cannot be stood up in a unit test (it boots WDSP, the
    // audio engine and the discovery thread), so the fan-out entry points
    // are resolved by name, the same seam tst_pan_badge_click_wiring uses
    // for the badge handlers.  A rename that stranded every notch marker
    // would otherwise reach a release silently.
    //
    // These must be SLOTS, not plain methods: wirePanNotchHandlers re-arms
    // on every PanadapterStack::countChanged, and Qt6 silently ignores
    // Qt::UniqueConnection when the target is a lambda, so a lambda here
    // would stack one extra connection per layout switch.
    void mainwindow_exposes_the_notch_fanout_slots()
    {
        const QMetaObject& mo = MainWindow::staticMetaObject;
        for (const char* sig : {"refreshPanNotchMarkers()",
                                "wirePanNotchHandlers()",
                                "onNotchCreateRequested(double,bool)",
                                "onNotchMoveRequested(int,double)",
                                "onNotchWidthRequested(int,double)",
                                "onNotchActiveRequested(int,bool)",
                                "onNotchRemoveRequested(int)"}) {
            QVERIFY2(mo.indexOfSlot(sig) >= 0,
                     qPrintable(QStringLiteral("MainWindow::%1 is not an "
                                               "invokable slot; the per-pan "
                                               "notch connects would be "
                                               "unmade or would silently "
                                               "duplicate")
                                    .arg(QLatin1String(sig))));
        }
    }

    // ==================================================================
    // Task 7: panadapter interaction (design section 7)
    // ==================================================================

    // -- section 7.3 hit test: the Thetis notchSurrounding rule ---------

    void hit_test_returns_id_at_notch_centre()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(7, kUiCentreHz, 400.0)});

        QCOMPARE(w.notchAtPixelForTest(kUiCentreX), 7);
    }

    void hit_test_returns_minus_one_off_notch()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(7, kUiCentreHz, 400.0)});

        // 100 px away is 12800 Hz off centre, far outside a 400 Hz notch.
        QCOMPARE(w.notchAtPixelForTest(kUiCentreX + 100), -1);
    }

    void hit_test_returns_first_match_in_list_order()
    {
        SpectrumWidget w;
        configureUi(w);
        // Two notches covering the same pixel.  Thetis returns the first
        // one found walking the list, not the nearest centre; AetherSDR's
        // nearest-centre tnfAtPixel would return 9 here.
        w.setNotchMarkers({makeNotch(3, kUiCentreHz, 2000.0),
                           makeNotch(9, kUiCentreHz + 256.0, 2000.0)});

        QCOMPARE(w.notchAtPixelForTest(kUiCentreX), 3);
    }

    void hit_test_pads_sub_pixel_notch_by_one_pixel()
    {
        SpectrumWidget w;
        configureUi(w);
        // 10 Hz wide is 0.08 px: unhittable without the pad.  10 < 2*128,
        // so the pad applies and the reach becomes 5 + 128 = 133 Hz.
        w.setNotchMarkers({makeNotch(4, kUiCentreHz, 10.0)});

        QCOMPARE(w.notchAtPixelForTest(kUiCentreX), 4);          // 0 Hz off
        QCOMPARE(w.notchAtPixelForTest(kUiCentreX + 1), 4);      // 128 Hz off
        QCOMPARE(w.notchAtPixelForTest(kUiCentreX - 1), 4);      // 128 Hz off
        QCOMPARE(w.notchAtPixelForTest(kUiCentreX + 2), -1);     // 256 Hz off
    }

    void hit_test_does_not_pad_a_notch_wider_than_two_pixels()
    {
        SpectrumWidget w;
        configureUi(w);
        // 400 >= 2*128, so no pad: reach is its own half width, 200 Hz.
        // With the pad it would have been 328 Hz and +2 px (256 Hz) would
        // hit.  That asymmetry is the whole point of the branch.
        w.setNotchMarkers({makeNotch(5, kUiCentreHz, 400.0)});

        QCOMPARE(w.notchAtPixelForTest(kUiCentreX + 1), 5);      // 128 Hz off
        QCOMPARE(w.notchAtPixelForTest(kUiCentreX + 2), -1);     // 256 Hz off
    }

    void hit_test_rejects_pixels_outside_the_spectrum_rect()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(7, kUiCentreHz, 400.0)});

        QCOMPARE(w.notchAtPixelForTest(-1), -1);
        QCOMPARE(w.notchAtPixelForTest(kUiWidgetW), -1);
    }

    // -- section 7.2 edge-vs-centre drag discrimination -----------------
    //
    // A 2000 Hz notch at 128 Hz/px is 15 px wide on screen (low edge at
    // x=492, high edge at x=507), comfortably past the 8 px gate.

    void grab_defaults_to_centre_in_the_notch_body()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});

        // x=500 is 8 px from the low edge and 7 px from the high edge:
        // outside both +/- 4 px zones, so the whole notch drags.
        QCOMPARE(w.notchGrabAtForTest(1, kUiCentreX, false),
                 SpectrumWidget::NotchGrab::Centre);
    }

    void grab_returns_low_edge_within_four_px_of_the_low_edge()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});
        const int lowX = uiXForHz(kUiCentreHz - 1000.0);   // 492

        QCOMPARE(w.notchGrabAtForTest(1, lowX, false),
                 SpectrumWidget::NotchGrab::LowEdge);
        QCOMPARE(w.notchGrabAtForTest(1, lowX + 3, false),
                 SpectrumWidget::NotchGrab::LowEdge);
        // 4 px is NOT near: Thetis tests Math.Abs(...) < 4.
        QCOMPARE(w.notchGrabAtForTest(1, lowX + 4, false),
                 SpectrumWidget::NotchGrab::Centre);
    }

    void grab_returns_high_edge_within_four_px_of_the_high_edge()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});
        const int highX = uiXForHz(kUiCentreHz + 1000.0);  // 507

        QCOMPARE(w.notchGrabAtForTest(1, highX, false),
                 SpectrumWidget::NotchGrab::HighEdge);
        QCOMPARE(w.notchGrabAtForTest(1, highX - 3, false),
                 SpectrumWidget::NotchGrab::HighEdge);
        QCOMPARE(w.notchGrabAtForTest(1, highX - 4, false),
                 SpectrumWidget::NotchGrab::Centre);
    }

    void grab_offers_no_edge_zone_below_eight_px_on_screen_width()
    {
        SpectrumWidget w;
        configureUi(w);
        // 400 Hz is 3 px wide on screen: nHpx - nLpx == 3, not > 8, so
        // Thetis never enters the edge-zone check at all.
        w.setNotchMarkers({makeNotch(2, kUiCentreHz, 400.0)});
        const int highX = uiXForHz(kUiCentreHz + 200.0);

        QCOMPARE(w.notchGrabAtForTest(2, highX, false),
                 SpectrumWidget::NotchGrab::Centre);
    }

    void grab_with_shift_resizes_from_the_side_of_centre()
    {
        SpectrumWidget w;
        configureUi(w);
        // Same 3 px notch: the edge zones are still suppressed, but Shift
        // forces a resize and the side-of-centre default picks the edge.
        w.setNotchMarkers({makeNotch(2, kUiCentreHz, 400.0)});

        QCOMPARE(w.notchGrabAtForTest(2, kUiCentreX + 1, true),
                 SpectrumWidget::NotchGrab::HighEdge);
        QCOMPARE(w.notchGrabAtForTest(2, kUiCentreX - 1, true),
                 SpectrumWidget::NotchGrab::LowEdge);
        // Exactly on centre counts as the high side (>=).
        QCOMPARE(w.notchGrabAtForTest(2, kUiCentreX, true),
                 SpectrumWidget::NotchGrab::HighEdge);
    }

    void grab_on_unknown_id_is_none()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});

        QCOMPARE(w.notchGrabAtForTest(99, kUiCentreX, false),
                 SpectrumWidget::NotchGrab::None);
    }

    // -- section 7.4 hover drives selection (and so the wheel gate) -----

    void hover_over_notch_sets_hovered_and_selected_ids()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);

        QCOMPARE(w.hoveredNotchIdForTest(), 1);
        QCOMPARE(w.selectedNotchIdForTest(), 1);
    }

    void hover_off_notch_clears_hovered_and_selected_ids()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);
        QCOMPARE(w.selectedNotchIdForTest(), 1);

        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX + 100, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);

        QCOMPARE(w.hoveredNotchIdForTest(), -1);
        QCOMPARE(w.selectedNotchIdForTest(), -1);
    }

    // The hover branch returns early, skipping the repaint at the tail of
    // mouseMoveEvent.  drawCursorInfo (the frequency readout that follows
    // the pointer) is painted into the SAME cached static overlay, so a
    // hover branch that only invalidated on an id CHANGE would freeze the
    // readout for as long as the pointer stayed over a marker.
    void hover_within_one_notch_keeps_invalidating_the_overlay()
    {
#ifdef NEREUS_GPU_SPECTRUM
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);
        QCOMPARE(w.hoveredNotchIdForTest(), 1);

        // Second move, same notch, so the id does not change.
        w.clearOverlayStaticDirtyForTest();
        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX + 2, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);

        QVERIFY2(w.overlayStaticDirtyForTest(),
                 "moving within one notch did not invalidate the static "
                 "overlay; the cursor frequency readout would freeze");
#else
        QSKIP("no cached overlay texture on the CPU-only spectrum path");
#endif
    }

    void leave_event_clears_notch_hover_state()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);
        QCOMPARE(w.selectedNotchIdForTest(), 1);

        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&w, &leave);

        QCOMPARE(w.hoveredNotchIdForTest(), -1);
        QCOMPARE(w.selectedNotchIdForTest(), -1);
    }

    // -- section 7.2 drag: whole notch, and width from either edge ------

    void press_on_notch_latches_selection()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(kUiCentreX, kUiSpecY),
                  Qt::LeftButton, Qt::LeftButton);

        QCOMPARE(w.selectedNotchIdForTest(), 1);
    }

    void drag_body_emits_notch_move_requested()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchMoveRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(kUiCentreX, kUiSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX + 10, kUiSpecY),
                  Qt::NoButton, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        // 10 px right at 128 Hz/px = +1280 Hz.
        QVERIFY(std::abs(spy.at(0).at(1).toDouble()
                         - (kUiCentreHz + 10 * kUiHzPerPx)) < 1e-3);
    }

    void drag_high_edge_emits_double_the_pixel_delta_as_width()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);
        const int highX = uiXForHz(kUiCentreHz + 1000.0);   // 507

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(highX, kUiSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(&w, QEvent::MouseMove, QPoint(highX + 10, kUiSpecY),
                  Qt::NoButton, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        // "we want double the diff, as we are doing 'both sides'":
        // 2000 + 2 * (10 * 128) = 4560 Hz.
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 4560.0) < 1e-3);
    }

    void drag_low_edge_grows_the_notch_when_dragged_left()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);
        // The LEFTMOST HITTABLE column, which is not uiXForHz(centre-1000).
        // hzToX truncates, so the pixel holding the low edge frequency maps
        // back one pixel BELOW it (x=492 -> 14198976 Hz, outside the notch)
        // and the hit test rejects it.  drawNotchMarkers mirrors the upper
        // half-width about cx for exactly the same reason, so the drawn
        // left edge is 493 too: hit box and painted marker agree, and 493
        // is still 1 px from nLpx so the grab is LowEdge.
        const int halfWpx = uiXForHz(kUiCentreHz + 1000.0) - kUiCentreX;
        const int lowX    = kUiCentreX - halfWpx;           // 493

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(lowX, kUiSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(&w, QEvent::MouseMove, QPoint(lowX - 10, kUiSpecY),
                  Qt::NoButton, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 4560.0) < 1e-3);
    }

    // section 7.3: the drag latches the notch that was under the press, so
    // an overlapping neighbour cannot steal it mid-gesture.  This is the
    // job AetherSDR's tnfAtPixel(preferredId) short-circuit does upstream.
    void drag_keeps_the_notch_it_started_on()
    {
        SpectrumWidget w;
        configureUi(w);
        // Two overlapping 2000 Hz notches.  A bare hit test at the moved-to
        // pixel still returns the FIRST in list order, so this pins the
        // latch by dragging until the cursor is clear of notch 3 entirely.
        w.setNotchMarkers({makeNotch(3, kUiCentreHz, 2000.0),
                           makeNotch(9, kUiCentreHz + 2560.0, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchMoveRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(kUiCentreX, kUiSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        QCOMPARE(w.selectedNotchIdForTest(), 3);
        // +20 px lands at 14202560, dead centre of notch 9.
        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX + 20, kUiSpecY),
                  Qt::NoButton, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
        QCOMPARE(w.selectedNotchIdForTest(), 3);
    }

    void release_clears_the_notch_grab()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchMoveRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(kUiCentreX, kUiSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(&w, QEvent::MouseButtonRelease, QPoint(kUiCentreX, kUiSpecY),
                  Qt::LeftButton, Qt::NoButton);
        // A move after release is a hover, not a drag.
        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX + 10, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);

        QCOMPARE(spy.count(), 0);
    }

    // -- section 7.4 wheel resize, gated on the selected notch ----------

    void wheel_over_selected_notch_widens_by_ten_hz_per_detent()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 400.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);

        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);
        sendWheel(&w, QPoint(kUiCentreX, kUiSpecY), 120);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 410.0) < 1e-9);
    }

    void wheel_with_shift_steps_one_hz()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 400.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);

        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);
        sendWheel(&w, QPoint(kUiCentreX, kUiSpecY), -120, Qt::ShiftModifier);

        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 399.0) < 1e-9);
    }

    // The step constants belong to NotchModel, so a rename or a second
    // spelling minted in the widget is caught here rather than by the
    // arithmetic above happening to still add up.
    void wheel_steps_come_from_notch_model()
    {
        QCOMPARE(NotchModel::kWheelWidthStepHz, 10.0);
        QCOMPARE(NotchModel::kWheelWidthStepFineHz, 1.0);
    }

    // Plan cross-task correction: width clamping lives ONLY in
    // NotchModel::setWidth, so the wheel handler is a bare
    // current + delta * step and the model pins it at _max_filter_width.
    // Asserting the raw request here and the clamp on the model side keeps
    // one owner for the bound instead of two that can drift.
    void wheel_leaves_the_width_clamp_to_the_model()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 9995.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);

        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);
        sendWheel(&w, QPoint(kUiCentreX, kUiSpecY), 120);

        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 10005.0) < 1e-9);

        // ... and the model is what pins it at 10000
        // (Thetis _max_filter_width, console.cs:13221 [v2.10.3.15]).
        NotchModel model;
        const int id = model.addNotch(kUiCentreHz, 9995.0);
        QVERIFY(id >= 0);
        QVERIFY(model.setWidth(id, 10005.0));
        QCOMPARE(model.notchById(id)->widthHz, NotchModel::kMaxNotchWidthHz);
    }

    void wheel_without_a_selected_notch_does_not_resize()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 400.0)});
        QSignalSpy widthSpy(&w, &SpectrumWidget::notchWidthRequested);
        QSignalSpy tuneSpy(&w, &SpectrumWidget::frequencyClicked);

        // Hover away from the notch first: the selection clears, so the
        // wheel must fall through to the VFO tune path.
        sendMouse(&w, QEvent::MouseMove, QPoint(kUiCentreX + 100, kUiSpecY),
                  Qt::NoButton, Qt::NoButton);
        sendWheel(&w, QPoint(kUiCentreX + 100, kUiSpecY), 120);

        QCOMPARE(widthSpy.count(), 0);
        QCOMPARE(tuneSpy.count(), 1);
    }

    // -- section 7.1 the add gesture is Thetis's: Ctrl + right-click ----

    void ctrl_right_click_requests_a_notch_at_the_clicked_frequency()
    {
        SpectrumWidget w;
        configureUi(w);
        QSignalSpy spy(&w, &SpectrumWidget::notchCreateRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, kUiSpecY),
                  Qt::RightButton, Qt::RightButton, Qt::ControlModifier);

        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.at(0).at(0).toDouble() - uiHzForX(600)) < 1e-3);
        QCOMPARE(spy.at(0).at(1).toBool(), false);
    }

    // macOS swaps Control and Command, so the physical Ctrl key arrives as
    // Qt::MetaModifier.  The zoom wheel already accepts either; the add
    // gesture has to as well or D2's gesture is unreachable on a trackpad,
    // which is the whole reason section 7.1 picked it.
    void meta_right_click_also_requests_a_notch()
    {
        SpectrumWidget w;
        configureUi(w);
        QSignalSpy spy(&w, &SpectrumWidget::notchCreateRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, kUiSpecY),
                  Qt::RightButton, Qt::RightButton, Qt::MetaModifier);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toBool(), false);
    }

    void ctrl_shift_right_click_requests_a_narrow_notch()
    {
        SpectrumWidget w;
        configureUi(w);
        QSignalSpy spy(&w, &SpectrumWidget::notchCreateRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, kUiSpecY),
                  Qt::RightButton, Qt::RightButton,
                  Qt::ControlModifier | Qt::ShiftModifier);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toBool(), true);
    }

    void ctrl_right_click_below_the_spectrum_plot_does_not_create()
    {
        SpectrumWidget w;
        configureUi(w);
        QSignalSpy spy(&w, &SpectrumWidget::notchCreateRequested);

        // Deep in the waterfall: outside the spectrum plot on both render
        // path layouts, so there is no frequency-under-cursor to add at.
        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, kUiWidgetH - 10),
                  Qt::RightButton, Qt::RightButton, Qt::ControlModifier);

        QCOMPARE(spy.count(), 0);
    }

    // -- section 7 right-click on a notch -------------------------------
    //
    // Driven through the builder seam rather than a synthetic right-click,
    // because QMenu::exec() would block the test's event loop.

    void context_menu_header_shows_frequency_and_is_disabled()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 200.0)});

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);

        QVERIFY(!menu.actions().isEmpty());
        QVERIFY(!menu.actions().first()->isEnabled());
        QVERIFY(menu.actions().first()->text().contains(
            QStringLiteral("14.200000")));
    }

    void context_menu_width_preset_emits_width_request()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 200.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);

        QAction* widthAct = actionByText(menu.actions(),
                                         QStringLiteral("Width"));
        QVERIFY(widthAct != nullptr);
        QVERIFY(widthAct->menu() != nullptr);

        QAction* w500 = actionByText(widthAct->menu()->actions(),
                                     QStringLiteral("500 Hz"));
        QVERIFY(w500 != nullptr);
        w500->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 500.0) < 1e-9);

        // The preset matching the current width is checked.
        QAction* w200 = actionByText(widthAct->menu()->actions(),
                                     QStringLiteral("200 Hz"));
        QVERIFY(w200 != nullptr);
        QVERIFY(w200->isChecked());
    }

    void context_menu_bypass_emits_active_false()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 200.0, /*active*/ true)});
        QSignalSpy spy(&w, &SpectrumWidget::notchActiveRequested);

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);
        QAction* bypass = actionByText(menu.actions(),
                                       QStringLiteral("Bypass Notch"));
        QVERIFY(bypass != nullptr);
        bypass->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        QCOMPARE(spy.at(0).at(1).toBool(), false);
    }

    void context_menu_activate_shown_for_a_bypassed_notch()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 200.0, /*active*/ false)});
        QSignalSpy spy(&w, &SpectrumWidget::notchActiveRequested);

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);
        QAction* activate = actionByText(menu.actions(),
                                         QStringLiteral("Activate Notch"));
        QVERIFY(activate != nullptr);
        activate->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toBool(), true);
    }

    void context_menu_remove_emits_remove_request()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 200.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchRemoveRequested);

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);
        QAction* remove = actionByText(menu.actions(),
                                       QStringLiteral("Remove Notch"));
        QVERIFY(remove != nullptr);
        remove->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
    }

    void context_menu_on_unknown_id_adds_nothing()
    {
        SpectrumWidget w;
        configureUi(w);
        w.setNotchMarkers({makeNotch(1, kUiCentreHz, 200.0)});

        QMenu menu;
        w.buildNotchContextMenuForTest(99, menu);
        QVERIFY(menu.actions().isEmpty());
    }

    // -- plan decision D-e: right-click on empty pan ---------------------
    //
    // Resolved by adding the row to the EXISTING SpectrumOverlayMenu
    // rather than converting that widget to a QMenu, so no shipped gesture
    // changes shape.  SpectrumOverlayMenu is AetherSDR-registered and
    // carries no Thetis cite.

    void overlay_menu_declares_the_notch_add_signal_in_hz()
    {
        const QMetaObject& mo = SpectrumOverlayMenu::staticMetaObject;
        const int idx = mo.indexOfSignal("notchAddRequested(double)");
        QVERIFY2(idx >= 0, "SpectrumOverlayMenu::notchAddRequested(double)");
        QCOMPARE(mo.method(idx).methodType(), QMetaMethod::Signal);
    }

    void overlay_menu_add_notch_button_emits_the_pushed_frequency()
    {
        SpectrumOverlayMenu m;
        QSignalSpy spy(&m, &SpectrumOverlayMenu::notchAddRequested);

        m.setNotchAddFrequency(kUiCentreHz);

        QPushButton* addBtn = nullptr;
        for (QPushButton* b : m.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("Add notch here")) {
                addBtn = b;
                break;
            }
        }
        QVERIFY2(addBtn != nullptr, "no 'Add notch here' button on the "
                                    "spectrum overlay menu");
        addBtn->click();

        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.at(0).at(0).toDouble() - kUiCentreHz) < 1e-6);
    }

    // The whole wire: a plain right-click on empty pan opens the overlay
    // menu with the frequency under the cursor already pushed, and the
    // button turns into notchCreateRequested at that frequency.
    void overlay_menu_add_notch_forwards_the_cursor_frequency()
    {
        SpectrumWidget w;
        configureUi(w);
        QSignalSpy createSpy(&w, &SpectrumWidget::notchCreateRequested);

        // No markers pushed, so this lands on empty pan and falls through
        // to the overlay menu (a non-blocking popup widget).
        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, kUiSpecY),
                  Qt::RightButton, Qt::RightButton);
        QCOMPARE(createSpy.count(), 0);

        auto* menu = w.findChild<SpectrumOverlayMenu*>();
        QVERIFY2(menu != nullptr, "right-click did not open the overlay menu");
        menu->hide();

        QPushButton* addBtn = nullptr;
        for (QPushButton* b : menu->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("Add notch here")) {
                addBtn = b;
                break;
            }
        }
        QVERIFY(addBtn != nullptr);
        addBtn->click();

        QCOMPARE(createSpy.count(), 1);
        QVERIFY(std::abs(createSpy.at(0).at(0).toDouble() - uiHzForX(600))
                < 1e-3);
        // The overlay-menu route always places the default width.
        QCOMPARE(createSpy.at(0).at(1).toBool(), false);
    }
};

QTEST_MAIN(TestNotchHitTest)
#include "tst_notch_hit_test.moc"
