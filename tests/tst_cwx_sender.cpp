// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_cwx_sender.cpp  (NereusSDR)
// =================================================================
//
// CW transmit Task 8 (R-CW-15, R-CW-31 to R-CW-33): the CWX sender.
//   - With an injected clock and sleeper: element times, every key edge on
//     its schedule, speed marks, repeat, stop, the Key watchdog, the HL2
//     CWX PTT, break-in and MOX, refusals and the key line swap.
//   - One wall-clock case on the sender's own thread: each key edge within
//     2 ms of its schedule (REALTIME; the load average is logged).
//   - RadioModel with a fake connection: the request gate, the aborts, and
//     the key line released on teardown, on a connection swap and on a
//     transmit cut's catch-up.
// Nothing here opens a radio socket or keys a radio: the key lines are the
// fake connections' own, and the fake connections send nothing.
//
// Modification history (NereusSDR):
//   2026-10-29: Preallocate wall-clock edge recording and report the
//               sender's actual Linux scheduling policy after timing.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: Propose actual atomic mailbox behavioral RED cases for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: F7 review correction: stop the model during a controlled
//               Off/no-MOX post-message hang without a stop announcement.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: Final fix wave F13: HL2 in Off never drives CWX or its PTT
//               without MOX, including the message hang and held Key.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: Final fix wave: quiet logical Off stops and a keyed MOX positive control.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: CW transmit, the CWX sender. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
//   2026-10-27: CW transmit, the Key's timed release for the TCI keyer
//               (releaseKeyAfter) and keyHeldChanged. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#if !defined(Q_OS_WIN)
#include <stdlib.h>
#endif
#if defined(Q_OS_LINUX)
#include <pthread.h>
#include <sched.h>
#endif

#include "OperatorWording.h"
#include "RealtimeTestLoad.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/CwKeyLine.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/safety/RemoteCwSafetyState.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using namespace std::chrono_literals;
using SteadyTime = std::chrono::steady_clock::time_point;

#ifdef NEREUS_BUILD_TESTS
namespace NereusSDR {
// Read-only mailbox observation; take() calls the REAL production helper
// under its actual mutex. No test writes/seeds mailbox or remote content.
struct CwxSenderMailboxTestAccess {
    static quint64 word(const CwxSender& sender)
    {
        return sender.m_remoteRequests.load(std::memory_order_acquire);
    }
    static quint32 epoch(const CwxSender& sender) { return quint32(word(sender) >> 32); }
    static quint32 pending(const CwxSender& sender) { return quint32(word(sender) & 3u); }
    static bool wake(const CwxSender& sender) { return sender.m_wake.load(std::memory_order_acquire); }
    static quint32 take(CwxSender& sender, quint32 expectedEpoch)
    {
        const std::lock_guard<std::mutex> lock(sender.m_mutex);
        return sender.takeRemoteRequestsLocked(expectedEpoch);
    }
};
} // namespace NereusSDR
#endif

namespace {

#ifdef NEREUS_BUILD_TESTS
// Harness wait bounds are synchronization watchdogs, not RF policy timers.
struct MailboxLatch {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered{false};
    bool released{false};
    std::atomic<quint64> candidate{0};
    void enterAndWait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        cv.notify_all();
        cv.wait(lock, [this]() { return released; });
    }
    bool waitEntered()
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, 2s, [this]() { return entered; });
    }
    void release()
    {
        { const std::lock_guard<std::mutex> lock(mutex); released = true; }
        cv.notify_all();
    }
};
struct HeldSenderMutex {
    CwxSender& sender;
    std::shared_ptr<MailboxLatch> latch{std::make_shared<MailboxLatch>()};
    std::thread thread;
    explicit HeldSenderMutex(CwxSender& value) : sender(value), thread([this]() {
        sender.setKeyListener([latch = latch](bool) { latch->enterAndWait(); });
    }) {}
    void release()
    {
        latch->release();
        if (thread.joinable()) { thread.join(); }
        sender.setKeyListener({});
    }
    ~HeldSenderMutex() { release(); }
};
#endif

// ── The injected clock and sleeper ─────────────────────────────────────────
// The sleeper moves the clock to the deadline at once. A hook may act
// first (a press, a stop) and say the sleep was cut short. While frozen the
// sleeper waits until thawed, so a test can act between two elements.
class FakeTime {
public:
    SteadyTime start{SteadyTime{} + 1000h};

    SteadyTime now()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_now;
    }
    void set(SteadyTime t)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_now = t;
    }
    // Return true: the sleep ended early (the clock is where the hook left it).
    void setHook(std::function<bool(SteadyTime until)> hook) { m_hook = std::move(hook); }
    void freeze()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_frozen = true;
    }
    // One-shot barrier before advancing to this element deadline. The
    // preceding tick has finished when frozenBefore() observes the sleeper.
    void freezeBefore(SteadyTime until)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_freezeBefore = until;
    }
    bool frozenBefore(SteadyTime until)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_frozen && m_sleepingUntil == until;
    }
    // Observe the sleeper's actual parked state without pumping Qt's
    // queued callbacks or assuming freeze() already stopped its first tick.
    bool waitUntilFrozen()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_cv.wait_for(lock, 2s, [this]() {
            return m_frozen && m_sleepingUntil.has_value();
        });
    }
    void thaw()
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_frozen = false;
            m_freezeBefore.reset(); // Teardown also cancels a not-yet-reached barrier.
        }
        m_cv.notify_all();
    }
    void sleep(SteadyTime until)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_freezeBefore == until) {
                m_freezeBefore.reset();
                m_frozen = true;
            }
            m_sleepingUntil = until;
            m_cv.notify_all();
            m_cv.wait(lock, [this] { return !m_frozen; });
            m_sleepingUntil.reset();
        }
        if (m_hook && m_hook(until)) {
            return;
        }
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (m_now < until) {
            m_now = until;
        }
    }
    qint64 ms(SteadyTime t) const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(t - start).count();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    SteadyTime m_now{SteadyTime{} + 1000h};
    bool m_frozen{false};
    std::optional<SteadyTime> m_freezeBefore;
    std::optional<SteadyTime> m_sleepingUntil;
    std::function<bool(SteadyTime)> m_hook;
};

// One state of the key line after a change.
struct Snap {
    qint64 ms{0};
    bool cwx{false};
    bool ptt{false};
    bool operator==(const Snap& o) const { return ms == o.ms && cwx == o.cwx && ptt == o.ptt; }
};

class Recorder {
public:
    void attach(const std::shared_ptr<CwKeyLine>& line, std::function<qint64()> now)
    {
        m_line = line;
        line->setOnChange([this, now = std::move(now)]() {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_snaps.push_back(Snap{now(), m_line->cwx(), m_line->cwxPtt()});
        });
    }
    std::vector<Snap> snaps() const
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_snaps;
    }
    // The CWX key's edges: (ms, down).
    std::vector<std::pair<qint64, bool>> keyEdges() const
    {
        std::vector<std::pair<qint64, bool>> out;
        bool last = false;
        for (const Snap& s : snaps()) {
            if (s.cwx != last) {
                out.emplace_back(s.ms, s.cwx);
                last = s.cwx;
            }
        }
        return out;
    }
    bool anyPtt() const
    {
        for (const Snap& s : snaps()) {
            if (s.ptt) {
                return true;
            }
        }
        return false;
    }

private:
    std::shared_ptr<CwKeyLine> m_line;
    mutable std::mutex m_mutex;
    std::vector<Snap> m_snaps;
};

