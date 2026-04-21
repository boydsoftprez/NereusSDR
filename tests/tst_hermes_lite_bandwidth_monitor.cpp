// no-port-check: test fixture asserts HermesLiteBandwidthMonitor — upstream
// byte-rate compute_bps() port + NereusSDR throttle-detection layer.
//
// The upstream bandwidth_monitor.{c,h} (mi0bot/MW0LGE [@c26a8a4]) does NOT
// implement throttle detection. Throttle detection is a NereusSDR addition.
// These tests cover both the byte-rate port and the throttle state machine.
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/HermesLiteBandwidthMonitor.h"

using namespace NereusSDR;

class TestHermesLiteBandwidthMonitor : public QObject {
    Q_OBJECT
private slots:

    // Initial state: not throttled, zero counts, zero rates.
    void initial_state_clear()
    {
        HermesLiteBandwidthMonitor m;
        QVERIFY(!m.isThrottled());
        QCOMPARE(m.throttleEventCount(), 0);
        QCOMPARE(m.ep6IngressBytesPerSec(), 0.0);
        QCOMPARE(m.ep2EgressBytesPerSec(), 0.0);
    }

    // First tick with no data: prev_ms == 0 branch in compute_bps() → returns 0.
    // Source: mi0bot bandwidth_monitor.c:93-98 [@c26a8a4]
    void first_tick_no_data_returns_zero()
    {
        HermesLiteBandwidthMonitor m;
        m.tick();
        QCOMPARE(m.ep6IngressBytesPerSec(), 0.0);
        QCOMPARE(m.ep2EgressBytesPerSec(), 0.0);
    }

    // recordEp6Bytes with bytes <= 0 must be silently ignored.
    // Source: mi0bot bandwidth_monitor.c:76 — if (bytes > 0) guard [@c26a8a4]
    void record_non_positive_bytes_is_noop()
    {
        HermesLiteBandwidthMonitor m;
        m.recordEp6Bytes(0);
        m.recordEp6Bytes(-1);
        m.recordEp2Bytes(0);
        m.tick();
        // First tick always returns 0 (prev_ms == 0 branch), but the total
        // bytes counter must also be 0 — verified by the second tick having no rate.
        m.recordEp6Bytes(0);
        m.tick();
        QCOMPARE(m.ep6IngressBytesPerSec(), 0.0);
        QVERIFY(!m.isThrottled());
    }

    // After recording ep6 bytes and ticking twice (second tick has a non-zero
    // dt), the ingress rate must be positive.
    // Source: mi0bot bandwidth_monitor.c:101-111 compute_bps delta path [@c26a8a4]
    void ep6_rate_positive_after_second_tick()
    {
        HermesLiteBandwidthMonitor m;
        m.recordEp6Bytes(10000);
        m.tick();  // first tick: prev_ms == 0, returns 0, sets baseline
        // Give the clock a moment to advance so dt > 0 on the second tick.
        QTest::qSleep(5);
        m.recordEp6Bytes(10000);
        m.tick();  // second tick: computes real delta
        // Rate should be positive (10000 bytes over a few ms → many bytes/sec)
        QVERIFY(m.ep6IngressBytesPerSec() > 0.0);
    }

    // Throttle is NOT detected with only ep6 silence but no ep2 traffic —
    // the condition requires ep2 to be active (host is still sending commands).
    void throttle_requires_ep2_active()
    {
        HermesLiteBandwidthMonitor m;
        QSignalSpy spy(&m, &HermesLiteBandwidthMonitor::throttledChanged);

        // Establish ep6 baseline (sets prev_ms so compute_bps can measure)
        m.recordEp6Bytes(10000);
        m.recordEp2Bytes(5000);
        m.tick();
        QTest::qSleep(5);

        // Now: ep6 silent AND ep2 silent — should NOT throttle
        for (int i = 0; i < HermesLiteBandwidthMonitor::kThrottleTickThreshold + 2; ++i) {
            QTest::qSleep(5);
            m.tick();  // no bytes recorded → ep2 also silent
        }
        QVERIFY(!m.isThrottled());
        QCOMPARE(spy.count(), 0);
    }

