// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// references below are cite comments documenting which Thetis lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_pa_scaffold.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel PA-calibration safety hotfix scaffolding.
// Phase 3 Agent 3A of issue #167.
//
// Covers:
//   - Per-band normal-mode power (m_powerByBand[14], default 50 W)
//   - 3 ATT-on-TX-on-power-change safety properties
//     (forceAttwhenPSAoff, forceAttwhenPowerChangesWhenPSAon,
//      forceAttwhenPowerChangesWhenPSAonAndDecreased)
//   - m_lastPower sentinel (-1 default, runtime-only)
//   - pureSignalActive() predicate (returns false unconditionally
//     until 3M-4 PureSignal phase wires the live PS feedback DDC)
//
// Source references (cite comments only — no Thetis logic in tests):
//   console.cs:1813-1814 [v2.10.3.13] — power_by_band default 50 W
//   console.cs:29285-29310 [v2.10.3.13] — three ATT-on-TX safety props
//   console.cs:29298 [v2.10.3.13]     — _lastPower reset on toggle
// =================================================================

#include <QtTest/QtTest>
#include "models/TransmitModel.h"
#include "models/Band.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstTransmitModelPaScaffold : public QObject {
    Q_OBJECT

private slots:

    void init() {
        // Clear AppSettings before each test for isolation.
        AppSettings::instance().clear();
    }

    // =========================================================================
    // §1  Per-band normal-mode power array
    // =========================================================================

    // §1.1 — powerForBand() default 50 W on every band on first init.
    void powerForBand_defaultsTo50WForAllBands() {
        TransmitModel t;
        // Per-band normal-mode power covers HF amateur + GEN/WWV/XVTR (14).
        for (int i = 0; i < static_cast<int>(Band::SwlFirst); ++i) {
            QCOMPARE(t.powerForBand(static_cast<Band>(i)), 50);
        }
    }

    // §1.2 — setPowerForBand clamps above 100 W (slider upper bound) to 100.
    void setPowerForBand_clampsAbove100() {
        TransmitModel t;
        t.setPowerForBand(Band::Band20m, 200);
        QCOMPARE(t.powerForBand(Band::Band20m), 100);
    }

    // §1.2 (continued) — setPowerForBand clamps below 0 W to 0.
    void setPowerForBand_clampsBelowZero() {
        TransmitModel t;
        t.setPowerForBand(Band::Band20m, -50);
        QCOMPARE(t.powerForBand(Band::Band20m), 0);
    }

    // §1.3 — setPowerForBand emits powerByBandChanged(band, watts).
    void setPowerForBand_emitsPowerByBandChanged() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::powerByBandChanged);
        t.setPowerForBand(Band::Band40m, 75);
        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).value<Band>(), Band::Band40m);
        QCOMPARE(args.at(1).toInt(), 75);
    }

    // §1.4 — powerForBand round-trips via per-MAC AppSettings.
    void powerForBand_persistsPerMac() {
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");

        {
            TransmitModel t;
            t.loadFromSettings(mac);
            t.setPowerForBand(Band::Band20m, 75);
            t.setPowerForBand(Band::Band40m, 30);
            t.setPowerForBand(Band::Band6m,  10);
            t.persistToSettings(mac);
        }

        // Fresh model — loadFromSettings must restore the saved values.
        TransmitModel t2;
        t2.loadFromSettings(mac);
        QCOMPARE(t2.powerForBand(Band::Band20m), 75);
        QCOMPARE(t2.powerForBand(Band::Band40m), 30);
        QCOMPARE(t2.powerForBand(Band::Band6m),  10);
        // Bands we didn't set should still be 50 W default.
        QCOMPARE(t2.powerForBand(Band::Band80m), 50);
    }

    // =========================================================================
    // §2  ATT-on-TX-on-power-change safety properties — Thetis defaults
    // =========================================================================

    // §5 — defaults match Thetis console.cs:29285-29310 [v2.10.3.13].
    void attOnTx_defaults_matchThetis() {
        TransmitModel t;
        // forceAttwhenPSAoff defaults to TRUE (//MW0LGE [2.9.0.7])
        QVERIFY(t.forceAttwhenPSAoff());
        // forceAttwhenPowerChangesWhenPSAon defaults to TRUE (//MW0LGE [2.9.3.5])
        QVERIFY(t.forceAttwhenPowerChangesWhenPSAon());
        // forceAttwhenPowerChangesWhenPSAonAndDecreased defaults to FALSE
        QVERIFY(!t.forceAttwhenPowerChangesWhenPSAonAndDecreased());
    }

    // §6 — all 3 setters emit *Changed signals.
    void attOnTx_settersEmitChangedSignals() {
        TransmitModel t;

        // 1) forceAttwhenPSAoff (default true → set false → expect signal)
        QSignalSpy spyPsaOff(&t, &TransmitModel::forceAttwhenPSAoffChanged);
        t.setForceAttwhenPSAoff(false);
        QCOMPARE(spyPsaOff.count(), 1);
        QCOMPARE(spyPsaOff.takeFirst().at(0).toBool(), false);

        // 2) forceAttwhenPowerChangesWhenPSAon (default true → set false →
        //    expect signal)
        QSignalSpy spyPsaOn(&t,
            &TransmitModel::forceAttwhenPowerChangesWhenPSAonChanged);
        t.setForceAttwhenPowerChangesWhenPSAon(false);
        QCOMPARE(spyPsaOn.count(), 1);
        QCOMPARE(spyPsaOn.takeFirst().at(0).toBool(), false);

        // 3) forceAttwhenPowerChangesWhenPSAonAndDecreased
        //    (default false → set true → expect signal)
        QSignalSpy spyAnd(&t,
            &TransmitModel::forceAttwhenPowerChangesWhenPSAonAndDecreasedChanged);
        t.setForceAttwhenPowerChangesWhenPSAonAndDecreased(true);
        QCOMPARE(spyAnd.count(), 1);
        QCOMPARE(spyAnd.takeFirst().at(0).toBool(), true);
    }

    // =========================================================================
    // §3  m_lastPower sentinel — CRITICAL Thetis console.cs:29298 semantic
    // =========================================================================

    // §7 — _lastPower resets to -1 when forceAttwhenPowerChangesWhenPSAon
    //      changes value.  CRITICAL TEST.  Thetis console.cs:29298:
    //          if (value != _forceATTwhenPowerChangesWhenPSAon) _lastPower = -1;
    //          _forceATTwhenPowerChangesWhenPSAon = value;
    void lastPower_resetsOnPowerChangesWhenPSAonToggle() {
        TransmitModel t;
        // Default forceAttwhenPowerChangesWhenPSAon is true.
        // Establish a non-sentinel m_lastPower.
        t.setLastPower(50);
        QCOMPARE(t.lastPower(), 50);
        // Toggle the property from true → false.  Per Thetis line 29298,
        // this MUST reset m_lastPower to -1.
        t.setForceAttwhenPowerChangesWhenPSAon(false);
        QCOMPARE(t.lastPower(), -1);
    }

    // §8 — _lastPower does NOT reset on no-op write
    //      (value unchanged → no reset).  Thetis line 29298 guard:
    //          if (value != _forceATTwhenPowerChangesWhenPSAon) ...
    //      The reset only fires when the new value is different.
    void lastPower_doesNotResetOnNoOpWrite() {
        TransmitModel t;
        // Default forceAttwhenPowerChangesWhenPSAon is true.
        t.setLastPower(50);
        QCOMPARE(t.lastPower(), 50);
        // Setting the same value → guard short-circuits; m_lastPower untouched.
        t.setForceAttwhenPowerChangesWhenPSAon(true);
        QCOMPARE(t.lastPower(), 50);
    }

    // §9 — m_lastPower is RUNTIME-only, NOT persisted.
    //      Matches Thetis ephemeral behaviour: float _lastPower = -1
    //      is a private field, never serialised.  Fresh model on app
    //      restart always starts at -1.
    void lastPower_isNotPersisted() {
        const QString mac = QStringLiteral("ff:ee:dd:cc:bb:aa");

        {
            TransmitModel t;
            t.loadFromSettings(mac);
            t.setLastPower(50);
            QCOMPARE(t.lastPower(), 50);
            t.persistToSettings(mac);
        }

        // Fresh model — m_lastPower is runtime-only; loadFromSettings must
        // not restore it.
        TransmitModel t2;
        t2.loadFromSettings(mac);
        QCOMPARE(t2.lastPower(), -1);
    }

    // =========================================================================
    // §4  pureSignalActive() predicate — 3M-4 stub
    // =========================================================================

    // §10 — pureSignalActive() returns false unconditionally for Phase 3A.
    //       3M-4 PureSignal phase wires the live PS-A check.
    void pureSignalActive_returnsFalseUnconditionally() {
        TransmitModel t;
        QVERIFY(!t.pureSignalActive());
        // Toggling the existing pureSig property must NOT change
        // pureSignalActive() (3A scaffolding only — no live PS feedback DDC).
        t.setPureSigEnabled(true);
        QVERIFY(!t.pureSignalActive());
        t.setPureSigEnabled(false);
        QVERIFY(!t.pureSignalActive());
    }

    // =========================================================================
    // §5  Per-MAC ATT-on-TX persistence (extends §1.4 coverage)
    // =========================================================================

    // The 3 ATT-on-TX safety properties round-trip via per-MAC AppSettings
    // under hardware/<mac>/tx/.  Combined with §5 / §6 / §7 / §8 / §9 above.
    void attOnTx_safetyPropertiesPersistPerMac() {
        const QString mac = QStringLiteral("11:22:33:44:55:66");

        {
            TransmitModel t;
            t.loadFromSettings(mac);
            // Flip all three away from defaults so we can detect bad loads.
            t.setForceAttwhenPSAoff(false);
            t.setForceAttwhenPowerChangesWhenPSAon(false);
            t.setForceAttwhenPowerChangesWhenPSAonAndDecreased(true);
            t.persistToSettings(mac);
        }

        TransmitModel t2;
        t2.loadFromSettings(mac);
        QVERIFY(!t2.forceAttwhenPSAoff());
        QVERIFY(!t2.forceAttwhenPowerChangesWhenPSAon());
        QVERIFY(t2.forceAttwhenPowerChangesWhenPSAonAndDecreased());
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelPaScaffold)
#include "tst_transmit_model_pa_scaffold.moc"
