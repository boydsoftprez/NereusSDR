// no-port-check: NereusSDR-original two-process station ownership tests.

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUuid>

#include <atomic>

#include "core/AppSettings.h"
#include "core/CoreInit.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"
#include "core/daemon/StationControlSocket.h"
#include "core/station/StationHandover.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

// The daemon helper uses the real entry point in the test-mode settings
// sandbox, with a disconnected radio and no network listener.
#define main nereusdEntryPointForTest
#include "../src/server_main.cpp"
#undef main

using namespace NereusSDR;

namespace {

QString profileName()
{
    // macOS local sockets have a short pathname bound. The test-mode
    // profile root is already deep, so keep the unique suffix short.
    return QStringLiteral("h%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(6));
}

QString configFile(QTemporaryDir& dir, bool shortSocketPath = false)
{
    const QString path = dir.filePath(QStringLiteral("nereusd.conf"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { return {}; }
    file.write("radio_mac = 00:00:00:00:00:00\nremote_port = 0\nstatus_page = off\n");
    if (shortSocketPath) {
        file.write(QByteArray("state_directory = ") + dir.path().toUtf8() + '\n');
    }
    file.close();
    return path;
}

bool waitForText(QProcess& process, const QByteArray& expected, int timeoutMs = 15000)
{
    QByteArray text;
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeoutMs) {
        text += process.readAll();
        if (text.contains(expected)) { return true; }
        process.waitForReadyRead(100);
    }
    return false;
}

QString preservedSettingsPath(const QString& settingsPath)
{
    return settingsPath + QStringLiteral(".handover-test-original");
}

bool blockSettingsSave(const QString& settingsPath)
{
    const QString preserved = preservedSettingsPath(settingsPath);
    if (!QFileInfo(settingsPath).isFile() || QFileInfo::exists(preserved)
        || !QFile::rename(settingsPath, preserved)) {
        return false;
    }
    if (QDir().mkdir(settingsPath)) { return true; }
    QFile::rename(preserved, settingsPath);
    return false;
}

bool restoreSettingsFile(const QString& settingsPath)
{
    return QDir().rmdir(settingsPath)
        && QFile::rename(preservedSettingsPath(settingsPath), settingsPath);
}

QByteArray fileBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

} // namespace

class TestStationHandover : public QObject {
    Q_OBJECT
private slots:
    void cleanupTestCase() { CoreInit::shutdown(); }
    void liveOldLockAndSeparateProfile()
    {
        const QString profile = profileName();
        QProcess holder;
        holder.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--lock-helper"), profile});
        const auto cleanup = qScopeGuard([&] {
            if (holder.state() != QProcess::NotRunning) {
                holder.kill();
                holder.waitForFinished();
            }
            QDir(AppSettings::resolveConfigDir(profile)).removeRecursively();
        });
        QVERIFY(holder.waitForStarted());
        QVERIFY(waitForText(holder, "READY"));

        StationHandover competitor(profile);
        QString error;
        QVERIFY(!competitor.acquire(0, &error));
        QVERIFY(!error.isEmpty());
        QFile oldLock(competitor.lockPath());
        QVERIFY(oldLock.open(QIODevice::ReadWrite));
        QVERIFY(oldLock.setFileTime(QDateTime::currentDateTime().addSecs(-60),
                                   QFileDevice::FileModificationTime));
        oldLock.close();
        QVERIFY(!competitor.acquire(0, &error));

        const QString otherProfile = profileName();
        StationHandover other(otherProfile);
        QVERIFY(other.acquire(0));
        other.release();
        QDir(AppSettings::resolveConfigDir(otherProfile)).removeRecursively();
    }

    void crashedOwnerIsReclaimed()
    {
        const QString profile = profileName();
        QProcess holder;
        holder.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--lock-helper"), profile});
        const auto cleanup = qScopeGuard([&] {
            if (holder.state() != QProcess::NotRunning) {
                holder.kill();
                holder.waitForFinished();
            }
            QDir(AppSettings::resolveConfigDir(profile)).removeRecursively();
        });
        QVERIFY(holder.waitForStarted());
        QVERIFY(waitForText(holder, "READY"));
        holder.kill();
        QVERIFY(holder.waitForFinished());

        StationHandover recovered(profile);
        QVERIFY(recovered.acquire(1000));
        recovered.release();
    }

    void releaseReplyAndSocketPathHandoff()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString config = configFile(scratch, true);
        QVERIFY(!config.isEmpty());
        const QString socketPath = QDir(scratch.path()).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName));
        const QString profile = profileName();
        StationHandover profileLock(profile);
        QProcess daemon;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_PRIMED_BOARD"), QStringLiteral("1"));
        daemon.setProcessEnvironment(env);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"), config,
                      QStringLiteral("--profile"), profile});
        const auto cleanup = qScopeGuard([&] {
            if (daemon.state() != QProcess::NotRunning) {
                daemon.kill();
                daemon.waitForFinished();
            }
            QDir(AppSettings::resolveConfigDir(profile)).removeRecursively();
        });
        QVERIFY(daemon.waitForStarted());
        const bool ready = QTest::qWaitFor([&] {
            return QFileInfo::exists(socketPath);
        }, 15000);
        QVERIFY2(ready, qPrintable(QStringLiteral("socket %1, daemon state %2, output: %3")
            .arg(socketPath).arg(int(daemon.state()))
            .arg(QString::fromUtf8(daemon.readAll()))));
        QVERIFY(!profileLock.acquire(0));

        QProcess command;
        command.start(QCoreApplication::applicationFilePath(),
                      {QStringLiteral("--command-helper"), QStringLiteral("--config"), config,
                       QStringLiteral("--profile"), profile, QStringLiteral("release")});
        QVERIFY(command.waitForStarted());
        QVERIFY2(command.waitForFinished(15000), qPrintable(command.errorString()));
        QCOMPARE(command.exitCode(), 0);
        QVERIFY(command.readAllStandardOutput().contains("The Core handed back the radio."));
        QVERIFY(profileLock.acquire(1000));
        QCOMPARE(QFileInfo::exists(socketPath), false);
        // The old listener's later destructor cannot unlink this new
        // owner's socket path after the lock has transferred.
        StationControlSocket successor([](const QStringList&) {
            return StationControlReply{true, QStringLiteral("new owner")};
        });
        QVERIFY(successor.listen(socketPath));
        QVERIFY(daemon.waitForFinished(5000));
        QCOMPARE(daemon.exitCode(), 0);
        QVERIFY(QFileInfo::exists(socketPath));
        successor.close();
        profileLock.release();
    }

    void absentRadioCanReleaseWithoutDiscovery()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString config = configFile(scratch, true);
        QVERIFY(!config.isEmpty());
        const QString socketPath = QDir(scratch.path()).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName));
        const QString profile = profileName();
        StationHandover profileLock(profile);
        QProcess daemon;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_DISABLE_DISCOVERY"), QStringLiteral("1"));
        daemon.setProcessEnvironment(env);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"), config,
                      QStringLiteral("--profile"), profile});
        const auto cleanup = qScopeGuard([&] {
            if (daemon.state() != QProcess::NotRunning) {
                daemon.kill();
                daemon.waitForFinished();
            }
            QDir(AppSettings::resolveConfigDir(profile)).removeRecursively();
        });
        QVERIFY(daemon.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(socketPath), 10000);
        QVERIFY(!profileLock.acquire(0));
        const StationControlReply reply = StationControlSocket::request(
            socketPath, {QStringLiteral("release")});
        QVERIFY2(reply.ok, qPrintable(reply.text));
        QVERIFY(profileLock.acquire(1000));
        profileLock.release();
        QVERIFY(daemon.waitForFinished(3000));
    }

    void preTeardownFailedSaveRestoresCommandsAndCanRetry()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString config = configFile(scratch, true);
        QVERIFY(!config.isEmpty());
        const QString socketPath = QDir(scratch.path()).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName));
        const QString profile = profileName();
        const QString settingsPath = AppSettings::resolveSettingsPath(profile);
        StationHandover profileLock(profile);
        QProcess daemon;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_DISABLE_DISCOVERY"), QStringLiteral("1"));
        daemon.setProcessEnvironment(env);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"), config,
                      QStringLiteral("--profile"), profile});
        const QString directory = AppSettings::resolveConfigDir(profile);
        const auto cleanup = qScopeGuard([&] {
            if (daemon.state() != QProcess::NotRunning) {
                daemon.kill();
                daemon.waitForFinished();
            }
            QDir(directory).removeRecursively();
        });
        QVERIFY(daemon.waitForStarted());
        const bool ready = QTest::qWaitFor([&] {
            return QFileInfo::exists(socketPath);
        }, 15000);
        QVERIFY2(ready, qPrintable(QStringLiteral("socket %1, daemon state %2, output: %3")
            .arg(socketPath).arg(int(daemon.state()))
            .arg(QString::fromUtf8(daemon.readAll()))));
        const QByteArray originalBytes = fileBytes(settingsPath);
        QVERIFY2(!originalBytes.isEmpty(), qPrintable(settingsPath));
        const QString sentinelPath = QDir(directory).filePath(QStringLiteral("handover-sentinel"));
        const QByteArray sentinelBytes("leave this profile sentinel unchanged");
        {
            QFile sentinel(sentinelPath);
            QVERIFY(sentinel.open(QIODevice::WriteOnly));
            QCOMPARE(sentinel.write(sentinelBytes), sentinelBytes.size());
        }
        const StationControlReply baselineShow = StationControlSocket::request(
            socketPath, {QStringLiteral("pairing"), QStringLiteral("show")});
        QVERIFY2(!baselineShow.text.contains(QStringLiteral("Only status and release")),
                 qPrintable(baselineShow.text));
        QVERIFY(blockSettingsSave(settingsPath));
        const StationControlReply failed = StationControlSocket::request(
            socketPath, {QStringLiteral("release")});
        QVERIFY(!failed.ok);
        QVERIFY2(failed.text.contains(QStringLiteral("could not save"))
                 || failed.text.contains(QStringLiteral("could not be saved")),
                 qPrintable(failed.text));
        QVERIFY(QFileInfo(settingsPath).isDir());
        QCOMPARE(fileBytes(preservedSettingsPath(settingsPath)), originalBytes);
        QCOMPARE(fileBytes(sentinelPath), sentinelBytes);
        QCOMPARE(daemon.state(), QProcess::Running);
        QVERIFY(!profileLock.acquire(0));
        StationControlReply show;
        QTRY_VERIFY_WITH_TIMEOUT((show = StationControlSocket::request(
            socketPath, {QStringLiteral("pairing"), QStringLiteral("show")})).ok
                == baselineShow.ok && show.text == baselineShow.text, 5000);
        const StationControlReply mutation = StationControlSocket::request(
            socketPath, {QStringLiteral("pairing"), QStringLiteral("open")});
        // The Core has recovered the normal command path. Whether opening
        // pairing changes state depends on its existing pairing state.
        QVERIFY2(!mutation.text.contains(QStringLiteral("Only status and release")),
                 qPrintable(mutation.text));
        QVERIFY(restoreSettingsFile(settingsPath));
        QCOMPARE(fileBytes(settingsPath), originalBytes);
        const StationControlReply retry = StationControlSocket::request(
            socketPath, {QStringLiteral("release")});
        QVERIFY2(retry.ok, qPrintable(retry.text));
        QVERIFY(profileLock.acquire(1000));
        profileLock.release();
    }

    void postTeardownFailedSaveStaysFencedAndCanRetry()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString config = configFile(scratch, true);
        QVERIFY(!config.isEmpty());
        const QString socketPath = QDir(scratch.path()).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName));
        const QString profile = profileName();
        const QString directory = AppSettings::resolveConfigDir(profile);
        const QString settingsPath = AppSettings::resolveSettingsPath(profile);
        StationHandover profileLock(profile);
        QProcess daemon;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_DISABLE_DISCOVERY"), QStringLiteral("1"));
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_PRIMED_BOARD"), QStringLiteral("1"));
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_FAIL_AFTER_STOP"), QStringLiteral("1"));
        daemon.setProcessEnvironment(env);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"), config,
                      QStringLiteral("--profile"), profile});
        const auto cleanup = qScopeGuard([&] {
            if (daemon.state() != QProcess::NotRunning) {
                daemon.kill();
                daemon.waitForFinished();
            }
            QDir(directory).removeRecursively();
        });
        QVERIFY(daemon.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(socketPath), 10000);
        const QByteArray originalBytes = fileBytes(settingsPath);
        QVERIFY2(!originalBytes.isEmpty(), qPrintable(settingsPath));
        const QString sentinelPath = QDir(directory).filePath(QStringLiteral("handover-sentinel"));
        const QByteArray sentinelBytes("leave this profile sentinel unchanged");
        {
            QFile sentinel(sentinelPath);
            QVERIFY(sentinel.open(QIODevice::WriteOnly));
            QCOMPARE(sentinel.write(sentinelBytes), sentinelBytes.size());
        }
        const StationControlReply failed = StationControlSocket::request(
            socketPath, {QStringLiteral("release")});
        QVERIFY(!failed.ok);
        QVERIFY2(failed.text.contains(QStringLiteral("could not save")),
                 qPrintable(failed.text));
        QVERIFY(QFileInfo(settingsPath).isDir());
        // stop() legitimately writes settings before this test-only hook
        // blocks the final save. Preserve those post-stop bytes exactly.
        const QByteArray preservedBytes = fileBytes(preservedSettingsPath(settingsPath));
        QVERIFY(!preservedBytes.isEmpty());
        QVERIFY(preservedBytes.contains("<DaemonProfileSeeded>True</DaemonProfileSeeded>"));
        QCOMPARE(fileBytes(sentinelPath), sentinelBytes);
        QCOMPARE(daemon.state(), QProcess::Running);
        QVERIFY(!profileLock.acquire(0));
        const StationControlReply mutation = StationControlSocket::request(
            socketPath, {QStringLiteral("pairing"), QStringLiteral("open")});
        QVERIFY(!mutation.ok);
        QVERIFY2(mutation.text.contains(QStringLiteral("Only status and release")),
                 qPrintable(mutation.text));
        QCOMPARE(fileBytes(preservedSettingsPath(settingsPath)), preservedBytes);
        QCOMPARE(fileBytes(sentinelPath), sentinelBytes);
        QVERIFY(restoreSettingsFile(settingsPath));
        QCOMPARE(fileBytes(settingsPath), preservedBytes);
        const StationControlReply retry = StationControlSocket::request(
            socketPath, {QStringLiteral("release")});
        QVERIFY2(retry.ok, qPrintable(retry.text));
        QVERIFY(profileLock.acquire(1000));
        profileLock.release();
    }

    void dirtyOfflineReceiverKeepsProcessAndLock()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString config = configFile(scratch, true);
        QVERIFY(!config.isEmpty());
        const QString socketPath = QDir(scratch.path()).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName));
        const QString profile = profileName();
        StationHandover profileLock(profile);
        QProcess daemon;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_DISABLE_DISCOVERY"), QStringLiteral("1"));
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_DIRTY_OFFLINE"), QStringLiteral("1"));
        daemon.setProcessEnvironment(env);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"), config,
                      QStringLiteral("--profile"), profile});
        const auto cleanup = qScopeGuard([&] {
            if (daemon.state() != QProcess::NotRunning) {
                daemon.kill();
                daemon.waitForFinished();
            }
            QDir(AppSettings::resolveConfigDir(profile)).removeRecursively();
        });
        QVERIFY(daemon.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(socketPath), 10000);
        const StationControlReply refused = StationControlSocket::request(
            socketPath, {QStringLiteral("release")});
        QVERIFY(!refused.ok);
        QVERIFY2(refused.text.contains(QStringLiteral("unsaved receiver changes")),
                 qPrintable(refused.text));
        QCOMPARE(daemon.state(), QProcess::Running);
        QVERIFY(!profileLock.acquire(0));
        const StationControlReply retry = StationControlSocket::request(
            socketPath, {QStringLiteral("release")});
        QVERIFY(!retry.ok);
        QVERIFY(retry.text.contains(QStringLiteral("unsaved receiver changes")));
        QVERIFY(!profileLock.acquire(0));
    }

    void parsedReleaseWaitsForDeferredCompletion()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString config = configFile(scratch, true);
        QVERIFY(!config.isEmpty());
        const QString socketPath = QDir(scratch.path()).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName));
        const QString profile = profileName();
        StationHandover profileLock(profile);
        QProcess daemon;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_PRIMED_BOARD"), QStringLiteral("1"));
        // Pass beyond the ordinary five-second incomplete-request timer.
        // The parsed release has its own bounded completion lifetime.
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_RELEASE_DELAY_MS"),
                   QStringLiteral("5200"));
        daemon.setProcessEnvironment(env);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"), config,
                      QStringLiteral("--profile"), profile});
        const auto cleanup = qScopeGuard([&] {
            if (daemon.state() != QProcess::NotRunning) {
                daemon.kill();
                daemon.waitForFinished();
            }
            QDir(AppSettings::resolveConfigDir(profile)).removeRecursively();
        });
        QVERIFY(daemon.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(socketPath), 10000);

        QLocalSocket client;
        client.connectToServer(socketPath);
        QVERIFY(client.waitForConnected(1000));
        const QByteArray request = QJsonDocument(QJsonObject{
            {QStringLiteral("args"), QJsonArray{QStringLiteral("release")}}})
            .toJson(QJsonDocument::Compact) + '\n';
        QCOMPARE(client.write(request), qint64(request.size()));
        QVERIFY(client.waitForBytesWritten(1000));
        QTest::qWait(150);
        QCOMPARE(client.bytesAvailable(), qint64(0));
        QVERIFY(!profileLock.acquire(0));
        QVERIFY(client.waitForReadyRead(7000));
        const QJsonObject answer = QJsonDocument::fromJson(client.readAll()).object();
        QVERIFY(answer.value(QStringLiteral("ok")).toBool());
        QVERIFY(profileLock.acquire(1000));
        profileLock.release();
        QVERIFY(daemon.waitForFinished(3000));
    }

    void releaseWithoutCoreDoesNotCreateProfile()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString config = configFile(scratch);
        QVERIFY(!config.isEmpty());
        const QString profile = profileName();
        const QString directory = AppSettings::resolveConfigDir(profile);
        QDir(directory).removeRecursively();
        QProcess command;
        command.start(QCoreApplication::applicationFilePath(),
                      {QStringLiteral("--command-helper"), QStringLiteral("--config"), config,
                       QStringLiteral("--profile"), profile, QStringLiteral("release")});
        QVERIFY(command.waitForStarted());
        QVERIFY(command.waitForFinished(5000));
        QCOMPARE(command.exitCode(), 1);
        QVERIFY(command.readAllStandardError().contains("No Core answered"));
        QVERIFY(!QFileInfo::exists(directory));
    }

    void deferredConnectKeepsBorrowedModelAlive()
    {
        DaemonConfig config = DaemonConfig::defaults();
        config.remotePort = 0;
        config.statusPage = false;
        DaemonApp daemon;
        daemon.primeBoardForTest(HPSDRHW::HermesLite,
                                 QStringLiteral("02:00:00:00:00:49"));
        QVERIFY(daemon.start(config));
        RadioModel* const model = daemon.radioModelForTest();
        QVERIFY(model != nullptr);
        daemon.setRadioConnectInProgressForTest(true);
        daemon.beginStationRelease();
        QString reason;
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Pending);
        QCOMPARE(daemon.radioModelForTest(), model);
        daemon.setRadioConnectInProgressForTest(false);
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Stopped);
        QCOMPARE(daemon.radioModelForTest(), nullptr);
    }

    void invalidSavedLayoutIsKeptWhenRadioAbsent()
    {
        AppSettings::instance().remove(QStringLiteral("StationRadioChoice"));
        const QString mac = QStringLiteral("02:00:00:00:00:51");
        const QString raw = QStringLiteral("{\"version\":999,\"slices\":[{}],\"opaque\":\"keep me\"}");
        AppSettings& settings = AppSettings::instance();
        settings.setHardwareValue(mac, QStringLiteral("receiveLayout"), raw);
        const auto cleanup = qScopeGuard([&] {
            settings.remove(QStringLiteral("hardware/%1/receiveLayout")
                                .arg(AppSettings::normalizedRadioMac(mac)));
            settings.save();
        });
        DaemonConfig config = DaemonConfig::defaults();
        config.radioMac = mac;
        config.remotePort = 0;
        config.statusPage = false;
        DaemonApp daemon;
        daemon.setDiscoveryProviderForTest([] { return QList<RadioInfo>{}; });
        QVERIFY(daemon.start(config));
        QVERIFY(daemon.radioModelForTest()->receiveLayoutPendingAdmission());
        QCOMPARE(daemon.radioModelForTest()->receiveLayoutRestoreState(),
                 QStringLiteral("invalid"));
        daemon.beginStationRelease();
        QString reason;
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Stopped);
        QCOMPARE(settings.hardwareValue(mac, QStringLiteral("receiveLayout")).toString().toUtf8(),
                 raw.toUtf8());
    }

    void offlineReceiverEditRefusesReleaseAndKeepsLiveValue()
    {
        AppSettings::instance().remove(QStringLiteral("StationRadioChoice"));
        DaemonConfig config = DaemonConfig::defaults();
        config.radioMac = QStringLiteral("02:00:00:00:00:52");
        config.remotePort = 0;
        config.statusPage = false;
        DaemonApp daemon;
        daemon.setDiscoveryProviderForTest([] { return QList<RadioInfo>{}; });
        QVERIFY(daemon.start(config));
        RadioModel* const model = daemon.radioModelForTest();
        QVERIFY(model && model->receiveLayoutPendingAdmission());
        SliceModel* const slice = model->sliceById(0);
        QVERIFY(slice);
        const double changedFrequency = slice->frequency() + 1000.0;
        slice->setFrequency(changedFrequency);
        QCOMPARE(slice->frequency(), changedFrequency);
        daemon.beginStationRelease();
        QString reason;
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Failed);
        QVERIFY2(reason.contains(QStringLiteral("unsaved receiver changes")), qPrintable(reason));
        QCOMPARE(daemon.radioModelForTest(), model);
        QCOMPARE(slice->frequency(), changedFrequency);
        // A retry cannot establish a fresh baseline over the unsaved edit.
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Failed);
        daemon.stop();
    }

    void offlineRefusalDefersRecoveryAndKeepsOriginalModel()
    {
        QTcpServer portProbe;
        QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
        const quint16 port = portProbe.serverPort();
        portProbe.close();
        DaemonConfig config = DaemonConfig::defaults();
        config.radioMac = QStringLiteral("02:00:00:00:00:54");
        config.remoteBind = QStringLiteral("127.0.0.1");
        config.remotePort = port;
        config.statusPage = false;
        DaemonApp daemon;
        std::atomic<int> discoveryAttempts {0};
        daemon.setDiscoveryProviderForTest([&] {
            ++discoveryAttempts;
            return QList<RadioInfo>{};
        });
        QVERIFY(daemon.start(config));
        QVERIFY(daemon.stationListenerReady());
        QTRY_VERIFY_WITH_TIMEOUT(discoveryAttempts.load() >= 1, 5000);
        RadioModel* const model = daemon.radioModelForTest();
        QVERIFY(model && model->receiveLayoutPendingAdmission());
        SliceModel* const slice = model->sliceById(0);
        QVERIFY(slice);
        const double editedFrequency = slice->frequency() + 1000.0;
        slice->setFrequency(editedFrequency);

        daemon.beginStationRelease();
        QVERIFY(!daemon.stationListenerReady());
        daemon.setRadioConnectInProgressForTest(true);
        QString reason;
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Pending);
        QCOMPARE(daemon.recoverFailedStationRelease(),
                 DaemonApp::StationReleaseRecoveryResult::Pending);
        QCOMPARE(daemon.radioModelForTest(), model);
        daemon.setRadioConnectInProgressForTest(false);
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Failed);
        QVERIFY(reason.contains(QStringLiteral("unsaved receiver changes")));
        QCOMPARE(daemon.recoverFailedStationRelease(),
                 DaemonApp::StationReleaseRecoveryResult::Restored);
        QCOMPARE(daemon.radioModelForTest(), model);
        QCOMPARE(slice->frequency(), editedFrequency);
        QVERIFY(daemon.stationListenerReady());
        QCOMPARE(daemon.stationListenAttemptCountForTest(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(discoveryAttempts.load() >= 2, 5000);

        daemon.beginStationRelease();
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Failed);
        QCOMPARE(daemon.recoverFailedStationRelease(),
                 DaemonApp::StationReleaseRecoveryResult::Restored);
        QCOMPARE(daemon.radioModelForTest(), model);
        QCOMPARE(slice->frequency(), editedFrequency);
        QVERIFY(daemon.stationListenerReady());
        QCOMPARE(daemon.stationListenAttemptCountForTest(), 1);
        daemon.stop();
    }

    void offlineGainAndMembershipEditsRefuseRelease()
    {
        AppSettings::instance().remove(QStringLiteral("StationRadioChoice"));
        DaemonConfig config = DaemonConfig::defaults();
        config.radioMac = QStringLiteral("02:00:00:00:00:53");
        config.remotePort = 0;
        config.statusPage = false;
        {
            DaemonApp daemon;
            daemon.setDiscoveryProviderForTest([] { return QList<RadioInfo>{}; });
            QVERIFY(daemon.start(config));
            SliceModel* const slice = daemon.radioModelForTest()->sliceById(0);
            QVERIFY(slice);
            slice->setAfGain(slice->afGain() + 1);
            daemon.beginStationRelease();
            QString reason;
            QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                     DaemonApp::StationReleaseResult::Failed);
            QVERIFY(reason.contains(QStringLiteral("unsaved receiver changes")));
            daemon.stop();
        }
        {
            DaemonApp daemon;
            daemon.setDiscoveryProviderForTest([] { return QList<RadioInfo>{}; });
            QVERIFY(daemon.start(config));
            RadioModel* const model = daemon.radioModelForTest();
            QVERIFY(model->addSlice() >= 0);
            daemon.beginStationRelease();
            QString reason;
            QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                     DaemonApp::StationReleaseResult::Failed);
            QVERIFY(reason.contains(QStringLiteral("unsaved receiver changes")));
            daemon.stop();
        }
        {
            // Create the initial two-slice seed before enabling ingress
            // tracking, then exercise removal alone without prior edits.
            RadioModel model;
            model.prepareReceiveLayout(config.radioMac);
            QCOMPARE(model.addSlice(), 0);
            QCOMPARE(model.addSlice(), 1);
            model.beginStationHandoverEditTracking();
            model.removeSlice(1);
            QVERIFY(!model.sliceById(1));
            QString reason;
            QVERIFY(!model.saveForStationHandover(&reason));
            QVERIFY(reason.contains(QStringLiteral("unsaved receiver changes")));
        }
    }

    void stopAllTxIsFirstStationOperation()
    {
        QTcpServer portProbe;
        QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
        const quint16 port = portProbe.serverPort();
        portProbe.close();
        DaemonConfig config = DaemonConfig::defaults();
        config.remoteBind = QStringLiteral("127.0.0.1");
        config.remotePort = port;
        config.statusPage = false;
        DaemonApp daemon;
        daemon.primeBoardForTest(HPSDRHW::HermesLite,
                                 QStringLiteral("02:00:00:00:00:50"));
        QVERIFY(daemon.start(config));
        QVERIFY(daemon.stationListenerReady());
        bool stopWasFirst = false;
        daemon.setStopAllTxForTest([&] {
            stopWasFirst = daemon.stationListenerReady();
        });
        daemon.beginStationRelease();
        QVERIFY(stopWasFirst);
        QVERIFY(!daemon.stationListenerReady());
        QString reason;
        QCOMPARE(daemon.tryCompleteStationRelease(&reason),
                 DaemonApp::StationReleaseResult::Stopped);
    }

    // LINK minor 3 (TX path): a SIGTERM's stop() stops transmit first, as
    // the release does, before the station or the model goes.
    void stopStopsTransmitFirst()
    {
        QTcpServer portProbe;
        QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
        const quint16 port = portProbe.serverPort();
        portProbe.close();
        DaemonConfig config = DaemonConfig::defaults();
        config.remoteBind = QStringLiteral("127.0.0.1");
        config.remotePort = port;
        config.statusPage = false;
        DaemonApp daemon;
        daemon.primeBoardForTest(HPSDRHW::HermesLite,
                                 QStringLiteral("02:00:00:00:00:51"));
        QVERIFY(daemon.start(config));
        QVERIFY(daemon.stationListenerReady());
        int stops = 0;
        bool stopWasFirst = false;
        daemon.setStopAllTxForTest([&] {
            if (stops++ == 0) {
                stopWasFirst = daemon.stationListenerReady()
                               && daemon.radioModelForTest() != nullptr;
            }
        });
        daemon.stop();
        QCOMPARE(stops, 1);
        QVERIFY(stopWasFirst);
        QVERIFY(!daemon.stationListenerReady());
    }
};

int main(int argc, char* argv[])
{
    if (argc > 1 && (QString::fromLocal8Bit(argv[1]) == QLatin1String("--daemon-helper")
                     || QString::fromLocal8Bit(argv[1]) == QLatin1String("--command-helper"))) {
        return nereusdEntryPointForTest(argc - 1, argv + 1);
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
    TestStationHandover tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_station_handover.moc"
