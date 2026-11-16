// no-port-check: NereusSDR-original process ownership.

#include "core/station/StationHandover.h"

#include "core/AppSettings.h"
#include "core/daemon/StationControlSocket.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QStorageInfo>
#include <QSysInfo>

#include <algorithm>
#include <array>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace NereusSDR {

namespace {

// The lock file's lines, as QLockFile writes them. Checked against QLockFile
// itself by tests/tst_station_foreign_lock.cpp.
constexpr int kLockLinePid = 0;
constexpr int kLockLineHostName = 2;
constexpr int kLockLineMachineId = 3;
constexpr int kLockLinesRequired = 3;
constexpr qint64 kLockFileMaxBytes = 4096;

QByteArray lockFileBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(kLockFileMaxBytes) : QByteArray{};
}

// A program holding a file on a network drive can stop this computer from even
// reading it, which says nothing about whose lock it is. Measured over SMB with
// the holder on another computer: the size still shows and the read comes back
// empty.
bool lockFileIsUnreadable(const QString& path)
{
    return QFileInfo(path).size() > 0 && lockFileBytes(path).isEmpty();
}

QString computerNameForOperator(const QString& hostName)
{
    if (hostName.isEmpty()) {
        return QStringLiteral("another computer");
    }
    // A computer set up from another one's backup keeps its name.
    if (hostName == QSysInfo::machineHostName()) {
        return QStringLiteral("another computer with the same name (%1)").arg(hostName);
    }
    return hostName;
}

// Said after every failed removal, whatever stopped it, so it names each cause
// only as a possibility. Moving the file aside is the way out of all of them.
QString couldNotRemoveText(const QString& computerName, const QString& lockPath)
{
    return QStringLiteral("This profile is marked as in use on %1, and NereusSDR could not "
                          "remove that mark. A program may still be holding it, the mark may "
                          "have just changed, or this folder may not allow the change. If "
                          "NereusSDR is running on that computer, quit it and try again. If it "
                          "is not, move this file aside and start NereusSDR again: %2")
        .arg(computerName, lockPath);
}

QString unreadableLockText(const QString& lockPath)
{
    return QStringLiteral("This profile is marked as in use, and NereusSDR could not read by "
                          "which computer. A program on this computer or on another computer "
                          "may be holding it, and its folder is on a network drive or a kind of "
                          "disk NereusSDR does not recognize. Quit NereusSDR wherever it is "
                          "running, then try again. If it is not running anywhere, move this "
                          "file aside and start NereusSDR again: %1")
        .arg(lockPath);
}

} // namespace

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

void StationHandover::setControlSocketPathForTest(const QString& path)
{
    m_controlSocketPathForTest = path;
}

QString StationHandover::controlSocketPath() const
{
    if (!m_controlSocketPathForTest.isEmpty()) {
        return m_controlSocketPathForTest;
    }
    return QDir(m_directory).filePath(
        QString::fromLatin1(StationControlSocket::kSocketName));
}

bool StationHandover::ownsProfile() const
{
    return m_lock->isLocked();
}

bool StationHandover::lockedByOtherComputer() const
{
    return m_otherComputerState == OtherComputerState::NeedsOperator;
}

QString StationHandover::otherComputerName() const
{
    return m_otherComputerState == OtherComputerState::None
        ? QString{}
        : m_otherComputerLock.computerName;
}

QString StationHandover::takeoverNote() const
{
    return m_takeoverNote;
}

void StationHandover::setFolderIsOnThisComputerForTest(bool onThisComputer)
{
    m_folderIsOnThisComputerForTest = onThisComputer;
}

bool StationHandover::diskTypeIsOnThisComputer(const QByteArray& diskType)
{
    // Network and shared types (smbfs, cifs, smb3, nfs, nfs4, afpfs, webdav,
    // fuse.sshfs, 9p, virtiofs, prl_fs, gfs2, ocfs2) are left off on purpose,
    // and so is anything not listed here. fuseblk is FUSE over a local block
    // device. overlay is a container's own writable layer.
    static constexpr std::array<const char*, 20> kLocalDiskTypes{
        "apfs", "hfs",  "exfat",   "msdos", "vfat",  "ntfs",  "ntfs3",
        "ext2", "ext3", "ext4",    "xfs",   "btrfs", "f2fs",  "zfs",
        "jfs",  "bcachefs", "fuseblk", "tmpfs", "ramfs", "overlay"};
    const QByteArray lowered = diskType.toLower();
    return std::any_of(kLocalDiskTypes.begin(), kLocalDiskTypes.end(),
                       [&](const char* local) { return lowered == local; });
}

