#pragma once
// no-port-check: NereusSDR-original process ownership.

#include <QByteArray>
#include <QString>

#include <memory>
#include <optional>

class QLockFile;

namespace NereusSDR {

// Own one settings profile from before CoreInit until every writer has stopped.
// The lock is deliberately independent of StationHost: remote-only windows
// still load and write the same profile.
//
// Qt never treats a lock written on another computer as stale, even when the
// program that wrote it is long gone, because it cannot look up a process on
// that computer. So a profile folder that moved to a new computer after a crash
// would stay locked forever. The rule here, decided by where the folder is:
//
//  - On a disk in or plugged into this computer, acquire() takes the lock and
//    takeoverNote() says so.
//  - On a network drive or a disk type not on the list, the other computer
//    could really still be using it, so nothing is taken without a person.
//    lockedByOtherComputer() turns true and takeOverFromOtherComputer() is the
//    person's answer.
//
// Either way a program still holding the file blocks the takeover. Across a
// network drive that block depends on the drive passing file locks along. It was
// measured working over SMB and not measured on anything else.
//
// The limit of the first case: a folder on this computer's disk can still be in
// use elsewhere. This computer may share it over NFS or SMB with another computer
// running NereusSDR from the share, or a sync service may keep a copy on another
// computer. Nothing here can see either one, and there is deliberately no
// heartbeat or second lock to detect them, so the note tells the operator instead
// of claiming that nothing else could be using the folder.
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
    // A local socket's path has a short limit that a test profile can pass.
    void setControlSocketPathForTest(const QString& path);

    // True after a failed attempt when the lock came from another computer and
    // this folder might be reachable from that computer too.
    bool lockedByOtherComputer() const;
    // The other computer, in words for the operator. Empty unless the last
    // attempt found its lock.
    QString otherComputerName() const;
    // Removes the lock the last attempt saw and takes the profile. Refuses if
    // the file changed since then, a program still holds it, or the folder does
    // not allow it.
    bool takeOverFromOtherComputer(QString* error = nullptr);
    // One sentence for the log when this process took another computer's lock.
    // The log opens only after the profile is owned, so the caller writes it.
    QString takeoverNote() const;

    // Whether a disk of this type is in or plugged into this computer. A list
    // of known local types, so anything unrecognized gets the careful path.
    static bool diskTypeIsOnThisComputer(const QByteArray& diskType);

    // Whether a lock file's contents were written on another computer, by the
    // same test QLockFile makes before it refuses one.
    static bool lockCameFromOtherComputer(const QByteArray& lockBytes,
                                          const QByteArray& ownMachineId,
                                          const QString& ownHostName);

    void setFolderIsOnThisComputerForTest(bool onThisComputer);

private:
    struct OtherComputerLock {
        QByteArray bytes;
        QString computerName;
    };

    // CouldNotRemove: a lock naming another computer that was not removed, for
    // any reason. UnreadableLock: a lock on a network drive that could not be
    // read, so it could be this computer's or another's.
    enum class OtherComputerState { None, NeedsOperator, CouldNotRemove, UnreadableLock };

    bool folderIsOnThisComputer() const;
    std::optional<OtherComputerLock> readOtherComputerLock() const;
    bool removeLockFromOtherComputer(const QByteArray& seenBytes);

    QString m_directory;
    QString m_controlSocketPathForTest;
    std::unique_ptr<QLockFile> m_lock;
    std::optional<bool> m_folderIsOnThisComputerForTest;
    OtherComputerState m_otherComputerState{OtherComputerState::None};
    OtherComputerLock m_otherComputerLock;
    QString m_takeoverNote;
};

} // namespace NereusSDR
