// no-port-check: NereusSDR-original. iPhone app plan Task 47.
#include "gui/StationServiceManager.h"

#include "core/AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QXmlStreamReader>

#include <algorithm>

#if defined(Q_OS_MAC)
#include <unistd.h>
#endif

namespace NereusSDR {
namespace {

constexpr auto kLabel = "com.boydsoftprez.NereusSDR.station";
constexpr auto kWindowsTask = "NereusSDR Station";
// The script contains no user-controlled text. Task Scheduler's numeric
// TASK_STATE values are independent of the Windows display language.
constexpr auto kWindowsStateScript =
    "$ErrorActionPreference='Stop'; "
    "try { $s=New-Object -ComObject 'Schedule.Service'; $s.Connect(); "
    "$t=$s.GetFolder('\\').GetTask('NereusSDR Station'); "
    "[Console]::Out.WriteLine([int]$t.State) } "
    "catch { $e=$_.Exception; while ($e.InnerException) { $e=$e.InnerException }; "
    "if ($e.HResult -eq -2147024894) { [Console]::Out.WriteLine(-1) } "
    "else { exit 1 } }";

StationServiceCommandResult runProcess(const QString& program, const QStringList& args)
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(5000)) {
        return {-1, process.errorString()};
    }
    if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished();
        return {-1, QStringLiteral("service command timed out")};
    }
    return {process.exitCode(), QString::fromLocal8Bit(process.readAllStandardOutput())
                                    + QString::fromLocal8Bit(process.readAllStandardError())};
}

QString xml(const QString& value)
{
    return value.toHtmlEscaped();
}

QString systemdArgument(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('%'), QStringLiteral("%%"));
    escaped.replace(QLatin1Char('$'), QStringLiteral("$$"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// Windows' CreateProcess argument quoting, kept inside Task Scheduler XML's
// Arguments field; no cmd.exe or shell expansion participates.
QString windowsArgument(const QString& value)
{
    QString quoted = QStringLiteral("\"");
    int backslashes = 0;
    for (const QChar ch : value) {
        if (ch == QLatin1Char('\\')) {
            ++backslashes;
        } else if (ch == QLatin1Char('"')) {
            quoted += QString(backslashes * 2 + 1, QLatin1Char('\\')) + ch;
            backslashes = 0;
        } else {
            quoted += QString(backslashes, QLatin1Char('\\')) + ch;
            backslashes = 0;
        }
    }
    quoted += QString(backslashes * 2, QLatin1Char('\\')) + QLatin1Char('"');
    return quoted;
}

bool hasLineBreak(const QString& value)
{
    return std::any_of(value.begin(), value.end(), [](QChar ch) {
        return ch.unicode() < 0x20 || ch.unicode() == 0x7f;
    });
}

bool xmlToggle(const QByteArray& bytes, const QString& key)
{
    QXmlStreamReader reader(bytes);
    bool pending = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("key")) {
            pending = reader.readElementText() == key;
        } else if (pending && reader.isStartElement()) {
            return reader.name() == QStringLiteral("true");
        }
    }
    return false;
}

} // namespace

StationServiceManager::StationServiceManager(StationServiceOptions options, QObject* parent)
    : QObject(parent)
    , m_options(std::move(options))
{
    if (m_options.inheritActiveProfile && m_options.profile.isEmpty()) {
        m_options.profile = AppSettings::profileOverride();
    }
    if (m_options.profileDirectory.isEmpty()) {
        m_options.profileDirectory = AppSettings::resolveConfigDir(m_options.profile);
    }
    if (m_options.homeDirectory.isEmpty()) {
        m_options.homeDirectory = QDir::homePath();
    }
    if (m_options.binaryPath.isEmpty()) {
        const StationLaunchSpec launch = locateStationLaunch();
        m_options.binaryPath = launch.program;
        m_options.prefixArguments = launch.prefixArguments;
        m_locatorError = launch.error;
    }
    if (m_options.userName.isEmpty()) {
        m_options.userName = qEnvironmentVariable("USER");
    }
#if defined(Q_OS_MAC)
    if (m_options.userId.isEmpty()) {
        m_options.userId = QString::number(getuid());
    }
#endif
    if (!m_options.runner) {
        m_options.runner = runProcess;
    }
}

