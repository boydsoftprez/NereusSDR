// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_transmit_state_facade.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 39 (D14, R-IOS-13, R-IOS-21): the mirrored
// `txState` object (TransmitState). No test keys a real radio: the model
// has a mock connection or none, and the transmit channel wrapper has no
// WDSP channel behind it.
//
// On a model of its own (the Task 38 rig):
//   1. Unbound, it reads a Core with nothing keyed and nothing stopped.
//   2. A key publishes keyed, who keyed, how and when (the Core's clock),
//      the transmit slice and the time left; the unkey clears them.
//   3. While keyed the meters are read ten times a second from the pump
//      and a reading that changed is sent; one that did not is not.
//   4. Unkeyed the pump is stopped and only a change of the radio's power
//      readings is sent.
//   5. The time-out advances stopSerial once, reason timeOut, with the
//      limit in the words; the Core's StopAllTx that made it counts no
//      second time.
//   6. A StopAllTx with no reason of its own is the Core's own: station.
//   7. A starvation stop (the reason Task 37 records) advances stopSerial
//      once; a later reason for the same key changes nothing; the next key
//      can be stopped again. Nothing counts before any key.
//   8. Every stop text is plain operator words and says "the Core".
//   9. A window's copy takes the Core's values and nothing else; clearing
//      it keeps the last stop.
//  10. Control logging lane: an unkey logs one line with the key's
//      leveler, leveler gain, ALC, ALC gain and compression peaks from the
//      readings the pump took, "none" for a reading it did not have; a
//      second key's peaks start fresh.
//
// Through the Core (StationServer, over the loopback):
//  10. Only a device whose hello declared remoteTx gets txStateVersion and
//      the object.
//  11. The keyed device's link lost: stopSerial once, reason linkLost, the
//      device named; the other device is sent it.
//  12. The keyed device revoked: stopSerial once, reason revoked.
//  13. (Merge of Tasks 37 to 39) The watchdog's stop of a link gone quiet,
//      the starvation's stop and the time-out's stop each reach the keyed
//      phone and the other device with their own reason and words.
//  14. (Unkey drain review) The keyed device revoked is the Core's stop:
//      with transmit audio queued on a Protocol 1 or Protocol 2 connection
//      its unkey never waits for the send ring (G-05) before the radio
//      reaches receive.
//  15. (Unkey drain review) So is the keyed device's lost link, whether its
//      link drops or the same device connects again over a new one.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 39 (D14, R-IOS-13,
//               R-IOS-21), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: merge of Tasks 37 to 39: the watchdog's, the starvation's
//               and the time-out's stops over the wire. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13): the
//               radio keeps transmit after its press. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: Unkey drain review: the Core's stops (a revoke, a lost
//               link, the same device connecting again) never wait for the
//               send ring, on Protocol 1 and Protocol 2. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-01: Control logging lane: the unkey's stage peaks line, and a
//               second key's peaks start fresh. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QElapsedTimer>
#include <QScopeGuard>

#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/meters/TxMeterPump.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/safety/TxTimeOutTimer.h"
#include "core/session/TransmitStateFacade.h"

#include <functional>
#include <memory>
#include <vector>

namespace {

constexpr qint64 kSecond = 1000;

void pumpEvents(int passes = 8)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// Fires the meter pump's timer once, as its 100 ms passing would.
void tickMeters(TransmitState& state)
{
    QTimer* timer = state.meterPump()->findChild<QTimer*>();
    QVERIFY(timer != nullptr);
    QVERIFY(timer->isActive());
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
}

// Records MOX and relay writes; never keys a real radio.
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

// A local model on 20 m USB with a mock connection and a TX channel wrapper
// with no WDSP channel behind it, the MOX walk driven by processEvents, the
// time-out and the transmit state on the test's clock.
struct Rig {
    RadioModel model;
    MockConnection conn;
    TxChannel tx{WdspEngine::kTxChannelId};
    qint64 nowMs{0};
    TransmitState state;

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
        model.txTimeOutTimer()->setClock([this]() { return nowMs; });
        model.txTimeOutTimer()->setPinger([](const QString&, std::function<void(bool)> done) {
            done(true);
        });
        state.setClock([this]() { return nowMs; });
        state.bind(&model);
    }
    ~Rig()
    {
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    void keyedBy(const QString& kind)
    {
        RadioModel::KeyedBy who;
        who.deviceId = QByteArrayLiteral("device-1");
        who.deviceName = QStringLiteral("Test device");
        who.deviceKind = kind;
        who.trigger = QByteArrayLiteral("screen");
        who.epoch = 1;
        model.setKeyedBy(who);
    }

    void key(const QString& kind = QStringLiteral("phone"))
    {
        keyedBy(kind);
        model.moxController()->setMox(true);
        pumpEvents();
    }

    void unkey()
    {
        model.moxController()->setMox(false);
        pumpEvents();
        model.setKeyedBy({});
        pumpEvents();
    }

    // The time-out's one-second tick, on every whole second after now.
    void tickUntil(qint64 toMs)
    {
        for (qint64 t = (nowMs / kSecond + 1) * kSecond; t <= toMs; t += kSecond) {
            nowMs = t;
            model.txTimeOutTimer()->tick();
            pumpEvents(2);
        }
    }
};

// Two devices on one Core that allows remote transmit, both declaring
// remoteTx.
struct Pair {
    Core core;
    Device a{QStringLiteral("Grant's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone")};
    Device b{QStringLiteral("iPad"), QStringLiteral("tablet")};
    LoopbackTransport* appA{nullptr};
    LoopbackTransport* appB{nullptr};

