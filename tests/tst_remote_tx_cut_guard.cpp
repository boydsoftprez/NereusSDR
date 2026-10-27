// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_remote_tx_cut_guard.cpp  (NereusSDR)
// =================================================================
//
// RemoteTxCutGuard: a keyed remote device's RF is held silent on the
// guard's own thread when keepalives heard off the main thread stop for
// more than kHoldMs (RemoteTxWatchdog::kLinkHoldMs), released when one is
// heard again, and the transmit is cut once the silence passes kCutMs
// (RemoteTxWatchdog::kLinkCutMs), while the main thread is held. "Main held" here means the test's main thread sleeps with
// std::this_thread::sleep_for and runs no event loop (the guard is not a
// QObject and needs none).
//
// Real time throughout: labelled realtime. A hold comes more than kHoldMs
// and at most kHoldMs + 100 ms after the last stamp, and a cut more than
// kCutMs and at most kCutMs + 100 ms after it, the bounds
// tst_tx_link_loss_each_path uses.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-15: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), remote transmit cut off the main thread, with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-16: a cut comes after more than kDeadlineMs of silence, never
//               at it (the watchdog's rule): every bound is now strict.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: the stop bound is named
//               RemoteTxWatchdog::kLinkCutMs (renamed from
//               kLinkLossDeadlineMs). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: the guard holds RF at
//               kHoldMs and cuts at kCutMs; a receipt during the hold
//               releases it, and disarm() or a new arm() releases it
//               before returning; the main thread is held 3500 ms.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut, fix wave A: a guard
//               destroyed while holding calls hold(false) exactly once,
//               on the destroying thread, before its destructor returns,
//               and never cuts. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================


#include "core/safety/RemoteTxCutGuard.h"

#include "RealtimeTestLoad.h"

#include <QtTest>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace NereusSDR;
using namespace std::chrono_literals;

namespace {

using Clock = RemoteTxCutGuard::Clock;

constexpr qint64 kHoldMs = RemoteTxCutGuard::kHoldMs;
constexpr qint64 kCutMs = RemoteTxCutGuard::kCutMs;
constexpr qint64 kHoldBoundMs = RemoteTxWatchdog::kLinkHoldMs + 100;
constexpr qint64 kBoundMs = RemoteTxWatchdog::kLinkCutMs + 100;
constexpr auto kTick = 100ms;

// What the guard called, in order: a hold (on or off) or a cut.
struct Event {
    enum class Kind { HoldOn, HoldOff, Cut };
    Kind kind{Kind::Cut};
    quint64 epoch{0};
    quint64 serial{0};
    qint64 silentMs{0};
    std::thread::id thread;
    Clock::time_point at;
};

// Shared with the guard's CutFn and HoldFn; outlives the guard.
struct EventLog {
    std::mutex mutex;
    std::vector<Event> events;

    RemoteTxCutGuard::CutFn cutFn()
    {
        return [this](quint64 epoch, quint64 serial, qint64 silentMs) {
            add(Event{Event::Kind::Cut, epoch, serial, silentMs, std::this_thread::get_id(),
                      Clock::now()});
        };
    }

    RemoteTxCutGuard::HoldFn holdFn()
    {
        return [this](bool held) {
            add(Event{held ? Event::Kind::HoldOn : Event::Kind::HoldOff, 0, 0, 0,
                      std::this_thread::get_id(), Clock::now()});
        };
    }

    std::vector<Event> snapshot()
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return events;
    }

    std::vector<Event> cuts()
    {
        std::vector<Event> out;
        for (const Event& e : snapshot()) {
            if (e.kind == Event::Kind::Cut) {
                out.push_back(e);
            }
        }
        return out;
    }

private:
    void add(const Event& e)
    {
        const std::lock_guard<std::mutex> lock(mutex);
        events.push_back(e);
    }
};

