// no-port-check: NereusSDR-original.
// =================================================================
// src/core/DspControlThread.h  (NereusSDR)
// =================================================================
// A DSP control lane: one thread per lane (receive, transmit) that runs WDSP
// control calls (setters, getters, channel state changes) off the event
// loop, so a DSP worker that holds its channel's lock for a long block can
// never stall the event loop (R-R3-39, spec section 4.6). Thetis has no
// equivalent; no upstream logic is involved.
//
// Jobs run one at a time, in the order they were posted (so each posting
// thread's own order is kept). A keyed job replaces a still-queued job with
// the same key (the newest value wins, at the newest position), but never
// one queued before a barrier. request() runs a job on the lane and hands
// its result to a callback on the context object's thread, and never after
// the context has been deleted. Nothing here ever makes the caller wait for
// the lane.
//
// Placement: a lane is a plain QThread started from the Core's main thread.
// When thread placement is on, the main thread already runs on the
// housekeeping cores (ThreadPlacement::start), and a new thread starts with
// its creator's cores, so a lane runs with everything else; it takes no
// core of its own and registers no role.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-39).
// =================================================================

#pragma once

#include <QObject>
#include <QThread>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace NereusSDR {

enum class DspLane { Receive, Transmit };

namespace detail {

/// Carries one request()'s completion to its context's thread. It has no
/// thread of its own (moved to no thread), so the lane may emit it and
/// delete it; the queued connection to the context is broken by Qt if the
/// context is deleted first.
class DspRequestRelay final : public QObject {
    Q_OBJECT
public:
    DspRequestRelay() = default;

signals:
    void finished();
};

} // namespace detail

class DspControlThread final : public QObject {
    Q_OBJECT

public:
    using Job = std::function<void()>;

    explicit DspControlThread(DspLane lane, QObject* parent = nullptr);
    ~DspControlThread() override;

    DspControlThread(const DspControlThread&) = delete;
    DspControlThread& operator=(const DspControlThread&) = delete;

    DspLane lane() const noexcept { return m_lane; }

    /// Starts the lane's thread. Jobs posted before start() wait for it.
    /// Call from the thread that owns this object.
    void start();

    /// Runs every job already queued, then ends and joins the lane's thread.
    /// Jobs posted after stop() begins are dropped with a warning. A lane
    /// that was never started drops its queued jobs. Never call it from the
    /// lane itself. start() may be called again afterwards.
    void stop();

    /// Queues a job. Any thread; never waits for the lane.
    void post(Job job);

    /// Queues a job that replaces a still-queued job with the same key (the
    /// replaced job never runs; the new one takes the end of the queue). A
    /// job queued before a barrier is never replaced. Any thread.
    void postKeyed(quint64 key, Job job);

    /// Queues a job that keyed jobs never cross: no keyed job posted after
    /// it replaces one posted before it. Any thread.
    void postBarrier(Job job);

    /// Runs `job` on the lane, then `done(result)` on `context`'s thread
    /// (through its event loop). `done` never runs once `context` has been
    /// deleted. Any thread; never waits for the lane.
    template <class R>
    void request(std::function<R()> job, QObject* context, std::function<void(R)> done);

    /// True when called from inside one of this lane's jobs.
    bool isCurrentThread() const noexcept;

    /// Tests only: waits until the queue is empty and no job is running.
    /// False on timeout.
    bool waitIdleForTest(int timeoutMs);

private:
    enum class EntryKind { Plain, Keyed, Barrier };
    struct Entry {
        EntryKind kind{EntryKind::Plain};
        quint64 key{0};
        Job job;
    };

    void enqueue(Entry entry);
    void scheduleDrainLocked();
    void drain();

    const DspLane m_lane;

    std::unique_ptr<QThread> m_thread;   // owner's thread only
    std::unique_ptr<QObject> m_worker;   // lives on m_thread; runs drain()
    std::atomic<Qt::HANDLE> m_laneThreadId{nullptr};

    std::mutex m_mutex;
    std::condition_variable m_idle;
    std::deque<Entry> m_queue;           // m_mutex
    bool m_running{false};               // m_mutex: thread started
    bool m_accepting{true};              // m_mutex: false while stopping
    bool m_drainScheduled{false};        // m_mutex
    bool m_busy{false};                  // m_mutex: a job is running
};

template <class R>
void DspControlThread::request(std::function<R()> job, QObject* context,
                               std::function<void(R)> done)
{
    if (context == nullptr) {
        // No thread to answer on: run the job for its effect only.
        post([job = std::move(job)]() {
            if (job) {
                (void)job();
            }
        });
        return;
    }
    auto relay = std::make_shared<detail::DspRequestRelay>();
    // No thread affinity: the lane emits it and its last reference (the
    // job below) deletes it on the lane.
    relay->moveToThread(nullptr);
    auto result = std::make_shared<std::optional<R>>();
    QObject::connect(
        relay.get(), &detail::DspRequestRelay::finished, context,
        [result, done = std::move(done)]() mutable {
            if (result->has_value() && done) {
                done(std::move(**result));
            }
        },
        Qt::QueuedConnection);
    post([relay, result, job = std::move(job)]() mutable {
        if (job) {
            result->emplace(job());
        }
        emit relay->finished();
    });
}

} // namespace NereusSDR