// Morse for the expected schedules, written out here on its own (not read
// from the sender): dot one element, dash three, one element between the
// marks of a character, three between characters, seven between words.
QString morseOf(QChar c)
{
    switch (c.toUpper().unicode()) {
    case 'A': return QStringLiteral(".-");
    case 'E': return QStringLiteral(".");
    case 'I': return QStringLiteral("..");
    case 'P': return QStringLiteral(".--.");
    case 'R': return QStringLiteral(".-.");
    case 'S': return QStringLiteral("...");
    case 'T': return QStringLiteral("-");
    default: return QString();
    }
}

// The key edges of `text` at `elementMs`, from 0.
std::vector<std::pair<qint64, bool>> expectedEdges(const QString& text, qint64 elementMs)
{
    std::vector<std::pair<qint64, bool>> out;
    qint64 t = 0;
    for (const QChar c : text) {
        if (c == QLatin1Char(' ')) {
            t += 4 * elementMs;   // with the 3 after the last character, 7
            continue;
        }
        const QString m = morseOf(c);
        for (const QChar e : m) {
            out.emplace_back(t, true);
            t += (e == QLatin1Char('-') ? 3 : 1) * elementMs;
            out.emplace_back(t, false);
            t += elementMs;
        }
        t += 2 * elementMs;
    }
    return out;
}

// A sender on the fake clock, on its own line, break-in active.
struct Bench {
    FakeTime time;
    std::shared_ptr<CwKeyLine> line{std::make_shared<CwKeyLine>()};
    Recorder rec;
    std::unique_ptr<CwxSender> sender;

    Bench()
    {
        rec.attach(line, [this]() { return time.ms(time.now()); });
        sender = std::make_unique<CwxSender>([this]() { return time.now(); },
                                             [this](SteadyTime until) { time.sleep(until); });
        sender->setKeyLine(line);
        sender->setBreakInActive(true);
        sender->setSpeedWpm(20);
    }
    ~Bench()
    {
        time.thaw();
        sender.reset();
    }
    bool idle() const { return !sender->isSending(); }
};

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

double loadAverage1()
{
#if defined(Q_OS_WIN)
    return -1.0;
#else
    double load[3] = {0.0, 0.0, 0.0};
    return getloadavg(load, 3) == 3 ? load[0] : -1.0;
#endif
}

// ── RadioModel with a fake connection ──────────────────────────────────────
class CwxMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CwxMockConnection(QObject* parent = nullptr)
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

// Counts the changes on a connection's key line and whether CWX ever went
// down. The sender in RadioModel runs on the real clock.
struct LineWatch {
    std::shared_ptr<CwKeyLine> line;
    std::atomic<int> changes{0};
    std::atomic<bool> keyed{false};

    explicit LineWatch(std::shared_ptr<CwKeyLine> l)
        : line(std::move(l))
    {
        line->setOnChange([this]() {
            changes.fetch_add(1);
            if (line->cwx()) {
                keyed.store(true);
            }
        });
    }
};

// A local model on a P2 OrionMKII with a fake connection, one slice on
// 20 m CWU, Semi break-in, CWX at 60 WPM.
struct Rig {
    FakeTime* const time;
    RadioModel model;
    CwxMockConnection conn;
    SliceModel* slice{nullptr};

    explicit Rig(FakeTime* testTime = nullptr)
        : time(testTime)
        , model(RadioModel::Role::Local,
                testTime ? std::make_unique<CwxSender>(
                    [testTime]() { return testTime->now(); },
                    [testTime](SteadyTime until) { testTime->sleep(until); }) : nullptr,
                nullptr)
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
        pump();
    }
    ~Rig()
    {
        if (time != nullptr) {
            model.cwxSender()->stop();
            time->thaw(); // Always unblock before model destruction joins.
        }
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }
    CwxSender* cwx() { return model.cwxSender(); }
};

} // namespace

class TestCwxSender : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup()
    {
        RealtimeTestLoad::printLoadAverageIfFailed();
        AppSettings::instance().clear();
    }

