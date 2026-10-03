// no-port-check: NereusSDR-original Task 48 process handover acceptance.
// A GUI owner and the real daemon helper share one sandbox profile. The
// service command runner is fake; it never invokes the host OS service tool.

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

#include "core/AppSettings.h"
#include "core/CoreInit.h"
#include "core/RadioDiscovery.h"
#include "core/ReceiveLayoutStore.h"
#include "core/daemon/StationControlSocket.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "core/station/StationHandover.h"
#include "core/station/StationHost.h"
#include "gui/GuiDesktopStationRuntime.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"

#define main nereusdEntryPointForTest
#include "../src/server_main.cpp"
#undef main

using namespace NereusSDR;

namespace {

void marker(const QByteArray& event)
{
    std::fputs(event.constData(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

QString profileName()
{
    // Short names keep the profile-local control socket below macOS' limit.
    return QStringLiteral("r%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(6));
}

bool writeConfig(const QString& profileDirectory, quint16 port,
                 const QString& radioMac = {})
{
    QFile config(QDir(profileDirectory).filePath(QStringLiteral("station.conf")));
    const QByteArray bytes = QStringLiteral("remote_bind = 127.0.0.1\nremote_port = %1\n"
        "status_page = off\nrendezvous_servers =\nradio_mac = %2\n")
        .arg(port).arg(radioMac).toUtf8();
    return config.open(QIODevice::WriteOnly) && config.write(bytes) == bytes.size();
}

// The GUI helper follows main.cpp's component lifetime and save/unlock order.
// A file command avoids blocking its Qt loop while the parent runs the client.
int guiHelper(int argc, char* argv[])
{
    QApplication app(argc, argv);
    if (argc != 5) { return 10; }
    const QString profile = QString::fromLocal8Bit(argv[2]);
    const QString home = QString::fromLocal8Bit(argv[3]);
    const QString phase = QString::fromLocal8Bit(argv[4]);
    AppSettings::setProfileOverride(profile);
    StationHandover ownership(profile);
    QString error;
    const bool acquired = phase == QLatin1String("reopen")
        ? ownership.reclaimFromBackground(15000, &error) : ownership.acquire(0, &error);
    if (!acquired) { marker("ERROR acquire " + error.toUtf8()); return 11; }
    CoreInit::initialize(profile);
    AppSettings& settings = AppSettings::instance();
    RadioDiscovery discovery;
    discovery.holdOffScans(std::chrono::minutes{5});

    StationServiceOptions options;
    options.profile = profile;
    options.inheritActiveProfile = false;
    options.profileDirectory = AppSettings::resolveConfigDir(profile);
    options.homeDirectory = home;
    options.binaryPath = QCoreApplication::applicationFilePath();
    options.platform = StationPlatform::Linux;
    options.runner = [](const QString& command, const QStringList& args) {
        if (command != QLatin1String("systemctl")) {
            marker("ERROR unexpected-service-command " + command.toUtf8());
            return StationServiceCommandResult{1, {}};
        }
        if (args.contains(QStringLiteral("show"))) {
            return StationServiceCommandResult{0,
                QStringLiteral("LoadState=not-found\nActiveState=inactive\n")};
        }
        if (args.contains(QStringLiteral("is-enabled"))) {
            return StationServiceCommandResult{1, {}};
        }
        if (args.contains(QStringLiteral("start"))) {
            marker("LAUNCH_REQUEST");
        }
        return StationServiceCommandResult{0, {}};
    };

    GuiSessionCoordinator sessions;
    if (!sessions.configureDesktopStation(profile, ownership.ownsProfile(), options)
        || !sessions.replace({}, false, &error)) {
        marker("ERROR session " + error.toUtf8());
        CoreInit::shutdown();
        return 12;
    }
    GuiDesktopStationRuntime* const runtime = sessions.desktopRuntime();
    if (!runtime || !runtime->controller()->host()
        || !runtime->controller()->host()->listenerReady()
        || !sessions.window() || sessions.window()->radioModel()->connection()) {
        marker("ERROR listener");
        sessions.shutdown();
        CoreInit::shutdown();
        return 13;
    }
    marker("READY " + phase.toUtf8() + " " + QByteArray::number(app.applicationPid()));

    QTimer command;
    command.setInterval(20);
    QObject::connect(&command, &QTimer::timeout, &app, [&] {
        const QString commandPath = QDir(home).filePath(QStringLiteral("stop-%1").arg(phase));
        if (!QFileInfo::exists(commandPath)) { return; }
        command.stop();
        QFile::remove(commandPath);
        if (phase == QLatin1String("first")) {
            if (!sessions.prepareApplicationQuit(&error)
                || !sessions.backgroundServiceOptions()) {
                marker("ERROR prepare " + error.toUtf8());
                app.exit(14);
                return;
            }
        } else if (!sessions.prepareApplicationQuit(&error)) {
            marker("ERROR prepare " + error.toUtf8());
            app.exit(15);
            return;
        }
        const std::optional<StationServiceOptions> background =
            sessions.backgroundServiceOptions();
        sessions.shutdown();
        if (!settings.save(&error)) {
            marker("ERROR final-save " + error.toUtf8());
            app.exit(16);
            return;
        }
        CoreInit::shutdown();
        ownership.release();
        StationHandover probe(profile);
        const bool lockFree = probe.acquire(0);
        if (lockFree) { probe.release(); }
        QTcpServer listenerProbe;
        const quint16 port = static_cast<quint16>(qEnvironmentVariableIntValue(
            "NEREUS_HANDOVER_TEST_PORT"));
        const bool listenerFree = listenerProbe.listen(QHostAddress::LocalHost, port);
        listenerProbe.close();
        if (!lockFree || !listenerFree) {
            marker("ERROR owner-not-retired");
            app.exit(17);
            return;
        }
        marker("RETIRED " + phase.toUtf8());
        if (background) {
            StationServiceManager service(*background);
            if (!service.startBackground()) {
                marker("ERROR service " + service.lastError().toUtf8());
                app.exit(18);
                return;
            }
        }
        app.quit();
    });
    command.start();
    const int result = app.exec();
    if (ownership.ownsProfile()) {
        sessions.shutdown();
        CoreInit::shutdown();
        ownership.release();
    }
    return result;
}

bool waitForMarker(QProcess& process, QByteArray& output, const QByteArray& expected,
                   int timeoutMs)
{
    return QTest::qWaitFor([&] {
        output += process.readAllStandardOutput();
        return output.contains(expected) || process.state() == QProcess::NotRunning;
    }, timeoutMs) && output.contains(expected);
}

bool stopHelper(const QString& home, const QString& phase)
{
    QFile command(QDir(home).filePath(QStringLiteral("stop-%1").arg(phase)));
    return command.open(QIODevice::WriteOnly);
}

} // namespace

class TestStationHandoverRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void refusedOfflineReleaseRestoresPairedListener()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString profile = profileName();
        const QString profileDirectory = AppSettings::resolveConfigDir(profile);
        const auto cleanup = qScopeGuard([&] { QDir(profileDirectory).removeRecursively(); });
        QVERIFY(QDir().mkpath(profileDirectory));
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const quint16 port = reservation.serverPort();
        const QString radioMac = QStringLiteral("02:00:00:00:00:49");
        QVERIFY(writeConfig(profileDirectory, port, radioMac));
        const QString settingsPath = QDir(profileDirectory).filePath(
            QStringLiteral("NereusSDR.settings"));
        AppSettings saved(settingsPath);
        const QList<ReceiveSliceState> baseline{
            {0, QStringLiteral("pan-0"), 14293200.0, DSPMode::USB},
        };
        QVERIFY(ReceiveLayoutStore::stage(saved, radioMac, baseline));
        QVERIFY(saved.save());
        AppSettings baselineSettings(settingsPath);
        baselineSettings.load();
        const QVariant baselineLayout = baselineSettings.hardwareValue(
            AppSettings::normalizedRadioMac(radioMac), QStringLiteral("receiveLayout"));
        QVERIFY(baselineLayout.isValid());
        const auto savedLayout = ReceiveLayoutStore::load(baselineSettings, radioMac);
        QCOMPARE(savedLayout.state, ReceiveLayoutStore::LoadState::Loaded);
        QCOMPARE(savedLayout.slices.size(), 1);
        QCOMPARE(savedLayout.slices.front().frequencyHz, baseline.front().frequencyHz);

        const StationIdentity station = StationIdentity::loadOrCreate(profileDirectory);
        QVERIFY2(station.isValid(), qPrintable(station.lastError()));
        const auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(scratch.filePath(QStringLiteral("client"))));
        QVERIFY2(key->isValid(), qPrintable(key->lastError()));
        DeviceStore devices(profileDirectory);
        PairedDevice paired;
        paired.id = key->fingerprint();
        paired.publicKeySpki = key->publicKeySpki();
        paired.name = QStringLiteral("Refused handover client");
        paired.kind = QStringLiteral("computer");
        QVERIFY2(devices.add(paired), qPrintable(devices.lastError()));
        reservation.close();

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_DISABLE_DISCOVERY"), QStringLiteral("1"));
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_DIRTY_OFFLINE"), QStringLiteral("1"));
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_RECOVERY_DELAY_MS"),
                   QStringLiteral("1500"));
        QProcess daemon;
        daemon.setProcessEnvironment(env);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"),
                      QDir(profileDirectory).filePath(QStringLiteral("station.conf")),
                      QStringLiteral("--profile"), profile});
        const auto stopDaemon = qScopeGuard([&] {
            if (daemon.state() != QProcess::NotRunning) {
                daemon.kill();
                daemon.waitForFinished(3000);
            }
        });
        QVERIFY(daemon.waitForStarted());
        const QString socketPath = QDir(profileDirectory).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(socketPath), 10000);
        StationHandover competitor(profile);
        QVERIFY(!competitor.acquire(0));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setDeviceIdentity(key, paired.name);
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        QSignalSpy reconnects(&client, &StationClient::reconnectScheduled);
        client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(port)),
                                {}, {}, false, station.fingerprint());
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.count(), 1, 15000);

        for (int attempt = 1; attempt <= 2; ++attempt) {
            const StationControlReply refused = StationControlSocket::request(
                socketPath, {QStringLiteral("release")});
            QVERIFY(!refused.ok);
            QVERIFY2(refused.text.contains(QStringLiteral("unsaved receiver changes")),
                     qPrintable(refused.text));
            QCOMPARE(daemon.state(), QProcess::Running);
            QVERIFY(!competitor.acquire(0));
            const StationControlReply duplicate = StationControlSocket::request(
                socketPath, {QStringLiteral("release")});
            QVERIFY(!duplicate.ok);
            QVERIFY2(duplicate.text.contains(QStringLiteral("already handing back")),
                     qPrintable(duplicate.text));
            const StationControlReply mutation = StationControlSocket::request(
                socketPath, {QStringLiteral("pairing"), QStringLiteral("open")});
            QVERIFY(!mutation.ok);
            QVERIFY2(mutation.text.contains(QStringLiteral("Only status and release")),
                     qPrintable(mutation.text));
            AppSettings reloaded(settingsPath);
            reloaded.load();
            QCOMPARE(reloaded.hardwareValue(AppSettings::normalizedRadioMac(radioMac),
                                            QStringLiteral("receiveLayout")), baselineLayout);
            const auto stillSaved = ReceiveLayoutStore::load(reloaded, radioMac);
            QCOMPARE(stillSaved.state, ReceiveLayoutStore::LoadState::Loaded);
            QCOMPARE(stillSaved.slices.front().frequencyHz, baseline.front().frequencyHz);
            QTRY_COMPARE_WITH_TIMEOUT(handshakes.count(), attempt + 1, 15000);
            QCOMPARE(client.stationIdentityFingerprint(), station.fingerprint());
            QVERIFY(reconnects.count() >= attempt);
            QTcpServer duplicateListener;
            QVERIFY(!duplicateListener.listen(QHostAddress::LocalHost, port));
        }
        client.disconnectFromStation(QStringLiteral("test finished"));
        qInfo() << "refused release kept saved layout, lock, and paired listener; handshakes"
                << handshakes.count() << "port" << port;
    }

    void pairedClientFollowsBothOwners()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString profile = profileName();
        const QString profileDirectory = AppSettings::resolveConfigDir(profile);
        const auto cleanup = qScopeGuard([&] {
            QDir(profileDirectory).removeRecursively();
        });
        QVERIFY(QDir().mkpath(profileDirectory));
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const quint16 port = reservation.serverPort();
        QVERIFY(writeConfig(profileDirectory, port));
        AppSettings saved(QDir(profileDirectory).filePath(QStringLiteral("NereusSDR.settings")));
        saved.setValue(QStringLiteral("DesktopCore/Run"), true);
        saved.setValue(QStringLiteral("DesktopCore/KeepRunning"), true);
        QVERIFY(saved.save());

        const StationIdentity station = StationIdentity::loadOrCreate(profileDirectory);
        QVERIFY2(station.isValid(), qPrintable(station.lastError()));
        const auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(scratch.filePath(QStringLiteral("client"))));
        QVERIFY2(key->isValid(), qPrintable(key->lastError()));
        DeviceStore devices(profileDirectory);
        PairedDevice paired;
        paired.id = key->fingerprint();
        paired.publicKeySpki = key->publicKeySpki();
        paired.name = QStringLiteral("Handover test client");
        paired.kind = QStringLiteral("computer");
        QVERIFY2(devices.add(paired), qPrintable(devices.lastError()));
        reservation.close();

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        env.insert(QStringLiteral("NEREUS_HANDOVER_TEST_PORT"), QString::number(port));
        QProcess first;
        first.setProcessEnvironment(env);
        first.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("--gui-helper"), profile, scratch.path(),
                     QStringLiteral("first")});
        QProcess daemon;
        QProcess reopened;
        const auto stopProcesses = qScopeGuard([&] {
            for (QProcess* process : {&first, &daemon, &reopened}) {
                if (process->state() != QProcess::NotRunning) {
                    process->kill();
                    process->waitForFinished(3000);
                }
            }
        });
        QVERIFY(first.waitForStarted());
        const qint64 firstPid = first.processId();
        QByteArray firstOutput;
        QVERIFY2(waitForMarker(first, firstOutput, "READY first", 15000), firstOutput.constData());
        StationHandover competitor(profile);
        QVERIFY(!competitor.acquire(0));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setDeviceIdentity(key, QStringLiteral("Handover test client"));
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        QSignalSpy reconnects(&client, &StationClient::reconnectScheduled);
        client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(port)),
                                {}, {}, false, station.fingerprint());
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.count(), 1, 15000);
        QCOMPARE(client.stationIdentityFingerprint(), station.fingerprint());

        QElapsedTimer toDaemon;
        toDaemon.start();
        QVERIFY(stopHelper(scratch.path(), QStringLiteral("first")));
        QVERIFY2(waitForMarker(first, firstOutput, "RETIRED first", 10000), firstOutput.constData());
        QVERIFY2(waitForMarker(first, firstOutput, "LAUNCH_REQUEST", 10000), firstOutput.constData());
        QVERIFY(firstOutput.indexOf("RETIRED first") < firstOutput.indexOf("LAUNCH_REQUEST"));
        QVERIFY(first.state() == QProcess::NotRunning || first.waitForFinished(3000));
        QCOMPARE(first.exitCode(), 0);
        QVERIFY(QFileInfo::exists(QDir(scratch.path()).filePath(
            QStringLiteral(".config/systemd/user/nereusd.service"))));
        QVERIFY(competitor.acquire(0));
        competitor.release();
        QVERIFY(!QFileInfo::exists(QDir(profileDirectory).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName))));

        QProcessEnvironment daemonEnv = env;
        daemonEnv.insert(QStringLiteral("NEREUS_HANDOVER_TEST_DISABLE_DISCOVERY"),
                         QStringLiteral("1"));
        // Complete the logical receive-layout admission without a physical
        // board. In discovery-disabled mode, a paired client's ordinary
        // receiver adoption is deliberately unsaveable until admission.
        daemonEnv.insert(QStringLiteral("NEREUS_HANDOVER_TEST_PRIMED_BOARD"),
                         QStringLiteral("1"));
        daemon.setProcessEnvironment(daemonEnv);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"),
                      QDir(profileDirectory).filePath(QStringLiteral("station.conf")),
                      QStringLiteral("--profile"), profile});
        QVERIFY(daemon.waitForStarted());
        const qint64 daemonPid = daemon.processId();
        const QString socketPath = QDir(profileDirectory).filePath(
            QString::fromLatin1(StationControlSocket::kSocketName));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(socketPath), 10000);
        QVERIFY(!competitor.acquire(0));
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.count(), 2, 15000);
        QVERIFY2(toDaemon.elapsed() <= 15000, qPrintable(QString::number(toDaemon.elapsed())));
        const qint64 guiToDaemonMs = toDaemon.elapsed();
        QCOMPARE(client.stationIdentityFingerprint(), station.fingerprint());
        QVERIFY(reconnects.count() >= 1);

        QElapsedTimer toGui;
        toGui.start();
        reopened.setProcessEnvironment(env);
        reopened.start(QCoreApplication::applicationFilePath(),
                       {QStringLiteral("--gui-helper"), profile, scratch.path(),
                        QStringLiteral("reopen")});
        QVERIFY(reopened.waitForStarted());
        const qint64 reopenedPid = reopened.processId();
        QByteArray reopenOutput;
        const bool reopenedReady = waitForMarker(reopened, reopenOutput, "READY reopen", 15000);
        if (!reopenedReady) {
            const bool finishedForDiagnosis = QTest::qWaitFor([&] {
                reopenOutput += reopened.readAllStandardOutput();
                return reopened.state() == QProcess::NotRunning;
            }, 1200);
            Q_UNUSED(finishedForDiagnosis);
        }
        QVERIFY2(reopenedReady,
                 qPrintable(QStringLiteral("GUI state=%1 error=%2 stdout=%3 stderr=%4; "
                                           "daemon state=%5 stdout=%6 stderr=%7")
                     .arg(int(reopened.state())).arg(reopened.errorString())
                     .arg(QString::fromUtf8(reopenOutput))
                     .arg(QString::fromUtf8(reopened.readAllStandardError()))
                     .arg(int(daemon.state()))
                     .arg(QString::fromUtf8(daemon.readAllStandardOutput()))
                     .arg(QString::fromUtf8(daemon.readAllStandardError()))));
        QVERIFY(daemon.state() == QProcess::NotRunning || daemon.waitForFinished(3000));
        QCOMPARE(daemon.exitCode(), 0);
        QVERIFY(!competitor.acquire(0));
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.count(), 3, 15000);
        QVERIFY2(toGui.elapsed() <= 15000, qPrintable(QString::number(toGui.elapsed())));
        const qint64 daemonToGuiMs = toGui.elapsed();
        QCOMPARE(client.stationIdentityFingerprint(), station.fingerprint());
        QVERIFY(reconnects.count() >= 2);
        DeviceStore after(profileDirectory);
        QVERIFY(after.find(paired.id).has_value());
        QCOMPARE(after.list().size(), 1);

        client.disconnectFromStation(QStringLiteral("test finished"));
        QVERIFY(stopHelper(scratch.path(), QStringLiteral("reopen")));
        QVERIFY2(waitForMarker(reopened, reopenOutput, "RETIRED reopen", 10000),
                 reopenOutput.constData());
        QVERIFY(reopened.state() == QProcess::NotRunning || reopened.waitForFinished(3000));
        QCOMPARE(reopened.exitCode(), 0);
        QVERIFY(competitor.acquire(0));
        competitor.release();
        qInfo().noquote() << "handover elapsed ms GUI-to-daemon" << guiToDaemonMs
                          << "daemon-to-GUI" << daemonToGuiMs << "port" << port
                          << "client handshakes" << handshakes.count()
                          << "owner PIDs" << firstPid << daemonPid << reopenedPid;
    }
};

int main(int argc, char* argv[])
{
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--daemon-helper")) {
        return nereusdEntryPointForTest(argc - 1, argv + 1);
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--gui-helper")) {
        return guiHelper(argc, argv);
    }
    // QLocalServer's Unix path includes the profile root. CTest's ordinary
    // per-test home is too deep on macOS, so make a short, still isolated
    // home before Qt or AppSettings resolves any writable location.
#ifdef Q_OS_MAC
    QTemporaryDir shortHome(QStringLiteral("/tmp/nrh-XXXXXX"));
#else
    QTemporaryDir shortHome(QDir::temp().filePath(QStringLiteral("nrh-XXXXXX")));
#endif
    if (!shortHome.isValid()) { return 20; }
    qputenv("HOME", shortHome.path().toUtf8());
    qputenv("CFFIXED_USER_HOME", shortHome.path().toUtf8());
    QApplication app(argc, argv);
    CoreInit::initialize();
    TestStationHandoverRoundtrip tests;
    const int result = QTest::qExec(&tests, argc, argv);
    CoreInit::shutdown();
    return result;
}

#include "tst_station_handover_roundtrip.moc"