QString StationServiceManager::entryPath() const
{
    const QDir home(m_options.homeDirectory);
    switch (m_options.platform) {
    case StationPlatform::MacOS:
        return home.filePath(QStringLiteral("Library/LaunchAgents/") + QString::fromLatin1(kLabel)
                             + QStringLiteral(".plist"));
    case StationPlatform::Linux:
        return home.filePath(QStringLiteral(".config/systemd/user/nereusd.service"));
    case StationPlatform::Windows:
        return home.filePath(QStringLiteral("AppData/Local/NereusSDR/nereusd-task.xml"));
    }
    return {};
}

QString StationServiceManager::configPath() const
{
    return QDir(m_options.profileDirectory).filePath(QStringLiteral("station.conf"));
}

QString StationServiceManager::serviceName() const
{
    return m_options.platform == StationPlatform::Windows ? QString::fromLatin1(kWindowsTask)
                                                          : QString::fromLatin1(kLabel);
}

void StationServiceManager::fail(const QString& error)
{
    m_lastError = error;
    m_startupMode = StartupMode::Failed;
}

bool StationServiceManager::run(const QString& program, const QStringList& args,
                                QString* output) const
{
    const StationServiceCommandResult result = m_options.runner(program, args);
    if (output) {
        *output = result.output;
    }
    return result.exitCode == 0;
}

bool StationServiceManager::validateLaunch()
{
    if (m_options.binaryPath.isEmpty()) {
        fail(m_locatorError.isEmpty() ? QStringLiteral("The packaged Core app is missing. Reinstall NereusSDR.")
                                      : m_locatorError);
        return false;
    }
    if ((!m_options.profile.isEmpty() && !AppSettings::isValidProfileName(m_options.profile))
        || !QDir::isAbsolutePath(m_options.profileDirectory)
        || !QDir::isAbsolutePath(m_options.homeDirectory)
        || !QDir::isAbsolutePath(m_options.binaryPath)
        || hasLineBreak(m_options.profileDirectory) || hasLineBreak(m_options.homeDirectory)
        || hasLineBreak(m_options.binaryPath)
        || std::any_of(m_options.prefixArguments.begin(), m_options.prefixArguments.end(), hasLineBreak)) {
        fail(QStringLiteral("Choose a valid Core profile and installed Core app."));
        return false;
    }
    if (!QFileInfo(m_options.binaryPath).isFile()
        || !QFileInfo(m_options.binaryPath).isExecutable()) {
        fail(QStringLiteral("The packaged Core app is missing. Reinstall NereusSDR."));
        return false;
    }
    if (!QFileInfo(configPath()).isFile()) {
        fail(QStringLiteral("The Core settings file is missing. Set up this Core before starting it."));
        return false;
    }
    return true;
}

