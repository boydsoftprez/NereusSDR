// no-port-check: NereusSDR-original unit-test file.  The Thetis references
// below are cite comments documenting which upstream lines each assertion
// verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_anti_vox.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel anti-VOX properties (2x):
//   antiVoxGainDb / antiVoxSourceVax
//
// Phase 3M-1b C.4.
//
// Source references (cited for traceability; logic ported in TransmitModel.cpp):
//   audio.cs:446-454 [v2.10.3.13]  — Audio.AntiVOXSourceVAC:
//     private static bool antivox_source_VAC = false;
//   setup.designer.cs:44699-44728 [v2.10.3.13]  — udAntiVoxGain:
//     Minimum = -60, Maximum = 60 (decoded from C# decimal int[4] format;
//     DecimalPlaces=1 so the display unit is dB×0.1; NereusSDR stores as int dB).
//   setup.cs:18986-18989 [v2.10.3.13]  — udAntiVoxGain_ValueChanged:
//     cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
//   phase3m-1b-thetis-pre-code-review.md §1.4 (mapping table) + §3.4
//     (CMSetAntiVoxSourceWhat — path-agnostic anti-VOX state machine).
// =================================================================

#include <QtTest/QtTest>
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TstTransmitModelAntiVox : public QObject {
    Q_OBJECT
private slots:

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // DEFAULT VALUES
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void default_antiVoxGainDb_isZero() {
        // NereusSDR-original default 0 dB (plan §C.4 — safe sane default).
        // Thetis udAntiVoxGain designer default is 1.0 dB
        // (setup.designer.cs:44723-44727 [v2.10.3.13]: Value = {10,0,0,0}
        // with DecimalPlaces=1 → 1.0 dB display), but NereusSDR uses 0 dB
        // as a conservative starting point.
        TransmitModel t;
        QCOMPARE(t.antiVoxGainDb(), 0);
    }

    void default_antiVoxSourceVax_isFalse() {
        // Matches Thetis audio.cs:446 [v2.10.3.13]:
        //   private static bool antivox_source_VAC = false;
        // false = local-RX source (not VAX/VAC).
        // NereusSDR renames VAC→VAX for consistency with the rest of the project.
        TransmitModel t;
        QCOMPARE(t.antiVoxSourceVax(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // ROUND-TRIP SETTERS (set → get → matches)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setAntiVoxGainDb_roundTrip() {
        TransmitModel t;
        t.setAntiVoxGainDb(-20);
        QCOMPARE(t.antiVoxGainDb(), -20);
    }

    void setAntiVoxSourceVax_true_roundTrip() {
        TransmitModel t;
        t.setAntiVoxSourceVax(true);
        QCOMPARE(t.antiVoxSourceVax(), true);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SIGNAL EMISSION
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setAntiVoxGainDb_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::antiVoxGainDbChanged);
        t.setAntiVoxGainDb(-30);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), -30);
    }

    void setAntiVoxSourceVax_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::antiVoxSourceVaxChanged);
        t.setAntiVoxSourceVax(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // IDEMPOTENT GUARD (no signal on same-value set)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void idempotent_antiVoxGainDb_default_noSignal() {
        // setAntiVoxGainDb(0) on fresh model (default = 0) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::antiVoxGainDbChanged);
        t.setAntiVoxGainDb(0);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_antiVoxSourceVax_default_noSignal() {
        // setAntiVoxSourceVax(false) on fresh model (default = false) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::antiVoxSourceVaxChanged);
        t.setAntiVoxSourceVax(false);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_antiVoxGainDb_atMin_noSignal() {
        // Set to kAntiVoxGainDbMin, then set again — must NOT emit second time.
        TransmitModel t;
        t.setAntiVoxGainDb(TransmitModel::kAntiVoxGainDbMin);
        QSignalSpy spy(&t, &TransmitModel::antiVoxGainDbChanged);
        t.setAntiVoxGainDb(TransmitModel::kAntiVoxGainDbMin);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_antiVoxGainDb_atMax_noSignal() {
        // Set to kAntiVoxGainDbMax, then set again — must NOT emit second time.
        TransmitModel t;
        t.setAntiVoxGainDb(TransmitModel::kAntiVoxGainDbMax);
        QSignalSpy spy(&t, &TransmitModel::antiVoxGainDbChanged);
        t.setAntiVoxGainDb(TransmitModel::kAntiVoxGainDbMax);
        QCOMPARE(spy.count(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // RANGE CLAMPING
    // (udAntiVoxGain: Minimum=-60, Maximum=60 per
    //  setup.designer.cs:44708-44717 [v2.10.3.13])
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void antiVoxGainDb_clampBelowMin() {
        // udAntiVoxGain.Minimum = -60 (setup.designer.cs:44713-44717 [v2.10.3.13]).
        TransmitModel t;
        t.setAntiVoxGainDb(-200);
        QCOMPARE(t.antiVoxGainDb(), TransmitModel::kAntiVoxGainDbMin);
    }

    void antiVoxGainDb_clampAboveMax() {
        // udAntiVoxGain.Maximum = 60 (setup.designer.cs:44708-44712 [v2.10.3.13]).
        TransmitModel t;
        t.setAntiVoxGainDb(200);
        QCOMPARE(t.antiVoxGainDb(), TransmitModel::kAntiVoxGainDbMax);
    }

    void antiVoxGainDb_clampAtMinBoundary() {
        TransmitModel t;
        t.setAntiVoxGainDb(TransmitModel::kAntiVoxGainDbMin);
        QCOMPARE(t.antiVoxGainDb(), TransmitModel::kAntiVoxGainDbMin);
    }

    void antiVoxGainDb_clampAtMaxBoundary() {
        TransmitModel t;
        t.setAntiVoxGainDb(TransmitModel::kAntiVoxGainDbMax);
        QCOMPARE(t.antiVoxGainDb(), TransmitModel::kAntiVoxGainDbMax);
    }

    void antiVoxGainDb_clampSignalCarriesClampedValue() {
        // Signal must carry clamped value (kAntiVoxGainDbMin = -60), not raw input.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::antiVoxGainDbChanged);
        t.setAntiVoxGainDb(-999);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), TransmitModel::kAntiVoxGainDbMin);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // CONSTANTS SANITY
    // (udAntiVoxGain: Minimum=-60, Maximum=60 per
    //  setup.designer.cs:44708-44717 [v2.10.3.13])
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void constants_antiVoxGainDb_minLessThanMax() {
        QVERIFY(TransmitModel::kAntiVoxGainDbMin < TransmitModel::kAntiVoxGainDbMax);
    }

    void constants_antiVoxGainDb_expectedValues() {
        // Exact values from setup.designer.cs:44708-44717 [v2.10.3.13]:
        //   udAntiVoxGain.Maximum decoded from decimal{60,0,0,0}  = 60
        //   udAntiVoxGain.Minimum decoded from decimal{60,0,0,-2147483648} = -60
        // (DecimalPlaces=1, so display unit is ×0.1 dB; NereusSDR stores as int dB
        //  matching the WDSP SetAntiVOXGain power-of-10 formula in setup.cs:18989.)
        QCOMPARE(TransmitModel::kAntiVoxGainDbMin, -60);
        QCOMPARE(TransmitModel::kAntiVoxGainDbMax,  60);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelAntiVox)
#include "tst_transmit_model_anti_vox.moc"
