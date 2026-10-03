#include "gui/StationServiceManager.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>

using namespace NereusSDR;

class StationServiceManagerTest : public QObject {
    Q_OBJECT
private slots:
    void missingConfigRefusesLaunch();
    void macLaunchAgentEscapesPathsAndKeepsServiceOwned();
    void linuxLingerFallbackAndDisable();
    void windowsTaskQuotesArgumentsAndDisablesLogin();
    void stopRefusesFailedStateQuery();
    void macReloadsIdleRegistration();
    void emptyDefaultProfileIsExplicit();
    void pendingStateIsNotStopped();
};

struct Fixture {
    QTemporaryDir temp;
    QString binary;
    QString profileDir;
    QStringList calls;
    bool lingerAllowed = false;
    bool alreadyLingered = false;
    bool active = false;
    bool enabled = false;
    bool taskExists = false;
    bool macLoaded = false;
    bool linuxPresent = false;
    bool queryFails = false;
    QString linuxState;
    QString windowsState;
    QString macState;

    Fixture()
    {
        binary = temp.filePath(QStringLiteral("bin with & space/nereusd.exe"));
        profileDir = temp.filePath(QStringLiteral("profile & $ space"));
        QDir().mkpath(QFileInfo(binary).absolutePath());
        QDir().mkpath(profileDir);
        QFile file(binary);
        if (!file.open(QIODevice::WriteOnly)) qFatal("fixture binary open failed");
        file.write("binary");
        file.close();
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    void addConfig()
    {
        QFile file(QDir(profileDir).filePath(QStringLiteral("station.conf")));
        if (!file.open(QIODevice::WriteOnly)) qFatal("fixture config open failed");
        file.write("[station]\n");
    }
    StationServiceOptions options(StationPlatform platform)
    {
        StationServiceOptions o;
        o.profile = QStringLiteral("Desk_1");
        o.profileDirectory = profileDir;
        o.homeDirectory = temp.filePath(QStringLiteral("home"));
        o.binaryPath = binary;
        o.userName = QStringLiteral("testuser");
        o.userId = QStringLiteral("1234");
        o.platform = platform;
        o.runner = [this](const QString& command, const QStringList& args) {
            calls << command + QLatin1Char(' ') + args.join(QLatin1Char('|'));
            if (queryFails && ((command == QStringLiteral("powershell.exe"))
                               || (command == QStringLiteral("launchctl") && args.contains(QStringLiteral("list")))
                               || (command == QStringLiteral("systemctl") && args.contains(QStringLiteral("show")))))
                return StationServiceCommandResult{1, QStringLiteral("query failed")};
            if (command == QStringLiteral("loginctl") && args.contains(QStringLiteral("show-user")))
                return StationServiceCommandResult{0, alreadyLingered ? QStringLiteral("yes\n") : QStringLiteral("no\n")};
            if (command == QStringLiteral("loginctl"))
                return StationServiceCommandResult{lingerAllowed ? 0 : 1, {}};
            if (command == QStringLiteral("systemctl") && args.contains(QStringLiteral("is-enabled")))
                return StationServiceCommandResult{enabled ? 0 : 1, {}};
            if (command == QStringLiteral("systemctl") && args.contains(QStringLiteral("enable"))) {
                enabled = true;
                return StationServiceCommandResult{0, {}};
            }
            if (command == QStringLiteral("systemctl") && args.contains(QStringLiteral("disable"))) {
                enabled = false;
                return StationServiceCommandResult{0, {}};
            }
            if (command == QStringLiteral("systemctl") && args.contains(QStringLiteral("show")))
                return StationServiceCommandResult{0,
                    linuxPresent ? QStringLiteral("LoadState=loaded\nActiveState=%1\n")
                                        .arg(linuxState.isEmpty()
                                             ? (active ? QStringLiteral("active") : QStringLiteral("inactive"))
                                             : linuxState)
                                 : QStringLiteral("LoadState=not-found\nActiveState=inactive\n")};
            if (command == QStringLiteral("systemctl") && args.contains(QStringLiteral("start"))) {
                active = true;
                linuxPresent = true;
            }
            if (command == QStringLiteral("systemctl") && args.contains(QStringLiteral("stop"))) active = false;
            if (command == QStringLiteral("launchctl") && args.contains(QStringLiteral("list")))
                return StationServiceCommandResult{0, macLoaded
                    ? QStringLiteral("PID\tStatus\tLabel\n-\t0\tcom.boydsoftprez.NereusSDR.station\n")
                    : QStringLiteral("PID\tStatus\tLabel\n")};
            if (command == QStringLiteral("launchctl") && args.contains(QStringLiteral("print")))
                return StationServiceCommandResult{0, QStringLiteral("state = ")
                    + (macState.isEmpty() ? (active ? QStringLiteral("running") : QStringLiteral("waiting"))
                                          : macState)};
            if (command == QStringLiteral("launchctl") && args.contains(QStringLiteral("bootstrap"))) macLoaded = true;
            if (command == QStringLiteral("launchctl") && args.contains(QStringLiteral("bootout"))) {
                macLoaded = false;
                active = false;
            }
            if (command == QStringLiteral("launchctl") && args.contains(QStringLiteral("kickstart"))) active = true;
            if (command == QStringLiteral("powershell.exe"))
                return StationServiceCommandResult{0, taskExists ? (windowsState.isEmpty()
                    ? (active ? QStringLiteral("4\n") : QStringLiteral("3\n")) : windowsState)
                                                               : QStringLiteral("-1\n")};
            if (command == QStringLiteral("schtasks") && args.contains(QStringLiteral("/Create")))
                taskExists = true;
            if (command == QStringLiteral("schtasks") && args.contains(QStringLiteral("/Run")))
                active = true;
            if (command == QStringLiteral("schtasks") && args.contains(QStringLiteral("/End")))
                active = false;
            if (command == QStringLiteral("schtasks") && args.contains(QStringLiteral("/Delete"))) {
                taskExists = false;
                active = false;
            }
            return StationServiceCommandResult{0, {}};
        };
        return o;
    }
};

void StationServiceManagerTest::missingConfigRefusesLaunch()
{
    Fixture f;
    StationServiceManager manager(f.options(StationPlatform::MacOS));
    QVERIFY(!manager.startBackground());
    QVERIFY(manager.lastError().contains(QStringLiteral("settings file")));
    QVERIFY(!f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("bootstrap")));
    QVERIFY(!QFileInfo::exists(manager.entryPath()));
}

