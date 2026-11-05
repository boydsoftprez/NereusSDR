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
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

#include <cstdio>

#include "core/AppSettings.h"
#include "core/daemon/StationControlSocket.h"
#include "core/station/StationHandover.h"

using namespace NereusSDR;

namespace {

const QByteArray kOtherMachineId("017FA02B-0000-5000-A000-000000000000");
const QByteArray kOtherHostName("Old-MacBook.local");
const QString kCoreRefusal = QStringLiteral("The Core is already handing back the radio.");

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

// The words after any failed removal: what happened, the way out, and no claim
// about why it failed.
bool saysCouldNotRemove(const QString& error, const QString& lockPath)
{
    return error.contains(QStringLiteral("could not remove"))
        && error.contains(QStringLiteral("may still be holding"))
        && error.contains(QStringLiteral("move this file aside"))
        && error.contains(lockPath)
        && !error.contains(QStringLiteral("is still holding"));
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

    // What Qt itself does with each kind of lock on whatever computer this runs
    // on, against the rule StationHandover uses. A Qt upgrade that changes it
    // fails here and not on an operator's computer. Containers usually have no
    // machine id, so this also runs the id-less half of the table on CI.
    void ruleMatchesQtHere_data()
    {
        const QByteArray ownHost = QSysInfo::machineHostName().toUtf8();
        const QByteArray ownId = QSysInfo::machineUniqueId();
        QTest::addColumn<QByteArray>("hostName");
        QTest::addColumn<QByteArray>("machineId");
        QTest::newRow("this name, this id") << ownHost << ownId;
        QTest::newRow("other name, this id") << kOtherHostName << ownId;
        QTest::newRow("this name, other id") << ownHost << kOtherMachineId;
        QTest::newRow("other name, other id") << kOtherHostName << kOtherMachineId;
        QTest::newRow("other name, no id") << kOtherHostName << QByteArray{};
        QTest::newRow("this name, no id") << ownHost << QByteArray{};
    }

    void ruleMatchesQtHere()
    {
        QFETCH(QByteArray, hostName);
        QFETCH(QByteArray, machineId);
        const QString directory = prepareProfile();
        QVERIFY(!directory.isEmpty());
        const QString path = QDir(directory).filePath(QStringLiteral("station.lock"));
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        const QByteArray lines = lockLines(pid, hostName, machineId);
        QVERIFY(writeFile(path, lines));

        QLockFile lock(path);
        lock.setStaleLockTime(0);
        const bool qtTookIt = lock.tryLock(0);
        QCOMPARE(StationHandover::lockCameFromOtherComputer(lines, QSysInfo::machineUniqueId(),
                                                            QSysInfo::machineHostName()),
                 !qtTookIt);
    }

    // The same table with both kinds of computer, wherever this runs. Each row
    // was measured against real QLockFile on Qt 6.8.2 on Linux, with the machine
    // id visible and hidden, using a dead pid.
    void ruleTable_data()
    {
        const QByteArray own("OWN-ID");
        const QByteArray other("OTHER-ID");
        QTest::addColumn<QByteArray>("hostName");
        QTest::addColumn<QByteArray>("machineId");
        QTest::addColumn<QByteArray>("ownMachineId");
        QTest::addColumn<bool>("fromOtherComputer");
        QTest::newRow("has id: this name, this id") << QByteArray("me") << own << own << false;
        QTest::newRow("has id: other name, this id") << QByteArray("them") << own << own << false;
        QTest::newRow("has id: this name, other id") << QByteArray("me") << other << own << true;
        QTest::newRow("has id: other name, other id") << QByteArray("them") << other << own << true;
        QTest::newRow("has id: other name, no id") << QByteArray("them") << QByteArray{} << own << true;
        QTest::newRow("has id: this name, no id") << QByteArray("me") << QByteArray{} << own << false;
        QTest::newRow("no id: this name, an id") << QByteArray("me") << own << QByteArray{} << false;
        QTest::newRow("no id: other name, an id") << QByteArray("them") << own << QByteArray{} << true;
        QTest::newRow("no id: other name, no id") << QByteArray("them") << QByteArray{} << QByteArray{} << true;
        QTest::newRow("no id: this name, no id") << QByteArray("me") << QByteArray{} << QByteArray{} << false;
    }

    void ruleTable()
    {
        QFETCH(QByteArray, hostName);
        QFETCH(QByteArray, machineId);
        QFETCH(QByteArray, ownMachineId);
        QFETCH(bool, fromOtherComputer);
        QCOMPARE(StationHandover::lockCameFromOtherComputer(lockLines(4242, hostName, machineId),
                                                            ownMachineId, QStringLiteral("me")),
                 fromOtherComputer);
    }

    void notALockSaysNothingAboutAnotherComputer()
    {
        const QByteArray own("OWN-ID");
        const QString me = QStringLiteral("me");
        QVERIFY(!StationHandover::lockCameFromOtherComputer(QByteArray{}, own, me));
        QVERIFY(!StationHandover::lockCameFromOtherComputer("4242\n", own, me));
        QVERIFY(!StationHandover::lockCameFromOtherComputer("4242\nNereusSDR\n", own, me));
        QVERIFY(!StationHandover::lockCameFromOtherComputer("not a pid\nx\nthem\nOTHER\n", own, me));
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
        // Without a machine id here, Qt goes by the name and takes this one
        // itself, so nothing was taken over from anyone. See ruleTable.
        if (hostName == QSysInfo::machineHostName().toUtf8()
            && QSysInfo::machineUniqueId().isEmpty()) {
            namedAs.clear();
        }
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
        if (namedAs.isEmpty()) {
            QVERIFY(ownership.takeoverNote().isEmpty());
        } else {
            QVERIFY2(ownership.takeoverNote().contains(namedAs),
                     qPrintable(ownership.takeoverNote()));
        }

        // It is ours now: this process, this computer's name and id. An empty id
        // would match anything, so each line is compared on its own. QLockFile on
        // Windows shares its open file for reading, so this reads back there too.
        const QList<QByteArray> ownLock = readFile(ownership.lockPath()).split('\n');
        QCOMPARE(ownLock.value(0).toLongLong(), QCoreApplication::applicationPid());
        QCOMPARE(QString::fromUtf8(ownLock.value(2)), QSysInfo::machineHostName());
        QCOMPARE(ownLock.value(3), QSysInfo::machineUniqueId());

        // And it goes away cleanly.
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

    // A folder on this disk can still be shared or synced to the other computer,
    // and nothing here can see that, so the note must not promise otherwise.
    void noteOnThisComputerStatesItsLimit()
    {
        QVERIFY(!prepareProfile().isEmpty());
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(true);
        QVERIFY(writeFile(ownership.lockPath(), lockLines(pid, kOtherHostName, kOtherMachineId)));
        QString error;
        QVERIFY2(ownership.acquire(0, &error), qPrintable(error));
        const QString note = ownership.takeoverNote();
        QVERIFY2(note.contains(QStringLiteral("took it over")), qPrintable(note));
        QVERIFY2(note.contains(QStringLiteral("network share")), qPrintable(note));
        QVERIFY2(note.contains(QStringLiteral("sync service")), qPrintable(note));
        QVERIFY2(!note.contains(QStringLiteral("nothing else")), qPrintable(note));
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

        // Only a person can settle this, so a Core does not spend its wait on it.
        QElapsedTimer coreWait;
        coreWait.start();
        QVERIFY(!ownership.acquire(15000, &error));
        QVERIFY(coreWait.elapsed() < 5000);
        QVERIFY(ownership.lockedByOtherComputer());
        QVERIFY2(error.contains(ownership.lockPath()), qPrintable(error));

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
        QVERIFY2(saysCouldNotRemove(error, ownership.lockPath()), qPrintable(error));
        QVERIFY(!ownership.ownsProfile());
        QCOMPARE(readFile(ownership.lockPath()), newer);

        // The next look finds the newer computer and asks about that one.
        QVERIFY(!ownership.acquire(0));
        QCOMPARE(ownership.otherComputerName(), QStringLiteral("Third-Computer"));
    }

    // Another program is clearing the lock at this moment. Nothing is known to
    // be holding the profile, so the words must not say so.
    void busyRemovalIsNotCalledAProgramHoldingTheProfile()
    {
        QVERIFY(!prepareProfile().isEmpty());
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        const QByteArray lines = lockLines(pid, kOtherHostName, kOtherMachineId);
        StationHandover onThisComputer(m_profile);
        onThisComputer.setFolderIsOnThisComputerForTest(true);
        QVERIFY(writeFile(onThisComputer.lockPath(), lines));
        QLockFile removing(onThisComputer.lockPath() + QStringLiteral(".rmlock"));
        QVERIFY(removing.tryLock(0));

        QString error;
        QVERIFY(!onThisComputer.acquire(0, &error));
        QVERIFY2(saysCouldNotRemove(error, onThisComputer.lockPath()), qPrintable(error));
        QVERIFY2(error.contains(QString::fromUtf8(kOtherHostName)), qPrintable(error));
        QCOMPARE(readFile(onThisComputer.lockPath()), lines);

        StationHandover onNetworkDrive(m_profile);
        onNetworkDrive.setFolderIsOnThisComputerForTest(false);
        QVERIFY(!onNetworkDrive.acquire(0));
        QVERIFY(onNetworkDrive.lockedByOtherComputer());
        QVERIFY(!onNetworkDrive.takeOverFromOtherComputer(&error));
        QVERIFY2(saysCouldNotRemove(error, onNetworkDrive.lockPath()), qPrintable(error));
        QCOMPARE(readFile(onNetworkDrive.lockPath()), lines);
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
    // sharing this disk would look. Windows opens a live lock sharing only reads,
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
        QVERIFY2(saysCouldNotRemove(error, onThisComputer.lockPath()), qPrintable(error));
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
        QVERIFY2(saysCouldNotRemove(error, onNetworkDrive.lockPath()), qPrintable(error));
        QCOMPARE(readFile(onNetworkDrive.lockPath()), lines);
    }

    // A wait is for a holder that can still leave, whatever the file says about
    // where it came from. The helper exits on its own a moment after it is ready.
    void waitStaysForAHolderThatCanLeave_data()
    {
        QTest::addColumn<bool>("namesAnotherComputer");
        QTest::newRow("this computer's lock") << false;
        QTest::newRow("names another computer") << true;
    }

    void waitStaysForAHolderThatCanLeave()
    {
        QFETCH(bool, namesAnotherComputer);
        QProcess holder;
        holder.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--lock-helper"), m_profile, QStringLiteral("1500")});
        const auto stop = qScopeGuard([&] {
            holder.kill();
            holder.waitForFinished();
        });
        QVERIFY(holder.waitForStarted());
        QVERIFY(waitForReady(holder));

        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(true);
        if (namesAnotherComputer) {
            QVERIFY(writeFile(ownership.lockPath(),
                              lockLines(holder.processId(), kOtherHostName, kOtherMachineId)));
        }
        // Real time on purpose: the helper's exit is what ends this wait, and the
        // bound is the same one a Core uses.
        QString error;
        QVERIFY2(ownership.acquire(15000, &error), qPrintable(error));
        ownership.release();
    }

    // No write permission on the lock: removal fails with nothing holding it.
    void readOnlyLockIsNotCalledAProgramHoldingTheProfile()
    {
        QVERIFY(!prepareProfile().isEmpty());
        const qint64 pid = deadPid();
        QVERIFY(pid != 0);
        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(true);
        const QByteArray lines = lockLines(pid, kOtherHostName, kOtherMachineId);
        QVERIFY(writeFile(ownership.lockPath(), lines));
        QVERIFY(QFile::setPermissions(ownership.lockPath(), QFileDevice::ReadOwner));
        if (QFile(ownership.lockPath()).open(QIODevice::WriteOnly | QIODevice::Append)) {
            QSKIP("An account that can write anything cannot set this up.");
        }

        QString error;
        QVERIFY(!ownership.acquire(0, &error));
        QVERIFY2(saysCouldNotRemove(error, ownership.lockPath()), qPrintable(error));
        QCOMPARE(readFile(ownership.lockPath()), lines);
    }

    // Over SMB, a program on another computer holding the lock stops this one
    // reading it at all. No read permission makes the file look the same here.
    // Nothing shows whose lock it is, so the words name both computers.
    void unreadableLockOnANetworkDriveCouldBeEitherComputers()
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
        QVERIFY(ownership.otherComputerName().isEmpty());
        QVERIFY2(error.contains(QStringLiteral("this computer or on another computer")),
                 qPrintable(error));
        QVERIFY2(error.contains(ownership.lockPath()), qPrintable(error));
        QVERIFY2(error.contains(QStringLiteral("move this file aside")), qPrintable(error));

        // No Core answers here, so a window hears the same words right away.
        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(!ownership.reclaimFromBackground(15000, &error));
        QVERIFY(elapsed.elapsed() < 5000);
        QVERIFY2(!error.contains(QStringLiteral("No Core answered")), qPrintable(error));
        QVERIFY2(error.contains(QStringLiteral("this computer or on another computer")),
                 qPrintable(error));

        // On this computer's disk an unreadable lock is an ordinary owner.
        StationHandover onThisComputer(m_profile);
        onThisComputer.setFolderIsOnThisComputerForTest(true);
        QVERIFY(!onThisComputer.acquire(0, &error));
        QCOMPARE(error, QStringLiteral("Another Core owns this profile."));
    }

    // A Core on this computer whose lock cannot be read gets asked to release,
    // and its answer decides. It really listens at the socket a window asks.
    void coreHereAnswersForAnUnreadableLock_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<int>("timeoutMs");
        QTest::addColumn<bool>("taken");
        QTest::newRow("releases") << QStringLiteral("release") << 15000 << true;
        // The window spends the rest of its time waiting after a refusal, so this
        // bound is what the case costs. It only has to leave room for the reply.
        QTest::newRow("refuses") << QStringLiteral("refuse") << 3000 << false;
    }

