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
//   2026-04-23 — Added rnnoiseDefaultSmallBin() and dfnrModelTarball()
//                 helpers for Sub-epic C-1 packaging. Refactored to
//                 share probe logic via internal probeModel() helper.
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

// Search all standard install locations for a model file given its
// subdirectory (e.g. "rnnoise" or "dfnet3") and filename.
// The probe sequence is the same for every model:
//   1. <exeDir>/models/<subdir>/<filename>         Windows portable / Linux installed adjacent
//   2. <exeDir>/../Resources/models/<subdir>/...   macOS .app bundle
//   3. <exeDir>/../share/NereusSDR/models/<subdir>/... Linux install prefix
//   4. <XDG_DATA>/NereusSDR/models/<subdir>/...    XDG (Linux user install)
//   5-7. Dev build fallbacks (2-4 levels up from build-dir binary)
QString probeModel(const QString& subdir, const QString& filename)
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString base   = QStringLiteral("/models/") + subdir + QStringLiteral("/") + filename;

    // 1. Adjacent to the executable (Windows portable / Linux installed adjacent).
    QString p = probe(exeDir + base);
    if (!p.isEmpty()) { return p; }

    // 2. macOS .app bundle Resources.
    p = probe(exeDir + QStringLiteral("/../Resources") + base);
    if (!p.isEmpty()) { return QDir(p).canonicalPath(); }

    // 3. Linux install prefix (cmake --install -> /usr/local/share).
    p = probe(exeDir + QStringLiteral("/../share/NereusSDR") + base);
    if (!p.isEmpty()) { return QDir(p).canonicalPath(); }

    // 4. XDG data dir (Linux user install).
    const QString xdg = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    if (!xdg.isEmpty()) {
        p = probe(xdg + QStringLiteral("/NereusSDR") + base);
        if (!p.isEmpty()) { return p; }
    }

    // 5-8. Dev build — binary is inside the build tree, source is a few levels up.
    //    Windows: build/NereusSDR.exe                              (1 level)
    //    Linux:   build/NereusSDR                                  (1 level)
    //    Linux:   build/bin/NereusSDR                              (2 levels)
    //    macOS:   build/NereusSDR.app/Contents/MacOS/NereusSDR     (4 levels)
    //    The third_party/<libname>/models/ path is relative to the repo root.
    const QString libname = (subdir == QStringLiteral("dfnet3"))
        ? QStringLiteral("deepfilter")
        : QStringLiteral("rnnoise");
    for (const char* rel : {
             "/../third_party/",
             "/../../third_party/",
             "/../../../third_party/",
             "/../../../../third_party/",
         }) {
        p = probe(exeDir + QLatin1String(rel) + libname
                  + QStringLiteral("/models/") + filename);
        if (!p.isEmpty()) { return QDir(p).canonicalPath(); }
    }

    return QString();
}

} // namespace

QString rnnoiseDefaultLargeBin()
{
    return probeModel(QStringLiteral("rnnoise"), QStringLiteral("Default_large.bin"));
}

QString rnnoiseDefaultSmallBin()
{
    return probeModel(QStringLiteral("rnnoise"), QStringLiteral("Default_small.bin"));
}

QString dfnrModelTarball()
{
    return probeModel(QStringLiteral("dfnet3"), QStringLiteral("DeepFilterNet3_onnx.tar.gz"));
}

} // namespace NereusSDR::ModelPaths