#ifdef NEREUS_BUILD_TESTS
    void remoteMailboxPublicationWhileActualSenderMutexHeld_data()
    {
        QTest::addColumn<int>("sequence");
        QTest::addColumn<quint32>("expectedFlags");
        QTest::newRow("pause") << 0 << quint32{1};
        QTest::newRow("cancel") << 1 << quint32{2};
        QTest::newRow("cancel-after-invalidate") << 2 << quint32{2};
        QTest::newRow("pause-then-cancel") << 3 << quint32{2};
        QTest::newRow("cancel-then-pause") << 4 << quint32{2};
    }
    void remoteMailboxPublicationWhileActualSenderMutexHeld()
    {
        QFETCH(int, sequence);
        QFETCH(quint32, expectedFlags);
        CwxSender sender;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        safety->begin(11);
        QVERIFY(safety->snapshot().valid); // prerequisite, never RED
        sender.setRemoteSafetyState(safety);
        sender.bindRemoteActivity(11);
        QCOMPARE(CwxSenderMailboxTestAccess::epoch(sender), 11u); // setup
        QCOMPARE(CwxSenderMailboxTestAccess::pending(sender), 0u); // setup
        QVERIFY(!CwxSenderMailboxTestAccess::wake(sender)); // no inherited wake
        struct EntryProbe {
            std::atomic<unsigned> entries{0};
            std::atomic<quint32> epoch{0};
        };
        const auto entry = std::make_shared<EntryProbe>();
        sender.setRemoteRequestObserverForTest([entry](CwxSender::RemoteRequestKindForTest,
                                                      quint32 epoch) noexcept {
            entry->epoch.store(epoch);
            entry->entries.fetch_add(1);
        });
        if (sequence == 2) { QVERIFY(safety->invalidate(11)); }
        HeldSenderMutex held(sender);
        QVERIFY2(held.latch->waitEntered(), "fixture failed to hold real sender mutex");
        const auto done = std::make_shared<MailboxLatch>();
        std::thread requester([&]() {
            if (sequence == 0) { sender.requestRemotePause(11); }
            else if (sequence == 1 || sequence == 2) { sender.requestRemoteCancel(11); }
            else if (sequence == 3) {
                sender.requestRemotePause(11); sender.requestRemoteCancel(11);
            } else {
                sender.requestRemoteCancel(11); sender.requestRemotePause(11);
            }
            { const std::lock_guard<std::mutex> lock(done->mutex); done->entered = true; }
            done->cv.notify_all();
        });
        const bool returnedWhileHeld = done->waitEntered();
        const quint64 publishedWhileHeld = CwxSenderMailboxTestAccess::word(sender);
        const bool wakeWhileHeld = CwxSenderMailboxTestAccess::wake(sender);
        held.release();
        requester.join();
        QVERIFY2(returnedWhileHeld, "request required sender mutex/main progress");
        QCOMPARE(entry->entries.load(), sequence < 3 ? 1u : 2u); // ENTRY control only
        QCOMPARE(entry->epoch.load(), 11u); // ENTRY control only
        QCOMPARE(quint32(publishedWhileHeld >> 32), 11u);
        QCOMPARE(quint32(publishedWhileHeld & 3u), expectedFlags); // REAL publication RED
        QVERIFY(wakeWhileHeld); // publication wake is not consumption
    }

    void remoteBindAndPublicationDoNotWaitForSenderMutex()
    {
        CwxSender sender;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        safety->begin(11);
        QVERIFY(safety->snapshot().valid);
        sender.setRemoteSafetyState(safety); // installation OUTSIDE stall
        HeldSenderMutex held(sender);
        QVERIFY2(held.latch->waitEntered(), "fixture failed to hold real sender mutex");
        const auto done = std::make_shared<MailboxLatch>();
        std::thread binder([&]() {
            sender.bindRemoteActivity(11); sender.requestRemotePause(11);
            { const std::lock_guard<std::mutex> lock(done->mutex); done->entered = true; }
            done->cv.notify_all();
        });
        const bool returnedWhileHeld = done->waitEntered();
        const quint64 word = CwxSenderMailboxTestAccess::word(sender);
        held.release(); binder.join();
        QVERIFY2(returnedWhileHeld, "binding/publication waited for sender mutex");
        QCOMPARE(quint32(word >> 32), 11u); // atomic binding control
        QCOMPARE(quint32(word & 3u), 1u); // REAL publication RED
    }

    void remoteStaleRequestsAfterBindTwelvePreserveTwelve()
    {
        CwxSender sender;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        safety->begin(12);
        QVERIFY(safety->snapshot().valid);
        sender.setRemoteSafetyState(safety); sender.bindRemoteActivity(12);
        QCOMPARE(CwxSenderMailboxTestAccess::epoch(sender), 12u);
        sender.requestRemoteCancel(12);
        QCOMPARE(CwxSenderMailboxTestAccess::pending(sender), 2u); // publication prerequisite
        sender.requestRemotePause(11); sender.requestRemoteCancel(11);
        sender.cancelRemoteActivity(11); // actual public cancellation entry
        QCOMPARE(CwxSenderMailboxTestAccess::epoch(sender), 12u);
        QCOMPARE(CwxSenderMailboxTestAccess::pending(sender), 2u); // stale behavior
        sender.bindRemoteActivity(12);
        QCOMPARE(CwxSenderMailboxTestAccess::pending(sender), 2u); // idempotent bind
    }

    void remoteCapturedElevenCannotPublishOverTwelve()
    {
        CwxSender sender;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        safety->begin(11);
        QVERIFY(safety->snapshot().valid);
        sender.setRemoteSafetyState(safety); sender.bindRemoteActivity(11);
        QCOMPARE(CwxSenderMailboxTestAccess::epoch(sender), 11u);
        const auto barrier = std::make_shared<MailboxLatch>();
        sender.setRemoteMailboxBeforeCasObserverForTest([barrier](
            CwxSender::RemoteMailboxCasKindForTest kind, quint32 epoch, quint64 candidate) {
            if (kind == CwxSender::RemoteMailboxCasKindForTest::Pause && epoch == 11) {
                barrier->candidate.store(candidate);
                barrier->enterAndWait();
            }
        });
        std::thread old([&]() { sender.requestRemotePause(11); });
        const bool entered = barrier->waitEntered();
        if (entered) {
            safety->finish(11); safety->begin(12); sender.bindRemoteActivity(12);
            sender.requestRemoteCancel(12);
        }
        const quint64 beforeRelease = CwxSenderMailboxTestAccess::word(sender);
        barrier->release(); old.join();
        QVERIFY2(entered, "missing actual candidate-read checkpoint; not RED");
        QCOMPARE(quint32(barrier->candidate.load() >> 32), 11u);
        QCOMPARE(quint32(beforeRelease >> 32), 12u);
        QCOMPARE(quint32(beforeRelease & 3u), 2u); // publication prerequisite
        QCOMPARE(CwxSenderMailboxTestAccess::word(sender), beforeRelease); // stale CAS behavior
    }

    void remoteTakeRetainsBindingAndAllowsLaterCancel()
    {
        CwxSender sender;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        safety->begin(11);
        QVERIFY(safety->snapshot().valid);
        sender.setRemoteSafetyState(safety); sender.bindRemoteActivity(11);
        QCOMPARE(CwxSenderMailboxTestAccess::epoch(sender), 11u);
        sender.requestRemotePause(11);
        QCOMPARE(CwxSenderMailboxTestAccess::pending(sender), 1u); // prerequisite
        QCOMPARE(CwxSenderMailboxTestAccess::take(sender, 11), 1u); // consumption RED wave
        QCOMPARE(CwxSenderMailboxTestAccess::epoch(sender), 11u);
        QCOMPARE(CwxSenderMailboxTestAccess::pending(sender), 0u);
        QVERIFY(safety->invalidate(11));
        sender.requestRemoteCancel(11); // invalidated shared value still bound
        QCOMPARE(CwxSenderMailboxTestAccess::pending(sender), 2u);
        QCOMPARE(CwxSenderMailboxTestAccess::take(sender, 11), 2u);
        QCOMPARE(CwxSenderMailboxTestAccess::epoch(sender), 11u);
    }

    void remoteStaleTakeCannotClearTwelve()
    {
        CwxSender sender;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        safety->begin(11);
        QVERIFY(safety->snapshot().valid);
        sender.setRemoteSafetyState(safety); sender.bindRemoteActivity(11);
        sender.requestRemotePause(11);
        QCOMPARE(CwxSenderMailboxTestAccess::pending(sender), 1u); // prerequisite
        const auto barrier = std::make_shared<MailboxLatch>();
        sender.setRemoteMailboxBeforeCasObserverForTest([barrier](
            CwxSender::RemoteMailboxCasKindForTest kind, quint32 epoch, quint64 candidate) {
            if (kind == CwxSender::RemoteMailboxCasKindForTest::Consume && epoch == 11) {
                barrier->candidate.store(candidate);
                barrier->enterAndWait();
            }
        });
        quint32 taken = 0;
        std::thread consumer([&]() { taken = CwxSenderMailboxTestAccess::take(sender, 11); });
        const bool entered = barrier->waitEntered();
        if (entered) {
            safety->finish(11); safety->begin(12); sender.bindRemoteActivity(12);
            sender.requestRemoteCancel(12); // must not need consumer's mutex
        }
        const quint64 twelve = CwxSenderMailboxTestAccess::word(sender);
        barrier->release(); consumer.join();
        QVERIFY2(entered, "missing actual consumption checkpoint; not RED");
        QCOMPARE(quint32(barrier->candidate.load() >> 32), 11u);
        QCOMPARE(quint32(twelve >> 32), 12u);
        QCOMPARE(quint32(twelve & 3u), 2u); // prerequisite
        QCOMPARE(taken, 0u); // stale consumption may not apply11
        QCOMPARE(CwxSenderMailboxTestAccess::word(sender), twelve);
    }

    // Real accepted FIFO content, observed through its actual key line.
    void remoteHoldRestartsOnlyUnfinishedLetter()
    {
        Bench b;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        b.sender->setRemoteSafetyState(safety);
        QSignalSpy starts(b.sender.get(), &CwxSender::characterStarted);
        b.time.freezeBefore(b.time.start + 300ms); // E complete; first T mark at240
        QVERIFY(b.sender->sendRemoteText(11, QStringLiteral("ET"), [&]() {
            safety->begin(11); b.sender->bindRemoteActivity(11); return true;
        }));
        QTRY_VERIFY(b.time.frozenBefore(b.time.start + 300ms));
        QVERIFY(b.line->cwx());
        QVERIFY(safety->setHold(11, TxChannel::RfHold::Link));
        QVERIFY(b.sender->pauseRemoteActivity(11));
        QVERIFY(!b.line->cwx());
        b.time.set(b.time.now() + 1s);
        QVERIFY(b.sender->prepareRemoteResume(11));
        QVERIFY(safety->clearHold(11, TxChannel::RfHold::Link));
        QVERIFY(b.sender->resumeRemoteActivity(11));
        b.time.thaw();
        QTRY_VERIFY(b.idle());
        pump();
        QCOMPARE(starts.count(), 2); // replay is not another accepted character
        const auto edges = b.rec.keyEdges();
        QCOMPARE(edges.size(), size_t{6});
        QCOMPARE(edges[0].first, qint64{0});
        QCOMPARE(edges[1].first, qint64{60});
        QVERIFY(edges[4].second);
        QVERIFY(!edges[5].second);
        QCOMPARE(edges[5].first - edges[4].first, qint64{180});
    }

    void completedRemoteOwnerCannotReleaseNewLocalContent()
    {
        FakeTime time;
        Rig rig(&time);
        time.freeze();
        const QByteArray device("retired-window");
        QVERIFY(rig.model.requestRemoteCwxFor(device, false,
            [](CwxSender& sender, quint32 epoch, const std::function<bool()>& admit) {
                return sender.sendRemoteText(epoch, QStringLiteral("E"), admit);
            }).isEmpty());
        QCOMPARE(rig.model.cwxSendingFor(), device);
        rig.cwx()->stop(); // ended callback intentionally remains queued
        QVERIFY(rig.cwx()->sendText(QStringLiteral("ET"))); // actual local0 FIFO
        // The first element may legitimately start before the frozen
        // sleeper is reached. Park before taking the exact FIFO snapshot;
        // waiting on the CV leaves the old ended callback queued.
        QVERIFY2(time.waitUntilFrozen(), "sender did not reach the frozen sleeper");
        const int queued = rig.cwx()->queuedCharacters();
        QVERIFY(queued > 0);
        QVERIFY(rig.model.cwxSendingFor().isEmpty());
        QVERIFY(!rig.model.cwxActiveFor(device));
        rig.model.stopCwxFor(device); // old session/Stop scope
        QVERIFY(rig.cwx()->isSending());
        QCOMPARE(rig.cwx()->queuedCharacters(), queued);
    }

    void remoteRejectedReplacementPreservesAcceptedContent()
    {
        Bench b;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        b.sender->setRemoteSafetyState(safety);
        b.time.freeze();
        QVERIFY(b.sender->sendRemoteText(11, QStringLiteral("EE"), [&]() {
            safety->begin(11); b.sender->bindRemoteActivity(11); return true;
        }));
        QTRY_COMPARE(b.sender->queuedCharacters(), 1);
        b.sender->setMemories({QStringLiteral("T")});
        const int queued = b.sender->queuedCharacters();
        QVERIFY(!b.sender->sendRemoteMemory(12, 1, []() { return false; }));
        QCOMPARE(b.sender->acceptedRemoteEpoch(), 11u);
        QCOMPARE(safety->snapshot().epoch, 11u);
        QCOMPARE(b.sender->queuedCharacters(), queued);
        QVERIFY(b.line->cwx());
        QVERIFY(b.sender->isSending());
    }

    void remoteStopWhilePausedCannotResume()
    {
        Bench b;
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        b.sender->setRemoteSafetyState(safety);
        b.time.freeze();
        QVERIFY(b.sender->sendRemoteText(11, QStringLiteral("TT"), [&]() {
            safety->begin(11); b.sender->bindRemoteActivity(11); return true;
        }));
        QTRY_VERIFY(b.line->cwx());
        QVERIFY(safety->setHold(11, TxChannel::RfHold::Link));
        QVERIFY(b.sender->pauseRemoteActivity(11));
        b.sender->stop();
        QVERIFY(!b.sender->prepareRemoteResume(11));
        QVERIFY(!b.sender->resumeRemoteActivity(11));
        QCOMPARE(b.sender->acceptedRemoteEpoch(), 0u);
        QVERIFY(!b.line->cwx());
    }

    void remoteUnboundRequestsDoNotAdoptLocalWork()
    {
        Bench b;
        b.time.freeze();
        QVERIFY(b.sender->sendText(QStringLiteral("EE"))); // ACTUAL local work
        QTRY_COMPARE(b.sender->queuedCharacters(), 1); // first element, then frozen
        const int queued = b.sender->queuedCharacters();
        QVERIFY(queued > 0);
        b.sender->requestRemotePause(11); b.sender->requestRemoteCancel(11);
        QCOMPARE(CwxSenderMailboxTestAccess::word(*b.sender), quint64{0});
        QCOMPARE(b.sender->queuedCharacters(), queued);
        QVERIFY(b.sender->isSending());
    }

    void remoteBareBindingAndOldCancelLeaveLocalQueueAlone()
    {
        Bench b;
        b.time.freeze();
        QVERIFY(b.sender->sendText(QStringLiteral("EE")));
        QTRY_COMPARE(b.sender->queuedCharacters(), 1); // first element, then frozen
        const int queued = b.sender->queuedCharacters();
        QVERIFY(queued > 0);
        const auto safety = std::make_shared<RemoteCwSafetyState>();
        safety->begin(12);
        QVERIFY(safety->snapshot().valid);
        b.sender->setRemoteSafetyState(safety); b.sender->bindRemoteActivity(12);
        QCOMPARE(CwxSenderMailboxTestAccess::epoch(*b.sender), 12u);
        b.sender->cancelRemoteActivity(11);
        QCOMPARE(b.sender->queuedCharacters(), queued);
        QVERIFY(b.sender->isSending());
    }
