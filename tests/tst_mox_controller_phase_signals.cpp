// =================================================================
// tests/tst_mox_controller_phase_signals.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
//
// Verifies the 6 phase signals added in Phase 3M-1a Task B.4:
//   txAboutToBegin()        — RX→TX entry
//   hardwareFlipped(bool)   — hardware routing committed/released (Option B)
//   txReady()               — RX→TX terminal (TX I/Q stream on)
//   txAboutToEnd()          — TX→RX entry
//   txaFlushed()            — TX→RX: in-flight samples cleared
//   rxReady()               — TX→RX terminal (RX channel on)
//
// Codex P1 invariant: subscribers attach to phase boundary signals,
// not to individual low-level setters. This test file is the
// mechanical enforcement of that invariant.
//
// Phase signals derived from chkMOX_CheckedChanged2 RX→TX/TX→RX
// ordering (console.cs:29311-29678 [v2.10.3.13]).
// See pre-code review §1.4 for emit point rationale.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QCoreApplication>

#include "core/MoxController.h"

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// Helper: run processEvents() enough times to drain the full TX→RX walk
// (two 0ms timers: keyUpDelay → TxToRxFlush, then pttOutDelay → Rx).
// ---------------------------------------------------------------------------
static void drainTxToRxWalk()
{
    QCoreApplication::processEvents(); // keyUpDelayTimer → TxToRxFlush
    QCoreApplication::processEvents(); // pttOutDelayTimer → Rx
}

// ---------------------------------------------------------------------------

