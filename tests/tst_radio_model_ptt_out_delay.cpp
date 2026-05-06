// =================================================================
// tests/tst_radio_model_ptt_out_delay.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this test
// file. The test exercises:
//   - RadioModel wires TransmitModel::pttOutDelayMsChanged →
//       MoxController::setPttOutDelayMs   — radio-mic-input Task 20
//   - RadioModel wires TransmitModel::allModeMicPTTChanged →
//       MoxController::setAllModeMicPTT   — radio-mic-input Task 20
//
// Both endpoints live on the main thread (RadioModel owns MoxController
// directly; default Qt::AutoConnection resolves to Qt::DirectConnection).
// QCOMPARE is used directly without QTRY_COMPARE because emission is
// synchronous; QTRY is left for resilience against any future thread move.
//
// Source references (for traceability):
//   Thetis Project Files/Source/Console/console.cs:12022
//     [v2.10.3.13+501e3f51] — _all_mode_mic_ptt default false.
//   Thetis Project Files/Source/Console/console.cs:19694
//     [v2.10.3.13+501e3f51] — ptt_out_delay default 20.
// =================================================================

// no-port-check: NereusSDR-original test file — no upstream Thetis port.

#include <QtTest/QtTest>

#include "core/MoxController.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestRadioModelPttOutDelay : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // §1 — pttOutDelayMs round-trip: TransmitModel → MoxController
    // ════════════════════════════════════════════════════════════════════════

    void tmChangeReachesMoxController()
    {
        RadioModel rm;
        QVERIFY(rm.moxController() != nullptr);

        // Default state matches the Thetis default (console.cs:19694
        // [v2.10.3.13+501e3f51] private int ptt_out_delay = 20).
        QCOMPARE(rm.moxController()->pttOutDelayMs(), 20);

        rm.transmitModel().setPttOutDelayMs(123);
        QTRY_COMPARE(rm.moxController()->pttOutDelayMs(), 123);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §2 — allModeMicPTT round-trip: TransmitModel → MoxController
    // ════════════════════════════════════════════════════════════════════════

    void allModeFlagReachesMox()
    {
        RadioModel rm;
        QVERIFY(rm.moxController() != nullptr);

        // Default false (Thetis console.cs:12022 [v2.10.3.13+501e3f51]).
        QCOMPARE(rm.moxController()->allModeMicPTT(), false);

        rm.transmitModel().setAllModeMicPTT(true);
        QTRY_COMPARE(rm.moxController()->allModeMicPTT(), true);

        rm.transmitModel().setAllModeMicPTT(false);
        QTRY_COMPARE(rm.moxController()->allModeMicPTT(), false);
    }

    // ════════════════════════════════════════════════════════════════════════
    // §3 — Clamp surface check: pttOutDelayMs above 500 clamps in MoxController
    //
    // The TransmitModel setter clamps to [0, 500] (Task 7), so the emitted
    // value never exceeds 500.  But MoxController's setPttOutDelayMs (Task
    // 18) also clamps as a defensive belt-and-braces.  This case verifies
    // the wired signal carries the model-clamped value through to MoxController.
    // ════════════════════════════════════════════════════════════════════════

    void clampPropagatesThroughWiring()
    {
        RadioModel rm;

        rm.transmitModel().setPttOutDelayMs(10000);  // way above range
        // Both layers clamp to 500 — the value the MoxController sees
        // matches the model's clamped value.
        QTRY_COMPARE(rm.transmitModel().pttOutDelayMs(), 500);
        QTRY_COMPARE(rm.moxController()->pttOutDelayMs(), 500);
    }
};

QTEST_MAIN(TestRadioModelPttOutDelay)
#include "tst_radio_model_ptt_out_delay.moc"
