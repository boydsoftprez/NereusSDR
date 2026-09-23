// =================================================================
// tests/tst_pan_flag_positions.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F: a pan hosting several slices must place each flag at ITS OWN
// slice frequency.
//
// Bench-reported 2026-07-28: "If I add B flag to panadapter 1, A and B are
// still overlaid and stuck on top of each other." The models were already
// correct -- tst_radio_model_slice_lifecycle pins that two slices sharing one
// DDC window hold independent frequencies and independent shift offsets. The
// defect was purely in placement: updateVfoPositions() derived ONE x from the
// pan's single m_vfoHz and moved every flag to it, so the flags tracked
// whichever slice most recently called setVfoFrequency rather than their own.
//
// Second bench report, same shape one step further in: "The pass band of the
// second flag disappears when not active. Let's keep it." drawVfoMarker()
// painted ONE marker per pan from that same m_vfoHz plus the pan's single
// m_filterLowHz/m_filterHighHz pair, so only the most recently tuned slice
// got a shaded passband. The geometry half of that decision now lives in
// SpectrumWidget::sliceMarkerGeometry(), which is what the marker cases below
// pin -- the pixel emission needs a shown QRhiWidget and stays untested.
//
// Third bench report, Sub-Epic J, same two flags: "with Slice A selected,
// Slice B's flag covered A's, clipping A's frequency readout to ".955.300"
// instead of "3.955.300"." Both flags were correctly PLACED by the fixes
// above; which one painted on TOP was still wrong. addVfoWidget's raise() at
// creation is a one-shot: updateVfoPositions() -- which runs every render
// frame, see its own comment -- separately raises every visible flag each
// pass in m_vfoWidgets' ascending slice-index order, which puts whichever
// slice has the HIGHER index on top after every frame regardless of which
// one is active. setFrontSliceIndex() is the pin that survives that loop.
// See SpectrumWidget.h/.cpp for the full rationale. Qt's sibling stacking
// order is the parent's QObject::children() list order (raise()/lower() are
// implemented by reordering it), so it is assertable here without a shown,
// rendering QRhiWidget.
//
// Fourth, the markers' colours. Every slice's centre line, edge lines and
// triangle were drawn in slice A's cyan, so with two slices on one pan the
// lines could not be told apart; only the flags carried the slice colour.
// Each marker now takes its own slice's colour, and the slices the operator
// has not selected draw darker with neutral grey edges (AetherSDR's current
// rule, picked by JJ 2026-09-23). The colours are part of
// sliceMarkerGeometry(), so they are pinned here with the rest of the
// marker decision.
// =================================================================
#include <QtTest/QtTest>
#include <QApplication>
#include <QPushButton>
#include <QSet>

#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

// 20 m window wide enough that two flags 100 kHz apart land far apart on
// screen and neither hits updatePosition()'s edge-clamp, which would mask a
// placement bug by pinning both to the same clamped x.
constexpr double kCentreHz    = 14'200'000.0;
constexpr double kSpanHz      =    192'000.0;
constexpr double kSliceAHz    = 14'150'000.0;
constexpr double kSliceBHz    = 14'250'000.0;
// Far outside the window above, so it exercises the off-window branch.
constexpr double kOffWindowHz = 14'900'000.0;

// Two deliberately DIFFERENT filters. A wide USB passband next to a narrow CW
// one is the case a single pan-level filter pair cannot represent: reuse the
// pan's edges for both and the CW slice draws a 2.7 kHz band.
constexpr int kUsbLowHz  =  150;
constexpr int kUsbHighHz = 2850;
constexpr int kCwLowHz   = -250;
constexpr int kCwHighHz  =  250;

void placePan(SpectrumWidget& w)
{
    w.resize(1200, 500);
    w.setSampleRate(kSpanHz);
    w.setDdcCenterFrequency(kCentreHz);
    w.setFrequencyRange(kCentreHz, kSpanHz);
}

