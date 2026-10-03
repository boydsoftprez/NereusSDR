// no-port-check: test harness citing mi0bot-Thetis UpdateIOBoard and the
// Thetis TXInhibit setter as the behavioural spec it asserts against;
// contains no ported logic of its own.
// =================================================================
// tests/tst_hl2_io_board_fault.cpp  (NereusSDR)
// =================================================================
//
// HL2 port part 2, item 1: the Hermes Lite 2 I/O board's fault register
// holds transmit off, with the reason shown on the local window, a remote
// window and the phone.
//
// The spec, from mi0bot-Thetis console.cs:25874-25888 [@c26a8a4]
// (UpdateIOBoard, after each read of the general registers):
//
//     if (0 != ioBoard.readRegister(IOBoard.Registers.REG_FAULT))
//     {
//         TXInhibit = true;
//         infoBar.Warning("I/O Board: Fault Code " + ...REG_FAULT...);
//         AutoTuningHL2(ProtocolEvent.Idle);
//     }
//
// and the TXInhibit setter (Thetis console.cs:15341-15363 [v2.10.3.15]):
// the transmit buttons are disabled, a keyed MOX is dropped, and the
// status bar's TX Inhibit label shows.
//
// Operator ruling (2026-09-29): hold transmit off steadily while the last
// fault read is non-zero, and clear it when a read shows zero (mi0bot
// never clears it; its PollTXInhibit writes TXInhibit every 100 ms, so
// the fault would flicker). The reason is mi0bot's text,
// "I/O Board: Fault Code N".
//
// No hardware; nothing keys a radio; no audio device is opened.
//
// Modification history (NereusSDR):
//   2026-09-29: created, by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>

#include <memory>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/IoBoardHl2.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/safety/TxInhibitMonitor.h"
#include "core/safety/TxRefusal.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/applets/TxApplet.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::safety::TxInhibitMonitor;
using NereusSDR::Test::LoopbackTransport;

class FaultMockConnection : public RadioConnection {
    Q_OBJECT
public:
    QStringList log;

    explicit FaultMockConnection(QObject* parent = nullptr)
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

namespace {

void pump()
{
    for (int i = 0; i < 6; ++i) {
        QCoreApplication::processEvents();
    }
}

QString faultText(int code)
{
    return QStringLiteral("I/O Board: Fault Code %1").arg(code);
}

// One answered read of the general registers (REG_INPUT_PINS..
// REG_FIRMWARE_MAJOR), as the poll's step 1 asks for it: C1 carries the
// firmware major, C2 the fault, C3 the tuner, C4 the input pins.
void feedGeneralRead(RadioModel& m, quint8 fault, quint8 pins = 0)
{
    IoBoardHl2& io = m.ioBoardMutable();
    io.pushPendingRead({IoBoardHl2::kI2cAddrGeneral,
                        static_cast<quint8>(IoBoardHl2::Register::REG_INPUT_PINS)});
    io.applyI2cReadResponse(static_cast<quint8>(0x80 | 0x1D), /*fw major*/ 1,
                            fault, /*tuner*/ 0, pins);
    pump();
}

KeyerIdentity remote(const QByteArray& id)
{
    KeyerIdentity keyer;
    keyer.deviceId = id;
    keyer.source = PttMode::None;
    return keyer;
}

// A local radio of the given model: a connected mock, a synchronous MOX
// walk, one USB slice. With Timers::Real the MOX walk keeps its default
// delays and a TX channel wrapper (no WDSP channel behind it) carries the
// RF gate, so a test can tell an immediate stop from the operator's unkey.
enum class Timers { Zero, Real };

struct Rig {
    std::unique_ptr<FaultMockConnection> conn;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<TxChannel> tx;

    explicit Rig(HPSDRModel hw = HPSDRModel::HERMESLITE, Timers timers = Timers::Zero)
        : conn(std::make_unique<FaultMockConnection>())
        , model(std::make_unique<RadioModel>())
    {
        AppSettings::instance().clear();
        model->setHpsdrModelForTest(hw);
        model->injectConnectionForTest(conn.get());
        if (timers == Timers::Zero) {
            model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        } else {
            model->moxController()->setTimerIntervals(
                MoxController::kRfDelayMs, MoxController::kMoxDelayMs,
                MoxController::kSpaceDelayMs, MoxController::kKeyUpDelayMs,
                MoxController::kPttOutDelayMs, MoxController::kBreakInDelayMs);
            tx = std::make_unique<TxChannel>(WdspEngine::kTxChannelId);
            model->injectTxChannelForTest(tx.get());
            model->wireTxChannelKeyingForTest();
            TwoToneController* twoTone = model->twoToneController();
            twoTone->setTxChannel(tx.get());
            twoTone->setPowerOn(true);
            twoTone->setSettleDelaysMs(0, 0);
        }
        model->setTuneOffSettleMsForTest(0);
        model->addSlice();
        model->activeSlice()->setDspMode(DSPMode::USB);
    }

