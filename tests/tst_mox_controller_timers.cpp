// =================================================================
// tests/tst_mox_controller_timers.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
//
// Verifies the 6 QTimer chains added in Phase 3M-1a Task B.3:
//   - Default timer intervals match Thetis [v2.10.3.13] constants.
//   - RX→TX walk: Rx → RxToTxRfDelay → Tx (rfDelay timer).
//   - TX→RX walk: Tx → TxToRxInFlight → TxToRxFlush → Rx
//     (keyUpDelayTimer → pttOutDelayTimer).
//   - moxStateChanged(true)  fires at END of RX→TX walk, not at start.
//   - moxStateChanged(false) fires at END of TX→RX walk, not at start.
//   - stateChanged signals fire in expected order.
//   - With 0ms intervals, walk completes via processEvents() alone.
//   - breakInDelay timer is NEVER started in 3M-1a.
//   - spaceDelay timer is NEVER started when kSpaceDelayMs == 0.
//
// Two test strategies:
//   1. Zero-interval (synchronous): setTimerIntervals(0,...) + processEvents()
//      for ordering and signal correctness.
//   2. Wall-clock (cadence): QTest::qWait() with small overheads to verify
//      the timers actually fire after real elapsed time.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QTimer>

#include "core/MoxController.h"

using namespace NereusSDR;

// ---------------------------------------------------------------------------

