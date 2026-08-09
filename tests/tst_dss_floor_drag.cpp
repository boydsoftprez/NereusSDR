// tests/tst_dss_floor_drag.cpp
//
// no-port-check: NereusSDR-original mouse gesture (3D Stacked-Trace
// Spectrum Plan Task 16, bench fix). Upstream AetherSDR's 3D Floor has no
// drag binding at all -- its dBm-strip drag-pan gesture only ever moves
// the Ref Level equivalent -- so there is no upstream logic to port for
// the 3D branch this file pins. See design doc addendum "Tasks 16-18" in
// docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md.
//
// Bug (bench report): "when I grab and try to slide the dB scale on the
// right up and down that does not seem to work on the 3D view". Root
// cause: the plain vertical drag on the right-edge dBm strip always wrote
// m_refLevel (SpectrumWidget.cpp, m_draggingDbm), which the 3D surface
// never reads -- it anchors to dssFloorDbm() == m_nfLerpAverage -
// m_dssFloorDepth instead (see that function). The gesture ran, mutated a
// variable nothing looked at, and the 3D view had no opinion about it.
//
// Fix: in 3D mode the same drag now targets dssFloorDepth() instead of
// refLevel(), with the full strip height mapped to the full 0..24 range
// (NOT reused from 2D's m_dynamicRange/specH, which is tuned for a much
// wider span -- -160..+20 -- and would saturate the travel within a few
// percent of the strip). 2D is gated off entirely so its existing formula
// and output are untouched.
//
// Each "not inverted" test below pins BOTH halves of the mode gate at
// once (the value that must move, and the value that must NOT), because
// that is specifically what an inverted mode branch gets backwards -- a
// branch swap makes a 3D drag move refLevel() instead and a 2D drag move
// dssFloorDepth() instead, which the paired QCOMPARE calls catch either
// way. Confirmed by mutation: temporarily inverting the
// `m_spectrumRenderMode == SpectrumRenderMode::Mode3D` check in
// SpectrumWidget.cpp fails dragDownIn3D_movesFloorDepthNotRefLevel,
// dragUpIn3D_movesFloorDepthDownNotRefLevel AND
// dragDownIn2D_movesRefLevelNotFloorDepth.

#include <QtTest/QtTest>
#include <QMouseEvent>
#include <cmath>

#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

// Same idiom as tst_dss_overlay_menu.cpp's sendMouse() (itself following
// tst_notch_hit_test.cpp's original): a directly synthesized QMouseEvent
// via QApplication::sendEvent is this project's established way to drive
// SpectrumWidget's mouse handlers headlessly. QTest::mouseMove with no
// button held never reaches the widget at all.
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos,
               Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   button, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &me);
}

// Press, move by dy pixels, release -- at the dBm strip's drag-pan zone
// (below the kDbmArrowH-tall up/down arrow row, so the press starts a
// drag-pan rather than an arrow click; see SpectrumWidget.cpp's "1. dBm
// scale strip" mousePressEvent block). mx sits mid-strip; y0 is fixed
// comfortably below the arrow row.
void dragDbmStrip(SpectrumWidget& w, int dy)
{
    const int mx = w.width() - 18;   // centre of the 36px-wide strip
    const int y0 = 60;               // clear of the 14px arrow row
    sendMouse(&w, QEvent::MouseButtonPress, QPoint(mx, y0),
              Qt::LeftButton, Qt::LeftButton);
    sendMouse(&w, QEvent::MouseMove, QPoint(mx, y0 + dy),
              Qt::NoButton, Qt::LeftButton);
    sendMouse(&w, QEvent::MouseButtonRelease, QPoint(mx, y0 + dy),
              Qt::LeftButton, Qt::NoButton);
}

// Common fixture every test needs:
//  - resize() before anything else: mousePressEvent's dBm-strip hit test
//    reads width()/height(), both 0 on an unresized widget, so the press
//    would silently miss the strip and m_draggingDbm would never latch --
//    the WIDGET TEST TRAP this plan has already hit twice, here in a new
//    shape (a missed hit-test rather than a null waterfall image).
//  - show() + qWaitForWindowExposed(): matches every other SpectrumWidget
//    mouse test in this suite (tst_dss_overlay_menu.cpp,
//    tst_notch_hit_test.cpp, tst_dss_floor_and_span.cpp).
//  - setConnectionState(Connected): mousePressEvent's Phase 3Q-8 guard
//    swallows every left-click and returns before the dBm-strip check
//    while not Connected (see tst_notch_hit_test.cpp's configureUi(),
//    same trap).
// 1000x1000 keeps specH large under EITHER of specHFromHeight's two
// layout formulas (GPU vs CPU-only build), so the specH/6 drag the scale
// tests below use rounds to within a small fraction of one depth unit of
// the exact 24/6 = 4 target regardless of which formula is active.
void setUpWidget(SpectrumWidget& w)
{
    w.resize(1000, 1000);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.setConnectionState(ConnectionState::Connected);
}

} // namespace

