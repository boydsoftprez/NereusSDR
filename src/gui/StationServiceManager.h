#pragma once
// no-port-check: NereusSDR-original. iPhone app plan Task 47.

#include "core/StationBinaryLocator.h"

#include <QObject>
#include <QStringList>

#include <functional>

namespace NereusSDR {

// Commands are argument vectors, never shell command lines. Tests replace
// this runner so no service tool or user configuration is touched.
struct StationServiceCommandResult {
    int exitCode = -1;
    QString output;
};
using StationServiceRunner =
    std::function<StationServiceCommandResult(const QString&, const QStringList&)>;

struct StationServiceOptions {
    QString profile;
    bool inheritActiveProfile = true;
    QString profileDirectory;
    QString homeDirectory;
    QString binaryPath;
    QStringList prefixArguments;
    QString userName;
    QString userId;
    StationPlatform platform = currentStationPlatform();
    StationServiceRunner runner;
};

class StationServiceManager : public QObject {
    Q_OBJECT
public:
    enum class StartupMode { Disabled, Boot, Login, Failed };

    explicit StationServiceManager(StationServiceOptions options = {}, QObject* parent = nullptr);
    bool startBackground();
    bool stopBackground();
    bool isBackgroundRunning() const;
    bool setStartWithComputer(bool enabled);
    bool startsWithComputer() const;

    StartupMode startupMode() const { return m_startupMode; }
    QString lastError() const { return m_lastError; }
    QString entryPath() const;
    QString configPath() const;

private:
    enum class ServiceState { Absent, Stopped, Pending, Running, Error };
    bool validateLaunch();
    bool writeEntry(bool startAtLogin);
    bool run(const QString& program, const QStringList& args, QString* output = nullptr) const;
    ServiceState probeState() const;
    QString serviceName() const;
    void fail(const QString& error);

    StationServiceOptions m_options;
    StartupMode m_startupMode = StartupMode::Disabled;
    QString m_lastError;
    QString m_locatorError;
};

} // namespace NereusSDR
