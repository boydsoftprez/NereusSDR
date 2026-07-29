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
// =================================================================
#include <QtTest/QtTest>
#include <QApplication>

#include "gui/SpectrumWidget.h"
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

void placePan(SpectrumWidget& w)
{
    w.resize(1200, 500);
    w.setSampleRate(kSpanHz);
    w.setDdcCenterFrequency(kCentreHz);
    w.setFrequencyRange(kCentreHz, kSpanHz);
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
};

QTEST_MAIN(TestPanFlagPositions)
#include "tst_pan_flag_positions.moc"
