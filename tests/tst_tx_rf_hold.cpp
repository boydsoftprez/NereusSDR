// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_tx_rf_hold.cpp  (NereusSDR)
// =================================================================
//
// Remote transmit hold then cut: the TX channel's RF hold (TxChannel::
// setRfHold) and RadioModel's wrapper (setRemoteRfHold).
//
//   heldBlocksAreZeroAtTheSameCadence: with the gate open and a Link hold,
//     every block the connection receives has the usual sample count and
//     only exact zeros, and as many blocks arrive per worker iteration as
//     unheld.
//   aHoldLeavesTheGateAndSequenceAlone: during a hold the gate stays open
//     for the run's sequence and rfGateOpened is not emitted.
//   releasingTheLastReasonSendsRealOutput: Link off with nothing else set,
//     the next block carries the channel's real output; Link and Mic both
//     on, Link off still sends zeros until Mic goes off too.
//   closeRfGateDuringAHoldSendsNothing: closeRfGate during a hold stops
//     every block, as without one.
//   theAmMonitorTapGetsTheHeldBlock: the AM monitor tap sees the same zero
//     blocks the connection does.
//   modelHoldEdgesAndMoxClear: setRemoteRfHold emits remoteRfHoldChanged
//     once per edge of the mask; MOX's return to receive clears every
//     reason and emits (false).
//   aHoldLeavesTheTimeOutAlone: the TX time-out counts the same with and
//     without a 1 s hold and fires at the same moment.
//   holdOnWaitsForABlockInsideTheSend: with a block stuck inside sendTxIq
//     on a latch, setRfHold(Link, true) from another thread does not
//     return until the latch opens, and no block the connection accepts
//     after it returns carries a non-zero sample.
//
// REALTIME (as tst_stop_transmit_now): a real TX channel runs here.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Remote transmit hold then cut: the RF hold's contract.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Remote transmit hold then cut, fix wave A: setRfHold(on)
//                waits for a block already inside sendTxIq. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include "RealtimeTestLoad.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <numbers>
#include <thread>
#include <vector>

#include "core/AmModulationAnalyzer.h"
#include "core/AppSettings.h"
#include "core/DspControlThread.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TwoToneController.h"
#include "core/TxAnalyzer.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/safety/TxTimeOutTimer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kTxId = WdspEngine::kTxChannelId;
constexpr int kRateHz = 48000;
constexpr int kTxInSize = 64;
// Enough blocks for WDSP's TX chain to settle and pass the tone.
constexpr int kWarmUpBlocks = 400;
constexpr int kCountBlocks = 200;
constexpr qint64 kSecond = 1000;
// The first OpenChannel in a process plans its FFTs with no wisdom: about
// 30 s on a quiet machine, over 100 s under a heavy build.
constexpr int kLaneIdleTimeoutMs = 600000;

// Records every TX I/Q block on the caller's thread: its sample count and
// whether any sample was not exactly zero. Never keys a real radio.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr) : RadioConnection(parent)
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
    void sendTxIq(const float* iq, int n) override
    {
        bool nonZero = false;
        for (int i = 0; i < 2 * n; ++i) {
            if (iq[i] != 0.0f) {
                nonZero = true;
                break;
            }
        }
        // Fix wave A: one send may be held on a latch, inside the sender
        // section, until the test opens it.
        if (blockNextSend.exchange(false)) {
            std::unique_lock<std::mutex> lock(latchMutex);
            sendEntered = true;
            latchChanged.notify_all();
            latchChanged.wait(lock, [this] { return latchOpen; });
        }
        blocks.push_back({n, nonZero});
    }

    // Opens the latch a held send waits on.
    void openLatch()
    {
        const std::lock_guard<std::mutex> lock(latchMutex);
        latchOpen = true;
        latchChanged.notify_all();
    }
    // Waits, at most `forMs`, for a held send to enter the latch.
    bool waitForSendEntered(std::chrono::milliseconds forMs)
    {
        std::unique_lock<std::mutex> lock(latchMutex);
        return latchChanged.wait_for(lock, forMs, [this] { return sendEntered; });
    }

    std::atomic<bool> blockNextSend{false};
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool on) override { log << (on ? QStringLiteral("MOX on") : QStringLiteral("MOX off")); }
    void setTrxRelay(bool on) override
    {
        log << (on ? QStringLiteral("relay on") : QStringLiteral("relay off"));
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

    struct Block {
        int samples;
        bool nonZero;
    };
    std::vector<Block> blocks;
    QStringList log;

private:
    std::mutex latchMutex;
    std::condition_variable latchChanged;
    bool sendEntered{false};
    bool latchOpen{false};
};

