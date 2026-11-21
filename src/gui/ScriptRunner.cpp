// no-port-check: NereusSDR-original start/exit script runner.
#include "ScriptRunner.h"
#include "HelperApps.h"
#include "core/AppSettings.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace NereusSDR {
namespace {

void hideProcess(QProcess& process)
{
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
        if (arguments->startupInfo) {
            arguments->startupInfo->wShowWindow = SW_HIDE;
        }
    });
#else
    Q_UNUSED(process)
#endif
}

// Splits "C:\path\script.bat arg1 arg2" at the first script extension, as
// Thetis does (RunScriptEntry).
bool splitEntry(const QString& entry, QString* file, QString* arguments)
{
    const QString trimmed = entry.trimmed();
    const QString lower = trimmed.toLower();
    int position = -1;
    for (const QString& extension : {QStringLiteral(".exe"), QStringLiteral(".cmd"), QStringLiteral(".bat")}) {
        const int found = lower.indexOf(extension);
        if (found >= 0 && (position < 0 || found < position)) { position = found; }
    }
    if (position < 0) { return false; }
    if (file) { *file = trimmed.left(position + 4).trimmed(); }
    if (arguments) { *arguments = trimmed.mid(position + 4).trimmed(); }
    return true;
}

} // namespace

ScriptRunner& ScriptRunner::instance()
{
    static ScriptRunner* runner = new ScriptRunner(QCoreApplication::instance());
    return *runner;
}

ScriptRunner::ScriptRunner(QObject* parent) : QObject(parent) {}

QString ScriptRunner::scriptsDirectory()
{
    return HelperApps::installRoot() + QStringLiteral("/SDR Scripts");
}

QString ScriptRunner::defaultStartScript()
{
    return scriptsDirectory() + QStringLiteral("/nereus-start.bat");
}

QString ScriptRunner::defaultStopScript()
{
    return scriptsDirectory() + QStringLiteral("/nereus-stop.bat");
}

bool ScriptRunner::seed(QString* error)
{
    const QString source = QCoreApplication::applicationDirPath() + QStringLiteral("/SDR Scripts");
    const QString destination = scriptsDirectory();
    if (!QFileInfo::exists(source)) {
        if (error) { *error = QStringLiteral("The shipped SDR Scripts folder is missing."); }
        return false;
    }
    if (!QDir().mkpath(destination)) {
        if (error) { *error = QStringLiteral("Could not create %1.").arg(destination); }
        return false;
    }
    QDirIterator files(source, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString from = files.next();
        const QString relative = QDir(source).relativeFilePath(from);
        // The bundled JRE stays in the install folder. Copying ~130 MB per
        // user is wasteful; jre-home.txt points the seeded scripts at it.
        const QString relativePosix = QDir::fromNativeSeparators(relative);
        if (relativePosix == QLatin1String("jre") || relativePosix.startsWith(QLatin1String("jre/"))) {
            continue;
        }
        const QString to = destination + QLatin1Char('/') + relative;
        QDir().mkpath(QFileInfo(to).absolutePath());
        // Never overwrite: a user-edited start/stop script is kept.
        if (!QFileInfo::exists(to) && !QFile::copy(from, to)) {
            if (error) { *error = QStringLiteral("Could not install %1.").arg(relative); }
            return false;
        }
    }
    const QString jre = source + QStringLiteral("/jre");
    if (QFileInfo::exists(jre + QStringLiteral("/bin/java.exe"))) {
        QFile marker(destination + QStringLiteral("/jre-home.txt"));
        if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            marker.write(QDir::toNativeSeparators(jre).toUtf8());
        }
    }
    return true;
}

bool ScriptRunner::startEnabled()
{
    return AppSettings::instance().value(QStringLiteral("Scripts/StartEnabled"), false).toBool();
}

bool ScriptRunner::stopEnabled()
{
    return AppSettings::instance().value(QStringLiteral("Scripts/StopEnabled"), false).toBool();
}

QString ScriptRunner::startScriptPath()
{
    const QString saved = AppSettings::instance().value(QStringLiteral("Scripts/StartPath")).toString().trimmed();
    return saved.isEmpty() ? defaultStartScript() : saved;
}

QString ScriptRunner::stopScriptPath()
{
    const QString saved = AppSettings::instance().value(QStringLiteral("Scripts/StopPath")).toString().trimmed();
    return saved.isEmpty() ? defaultStopScript() : saved;
}

