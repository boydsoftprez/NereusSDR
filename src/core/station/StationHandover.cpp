// no-port-check: NereusSDR-original process ownership.

#include "core/station/StationHandover.h"

#include "core/AppSettings.h"
#include "core/daemon/StationControlSocket.h"

#include <QDir>
#include <QElapsedTimer>
#include <QLockFile>

namespace NereusSDR {

StationHandover::StationHandover(const QString& profile)
    : m_directory(AppSettings::resolveConfigDir(profile))
    , m_lock(std::make_unique<QLockFile>(lockPath()))
{
    // A station can own the profile for months. The default 30-second age
    // must never turn a live owner's lock into a stale lock.
    m_lock->setStaleLockTime(0);
}

StationHandover::~StationHandover() = default;

QString StationHandover::lockPath() const
{
    return QDir(m_directory).filePath(QStringLiteral("station.lock"));
}

QString StationHandover::controlSocketPath() const
{
    return QDir(m_directory).filePath(
        QString::fromLatin1(StationControlSocket::kSocketName));
}

bool StationHandover::ownsProfile() const
{
    return m_lock->isLocked();
}

bool StationHandover::acquire(int timeoutMs, QString* error)
{
    if (error) { error->clear(); }
    if (ownsProfile()) { return true; }
    if (!QDir().mkpath(m_directory)) {
        if (error) { *error = QStringLiteral("The Core profile directory could not be created."); }
        return false;
    }
    if (m_lock->tryLock(qMax(0, timeoutMs))) { return true; }
    if (error) {
        *error = m_lock->error() == QLockFile::LockFailedError
            ? QStringLiteral("Another Core owns this profile.")
            : QStringLiteral("The Core profile could not be locked.");
    }
    return false;
}

bool StationHandover::reclaimFromBackground(int timeoutMs, QString* error)
{
    QElapsedTimer elapsed;
    elapsed.start();
    if (acquire(0, error)) { return true; }
    if (m_lock->error() != QLockFile::LockFailedError) { return false; }

    // This exact path can only name the Core using the selected profile.
    // A status/release command at a packaged Core uses candidatePathsFor;
    // the GUI intentionally does not search those other profiles.
    const StationControlReply reply = StationControlSocket::request(
        controlSocketPath(), {QStringLiteral("release")}, timeoutMs);
    const int left = qMax(0, timeoutMs - int(elapsed.elapsed()));
    if (acquire(left, error)) { return true; }
    if (error && !reply.ok && !reply.text.isEmpty()) { *error = reply.text; }
    return false;
}

void StationHandover::release()
{
    m_lock->unlock();
}

} // namespace NereusSDR