// Calls heardOffMain(epoch, now) every 100 ms for `forMs`, keeping the last
// stamp it handed over.
class Feeder {
public:
    Feeder(RemoteTxCutGuard& guard, quint64 epoch, std::chrono::milliseconds forMs)
    {
        m_thread = std::thread([this, &guard, epoch, forMs] {
            const Clock::time_point start = Clock::now();
            Clock::time_point next = start;
            while (Clock::now() - start < forMs) {
                const Clock::time_point stamp = Clock::now();
                guard.heardOffMain(epoch, stamp);
                m_lastStampNs.store(stamp.time_since_epoch().count());
                next += kTick;
                std::this_thread::sleep_until(next);
            }
        });
        m_threadId = m_thread.get_id();
    }

    ~Feeder() { join(); }
    Feeder(const Feeder&) = delete;
    Feeder& operator=(const Feeder&) = delete;

    void join()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    std::thread::id id() const { return m_threadId; }

    Clock::time_point lastStamp() const
    {
        return Clock::time_point(Clock::duration(m_lastStampNs.load()));
    }

private:
    std::atomic<Clock::rep> m_lastStampNs{0};
    std::thread::id m_threadId;
    std::thread m_thread;
};

qint64 msBetween(Clock::time_point from, Clock::time_point to)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count();
}

void holdMain(std::chrono::milliseconds forMs)
{
    std::this_thread::sleep_for(forMs);
}

// Sleeps the main thread (no event loop) until `done` or `forMs` passes.
template <typename Done>
bool holdMainUntil(Done done, std::chrono::milliseconds forMs)
{
    const Clock::time_point until = Clock::now() + forMs;
    while (!done()) {
        if (Clock::now() >= until) {
            return false;
        }
        std::this_thread::sleep_for(2ms);
    }
    return true;
}

// One arm's whole silence, after `lastHeard`: a hold on, the cut, a hold
// off, in that order, all on one thread that is neither main nor `other`.
// The hold comes past kHoldMs and the cut past kCutMs, each within 100 ms.
void verifyHoldThenCut(const std::vector<Event>& events, Clock::time_point lastHeard,
                       quint64 epoch, quint64 serial, std::thread::id other)
{
    QCOMPARE(int(events.size()), 3);
    const Event& on = events.at(0);
    const Event& cut = events.at(1);
    const Event& off = events.at(2);
    QVERIFY(on.kind == Event::Kind::HoldOn);
    QVERIFY(cut.kind == Event::Kind::Cut);
    QVERIFY(off.kind == Event::Kind::HoldOff);
    QVERIFY(on.thread != std::this_thread::get_id());
    QVERIFY(on.thread != other);
    QVERIFY(cut.thread == on.thread);
    QVERIFY(off.thread == on.thread);
    QCOMPARE(cut.epoch, epoch);
    QCOMPARE(cut.serial, serial);
    const qint64 holdAfter = msBetween(lastHeard, on.at);
    QVERIFY2(holdAfter > kHoldMs && holdAfter <= kHoldBoundMs,
             qPrintable(QStringLiteral("hold %1 ms after the last stamp").arg(holdAfter)));
    const qint64 cutAfter = msBetween(lastHeard, cut.at);
    QVERIFY2(cutAfter > kCutMs && cutAfter <= kBoundMs,
             qPrintable(QStringLiteral("cut %1 ms after the last stamp").arg(cutAfter)));
    QVERIFY2(cut.silentMs > kCutMs && cut.silentMs <= kBoundMs,
             qPrintable(QStringLiteral("silentMs %1").arg(cut.silentMs)));
}

} // namespace

class TestRemoteTxCutGuard : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { RealtimeTestLoad::printLoadAverageIfFailed(); }

    void cutsOnTimeWithMainHeld();
    void noFalseCutWithStreamAliveWhileMainHeld();
    void notJudgingBeforeOffMainReceipt();
    void heardOnMainHoldsJudgingGuard();
    void disarmAndRearm();
    void destroyingJudgingGuardIsPromptAndSilent();
    void receiptDuringHoldReleasesItOnTheGuardThread();
    void disarmDuringHoldReleasesBeforeReturning();
    void rearmDuringHoldReleasesTheOldHold();
    void destroyingAHoldingGuardReleasesOnceAndNeverCuts();
};