void StationServiceManagerTest::macLaunchAgentEscapesPathsAndKeepsServiceOwned()
{
    Fixture f;
    f.addConfig();
    StationServiceManager manager(f.options(StationPlatform::MacOS));
    QVERIFY(manager.setStartWithComputer(true));
    QCOMPARE(manager.startupMode(), StationServiceManager::StartupMode::Login);
    QVERIFY(manager.startsWithComputer());
    QVERIFY(manager.startBackground());
    QFile file(manager.entryPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray xml = file.readAll();
    QVERIFY(xml.contains("bin with &amp; space"));
    QVERIFY(xml.contains("profile &amp; $ space/station.conf"));
    QVERIFY(xml.contains("<key>RunAtLoad</key><true/>"));
    QXmlStreamReader macXml(xml);
    while (!macXml.atEnd()) macXml.readNext();
    QVERIFY(!macXml.hasError());
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("launchctl bootstrap|gui/1234")));
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("launchctl kickstart|gui/1234/")));
    QVERIFY(manager.setStartWithComputer(false));
    QVERIFY(!manager.startsWithComputer());
    QCOMPARE(manager.startupMode(), StationServiceManager::StartupMode::Disabled);
    QVERIFY(!QFileInfo::exists(manager.entryPath()));
}

void StationServiceManagerTest::linuxLingerFallbackAndDisable()
{
    Fixture f;
    f.addConfig();
    auto options = f.options(StationPlatform::Linux);
    options.prefixArguments = {QStringLiteral("--nereus-station")};
    StationServiceManager manager(options);
    QVERIFY(manager.setStartWithComputer(true));
    QCOMPARE(manager.startupMode(), StationServiceManager::StartupMode::Login);
    QVERIFY(manager.lastError().contains(QStringLiteral("login")));
    QVERIFY(manager.startBackground());
    QFile file(manager.entryPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray unit = file.readAll();
    QVERIFY(unit.contains("ExecStart=\""));
    QVERIFY(unit.contains("\"--nereus-station\""));
    QVERIFY(unit.contains("\"--profile\" \"Desk_1\""));
    QVERIFY(unit.contains("profile & $$ space/station.conf"));
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("systemctl --user|start|nereusd.service")));
    f.lingerAllowed = true;
    QVERIFY(manager.setStartWithComputer(true));
    QCOMPARE(manager.startupMode(), StationServiceManager::StartupMode::Boot);
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("loginctl --no-ask-password|enable-linger|testuser")));
    f.alreadyLingered = true;
    f.lingerAllowed = false;
    const qsizetype priorCalls = f.calls.size();
    QVERIFY(manager.setStartWithComputer(true));
    QCOMPARE(manager.startupMode(), StationServiceManager::StartupMode::Boot);
    QVERIFY(!f.calls.mid(priorCalls).join(QLatin1Char('\n')).contains(QStringLiteral("enable-linger")));
    QVERIFY(manager.setStartWithComputer(false));
    QCOMPARE(manager.startupMode(), StationServiceManager::StartupMode::Disabled);
    QVERIFY(!QFileInfo::exists(manager.entryPath()));
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("systemctl --user|disable|nereusd.service")));
}

