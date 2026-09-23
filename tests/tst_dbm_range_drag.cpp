// tests/tst_dbm_range_drag.cpp
//
// 3D stacked-trace spectrum plan, Task 19: Ctrl-drag (Cmd/Meta too) on the
// right-edge dBm strip zooms the amplitude span with the bottom pinned,
// instead of panning it the way the plain drag (m_draggingDbm, pinned by
// tst_dss_floor_drag.cpp) does. Ported from AetherSDR's m_draggingDbmRange
// gesture (SpectrumWidget.cpp:3365, 9537, 10493-10501, 10893-10926
// [@1872028c]) -- see SpectrumWidget.cpp's own Task 19 comments for the
// exact upstream citations at each of the three sites this file exercises
// (press/move/release).
//
// 3D DECISION: upstream's own m_draggingDbmRange move handler has NO is3D
// branch anywhere (contrast m_draggingDssFloor two arms below it in the
// press handler, which upstream DOES gate on is3D) -- it always recomputes
// both m_dynamicRange and m_refLevel, unconditionally, regardless of mode.
// NereusSDR follows that: the same code path runs in 2D and 3D. This is
// not a no-op in 3D even though m_refLevel has no 3D reader -- dssSpanDb()
// (the 3D surface's amplitude span) reads m_dynamicRange directly, so 3D
// genuinely zooms. ctrlDragIn3D_zoomsSpanAndKeepsBottomAnchored below pins
// this specifically: it would fail if the 3D branch skipped the
// m_refLevel recompute (the alternative choice), since the bottom-anchor
// invariant depends on refLevel and dynamicRange moving together.
//
// Each test below states the wrong implementation it catches. All were
// confirmed by mutation (introduce the described bug in SpectrumWidget.cpp,
// rebuild, confirm the named test fails; revert, confirm green again) per
// this epic's test-quality bar -- see the design doc addendum and the task
// 19 report for the mutation log. Five earlier tests in this epic reached
// main unable to fail; this file's mutation pass is the guard against a
// sixth.

#include <QtTest/QtTest>
#include <QMouseEvent>
#include <cmath>

#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

// Same idiom as tst_dss_floor_drag.cpp's sendMouse()/dragDbmStrip(), with a
// modifiers parameter added so the Ctrl/Meta-gated range-zoom gesture can be
// driven alongside the plain (NoModifier) drag it pins for regression.
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos,
               Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   button, buttons, modifiers);
    QApplication::sendEvent(w, &me);
}

// Press, move by dy pixels, release -- at the dBm strip's drag zone (below
// the kDbmArrowH-tall up/down arrow row). mx sits mid-strip; y0 is fixed
// comfortably below the arrow row. dy may be negative (drag up) or exceed
// the widget height (the move handlers never re-check widget bounds once a
// drag has latched, matching m_draggingDbm's own move handler).
void dragDbmStrip(SpectrumWidget& w, int dy,
                   Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const int mx = w.width() - 18;   // centre of the 36px-wide strip
    const int y0 = 60;               // clear of the 14px arrow row
    sendMouse(&w, QEvent::MouseButtonPress, QPoint(mx, y0),
              Qt::LeftButton, Qt::LeftButton, modifiers);
    sendMouse(&w, QEvent::MouseMove, QPoint(mx, y0 + dy),
              Qt::NoButton, Qt::LeftButton, modifiers);
    sendMouse(&w, QEvent::MouseButtonRelease, QPoint(mx, y0 + dy),
              Qt::LeftButton, Qt::NoButton, modifiers);
}

// Same fixture as tst_dss_floor_drag.cpp's setUpWidget(): resize() before
// anything else (mousePressEvent's dBm-strip hit test reads width()/
// height(), both 0 on an unresized widget -- the WIDGET TEST TRAP this plan
// has hit twice), show() + qWaitForWindowExposed() (matches every other
// SpectrumWidget mouse test in this suite), and setConnectionState(Connected)
// (mousePressEvent's Phase 3Q-8 guard swallows every left-click while not
// Connected). 1000x1000 keeps specH large under either of specHFromHeight's
// two layout formulas (GPU vs CPU-only build).
void setUpWidget(SpectrumWidget& w)
{
    w.resize(1000, 1000);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.setConnectionState(ConnectionState::Connected);
}

} // namespace