// Acceptance 1: main held 3500 ms; RF held from about 400 ms and cut at
// about 3000 ms after the last stamp, off the main thread.
void TestRemoteTxCutGuard::cutsOnTimeWithMainHeld()
{
    EventLog log;
    RemoteTxCutGuard guard(log.cutFn(), log.holdFn());
    const quint64 serial = guard.arm(7);
    QCOMPARE(serial, quint64(1));

    Feeder feeder(guard, 7, 500ms);
    holdMain(3500ms);
    feeder.join();
    // Still held: the cut and its release, if they have not run yet.
    std::this_thread::sleep_until(feeder.lastStamp() + std::chrono::milliseconds(kBoundMs + 50));

    verifyHoldThenCut(log.snapshot(), feeder.lastStamp(), 7, serial, feeder.id());
    QVERIFY(!guard.isJudging());
    QVERIFY(!guard.isHolding());

    // Nothing more for that arm.
    holdMain(std::chrono::milliseconds(kHoldBoundMs + 100));
    QCOMPARE(int(log.snapshot().size()), 3);
}

// Acceptance 2.
void TestRemoteTxCutGuard::noFalseCutWithStreamAliveWhileMainHeld()
{
    EventLog log;
    RemoteTxCutGuard guard(log.cutFn(), log.holdFn());
    const quint64 serial = guard.arm(7);

    Feeder feeder(guard, 7, 1500ms);
    holdMain(1000ms);
    QVERIFY(log.snapshot().empty());   // nothing held, nothing cut
    QVERIFY(guard.isJudging());
    QVERIFY(!guard.isHolding());

    feeder.join();
    holdMain(std::chrono::milliseconds(kBoundMs + 200));

    verifyHoldThenCut(log.snapshot(), feeder.lastStamp(), 7, serial, feeder.id());
}

