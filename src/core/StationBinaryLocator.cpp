// no-port-check: NereusSDR-original. Task 47 packaged station lookup.
#include "core/StationBinaryLocator.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace NereusSDR {

QString locateStationBinary(const QString& applicationDir, StationPlatform platform)
{
    if (applicationDir.isEmpty()) {
        return {};
    }
    const QDir appDir(applicationDir);
    QStringList candidates;
    switch (platform) {
    case StationPlatform::MacOS:
        candidates << appDir.filePath(
            QStringLiteral("../Helpers/NereusStation.app/Contents/MacOS/nereusd"))
                   << appDir.filePath(QStringLiteral("nereusd"));
        break;
    case StationPlatform::Linux:
        candidates << appDir.filePath(QStringLiteral("nereusd"));
        break;
    case StationPlatform::Windows:
        candidates << appDir.filePath(QStringLiteral("nereusd.exe"));
        break;
    }
    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.isFile() && info.isExecutable()) {
            return QDir::cleanPath(info.absoluteFilePath());
        }
    }
    return {};
}

StationLaunchSpec locateStationLaunch(const QString& applicationDir, StationPlatform platform,
                                      const QString& appImagePath, const QString& appDirPath)
{
    const bool transientMount = applicationDir.startsWith(QDir::tempPath() + QStringLiteral("/.mount_"));
    if (platform == StationPlatform::Linux && (!appDirPath.isEmpty() || transientMount)) {
        const QFileInfo image(appImagePath);
        if (!image.isAbsolute() || !image.isFile() || !image.isExecutable()) {
            return {{}, {}, QStringLiteral("The AppImage path is unavailable. Move the AppImage to a stable location and reopen NereusSDR.")};
        }
        return {QDir::cleanPath(image.absoluteFilePath()), {QStringLiteral("--nereus-station")}, {}};
    }
    const QString binary = locateStationBinary(applicationDir, platform);
    if (binary.isEmpty()) {
        return {{}, {}, QStringLiteral("The packaged Core app is missing. Reinstall NereusSDR.")};
    }
    return {binary, {}, {}};
}

} // namespace NereusSDR
