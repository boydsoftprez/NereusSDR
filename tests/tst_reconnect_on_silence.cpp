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
#include <QElapsedTimer>
#include <QSignalSpy>

#include <memory>

#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

class TestReconnectOnSilence : public QObject {
    Q_OBJECT
private:
    // Compress the reconnect timeline ~13x: watchdog 2000ms -> 150ms,
    // reconnect interval 5000ms -> 300ms. Without this the second test
    // method sleeps 42 real seconds and sets the parallel floor for
    // the entire suite.
    //
    // 150ms is deliberately NOT tighter. The silence threshold must sit
    // comfortably above BOTH the watchdog's own sampling period
    // (kWatchdogTickMs = 25ms) and P1FakeRadio's ep6 stream cadence
    // (10ms, P1FakeRadio.cpp:54), because a frame is only observed
    // after a subsequent UDP readyRead dispatch. An earlier revision
    // used 20ms, which is BELOW the 25ms watchdog tick: any event-loop
    // stall past 20ms would trip a spurious LinkLost while the fake was
    // still streaming. That passes on an idle dev machine and goes
    // flaky on loaded CI. 150ms leaves 6 watchdog ticks of margin.
    static constexpr int kTestWatchdogSilenceMs   = 150;
    static constexpr int kTestReconnectIntervalMs = 300;

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

    // Arrange step: bring a fresh P1RadioConnection up to Connected against
    // `fake`, re-arming if the connect watchdog tears an attempt down.
    // Returns nullptr if no attempt reached Connected.
    //
    // connectToRadio() starts the connect watchdog, a hard 2000 ms deadline
    // (kConnectTimeoutMs, P1RadioConnection.h) that setReconnectTimingForTest()
    // does NOT compress. If no ep6 frame lands inside it, onConnectTimeout()
    // closes the socket, sets Disconnected and emits connectFailed(Timeout).
    // That teardown is terminal for the object: with the socket closed, no
    // later frame can revive it, so every subsequent wait for Connected is
    // unsatisfiable and burns its full timeout before failing.
    //
    // Here the "radio" is a QTimer inside this same single-threaded process,
    // so a machine loaded enough to starve the event loop for two seconds
    // starves the fake's ep6 tick with it, and the watchdog fires while the
    // fake is perfectly healthy. Measured 2026-08-01, running this binary at
    // background QoS (E-cores) against 192 competing background-QoS spinners:
    // the pre-fix test failed 8 of 12 runs, always at the first wait for
    // Connected, and the correlation was exact. The 8 failing logs each
    // carried one "Connect watchdog fired, no ep6 frame within 2000 ms" and
    // the 4 passing logs carried none, with nothing wrong anywhere in the
    // reconnect path this test actually covers. A real radio streams ep6
    // whether or not this process is scheduled, so the confound is an
    // artifact of the in-process fake rather than a product bug.
    //
    // Re-arming keeps the coverage intact: the silence path below still runs
    // only against a link that genuinely reached Connected, and a connect
    // path that is really broken still fails after kConnectAttempts tries.
    static std::unique_ptr<P1RadioConnection> bringLinkUp(const P1FakeRadio& fake) {
        constexpr int kConnectAttempts  = 4;
        constexpr int kPerAttemptWaitMs = 3000;

        for (int attempt = 0; attempt < kConnectAttempts; ++attempt) {
            auto conn = std::make_unique<P1RadioConnection>();
            conn->init();
            conn->setReconnectTimingForTest(kTestWatchdogSilenceMs,
                                            kTestReconnectIntervalMs);
            conn->connectToRadio(makeInfo(fake));

            QElapsedTimer waited;
            waited.start();
            while (waited.elapsed() < kPerAttemptWaitMs) {
                if (conn->state() == ConnectionState::Connected) {
                    return conn;
                }
                if (conn->state() == ConnectionState::Disconnected) {
                    // Connect watchdog fired: this object is finished, so stop
                    // waiting on it and build a fresh one.
                    break;
                }
                QTest::qWait(10);
            }
        }
        return nullptr;
    }

private slots:
    void silenceTriggersErrorThenRecoversOnResume() {
        P1FakeRadio fake;
        fake.start();

        std::unique_ptr<P1RadioConnection> connPtr = bringLinkUp(fake);
        QVERIFY2(connPtr != nullptr,
                 "link never reached Connected in any connect attempt");
        P1RadioConnection& conn = *connPtr;

        // Wait for the fake to see the metis-start and begin streaming.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 2000);

        // Hand the radio back the moment the link is declared lost, driven by
        // the transition itself rather than by a polled sample of state().
        // Polling raced the retry chain: QTRY samples every 50 ms, the first
        // retry fires kTestReconnectIntervalMs after the trip, and the chain
        // is capped at kMaxReconnectAttempts (3). A poll that ran late enough
        // could therefore resume the fake only after the connection had spent
        // its entire retry budget, and after that nothing brings the link back
        // up. Reacting to the signal removes the window entirely: resume()
        // lands in the same event-loop turn that declared LinkLost, which is
        // before the first retry by construction.
        bool sawLinkLost = false;
        QObject::connect(&conn, &P1RadioConnection::connectionStateChanged,
                         &conn, [&fake, &sawLinkLost](ConnectionState s) {
                             if (s == ConnectionState::LinkLost && !sawLinkLost) {
                                 sawLinkLost = true;
                                 fake.resume();
                             }
                         });

        // Kick the fake into silence: it stops replying to ep2 and stops ep6.
        fake.goSilent();

        // Watchdog trips after the compressed silence threshold -> LinkLost.
        // Asserted on the recorded transition rather than on a sampled
        // state(), so the check cannot miss a LinkLost the retry chain has
        // already moved past.
        QTRY_VERIFY_WITH_TIMEOUT(sawLinkLost, 2000);

        // resume() ran on that transition, so the first reconnect attempt
        // (metis-stop + metis-start, kTestReconnectIntervalMs later) reaches a
        // radio that answers, and the ep6 stream restores the link.
        QTRY_VERIFY_WITH_TIMEOUT(
            conn.state() == ConnectionState::Connected, 2000);

        conn.disconnect();
        fake.stop();
    }