bool StationServiceManager::writeEntry(bool startAtLogin)
{
    const QFileInfo entry(entryPath());
    if (!QDir().mkpath(entry.absolutePath())) {
        fail(QStringLiteral("Could not create the Core startup folder."));
        return false;
    }
    QString content;
    if (m_options.platform == StationPlatform::MacOS) {
        QString prefixXml;
        for (const QString& arg : m_options.prefixArguments) {
            prefixXml += QStringLiteral("<string>%1</string>").arg(xml(arg));
        }
        content = QStringLiteral(
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                      "<plist version=\"1.0\"><dict>"
                      "<key>Label</key><string>%1</string>"
                      "<key>ProgramArguments</key><array>"
                      "<string>%2</string>%3<string>--profile</string><string>%4</string>"
                      "<string>--config</string><string>%5</string></array>"
                      "<key>RunAtLoad</key>%6"
                      "<key>KeepAlive</key><false/>"
                      "</dict></plist>\n")
                      .arg(xml(serviceName()), xml(m_options.binaryPath), prefixXml,
                           xml(m_options.profile), xml(configPath()),
                           startAtLogin ? QStringLiteral("<true/>") : QStringLiteral("<false/>"));
    } else if (m_options.platform == StationPlatform::Linux) {
        QStringList arguments = m_options.prefixArguments;
        arguments << QStringLiteral("--profile") << m_options.profile
                  << QStringLiteral("--config") << configPath();
        QStringList escapedArguments;
        for (const QString& argument : arguments) {
            escapedArguments << systemdArgument(argument);
        }
        content = QStringLiteral("[Unit]\nDescription=NereusSDR background station\n"
                                 "After=network-online.target\nWants=network-online.target\n\n"
                                 "[Service]\nType=exec\nExecStart=%1 %2\n"
                                 "Restart=on-failure\nRestartSec=5\n\n"
                                 "[Install]\nWantedBy=default.target\n")
                      .arg(systemdArgument(m_options.binaryPath), escapedArguments.join(QLatin1Char(' ')));
    } else {
        QStringList arguments = m_options.prefixArguments;
        arguments << QStringLiteral("--profile") << m_options.profile
                  << QStringLiteral("--config") << configPath();
        QStringList quotedArguments;
        for (const QString& argument : arguments) {
            quotedArguments << windowsArgument(argument);
        }
        content = QStringLiteral(
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<Task version=\"1.4\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
                      "<Triggers><LogonTrigger><Enabled>%1</Enabled></LogonTrigger></Triggers>"
                      "<Principals><Principal id=\"Author\"><LogonType>InteractiveToken</LogonType>"
                      "<RunLevel>LeastPrivilege</RunLevel></Principal></Principals>"
                      "<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
                      "<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"
                      "<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
                      "<AllowHardTerminate>true</AllowHardTerminate>"
                      "<StartWhenAvailable>true</StartWhenAvailable>"
                      "<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>"
                      "<Enabled>true</Enabled><AllowStartOnDemand>true</AllowStartOnDemand>"
                      "</Settings><Actions Context=\"Author\"><Exec><Command>%2</Command>"
                      "<Arguments>%3</Arguments></Exec></Actions></Task>\n")
                      .arg(startAtLogin ? QStringLiteral("true") : QStringLiteral("false"),
                           xml(m_options.binaryPath),
                           xml(quotedArguments.join(QLatin1Char(' '))));
    }
    QSaveFile file(entryPath());
    const QByteArray bytes = content.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        fail(QStringLiteral("Could not save the Core startup entry."));
        return false;
    }
    if (m_options.platform != StationPlatform::Windows
        && !QFile::setPermissions(entryPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        fail(QStringLiteral("Could not protect the Core startup entry."));
        return false;
    }
    return true;
}

StationServiceManager::ServiceState StationServiceManager::probeState() const
{
    QString output;
    if (m_options.platform == StationPlatform::MacOS) {
        if (!run(QStringLiteral("launchctl"), {QStringLiteral("list")}, &output)) {
            return ServiceState::Error;
        }
        bool found = false;
        for (const QString& line : output.split(QLatin1Char('\n'))) {
            const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                                  Qt::SkipEmptyParts);
            if (!fields.isEmpty() && fields.last() == serviceName()) {
                found = true;
                break;
            }
        }
        if (!found) return ServiceState::Absent;
        if (!run(QStringLiteral("launchctl"),
                 {QStringLiteral("print"), QStringLiteral("gui/%1/%2")
                                               .arg(m_options.userId, serviceName())}, &output)) {
            return ServiceState::Error;
        }
        if (output.contains(QStringLiteral("state = running"))) return ServiceState::Running;
        if (output.contains(QStringLiteral("state = waiting"))) return ServiceState::Stopped;
        if (output.contains(QStringLiteral("state = "))) return ServiceState::Pending;
        return ServiceState::Error;
    }
    if (m_options.platform == StationPlatform::Linux) {
        if (!run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("show"),
                                                 QStringLiteral("nereusd.service"),
                                                 QStringLiteral("--property=LoadState"),
                                                 QStringLiteral("--property=ActiveState"),
                                                 QStringLiteral("--no-pager")}, &output)) {
            return ServiceState::Error;
        }
        if (output.contains(QStringLiteral("LoadState=not-found"))) return ServiceState::Absent;
        if (!output.contains(QStringLiteral("LoadState=loaded"))) return ServiceState::Error;
        if (output.contains(QStringLiteral("ActiveState=active"))) return ServiceState::Running;
        if (output.contains(QStringLiteral("ActiveState=inactive"))
            || output.contains(QStringLiteral("ActiveState=failed"))) return ServiceState::Stopped;
        if (output.contains(QStringLiteral("ActiveState=activating"))
            || output.contains(QStringLiteral("ActiveState=deactivating"))
            || output.contains(QStringLiteral("ActiveState=reloading"))) return ServiceState::Pending;
        return ServiceState::Error;
    }
    if (!run(QStringLiteral("powershell.exe"),
             {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
              QStringLiteral("-Command"), QString::fromLatin1(kWindowsStateScript)}, &output)) {
        return ServiceState::Error;
    }
    const QString state = output.trimmed();
    if (state == QStringLiteral("-1")) return ServiceState::Absent;
    if (state == QStringLiteral("4")) return ServiceState::Running;
    if (state == QStringLiteral("2")) return ServiceState::Pending;
    if (state == QStringLiteral("1") || state == QStringLiteral("3")) return ServiceState::Stopped;
    return ServiceState::Error;
}

