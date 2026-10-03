// no-port-check: NereusSDR-original.
// =================================================================
// src/core/DspControlThread.cpp  (NereusSDR)
// =================================================================
// See DspControlThread.h. NereusSDR-original; no upstream logic.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-39).
// =================================================================

#include "core/DspControlThread.h"

#include "core/LogCategories.h"

#include <QMetaObject>

#include <chrono>

namespace NereusSDR {

DspControlThread::DspControlThread(DspLane lane, QObject* parent)
    : QObject(parent)
    , m_lane(lane)
{
}

DspControlThread::~DspControlThread()
{
    stop();
}

void DspControlThread::start()
{
    if (m_thread) {
        return;
    }
    auto thread = std::make_unique<QThread>();
    thread->setObjectName(m_lane == DspLane::Receive ? QStringLiteral("DspControlRx")
                                                     : QStringLiteral("DspControlTx"));
    auto worker = std::make_unique<QObject>();
    worker->moveToThread(thread.get());
    // Runs on the new thread before its event loop, so the lane's ID is
    // known before any job can run.
    connect(thread.get(), &QThread::started, worker.get(),
            [this]() {
                m_laneThreadId.store(QThread::currentThreadId(), std::memory_order_release);
            },
            Qt::DirectConnection);
    thread->start();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_thread = std::move(thread);
    m_worker = std::move(worker);
    m_running = true;
    m_accepting = true;
    m_drainScheduled = false;
    if (!m_queue.empty()) {
        scheduleDrainLocked();
    }
}

void DspControlThread::stop()
{
    if (!m_thread) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_queue.empty()) {
            qCWarning(lcDsp) << "DSP control lane was never started;"
                             << m_queue.size() << "queued job(s) dropped";
            m_queue.clear();
        }
        return;
    }
    if (isCurrentThread()) {
        qCWarning(lcDsp) << "DSP control lane cannot stop itself from one of its jobs";
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accepting = false;
        // Queued after any drain already scheduled: runs what is left, then
        // ends the thread's event loop.
        QMetaObject::invokeMethod(
            m_worker.get(),
            [this]() {
                drain();
                QThread::currentThread()->quit();
            },
            Qt::QueuedConnection);
    }
    m_thread->wait();

    std::lock_guard<std::mutex> lock(m_mutex);
    // The thread has finished; its worker has no events left to run.
    m_worker.reset();
    m_thread.reset();
    m_laneThreadId.store(nullptr, std::memory_order_release);
    m_running = false;
    m_drainScheduled = false;
    m_busy = false;
    m_idle.notify_all();
}

void DspControlThread::post(Job job)
{
    enqueue(Entry{EntryKind::Plain, 0, std::move(job)});
}

void DspControlThread::postKeyed(quint64 key, Job job)
{
    enqueue(Entry{EntryKind::Keyed, key, std::move(job)});
}

void DspControlThread::postBarrier(Job job)
{
    enqueue(Entry{EntryKind::Barrier, 0, std::move(job)});
}

bool DspControlThread::isCurrentThread() const noexcept
{
    const Qt::HANDLE lane = m_laneThreadId.load(std::memory_order_acquire);
    return lane != nullptr && QThread::currentThreadId() == lane;
}

bool DspControlThread::waitIdleForTest(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_idle.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                           [this]() { return m_queue.empty() && !m_busy; });
}

void DspControlThread::enqueue(Entry entry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_accepting) {
        qCWarning(lcDsp) << "DSP control lane is stopping; a job was dropped";
        return;
    }
    if (entry.kind == EntryKind::Keyed) {
        // Newest first, back to the most recent barrier: a job queued
        // before a barrier stays.
        for (auto it = m_queue.rbegin(); it != m_queue.rend(); ++it) {
            if (it->kind == EntryKind::Barrier) {
                break;
            }
            if (it->kind == EntryKind::Keyed && it->key == entry.key) {
                m_queue.erase(std::next(it).base());
                break;
            }
        }
    }
    m_queue.push_back(std::move(entry));
    scheduleDrainLocked();
}

void DspControlThread::scheduleDrainLocked()
{
    if (!m_running || m_drainScheduled) {
        return;
    }
    m_drainScheduled = true;
    // Posts one event to the lane; never waits for it. Called under
    // m_mutex so stop() cannot delete the worker in between; the lane
    // never takes m_mutex while holding Qt's event queue lock, so there is
    // no lock-order cycle.
    QMetaObject::invokeMethod(m_worker.get(), [this]() { drain(); },
                              Qt::QueuedConnection);
}

void DspControlThread::drain()
{
    for (;;) {
        Job job;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty()) {
                m_drainScheduled = false;
                m_busy = false;
                m_idle.notify_all();
                return;
            }
            job = std::move(m_queue.front().job);
            m_queue.pop_front();
            m_busy = true;
        }
        if (job) {
            job();
        }
        // The job, and anything it captured (a request's relay), is
        // released here, on the lane.
    }
}

} // namespace NereusSDR
