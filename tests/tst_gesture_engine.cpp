// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// Press against hold, on an injected clock.
#include <QtTest>

#include "gui/hotkeys/devices/GestureEngine.h"

using namespace NereusSDR;

namespace {

// A local copy: QCOMPARE binds a reference, and MinGW can't import an exported
// class's static constexpr by address (CONTRIBUTING, Windows DLL Boundaries).
constexpr int kHoldMs = GestureEngine::kHoldMs;

DeviceEvent down(const char* control)
{
    return DeviceEvent{DeviceEvent::Kind::ButtonDown, QString::fromLatin1(control)};
}

DeviceEvent up(const char* control)
{
    return DeviceEvent{DeviceEvent::Kind::ButtonUp, QString::fromLatin1(control)};
}

bool isToken(const DeviceEvent& event, const char* control, ControlGesture gesture)
{
    return event.kind == DeviceEvent::Kind::TokenGesture && event.control == QLatin1String(control)
        && event.gesture == gesture;
}

} // namespace

class TestGestureEngine : public QObject {
    Q_OBJECT

private slots:
    void holdTimeIsUpstreams()
    {
        QCOMPARE(kHoldMs, 600);
    }

    void releaseBeforeTheTimeIsPress()
    {
        GestureEngine engine;
        QVERIFY(engine.onEvent(down("f1"), 1000).isEmpty());
        QVERIFY(engine.waiting());
        QVERIFY(engine.onTick(1100).isEmpty());
        const QList<DeviceEvent> events = engine.onEvent(up("f1"), 1200);
        QCOMPARE(events.size(), 1);
        QVERIFY(isToken(events.at(0), "f1", ControlGesture::Press));
        QVERIFY(!engine.waiting());
    }

    void heldFiresOnceAndItsReleaseFiresNothing()
    {
        GestureEngine engine;
        engine.onEvent(down("f1"), 1000);
        QVERIFY(engine.onTick(1000 + kHoldMs - 1).isEmpty());
        const QList<DeviceEvent> events = engine.onTick(1000 + kHoldMs);
        QCOMPARE(events.size(), 1);
        QVERIFY(isToken(events.at(0), "f1", ControlGesture::Hold));
        QVERIFY(!engine.waiting());
        QVERIFY(engine.onTick(5000).isEmpty());
        QVERIFY(engine.onEvent(up("f1"), 6000).isEmpty());
    }

    void releasePastTheTimeWithNoTickIsStillHold()
    {
        GestureEngine engine;
        engine.onEvent(down("f1"), 0);
        const QList<DeviceEvent> events = engine.onEvent(up("f1"), kHoldMs);
        QCOMPARE(events.size(), 1);
        QVERIFY(isToken(events.at(0), "f1", ControlGesture::Hold));
    }

    void bothButtonsKeepTheirOwnTime()
    {
        GestureEngine engine;
        engine.onEvent(down("f1"), 0);
        engine.onEvent(down("f2"), 400);
        const QList<DeviceEvent> first = engine.onTick(kHoldMs);
        QCOMPARE(first.size(), 1);
        QVERIFY(isToken(first.at(0), "f1", ControlGesture::Hold));
        const QList<DeviceEvent> release = engine.onEvent(up("f2"), 800);
        QCOMPARE(release.size(), 1);
        QVERIFY(isToken(release.at(0), "f2", ControlGesture::Press));
    }

    void repeatedDownKeepsTheFirstTime()
    {
        GestureEngine engine;
        engine.onEvent(down("f1"), 0);
        engine.onEvent(down("f1"), 500);
        QCOMPARE(engine.onTick(kHoldMs).size(), 1);
    }

    void aReleaseWithNoPressIsNothing()
    {
        GestureEngine engine;
        QVERIFY(engine.onEvent(up("f1"), 100).isEmpty());
    }

    void otherEventsPassThrough()
    {
        GestureEngine engine;
        const DeviceEvent rotate{DeviceEvent::Kind::Rotate, QStringLiteral("wheel"), 3};
        const QList<DeviceEvent> events = engine.onEvent(rotate, 0);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.at(0).value, 3);
    }

    void resetForgetsWhatIsDown()
    {
        GestureEngine engine;
        engine.onEvent(down("f1"), 0);
        engine.reset();
        QVERIFY(!engine.waiting());
        QVERIFY(engine.onTick(5000).isEmpty());
        QVERIFY(engine.onEvent(up("f1"), 5000).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestGestureEngine)
#include "tst_gesture_engine.moc"
