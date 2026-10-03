// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_amp_changeover_gate.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 77 fix round 2 (R-IOS-02, R-IOS-03, R-IOS-13): the
// Power Genius is never switched around a key, and a key never puts RF
// into it while it switches.
//
//   1. From any operate=0 or operate=1 written to the amplifier until its
//      status reports the commanded state, a key's RF waits at the RF-flow
//      gate (a MOX click, VOX, TUNE): the TX channel's RF gate stays shut
//      until the injected state edge, then opens.
//   2. Still waiting 1.5 s after the command, the key is stopped with
//      plain words, its RF never started.
//   3. With no Power Genius connected (or its link gone), nothing changes.
//   4. The Core's own Tuner Genius autotune: refused on the air (a TUNE
//      click and a tuner's hardware TUNE alike), ended without keying when
//      MOX comes on during its standby wait; the amplifier's restore is
//      sent only with nothing keyed, no PTT down and nothing unconfirmed,
//      and dropped when the amplifier changes state on its own.
//   5. Fix round 3: the Power Genius's OPERATE and STANDBY are refused
//      while a Tuner Genius cycle runs; an operate=1 that reaches the
//      amplifier anyway during the cycle's standby wait ends the cycle
//      without keying, the amplifier left as commanded; a FAULT report or
//      an error reply ends the changeover (a held key goes out barefoot);
//      the 1.5 s stop says how to transmit without the amplifier, and a
//      fresh key after it goes out once the amplifier reports; a CAT or
//      TCI release that never keyed is reported as a release.
//   6. Fix round 4: one amplifier command at a time (a refused second
//      command keeps the wait for the first; an unreadable reply code
//      refuses nothing); a relayed SmartSDR operate=1 is held while the
//      radio transmits; an ended cycle's failsafe leaves a new cycle alone;
//      an uncommanded operate under a barefoot key stops it.
//
// A local RadioModel with a mock radio, a TX channel wired as the connect
// path wires it (no WDSP channel: the RF gate is the observable), and the
// amplifier and tuner connections fed status lines.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 77 fix round 2, with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: fix round 3 cases (item 5 above). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: fix round 4 cases (item 6 above). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QSignalSpy>

#include <atomic>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/PgxlConnection.h"
#include "core/RadioConnection.h"
#include "core/TgxlConnection.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/safety/TxRefusal.h"
#include "OperatorWording.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// The amplifier and tuner connections here have no socket: their writes
// are refused by Qt ("device not open"), which is noise, not a finding.
QtMessageHandler g_previousHandler = nullptr;
void quietUnopenedSockets(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (msg.startsWith(QLatin1String("QIODevice::write")) && msg.contains(QLatin1String("device not open"))) {
        return;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, msg);
    }
}

const QString kAmpNotSwitched = QStringLiteral(
    "The amplifier did not answer. Put it in standby or disconnect it in Setup to transmit"
    " without it.");
const QString kNotStandbyForTune = QStringLiteral(
    "The amplifier did not go to standby for tuning. Put it in standby or disconnect it in"
    " Setup, then tune again.");