bool StationServiceManager::isBackgroundRunning() const
{
    return probeState() == ServiceState::Running;
}

bool StationServiceManager::startBackground()
{
    m_lastError.clear();
    const ServiceState initial = probeState();
    if (initial == ServiceState::Running) {
        return true;
    }
    if (initial == ServiceState::Error) {
        fail(QStringLiteral("Could not check the background Core state."));
        return false;
    }
    if (initial == ServiceState::Pending) {
        fail(QStringLiteral("The background Core is changing state. Try again shortly."));
        return false;
    }
    if (!validateLaunch()) {
        return false;
    }
    const bool atLogin = startsWithComputer();
    if (!writeEntry(atLogin)) {
        return false;
    }
    if (m_options.platform == StationPlatform::MacOS) {
        const QString domain = QStringLiteral("gui/%1").arg(m_options.userId);
        // launchd does not reload ProgramArguments on kickstart. Replace an
        // idle registration so a changed binary/profile takes effect.
        const ServiceState current = probeState();
        if (current == ServiceState::Error) {
            fail(QStringLiteral("Could not check the background Core state."));
            return false;
        }
        if (current == ServiceState::Pending) {
            fail(QStringLiteral("The background Core is changing state. Try again shortly."));
            return false;
        }
        if (current == ServiceState::Running) return true;
        if (current == ServiceState::Stopped
            && !run(QStringLiteral("launchctl"),
                    {QStringLiteral("bootout"), domain + QLatin1Char('/') + serviceName()})) {
            fail(QStringLiteral("Could not reload the background Core on macOS."));
            return false;
        }
        if (!run(QStringLiteral("launchctl"),
                 {QStringLiteral("bootstrap"), domain, entryPath()})) {
            fail(QStringLiteral("Could not register the background Core with macOS."));
            return false;
        }
        if (!run(QStringLiteral("launchctl"),
                 {QStringLiteral("kickstart"), domain + QLatin1Char('/') + serviceName()})) {
            fail(QStringLiteral("Could not start the background Core on macOS."));
            return false;
        }
    } else if (m_options.platform == StationPlatform::Linux) {
        if (!run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("daemon-reload")})
            || !run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("start"),
                                                   QStringLiteral("nereusd.service")})) {
            fail(QStringLiteral("Could not start the background Core in this login."));
            return false;
        }
    } else {
        if (!run(QStringLiteral("schtasks"), {QStringLiteral("/Create"), QStringLiteral("/F"),
                                              QStringLiteral("/TN"), serviceName(),
                                              QStringLiteral("/XML"), entryPath()})
            || !run(QStringLiteral("schtasks"), {QStringLiteral("/Run"), QStringLiteral("/TN"),
                                                  serviceName()})) {
            fail(QStringLiteral("Could not start the background Core at this login."));
            return false;
        }
    }
    return true;
}

bool StationServiceManager::stopBackground()
{
    m_lastError.clear();
    const ServiceState state = probeState();
    if (state == ServiceState::Error) {
        fail(QStringLiteral("Could not check the background Core state."));
        return false;
    }
    if (state == ServiceState::Pending) {
        fail(QStringLiteral("The background Core is changing state. Try again shortly."));
        return false;
    }
    if (state != ServiceState::Running) {
        return true;
    }
    bool stopped = false;
    if (m_options.platform == StationPlatform::MacOS) {
        stopped = run(QStringLiteral("launchctl"),
                      {QStringLiteral("bootout"), QStringLiteral("gui/%1/%2")
                                                        .arg(m_options.userId, serviceName())});
    } else if (m_options.platform == StationPlatform::Linux) {
        stopped = run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("stop"),
                                                     QStringLiteral("nereusd.service")});
    } else {
        stopped = run(QStringLiteral("schtasks"), {QStringLiteral("/End"), QStringLiteral("/TN"),
                                                   serviceName()});
    }
    if (!stopped) {
        fail(QStringLiteral("Could not stop the background Core."));
    }
    return stopped;
}

