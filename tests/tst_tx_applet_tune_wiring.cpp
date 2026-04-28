// no-port-check: unit tests for TxApplet wiring model-layer contracts.
// Phase 3M-1a H.3.
//
// TxApplet::wireControls() connects four controls to the model layer.
// This file verifies the model APIs those controls depend on:
//
//   1. TransmitModel::setPower / power() round-trip.
//   2. TransmitModel::setTunePowerForBand / tunePowerForBand round-trip.
//   3. TransmitModel::tunePowerByBandChanged fires on change.
//   4. tunePowerByBandChanged NOT fired for unaffected bands.
//   5. TransmitModel::powerChanged fires on change.
//   6. MoxController isMox() default is false.
//   7. MoxController isManualMox() default is false.
//   8. MoxController moxStateChanged(false) fires on TX→RX walk completion.
//   9. MoxController manualMoxChanged(true) fires on setTune(true).
//  10. TxApplet setCurrentBand keeps band internally (no crash, model sync).
//
// H.3 does NOT construct a full TxApplet in headless tests because TxApplet
// is a QWidget subclass that requires a QApplication / display. The wiring
// that TxApplet.wireControls() attaches is validated by exercising the same
// model signals/slots that the applet connects to, confirming they behave
// as the applet expects without creating a visible window.
//
// Thetis references (model-layer behavior):
//   TransmitModel::setPower  — console.cs:4822 [v2.10.3.13] TXF power setter
//   tunePowerByBand          — console.cs:12094 [v2.10.3.13] tunePower_by_band[]
//   MoxController::setTune   — console.cs:29978-30157 [v2.10.3.13] chkTUN_CheckedChanged

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "models/TransmitModel.h"
#include "core/MoxController.h"
#include "models/Band.h"

using namespace NereusSDR;

class TestTxAppletTuneWiring : public QObject
{
    Q_OBJECT

private slots:
    void transmitModel_setPower_roundtrip();
    void transmitModel_setTunePowerForBand_roundtrip();
    void transmitModel_tunePowerByBandChanged_fires();
    void transmitModel_tunePowerByBandChanged_notFiredForOtherBand();
    void transmitModel_powerChanged_fires();
    void moxController_isMox_defaultFalse();
    void moxController_isManualMox_defaultFalse();
    void moxController_moxStateChanged_firesOnTxToRxWalk();
    void moxController_manualMoxChanged_firesOnSetTune();
    void transmitModel_tunePowerForBand_storesPerBand();
};

// 1. TransmitModel::setPower / power() round-trip
void TestTxAppletTuneWiring::transmitModel_setPower_roundtrip()
{
    TransmitModel tx;
    tx.setPower(75);
    QCOMPARE(tx.power(), 75);

    tx.setPower(0);
    QCOMPARE(tx.power(), 0);

    tx.setPower(100);
    QCOMPARE(tx.power(), 100);
}

// 2. TransmitModel::setTunePowerForBand / tunePowerForBand round-trip
void TestTxAppletTuneWiring::transmitModel_setTunePowerForBand_roundtrip()
{
    TransmitModel tx;
    tx.setTunePowerForBand(Band::Band20m, 15);
    QCOMPARE(tx.tunePowerForBand(Band::Band20m), 15);

    tx.setTunePowerForBand(Band::Band40m, 30);
    QCOMPARE(tx.tunePowerForBand(Band::Band40m), 30);

    // 20m unchanged after setting 40m
    QCOMPARE(tx.tunePowerForBand(Band::Band20m), 15);
}

// 3. TransmitModel::tunePowerByBandChanged fires on change
void TestTxAppletTuneWiring::transmitModel_tunePowerByBandChanged_fires()
{
    TransmitModel tx;
    QSignalSpy spy(&tx, &TransmitModel::tunePowerByBandChanged);

    tx.setTunePowerForBand(Band::Band20m, 20);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<Band>(), Band::Band20m);
    QCOMPARE(spy.at(0).at(1).toInt(), 20);
}

// 4. tunePowerByBandChanged NOT fired for unaffected bands (signal is per-band)
void TestTxAppletTuneWiring::transmitModel_tunePowerByBandChanged_notFiredForOtherBand()
{
    TransmitModel tx;
    QSignalSpy spy(&tx, &TransmitModel::tunePowerByBandChanged);

    tx.setTunePowerForBand(Band::Band20m, 10);
    // Only one signal, for 20m — not for every band
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<Band>(), Band::Band20m);
}

