// =================================================================
// tests/tst_mox_controller_set_ptt_out_delay_ms.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this test
// file. The test exercises:
//   - MoxController::setPttOutDelayMs(int)   — radio-mic-input Task 18
//   - MoxController::pttOutDelayMs() getter  — radio-mic-input Task 18
//
// Distinct from the test-only setTimerIntervals(...) helper: this slot
// only updates the ptt_out timer interval, leaving rf/mox/space/keyUp/
// breakIn untouched.
//
// Source references (for traceability):
//   Thetis Project Files/Source/Console/console.cs:19694
//     [v2.10.3.13+501e3f51] — private int ptt_out_delay = 20;
//   Thetis Project Files/Source/Console/setup.designer.cs:9029-9226
//     [v2.10.3.13+501e3f51] — udGenPTTOutDelay range 0..500.
// =================================================================

// no-port-check: NereusSDR-original test file — no upstream Thetis port.

#include <QtTest/QtTest>

#include "core/MoxController.h"

using namespace NereusSDR;

class TestMoxControllerSetPttOutDelayMs : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // §1 — Default value matches Thetis ptt_out_delay = 20
    // ════════════════════════════════════════════════════════════════════════

    void defaultIsTwenty()
    {
        // From Thetis console.cs:19694 [v2.10.3.13+501e3f51]:
        //   private int ptt_out_delay = 20;
        MoxController mc;
        QCOMPARE(mc.pttOutDelayMs(), 20);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §2 — In-range value updates the interval
    // ════════════════════════════════════════════════════════════════════════

    void setterUpdatesInterval()
    {
        MoxController mc;
        mc.setPttOutDelayMs(75);
        QCOMPARE(mc.pttOutDelayMs(), 75);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §3 — Below-range clamps to 0
    //
    // udGenPTTOutDelay designer range starts at 0 (setup.designer.cs:9029-9226
    // [v2.10.3.13+501e3f51]). Negative values are clamped, not rejected, so
    // the slot is safe against malformed AppSettings entries.
    // ════════════════════════════════════════════════════════════════════════

    void clampsBelow()
    {
        MoxController mc;
        mc.setPttOutDelayMs(-5);
        QCOMPARE(mc.pttOutDelayMs(), 0);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §4 — Above-range clamps to 500
    // ════════════════════════════════════════════════════════════════════════

    void clampsAbove()
    {
        MoxController mc;
        mc.setPttOutDelayMs(800);
        QCOMPARE(mc.pttOutDelayMs(), 500);
    }
};

QTEST_MAIN(TestMoxControllerSetPttOutDelayMs)
#include "tst_mox_controller_set_ptt_out_delay_ms.moc"