    void coreHereAnswersForAnUnreadableLock()
    {
        QFETCH(QString, mode);
        QFETCH(int, timeoutMs);
        QFETCH(bool, taken);
        // A short path: a local socket's name has a limit a test profile can pass.
        QTemporaryDir socketDir(QStringLiteral("/tmp/nl-XXXXXX"));
        QVERIFY(socketDir.isValid());
        const QString socketPath = socketDir.filePath(QStringLiteral("c"));

        QProcess core;
        core.start(QCoreApplication::applicationFilePath(),
                   {QStringLiteral("--control-helper"), m_profile, socketPath, mode});
        const auto stop = qScopeGuard([&] {
            core.kill();
            core.waitForFinished();
        });
        QVERIFY(core.waitForStarted());
        QVERIFY(waitForReady(core));

        StationHandover ownership(m_profile);
        ownership.setFolderIsOnThisComputerForTest(false);
        ownership.setControlSocketPathForTest(socketPath);
        if (!readFile(ownership.lockPath()).isEmpty()) {
            QSKIP("An account that can read anything cannot set this up.");
        }

        QString error;
        QCOMPARE(ownership.reclaimFromBackground(timeoutMs, &error), taken);
        QCOMPARE(ownership.ownsProfile(), taken);
        if (taken) {
            QVERIFY2(error.isEmpty(), qPrintable(error));
            ownership.release();
        } else {
            QCOMPARE(error, kCoreRefusal);
        }
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
        if (argc > 3) {
            QTimer::singleShot(QString::fromLocal8Bit(argv[3]).toInt(), &app,
                               &QCoreApplication::quit);
        }
        return app.exec();
    }
    if (argc > 4 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--control-helper")) {
        // A Core on this computer, as far as a window can tell: it owns the
        // profile, its lock cannot be read, and it answers on the control socket.
        QCoreApplication app(argc, argv);
        StationHandover lock(QString::fromLocal8Bit(argv[2]));
        if (!lock.acquire(0)) { return 3; }
        if (!QFile::setPermissions(lock.lockPath(), QFileDevice::Permissions{})) { return 4; }
        const bool refuse = QString::fromLocal8Bit(argv[4]) == QLatin1String("refuse");
        StationControlSocket control([&](const QStringList&) -> StationControlReply {
            if (refuse) { return {false, kCoreRefusal}; }
            lock.release();
            return {true, QStringLiteral("The Core handed back the radio.")};
        });
        if (!control.listen(QString::fromLocal8Bit(argv[3]))) { return 5; }
        std::fputs("READY\n", stdout);
        std::fflush(stdout);
        return app.exec();
    }
    QCoreApplication app(argc, argv);
    TestStationForeignLock tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_station_foreign_lock.moc"
