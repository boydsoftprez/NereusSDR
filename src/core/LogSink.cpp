// no-port-check: NereusSDR-original. See LogSink.h.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/LogSink.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-49 (remote-window parity Task
// 22). See the header for the design.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Created (remote-window parity Task 22,
//                                    R-R3-49). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Bounded producer retries and drain
//                                    batches, refined with OpenAI Codex
//                                    assistance.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave: tryDrainNow for a fatal
//                                    message. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "LogSink.h"

#include <QByteArray>
#include <QFile>
#include <QScopeGuard>

#include <chrono>
#include <cstdio>

namespace NereusSDR {

namespace {

// Set while this thread is inside a drain, so a message logged from the
// drain itself (a fatal from QString or QFile) never tries the drain mutex
// it already holds.
thread_local bool t_draining = false;

std::size_t roundUpToPowerOfTwo(std::size_t value)
{
    std::size_t result = 2;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

} // namespace

LogSink& LogSink::instance()
{
    // Intentionally never destroyed, as CoreInit's log file is: Qt can log
    // from QThreadStoragePrivate::finish after static destructors run, and
    // a line offered then must still land in a live ring.
    static LogSink* const sink = std::make_unique<LogSink>().release();
    return *sink;
}

LogSink::LogSink(std::size_t capacity)
{
    const std::size_t size = roundUpToPowerOfTwo(capacity);
    m_cells = std::make_unique<Cell[]>(size);
    m_mask = size - 1;
    for (std::size_t i = 0; i < size; ++i) {
        m_cells[i].sequence.store(i, std::memory_order_relaxed);
    }
    m_recent.reserve(kRecentCapacity);
}

LogSink::~LogSink()
{
    stop();
}

bool LogSink::offer(QString line)
{
    std::size_t pos = m_enqueuePos.load(std::memory_order_relaxed);
    Cell* cell = nullptr;
    bool claimed = false;
    constexpr int kMaxClaimAttempts = 64;
    for (int attempt = 0; attempt < kMaxClaimAttempts; ++attempt) {
        cell = &m_cells[pos & m_mask];
        const std::size_t sequence = cell->sequence.load(std::memory_order_acquire);
        const auto difference = static_cast<std::intptr_t>(sequence)
            - static_cast<std::intptr_t>(pos);
        if (difference == 0) {
            if (m_enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                claimed = true;
                break;
            }
        } else if (difference < 0) {
            // Full: the writer is behind. Never wait for it.
            m_dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        } else {
            pos = m_enqueuePos.load(std::memory_order_relaxed);
        }
    }
    if (!claimed) {
        // This path is reached only when a slot was not claimed. An audio
        // producer never spins indefinitely under logging contention.
        m_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    cell->line = std::move(line);
    cell->sequence.store(pos + 1, std::memory_order_release);
    return true;
}

bool LogSink::take(QString* line)
{
    std::size_t pos = m_dequeuePos.load(std::memory_order_relaxed);
    Cell* cell = nullptr;
    for (;;) {
        cell = &m_cells[pos & m_mask];
        const std::size_t sequence = cell->sequence.load(std::memory_order_acquire);
        const auto difference = static_cast<std::intptr_t>(sequence)
            - static_cast<std::intptr_t>(pos + 1);
        if (difference == 0) {
            if (m_dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                break;
            }
        } else if (difference < 0) {
            return false; // empty
        } else {
            pos = m_dequeuePos.load(std::memory_order_relaxed);
        }
    }
    *line = std::move(cell->line);
    cell->line = QString();
    cell->sequence.store(pos + m_mask + 1, std::memory_order_release);
    return true;
}

void LogSink::setOutputs(QFile* file, bool toStderr)
{
    const std::lock_guard<std::mutex> lock(m_drainMutex);
    m_file = file;
    m_toStderr = toStderr;
}

void LogSink::start()
{
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    m_stopRequested.store(false, std::memory_order_release);
    m_writer = std::thread([this]() { writerLoop(); });
}

void LogSink::stop()
{
    if (m_running.load(std::memory_order_acquire)) {
        m_stopRequested.store(true, std::memory_order_release);
        if (m_writer.joinable()) {
            m_writer.join();
        }
        m_running.store(false, std::memory_order_release);
    }
    drainNow();
}

void LogSink::drainNow()
{
    const std::lock_guard<std::mutex> lock(m_drainMutex);
    drainLocked();
}

bool LogSink::tryDrainNow()
{
    if (t_draining) {
        return false;
    }
    std::unique_lock<std::mutex> lock(m_drainMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return false;
    }
    drainLocked();
    return true;
}

void LogSink::writerLoop()
{
    while (!m_stopRequested.load(std::memory_order_acquire)) {
        {
            const std::lock_guard<std::mutex> lock(m_drainMutex);
            drainLocked();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kDrainIntervalMs));
    }
}

void LogSink::drainLocked()
{
    t_draining = true;
    const auto leaving = qScopeGuard([] { t_draining = false; });
    QList<QString> lines;
    QString line;
    // A producer can refill every slot as fast as it is consumed. A drain
    // takes no more than one ring's worth, so it always returns even then.
    // A following writer turn handles what arrived during this batch.
    const std::size_t batchLimit = m_mask + 1;
    for (std::size_t taken = 0; taken < batchLimit && take(&line); ++taken) {
        lines.append(std::move(line));
        if (m_afterTake) { m_afterTake(); }
    }
    const quint64 dropped = m_dropped.load(std::memory_order_relaxed);
    if (dropped != m_droppedReported) {
        lines.append(QStringLiteral("[log] %1 log lines were dropped because the log could "
                                    "not keep up.\n")
                         .arg(dropped - m_droppedReported));
        m_droppedReported = dropped;
    }
    if (lines.isEmpty()) {
        return;
    }
    for (const QString& text : lines) {
        if (m_beforeWrite) {
            m_beforeWrite();
        }
        const QByteArray utf8 = text.toUtf8();
        if (m_file != nullptr && m_file->isOpen()) {
            m_file->write(utf8);
        }
        if (m_toStderr) {
            std::fwrite(utf8.constData(), 1, static_cast<std::size_t>(utf8.size()), stderr);
        }
    }
    if (m_file != nullptr && m_file->isOpen()) {
        m_file->flush();
    }
    if (m_toStderr) {
        std::fflush(stderr);
    }
    const std::lock_guard<std::mutex> recentLock(m_recentMutex);
    for (const QString& text : lines) {
        QString trimmed = text;
        if (trimmed.endsWith(QLatin1Char('\n'))) {
            trimmed.chop(1);
        }
        m_recent.push_back(LogSinkLine{m_nextSequence++, trimmed});
    }
    if (static_cast<int>(m_recent.size()) > kRecentCapacity) {
        m_recent.erase(m_recent.begin(),
                       m_recent.begin() + (static_cast<int>(m_recent.size()) - kRecentCapacity));
    }
}

QList<LogSinkLine> LogSink::linesSince(quint64 after) const
{
    const std::lock_guard<std::mutex> lock(m_recentMutex);
    QList<LogSinkLine> out;
    for (const LogSinkLine& entry : m_recent) {
        if (entry.sequence > after) {
            out.append(entry);
        }
    }
    return out;
}

quint64 LogSink::lastSequence() const
{
    const std::lock_guard<std::mutex> lock(m_recentMutex);
    return m_recent.empty() ? 0 : m_recent.back().sequence;
}

} // namespace NereusSDR