bool StationHandover::folderIsOnThisComputer() const
{
    if (m_folderIsOnThisComputerForTest) {
        return *m_folderIsOnThisComputerForTest;
    }
    const QStorageInfo storage(m_directory);
    if (!storage.isValid()) {
        return false;
    }
#ifdef Q_OS_WIN
    // Windows reports a mapped network drive by the type of the disk behind it,
    // so its type name cannot answer this. Ask for the kind of drive instead.
    QString root = QDir::toNativeSeparators(storage.rootPath());
    if (root.isEmpty()) {
        return false;
    }
    if (!root.endsWith(QLatin1Char('\\'))) {
        root += QLatin1Char('\\');
    }
    const UINT driveType = GetDriveTypeW(reinterpret_cast<LPCWSTR>(root.utf16()));
    return driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE
        || driveType == DRIVE_RAMDISK;
#else
    return diskTypeIsOnThisComputer(storage.fileSystemType());
#endif
}

bool StationHandover::lockCameFromOtherComputer(const QByteArray& lockBytes,
                                                const QByteArray& ownMachineId,
                                                const QString& ownHostName)
{
    const QList<QByteArray> lines = lockBytes.split('\n');
    // A file a starting program has not finished writing, or anything else that
    // is not a lock, says nothing about another computer.
    if (lines.size() < kLockLinesRequired) {
        return false;
    }
    bool pidIsNumber = false;
    lines.at(kLockLinePid).toLongLong(&pidIsNumber);
    if (!pidIsNumber) {
        return false;
    }

    // The answer QLockFile reaches, measured on Qt 6.8.2 on Linux and Qt 6.11 on
    // macOS and not on Windows: the machine id decides when the file and this
    // computer both have one, and the host name otherwise. A container often has
    // no machine id at all. With no id, QLockFile on Windows compares COMPUTERNAME
    // and not the name this is given, so the two can differ there.
    const QByteArray machineId = lines.value(kLockLineMachineId);
    if (!machineId.isEmpty() && !ownMachineId.isEmpty()) {
        return machineId != ownMachineId;
    }
    const QString hostName = QString::fromUtf8(lines.at(kLockLineHostName));
    return !hostName.isEmpty() && hostName != ownHostName;
}

std::optional<StationHandover::OtherComputerLock> StationHandover::readOtherComputerLock() const
{
    const QByteArray bytes = lockFileBytes(lockPath());
    if (!lockCameFromOtherComputer(bytes, QSysInfo::machineUniqueId(),
                                   QSysInfo::machineHostName())) {
        return std::nullopt;
    }
    const QString hostName = QString::fromUtf8(bytes.split('\n').at(kLockLineHostName));
    return OtherComputerLock{bytes, computerNameForOperator(hostName)};
}

bool StationHandover::removeLockFromOtherComputer(const QByteArray& seenBytes)
{
    // The same guard file QLockFile takes when it clears a stale lock itself, so
    // two programs starting together cannot both remove and then both lock.
    QLockFile guard(lockPath() + QStringLiteral(".rmlock"));
    if (!guard.tryLock(0)) {
        return false;
    }
    // Only ever the lock that was looked at. A program that took the profile a
    // moment ago wrote its own, and that one is not a leftover.
    if (lockFileBytes(lockPath()) != seenBytes) {
        return false;
    }
    // Refused while a program still holds the file: QLockFile keeps an operating
    // system lock on it for the owner's whole life, and removal needs that lock.
    return m_lock->removeStaleLockFile();
}

