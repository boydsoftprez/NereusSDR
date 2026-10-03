// no-port-check: NereusSDR-original. Thetis's TimeOutTimerManager.cs and
// console.cs timeOutTimer are cited beside the port in
// src/core/safety/TxTimeOutTimer.{h,cpp} and RadioModel.cpp; this test
// translates no C#.
//
// iPhone app plan Task 38 (R-IOS-04, D29): the transmit time-out. The
// transmit safety boundary, so the invariants come first:
//
//   The timer on its own (an injected clock and pinger, tick() driven by
//   the test):
//     1. The MOX time-out fires once the seconds since the rising edge of
//        MOX reach the limit, and not a tick before.
//     2. Off, it never fires, however long the key.
//     3. A limit changed while keyed counts from key-down.
//     4. Unkeying stops the tick; the next key starts from its own edge.
//     5. The ping time-out: a good ping holds it off; it fires once the
//        last good ping is older than its limit; a reply from an earlier
//        key counts for nothing; the MOX time-out firing first sends no
//        ping.
//     6. remainingSeconds: the whole seconds left, -1 unkeyed or off.
//     7. The system ping answers "no" for a host that does not parse,
//        without running anything.
//
//   On a model with a radio (a mock connection; nothing keys a real radio):
//     8. Keyed from a phone with the defaults: stopped between 180 and
//        181 s after key-down, with "MOX Time Out Timer", the stop reason
//        timeOut with its limit, and MOX and the relay off.
//     9. Keyed from a computer, or at the station, with Thetis's defaults:
//        never stopped (30 minutes of ticks).
//    10. TUNE and two-tone from a phone are stopped too, as StopAllTx does.
//    11. The phone limit changed while keyed applies from key-down.
//    12. Thetis's MOX time-out switched on stops a computer's key.
//    13. timeOutRemainingSeconds follows the key and the device's kind.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QProcess>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/safety/TxTimeOutTimer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <functional>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr qint64 kSecond = 1000;

// A clock the test moves by hand.
struct FakeClock {
    qint64 nowMs{0};
    TxTimeOutTimer::Clock clock()
    {
        return [this]() { return nowMs; };
    }
};

// Records each ping and answers it only when the test says so.
struct FakePinger {
    struct Ping {
        QString host;
        std::function<void(bool)> done;
    };
    std::vector<Ping> pings;

    TxTimeOutTimer::Pinger pinger()
    {
        return [this](const QString& host, std::function<void(bool)> done) {
            pings.push_back({host, std::move(done)});
        };
    }
    void answerLast(bool success)
    {
        QVERIFY(!pings.empty());
        auto done = pings.back().done;
        pings.pop_back();
        done(success);
    }
};

// Ticks the timer once a second from `fromMs` up to `toMs` inclusive.
void tickEverySecond(TxTimeOutTimer& timer, FakeClock& clock, qint64 fromMs, qint64 toMs)
{
    for (qint64 t = fromMs; t <= toMs; t += kSecond) {
        clock.nowMs = t;
        timer.tick();
    }
}

void pump(int passes = 8)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// Records MOX and relay writes; never keys a real radio.
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

