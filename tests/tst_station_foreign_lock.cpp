// no-port-check: NereusSDR-original station ownership tests.
//
// A station.lock written on another computer: Qt refuses it forever, so these
// tests write the same lines QLockFile writes into a temporary test profile and
// check what StationHandover does with them.

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QProcess>
#include <QScopeGuard>
#include <QSysInfo>
#include <QUuid>

#include <cstdio>

#include "core/AppSettings.h"
#include "core/station/StationHandover.h"

using namespace NereusSDR;

namespace {

const QByteArray kOtherMachineId("017FA02B-0000-5000-A000-000000000000");
const QByteArray kOtherHostName("Old-MacBook.local");

QString profileName()
{
    return QStringLiteral("f%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(6));
}

// The pid of a process that has really gone, so it cannot be a guess that
// happens to name something running.
qint64 deadPid()
{
    QProcess process;
    process.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--exit-helper")});
    if (!process.waitForStarted()) { return 0; }
    const qint64 pid = process.processId();
    return process.waitForFinished() ? pid : 0;
}

QByteArray lockLines(qint64 pid, const QByteArray& hostName, const QByteArray& machineId)
{
    QByteArray lines = QByteArray::number(pid) + "\nNereusSDR\n" + hostName + '\n';
    if (!machineId.isEmpty()) { lines += machineId + '\n'; }
    return lines;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool waitForReady(QProcess& process)
{
    QByteArray text;
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 15000) {
        text += process.readAll();
        if (text.contains("READY")) { return true; }
        process.waitForReadyRead(100);
    }
    return false;
}

} // namespace

class TestStationForeignLock : public QObject {
    Q_OBJECT

    QString m_profile;

    QString prepareProfile() const
    {
        const QString directory = AppSettings::resolveConfigDir(m_profile);
        return QDir().mkpath(directory) ? directory : QString{};
    }

private slots:
    void init() { m_profile = profileName(); }

    void cleanup() { QDir(AppSettings::resolveConfigDir(m_profile)).removeRecursively(); }

    // Pins the two facts this fix stands on, so a Qt upgrade that changes either
    // one fails here and not on an operator's computer.
    void qtStillRefusesAnotherComputersLock()
    {
        const QString directory = prepareProfile();
        QVERIFY(!directory.isEmpty());
        const QString path = QDir(directory).filePath(QStringLiteral("station.lock"));
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);

        QVERIFY(writeFile(path, lockLines(pid, kOtherHostName, kOtherMachineId)));
        QLockFile foreign(path);
        foreign.setStaleLockTime(0);
        QVERIFY(!foreign.tryLock(0));
        QCOMPARE(foreign.error(), QLockFile::LockFailedError);

        // The scratch copy that proved the cause: the same lines with this
        // computer's id are taken at once.
        QVERIFY(writeFile(path, lockLines(pid, kOtherHostName, QSysInfo::machineUniqueId())));
        QLockFile local(path);
        local.setStaleLockTime(0);
        QVERIFY(local.tryLock(0));
    }

    void takenOnThisComputerWithoutWaiting_data()
    {
        QTest::addColumn<QByteArray>("hostName");
        QTest::addColumn<QByteArray>("machineId");
        QTest::addColumn<QString>("namedAs");
        QTest::newRow("other id and name") << kOtherHostName << kOtherMachineId
                                           << QString::fromUtf8(kOtherHostName);
        // What a computer set up from another one's backup looks like.
        QTest::newRow("other id, same name")
            << QSysInfo::machineHostName().toUtf8() << kOtherMachineId
            << QStringLiteral("another computer with the same name");
        QTest::newRow("no id, other name") << kOtherHostName << QByteArray{}
                                           << QString::fromUtf8(kOtherHostName);
    }

    void takenOnThisComputerWithoutWaiting()
    {
        QFETCH(QByteArray, hostName);
        QFETCH(QByteArray, machineId);
        QFETCH(QString, namedAs);
        QVERIFY(!prepareProfile().isEmpty());
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(true);
        QVERIFY(writeFile(ownership.lockPath(), lockLines(pid, hostName, machineId)));

        QElapsedTimer elapsed;
        elapsed.start();
        QString error;
        QVERIFY2(ownership.acquire(15000, &error), qPrintable(error));
        QVERIFY(elapsed.elapsed() < 5000);
        QVERIFY(ownership.ownsProfile());
        QVERIFY(!ownership.lockedByOtherComputer());
        QVERIFY2(ownership.takeoverNote().contains(namedAs),
                 qPrintable(ownership.takeoverNote()));

        // It is ours now, with this computer's id, and goes away cleanly.
        QVERIFY(readFile(ownership.lockPath()).contains(QSysInfo::machineUniqueId()));
        ownership.release();
        QVERIFY(!QFile::exists(ownership.lockPath()));
        QVERIFY(!QFile::exists(ownership.lockPath() + QStringLiteral(".rmlock")));
    }