class TestDbmRangeDrag : public QObject {
    Q_OBJECT

private slots:
    // Wrong implementations this catches:
    //  - Ctrl not wired at all (falls through to the plain drag instead):
    //    dynamicRange() would not move from 100.
    //  - top-anchored instead of bottom-anchored (copy of the plain drag's
    //    anchor instead of the range-drag's own): the bottom invariant
    //    would fail even though dynamicRange did change.
    //  - integer division of dy/dragHeight before the float range multiply
    //    (missing static_cast<float>): deltaDb truncates to 0 for any drag
    //    smaller than a full strip height, so dynamicRange would stay at
    //    (or very near) 100 instead of landing near 50.
    //  - a sign error: dynamicRange would GROW instead of shrink for a
    //    downward drag.
    void ctrlDragIn2D_zoomsDynamicRangeAndAnchorsBottom()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.setDbmRange(-120.0f, -20.0f);   // refLevel=-20, dynamicRange=100, bottom=-120
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        const int dyArg = specH / 2;   // downward drag: shrinks the span
        dragDbmStrip(w, dyArg, Qt::ControlModifier);

        const float expectedDelta =
            -(static_cast<float>(dyArg) / static_cast<float>(specH)) * 100.0f;
        const float expectedRange = 100.0f + expectedDelta;
        QVERIFY(std::abs(w.dynamicRange() - expectedRange) < 0.01f);
        // Sanity: this drag must land well clear of the clamp floor (10) so
        // it is actually exercising the linear formula, not the clamp --
        // ctrlDragRange_clampsAtMinimum below owns the clamp itself.
        QVERIFY(w.dynamicRange() > 20.0f);
        QVERIFY(w.dynamicRange() < 90.0f);

