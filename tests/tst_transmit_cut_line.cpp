// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_transmit_cut_line.cpp  (NereusSDR)
// =================================================================
//
// The transmit cut from a thread other than main (2026-10-15):
// RadioConnection's cut latch and TransmitCutLine, held by RadioModel.
//
//   cutRunsOnTheConnectionThreadWhileMainSleeps: cutTransmit() from a
//     std::thread while the main thread sleeps 500 ms runs MOX off and relay
//     off on the connection's own QThread within 50 ms; a second cut adds
//     nothing.
//   lineCutsAKeyedModelThenCatchUpRestores: with MOX keyed, the model's
//     cut line, cut from a std::thread while main sleeps 500 ms, closes the
//     RF gate and reaches the connection within 50 ms; catchUpTransmitCut
//     then gives one MOX-off walk and one transmitStopped with its text, no
//     key reaches the connection, the cut clears, and the next key reaches
//     the connection and opens the RF gate again.
//   catchUpWithNothingKeyedEmitsNothing: catchUpTransmitCut with nothing
//     keyed emits nothing and clears the cut.
//   clearedTargetsAreNotCut: once the model lets go of its connection and
//     TX channel, a cut reaches neither.
//   aServerGoneBeforeTheCutsPostClearsTheCut: the station server's cut
//     guard cuts while main sleeps, and the server is destroyed before its
//     post runs; the connection's cut is still cleared and the next key
//     reaches the connection.
//   lineHoldSetsOnlyTheGuardBit: hold(true) from a std::thread while main
//     sleeps sets only the TX channel's Guard hold bit: the gate stays
//     open and nothing reaches the connection; hold(false) clears it; cut()
//     still closes the gate and latches cutTransmit; a line with no
//     targets does nothing.
//   aServerGuardHoldsWithinTheHoldBound: the station server's cut guard,
//     armed and heard once while main sleeps, sets the Guard bit more than
//     kLinkHoldMs and at most kLinkHoldMs + 100 ms after the heard, with no
//     cut; a receipt clears it.
//   aServerGoneDuringAGuardHoldClearsTheGuardBit: the station server is
//     destroyed while its cut guard holds the TX channel; the Guard bit is
//     clear once the destructor returns, with no cut and the key still on.
//
// REALTIME: wall-clock bounds while the main thread is held.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-27: Propose unused isolated CW bridge RED bootstrap for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-15: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), remote transmit cut off the main thread, with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-15: review fix wave: the off-main side waits (bounded) for
//               the cut's entries before main reads them, and the key's
//               entries are in before a mark; a station server destroyed
//               before its cut's post still clears the cut. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: the stop bound is named
//               RemoteTxWatchdog::kLinkCutMs (renamed from
//               kLinkLossDeadlineMs). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: the line's hold sets only
//               the Guard bit, and the server's guard holds within
//               kLinkHoldMs + 100 ms. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut, fix wave A: a station server
//               destroyed during its guard's hold leaves the Guard bit
//               clear. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QThread>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <QEvent>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "RealtimeTestLoad.h"
#include "core/AppSettings.h"
#include "core/CwxSender.h"
#include "core/safety/RemoteCwSafetyState.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/safety/RemoteTxCutGuard.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/safety/TransmitCutLine.h"
#include "core/session/StationServer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "fakes/UpgradedCoreToken.h"

#include <QTemporaryDir>
#include <initializer_list>
#include <memory>

using namespace NereusSDR;

#ifdef NEREUS_BUILD_TESTS
namespace NereusSDR {
// Reuses existing guarded friendship; READS immutable real setup only.
struct CwxSenderMailboxTestAccess {
    static std::shared_ptr<RemoteCwSafetyState> safety(const CwxSender& s) { return s.m_remoteSafetyState; }
    static quint64 word(const CwxSender& s) { return s.m_remoteRequests.load(std::memory_order_acquire); }
};
struct TransmitCutLineTestAccess {
    static bool lifetimeLockAvailable(TransmitCutLine& line)
    {
        const std::unique_lock<std::mutex> lock(line.m_mutex, std::try_to_lock);
        return lock.owns_lock();
    }
};
} // namespace NereusSDR
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kMainHoldMs = 500;
constexpr double kReachBoundMs = 50.0;
// The bound tst_tx_link_loss_each_path uses (kBoundMs).
constexpr qint64 kCutBoundMs = RemoteTxWatchdog::kLinkCutMs + 100;
// The hold's bound, the same 100 ms past kLinkHoldMs.
constexpr qint64 kHoldBoundMs = RemoteTxWatchdog::kLinkHoldMs + 100;
constexpr quint32 kGuardBit = static_cast<quint32>(TxChannel::RfHold::Guard);

