// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_remote_tx_cut_guard.cpp  (NereusSDR)
// =================================================================
//
// RemoteTxCutGuard: a keyed remote device's transmit is cut on the guard's
// own thread when keepalives heard off the main thread stop for
// RemoteTxWatchdog::kLinkCutMs, while the main thread is
// held. "Main held" here means the test's main thread sleeps with
// std::this_thread::sleep_for and runs no event loop (the guard is not a
// QObject and needs none).
//
// Real time throughout: labelled realtime. A cut comes more than
// kLinkCutMs and at most kLinkCutMs + 100 ms
// after the last stamp, the bounds tst_tx_link_loss_each_path uses.
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

constexpr qint64 kDeadlineMs = RemoteTxCutGuard::kDeadlineMs;
constexpr qint64 kBoundMs = RemoteTxWatchdog::kLinkCutMs + 100;
constexpr auto kTick = 100ms;

struct Cut {
    quint64 epoch{0};
    quint64 serial{0};
    qint64 silentMs{0};
    std::thread::id thread;
    Clock::time_point at;
};

// Shared with the guard's CutFn; outlives the guard.
struct CutLog {
    std::mutex mutex;
    std::vector<Cut> cuts;

    RemoteTxCutGuard::CutFn fn()
    {
        return [this](quint64 epoch, quint64 serial, qint64 silentMs) {
            const Clock::time_point at = Clock::now();
            const std::lock_guard<std::mutex> lock(mutex);
            cuts.push_back(Cut{epoch, serial, silentMs, std::this_thread::get_id(), at});
        };
    }

    std::vector<Cut> snapshot()
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return cuts;
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
};

// Acceptance 1.
void TestRemoteTxCutGuard::cutsOnTimeWithMainHeld()
{
    CutLog log;
    RemoteTxCutGuard guard(log.fn());
    const quint64 serial = guard.arm(7);
    QCOMPARE(serial, quint64(1));

    Feeder feeder(guard, 7, 500ms);
    holdMain(1500ms);
    feeder.join();

    const std::vector<Cut> cuts = log.snapshot();
    QCOMPARE(int(cuts.size()), 1);
    const Cut& cut = cuts.front();
    QVERIFY(cut.thread != std::this_thread::get_id());
    QVERIFY(cut.thread != feeder.id());
    QCOMPARE(cut.epoch, quint64(7));
    QCOMPARE(cut.serial, serial);
    const qint64 afterLast = msBetween(feeder.lastStamp(), cut.at);
    QVERIFY2(afterLast > kDeadlineMs && afterLast <= kBoundMs,
             qPrintable(QStringLiteral("cut %1 ms after the last stamp").arg(afterLast)));
    QVERIFY2(cut.silentMs > kDeadlineMs && cut.silentMs <= kBoundMs,
             qPrintable(QStringLiteral("silentMs %1").arg(cut.silentMs)));
    QVERIFY(!guard.isJudging());
}

// Acceptance 2.
void TestRemoteTxCutGuard::noFalseCutWithStreamAliveWhileMainHeld()
{
    CutLog log;
    RemoteTxCutGuard guard(log.fn());
    const quint64 serial = guard.arm(7);

    Feeder feeder(guard, 7, 1500ms);
    holdMain(1000ms);
    QVERIFY(log.snapshot().empty());
    QVERIFY(guard.isJudging());

    feeder.join();
    holdMain(std::chrono::milliseconds(kBoundMs + 200));

    const std::vector<Cut> cuts = log.snapshot();
    QCOMPARE(int(cuts.size()), 1);
    QCOMPARE(cuts.front().epoch, quint64(7));
    QCOMPARE(cuts.front().serial, serial);
    const qint64 afterLast = msBetween(feeder.lastStamp(), cuts.front().at);
    QVERIFY2(afterLast > kDeadlineMs && afterLast <= kBoundMs,
             qPrintable(QStringLiteral("cut %1 ms after the last stamp").arg(afterLast)));
}

// Acceptance 3.
void TestRemoteTxCutGuard::notJudgingBeforeOffMainReceipt()
{
    CutLog log;
    RemoteTxCutGuard guard(log.fn());
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
    CutLog log;
    RemoteTxCutGuard guard(log.fn());
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
    const std::vector<Cut> cuts = log.snapshot();
    QCOMPARE(int(cuts.size()), 1);
    QCOMPARE(cuts.front().serial, serial);
    const qint64 afterLast = msBetween(lastStamp, cuts.front().at);
    QVERIFY2(afterLast > kDeadlineMs && afterLast <= kBoundMs,
             qPrintable(QStringLiteral("cut %1 ms after the last stamp").arg(afterLast)));
}

// Acceptance 5.
void TestRemoteTxCutGuard::disarmAndRearm()
{
    CutLog log;
    RemoteTxCutGuard guard(log.fn());

    // disarm() before the deadline: no cut.
    const quint64 first = guard.arm(7);
    guard.heardOffMain(7, Clock::now());
    QVERIFY(guard.isJudging());
    holdMain(200ms);
    guard.disarm();
    QVERIFY(!guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QVERIFY(log.snapshot().empty());

    // A fresh arm cuts on silence.
    const quint64 second = guard.arm(7);
    QVERIFY(second > first);
    guard.heardOffMain(7, Clock::now());
    holdMain(std::chrono::milliseconds(kBoundMs + 200));
    std::vector<Cut> cuts = log.snapshot();
    QCOMPARE(int(cuts.size()), 1);
    QCOMPARE(cuts.back().serial, second);

    // A heard after the cut does not undo it or start judging again.
    guard.heardOffMain(7, Clock::now());
    QVERIFY(!guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QCOMPARE(int(log.snapshot().size()), 1);

    // arm(7) again after a cut: new serial, not judging until heardOffMain.
    const quint64 third = guard.arm(7);
    QVERIFY(third > second);
    QCOMPARE(guard.armSerial(), third);
    QVERIFY(!guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 100));
    QCOMPARE(int(log.snapshot().size()), 1);

    const Clock::time_point heard = Clock::now();
    guard.heardOffMain(7, heard);
    QVERIFY(guard.isJudging());
    holdMain(std::chrono::milliseconds(kBoundMs + 200));
    cuts = log.snapshot();
    QCOMPARE(int(cuts.size()), 2);
    QCOMPARE(cuts.back().serial, third);
    QCOMPARE(cuts.back().epoch, quint64(7));
    const qint64 afterLast = msBetween(heard, cuts.back().at);
    QVERIFY2(afterLast > kDeadlineMs && afterLast <= kBoundMs,
             qPrintable(QStringLiteral("cut %1 ms after the heard").arg(afterLast)));
}

// Acceptance 6.
void TestRemoteTxCutGuard::destroyingJudgingGuardIsPromptAndSilent()
{
    CutLog log;
    auto guard = std::make_unique<RemoteTxCutGuard>(log.fn());
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

QTEST_GUILESS_MAIN(TestRemoteTxCutGuard)
#include "tst_remote_tx_cut_guard.moc"
