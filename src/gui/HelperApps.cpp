// no-port-check: NereusSDR-original helper installation and supervision.
#include "HelperApps.h"
#include "core/AppSettings.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
#include <QCoreApplication>
#include <QHostAddress>
#include <QUdpSocket>
#include <QUuid>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace NereusSDR {
namespace {

constexpr auto kWaveNodeFolder = "WaveNodeUsb";
constexpr auto kWaveNodeExe = "WaveNodeUsb.exe";
constexpr auto kHfAutoFrontendFolder = "HF-Auto MMIO";
constexpr auto kHfAutoFrontendExe = "HFAuto_MMIO.exe";
constexpr auto kHfAutoControllerFolder = "HF-Auto Controller";
constexpr auto kHfAutoControllerExe = "HFAutoController.exe";
constexpr quint16 kHfAutoCommandPort = 12052;

QString localDataRoot()
{
    // AppConfigLocation is the folder that already holds NereusSDR.settings
    // (%LOCALAPPDATA%\NereusSDR on Windows). AppLocalDataLocation would add a
    // second NereusSDR segment because the organisation and application names
    // are the same.
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (root.isEmpty()) {
        root = QDir::homePath() + QStringLiteral("/AppData/Local/NereusSDR");
    }
    return root;
}

bool isBinary(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("exe") || suffix == QStringLiteral("dll");
}

// Seeds one shipped helper folder into the per-user helpers directory. The
// executable is required; other files (INI, licence, logs) are never
// overwritten once they exist. Binaries refresh when the shipped copy is newer.
bool installHelperFolder(const QString& folder, const QString& exe, QString* error)
{
    const QString source = QCoreApplication::applicationDirPath() + QLatin1Char('/') + folder;
    const QString destination = HelperApps::installRoot() + QLatin1Char('/') + folder;
    const QString sourceExe = source + QLatin1Char('/') + exe;
    if (!QFileInfo::exists(sourceExe)) {
        if (error) {
            *error = QStringLiteral("The shipped %1 folder is missing.").arg(folder);
        }
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
        const QString to = destination + QLatin1Char('/') + relative;
        QDir().mkpath(QFileInfo(to).absolutePath());
        const QFileInfo target(to);
        const bool copy = !target.exists()
            || (isBinary(to) && QFileInfo(from).lastModified() > target.lastModified());
        if (copy) {
            // QFile::copy() refuses an existing destination. Replacing a
            // helper binary is the normal update path: the installer rounds
            // its timestamps, so the shipped copy can look newer than the
            // per-user one even when the bytes are identical. start() has
            // already stopped a leftover instance, so the old image is not
            // locked; remove it before copying.
            if (target.exists() && !QFile::remove(to)) {
                if (error) {
                    *error = QStringLiteral("Could not replace %1 (it may still be running).")
                                 .arg(relative);
                }
                return false;
            }
            if (!QFile::copy(from, to)) {
                if (error) { *error = QStringLiteral("Could not install %1.").arg(relative); }
                return false;
            }
        }
    }
    if (!QFileInfo::exists(destination + QLatin1Char('/') + exe)) {
        if (error) { *error = QStringLiteral("%1 was not installed.").arg(folder); }
        return false;
    }
    return true;
}

#ifdef Q_OS_WIN
// True when any process with this executable name is running, including one
// this Nereus process does not own (left behind by an earlier session or a
// forced close).
bool processNamedRunning(const QString& exeName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) { return false; }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (QString::fromWCharArray(entry.szExeFile)
                    .compare(exeName, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

// A helper left running by an earlier Nereus session still owns the tuner's
// (or meter's) serial port. Stop it before starting the fresh copy; a second
// instance would start but could never talk to the device, leaving its MMIO
// feed silent.
void terminateProcessesNamed(const QString& exeName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) { return; }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (QString::fromWCharArray(entry.szExeFile)
                    .compare(exeName, Qt::CaseInsensitive) != 0) {
                continue;
            }
            HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE,
                                         FALSE, entry.th32ProcessID);
            if (process != nullptr) {
                TerminateProcess(process, 0);
                WaitForSingleObject(process, 3000);
                CloseHandle(process);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
}
#endif

} // namespace

struct HelperApps::Entry {
    QString folder;
    QString exe;
    std::unique_ptr<QProcess> process;
    bool running = false;
    bool stopping = false;
};

HelperApps::HelperApps(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<HelperApps::Helper>("NereusSDR::HelperApps::Helper");
    m_waveNode = std::make_unique<Entry>();
    m_waveNode->folder = QString::fromLatin1(kWaveNodeFolder);
    m_waveNode->exe = QString::fromLatin1(kWaveNodeExe);

    m_hfAuto = std::make_unique<Entry>();
    m_hfAuto->folder = QString::fromLatin1(kHfAutoFrontendFolder);
    m_hfAuto->exe = QString::fromLatin1(kHfAutoFrontendExe);

    // WaveNode sends XML continuously; Thetis treats 20 s of silence after
    // data (or 40 s from enabling without any) as lost communication.
    m_faultTimer.setInterval(5000);
    connect(&m_faultTimer, &QTimer::timeout, this, &HelperApps::checkWaveNodeHealth);
    connect(&ExternalVariableEngine::instance(), &ExternalVariableEngine::dataReceived,
            this, &HelperApps::noteData);
    // A helper's MMIO input is part of enabling it. If its port cannot be
    // opened, fail the helper the same way a lost process/connection does.
    connect(&ExternalVariableEngine::instance(), &ExternalVariableEngine::endpointFault,
            this, [this](const QUuid& guid, const QString& message) {
        const auto matches = [&guid](Helper helper) {
            MmioEndpoint* endpoint = ExternalVariableEngine::instance().endpointByFourChar(
                helper == Helper::WaveNode ? QStringLiteral("Zjjg") : QStringLiteral("QxG4"));
            return endpoint && endpoint->guid() == guid;
        };
        if (matches(Helper::WaveNode) && enabled(Helper::WaveNode)) {
            reportFault(Helper::WaveNode,
                        QStringLiteral("WaveNode MMIO failed: %1").arg(message));
        } else if (matches(Helper::HfAuto) && enabled(Helper::HfAuto)) {
            reportFault(Helper::HfAuto,
                        QStringLiteral("HF-Auto MMIO failed: %1").arg(message));
        }
    });
}

HelperApps::~HelperApps() { stopAll(); }

HelperApps& HelperApps::instance()
{
    static HelperApps* service = new HelperApps(QCoreApplication::instance());
    return *service;
}

QString HelperApps::installRoot()
{
    return localDataRoot() + QStringLiteral("/helpers");
}

QString HelperApps::folderName(Helper helper)
{
    return helper == Helper::WaveNode ? QString::fromLatin1(kWaveNodeFolder)
                                      : QString::fromLatin1(kHfAutoFrontendFolder);
}

QString HelperApps::helperDirectory(Helper helper)
{
    return installRoot() + QLatin1Char('/') + folderName(helper);
}

QString HelperApps::executablePath(Helper helper)
{
    return helperDirectory(helper) + QLatin1Char('/')
        + (helper == Helper::WaveNode ? QString::fromLatin1(kWaveNodeExe)
                                      : QString::fromLatin1(kHfAutoFrontendExe));
}

bool HelperApps::ensureInstalled(Helper helper, QString* error)
{
    if (helper == Helper::HfAuto) {
        // The MMIO frontend is the process Nereus starts. The original
        // controller stays a separate licensed dependency that the MMIO
        // helper supervises; both folders are seeded for a complete install.
        if (!installHelperFolder(QString::fromLatin1(kHfAutoFrontendFolder),
                                 QString::fromLatin1(kHfAutoFrontendExe), error)) {
            return false;
        }
        return installHelperFolder(QString::fromLatin1(kHfAutoControllerFolder),
                                   QString::fromLatin1(kHfAutoControllerExe), error);
    }
    return installHelperFolder(QString::fromLatin1(kWaveNodeFolder),
                               QString::fromLatin1(kWaveNodeExe), error);
}

bool HelperApps::removeInstalled(Helper helper, QString* error)
{
    if (helper == Helper::WaveNode) {
        // Nothing is installed by Nereus; leave the user's helper alone.
        Q_UNUSED(error)
        return true;
    }
    const QString directory = helperDirectory(helper);
    if (!QFileInfo::exists(directory)) { return true; }
    if (QDir(directory).removeRecursively()) { return true; }
    if (error) { *error = QStringLiteral("Could not remove %1.").arg(directory); }
    return false;
}

HelperApps::Entry& HelperApps::entry(Helper helper)
{
    return helper == Helper::WaveNode ? *m_waveNode : *m_hfAuto;
}

const HelperApps::Entry& HelperApps::entry(Helper helper) const
{
    return helper == Helper::WaveNode ? *m_waveNode : *m_hfAuto;
}

bool HelperApps::isRunning(Helper helper) const
{
    if (helper == Helper::WaveNode) {
        // Managed child: the process we started (and still own) is running, or
        // its XML is arriving (a helper the operator started themselves).
        const Entry& value = entry(helper);
        if (value.running && value.process
            && value.process->state() != QProcess::NotRunning) {
            return true;
        }
        return m_waveNodeSeenData && m_waveNodeLastData.isValid()
            && m_waveNodeLastData.secsTo(QDateTime::currentDateTimeUtc()) <= 20;
    }
    const Entry& value = entry(helper);
    return value.running && value.process
        && value.process->state() != QProcess::NotRunning;
}

bool HelperApps::enabled(Helper helper) const
{
    return AppSettings::instance()
        .value(helper == Helper::WaveNode ? QStringLiteral("Helpers/WaveNodeEnabled")
                                          : QStringLiteral("Helpers/HfAutoEnabled"), false)
        .toBool();
}

void HelperApps::setEnabled(Helper helper, bool on)
{
    AppSettings::instance().setValue(
        helper == Helper::WaveNode ? QStringLiteral("Helpers/WaveNodeEnabled")
                                   : QStringLiteral("Helpers/HfAutoEnabled"), on);
    syncEndpoints(helper, on);
    if (helper == Helper::WaveNode) {
        if (on) {
            m_waveNodeEnabledAt = QDateTime::currentDateTimeUtc();
            m_waveNodeLastData = {};
            m_waveNodeSeenData = false;
            m_faultTimer.start();
        } else {
            m_faultTimer.stop();
        }
    }
    emit enabledChanged(helper, on);
}

void HelperApps::syncEndpoints(Helper helper, bool enabled)
{
    ExternalVariableEngine& engine = ExternalVariableEngine::instance();
    const QString fourChar = helper == Helper::WaveNode ? QStringLiteral("Zjjg")
                                                        : QStringLiteral("QxG4");
    if (MmioEndpoint* existing = engine.endpointByFourChar(fourChar)) {
        if (!enabled) {
            engine.removeEndpoint(existing->guid());
        } else {
            // Enabling always re-opens the listener. An endpoint left from a
            // failed earlier bind (or an edited/stopped one) is restarted,
            // and a successful one simply re-binds.
            engine.updateEndpoint(existing);
        }
        return;
    }
    if (!enabled) { return; }

    auto* endpoint = new MmioEndpoint();
    endpoint->setGuid(QUuid::createUuid());
    endpoint->setName(helper == Helper::WaveNode ? QStringLiteral("WaveNode")
                                                 : QStringLiteral("HF-Auto"));
    endpoint->setTransport(MmioEndpoint::Transport::UdpListener);
    endpoint->setFormat(helper == Helper::WaveNode ? MmioEndpoint::Format::Xml
                                                   : MmioEndpoint::Format::Json);
    endpoint->setHost(QStringLiteral("127.0.0.1"));
    endpoint->setPort(helper == Helper::WaveNode ? 9388 : 12050);
    endpoint->setDirection(helper == Helper::WaveNode ? MmioEndpoint::Direction::Both
                                                      : MmioEndpoint::Direction::In);
    endpoint->setTerminator(MmioEndpoint::Terminator::None);
    if (helper == Helper::WaveNode) {
        endpoint->setUdpTargetHost(QStringLiteral("127.0.0.1"));
        endpoint->setUdpTargetPort(9389);
    }
    // Set after setGuid(), which drops a previous ID.
    endpoint->setFourChar(fourChar);
    engine.addEndpoint(endpoint);
}

bool HelperApps::start(Helper helper, QString* error)
{
    Entry& value = entry(helper);
    if (helper == Helper::WaveNode) {
        // Part of the solution: seeded from the shipped WaveNodeUsb folder,
        // started minimised with Nereus and stopped with it below. The
        // listener bookkeeping is the only WaveNode-specific part.
        m_waveNodeEnabledAt = QDateTime::currentDateTimeUtc();
        m_waveNodeLastData = {};
        m_waveNodeSeenData = false;
    }
    if (isRunning(helper)) { syncEndpoints(helper, true); return true; }
#ifdef Q_OS_WIN
    // Replace a helper left running by an earlier session (normal close,
    // crash, or a forced stop of Nereus) before installing. Two copies
    // cannot share the tuner's serial port; the new one would run but never
    // feed MMIO. Doing this first also unlocks the image the install wants
    // to replace.
    const QString exeName = QFileInfo(executablePath(helper)).fileName();
    if (processNamedRunning(exeName)) {
        qInfo()
            << "Replacing a helper left running by an earlier session:"
            << exeName;
        terminateProcessesNamed(exeName);
    }
    if (helper == Helper::HfAuto
        && processNamedRunning(QString::fromLatin1(kHfAutoControllerExe))) {
        qInfo() << "Stopping a leftover HF-Auto Controller before starting the MMIO helper.";
        terminateProcessesNamed(QString::fromLatin1(kHfAutoControllerExe));
    }
#endif
    if (!ensureInstalled(helper, error)) { return false; }
    if (!value.process) { value.process = std::make_unique<QProcess>(); }
    QProcess& process = *value.process;
    process.setProgram(executablePath(helper));
    process.setArguments({QStringLiteral("--minimized")});
    process.setWorkingDirectory(helperDirectory(helper));
    process.setProcessChannelMode(QProcess::ForwardedChannels);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
        if (arguments->startupInfo) {
            arguments->startupInfo->wShowWindow = SW_SHOWMINNOACTIVE;
        }
    });