double msBetween(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Records MOX and relay writes with their thread and time, from any thread.
class RecordingConnection : public RadioConnection {
    Q_OBJECT
public:
    struct Entry {
        QString what;
        QThread* thread{nullptr};
        Clock::time_point at;
    };

    RecordingConnection() : RadioConnection(nullptr)
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
    void setMox(bool on) override { add(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off")); }
    void setTrxRelay(bool on) override
    {
        add(on ? QStringLiteral("relay on") : QStringLiteral("relay off"));
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

    std::vector<Entry> entries() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries;
    }
    // Entries recorded from index `from` on.
    std::vector<Entry> since(std::size_t from) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (from >= m_entries.size()) {
            return {};
        }
        return std::vector<Entry>(m_entries.begin() + static_cast<std::ptrdiff_t>(from),
                                  m_entries.end());
    }
    std::size_t count() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries.size();
    }

private:
    void add(const QString& what)
    {
        const Clock::time_point now = Clock::now();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back({what, QThread::currentThread(), now});
    }

    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
};

const RecordingConnection::Entry* findEntry(const std::vector<RecordingConnection::Entry>& list,
                                            const QString& what)
{
    for (const auto& e : list) {
        if (e.what == what) {
            return &e;
        }
    }
    return nullptr;
}

// On the calling thread (off main): waits, at most kReachBoundMs, until
// every one of `what` is among the entries from index `from` on. The main
// thread then reads them after it has proved they arrived off main, never
// racing the connection thread's queued writes.
void waitOffMainForEntries(const RecordingConnection& conn, std::size_t from,
                           std::initializer_list<const char*> what)
{
    const Clock::time_point until =
        Clock::now() + std::chrono::microseconds(static_cast<qint64>(kReachBoundMs * 1000.0));
    while (Clock::now() < until) {
        const std::vector<RecordingConnection::Entry> list = conn.since(from);
        bool all = true;
        for (const char* w : what) {
            if (findEntry(list, QString::fromLatin1(w)) == nullptr) {
                all = false;
                break;
            }
        }
        if (all) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

void pump(int passes = 8)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// Calls `fn` on a std::thread of its own, recording when it began, while the
// main thread sleeps kMainHoldMs without running its event loop.
Clock::time_point runOffMainWhileMainSleeps(const std::function<void()>& fn)
{
    std::atomic<qint64> startedNs{0};
    std::thread worker([&]() {
        startedNs.store(Clock::now().time_since_epoch().count());
        fn();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(kMainHoldMs));
    worker.join();
    return Clock::time_point(Clock::duration(startedNs.load()));
}

#ifdef NEREUS_BUILD_TESTS
struct BridgeLatch {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered{false};
    bool released{false};
    void enterAndWait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true; cv.notify_all();
        cv.wait(lock, [this]() { return released; });
    }
    void signalEntered()
    {
        {
            const std::lock_guard<std::mutex> lock(mutex);
            entered = true;
        }
        cv.notify_all();
    }
    bool waitEntered()
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(2), [this]() { return entered; });
    }
    void release()
    {
        { const std::lock_guard<std::mutex> lock(mutex); released = true; }
        cv.notify_all();
    }
};
struct BridgeLockProbe {
    std::atomic<bool> released{false}, cutLeft{false};
    std::atomic<bool> contended{false}, enteredWhileClosed{false};
    std::atomic<bool> acquiredAfterRelease{false}, returnedWhileClosed{false};
    std::atomic<bool> fallbackFired{false};
};
struct BridgeProbe {
    std::atomic<bool> mute{false}, entryMute{false}, entryCut{false};
    std::atomic<quint32> entries{0}, requestEpoch{0}, entryMask{0}, entryTxMask{0};
    std::atomic<quint32> recoveryCount{0}, recoveryEpoch{0};
};
class BridgeConnection : public RecordingConnection {
public:
    std::vector<RemoteCwSafetySnapshot> applies; // connection stays on main
protected:
    void applyRemoteRfHold() override { applies.push_back(remoteCwSafetyState()->snapshot()); }
};
struct RemoteBridgeRig {
    std::unique_ptr<BridgeConnection> conn{std::make_unique<BridgeConnection>()};
    std::unique_ptr<TxChannel> tx;
    std::unique_ptr<CwxSender> sender{std::make_unique<CwxSender>()};
    std::shared_ptr<TransmitCutLine> line;
    std::shared_ptr<RemoteCwSafetyState> safety{conn->remoteCwSafetyState()};
    std::shared_ptr<BridgeProbe> probe{std::make_shared<BridgeProbe>()};
    explicit RemoteBridgeRig(bool withTx = true,
        std::shared_ptr<TransmitCutLine> bridge = std::make_shared<TransmitCutLine>()) : line(std::move(bridge))
    {
        if (withTx) {
            tx = std::make_unique<TxChannel>(WdspEngine::kTxChannelId);
        }
        // Real installation takes sender mutex, BEFORE bridge reachability.
        sender->setRemoteSafetyState(safety);
        sender->setKeyLine(conn->cwKeyLine());
        sender->setRemoteRequestObserverForTest([state = safety, p = probe, c = conn.get(), t = tx.get()]
            (CwxSender::RemoteRequestKindForTest, quint32 epoch) noexcept {
                const RemoteCwSafetySnapshot snapshot = state->snapshot();
                p->requestEpoch.store(epoch); p->entryMask.store(snapshot.holdMask);
                p->entryTxMask.store(t ? t->rfHoldMask() : 0);
                p->entryCut.store(c->transmitCut()); p->entryMute.store(p->mute.load());
                p->entries.fetch_add(1);
            }); // actual nonvirtual ENTRY, no wait/Qt/mutex/re-entry
        line->setTargets(conn.get(), tx.get());
        line->setRemoteCwTargets(sender.get(), [p = probe](bool held) noexcept { p->mute.store(held); },
            [p = probe](quint32 epoch) noexcept { p->recoveryEpoch.store(epoch); p->recoveryCount.fetch_add(1); });
    }
    bool admit(quint32 epoch) { return line->beginRemoteCw(epoch); }
    void detach()
    {
        line->setRemoteCwTargets(nullptr, {}, {});
        line->setTargets(nullptr, nullptr);
    }
    ~RemoteBridgeRig() { detach(); } // before sender/connection/TX destruction
};
#endif

// A local model with a slice on 20 m USB, a recording connection on a
// QThread of its own (as the radio connection lives), a TX channel wrapper
// with no WDSP channel behind it wired for keying, and MoxController's walk
// driven by the event loop.
struct Rig {
    RadioModel model;
    RecordingConnection conn;
    QThread connThread;
    TxChannel tx{WdspEngine::kTxChannelId};

    Rig()
    {
        AppSettings::instance().clear();
        model.setCapsForTest(/*hasAlex=*/false);
        conn.moveToThread(&connThread);
        connThread.start();
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
    }
    ~Rig()
    {
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        connThread.quit();
        connThread.wait();
        AppSettings::instance().clear();
    }
};

} // namespace

