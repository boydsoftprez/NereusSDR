// =================================================================
// tests/tst_mox_controller_tune.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
//
// Verifies the setTune(bool) slot added in Phase 3M-1a Task B.5:
//   - setTune(true) engages MOX + sets manualMox / pttMode = Manual
//   - setTune(false) releases MOX + clears manualMox (not pttMode)
//   - manualMoxChanged(bool) signal on m_manualMox transitions
//   - Idempotent guard on the manualMox flag
//   - Phase signals fire correctly through TUN paths
//   - Codex P2 invariant preserved (runMoxSafetyEffects on every call)
//   - TUN→RAW-MOX→TUN interaction edge cases
//
// Logic ported from Thetis chkTUN_CheckedChanged
// (console.cs:29978-30157 [v2.10.3.13]) — specifically the flag-
// assignment block at lines 30093-30094 and the clear at line 30142.
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
// SpyMoxController — re-uses the pattern from tst_mox_controller_basic.cpp.
//
// Override runMoxSafetyEffects to count how many times it fires and capture
// the argument. Required for the Codex P2 invariant verification test.
// ---------------------------------------------------------------------------
class SpyMoxController : public MoxController {
public:
    explicit SpyMoxController(QObject* parent = nullptr)
        : MoxController(parent)
    {}

    int  safetyCallCount() const { return m_safetyCallCount; }
    bool lastSafetyArg()   const { return m_lastSafetyArg; }

protected:
    void runMoxSafetyEffects(bool newMox) override
    {
        ++m_safetyCallCount;
        m_lastSafetyArg = newMox;
        MoxController::runMoxSafetyEffects(newMox);
    }

private:
    int  m_safetyCallCount{0};
    bool m_lastSafetyArg{false};
};

// ---------------------------------------------------------------------------

