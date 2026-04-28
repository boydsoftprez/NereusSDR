// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// and "setup.designer.cs" references below are cite comments documenting
// which Thetis lines each assertion verifies; no Thetis logic is ported
// in this test file.
// =================================================================
// tests/tst_transmit_model_mic_jack_flags.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel mic-jack flag properties (8x).
//
// Phase 3M-1b C.2.
//
// Source references (cited for traceability; logic ported in TransmitModel.cpp):
//   console.cs:13213-13260 [v2.10.3.13] — LineIn / LineInBoost / MicBoost / MicXlr
//     property block with defaults.
//   console.cs:28752 [v2.10.3.13] — MicMute: "NOTE: although called MicMute,
//     true = mic in use"
//   console.designer.cs:2029-2030 [v2.10.3.13] — chkMicMute.Checked = true default.
//   console.cs:19757-19766 [v2.10.3.13] — MicPTTDisabled:
//     private bool mic_ptt_disabled = false; ... NetworkIO.SetMicPTT(Convert.ToInt32(value));
//   setup.designer.cs:8683 [v2.10.3.13] — radOrionMicTip.Checked = true (Tip default).
//   setup.designer.cs:8779 [v2.10.3.13] — radOrionBiasOff.Checked = true (bias-off default).
//   setup.designer.cs:46898-46919 [v2.10.3.13] — udLineInBoost range: Minimum=-34.5,
//     Maximum=12.0, Value=0.0 (decoded from C# decimal int[4] format).
// =================================================================