void pump(int passes = 8)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

#ifdef HAVE_WDSP
// A real TX channel on its own transmit lane (as tst_tx_analyzer_skirt
// opens one), its gate opened by setRunningAsync(true), and a 1 kHz tone
// driven one TX worker iteration at a time on this thread.
struct ChannelRig {
    DspControlThread lane{DspLane::Transmit};
    WdspEngine engine;
    MockConnection conn;
    std::unique_ptr<TxAnalyzer> analyzer;
    TxChannel* tx{nullptr};
    quint64 runSequence{0};
    std::vector<double> block = std::vector<double>(2 * kTxInSize, 0.0);
    long n{0};

    // `markInitialized` sets the engine's m_initialized (friend access,
    // NEREUS_BUILD_TESTS, through TestTxRfHold).
    bool up(const std::function<void(WdspEngine&)>& markInitialized)
    {
        lane.start();
        // The TX siphon feeds display 5, so it must exist before any block
        // (as tst_stop_transmit_now sets it up).
        analyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId, nullptr, &lane);
        analyzer->setSampleRate(96000.0);
        analyzer->setOutputFps(15);
        analyzer->start();
        markInitialized(engine);
        engine.setTransmitLane(&lane);
        tx = engine.createTxChannel(kTxId, kTxInSize, WdspEngine::kTxDspBufferSize, kRateHz,
                                    WdspEngine::kTxDspSampleRate, kRateHz);
        if (tx == nullptr || !lane.waitIdleForTest(kLaneIdleTimeoutMs)) {
            return false;
        }
        analyzer->setBlockSize(tx->dspBlockFrames());
        tx->setConnection(&conn);
        tx->onModeChanged(DSPMode::USB);
        runSequence = tx->setRunningAsync(true);
        if (!lane.waitIdleForTest(kLaneIdleTimeoutMs)
            || !QTest::qWaitFor([this] { return tx->isRfGateOpenForSequence(runSequence); },
                                kLaneIdleTimeoutMs)) {
            return false;
        }
        drive(kWarmUpBlocks);
        return true;
    }

    ~ChannelRig()
    {
        if (tx != nullptr) {
            tx->setAmModulationTap(nullptr);
            tx->setConnection(nullptr);
            tx->setRunningAsync(false);
            lane.waitIdleForTest(kLaneIdleTimeoutMs);
        }
        engine.shutdown();
        analyzer.reset();
        lane.waitIdleForTest(kLaneIdleTimeoutMs);
        engine.setTransmitLane(nullptr);
        lane.stop();
    }

    // `iterations` TX worker iterations; returns the blocks they sent.
    std::vector<MockConnection::Block> drive(int iterations)
    {
        const std::size_t from = conn.blocks.size();
        for (int b = 0; b < iterations; ++b) {
            for (int i = 0; i < kTxInSize; ++i, ++n) {
                block[static_cast<std::size_t>(2 * i)] =
                    0.1 * std::sin(2.0 * std::numbers::pi * 1000.0
                                   * static_cast<double>(n) / kRateHz);
            }
            tx->driveOneTxBlockFromInterleaved(block.data());
        }
        return {conn.blocks.begin() + static_cast<std::ptrdiff_t>(from), conn.blocks.end()};
    }
};

int countNonZero(const std::vector<MockConnection::Block>& blocks)
{
    int count = 0;
    for (const auto& b : blocks) {
        count += b.nonZero ? 1 : 0;
    }
    return count;
}

bool allSamples(const std::vector<MockConnection::Block>& blocks, int samples)
{
    for (const auto& b : blocks) {
        if (b.samples != samples) {
            return false;
        }
    }
    return true;
}
#endif // HAVE_WDSP

// A local model on 20 m USB with a mock connection, a TX channel wrapper
// with no WDSP channel behind it, the MOX walk driven by processEvents, and
// the time-out on the test's clock (as tst_tx_time_out_timer's rig).
struct ModelRig {
    RadioModel model;
    MockConnection conn;
    TxChannel tx{WdspEngine::kTxChannelId};
    qint64 nowMs{0};

