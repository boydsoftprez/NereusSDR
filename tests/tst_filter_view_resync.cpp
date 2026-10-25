// no-port-check: NereusSDR-original regression test.
// =================================================================
// tests/tst_filter_view_resync.cpp  (NereusSDR)
// =================================================================
//
// A view that drew a dragged filter shows what the slice kept. Thetis's
// display only ever takes the constrained edges (console.cs:7589-7590
// [v2.10.3.15], after ConstrainFilter at 7580 and the zero-width refusal
// at 7582). When the slice is already at the 10 kHz cap, a drag further
// out leaves the stored pair where it is; the slice re-sends that pair so
// the receiver flag (and the pan shading it feeds) and the RX applet's
// passband widget drop the dragged value.
//
// The RX applet's centre drag moves the passband through
// SliceModel::shiftFilter, as Thetis's filter shift and width sliders do
// (console.cs:35202, 35420 [v2.10.3.15]): at the cap the width is kept.
//
// Driven through the widgets' own event handlers with sent mouse events
// (offscreen).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Claude Code.
//   2026-10-24 - The centre drag goes on past the cap with the slice
//                 already there, and the widget shows the stored filter.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QMouseEvent>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "gui/SliceFlagPresentationBinding.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/RxApplet.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int    kW = 800;
constexpr int    kH = 400;
constexpr double kCenterHz = 14200000.0;
constexpr double kBandwidthHz = 80000.0;   // 100 Hz per pixel
constexpr int    kSpecY = 50;

// As tst_spectrum_receiver_grab: SpectrumWidget::hzToX with the dBm strip
// hidden.
int xForHz(double hz)
{
    const double lowHz = kCenterHz - kBandwidthHz / 2.0;
    return static_cast<int>((hz - lowHz) / kBandwidthHz * kW);
}

void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)), button, buttons,
                   Qt::NoModifier);
    QApplication::sendEvent(w, &me);
}

void press(QWidget* w, int x, int y)
{
    sendMouse(w, QEvent::MouseButtonPress, QPoint(x, y), Qt::LeftButton, Qt::LeftButton);
}

void moveTo(QWidget* w, int x, int y)
{
    sendMouse(w, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::LeftButton);
}

void release(QWidget* w, int x, int y)
{
    sendMouse(w, QEvent::MouseButtonRelease, QPoint(x, y), Qt::LeftButton, Qt::NoButton);
}

// FilterPassbandWidget at 632 px: 600 usable, so 10 Hz per pixel; the
// centre zone runs from x 105 to 527 (its press zones).
constexpr int kPassbandW = 632;
constexpr int kPassbandH = 48;

} // namespace

class TestFilterViewResync : public QObject
{
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // The receiver flag on a pan, wired to its slice as MainWindow wires
    // it: the flag's filterChanged sets the slice, and the slice's
    // filterChanged comes back to the flag and to the pan's passband.
    void flagDragPastTheCapShowsTheCap()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        slice.setFilter(100, 9500);

        SpectrumWidget sw;
        sw.resize(kW, kH);
        sw.setDbmScaleVisible(false);
        sw.setFrequencyRange(kCenterHz, kBandwidthHz);
        sw.setConnectionState(ConnectionState::Connected);
        sw.setStepSize(100);
        sw.setVfoFrequency(kCenterHz);
        sw.setFilterOffset(slice.filterLow(), slice.filterHigh());
        VfoWidget* flag = sw.addVfoWidget(0);
        flag->setFrequency(kCenterHz);
        flag->setFilter(slice.filterLow(), slice.filterHigh());
        flag->setStepHz(100);
        flag->setActiveSlice(true);

        QObject::connect(flag, &VfoWidget::filterChanged, &slice,
                         [&slice](int low, int high) { slice.setFilter(low, high); });
        SliceFlagHostHooks hooks;
        hooks.filterChanged = [&sw](int low, int high) { sw.setFilterOffset(low, high); };
        wireSliceFlagStatePresentation(&slice, flag, std::move(hooks));

        // The high edge, 3 px inside: out 10 px (10500 Hz), then 20 px
        // (11500 Hz). The first reaches the cap, the second goes past it
        // with the slice already there.
        const int highX = xForHz(kCenterHz + 9500.0);
        press(&sw, highX - 3, kSpecY);
        moveTo(&sw, highX + 7, kSpecY);
        QCOMPARE(slice.filterHigh(), 10000);
        moveTo(&sw, highX + 17, kSpecY);
        release(&sw, highX + 17, kSpecY);

        QCOMPARE(slice.filterLow(), 100);
        QCOMPARE(slice.filterHigh(), 10000);
        QCOMPARE(flag->filterLow(), 100);
        QCOMPARE(flag->filterHigh(), 10000);
        QCOMPARE(sw.filterHighForTest(), 10000);
    }

    // The RX applet's passband widget, edge drag: the same, through
    // RxApplet's own wiring.
    void passbandEdgeDragPastTheCapShowsTheCap()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        slice.setFilter(100, 9500);
        RxApplet applet(nullptr, nullptr);
        applet.setSlice(&slice);   // connects the slice, as the window does
        auto* passband = applet.findChild<FilterPassbandWidget*>();
        QVERIFY(passband);
        passband->resize(kPassbandW, kPassbandH);
        QCOMPARE(passband->filterHi(), 9500);

        // Right of the high line: out 100 px (+1000 Hz), then 110 px.
        press(passband, 600, 20);
        moveTo(passband, 700, 20);
        QCOMPARE(slice.filterHigh(), 10000);
        moveTo(passband, 710, 20);
        release(passband, 710, 20);

        QCOMPARE(slice.filterHigh(), 10000);
        QCOMPARE(passband->filterLo(), 100);
        QCOMPARE(passband->filterHi(), 10000);
    }

    // The centre drag moves the passband with filterShift true: past the
    // cap the width is kept (8100..10000), where an edge-by-edge set would
    // pin the low edge at the cap too and be refused as zero width.
    void passbandCentreDragKeepsTheWidth()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        slice.setFilter(8000, 9900);
        RxApplet applet(nullptr, nullptr);
        applet.setSlice(&slice);   // connects the slice, as the window does
        auto* passband = applet.findChild<FilterPassbandWidget*>();
        QVERIFY(passband);
        passband->resize(kPassbandW, kPassbandH);
        QSignalSpy shifted(passband, &FilterPassbandWidget::passbandShifted);
        QSignalSpy edged(passband, &FilterPassbandWidget::filterChanged);

        // 300 px right in the centre zone: +3000 Hz, 11000..12900 asked.
        // Then 310 px (11100..13000) with the slice already at the cap.
        press(passband, 150, 20);
        moveTo(passband, 450, 20);
        QCOMPARE(slice.filterLow(), 8100);
        QCOMPARE(slice.filterHigh(), 10000);
        moveTo(passband, 460, 20);
        release(passband, 460, 20);

        QCOMPARE(shifted.count(), 2);
        QCOMPARE(edged.count(), 0);
        QCOMPARE(slice.filterLow(), 8100);
        QCOMPARE(slice.filterHigh(), 10000);
        QCOMPARE(passband->filterLo(), 8100);
        QCOMPARE(passband->filterHi(), 10000);
    }
};

QTEST_MAIN(TestFilterViewResync)
#include "tst_filter_view_resync.moc"
