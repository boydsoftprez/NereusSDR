// =================================================================
// tests/tst_spectrum_receiver_grab.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Every receiver a pan shows can be grabbed: its filter edges (8 px outside
// the band, shrinking inside it so 6 px of body remain), its band body, and
// a strip 8 px either side of its center line. The selected receiver wins
// any overlap; a press on another receiver selects it and the same drag
// tunes it. A drag keeps the grab offset, a press with no move does not
// tune, and nothing tunes while the pan shows transmit. A drag moves the
// receiver grabbed, through its flag, and tunes by its travel from the
// press whatever the view does meanwhile.
//
// Driven through the widget's own event handlers with sent mouse events
// (offscreen, so this proves the hit test and the tune, not a real click
// through the macOS view tree).
// =================================================================
#include <QtTest/QtTest>
#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QSignalSpy>

#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

constexpr int    kW = 800;
constexpr int    kH = 400;
constexpr double kCenterHz = 14200000.0;
constexpr double kBandwidthHz = 80000.0;   // 100 Hz per pixel
constexpr double kHzPerPx = kBandwidthHz / kW;
constexpr int    kSpecY = 50;               // inside the spectrum area

// Mirrors SpectrumWidget::hzToX with the dBm strip hidden (the spectrum
// rect is the full width), truncating cast included.
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

void press(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseButtonPress, QPoint(x, y), Qt::LeftButton, Qt::LeftButton);
}

void moveTo(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::LeftButton);
}

void hover(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::NoButton);
}

void release(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseButtonRelease, QPoint(x, y), Qt::LeftButton, Qt::NoButton);
}

// Sends a wheel event to w; returns whether w accepted it.
bool sendWheel(QWidget* w, QPoint pos, QPoint pixelDelta, QPoint angleDelta,
               Qt::ScrollPhase phase)
{
    QWheelEvent ev(QPointF(pos), w->mapToGlobal(QPointF(pos)), pixelDelta, angleDelta,
                   Qt::NoButton, Qt::NoModifier, phase, false);
    QApplication::sendEvent(w, &ev);
    return ev.isAccepted();
}

struct Pan {
    SpectrumWidget sw;
    Pan()
    {
        sw.resize(kW, kH);
        sw.setDbmScaleVisible(false);
        sw.setFrequencyRange(kCenterHz, kBandwidthHz);
        sw.setConnectionState(ConnectionState::Connected);
        sw.setStepSize(100);
    }
    VfoWidget* receiver(int slice, double hz, int low, int high, bool selected)
    {
        VfoWidget* flag = sw.addVfoWidget(slice);
        flag->setFrequency(hz);
        flag->setFilter(low, high);
        flag->setStepHz(100);
        flag->setActiveSlice(selected);
        return flag;
    }
};

} // namespace