// Acceptance 3.
void TestRemoteTxCutGuard::notJudgingBeforeOffMainReceipt()
{
    EventLog log;
    RemoteTxCutGuard guard(log.cutFn(), log.holdFn());
    guard.arm(7);

    const Clock::time_point start = Clock::now();
    Clock::time_point next = start;
    while (Clock::now() - start < 1000ms) {
        guard.heardOnMain(Clock::now());
        next += kTick;
        std::this_thread::sleep_until(next);
    }
    holdMain(1000ms);
    QVERIFY(log.snapshot().empty());
    QVERIFY(!guard.isJudging());

    // Another media session's epoch does not start judging.
    guard.heardOffMain(8, Clock::now());
    QVERIFY(!guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QVERIFY(log.snapshot().empty());

    // Epoch 0 (no media session) never starts judging, even when armed so.
    guard.arm(0);
    guard.heardOffMain(0, Clock::now());
    QVERIFY(!guard.isJudging());

    // Nor does a heard while disarmed.
    guard.arm(7);
    guard.disarm();
    guard.heardOffMain(7, Clock::now());
    QVERIFY(!guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QVERIFY(log.snapshot().empty());
}

// Acceptance 4.
void TestRemoteTxCutGuard::heardOnMainHoldsJudgingGuard()
{
    EventLog log;
    RemoteTxCutGuard guard(log.cutFn(), log.holdFn());
    const quint64 serial = guard.arm(7);
    guard.heardOffMain(7, Clock::now());
    QVERIFY(guard.isJudging());

    const Clock::time_point start = Clock::now();
    Clock::time_point next = start;
    Clock::time_point lastStamp = start;
    while (Clock::now() - start < 1000ms) {
        lastStamp = Clock::now();
        guard.heardOnMain(lastStamp);
        next += kTick;
        std::this_thread::sleep_until(next);
    }
    QVERIFY(log.snapshot().empty());

    holdMain(std::chrono::milliseconds(kBoundMs + 200));
    verifyHoldThenCut(log.snapshot(), lastStamp, 7, serial, std::thread::id());
}

// Acceptance 5.
void TestRemoteTxCutGuard::disarmAndRearm()
{
    EventLog log;
    RemoteTxCutGuard guard(log.cutFn(), log.holdFn());

    // disarm() before the hold: nothing held, no cut.
    const quint64 first = guard.arm(7);
    guard.heardOffMain(7, Clock::now());
    QVERIFY(guard.isJudging());
    holdMain(200ms);
    guard.disarm();
    QVERIFY(!guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QVERIFY(log.snapshot().empty());

    // A fresh arm holds, then cuts, on silence.
    const quint64 second = guard.arm(7);
    QVERIFY(second > first);
    const Clock::time_point heard2 = Clock::now();
    guard.heardOffMain(7, heard2);
    holdMain(std::chrono::milliseconds(kBoundMs + 200));
    verifyHoldThenCut(log.snapshot(), heard2, 7, second, std::thread::id());

    // A heard after the cut does not undo it, hold or start judging again.
    guard.heardOffMain(7, Clock::now());
    QVERIFY(!guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QCOMPARE(int(log.snapshot().size()), 3);

    // arm(7) again after a cut: new serial, not judging until heardOffMain.
    const quint64 third = guard.arm(7);
    QVERIFY(third > second);
    QCOMPARE(guard.armSerial(), third);
    QVERIFY(!guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QCOMPARE(int(log.snapshot().size()), 3);

    const Clock::time_point heard = Clock::now();
    guard.heardOffMain(7, heard);
    QVERIFY(guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 200));
    const std::vector<Event> all = log.snapshot();
    QCOMPARE(int(all.size()), 6);
    verifyHoldThenCut(std::vector<Event>(all.begin() + 3, all.end()), heard, 7, third,
                      std::thread::id());
}

// Acceptance 6.
void TestRemoteTxCutGuard::destroyingJudgingGuardIsPromptAndSilent()
{
    EventLog log;
    auto guard = std::make_unique<RemoteTxCutGuard>(log.cutFn(), log.holdFn());
    guard->arm(7);
    guard->heardOffMain(7, Clock::now());
    QVERIFY(guard->isJudging());
    holdMain(100ms);

    const Clock::time_point before = Clock::now();
    guard.reset();
    const qint64 tookMs = msBetween(before, Clock::now());
    QVERIFY2(tookMs < 100, qPrintable(QStringLiteral("destructor took %1 ms").arg(tookMs)));

    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QVERIFY(log.snapshot().empty());
}

// A receipt during the hold, off main or on main: hold(false) on the
// guard's thread, judging goes on, a later silence holds again, no cut.
void TestRemoteTxCutGuard::receiptDuringHoldReleasesItOnTheGuardThread()
{
    EventLog log;
    RemoteTxCutGuard guard(log.cutFn(), log.holdFn());
    guard.arm(7);
    const Clock::time_point heard = Clock::now();
    guard.heardOffMain(7, heard);

    QVERIFY(holdMainUntil([&guard] { return guard.isHolding(); },
                          std::chrono::milliseconds(kHoldBoundMs + 100)));
    std::vector<Event> events = log.snapshot();
    QCOMPARE(int(events.size()), 1);
    QVERIFY(events.at(0).kind == Event::Kind::HoldOn);
    const std::thread::id guardThread = events.at(0).thread;
    QVERIFY(guardThread != std::this_thread::get_id());
    const qint64 holdAfter = msBetween(heard, events.at(0).at);
    QVERIFY2(holdAfter > kHoldMs && holdAfter <= kHoldBoundMs,
             qPrintable(QStringLiteral("hold %1 ms after the heard").arg(holdAfter)));

    // An off-main receipt releases it, on the guard's thread.
    const Clock::time_point heardAgain = Clock::now();
    guard.heardOffMain(7, heardAgain);
    QVERIFY(holdMainUntil([&guard] { return !guard.isHolding(); }, 100ms));
    QVERIFY(holdMainUntil([&log] { return log.snapshot().size() >= 2; }, 100ms));
    events = log.snapshot();
    QCOMPARE(int(events.size()), 2);
    QVERIFY(events.at(1).kind == Event::Kind::HoldOff);
    QVERIFY(events.at(1).thread == guardThread);
    QVERIFY(guard.isJudging());

    // A later silence holds again, from the new receipt.
    QVERIFY(holdMainUntil([&guard] { return guard.isHolding(); },
                          std::chrono::milliseconds(kHoldBoundMs + 100)));
    events = log.snapshot();
    QCOMPARE(int(events.size()), 3);
    QVERIFY(events.at(2).kind == Event::Kind::HoldOn);
    const qint64 againAfter = msBetween(heardAgain, events.at(2).at);
    QVERIFY2(againAfter > kHoldMs && againAfter <= kHoldBoundMs,
             qPrintable(QStringLiteral("second hold %1 ms after the heard").arg(againAfter)));

    // An on-main receipt releases it the same way.
    guard.heardOnMain(Clock::now());
    QVERIFY(holdMainUntil([&guard] { return !guard.isHolding(); }, 100ms));
    QVERIFY(holdMainUntil([&log] { return log.snapshot().size() >= 4; }, 100ms));
    events = log.snapshot();
    QCOMPARE(int(events.size()), 4);
    QVERIFY(events.at(3).kind == Event::Kind::HoldOff);
    QVERIFY(events.at(3).thread == guardThread);
    QVERIFY(guard.isJudging());

    guard.disarm();
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QVERIFY(log.cuts().empty());
    QCOMPARE(int(log.snapshot().size()), 4);
}

// disarm() during a hold: hold(false) before disarm() returns; no cut.
void TestRemoteTxCutGuard::disarmDuringHoldReleasesBeforeReturning()
{
    EventLog log;
    RemoteTxCutGuard guard(log.cutFn(), log.holdFn());
    guard.arm(7);
    guard.heardOffMain(7, Clock::now());
    QVERIFY(holdMainUntil([&guard] { return guard.isHolding(); },
                          std::chrono::milliseconds(kHoldBoundMs + 100)));

    guard.disarm();
    std::vector<Event> events = log.snapshot();
    QCOMPARE(int(events.size()), 2);
    QVERIFY(events.at(0).kind == Event::Kind::HoldOn);
    QVERIFY(events.at(1).kind == Event::Kind::HoldOff);
    QVERIFY(!guard.isHolding());
    QVERIFY(!guard.isJudging());

    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QCOMPARE(int(log.snapshot().size()), 2);
}

// A new arm() during the previous arm's hold releases that hold first.
void TestRemoteTxCutGuard::rearmDuringHoldReleasesTheOldHold()
{
    EventLog log;
    RemoteTxCutGuard guard(log.cutFn(), log.holdFn());
    const quint64 first = guard.arm(7);
    guard.heardOffMain(7, Clock::now());
    QVERIFY(holdMainUntil([&guard] { return guard.isHolding(); },
                          std::chrono::milliseconds(kHoldBoundMs + 100)));

    const quint64 second = guard.arm(7);
    QVERIFY(second > first);
    std::vector<Event> events = log.snapshot();
    QCOMPARE(int(events.size()), 2);
    QVERIFY(events.at(1).kind == Event::Kind::HoldOff);
    QVERIFY(!guard.isHolding());
    QVERIFY(!guard.isJudging());

    // The old arm is never cut; the new one is not judging yet.
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QCOMPARE(int(log.snapshot().size()), 2);
}

// Fix wave A: destroyed while holding, the guard calls hold(false) once,
// on the destroying thread, before its destructor returns; no cut and
// nothing else runs after it.
void TestRemoteTxCutGuard::destroyingAHoldingGuardReleasesOnceAndNeverCuts()
{
    EventLog log;
    auto guard = std::make_unique<RemoteTxCutGuard>(log.cutFn(), log.holdFn());
    guard->arm(7);
    guard->heardOffMain(7, Clock::now());
    QVERIFY(guard->isJudging());
    QVERIFY(holdMainUntil([&guard] { return guard->isHolding(); },
                          std::chrono::milliseconds(kHoldBoundMs + 100)));

    guard.reset();
    const std::vector<Event> events = log.snapshot();
    QCOMPARE(int(events.size()), 2);
    QVERIFY(events.at(0).kind == Event::Kind::HoldOn);
    QVERIFY(events.at(1).kind == Event::Kind::HoldOff);
    QVERIFY(events.at(1).thread == std::this_thread::get_id());

    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QCOMPARE(int(log.snapshot().size()), 2);
    QVERIFY(log.cuts().empty());
}

QTEST_GUILESS_MAIN(TestRemoteTxCutGuard)
#include "tst_remote_tx_cut_guard.moc"
