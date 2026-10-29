// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_tci_cw.cpp  (NereusSDR)
// =================================================================
//
// CW transmit Task 9 (R-CW-35, R-CW-52): the TCI CW commands on a local
// model with a fake connection: cw_macros and cw_msg send through the CWX
// sender, cw_macros_speed (set, query, up, down) and cw_keyer_speed set and
// report, keyer keys the CWX bit through the sender and raises MOX in Semi,
// a press with no release drops 3000 ms after it, a timed press or release
// drops on time, cw_macros_stop aborts, cw_macros_empty with the terminal
// on, the init burst's real values, the owner rule, and refusals. Nothing
// here opens a radio socket or keys a radio: the key line is the fake
// connection's own and the fake connection sends nothing.
//
// Modification history (NereusSDR):
//   2026-10-27: F3: controlled sender time proves watchdog and timed
//               releases at exact logical deadlines, including a repeated
//               held press. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: CW transmit, CAT and TCI CW commands. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <type_traits>

#include "core/AppSettings.h"
#include "core/CwKeyLine.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TciProtocol.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// In particular, the injection overload requires its third argument, so
// existing (Role, nullptr) calls still select the QObject-parent overload.
static_assert(std::is_constructible_v<RadioModel>);
static_assert(std::is_constructible_v<RadioModel, QObject*>);
static_assert(std::is_constructible_v<RadioModel, RadioModel::Role>);
static_assert(std::is_constructible_v<RadioModel, RadioModel::Role, std::nullptr_t>);

// Test-owned monotonic time. Returning from sleep on every generation change
// lets the sender re-read a shortened deadline as well as an advanced clock.
// The fixture shuts this down before RadioModel destroys/joins the sender.
class ControlledCwxTime {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    TimePoint now()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return timePoint(m_nowMs);
    }
    qint64 nowMs()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_nowMs;
    }
    void sleep(TimePoint until)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_shutdown || timePoint(m_nowMs) >= until) {
            return;
        }
        // Keep a wake that arrives between run()'s deadline snapshot and
        // entry here; otherwise a just-shortened deadline could be missed.
        if (m_wakePending) {
            m_wakePending = false;
            return;
        }
        m_waiting = true;
        m_waitingAtMs = m_nowMs;
        m_deadlineMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            until - timePoint(0)).count();
        const quint64 generation = m_generation;
        m_changed.wait(lock, [this, generation]() {
            return m_shutdown || m_generation != generation;
        });
        m_waiting = false;
        m_wakePending = false;
    }
    void advanceTo(qint64 ms)
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_nowMs = ms;
            m_waiting = false;
            m_wakePending = true;
            ++m_generation;
        }
        m_changed.notify_all();
    }
    void wake()
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_waiting = false;
            m_wakePending = true;
            ++m_generation;
        }
        m_changed.notify_all();
    }
    bool waitingAt(qint64 nowMs, qint64 deadlineMs)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_waiting && m_waitingAtMs == nowMs && m_deadlineMs == deadlineMs;
    }
    void shutdown()
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_shutdown = true;
        }
        m_changed.notify_all();
    }
    std::unique_ptr<CwxSender> makeSender()
    {
        return std::make_unique<CwxSender>([this]() { return now(); },
                                          [this](TimePoint until) { sleep(until); });
    }

private:
    static TimePoint timePoint(qint64 ms)
    {
        return TimePoint{} + std::chrono::hours(1000) + std::chrono::milliseconds(ms);
    }
    std::mutex m_mutex;
    std::condition_variable m_changed;
    qint64 m_nowMs{0};
    qint64 m_waitingAtMs{-1};
    qint64 m_deadlineMs{-1};
    quint64 m_generation{0};
    bool m_waiting{false};
    bool m_wakePending{false};
    bool m_shutdown{false};
};

class TciCwMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit TciCwMockConnection(QObject* parent = nullptr)
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

RadioInfo radioInfo(ProtocolVersion protocol, HPSDRHW board, int firmware)
{
    RadioInfo info;
    info.protocol = protocol;
    info.boardType = board;
    info.firmwareVersion = firmware;
    return info;
}

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// CWX key edges, on controlled sender time when that fixture mode is used.
struct KeyTimes {
    QElapsedTimer clock;
    std::atomic<qint64> downAt{-1};
    std::atomic<qint64> upAt{-1};
    std::atomic<int> downs{0};
    std::atomic<int> ups{0};
};

// A local model with a fake connection, one slice on 20 m CWU bound for
// transmit, Semi break-in, CWX at 60 WPM, and the TCI protocol on it.
struct Rig {
    ControlledCwxTime time; // Declared first: outlives the owned sender.
    RadioModel model;
    TciCwMockConnection conn;
    SliceModel* slice{nullptr};
    std::unique_ptr<TciProtocol> tci;
    KeyTimes times;

