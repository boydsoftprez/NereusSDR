// =================================================================
// tests/tst_mox_controller_anti_vox.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this test
// file. The test exercises:
//   - MoxController::setVoxHangTime(int ms)              — H.3 Phase 3M-1b
//   - MoxController::setAntiVoxGain(int dB)              — H.3 Phase 3M-1b
//   - MoxController::setAntiVoxSourceVax(bool useVax)    — H.3 Phase 3M-1b
//   - MoxController::voxHangTimeRequested(double seconds) — H.3 signal
//   - MoxController::antiVoxGainRequested(double gain)    — H.3 signal
//   - MoxController::antiVoxSourceWhatRequested(bool)     — H.3 signal
//
// Source references (for traceability):
//   Thetis Project Files/Source/Console/setup.cs:18896-18900 [v2.10.3.13]
//     — udDEXPHold_ValueChanged: ms→seconds before SetDEXPHoldTime.
//   Thetis Project Files/Source/Console/setup.cs:18986-18990 [v2.10.3.13]
//     — udAntiVoxGain_ValueChanged: dB→linear (/20.0) before SetAntiVOXGain.
//   Thetis Project Files/Source/Console/cmaster.cs:912-943 [v2.10.3.13]
//     — CMSetAntiVoxSourceWhat: useVAC=false → all RX slots get source=1.
//   Plan §3 H.3 + pre-code review §3.4: useVax=true deferred to 3M-3a.
//
// Default member values (from Thetis source + TransmitModel):
//   m_voxHangTimeMs      = 500   (udDEXPHold designer default)
//   m_antiVoxGainDb      = 0     (TransmitModel default)
//   m_antiVoxSourceVax   = false (audio.cs:447 [v2.10.3.13]: antivox_source_VAC = false)
//   m_micBoost           = true  (console.cs:13237 [v2.10.3.13]: mic_boost = true)
//
// Formulae verified against Thetis:
//   Hang time seconds = ms / 1000.0
//   Anti-VOX gain linear = pow(10.0, dB / 20.0)   ← /20.0 voltage scaling
// =================================================================

// no-port-check: NereusSDR-original test file — no upstream Thetis port.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QLoggingCategory>

#include <cmath>

#include "core/MoxController.h"

using namespace NereusSDR;