// A local model on 20 m USB with a mock connection, a TX channel wrapper
// with no WDSP channel behind it, two-tone ready, the MOX walk driven by
// processEvents, and the time-out on the test's clock (its one-second
// QTimer never reaches a tick inside a test: the test ticks it itself).
struct Rig {
    RadioModel model;
    MockConnection conn;
    TxChannel tx{WdspEngine::kTxChannelId};
    FakeClock clock;

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
        model.txTimeOutTimer()->setClock(clock.clock());
        model.txTimeOutTimer()->setPinger([](const QString&, std::function<void(bool)> done) {
            done(true);
        });
    }
    ~Rig()
    {
        model.twoToneController()->setTxChannel(nullptr);
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    void keyedBy(const QString& kind)
    {
        RadioModel::KeyedBy who;
        who.deviceId = kind.isEmpty() ? QByteArray() : QByteArrayLiteral("device-1");
        who.deviceName = QStringLiteral("Test device");
        who.deviceKind = kind;
        who.trigger = QByteArrayLiteral("screen");
        who.epoch = 1;
        model.setKeyedBy(who);
    }

    // Ticks on every whole second after now, up to `toMs` inclusive, as the
    // one-second tick would.
    void tickUntil(qint64 toMs)
    {
        for (qint64 t = (clock.nowMs / kSecond + 1) * kSecond; t <= toMs; t += kSecond) {
            clock.nowMs = t;
            model.txTimeOutTimer()->tick();
            pump(2);
        }
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

TxTimeOutTimer::Settings moxLimit(int seconds)
{
    TxTimeOutTimer::Settings limits;
    limits.moxEnabled = true;
    limits.moxSeconds = seconds;
    return limits;
}

} // namespace

class TestTxTimeOutTimer : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── The timer on its own ────────────────────────────────────────────

    void moxTimeOutFiresAtTheLimitFromTheRisingEdge()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        timer.setClock(clock.clock());
        timer.setSettingsSource([]() { return moxLimit(180); });
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);

        // Key-down 0.3 s into a tick period; the tick keeps its own phase.
        clock.nowMs = 300;
        timer.onMox(0, false, true);
        QVERIFY(timer.isTicking());

        tickEverySecond(timer, clock, 1000, 180'000);  // last tick: 179.7 s keyed
        QCOMPARE(fired.count(), 0);

        clock.nowMs = 181'000;                        // 180.7 s keyed
        timer.tick();
        QCOMPARE(fired.count(), 1);
        QCOMPARE(fired.at(0).at(0).toString(), QStringLiteral("MOX"));
        QCOMPARE(fired.at(0).at(1).toInt(), 180);
        const qint64 keyedMs = clock.nowMs - 300;
        QVERIFY(keyedMs >= 180'000 && keyedMs <= 181'000);

        // Thetis calls back every second while the condition holds.
        clock.nowMs = 182'000;
        timer.tick();
        QCOMPARE(fired.count(), 2);
    }

    void aKeyExactlyAtTheLimitFires()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        timer.setClock(clock.clock());
        timer.setSettingsSource([]() { return moxLimit(30); });
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);
        timer.onMox(0, false, true);
        clock.nowMs = 29'999;
        timer.tick();
        QCOMPARE(fired.count(), 0);
        clock.nowMs = 30'000;  // (now - _lastMox).TotalSeconds >= _moxTimeOutSeconds
        timer.tick();
        QCOMPARE(fired.count(), 1);
    }

    void offNeverFires()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        timer.setClock(clock.clock());
        timer.setSettingsSource([]() { return TxTimeOutTimer::Settings{}; });  // Thetis defaults
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);
        timer.onMox(0, false, true);
        tickEverySecond(timer, clock, 1000, 30 * 60 * kSecond);
        QCOMPARE(fired.count(), 0);
        QCOMPARE(timer.remainingSeconds(), -1);
    }

    void aLimitChangedWhileKeyedCountsFromKeyDown()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        timer.setClock(clock.clock());
        int limit = 180;
        timer.setSettingsSource([&limit]() { return moxLimit(limit); });
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);
        timer.onMox(0, false, true);

        tickEverySecond(timer, clock, 1000, 100'000);
        QCOMPARE(fired.count(), 0);
        // Lowered below the time already keyed: the next tick fires.
        limit = 60;
        clock.nowMs = 101'000;
        timer.tick();
        QCOMPARE(fired.count(), 1);
        QCOMPARE(fired.at(0).at(1).toInt(), 60);

        // Raised while keyed: still counted from the same key-down.
        TxTimeOutTimer longer;
        FakeClock clock2;
        longer.setClock(clock2.clock());
        int limit2 = 180;
        longer.setSettingsSource([&limit2]() { return moxLimit(limit2); });
        QSignalSpy fired2(&longer, &TxTimeOutTimer::timedOut);
        longer.onMox(0, false, true);
        tickEverySecond(longer, clock2, 1000, 170'000);
        limit2 = 300;
        tickEverySecond(longer, clock2, 171'000, 299'000);
        QCOMPARE(fired2.count(), 0);
        clock2.nowMs = 300'000;
        longer.tick();
        QCOMPARE(fired2.count(), 1);
    }

    void unkeyingStopsTheTickAndTheNextKeyStartsAgain()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        timer.setClock(clock.clock());
        timer.setSettingsSource([]() { return moxLimit(60); });
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);

        timer.onMox(0, false, true);
        tickEverySecond(timer, clock, 1000, 50'000);
        timer.onMox(0, true, false);
        QVERIFY(!timer.isTicking());
        QCOMPARE(timer.remainingSeconds(), -1);
        // Unkeyed, a tick does nothing however long it has been.
        clock.nowMs = 500'000;
        timer.tick();
        QCOMPARE(fired.count(), 0);

        // A new key counts from its own edge.
        timer.onMox(0, false, true);
        QVERIFY(timer.isTicking());
        QCOMPARE(timer.remainingSeconds(), 60);
        tickEverySecond(timer, clock, 501'000, 559'000);
        QCOMPARE(fired.count(), 0);
        clock.nowMs = 560'000;
        timer.tick();
        QCOMPARE(fired.count(), 1);
    }

    void pingTimeOutFiresWhenTheLastGoodPingIsTooOld()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        FakePinger pinger;
        timer.setClock(clock.clock());
        timer.setPinger(pinger.pinger());
        TxTimeOutTimer::Settings limits;
        limits.pingEnabled = true;
        limits.pingSeconds = 30;
        limits.pingHost = QStringLiteral("192.0.2.1");
        timer.setSettingsSource([limits]() { return limits; });
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);

        timer.onMox(0, false, true);
        // Good pings for 20 s.
        for (qint64 t = 1000; t <= 20'000; t += kSecond) {
            clock.nowMs = t;
            timer.tick();
            QCOMPARE(pinger.pings.size(), size_t(1));
            QCOMPARE(pinger.pings.back().host, QStringLiteral("192.0.2.1"));
            pinger.answerLast(true);
        }
        // Then none: the last good one was at 20 s, so 50 s is the first
        // tick 30 s after it.
        for (qint64 t = 21'000; t <= 49'000; t += kSecond) {
            clock.nowMs = t;
            timer.tick();
            pinger.answerLast(false);
        }
        QCOMPARE(fired.count(), 0);
        clock.nowMs = 50'000;
        timer.tick();
        pinger.answerLast(false);
        QCOMPARE(fired.count(), 1);
        QCOMPARE(fired.at(0).at(0).toString(), QStringLiteral("PING"));
        QCOMPARE(fired.at(0).at(1).toInt(), 30);
        // The ping time-out is not a countdown on screen.
        QCOMPARE(timer.remainingSeconds(), -1);
    }

    void aPingThatSucceedsAtTheLastMomentHoldsItOff()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        FakePinger pinger;
        timer.setClock(clock.clock());
        timer.setPinger(pinger.pinger());
        TxTimeOutTimer::Settings limits;
        limits.pingEnabled = true;
        limits.pingSeconds = 30;
        timer.setSettingsSource([limits]() { return limits; });
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);

        timer.onMox(0, false, true);
        for (qint64 t = 1000; t <= 29'000; t += kSecond) {
            clock.nowMs = t;
            timer.tick();
            pinger.answerLast(false);
        }
        // Thetis's tick pings first and decides after: a good ping in the
        // tick that would fire holds it off.
        clock.nowMs = 30'000;
        timer.tick();
        pinger.answerLast(true);
        QCOMPARE(fired.count(), 0);
    }

    void aPingReplyFromAnEarlierKeyCountsForNothing()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        FakePinger pinger;
        timer.setClock(clock.clock());
        timer.setPinger(pinger.pinger());
        TxTimeOutTimer::Settings limits;
        limits.pingEnabled = true;
        limits.pingSeconds = 30;
        timer.setSettingsSource([limits]() { return limits; });
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);

        timer.onMox(0, false, true);
        clock.nowMs = 40'000;
        timer.tick();                   // a ping in flight from the first key
        QCOMPARE(pinger.pings.size(), size_t(1));
        auto late = pinger.pings.back().done;
        pinger.pings.clear();

        timer.onMox(0, true, false);
        late(false);                    // arrives unkeyed: ignored
        QCOMPARE(fired.count(), 0);

        clock.nowMs = 41'000;
        timer.onMox(0, false, true);
        late(false);                    // arrives in the next key: ignored
        QCOMPARE(fired.count(), 0);
    }

    void theMoxTimeOutFiringFirstSendsNoPing()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        FakePinger pinger;
        timer.setClock(clock.clock());
        timer.setPinger(pinger.pinger());
        TxTimeOutTimer::Settings limits = moxLimit(30);
        limits.pingEnabled = true;
        limits.pingSeconds = 30;
        timer.setSettingsSource([limits]() { return limits; });
        QSignalSpy fired(&timer, &TxTimeOutTimer::timedOut);

        timer.onMox(0, false, true);
        clock.nowMs = 30'000;
        timer.tick();
        QCOMPARE(fired.count(), 1);
        QCOMPARE(fired.at(0).at(0).toString(), QStringLiteral("MOX"));
        QVERIFY(pinger.pings.empty());
    }

    void remainingSecondsCountsDown()
    {
        TxTimeOutTimer timer;
        FakeClock clock;
        timer.setClock(clock.clock());
        timer.setSettingsSource([]() { return moxLimit(180); });
        QCOMPARE(timer.remainingSeconds(), -1);  // unkeyed

        timer.onMox(0, false, true);
        QCOMPARE(timer.remainingSeconds(), 180);
        clock.nowMs = 300;
        QCOMPARE(timer.remainingSeconds(), 180);  // 179.7 s rounds up
        clock.nowMs = 1000;
        QCOMPARE(timer.remainingSeconds(), 179);
        clock.nowMs = 179'999;
        QCOMPARE(timer.remainingSeconds(), 1);
        clock.nowMs = 180'000;
        QCOMPARE(timer.remainingSeconds(), 0);
        clock.nowMs = 200'000;
        QCOMPARE(timer.remainingSeconds(), 0);
    }

    void theSystemPingRefusesAHostThatDoesNotParse()
    {
        QObject context;
        bool answered = false;
        bool result = true;
        TxTimeOutTimer::systemPing(&context, QStringLiteral("8.8.8.8; rm -rf /"),
                                   [&](bool ok) {
                                       answered = true;
                                       result = ok;
                                   });
        QVERIFY(context.findChildren<QProcess*>().isEmpty());  // nothing started
        QTRY_VERIFY_WITH_TIMEOUT(answered, 1000);
        QVERIFY(!result);
    }

    // ── On a model with a radio ────────────────────────────────────────

    void aPhoneKeyStopsBetween180And181Seconds()
    {
        Rig rig;
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        QSignalSpy reason(&rig.model, &RadioModel::transmitStopReasonRaised);

        rig.clock.nowMs = 400;  // key-down 0.4 s into a tick period
        rig.keyedBy(QStringLiteral("phone"));
        rig.model.moxController()->setMox(true);
        pump();
        QVERIFY(rig.model.mox());
        QVERIFY(rig.model.txTimeOutTimer()->isTicking());
        QCOMPARE(rig.model.timeOutRemainingSeconds(), 180);

        rig.tickUntil(180'000);  // last tick 179.6 s keyed
        QVERIFY(rig.model.mox());
        QCOMPARE(stopped.count(), 0);
        QCOMPARE(rig.model.timeOutRemainingSeconds(), 1);

        rig.conn.log.clear();
        rig.tickUntil(181'000);  // 180.6 s keyed
        QCOMPARE(rig.conn.log.value(0), QStringLiteral("MOX off"));
        QCOMPARE(rig.conn.log.value(1), QStringLiteral("relay off"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), QStringLiteral("MOX Time Out Timer"));
        QCOMPARE(reason.count(), 1);
        QCOMPARE(reason.at(0).at(0).toByteArray(), QByteArrayLiteral("timeOut"));
        QCOMPARE(reason.at(0).at(1).toInt(), 180);
        const RadioModel::TransmitStopReason last = rig.model.lastTransmitStopReason();
        QCOMPARE(last.code, QByteArrayLiteral("timeOut"));
        QCOMPARE(last.which, QByteArrayLiteral("mox"));
        QCOMPARE(last.limitSeconds, 180);

        // Unkeyed: no tick, nothing left, and later ticks stop nothing more.
        QVERIFY(!rig.model.txTimeOutTimer()->isTicking());
        QCOMPARE(rig.model.timeOutRemainingSeconds(), -1);
        rig.tickUntil(190'000);
        QCOMPARE(stopped.count(), 1);
        QVERIFY2(!rig.conn.log.contains(QStringLiteral("MOX on")),
                 "nothing keyed the radio again after the time-out");
    }

    void aTabletKeyStopsToo()
    {
        Rig rig;
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.keyedBy(QStringLiteral("tablet"));
        rig.model.moxController()->setMox(true);
        pump();
        rig.tickUntil(179'000);
        QCOMPARE(stopped.count(), 0);
        rig.tickUntil(180'000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
        QCOMPARE(stopped.count(), 1);
    }

    void aComputerOrTheStationWithThetisDefaultsIsNeverStopped()
    {
        for (const QString& kind : {QStringLiteral("computer"), QStringLiteral("station"),
                                    QString()}) {
            Rig rig;
            QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
            rig.keyedBy(kind);
            rig.model.moxController()->setMox(true);
            pump();
            QVERIFY(rig.model.mox());
            QCOMPARE(rig.model.timeOutRemainingSeconds(), -1);
            rig.tickUntil(30 * 60 * kSecond);
            QVERIFY2(rig.model.mox(), qPrintable(QStringLiteral("kind '%1' was stopped").arg(kind)));
            QCOMPARE(stopped.count(), 0);
            rig.model.moxController()->setMox(false);
            pump();
        }
    }

    void tuneFromAPhoneIsStopped()
    {
        Rig rig;
        rig.keyedBy(QStringLiteral("phone"));
        rig.model.setTune(true);
        pump();
        QVERIFY(rig.model.isTune());
        QVERIFY(rig.model.mox());
        QVERIFY(rig.model.txTimeOutTimer()->isTicking());

        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.tickUntil(180'000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
        QVERIFY(!rig.model.isTune());
        QCOMPARE(stopped.count(), 1);
    }

    void twoToneFromAPhoneIsStopped()
    {
        Rig rig;
        rig.keyedBy(QStringLiteral("phone"));
        rig.model.twoToneController()->setActive(true);
        pump();
        QVERIFY(rig.model.twoToneController()->isActive());
        QVERIFY(rig.model.mox());

        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.tickUntil(180'000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
        QVERIFY(!rig.model.twoToneController()->isActive());
        QCOMPARE(stopped.count(), 1);
    }

    void thePhoneLimitChangedWhileKeyedCountsFromKeyDown()
    {
        Rig rig;
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.keyedBy(QStringLiteral("phone"));
        rig.model.moxController()->setMox(true);
        pump();
        rig.tickUntil(100'000);
        QCOMPARE(stopped.count(), 0);
        QCOMPARE(rig.model.timeOutRemainingSeconds(), 80);

        // A window's change reaches the Core's settings; the next tick reads it.
        AppSettings::instance().setValue(QStringLiteral("RemoteMoxTimeOutSeconds"), 120);
        QCOMPARE(rig.model.timeOutRemainingSeconds(), 20);
        rig.tickUntil(119'000);
        QCOMPARE(stopped.count(), 0);
        rig.tickUntil(120'000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(rig.model.lastTransmitStopReason().limitSeconds, 120);
    }

    void thePhoneTimeOutSwitchedOffNeverStops()
    {
        Rig rig;
        AppSettings::instance().setValue(QStringLiteral("RemoteMoxTimeOutEnabled"),
                                         QStringLiteral("False"));
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.keyedBy(QStringLiteral("phone"));
        rig.model.moxController()->setMox(true);
        pump();
        QCOMPARE(rig.model.timeOutRemainingSeconds(), -1);
        rig.tickUntil(30 * 60 * kSecond);
        QVERIFY(rig.model.mox());
        QCOMPARE(stopped.count(), 0);
        rig.model.moxController()->setMox(false);
        pump();
    }

    void thetisMoxTimeOutSwitchedOnStopsAComputersKey()
    {
        Rig rig;
        AppSettings::instance().setValue(QStringLiteral("MoxTimeOutEnabled"),
                                         QStringLiteral("True"));
        AppSettings::instance().setValue(QStringLiteral("MoxTimeOutSeconds"), 30);
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.keyedBy(QStringLiteral("computer"));
        rig.model.moxController()->setMox(true);
        pump();
        QCOMPARE(rig.model.timeOutRemainingSeconds(), 30);
        rig.tickUntil(29'000);
        QCOMPARE(stopped.count(), 0);
        rig.tickUntil(30'000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.allOff(), 5000);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), QStringLiteral("MOX Time Out Timer"));
    }

    void settingsForEachKindOfDevice()
    {
        // Defaults: Thetis's (off, 180, 8.8.8.8) for the station and
        // computers; on at 180 for phones and tablets, with no ping.
        const TxTimeOutTimer::Settings station = RadioModel::txTimeOutSettingsFor(QString());
        QVERIFY(!station.moxEnabled);
        QCOMPARE(station.moxSeconds, 180);
        QVERIFY(!station.pingEnabled);
        QCOMPARE(station.pingSeconds, 180);
        QCOMPARE(station.pingHost, QStringLiteral("8.8.8.8"));
        const TxTimeOutTimer::Settings computer =
            RadioModel::txTimeOutSettingsFor(QStringLiteral("computer"));
        QVERIFY(!computer.moxEnabled);
        const TxTimeOutTimer::Settings phone =
            RadioModel::txTimeOutSettingsFor(QStringLiteral("phone"));
        QVERIFY(phone.moxEnabled);
        QCOMPARE(phone.moxSeconds, 180);
        QVERIFY(!phone.pingEnabled);

        // Out of range is held to 30..1800; a host that does not parse
        // leaves the default.
        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("RemoteMoxTimeOutSeconds"), 5);
        s.setValue(QStringLiteral("MoxTimeOutSeconds"), 99999);
        s.setValue(QStringLiteral("PingTimeOutHost"), QStringLiteral("not an address"));
        QCOMPARE(RadioModel::txTimeOutSettingsFor(QStringLiteral("tablet")).moxSeconds, 30);
        QCOMPARE(RadioModel::txTimeOutSettingsFor(QStringLiteral("computer")).moxSeconds, 1800);
        QCOMPARE(RadioModel::txTimeOutSettingsFor(QStringLiteral("computer")).pingHost,
                 QStringLiteral("8.8.8.8"));
    }
};

QTEST_MAIN(TestTxTimeOutTimer)
#include "tst_tx_time_out_timer.moc"