class TestTransmitCutLine : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup()
    {
        AppSettings::instance().clear();
        RealtimeTestLoad::printLoadAverageIfFailed();
    }

    void cutRunsOnTheConnectionThreadWhileMainSleeps()
    {
        RecordingConnection conn;
        QThread connThread;
        conn.moveToThread(&connThread);
        connThread.start();

        const Clock::time_point cutAt = runOffMainWhileMainSleeps([&conn]() {
            conn.cutTransmit();
            waitOffMainForEntries(conn, 0, {"MOX off", "relay off"});
        });

        const auto entries = conn.entries();
        const auto* moxOff = findEntry(entries, QStringLiteral("MOX off"));
        const auto* relayOff = findEntry(entries, QStringLiteral("relay off"));
        qInfo("cut: MOX off +%.2f ms, relay off +%.2f ms (limit %.0f), entries %zu",
              moxOff ? msBetween(cutAt, moxOff->at) : -1.0,
              relayOff ? msBetween(cutAt, relayOff->at) : -1.0, kReachBoundMs, entries.size());
        QVERIFY2(moxOff && relayOff, "the cut did not reach the connection while main slept");
        QCOMPARE(entries.size(), std::size_t(2));
        QCOMPARE(entries.at(0).what, QStringLiteral("MOX off"));
        QCOMPARE(entries.at(1).what, QStringLiteral("relay off"));
        QCOMPARE(moxOff->thread, &connThread);
        QCOMPARE(relayOff->thread, &connThread);
        QVERIFY2(msBetween(cutAt, moxOff->at) < kReachBoundMs, "MOX off was late");
        QVERIFY2(msBetween(cutAt, relayOff->at) < kReachBoundMs, "relay off was late");
        QVERIFY2(msBetween(cutAt, relayOff->at) < double(kCutBoundMs), "past the cut bound");
        QVERIFY(conn.transmitCut());

        // A second cut adds nothing.
        std::thread again([&conn]() { conn.cutTransmit(); });
        again.join();
        QTest::qWait(100);
        QCOMPARE(conn.count(), std::size_t(2));
        QVERIFY(conn.transmitCut());

        // Cleared on its own thread; a later cut runs again.
        QMetaObject::invokeMethod(&conn, &RadioConnection::clearTransmitCut,
                                  Qt::QueuedConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!conn.transmitCut(), 2000);
        conn.cutTransmit();
        QTRY_COMPARE_WITH_TIMEOUT(conn.count(), std::size_t(4), 2000);

        connThread.quit();
        connThread.wait();
    }

    void lineCutsAKeyedModelThenCatchUpRestores()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.mox() && rig.tx.isRfGateOpen()
                                     && findEntry(rig.conn.entries(), QStringLiteral("MOX on")),
                                 5000);

        QSignalSpy moxSpy(rig.model.moxController(), &MoxController::moxStateChanged);
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        const std::size_t mark = rig.conn.count();
        const std::shared_ptr<TransmitCutLine> line = rig.model.transmitCutLine();
        QVERIFY(line != nullptr);

        const Clock::time_point cutAt = runOffMainWhileMainSleeps([line, &rig, mark]() {
            line->cut();
            waitOffMainForEntries(rig.conn, mark, {"MOX off", "relay off"});
        });

        // Before the main thread runs anything: RF shut, the connection cut.
        const bool gateOpenAfterCut = rig.tx.isRfGateOpen();
        const auto afterCut = rig.conn.since(mark);
        const auto* moxOff = findEntry(afterCut, QStringLiteral("MOX off"));
        const auto* relayOff = findEntry(afterCut, QStringLiteral("relay off"));
        qInfo("line cut: MOX off +%.2f ms, relay off +%.2f ms (limit %.0f)",
              moxOff ? msBetween(cutAt, moxOff->at) : -1.0,
              relayOff ? msBetween(cutAt, relayOff->at) : -1.0, kReachBoundMs);
        QVERIFY2(!gateOpenAfterCut, "the cut left the RF gate open");
        QVERIFY2(moxOff && relayOff, "the cut did not reach the connection while main slept");
        QCOMPARE(moxOff->thread, &rig.connThread);
        QVERIFY2(msBetween(cutAt, moxOff->at) < kReachBoundMs, "MOX off was late");
        QVERIFY2(msBetween(cutAt, relayOff->at) < kReachBoundMs, "relay off was late");
        QVERIFY(rig.conn.transmitCut());
        QVERIFY(rig.model.mox());   // the model has not caught up yet

        const QString text = QStringLiteral("Remote link lost");
        rig.model.catchUpTransmitCut(text);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.model.mox()
                                     && rig.model.moxController()->state() == MoxState::Rx,
                                 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 2000);
        pump();
        QTest::qWait(50);
        pump();

        int moxOffEdges = 0;
        for (const auto& args : moxSpy) {
            QCOMPARE(args.at(0).toBool(), false);
            ++moxOffEdges;
        }
        QCOMPARE(moxOffEdges, 1);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), text);
        QVERIFY2(!findEntry(rig.conn.since(mark), QStringLiteral("MOX on")),
                 "a key reached the connection after the cut");
        QVERIFY(!rig.tx.isRfGateOpen());

        // The next key keys normally: the connection hears it and the RF
        // gate the cut closed opens again.
        const std::size_t beforeKey = rig.conn.count();
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(findEntry(rig.conn.since(beforeKey), QStringLiteral("MOX on")),
                                 5000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen(), 5000);
        QVERIFY(rig.model.mox());
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.moxController()->state() == MoxState::Rx, 5000);
    }

    void catchUpWithNothingKeyedEmitsNothing()
    {
        Rig rig;
        pump();
        QSignalSpy moxSpy(rig.model.moxController(), &MoxController::moxStateChanged);
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.model.transmitCutLine()->cut();
        QVERIFY(rig.conn.transmitCut());
        QTRY_VERIFY_WITH_TIMEOUT(findEntry(rig.conn.entries(), QStringLiteral("relay off")), 2000);
        const std::size_t mark = rig.conn.count();

        rig.model.catchUpTransmitCut(QStringLiteral("Remote link lost"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 2000);
        pump();
        QTest::qWait(50);
        pump();

        QCOMPARE(moxSpy.count(), 0);
        QCOMPARE(stopped.count(), 0);
        QCOMPARE(rig.conn.count(), mark);
        QVERIFY(!rig.model.mox());
    }

    void clearedTargetsAreNotCut()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        // The key's own writes are in before the mark (they are queued to
        // the connection's thread and can land after the RF gate opens).
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen()
                                     && findEntry(rig.conn.entries(), QStringLiteral("MOX on"))
                                     && findEntry(rig.conn.entries(), QStringLiteral("relay on")),
                                 5000);
        const std::shared_ptr<TransmitCutLine> line = rig.model.transmitCutLine();

        rig.model.injectTxChannelForTest(nullptr);
        rig.model.injectConnectionForTest(nullptr);
        const std::size_t mark = rig.conn.count();
        line->cut();
        QTest::qWait(50);
        QVERIFY(rig.tx.isRfGateOpen());
        QVERIFY(!rig.conn.transmitCut());
        QCOMPARE(rig.conn.count(), mark);

        // Back on the model, the next cut reaches both.
        rig.model.injectConnectionForTest(&rig.conn);
        rig.model.injectTxChannelForTest(&rig.tx);
        line->cut();
        QVERIFY(!rig.tx.isRfGateOpen());
        QVERIFY(rig.conn.transmitCut());
        rig.model.catchUpTransmitCut(QString());
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut() && !rig.model.mox(), 5000);
    }

    // The station server's cut guard cuts a keyed model while main sleeps,
    // and the server is destroyed before the event loop runs the cut's post
    // (Qt drops it with the server). The server's destructor catches the
    // model up: the connection's cut is cleared, the model is unkeyed with
    // the watchdog's sentence, and the next key reaches the connection.
    void aServerGoneBeforeTheCutsPostClearsTheCut()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.mox() && rig.tx.isRfGateOpen()
                                     && findEntry(rig.conn.entries(), QStringLiteral("MOX on"))
                                     && findEntry(rig.conn.entries(), QStringLiteral("relay on")),
                                 5000);
        QTemporaryDir settingsDir;
        QTemporaryDir securityDir;
        QVERIFY(settingsDir.isValid());
        QVERIFY(securityDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        auto server = std::make_unique<StationServer>(
            &rig.model, settings, Test::seedCoreIdentity(securityDir.path()));
        // Without it the server leaves the model a receive-only station,
        // and the next key below would be refused for that.
        server->setRemoteTransmitAllowed(true);
        RemoteTxCutGuard* const guard = server->txCutGuard();
        QVERIFY(guard != nullptr);
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);

        // Armed for a media session and heard once, then silent while main
        // sleeps past the deadline: the guard cuts on its own thread and
        // posts to the server.
        constexpr quint64 kMediaEpoch = 7;
        guard->arm(kMediaEpoch);
        guard->heardOffMain(kMediaEpoch, Clock::now());
        QVERIFY(guard->isJudging());
        // No event loop here: the post must not run before the reset.
        const Clock::time_point until =
            Clock::now() + std::chrono::milliseconds(RemoteTxWatchdog::kLinkCutMs + 2000);
        while (Clock::now() < until && (guard->isJudging() || !rig.conn.transmitCut())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        QVERIFY(!guard->isJudging());
        QVERIFY(rig.conn.transmitCut());

        // Gone before its post runs.
        server.reset();

        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.model.mox()
                                     && rig.model.moxController()->state() == MoxState::Rx,
                                 5000);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(),
                 RemoteTxWatchdog::stopMessage(QStringLiteral("a device")));

        // The next key is TUNE: once a station server has run, the model
        // allows PC capture, and a MOX key here would wait for a microphone
        // this rig does not have; TUNE does not read it.
        const std::size_t beforeKey = rig.conn.count();
        rig.model.setTune(true);
        QTRY_VERIFY_WITH_TIMEOUT(findEntry(rig.conn.since(beforeKey), QStringLiteral("MOX on")),
                                 5000);
        rig.model.setTune(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.moxController()->state() == MoxState::Rx, 5000);
    }

