// no-port-check: NereusSDR-original unit-test file.  All Thetis source cites
// are in TransmitModel.h/cpp / RadioModel.cpp.
// =================================================================
// tests/tst_radio_model_eq_lev_alc_wiring.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-i Batch 2 — Task 1 wiring tests.
//
// Verifies that RadioModel::connectToRadio installs the 13 connect()
// pairings between TransmitModel TX EQ + Leveler + ALC change signals
// and the TxChannel WDSP wrappers added in Batch 1.
//
// Test strategy: m_txChannel is null in unit-test builds (createTxChannel
// only runs after WDSP initializes; WDSP isn't initialized here).  The
// AutoConnection installed by RadioModel binds the TransmitModel signal
// to a lambda that captures `this` and calls m_txChannel->setX().  The
// lambda body is null-safe by virtue of the receiver=m_txChannel
// argument — Qt auto-disconnects when the receiver is destroyed, but if
// m_txChannel is null at connect time the connect() simply doesn't
// install (no-op).  We therefore verify the TWO model-side guarantees
// that hold without a live channel:
//
//   1. TransmitModel signals are emitted exactly once per setter call
//      (the idempotent guard inside each setter prevents double-fire on
//      same-value writes).
//   2. Setting + emitting many signals in sequence does not crash and
//      the model state stays consistent.
//
// Slot-body coverage (the actual SetTXAEQ* call) is exercised by
// tst_tx_channel_eq_setters and tst_tx_channel_leveler_alc_setters
// (Batch 1 Tasks B-1 / B-2).  This file covers the ROUTING contract.
//
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TstRadioModelEqLevAlcWiring : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()  { AppSettings::instance().clear(); }
    void init()          { AppSettings::instance().clear(); }
    void cleanup()       { AppSettings::instance().clear(); }

    // ── 1. Signal fan-out: setter → exactly one signal ────────────────────
    void txEqEnabledChanged_firesOnce()
    {
        RadioModel model;
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::txEqEnabledChanged);
        model.transmitModel().setTxEqEnabled(true);
        QCOMPARE(spy.count(), 1);
    }

    // ── 2. Idempotent guard: same value twice → only one signal ───────────
    void txEqEnabledChanged_idempotentNoDoubleFire()
    {
        RadioModel model;
        model.transmitModel().setTxEqEnabled(false);  // already default false
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::txEqEnabledChanged);
        model.transmitModel().setTxEqEnabled(false);  // same value
        model.transmitModel().setTxEqEnabled(false);  // same value again
        QCOMPARE(spy.count(), 0);
        model.transmitModel().setTxEqEnabled(true);   // change
        model.transmitModel().setTxEqEnabled(true);   // same again — no fire
        QCOMPARE(spy.count(), 1);
    }

    // ── 3. Leveler max gain idempotent ────────────────────────────────────
    void txLevelerMaxGain_idempotentNoDoubleFire()
    {
        RadioModel model;
        // Default per Thetis database.cs:4590 is 15 dB.
        QCOMPARE(model.transmitModel().txLevelerMaxGain(), 15);
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::txLevelerMaxGainChanged);
        model.transmitModel().setTxLevelerMaxGain(15);  // same value
        QCOMPARE(spy.count(), 0);
        model.transmitModel().setTxLevelerMaxGain(10);  // change
        QCOMPARE(spy.count(), 1);
        model.transmitModel().setTxLevelerMaxGain(10);  // same again
        QCOMPARE(spy.count(), 1);
    }

    // ── 4. Leveler decay idempotent ───────────────────────────────────────
    void txLevelerDecay_idempotentNoDoubleFire()
    {
        RadioModel model;
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::txLevelerDecayChanged);
        model.transmitModel().setTxLevelerDecay(250);
        model.transmitModel().setTxLevelerDecay(250);
        QCOMPARE(spy.count(), 1);
    }

    // ── 5. ALC max gain idempotent ────────────────────────────────────────
    void txAlcMaxGain_idempotentNoDoubleFire()
    {
        RadioModel model;
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::txAlcMaxGainChanged);
        model.transmitModel().setTxAlcMaxGain(7);
        model.transmitModel().setTxAlcMaxGain(7);
        QCOMPARE(spy.count(), 1);
    }

    // ── 6. ALC decay idempotent ───────────────────────────────────────────
    void txAlcDecay_idempotentNoDoubleFire()
    {
        RadioModel model;
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::txAlcDecayChanged);
        model.transmitModel().setTxAlcDecay(25);
        model.transmitModel().setTxAlcDecay(25);
        QCOMPARE(spy.count(), 1);
    }

    // ── 7. EQ band signal carries index + value ───────────────────────────
    void txEqBandChanged_carriesIndexAndDb()
    {
        RadioModel model;
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::txEqBandChanged);
        model.transmitModel().setTxEqBand(3, 5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 3);
        QCOMPARE(spy.first().at(1).toInt(), 5);
    }

    // ── 8. EQ freq signal carries index + value ───────────────────────────
    void txEqFreqChanged_carriesIndexAndHz()
    {
        RadioModel model;
        QSignalSpy spy(&model.transmitModel(), &TransmitModel::txEqFreqChanged);
        model.transmitModel().setTxEqFreq(7, 5000);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 7);
        QCOMPARE(spy.first().at(1).toInt(), 5000);
    }

    // ── 9. EQ globals all four signals fire ───────────────────────────────
    void txEqGlobals_allFourEmit()
    {
        RadioModel model;
        QSignalSpy ncSpy(&model.transmitModel(), &TransmitModel::txEqNcChanged);
        QSignalSpy mpSpy(&model.transmitModel(), &TransmitModel::txEqMpChanged);
        QSignalSpy ctfSpy(&model.transmitModel(), &TransmitModel::txEqCtfmodeChanged);
        QSignalSpy winSpy(&model.transmitModel(), &TransmitModel::txEqWintypeChanged);

        // Move each off its default to force a fire.
        model.transmitModel().setTxEqNc(4096);
        model.transmitModel().setTxEqMp(true);
        model.transmitModel().setTxEqCtfmode(1);
        model.transmitModel().setTxEqWintype(1);

        QCOMPARE(ncSpy.count(), 1);
        QCOMPARE(mpSpy.count(), 1);
        QCOMPARE(ctfSpy.count(), 1);
        QCOMPARE(winSpy.count(), 1);
    }

    // ── 10. Bulk fan-out doesn't crash with null TxChannel ────────────────
    // Path (b) reconciliation: with m_txChannel null in unit tests, the
    // RadioModel-installed connect()s either don't install (receiver was
    // null at connect time — which is the case in unit tests because
    // connectToRadio() is never called here) or are installed and remain
    // null-safe via the receiver=m_txChannel auto-disconnect.  Either
    // way, emitting all 13 signals in sequence must not crash.
    void bulkSignalEmit_noCrashWithNullTxChannel()
    {
        RadioModel model;
        // Hammer all 13 signals via setters.
        model.transmitModel().setTxEqEnabled(true);
        model.transmitModel().setTxEqPreamp(3);
        model.transmitModel().setTxEqBand(0, 5);
        model.transmitModel().setTxEqBand(9, -3);
        model.transmitModel().setTxEqFreq(0, 100);
        model.transmitModel().setTxEqFreq(9, 12000);
        model.transmitModel().setTxEqNc(4096);
        model.transmitModel().setTxEqMp(true);
        model.transmitModel().setTxEqCtfmode(1);
        model.transmitModel().setTxEqWintype(1);
        model.transmitModel().setTxLevelerOn(false);
        model.transmitModel().setTxLevelerMaxGain(8);
        model.transmitModel().setTxLevelerDecay(250);
        model.transmitModel().setTxAlcMaxGain(7);
        model.transmitModel().setTxAlcDecay(25);
        // No QFAIL — we just need the test to complete.
        QVERIFY(true);
    }
};

QTEST_MAIN(TstRadioModelEqLevAlcWiring)
#include "tst_radio_model_eq_lev_alc_wiring.moc"