class MockConnection : public RadioConnection {
    Q_OBJECT
public:
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
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
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

struct Rig {
    RadioModel model;
    MockConnection conn;
    TxChannel tx{WdspEngine::kTxChannelId};
    std::atomic<int> rfOpens{0};
    // Every operate command the amplifier was sent: whether RF could flow
    // (MOX on or walking) and whether the test held a PTT down then.
    struct Sent {
        QString command;
        bool rf{false};
        bool pttDown{false};
    };
    QList<Sent> sent;
    bool pttDown{false};

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
        model.wireTxChannelKeyingForTest();
        tx.setRfGateObserverForTest([this](bool open) {
            if (open) {
                ++rfOpens;
            }
        });
        MoxController* mox = model.moxController();
        QObject::connect(model.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting,
                         [this, mox](const QString& frame) {
                             const int bar = frame.indexOf(QLatin1Char('|'));
                             const QString command = frame.mid(bar + 1);
                             if (command.startsWith(QLatin1String("operate="))) {
                                 sent.append({command,
                                              mox->isMox() || mox->state() != MoxState::Rx,
                                              pttDown});
                             }
                         });
    }
    ~Rig()
    {
        tx.setRfGateObserverForTest({});
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    PgxlConnection* amp() { return model.pgxlConnection(); }

    // The Power Genius on the air-side of this computer, operating.
    void connectAmp(const QString& state = QStringLiteral("OPERATE"))
    {
        amp()->injectLineForTesting(QStringLiteral("V3.8.9"));
        amp()->injectLineForTesting(QStringLiteral("R1|0|state=%1").arg(state));
    }
    void ampReports(const QString& state)
    {
        amp()->injectLineForTesting(QStringLiteral("S0|status state=%1").arg(state));
    }
    void connectTuner()
    {
        model.tgxlConnection()->injectLineForTesting(QStringLiteral("V1.2.17"));
    }

    int sentCount(const QString& command) const
    {
        int n = 0;
        for (const Sent& s : sent) {
            if (s.command == command) {
                ++n;
            }
        }
        return n;
    }

    bool keyed() { return model.moxController()->isMox(); }
    bool atRx() { return model.moxController()->state() == MoxState::Rx && !keyed(); }
};

enum class Key { Mox, Vox, Tune };

void keyWith(Rig& rig, Key key)
{
    MoxController* mox = rig.model.moxController();
    switch (key) {
    case Key::Mox:
        mox->setMox(true);
        break;
    case Key::Vox:
        rig.model.transmitModel().setVoxEnabled(true);
        rig.pttDown = true;
        mox->onVoxActive(true);
        break;
    case Key::Tune:
        rig.model.setTune(true);
        break;
    }
}

void unkeyWith(Rig& rig, Key key)
{
    MoxController* mox = rig.model.moxController();
    switch (key) {
    case Key::Mox:
        mox->setMox(false);
        break;
    case Key::Vox:
        rig.pttDown = false;
        mox->onVoxActive(false);
        rig.model.transmitModel().setVoxEnabled(false);
        break;
    case Key::Tune:
        rig.model.setTune(false);
        break;
    }
}

const char* keyName(Key key)
{
    switch (key) {
    case Key::Mox: return "MOX";
    case Key::Vox: return "VOX";
    case Key::Tune: return "TUNE";
    }
    return "?";
}

} // namespace