void StationServiceManagerTest::windowsTaskQuotesArgumentsAndDisablesLogin()
{
    Fixture f;
    f.addConfig();
    StationServiceManager manager(f.options(StationPlatform::Windows));
    QVERIFY(manager.setStartWithComputer(true));
    QVERIFY(manager.startsWithComputer());
    QVERIFY(manager.startBackground());
    QFile file(manager.entryPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray xml = file.readAll();
    QVERIFY(xml.contains("encoding=\"UTF-8\""));
    QVERIFY(xml.contains("<Command>"));
    QVERIFY(xml.contains("&quot;Desk_1&quot;"));
    QVERIFY(xml.contains("profile &amp; $ space/station.conf"));
    QXmlStreamReader taskXml(xml);
    while (!taskXml.atEnd()) taskXml.readNext();
    QVERIFY(!taskXml.hasError());
    QXmlStreamReader semanticXml(xml);
    QString timeLimit;
    while (!semanticXml.atEnd()) {
        semanticXml.readNext();
        if (semanticXml.isStartElement() && semanticXml.name() == QStringLiteral("ExecutionTimeLimit"))
            timeLimit = semanticXml.readElementText();
    }
    QCOMPARE(timeLimit, QStringLiteral("PT0S"));
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("schtasks /Create|/F|/TN|NereusSDR Station|/XML|")));
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("schtasks /Run|/TN|NereusSDR Station")));
    const qsizetype priorStartCalls = f.calls.size();
    QVERIFY(manager.startBackground());
    QVERIFY(!f.calls.mid(priorStartCalls).join(QLatin1Char('\n')).contains(QStringLiteral("/Run")));
    QVERIFY(manager.setStartWithComputer(false));
    QVERIFY(!manager.startsWithComputer());
    QVERIFY(QFileInfo::exists(manager.entryPath()));
    QVERIFY(f.taskExists);
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("schtasks /Create|/F|/TN|NereusSDR Station|/XML|")));
    QVERIFY(manager.stopBackground());
    QVERIFY(f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("powershell.exe -NoProfile|-NonInteractive|-Command|")));
    QVERIFY(!f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("Status: Running")));
    QVERIFY(!f.active);
    QVERIFY(manager.stopBackground());
}