#include <QtTest/QtTest>
#include <cmath>
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TstTransmitModelMicJackFlags : public QObject {
    Q_OBJECT
private slots:

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // DEFAULT VALUES
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void default_micMute_isTrue() {
        // Counter-intuitive: true = mic in use (not muted).
        // Verified by console.cs:28752 [v2.10.3.13]: "NOTE: although called
        // MicMute, true = mic in use" and console.designer.cs:2029-2030:
        // "Checked = true; CheckState = Checked".
        TransmitModel t;
        QCOMPARE(t.micMute(), true);
    }

    void default_micBoost_isTrue() {
        // From Thetis console.cs:13237 [v2.10.3.13]: private bool mic_boost = true;
        TransmitModel t;
        QCOMPARE(t.micBoost(), true);
    }

    void default_micXlr_isTrue() {
        // From Thetis console.cs:13249 [v2.10.3.13]: private bool mic_xlr = true;
        TransmitModel t;
        QCOMPARE(t.micXlr(), true);
    }

    void default_lineIn_isFalse() {
        // From Thetis console.cs:13213 [v2.10.3.13]: private bool line_in = false;
        TransmitModel t;
        QCOMPARE(t.lineIn(), false);
    }

    void default_lineInBoost_isZero() {
        // From Thetis console.cs:13225 [v2.10.3.13]: private double line_in_boost = 0.0;
        // Also setup.designer.cs:46914-46918: udLineInBoost.Value = 0.
        TransmitModel t;
        QVERIFY(std::abs(t.lineInBoost() - 0.0) < 1e-10);
    }

    void default_micTipRing_isTrue() {
        // NereusSDR-original: TRUE = Tip is mic (intuitive).
        // Thetis radOrionMicTip.Checked = true (setup.designer.cs:8683 [v2.10.3.13]).
        // Wire-bit polarity inversion happens at RadioConnection::setMicTipRing (Phase G).
        TransmitModel t;
        QCOMPARE(t.micTipRing(), true);
    }

    void default_micBias_isFalse() {
        // NereusSDR-original: FALSE = bias off by default.
        // Thetis radOrionBiasOff.Checked = true (setup.designer.cs:8779 [v2.10.3.13]).
        TransmitModel t;
        QCOMPARE(t.micBias(), false);
    }

    void default_micPttDisabled_isFalse() {
        // From Thetis console.cs:19757 [v2.10.3.13]:
        //   private bool mic_ptt_disabled = false;
        // PTT enabled by default (sensible safety default).
        TransmitModel t;
        QCOMPARE(t.micPttDisabled(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // ROUND-TRIP SETTERS (set → get → matches)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setMicMute_false_roundTrip() {
        TransmitModel t;
        t.setMicMute(false);
        QCOMPARE(t.micMute(), false);
    }

    void setMicBoost_false_roundTrip() {
        TransmitModel t;
        t.setMicBoost(false);
        QCOMPARE(t.micBoost(), false);
    }

    void setMicXlr_false_roundTrip() {
        TransmitModel t;
        t.setMicXlr(false);
        QCOMPARE(t.micXlr(), false);
    }

    void setLineIn_true_roundTrip() {
        TransmitModel t;
        t.setLineIn(true);
        QCOMPARE(t.lineIn(), true);
    }

    void setLineInBoost_positiveValue_roundTrip() {
        TransmitModel t;
        t.setLineInBoost(6.0);
        QVERIFY(std::abs(t.lineInBoost() - 6.0) < 1e-10);
    }

    void setMicTipRing_false_roundTrip() {
        TransmitModel t;
        t.setMicTipRing(false);
        QCOMPARE(t.micTipRing(), false);
    }

    void setMicBias_true_roundTrip() {
        TransmitModel t;
        t.setMicBias(true);
        QCOMPARE(t.micBias(), true);
    }

    void setMicPttDisabled_true_roundTrip() {
        TransmitModel t;
        t.setMicPttDisabled(true);
        QCOMPARE(t.micPttDisabled(), true);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SIGNAL EMISSION
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setMicMute_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micMuteChanged);
        t.setMicMute(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    void setMicBoost_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micBoostChanged);
        t.setMicBoost(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    void setMicXlr_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micXlrChanged);
        t.setMicXlr(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    void setLineIn_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInChanged);
        t.setLineIn(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);
    }

    void setLineInBoost_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInBoostChanged);
        t.setLineInBoost(6.0);
        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.first().at(0).toDouble() - 6.0) < 1e-10);
    }

    void setMicTipRing_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micTipRingChanged);
        t.setMicTipRing(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    void setMicBias_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micBiasChanged);
        t.setMicBias(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);
    }

    void setMicPttDisabled_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micPttDisabledChanged);
        t.setMicPttDisabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // IDEMPOTENT GUARD (no signal on same-value set)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void idempotent_micMute_default_noSignal() {
        // setMicMute(true) on a fresh model (default = true) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micMuteChanged);
        t.setMicMute(true);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_micBoost_default_noSignal() {
        // setMicBoost(true) on fresh model (default = true) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micBoostChanged);
        t.setMicBoost(true);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_lineIn_explicitSameValue_noSignal() {
        // Set lineIn = true, then set again — second call must not emit.
        TransmitModel t;
        t.setLineIn(true);
        QSignalSpy spy(&t, &TransmitModel::lineInChanged);
        t.setLineIn(true);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_lineInBoost_sameValue_noSignal() {
        // setLineInBoost(0.0) on fresh model (default = 0.0) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInBoostChanged);
        t.setLineInBoost(0.0);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_micPttDisabled_default_noSignal() {
        // setMicPttDisabled(false) on fresh model (default = false) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::micPttDisabledChanged);
        t.setMicPttDisabled(false);
        QCOMPARE(spy.count(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // LINEINBOOST RANGE CLAMPING
    // (From Thetis setup.designer.cs:46898-46907 [v2.10.3.13]:
    //  udLineInBoost.Minimum = -34.5, udLineInBoost.Maximum = 12.0)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void lineInBoost_clampBelowMin() {
        TransmitModel t;
        t.setLineInBoost(-100.0);
        QCOMPARE(t.lineInBoost(), TransmitModel::kLineInBoostMin);
    }

    void lineInBoost_clampAboveMax() {
        TransmitModel t;
        t.setLineInBoost(100.0);
        QCOMPARE(t.lineInBoost(), TransmitModel::kLineInBoostMax);
    }

    void lineInBoost_clampAtMinBoundary() {
        TransmitModel t;
        t.setLineInBoost(TransmitModel::kLineInBoostMin);
        QCOMPARE(t.lineInBoost(), TransmitModel::kLineInBoostMin);
    }

    void lineInBoost_clampAtMaxBoundary() {
        TransmitModel t;
        t.setLineInBoost(TransmitModel::kLineInBoostMax);
        QCOMPARE(t.lineInBoost(), TransmitModel::kLineInBoostMax);
    }

    void lineInBoost_clampSignalCarriesClampedValue() {
        // Signal must carry clamped value, not the raw input.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInBoostChanged);
        t.setLineInBoost(-200.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), TransmitModel::kLineInBoostMin);
    }

    void lineInBoost_constants_haveExpectedValues() {
        // kLineInBoostMin = -34.5, kLineInBoostMax = 12.0 per Thetis
        // setup.designer.cs:46898-46907 [v2.10.3.13].
        QCOMPARE(TransmitModel::kLineInBoostMin, -34.5);
        QCOMPARE(TransmitModel::kLineInBoostMax,  12.0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // COUNTER-INTUITIVE NAMING INVARIANT (micMute semantics)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void micMute_defaultTrueIsMicInUse() {
        // Explicit assertion that the preserved counter-intuitive polarity is
        // in effect: the default value of TRUE means the mic IS in use (not muted).
        // The Thetis comment at console.cs:28752 [v2.10.3.13] reads:
        //   "NOTE: although called MicMute, true = mic in use"
        TransmitModel t;
        const bool micIsInUse = t.micMute();  // true = mic in use (Thetis naming)
        QVERIFY(micIsInUse);                  // mic is usable by default
    }

    void micMute_falseIsMuted() {
        // Explicit: setMicMute(false) means the mic IS muted.
        TransmitModel t;
        t.setMicMute(false);
        const bool micIsInUse = t.micMute();
        QVERIFY(!micIsInUse);  // false = mic muted (Thetis naming: checked=false → muted)
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelMicJackFlags)
#include "tst_transmit_model_mic_jack_flags.moc"