    ModelRig()
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
    }
    ~ModelRig()
    {
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    void keyedByPhone()
    {
        RadioModel::KeyedBy who;
        who.deviceId = QByteArrayLiteral("device-1");
        who.deviceName = QStringLiteral("Test device");
        who.deviceKind = QStringLiteral("phone");
        who.trigger = QByteArrayLiteral("screen");
        who.epoch = 1;
        model.setKeyedBy(who);
    }

    // Ticks on every whole second after now, up to `toMs` inclusive.
    void tickUntil(qint64 toMs)
    {
        for (qint64 t = (nowMs / kSecond + 1) * kSecond; t <= toMs; t += kSecond) {
            nowMs = t;
            model.txTimeOutTimer()->tick();
            pump(2);
        }
    }
};

} // namespace

class TestTxRfHold : public QObject {
    Q_OBJECT

    static void markInitialized(WdspEngine& engine)
    {
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
    }

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup()
    {
        AppSettings::instance().clear();
        RealtimeTestLoad::printLoadAverageIfFailed();
    }

    void heldBlocksAreZeroAtTheSameCadence()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP");
#else
        ChannelRig rig;
        QVERIFY2(rig.up(&markInitialized), "the TX channel did not come up");
        const auto unheld = rig.drive(kCountBlocks);
        QCOMPARE(static_cast<int>(unheld.size()), kCountBlocks);
        const int samples = unheld.front().samples;
        QVERIFY(samples > 0);
        QVERIFY(allSamples(unheld, samples));
        QVERIFY2(countNonZero(unheld) > 0, "the unheld channel sent no real output");

        rig.tx->setRfHold(TxChannel::RfHold::Link, true);
        QVERIFY(rig.tx->isRfHeld());
        QCOMPARE(rig.tx->rfHoldMask(), static_cast<quint32>(TxChannel::RfHold::Link));
        const auto held = rig.drive(kCountBlocks);
        QCOMPARE(held.size(), unheld.size());
        QVERIFY(allSamples(held, samples));
        QCOMPARE(countNonZero(held), 0);
#endif
    }

    void aHoldLeavesTheGateAndSequenceAlone()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP");
#else
        ChannelRig rig;
        QVERIFY2(rig.up(&markInitialized), "the TX channel did not come up");
        QSignalSpy opened(rig.tx, &TxChannel::rfGateOpened);
        rig.tx->setRfHold(TxChannel::RfHold::Link, true);
        QVERIFY(rig.tx->isRfGateOpen());
        QVERIFY(rig.tx->isRfGateOpenForSequence(rig.runSequence));
        rig.drive(kCountBlocks);
        QVERIFY(rig.tx->isRfGateOpen());
        QVERIFY(rig.tx->isRfGateOpenForSequence(rig.runSequence));
        rig.tx->setRfHold(TxChannel::RfHold::Link, false);
        QVERIFY(rig.tx->isRfGateOpenForSequence(rig.runSequence));
        rig.drive(1);
        QCOMPARE(opened.count(), 0);
#endif
    }

    void releasingTheLastReasonSendsRealOutput()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP");
#else
        ChannelRig rig;
        QVERIFY2(rig.up(&markInitialized), "the TX channel did not come up");

        // One reason: the very next block after it clears is real output.
        rig.tx->setRfHold(TxChannel::RfHold::Link, true);
        QCOMPARE(countNonZero(rig.drive(kCountBlocks)), 0);
        rig.tx->setRfHold(TxChannel::RfHold::Link, false);
        QVERIFY(!rig.tx->isRfHeld());
        QCOMPARE(rig.tx->rfHoldMask(), 0u);
        const auto next = rig.drive(1);
        QCOMPARE(next.size(), std::size_t(1));
        QVERIFY2(next.front().nonZero, "the first block after the hold was not real output");

        // Two reasons: still held until the last clears.
        rig.tx->setRfHold(TxChannel::RfHold::Link, true);
        rig.tx->setRfHold(TxChannel::RfHold::Mic, true);
        QCOMPARE(rig.tx->rfHoldMask(),
                 static_cast<quint32>(TxChannel::RfHold::Link)
                     | static_cast<quint32>(TxChannel::RfHold::Mic));
        QCOMPARE(countNonZero(rig.drive(kCountBlocks)), 0);
        rig.tx->setRfHold(TxChannel::RfHold::Link, false);
        QVERIFY(rig.tx->isRfHeld());
        QCOMPARE(rig.tx->rfHoldMask(), static_cast<quint32>(TxChannel::RfHold::Mic));
        QCOMPARE(countNonZero(rig.drive(kCountBlocks)), 0);
        rig.tx->setRfHold(TxChannel::RfHold::Mic, false);
        const auto released = rig.drive(1);
        QCOMPARE(released.size(), std::size_t(1));
        QVERIFY2(released.front().nonZero, "the first block after the last reason was not real output");

        // clearRfHolds turns every reason off.
        rig.tx->setRfHold(TxChannel::RfHold::Guard, true);
        rig.tx->setRfHold(TxChannel::RfHold::Mic, true);
        rig.tx->clearRfHolds();
        QCOMPARE(rig.tx->rfHoldMask(), 0u);
        QVERIFY(rig.drive(1).front().nonZero);
