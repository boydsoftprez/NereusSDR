// =================================================================
// src/core/ModelPaths.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original (no Thetis equivalent — Thetis bundles rnnoise
// as a DLL and loads it via P/Invoke; NereusSDR resolves the .bin
// file path at runtime across platform install layouts).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Written for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted development via Anthropic Claude Code.
// =================================================================

#include "ModelPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace NereusSDR::ModelPaths {

namespace {

// Probe a path and return it if readable, else empty.
QString probe(const QString& path)
{
    return QFile::exists(path) ? path : QString();
}

} // namespace

QString rnnoiseDefaultLargeBin()
{
    const QString filename = QStringLiteral("Default_large.bin");
    const QString exeDir   = QCoreApplication::applicationDirPath();

    // 1. Adjacent to the executable (Windows portable / Linux binary).
    QString p = probe(exeDir + QStringLiteral("/models/rnnoise/") + filename);
    if (!p.isEmpty()) { return p; }

    // 2. macOS .app bundle Resources.
    p = probe(exeDir + QStringLiteral("/../Resources/models/rnnoise/") + filename);
    if (!p.isEmpty()) { return QDir(p).canonicalPath(); }

    // 3. Linux install prefix (cmake --install → /usr/local/share).
    p = probe(exeDir + QStringLiteral("/../share/NereusSDR/models/rnnoise/") + filename);
    if (!p.isEmpty()) { return QDir(p).canonicalPath(); }

    // 4. XDG data dir (Linux user install).
    const QString xdg = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    if (!xdg.isEmpty()) {
        p = probe(xdg + QStringLiteral("/NereusSDR/models/rnnoise/") + filename);
        if (!p.isEmpty()) { return p; }
    }

    // 5. Dev build — binary is inside the build tree, source is a few levels up.
    //    macOS:  build/NereusSDR.app/Contents/MacOS/NereusSDR
    //    Linux:  build/NereusSDR  (or build/bin/NereusSDR)
    for (const char* rel : {
             "/../../third_party/rnnoise/models/",       // macOS .app/Contents/MacOS → source
             "/../../../third_party/rnnoise/models/",    // Linux: build/bin → source
             "/../../../../third_party/rnnoise/models/",
         }) {
        p = probe(exeDir + QLatin1String(rel) + filename);
        if (!p.isEmpty()) { return QDir(p).canonicalPath(); }
    }

    return QString();
}

} // namespace NereusSDR::ModelPaths
