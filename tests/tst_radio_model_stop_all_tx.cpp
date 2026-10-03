// no-port-check: NereusSDR-original. Thetis's StopAllTx (console.cs) is
// cited in RadioModel.cpp beside the port; this test translates no C#.
//
// Task 33 (R-IOS-03, remote design §12.1): RadioModel::stopAllTx and the
// emergency stop it starts with.
//
//   1. MOX on, TUNE on, two-tone on: each ends with MOX, manual MOX, TUNE
//      and two-tone all off, and transmitStopped fires once with the
//      message. MOX and the relay go off to the connection before
//      stopAllTx returns, ahead of the normal unkey.
//   2. Nothing keyed: nothing happens and nothing is emitted.
//   3. A mic PTT still held after the stop does not key again until it has
//      been released (Thetis's _stop_all_tx latch).
//   4. A key queued before the stop (its hardware flip not yet run) does
//      not key the radio after it; the next fresh key does.
//   5. stopTransmitNow alone writes MOX and relay off at once and leaves
//      the keying state to its caller.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// Records MOX and relay writes in order.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    QStringList log;

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool on) override
    {
        log.append(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off"));
    }
    void setTrxRelay(bool on) override
    {
        log.append(on ? QStringLiteral("relay on") : QStringLiteral("relay off"));
    }
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

void pump(int passes = 8)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// A local model with a connected slice on 20 m USB, a mock connection, a TX
// channel wrapper with no WDSP channel behind it, two-tone ready to run, and
// MoxController's walk driven by processEvents.
struct Rig {
    RadioModel model;
    MockConnection conn;
    TxChannel tx{WdspEngine::kTxChannelId};

    Rig()
    {
        AppSettings::instance().clear();
        model.setCapsForTest(/*hasAlex=*/false);
        model.injectConnectionForTest(&conn);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        if (SliceModel* slice = model.activeSlice()) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14'200'000.0);
        }
        model.injectTxChannelForTest(&tx);
        TwoToneController* twoTone = model.twoToneController();
        twoTone->setTxChannel(&tx);
        twoTone->setPowerOn(true);
        twoTone->setSettleDelaysMs(0, 0);
    }
    ~Rig()
    {
        model.twoToneController()->setTxChannel(nullptr);
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    bool allOff()
    {
        return !model.mox()
            && !model.moxController()->isManualMox()
            && !model.isTune()
            && !model.twoToneController()->isActive()
            && !model.twoToneController()->isActivationInFlight();
    }
};

} // namespace

class TestRadioModelStopAllTx : public QObject {
    Q_OBJECT