class TestMoxControllerTimers : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // §1 — Default constant values
    //
    // Verify the kXxxMs constants match the Thetis field declarations:
    //   console.cs:19659-19698 [v2.10.3.13] — mox_delay=10, space_mox_delay=0,
    //     key_up_delay=10, rf_delay=30, ptt_out_delay=20
    //   console.cs:18494      [v2.10.3.13] — break_in_delay=300
    // ════════════════════════════════════════════════════════════════════════

    void constants_rfDelay_is30()
    {
        // From Thetis console.cs:19687 — private int rf_delay = 30 [v2.10.3.13]
        QCOMPARE(MoxController::kRfDelayMs, 30);
    }

    void constants_moxDelay_is10()
    {
        // From Thetis console.cs:19659 — private int mox_delay = 10 [v2.10.3.13]
        QCOMPARE(MoxController::kMoxDelayMs, 10);
    }

    void constants_spaceDelay_is0()
    {
        // From Thetis console.cs:19669 — private int space_mox_delay = 0 [v2.10.3.13]
        QCOMPARE(MoxController::kSpaceDelayMs, 0);
    }

    void constants_keyUpDelay_is10()
    {
        // From Thetis console.cs:19677 — private int key_up_delay = 10 [v2.10.3.13]
        QCOMPARE(MoxController::kKeyUpDelayMs, 10);
    }

    void constants_pttOutDelay_is20()
    {
        // From Thetis console.cs:19694 — private int ptt_out_delay = 20 [v2.10.3.13]
        QCOMPARE(MoxController::kPttOutDelayMs, 20);
    }

    void constants_breakInDelay_is300()
    {
        // From Thetis console.cs:18494 — private double break_in_delay = 300 [v2.10.3.13]
        // 3M-2 CW QSK; not started in 3M-1a.
        QCOMPARE(MoxController::kBreakInDelayMs, 300);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §2 — RX→TX walk (zero-interval, synchronous)
    // ════════════════════════════════════════════════════════════════════════

    // Walk: Rx → RxToTxRfDelay (immediate timer) → Tx

    void rxToTx_stateWalk_Rx_RfDelay_Tx()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::stateChanged);

        ctrl.setMox(true);
        // After setMox: state == RxToTxRfDelay, timer pending.
        QCOMPARE(ctrl.state(), MoxState::RxToTxRfDelay);
        // moxStateChanged has NOT fired yet.
        // Process events to fire the 0ms rfDelay timer.
        QCoreApplication::processEvents();
        // Now state should be Tx.
        QCOMPARE(ctrl.state(), MoxState::Tx);

        // Spy should show: RxToTxRfDelay, then Tx — exactly 2 emissions.
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).value<MoxState>(), MoxState::RxToTxRfDelay);
        QCOMPARE(spy.at(1).at(0).value<MoxState>(), MoxState::Tx);
    }

    void rxToTx_moxStateChanged_firesAtEnd_notAtStart()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(true);
        // BEFORE processEvents: timer hasn't fired; moxStateChanged must
        // not have fired yet (it fires at end of walk).
        QCOMPARE(spy.count(), 0);

        QCoreApplication::processEvents();
        // AFTER processEvents: rfDelay timer has fired; walk complete.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void rxToTx_isMox_isTrueBeforeTimerFires()
    {
        // m_mox is committed in setMox() (Step 3) BEFORE the timer starts.
        // So isMox() is true immediately after setMox(true), even before
        // the rfDelay timer fires.
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QVERIFY(ctrl.isMox()); // committed synchronously in setMox()

        QCoreApplication::processEvents();
        QVERIFY(ctrl.isMox()); // still true after walk completes
    }

    // ════════════════════════════════════════════════════════════════════════
    // §3 — TX→RX walk (zero-interval, synchronous)
    // ════════════════════════════════════════════════════════════════════════

    // Walk: Tx → TxToRxInFlight (10ms mox_delay) → TxToRxFlush (20ms pttOutDelay) → Rx

    void txToRx_stateWalk_Tx_InFlight_Flush_Rx()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        // First get into Tx state.
        ctrl.setMox(true);
        QCoreApplication::processEvents();
        QCOMPARE(ctrl.state(), MoxState::Tx);

        QSignalSpy spy(&ctrl, &MoxController::stateChanged);

        ctrl.setMox(false);
        // After setMox: state == TxToRxInFlight, keyUpTimer pending.
        QCOMPARE(ctrl.state(), MoxState::TxToRxInFlight);
        // moxStateChanged must not have fired yet.

        QCoreApplication::processEvents(); // fires keyUpDelayTimer → TxToRxFlush
        QCOMPARE(ctrl.state(), MoxState::TxToRxFlush);

        QCoreApplication::processEvents(); // fires pttOutDelayTimer → Rx
        QCOMPARE(ctrl.state(), MoxState::Rx);

        // Spy: TxToRxInFlight, TxToRxFlush, Rx — exactly 3 emissions.
        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.at(0).at(0).value<MoxState>(), MoxState::TxToRxInFlight);
        QCOMPARE(spy.at(1).at(0).value<MoxState>(), MoxState::TxToRxFlush);
        QCOMPARE(spy.at(2).at(0).value<MoxState>(), MoxState::Rx);
    }

    void txToRx_moxStateChanged_firesAtEnd_notAtStart()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy spy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(false);
        // After setMox: NOT fired yet.
        QCOMPARE(spy.count(), 0);

        QCoreApplication::processEvents(); // fires keyUpDelayTimer
        // Still in TxToRxFlush — not yet fired.
        QCOMPARE(spy.count(), 0);

        QCoreApplication::processEvents(); // fires pttOutDelayTimer → Rx
        // NOW fired.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    void txToRx_isMox_isFalseBeforeTimerFires()
    {
        // m_mox is committed in setMox() (Step 3) synchronously.
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        ctrl.setMox(false);
        QVERIFY(!ctrl.isMox()); // committed synchronously in setMox()
    }

    // ════════════════════════════════════════════════════════════════════════
    // §4 — Full Rx→Tx→Rx round-trip signal ordering
    // ════════════════════════════════════════════════════════════════════════

    void fullRoundTrip_stateChangedOrder()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::stateChanged);

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        ctrl.setMox(false);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        // Expected order: RxToTxRfDelay, Tx, TxToRxInFlight, TxToRxFlush, Rx
        QCOMPARE(spy.count(), 5);
        QCOMPARE(spy.at(0).at(0).value<MoxState>(), MoxState::RxToTxRfDelay);
        QCOMPARE(spy.at(1).at(0).value<MoxState>(), MoxState::Tx);
        QCOMPARE(spy.at(2).at(0).value<MoxState>(), MoxState::TxToRxInFlight);
        QCOMPARE(spy.at(3).at(0).value<MoxState>(), MoxState::TxToRxFlush);
        QCOMPARE(spy.at(4).at(0).value<MoxState>(), MoxState::Rx);
    }

    void fullRoundTrip_moxStateChangedOrder()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        ctrl.setMox(false);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        // Exactly 2 emissions: true at end of TX walk, false at end of RX walk.
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §5 — breakInDelay timer NOT started in 3M-1a
    // ════════════════════════════════════════════════════════════════════════

    void breakInTimer_neverStarted_inRxToTxPath()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        // Spy on breakInDelayTimer's timeout signal via a member accessor.
        // We can't access m_breakInDelayTimer directly, so we use the
        // public kBreakInDelayMs constant to verify the value and trust the
        // slot body's documented "not started in 3M-1a" guarantee.
        // Functional verification: a full RX→TX→RX cycle completes without
        // any unexpected stateChanged emissions that would indicate
        // TxToRxBreakIn was entered.
        QSignalSpy spy(&ctrl, &MoxController::stateChanged);

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        ctrl.setMox(false);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        // Verify TxToRxBreakIn was NEVER emitted by stateChanged.
        for (int i = 0; i < spy.count(); ++i) {
            MoxState s = spy.at(i).at(0).value<MoxState>();
            QVERIFY2(s != MoxState::TxToRxBreakIn,
                     "TxToRxBreakIn state entered in 3M-1a — breakInTimer fired unexpectedly");
        }
    }

    void breakInTimer_neverStarted_inTxToRxPath()
    {
        // Same check from TX→RX direction only.
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy spy(&ctrl, &MoxController::stateChanged);

        ctrl.setMox(false);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        for (int i = 0; i < spy.count(); ++i) {
            MoxState s = spy.at(i).at(0).value<MoxState>();
            QVERIFY2(s != MoxState::TxToRxBreakIn,
                     "TxToRxBreakIn state entered — breakInDelayTimer must not start in 3M-1a");
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // §6 — spaceDelay timer NOT started when kSpaceDelayMs == 0
    // ════════════════════════════════════════════════════════════════════════

    void spaceDelayTimer_neverFires_whenKSpaceDelayIsZero()
    {
        // kSpaceDelayMs == 0, so space_mox_delay skip guard is active.
        // Verify TxToRxBreakIn (the state that would be driven by a
        // non-zero spaceDelay in future) is never reached.
        QCOMPARE(MoxController::kSpaceDelayMs, 0);

        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy spy(&ctrl, &MoxController::stateChanged);

        ctrl.setMox(false);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        // Confirm walk went InFlight → Flush → Rx, no space-delay-driven state.
        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.at(0).at(0).value<MoxState>(), MoxState::TxToRxInFlight);
        QCOMPARE(spy.at(1).at(0).value<MoxState>(), MoxState::TxToRxFlush);
        QCOMPARE(spy.at(2).at(0).value<MoxState>(), MoxState::Rx);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §7 — Wall-clock cadence tests (real elapsed time)
    //
    // These tests use QTest::qWait() to verify that the timers actually
    // fire after real elapsed time, not just synchronously. Add generous
    // overhead (2×) to avoid CI flakiness.
    // ════════════════════════════════════════════════════════════════════════

    void rxToTx_wallClock_stateIsTxAfterRfDelay()
    {
        MoxController ctrl;
        // Use default Thetis intervals: rfDelay=30ms.

        ctrl.setMox(true);
        // Immediately after setMox: state == RxToTxRfDelay.
        QCOMPARE(ctrl.state(), MoxState::RxToTxRfDelay);

        // Wait long enough for rfDelay (30ms) to fire — allow 80ms overhead.
        QTest::qWait(80);
        QCOMPARE(ctrl.state(), MoxState::Tx);
    }

    void txToRx_wallClock_stateIsRxAfterBothDelays()
    {
        MoxController ctrl;
        // Default intervals: keyUpDelay=10ms + pttOutDelay=20ms = 30ms total.

        ctrl.setMox(true);
        QTest::qWait(80); // wait for RX→TX to complete first
        QCOMPARE(ctrl.state(), MoxState::Tx);

        ctrl.setMox(false);
        // Immediately after setMox: state == TxToRxInFlight.
        QCOMPARE(ctrl.state(), MoxState::TxToRxInFlight);

        // Wait for both delays (10 + 20 = 30ms total) with generous overhead.
        QTest::qWait(80);
        QCOMPARE(ctrl.state(), MoxState::Rx);
    }

    void rxToTx_wallClock_moxStateChangedFiresAfterRfDelay()
    {
        MoxController ctrl;

        QSignalSpy spy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(true);
        // Before rfDelay fires: no signal.
        QCOMPARE(spy.count(), 0);

        QTest::qWait(80);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void txToRx_wallClock_moxStateChangedFiresAfterBothDelays()
    {
        MoxController ctrl;

        ctrl.setMox(true);
        QTest::qWait(80);

        QSignalSpy spy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(false);
        QCOMPARE(spy.count(), 0);

        QTest::qWait(80);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §8 — Rapid toggle safety: setMox(false) during RX→TX walk
    // ════════════════════════════════════════════════════════════════════════

    void rapidToggle_cancelsPreviousWalk()
    {
        // If the user calls setMox(false) while the RX→TX walk is still
        // in-progress (e.g. in RxToTxRfDelay), stopAllTimers() must cancel
        // the rfDelay timer so it doesn't fire and drive to Tx after we've
        // already started the TX→RX walk.
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(true);
        // Immediately call setMox(false) before processEvents drives rfDelay.
        ctrl.setMox(false);

        // Process all pending events.
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        // State must end at Rx — the rfDelay timer was cancelled.
        QCOMPARE(ctrl.state(), MoxState::Rx);
        // m_mox must be false.
        QVERIFY(!ctrl.isMox());

        // Invariant: stopAllTimers cancelled the rfDelay timer before
        // onRfDelayElapsed could fire, so moxStateChanged(true) was never
        // emitted — only the final TX→RX moxStateChanged(false).
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }
};

QTEST_GUILESS_MAIN(TestMoxControllerTimers)
#include "tst_mox_controller_timers.moc"
