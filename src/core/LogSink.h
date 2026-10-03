#pragma once
// no-port-check: NereusSDR-original. Where the process's log lines go: a
// bounded queue no logging thread ever waits on, drained to the log file
// and stderr by a writer thread of its own.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/LogSink.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-49 (remote-window parity Task
// 22, the controller's ruling 2: a debug category turned on while the radio
// is on the air must never stall the audio path, in a local window or a
// remote one).
//
// CoreInit's Qt message handler used to write each line to the log file and
// to stderr on the thread that logged it, flushing every time. A logging
// category turned on while keyed could then put file I/O on the audio
// thread. The handler now only offers the line to this sink: a fixed ring
// of slots claimed with at most 64 compare-and-swap attempts (the bounded
// queue of Dmitry Vyukov's design), so offering a line never takes a lock
// or waits on the writer.
// When the ring is full the line is counted and dropped; the writer says
// how many were dropped in the log itself.
//
// The writer thread drains the ring every kDrainIntervalMs, writes each
// line to the file and stderr, and keeps the newest kRecentCapacity lines
// with increasing sequence numbers. The Core's `coreLog` record stream
// (the station link, section 7.7) reads those (linesSince), on the Core's
// main thread, under a mutex only the writer and that reader take.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Created (remote-window parity Task 22,
//                                    R-R3-49). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave: tryDrainNow, so a fatal
//                                    message reaches the file before Qt
//                                    aborts. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QList>
#include <QString>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class QFile;

namespace NereusSDR {

/// One line the writer has written, with its place in the process's log.
struct LogSinkLine {
    quint64 sequence = 0;
    QString text;
};

class LogSink {
public:
    static constexpr std::size_t kQueueCapacity = 8192;   // a power of two
    static constexpr int kRecentCapacity = 1000;
    static constexpr int kDrainIntervalMs = 20;

    /// The process's sink (never destroyed: Qt may log during static
    /// teardown).
    static LogSink& instance();

    /// For a test: a sink of its own, with no writer thread until start().
    explicit LogSink(std::size_t capacity = kQueueCapacity);
    ~LogSink();
    LogSink(const LogSink&) = delete;
    LogSink& operator=(const LogSink&) = delete;

    /// Offers one line (its trailing newline included). Never waits on
    /// the writer and never takes a lock; false when the ring was full or
    /// producer contention exceeded the claim bound (counted as dropped).
    bool offer(QString line);

    /// Where the writer writes: a file already open for writing (or null
    /// for none) and whether it also writes stderr. Set before start().
    void setOutputs(QFile* file, bool toStderr);

    /// Starts the writer thread (no change when it runs).
    void start();
    /// Drains what waits, then stops the writer thread.
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }

    /// Drains what waits now, on the calling thread (a fatal message, a
    /// shutdown, a test). Waits for a drain the writer is doing.
    void drainNow();
    /// Drains what waits now, on the calling thread, unless a drain is
    /// already running (the writer's, or this thread's own): then false at
    /// once. For a fatal message, which Qt follows with abort().
    bool tryDrainNow();

    /// The recent lines with a sequence above `after`, oldest first.
    QList<LogSinkLine> linesSince(quint64 after) const;
    /// The newest line's sequence (0 before any).
    quint64 lastSequence() const;
    /// Lines dropped because the ring was full, since the process started.
    quint64 droppedCount() const { return m_dropped.load(std::memory_order_relaxed); }

    /// For a test: runs before each line is written, on the writer's thread
    /// (to stand in for a slow disk).
    void setBeforeWriteForTest(std::function<void()> hook) { m_beforeWrite = std::move(hook); }
    /// For a test: runs after a line is removed from the ring, before the
    /// next take. Used to hold a producer refill at every drain step.
    void setAfterTakeForTest(std::function<void()> hook) { m_afterTake = std::move(hook); }

private:
    struct Cell {
        std::atomic<std::size_t> sequence{0};
        QString line;
    };

    bool take(QString* line);
    void drainLocked();
    void writerLoop();

    std::unique_ptr<Cell[]> m_cells;
    std::size_t m_mask = 0;
    alignas(64) std::atomic<std::size_t> m_enqueuePos{0};
    alignas(64) std::atomic<std::size_t> m_dequeuePos{0};
    std::atomic<quint64> m_dropped{0};
    quint64 m_droppedReported = 0;

    std::mutex m_drainMutex;          // the writer, drainNow() and tryDrainNow(), never offer()
    QFile* m_file = nullptr;
    bool m_toStderr = false;
    std::function<void()> m_beforeWrite;
    std::function<void()> m_afterTake;

    mutable std::mutex m_recentMutex; // the writer and linesSince()
    std::vector<LogSinkLine> m_recent; // oldest first
    quint64 m_nextSequence = 1;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    std::thread m_writer;
};

} // namespace NereusSDR