// A sibling widget's position in its parent's QObject::children() list IS
// its paint z-order for plain QWidget siblings: raise()/lower() are
// implemented by moving the widget's entry within that same list. Higher
// index = painted later = on top. -1 (no parent) can never collide with a
// real index, so callers can tell "not found" from "at the back". Takes a
// non-const QWidget*: QList<QObject*>::indexOf() cannot match a
// const QWidget* argument (it would drop const on the pointee), and every
// caller here already holds a non-const pointer anyway.
int indexInParent(QWidget* w)
{
    QWidget* parent = w->parentWidget();
    return parent ? parent->children().indexOf(w) : -1;
}

} // namespace

class TestPanFlagPositions : public QObject
{
    Q_OBJECT

private slots:
    // Two slices co-hosted on one pan, tuned 100 kHz apart. Their flags must
    // sit at two different x positions, in frequency order.
    void two_flags_sit_at_their_own_frequencies()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        QVERIFY(flagA);
        QVERIFY(flagB);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);

        // The pan's own VFO marker sits on A -- the state that used to drag
        // B's flag on top of A's.
        w.setVfoFrequency(kSliceAHz);
        w.updateVfoPositions();

        QVERIFY2(flagA->x() != flagB->x(),
                 "co-hosted flags are stacked at one x");
        QVERIFY2(flagB->x() > flagA->x(),
                 "the higher-frequency flag must sit to the right");
    }

    // Same pan, but this time the PAN's VFO is parked on B. A's flag must not
    // follow it: the two are independent.
    void moving_the_pan_vfo_does_not_drag_the_other_flag()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);

        w.setVfoFrequency(kSliceAHz);
        w.updateVfoPositions();
        const int xABefore = flagA->x();

        w.setVfoFrequency(kSliceBHz);
        w.updateVfoPositions();

        QCOMPARE(flagA->x(), xABefore);
    }

    // A slice tuned outside this pan's visible window has no x to sit at, so
    // its flag hides -- but only that flag. Before the fix the whole pan's
    // flags hid or showed together off the pan-level off-screen state.
    void an_off_window_flag_hides_without_hiding_its_neighbour()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kOffWindowHz);

        w.setVfoFrequency(kSliceAHz);
        w.updateVfoPositions();

        // isHidden(), not isVisible(): the pan itself is never shown in the
        // harness, so isVisible() is false for every child regardless.
        QVERIFY2(!flagA->isHidden(), "the in-window flag must stay up");
        QVERIFY2(flagB->isHidden(), "the off-window flag must hide");
    }

    // Single-slice pans are the overwhelmingly common case and must be
    // byte-identical to the pre-fix behaviour: one flag, at the pan VFO.
    void a_lone_flag_still_lands_on_the_pan_vfo()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        flagA->setFrequency(kSliceAHz);
        w.setVfoFrequency(kSliceAHz);
        w.updateVfoPositions();
        const int xOnA = flagA->x();

        flagA->setFrequency(kSliceBHz);
        w.setVfoFrequency(kSliceBHz);
        w.updateVfoPositions();

        QVERIFY(flagA->x() > xOnA);
    }

    // ---- Passband markers (second bench report) ----

    // Two co-hosted slices produce TWO markers, each carrying its own centre
    // and its own filter edges. One marker per pan is what made B's passband
    // vanish whenever A was the last slice tuned.
    void two_slices_each_shade_their_own_passband()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        QVERIFY(flagA);
        QVERIFY(flagB);
        flagA->setFrequency(kSliceAHz);
        flagA->setFilter(kUsbLowHz, kUsbHighHz);
        flagB->setFrequency(kSliceBHz);
        flagB->setFilter(kCwLowHz, kCwHighHz);

        // Pan-level state parked on A, the state that used to be the only
        // passband drawn.
        w.setVfoFrequency(kSliceAHz);
        w.setFilterOffset(kUsbLowHz, kUsbHighHz);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 2);

        QCOMPARE(geo[0].centreHz, kSliceAHz);
        QCOMPARE(geo[0].filterLowHz, kUsbLowHz);
        QCOMPARE(geo[0].filterHighHz, kUsbHighHz);

        QCOMPARE(geo[1].centreHz, kSliceBHz);
        QCOMPARE(geo[1].filterLowHz, kCwLowHz);
        QCOMPARE(geo[1].filterHighHz, kCwHighHz);
    }

    // The operator's words: "The pass band of the second flag disappears when
    // not active." Activating B moves the pan's VFO and filter onto B; A's
    // marker must survive that untouched.
    void the_inactive_slice_keeps_its_passband()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagA->setFilter(kUsbLowHz, kUsbHighHz);
        flagB->setFrequency(kSliceBHz);
        flagB->setFilter(kCwLowHz, kCwHighHz);

        // B becomes the active slice: MainWindow pushes ITS frequency and ITS
        // filter to the hosting pan, overwriting A's.
        w.setVfoFrequency(kSliceBHz);
        w.setFilterOffset(kCwLowHz, kCwHighHz);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 2);
        QCOMPARE(geo[0].centreHz, kSliceAHz);
        QCOMPARE(geo[0].filterLowHz, kUsbLowHz);
        QCOMPARE(geo[0].filterHighHz, kUsbHighHz);
    }

    // Each marker's triangle hangs off ITS OWN flag. drawVfoMarker looked the
    // owner up as m_vfoWidgets[0] unconditionally, so every triangle would
    // have taken slice A's flag height.
    void each_marker_owns_the_flag_its_triangle_hangs_from()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);
        w.setVfoFrequency(kSliceAHz);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 2);
        QCOMPARE(geo[0].flag, static_cast<const VfoWidget*>(flagA));
        QCOMPARE(geo[1].flag, static_cast<const VfoWidget*>(flagB));
    }

    // Single-slice pans are the common case and must be byte-identical to the
    // pre-fix behaviour: exactly one marker, at the pan VFO, with the pan's
    // filter.
    void a_lone_slice_marker_is_unchanged()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        flagA->setFrequency(kSliceAHz);
        flagA->setFilter(kUsbLowHz, kUsbHighHz);
        w.setVfoFrequency(kSliceAHz);
        w.setFilterOffset(kUsbLowHz, kUsbHighHz);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QCOMPARE(geo[0].centreHz, kSliceAHz);
        QCOMPARE(geo[0].filterLowHz, kUsbLowHz);
        QCOMPARE(geo[0].filterHighHz, kUsbHighHz);
        QCOMPARE(geo[0].flag, static_cast<const VfoWidget*>(flagA));
    }

    // wireSliceToSpectrum() seeds the pan's VFO before it builds the flag, and
    // a pan can paint in that window. With no flag to read, the pan falls back
    // to marking its own VFO exactly as it always did.
    void a_pan_with_no_flag_yet_still_marks_its_own_vfo()
    {
        SpectrumWidget w;
        placePan(w);

        w.setVfoFrequency(kSliceAHz);
        w.setFilterOffset(kUsbLowHz, kUsbHighHz);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QCOMPARE(geo[0].centreHz, kSliceAHz);
        QCOMPARE(geo[0].filterLowHz, kUsbLowHz);
        QCOMPARE(geo[0].filterHighHz, kUsbHighHz);
        QVERIFY(geo[0].flag == nullptr);
    }

    // A pan with no slice on it at all draws nothing. Guards the m_vfoHz <= 0
    // early-out that the pre-split drawVfoMarker opened with.
    void an_unseeded_pan_draws_no_marker()
    {
        SpectrumWidget w;
        placePan(w);

        QVERIFY(w.sliceMarkerGeometry().isEmpty());
    }

    // ---- Flag z-order (third bench report, Sub-Epic J) ----

    // A is created first, B second. Before the fix, this is exactly the
    // reported bug shape: the higher-index flag (B) always ends up on top
    // after a position pass, whether or not it is the one the operator
    // selected. This case pins that baseline, then pins the fix on top of
    // it in one place, so a reader sees both halves together.
    void the_active_flag_stays_on_top_of_a_higher_index_neighbour()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);
        w.updateVfoPositions();

        QVERIFY2(indexInParent(flagB) > indexInParent(flagA),
                 "baseline: the higher slice index (B) sits on top before "
                 "any selection is made -- this is the reported bug shape");

        // Operator selects A -- the lower slice index, the one the bug
        // buries.
        w.setFrontSliceIndex(0);

        QVERIFY2(indexInParent(flagA) > indexInParent(flagB),
                 "the active flag (A) must come to the front even though "
                 "its neighbour has the higher slice index");
    }

    // updateVfoPositions() runs every render frame (see its own comment)
    // and unconditionally raises every visible flag once per pass, in
    // ascending slice-index order. A raise() that only fires once, at
    // selection time, would be undone by the very next frame. The pin has
    // to be reasserted every pass to hold, which is exactly what a naive
    // "just call raise() when the operator clicks" fix would miss on the
    // bench even with this same test passing once.
    void the_pin_survives_the_next_position_pass()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);
        w.updateVfoPositions();

        w.setFrontSliceIndex(0);
        QVERIFY(indexInParent(flagA) > indexInParent(flagB));

        // Several more render frames, exactly as the live paint/render cycle
        // would drive them, with no further selection in between.
        w.updateVfoPositions();
        w.updateVfoPositions();
        w.updateVfoPositions();

        QVERIFY2(indexInParent(flagA) > indexInParent(flagB),
                 "the pin must survive updateVfoPositions(), which runs "
                 "every frame and otherwise re-applies ascending-index order");
    }

    // The close/lock/record/play buttons are parented to the SpectrumWidget,
    // not to the flag (see VfoWidget::destroyFloatingButtons), so raising
    // the flag body alone can leave its own buttons stranded behind a flag
    // that used to sit above it -- a flag on top with a dead button column
    // showing through underneath. The active flag's buttons must front with
    // it.
    void the_active_flags_own_buttons_come_to_the_front_with_it()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);
        // First position pass lazily builds each flag's floating buttons.
        w.updateVfoPositions();

        QPushButton* aClose = flagA->closeButtonForTest();
        QVERIFY2(aClose, "buttons must exist after the first position pass");

        w.setFrontSliceIndex(0);

        QVERIFY2(indexInParent(aClose) > indexInParent(flagB),
                 "flag A's own close button must front with flag A, not "
                 "sit behind flag B");
    }

    // A single-slice pan has no neighbour to be buried under or to bury.
    // Pinning the pan's only flag must not hide it, move it, or otherwise
    // change its behaviour.
    void a_lone_flag_is_unaffected_by_the_pin()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        flagA->setFrequency(kSliceAHz);
        w.updateVfoPositions();
        const int xBefore = flagA->x();

        w.setFrontSliceIndex(0);
        w.updateVfoPositions();

        QCOMPARE(flagA->x(), xBefore);
        QVERIFY(!flagA->isHidden());
    }

    // A pin for a slice this pan does not host (a different pan's slice ID,
    // or one whose flag has not been created here yet) must not touch this
    // pan's own flags -- the standing "a pan acts on its own state" rule.
    void a_pin_for_an_unhosted_slice_leaves_this_pans_order_alone()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);
        w.updateVfoPositions();

        // Baseline established the normal way: nobody has pinned a front
        // slice yet, so B (the higher slice index) sits on top.
        QVERIFY(indexInParent(flagB) > indexInParent(flagA));

        w.setFrontSliceIndex(97);  // hosted by no pan in this test
        w.updateVfoPositions();

        QVERIFY2(indexInParent(flagB) > indexInParent(flagA),
                 "a pin for a slice this pan does not host must not perturb "
                 "this pan's own flag order");
    }

    // ---- Marker colours (fourth report) ----

    // The reported shape: B selected on a pan it shares with A. B's centre
    // line, triangle and edges must be B's own colour, not A's cyan.
    void the_selected_slice_draws_in_its_own_colour()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);
        flagA->setActiveSlice(false);
        flagB->setActiveSlice(true);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 2);
        const auto& b = geo[1];  // the selected slice paints last
        QCOMPARE(b.flag, static_cast<const VfoWidget*>(flagB));
        QCOMPARE(b.sliceIndex, 1);
        QVERIFY(b.active);
        QCOMPARE(b.lineColor, VfoWidget::sliceColor(1));
        QCOMPARE(b.edgeColor, VfoWidget::sliceColor(1));
        QVERIFY2(b.lineColor != VfoWidget::sliceColor(0),
                 "slice B's marker is still drawn in slice A's cyan");
    }

    // A slice the operator has not selected draws its centre line and
    // triangle in the darker partner of its own colour, and its edges in the
    // neutral grey so its passband stays easy to see.
    void a_slice_not_selected_draws_darker_with_grey_edges()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);
        flagA->setActiveSlice(true);
        flagB->setActiveSlice(false);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 2);

        const auto& b = geo[0];
        QCOMPARE(b.sliceIndex, 1);
        QVERIFY(!b.active);
        QCOMPARE(b.lineColor, VfoWidget::sliceDimColor(1));
        QCOMPARE(b.edgeColor, QColor(Style::kTextSecondary));

        const auto& a = geo[1];
        QCOMPARE(a.sliceIndex, 0);
        QVERIFY(a.active);
        QCOMPARE(a.lineColor, VfoWidget::sliceColor(0));
        QCOMPARE(a.edgeColor, VfoWidget::sliceColor(0));
    }

    // Four slices on one pan, each selected in turn. Every marker carries
    // its own slice's colour, bright or dim, and no two markers share one.
    void every_slice_has_a_colour_of_its_own()
    {
        SpectrumWidget w;
        placePan(w);

        constexpr int kSlices = 4;
        QVector<VfoWidget*> flags;
        for (int i = 0; i < kSlices; ++i) {
            VfoWidget* flag = w.addVfoWidget(i);
            flag->setFrequency(kSliceAHz + i * 20'000.0);
            flags.append(flag);
        }

        for (int selected = 0; selected < kSlices; ++selected) {
            for (int i = 0; i < kSlices; ++i) {
                flags[i]->setActiveSlice(i == selected);
            }

            const auto geo = w.sliceMarkerGeometry();
            QCOMPARE(geo.size(), kSlices);
            QSet<QRgb> seen;
            for (const auto& g : geo) {
                QCOMPARE(g.active, g.sliceIndex == selected);
                QCOMPARE(g.lineColor, g.active
                                          ? VfoWidget::sliceColor(g.sliceIndex)
                                          : VfoWidget::sliceDimColor(g.sliceIndex));
                seen.insert(g.lineColor.rgb());
            }
            QCOMPARE(seen.size(), kSlices);
            QCOMPARE(geo.last().sliceIndex, selected);
        }
    }

    // The selected slice's marker paints last whatever its slice index, so
    // it sits on top wherever two markers overlap.
    void the_selected_marker_paints_on_top()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);

        flagA->setActiveSlice(true);
        flagB->setActiveSlice(false);
        auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 2);
        QCOMPARE(geo[0].flag, static_cast<const VfoWidget*>(flagB));
        QCOMPARE(geo[1].flag, static_cast<const VfoWidget*>(flagA));

        flagA->setActiveSlice(false);
        flagB->setActiveSlice(true);
        geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 2);
        QCOMPARE(geo[0].flag, static_cast<const VfoWidget*>(flagA));
        QCOMPARE(geo[1].flag, static_cast<const VfoWidget*>(flagB));
    }

    // One selection for the whole window: with it on another pan, this pan
    // hosts no selected slice and every marker on it draws darker, in slice
    // order.
    void a_pan_without_the_selected_slice_draws_every_marker_darker()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);
        flagA->setActiveSlice(false);
        flagB->setActiveSlice(false);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 2);
        for (int i = 0; i < 2; ++i) {
            QCOMPARE(geo[i].sliceIndex, i);
            QVERIFY(!geo[i].active);
            QCOMPARE(geo[i].lineColor, VfoWidget::sliceDimColor(i));
            QCOMPARE(geo[i].edgeColor, QColor(Style::kTextSecondary));
        }
    }

    // A single-slice pan must look exactly as it did before slices had
    // colours: slice A's cyan on the edges, the centre line and the
    // triangle. A new flag defaults to selected, so it holds before anyone
    // tells the flag, and after.
    void a_lone_slice_marker_stays_cyan()
    {
        SpectrumWidget w;
        placePan(w);

        VfoWidget* flagA = w.addVfoWidget(0);
        flagA->setFrequency(kSliceAHz);
        const QColor cyan(0x00, 0xd4, 0xff);

        auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QVERIFY(geo[0].active);
        QCOMPARE(geo[0].lineColor, cyan);
        QCOMPARE(geo[0].edgeColor, cyan);

        flagA->setActiveSlice(true);
        geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QCOMPARE(geo[0].lineColor, cyan);
        QCOMPARE(geo[0].edgeColor, cyan);
    }

    // The no-flag-yet fallback has no flag to name its slice. It is slice A,
    // selected: the cyan this marker has always been drawn in.
    void the_no_flag_fallback_stays_cyan()
    {
        SpectrumWidget w;
        placePan(w);
        w.setVfoFrequency(kSliceAHz);

        const auto geo = w.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QCOMPARE(geo[0].sliceIndex, 0);
        QVERIFY(geo[0].active);
        QCOMPARE(geo[0].lineColor, QColor(0x00, 0xd4, 0xff));
        QCOMPARE(geo[0].edgeColor, QColor(0x00, 0xd4, 0xff));
    }

    // A flag reports a change of selection once, and only when it changes.
    void a_flag_reports_only_real_selection_changes()
    {
        SpectrumWidget w;
        VfoWidget* flag = w.addVfoWidget(0);
        QSignalSpy spy(flag, &VfoWidget::activeSliceChanged);

        flag->setActiveSlice(true);   // already the default
        QCOMPARE(spy.count(), 0);

        flag->setActiveSlice(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
        QVERIFY(!flag->isActiveSlice());

        flag->setActiveSlice(false);
        QCOMPARE(spy.count(), 1);
    }

    // The GPU path caches the markers in the static overlay, so a change of
    // selection must throw that cache away; a bare update() would keep
    // showing the old colours on the shipping path.
    void a_selection_change_redraws_the_cached_markers()
    {
#ifdef NEREUS_GPU_SPECTRUM
        SpectrumWidget w;
        placePan(w);
        VfoWidget* flagA = w.addVfoWidget(0);
        VfoWidget* flagB = w.addVfoWidget(1);
        flagA->setFrequency(kSliceAHz);
        flagB->setFrequency(kSliceBHz);

        w.clearOverlayStaticDirtyForTest();
        flagB->setActiveSlice(false);
        QVERIFY2(w.overlayStaticDirtyForTest(),
                 "a selection change did not invalidate the static overlay");

        w.clearOverlayStaticDirtyForTest();
        flagB->setActiveSlice(false);  // no change
        QVERIFY2(!w.overlayStaticDirtyForTest(),
                 "re-sending the same selection invalidated the overlay");
#else
        QSKIP("no cached overlay texture on the CPU-only spectrum path");
#endif
    }
};

QTEST_MAIN(TestPanFlagPositions)
#include "tst_pan_flag_positions.moc"