    explicit Rig(bool controlledTime = false)
        : model(RadioModel::Role::Local, controlledTime ? time.makeSender() : nullptr, nullptr)
    {
        AppSettings::instance().clear();
        model.setLastRadioInfoForTest(radioInfo(ProtocolVersion::Protocol2, HPSDRHW::OrionMKII, 22));
        model.injectConnectionForTest(&conn);
        model.wireCwKeyerConnectionForTest();
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        slice = model.sliceById(model.addSlice());
        slice->setDspMode(DSPMode::CWU);
        slice->setFrequency(14'050'000.0);
        model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        model.transmitModel().setCwxSpeed(60);
        tci = std::make_unique<TciProtocol>(&model);
        times.clock.start();
        conn.cwKeyLine()->setOnChange([this, controlledTime]() {
            const bool down = conn.cwKeyLine()->cwx();
            const qint64 now = controlledTime ? time.nowMs() : times.clock.elapsed();
            if (down) {
                times.downAt.store(now);
                times.downs.fetch_add(1);
            } else {
                times.upAt.store(now);
                times.ups.fetch_add(1);
            }
        });
        pump();
    }
    ~Rig()
    {
        model.cwxSender()->stop();
        time.shutdown(); // Also runs after an early QVERIFY return.
        pump();
        conn.cwKeyLine()->setOnChange({});
        tci.reset();
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }
    QString cmd(const QString& line) { return tci->handleCommand(line); }
    QStringList notifications()
    {
        QStringList out;
        while (tci->hasPendingNotification()) {
            out << tci->takePendingNotification();
        }
        return out;
    }
    CwxSender* cwx() { return model.cwxSender(); }
    bool keyed() { return conn.cwKeyLine()->cwx(); }
};

} // namespace

