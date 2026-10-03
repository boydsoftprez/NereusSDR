// no-port-check: NereusSDR-original. Thetis's loss-of-sync power-off
// (console.cs) is cited in RadioModel.cpp beside the code; this test
// translates no C#.
//
// TX safety (whole-branch review 2026-09-30): a lost radio link ends every
// transmission in the model, so the MOX button and the windows read unkeyed
// and nothing is left keyed for the connection's reconnect to resume.
//
//   1. MOX keyed: LinkLost leaves MOX, manual MOX, TUNE and two-tone off,
//      MOX and the relay go off to the connection, transmitStopped fires
//      once, and nothing keys the connection again.
//   2. TUNE keyed: the same.
//   3. Nothing keyed: LinkLost emits no stop.
//   4. The link back (Connected) does not key anything.
//   5. Fix round 1 (I1): while the link is lost, and through the reconnect
//      that follows, every key is refused with "The link to the radio is
//      down." (code interlock), MOX, TUN and 2TONE are locked with that
//      reason (VOX is not), and the model never shows TX; when the link is
//      back the lock lifts and a key reaches the connection.
//   6. Fix round 1 (I2): the link back pushes the PureSignal enable to the
//      connection again, whatever the protocol.
//   7. Follow-up (Minor 1): the operator's disconnect after a lost link
//      clears the lost-link lock, so the window no longer says the link is
//      down on a station disconnected on purpose.
//   8. Fix round 2: automatic recovery's retire (the Core's
//      retire-and-reconnect, the hosted GUI's retry) keeps the lock and its
//      reason through the Disconnected wait and the rebuilt link's
//      Connecting and Probing, refuses a key there, and lifts on Connected.
//   9. Fix round 2: the operator's disconnect made after such a retire, when
//      there is no connection left to tear down, still clears the lock.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/safety/TxRefusal.h"
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
    void setPuresignalRun(bool on) override
    {
        log.append(on ? QStringLiteral("PS on") : QStringLiteral("PS off"));
    }
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void setStateForTest(ConnectionState s) { setState(s); }
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

class TestRadioModelLinkLostUnkey : public QObject {
    Q_OBJECT

    static void checkLinkLossUnkeys(Rig& rig)
    {
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.conn.log.clear();

        rig.conn.setStateForTest(ConnectionState::LinkLost);
        rig.model.onConnectionStateChangedForTest(ConnectionState::LinkLost);

        QVERIFY(rig.conn.log.size() >= 2);
        QCOMPARE(rig.conn.log.at(0), QStringLiteral("MOX off"));
        QCOMPARE(rig.conn.log.at(1), QStringLiteral("relay off"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
        pump();
        QVERIFY(rig.allOff());
        QCOMPARE(rig.model.moxController()->state(), MoxState::Rx);
        QCOMPARE(stopped.count(), 1);
        QVERIFY(!stopped.at(0).at(0).toString().isEmpty());

        // The link comes back: still unkeyed, and nothing keyed the radio.
        rig.conn.setStateForTest(ConnectionState::Connected);
        rig.model.onConnectionStateChangedForTest(ConnectionState::Connected);
        pump();
        QVERIFY(rig.allOff());
        QVERIFY2(!rig.conn.log.contains(QStringLiteral("MOX on")),
                 "nothing keyed the radio again after the link loss");
    }

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void linkLossUnkeysMox()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        pump();
        QVERIFY(rig.model.mox());
        checkLinkLossUnkeys(rig);
    }

    void linkLossUnkeysTune()
    {
        Rig rig;
        rig.model.setTune(true);
        pump();
        QVERIFY(rig.model.isTune());
        QVERIFY(rig.model.mox());
        checkLinkLossUnkeys(rig);
    }

    void linkLossUnkeyedEmitsNoStop()
    {
        Rig rig;
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.conn.log.clear();
        rig.conn.setStateForTest(ConnectionState::LinkLost);
        rig.model.onConnectionStateChangedForTest(ConnectionState::LinkLost);
        pump();
        QCOMPARE(stopped.count(), 0);
        QVERIFY(!rig.conn.log.contains(QStringLiteral("MOX on")));
    }

    void outageRefusesEveryKeyWithItsReason()
    {
        Rig rig;
        MoxController* mox = rig.model.moxController();
        const QString reason = QStringLiteral("The link to the radio is down.");
        QSignalSpy downChanged(&rig.model, &RadioModel::radioLinkDownChanged);
        QSignalSpy rejected(mox, &MoxController::moxRejected);
        QSignalSpy refused(mox, &MoxController::moxRefused);
        QSignalSpy moxState(mox, &MoxController::moxStateChanged);
        rig.conn.log.clear();

        rig.conn.setStateForTest(ConnectionState::LinkLost);
        rig.model.onConnectionStateChangedForTest(ConnectionState::LinkLost);
        pump();
        QVERIFY(rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 1);
        QCOMPARE(downChanged.at(0).at(0).toBool(), true);

        // The windows' lock: MOX, TUN and 2TONE with the reason, not VOX.
        QVERIFY(rig.model.transmitButtonsLocked());
        QVERIFY(rig.model.transmitLockCoversMox());
        QVERIFY(!rig.model.transmitLockCoversVox());
        QCOMPARE(rig.model.transmitLockReasonAlongside(QString()), reason);
        // The refusal remote windows and the phone are sent.
        QCOMPARE(mox->transmitBlockReason(), reason);
        QCOMPARE(mox->transmitBlockRefusal().code, QByteArray(TxRefusals::kInterlock));
        QCOMPARE(mox->transmitBlockRefusal().text, reason);

        mox->setMox(true);
        pump();
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), reason);
        QCOMPARE(refused.count(), 1);
        QVERIFY(!rig.model.mox());