class TestSpectrumReceiverGrab : public QObject {
    Q_OBJECT
private slots:
    void narrow_band_center_press_tunes()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 400, true);   // 4 px wide
        QSignalSpy tuned(a, &VfoWidget::frequencyChanged);
        QSignalSpy panned(&p.sw, &SpectrumWidget::centerChanged);
        QSignalSpy edged(a, &VfoWidget::filterChanged);
        const int x = xForHz(kCenterHz + 200.0);
        press(&p.sw, x);
        moveTo(&p.sw, x + 20);
        release(&p.sw, x + 20);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 20 * kHzPerPx);
        QCOMPARE(panned.count(), 0);
        QCOMPARE(edged.count(), 0);
    }

    void wide_band_edge_zones()
    {
        // A 30 px band whose low edge sits 20 px above the center line, so
        // every press below is outside the center line's 8 px strip (which
        // wins over an edge, center_line_wins_over_a_near_edge).
        const int loX = xForHz(kCenterHz + 2000.0);
        {
            Pan p;
            VfoWidget* a = p.receiver(0, kCenterHz, 2000, 5000, true);
            QSignalSpy edged(a, &VfoWidget::filterChanged);
            press(&p.sw, loX + 3);   // 3 px inside the low edge
            moveTo(&p.sw, loX + 13);
            release(&p.sw, loX + 13);
            QCOMPARE(edged.count(), 1);
            QCOMPARE(edged.first().at(0).toInt(), 3000);
            QCOMPARE(edged.first().at(1).toInt(), 5000);
        }
        {
            Pan p;
            VfoWidget* a = p.receiver(0, kCenterHz, 2000, 5000, true);
            QSignalSpy edged(a, &VfoWidget::filterChanged);
            press(&p.sw, loX - 7);   // 7 px outside
            moveTo(&p.sw, loX - 17);
            release(&p.sw, loX - 17);
            QCOMPARE(edged.count(), 1);
            QCOMPARE(edged.first().at(0).toInt(), 1000);
        }
        {
            Pan p;
            VfoWidget* a = p.receiver(0, kCenterHz, 2000, 5000, true);
            QSignalSpy edged(a, &VfoWidget::filterChanged);
            QSignalSpy tuned(a, &VfoWidget::frequencyChanged);
            QSignalSpy panned(&p.sw, &SpectrumWidget::centerChanged);
            press(&p.sw, loX - 9);   // 9 px outside: the pan
            moveTo(&p.sw, loX - 49);
            release(&p.sw, loX - 49);
            QCOMPARE(edged.count(), 0);
            QCOMPARE(tuned.count(), 0);
            QVERIFY(panned.count() >= 1);
        }
    }

    // The center line wins within its strip: a selected USB receiver whose
    // low edge is 1 px above its center line (100 Hz low cut) tunes from a
    // press on the line or on its triangle, rather than resizing the low
    // edge. Outside the strip the edge still resizes (wide_band_edge_zones).
    void center_line_wins_over_a_near_edge()
    {
        const int centerX = xForHz(kCenterHz);
        const QPoint presses[] = {
            QPoint(centerX, kSpecY),      // the center line
            QPoint(centerX - 4, 3),       // the triangle at the spectrum's top
        };
        for (const QPoint& at : presses) {
            Pan p;
            VfoWidget* a = p.receiver(0, kCenterHz, 100, 3000, true);
            QSignalSpy edged(a, &VfoWidget::filterChanged);
            QSignalSpy tuned(a, &VfoWidget::frequencyChanged);
            QVERIFY(p.sw.spectrumRectForTest().contains(at));
            press(&p.sw, at.x(), at.y());
            moveTo(&p.sw, at.x() + 10, at.y());
            release(&p.sw, at.x() + 10, at.y());
            QCOMPARE(edged.count(), 0);
            QCOMPARE(tuned.count(), 1);
            QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 10 * kHzPerPx);
        }
    }

    void center_strip_keeps_the_grab_offset()
    {
        Pan p;
        // The band sits 10 to 30 px left of the center line.
        VfoWidget* a = p.receiver(0, kCenterHz, -3000, -1000, true);
        QSignalSpy tuned(a, &VfoWidget::frequencyChanged);
        const int x = xForHz(kCenterHz) + 6;
        press(&p.sw, x, 5);
        moveTo(&p.sw, x + 10, 5);
        release(&p.sw, x + 10, 5);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 10 * kHzPerPx);
    }

    void press_and_release_without_a_move_does_not_tune()
    {
        Pan p;
        // Off the step grid, so a snap alone would move it.
        VfoWidget* a = p.receiver(0, kCenterHz + 50.0, 0, 3000, true);
        QSignalSpy tuned(a, &VfoWidget::frequencyChanged);
        const int x = xForHz(kCenterHz + 1500.0);
        press(&p.sw, x);
        moveTo(&p.sw, x, kSpecY + 3);   // vertical wobble only
        release(&p.sw, x, kSpecY + 3);
        QCOMPARE(tuned.count(), 0);
    }

    // Once the drag has left the press column, a return to it tunes back:
    // the receiver ends at its start, not at the last sample.
    void return_to_the_press_column_tunes_back()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        QSignalSpy tuned(a, &VfoWidget::frequencyChanged);
        const int x = xForHz(kCenterHz + 1500.0);
        press(&p.sw, x);
        moveTo(&p.sw, x + 10);
        moveTo(&p.sw, x);
        release(&p.sw, x);
        QCOMPARE(tuned.count(), 2);
        QCOMPARE(tuned.at(0).first().toDouble(), kCenterHz + 10 * kHzPerPx);
        QCOMPARE(tuned.at(1).first().toDouble(), kCenterHz);
    }

    // A drag tunes by the cursor's pixel travel since the press, so a view
    // that re-centers between the press and the move (selecting a receiver
    // from another stream re-centers the pan) does not retune by the jump.
    void view_recenter_mid_drag_does_not_jump()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        QSignalSpy tuned(a, &VfoWidget::frequencyChanged);
        const int x = xForHz(kCenterHz + 1500.0);
        press(&p.sw, x);
        p.sw.setFrequencyRange(kCenterHz + 25000.0, kBandwidthHz);
        moveTo(&p.sw, x + 10);
        release(&p.sw, x + 10);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 10 * kHzPerPx);
    }

    // Without CTUN the view scrolls to keep the receiver 10% inside its
    // edges, and the drag goes on by its travel across that scroll.
    void scroll_margin_follows_the_drag_without_ctun()
    {
        Pan p;
        p.sw.setVfoFrequency(kCenterHz);
        p.sw.setCtunEnabled(false);   // re-centers the view on the VFO
        QCOMPARE(p.sw.centerFrequency(), kCenterHz);
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        // The model's echo: the slice's new frequency reaches the pan.
        QObject::connect(a, &VfoWidget::frequencyChanged, &p.sw,
                         [&p](double hz) { p.sw.setVfoFrequency(hz); });
        QSignalSpy tuned(a, &VfoWidget::frequencyChanged);
        QSignalSpy panned(&p.sw, &SpectrumWidget::centerChanged);
        const int x = xForHz(kCenterHz + 1500.0);
        press(&p.sw, x);
        moveTo(&p.sw, x + 350);   // 35 kHz: into the right-hand 10%
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.last().first().toDouble(), kCenterHz + 35000.0);
        QCOMPARE(panned.count(), 1);
        // The receiver sits 10% (8 kHz) in from the new right edge.
        QCOMPARE(p.sw.centerFrequency(), kCenterHz + 35000.0 - 40000.0 + 8000.0);
        moveTo(&p.sw, x + 360);
        release(&p.sw, x + 360);
        QCOMPARE(tuned.last().first().toDouble(), kCenterHz + 36000.0);
    }

    // The hover cursor uses the press's hit test: the drag cursor over a
    // receiver, the cross off it.
    void hover_cursor_follows_the_grab()
    {
        Pan p;
        p.receiver(0, kCenterHz, 0, 3000, true);
        hover(&p.sw, xForHz(kCenterHz + 1500.0));
        QCOMPARE(p.sw.cursor().shape(), Qt::SizeHorCursor);
        hover(&p.sw, xForHz(kCenterHz - 10000.0));
        QCOMPARE(p.sw.cursor().shape(), Qt::CrossCursor);
    }

    void press_on_another_receiver_selects_and_tunes_it()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        VfoWidget* b = p.receiver(1, kCenterHz + 10000.0, 0, 3000, false);
        QSignalSpy tunedPan(&p.sw, &SpectrumWidget::frequencyClicked);
        QSignalSpy tunedA(a, &VfoWidget::frequencyChanged);
        QSignalSpy tunedB(b, &VfoWidget::frequencyChanged);
        QSignalSpy selectB(b, &VfoWidget::sliceActivationRequested);
        QSignalSpy edged(&p.sw, &SpectrumWidget::filterEdgeDragged);

        const int x = xForHz(kCenterHz + 11500.0);   // B's band body
        press(&p.sw, x);
        QCOMPARE(selectB.count(), 1);
        QCOMPARE(selectB.first().first().toInt(), 1);
        moveTo(&p.sw, x + 10);
        release(&p.sw, x + 10);
        QCOMPARE(tunedB.count(), 1);
        QCOMPARE(tunedB.first().first().toDouble(), kCenterHz + 10000.0 + 10 * kHzPerPx);
        QCOMPARE(tunedA.count(), 0);
        QCOMPARE(tunedPan.count(), 0);

        // B's edge, too, tunes B rather than resizing it. B is where the
        // drag above left it (its flag took the tune).
        tunedB.clear();
        QCOMPARE(b->frequency(), kCenterHz + 10000.0 + 10 * kHzPerPx);
        const int bLowX = xForHz(b->frequency());
        press(&p.sw, bLowX - 4);
        moveTo(&p.sw, bLowX + 6);
        release(&p.sw, bLowX + 6);
        QCOMPARE(edged.count(), 0);
        QCOMPARE(tunedB.count(), 1);
    }

    void selected_receiver_wins_an_overlap()
    {
        Pan p;
        VfoWidget* b = p.receiver(1, kCenterHz + 1000.0, 0, 3000, false);
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        QSignalSpy tunedA(a, &VfoWidget::frequencyChanged);
        QSignalSpy tunedB(b, &VfoWidget::frequencyChanged);
        QSignalSpy selectB(b, &VfoWidget::sliceActivationRequested);
        const int x = xForHz(kCenterHz + 2000.0);   // inside both bands
        press(&p.sw, x);
        moveTo(&p.sw, x + 10);
        release(&p.sw, x + 10);
        QCOMPARE(selectB.count(), 0);
        QCOMPARE(tunedB.count(), 0);
        QCOMPARE(tunedA.count(), 1);
        QCOMPARE(tunedA.first().first().toDouble(), kCenterHz + 10 * kHzPerPx);
    }

    void transmit_overlay_withholds_every_drag()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        VfoWidget* b = p.receiver(1, kCenterHz + 10000.0, 0, 3000, false);
        QSignalSpy tunedA(a, &VfoWidget::frequencyChanged);
        QSignalSpy tunedB(b, &VfoWidget::frequencyChanged);
        const int xA = xForHz(kCenterHz + 1500.0);
        const int xB = xForHz(kCenterHz + 11500.0);

        // Receiving, both drags tune: the presses land on the bands.
        press(&p.sw, xA);
        moveTo(&p.sw, xA + 10);
        release(&p.sw, xA + 10);
        QCOMPARE(tunedA.count(), 1);

        p.sw.setMoxOverlay(true);
        press(&p.sw, xA);
        moveTo(&p.sw, xA + 20);
        release(&p.sw, xA + 20);
        press(&p.sw, xB);
        moveTo(&p.sw, xB + 20);
        release(&p.sw, xB + 20);
        QCOMPARE(tunedA.count(), 1);
        QCOMPARE(tunedB.count(), 0);

        p.sw.setMoxOverlay(false);
        press(&p.sw, xB);
        moveTo(&p.sw, xB + 20);
        release(&p.sw, xB + 20);
        QCOMPARE(tunedB.count(), 1);
    }

    void locked_receiver_does_not_move()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        a->setLocked(true);
        QSignalSpy tunedA(a, &VfoWidget::frequencyChanged);
        QSignalSpy tunedPan(&p.sw, &SpectrumWidget::frequencyClicked);
        const int x = xForHz(kCenterHz + 1500.0);
        press(&p.sw, x);
        moveTo(&p.sw, x + 20);
        release(&p.sw, x + 20);
        QCOMPARE(tunedA.count(), 0);
        QCOMPARE(tunedPan.count(), 0);
    }

    // A drag moves the receiver grabbed. The pan's own slice (A: its VFO
    // and filter, what frequencyClicked and filterEdgeDragged act on through
    // MainWindow::sliceForPan) is not the selected one here; B is. Dragging
    // B's body and B's edge changes B through its flag and leaves A, the
    // pan's signals and the pan's own filter alone.
    void drag_moves_the_grabbed_receiver_not_the_pans_own()
    {
        Pan p;
        p.sw.setVfoFrequency(kCenterHz);
        p.sw.setFilterOffset(0, 3000);
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, false);
        VfoWidget* b = p.receiver(1, kCenterHz + 10000.0, 0, 3000, true);
        QSignalSpy tunedPan(&p.sw, &SpectrumWidget::frequencyClicked);
        QSignalSpy edgedPan(&p.sw, &SpectrumWidget::filterEdgeDragged);
        QSignalSpy tunedA(a, &VfoWidget::frequencyChanged);
        QSignalSpy filterA(a, &VfoWidget::filterChanged);
        QSignalSpy tunedB(b, &VfoWidget::frequencyChanged);
        QSignalSpy filterB(b, &VfoWidget::filterChanged);

        // B's body.
        const int xB = xForHz(kCenterHz + 11500.0);
        press(&p.sw, xB);
        moveTo(&p.sw, xB + 10);
        release(&p.sw, xB + 10);
        QCOMPARE(tunedB.count(), 1);
        QCOMPARE(tunedB.first().first().toDouble(), kCenterHz + 10000.0 + 10 * kHzPerPx);

        // B's high edge, 3 px inside it: out 10 px.
        const int bHighX = xForHz(b->frequency() + 3000.0);
        press(&p.sw, bHighX - 3);
        moveTo(&p.sw, bHighX + 7);
        release(&p.sw, bHighX + 7);
        QCOMPARE(filterB.count(), 1);
        QCOMPARE(filterB.first().at(0).toInt(), 0);
        QCOMPARE(filterB.first().at(1).toInt(), 4000);
        QCOMPARE(b->filterHigh(), 4000);

        QCOMPARE(tunedPan.count(), 0);
        QCOMPARE(edgedPan.count(), 0);
        QCOMPARE(tunedA.count(), 0);
        QCOMPARE(filterA.count(), 0);
        QCOMPARE(p.sw.filterLowForTest(), 0);
        QCOMPARE(a->frequency(), kCenterHz);
        QCOMPARE(a->filterHigh(), 3000);
    }

    // A wheel event that makes no step (a horizontal one, or a trackpad
    // event short of its 15 px) is ignored, so it reaches the parent, as
    // before the wheel steps and as AetherSDR does; a mouse notch tunes.
    // QApplication passes a real (spontaneous) wheel event to the
    // parent when the widget leaves it unaccepted; a sent one never
    // propagates, so the accepted flag is what this checks.
    void wheel_with_no_step_reaches_the_parent()
    {
        Pan p;
        p.sw.setVfoFrequency(kCenterHz);
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        const QPoint at(xForHz(kCenterHz - 20000.0), kSpecY);

        // Horizontal mouse wheel.
        QVERIFY(!sendWheel(&p.sw, at, QPoint(), QPoint(120, 0), Qt::NoScrollPhase));
        // Horizontal trackpad swipe.
        QVERIFY(!sendWheel(&p.sw, at, QPoint(20, 0), QPoint(160, 0), Qt::ScrollUpdate));
        // Vertical trackpad travel short of a step.
        QVERIFY(!sendWheel(&p.sw, at, QPoint(0, 5), QPoint(0, 40), Qt::ScrollUpdate));
        QCOMPARE(tuned.count(), 0);

        // A mouse notch steps. (Its acceptance is not checked: every stepped
        // branch ends in QWidget::wheelEvent, which leaves it unaccepted, as
        // it did before the wheel steps.)
        sendWheel(&p.sw, at, QPoint(), QPoint(0, 120), Qt::NoScrollPhase);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 100.0);
    }

    // A pan with no flags (a bare marker for its own VFO) still tunes and
    // resizes through its own signals.
    void pan_without_flags_tunes_and_resizes_its_own()
    {
        Pan p;
        p.sw.setVfoFrequency(kCenterHz);
        p.sw.setFilterOffset(2000, 5000);
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        QSignalSpy edged(&p.sw, &SpectrumWidget::filterEdgeDragged);
        const int x = xForHz(kCenterHz + 3500.0);
        press(&p.sw, x);
        moveTo(&p.sw, x + 10);
        release(&p.sw, x + 10);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 10 * kHzPerPx);

        const int loX = xForHz(kCenterHz + 2000.0);
        press(&p.sw, loX + 3);
        moveTo(&p.sw, loX + 13);
        release(&p.sw, loX + 13);
        QCOMPARE(edged.count(), 1);
        QCOMPARE(edged.first().at(0).toInt(), 3000);
        QCOMPARE(edged.first().at(1).toInt(), 5000);
        QCOMPARE(p.sw.filterLowForTest(), 3000);
    }
};

QTEST_MAIN(TestSpectrumReceiverGrab)
#include "tst_spectrum_receiver_grab.moc"
