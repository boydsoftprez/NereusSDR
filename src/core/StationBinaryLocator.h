#pragma once
// no-port-check: NereusSDR-original. Task 47 packaged station lookup.

#include <QCoreApplication>
#include <QString>
#include <QStringList>

namespace NereusSDR {

enum class StationPlatform { MacOS, Linux, Windows };

constexpr StationPlatform currentStationPlatform()
{
#if defined(Q_OS_MAC)
    return StationPlatform::MacOS;
#elif defined(Q_OS_WIN)
    return StationPlatform::Windows;
#else
    return StationPlatform::Linux;
#endif
}

// Returns an executable within the application's package or beside it.
// The explicit platform permits layout tests on any development host.
QString locateStationBinary(
    const QString& applicationDir = QCoreApplication::applicationDirPath(),
    StationPlatform platform = currentStationPlatform());

struct StationLaunchSpec {
    QString program;
    QStringList prefixArguments;
    QString error;
};

// For an AppImage, service units must launch the stable AppImage file,
// never its transient /tmp/.mount_* payload. AppRun dispatches the flag.
StationLaunchSpec locateStationLaunch(
    const QString& applicationDir = QCoreApplication::applicationDirPath(),
    StationPlatform platform = currentStationPlatform(),
    const QString& appImagePath = qEnvironmentVariable("APPIMAGE"),
    const QString& appDirPath = qEnvironmentVariable("APPDIR"));

} // namespace NereusSDR
