// =================================================================
// tests/tst_wheel_step_accumulator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// WheelStepAccumulator turns trackpad pixel deltas and mouse angle deltas
// into at most one step per event: 15 px of trackpad travel per step, no
// momentum, no sideways swipes, no more than one trackpad step per 50 ms,
// and one step per 120 angle units with no rate limit on a mouse.
// =================================================================
#include <QtTest/QtTest>
#include <QWheelEvent>

#include "gui/WheelStepAccumulator.h"

using namespace NereusSDR;

namespace {

int trackpad(WheelStepAccumulator& acc, int x, int y,
             Qt::ScrollPhase phase = Qt::ScrollUpdate)
{
    const QWheelEvent ev(QPointF(5, 5), QPointF(5, 5), QPoint(x, y), QPoint(x * 8, y * 8),
                         Qt::NoButton, Qt::NoModifier, phase, false);
    return acc.stepsFor(ev);
}

int mouse(WheelStepAccumulator& acc, int angleY)
{
    const QWheelEvent ev(QPointF(5, 5), QPointF(5, 5), QPoint(), QPoint(0, angleY),
                         Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    return acc.stepsFor(ev);
}

// A phase-less event carrying both deltas, as Qt's macOS plugin sends for a
// mouse wheel.
int phaseless(WheelStepAccumulator& acc, int pixelY, int angleY)
{
    const QWheelEvent ev(QPointF(5, 5), QPointF(5, 5), QPoint(0, pixelY), QPoint(0, angleY),
                         Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    return acc.stepsFor(ev);
}

// A phased event with no pixel delta.
int phasedAngleOnly(WheelStepAccumulator& acc, int angleY)
{
    const QWheelEvent ev(QPointF(5, 5), QPointF(5, 5), QPoint(), QPoint(0, angleY),
                         Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
    return acc.stepsFor(ev);
}

// A clock the test moves by hand, starting well past zero.
struct TestClock {
    qint64 now{1000};
    void install(WheelStepAccumulator& acc)
    {
        acc.setClockForTest([this]() { return now; });
    }
};

} // namespace

class TestWheelStepAccumulator : public QObject {
    Q_OBJECT
private slots:
    void small_trackpad_deltas_add_up_to_one_step()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QCOMPARE(trackpad(acc, 0, 5), 0);
        QCOMPARE(trackpad(acc, 0, 5), 0);
        QCOMPARE(trackpad(acc, 0, 4), 0);
        QCOMPARE(trackpad(acc, 0, 1), 1);
        clock.now += 100;
        QCOMPARE(trackpad(acc, 0, -15), -1);
    }

    void one_large_trackpad_event_is_one_step()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QCOMPARE(trackpad(acc, 0, 120), 1);
    }

    void momentum_is_ignored()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QCOMPARE(trackpad(acc, 0, 30, Qt::ScrollMomentum), 0);
        clock.now += 100;
        QCOMPARE(trackpad(acc, 0, 30, Qt::ScrollMomentum), 0);
    }

    void sideways_swipe_is_ignored()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QCOMPARE(trackpad(acc, 20, 5), 0);
    }

    void trackpad_steps_are_at_least_50_ms_apart()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QCOMPARE(trackpad(acc, 0, 15), 1);
        clock.now += 10;
        QCOMPARE(trackpad(acc, 0, 15), 0);
        clock.now += 50;   // 60 ms after the first
        QCOMPARE(trackpad(acc, 0, 15), 1);
    }

    void scroll_begin_clears_a_leftover()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QCOMPARE(trackpad(acc, 0, 14), 0);
        QCOMPARE(trackpad(acc, 0, 0, Qt::ScrollBegin), 0);
        QCOMPARE(trackpad(acc, 0, 14), 0);
    }

    void mouse_wheel_steps_per_notch_without_rate_limit()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QCOMPARE(mouse(acc, 40), 0);
        QCOMPARE(mouse(acc, 40), 0);
        QCOMPARE(mouse(acc, 40), 1);
        QCOMPARE(mouse(acc, 120), 1);
        QCOMPARE(mouse(acc, 960), 1);

        WheelStepAccumulator quick;
        TestClock quickClock;
        quickClock.install(quick);
        QCOMPARE(mouse(quick, 120), 1);
        quickClock.now += 1;
        QCOMPARE(mouse(quick, 120), 1);
        QCOMPARE(mouse(quick, -120), -1);
    }

    // Qt's macOS plugin fills pixelDelta for a mouse wheel too, with no
    // scroll phase: such an event steps by its angle, with no rate limit.
    void phaseless_event_with_pixels_is_a_mouse_wheel()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QVERIFY(!WheelStepAccumulator::isTrackpad(
            QWheelEvent(QPointF(5, 5), QPointF(5, 5), QPoint(0, 20), QPoint(0, 120),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false)));
        QCOMPARE(phaseless(acc, 20, 120), 1);

        WheelStepAccumulator quick;
        TestClock quickClock;
        quickClock.install(quick);
        QCOMPARE(phaseless(quick, 20, 120), 1);
        quickClock.now += 1;
        QCOMPARE(phaseless(quick, 20, 120), 1);

        // A slow notch: 8 px would never reach 15 px per step in time,
        // its 40 angle units are a third of a notch.
        WheelStepAccumulator slow;
        TestClock slowClock;
        slowClock.install(slow);
        QCOMPARE(phaseless(slow, 8, 40), 0);
        QCOMPARE(phaseless(slow, 8, 40), 0);
        QCOMPARE(phaseless(slow, 8, 40), 1);
    }

    // A phased event is a trackpad's even with no pixel delta; its angle
    // delta counts eight units to a pixel, so 120 units (15 px) is a step,
    // and the 50 ms limit applies.
    void phased_event_without_pixels_is_a_trackpad()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        QVERIFY(WheelStepAccumulator::isTrackpad(
            QWheelEvent(QPointF(5, 5), QPointF(5, 5), QPoint(), QPoint(0, 30),
                        Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false)));
        QCOMPARE(phasedAngleOnly(acc, 30), 0);
        QCOMPARE(phasedAngleOnly(acc, 30), 0);
        QCOMPARE(phasedAngleOnly(acc, 30), 0);
        QCOMPARE(phasedAngleOnly(acc, 30), 1);   // 120 units
        clock.now += 1;
        QCOMPARE(phasedAngleOnly(acc, 120), 0);  // rate limited: a trackpad
        clock.now += 100;
        QCOMPARE(phasedAngleOnly(acc, 120), 1);
        clock.now += 100;
        QCOMPARE(phasedAngleOnly(acc, -120), -1);
    }

    // Small angle-only deltas add up: fifteen of 8 units are one step.
    void small_angle_only_deltas_add_up()
    {
        WheelStepAccumulator acc;
        TestClock clock;
        clock.install(acc);
        int total = 0;
        for (int i = 0; i < 15; ++i) {
            const int step = phasedAngleOnly(acc, 8);
            QCOMPARE(step, i == 14 ? 1 : 0);
            total += step;
        }
        QCOMPARE(total, 1);
    }
};

QTEST_GUILESS_MAIN(TestWheelStepAccumulator)
#include "tst_wheel_step_accumulator.moc"