#ifdef NEREUS_BUILD_TESTS
    void remoteBridgeFixtureSharesRealStateAndBinding()
    {
        RemoteBridgeRig rig;
        QCOMPARE(CwxSenderMailboxTestAccess::safety(*rig.sender).get(), rig.safety.get());
        QCOMPARE(rig.conn->remoteCwSafetyState().get(), rig.safety.get());
        QVERIFY(rig.admit(11));
        QVERIFY(rig.safety->snapshot().valid);
        QCOMPARE(rig.safety->snapshot().epoch, 11u);
        QCOMPARE(quint32(CwxSenderMailboxTestAccess::word(*rig.sender) >> 32), 11u);
        QCOMPARE(rig.sender->queuedCharacters(), 0); // reserved, NO accepted remote content
    }

    void remoteHoldMutesBeforeActualPauseEntry()
    {
        RemoteBridgeRig rig;
        QVERIFY(rig.admit(11)); // prerequisite, never RED
        // Hold actual sender mutex through existing listener, not a fake sender.
        const auto held = std::make_shared<BridgeLatch>();
        std::thread locker([&rig, held]() { rig.sender->setKeyListener([held](bool) { held->enterAndWait(); }); });
        const bool entered = held->waitEntered();
        const auto done = std::make_shared<BridgeLatch>();
        std::thread holder;
        if (entered) {
            holder = std::thread([&rig, done]() {
                rig.line->hold(11, true);
                { const std::lock_guard<std::mutex> lock(done->mutex); done->entered = true; }
                done->cv.notify_all();
            });
        }
        const bool returned = entered && done->waitEntered();
        held->release();
        locker.join();
        if (holder.joinable()) {
            holder.join();
        }
        rig.sender->setKeyListener({});
        QVERIFY2(entered, "actual sender mutex setup failed");
        QVERIFY2(returned, "bridge hold waited for sender mutex/main");
        QCOMPARE(rig.probe->entries.load(), 1u);
        QCOMPARE(rig.probe->requestEpoch.load(), 11u);
        QCOMPARE(rig.probe->entryMask.load(), kGuardBit);
        QCOMPARE(rig.probe->entryTxMask.load(), kGuardBit);
        QVERIFY(rig.probe->entryMute.load()); // intended missing-mute ordering RED
        QVERIFY(!rig.probe->entryCut.load());
        QCOMPARE(quint32(CwxSenderMailboxTestAccess::word(*rig.sender) & 3u), 1u);
    }

    void remoteHoldOffRequestsRecoveryWithoutUnmask()
    {
        RemoteBridgeRig rig;
        QVERIFY(rig.admit(11));
        QVERIFY(rig.conn->setRemoteCwHold(11, TxChannel::RfHold::Guard, true));
        rig.tx->setRfHold(TxChannel::RfHold::Guard, true);
        rig.probe->mute.store(true); // injected audio target's initial held state
        rig.line->hold(11, false);
        QCOMPARE(rig.probe->recoveryCount.load(), 1u); // intended recovery-request RED
        QCOMPARE(rig.probe->recoveryEpoch.load(), 11u);
        QCOMPARE(rig.safety->snapshot().holdMask, kGuardBit);
        QCOMPARE(rig.tx->rfHoldMask(), kGuardBit);
        QVERIFY(rig.probe->mute.load());
        QCOMPARE(rig.probe->entries.load(), 0u);
    }

    void remoteCutCheckpointPrecedesImmediateLatch()
    {
        RemoteBridgeRig rig;
        QVERIFY(rig.admit(11));
        const auto barrier = std::make_shared<BridgeLatch>();
        const auto captured = std::make_shared<std::atomic<quint32>>(0);
        rig.line->setRemoteCwCutValidatedObserverForTest([barrier, captured](quint32 epoch) {
            captured->store(epoch); barrier->enterAndWait();
        });
        std::thread cutter([&rig]() { rig.line->cut(11); });
        const bool entered = barrier->waitEntered();
        const bool invalidated = rig.safety->snapshot().invalidated;
        const bool latchBeforeRelease = rig.conn->transmitCut();
        const bool lockAvailable = entered && TransmitCutLineTestAccess::lifetimeLockAvailable(*rig.line);
        barrier->release(); cutter.join();
        QVERIFY2(entered, "actual invalidate-to-latch checkpoint missing");
        QCOMPARE(captured->load(), 11u);
        QVERIFY(invalidated); QVERIFY(!latchBeforeRelease); QVERIFY(!lockAvailable);
        QVERIFY(rig.conn->transmitCut()); // intended immediate-latch RED
        QCOMPARE(rig.probe->entries.load(), 1u);
        QVERIFY(rig.probe->entryCut.load()); QVERIFY(rig.probe->entryMute.load());
        QCOMPARE(quint32(CwxSenderMailboxTestAccess::word(*rig.sender) & 3u), 2u);
    }

    void replacedRemoteTargetsRejectStaleEpoch_data()
    {
        QTest::addColumn<int>("event");
        QTest::newRow("hold-on") << 0; QTest::newRow("hold-off") << 1; QTest::newRow("cut") << 2;
    }
    void replacedRemoteTargetsRejectStaleEpoch()
    {
        QFETCH(int, event);
        const auto line = std::make_shared<TransmitCutLine>();
        auto old = std::make_unique<RemoteBridgeRig>(true, line);
        QVERIFY(old->admit(11));
        const auto retiredProbe = old->probe;
        // Reserved binding has NO accepted sender content. This is fixture
        // retirement, NOT proof of cancel consumption/remote message cleanup.
        old->detach(); old->safety->finish(11); old.reset(); // main-thread destruction
        RemoteBridgeRig next(true, line);
        QVERIFY(next.admit(12));
        QVERIFY(next.conn->setRemoteCwHold(12, TxChannel::RfHold::Link, true));
        next.tx->setRfHold(TxChannel::RfHold::Mic, true);
        if (event == 0) {
            line->hold(11, true);
        } else if (event == 1) {
            line->hold(11, false);
        } else {
            line->cut(11);
        }
        QCOMPARE(next.safety->snapshot().epoch, 12u);
        QCOMPARE(next.safety->snapshot().holdMask, quint32(TxChannel::RfHold::Link));
        QVERIFY(next.safety->snapshot().valid); QVERIFY(!next.conn->transmitCut());
        QCOMPARE(next.tx->rfHoldMask(), quint32(TxChannel::RfHold::Mic));
        QCOMPARE(next.probe->entries.load(), 0u); QCOMPARE(next.probe->recoveryCount.load(), 0u);
        QVERIFY(!next.probe->mute.load()); QCOMPARE(retiredProbe->entries.load(), 0u);
    }

    void remoteHoldWithoutTxUsesSharedSuppression()
    {
        RemoteBridgeRig rig(false);
        QVERIFY(rig.admit(11));
        rig.line->hold(11, true);
        QCOMPARE(rig.probe->entryMask.load(), kGuardBit);
        QCOMPARE(rig.probe->entries.load(), 1u);
        QVERIFY(rig.probe->entryMute.load()); // intended no-TX mute RED
    }

    void queuedRemoteApplyReadsCurrentEpoch()
    {
        RemoteBridgeRig rig;
        QVERIFY(rig.admit(11));
        QVERIFY(rig.conn->setRemoteCwHold(11, TxChannel::RfHold::Guard, true));
        // Empty reserved binding ONLY: no accepted remote message is seeded.
        rig.safety->finish(11);
        QVERIFY(rig.admit(12));
        QCoreApplication::sendPostedEvents(rig.conn.get(), QEvent::MetaCall);
        QCOMPARE(rig.conn->applies.size(), std::size_t(1));
        QCOMPARE(rig.conn->applies.front().epoch, 12u);
        QCOMPARE(rig.conn->applies.front().holdMask, 0u); // state seam, NOT RF proof
    }

    void sameConnectionSuccessorRejectsOldCut()
    {
        RemoteBridgeRig rig;
        QVERIFY(rig.admit(11));
        QCOMPARE(rig.sender->queuedCharacters(), 0); // empty reserved binding only
        rig.safety->finish(11);
        QVERIFY(rig.admit(12));
        QVERIFY(rig.conn->setRemoteCwHold(12, TxChannel::RfHold::Link, true));
        rig.tx->setRfHold(TxChannel::RfHold::Mic, true);
        rig.line->cut(11); // SAME physical connection, captured stale epoch
        QCOMPARE(rig.safety->snapshot().epoch, 12u);
        QVERIFY(rig.safety->snapshot().valid);
        QCOMPARE(rig.safety->snapshot().holdMask, quint32(TxChannel::RfHold::Link));
        QCOMPARE(rig.tx->rfHoldMask(), quint32(TxChannel::RfHold::Mic));
        QCOMPARE(quint32(CwxSenderMailboxTestAccess::word(*rig.sender) >> 32), 12u);
        QCOMPARE(rig.probe->entries.load(), 0u);
        QVERIFY(!rig.conn->transmitCut());
    }

    void beginAndOldCutUseSameLifetimeCriticalSection()
    {
        RemoteBridgeRig rig;
        QVERIFY(rig.admit(11));
        const auto cut = std::make_shared<BridgeLatch>();
        const auto observed = std::make_shared<BridgeLockProbe>();
        const auto teardown = std::make_shared<BridgeLatch>();
        rig.line->setRemoteCwCutValidatedObserverForTest([cut, observed](quint32) {
            cut->enterAndWait();
            observed->cutLeft.store(true);
        });
        rig.line->setRemoteCwLockObserverForTest([cut, observed, teardown](TransmitCutLine::RemoteCwLockPointForTest point) {
            if (point == TransmitCutLine::RemoteCwLockPointForTest::BeginContended) {
                observed->contended.store(true);
                observed->enteredWhileClosed.store(!observed->released.load() && !observed->cutLeft.load());
                observed->released.store(true);
                cut->release(); // Actual failed try_lock already happened.
                teardown->signalEntered();
            } else if (point == TransmitCutLine::RemoteCwLockPointForTest::BeginAcquired) {
                observed->acquiredAfterRelease.store(observed->released.load() && observed->cutLeft.load());
            }
        });
        std::thread cutter([&rig]() { rig.line->cut(11); });
        const bool entered = cut->waitEntered();
        std::thread fallback;
        bool admitted12 = false;
        if (entered) {
            fallback = std::thread([cut, observed, teardown]() {
                if (!teardown->waitEntered()) {
                    observed->fallbackFired.store(true);
                    observed->released.store(true);
                    cut->release(); // Teardown ONLY; invalidates this fixture.
                }
            });
            // Empty reserved binding only: NOT accepted-message cleanup.
            rig.safety->finish(11);
            admitted12 = rig.line->beginRemoteCw(12); // MAIN, actual contended call
            observed->returnedWhileClosed.store(!observed->released.load());
        }
        // Cleanup must release/join even if fixture/acquisition regresses.
        observed->released.store(true);
        cut->release();
        teardown->signalEntered(); // Also cancel watchdog if main returned unexpectedly.
        cutter.join();
        if (fallback.joinable()) {
            fallback.join();
        }
        QVERIFY2(entered, "actual cut checkpoint missing");
        QVERIFY2(!observed->fallbackFired.load(), "watchdog teardown fired: invalid fixture, NOT RED");
        QVERIFY(observed->contended.load());
        QVERIFY(observed->enteredWhileClosed.load());
        QVERIFY(observed->acquiredAfterRelease.load());
        QVERIFY(!observed->returnedWhileClosed.load());
        QVERIFY(rig.conn->transmitCut()); // baseline first latch RED, NOT completed race GREEN
        QVERIFY(!admitted12);
        // Same-connection opposite order has its own real control above.
    }

    void detachWaitsForActualCutBeforeTargetDestruction()
    {
        RemoteBridgeRig rig;
        QVERIFY(rig.admit(11));
        const auto cut = std::make_shared<BridgeLatch>();
        const auto observed = std::make_shared<BridgeLockProbe>();
        const auto teardown = std::make_shared<BridgeLatch>();
        rig.line->setRemoteCwCutValidatedObserverForTest([cut, observed](quint32) {
            cut->enterAndWait();
            observed->cutLeft.store(true);
        });
        rig.line->setRemoteCwLockObserverForTest([cut, observed, teardown](TransmitCutLine::RemoteCwLockPointForTest point) {
            if (point == TransmitCutLine::RemoteCwLockPointForTest::TargetsContended) {
                observed->contended.store(true);
                observed->enteredWhileClosed.store(!observed->released.load() && !observed->cutLeft.load());
                observed->released.store(true);
                cut->release();
                teardown->signalEntered();
            } else if (point == TransmitCutLine::RemoteCwLockPointForTest::TargetsAcquired) {
                observed->acquiredAfterRelease.store(observed->released.load() && observed->cutLeft.load());
            }
        });
        std::thread cutter([&rig]() { rig.line->cut(11); });
        const bool entered = cut->waitEntered();
        std::thread fallback;
        if (entered) {
            fallback = std::thread([cut, observed, teardown]() {
                if (!teardown->waitEntered()) {
                    observed->fallbackFired.store(true);
                    observed->released.store(true);
                    cut->release(); // Teardown ONLY; invalidates this fixture.
                }
            });
            rig.detach(); // MAIN actual setter fails try_lock, then blocks on SAME mutex
            observed->returnedWhileClosed.store(!observed->released.load());
        }
        observed->released.store(true);
        cut->release();
        teardown->signalEntered(); // Also cancel watchdog if main returned unexpectedly.
        cutter.join();
        if (fallback.joinable()) {
            fallback.join();
        }
        QVERIFY2(entered, "actual cut checkpoint missing");
        QVERIFY2(!observed->fallbackFired.load(), "watchdog teardown fired: invalid fixture, NOT RED");
        QVERIFY(observed->contended.load());
        QVERIFY(observed->enteredWhileClosed.load());
        QVERIFY(observed->acquiredAfterRelease.load());
        QVERIFY(!observed->returnedWhileClosed.load());
        QVERIFY(rig.conn->transmitCut()); // baseline first latch RED; later lifetime assertions pending
        const auto retained = rig.line;
        const auto p = rig.probe;
        rig.sender.reset(); rig.tx.reset(); rig.conn.reset(); // MAIN after detach+join
        const quint32 mark = p->entries.load();
        retained->hold(11, true); retained->hold(11, false); retained->cut(11);
        QCOMPARE(p->entries.load(), mark);
    }