#endif
    process.start();
    if (!process.waitForStarted(7000)) {
        const QString reason = QStringLiteral("%1 did not start: %2")
                                   .arg(folderName(helper), process.errorString());
        if (error) { *error = reason; }
        reportFault(helper, reason);
        return false;
    }
    value.running = true;
    syncEndpoints(helper, true);
    connect(&process, &QProcess::finished, this, [this, helper](int code, QProcess::ExitStatus) {
        Entry& current = entry(helper);
        if (current.stopping) {
            current.running = false;
            emit runningChanged(helper, false);
            return;
        }
        if (!current.running) { return; }
        current.running = false;
        emit runningChanged(helper, false);
        if (enabled(helper)) {
            reportFault(helper, QStringLiteral("%1 exited unexpectedly (code %2).")
                                   .arg(folderName(helper)).arg(code));
        }
    });
    emit runningChanged(helper, true);
    return true;
}

void HelperApps::stop(Helper helper)
{
    if (helper == Helper::WaveNode) {
        // The helper is a managed child now; the common path below stops it.
        m_waveNodeSeenData = false;
        m_waveNodeLastData = {};
    }
    Entry& value = entry(helper);
    if (!value.process) {
        if (value.running) { value.running = false; emit runningChanged(helper, false); }
        return;
    }
    QProcess& process = *value.process;
    if (process.state() != QProcess::NotRunning) {
        value.stopping = true;
        bool stopped = false;
        if (helper == Helper::HfAuto) {
            // The MMIO helper lives in the tray, so process.terminate()
            // (WM_CLOSE to a visible window) never reaches it and it would be
            // force-killed without restoring the controller's UDP/CAT config.
            // Ask it to close through its command port first.
            QUdpSocket socket;
            socket.writeDatagram(QByteArrayLiteral(R"({"command":"shutdown"})"),
                                 QHostAddress::LocalHost, kHfAutoCommandPort);
            socket.waitForBytesWritten(100);
            stopped = process.waitForFinished(3000);
        }
        if (!stopped) {
            process.terminate();
            if (!process.waitForFinished(3000)) {
                process.kill();
                process.waitForFinished(2000);
            }
        }
        value.stopping = false;
    }
    if (value.running) {
        value.running = false;
        emit runningChanged(helper, false);
    }
#ifdef Q_OS_WIN
    // The MMIO frontend stops and restores the controller itself. If it was
    // force-killed, clean up a leftover controller so the next start is clean.
    if (helper == Helper::HfAuto
        && processNamedRunning(QString::fromLatin1(kHfAutoControllerExe))) {
        terminateProcessesNamed(QString::fromLatin1(kHfAutoControllerExe));
    }
#endif
}

void HelperApps::noteData(const QUuid& guid)
{
    MmioEndpoint* endpoint = ExternalVariableEngine::instance()
                                 .endpointByFourChar(QStringLiteral("Zjjg"));
    if (!endpoint || endpoint->guid() != guid) { return; }
    m_waveNodeSeenData = true;
    m_waveNodeLastData = QDateTime::currentDateTimeUtc();
}

void HelperApps::checkWaveNodeHealth()
{
    // The helper is managed like HF-Auto now: the QProcess finished handler
    // reports a crash. Its XML stops while the device is unplugged, which the
    // MMIO endpoint fault already surfaces, so no timer check is needed here.
}

void HelperApps::reportFault(Helper helper, const QString& reason)
{
    qWarning() << "helper fault:" << folderName(helper) << reason;
    emit fault(helper, reason);
    setEnabled(helper, false);
    stop(helper);
}

void HelperApps::stopAll()
{
    stop(Helper::WaveNode);
    stop(Helper::HfAuto);
}

} // namespace NereusSDR
