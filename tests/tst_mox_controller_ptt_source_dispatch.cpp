// =================================================================
// tests/tst_mox_controller_ptt_source_dispatch.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this test
// file. The test exercises:
//   - MoxController::onMicPttFromRadio(bool)  — H.4 Phase 3M-1b
//   - MoxController::onCatPtt(bool)           — H.4 Phase 3M-1b
//   - MoxController::onVoxActive(bool)        — H.4 Phase 3M-1b
//   - MoxController::onSpacePtt(bool)         — H.4 Phase 3M-1b
//   - MoxController::onX2Ptt(bool)            — H.4 Phase 3M-1b
//   - MoxController::onCwPtt(bool)            — H.4 rejected (3M-2)
//   - MoxController::onTciPtt(bool)           — H.4 rejected (3J)
//
// Source references (for traceability):
//   Thetis Project Files/Source/Console/console.cs [v2.10.3.13]:
//     PollPTT: console.cs:25416-25560
//       _current_ptt_mode = PTTMode.TCI   console.cs:25463
//       _current_ptt_mode = PTTMode.CAT   console.cs:25469
//       _current_ptt_mode = PTTMode.CW    console.cs:25475
//       _current_ptt_mode = PTTMode.MIC   console.cs:25492
//       _current_ptt_mode = PTTMode.VOX   console.cs:25507
//     Console_KeyDown case Keys.Space:
//       _current_ptt_mode = PTTMode.SPACE  console.cs:26680
//   Thetis Project Files/Source/Console/enums.cs:346-359 [v2.10.3.13]:
//     PTTMode enum values (X2=4, CAT=5, VOX=6, SPACE=7, etc.)
//   Pre-code review §0.3: PTT-source dispatch API contract.
//   Master design §5.2.1: MoxController as the dispatch layer.
//
// Test discipline:
//   - All tests use setTimerIntervals(0,0,0,0,0,0) to make the state-machine
//     walk synchronous so QCoreApplication::processEvents() drives the
//     entire TX→RX/RX→TX transition in-process, matching the pattern
//     established in tst_mox_controller_phase_signals.cpp (H.1/H.2/H.3).
//   - qCWarning for rejected slots is suppressed via setFilterRules per
//     the precedent in tst_mox_controller_anti_vox.cpp §C.2 (H.3).
// =================================================================

// no-port-check: NereusSDR-original test file — no upstream Thetis port.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QLoggingCategory>

#include "core/MoxController.h"
#include "core/PttMode.h"

using namespace NereusSDR;

// Convenience: make the state-machine walk synchronous for all tests.
static void makeSync(MoxController& ctrl)
{
    ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
}

// Convenience: drive all pending timer signals through to completion.
static void drainEvents()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents(); // two passes: some timers fire on first pass
}

