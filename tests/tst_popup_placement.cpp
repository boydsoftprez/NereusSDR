// =================================================================
// tests/tst_popup_placement.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F: a Qt::Popup opened at the cursor has to be clamped onto the
// screen by hand.
//
// Qt constrains QMenu but NOT a plain QWidget carrying Qt::Popup, which is
// what SpectrumOverlayMenu is. Measured on Qt 6.11 / macOS 26: a 250x350
// popup asked for y=869 on a screen whose available rect is
// QRect(0,33 1512x897) is placed at exactly y=869, leaving 289 px hanging
// below the visible area. Bench-reported 2026-07-28: right-clicking the LOWER
// pan of a 2v layout put the waterfall controls off the bottom of the screen,
// which read as "I have now lost the ability to adjust the second RX
// waterfall via the right click menu."
// =================================================================
#include <QtTest/QtTest>

#include "gui/popup_placement.h"

using namespace NereusSDR;

class TestPopupPlacement : public QObject
{
    Q_OBJECT

private slots:
    // A popup that already fits is left exactly where the cursor put it.
    void a_popup_that_fits_is_not_moved()
    {
        const QRect avail(0, 33, 1512, 897);
        QCOMPARE(PopupPlacement::clampToAvailable(QPoint(200, 200),
                                                  QSize(250, 350), avail),
                 QPoint(200, 200));
    }

    // The reported case: the lower pan of a 2v layout, cursor near the bottom.
    void a_popup_overflowing_the_bottom_slides_up_until_it_fits()
    {
        const QRect avail(0, 33, 1512, 897);   // as measured on the bench Mac
        const QPoint placed = PopupPlacement::clampToAvailable(
            QPoint(200, 869), QSize(250, 350), avail);
        QCOMPARE(placed.x(), 200);             // horizontal position is fine
        QCOMPARE(placed.y(), 580);             // 33 + 897 - 350
        QVERIFY(placed.y() + 350 <= avail.bottom() + 1);
    }

    void a_popup_overflowing_the_right_edge_slides_left()
    {
        const QRect avail(0, 33, 1512, 897);
        const QPoint placed = PopupPlacement::clampToAvailable(
            QPoint(1400, 100), QSize(250, 350), avail);
        QCOMPARE(placed.x(), 1262);            // 0 + 1512 - 250
        QCOMPARE(placed.y(), 100);
    }

    // Multi-monitor: the available rect of a secondary screen does not start
    // at the origin, so clamping must respect its left/top, not zero.
    void clamping_respects_a_non_origin_screen()
    {
        const QRect avail(-1920, 100, 1920, 1080);
        QCOMPARE(PopupPlacement::clampToAvailable(QPoint(-2500, 0),
                                                  QSize(250, 350), avail),
                 QPoint(-1920, 100));
    }

    // Degenerate: a popup taller than the screen cannot fit, so pin its top
    // left corner rather than pushing its TOP off the screen (which would put
    // the title and the first controls out of reach instead of the last ones).
    void a_popup_larger_than_the_screen_pins_to_the_top_left()
    {
        const QRect avail(0, 33, 400, 300);
        QCOMPARE(PopupPlacement::clampToAvailable(QPoint(200, 200),
                                                  QSize(500, 600), avail),
                 QPoint(0, 33));
    }

    // An empty/invalid screen rect must not move the popup to a garbage
    // position; leaving it where the cursor was is the safe answer.
    void an_invalid_screen_rect_leaves_the_popup_alone()
    {
        QCOMPARE(PopupPlacement::clampToAvailable(QPoint(200, 200),
                                                  QSize(250, 350), QRect()),
                 QPoint(200, 200));
    }
};

QTEST_APPLESS_MAIN(TestPopupPlacement)
#include "tst_popup_placement.moc"