class TestDssFloorDrag : public QObject {
    Q_OBJECT

private slots:
    // Wrong implementation this catches: the mode branch missing, or
    // inverted so a 3D drag falls into the 2D arm -- either leaves
    // dssFloorDepth() frozen at its seeded 10 and moves refLevel()
    // instead, failing the FIRST QCOMPARE (wrong branch entirely) or the
    // SECOND (refLevel leaking a change it must not have in 3D mode).
    // Also catches the WRONG SCALE (2D's m_dynamicRange/specH reused
    // as-is): with the default 68 dB dynamicRange, a specH/6 drag would
    // move ~11 units and saturate near 24 instead of landing on 14.
    void dragDownIn3D_movesFloorDepthNotRefLevel()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssFloorDepth(10);
        const float refBefore = w.refLevel();
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);   // sanity: fixture assumes headroom below y0

        // specH/6 cancels specH out of the expected delta: ANY correct
        // "map the full strip height to the full 0..24 range" mapping
        // moves exactly 24/6 = 4 units for a 1/6-height drag, regardless
        // of the actual specH pixel count.
        dragDbmStrip(w, specH / 6);

        QCOMPARE(w.dssFloorDepth(), 14);
        QCOMPARE(w.refLevel(), refBefore);
    }

    // Wrong implementation this catches: a sign error in the 3D branch
    // (e.g. subtracting the depth delta on a downward drag, or applying
    // it un-negated on an upward one) -- would move depth to 14 on an
    // UPWARD drag instead of down to 6, exactly backwards from the
    // required "drag down reveals more noise, drag up hides it".
    void dragUpIn3D_movesFloorDepthDownNotRefLevel()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssFloorDepth(10);
        const float refBefore = w.refLevel();
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        dragDbmStrip(w, -(specH / 6));

        QCOMPARE(w.dssFloorDepth(), 6);
        QCOMPARE(w.refLevel(), refBefore);
    }

    // Wrong implementation this catches: the mode branch inverted so a 2D
    // drag falls into the 3D arm -- dssFloorDepth() would drift off its
    // seeded 10 and refLevel() would stay frozen, failing both
    // assertions. The exact numeric pin on refLevel (not just "it
    // changed") additionally catches an edit that disturbed the
    // pre-existing 2D formula while adding the branch around it -- e.g. a
    // stray change to dbPerPixel or the qBound clamp arguments.
    void dragDownIn2D_movesRefLevelNotFloorDepth()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.setDssFloorDepth(10);
        const float refBefore = w.refLevel();
        const float dynRange = w.dynamicRange();
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        const int dy = specH / 6;
        dragDbmStrip(w, dy);

        const float expectedRef = qBound(-160.0f,
            refBefore + static_cast<float>(dy) * dynRange
                / static_cast<float>(specH),
            20.0f);
        QVERIFY(std::abs(w.refLevel() - expectedRef) < 0.01f);
        QCOMPARE(w.dssFloorDepth(), 10);
    }

    // Point 3 of the fix: the 0..24 bound must hold under the NEW 3D
    // mapping, not merely in setDssFloorDepth()'s own clamp considered in
    // isolation. Wrong implementation this catches: a drag handler that
    // bypassed setDssFloorDepth() (writing m_dssFloorDepth directly, as
    // the 2D arm writes m_refLevel directly) or that reimplemented the
    // clamp with the wrong bound would run past 0/24 here instead of
    // saturating exactly at them.
    void dragIn3D_saturatesAtZeroAndTwentyFour()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));

        w.setDssFloorDepth(12);
        dragDbmStrip(w, 100000);   // far more than 24 units at any specH
        QCOMPARE(w.dssFloorDepth(), 24);

        w.setDssFloorDepth(12);
        dragDbmStrip(w, -100000);
        QCOMPARE(w.dssFloorDepth(), 0);
    }
};

QTEST_MAIN(TestDssFloorDrag)
#include "tst_dss_floor_drag.moc"