class TestMoxControllerPttSourceDispatch : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // § A — onMicPttFromRadio: MIC PTT from radio hardware
    // ════════════════════════════════════════════════════════════════════════

    // §A.1 — press sets PttMode::Mic and engages MOX
    //
    // onMicPttFromRadio(true) must:
    //   1. emit pttModeChanged(PttMode::Mic)
    //   2. transition MOX through to moxStateChanged(true) at end of walk
    void mic_press_setsPttModeAndEngagesMox()
    {
        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onMicPttFromRadio(true);
        drainEvents();

        // PttMode must be Mic
        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).value<PttMode>(), PttMode::Mic);
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);

        // MOX must be engaged
        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isMox());
    }

    // §A.2 — release engages setMox(false); PttMode is NOT cleared
    //
    // onMicPttFromRadio(false) must drive setMox(false), emitting
    // moxStateChanged(false) at end of the TX→RX walk.
    // m_pttMode must remain PttMode::Mic — not cleared by the dispatch slot
    // (F.1 contract: RadioModel hardwareFlipped(false) subscriber clears it).
    void mic_release_drivesMoxOff_pttModeRetained()
    {
        MoxController ctrl;
        makeSync(ctrl);

        ctrl.onMicPttFromRadio(true);
        drainEvents();

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onMicPttFromRadio(false);
        drainEvents();

        // MOX released
        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), false);
        QVERIFY(!ctrl.isMox());

        // PttMode NOT cleared — F.1 contract
        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § B — onCatPtt: CAT PTT command
    // ════════════════════════════════════════════════════════════════════════

    // §B.1 — press sets PttMode::Cat and engages MOX
    void cat_press_setsPttModeAndEngagesMox()
    {
        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onCatPtt(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).value<PttMode>(), PttMode::Cat);
        QCOMPARE(ctrl.pttMode(), PttMode::Cat);

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isMox());
    }

    // §B.2 — release drives MOX off; PttMode retained
    void cat_release_drivesMoxOff_pttModeRetained()
    {
        MoxController ctrl;
        makeSync(ctrl);

        ctrl.onCatPtt(true);
        drainEvents();

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onCatPtt(false);
        drainEvents();

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), false);
        QVERIFY(!ctrl.isMox());

        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(ctrl.pttMode(), PttMode::Cat);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § C — onVoxActive: WDSP VOX/DEXP gate crossing
    // ════════════════════════════════════════════════════════════════════════

    // §C.1 — active sets PttMode::Vox and engages MOX
    void vox_active_setsPttModeAndEngagesMox()
    {
        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onVoxActive(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).value<PttMode>(), PttMode::Vox);
        QCOMPARE(ctrl.pttMode(), PttMode::Vox);

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isMox());
    }

    // §C.2 — inactive drives MOX off; PttMode retained
    void vox_inactive_drivesMoxOff_pttModeRetained()
    {
        MoxController ctrl;
        makeSync(ctrl);

        ctrl.onVoxActive(true);
        drainEvents();

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onVoxActive(false);
        drainEvents();

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), false);
        QVERIFY(!ctrl.isMox());

        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(ctrl.pttMode(), PttMode::Vox);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § D — onSpacePtt: spacebar PTT
    // ════════════════════════════════════════════════════════════════════════

    // §D.1 — press sets PttMode::Space and engages MOX
    void space_press_setsPttModeAndEngagesMox()
    {
        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onSpacePtt(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).value<PttMode>(), PttMode::Space);
        QCOMPARE(ctrl.pttMode(), PttMode::Space);

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isMox());
    }

    // §D.2 — release drives MOX off; PttMode retained
    void space_release_drivesMoxOff_pttModeRetained()
    {
        MoxController ctrl;
        makeSync(ctrl);

        ctrl.onSpacePtt(true);
        drainEvents();

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onSpacePtt(false);
        drainEvents();

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), false);
        QVERIFY(!ctrl.isMox());

        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(ctrl.pttMode(), PttMode::Space);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § E — onX2Ptt: X2 jack external PTT trigger
    // ════════════════════════════════════════════════════════════════════════

    // §E.1 — press sets PttMode::X2 and engages MOX
    void x2_press_setsPttModeAndEngagesMox()
    {
        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onX2Ptt(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).value<PttMode>(), PttMode::X2);
        QCOMPARE(ctrl.pttMode(), PttMode::X2);

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isMox());
    }

    // §E.2 — release drives MOX off; PttMode retained
    void x2_release_drivesMoxOff_pttModeRetained()
    {
        MoxController ctrl;
        makeSync(ctrl);

        ctrl.onX2Ptt(true);
        drainEvents();

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onX2Ptt(false);
        drainEvents();

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), false);
        QVERIFY(!ctrl.isMox());

        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(ctrl.pttMode(), PttMode::X2);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § F — Rejected slots: onCwPtt, onTciPtt
    //
    // All rejection tests suppress qCWarning via setFilterRules per the
    // precedent in tst_mox_controller_anti_vox.cpp §C.2 (H.3 pattern).
    // ════════════════════════════════════════════════════════════════════════

    // §F.1 — CW press is rejected: no pttModeChanged, no moxStateChanged
    void cw_press_rejected_noSignals()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp=false"));

        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onCwPtt(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(moxSpy.count(), 0);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);

        QLoggingCategory::setFilterRules(QString());
    }

    // §F.2 — CW release is rejected: no signals regardless of direction
    void cw_release_rejected_noSignals()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp=false"));

        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onCwPtt(false);
        drainEvents();

        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(moxSpy.count(), 0);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);

        QLoggingCategory::setFilterRules(QString());
    }

    // §F.3 — TCI press is rejected: no pttModeChanged, no moxStateChanged
    void tci_press_rejected_noSignals()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp=false"));

        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onTciPtt(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(moxSpy.count(), 0);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);

        QLoggingCategory::setFilterRules(QString());
    }

    // §F.4 — TCI release is rejected: no signals regardless of direction
    void tci_release_rejected_noSignals()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp=false"));

        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onTciPtt(false);
        drainEvents();

        QCOMPARE(pttSpy.count(), 0);
        QCOMPARE(moxSpy.count(), 0);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);

        QLoggingCategory::setFilterRules(QString());
    }

    // ════════════════════════════════════════════════════════════════════════
    // § G — Cross-source switching (last-setter-wins semantic)
    // ════════════════════════════════════════════════════════════════════════

    // §G.1 — Mic press then Cat press: PttMode transitions Mic → Cat
    //
    // Both sources assert PTT; the second call (Cat) updates PttMode.
    // This tests the last-setter-wins semantic (no refcounting or arbitration
    // in the dispatch layer — the upstream PollPTT equivalent handles that).
    void crossSource_micPressedThenCatPressed_pttModeTransitions()
    {
        MoxController ctrl;
        makeSync(ctrl);

        ctrl.onMicPttFromRadio(true);
        drainEvents();

        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        QVERIFY(ctrl.isMox());

        // Cat press while Mic is active — PttMode transitions to Cat
        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);

        ctrl.onCatPtt(true);
        drainEvents();

        // PttMode should now be Cat
        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).value<PttMode>(), PttMode::Cat);
        QCOMPARE(ctrl.pttMode(), PttMode::Cat);
        // MOX still engaged (was already on; setMox(true) idempotent on state)
        QVERIFY(ctrl.isMox());
    }

    // §G.2 — Mic press then Cat release: PttMode stays Mic, MOX stays engaged
    //
    // Cat releases while Mic is still active.  The Cat release calls setMox(false)
    // which releases MOX.  This verifies the dispatch layer's "last-setter-wins"
    // semantics: Cat release wins and drops MOX even though Mic is still pressed.
    // (In production, the upstream PollPTT/arbiter would NOT call onCatPtt(false)
    // while Mic is still active; this test verifies the raw dispatch semantic.)
    void crossSource_micPressedThenCatReleased_moxDrops()
    {
        MoxController ctrl;
        makeSync(ctrl);

        ctrl.onMicPttFromRadio(true);
        drainEvents();

        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        // Cat releases while Mic is still pressed — last-setter-wins drops MOX
        ctrl.onCatPtt(false);
        drainEvents();

        // Cat release: pressed=false → setPttMode NOT called (no pttModeChanged)
        QCOMPARE(pttSpy.count(), 0);
        // setMox(false) called; MOX should be released
        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), false);
        QVERIFY(!ctrl.isMox());
        // PttMode retains Mic (not cleared by dispatch slot)
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § H — Idempotency
    // ════════════════════════════════════════════════════════════════════════

    // §H.1 — Repeated onMicPttFromRadio(true) is idempotent
    //
    // Second call: setPttMode(Mic) is idempotent (mode unchanged → no emit);
    // setMox(true) runs safety effects (Codex P2) but is idempotent on state
    // advance (m_mox already true → no new walk, no new moxStateChanged).
    // pttModeChanged must fire only once (first call), not twice.
    void mic_idempotent_repeatedPressNoDoubleEmit()
    {
        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onMicPttFromRadio(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(moxSpy.count(), 1);

        // Second press — both idempotent
        ctrl.onMicPttFromRadio(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1); // no new pttModeChanged
        QCOMPARE(moxSpy.count(), 1); // no new moxStateChanged (state advance gated)
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        QVERIFY(ctrl.isMox());
    }

    // §H.2 — Repeated onVoxActive(true) is idempotent
    void vox_idempotent_repeatedActiveNoDoubleEmit()
    {
        MoxController ctrl;
        makeSync(ctrl);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.onVoxActive(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(moxSpy.count(), 1);

        ctrl.onVoxActive(true);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(ctrl.pttMode(), PttMode::Vox);
        QVERIFY(ctrl.isMox());
    }

    // ════════════════════════════════════════════════════════════════════════
    // § I — PttMode integer values (spot-check vs enums.cs:346-359 [v2.10.3.13])
    // ════════════════════════════════════════════════════════════════════════

    // §I.1 — PttMode integer values match Thetis enums.cs:346-359 [v2.10.3.13]
    //
    // From Thetis enums.cs:346-359 [v2.10.3.13]:
    //   MIC=2, X2=4, CAT=5, VOX=6, SPACE=7
    // Spot-checked against PttMode.h values.
    void pttMode_integerValues_matchThetisEnums()
    {
        // From Thetis enums.cs:350 [v2.10.3.13]: MIC = 2
        QCOMPARE(static_cast<int>(PttMode::Mic), 2);
        // From Thetis enums.cs:353 [v2.10.3.13]: X2 = 4
        QCOMPARE(static_cast<int>(PttMode::X2), 4);
        // From Thetis enums.cs:354 [v2.10.3.13]: CAT = 5
        QCOMPARE(static_cast<int>(PttMode::Cat), 5);
        // From Thetis enums.cs:355 [v2.10.3.13]: VOX = 6
        QCOMPARE(static_cast<int>(PttMode::Vox), 6);
        // From Thetis enums.cs:356 [v2.10.3.13]: SPACE = 7
        QCOMPARE(static_cast<int>(PttMode::Space), 7);
    }
};

QTEST_MAIN(TestMoxControllerPttSourceDispatch)
#include "tst_mox_controller_ptt_source_dispatch.moc"