#endif
    }

    void closeRfGateDuringAHoldSendsNothing()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP");
#else
        ChannelRig rig;
        QVERIFY2(rig.up(&markInitialized), "the TX channel did not come up");
        rig.tx->setRfHold(TxChannel::RfHold::Link, true);
        QVERIFY(!rig.drive(10).empty());
        rig.tx->closeRfGate();
        QVERIFY(!rig.tx->isRfGateOpen());
        QVERIFY(rig.drive(kCountBlocks).empty());
        rig.tx->setRfHold(TxChannel::RfHold::Link, false);
        QVERIFY(rig.drive(kCountBlocks).empty());
#endif
    }

    void theAmMonitorTapGetsTheHeldBlock()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP");
#else
        ChannelRig rig;
        QVERIFY2(rig.up(&markInitialized), "the TX channel did not come up");

        // Unheld, a fresh tap sees the tone's envelope.
        AmModulationAnalyzer unheldTap;
        unheldTap.setSampleRate(kRateHz);
        rig.tx->setAmModulationTap(&unheldTap);
        const auto unheld = rig.drive(kCountBlocks);
        rig.tx->setAmModulationTap(nullptr);
        const AmModulationAnalyzer::Snapshot real = unheldTap.snapshot();
        QCOMPARE(real.framesSeen,
                 static_cast<std::uint64_t>(unheld.size())
                     * static_cast<std::uint64_t>(unheld.front().samples));
        QVERIFY2(real.carrierLevel > 0.0, "the unheld tap saw no signal");

        // Held, a fresh tap sees every block, all zero.
        rig.tx->setRfHold(TxChannel::RfHold::Link, true);
        AmModulationAnalyzer heldTap;
        heldTap.setSampleRate(kRateHz);
        rig.tx->setAmModulationTap(&heldTap);
        const auto held = rig.drive(kCountBlocks);
        rig.tx->setAmModulationTap(nullptr);
        const AmModulationAnalyzer::Snapshot silent = heldTap.snapshot();
        QCOMPARE(countNonZero(held), 0);
        QCOMPARE(silent.framesSeen,
                 static_cast<std::uint64_t>(held.size())
                     * static_cast<std::uint64_t>(held.front().samples));
        QCOMPARE(silent.carrierLevel, 0.0);
        QVERIFY(!silent.carrierPresent);