        const float bottomAfter = w.refLevel() - w.dynamicRange();
        QVERIFY(std::abs(bottomAfter - (-120.0f)) < 0.01f);
    }

    // Wrong implementations this catches:
    //  - my Task 19 press-handler insertion accidentally short-circuits or
    //    reorders the pre-existing plain-drag path (e.g. placed the Ctrl
    //    check so it swallows a NoModifier press too, or broke the
    //    arrowRow/body fallthrough): refLevel would stay frozen instead of
    //    moving per Task 16's unchanged 2D formula.
    //  - the new m_draggingDbmRange move-handler block accidentally fires
    //    (or bleeds state) on a plain drag: dynamicRange() would move off
    //    its seeded 100.
    void plainDragIn2D_doesNotChangeDynamicRange()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.setDbmRange(-120.0f, -20.0f);
        const float dynRangeBefore = w.dynamicRange();
        const float refBefore = w.refLevel();
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        const int dyArg = specH / 6;
        dragDbmStrip(w, dyArg);   // NoModifier: plain drag

        QCOMPARE(w.dynamicRange(), dynRangeBefore);

        // Task 16's pre-existing 2D formula, unchanged -- re-pinned here
        // (not just "did it move") so a Task 19 edit that nicked the
        // neighbouring plain-drag arithmetic would also be caught.
        const float expectedRef = qBound(-160.0f,
            refBefore + static_cast<float>(dyArg) * dynRangeBefore
                / static_cast<float>(specH),
            20.0f);
        QVERIFY(std::abs(w.refLevel() - expectedRef) < 0.01f);
    }

    // Wrong implementations this catches: same as the 2D sibling above, but
    // for the 3D arm of the plain drag (Task 16). Also re-pins Task 16's own
    // exact scale (specH/6 -> 24/6=4 units) and its refLevel-frozen
    // invariant, so a Task 19 edit that disturbed the neighbouring 3D
    // branch would be caught here even though Task 19 does not intend to
    // touch it.
    void plainDragIn3D_doesNotChangeDynamicRange()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-120.0f, -20.0f);
        w.setDssFloorDepth(10);
        const float dynRangeBefore = w.dynamicRange();
        const float refBefore = w.refLevel();
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        dragDbmStrip(w, specH / 6);   // NoModifier: plain drag

        QCOMPARE(w.dynamicRange(), dynRangeBefore);
        QCOMPARE(w.dssFloorDepth(), 14);
        QCOMPARE(w.refLevel(), refBefore);
    }

    // Wrong implementation this catches: a missing or too-weak floor clamp
    // (e.g. reusing setDssFloorDepth's [0,24] bound by copy-paste mistake,
    // or omitting clampDbmRangeForBottom entirely) -- a full-height drag
    // would then drive dynamicRange to 0 (or negative) instead of
    // saturating at upstream's kMinDisplayRangeDb=10.
    void ctrlDragRange_clampsAtMinimum()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.setDbmRange(-120.0f, -20.0f);
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        // Full strip height, straight down: dy/dragHeight lands on exactly
        // -1.0 (integer -specH over integer specH, exact in float for any
        // realistic pixel count), so the pre-clamp value is exactly
        // 100 + (-1.0 * 100) = 0 -- well past the 10 dB floor regardless of
        // any small pixel-rounding slop.
        dragDbmStrip(w, specH, Qt::ControlModifier);

        QCOMPARE(w.dynamicRange(), 10.0f);
        QCOMPARE(w.refLevel(), -110.0f);   // bottom (-120) + clamped range (10)
    }

    // Wrong implementation this catches: an unbounded (or wrongly-bounded)
    // ceiling -- e.g. reusing the sibling wheel-zoom gesture's local 200
    // bound (qBound(10.0f, ..., 200.0f), a few hundred lines below in
    // wheelEvent) instead of porting upstream's own kMaxDisplayRangeDb=180.
    // A copy-paste of 200 here would make this test observe 200, not 180,
    // and fail.
    void ctrlDragRange_clampsAtMaximum()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.setDbmRange(-120.0f, -20.0f);
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        // Full strip height, straight up: pre-clamp value is exactly
        // 100 + (+1.0 * 100) = 200 -- past the 180 dB ceiling with the same
        // exact-ratio reasoning as the minimum test above.
        dragDbmStrip(w, -specH, Qt::ControlModifier);

        QCOMPARE(w.dynamicRange(), 180.0f);
        QCOMPARE(w.refLevel(), 60.0f);   // bottom (-120) + clamped range (180)
    }

    // THE 3D-DECISION TEST. Wrong implementation this catches: choice (a)
    // from the task ("skip the meaningless refLevel recompute in 3D") --
    // that alternative would leave refLevel() frozen at its pre-drag value
    // while dynamicRange() still changes, breaking the bottom-anchor
    // invariant this test checks. Also catches Task 19's 3D branch getting
    // cross-wired into Task 16's dssFloorDepth (dssFloorDepth would move
    // off its seeded 10 instead of dynamicRange changing).
    void ctrlDragIn3D_zoomsSpanAndKeepsBottomAnchored()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-120.0f, -20.0f);
        w.setDssFloorDepth(10);
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        dragDbmStrip(w, specH / 2, Qt::ControlModifier);

        // dssSpanDb() reads m_dynamicRange directly, so the 3D surface's
        // span genuinely changed -- this is the reason Ctrl-drag is wired
        // identically in both modes rather than being suppressed in 3D.
        QVERIFY(w.dynamicRange() > 20.0f);
        QVERIFY(w.dynamicRange() < 90.0f);

        // Orthogonal to Task 16's gesture: 3D Floor must not move.
        QCOMPARE(w.dssFloorDepth(), 10);

        // The bottom-anchor invariant: would fail under the "skip the
        // recompute in 3D" alternative, since refLevel() would still be
        // -20 while dynamicRange() shrank, making this difference != -120.
        const float bottomAfter = w.refLevel() - w.dynamicRange();
        QVERIFY(std::abs(bottomAfter - (-120.0f)) < 0.01f);
    }

    // Wrong implementation this catches: gating the press-handler check on
    // Qt::ControlModifier alone (the upstream #ifdef Q_OS_MAC / non-mac
    // split, ported literally, would only accept Meta on a build compiled
    // with Q_OS_MAC defined). NereusSDR's chosen simplification -- checked
    // against the existing notch-add / wheel-zoom modifier idioms already
    // in this file -- accepts Ctrl OR Meta unconditionally on every
    // platform, so Meta alone must engage the gesture in this test binary
    // regardless of which OS it was built on.
    void metaModifierAloneAlsoEngagesRangeDrag()
    {
        SpectrumWidget w;
        setUpWidget(w);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.setDbmRange(-120.0f, -20.0f);
        const int specH = w.notchSpecRectForTest().height();
        QVERIFY(specH > 60);

        dragDbmStrip(w, specH / 2, Qt::MetaModifier);   // Meta only, no Control

        QVERIFY(w.dynamicRange() > 20.0f);
        QVERIFY(w.dynamicRange() < 90.0f);
    }
};

QTEST_MAIN(TestDbmRangeDrag)
#include "tst_dbm_range_drag.moc"