bool StationHandover::acquire(int timeoutMs, QString* error)
{
    if (error) { error->clear(); }
    if (ownsProfile()) { return true; }
    m_otherComputerState = OtherComputerState::None;
    m_otherComputerLock = {};
    if (!QDir().mkpath(m_directory)) {
        if (error) { *error = QStringLiteral("The Core profile directory could not be created."); }
        return false;
    }
    if (m_lock->tryLock(0)) { return true; }

    if (m_lock->error() == QLockFile::LockFailedError) {
        if (const auto other = readOtherComputerLock()) {
            m_otherComputerLock = *other;
            if (!folderIsOnThisComputer()) {
                m_otherComputerState = OtherComputerState::NeedsOperator;
            } else if (removeLockFromOtherComputer(other->bytes) && m_lock->tryLock(0)) {
                m_otherComputerLock = {};
                m_takeoverNote = QStringLiteral(
                    "This profile was marked as in use on %1. Its folder is on a disk on this "
                    "computer, so NereusSDR took it over. If that computer can still reach this "
                    "folder through a network share or a sync service, quit NereusSDR there: two "
                    "computers using one profile can overwrite each other's settings.")
                                     .arg(other->computerName);
                return true;
            } else {
                m_otherComputerState = OtherComputerState::CouldNotRemove;
            }
        } else if (lockFileIsUnreadable(lockPath()) && !folderIsOnThisComputer()) {
            m_otherComputerState = OtherComputerState::UnreadableLock;
        }
    }

    // A Core does not wait on another computer's lock on a network drive. That
    // lock can clear if the other computer's Core is quitting right now, but
    // usually it is a leftover that only a person can settle, so this says what
    // to do right away and a service restart looks again. Other locks keep the
    // wait, because their holder may leave.
    if (m_otherComputerState != OtherComputerState::NeedsOperator && timeoutMs > 0
        && m_lock->tryLock(timeoutMs)) {
        m_otherComputerState = OtherComputerState::None;
        m_otherComputerLock = {};
        return true;
    }
    if (error) {
        if (m_otherComputerState == OtherComputerState::NeedsOperator) {
            *error = QStringLiteral(
                "This profile is marked as in use on %1. Its folder is on a network drive or a "
                "kind of disk NereusSDR does not recognize, so NereusSDR cannot tell whether "
                "that computer is still using it, and it was left alone. If NereusSDR is running "
                "there, stop it. If it is not, move this file aside and start NereusSDR again: %2")
                         .arg(m_otherComputerLock.computerName, lockPath());
        } else if (m_otherComputerState == OtherComputerState::CouldNotRemove) {
            *error = couldNotRemoveText(m_otherComputerLock.computerName, lockPath());
        } else if (m_otherComputerState == OtherComputerState::UnreadableLock) {
            *error = unreadableLockText(lockPath());
        } else {
            *error = m_lock->error() == QLockFile::LockFailedError
                ? QStringLiteral("Another Core owns this profile.")
                : QStringLiteral("The Core profile could not be locked.");
        }
    }
    return false;
}

bool StationHandover::takeOverFromOtherComputer(QString* error)
{
    if (error) { error->clear(); }
    if (ownsProfile()) { return true; }
    if (m_otherComputerState != OtherComputerState::NeedsOperator) {
        return acquire(0, error);
    }
    const OtherComputerLock seen = m_otherComputerLock;
    if (removeLockFromOtherComputer(seen.bytes) && m_lock->tryLock(0)) {
        m_otherComputerState = OtherComputerState::None;
        m_otherComputerLock = {};
        m_takeoverNote = QStringLiteral(
            "This profile was marked as in use on %1. The operator chose to take it over.")
                             .arg(seen.computerName);
        return true;
    }
    m_otherComputerState = OtherComputerState::CouldNotRemove;
    if (error) { *error = couldNotRemoveText(seen.computerName, lockPath()); }
    return false;
}

bool StationHandover::reclaimFromBackground(int timeoutMs, QString* error)
{
    QElapsedTimer elapsed;
    elapsed.start();
    if (acquire(0, error)) { return true; }
    if (m_lock->error() != QLockFile::LockFailedError) { return false; }
    // No Core here can answer for a lock naming another computer, so a window
    // tells the operator right away and leaves any retry to them. An unreadable
    // lock could be a Core on this computer, so that one still asks.
    const auto lockIsAnotherComputers = [this] {
        return m_otherComputerState == OtherComputerState::NeedsOperator
            || m_otherComputerState == OtherComputerState::CouldNotRemove;
    };
    if (lockIsAnotherComputers()) { return false; }

    // This exact path can only name the Core using the selected profile.
    // A status/release command at a packaged Core uses candidatePathsFor;
    // the GUI intentionally does not search those other profiles.
    const StationControlReply reply = StationControlSocket::request(
        controlSocketPath(), {QStringLiteral("release")}, timeoutMs);
    // Nothing here answered, which is not proof the holder is elsewhere: on Mac
    // and Linux a Core may be unable to make its socket on this drive. The operator
    // hears the words from acquire() right away, as for a lock naming another
    // computer.
    if (m_otherComputerState == OtherComputerState::UnreadableLock && !reply.answered) {
        return false;
    }
    const int left = qMax(0, timeoutMs - int(elapsed.elapsed()));
    if (acquire(left, error)) { return true; }
    if (lockIsAnotherComputers()) { return false; }
    if (error && !reply.ok && !reply.text.isEmpty()) { *error = reply.text; }
    return false;
}

void StationHandover::release()
{
    m_lock->unlock();
}

} // namespace NereusSDR