class TestMoxControllerTune : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // §1 — setTune(true): TUN-on engages MOX and sets flags
    // ════════════════════════════════════════════════════════════════════════

    void setTuneTrue_isMoxBecomesTrue()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QVERIFY(ctrl.isMox());
    }

    void setTuneTrue_isManualMoxBecomesTrue()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QVERIFY(ctrl.isManualMox());
    }

    void setTuneTrue_pttModeBecomesManual()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QCOMPARE(ctrl.pttMode(), PttMode::Manual);
    }

    void setTuneTrue_manualMoxChangedFires_once_withTrue()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        QSignalSpy spy(&ctrl, &MoxController::manualMoxChanged);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setTuneTrue_pttModeChangedFires_once_withManual()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        QSignalSpy spy(&ctrl, &MoxController::pttModeChanged);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<PttMode>(), PttMode::Manual);
    }

    void setTuneTrue_stateBecomesToTx()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QCOMPARE(ctrl.state(), MoxState::Tx);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §2 — setTune(false): TUN-off releases MOX and clears manualMox
    // ════════════════════════════════════════════════════════════════════════

    void setTuneFalse_isMoxBecomesFalse()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        ctrl.setTune(false);
        drainTxToRxWalk();
        QVERIFY(!ctrl.isMox());
    }

    void setTuneFalse_isManualMoxBecomesFalse()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        ctrl.setTune(false);
        drainTxToRxWalk();
        QVERIFY(!ctrl.isManualMox());
    }

    void setTuneFalse_pttModeStaysManual()
    {
        // Per Thetis: _current_ptt_mode is NOT cleared in chkTUN's TUN-off
        // path. It clears indirectly via chkMOX_CheckedChanged2 TX→RX branch
        // (console.cs:29496 [v2.10.3.13]). In NereusSDR that belongs to F.1.
        // So after setTune(false), m_pttMode must still be Manual.
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        ctrl.setTune(false);
        drainTxToRxWalk();
        QCOMPARE(ctrl.pttMode(), PttMode::Manual);
    }

    void setTuneFalse_manualMoxChangedFires_once_withFalse()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();

        QSignalSpy spy(&ctrl, &MoxController::manualMoxChanged);
        ctrl.setTune(false);
        drainTxToRxWalk();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    void setTuneFalse_stateBecomesRx()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        ctrl.setTune(false);
        drainTxToRxWalk();
        QCOMPARE(ctrl.state(), MoxState::Rx);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §3 — Phase signals fire correctly through TUN paths
    //
    // setTune(true) → txAboutToBegin → hardwareFlipped(true) → txReady
    // setTune(false) → txAboutToEnd → hardwareFlipped(false) → txaFlushed → rxReady
    // ════════════════════════════════════════════════════════════════════════

    void setTuneTrue_rxToTxPhaseSignalOrder()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QStringList log;
        connect(&ctrl, &MoxController::manualMoxChanged,
                this, [&](bool v) { log.append(v ? "manualMoxChanged(true)" : "manualMoxChanged(false)"); });
        connect(&ctrl, &MoxController::pttModeChanged,
                this, [&](PttMode) { log.append("pttModeChanged(Manual)"); });
        connect(&ctrl, &MoxController::txAboutToBegin,
                this, [&]() { log.append("txAboutToBegin"); });
        connect(&ctrl, &MoxController::hardwareFlipped,
                this, [&](bool isTx) { log.append(isTx ? "hardwareFlipped(true)" : "hardwareFlipped(false)"); });
        connect(&ctrl, &MoxController::txReady,
                this, [&]() { log.append("txReady"); });

        ctrl.setTune(true);
        QCoreApplication::processEvents();

        // Expected order: flags set before setMox (NereusSDR ordering choice),
        // then the phase signals in Codex P1 order.
        QCOMPARE(log.size(), 5);
        QCOMPARE(log.at(0), QStringLiteral("pttModeChanged(Manual)"));
        QCOMPARE(log.at(1), QStringLiteral("manualMoxChanged(true)"));
        QCOMPARE(log.at(2), QStringLiteral("txAboutToBegin"));
        QCOMPARE(log.at(3), QStringLiteral("hardwareFlipped(true)"));
        QCOMPARE(log.at(4), QStringLiteral("txReady"));
    }

    void setTuneFalse_txToRxPhaseSignalOrder()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
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
        connect(&ctrl, &MoxController::manualMoxChanged,
                this, [&](bool v) { log.append(v ? "manualMoxChanged(true)" : "manualMoxChanged(false)"); });

        ctrl.setTune(false);
        drainTxToRxWalk();

        // Expected order:
        //   setMox(false) is called synchronously from setTune(false) and emits
        //   the two synchronous phase signals (txAboutToEnd, hardwareFlipped(false))
        //   BEFORE returning. After setMox(false) returns, setTune(false) clears
        //   m_manualMox and emits manualMoxChanged(false) synchronously.
        //   The timer-driven signals (txaFlushed, rxReady) fire only on
        //   drainTxToRxWalk(). So the actual order is:
        //     txAboutToEnd → hardwareFlipped(false) → manualMoxChanged(false)
        //     → txaFlushed → rxReady
        //
        // This ordering confirms the §8 invariant: m_manualMox is still TRUE at
        // txAboutToEnd / hardwareFlipped(false) time (synchronous, inside setMox),
        // and is cleared to false immediately after setMox(false) returns (before
        // any timer-driven signals). Phase-signal subscribers that need m_manualMox
        // to distinguish a TUN-release from a raw MOX-release should check it in
        // their txAboutToEnd or hardwareFlipped(false) slot — not in rxReady.
        QCOMPARE(log.size(), 5);
        QCOMPARE(log.at(0), QStringLiteral("txAboutToEnd"));
        QCOMPARE(log.at(1), QStringLiteral("hardwareFlipped(false)"));
        QCOMPARE(log.at(2), QStringLiteral("manualMoxChanged(false)"));
        QCOMPARE(log.at(3), QStringLiteral("txaFlushed"));
        QCOMPARE(log.at(4), QStringLiteral("rxReady"));
    }

    // ════════════════════════════════════════════════════════════════════════
    // §4 — Idempotent guard on the manualMox flag
    //
    // setTune(true) twice → manualMoxChanged fires only once (first call)
    // because the flag is already true on the second call.
    // MOX itself is also idempotent (setMox guard prevents second walk).
    // ════════════════════════════════════════════════════════════════════════

    void setTuneTrue_twice_manualMoxChangedFires_once()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        QSignalSpy spy(&ctrl, &MoxController::manualMoxChanged);

        ctrl.setTune(true);
        QCoreApplication::processEvents();

        ctrl.setTune(true); // idempotent — manualMox already true
        QCoreApplication::processEvents();

        QCOMPARE(spy.count(), 1); // only the first call emits
    }

    void setTuneTrue_twice_isManualMoxStillTrue()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QVERIFY(ctrl.isManualMox());
    }

    // ════════════════════════════════════════════════════════════════════════
    // §5 — TUN→RAW-MOX→TUN combinations
    //
    // 5a: Direct setMox(true) then setTune(true):
    //     MOX was already on; manualMox transitions from false→true.
    //     manualMoxChanged(true) fires; isMox remains true.
    //
    // 5b: setTune(true) then setMox(false):
    //     MOX releases but manualMox stays true (setMox doesn't touch it).
    //     Then setTune(false): MOX already off (idempotent setMox), but
    //     manualMox transitions true→false; manualMoxChanged(false) fires.
    // ════════════════════════════════════════════════════════════════════════

    void rawMoxThenTune_manualMoxChangedFires()
    {
        // 5a: setMox(true) directly, then setTune(true)
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        QVERIFY(!ctrl.isManualMox()); // raw MOX does not set manualMox

        QSignalSpy spy(&ctrl, &MoxController::manualMoxChanged);
        ctrl.setTune(true);
        QCoreApplication::processEvents();

        // manualMox flips false→true even though MOX was already on
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isManualMox());
        QVERIFY(ctrl.isMox());
    }

    void tuneThenRawMoxOff_manualMoxPersists()
    {
        // 5b part 1: setTune(true) then setMox(false)
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QVERIFY(ctrl.isManualMox());

        // Raw setMox(false) releases MOX but must NOT touch m_manualMox
        ctrl.setMox(false);
        drainTxToRxWalk();
        QVERIFY(!ctrl.isMox());
        QVERIFY(ctrl.isManualMox()); // still true — setMox doesn't clear it
    }

    void tuneThenRawMoxOff_thenTuneOff_clearsManualMox()
    {
        // 5b part 2: after setTune(true) + setMox(false), setTune(false)
        // MOX is already off; setMox(false) inside setTune is idempotent.
        // But m_manualMox must still transition true→false.
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setTune(true);
        QCoreApplication::processEvents();
        ctrl.setMox(false);
        drainTxToRxWalk();
        QVERIFY(ctrl.isManualMox()); // still set from setTune(true)

        QSignalSpy spy(&ctrl, &MoxController::manualMoxChanged);
        ctrl.setTune(false);
        drainTxToRxWalk(); // setMox(false) is idempotent; timers don't fire

        QVERIFY(!ctrl.isManualMox());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §6 — Codex P2 invariant under TUN
    //
    // runMoxSafetyEffects must fire on every setMox call, including
    // the idempotent one inside setTune(true) called twice.
    // setTune(true) always calls setMox(true), which always calls
    // runMoxSafetyEffects — even when m_mox is already true.
    // ════════════════════════════════════════════════════════════════════════

    void setTuneTrue_codexP2_safetyEffectFires()
    {
        SpyMoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        // First setTune(true): real transition (Rx→Tx).
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QCOMPARE(ctrl.safetyCallCount(), 1);
        QVERIFY(ctrl.lastSafetyArg());

        // Second setTune(true): setMox(true) still calls safety effects,
        // even though m_mox is already true (Codex P2 invariant).
        ctrl.setTune(true);
        QCoreApplication::processEvents();
        QCOMPARE(ctrl.safetyCallCount(), 2);
        QVERIFY(ctrl.lastSafetyArg());
    }

    void setTuneFalse_codexP2_safetyEffectFires()
    {
        SpyMoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        ctrl.setTune(true);
        QCoreApplication::processEvents();
        int countAfterOn = ctrl.safetyCallCount();

        ctrl.setTune(false);
        drainTxToRxWalk();

        // setTune(false) calls setMox(false) which always runs safety effects.
        QCOMPARE(ctrl.safetyCallCount(), countAfterOn + 1);
        QVERIFY(!ctrl.lastSafetyArg());
    }

    // ════════════════════════════════════════════════════════════════════════
    // §7 — Flags are set BEFORE setMox (NereusSDR ordering choice)
    //
    // Verify that when txAboutToBegin fires (synchronously inside setMox),
    // m_manualMox is already true. This confirms the "flags before MOX"
    // ordering that lets phase-signal subscribers see a consistent snapshot.
    // ════════════════════════════════════════════════════════════════════════

    void setTuneTrue_manualMoxTrue_atTxAboutToBeginTime()
    {
        class OrderProbe : public QObject {
        public:
            MoxController* ctrl{nullptr};
            bool manualMoxAtEmitTime{false};
            PttMode pttModeAtEmitTime{PttMode::None};
        };
        OrderProbe probe;

        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        probe.ctrl = &ctrl;

        connect(&ctrl, &MoxController::txAboutToBegin,
                &probe, [&]() {
                    probe.manualMoxAtEmitTime = ctrl.isManualMox();
                    probe.pttModeAtEmitTime   = ctrl.pttMode();
                });

        ctrl.setTune(true);
        QCoreApplication::processEvents();

        QVERIFY(probe.manualMoxAtEmitTime);
        QCOMPARE(probe.pttModeAtEmitTime, PttMode::Manual);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §8 — manualMox stays true during TX→RX phase signals
    //
    // When setTune(false) is called, setMox(false) fires phase signals
    // (txAboutToEnd, hardwareFlipped(false)) BEFORE manualMox is cleared.
    // Verify by capturing isManualMox() inside txAboutToEnd.
    // ════════════════════════════════════════════════════════════════════════

    void setTuneFalse_manualMoxStillTrue_atTxAboutToEndTime()
    {
        bool manualMoxAtTxAboutToEnd = false;

        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        connect(&ctrl, &MoxController::txAboutToEnd,
                &ctrl, [&]() { manualMoxAtTxAboutToEnd = ctrl.isManualMox(); });

        ctrl.setTune(true);
        QCoreApplication::processEvents();
        ctrl.setTune(false);
        drainTxToRxWalk();

        // At txAboutToEnd time (inside setMox(false)), manualMox must still be true.
        QVERIFY(manualMoxAtTxAboutToEnd);
    }
};

QTEST_GUILESS_MAIN(TestMoxControllerTune)
#include "tst_mox_controller_tune.moc"