int ScriptRunner::powerOnDelaySeconds()
{
    const int seconds = AppSettings::instance().value(QStringLiteral("Scripts/PowerOnDelay"), 2).toInt();
    return qBound(0, seconds, 300);
}

bool ScriptRunner::removeInstalled(QString* error)
{
    const QString directory = scriptsDirectory();
    if (!QFileInfo::exists(directory)) { return true; }
    if (QDir(directory).removeRecursively()) { return true; }
    if (error) { *error = QStringLiteral("Could not remove %1.").arg(directory); }
    return false;
}

bool ScriptRunner::launchStartScript()
{
    QString file;
    QString arguments;
    if (!splitEntry(startScriptPath(), &file, &arguments) || !QFileInfo::exists(file)) {
        return false;
    }
    m_startScript = new QProcess(this);
    hideProcess(*m_startScript);
    m_startScript->setWorkingDirectory(QFileInfo(file).absolutePath());
    const QString suffix = QFileInfo(file).suffix().toLower();
    if (suffix == QStringLiteral("bat") || suffix == QStringLiteral("cmd")) {
        QStringList command{ QStringLiteral("/c"), QDir::toNativeSeparators(file) };
        if (!arguments.isEmpty()) { command << arguments; }
        m_startScript->start(QStringLiteral("cmd.exe"), command);
    } else {
        QStringList command;
        if (!arguments.isEmpty()) { command << arguments; }
        m_startScript->start(file, command);
    }
    m_startLaunched = true;
    connect(m_startScript, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        notifyStartScriptFinished();
    });
    connect(m_startScript, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        notifyStartScriptFinished();
    });
    // A hung script cannot hold auto-connect forever (Thetis gives up at 60 s).
    QTimer::singleShot(60000, this, [this] { notifyStartScriptFinished(); });
    return true;
}

void ScriptRunner::notifyStartScriptFinished()
{
    if (m_finishedNotified) { return; }
    m_finishedNotified = true;
    emit startScriptFinished();
}

void ScriptRunner::runStartScript()
{
    if (!startEnabled() || m_startLaunched) { return; }
    // The files are installed only for users who select a script.
    if (!QFileInfo::exists(startScriptPath())) { seed(); }
    launchStartScript();
}

void ScriptRunner::runStopScript()
{
    if (!stopEnabled()) { return; }
    if (!QFileInfo::exists(stopScriptPath())) { seed(); }
    QString file;
    QString arguments;
    if (!splitEntry(stopScriptPath(), &file, &arguments) || !QFileInfo::exists(file)) {
        return;
    }
    QProcess process;
    hideProcess(process);
    process.setWorkingDirectory(QFileInfo(file).absolutePath());
    const QString suffix = QFileInfo(file).suffix().toLower();
    if (suffix == QStringLiteral("bat") || suffix == QStringLiteral("cmd")) {
        QStringList command{ QStringLiteral("/c"), QDir::toNativeSeparators(file) };
        if (!arguments.isEmpty()) { command << arguments; }
        process.start(QStringLiteral("cmd.exe"), command);
    } else {
        QStringList command;
        if (!arguments.isEmpty()) { command << arguments; }
        process.start(file, command);
    }
    // Wait for the relay-off script, as Thetis does (10 s).
    if (process.waitForStarted(5000)) {
        process.waitForFinished(10000);
    }
}

bool ScriptRunner::holdAutoConnect(QObject* context, std::function<void()> callback)
{
    if (!context || !startEnabled()) { return false; }
    QString file;
    QString arguments;
    if (!splitEntry(startScriptPath(), &file, &arguments) || !QFileInfo::exists(file)) {
        return false;
    }
    if (!m_startLaunched) { runStartScript(); }
    const int delayMs = powerOnDelaySeconds() * 1000;
    const bool running = m_startScript && m_startScript->state() != QProcess::NotRunning;
    if (running) {
        connect(this, &ScriptRunner::startScriptFinished, context,
                [context, callback, delayMs] {
                    QTimer::singleShot(delayMs, context, callback);
                }, Qt::SingleShotConnection);
    } else {
        QTimer::singleShot(delayMs, context, callback);
    }
    return true;
}

} // namespace NereusSDR