        rig.model.setTune(true);
        pump();
        QVERIFY(!rig.model.isTune());
        QVERIFY(!rig.model.mox());

        // The reconnect under way (Connecting) still refuses.
        rig.conn.setStateForTest(ConnectionState::Connecting);
        rig.model.onConnectionStateChangedForTest(ConnectionState::Connecting);
        pump();
        QVERIFY(rig.model.isRadioLinkDown());
        mox->setMox(true);
        pump();
        QVERIFY(!rig.model.mox());

        for (const QList<QVariant>& args : moxState) {
            QVERIFY2(!args.at(0).toBool(), "the model showed TX during the outage");
        }
        QVERIFY2(!rig.conn.log.contains(QStringLiteral("MOX on")),
                 "a key reached the connection during the outage");

        // The link is back: the lock lifts and the next key goes out.
        rig.conn.setStateForTest(ConnectionState::Connected);
        rig.model.onConnectionStateChangedForTest(ConnectionState::Connected);
        pump();
        QVERIFY(!rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 2);
        QCOMPARE(downChanged.at(1).at(0).toBool(), false);
        QVERIFY(!rig.model.transmitButtonsLocked());
        QVERIFY(mox->transmitBlockReason().isEmpty());

        mox->setMox(true);
        pump();
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.mox(), 5000);
        QVERIFY(rig.conn.log.contains(QStringLiteral("MOX on")));
        mox->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
    }

    void linkBackPutsPureSignalEnableBack()
    {
        Rig rig;
        rig.model.transmitModel().setPureSigEnabled(true);
        pump();
        rig.conn.setStateForTest(ConnectionState::LinkLost);
        rig.model.onConnectionStateChangedForTest(ConnectionState::LinkLost);
        pump();
        rig.conn.log.clear();

        rig.conn.setStateForTest(ConnectionState::Connected);
        rig.model.onConnectionStateChangedForTest(ConnectionState::Connected);
        pump();
        QVERIFY2(rig.conn.log.contains(QStringLiteral("PS on")),
                 qPrintable(rig.conn.log.join(QStringLiteral(", "))));
        QVERIFY(!rig.conn.log.contains(QStringLiteral("PS off")));
    }

    void disconnectAfterLinkLossClearsTheLock()
    {
        Rig rig;
        const QString reason = QStringLiteral("The link to the radio is down.");
        QSignalSpy downChanged(&rig.model, &RadioModel::radioLinkDownChanged);
        rig.conn.setStateForTest(ConnectionState::LinkLost);
        rig.model.onConnectionStateChangedForTest(ConnectionState::LinkLost);
        pump();
        QVERIFY(rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 1);

        // The operator's disconnect goes through teardownConnection, which
        // sets Disconnected itself and never reaches onConnectionStateChanged;
        // disconnectFromRadio lifts the lock after it.
        rig.model.disconnectFromRadio();
        pump();
        QCOMPARE(rig.model.connectionState(), ConnectionState::Disconnected);
        QVERIFY(!rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 2);
        QCOMPARE(downChanged.at(1).at(0).toBool(), false);
        QVERIFY(rig.model.transmitLockReasonAlongside(QString()) != reason);
        QVERIFY(rig.model.moxController()->transmitBlockReason() != reason);
        QVERIFY(!rig.model.moxController()->isRadioLinkDown());
    }

    void recoveryRetireKeepsTheLockUntilConnected()
    {
        MockConnection rebuilt;  // outlives the rig, which holds it last
        Rig rig;
        MoxController* mox = rig.model.moxController();
        const QString reason = QStringLiteral("The link to the radio is down.");
        QSignalSpy downChanged(&rig.model, &RadioModel::radioLinkDownChanged);
        QSignalSpy rejected(mox, &MoxController::moxRejected);
        QSignalSpy moxState(mox, &MoxController::moxStateChanged);

        rig.conn.setStateForTest(ConnectionState::LinkLost);
        rig.model.onConnectionStateChangedForTest(ConnectionState::LinkLost);
        pump();
        QVERIFY(rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 1);

        // The recovery retire: Disconnected, and the lock holds.
        rig.model.retireConnectionForRecovery();
        pump();
        QCOMPARE(rig.model.connectionState(), ConnectionState::Disconnected);
        QVERIFY(rig.model.connection() == nullptr);
        QVERIFY(rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 1);
        QCOMPARE(rig.model.transmitLockReasonAlongside(QString()), reason);
        QCOMPARE(mox->transmitBlockRefusal().text, reason);

        // The rebuilt link: Connecting then Probing, each still locked with
        // the reason a remote window is sent, and a key is refused.
        rebuilt.log.clear();
        rig.model.injectConnectionForTest(&rebuilt);
        for (const ConnectionState s : {ConnectionState::Connecting,
                                        ConnectionState::Probing}) {
            rebuilt.setStateForTest(s);
            rig.model.onConnectionStateChangedForTest(s);
            pump();
            QVERIFY(rig.model.isRadioLinkDown());
            QVERIFY(rig.model.transmitButtonsLocked());
            QCOMPARE(rig.model.transmitLockReasonAlongside(QString()), reason);
            QCOMPARE(mox->transmitBlockReason(), reason);
            QCOMPARE(mox->transmitBlockRefusal().code,
                     QByteArray(TxRefusals::kInterlock));
            QCOMPARE(mox->transmitBlockRefusal().text, reason);
            const int before = rejected.count();
            mox->setMox(true);
            pump();
            QCOMPARE(rejected.count(), before + 1);
            QCOMPARE(rejected.at(before).at(0).toString(), reason);
            QVERIFY(!rig.model.mox());
        }
        QCOMPARE(downChanged.count(), 1);
        for (const QList<QVariant>& args : moxState) {
            QVERIFY2(!args.at(0).toBool(), "the model showed TX during the recovery");
        }
        QVERIFY2(!rebuilt.log.contains(QStringLiteral("MOX on")),
                 "a key reached the rebuilt link before it was Connected");

        // Connected: the lock lifts and the next key is accepted. (The
        // retire released the slice's TX binding, which only a real
        // connectToRadio restores, so this harness checks the gate, not
        // the wire.)
        rebuilt.setStateForTest(ConnectionState::Connected);
        rig.model.onConnectionStateChangedForTest(ConnectionState::Connected);
        pump();
        QVERIFY(!rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 2);
        QCOMPARE(downChanged.at(1).at(0).toBool(), false);
        QVERIFY(!rig.model.transmitButtonsLocked());
        QVERIFY(mox->transmitBlockReason().isEmpty());
        const int rejectedBefore = rejected.count();
        mox->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.mox(), 5000);
        QCOMPARE(rejected.count(), rejectedBefore);
        mox->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
    }

    void operatorDisconnectAfterRetireClearsTheLock()
    {
        Rig rig;
        const QString reason = QStringLiteral("The link to the radio is down.");
        QSignalSpy downChanged(&rig.model, &RadioModel::radioLinkDownChanged);
        rig.conn.setStateForTest(ConnectionState::LinkLost);
        rig.model.onConnectionStateChangedForTest(ConnectionState::LinkLost);
        pump();
        rig.model.retireConnectionForRecovery();
        pump();
        QVERIFY(rig.model.connection() == nullptr);
        QVERIFY(rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 1);

        // The operator's Disconnect while the Core waits to reconnect: there
        // is no connection for teardown to retire, and the lock still lifts.
        rig.model.disconnectFromRadio();
        pump();
        QCOMPARE(rig.model.connectionState(), ConnectionState::Disconnected);
        QVERIFY(!rig.model.isRadioLinkDown());
        QCOMPARE(downChanged.count(), 2);
        QCOMPARE(downChanged.at(1).at(0).toBool(), false);
        QVERIFY(rig.model.transmitLockReasonAlongside(QString()) != reason);
        QVERIFY(rig.model.moxController()->transmitBlockReason() != reason);
        QVERIFY(!rig.model.moxController()->isRadioLinkDown());
    }
};

QTEST_MAIN(TestRadioModelLinkLostUnkey)
#include "tst_radio_model_link_lost_unkey.moc"