    // Throttle IS detected when ep6 goes silent while ep2 keeps flowing.
    void throttle_detected_when_ep6_silent_ep2_active()
    {
        HermesLiteBandwidthMonitor m;
        QSignalSpy spy(&m, &HermesLiteBandwidthMonitor::throttledChanged);

        // Establish baseline so compute_bps can measure real deltas.
        m.recordEp6Bytes(10000);
        m.recordEp2Bytes(5000);
        m.tick();

        // Feed enough ticks to establish ep2 rate.
        for (int i = 0; i < 3; ++i) {
            QTest::qSleep(5);
            m.recordEp6Bytes(10000);
            m.recordEp2Bytes(5000);
            m.tick();
        }
        QVERIFY(!m.isThrottled());

        // Now ep6 goes silent while ep2 continues.
        for (int i = 0; i < HermesLiteBandwidthMonitor::kThrottleTickThreshold; ++i) {
            QTest::qSleep(5);
            m.recordEp2Bytes(5000);
            m.tick();
        }
        QVERIFY(m.isThrottled());
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().at(0).toBool(), true);
    }

    // Throttle clears when ep6 traffic resumes.
    void throttle_clears_when_ep6_resumes()
    {
        HermesLiteBandwidthMonitor m;

        // Establish baseline + trigger throttle
        m.recordEp6Bytes(10000);
        m.recordEp2Bytes(5000);
        m.tick();
        for (int i = 0; i < 3; ++i) {
            QTest::qSleep(5);
            m.recordEp6Bytes(10000);
            m.recordEp2Bytes(5000);
            m.tick();
        }
        for (int i = 0; i < HermesLiteBandwidthMonitor::kThrottleTickThreshold; ++i) {
            QTest::qSleep(5);
            m.recordEp2Bytes(5000);
            m.tick();
        }
        QVERIFY(m.isThrottled());

        QSignalSpy spy(&m, &HermesLiteBandwidthMonitor::throttledChanged);

        // ep6 resumes
        for (int i = 0; i < 2; ++i) {
            QTest::qSleep(5);
            m.recordEp6Bytes(10000);
            m.recordEp2Bytes(5000);
            m.tick();
        }
        QVERIFY(!m.isThrottled());
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().at(0).toBool(), false);
    }

    // throttleEventCount increments on each assertion of the throttle flag.
    void throttle_event_count_increments()
    {
        HermesLiteBandwidthMonitor m;

        auto runThrottle = [&]() {
            // Establish ep6 baseline
            m.recordEp6Bytes(10000);
            m.recordEp2Bytes(5000);
            m.tick();
            for (int i = 0; i < 3; ++i) {
                QTest::qSleep(5);
                m.recordEp6Bytes(10000);
                m.recordEp2Bytes(5000);
                m.tick();
            }
            // Trigger throttle
            for (int i = 0; i < HermesLiteBandwidthMonitor::kThrottleTickThreshold; ++i) {
                QTest::qSleep(5);
                m.recordEp2Bytes(5000);
                m.tick();
            }
        };

        auto runRecover = [&]() {
            for (int i = 0; i < 3; ++i) {
                QTest::qSleep(5);
                m.recordEp6Bytes(10000);
                m.recordEp2Bytes(5000);
                m.tick();
            }
        };

        runThrottle();
        QCOMPARE(m.throttleEventCount(), 1);

        runRecover();
        runThrottle();
        QCOMPARE(m.throttleEventCount(), 2);
    }

    // liveStatsUpdated is emitted on every tick().
    void live_stats_updated_emitted_on_tick()
    {
        HermesLiteBandwidthMonitor m;
        QSignalSpy spy(&m, &HermesLiteBandwidthMonitor::liveStatsUpdated);
        m.tick();
        QCOMPARE(spy.count(), 1);
        m.tick();
        QCOMPARE(spy.count(), 2);
    }

    // reset() clears all state including rates, throttle flag, and event count.
    // Source: mi0bot bandwidth_monitor.c:59-72 bandwidth_monitor_reset() [@c26a8a4]
    void reset_clears_state()
    {
        HermesLiteBandwidthMonitor m;

        m.recordEp6Bytes(1000);
        m.recordEp2Bytes(500);
        m.tick();
        QTest::qSleep(5);
        m.recordEp6Bytes(1000);
        m.tick();

        m.reset();

        QVERIFY(!m.isThrottled());
        QCOMPARE(m.throttleEventCount(), 0);
        QCOMPARE(m.ep6IngressBytesPerSec(), 0.0);
        QCOMPARE(m.ep2EgressBytesPerSec(), 0.0);

        // After reset, next tick should behave like the very first tick
        // (prev_ms == 0 → returns 0 again).
        m.recordEp6Bytes(1000);
        m.tick();
        QCOMPARE(m.ep6IngressBytesPerSec(), 0.0);  // first tick after reset
    }

    // reset() emits throttledChanged(false) if previously throttled.
    void reset_emits_signal_if_throttled()
    {
        HermesLiteBandwidthMonitor m;

        m.recordEp6Bytes(10000);
        m.recordEp2Bytes(5000);
        m.tick();
        for (int i = 0; i < 3; ++i) {
            QTest::qSleep(5);
            m.recordEp6Bytes(10000);
            m.recordEp2Bytes(5000);
            m.tick();
        }
        for (int i = 0; i < HermesLiteBandwidthMonitor::kThrottleTickThreshold; ++i) {
            QTest::qSleep(5);
            m.recordEp2Bytes(5000);
            m.tick();
        }
        QVERIFY(m.isThrottled());

        QSignalSpy spy(&m, &HermesLiteBandwidthMonitor::throttledChanged);
        m.reset();
        QVERIFY(!m.isThrottled());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }
};

QTEST_APPLESS_MAIN(TestHermesLiteBandwidthMonitor)
#include "tst_hermes_lite_bandwidth_monitor.moc"
