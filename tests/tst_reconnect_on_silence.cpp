// tests/tst_reconnect_on_silence.cpp
//
// Phase 3I Task 10 — reconnection state machine test for P1RadioConnection.
//
// Tests the §3.6 state machine:
//   Connected → (2s silence) → LinkLost → (5s × up to 3 retries) → Connected or LinkLost
//
// Phase 3Q-1 note: ConnectionState::LinkLost was removed from the 5-value enum
// (Disconnected/Probing/Connecting/Connected/LinkLost). A watchdog timeout is
// semantically "link lost" (was connected; frames stopped), so Error → LinkLost.
//
// Uses P1FakeRadio auto-streaming (Task 10 addition: goSilent() stops ep6 output
// and resume() re-enables it, simulating a transient vs permanent radio failure).

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

class TestReconnectOnSilence : public QObject {
    Q_OBJECT
private:
    static RadioInfo makeInfo(const P1FakeRadio& fake) {
        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.firmwareVersion = 72;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        return info;
    }

private slots:
    void silenceTriggersErrorThenRecoversOnResume() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        // Compress the reconnect timeline 100x: watchdog 2000ms -> 20ms,
        // reconnect interval 5000ms -> 50ms. Without this the second test
        // method sleeps 42 real seconds and sets the parallel floor for
        // the entire suite.
        conn.setReconnectTimingForTest(20, 50);
        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(
            conn.state() == ConnectionState::Connected, 500);

        // Wait for the fake to see the metis-start and begin streaming.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 500);

        // Kick the fake into silence — it stops replying to ep2 and stops ep6.
        fake.goSilent();

        // Watchdog should trip after 2s → LinkLost.
        QTRY_VERIFY_WITH_TIMEOUT(
            conn.state() == ConnectionState::LinkLost, 500);

        // Resume the fake before the reconnect window closes.
        // The reconnect timer fires after 5s — resume immediately so the next
        // attempt (metis-stop + metis-start) gets a response.
        fake.resume();

        // Within the reconnect cycle (5s × up to 3 = 15s max), should recover.
        QTRY_VERIFY_WITH_TIMEOUT(
            conn.state() == ConnectionState::Connected, 500);

        conn.disconnect();
        fake.stop();
    }

    void boundedRetriesExhaustStayInLinkLost() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        // Compress the reconnect timeline 100x: watchdog 2000ms -> 20ms,
        // reconnect interval 5000ms -> 50ms. Without this the second test
        // method sleeps 42 real seconds and sets the parallel floor for
        // the entire suite.
        conn.setReconnectTimingForTest(20, 50);
        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(
            conn.state() == ConnectionState::Connected, 3000);

        // Wait for the fake to process metis-start.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        // Watch every state transition from here on. The retry chain is
        // asserted by transition COUNT rather than by sleeping past a
        // hand-computed deadline: a fixed qWait races the timer chain and
        // goes flaky under parallel ctest load, which is precisely the
        // environment this test now runs in.
        QSignalSpy transitions(&conn, &P1RadioConnection::connectionStateChanged);

        // Fake stops completely — no discovery reply, no nothing.
        fake.stop();

        // Timeline is driven by two P1RadioConnection timing values that
        // the test compresses 100x via setReconnectTimingForTest():
        //   watchdog silence  2000ms -> 20ms
        //   reconnect interval 5000ms -> 50ms
        //
        // kMaxReconnectAttempts is 3, so the bounded chain produces exactly
        // 7 transitions after fake.stop() — LinkLost, then (Connecting,
        // LinkLost) once per attempt:
        //   LinkLost   (watchdog trips, attempt counter 0)
        //   Connecting (reconnect timeout, attempt 1) → LinkLost (watchdog)
        //   Connecting (reconnect timeout, attempt 2) → LinkLost (watchdog)
        //   Connecting (reconnect timeout, attempt 3) → LinkLost (watchdog)
        // The next reconnect timeout finds attempts == kMaxReconnectAttempts
        // and returns without a transition, so the chain terminates here.
        //
        // Measured wall time for those 7 transitions on an idle machine is
        // ~420ms (each cycle is the 50ms reconnect interval plus the
        // watchdog re-trip, which costs a few 25ms kWatchdogTickMs ticks
        // because onReconnectTimeout does not reset m_lastEp6At). The 4000ms
        // ceiling is a generous upper bound only — QTRY returns as soon as
        // the count is reached, so a healthy run still finishes in well
        // under a second.
        QTRY_VERIFY_WITH_TIMEOUT(transitions.count() >= 7, 4000);
        QCOMPARE(transitions.count(), 7);
        QCOMPARE(conn.state(), ConnectionState::LinkLost);

        // Confirm the retries really are bounded: wait 250ms (5x the
        // compressed reconnect interval) and require that NO further
        // transition occurred. Asserting on the spy count rather than on
        // state() alone is deliberate — an unbounded 4th retry would go
        // Connecting → LinkLost and could leave state() reading LinkLost
        // again by the time we sampled it.
        QTest::qWait(250);
        QCOMPARE(transitions.count(), 7);
        QCOMPARE(conn.state(), ConnectionState::LinkLost);

        conn.disconnect();
    }
};

QTEST_MAIN(TestReconnectOnSilence)
#include "tst_reconnect_on_silence.moc"