#endif
    }

    void modelHoldEdgesAndMoxClear()
    {
        ModelRig rig;
        QSignalSpy changed(&rig.model, &RadioModel::remoteRfHoldChanged);
        rig.model.moxController()->setMox(true);
        pump();
        QVERIFY(rig.model.mox());

        rig.model.setRemoteRfHold(TxChannel::RfHold::Link, true);
        QVERIFY(rig.model.isRemoteRfHeld());
        QVERIFY(rig.tx.isRfHeld());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toBool(), true);

        // A second reason and a repeat are not edges.
        rig.model.setRemoteRfHold(TxChannel::RfHold::Mic, true);
        rig.model.setRemoteRfHold(TxChannel::RfHold::Link, true);
        QCOMPARE(changed.count(), 1);
        rig.model.setRemoteRfHold(TxChannel::RfHold::Link, false);
        QVERIFY(rig.model.isRemoteRfHeld());
        QCOMPARE(rig.tx.rfHoldMask(), static_cast<quint32>(TxChannel::RfHold::Mic));
        QCOMPARE(changed.count(), 1);

        // The last reason clearing is the falling edge.
        rig.model.setRemoteRfHold(TxChannel::RfHold::Mic, false);
        QVERIFY(!rig.model.isRemoteRfHeld());
        QVERIFY(!rig.tx.isRfHeld());
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.at(1).at(0).toBool(), false);
        rig.model.setRemoteRfHold(TxChannel::RfHold::Mic, false);
        QCOMPARE(changed.count(), 2);

        // MOX's return to receive clears every reason and emits (false).
        rig.model.setRemoteRfHold(TxChannel::RfHold::Link, true);
        rig.model.setRemoteRfHold(TxChannel::RfHold::Guard, true);
        QCOMPARE(changed.count(), 3);
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.moxController()->state() == MoxState::Rx, 5000);
        QVERIFY(!rig.model.isRemoteRfHeld());
        QCOMPARE(rig.tx.rfHoldMask(), 0u);
        QCOMPARE(changed.count(), 4);
        QCOMPARE(changed.at(3).at(0).toBool(), false);

        // Unheld at receive: the next return to receive emits nothing.
        rig.model.moxController()->setMox(true);
        pump();
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.moxController()->state() == MoxState::Rx, 5000);
        QCOMPARE(changed.count(), 4);
        QVERIFY2(!rig.conn.log.isEmpty(), "the MOX walk never reached the connection");
    }

    void aHoldLeavesTheTimeOutAlone()
    {
        // Two keys by a phone (180 s limit), the second with a 1 s hold:
        // the same remaining time at each tick, and both stop on the same
        // tick.
        std::vector<int> remaining[2];
        qint64 stoppedAt[2] = {-1, -1};
        for (int run = 0; run < 2; ++run) {
            ModelRig rig;
            QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
            rig.nowMs = 400;
            rig.keyedByPhone();
            rig.model.moxController()->setMox(true);
            pump();
            QVERIFY(rig.model.mox());
            QVERIFY(rig.model.txTimeOutTimer()->isTicking());
            for (qint64 t = kSecond; t <= 182 * kSecond && stopped.count() == 0; t += kSecond) {
                if (run == 1 && t == 10 * kSecond) {
                    rig.model.setRemoteRfHold(TxChannel::RfHold::Link, true);
                }
                if (run == 1 && t == 11 * kSecond) {
                    rig.model.setRemoteRfHold(TxChannel::RfHold::Link, false);
                }
                rig.tickUntil(t);
                if (stopped.count() > 0) {
                    stoppedAt[run] = t;
                } else {
                    QVERIFY(rig.model.txTimeOutTimer()->isTicking());
                    remaining[run].push_back(rig.model.timeOutRemainingSeconds());
                }
            }
            QCOMPARE(stopped.count(), 1);
        }
        QCOMPARE(remaining[1], remaining[0]);
        QVERIFY(stoppedAt[0] > 0);
        QCOMPARE(stoppedAt[1], stoppedAt[0]);
    }

    void holdOnWaitsForABlockInsideTheSend()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP");
#else
        ChannelRig rig;
        QVERIFY2(rig.up(&markInitialized), "the TX channel did not come up");

        // The worker's next block stops inside sendTxIq on the latch.
        const std::size_t before = rig.conn.blocks.size();
        rig.conn.blockNextSend = true;
        std::thread worker([&rig] { rig.drive(1); });
        if (!rig.conn.waitForSendEntered(std::chrono::seconds(10))) {
            rig.conn.openLatch();
            worker.join();
            QFAIL("the block never reached sendTxIq");
        }

        // The hold from a second thread waits for that block.
        std::atomic<bool> holdReturned{false};
        std::thread holder([&rig, &holdReturned] {
            rig.tx->setRfHold(TxChannel::RfHold::Link, true);
            holdReturned = true;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const bool returnedWhileLatched = holdReturned.load();
        rig.conn.openLatch();
        worker.join();
        holder.join();
        QVERIFY2(!returnedWhileLatched,
                 "setRfHold(on) returned while a block was inside sendTxIq");
        QVERIFY(holdReturned.load());
        QVERIFY(rig.tx->isRfHeld());
        QCOMPARE(rig.conn.blocks.size(), before + 1);

        // Every block accepted after the hold returned is silent.
        const auto after = rig.drive(kCountBlocks);
        QCOMPARE(static_cast<int>(after.size()), kCountBlocks);
        QCOMPARE(countNonZero(after), 0);
#endif
    }
};

QTEST_MAIN(TestTxRfHold)
#include "tst_tx_rf_hold.moc"