class TestMoxControllerAntiVox : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // § A — setVoxHangTime tests
    // ════════════════════════════════════════════════════════════════════════

    // §A.1 — NaN sentinel: first call always emits (default value, spy first)
    //
    // A fresh MoxController has m_lastVoxHangTimeEmitted = NaN.  Calling
    // setVoxHangTime(500) (= default) must still emit because the NaN sentinel
    // forces the very first call through, priming WDSP at startup.
    void hangTime_nanSentinel_firstCallAlwaysEmits()
    {
        MoxController ctrl;
        // Attach spy before any setter call — fresh NaN state.
        QSignalSpy spy(&ctrl, &MoxController::voxHangTimeRequested);

        ctrl.setVoxHangTime(500);  // default value, NaN sentinel forces emit

        QCOMPARE(spy.count(), 1);
        // 500 ms → 0.5 seconds
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 0.5));
    }

    // §A.2 — Change emits the converted seconds value
    //
    // setVoxHangTime(1000) must emit exactly 1.0 seconds.
    // Prime NaN sentinel first so the spy only catches the new-value emit.
    void hangTime_change_emitsNewValue()
    {
        MoxController ctrl;
        ctrl.setVoxHangTime(500);  // prime NaN sentinel

        QSignalSpy spy(&ctrl, &MoxController::voxHangTimeRequested);
        ctrl.setVoxHangTime(1000);  // 1000 ms → 1.0 seconds

        QCOMPARE(spy.count(), 1);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 1.0));
    }

    // §A.3 — Idempotent: repeat same value does not re-emit
    //
    // After emitting 0.5 s, a second call with 500 ms must be suppressed.
    void hangTime_idempotent_noDoubleEmit()
    {
        MoxController ctrl;
        ctrl.setVoxHangTime(500);  // prime NaN sentinel; emits 0.5

        QSignalSpy spy(&ctrl, &MoxController::voxHangTimeRequested);
        ctrl.setVoxHangTime(500);  // same → no emit

        QCOMPARE(spy.count(), 0);
    }

    // §A.4 — Boundary: 1 ms → 0.001 seconds
    //
    // From Thetis setup.cs:18899 [v2.10.3.13]: Value / 1000.0
    // 1 ms / 1000 = 0.001 — small but non-zero, non-NaN.
    void hangTime_boundary_1ms()
    {
        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::voxHangTimeRequested);

        ctrl.setVoxHangTime(1);  // 1 ms → 0.001 s, NaN sentinel forces emit

        QCOMPARE(spy.count(), 1);
        const double val = spy.at(0).at(0).toDouble();
        QVERIFY(!std::isnan(val));
        QVERIFY(!std::isinf(val));
        QVERIFY(val > 0.0);
        QVERIFY(qFuzzyCompare(val, 0.001));
    }

    // §A.5 — Boundary: 10000 ms → 10.0 seconds
    void hangTime_boundary_10000ms()
    {
        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::voxHangTimeRequested);

        ctrl.setVoxHangTime(10000);  // 10000 ms → 10.0 s

        QCOMPARE(spy.count(), 1);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 10.0));
    }

    // ════════════════════════════════════════════════════════════════════════
    // § B — setAntiVoxGain tests
    // ════════════════════════════════════════════════════════════════════════

    // §B.1 — Default state: setAntiVoxGain(0) emits linear = 1.0
    //
    // From Thetis setup.cs:18989 [v2.10.3.13]:
    //   Math.Pow(10.0, 0.0 / 20.0) = pow(10, 0) = 1.0
    // NaN sentinel forces first-call emit even at default value.
    void antiVoxGain_defaultZeroDb_emitsUnity()
    {
        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::antiVoxGainRequested);

        ctrl.setAntiVoxGain(0);  // 0 dB → 1.0 linear; NaN sentinel forces emit

        QCOMPARE(spy.count(), 1);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 1.0));
    }

    // §B.2 — Change emits the converted linear value
    //
    // From Thetis setup.cs:18989 [v2.10.3.13]:
    //   Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0)
    // -20 dB: pow(10, -1) = 0.1
    void antiVoxGain_minus20dB_emitsOneTenth()
    {
        MoxController ctrl;
        ctrl.setAntiVoxGain(0);  // prime NaN sentinel

        QSignalSpy spy(&ctrl, &MoxController::antiVoxGainRequested);
        ctrl.setAntiVoxGain(-20);  // -20 dB → pow(10, -1) = 0.1

        QCOMPARE(spy.count(), 1);
        QVERIFY(qFuzzyCompare(spy.at(0).at(0).toDouble(), 0.1));
    }

    // §B.3 — Idempotent: repeat same dB does not re-emit
    void antiVoxGain_idempotent_noDoubleEmit()
    {
        MoxController ctrl;
        ctrl.setAntiVoxGain(0);  // prime NaN sentinel; emits 1.0

        QSignalSpy spy(&ctrl, &MoxController::antiVoxGainRequested);
        ctrl.setAntiVoxGain(0);  // same → no emit

        QCOMPARE(spy.count(), 0);
    }

    // §B.4 — Boundary: -60 dB → 0.001
    //
    // From Thetis setup.cs [v2.10.3.13]:
    //   udAntiVoxGain.Minimum = -60 (from TransmitModel kAntiVoxGainDbMin)
    // pow(10, -60/20) = pow(10, -3) = 0.001
    void antiVoxGain_boundary_minus60dB()
    {
        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::antiVoxGainRequested);

        ctrl.setAntiVoxGain(-60);

        QCOMPARE(spy.count(), 1);
        const double val = spy.at(0).at(0).toDouble();
        QVERIFY(!std::isnan(val));
        QVERIFY(!std::isinf(val));
        QVERIFY(val > 0.0);
        QVERIFY(qFuzzyCompare(val, 0.001));
    }

    // §B.5 — Boundary: 0 dB → 1.0 (unity; non-NaN, non-Inf, positive)
    void antiVoxGain_boundary_0dB_unity()
    {
        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::antiVoxGainRequested);

        ctrl.setAntiVoxGain(0);

        QCOMPARE(spy.count(), 1);
        const double val = spy.at(0).at(0).toDouble();
        QVERIFY(!std::isnan(val));
        QVERIFY(!std::isinf(val));
        QVERIFY(qFuzzyCompare(val, 1.0));
    }

    // §B.6 — NaN sentinel: first call emits even at default
    //
    // Spy attached before any setAntiVoxGain call.
    void antiVoxGain_nanSentinel_firstCallAlwaysEmits()
    {
        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::antiVoxGainRequested);

        ctrl.setAntiVoxGain(0);  // default; NaN sentinel forces emit

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toDouble() > 0.0);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § C — setAntiVoxSourceVax tests
    // ════════════════════════════════════════════════════════════════════════

    // §C.1 — Default useVax=false emits antiVoxSourceWhatRequested(false)
    //
    // Matches Thetis cmaster.cs:937-942 [v2.10.3.13] else branch
    // (use audio going to hardware minus MON): all RX slots get source=1.
    // First call must emit even though false matches the default field value
    // because m_antiVoxSourceVaxInitialized starts as false.
    void antiVoxSource_false_emitsSignal()
    {
        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::antiVoxSourceWhatRequested);

        ctrl.setAntiVoxSourceVax(false);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    // §C.2 — useVax=true is rejected: no signal emitted
    //
    // Deferred to 3M-3a per plan §3 H.3. The setAntiVoxSourceVax(true) call
    // must log qCWarning and return WITHOUT updating m_antiVoxSourceVax or
    // emitting antiVoxSourceWhatRequested(true).  Verified by spy count == 0.
    void antiVoxSource_trueRejected_noSignalEmitted()
    {
        // Suppress the qCWarning so test output is clean.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp=false"));

        MoxController ctrl;
        QSignalSpy spy(&ctrl, &MoxController::antiVoxSourceWhatRequested);

        ctrl.setAntiVoxSourceVax(true);

        QCOMPARE(spy.count(), 0);

        // Restore logging so other tests are unaffected.
        QLoggingCategory::setFilterRules(QString());
    }

    // §C.3 — useVax=true rejected does NOT update the state field
    //
    // After a rejected true call, a subsequent false call must still emit
    // (because m_antiVoxSourceVax remained false and !m_antiVoxSourceVaxInitialized
    // — the first valid call to setAntiVoxSourceVax(false) primes the state).
    void antiVoxSource_trueRejectedDoesNotUpdateState()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp=false"));

        MoxController ctrl;
        ctrl.setAntiVoxSourceVax(true);  // rejected; state stays uninitialized

        QSignalSpy spy(&ctrl, &MoxController::antiVoxSourceWhatRequested);
        ctrl.setAntiVoxSourceVax(false);  // first valid call; must emit

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);

        QLoggingCategory::setFilterRules(QString());
    }

    // §C.4 — Idempotent: second false call after initialized does not re-emit
    //
    // Once m_antiVoxSourceVaxInitialized=true and m_antiVoxSourceVax=false,
    // a second setAntiVoxSourceVax(false) must be suppressed.
    void antiVoxSource_idempotent_noDoubleEmit()
    {
        MoxController ctrl;
        ctrl.setAntiVoxSourceVax(false);  // prime; emits once

        QSignalSpy spy(&ctrl, &MoxController::antiVoxSourceWhatRequested);
        ctrl.setAntiVoxSourceVax(false);  // same value → no emit

        QCOMPARE(spy.count(), 0);
    }

    // §C.5 — Toggle false → true → false
    //
    // First false emits.  True is rejected (spy unchanged).
    // Second false: state is still false AND initialized → idempotent → no emit.
    void antiVoxSource_toggleFalseTrueFalse()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp=false"));

        MoxController ctrl;
        ctrl.setAntiVoxSourceVax(false);  // emits; initializes

        QSignalSpy spy(&ctrl, &MoxController::antiVoxSourceWhatRequested);
        ctrl.setAntiVoxSourceVax(true);   // rejected; no emit; state still false
        ctrl.setAntiVoxSourceVax(false);  // state was false + initialized → idempotent

        // true was rejected, second false was idempotent → spy must be empty.
        QCOMPARE(spy.count(), 0);

        QLoggingCategory::setFilterRules(QString());
    }
};

QTEST_MAIN(TestMoxControllerAntiVox)
#include "tst_mox_controller_anti_vox.moc"