#endif

    // ── Element times ──────────────────────────────────────────────────────
    void elementTimeIs1200OverWpm()
    {
        QCOMPARE(CwxSender::elementMs(20), 60);
        QCOMPARE(CwxSender::elementMs(22), 54);   // whole ms, as Thetis
        QCOMPARE(CwxSender::elementMs(30), 40);
        QCOMPARE(CwxSender::elementMs(60), 20);
        QCOMPARE(CwxSender::elementMs(5), 240);
        QCOMPARE(CwxSender::elementMs(1), 240);    // clamped to 5
        QCOMPARE(CwxSender::elementMs(99), 20);    // clamped to 60
    }

    void eAt20KeysSixtyMsThenTheCharacterGap()
    {
        Bench b;
        const SteadyTime t0 = b.time.start;
        b.time.set(t0);
        QVERIFY(b.sender->sendText(QStringLiteral("EE")));
        QTRY_VERIFY(b.idle());
        using Edges = std::vector<std::pair<qint64, bool>>;
        // E: 60 ms down; the next character 3 elements after the key up.
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {60, false}, {240, true}, {300, false}}));
        QVERIFY(!b.rec.anyPtt());
        // The hang: idle 300 ms after the last key down.
        QCOMPARE(b.time.ms(b.time.now()), qint64(540));
    }

    void parisAt20TakesFiftyElementsOnSchedule()
    {
        Bench b;
        b.time.set(b.time.start);
        QVERIFY(b.sender->sendText(QStringLiteral("PARIS E")));
        QTRY_VERIFY(b.idle());
        const auto expected = expectedEdges(QStringLiteral("PARIS E"), 60);
        QCOMPARE(b.rec.keyEdges(), expected);
        // "PARIS " is 50 elements: the E after it keys at 3000 ms.
        const auto edges = b.rec.keyEdges();
        QVERIFY(edges.size() >= 2);
        QCOMPARE(edges[edges.size() - 2], (std::pair<qint64, bool>{3000, true}));
    }

    void speedMarkRaisesTheNextSegmentByFive()
    {
        Bench b;
        b.time.set(b.time.start);
        QVERIFY(b.sender->sendText(QStringLiteral("E>E<E")));
        QTRY_VERIFY(b.idle());
        using Edges = std::vector<std::pair<qint64, bool>>;
        // 20 WPM (60 ms), then 25 (48 ms), then 20 again.
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {60, false},
                                          {240, true}, {288, false},
                                          {432, true}, {492, false}}));
    }

    void prosignBetweenBarsSendsItsCharacter()
    {
        Bench b;
        b.time.set(b.time.start);
        QVERIFY(b.sender->sendText(QStringLiteral("|SK|")));
        QTRY_VERIFY(b.idle());
        // SK is ...-.- : three dots, a dash, a dot, a dash.
        using Edges = std::vector<std::pair<qint64, bool>>;
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {60, false}, {120, true}, {180, false},
                                          {240, true}, {300, false}, {360, true}, {540, false},
                                          {600, true}, {660, false}, {720, true}, {900, false}}));
    }

    // ── Repeat ─────────────────────────────────────────────────────────────
    void memoryEndingInQuoteRepeatsAfterTheDelay()
    {
        Bench b;
        b.time.set(b.time.start);
        b.sender->setRepeat(false, 1);
        b.sender->setMemories({QStringLiteral("E\"")});
        b.time.setHook([&b](SteadyTime until) {
            if (b.time.ms(until) > 3000) {
                b.sender->stop();
                return true;
            }
            return false;
        });
        QVERIFY(b.sender->sendMemory(1));
        QTRY_VERIFY(b.idle());
        // E (4 elements), the pause element, 1000 / 60 = 16 pause elements,
        // then the reload: 21 elements of 60 ms to the next E.
        using Edges = std::vector<std::pair<qint64, bool>>;
        const Edges edges = b.rec.keyEdges();
        QVERIFY(edges.size() >= 4);
        QCOMPARE(Edges(edges.begin(), edges.begin() + 4),
                 (Edges{{0, true}, {60, false}, {1260, true}, {1320, false}}));
        QVERIFY(!b.rec.snaps().back().cwx);
    }

    void rptSwitchRepeatsWithoutTheMarker()
    {
        Bench b;
        b.time.set(b.time.start);
        b.sender->setRepeat(true, 1);
        b.sender->setMemories({QStringLiteral("E")});
        b.time.setHook([&b](SteadyTime until) {
            if (b.time.ms(until) > 3000) {
                b.sender->stop();
                return true;
            }
            return false;
        });
        QVERIFY(b.sender->sendMemory(1));
        QTRY_VERIFY(b.idle());
        using Edges = std::vector<std::pair<qint64, bool>>;
        const Edges edges = b.rec.keyEdges();
        QVERIFY(edges.size() >= 6);
        QCOMPARE(Edges(edges.begin(), edges.begin() + 6),
                 (Edges{{0, true}, {60, false}, {1260, true}, {1320, false},
                        {2520, true}, {2580, false}}));
    }

    void rptOffMemoryWithoutTheMarkerSendsOnce()
    {
        Bench b;
        b.time.set(b.time.start);
        b.sender->setMemories({QStringLiteral("E")});
        QSignalSpy emptied(b.sender.get(), &CwxSender::emptied);
        QVERIFY(b.sender->sendMemory(1));
        QTRY_VERIFY(b.idle());
        using Edges = std::vector<std::pair<qint64, bool>>;
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {60, false}}));
        QTRY_COMPARE(emptied.count(), 1);
    }

    // ── Stop ───────────────────────────────────────────────────────────────
    void stopMidElementReleasesAtOnceAndClears()
    {
        Bench b;
        b.time.set(b.time.start);
        QSignalSpy emptied(b.sender.get(), &CwxSender::emptied);
        QSignalSpy sending(b.sender.get(), &CwxSender::sendingChanged);
        b.time.setHook([&b](SteadyTime until) {
            if (b.time.ms(until) == 60) {
                // Halfway through P's first dot.
                b.time.set(b.time.start + 30ms);
                b.sender->stop();
                return true;
            }
            return false;
        });
        QVERIFY(b.sender->sendText(QStringLiteral("PARIS")));
        QTRY_VERIFY(b.idle());
        using Edges = std::vector<std::pair<qint64, bool>>;
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {30, false}}));
        QCOMPARE(b.sender->queuedCharacters(), 0);
        QTRY_COMPARE(emptied.count(), 1);
        QTRY_VERIFY(!sending.isEmpty() && !sending.last().at(0).toBool());
        // Nothing more is keyed.
        pump();
        QCOMPARE(b.rec.keyEdges().size(), size_t(2));
    }

    void queuedCharactersCountsWhatHasNotStarted()
    {
        Bench b;
        b.time.set(b.time.start);
        QSignalSpy started(b.sender.get(), &CwxSender::characterStarted);
        b.time.freeze();
        QVERIFY(b.sender->sendText(QStringLiteral("ABC")));
        // The first element of A runs at once; B and C wait.
        QTRY_COMPARE(b.sender->queuedCharacters(), 2);
        b.time.thaw();
        QTRY_VERIFY(b.idle());
        QCOMPARE(b.sender->queuedCharacters(), 0);
        QTRY_COMPARE(started.count(), 3);
        QCOMPARE(started.at(0).at(0).toInt(), 0);
        QCOMPARE(started.at(1).at(0).toInt(), 1);
        QCOMPARE(started.at(2).at(0).toInt(), 2);
    }

    // ── The Key ────────────────────────────────────────────────────────────
    void keyHeldDropsThreeSecondsAfterTheLastPress()
    {
        Bench b;
        b.time.set(b.time.start);
        bool pressedAgain = false;
        b.time.setHook([&b, &pressedAgain](SteadyTime until) {
            if (!pressedAgain && b.time.ms(until) == 3000) {
                pressedAgain = true;
                b.time.set(b.time.start + 1000ms);
                b.sender->setKeyHeld(true);
                return true;
            }
            return false;
        });
        b.sender->setKeyHeld(true);
        QVERIFY(b.line->cwx());   // keyed at once
        QTRY_VERIFY(b.idle());
        using Edges = std::vector<std::pair<qint64, bool>>;
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {4000, false}}));
    }

    void keyReleaseDropsAtOnce()
    {
        Bench b;
        b.time.set(b.time.start);
        b.time.freeze();
        b.sender->setKeyHeld(true);
        QVERIFY(b.line->cwx());
        QVERIFY(b.sender->isSending());
        b.sender->setKeyHeld(false);
        QVERIFY(!b.line->cwx());
        QVERIFY(b.idle());
        using Edges = std::vector<std::pair<qint64, bool>>;
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {0, false}}));
    }

    // R-CW-35, the TCI keyer: the Key goes up `ms` after it went down.
    void keyReleaseAfterFallsThatLongAfterThePress()
    {
        Bench b;
        b.time.set(b.time.start);
        bool scheduled = false;
        b.time.setHook([&b, &scheduled](SteadyTime until) {
            if (!scheduled && b.time.ms(until) == 3000) {
                scheduled = true;
                b.sender->releaseKeyAfter(120);
                return true;
            }
            return false;
        });
        QSignalSpy held(b.sender.get(), &CwxSender::keyHeldChanged);
        b.sender->setKeyHeld(true);
        QVERIFY(b.sender->isKeyHeld());
        QTRY_VERIFY(b.idle());
        QVERIFY(!b.sender->isKeyHeld());
        using Edges = std::vector<std::pair<qint64, bool>>;
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {120, false}}));
        QTRY_COMPARE(held.count(), 2);
        QCOMPARE(held.at(0).at(0).toBool(), true);
        QCOMPARE(held.at(1).at(0).toBool(), false);
    }

    void keyReleaseAfterATimeAlreadyPassedReleasesAtOnce()
    {
        Bench b;
        b.time.set(b.time.start);
        b.time.freeze();
        b.sender->setKeyHeld(true);
        b.time.set(b.time.start + 200ms);
        b.sender->releaseKeyAfter(120);
        QVERIFY(!b.line->cwx());
        QVERIFY(!b.sender->isKeyHeld());
        using Edges = std::vector<std::pair<qint64, bool>>;
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {200, false}}));
        b.sender->releaseKeyAfter(120);   // not held: nothing
        QCOMPARE(b.rec.keyEdges().size(), std::size_t(2));
    }

    void keyReleaseAfterNeverOutlastsTheWatchdog()
    {
        Bench b;
        b.time.set(b.time.start);
        bool scheduled = false;
        b.time.setHook([&b, &scheduled](SteadyTime until) {
            if (!scheduled && b.time.ms(until) == 3000) {
                scheduled = true;
                b.sender->releaseKeyAfter(5000);
                return true;
            }
            return false;
        });
        b.sender->setKeyHeld(true);
        QTRY_VERIFY(b.idle());
        using Edges = std::vector<std::pair<qint64, bool>>;
        QCOMPARE(b.rec.keyEdges(), (Edges{{0, true}, {3000, false}}));
    }

    // ── HL2 CWX PTT (R-CW-15) ─────────────────────────────────────────────
    void hl2PttLeadsTheKeyAndHangsAfterTheLastElement()
    {
        Bench b;
        b.time.set(b.time.start);
        b.sender->setHl2(true);
        QVERIFY(b.sender->sendText(QStringLiteral("E")));
        QTRY_VERIFY(b.idle());
        // PTT up before the key, the key 60 ms, PTT down 300 ms after the
        // key went down (the hang, counted through the key-up elements).
        const std::vector<Snap> expected{{0, false, true}, {0, true, true}, {60, false, true},
                                         {300, false, false}};
        QCOMPARE(b.rec.snaps(), expected);
    }

    void hl2MemoryAt60LeadsPttByThePttDelay()
    {
        Bench b;
        b.time.set(b.time.start);
        b.sender->setHl2(true);
        b.sender->setSpeedWpm(60);
        b.sender->setMemories({QStringLiteral("E")});
        QVERIFY(b.sender->sendMemory(1));
        QTRY_VERIFY(b.idle());
        // 50 / 20 = 2 early PTT elements, then the key; PTT down 300 ms
        // after the key down.
        const std::vector<Snap> expected{{0, false, true}, {40, true, true}, {60, false, true},
                                         {340, false, false}};
        QCOMPARE(b.rec.snaps(), expected);
    }

    // F13: the sender's manual-Off rule also gates the HL2 PTT bit.
    // A full fake-clock message traverses its hang with no wire edges.
    void hl2InOffWithoutMox_messageAndHangStayOff_data()
    {
        QTest::addColumn<bool>("memory");
        QTest::newRow("text") << false;
        QTest::newRow("memory") << true;
    }

    void hl2InOffWithoutMox_messageAndHangStayOff()
    {
        QFETCH(bool, memory);
        Bench b;
        b.time.set(b.time.start);
        b.sender->setHl2(true);
        b.sender->setBreakInActive(false);
        b.sender->setMemories({QStringLiteral("E")});
        QVERIFY(memory ? b.sender->sendMemory(1) : b.sender->sendText(QStringLiteral("E")));
        QTRY_VERIFY(b.idle());
        QVERIFY(b.time.ms(b.time.now()) >= 300); // message plus the existing hang
        QVERIFY(b.rec.snaps().empty());
        QVERIFY(!b.line->cwx());
        QVERIFY(!b.line->cwxPtt());

        // MOX supplies the manual key in Off for either request path.
        b.sender->setMoxActive(true);
        QVERIFY(memory ? b.sender->sendMemory(1) : b.sender->sendText(QStringLiteral("E")));
        QTRY_VERIFY(b.idle());
        QVERIFY(b.rec.anyPtt());
        QCOMPARE(b.rec.keyEdges().size(), size_t(2));
        QVERIFY(!b.line->cwx());
        QVERIFY(!b.line->cwxPtt());
    }

    void hl2InOff_heldKeyNeedsMoxAndReleasesBothBitsWithIt()
    {
        Bench b;
        b.time.set(b.time.start);
        b.time.freeze();
        b.sender->setHl2(true);
        b.sender->setBreakInActive(false);
        b.sender->setKeyHeld(true);
        QVERIFY(b.sender->isKeyHeld());
        QVERIFY(b.rec.snaps().empty());
        QVERIFY(!b.line->cwx());
        QVERIFY(!b.line->cwxPtt());
        b.sender->setMoxActive(true);
        QVERIFY(b.line->cwx());
        QVERIFY(b.line->cwxPtt());
        b.sender->setMoxActive(false);
        QVERIFY(!b.line->cwx());
        QVERIFY(!b.line->cwxPtt());
        b.sender->setKeyHeld(false);
        QVERIFY(!b.sender->isKeyHeld());
        b.time.thaw();
    }

    void nonHl2NeverSetsThePtt()
    {
        Bench b;
        b.time.set(b.time.start);
        b.sender->setSpeedWpm(60);
        b.sender->setMemories({QStringLiteral("TEST")});
        QVERIFY(b.sender->sendMemory(1));
        QTRY_VERIFY(b.idle());
        QVERIFY(!b.rec.keyEdges().empty());
        QVERIFY(!b.rec.anyPtt());
    }

    // ── Break-in and MOX (R-CW-32) ────────────────────────────────────────
    void inOffCwxKeysOnlyWhileMoxIsHeld()
    {
        Bench b;
        b.time.set(b.time.start);
        b.sender->setBreakInActive(false);
        QVERIFY(b.sender->sendText(QStringLiteral("E")));
        QTRY_VERIFY(b.idle());
        QVERIFY(b.rec.snaps().empty());

        b.sender->setMoxActive(true);
        QVERIFY(b.sender->sendText(QStringLiteral("E")));
        QTRY_VERIFY(b.idle());
        QCOMPARE(b.rec.keyEdges().size(), size_t(2));

        // MOX released mid-element: the key comes up at once.
        b.time.freeze();
        QVERIFY(b.sender->sendText(QStringLiteral("T")));
        QTRY_VERIFY(b.line->cwx());
        b.sender->setMoxActive(false);
        QVERIFY(!b.line->cwx());
        b.time.thaw();
        QTRY_VERIFY(b.idle());
        QVERIFY(!b.line->cwx());
    }

    // ── Refusals ───────────────────────────────────────────────────────────
    void gateWordsRefuseEverySendAndTheKey()
    {
        Bench b;
        const QString words = QStringLiteral("Put the transmit slice in CW to use the radio's keyer.");
        b.sender->setRequestGate([words]() { return words; });
        b.sender->setMemories({QStringLiteral("CQ")});
        QSignalSpy refused(b.sender.get(), &CwxSender::refused);
        QVERIFY(!b.sender->sendText(QStringLiteral("CQ")));
        QVERIFY(!b.sender->sendMemory(1));
        b.sender->setKeyHeld(true);
        QCOMPARE(refused.count(), 3);
        for (const QList<QVariant>& r : refused) {
            QCOMPARE(r.at(0).toString(), words);
        }
        QVERIFY(b.idle());
        QVERIFY(b.rec.snaps().empty());
    }

    void nothingToSendIsRefusedWithWords()
    {
        Bench b;
        QSignalSpy refused(b.sender.get(), &CwxSender::refused);
        QVERIFY(!b.sender->sendText(QString()));
        QCOMPARE(refused.last().at(0).toString(), QString::fromLatin1(kCwxNothingToSendText));
        QVERIFY(!b.sender->sendMemory(1));   // memories are empty by default
        QCOMPARE(refused.last().at(0).toString(), QStringLiteral("Memory 1 is empty."));
        QVERIFY(!b.sender->sendMemory(9));
        QCOMPARE(refused.last().at(0).toString(), QStringLiteral("There is no memory 9."));
        QVERIFY(b.rec.snaps().empty());
        for (const char* text : {kCwxNotCwText, kCwxNothingToSendText, kCwxMemoryEmptyText,
                                 kCwxNoSuchMemoryText}) {
            QVERIFY2(OperatorWording::isPlain(QString::fromLatin1(text)), text);
        }
    }

    // ── The key line ───────────────────────────────────────────────────────
    void swappingTheLineReleasesTheOldOneAndAborts()
    {
        Bench b;
        b.time.set(b.time.start);
        b.time.freeze();
        QVERIFY(b.sender->sendText(QStringLiteral("TTTT")));
        QTRY_VERIFY(b.line->cwx());
        auto other = std::make_shared<CwKeyLine>();
        b.sender->setKeyLine(other);
        QVERIFY(!b.line->cwx());
        QVERIFY(!b.line->cwxPtt());
        QVERIFY(b.idle());
        const size_t snaps = b.rec.snaps().size();
        b.time.thaw();
        pump();
        QCOMPARE(b.rec.snaps().size(), snaps);
        QVERIFY(!other->cwx());
        b.sender->setKeyLine(nullptr);
    }

    // ── Wall clock, on the sender's own thread ─────────────────────────────
    void testAt30KeysEachEdgeWithinTwoMs()
    {
        const double loadBefore = loadAverage1();
        const auto expected = expectedEdges(QStringLiteral("TEST"), 40);
        auto line = std::make_shared<CwKeyLine>();
        std::mutex m;
        std::vector<std::pair<SteadyTime, bool>> edges;
        edges.reserve(expected.size());
#if defined(Q_OS_LINUX)
        int senderPolicy = -1;
        sched_param senderParameters{};
        int senderPolicyResult = -1;
#endif
        line->setOnChange([&]() {
            const SteadyTime t = std::chrono::steady_clock::now();
            const std::lock_guard<std::mutex> lock(m);
            edges.emplace_back(t, line->cwx());
#if defined(Q_OS_LINUX)
            // Timestamp first; query once on the actual sender thread and
            // print only after the timed run, including on a failed edge.
            if (edges.size() == 1) {
                senderPolicyResult = pthread_getschedparam(pthread_self(), &senderPolicy,
                                                           &senderParameters);
            }
#endif
        });
        CwxSender sender;
        sender.setKeyLine(line);
        sender.setBreakInActive(true);
        sender.setSpeedWpm(30);
        // The schedule starts when the text is sent, so a late first edge
        // counts against the sender too.
        const SteadyTime requested = std::chrono::steady_clock::now();
        QVERIFY(sender.sendText(QStringLiteral("TEST")));
        QTRY_VERIFY_WITH_TIMEOUT(!sender.isSending(), 5000);
        sender.setKeyLine(nullptr);

#if defined(Q_OS_LINUX)
        qInfo("CWX real-time: sender pthread_getschedparam=%d policy=%d priority=%d",
              senderPolicyResult, senderPolicy, senderParameters.sched_priority);
#endif
        // The PTT delay leads the first key by whole elements, as Thetis:
        // 50 / 40 is one element at 30 WPM.
        constexpr qint64 kLeadMs = (CwxSender::kPttDelayMs / 40) * 40;
        std::vector<std::pair<SteadyTime, bool>> got;
        {
            const std::lock_guard<std::mutex> lock(m);
            got = edges;
        }
        QCOMPARE(got.size(), expected.size());
        qint64 worstUs = 0;
        QStringList offsets;
        for (size_t i = 0; i < got.size(); ++i) {
            QCOMPARE(got[i].second, expected[i].second);
            const qint64 atUs =
                std::chrono::duration_cast<std::chrono::microseconds>(got[i].first - requested).count();
            const qint64 offUs = atUs - (kLeadMs + expected[i].first) * 1000;
            worstUs = std::max(worstUs, offUs < 0 ? -offUs : offUs);
            offsets << QString::number(offUs / 1000.0, 'f', 3);
        }
        qInfo("CWX real-time: edge offsets (ms) %s", qPrintable(offsets.join(QLatin1Char(' '))));
        qInfo("CWX real-time: %d edges, worst %.3f ms off schedule, load average %.2f before, %.2f after",
              static_cast<int>(got.size()), worstUs / 1000.0, loadBefore, loadAverage1());
        QVERIFY2(worstUs <= 2000, qPrintable(QStringLiteral("worst edge %1 us off").arg(worstUs)));
    }

    // ── RadioModel: the request gate and the aborts (R-CW-32, R-CW-33) ────
    void modelRefusesOutsideCw()
    {
        Rig rig;
        rig.slice->setDspMode(DSPMode::USB);
        rig.slice->setFrequency(14'200'000.0);
        pump();
        QSignalSpy refused(rig.cwx(), &CwxSender::refused);
        QVERIFY(!rig.cwx()->sendText(QStringLiteral("CQ")));
        QCOMPARE(refused.last().at(0).toString(), QString::fromLatin1(kCwxNotCwText));
    }

    void modelRefusesWithTheKeyerGatesWords()
    {
        Rig rig;
        rig.model.setStationRadioChangeUnderway(true);
        pump();
        QVERIFY(!rig.model.cwKeyerController()->keyerAllowed());
        QSignalSpy refused(rig.cwx(), &CwxSender::refused);
        QVERIFY(!rig.cwx()->sendText(QStringLiteral("CQ")));
        QCOMPARE(refused.last().at(0).toString(), rig.model.cwKeyerController()->refusal().text);
        QVERIFY(!refused.last().at(0).toString().isEmpty());
    }

    void modelGateTurningFalseMidMessageAborts()
    {
        Rig rig;
        LineWatch watch(rig.conn.cwKeyLine());
        QVERIFY(rig.cwx()->sendText(QStringLiteral("TTTTTTTTTTTTTTTTTTTT")));
        QTRY_VERIFY(watch.keyed.load());
        rig.model.setStationRadioChangeUnderway(true);
        pump();
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
    }

    void modelLeavingCwAborts()
    {
        Rig rig;
        LineWatch watch(rig.conn.cwKeyLine());
        QVERIFY(rig.cwx()->sendText(QStringLiteral("TTTTTTTTTTTTTTTTTTTT")));
        QTRY_VERIFY(watch.keyed.load());
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
    }

    // ── RadioModel: key-line writers let go before the connection goes ────
    void modelTeardownReleasesTheLineAndLetsGo()
    {
        Rig rig;
        LineWatch watch(rig.conn.cwKeyLine());
        QVERIFY(rig.cwx()->sendText(QStringLiteral("TTTTTTTTTTTTTTTTTTTT")));
        QTRY_VERIFY(watch.keyed.load());
        rig.model.disconnectFromRadio();
        pump();
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        const int changes = watch.changes.load();
        // Nothing writes the old line again, even when asked to send.
        rig.cwx()->sendText(QStringLiteral("TTTT"));
        QTest::qWait(300);
        QCOMPARE(watch.changes.load(), changes);
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
    }

    void modelConnectionSwapReleasesTheOldLine()
    {
        CwxMockConnection next;   // outlives the rig, which holds it last
        Rig rig;
        LineWatch oldWatch(rig.conn.cwKeyLine());
        LineWatch newWatch(next.cwKeyLine());
        QVERIFY(rig.cwx()->sendText(QStringLiteral("TTTTTTTTTTTTTTTTTTTT")));
        QTRY_VERIFY(oldWatch.keyed.load());
        rig.model.injectConnectionForTest(&next);
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.cwx()->isSending());
        const int oldChanges = oldWatch.changes.load();
        rig.model.wireCwKeyerConnectionForTest();
        pump();
        QVERIFY(rig.cwx()->sendText(QStringLiteral("TTTT")));
        QTRY_VERIFY(newWatch.keyed.load());
        QCOMPARE(oldWatch.changes.load(), oldChanges);
        rig.cwx()->stop();
    }

    // Early re-review: a cut mid-message and its catch-up release the key
    // line, so clearing the cut (which resends the stored key bits) keys
    // nothing.
    void modelCutMidMessageThenClearKeysNothing()
    {
        Rig rig;
        LineWatch watch(rig.conn.cwKeyLine());
        rig.model.transmitModel().setCwxRepeat(true);
        rig.model.transmitModel().setCwxMemory1(QStringLiteral("TTTTTTTTTT"));
        pump();
        QVERIFY(rig.cwx()->sendMemory(1));
        QTRY_VERIFY(watch.keyed.load());
        QTest::ignoreMessage(QtWarningMsg, "Transmit cut: MOX and the T/R relay off");
        rig.conn.cutTransmit();
        pump();
        QVERIFY(rig.conn.transmitCut());
        rig.model.catchUpTransmitCut(QStringLiteral("The link went quiet."));
        pump();
        QTRY_VERIFY(!rig.conn.transmitCut());
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        watch.keyed.store(false);
        QTest::qWait(300);
        QVERIFY(!watch.keyed.load());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
    }

    void modelStopAllTxInOffWithoutMox_clearsLogicalSendQuietly()
    {
        Rig rig;
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        pump();
        QVERIFY(!rig.model.mox());
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        QVERIFY(rig.cwx()->sendText(QString(1000, QLatin1Char('T'))));
        QVERIFY(rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        rig.model.stopAllTx(QStringLiteral("Stopped."));
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        QCOMPARE(stopped.count(), 0);
        rig.cwx()->setKeyHeld(true);
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.model.stopAllTx(QStringLiteral("Stopped."));
        QVERIFY(!rig.cwx()->isKeyHeld());
        QCOMPARE(stopped.count(), 0);
    }

    // F7: finish the entire character, then stop inside the existing hang.
    // No repeat, current mark, queued text or held Key can explain sending.
    void modelStopAllTxInOffWithoutMox_clearsPostMessageHangQuietly()
    {
        FakeTime time; // Outlives Rig's injected sender and its join.
        std::atomic<bool> lastLogicalKey{false};
        std::atomic<int> logicalDowns{0};
        std::atomic<int> logicalUps{0};
        std::atomic<qint64> downAt{-1};
        std::atomic<qint64> upAt{-1};
        Rig rig(&time);
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        rig.model.transmitModel().setCwxRepeat(false);
        pump();
        QCOMPARE(rig.model.cwKeyerController()->effectiveBreakInMode(), CwBreakInMode::Off);
        QVERIFY(!rig.model.mox());
        QVERIFY(!rig.model.transmitModel().cwxRepeat());
        QVERIFY(rig.model.audioEngine()->cwSidetoneRun());
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        QSignalSpy started(rig.cwx(), &CwxSender::characterStarted);

        // The existing E schedule is one dot plus three up elements. At
        // 60 WPM the PTT lead is 40 ms: down40, up60, trailing ups80/100,
        // then the empty-queue hang tick120. Park before its next tick140,
        // well before the existing 300 ms hang from down40 ends at340.
        const int elementMs = CwxSender::elementMs(60);
        const qint64 leadMs = (CwxSender::kPttDelayMs / elementMs) * elementMs;
        const qint64 hangOnlyAtMs = leadMs + 4 * elementMs;
        const SteadyTime parkedDeadline = time.start
            + std::chrono::milliseconds(hangOnlyAtMs + elementMs);
        time.freezeBefore(parkedDeadline);
        AudioEngine* const engine = rig.model.audioEngine();
        // Observe the model's actual sidetone feed without replacing its
        // key listener. This hook runs after each completed sender tick.
        time.setHook([&time, &lastLogicalKey, &logicalDowns, &logicalUps,
                      &downAt, &upAt, engine](SteadyTime) {
            const bool key = engine->cwSidetoneKeyed();
            if (lastLogicalKey.exchange(key) != key) {
                if (key) {
                    downAt.store(time.ms(time.now()));
                    logicalDowns.fetch_add(1);
                } else {
                    upAt.store(time.ms(time.now()));
                    logicalUps.fetch_add(1);
                }
            }
            return false;
        });
        QVERIFY(rig.cwx()->sendText(QStringLiteral("E")));
        QTRY_VERIFY_WITH_TIMEOUT(time.frozenBefore(parkedDeadline), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 2000);
        QCOMPARE(started.first().first().toInt(), 0);
        QCOMPARE(time.ms(time.now()), hangOnlyAtMs);
        QCOMPARE(logicalDowns.load(), 1);
        QCOMPARE(logicalUps.load(), 1);
        QCOMPARE(downAt.load(), leadMs);
        QCOMPARE(upAt.load(), leadMs + elementMs);
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(!engine->cwSidetoneKeyed());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        QVERIFY(!rig.model.mox());
        QVERIFY(rig.cwx()->isSending()); // Only the existing hang remains.

        rig.model.stopAllTx(QStringLiteral("Stopped."));
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.cwx()->isKeyHeld());
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QVERIFY(!rig.conn.cwKeyLine()->cwxPtt());
        QVERIFY(!engine->cwSidetoneKeyed());
        QVERIFY(!rig.model.mox());
        pump();
        QCOMPARE(stopped.count(), 0);
        QCOMPARE(logicalDowns.load(), 1);
        QCOMPARE(logicalUps.load(), 1);
        QCOMPARE(time.ms(time.now()), hangOnlyAtMs); // Never traversed hang.
    }

    void modelStopAllTxInOffWithMox_announcesAndClearsSender()
    {
        Rig rig;
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        pump();
        rig.model.moxController()->setMox(true);
        QVERIFY(rig.model.mox());
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.cwx()->setKeyHeld(true);
        QVERIFY(rig.conn.cwKeyLine()->cwx());
        rig.model.stopAllTx(QStringLiteral("Stopped."));
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QCOMPARE(stopped.count(), 1);
    }

    void modelStopAllTxInQsk_announcesAndClearsSender()
    {
        Rig rig;
        rig.model.setCapsForTest(/*hasAlex=*/false);
        rig.model.cwKeyerController()->reevaluate();
        QVERIFY2(rig.model.cwKeyerController()->qskAvailable(),
                 qPrintable(rig.model.cwKeyerController()->qskUnavailableReason()));
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Qsk));
        pump();
        QCOMPARE(rig.model.cwKeyerController()->effectiveBreakInMode(), CwBreakInMode::Qsk);
        QVERIFY(!rig.model.mox());
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.cwx()->setKeyHeld(true);
        QVERIFY(rig.conn.cwKeyLine()->cwx());
        rig.model.stopAllTx(QStringLiteral("Stopped."));
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QCOMPARE(stopped.count(), 1);
    }

    void modelStopAllTxStopsCwx()
    {
        Rig rig;
        LineWatch watch(rig.conn.cwKeyLine());
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        QVERIFY(rig.cwx()->sendText(QStringLiteral("TTTTTTTTTTTTTTTTTTTT")));
        QTRY_VERIFY(watch.keyed.load());
        rig.model.stopAllTx(QStringLiteral("Stopped."));
        pump();
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
        QCOMPARE(stopped.count(), 1);
    }
};

QTEST_MAIN(TestCwxSender)
#include "tst_cwx_sender.moc"