class TestTciCw : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── Macros ─────────────────────────────────────────────────────────────
    void cwMacrosSendsThroughTheSender()
    {
        Rig rig;
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros:0,CQ TEST;")), QString());
        QVERIFY(rig.cwx()->isSending());
        QTRY_VERIFY(rig.keyed());
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 10000);
        QVERIFY(!rig.keyed());
    }

    void cwMacrosStopAborts()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_macros:0,TTTTTTTTTTTTTTTTTTTT;"));
        QTRY_VERIFY(rig.keyed());
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros_stop;")), QString());
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.keyed());
    }

    void cwMsgSendsPrefixCallsignAndSuffixAndTheCallsignMayChange()
    {
        Rig rig;
        QSignalSpy started(rig.cwx(), &CwxSender::characterStarted);
        rig.cmd(QStringLiteral("cw_msg:0,TTTT,K1A,E;"));
        // Changed while the prefix sends: five characters, not three.
        rig.cmd(QStringLiteral("cw_msg:W1XYZ;"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 15000);
        pump();
        QCOMPARE(started.count(), 4 + 5 + 1);
    }

    void cwMacrosEmptyWithTheTerminalOn()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_terminal:0,true;"));
        rig.notifications();
        rig.cmd(QStringLiteral("cw_macros:0,EE;"));
        QVERIFY(rig.model.mox());   // the terminal holds TCI PTT
        QStringList seen;
        const auto emptySeen = [&rig, &seen]() {
            seen << rig.notifications();
            return seen.contains(QStringLiteral("cw_macros_empty:0;"));
        };
        QTRY_VERIFY_WITH_TIMEOUT(emptySeen(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 5000);
        pump();
        QVERIFY(rig.model.mox());   // the terminal keeps it while on
        rig.cmd(QStringLiteral("cw_terminal:0,false;"));
        pump();
        QVERIFY(!rig.model.mox());
    }

    void cwMacrosEmptyNotSentWithTheTerminalOff()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_macros:0,EE;"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 5000);
        pump();
        QVERIFY(!rig.notifications().contains(QStringLiteral("cw_macros_empty:0;")));
        QVERIFY(!rig.model.mox());
    }

    // ── Speeds ─────────────────────────────────────────────────────────────
    void cwMacrosSpeedSetsAndReports()
    {
        Rig rig;
        rig.notifications();
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros_speed:28;")), QString());
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 28);
        QVERIFY(rig.notifications().contains(QStringLiteral("cw_macros_speed:28;")));
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros_speed;")), QStringLiteral("cw_macros_speed:28;"));
        rig.cmd(QStringLiteral("cw_macros_speed_up:5;"));
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 33);
        rig.cmd(QStringLiteral("cw_macros_speed_down:10;"));
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 23);
        rig.cmd(QStringLiteral("cw_macros_speed:99;"));
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 60);   // D12
    }

    void cwKeyerSpeedSetsAndReports()
    {
        Rig rig;
        rig.notifications();
        QCOMPARE(rig.cmd(QStringLiteral("cw_keyer_speed:33;")), QString());
        QCOMPARE(rig.model.transmitModel().cwKeyerSpeed(), 33);
        QVERIFY(rig.notifications().contains(QStringLiteral("cw_keyer_speed:33;")));
        QCOMPARE(rig.cmd(QStringLiteral("cw_keyer_speed;")), QStringLiteral("cw_keyer_speed:33;"));
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros_delay;")), QStringLiteral("cw_macros_delay:50;"));
    }

    void initBurstCarriesTheRealSpeeds()
    {
        Rig rig;
        rig.model.transmitModel().setCwxSpeed(28);
        rig.model.transmitModel().setCwKeyerSpeed(33);
        const QStringList burst = rig.tci->buildInitBurst();
        QVERIFY(burst.contains(QStringLiteral("cw_macros_speed:28;")));
        QVERIFY(burst.contains(QStringLiteral("cw_macros_delay:50;")));
        QVERIFY(burst.contains(QStringLiteral("cw_keyer_speed:33;")));
    }

    // ── The keyer ──────────────────────────────────────────────────────────
    void keyerKeysTheCwxBitAndMoxInSemi()
    {
        Rig rig(true);
        QCOMPARE(rig.cmd(QStringLiteral("keyer:0,true;")), QString());
        QVERIFY(rig.keyed());
        QVERIFY(rig.cwx()->isKeyHeld());
        QVERIFY(rig.model.mox());
        rig.cmd(QStringLiteral("keyer:0,false;"));
        QVERIFY(!rig.keyed());
        pump();
        QVERIFY(!rig.model.mox());
    }

    void keyerInOffRaisesNoMox()
    {
        Rig rig(true);
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        pump();
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.cwx()->isKeyHeld());
        QVERIFY(!rig.model.mox());
        rig.cmd(QStringLiteral("keyer:0,false;"));
        QVERIFY(!rig.cwx()->isKeyHeld());
    }

    void keyerWithNoReleaseDrops3000MsAfterThePress()
    {
        Rig rig(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.keyed());
        QVERIFY(rig.cwx()->isKeyHeld());
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(0, 3000), 2000);
        QCOMPARE(rig.times.downs.load(), 1);
        QCOMPARE(rig.times.ups.load(), 0);

        // Reach the second press while the original Key is demonstrably
        // held. Repeated presses must neither add an edge nor move expiry.
        rig.time.advanceTo(500);
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(500, 3000), 2000);
        QVERIFY(rig.keyed());
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.cmd(QStringLiteral("keyer:0,true;"));
        rig.time.wake();
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(500, 3000), 2000);
        QCOMPARE(rig.times.downs.load(), 1);
        rig.time.advanceTo(2999);
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(2999, 3000), 2000);
        QVERIFY(rig.keyed());
        QCOMPARE(rig.times.ups.load(), 0);
        rig.time.advanceTo(3000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isKeyHeld(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.times.ups.load(), 1, 2000);
        QVERIFY(!rig.keyed());
        QCOMPARE(rig.times.upAt.load(), qint64(3000));
        QTRY_VERIFY(!rig.model.mox());
        QVERIFY(!rig.cwx()->isSending());
    }

    void keyerPressWithADurationReleasesOnTime()
    {
        Rig rig(true);
        rig.cmd(QStringLiteral("keyer:0,true,120;"));
        QVERIFY(rig.keyed());
        rig.time.wake();
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(0, 120), 2000);
        rig.time.advanceTo(119);
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(119, 120), 2000);
        QVERIFY(rig.keyed());
        QVERIFY(rig.cwx()->isKeyHeld());
        QCOMPARE(rig.times.downs.load(), 1);
        QCOMPARE(rig.times.ups.load(), 0);
        rig.time.advanceTo(120);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isKeyHeld(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.times.ups.load(), 1, 2000);
        QVERIFY(!rig.keyed());
        QCOMPARE(rig.times.downAt.load(), qint64(0));
        QCOMPARE(rig.times.upAt.load(), qint64(120));
        QTRY_VERIFY(!rig.model.mox());
    }

    void keyerReleaseWithADurationCountsFromThePress()
    {
        Rig rig(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.keyed());
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(0, 3000), 2000);
        rig.time.advanceTo(80);
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(80, 3000), 2000);
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.cmd(QStringLiteral("keyer:0,false,200;"));
        rig.time.wake(); // Interrupt the already-entered 3 s sleep.
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(80, 200), 2000);
        QVERIFY(rig.keyed());
        rig.time.advanceTo(199);
        QTRY_VERIFY_WITH_TIMEOUT(rig.time.waitingAt(199, 200), 2000);
        QVERIFY(rig.keyed());
        QCOMPARE(rig.times.ups.load(), 0);
        rig.time.advanceTo(200);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isKeyHeld(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.times.ups.load(), 1, 2000);
        QVERIFY(!rig.keyed());
        QCOMPARE(rig.times.downs.load(), 1);
        QCOMPARE(rig.times.downAt.load(), qint64(0));
        QCOMPARE(rig.times.upAt.load(), qint64(200));
        QTRY_VERIFY(!rig.model.mox());
    }

    void cwMacrosStopReleasesTheKeyerAndItsMox()
    {
        Rig rig(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.model.mox());
        rig.cmd(QStringLiteral("cw_macros_stop;"));
        QVERIFY(!rig.keyed());
        pump();
        QVERIFY(!rig.model.mox());
    }

    void leavingCwReleasesTheKeyerAndItsMox()
    {
        Rig rig(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.keyed());
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        QVERIFY(!rig.keyed());
        QTRY_VERIFY(!rig.model.mox());
    }

    // ── One sender: the keyer and CWX text do not mix ──────────────────────
    void keyerPressWhileTextSendsIsIgnored()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_macros:0,TTTTTTTTTT;"));
        QVERIFY(rig.cwx()->isSending());
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(rig.cwx()->isSending());
        // CWX text from the applet or CAT too.
        rig.cmd(QStringLiteral("cw_macros_stop;"));
        QVERIFY(rig.cwx()->sendText(QStringLiteral("TTTTTTTTTT")));
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(rig.cwx()->isSending());
    }

    void macroWhileTheKeyerIsHeldIsIgnored()
    {
        Rig rig(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.cmd(QStringLiteral("cw_macros:0,CQ;"));
        QVERIFY(rig.cwx()->isKeyHeld());
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
        rig.cmd(QStringLiteral("keyer:0,false;"));
    }

    // ── The owner ──────────────────────────────────────────────────────────
    void anotherAppCannotKeyWhileOneOwnsTheKeyerAndLeavingStopsIt()
    {
        Rig rig(true);
        int appA = 0;
        int appB = 0;
        rig.tci->setCommandOwner(&appA);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        rig.tci->setCommandOwner(&appB);
        rig.cmd(QStringLiteral("keyer:0,false;"));      // not B's to release
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.cmd(QStringLiteral("cw_macros_stop;"));     // nor to stop
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.tci->setCommandOwner(nullptr);
        rig.tci->cwClientDisconnected(&appA);
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(!rig.keyed());
        pump();
        QVERIFY(!rig.model.mox());
    }

    // ── Refusals ───────────────────────────────────────────────────────────
    void refusedByTheGateKeysNothingAndTheSenderSaysWhy()
    {
        Rig rig;
        rig.model.setStationRadioChangeUnderway(true);
        pump();
        QVERIFY(!rig.model.cwKeyerController()->keyerAllowed());
        QSignalSpy refused(rig.cwx(), &CwxSender::refused);
        QCOMPARE(rig.cmd(QStringLiteral("keyer:0,true;")), QString());
        QVERIFY(!rig.keyed());
        QVERIFY(!rig.model.mox());
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros:0,CQ;")), QString());
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(refused.count(), 2);
        for (const QList<QVariant>& r : refused) {
            QCOMPARE(r.at(0).toString(), rig.model.cwKeyerController()->refusal().text);
        }
        QCOMPARE(rig.times.downs.load(), 0);
    }

    void keyerOutsideCwKeysNothing()
    {
        Rig rig;
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(!rig.keyed());
        QVERIFY(!rig.model.mox());
    }

    void remoteWindowAndReceiveOnlyServerKeyNothing()
    {
        Rig rig;
        rig.tci->setRemoteWindow(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        rig.cmd(QStringLiteral("cw_macros:0,CQ;"));
        rig.cmd(QStringLiteral("cw_macros_speed:40;"));
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 60);
        rig.tci->setRemoteWindow(false);
        rig.tci->setStationReceiveOnly(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        rig.cmd(QStringLiteral("cw_macros:0,CQ;"));
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.times.downs.load(), 0);
    }

    void rxOneNeedsTheSecondReceiver()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_macros:1,CQ;"));
        rig.cmd(QStringLiteral("keyer:1,true;"));
        QVERIFY(!rig.cwx()->isSending());
        rig.cmd(QStringLiteral("keyer:2,true;"));
        QVERIFY(!rig.cwx()->isSending());
    }
};

QTEST_GUILESS_MAIN(TestTciCw)
#include "tst_tci_cw.moc"