class TestAmpChangeoverGate : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        g_previousHandler = qInstallMessageHandler(quietUnopenedSockets);
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.pgxl.debug=false\n"
                                                        "nereus.tgxl.debug=false"));
    }
    void cleanupTestCase() { qInstallMessageHandler(g_previousHandler); }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // 1. Every key source holds its RF while the amplifier switches, and
    //    not a moment after the amplifier reports the commanded state.
    void aKeysRfWaitsForTheAmplifierToReportTheCommandedState()
    {
        for (const Key key : {Key::Mox, Key::Vox, Key::Tune}) {
            Rig rig;
            rig.connectAmp();
            QVERIFY(rig.amp()->isConnected());
            QVERIFY(rig.model.ampOperate());
            // A command from anyone (here the connection itself, as the
            // operator's own STANDBY sends it).
            rig.amp()->sendCommand(QStringLiteral("operate=0"));
            keyWith(rig, key);
            QTRY_VERIFY2(rig.keyed(), keyName(key));
            QTest::qWait(250);
            QVERIFY2(rig.keyed(), keyName(key));
            QVERIFY2(rig.rfOpens.load() == 0, keyName(key));
            QVERIFY2(!rig.tx.isRfGateOpen(), keyName(key));
            // The amplifier's standby arrives: the RF starts now.
            rig.ampReports(QStringLiteral("STANDBY"));
            QTRY_VERIFY2(rig.tx.isRfGateOpen(), keyName(key));
            QVERIFY(rig.keyed());
            unkeyWith(rig, key);
            QTRY_VERIFY2(rig.atRx(), keyName(key));
        }
    }

    // 1b. The same for operate=1 (the restore direction): RF waits for an
    //     operate-family state.
    void aKeysRfWaitsForTheAmplifiersOperate()
    {
        Rig rig;
        rig.connectAmp(QStringLiteral("STANDBY"));
        QVERIFY(!rig.model.ampOperate());
        rig.amp()->sendCommand(QStringLiteral("operate=1"));
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.keyed());
        QTest::qWait(250);
        QCOMPARE(rig.rfOpens.load(), 0);
        // A status still saying standby changes nothing.
        rig.ampReports(QStringLiteral("STANDBY"));
        QTest::qWait(100);
        QCOMPARE(rig.rfOpens.load(), 0);
        rig.ampReports(QStringLiteral("IDLE"));
        QTRY_VERIFY(rig.tx.isRfGateOpen());
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY(rig.atRx());
    }

    // 2. No report within 1.5 s of the command: the key is stopped, in
    //    plain words, and its RF never started.
    void aKeyStillHeldAfterTheBoundIsStoppedWithoutRf()
    {
        Rig rig;
        rig.connectAmp();
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        QElapsedTimer since;
        rig.amp()->sendCommand(QStringLiteral("operate=0"));
        since.start();
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.keyed());
        QTRY_VERIFY_WITH_TIMEOUT(!rig.keyed(), 4000);
        QVERIFY2(since.elapsed() >= 1400, qPrintable(QString::number(since.elapsed())));
        QTRY_VERIFY(rig.atRx());
        QCOMPARE(rig.rfOpens.load(), 0);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), kAmpNotSwitched);
        QCOMPARE(rig.model.lastTransmitStopReason().code, QByteArray("ampNotSwitched"));
        // A late report changes nothing: nothing keys again by itself.
        rig.ampReports(QStringLiteral("STANDBY"));
        QTest::qWait(100);
        QVERIFY(!rig.keyed());
        QCOMPARE(rig.rfOpens.load(), 0);
        QVERIFY(!rig.model.ampChangingOver());
        // Fix round 3: a fresh key now goes out (the amplifier reported).
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.tx.isRfGateOpen());
        QCOMPARE(rig.rfOpens.load(), 1);
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY(rig.atRx());
    }

    // 3. No Power Genius, a Power Genius with nothing commanded, or one
    //    whose link went down: the gate is as it always was.
    void withNoPowerGeniusChangingOverNothingChanges()
    {
        for (int kind = 0; kind < 3; ++kind) {
            Rig rig;
            if (kind >= 1) {
                rig.connectAmp();
            }
            if (kind == 2) {
                rig.amp()->sendCommand(QStringLiteral("operate=0"));
                rig.amp()->disconnect();
                QVERIFY(!rig.amp()->isConnected());
            }
            rig.model.moxController()->setMox(true);
            QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen(), 1000);
            rig.model.moxController()->setMox(false);
            QTRY_VERIFY(rig.atRx());
        }
    }

    // 4a. The Core's own autotune on the air (a TUNE click, the tuner's
    //     hardware TUNE): refused with the on-air words; nothing reaches
    //     the amplifier.
    void theLocalAutotuneIsRefusedOnTheAir()
    {
        for (const bool fromHardware : {false, true}) {
            Rig rig;
            rig.connectAmp();
            rig.connectTuner();
            QSignalSpy refused(&rig.model, &RadioModel::tuneRefused);
            rig.model.moxController()->setMox(true);
            QTRY_VERIFY(rig.keyed());
            rig.model.startTgxlAutotune(fromHardware);
            QVERIFY(!rig.model.isTgxlAutotuneInProgress());
            QCOMPARE(refused.count(), 1);
            QCOMPARE(refused.at(0).at(0).toString(), TxRefusals::radioOnAir().text);
            QCOMPARE(rig.sent.size(), 0);
            rig.model.moxController()->setMox(false);
            QTRY_VERIFY(rig.atRx());
            QCOMPARE(rig.sent.size(), 0);
        }
    }

    // 4b. MOX comes on while the Core's own cycle waits for the standby:
    //     the cycle ends without keying the tune carrier (the key's RF
    //     waits for the standby too), and the amplifier goes back only
    //     once the radio is in receive.
    void theLocalCycleEndsUnkeyedWhenMoxComesOnDuringItsWait()
    {
        Rig rig;
        rig.connectAmp();
        rig.connectTuner();
        rig.model.startTgxlAutotune(/*fromHardware=*/false);
        QVERIFY(rig.model.isTgxlAutotuneInProgress());
        QCOMPARE(rig.sentCount(QStringLiteral("operate=0")), 1);
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.keyed());
        QTest::qWait(100);
        QCOMPARE(rig.rfOpens.load(), 0);
        rig.ampReports(QStringLiteral("STANDBY"));
        QTRY_VERIFY(!rig.model.isTgxlAutotuneInProgress());
        QVERIFY(!rig.model.isTune());
        QTRY_VERIFY(rig.tx.isRfGateOpen());   // barefoot, the amplifier in standby
        QCOMPARE(rig.sentCount(QStringLiteral("operate=1")), 0);
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY(rig.atRx());
        QTRY_COMPARE(rig.sentCount(QStringLiteral("operate=1")), 1);
        for (const Rig::Sent& s : std::as_const(rig.sent)) {
            QVERIFY2(!s.rf, qPrintable(s.command));
        }
    }

    // 4c. The restore after a cycle waits for every PTT source, not only
    //     for MOX to read receive: a mic held through the tune carrier
    //     keys when the carrier ends, and the amplifier stays in standby
    //     (barefoot) for it; it goes back once the mic is released.
    void theRestoreWaitsForEveryPttSource()
    {
        Rig rig;
        rig.connectAmp();
        rig.connectTuner();
        MoxController* mox = rig.model.moxController();
        rig.model.startTgxlAutotune(/*fromHardware=*/false);
        rig.ampReports(QStringLiteral("STANDBY"));
        QTRY_VERIFY(rig.model.isTune() && rig.keyed());
        QTRY_VERIFY(rig.tx.isRfGateOpen());
        rig.pttDown = true;
        mox->onMicPttFromRadio(true);   // held off under the tune carrier
        rig.model.setTune(false);       // the cycle ends
        QTRY_VERIFY(!rig.model.isTgxlAutotuneInProgress());
        QTest::qWait(200);
        QCOMPARE(rig.sentCount(QStringLiteral("operate=1")), 0);
        rig.pttDown = false;
        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(rig.atRx());
        QTRY_COMPARE(rig.sentCount(QStringLiteral("operate=1")), 1);
        for (const Rig::Sent& s : std::as_const(rig.sent)) {
            QVERIFY2(!s.rf && !s.pttDown, qPrintable(s.command));
        }
    }

    // 4d. An owed restore is dropped when the amplifier changes state on
    //     its own (its front panel): the operator's choice stands.
    void anEdgeNobodyCommandedDropsTheOwedRestore()
    {
        Rig rig;
        rig.connectAmp();
        rig.connectTuner();
        MoxController* mox = rig.model.moxController();
        rig.model.startTgxlAutotune(/*fromHardware=*/false);
        rig.ampReports(QStringLiteral("STANDBY"));
        QTRY_VERIFY(rig.model.isTune() && rig.keyed());
        rig.pttDown = true;
        mox->onMicPttFromRadio(true);
        rig.model.setTune(false);
        QTRY_VERIFY(!rig.model.isTgxlAutotuneInProgress());
        // The front panel: operate, then standby again.
        rig.ampReports(QStringLiteral("OPERATE"));
        rig.ampReports(QStringLiteral("STANDBY"));
        rig.pttDown = false;
        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(rig.atRx());
        QTest::qWait(200);
        QCOMPARE(rig.sentCount(QStringLiteral("operate=1")), 0);
    }

    // 5a. Fix round 3: while the Core's own cycle runs, the Power Genius's
    //     OPERATE and STANDBY are refused with the tuning words (a local
    //     click); the wait is announced when the cycle starts and ends.
    void theAmplifierSwitchWaitsForATunerCycle()
    {
        Rig rig;
        rig.connectAmp();
        rig.connectTuner();
        QSignalSpy waitChanged(&rig.model, &RadioModel::pgxlSwitchWaitChanged);
        QSignalSpy refused(&rig.model, &RadioModel::accessoryRequestRefused);
        QVERIFY(!rig.model.pgxlSwitchRefusal(nullptr));
        rig.model.startTgxlAutotune(/*fromHardware=*/false);
        QVERIFY(rig.model.isTgxlAutotuneInProgress());
        QVERIFY(waitChanged.count() >= 1);
        QVERIFY(rig.model.pgxlSwitchWaitsForTuner());
        QString reason;
        QVERIFY(rig.model.pgxlSwitchRefusal(&reason));
        QCOMPARE(reason, RadioModel::tunerTuningReason());
        QVERIFY(rig.model.refuseLocalAccessorySwitchOnAir(QStringLiteral("pgxl")));
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(1).toString(), RadioModel::tunerTuningReason());
        // The Tuner Genius and RF-Kit switches keep their own rule.
        QVERIFY(!rig.model.refuseLocalAccessorySwitchOnAir(QStringLiteral("rfkit")));
        QCOMPARE(rig.sentCount(QStringLiteral("operate=0")), 1);
        QCOMPARE(rig.sentCount(QStringLiteral("operate=1")), 0);
        // The cycle ends (the amplifier never reports): the wait is over.
        const int before = waitChanged.count();
        QTRY_VERIFY_WITH_TIMEOUT(!rig.model.isTgxlAutotuneInProgress(), 4000);
        QVERIFY(waitChanged.count() > before);
        QVERIFY(!rig.model.pgxlSwitchWaitsForTuner());
        // Round 4: its operate=0 is still unconfirmed: still switching.
        QVERIFY(rig.model.pgxlSwitchRefusal(&reason));
        QCOMPARE(reason, RadioModel::ampStillSwitchingReason());
        rig.ampReports(QStringLiteral("STANDBY"));   // then the restore goes
        QTRY_COMPARE(rig.sentCount(QStringLiteral("operate=1")), 1);
        rig.ampReports(QStringLiteral("OPERATE"));
        QTRY_VERIFY(!rig.model.pgxlSwitchRefusal(nullptr));
    }

    // 5b. Fix round 3: an operate=1 that reaches the amplifier during the
    //     cycle's standby wait (anything but a window, which is refused):
    //     whether the amplifier's standby comes first or it reports only
    //     operating, no tune carrier is keyed, the cycle ends, and the
    //     amplifier is left as commanded (no restore of the cycle's own).
    void anOperateDuringTheStandbyWaitEndsTheCycleUnkeyed()
    {
        for (const bool standbyFirst : {true, false}) {
            Rig rig;
            rig.connectAmp();
            rig.connectTuner();
            QSignalSpy refused(&rig.model, &RadioModel::tuneRefused);
            rig.model.startTgxlAutotune(/*fromHardware=*/false);
            QVERIFY(rig.model.isTgxlAutotuneInProgress());
            QCOMPARE(rig.sentCount(QStringLiteral("operate=0")), 1);
            rig.amp()->sendCommand(QStringLiteral("operate=1"));   // the injected command
            if (standbyFirst) {
                rig.ampReports(QStringLiteral("STANDBY"));
                QTRY_VERIFY(!rig.model.isTgxlAutotuneInProgress());
                rig.ampReports(QStringLiteral("OPERATE"));
            } else {
                rig.ampReports(QStringLiteral("IDLE"));
                QTRY_VERIFY_WITH_TIMEOUT(!rig.model.isTgxlAutotuneInProgress(), 4000);
            }
            QVERIFY2(!rig.model.isTune(), standbyFirst ? "standby first" : "idle only");
            QVERIFY(!rig.keyed());
            QCOMPARE(rig.rfOpens.load(), 0);
            QCOMPARE(refused.count(), 1);
            QCOMPARE(refused.at(0).at(0).toString(), kNotStandbyForTune);
            QVERIFY(OperatorWording::isPlain(kNotStandbyForTune));
            QTRY_VERIFY(rig.model.ampOperate());
            QVERIFY(!rig.model.ampChangingOver());
            QTest::qWait(100);
            QCOMPARE(rig.sentCount(QStringLiteral("operate=1")), 1);   // only the injected one
            QCOMPARE(rig.rfOpens.load(), 0);
        }
    }

    // 5c. Fix round 3: an amplifier in FAULT while operate=1 is unconfirmed
    //     never reaches it: the changeover ends at the report and a held
    //     key goes out barefoot, well inside the 1.5 s bound.
    void aFaultWhileOperateIsOutstandingReleasesTheGate()
    {
        Rig rig;
        rig.connectAmp(QStringLiteral("STANDBY"));
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.amp()->sendCommand(QStringLiteral("operate=1"));
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.keyed());
        QTest::qWait(150);
        QCOMPARE(rig.rfOpens.load(), 0);
        rig.ampReports(QStringLiteral("FAULT"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen(), 500);
        QVERIFY(!rig.model.ampChangingOver());
        QVERIFY(!rig.model.ampOperate());
        QTest::qWait(1600);   // past the bound: nothing stops it
        QVERIFY(rig.keyed());
        QCOMPARE(stopped.count(), 0);
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY(rig.atRx());
    }

    // 5d. Fix round 3: an error reply to the operate command ends the
    //     changeover; a held key goes on with the amplifier as it last
    //     reported (here standby: barefoot). A reply to another command,
    //     or an accepted one, does not.
    void anErrorReplyToTheOperateCommandReleasesTheGate()
    {
        Rig rig;
        rig.connectAmp(QStringLiteral("STANDBY"));
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        const quint32 seq = rig.amp()->sendCommand(QStringLiteral("operate=1"));
        QVERIFY(seq != 0);
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.keyed());
        rig.amp()->injectLineForTesting(QStringLiteral("R%1|50000016|").arg(seq + 100));
        rig.amp()->injectLineForTesting(QStringLiteral("R%1|0|").arg(seq));
        QTest::qWait(150);
        QVERIFY(rig.model.ampChangingOver());
        QCOMPARE(rig.rfOpens.load(), 0);
        rig.amp()->injectLineForTesting(QStringLiteral("R%1|50000016|").arg(seq));
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen(), 500);
        QVERIFY(!rig.model.ampChangingOver());
        QTest::qWait(1600);
        QVERIFY(rig.keyed());
        QCOMPARE(stopped.count(), 0);
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY(rig.atRx());
    }

    // 5e. Fix round 3: the 1.5 s stop's words are plain and say how to
    //     transmit without the amplifier.
    void theStopsWordsSayHowToTransmitWithoutTheAmplifier()
    {
        QCOMPARE(RadioModel::ampNotSwitchedText(), kAmpNotSwitched);
        QCOMPARE(RadioModel::ampNotStandbyForTuneText(), kNotStandbyForTune);
        QVERIFY(OperatorWording::isPlain(kAmpNotSwitched));
    }

    // 5f. Fix round 3 (minor C): a CAT or TCI press that never keyed (MOX
    //     already on from a click) is reported as a release when it lets
    //     go, so an owed amplifier restore is retried then.
    void aCatOrTciReleaseThatNeverKeyedIsReported()
    {
        for (const bool cat : {true, false}) {
            Rig rig;
            MoxController* mox = rig.model.moxController();
            mox->setMox(true);
            QTRY_VERIFY(rig.keyed());
            if (cat) { mox->onCatPtt(true); } else { mox->onTciPtt(true); }
            QVERIFY(mox->anyPttSourceHeld());
            QSignalSpy released(mox, &MoxController::pttSourcesReleased);
            if (cat) { mox->onCatPtt(false); } else { mox->onTciPtt(false); }
            QVERIFY(!mox->anyPttSourceHeld());
            QVERIFY2(released.count() >= 1, cat ? "CAT" : "TCI");
            mox->setMox(false);
            QTRY_VERIFY(rig.atRx());
        }
    }

    // 6a. Fix round 4 (Important 1): one amplifier command at a time from a
    //     window (still switching words; standby stays open while operate=1
    //     is unconfirmed), and a refusal of a second command never ends the
    //     wait for the first: the held key stays held until the amp reports.
    void aRefusedSecondCommandKeepsWaitingForTheFirst()
    {
        Rig rig;
        rig.connectAmp(QStringLiteral("STANDBY"));
        QSignalSpy refused(&rig.model, &RadioModel::accessoryRequestRefused);
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        const quint32 first = rig.amp()->sendCommand(QStringLiteral("operate=1"));
        rig.amp()->injectLineForTesting(QStringLiteral("R%1|0|").arg(first));
        QVERIFY(rig.model.ampChangingOver());
        QVERIFY(rig.model.ampOperateUnconfirmed());
        QString reason;
        QVERIFY(rig.model.pgxlSwitchRefusal(&reason));
        QCOMPARE(reason, RadioModel::ampStillSwitchingReason());
        QVERIFY(OperatorWording::isPlain(reason));
        QVERIFY(rig.model.refuseLocalAccessorySwitchOnAir(QStringLiteral("pgxl")));
        QCOMPARE(refused.last().at(1).toString(), RadioModel::ampStillSwitchingReason());
        // Standby, the way out, is open.
        QVERIFY(!rig.model.pgxlSwitchRefusal(nullptr, /*standbyRequested=*/true));
        // A second operate=1 from outside the windows, refused by the amp.
        const quint32 second = rig.amp()->sendCommand(QStringLiteral("operate=1"));
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.keyed());
        rig.amp()->injectLineForTesting(QStringLiteral("R%1|50000016|").arg(second));
        QTest::qWait(200);
        QVERIFY(rig.model.ampChangingOver());
        QCOMPARE(rig.rfOpens.load(), 0);
        QVERIFY(!rig.tx.isRfGateOpen());
        rig.ampReports(QStringLiteral("OPERATE"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen(), 500);
        QCOMPARE(stopped.count(), 0);
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY(rig.atRx());
    }

    // 6b. Fix round 4 (Minor 4): a reply whose code cannot be read refuses
    //     nothing; the wait goes on.
    void anUnreadableReplyCodeDoesNotEndTheChangeover()
    {
        Rig rig;
        rig.connectAmp(QStringLiteral("STANDBY"));
        const quint32 seq = rig.amp()->sendCommand(QStringLiteral("operate=1"));
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.keyed());
        rig.amp()->injectLineForTesting(QStringLiteral("R%1|zz|").arg(seq));
        QTest::qWait(200);
        QVERIFY(rig.model.ampChangingOver());
        QCOMPARE(rig.rfOpens.load(), 0);
        rig.ampReports(QStringLiteral("IDLE"));
        QTRY_VERIFY(rig.tx.isRfGateOpen());
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY(rig.atRx());
    }

    // 6c. Fix round 4 (from Out of scope): a relayed SmartSDR operate=1
    //     while the radio transmits is held and sent after the unkey;
    //     operate=0 goes at once and drops a held operate=1.
    void aRelayedOperateIsHeldUntilTheRadioUnkeys()
    {
        Rig rig;
        rig.connectAmp(QStringLiteral("STANDBY"));
        MoxController* mox = rig.model.moxController();
        mox->setMox(true);
        QTRY_VERIFY(rig.tx.isRfGateOpen());
        rig.model.forwardAmplifierSet(QStringLiteral("PowerGeniusXL"), QStringLiteral("operate"),
                                      QStringLiteral("1"));
        QVERIFY(rig.model.isRelayedPgxlOperateHeld());
        QTest::qWait(100);
        QCOMPARE(rig.sentCount(QStringLiteral("operate=1")), 0);
        mox->setMox(false);
        QTRY_VERIFY(rig.atRx());
        QTRY_COMPARE(rig.sentCount(QStringLiteral("operate=1")), 1);
        QVERIFY(!rig.model.isRelayedPgxlOperateHeld());
        for (const Rig::Sent& s : std::as_const(rig.sent)) {
            QVERIFY2(!s.rf, qPrintable(s.command));
        }
        rig.ampReports(QStringLiteral("OPERATE"));
        QTRY_VERIFY(!rig.model.ampChangingOver());

        // Standby under the key goes at once, and drops a held operate=1.
        rig.ampReports(QStringLiteral("STANDBY"));
        mox->setMox(true);
        QTRY_VERIFY(rig.tx.isRfGateOpen());
        rig.model.forwardAmplifierSet(QStringLiteral("PowerGeniusXL"), QStringLiteral("operate"),
                                      QStringLiteral("1"));
        QVERIFY(rig.model.isRelayedPgxlOperateHeld());
        rig.model.forwardAmplifierSet(QStringLiteral("PowerGeniusXL"), QStringLiteral("operate"),
                                      QStringLiteral("0"));
        QVERIFY(!rig.model.isRelayedPgxlOperateHeld());
        QCOMPARE(rig.sentCount(QStringLiteral("operate=0")), 1);
        rig.ampReports(QStringLiteral("STANDBY"));
        mox->setMox(false);
        QTRY_VERIFY(rig.atRx());
        QTest::qWait(100);
        QCOMPARE(rig.sentCount(QStringLiteral("operate=1")), 1);
    }

    // 6d. Fix round 4 (from Out of scope): the 1.5 s standby failsafe of a
    //     cycle that ended never ends a newer cycle.
    void anEndedCyclesFailsafeLeavesANewCycleAlone()
    {
        Rig rig;
        rig.connectAmp();
        rig.connectTuner();
        QElapsedTimer since;
        since.start();
        rig.model.startTgxlAutotune(/*fromHardware=*/false);
        rig.ampReports(QStringLiteral("STANDBY"));
        QTRY_VERIFY(rig.model.isTune() && rig.keyed());
        rig.model.setTune(false);                   // the first cycle ends
        QTRY_VERIFY(!rig.model.isTgxlAutotuneInProgress());
        QTRY_VERIFY(rig.atRx());
        QTRY_COMPARE(rig.sentCount(QStringLiteral("operate=1")), 1);   // its restore
        rig.ampReports(QStringLiteral("OPERATE"));
        QTRY_VERIFY(!rig.model.ampChangingOver());
        QVERIFY2(since.elapsed() < 600, qPrintable(QString::number(since.elapsed())));
        while (since.elapsed() < 600) {   // its failsafe then lands at about 2.1 s
            QTest::qWait(20);
        }
        rig.model.startTgxlAutotune(/*fromHardware=*/false);   // the second
        QVERIFY(rig.model.isTgxlAutotuneInProgress());
        // Past the first cycle's 1.5 s, before the second's.
        while (since.elapsed() < 1650) {
            QTest::qWait(20);
        }
        QVERIFY(rig.model.isTgxlAutotuneInProgress());
        rig.ampReports(QStringLiteral("STANDBY"));
        QTRY_VERIFY(rig.model.isTune() && rig.keyed());
        rig.model.setTune(false);
        QTRY_VERIFY(!rig.model.isTgxlAutotuneInProgress());
        QTRY_VERIFY(rig.atRx());
    }

    // 6e. Fix round 4 (from Out of scope): a barefoot key past a FAULT is
    //     stopped, in plain words, when the amplifier goes to operate by
    //     itself.
    void anUncommandedOperateUnderAKeyStopsIt()
    {
        Rig rig;
        rig.connectAmp(QStringLiteral("FAULT"));
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.tx.isRfGateOpen());   // barefoot: no changeover
        rig.ampReports(QStringLiteral("FAULT"));
        QTest::qWait(50);
        QVERIFY(rig.keyed());
        rig.ampReports(QStringLiteral("OPERATE"));
        QTRY_VERIFY(!rig.keyed());
        QTRY_VERIFY(rig.atRx());
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), RadioModel::ampOperatedUnderKeyText());
        QVERIFY(OperatorWording::isPlain(RadioModel::ampOperatedUnderKeyText()));
        QCOMPARE(rig.model.lastTransmitStopReason().code,
                 QByteArray(RadioModel::kAmpOperatedUnderKeyStopCode));
        // Off the air, the same edge stops nothing.
        rig.ampReports(QStringLiteral("STANDBY"));
        rig.ampReports(QStringLiteral("OPERATE"));
        QTest::qWait(50);
        QCOMPARE(stopped.count(), 1);
    }
};

QTEST_MAIN(TestAmpChangeoverGate)
#include "tst_amp_changeover_gate.moc"