    // The temporary profile is really on this computer's disk, so this one goes
    // through the real disk check and not the test's answer.
    void realDiskCheckTakesItHere()
    {
        QVERIFY(!prepareProfile().isEmpty());
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        StationHandover ownership(m_profile);
        QVERIFY(writeFile(ownership.lockPath(), lockLines(pid, kOtherHostName, kOtherMachineId)));
        QString error;
        QVERIFY2(ownership.acquire(0, &error), qPrintable(error));
        QVERIFY(!ownership.takeoverNote().isEmpty());
        ownership.release();
    }

    void leftAloneOnANetworkDrive()
    {
        QVERIFY(!prepareProfile().isEmpty());
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        const QByteArray lines = lockLines(pid, kOtherHostName, kOtherMachineId);
        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(false);
        QVERIFY(writeFile(ownership.lockPath(), lines));

        // What a Core does: stays stopped, and says what to do.
        QString error;
        QVERIFY(!ownership.acquire(0, &error));
        QVERIFY(!ownership.ownsProfile());
        QVERIFY(ownership.lockedByOtherComputer());
        QCOMPARE(ownership.otherComputerName(), QString::fromUtf8(kOtherHostName));
        QVERIFY2(error.contains(QString::fromUtf8(kOtherHostName)), qPrintable(error));
        QVERIFY2(error.contains(ownership.lockPath()), qPrintable(error));
        QVERIFY(ownership.takeoverNote().isEmpty());
        QCOMPARE(readFile(ownership.lockPath()), lines);

        // What a window does: no socket, no 15-second wait, no word of a Core.
        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(!ownership.reclaimFromBackground(15000, &error));
        QVERIFY(elapsed.elapsed() < 5000);
        QVERIFY(ownership.lockedByOtherComputer());
        QVERIFY2(!error.contains(QStringLiteral("No Core answered")), qPrintable(error));
        QCOMPARE(readFile(ownership.lockPath()), lines);

        // The operator's Take Over.
        QVERIFY2(ownership.takeOverFromOtherComputer(&error), qPrintable(error));
        QVERIFY(ownership.ownsProfile());
        QVERIFY(!ownership.lockedByOtherComputer());
        QVERIFY(ownership.otherComputerName().isEmpty());
        QVERIFY2(ownership.takeoverNote().contains(QString::fromUtf8(kOtherHostName)),
                 qPrintable(ownership.takeoverNote()));
        ownership.release();
    }

