// =================================================================
// src/core/audio/CaptureHelperLocator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Per-platform lookup of the
// nereus-audio-capture helper; no Thetis logic.
// =================================================================

#include "core/audio/CaptureHelperLocator.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace NereusSDR {

QString locateCaptureHelper(const QString& applicationDir)
{
    if (applicationDir.isEmpty()) {
        return {};
    }

    const QDir appDir(applicationDir);
    QStringList candidates;
#if defined(Q_OS_MAC)
    candidates << appDir.filePath(QStringLiteral("../Helpers/nereus-audio-capture"))
               << appDir.filePath(QStringLiteral("nereus-audio-capture"));
#elif defined(Q_OS_WIN)
    candidates << appDir.filePath(QStringLiteral("nereus-audio-capture.exe"));
#else
    candidates << appDir.filePath(QStringLiteral("nereus-audio-capture"))
               << appDir.filePath(QStringLiteral("../lib/nereus/nereus-audio-capture"));
#endif

    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile() && info.isExecutable()) {
            return QDir::cleanPath(info.absoluteFilePath());
        }
    }
    return {};
}

} // namespace NereusSDR
