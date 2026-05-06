// =================================================================
// tests/tst_mox_controller_all_mode_mic_ptt.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this test
// file. The test exercises:
//   - MoxController::setAllModeMicPTT(bool)   — radio-mic-input Task 17
//   - MoxController::allModeMicPTT() getter   — radio-mic-input Task 17
//   - MoxController::onMicPttFromRadio mode-gate — radio-mic-input Task 19
//
// The Task 19 cases verify the AllModeMicPTT mode-gate inside
// onMicPttFromRadio: voice modes always fire, non-voice modes fire only
// when the AllModeMicPTT flag is set.
//
// Source references (for traceability):
//   Thetis Project Files/Source/Console/console.cs:12022
//     [v2.10.3.13+501e3f51] — private bool _all_mode_mic_ptt = false;
//   Thetis Project Files/Source/Console/console.cs:25480-25495
//     [v2.10.3.13+501e3f51] — PollPTT mic_ptt mode-gate.
//
// Voice family (8 modes from cmaster.cs:1043-1050 [v2.10.3.13]):
//   LSB, USB, DSB, AM, SAM, FM, DIGL, DIGU.
// Non-voice (gated unless AllModeMicPTT=true):
//   CWL, CWU, SPEC, DRM.
// =================================================================

// no-port-check: NereusSDR-original test file — no upstream Thetis port.

#include <QtTest/QtTest>

#include "core/MoxController.h"
#include "core/PttMode.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

// Convenience: make the state-machine walk synchronous for all tests.
// Mirrors the pattern in tst_mox_controller_ptt_source_dispatch.cpp.
static void makeSync(MoxController& ctrl)
{
    ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
}

// Convenience: drive pending timer signals through to completion.
static void drainEvents()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

class TestMoxControllerAllModeMicPtt : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // §1 — Task 17: storage + getter contract
    // ════════════════════════════════════════════════════════════════════════

    void defaultIsFalse()
    {
        // From Thetis console.cs:12022 [v2.10.3.13+501e3f51]:
        //   private bool _all_mode_mic_ptt = false;
        MoxController mc;
        QCOMPARE(mc.allModeMicPTT(), false);
    }

    void setterUpdatesInternalFlag()
    {
        MoxController mc;
        mc.setAllModeMicPTT(true);
        QCOMPARE(mc.allModeMicPTT(), true);
        mc.setAllModeMicPTT(false);
        QCOMPARE(mc.allModeMicPTT(), false);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §2 — Task 19: mode-gate inside onMicPttFromRadio
    //
    // From Thetis console.cs:25480-25495 [v2.10.3.13+501e3f51]:
    //   if ((tx_mode == DSPMode.LSB || ... || tx_mode == DSPMode.FM ||
    //        _all_mode_mic_ptt) &&
    //       mic_ptt &&
    //       _current_ptt_mode != PTTMode.CW)
    //   {
    //       _current_ptt_mode = PTTMode.MIC;
    //       chkMOX.Checked = true;
    //   }
    // ════════════════════════════════════════════════════════════════════════

    // §2.1 — Voice mode (USB) fires unconditionally regardless of flag.
    //
    // The voice-family disjunction (LSB || USB || ...) makes the mic-PTT
    // dispatch fire even when AllModeMicPTT is false.
    void voiceModeFiresUnconditionally_USB()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onModeChanged(DSPMode::USB);
        // AllModeMicPTT remains at its default false — must not matter for USB.
        QCOMPARE(ctrl.allModeMicPTT(), false);

        ctrl.onMicPttFromRadio(true);
        drainEvents();

        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        QVERIFY(ctrl.isMox());
    }

    // §2.2 — CW mode without the flag: mic_ptt must be ignored.
    //
    // From Thetis console.cs:25480-25495 [v2.10.3.13+501e3f51]: the disjunction
    // (voice || _all_mode_mic_ptt) must evaluate to false in CW unless the
    // flag is set, so neither PttMode nor MOX should change.
    //
    // CW dispatch in non-voice modes (mic_ptt → PttMode::CW per
    // console.cs:25473-25478 [v2.10.3.13+501e3f51]) is deferred to 3M-2; this
    // test asserts the gate intentionally suppresses the call in the meantime.
    void cwModeWithFlagOff_ignoresMicPtt()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onModeChanged(DSPMode::CWU);
        QCOMPARE(ctrl.allModeMicPTT(), false);

        const PttMode pttBefore = ctrl.pttMode();
        const bool    moxBefore = ctrl.isMox();

        ctrl.onMicPttFromRadio(true);
        drainEvents();

        // No state change: gate suppressed the call.
        QCOMPARE(ctrl.pttMode(), pttBefore);
        QCOMPARE(ctrl.isMox(),   moxBefore);
    }

    // §2.3 — CW mode with the flag set: mic_ptt fires as a MIC PTT.
    //
    // The (_all_mode_mic_ptt) disjunct in console.cs:25488 [v2.10.3.13+501e3f51]
    // makes the dispatch fire in CW once the flag is set.
    void cwModeWithFlagOn_fires()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onModeChanged(DSPMode::CWU);
        ctrl.setAllModeMicPTT(true);

        ctrl.onMicPttFromRadio(true);
        drainEvents();

        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        QVERIFY(ctrl.isMox());
    }
};

QTEST_MAIN(TestMoxControllerAllModeMicPtt)
#include "tst_mox_controller_all_mode_mic_ptt.moc"
