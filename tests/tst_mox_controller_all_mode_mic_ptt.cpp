// =================================================================
// tests/tst_mox_controller_all_mode_mic_ptt.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this test
// file. The test exercises:
//   - MoxController::setAllModeMicPTT(bool)   — radio-mic-input Task 17
//   - MoxController::allModeMicPTT() getter   — radio-mic-input Task 17
//
// Task 19 (mode-gate inside onMicPttFromRadio) extends this file with
// gate-behaviour cases.
//
// Source references (for traceability):
//   Thetis Project Files/Source/Console/console.cs:12022
//     [v2.10.3.13+501e3f51] — private bool _all_mode_mic_ptt = false;
//   Thetis Project Files/Source/Console/console.cs:25480-25495
//     [v2.10.3.13+501e3f51] — PollPTT mic_ptt mode-gate (Task 19 target).
// =================================================================

// no-port-check: NereusSDR-original test file — no upstream Thetis port.

#include <QtTest/QtTest>

#include "core/MoxController.h"

using namespace NereusSDR;

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
};

QTEST_MAIN(TestMoxControllerAllModeMicPtt)
#include "tst_mox_controller_all_mode_mic_ptt.moc"
