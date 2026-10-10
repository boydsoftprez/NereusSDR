// =================================================================
// src/core/SettingsFileWriter.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  One writer thread for the settings
// file, and the signal that reports each background save's result.
// =================================================================

#pragma once

#include <QObject>
#include <QString>
#include <QThreadPool>

#include <functional>

namespace NereusSDR {

/// Runs AppSettings::saveInBackground()'s file work away from the caller.
/// The settings save rebuilds the whole file and waits for the disk (140 to
/// 470 ms for a 671 KB file on a Rock 5C's eMMC, bench 2026-10-10), and the
/// Core's main thread, which also sends the receive audio, must not wait
/// with it. One thread, so saves reach the disk in the order they were asked
/// for.
class SettingsFileWriter : public QObject {
    Q_OBJECT
public:
    explicit SettingsFileWriter(QObject* parent = nullptr);
    /// Waits for the jobs already handed over.
    ~SettingsFileWriter() override;

    /// Queues `job` for the writer thread.
    void run(std::function<void()> job);

    /// Returns once every job handed over has finished.
    void waitUntilIdle();

    /// Reports from the writer thread; a receiver on another thread gets it
    /// queued.
    void report(quint64 ticket, bool saved, const QString& error);

signals:
    /// One background save has finished. `ticket` is the value
    /// AppSettings::saveInBackground() returned for it. Not emitted for a
    /// save that a later save replaced before it reached the disk.
    void finished(quint64 ticket, bool saved, const QString& error);

private:
    QThreadPool m_pool;
};

} // namespace NereusSDR