#endif

    void lineHoldSetsOnlyTheGuardBit()
    {
        // A line with no targets: hold does nothing.
        {
            TransmitCutLine empty;
            empty.hold(true);
            empty.hold(false);
        }

        Rig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen()
                                     && findEntry(rig.conn.entries(), QStringLiteral("MOX on"))
                                     && findEntry(rig.conn.entries(), QStringLiteral("relay on")),
                                 5000);
        const std::shared_ptr<TransmitCutLine> line = rig.model.transmitCutLine();
        QVERIFY(line != nullptr);
        QCOMPARE(rig.tx.rfHoldMask(), 0u);
        const std::size_t mark = rig.conn.count();

        std::atomic<quint32> maskAfterHold{0};
        std::atomic<bool> gateAfterHold{false};
        runOffMainWhileMainSleeps([line, &rig, &maskAfterHold, &gateAfterHold]() {
            line->hold(true);
            maskAfterHold.store(rig.tx.rfHoldMask());
            gateAfterHold.store(rig.tx.isRfGateOpen());
        });
        QCOMPARE(maskAfterHold.load(), kGuardBit);
        QVERIFY2(gateAfterHold.load(), "the hold closed the RF gate");
        QCOMPARE(rig.tx.rfHoldMask(), kGuardBit);
        QTest::qWait(50);
        QVERIFY(!rig.conn.transmitCut());
        QCOMPARE(rig.conn.count(), mark);
        QVERIFY(rig.model.mox());

        line->hold(false);
        QCOMPARE(rig.tx.rfHoldMask(), 0u);
        QVERIFY(rig.tx.isRfGateOpen());

        // cut() still closes the gate and latches cutTransmit.
        line->cut();
        QVERIFY(!rig.tx.isRfGateOpen());
        QVERIFY(rig.conn.transmitCut());
        rig.model.catchUpTransmitCut(QString());
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut() && !rig.model.mox(), 5000);
    }

    // The station server's guard holds the model's TX channel past
    // kLinkHoldMs of silence, off the main thread, without cutting; a
    // receipt gives RF back.
    void aServerGuardHoldsWithinTheHoldBound()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.mox() && rig.tx.isRfGateOpen()
                                     && findEntry(rig.conn.entries(), QStringLiteral("MOX on"))
                                     && findEntry(rig.conn.entries(), QStringLiteral("relay on")),
                                 5000);
        QTemporaryDir settingsDir;
        QTemporaryDir securityDir;
        QVERIFY(settingsDir.isValid());
        QVERIFY(securityDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        auto server = std::make_unique<StationServer>(
            &rig.model, settings, Test::seedCoreIdentity(securityDir.path()));
        server->setRemoteTransmitAllowed(true);
        RemoteTxCutGuard* const guard = server->txCutGuard();
        QVERIFY(guard != nullptr);
        const std::size_t mark = rig.conn.count();

        // Heard once, then silent while main sleeps with no event loop.
        constexpr quint64 kMediaEpoch = 7;
        guard->arm(kMediaEpoch);
        const Clock::time_point heard = Clock::now();
        guard->heardOffMain(kMediaEpoch, heard);
        const Clock::time_point until =
            heard + std::chrono::milliseconds(kHoldBoundMs + 500);
        while (Clock::now() < until && (rig.tx.rfHoldMask() & kGuardBit) == 0u) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const double heldAfter = msBetween(heard, Clock::now());
        qInfo("guard hold +%.2f ms after the heard (more than %lld, at most %lld)", heldAfter,
              static_cast<long long>(RemoteTxWatchdog::kLinkHoldMs),
              static_cast<long long>(kHoldBoundMs));
        QCOMPARE(rig.tx.rfHoldMask(), kGuardBit);
        QVERIFY(guard->isHolding());
        QVERIFY2(heldAfter > double(RemoteTxWatchdog::kLinkHoldMs)
                     && heldAfter <= double(kHoldBoundMs),
                 qPrintable(QStringLiteral("held %1 ms after the heard").arg(heldAfter)));
        QVERIFY(rig.tx.isRfGateOpen());
        QVERIFY(!rig.conn.transmitCut());
        QCOMPARE(rig.conn.count(), mark);

        // A receipt gives RF back, still off the main thread's event loop.
        guard->heardOffMain(kMediaEpoch, Clock::now());
        const Clock::time_point releaseBy = Clock::now() + std::chrono::milliseconds(100);
        while (Clock::now() < releaseBy && (rig.tx.rfHoldMask() & kGuardBit) != 0u) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        QCOMPARE(rig.tx.rfHoldMask(), 0u);
        QVERIFY(guard->isJudging());
        guard->disarm();
        QVERIFY(!rig.conn.transmitCut());

        server.reset();
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.moxController()->state() == MoxState::Rx, 5000);
    }

    // Fix wave A: the station server goes while its guard holds the model's
    // TX channel. The destructor releases the hold through the line before
    // it lets go of the line, so the Guard bit is clear when it returns,
    // nothing is cut and the key is untouched.
    void aServerGoneDuringAGuardHoldClearsTheGuardBit()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.mox() && rig.tx.isRfGateOpen()
                                     && findEntry(rig.conn.entries(), QStringLiteral("MOX on"))
                                     && findEntry(rig.conn.entries(), QStringLiteral("relay on")),
                                 5000);
        QTemporaryDir settingsDir;
        QTemporaryDir securityDir;
        QVERIFY(settingsDir.isValid());
        QVERIFY(securityDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        auto server = std::make_unique<StationServer>(
            &rig.model, settings, Test::seedCoreIdentity(securityDir.path()));
        server->setRemoteTransmitAllowed(true);
        RemoteTxCutGuard* const guard = server->txCutGuard();
        QVERIFY(guard != nullptr);
        const std::size_t mark = rig.conn.count();

        // Heard once, then silent past kLinkHoldMs with no event loop.
        constexpr quint64 kMediaEpoch = 7;
        guard->arm(kMediaEpoch);
        const Clock::time_point heard = Clock::now();
        guard->heardOffMain(kMediaEpoch, heard);
        const Clock::time_point until =
            heard + std::chrono::milliseconds(kHoldBoundMs + 500);
        while (Clock::now() < until && !guard->isHolding()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        QVERIFY(guard->isHolding());
        QCOMPARE(rig.tx.rfHoldMask(), kGuardBit);

        // Gone while holding.
        server.reset();
        QCOMPARE(rig.tx.rfHoldMask(), 0u);
        QVERIFY(rig.tx.isRfGateOpen());
        QVERIFY(!rig.conn.transmitCut());
        QCOMPARE(rig.conn.count(), mark);
        QVERIFY(rig.model.mox());

        rig.model.moxController()->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.moxController()->state() == MoxState::Rx, 5000);
    }
};

QTEST_MAIN(TestTransmitCutLine)
#include "tst_transmit_cut_line.moc"