    void takeOverRefusesALockThatChangedAfterTheOperatorSawIt()
    {
        QVERIFY(!prepareProfile().isEmpty());
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(false);
        QVERIFY(writeFile(ownership.lockPath(), lockLines(pid, kOtherHostName, kOtherMachineId)));
        QVERIFY(!ownership.acquire(0));
        QVERIFY(ownership.lockedByOtherComputer());

        const QByteArray newer = lockLines(pid, QByteArrayLiteral("Third-Computer"),
                                           QByteArrayLiteral("9999"));
        QVERIFY(writeFile(ownership.lockPath(), newer));
        QString error;
        QVERIFY(!ownership.takeOverFromOtherComputer(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!ownership.ownsProfile());
        QCOMPARE(readFile(ownership.lockPath()), newer);

        // The next look finds the newer computer and asks about that one.
        QVERIFY(!ownership.acquire(0));
        QCOMPARE(ownership.otherComputerName(), QStringLiteral("Third-Computer"));
    }

    void deadOwnerOnThisComputerIsNotAnotherComputer()
    {
        QVERIFY(!prepareProfile().isEmpty());
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        StationHandover ownership(m_profile);
        // Qt clears this one itself, wherever the folder is.
        ownership.setFolderIsOnThisComputerForTest(false);
        QVERIFY(writeFile(ownership.lockPath(),
                          lockLines(pid, QSysInfo::machineHostName().toUtf8(),
                                    QSysInfo::machineUniqueId())));
        QString error;
        QVERIFY2(ownership.acquire(0, &error), qPrintable(error));
        QVERIFY(!ownership.lockedByOtherComputer());
        QVERIFY(ownership.takeoverNote().isEmpty());
        ownership.release();
    }

    void liveOwnerOnThisComputerIsNeverTaken()
    {
        QProcess holder;
        holder.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--lock-helper"), m_profile});
        const auto stop = qScopeGuard([&] {
            holder.kill();
            holder.waitForFinished();
        });
        QVERIFY(holder.waitForStarted());
        QVERIFY(waitForReady(holder));

        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(true);
        QString error;
        QVERIFY(!ownership.acquire(0, &error));
        QVERIFY(!ownership.ownsProfile());
        QVERIFY(!ownership.lockedByOtherComputer());
        QVERIFY(ownership.otherComputerName().isEmpty());
        QCOMPARE(error, QStringLiteral("Another Core owns this profile."));
        QVERIFY(QFile::exists(ownership.lockPath()));
    }

#ifndef Q_OS_WIN
    // A program that really holds the file blocks the takeover even when the
    // file says another computer, the way a container with its own machine id
    // sharing this disk would look. Windows opens a live lock with no sharing,
    // so nothing can rewrite it there to set this up.
    void programStillHoldingTheFileBlocksTheTakeover()
    {
        QProcess holder;
        holder.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--lock-helper"), m_profile});
        const auto stop = qScopeGuard([&] {
            holder.kill();
            holder.waitForFinished();
        });
        QVERIFY(holder.waitForStarted());
        QVERIFY(waitForReady(holder));

        const QByteArray lines = lockLines(holder.processId(), kOtherHostName, kOtherMachineId);
        StationHandover onThisComputer(m_profile);
        onThisComputer.setFolderIsOnThisComputerForTest(true);
        QVERIFY(writeFile(onThisComputer.lockPath(), lines));

        QString error;
        QVERIFY(!onThisComputer.acquire(0, &error));
        QVERIFY(!onThisComputer.ownsProfile());
        QVERIFY(!onThisComputer.lockedByOtherComputer());
        QVERIFY2(error.contains(QString::fromUtf8(kOtherHostName)), qPrintable(error));
        QVERIFY2(error.contains(QStringLiteral("still holding")), qPrintable(error));
        QCOMPARE(readFile(onThisComputer.lockPath()), lines);

        // A window asks no Core and offers no takeover it cannot deliver.
        QVERIFY(!onThisComputer.reclaimFromBackground(15000, &error));
        QVERIFY2(!error.contains(QStringLiteral("No Core answered")), qPrintable(error));

        // The operator's Take Over meets the same block.
        StationHandover onNetworkDrive(m_profile);
        onNetworkDrive.setFolderIsOnThisComputerForTest(false);
        QVERIFY(!onNetworkDrive.acquire(0));
        QVERIFY(onNetworkDrive.lockedByOtherComputer());
        QVERIFY(!onNetworkDrive.takeOverFromOtherComputer(&error));
        QVERIFY(!onNetworkDrive.ownsProfile());
        QVERIFY2(error.contains(QStringLiteral("still holding")), qPrintable(error));
        QCOMPARE(readFile(onNetworkDrive.lockPath()), lines);
    }

    // Over SMB, a program on another computer holding the lock stops this one
    // reading it at all. No read permission makes the file look the same here.
    void unreadableLockOnANetworkDriveIsHeldElsewhere()
    {
        QVERIFY(!prepareProfile().isEmpty());
        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(false);
        const QByteArray lines = lockLines(deadPid(), kOtherHostName, kOtherMachineId);
        QVERIFY(writeFile(ownership.lockPath(), lines));
        QVERIFY(QFile::setPermissions(ownership.lockPath(), QFileDevice::Permissions{}));
        const auto restore = qScopeGuard([&] {
            QFile::setPermissions(ownership.lockPath(),
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        });
        if (!readFile(ownership.lockPath()).isEmpty()) {
            QSKIP("An account that can read anything cannot set this up.");
        }

        QString error;
        QVERIFY(!ownership.acquire(0, &error));
        QVERIFY(!ownership.lockedByOtherComputer());
        QVERIFY2(error.contains(QStringLiteral("another computer")), qPrintable(error));

        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(!ownership.reclaimFromBackground(15000, &error));
        QVERIFY(elapsed.elapsed() < 5000);
        QVERIFY2(!error.contains(QStringLiteral("No Core answered")), qPrintable(error));

        // On this computer's disk an unreadable lock is an ordinary owner.
        StationHandover onThisComputer(m_profile);
        onThisComputer.setFolderIsOnThisComputerForTest(true);
        QVERIFY(!onThisComputer.acquire(0, &error));
        QCOMPARE(error, QStringLiteral("Another Core owns this profile."));
    }
#endif

    void onlyKnownLocalDiskTypesCount_data()
    {
        QTest::addColumn<QByteArray>("diskType");
        QTest::addColumn<bool>("onThisComputer");
        for (const char* local : {"apfs", "hfs", "exfat", "msdos", "ntfs", "ext4", "xfs",
                                  "btrfs", "f2fs", "vfat", "tmpfs", "overlay", "APFS"}) {
            QTest::newRow(local) << QByteArray(local) << true;
        }
        for (const char* shared : {"smbfs", "cifs", "smb3", "nfs", "nfs4", "afpfs", "webdav",
                                   "fuse.sshfs", "9p", "virtiofs", "prl_fs", "gfs2", "ocfs2",
                                   "something-new", ""}) {
            QTest::newRow(shared[0] != '\0' ? shared : "empty") << QByteArray(shared) << false;
        }
    }

    void onlyKnownLocalDiskTypesCount()
    {
        QFETCH(QByteArray, diskType);
        QFETCH(bool, onThisComputer);
        QCOMPARE(StationHandover::diskTypeIsOnThisComputer(diskType), onThisComputer);
    }
};

int main(int argc, char* argv[])
{
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--exit-helper")) {
        return 0;
    }
    if (argc > 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--lock-helper")) {
        QCoreApplication app(argc, argv);
        StationHandover lock(QString::fromLocal8Bit(argv[2]));
        if (!lock.acquire(0)) { return 3; }
        std::fputs("READY\n", stdout);
        std::fflush(stdout);
        return app.exec();
    }
    QCoreApplication app(argc, argv);
    TestStationForeignLock tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_station_foreign_lock.moc"