    // stopAllTx from a keyed state: RF stops first (the gate closed, MOX and
    // the relay off to the connection before it returns), then every
    // keying state is cleared and transmitStopped fires once.
    static void checkStopsEverything(Rig& rig)
    {
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.conn.log.clear();

        rig.model.stopAllTx(QStringLiteral("Time Out Timer"));

        // Before any event runs: the direct write came first.
        QVERIFY(rig.conn.log.size() >= 2);
        QCOMPARE(rig.conn.log.at(0), QStringLiteral("MOX off"));
        QCOMPARE(rig.conn.log.at(1), QStringLiteral("relay off"));
        QVERIFY(!rig.tx.isRfGateOpen());

        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
        pump();
        QVERIFY(rig.allOff());
        QCOMPARE(rig.model.moxController()->state(), MoxState::Rx);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), QStringLiteral("Time Out Timer"));
        QVERIFY2(!rig.conn.log.contains(QStringLiteral("MOX on")),
                 "nothing keyed the radio again after the stop");
    }

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void stopsMox()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        pump();
        QVERIFY(rig.model.mox());
        checkStopsEverything(rig);
    }

    void stopsTune()
    {
        Rig rig;
        rig.model.setTune(true);
        pump();
        QVERIFY(rig.model.isTune());
        QVERIFY(rig.model.mox());
        QVERIFY(rig.model.moxController()->isManualMox());
        checkStopsEverything(rig);
    }

    void stopsTwoTone()
    {
        Rig rig;
        rig.model.twoToneController()->setActive(true);
        pump();
        QVERIFY(rig.model.twoToneController()->isActive());
        QVERIFY(rig.model.mox());
        checkStopsEverything(rig);
    }

    // Manual MOX alone (a refused key can leave it set) is one of Thetis's
    // four conditions.
    void stopsManualMoxAlone()
    {
        Rig rig;
        rig.model.moxController()->setTune(true);
        rig.model.moxController()->setMox(false);
        pump();
        QVERIFY(!rig.model.mox());
        QVERIFY(rig.model.moxController()->isManualMox());

        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.model.stopAllTx(QString());
        pump();
        QVERIFY(rig.allOff());
        QCOMPARE(stopped.count(), 1);
        QVERIFY(stopped.at(0).at(0).toString().isEmpty());
    }

    void nothingKeyedDoesNothing()
    {
        Rig rig;
        pump();
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        QSignalSpy walk(rig.model.moxController(), &MoxController::stateChanged);
        rig.conn.log.clear();

        rig.model.stopAllTx(QStringLiteral("Time Out Timer"));
        pump();

        QCOMPARE(stopped.count(), 0);
        QCOMPARE(walk.count(), 0);
        QVERIFY(rig.conn.log.isEmpty());
        QVERIFY(!rig.model.moxController()->isStopAllTxLatched());
        // Nothing was held, so the next key keys.
        rig.model.moxController()->setMox(true);
        pump();
        QVERIFY(rig.model.mox());
        QVERIFY(rig.conn.log.contains(QStringLiteral("MOX on")));
    }

    // Thetis console.cs PollPTT with _stop_all_tx: a PTT still held after
    // the stop does not switch anything back on until it is released.
    void heldMicPttDoesNotKeyAgainUntilReleased()
    {
        Rig rig;
        MoxController* mox = rig.model.moxController();
        mox->onMicPttFromRadio(true);
        pump();
        QVERIFY(rig.model.mox());

        rig.model.stopAllTx(QStringLiteral("Time Out Timer"));
        pump();
        QVERIFY(!rig.model.mox());
        QVERIFY(mox->isStopAllTxLatched());

        // The radio keeps reporting the held PTT on every status frame.
        rig.conn.log.clear();
        for (int i = 0; i < 5; ++i) {
            mox->onMicPttFromRadio(true);
            pump(2);
        }
        QVERIFY(!rig.model.mox());
        QVERIFY(!rig.conn.log.contains(QStringLiteral("MOX on")));

        // Released, then pressed again: that is a new press, and it keys.
        mox->onMicPttFromRadio(false);
        QVERIFY(!mox->isStopAllTxLatched());
        mox->onMicPttFromRadio(true);
        pump();
        QVERIFY(rig.model.mox());
        QVERIFY(rig.conn.log.contains(QStringLiteral("MOX on")));
    }

    // A key whose hardware flip is still queued when the stop runs must not
    // key the radio afterwards; the next fresh key does.
    void aKeyQueuedBeforeTheStopDoesNotKey()
    {
        Rig rig;
        rig.conn.log.clear();
        rig.model.moxController()->setMox(true);   // hardwareFlipped(true) queued
        rig.model.stopAllTx(QString());
        pump();
        QVERIFY(rig.allOff());
        QVERIFY2(!rig.conn.log.contains(QStringLiteral("MOX on")),
                 "the flip queued before the stop keyed the radio");
        QVERIFY(!rig.tx.isRfGateOpen());

        rig.conn.log.clear();
        rig.model.moxController()->setMox(true);
        pump();
        QVERIFY(rig.conn.log.contains(QStringLiteral("MOX on")));
        rig.model.moxController()->setMox(false);
        pump();
    }

    // stopTransmitNow alone: RF off at once, keying state left to the caller.
    void stopTransmitNowWritesMoxOffAtOnce()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        pump();
        rig.conn.log.clear();

        rig.model.stopTransmitNow(QStringLiteral("test"));

        QCOMPARE(rig.conn.log, QStringList({QStringLiteral("MOX off"),
                                            QStringLiteral("relay off")}));
        QVERIFY(!rig.tx.isRfGateOpen());
        // The caller clears the keying state; stopTransmitNow does not.
        QVERIFY(rig.model.mox());
        rig.model.moxController()->setMox(false);
        pump();
        QVERIFY(!rig.model.mox());
    }
};

QTEST_MAIN(TestRadioModelStopAllTx)
#include "tst_radio_model_stop_all_tx.moc"