void StationServiceManagerTest::stopRefusesFailedStateQuery()
{
    for (const auto platform : {StationPlatform::MacOS, StationPlatform::Linux, StationPlatform::Windows}) {
        Fixture f;
        f.queryFails = true;
        f.active = true;
        f.macLoaded = true;
        f.linuxPresent = true;
        f.taskExists = true;
        StationServiceManager manager(f.options(platform));
        QVERIFY(!manager.stopBackground());
        QVERIFY(manager.lastError().contains(QStringLiteral("check")));
        const QString calls = f.calls.join(QLatin1Char('\n'));
        QVERIFY(!calls.contains(QStringLiteral("bootout")));
        QVERIFY(!calls.contains(QStringLiteral("|stop|")));
        QVERIFY(!calls.contains(QStringLiteral("/End")));
    }
}

void StationServiceManagerTest::macReloadsIdleRegistration()
{
    Fixture f;
    f.addConfig();
    f.macLoaded = true;
    StationServiceManager manager(f.options(StationPlatform::MacOS));
    QVERIFY(manager.startBackground());
    const QString calls = f.calls.join(QLatin1Char('\n'));
    const qsizetype bootout = calls.indexOf(QStringLiteral("launchctl bootout|"));
    const qsizetype bootstrap = calls.indexOf(QStringLiteral("launchctl bootstrap|"));
    const qsizetype kickstart = calls.indexOf(QStringLiteral("launchctl kickstart|"));
    QVERIFY(bootout >= 0);
    QVERIFY(bootstrap > bootout);
    QVERIFY(kickstart > bootstrap);
    const qsizetype previous = f.calls.size();
    QVERIFY(manager.startBackground());
    QVERIFY(!f.calls.mid(previous).join(QLatin1Char('\n')).contains(QStringLiteral("bootout")));
}

void StationServiceManagerTest::emptyDefaultProfileIsExplicit()
{
    Fixture f;
    f.addConfig();
    auto options = f.options(StationPlatform::Linux);
    options.inheritActiveProfile = false;
    options.profile.clear();
    StationServiceManager manager(options);
    QVERIFY(manager.startBackground());
    QFile file(manager.entryPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().contains("\"--profile\" \"\""));
}

void StationServiceManagerTest::pendingStateIsNotStopped()
{
    for (const QString& state : {QStringLiteral("activating"), QStringLiteral("deactivating")}) {
        Fixture f;
        f.addConfig();
        f.linuxPresent = true;
        f.linuxState = state;
        StationServiceManager manager(f.options(StationPlatform::Linux));
        QVERIFY(!manager.stopBackground());
        QVERIFY(manager.lastError().contains(QStringLiteral("changing state")));
        QVERIFY(!manager.startBackground());
        QVERIFY(!f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("|start|")));
        QVERIFY(!f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("|stop|")));
    }
    {
        Fixture f;
        f.addConfig();
        f.taskExists = true;
        f.windowsState = QStringLiteral("2\n"); // TASK_STATE_QUEUED
        StationServiceManager manager(f.options(StationPlatform::Windows));
        QVERIFY(!manager.stopBackground());
        QVERIFY(!manager.startBackground());
        QVERIFY(manager.lastError().contains(QStringLiteral("changing state")));
        QVERIFY(!f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("/End")));
        QVERIFY(!f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("/Create")));
    }
    {
        Fixture f;
        f.addConfig();
        f.macLoaded = true;
        f.macState = QStringLiteral("spawning");
        StationServiceManager manager(f.options(StationPlatform::MacOS));
        QVERIFY(!manager.stopBackground());
        QVERIFY(!manager.startBackground());
        QVERIFY(!f.calls.join(QLatin1Char('\n')).contains(QStringLiteral("bootout")));
    }
}

QTEST_GUILESS_MAIN(StationServiceManagerTest)
#include "tst_station_service_manager.moc"