bool StationServiceManager::setStartWithComputer(bool enabled)
{
    m_lastError.clear();
    if (!enabled) {
        if (m_options.platform == StationPlatform::Windows) {
            const ServiceState state = probeState();
            if (state == ServiceState::Error) {
                fail(QStringLiteral("Could not check the Core login task."));
                return false;
            }
            const bool taskExists = state != ServiceState::Absent;
            if (!taskExists && !QFileInfo::exists(entryPath())) {
                m_startupMode = StartupMode::Disabled;
                return true;
            }
            // Keep the on-demand task registered so /End can still stop a
            // running station; remove only its logon trigger.
            if (!writeEntry(false)
                || (taskExists && !run(QStringLiteral("schtasks"),
                                       {QStringLiteral("/Create"), QStringLiteral("/F"),
                                        QStringLiteral("/TN"), serviceName(),
                                        QStringLiteral("/XML"), entryPath()}))) {
                fail(QStringLiteral("Could not remove the Core login trigger."));
                return false;
            }
            m_startupMode = StartupMode::Disabled;
            return true;
        }
        const bool hadEntry = QFileInfo::exists(entryPath());
        const bool wasEnabled = m_options.platform == StationPlatform::Linux && startsWithComputer();
        if (!hadEntry && !wasEnabled) {
            m_startupMode = StartupMode::Disabled;
            return true;
        }
        if (wasEnabled
            && !run(QStringLiteral("systemctl"), {QStringLiteral("--user"),
                                                    QStringLiteral("disable"), QStringLiteral("nereusd.service")})) {
            fail(QStringLiteral("Could not disable the Core login unit."));
            return false;
        }
        if (QFileInfo::exists(entryPath()) && !QFile::remove(entryPath())) {
            fail(QStringLiteral("Could not remove the Core startup entry."));
            return false;
        }
        if (m_options.platform == StationPlatform::Linux
            && !run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("daemon-reload")})) {
            fail(QStringLiteral("Could not reload the Core login unit."));
            return false;
        }
        m_startupMode = StartupMode::Disabled;
        return true;
    }
    if (!validateLaunch() || !writeEntry(true)) {
        return false;
    }
    if (m_options.platform == StationPlatform::MacOS) {
        m_startupMode = StartupMode::Login;
        return true;
    }
    if (m_options.platform == StationPlatform::Linux) {
        if (!run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("daemon-reload")})
            || !run(QStringLiteral("systemctl"), {QStringLiteral("--user"),
                                                   QStringLiteral("enable"),
                                                   QStringLiteral("nereusd.service")})) {
            fail(QStringLiteral("Could not change background Core startup for this login."));
            return false;
        }
        QString linger;
        const bool alreadyLingered = !m_options.userName.isEmpty()
            && run(QStringLiteral("loginctl"), {QStringLiteral("show-user"),
                                                 QStringLiteral("--property=Linger"),
                                                 QStringLiteral("--value"), m_options.userName},
                   &linger)
            && linger.trimmed() == QStringLiteral("yes");
        if (alreadyLingered
            || (!m_options.userName.isEmpty()
                && run(QStringLiteral("loginctl"), {QStringLiteral("--no-ask-password"),
                                                     QStringLiteral("enable-linger"),
                                                     m_options.userName}))) {
            m_startupMode = StartupMode::Boot;
        } else {
            m_startupMode = StartupMode::Login;
            m_lastError = QStringLiteral("The Core starts at login; boot startup is not available for this user.");
        }
        return true;
    }
    if (!run(QStringLiteral("schtasks"), {QStringLiteral("/Create"), QStringLiteral("/F"),
                                          QStringLiteral("/TN"), serviceName(),
                                          QStringLiteral("/XML"), entryPath()})) {
        fail(QStringLiteral("Could not change the Core login task."));
        return false;
    }
    m_startupMode = StartupMode::Login;
    return true;
}

bool StationServiceManager::startsWithComputer() const
{
    if (m_options.platform == StationPlatform::Linux) {
        return run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("is-enabled"),
                                                 QStringLiteral("--quiet"), QStringLiteral("nereusd.service")});
    }
    QFile file(entryPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray content = file.readAll();
    if (m_options.platform == StationPlatform::MacOS) {
        return xmlToggle(content, QStringLiteral("RunAtLoad"));
    }
    return content.contains(QByteArrayLiteral("<LogonTrigger><Enabled>true</Enabled>"));
}

} // namespace NereusSDR
