#pragma once
// no-port-check: NereusSDR-original process ownership.

#include <QString>

#include <memory>

class QLockFile;

namespace NereusSDR {

// Own one settings profile from before CoreInit until every writer has stopped.
// The lock is deliberately independent of StationHost: remote-only windows
// still load and write the same profile.
class StationHandover {
public:
    explicit StationHandover(const QString& profile);
    ~StationHandover();

    StationHandover(const StationHandover&) = delete;
    StationHandover& operator=(const StationHandover&) = delete;

    bool acquire(int timeoutMs, QString* error = nullptr);
    // Only the selected profile's socket is contacted. This is the desktop
    // path; the console command retains its existing packaged-home finder.
    bool reclaimFromBackground(int timeoutMs, QString* error = nullptr);
    void release();
    bool ownsProfile() const;
    QString lockPath() const;
    QString controlSocketPath() const;

private:
    QString m_directory;
    std::unique_ptr<QLockFile> m_lock;
};

} // namespace NereusSDR