    Pair()
    {
        allowTransmit(core);
        core.pair(a);
        core.pair(b);
        appA = core.signIn(a, kTransmitter);
        appB = core.signIn(b, kTransmitter);
    }

    TransmitState& state() { return *core.server->transmitState(); }

    bool keyA()
    {
        const QJsonObject r =
            core.invoke(appA, "tx.key", {utf8("trigger", QStringLiteral("screen"))});
        return r.value(QStringLiteral("accepted")).toBool(false);
    }
};

bool hasObject(const LoopbackTransport* app, const QString& key)
{
    for (const QJsonObject& o : ofType(app->received(), QStringLiteral("object.create"))) {
        if (o.value(QStringLiteral("key")).toString() == key) {
            return true;
        }
    }
    return false;
}

// The stop both devices were last sent on `txState`.
void verifyStopSent(const LoopbackTransport* app, const QString& reason, const QString& text)
{
    QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                        QStringLiteral("stopSerial")).toInteger(), 1);
    QCOMPARE(latest(app->received(), QStringLiteral("txState"),
                    QStringLiteral("stopReason")).toString(), reason);
    QCOMPARE(latest(app->received(), QStringLiteral("txState"),
                    QStringLiteral("stopText")).toString(), text);
    QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                        QStringLiteral("keyed")).toBool(), false);
}

} // namespace

// Control logging lane: collects the stage peak lines.
class StagePeakLog {
public:
    StagePeakLog()
    {
        s_lines.clear();
        s_previous = qInstallMessageHandler(&StagePeakLog::handle);
    }
    ~StagePeakLog() { qInstallMessageHandler(s_previous); }
    QStringList lines() const { return s_lines; }

private:
    static void handle(QtMsgType type, const QMessageLogContext& context, const QString& msg)
    {
        if (msg.startsWith(QStringLiteral("TX stage peaks"))) {
            s_lines.append(msg);
            return;
        }
        if (s_previous) {
            s_previous(type, context, msg);
        }
    }
    static inline QStringList s_lines;
    static inline QtMessageHandler s_previous = nullptr;
};

// Watches the log for the MoxController's send-ring wait lines: a wait
// the ceiling ended, or one a stop cut short. Either means an unkey began
// to wait for the send ring.
class SendRingWaitLog {
public:
    SendRingWaitLog()
    {
        s_line.clear();
        s_previous = qInstallMessageHandler(&SendRingWaitLog::handle);
    }
    ~SendRingWaitLog() { qInstallMessageHandler(s_previous); }
    bool seen() const { return !s_line.isEmpty(); }
    QString line() const { return s_line; }

private:
    static void handle(QtMsgType type, const QMessageLogContext& context, const QString& msg)
    {
        if (msg.contains(QStringLiteral("not waiting for the send ring"))
            || msg.contains(QStringLiteral("send ring did not drain"))) {
            s_line = msg;
        }
        if (s_previous) {
            s_previous(type, context, msg);
        }
    }
    static inline QString s_line;
    static inline QtMessageHandler s_previous{nullptr};
};

