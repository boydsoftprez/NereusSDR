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
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QLockFile>
#include <QSaveFile>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include <QScopeGuard>

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace NereusSDR {

// Only the drain thread accesses these files. A profile lock prevents a
// second process from trimming or pruning logs that this writer still owns.
class BoundedLogFile {
public:
    bool open(const QString& directory, qint64 maxBytes, int maxFiles)
    {
        m_directory = QDir(QDir(directory).absolutePath());
        m_maxBytes = maxBytes;
        m_maxFiles = maxFiles;
        if (maxBytes < 64 || maxFiles < 2 || !QDir().mkpath(directory)) {
            return fail("could not prepare log directory or retention limits");
        }
        m_lock = std::make_unique<QLockFile>(m_directory.filePath("nereussdr-log.lock"));
        // A live process keeps ownership regardless of the lock's age.
        m_lock->setStaleLockTime(0);
        if (!m_lock->tryLock(0)) {
            return fail("profile log writer already owned or lock unavailable");
        }
        for (const QString& name : names()) {
            if (!trimLegacy(m_directory.filePath(name))) {
                return fail("could not bound a closed legacy log");
            }
        }
        m_writable = rotate();
        return m_writable;
    }

    void write(const QByteArray& record)
    {
        if (!m_writable || !m_file) { return; }
        QByteArray bytes = record;
        if (bytes.size() > m_maxBytes) {
            const QByteArray marker("\n[log] oversized log entry truncated.\n");
            qsizetype end = static_cast<qsizetype>(m_maxBytes - marker.size());
            // Keep the beginning (timestamp/severity) and a complete UTF-8
            // code point; the full original still goes to stderr/recent.
            while (end > 0 && (static_cast<unsigned char>(bytes.at(end)) & 0xc0) == 0x80) {
                --end;
            }
            bytes = bytes.left(end) + marker;
        }
        if (m_file->size() > m_maxBytes - bytes.size()) {
            if (!rotate()) {
                m_writable = false;
                return;
            }
        }
        if (m_file->write(bytes) != bytes.size()) {
            m_writable = false;
            fail("log write failed; file output disabled");
        }
    }

    void flush()
    {
        if (m_file && m_file->isOpen() && !m_file->flush()) {
            m_writable = false;
            fail("log flush failed; file output disabled");
        }
    }

private:
    QStringList names() const
    {
        return m_directory.entryList({"nereussdr-*.log"},
                                     QDir::Files | QDir::NoSymLinks, QDir::Name);
    }

    bool fail(const char* reason) const
    {
        // Do not recurse through Qt's handler while holding the drain lock.
        std::fprintf(stderr, "Warning: %s (%s)\n", reason,
                     m_directory.path().toLocal8Bit().constData());
        return false;
    }

    bool trimLegacy(const QString& path)
    {
        QFile source(path);
        if (source.size() <= m_maxBytes) { return true; }
        if (!source.open(QIODevice::ReadOnly)) { return false; }
        const QByteArray marker("[log] earlier log bytes truncated.\n");
        const qint64 tailSize = m_maxBytes - marker.size();
        if (!source.seek(source.size() - tailSize)) { return false; }
        QByteArray tail = source.read(tailSize);
        if (tail.size() != tailSize) { return false; }
        // The seek can land inside a UTF-8 code point. Discard only its
        // continuation bytes, preserving the newest diagnostics.
        qsizetype start = 0;
        while (start < tail.size() && (static_cast<unsigned char>(tail.at(start)) & 0xc0) == 0x80) {
            ++start;
        }
        source.close();
        QSaveFile replacement(path);
        // QSaveFile's atomic rename keeps the old diagnostic if disk I/O
        // fails; never fall back to in-place truncation.
        replacement.setDirectWriteFallback(false);
        if (!replacement.open(QIODevice::WriteOnly)) { return false; }
        replacement.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        const QByteArray bounded = marker + tail.mid(start);
        return replacement.write(bounded) == bounded.size() && replacement.commit();
    }

    bool rotate()
    {
        if (m_file && !m_file->flush()) { return fail("could not flush log before rotation"); }
        const QString aliasPath = m_directory.filePath(
#ifdef Q_OS_WIN
            "nereussdr.log.lnk"
#else
            "nereussdr.log"
#endif
        );
        QString lastGoodPath = m_file ? m_file->fileName() : QFileInfo(aliasPath).symLinkTarget();
        if (lastGoodPath.isEmpty()) {
            const QStringList previous = names();
            if (!previous.isEmpty()) { lastGoodPath = m_directory.filePath(previous.last()); }
        }
        auto next = std::make_unique<QFile>();
        const QString prefix = "nereussdr-" +
            QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz");
        // NewOnly avoids truncating a same-millisecond log on restart.
        bool opened = false;
        for (int attempt = 0; attempt < 1000; ++attempt) {
            next->setFileName(m_directory.filePath(prefix +
                QString("-%1.log").arg(++m_serial, 6, 10, QLatin1Char('0'))));
            if (next->open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
                opened = true;
                break;
            }
            if (!QFile::exists(next->fileName())) { break; }
        }
        if (!opened) { return fail("could not open next log; file output disabled"); }
        const auto abandon = qScopeGuard([&]() {
            if (next) {
                const QString path = next->fileName();
                next->close();
                QFile::remove(path);
            }
        });
        if (!next->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            return fail("could not set next log permissions");
        }
        QStringList files = names();
        while (files.size() > m_maxFiles) {
            // Preserve the last good file during runtime AND startup,
            // even when a clock change made its filename sort oldest.
            auto oldest = std::find_if(files.begin(), files.end(), [&](const QString& name) {
                const QString path = m_directory.filePath(name);
                return path != next->fileName() && path != lastGoodPath;
            });
            if (oldest == files.end() || !m_directory.remove(*oldest)) {
                return fail("could not prune old log; file output disabled");
            }
            files.erase(oldest);
        }
        QString alias = m_directory.filePath("nereussdr.log");
#ifdef Q_OS_WIN
        // QFile::link uses shortcuts on Windows. Preserve the same .lnk
        // alias CoreInit used, without losing file logging on that platform.
        alias += ".lnk";
#endif
        const QString pendingAlias = alias +
#ifdef Q_OS_WIN
            ".new.lnk";
#else
            ".new";
#endif
        QFile::remove(pendingAlias);
        if (!QFile::link(next->fileName(), pendingAlias)) {
            return fail("could not prepare current log alias");
        }
        // Replace the previous link atomically, preserving it on failure.
#ifdef Q_OS_WIN
        const bool aliasReplaced = MoveFileExW(
            reinterpret_cast<LPCWSTR>(pendingAlias.utf16()),
            reinterpret_cast<LPCWSTR>(alias.utf16()), MOVEFILE_REPLACE_EXISTING) != 0;
#else
        const auto pendingBytes = QFile::encodeName(pendingAlias);
        const auto aliasBytes = QFile::encodeName(alias);
        const bool aliasReplaced = std::rename(pendingBytes.constData(), aliasBytes.constData()) == 0;
#endif
        if (!aliasReplaced) {
            QFile::remove(pendingAlias);
            return fail("could not replace current log alias");
        }
        m_file = std::move(next); // closes the previous file after success
        return true;
    }

    QDir m_directory;
    qint64 m_maxBytes = 0;
    int m_maxFiles = 0;
    quint64 m_serial = 0;
    bool m_writable = false;
    std::unique_ptr<QLockFile> m_lock;
    std::unique_ptr<QFile> m_file;
};

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
    m_rotatingFile.reset();
    m_file = file;
    m_toStderr = toStderr;
}

bool LogSink::setRotatingOutput(const QString& directory, bool toStderr,
                                qint64 maxBytes, int maxFiles)
{
    const std::lock_guard<std::mutex> lock(m_drainMutex);
    m_file = nullptr;
    m_rotatingFile.reset();
    m_toStderr = toStderr;
    auto output = std::make_unique<BoundedLogFile>();
    if (!output->open(directory, maxBytes, maxFiles)) { return false; }
    m_rotatingFile = std::move(output);
    return true;
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
        if (m_rotatingFile) {
            m_rotatingFile->write(utf8);
        } else if (m_file != nullptr && m_file->isOpen()) {
            m_file->write(utf8);
        }
        if (m_toStderr) {
            std::fwrite(utf8.constData(), 1, static_cast<std::size_t>(utf8.size()), stderr);
        }
    }
    if (m_rotatingFile) {
        m_rotatingFile->flush();
    } else if (m_file != nullptr && m_file->isOpen()) {
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
