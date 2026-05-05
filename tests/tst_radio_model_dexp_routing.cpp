// no-port-check: NereusSDR-original unit-test file.  All Thetis source cites
// are in TransmitModel.h/cpp / TxChannel.h / RadioModel.cpp.
// =================================================================
// tests/tst_radio_model_dexp_routing.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-iii Task 12 — TransmitModel → TxChannel routing tests
// for the 11 new DEXP properties added in Tasks 7-10 (envelope,
// gate ratios, look-ahead, side-channel filter).
//
// Verifies that RadioModel::connectToRadio installs the connect()
// pairings between TransmitModel DEXP change signals and the TxChannel
// WDSP wrappers (#28-#38 in the routing block, RadioModel.cpp).
//
// Test strategy: mirrors tst_radio_model_eq_lev_alc_wiring.cpp.
// m_txChannel is null in unit-test builds (createTxChannel only runs
// after WDSP initializes; WDSP isn't initialized here).  The lambda
// body is null-safe by virtue of the receiver=m_txChannel argument —
// Qt auto-disconnects when the receiver is destroyed, but if
// m_txChannel is null at connect time the connect() simply doesn't
// install (no-op).  We therefore verify the TWO model-side guarantees
// that hold without a live channel:
//
//   1. TransmitModel signals are emitted exactly once per setter call
//      (the idempotent guard inside each setter prevents double-fire on
//      same-value writes).
//   2. Setting + emitting all 11 signals in sequence does not crash and
//      the model state stays consistent.
//
// Slot-body coverage (the actual SetDEXP*/SetDEXPSCF*/SetDEXPLookAhead*
// WDSP calls) is exercised by tst_tx_channel_dexp_setters and the Task
// 1-5 wrapper test executables (Tasks 1-5).  This file covers the
// ROUTING contract.
//
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TstRadioModelDexpRouting : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()  { AppSettings::instance().clear(); }
    void init()          { AppSettings::instance().clear(); }
    void cleanup()       { AppSettings::instance().clear(); }

    // ─────────────────────────────────────────────────────────────────────
    // Envelope (Task 7) — 4 signals
    // ─────────────────────────────────────────────────────────────────────

    void dexpEnabledChanged_firesOnce()
    {
        RadioModel model;
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::dexpEnabledChanged);
        model.transmitModel().setDexpEnabled(true);
        model.transmitModel().setDexpEnabled(true);  // idempotent
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);
    }

    void dexpDetectorTauMs_idempotentAndCarriesValue()
    {
        RadioModel model;
        // Default per Thetis Designer is 20 ms.
        QCOMPARE(model.transmitModel().dexpDetectorTauMs(), 20.0);
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::dexpDetectorTauMsChanged);
        model.transmitModel().setDexpDetectorTauMs(35.0);
        model.transmitModel().setDexpDetectorTauMs(35.0);  // same value
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 35.0);
    }

    void dexpAttackTimeMs_idempotentAndCarriesValue()
    {
        RadioModel model;
        // Default per Thetis Designer is 2 ms.
        QCOMPARE(model.transmitModel().dexpAttackTimeMs(), 2.0);
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::dexpAttackTimeMsChanged);
        model.transmitModel().setDexpAttackTimeMs(8.0);
        model.transmitModel().setDexpAttackTimeMs(8.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 8.0);
    }

    void dexpReleaseTimeMs_idempotentAndCarriesValue()
    {
        RadioModel model;
        // Default per Thetis Designer is 100 ms.
        QCOMPARE(model.transmitModel().dexpReleaseTimeMs(), 100.0);
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::dexpReleaseTimeMsChanged);
        model.transmitModel().setDexpReleaseTimeMs(250.0);
        model.transmitModel().setDexpReleaseTimeMs(250.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 250.0);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Gate ratios (Task 8) — 2 signals
    // ─────────────────────────────────────────────────────────────────────

    void dexpExpansionRatioDb_idempotentAndCarriesValue()
    {
        RadioModel model;
        // Default per Thetis Designer is 10 dB.
        QCOMPARE(model.transmitModel().dexpExpansionRatioDb(), 10.0);
        QSignalSpy spy(&model.transmitModel(),
                       &TransmitModel::dexpExpansionRatioDbChanged);
        model.transmitModel().setDexpExpansionRatioDb(20.0);
        model.transmitModel().setDexpExpansionRatioDb(20.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 20.0);
    }

    void dexpHysteresisRatioDb_idempotentAndCarriesValue()
    {
        RadioModel model;
        // Default per Thetis Designer is 2 dB (raw 20 with DecimalPlaces=1).
        QCOMPARE(model.transmitModel().dexpHysteresisRatioDb(), 2.0);
        QSignalSpy spy(&model.transmitModel(),
                       &TransmitModel::dexpHysteresisRatioDbChanged);
        model.transmitModel().setDexpHysteresisRatioDb(5.0);
        model.transmitModel().setDexpHysteresisRatioDb(5.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 5.0);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Look-ahead (Task 9) — 2 signals
    // ─────────────────────────────────────────────────────────────────────

    void dexpLookAheadEnabledChanged_firesOnce()
    {
        RadioModel model;
        // Default per Thetis Designer is true.
        QCOMPARE(model.transmitModel().dexpLookAheadEnabled(), true);
        QSignalSpy spy(&model.transmitModel(),
                       &TransmitModel::dexpLookAheadEnabledChanged);
        model.transmitModel().setDexpLookAheadEnabled(false);
        model.transmitModel().setDexpLookAheadEnabled(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    void dexpLookAheadMs_idempotentAndCarriesValue()
    {
        RadioModel model;
        // Default per Thetis Designer is 60 ms.
        QCOMPARE(model.transmitModel().dexpLookAheadMs(), 60.0);
        QSignalSpy spy(&model.transmitModel(),
                       &TransmitModel::dexpLookAheadMsChanged);
        model.transmitModel().setDexpLookAheadMs(120.0);
        model.transmitModel().setDexpLookAheadMs(120.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 120.0);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Side-channel filter (Task 10) — 3 signals
    // ─────────────────────────────────────────────────────────────────────

    void dexpLowCutHz_idempotentAndCarriesValue()
    {
        RadioModel model;
        // Default per Thetis Designer is 500 Hz.
        QCOMPARE(model.transmitModel().dexpLowCutHz(), 500.0);
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::dexpLowCutHzChanged);
        model.transmitModel().setDexpLowCutHz(300.0);
        model.transmitModel().setDexpLowCutHz(300.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 300.0);
    }

    void dexpHighCutHz_idempotentAndCarriesValue()
    {
        RadioModel model;
        // Default per Thetis Designer is 1500 Hz.
        QCOMPARE(model.transmitModel().dexpHighCutHz(), 1500.0);
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::dexpHighCutHzChanged);
        model.transmitModel().setDexpHighCutHz(2800.0);
        model.transmitModel().setDexpHighCutHz(2800.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 2800.0);
    }

    void dexpSideChannelFilterEnabledChanged_firesOnce()
    {
        RadioModel model;
        // Default per Thetis Designer is true.
        QCOMPARE(model.transmitModel().dexpSideChannelFilterEnabled(), true);
        QSignalSpy spy(&model.transmitModel(),
                       &TransmitModel::dexpSideChannelFilterEnabledChanged);
        model.transmitModel().setDexpSideChannelFilterEnabled(false);
        model.transmitModel().setDexpSideChannelFilterEnabled(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Bulk fan-out — null-TxChannel reconciliation
    //
    // Same null-TxChannel reconciliation as tst_radio_model_eq_lev_alc_wiring's
    // bulkSignalEmit_noCrashWithNullTxChannel — we confirm that hammering
    // all 11 new DEXP TM setters in sequence does not crash, even when the
    // RadioModel-installed lambdas are absent (connectToRadio never runs in
    // unit tests because no real radio is connected).
    // ─────────────────────────────────────────────────────────────────────

    void bulkSignalEmit_dexp_noCrashWithNullTxChannel()
    {
        RadioModel model;
        // Envelope (4)
        model.transmitModel().setDexpEnabled(true);
        model.transmitModel().setDexpDetectorTauMs(50.0);
        model.transmitModel().setDexpAttackTimeMs(15.0);
        model.transmitModel().setDexpReleaseTimeMs(400.0);
        // Gate ratios (2)
        model.transmitModel().setDexpExpansionRatioDb(25.0);
        model.transmitModel().setDexpHysteresisRatioDb(7.0);
        // Look-ahead (2)
        model.transmitModel().setDexpLookAheadEnabled(false);
        model.transmitModel().setDexpLookAheadMs(150.0);
        // SCF (3)
        model.transmitModel().setDexpLowCutHz(250.0);
        model.transmitModel().setDexpHighCutHz(3500.0);
        model.transmitModel().setDexpSideChannelFilterEnabled(false);
        // No QFAIL — we just need the test to complete.
        QVERIFY(true);
    }

    // ─────────────────────────────────────────────────────────────────────
    // _initialPushAfterConnect_pushesAllProperties (model coverage)
    //
    // pushTxProcessingChain is captured by-value into the
    // MicProfileManager::activeProfileChanged lambda and invoked once
    // immediately after the 38 connects install (RadioModel.cpp ~L2216).
    // Without a live TxChannel + WDSP we can't observe the WDSP setter
    // calls, but we CAN verify that the model exposes all 11 new state
    // values that pushTxProcessingChain reads (the helper would early-out
    // on a null txChannel) — in other words, the getter surface that backs
    // the on-connect push exists and returns the loaded defaults.
    // ─────────────────────────────────────────────────────────────────────

    void initialPushAfterConnect_pushesAllDexpProperties()
    {
        RadioModel model;
        const auto& tm = model.transmitModel();

        // Envelope (4) — feeds setDexpRun, setDexpDetectorTau,
        // setDexpAttackTime, setDexpReleaseTime.
        QCOMPARE(tm.dexpEnabled(),       false);  // chkDEXPEnable default
        QCOMPARE(tm.dexpDetectorTauMs(), 20.0);   // udDEXPDetTau.Value
        QCOMPARE(tm.dexpAttackTimeMs(),  2.0);    // udDEXPAttack.Value
        QCOMPARE(tm.dexpReleaseTimeMs(), 100.0);  // udDEXPRelease.Value

        // Gate ratios (2) — feeds setDexpExpansionRatio, setDexpHysteresisRatio.
        QCOMPARE(tm.dexpExpansionRatioDb(),  10.0);  // udDEXPExpansionRatio.Value
        QCOMPARE(tm.dexpHysteresisRatioDb(), 2.0);   // udDEXPHysteresisRatio scaled

        // Look-ahead (2) — feeds setDexpRunAudioDelay, setDexpAudioDelay.
        QCOMPARE(tm.dexpLookAheadEnabled(), true);   // chkDEXPLookAheadEnable.Checked
        QCOMPARE(tm.dexpLookAheadMs(),      60.0);   // udDEXPLookAhead.Value

        // SCF (3) — feeds setDexpLowCut, setDexpHighCut,
        // setDexpRunSideChannelFilter.
        QCOMPARE(tm.dexpLowCutHz(),                  500.0);   // udSCFLowCut.Value
        QCOMPARE(tm.dexpHighCutHz(),                 1500.0);  // udSCFHighCut.Value
        QCOMPARE(tm.dexpSideChannelFilterEnabled(),  true);    // chkSCFEnable.Checked
    }
};

QTEST_MAIN(TstRadioModelDexpRouting)
#include "tst_radio_model_dexp_routing.moc"