// 5. TransmitModel::powerChanged fires on change
void TestTxAppletTuneWiring::transmitModel_powerChanged_fires()
{
    TransmitModel tx;
    QSignalSpy spy(&tx, &TransmitModel::powerChanged);

    tx.setPower(50);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 50);
}

// 6. MoxController isMox() default is false
void TestTxAppletTuneWiring::moxController_isMox_defaultFalse()
{
    MoxController mox;
    QVERIFY(!mox.isMox());
}

// 7. MoxController isManualMox() default is false
void TestTxAppletTuneWiring::moxController_isManualMox_defaultFalse()
{
    MoxController mox;
    QVERIFY(!mox.isManualMox());
}

// 8. MoxController moxStateChanged(false) fires at end of TX→RX walk.
// TxApplet::wireControls() wires moxStateChanged to m_moxBtn->setChecked().
// Confirming the signal fires (on the false leg = RX) after drain.
//
// setTimerIntervals(0,...) makes the timer walk synchronous so processEvents()
// drives the full RxToTxRfDelay → Tx and Tx → TxToRxFlush → Rx chain
// deterministically in unit tests. Pattern from tst_mox_controller_basic.cpp.
void TestTxAppletTuneWiring::moxController_moxStateChanged_firesOnTxToRxWalk()
{
    MoxController mox;
    mox.setTimerIntervals(0, 0, 0, 0, 0, 0);  // synchronous walk
    QSignalSpy spy(&mox, &MoxController::moxStateChanged);

    // Start TX — one processEvents drives RxToTxRfDelay → Tx + fires moxStateChanged(true)
    mox.setMox(true);
    QCoreApplication::processEvents();

    // Release TX — two processEvents drain the TxToRx walk
    mox.setMox(false);
    QCoreApplication::processEvents(); // keyUpDelayTimer → TxToRxFlush
    QCoreApplication::processEvents(); // pttOutDelayTimer → Rx → moxStateChanged(false)

    // Must have fired at least twice (TX engage and RX return)
    QVERIFY(spy.count() >= 2);

    // Last signal should be false (RX mode)
    const bool lastState = spy.last().at(0).toBool();
    QVERIFY(!lastState);
}

// 9. MoxController manualMoxChanged(true) fires on setTune(true).
// TxApplet::wireControls() wires manualMoxChanged to update TUNE button text.
//
// setTimerIntervals(0,...) makes the walk synchronous — same pattern as test 8.
void TestTxAppletTuneWiring::moxController_manualMoxChanged_firesOnSetTune()
{
    MoxController mox;
    mox.setTimerIntervals(0, 0, 0, 0, 0, 0);  // synchronous walk
    QSignalSpy spy(&mox, &MoxController::manualMoxChanged);

    // setTune(true) sets m_manualMox → emits manualMoxChanged(true)
    mox.setTune(true);
    QVERIFY(spy.count() >= 1);
    QVERIFY(spy.last().at(0).toBool());

    // setTune(false) clears m_manualMox → emits manualMoxChanged(false)
    mox.setTune(false);
    QCoreApplication::processEvents(); // drain TX→RX walk
    QCoreApplication::processEvents();
    QVERIFY(spy.count() >= 2);
    QVERIFY(!spy.last().at(0).toBool());
}

// 10. TransmitModel per-band isolation: each band stores independently.
// Validates the data model that TxApplet::setCurrentBand() reads from.
void TestTxAppletTuneWiring::transmitModel_tunePowerForBand_storesPerBand()
{
    TransmitModel tx;

    // Set distinct values for several bands
    tx.setTunePowerForBand(Band::Band160m, 5);
    tx.setTunePowerForBand(Band::Band80m,  10);
    tx.setTunePowerForBand(Band::Band40m,  15);
    tx.setTunePowerForBand(Band::Band20m,  20);
    tx.setTunePowerForBand(Band::Band10m,  25);

    // Each reads back independently
    QCOMPARE(tx.tunePowerForBand(Band::Band160m), 5);
    QCOMPARE(tx.tunePowerForBand(Band::Band80m),  10);
    QCOMPARE(tx.tunePowerForBand(Band::Band40m),  15);
    QCOMPARE(tx.tunePowerForBand(Band::Band20m),  20);
    QCOMPARE(tx.tunePowerForBand(Band::Band10m),  25);
}

QTEST_MAIN(TestTxAppletTuneWiring)
#include "tst_tx_applet_tune_wiring.moc"