class TestMoxControllerPhaseSignals : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // §1 — RX→TX phase signal ordering
    //
    // Full RX→TX walk with 0ms timers (synchronous):
    //   setMox(true) emits:  txAboutToBegin, hardwareFlipped(true)
    //   onRfDelayElapsed emits: txReady, moxStateChanged(true)
    // ════════════════════════════════════════════════════════════════════════

    void rxToTx_txAboutToBegin_firesExactlyOnce()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::txAboutToBegin);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(spy.count(), 1);
    }

    void rxToTx_hardwareFlipped_fires_isTx_true()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::hardwareFlipped);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void rxToTx_txReady_firesExactlyOnce()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::txReady);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(spy.count(), 1);
    }

    // Order within the RX→TX walk:
    //   txAboutToBegin → hardwareFlipped(true) → txReady → moxStateChanged(true)
    //
    // We verify this by connecting all four to a shared "signal log" that
    // records arrival order. QSignalSpy is per-signal, so we use a small
    // recorder lambda instead.
    void rxToTx_phaseSignalOrder()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QStringList log;
        connect(&ctrl, &MoxController::txAboutToBegin,
                this, [&]() { log.append("txAboutToBegin"); });
        connect(&ctrl, &MoxController::hardwareFlipped,
                this, [&](bool isTx) { log.append(isTx ? "hardwareFlipped(true)" : "hardwareFlipped(false)"); });
        connect(&ctrl, &MoxController::txReady,
                this, [&]() { log.append("txReady"); });
        connect(&ctrl, &MoxController::moxStateChanged,
                this, [&](bool on) { log.append(on ? "moxStateChanged(true)" : "moxStateChanged(false)"); });

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(log.size(), 4);
        QCOMPARE(log.at(0), QStringLiteral("txAboutToBegin"));
        QCOMPARE(log.at(1), QStringLiteral("hardwareFlipped(true)"));
        QCOMPARE(log.at(2), QStringLiteral("txReady"));
        QCOMPARE(log.at(3), QStringLiteral("moxStateChanged(true)"));
    }

    // hardwareFlipped fires BEFORE rfDelay timer elapses (i.e., at setMox
    // call time, not after processEvents). Verify by checking spy count
    // before and after processEvents.
    void rxToTx_hardwareFlipped_firesBeforeRfDelay()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy hardwareSpy(&ctrl, &MoxController::hardwareFlipped);
        QSignalSpy txReadySpy(&ctrl, &MoxController::txReady);

        ctrl.setMox(true);

        // BEFORE processEvents: hardwareFlipped must have fired (synchronous
        // emit in setMox body), txReady must NOT have fired yet (it fires in
        // the rfDelay slot, driven by processEvents).
        QCOMPARE(hardwareSpy.count(), 1);
        QCOMPARE(txReadySpy.count(), 0);

        QCoreApplication::processEvents();

        QCOMPARE(txReadySpy.count(), 1);
    }

    // txAboutToBegin fires before any state advance (still Rx at emit time).
    // Verify by capturing state() inside the signal handler.
    void rxToTx_txAboutToBegin_firesWhileStillInRx()
    {
        class StateProbe : public QObject {
        public:
            MoxController* ctrl{nullptr};
            MoxState stateAtEmit{MoxState::Tx}; // deliberately wrong default
        };
        StateProbe probe;

        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        probe.ctrl = &ctrl;

        connect(&ctrl, &MoxController::txAboutToBegin,
                &probe, [&]() { probe.stateAtEmit = ctrl.state(); });

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        // At txAboutToBegin emit time, the state walk has not yet started;
        // m_state is still Rx.
        QCOMPARE(probe.stateAtEmit, MoxState::Rx);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §2 — TX→RX phase signal ordering
    //
    // Full TX→RX walk with 0ms timers:
    //   setMox(false) emits: txAboutToEnd, hardwareFlipped(false)
    //   onKeyUpDelayElapsed emits: txaFlushed
    //   onPttOutElapsed emits: rxReady, moxStateChanged(false)
    // ════════════════════════════════════════════════════════════════════════

    void txToRx_txAboutToEnd_firesExactlyOnce()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy spy(&ctrl, &MoxController::txAboutToEnd);

        ctrl.setMox(false);
        drainTxToRxWalk();

        QCOMPARE(spy.count(), 1);
    }

    void txToRx_hardwareFlipped_fires_isTx_false()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy spy(&ctrl, &MoxController::hardwareFlipped);

        ctrl.setMox(false);
        drainTxToRxWalk();

        // Exactly one emission for the TX→RX direction.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    void txToRx_txaFlushed_firesExactlyOnce()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy spy(&ctrl, &MoxController::txaFlushed);

        ctrl.setMox(false);
        drainTxToRxWalk();

        QCOMPARE(spy.count(), 1);
    }

    void txToRx_rxReady_firesExactlyOnce()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy spy(&ctrl, &MoxController::rxReady);

        ctrl.setMox(false);
        drainTxToRxWalk();

        QCOMPARE(spy.count(), 1);
    }

    // Order within the TX→RX walk:
    //   txAboutToEnd → hardwareFlipped(false) → txaFlushed → rxReady
    //   → moxStateChanged(false)
    void txToRx_phaseSignalOrder()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QStringList log;
        connect(&ctrl, &MoxController::txAboutToEnd,
                this, [&]() { log.append("txAboutToEnd"); });
        connect(&ctrl, &MoxController::hardwareFlipped,
                this, [&](bool isTx) { log.append(isTx ? "hardwareFlipped(true)" : "hardwareFlipped(false)"); });
        connect(&ctrl, &MoxController::txaFlushed,
                this, [&]() { log.append("txaFlushed"); });
        connect(&ctrl, &MoxController::rxReady,
                this, [&]() { log.append("rxReady"); });
        connect(&ctrl, &MoxController::moxStateChanged,
                this, [&](bool on) { log.append(on ? "moxStateChanged(true)" : "moxStateChanged(false)"); });

        ctrl.setMox(false);
        drainTxToRxWalk();

        QCOMPARE(log.size(), 5);
        QCOMPARE(log.at(0), QStringLiteral("txAboutToEnd"));
        QCOMPARE(log.at(1), QStringLiteral("hardwareFlipped(false)"));
        QCOMPARE(log.at(2), QStringLiteral("txaFlushed"));
        QCOMPARE(log.at(3), QStringLiteral("rxReady"));
        QCOMPARE(log.at(4), QStringLiteral("moxStateChanged(false)"));
    }

    // txAboutToEnd and hardwareFlipped(false) fire at setMox(false) call time
    // (synchronous, before any timer elapses). txaFlushed fires after first
    // processEvents (keyUpDelayTimer). rxReady fires after second processEvents.
    void txToRx_hardwareFlipped_firesBeforeKeyUpDelay()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy hardwareSpy(&ctrl, &MoxController::hardwareFlipped);
        QSignalSpy txaFlushedSpy(&ctrl, &MoxController::txaFlushed);
        QSignalSpy rxReadySpy(&ctrl, &MoxController::rxReady);

        ctrl.setMox(false);

        // BEFORE processEvents: hardwareFlipped(false) must have fired.
        QCOMPARE(hardwareSpy.count(), 1);
        QCOMPARE(hardwareSpy.at(0).at(0).toBool(), false);
        // txaFlushed and rxReady must not have fired yet (timer-driven).
        QCOMPARE(txaFlushedSpy.count(), 0);
        QCOMPARE(rxReadySpy.count(), 0);

        QCoreApplication::processEvents(); // keyUpDelayTimer → txaFlushed + TxToRxFlush

        QCOMPARE(txaFlushedSpy.count(), 1);
        QCOMPARE(rxReadySpy.count(), 0); // pttOutDelayTimer not yet fired

        QCoreApplication::processEvents(); // pttOutDelayTimer → rxReady + Rx

        QCOMPARE(rxReadySpy.count(), 1);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §3 — Full round-trip: RX→TX then TX→RX
    //
    // hardwareFlipped fires twice across the round-trip:
    //   once with isTx=true (RX→TX), once with isTx=false (TX→RX).
    // All 6 phase signals fire exactly once each per direction.
    // ════════════════════════════════════════════════════════════════════════

    void roundTrip_hardwareFlipped_firesTwice_withCorrectPayloads()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy spy(&ctrl, &MoxController::hardwareFlipped);

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        ctrl.setMox(false);
        drainTxToRxWalk();

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toBool(), true);  // RX→TX
        QCOMPARE(spy.at(1).at(0).toBool(), false); // TX→RX
    }

    void roundTrip_allPhaseSignals_fireExactlyOnceEach()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy txAboutToBeginSpy(&ctrl, &MoxController::txAboutToBegin);
        QSignalSpy txReadySpy(&ctrl, &MoxController::txReady);
        QSignalSpy txAboutToEndSpy(&ctrl, &MoxController::txAboutToEnd);
        QSignalSpy txaFlushedSpy(&ctrl, &MoxController::txaFlushed);
        QSignalSpy rxReadySpy(&ctrl, &MoxController::rxReady);

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        ctrl.setMox(false);
        drainTxToRxWalk();

        QCOMPARE(txAboutToBeginSpy.count(), 1);
        QCOMPARE(txReadySpy.count(), 1);
        QCOMPARE(txAboutToEndSpy.count(), 1);
        QCOMPARE(txaFlushedSpy.count(), 1);
        QCOMPARE(rxReadySpy.count(), 1);
    }

    void roundTrip_fullSignalOrder()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QStringList log;
        connect(&ctrl, &MoxController::txAboutToBegin,
                this, [&]() { log.append("txAboutToBegin"); });
        connect(&ctrl, &MoxController::hardwareFlipped,
                this, [&](bool isTx) { log.append(isTx ? "hardwareFlipped(true)" : "hardwareFlipped(false)"); });
        connect(&ctrl, &MoxController::txReady,
                this, [&]() { log.append("txReady"); });
        connect(&ctrl, &MoxController::txAboutToEnd,
                this, [&]() { log.append("txAboutToEnd"); });
        connect(&ctrl, &MoxController::txaFlushed,
                this, [&]() { log.append("txaFlushed"); });
        connect(&ctrl, &MoxController::rxReady,
                this, [&]() { log.append("rxReady"); });

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        ctrl.setMox(false);
        drainTxToRxWalk();

        // Expected order across a full round-trip:
        QCOMPARE(log.size(), 7);
        QCOMPARE(log.at(0), QStringLiteral("txAboutToBegin"));
        QCOMPARE(log.at(1), QStringLiteral("hardwareFlipped(true)"));
        QCOMPARE(log.at(2), QStringLiteral("txReady"));
        QCOMPARE(log.at(3), QStringLiteral("txAboutToEnd"));
        QCOMPARE(log.at(4), QStringLiteral("hardwareFlipped(false)"));
        QCOMPARE(log.at(5), QStringLiteral("txaFlushed"));
        QCOMPARE(log.at(6), QStringLiteral("rxReady"));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §4 — Idempotent guard: phase signals must NOT fire on repeated calls
    //
    // Codex P1 invariant: each phase signal fires ONCE per real transition.
    // A repeated setMox(true) hits the idempotent guard (m_mox is already
    // true) and returns without emitting any phase signal.
    // ════════════════════════════════════════════════════════════════════════

    void idempotent_setMoxTrue_noPhaseSignalOnSecondCall()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy txAboutToBeginSpy(&ctrl, &MoxController::txAboutToBegin);
        QSignalSpy hardwareSpy(&ctrl, &MoxController::hardwareFlipped);
        QSignalSpy txReadySpy(&ctrl, &MoxController::txReady);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        // First call: phase signals must have fired once.
        QCOMPARE(txAboutToBeginSpy.count(), 1);
        QCOMPARE(hardwareSpy.count(), 1);
        QCOMPARE(txReadySpy.count(), 1);

        // Second call: idempotent, must NOT emit phase signals again.
        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(txAboutToBeginSpy.count(), 1);
        QCOMPARE(hardwareSpy.count(), 1);
        QCOMPARE(txReadySpy.count(), 1);
    }

    void idempotent_setMoxFalse_noPhaseSignalOnSecondCall()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy txAboutToEndSpy(&ctrl, &MoxController::txAboutToEnd);
        QSignalSpy txaFlushedSpy(&ctrl, &MoxController::txaFlushed);
        QSignalSpy rxReadySpy(&ctrl, &MoxController::rxReady);

        ctrl.setMox(false);
        drainTxToRxWalk();

        // First TX→RX: phase signals fired once.
        QCOMPARE(txAboutToEndSpy.count(), 1);
        QCOMPARE(txaFlushedSpy.count(), 1);
        QCOMPARE(rxReadySpy.count(), 1);

        // Second call from Rx state: idempotent, no signals.
        ctrl.setMox(false);
        drainTxToRxWalk();

        QCOMPARE(txAboutToEndSpy.count(), 1);
        QCOMPARE(txaFlushedSpy.count(), 1);
        QCOMPARE(rxReadySpy.count(), 1);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §5 — Rapid toggle: setMox(false) mid-way through RX→TX walk
    //
    // When setMox(false) is called before rfDelay fires, the in-flight walk
    // is cancelled (stopAllTimers). txReady must NOT fire because rfDelay
    // slot never runs.
    //
    // Expected phase signal sequence:
    //   setMox(true):   txAboutToBegin, hardwareFlipped(true)  [rfDelay cancelled]
    //   setMox(false):  txAboutToEnd, hardwareFlipped(false)
    //   drain:          txaFlushed, rxReady
    // ════════════════════════════════════════════════════════════════════════

    void rapidToggle_txReady_doesNotFire_whenRfDelayCancelled()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy txReadySpy(&ctrl, &MoxController::txReady);

        ctrl.setMox(true);
        // Immediately cancel by calling setMox(false) before processEvents
        // can drain the rfDelay timer.
        ctrl.setMox(false);
        drainTxToRxWalk();

        // txReady must NOT have fired — rfDelay was cancelled.
        QCOMPARE(txReadySpy.count(), 0);
    }

    void rapidToggle_phaseSignalSequence()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QStringList log;
        connect(&ctrl, &MoxController::txAboutToBegin,
                this, [&]() { log.append("txAboutToBegin"); });
        connect(&ctrl, &MoxController::hardwareFlipped,
                this, [&](bool isTx) { log.append(isTx ? "hardwareFlipped(true)" : "hardwareFlipped(false)"); });
        connect(&ctrl, &MoxController::txReady,
                this, [&]() { log.append("txReady"); });
        connect(&ctrl, &MoxController::txAboutToEnd,
                this, [&]() { log.append("txAboutToEnd"); });
        connect(&ctrl, &MoxController::txaFlushed,
                this, [&]() { log.append("txaFlushed"); });
        connect(&ctrl, &MoxController::rxReady,
                this, [&]() { log.append("rxReady"); });

        ctrl.setMox(true);
        ctrl.setMox(false);      // cancel before rfDelay fires
        drainTxToRxWalk();

        // txReady absent (rfDelay cancelled); txAboutToBegin and
        // hardwareFlipped(true) already emitted synchronously.
        QCOMPARE(log.size(), 6);
        QCOMPARE(log.at(0), QStringLiteral("txAboutToBegin"));
        QCOMPARE(log.at(1), QStringLiteral("hardwareFlipped(true)"));
        QCOMPARE(log.at(2), QStringLiteral("txAboutToEnd"));
        QCOMPARE(log.at(3), QStringLiteral("hardwareFlipped(false)"));
        QCOMPARE(log.at(4), QStringLiteral("txaFlushed"));
        QCOMPARE(log.at(5), QStringLiteral("rxReady"));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §6 — Codex P1: one emission per transition even if internal signals fire
    //
    // Verify that txAboutToBegin fires ONCE per real RX→TX transition.
    // stateChanged fires 2× during the same walk (RxToTxRfDelay + Tx), but
    // txAboutToBegin may only fire once.
    // ════════════════════════════════════════════════════════════════════════

    void codexP1_txAboutToBegin_firesOnce_per_transition()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy txAboutToBeginSpy(&ctrl, &MoxController::txAboutToBegin);
        QSignalSpy stateChangedSpy(&ctrl, &MoxController::stateChanged);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        // stateChanged fires 2× (RxToTxRfDelay + Tx) — this is expected.
        QCOMPARE(stateChangedSpy.count(), 2);
        // txAboutToBegin fires exactly once — Codex P1.
        QCOMPARE(txAboutToBeginSpy.count(), 1);
    }

    void codexP1_rxReady_firesOnce_per_transition()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QSignalSpy rxReadySpy(&ctrl, &MoxController::rxReady);
        QSignalSpy stateChangedSpy(&ctrl, &MoxController::stateChanged);

        ctrl.setMox(false);
        drainTxToRxWalk();

        // stateChanged fires 3× (TxToRxInFlight + TxToRxFlush + Rx).
        QCOMPARE(stateChangedSpy.count(), 3);
        // rxReady fires exactly once — Codex P1.
        QCOMPARE(rxReadySpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestMoxControllerPhaseSignals)
#include "tst_mox_controller_phase_signals.moc"