class TstTransmitStateFacade : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ---- On a model of its own ------------------------------------------------

    void unboundItReadsACoreWithNothingKeyed()
    {
        TransmitState state;
        QVERIFY(!state.keyed());
        QVERIFY(!state.tuning());
        QVERIFY(!state.twoTone());
        QCOMPARE(state.txSliceId(), -1);
        QVERIFY(state.keyedByName().isEmpty());
        QVERIFY(state.keyedByKind().isEmpty());
        QVERIFY(state.keyedTrigger().isEmpty());
        QCOMPARE(state.keyedSinceMs(), qint64(0));
        QCOMPARE(state.timeOutRemainingSeconds(), -1);
        QCOMPARE(state.forwardPowerWatts(), 0.0);
        QCOMPARE(state.reflectedPowerWatts(), 0.0);
        QCOMPARE(state.swr(), 1.0);
        QCOMPARE(state.alcDb(), TxMeterReadings::kNoReadingDb);
        QCOMPARE(state.micLevelDb(), TxMeterReadings::kNoReadingDb);
        QVERIFY(!state.txEnding());
        QVERIFY(state.stopReason().isEmpty());
        QVERIFY(state.stopText().isEmpty());
        QCOMPARE(state.stopSerial(), quint32(0));
        QVERIFY(!state.meterPump()->isRunning());
    }

    void aKeyPublishesWhoKeyedHowAndWhen()
    {
        Rig rig;
        QSignalSpy changed(&rig.state, &TransmitState::stateChanged);
        rig.nowMs = 5000;
        rig.key(QStringLiteral("phone"));
        QVERIFY(rig.state.keyed());
        QCOMPARE(rig.state.keyedByName(), QStringLiteral("Test device"));
        QCOMPARE(rig.state.keyedByKind(), QStringLiteral("phone"));
        QCOMPARE(rig.state.keyedTrigger(), QStringLiteral("screen"));
        QCOMPARE(rig.state.keyedSinceMs(), qint64(5000));
        QCOMPARE(rig.state.txSliceId(), rig.model.txSliceArbiter()->txBoundSliceId());
        QVERIFY(!rig.state.tuning());
        QVERIFY(!rig.state.twoTone());
        // The phone's time-out: 180 s from the key.
        QCOMPARE(rig.state.timeOutRemainingSeconds(), 180);
        QVERIFY(rig.state.meterPump()->isRunning());
        QVERIFY(changed.count() >= 1);

        rig.unkey();
        QVERIFY(!rig.state.keyed());
        QVERIFY(rig.state.keyedByName().isEmpty());
        QVERIFY(rig.state.keyedByKind().isEmpty());
        QVERIFY(rig.state.keyedTrigger().isEmpty());
        QCOMPARE(rig.state.keyedSinceMs(), qint64(0));
        QCOMPARE(rig.state.timeOutRemainingSeconds(), -1);
        QVERIFY(!rig.state.meterPump()->isRunning());
        // An unkey the device asked for is no stop.
        QCOMPARE(rig.state.stopSerial(), quint32(0));
    }

    void tuneIsKeyedAndTuning()
    {
        Rig rig;
        rig.keyedBy(QStringLiteral("phone"));
        rig.model.setTune(true);
        pumpEvents();
        QVERIFY(rig.state.keyed());
        QVERIFY(rig.state.tuning());
        rig.model.setTune(false);
        pumpEvents();
        QVERIFY(!rig.state.tuning());
    }

    void theTimeLeftCountsDownWithTheMeters()
    {
        Rig rig;
        rig.key();
        QCOMPARE(rig.state.timeOutRemainingSeconds(), 180);
        rig.nowMs = 60'500;   // 60.5 s keyed: 119.5 s left, 120 whole
        tickMeters(rig.state);
        QCOMPARE(rig.state.timeOutRemainingSeconds(), 120);
        rig.unkey();
    }

    void whileKeyedTheMetersAreReadTenTimesASecond()
    {
        Rig rig;
        int reads = 0;
        rig.state.meterPump()->setSource([&reads]() {
            ++reads;
            TxMeterReadings r;
            r.forwardPowerWatts = reads;           // changes every read
            r.reflectedPowerWatts = 1.0;
            r.swr = 1.2;
            r.alcDb = -3.0;
            r.micLevelDb = -12.0;
            return r;
        });
        QSignalSpy meters(&rig.state, &TransmitState::metersChanged);
        rig.key();
        const int atKey = reads;
        QVERIFY2(atKey >= 1, "the key did not read the meters at once");
        const int sentAtKey = meters.count();
        for (int i = 0; i < 10; ++i) {
            tickMeters(rig.state);
        }
        // Ten ticks (one second): ten readings, each different, each sent.
        QCOMPARE(reads - atKey, 10);
        QCOMPARE(meters.count() - sentAtKey, 10);
        QCOMPARE(rig.state.forwardPowerWatts(), double(reads));
        QCOMPARE(rig.state.alcDb(), -3.0);
        QCOMPARE(rig.state.micLevelDb(), -12.0);

        // A reading that did not change is not sent again.
        rig.state.meterPump()->setSource([]() {
            TxMeterReadings r;
            r.forwardPowerWatts = 50.0;
            return r;
        });
        tickMeters(rig.state);
        const int afterChange = meters.count();
        tickMeters(rig.state);
        tickMeters(rig.state);
        QCOMPARE(meters.count(), afterChange);
        rig.unkey();
    }

    // Control logging lane: the unkey logs the key's stage peaks once.
    void anUnkeyLogsTheKeysStagePeaks()
    {
        Rig rig;
        int reads = 0;
        rig.state.meterPump()->setSource([&reads]() {
            ++reads;
            TxMeterReadings r;
            r.levelerDb = -20.0 + reads;              // rises with each read
            r.levelerGainDb = reads == 2 ? 14.5 : 3.0;
            r.alcDb = -1.5;
            r.compressionDb = reads == 3 ? 6.0 : 1.0;
            return r;                                  // no ALC gain reading
        });
        StagePeakLog log;
        rig.key();
        for (int i = 0; i < 3; ++i) {
            tickMeters(rig.state);
        }
        QCOMPARE(log.lines().size(), 0);
        rig.unkey();
        const int taken = reads;
        QVERIFY(taken >= 4);
        QCOMPARE(log.lines(),
                 QStringList{QStringLiteral(
                     "TX stage peaks for key 1 (%1 readings): leveler %2 dB, leveler gain "
                     "14.5 dB, ALC -1.5 dB, ALC gain none, compression 6.0 dB")
                                 .arg(taken)
                                 .arg(-20.0 + taken, 0, 'f', 1)});
        // The next unkey with no key between logs nothing more.
        rig.unkey();
        QCOMPARE(log.lines().size(), 1);

        // A second key's peaks start fresh: lower readings than the first
        // key's are its peaks, and the ALC gain it reads shows.
        rig.state.meterPump()->setSource([&reads]() {
            ++reads;
            TxMeterReadings r;
            r.levelerDb = -30.0;
            r.levelerGainDb = 2.0;
            r.alcDb = -3.0;
            r.alcGainDb = 1.0;
            r.compressionDb = 0.5;
            return r;
        });
        reads = 0;
        rig.key();
        for (int i = 0; i < 2; ++i) {
            tickMeters(rig.state);
        }
        rig.unkey();
        QVERIFY(reads >= 3);
        QCOMPARE(log.lines().size(), 2);
        QCOMPARE(log.lines().last(),
                 QStringLiteral("TX stage peaks for key 2 (%1 readings): leveler -30.0 dB, "
                                "leveler gain 2.0 dB, ALC -3.0 dB, ALC gain 1.0 dB, compression "
                                "0.5 dB")
                     .arg(reads));
    }

    void unkeyedOnlyAChangeOfTheRadiosPowerIsSent()
    {
        Rig rig;
        rig.key();
        rig.unkey();
        QVERIFY(!rig.state.meterPump()->isRunning());
        QSignalSpy meters(&rig.state, &TransmitState::metersChanged);
        // The same values again: nothing.
        rig.model.radioStatus().setForwardPower(rig.model.radioStatus().forwardPowerWatts());
        pumpEvents();
        QCOMPARE(meters.count(), 0);
        // A change: sent.
        rig.model.radioStatus().setForwardPower(7.0);
        QCOMPARE(meters.count(), 1);
        QCOMPARE(rig.state.forwardPowerWatts(), 7.0);
        rig.model.radioStatus().setForwardPower(0.0);
        QCOMPARE(meters.count(), 2);
    }

    void aTimeOutAdvancesStopSerialOnceWithTheLimit()
    {
        Rig rig;
        QSignalSpy stops(&rig.state, &TransmitState::stopChanged);
        rig.key(QStringLiteral("phone"));
        rig.tickUntil(179'000);
        QCOMPARE(rig.state.stopSerial(), quint32(0));
        rig.tickUntil(181'000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.model.mox(), 5000);
        pumpEvents();
        QCOMPARE(rig.state.stopSerial(), quint32(1));
        QCOMPARE(rig.state.stopReason(), QStringLiteral("timeOut"));
        QCOMPARE(rig.state.stopText(),
                 QStringLiteral("Transmit stopped after 3:00, the Core's time-out for phones and "
                                "tablets."));
        QCOMPARE(stops.count(), 1);
        QVERIFY(!rig.state.keyed());
    }

    void aStopWithNoReasonOfItsOwnIsTheCoresOwn()
    {
        Rig rig;
        rig.key();
        rig.model.stopAllTx(QStringLiteral("a stop"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.model.mox(), 5000);
        pumpEvents();
        QCOMPARE(rig.state.stopSerial(), quint32(1));
        QCOMPARE(rig.state.stopReason(), QStringLiteral("station"));
        QCOMPARE(rig.state.stopText(), QStringLiteral("The Core stopped transmitting."));
        // Fix wave 2: the epoch of the key it stopped, taken as it stopped.
        QCOMPARE(rig.state.stopEpoch(), static_cast<qint64>(rig.model.keyingEpoch()));
    }

    void aStarvationStopsOnceAndTheNextKeyCanStopAgain()
    {
        Rig rig;
        QVERIFY2(!rig.state.recordStop(TransmitState::kStopStation, TransmitState::stationText()),
                 "a stop before any key counted");
        QCOMPARE(rig.state.stopSerial(), quint32(0));

        rig.key();
        // Task 37's starvation records its reason, then stops.
        QVERIFY(rig.state.recordStop(TransmitState::kStopMicStarved,
                                     TransmitState::micStarvedText(QStringLiteral("Test device"))));
        rig.model.stopAllTx(QStringLiteral("starved"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.model.mox(), 5000);
        pumpEvents();
        QCOMPARE(rig.state.stopSerial(), quint32(1));
        QCOMPARE(rig.state.stopReason(), QStringLiteral("micStarved"));
        QCOMPARE(rig.state.stopText(),
                 QStringLiteral("No microphone audio arrived from Test device, so the Core "
                                "stopped transmitting."));
        // A later reason for the same key changes nothing.
        QVERIFY(!rig.state.recordStop(TransmitState::kStopLinkLost,
                                      TransmitState::linkLostText(QStringLiteral("x"))));
        QCOMPARE(rig.state.stopSerial(), quint32(1));
        rig.model.setKeyedBy({});
        pumpEvents();

        // The next key can be stopped again.
        rig.key();
        QVERIFY(rig.state.recordStop(TransmitState::kStopLinkLost,
                                     TransmitState::linkLostText(QStringLiteral("Test device"))));
        QCOMPARE(rig.state.stopSerial(), quint32(2));
        QCOMPARE(rig.state.stopReason(), QStringLiteral("linkLost"));
        rig.unkey();
    }

    void everyStopTextIsPlainAndSaysTheCore()
    {
        const QStringList texts{
            TransmitState::timeOutText("mox", 180, QStringLiteral("phone")),
            TransmitState::timeOutText("mox", 180, QStringLiteral("tablet")),
            TransmitState::timeOutText("mox", 600, QStringLiteral("computer")),
            TransmitState::timeOutText("ping", 45, QStringLiteral("station")),
            TransmitState::linkLostText(QStringLiteral("iPad")),
            TransmitState::linkLostText(QString()),
            TransmitState::micStarvedText(QStringLiteral("iPad")),
            TransmitState::revokedText(QStringLiteral("iPad")),
            TransmitState::revokedText(QString()),
            TransmitState::takenOverText(QStringLiteral("iPad")),
            TransmitState::stationText(),
        };
        for (const QString& text : texts) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
            QVERIFY2(OperatorWording::coreCalledStationIn(text).isEmpty(), qPrintable(text));
            QVERIFY2(text.contains(QStringLiteral("Core")), qPrintable(text));
            QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
        }
        QCOMPARE(TransmitState::durationText(180), QStringLiteral("3:00"));
        QCOMPARE(TransmitState::durationText(45), QStringLiteral("0:45"));
        QCOMPARE(TransmitState::durationText(1800), QStringLiteral("30:00"));
        QCOMPARE(TransmitState::timeOutText("mox", 600, QStringLiteral("computer")),
                 QStringLiteral("Transmit stopped after 10:00, the Core's transmit time-out."));
        QCOMPARE(TransmitState::revokedText(QString()),
                 QStringLiteral("The device was removed from the Core, so the Core stopped "
                                "transmitting."));
    }

    void aWindowsCopyTakesTheCoresValues()
    {
        TransmitState copy;
        QSignalSpy state(&copy, &TransmitState::stateChanged);
        QSignalSpy meters(&copy, &TransmitState::metersChanged);
        QSignalSpy stops(&copy, &TransmitState::stopChanged);
        QVERIFY(copy.applyStationValue("keyed", true));
        QVERIFY(copy.applyStationValue("keyedByName", QStringLiteral("iPad")));
        QVERIFY(copy.applyStationValue("timeOutRemainingSeconds", 42));
        QVERIFY(copy.applyStationValue("forwardPowerWatts", 95.5));
        QVERIFY(copy.applyStationValue("reflectedPowerWatts", 2.0));
        QVERIFY(copy.applyStationValue("swr", 1.34));
        QVERIFY(copy.applyStationValue("alcDb", -2.5));
        QVERIFY(copy.applyStationValue("micLevelDb", -9.0));
        QVERIFY(copy.applyStationValue("stopReason", QStringLiteral("timeOut")));
        QVERIFY(copy.applyStationValue("stopSerial", qlonglong(4)));
        QVERIFY(copy.applyStationValue("stopEpoch", qlonglong(9)));
        QVERIFY(!copy.applyStationValue("notAProperty", 1));
        QVERIFY(copy.keyed());
        QCOMPARE(copy.keyedByName(), QStringLiteral("iPad"));
        QCOMPARE(copy.timeOutRemainingSeconds(), 42);
        QCOMPARE(copy.forwardPowerWatts(), 95.5);
        QCOMPARE(copy.reflectedPowerWatts(), 2.0);
        QCOMPARE(copy.swr(), 1.34);
        QCOMPARE(copy.alcDb(), -2.5);
        QCOMPARE(copy.micLevelDb(), -9.0);
        QCOMPARE(copy.stopSerial(), quint32(4));
        QCOMPARE(copy.stopEpoch(), qint64(9));
        QCOMPARE(state.count(), 2);
        QCOMPARE(meters.count(), 5);
        QCOMPARE(stops.count(), 3);
        // The same value again changes nothing.
        QVERIFY(copy.applyStationValue("keyed", true));
        QCOMPARE(state.count(), 2);

        copy.clearStationValues();
        QVERIFY(!copy.keyed());
        QVERIFY(copy.keyedByName().isEmpty());
        QCOMPARE(copy.timeOutRemainingSeconds(), -1);
        QCOMPARE(copy.meters(), TxMeterReadings{});
        QCOMPARE(copy.stopReason(), QStringLiteral("timeOut"));
        QCOMPARE(copy.stopSerial(), quint32(4));
    }

    // ---- Through the Core ---------------------------------------------------------

    void onlyADeviceDeclaringRemoteTxGetsTheObject()
    {
        Core core;
        allowTransmit(core);
        Device a{QStringLiteral("iPhone"), QStringLiteral("phone")};
        Device b{QStringLiteral("iPad"), QStringLiteral("tablet")};
        core.pair(a);
        core.pair(b);
        LoopbackTransport* transmitter = core.signIn(a, kTransmitter);
        LoopbackTransport* listener = core.signIn(b, kHolder);
        QVERIFY(admitted(transmitter) && admitted(listener));
        QCOMPARE(capability(transmitter->received(), QStringLiteral("txStateVersion")),
                 std::optional<qint64>(2));
        QVERIFY(hasObject(transmitter, QStringLiteral("txState")));
        QVERIFY(!capability(listener->received(), QStringLiteral("txStateVersion")).has_value());
        QVERIFY(!hasObject(listener, QStringLiteral("txState")));
        QVERIFY(!everSaw(listener, QStringLiteral("txState")));
    }

    void theKeyedDevicesLinkLostStopsOnceWithItsReason()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());
        QCOMPARE(p.state().keyedByName(), QStringLiteral("Grant's iPhone"));
        QCOMPARE(p.state().keyedByKind(), QStringLiteral("phone"));
        QTRY_COMPARE(latest(p.appB->received(), QStringLiteral("txState"),
                            QStringLiteral("keyed")).toBool(), true);

        p.appA->closeLink(QStringLiteral("lost"));
        QTRY_VERIFY(!p.state().keyed());
        QTRY_VERIFY(!p.core.model->moxController()->isMox());
        pumpEvents();
        QCOMPARE(p.state().stopSerial(), quint32(1));
        QCOMPARE(p.state().stopReason(), QStringLiteral("linkLost"));
        QCOMPARE(p.state().stopText(),
                 // Fix wave M3: the watchdog's sentence, one for every lost link.
                 QStringLiteral("The link to Grant's iPhone went quiet, so the Core stopped "
                                "transmitting."));
        QTRY_COMPARE(latest(p.appB->received(), QStringLiteral("txState"),
                            QStringLiteral("stopSerial")).toInteger(), 1);
        QCOMPARE(latest(p.appB->received(), QStringLiteral("txState"),
                        QStringLiteral("stopReason")).toString(),
                 QStringLiteral("linkLost"));
        // Fix wave 2: the stop names the key it ended by its keying epoch.
        QVERIFY(p.state().stopEpoch() > 0);
        QCOMPARE(p.state().stopEpoch(), static_cast<qint64>(p.core.model->keyingEpoch()));
        QCOMPARE(latest(p.appB->received(), QStringLiteral("txState"),
                        QStringLiteral("stopEpoch")).toInteger(),
                 p.state().stopEpoch());
        // Once: nothing later for the same key moves it.
        pumpEvents();
        QCOMPARE(p.state().stopSerial(), quint32(1));
    }

    void revokingTheKeyedDeviceStopsOnceWithItsReason()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());

        QVERIFY(p.core.server->deviceStore()->remove(p.a.key.fingerprint()));
        QTRY_VERIFY(!p.state().keyed());
        QTRY_VERIFY(!p.core.model->moxController()->isMox());
        pumpEvents();
        QCOMPARE(p.state().stopSerial(), quint32(1));
        QCOMPARE(p.state().stopReason(), QStringLiteral("revoked"));
        QCOMPARE(p.state().stopText(),
                 QStringLiteral("Grant's iPhone was removed from the Core, so the Core stopped "
                                "transmitting."));
        pumpEvents();
        QCOMPARE(p.state().stopSerial(), quint32(1));
        QTRY_COMPARE(latest(p.appB->received(), QStringLiteral("txState"),
                            QStringLiteral("stopReason")).toString(),
                     QStringLiteral("revoked"));
    }

    // 14. A revoke is the Core's stop: its unkey never waits for the send
    //     ring, on either protocol, whatever transmit audio is queued.
    // 15. So is a lost link (the keyed device's link drops, or the same
    //     device connects again over a new link): the Core stops it.
    void theCoresStopsNeverWaitForTheSendRing_data()
    {
        QTest::addColumn<int>("protocol");
        QTest::addColumn<QString>("stop");
        QTest::addColumn<QString>("reason");
        for (int protocol : {1, 2}) {
            const QByteArray tag = "protocol " + QByteArray::number(protocol);
            QTest::newRow(tag + ", revoked") << protocol << QStringLiteral("revoke")
                                             << QStringLiteral("revoked");
            QTest::newRow(tag + ", link lost") << protocol << QStringLiteral("linkLost")
                                               << QStringLiteral("linkLost");
            QTest::newRow(tag + ", connected again") << protocol << QStringLiteral("again")
                                                     << QStringLiteral("linkLost");
        }
    }
    void theCoresStopsNeverWaitForTheSendRing()
    {
        QFETCH(int, protocol);
        QFETCH(QString, stop);
        QFETCH(QString, reason);
        Pair p;
        // An unconnected connection: nothing sends, so what is queued stays.
        std::unique_ptr<RadioConnection> conn;
        if (protocol == 1) {
            conn = std::make_unique<P1RadioConnection>();
        } else {
            conn = std::make_unique<P2RadioConnection>();
        }
        p.core.model->injectConnectionForTest(conn.get());
        auto unplug = qScopeGuard([&p]() { p.core.model->injectConnectionForTest(nullptr); });
        MoxController* mox = p.core.model->moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);

        QVERIFY(admitted(p.appA) && admitted(p.appB));
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        std::vector<float> iq(2 * 2, 0.1f);
        conn->sendTxIq(iq.data(), 1);
        QVERIFY(!conn->txIqRingDrained());

        // A wait that starts and is cut short at once is still a wait: the
        // MoxController says so in the log when a stop ends one.
        SendRingWaitLog waitLog;
        if (stop == QStringLiteral("revoke")) {
            QVERIFY(p.core.server->deviceStore()->remove(p.a.key.fingerprint()));
        } else if (stop == QStringLiteral("linkLost")) {
            p.appA->closeLink(QStringLiteral("lost"));
        } else {
            p.core.signIn(p.a, kTransmitter);
        }
        bool waited = mox->isSendRingWaitActive();
        QElapsedTimer t;
        t.start();
        while ((mox->isMox() || mox->state() != MoxState::Rx) && t.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
            waited = waited || mox->isSendRingWaitActive();
        }
        QVERIFY(!waited);
        QVERIFY2(!waitLog.seen(), qPrintable(waitLog.line()));
        QVERIFY(!mox->isMox());
        QCOMPARE(mox->state(), MoxState::Rx);
        pumpEvents();
        QCOMPARE(p.state().stopReason(), reason);
    }

    void anUnkeyTheDeviceAskedForIsNoStop()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());
        // The first key on a fresh Core is epoch 1.
        const QJsonObject r = p.core.invoke(p.appA, "tx.unkey", {int64("epoch", 1)});
        QVERIFY(r.value(QStringLiteral("accepted")).toBool(false));
        QTRY_VERIFY(!p.state().keyed());
        pumpEvents();
        QCOMPARE(p.state().stopSerial(), quint32(0));
    }
    // ---- The Core's own stops, over the wire (merge of Tasks 37 to 39) ----

    // The watchdog: the keyed phone sends no keepalive; the Core stops more
    // than 400 ms later and both devices are told the link went quiet.
    void aQuietLinkIsTheWatchdogsStopWithItsReason()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        QSignalSpy tripped(p.core.server->txWatchdog(), &RemoteTxWatchdog::tripped);
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());
        // The Core's clock is the harness's; it moves with real time, 50 ms
        // a step, so the watchdog's real timer finds it moved.
        for (int i = 0; i < 40 && tripped.isEmpty(); ++i) {
            p.core.now += 50;
            QTest::qWait(50);
        }
        QCOMPARE(tripped.count(), 1);
        QTRY_VERIFY(!p.core.model->moxController()->isMox());
        pumpEvents();
        const QString text = QStringLiteral(
            "The link to Grant's iPhone went quiet, so the Core stopped transmitting.");
        QCOMPARE(p.state().stopReason(), QStringLiteral("linkLost"));
        QCOMPARE(p.state().stopText(), text);
        QCOMPARE(p.state().stopSerial(), quint32(1));
        verifyStopSent(p.appA, QStringLiteral("linkLost"), text);
        verifyStopSent(p.appB, QStringLiteral("linkLost"), text);
    }

    // The starvation action: the keyed phone's microphone line starves in
    // AM; both devices are told no microphone audio arrived.
    void aStarvedMicrophoneIsTheStarvationsStopWithItsReason()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        p.core.model->sliceById(0)->setDspMode(DSPMode::AM);
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());
        p.core.server->remoteMicStarved(p.a.key.fingerprint(), true);
        QTRY_VERIFY(!p.core.model->moxController()->isMox());
        pumpEvents();
        const QString text = QStringLiteral(
            "No microphone audio arrived from Grant's iPhone, so the Core stopped transmitting.");
        QCOMPARE(p.state().stopReason(), QStringLiteral("micStarved"));
        QCOMPARE(p.state().stopText(), text);
        QCOMPARE(p.state().stopSerial(), quint32(1));
        verifyStopSent(p.appA, QStringLiteral("micStarved"), text);
        verifyStopSent(p.appB, QStringLiteral("micStarved"), text);
    }

    // The transmit time-out: the phone's key reaches the phones' limit
    // (180 s by default); both devices are told the time-out stopped it.
    void aTimeOutIsItsOwnStopWithItsReason()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        qint64 timeOutNow = 0;
        p.core.model->txTimeOutTimer()->setClock([&timeOutNow]() { return timeOutNow; });
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());
        QTRY_COMPARE(latest(p.appA->received(), QStringLiteral("txState"),
                            QStringLiteral("timeOutRemainingSeconds")).toInteger(), 180);
        timeOutNow = 179 * kSecond;
        p.core.model->txTimeOutTimer()->tick();
        pumpEvents();
        QVERIFY(p.core.model->moxController()->isMox());
        timeOutNow = 180 * kSecond;
        p.core.model->txTimeOutTimer()->tick();
        QTRY_VERIFY(!p.core.model->moxController()->isMox());
        pumpEvents();
        const QString text = QStringLiteral(
            "Transmit stopped after 3:00, the Core's time-out for phones and tablets.");
        QCOMPARE(p.state().stopReason(), QStringLiteral("timeOut"));
        QCOMPARE(p.state().stopText(), text);
        QCOMPARE(p.state().stopSerial(), quint32(1));
        verifyStopSent(p.appA, QStringLiteral("timeOut"), text);
        verifyStopSent(p.appB, QStringLiteral("timeOut"), text);
    }

    // ---- Fix wave I4: txState names the holder (ruling 8.1) -------------

    // The device that keys on unheld transmit holds it, and every device's
    // txState names it: id, names, kind, source, epoch; how long, on the
    // Core's clock when it is sent; away when its link drops.
    void txStateNamesTheHolderForEveryDevice()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        QCOMPARE(p.state().holderDeviceId(), QString());
        QCOMPARE(p.state().holderSource(), QString());
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());
        QCOMPARE(p.state().holderDeviceId(), p.a.id());
        QCOMPARE(p.state().holderName(), QStringLiteral("Grant's iPhone"));
        QCOMPARE(p.state().holderShortName(), QStringLiteral("iPhone"));
        QCOMPARE(p.state().holderKind(), QStringLiteral("phone"));
        QCOMPARE(p.state().holderSource(), QStringLiteral("device"));
        QVERIFY(p.state().holderEpoch() > 0);
        QVERIFY(!p.state().holderAway());
        QVERIFY(!p.state().holderTransferring());
        const qint64 epoch = p.state().holderEpoch();
        // The other device reads the same holder.
        const auto seenByB = [&p](const char* name) {
            return latest(p.appB->received(), QStringLiteral("txState"), QString::fromLatin1(name));
        };
        QTRY_COMPARE(seenByB("holderDeviceId").toString(), p.a.id());
        QCOMPARE(seenByB("holderName").toString(), QStringLiteral("Grant's iPhone"));
        QCOMPARE(seenByB("holderShortName").toString(), QStringLiteral("iPhone"));
        QCOMPARE(seenByB("holderKind").toString(), QStringLiteral("phone"));
        QCOMPARE(seenByB("holderSource").toString(), QStringLiteral("device"));
        QCOMPARE(seenByB("holderEpoch").toInteger(), epoch);
        QCOMPARE(seenByB("holderAway").toBool(true), false);
        QCOMPARE(seenByB("holderTransferring").toBool(true), false);
        // The durations are whole seconds on the Core's clock.
        p.core.now += 5500;
        QCOMPARE(p.state().holderForSeconds(), qint64(5));
        QCOMPARE(p.state().keyedForSeconds(), qint64(5));
        // Unkeyed, still the holder.
        const QJsonObject unkey = p.core.invoke(
            p.appA, "tx.unkey", {int64("epoch", p.core.model->keyedBy().epoch)});
        QVERIFY(unkey.value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(!p.state().keyed());
        QCOMPARE(p.state().keyedForSeconds(), qint64(0));
        QCOMPARE(p.state().holderDeviceId(), p.a.id());
        // Its link drops: away, the same epoch.
        p.appA->closeLink(QStringLiteral("lost"));
        QTRY_VERIFY(p.state().holderAway());
        QCOMPARE(p.state().holderEpoch(), epoch);
        QTRY_COMPARE(seenByB("holderAway").toBool(false), true);
    }

    // Fix wave 2 (the re-review's minor): holderTransferring is true
    // whenever keys are refused for a transfer's reasons: during a dropped
    // keyed holder's fence as much as during a transfer.
    void aDroppedHoldersFenceShowsTransmitChangingHands()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        TransmitHolder* holder = p.core.server->transmitHolder();
        QVERIFY(p.keyA());
        QTRY_VERIFY(p.state().keyed());
        bool sawFence = false;
        QObject::connect(&p.state(), &TransmitState::holderChanged, &p.state(), [&]() {
            if (holder->isFenced()) {
                sawFence = true;
                QVERIFY(p.state().holderTransferring());
            }
        });
        p.appA->closeLink(QStringLiteral("lost"));
        QTRY_VERIFY(p.state().holderAway());
        QTRY_VERIFY(!holder->isFenced());
        QVERIFY(sawFence);
        QVERIFY(!p.state().holderTransferring());
        // (The fence here lasts less than one 50 ms delta flush, so B may
        // hear only its end.)
        QTRY_COMPARE(latest(p.appB->received(), QStringLiteral("txState"),
                            QStringLiteral("holderTransferring")).toBool(true),
                     false);
    }

    // The radio's own PTT holds transmit as "Radio", source radioPtt, and
    // (Task 77, ruling 8.1) keeps it, unkeyed, when the press ends, until a
    // device takes it.
    void theRadiosPttIsNamedRadioWithItsSource()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        MoxController* mox = p.core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(p.state().holderName(), QStringLiteral("Radio"));
        QCOMPARE(p.state().holderShortName(), QStringLiteral("Radio"));
        QCOMPARE(p.state().holderKind(), QStringLiteral("station"));
        QCOMPARE(p.state().holderSource(), QStringLiteral("radioPtt"));
        QCOMPARE(p.state().holderDeviceId(), QStringLiteral("station"));
        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(!mox->isMox());
        QTRY_VERIFY(!p.state().keyed());
        QCOMPARE(p.state().holderDeviceId(), QStringLiteral("station"));
        QCOMPARE(p.state().holderName(), QStringLiteral("Radio"));
        QCOMPARE(p.state().holderSource(), QStringLiteral("radioPtt"));
    }

    // A window's copy takes the holder as the Core sends it.
    void aWindowsCopyTakesTheHolder()
    {
        TransmitState copy;
        QSignalSpy changed(&copy, &TransmitState::holderChanged);
        QVERIFY(copy.applyStationValue("holderDeviceId", QStringLiteral("abc")));
        QVERIFY(copy.applyStationValue("holderName", QStringLiteral("Shack iPad")));
        QVERIFY(copy.applyStationValue("holderForSeconds", qlonglong(42)));
        QVERIFY(copy.applyStationValue("holderEpoch", qlonglong(7)));
        QVERIFY(copy.applyStationValue("holderAway", true));
        QVERIFY(copy.applyStationValue("holderTransferring", true));
        QVERIFY(copy.applyStationValue("keyedForSeconds", qlonglong(3)));
        QCOMPARE(copy.holderDeviceId(), QStringLiteral("abc"));
        QCOMPARE(copy.holderName(), QStringLiteral("Shack iPad"));
        QCOMPARE(copy.holderForSeconds(), qint64(42));
        QCOMPARE(copy.holderEpoch(), qint64(7));
        QVERIFY(copy.holderAway());
        QVERIFY(copy.holderTransferring());
        QCOMPARE(copy.keyedForSeconds(), qint64(3));
        QVERIFY(changed.count() >= 6);
        copy.clearStationValues();
        QCOMPARE(copy.holderDeviceId(), QString());
        QCOMPARE(copy.holderEpoch(), qint64(0));
        QVERIFY(!copy.holderAway());
    }
};

QTEST_GUILESS_MAIN(TstTransmitStateFacade)
#include "tst_transmit_state_facade.moc"