    ~Rig()
    {
        if (model->isTune()) {
            model->setTune(false);
            pump();
        }
        if (tx) {
            model->twoToneController()->setTxChannel(nullptr);
            QTRY_VERIFY(!model->transmitReleaseInProgress());
            model->injectTxChannelForTest(nullptr);
        }
        model->injectConnectionForTest(nullptr);
        model.reset();
        AppSettings::instance().clear();
    }

    MoxController* mox() const { return model->moxController(); }
};

} // namespace

class TestHl2IoBoardFault : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_securityDir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_securityDir.isValid());
        qRegisterMetaType<NereusSDR::TxRefusal>();
    }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ---- The monitor -------------------------------------------------------

    void faultHoldsTransmitOffWithoutTheExternalInhibitBox()
    {
        // mi0bot sets TXInhibit on a fault whatever the External TX
        // Inhibit box says; the box gates only the radio's input line.
        TxInhibitMonitor mon;
        QVERIFY(!mon.isEnabled());
        QSignalSpy changed(&mon, &TxInhibitMonitor::txInhibitedChanged);
        QSignalSpy fault(&mon, &TxInhibitMonitor::ioBoardFaultChanged);

        mon.notifyIoBoardFault(5);
        QVERIFY(mon.inhibited());
        QCOMPARE(mon.lastSource(), TxInhibitMonitor::Source::IoBoardFault);
        QCOMPARE(mon.ioBoardFaultCode(), quint8(5));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(fault.count(), 1);

        // Steady: the same code again changes nothing.
        mon.notifyIoBoardFault(5);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(fault.count(), 1);

        // A new code: still inhibited, the code is reported.
        mon.notifyIoBoardFault(7);
        QVERIFY(mon.inhibited());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(fault.count(), 2);
        QCOMPARE(mon.ioBoardFaultCode(), quint8(7));

        // A read of zero clears it.
        mon.notifyIoBoardFault(0);
        QVERIFY(!mon.inhibited());
        QCOMPARE(mon.lastSource(), TxInhibitMonitor::Source::None);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(fault.count(), 3);
    }

    void faultOutranksTheInputLineAndHandsBack()
    {
        TxInhibitMonitor mon;
        mon.setEnabled(true);
        mon.attachRadioInput(HPSDRModel::HERMESLITE, 1);
        mon.notifyUserDigitalInputs(0x00);     // I01 clear: the input asserts
        QCOMPARE(mon.lastSource(), TxInhibitMonitor::Source::UserIo01);

        mon.notifyIoBoardFault(3);
        QCOMPARE(mon.lastSource(), TxInhibitMonitor::Source::IoBoardFault);

        mon.notifyIoBoardFault(0);
        QVERIFY(mon.inhibited());
        QCOMPARE(mon.lastSource(), TxInhibitMonitor::Source::UserIo01);
    }

    // ---- A local HL2 -------------------------------------------------------

    void faultReadRefusesEveryKeyWithTheReason()
    {
        Rig rig;
        QVERIFY(rig.model->boardCapabilities().hasIoBoardHl2);
        feedGeneralRead(*rig.model, 0);
        QVERIFY(!rig.model->isTxInhibited());
        QVERIFY(rig.model->txInhibitReason().isEmpty());

        QSignalSpy reasonChanged(rig.model.get(), &RadioModel::txInhibitReasonChanged);
        feedGeneralRead(*rig.model, 5);
        QVERIFY(rig.model->isTxInhibited());
        QVERIFY(rig.mox()->isTxInhibited());
        QCOMPARE(rig.model->txInhibitReason(), faultText(5));
        QCOMPARE(rig.mox()->transmitBlockReason(), faultText(5));
        QVERIFY(OperatorWording::isPlain(rig.model->txInhibitReason()));
        QCOMPARE(reasonChanged.count(), 1);

        // Held steadily: another read of the same fault changes nothing.
        feedGeneralRead(*rig.model, 5);
        QVERIFY(rig.model->isTxInhibited());
        QCOMPARE(reasonChanged.count(), 1);

        // The operator's MOX button.
        QSignalSpy refused(rig.mox(), &MoxController::moxRefused);
        rig.mox()->setMox(true);
        pump();
        QVERIFY(!rig.mox()->isMox());
        QVERIFY(!rig.conn->log.contains(QStringLiteral("MOX on")));
        QVERIFY(refused.count() >= 1);
        QCOMPARE(refused.last().at(0).value<TxRefusal>().text, faultText(5));

        // A remote key (a phone).
        refused.clear();
        rig.mox()->setMox(true, remote("phone"));
        pump();
        QVERIFY(!rig.mox()->isMox());
        QVERIFY(refused.count() >= 1);
        QCOMPARE(refused.last().at(0).value<TxRefusal>().text, faultText(5));

        // TUNE.
        rig.model->setTune(true);
        pump();
        QVERIFY(!rig.model->isTune());
        QVERIFY(!rig.conn->log.contains(QStringLiteral("MOX on")));

        // A new code carries the new text.
        feedGeneralRead(*rig.model, 9);
        QCOMPARE(rig.model->txInhibitReason(), faultText(9));
        QCOMPARE(rig.mox()->transmitBlockReason(), faultText(9));

        // A read of zero clears it and the key goes through.
        feedGeneralRead(*rig.model, 0);
        QVERIFY(!rig.model->isTxInhibited());
        QVERIFY(!rig.mox()->isTxInhibited());
        QVERIFY(rig.model->txInhibitReason().isEmpty());
        rig.mox()->setMox(true);
        pump();
        QVERIFY(rig.mox()->isMox());
        rig.mox()->setMox(false);
        pump();
    }

    void theExternalInhibitStillReadsItsOwnReason()
    {
        // Without a fault the input line keeps its existing refusal.
        Rig rig;
        rig.model->setUseTxInhibit(true);
        rig.model->txInhibit().attachRadioInput(HPSDRModel::HERMESLITE, 1);
        rig.model->txInhibit().notifyUserDigitalInputs(0x00);
        pump();
        QVERIFY(rig.model->isTxInhibited());
        QVERIFY(rig.model->txInhibitReason().isEmpty());
        QSignalSpy refused(rig.mox(), &MoxController::moxRefused);
        rig.mox()->setMox(true);
        pump();
        QVERIFY(refused.count() >= 1);
        QCOMPARE(refused.last().at(0).value<TxRefusal>(), TxRefusals::txInhibited());
        rig.model->txInhibit().detachRadioInput();
    }

    // mi0bot's TXInhibit setter drops a keyed MOX:
    //   if (_tx_inhibit && chkMOX.Checked) chkMOX.Checked = false;
    // A fault is a hardware protection, so the drop is the immediate stop
    // (RadioModel::stopAllTx): the RF gate closes and MOX off goes to the
    // radio before the fault's answer has been handled, with no wait for
    // the operator unkey's delays or the send ring.
    static void checkStopsAtOnce(Rig& rig, quint8 code)
    {
        QSignalSpy stopped(rig.model.get(), &RadioModel::transmitStopped);
        rig.conn->log.clear();

        // The answer, handled without running the event loop.
        IoBoardHl2& io = rig.model->ioBoardMutable();
        io.pushPendingRead({IoBoardHl2::kI2cAddrGeneral,
                            static_cast<quint8>(IoBoardHl2::Register::REG_INPUT_PINS)});
        io.applyI2cReadResponse(static_cast<quint8>(0x80 | 0x1D), 1, code, 0, 0);

        QVERIFY2(!rig.conn->log.isEmpty(), "MOX off went to the radio at once");
        QCOMPARE(rig.conn->log.at(0), QStringLiteral("MOX off"));
        QVERIFY(!rig.tx->isRfGateOpen());
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), faultText(code));

        QTRY_VERIFY(!rig.mox()->isMox());
        QTRY_VERIFY(!rig.model->isTune());
        QTRY_VERIFY(!rig.model->twoToneController()->isActive());
        QTRY_VERIFY(!rig.model->transmitReleaseInProgress());
        QVERIFY(!rig.conn->log.contains(QStringLiteral("MOX on")));
        QVERIFY(rig.model->isTxInhibited());
    }

    void faultDuringTransmitDropsMox()
    {
        Rig rig(HPSDRModel::HERMESLITE, Timers::Real);
        feedGeneralRead(*rig.model, 0);
        rig.mox()->setMox(true);
        QTRY_VERIFY(rig.mox()->isMox());
        QTRY_VERIFY(rig.tx->isRfGateOpen());
        checkStopsAtOnce(rig, 4);
    }

    void faultDuringTuneDropsTune()
    {
        Rig rig(HPSDRModel::HERMESLITE, Timers::Real);
        feedGeneralRead(*rig.model, 0);
        rig.model->setTune(true);
        QTRY_VERIFY(rig.model->isTune());
        QTRY_VERIFY(rig.tx->isRfGateOpen());
        checkStopsAtOnce(rig, 2);
    }

    void faultDuringTwoToneDropsTwoTone()
    {
        Rig rig(HPSDRModel::HERMESLITE, Timers::Real);
        feedGeneralRead(*rig.model, 0);
        rig.model->twoToneController()->setActive(true);
        QTRY_VERIFY(rig.model->twoToneController()->isActive());
        QTRY_VERIFY(rig.tx->isRfGateOpen());
        checkStopsAtOnce(rig, 3);
    }

    // The operator's unkey is under way (its delays have not run out): a
    // fault then ends it at once as well.
    void faultDuringTheReleaseStopsAtOnce()
    {
        Rig rig(HPSDRModel::HERMESLITE, Timers::Real);
        feedGeneralRead(*rig.model, 0);
        rig.mox()->setMox(true);
        QTRY_VERIFY(rig.mox()->isMox());
        QTRY_VERIFY(rig.tx->isRfGateOpen());
        rig.conn->log.clear();
        rig.mox()->setMox(false);
        QVERIFY(rig.model->transmitReleaseInProgress());
        QVERIFY(!rig.conn->log.contains(QStringLiteral("MOX off")));
        checkStopsAtOnce(rig, 6);
    }

    // The fault arrives on the connection's thread, as the radio's answer
    // does: each answer carries its own fault byte, so two answers handled
    // before the main thread runs are both seen, in order.
    void faultReadFromTheConnectionThread()
    {
        Rig rig;
        feedGeneralRead(*rig.model, 0);
        QSignalSpy reasons(rig.model.get(), &RadioModel::txInhibitReasonChanged);
        QSignalSpy inhibited(rig.model.get(), &RadioModel::txInhibitedChanged);

        IoBoardHl2& io = rig.model->ioBoardMutable();
        std::unique_ptr<QThread> connThread(QThread::create([&io]() {
            for (const quint8 code : {quint8(5), quint8(0)}) {
                io.pushPendingRead({IoBoardHl2::kI2cAddrGeneral,
                                    static_cast<quint8>(IoBoardHl2::Register::REG_INPUT_PINS)});
                io.applyI2cReadResponse(static_cast<quint8>(0x80 | 0x1D), 1, code, 0, 0);
            }
        }));
        connThread->start();
        QVERIFY(connThread->wait(5000));

        QTRY_COMPARE(inhibited.count(), 2);
        QCOMPARE(inhibited.at(0).at(0).toBool(), true);
        QCOMPARE(inhibited.at(1).at(0).toBool(), false);
        QVERIFY(reasons.count() >= 1);
        QCOMPARE(reasons.at(0).at(0).toString(), faultText(5));
        QVERIFY(!rig.model->isTxInhibited());
        QVERIFY(rig.model->txInhibitReason().isEmpty());
    }

    // The model sends the reason before it announces inhibited, and
    // announces not-inhibited before it clears the reason. A remote window
    // applies them in that order, so it never holds inhibited with no
    // reason and shows the generic words for a moment. (The getters cannot
    // show this locally: both read the monitor at once. The order of the
    // signals, which the link follows, can.)
    void reasonNeverLagsTheInhibit()
    {
        Rig rig;
        feedGeneralRead(*rig.model, 0);
        RadioModel* m = rig.model.get();
        QStringList order;
        connect(m, &RadioModel::txInhibitedChanged, m, [&order](bool on) {
            order.append(on ? QStringLiteral("inhibited") : QStringLiteral("released"));
        });
        connect(m, &RadioModel::txInhibitReasonChanged, m, [&order](const QString& reason) {
            order.append(reason.isEmpty() ? QStringLiteral("reason cleared")
                                          : QStringLiteral("reason set"));
        });
        feedGeneralRead(*m, 5);
        QVERIFY(m->isTxInhibited());
        QVERIFY2(order.indexOf(QStringLiteral("reason set")) >= 0
                     && order.indexOf(QStringLiteral("reason set"))
                            < order.indexOf(QStringLiteral("inhibited")),
                 qPrintable(order.join(QStringLiteral(", "))));
        order.clear();
        feedGeneralRead(*m, 0);
        QVERIFY(!m->isTxInhibited());
        QVERIFY2(order.indexOf(QStringLiteral("released")) >= 0
                     && order.indexOf(QStringLiteral("released"))
                            < order.indexOf(QStringLiteral("reason cleared")),
                 qPrintable(order.join(QStringLiteral(", "))));
    }

    void disconnectClearsTheFault()
    {
        Rig rig;
        rig.model->wireTxInhibitInputForTest();
        feedGeneralRead(*rig.model, 6);
        QVERIFY(rig.model->isTxInhibited());
        rig.model->teardownTxInhibitInputForTest();
        QVERIFY(!rig.model->isTxInhibited());
        QVERIFY(rig.model->txInhibitReason().isEmpty());
    }

    void otherRadiosIgnoreTheRegister()
    {
        // Only the HL2 has the I/O board; a stray read on another radio
        // never holds transmit off.
        for (const HPSDRModel hw : {HPSDRModel::HERMES, HPSDRModel::ANAN_G2}) {
            Rig rig(hw);
            QVERIFY(!rig.model->boardCapabilities().hasIoBoardHl2);
            feedGeneralRead(*rig.model, 5);
            QVERIFY(!rig.model->isTxInhibited());
            QVERIFY(!rig.mox()->isTxInhibited());
            QVERIFY(rig.model->txInhibitReason().isEmpty());
        }
    }

    // ---- The visible cue ---------------------------------------------------

    void transmitButtonsAreDisabledWithTheReason()
    {
        Rig rig;
        TxApplet applet(rig.model.get());
        ContainerButtonDispatcher::Hooks hooks;
        hooks.transmitPermitted = [] { return true; };
        ContainerButtonDispatcher dispatcher(rig.model.get(), std::move(hooks));
        QPushButton* mox = applet.moxButton();
        QPushButton* tune = applet.tuneButton();
        QPushButton* twoTone = applet.twoToneButton();
        QPushButton* vox = applet.voxButton();
        QVERIFY(mox && tune && twoTone && vox);
        const QString moxTip = mox->toolTip();
        const QString tuneTip = tune->toolTip();
        QVERIFY(mox->isEnabled() && tune->isEnabled());

        feedGeneralRead(*rig.model, 5);
        for (QPushButton* b : {mox, tune, twoTone, vox}) {
            QVERIFY2(!b->isEnabled(), qPrintable(b->text() + QStringLiteral(" left enabled")));
            QCOMPARE(b->toolTip(), faultText(5));
            QVERIFY(OperatorWording::isPlain(b->toolTip()));
        }
        for (const auto id : {ContainerButtonDispatcher::Id::Mox,
                              ContainerButtonDispatcher::Id::Tun}) {
            const auto st = dispatcher.stateOf(id, 0);
            QVERIFY(!st.available);
            QCOMPARE(st.reason, faultText(5));
        }

        // The code changes: the reason follows.
        feedGeneralRead(*rig.model, 8);
        QCOMPARE(mox->toolTip(), faultText(8));

        feedGeneralRead(*rig.model, 0);
        for (QPushButton* b : {mox, tune, twoTone, vox}) {
            QVERIFY(b->isEnabled());
        }
        QCOMPARE(mox->toolTip(), moxTip);
        QCOMPARE(tune->toolTip(), tuneTip);
        QVERIFY(dispatcher.stateOf(ContainerButtonDispatcher::Id::Mox, 0).available);
    }

    // The lock covers every radio's TX inhibit, as Thetis's TXInhibit
    // setter does (console.cs:15341-15363 [v2.10.3.15]), not only the HL2
    // fault: another radio's External TX Inhibit line disables MOX and TUNE
    // with its reason.
    void externalInhibitOnAnotherRadioDisablesTheButtons()
    {
        Rig rig(HPSDRModel::HERMES);
        QVERIFY(!rig.model->boardCapabilities().hasIoBoardHl2);
        TxApplet applet(rig.model.get());
        ContainerButtonDispatcher::Hooks hooks;
        hooks.transmitPermitted = [] { return true; };
        ContainerButtonDispatcher dispatcher(rig.model.get(), std::move(hooks));
        QPushButton* mox = applet.moxButton();
        QPushButton* tune = applet.tuneButton();
        QVERIFY(mox && tune);
        QVERIFY(mox->isEnabled() && tune->isEnabled());
        const QString moxTip = mox->toolTip();

        rig.model->setUseTxInhibit(true);
        rig.model->txInhibit().attachRadioInput(HPSDRModel::HERMES, 1);
        rig.model->txInhibit().notifyUserDigitalInputs(0x00);   // I01 clear: asserted
        pump();
        QVERIFY(rig.model->isTxInhibited());
        const QString reason = TxRefusals::txInhibited().text;
        QVERIFY(!reason.isEmpty());
        QVERIFY(OperatorWording::isPlain(reason));
        for (QPushButton* b : {mox, tune}) {
            QVERIFY2(!b->isEnabled(), qPrintable(b->text() + QStringLiteral(" left enabled")));
            QCOMPARE(b->toolTip(), reason);
        }
        for (const auto id : {ContainerButtonDispatcher::Id::Mox,
                              ContainerButtonDispatcher::Id::Tun}) {
            const auto st = dispatcher.stateOf(id, 0);
            QVERIFY(!st.available);
            QCOMPARE(st.reason, reason);
        }

        rig.model->txInhibit().notifyUserDigitalInputs(0x01);
        pump();
        QVERIFY(!rig.model->isTxInhibited());
        QVERIFY(mox->isEnabled() && tune->isEnabled());
        QCOMPARE(mox->toolTip(), moxTip);
        rig.model->txInhibit().detachRadioInput();
    }

    // ---- A remote window ---------------------------------------------------

    void remoteWindowShowsTheFaultAndItsReason()
    {
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("station.settings")));
        Rig rig;
        RadioModel& core = *rig.model;
        {
            StationServer server(&core, coreSettings,
                                 NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
            RadioModel window(RadioModel::Role::Remote);
            SettingsProxy proxy;
            StationClient client(&window, &proxy);
            auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
            auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
            stationEnd->linkTo(clientEnd);
            QSignalSpy completed(&client, &StationClient::handshakeComplete);
            client.startSession(clientEnd, server.token());
            server.acceptTransport(stationEnd);
            QVERIFY(completed.wait(5000) || !completed.isEmpty());
            QVERIFY(client.capabilities().txInhibitReasonVersion >= 1);

            // The window never holds inhibited with no reason, which would
            // show the generic refusal words for a moment.
            bool sawGeneric = false;
            const auto check = [&window, &sawGeneric]() {
                if (window.isTxInhibited() && window.txInhibitReason().isEmpty()) {
                    sawGeneric = true;
                }
            };
            connect(&window, &RadioModel::txInhibitedChanged, &window, check);
            connect(&window, &RadioModel::txInhibitReasonChanged, &window, check);

            feedGeneralRead(core, 5);
            QTRY_VERIFY(window.isTxInhibited());
            QTRY_COMPARE(window.txInhibitReason(), faultText(5));

            // The window's transmit buttons carry the same reason, first,
            // joined with the window's own transmit-permission reason.
            TxApplet applet(&window);
            QTRY_VERIFY(!applet.moxButton()->isEnabled());
            QVERIFY2(applet.moxButton()->toolTip().startsWith(faultText(5)),
                     qPrintable(applet.moxButton()->toolTip()));
            QVERIFY(!applet.tuneButton()->isEnabled());
            QVERIFY2(applet.tuneButton()->toolTip().startsWith(faultText(5)),
                     qPrintable(applet.tuneButton()->toolTip()));

            feedGeneralRead(core, 0);
            QTRY_VERIFY(!window.isTxInhibited());
            QTRY_VERIFY(window.txInhibitReason().isEmpty());
            // The fault leaves the buttons; any other reason stays.
            QTRY_VERIFY(!applet.moxButton()->toolTip().contains(faultText(5)));
            QVERIFY(!applet.tuneButton()->toolTip().contains(faultText(5)));
            QVERIFY2(!sawGeneric, "the window showed inhibited with no reason");
        }
    }
};

QTEST_MAIN(TestHl2IoBoardFault)
#include "tst_hl2_io_board_fault.moc"