    void boundedRetriesExhaustStayInLinkLost() {
        P1FakeRadio fake;
        fake.start();

        std::unique_ptr<P1RadioConnection> connPtr = bringLinkUp(fake);
        QVERIFY2(connPtr != nullptr,
                 "link never reached Connected in any connect attempt");
        P1RadioConnection& conn = *connPtr;

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
        // the test compresses ~13x via setReconnectTimingForTest():
        //   watchdog silence  2000ms -> 150ms
        //   reconnect interval 5000ms -> 300ms
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
        // Expected wall time for those 7 transitions is ~1.9s: each cycle is
        // the 300ms reconnect interval plus the watchdog re-trip, which costs
        // 150ms plus a few 25ms kWatchdogTickMs ticks because
        // onReconnectTimeout does not reset m_lastEp6At. The 10000ms ceiling
        // is a generous upper bound only — QTRY returns as soon as the count
        // is reached, so a healthy run still finishes in ~2s. The headroom is
        // for loaded CI, where every timer in the chain stretches.
        QTRY_VERIFY_WITH_TIMEOUT(transitions.count() >= 7, 10000);
        QCOMPARE(transitions.count(), 7);
        QCOMPARE(conn.state(), ConnectionState::LinkLost);

        // Confirm the retries really are bounded: wait 1500ms (5x the
        // compressed reconnect interval) and require that NO further
        // transition occurred. Asserting on the spy count rather than on
        // state() alone is deliberate — an unbounded 4th retry would go
        // Connecting → LinkLost and could leave state() reading LinkLost
        // again by the time we sampled it.
        QTest::qWait(1500);
        QCOMPARE(transitions.count(), 7);
        QCOMPARE(conn.state(), ConnectionState::LinkLost);

        conn.disconnect();
    }
};

QTEST_MAIN(TestReconnectOnSilence)
#include "tst_reconnect_on_silence.moc"
