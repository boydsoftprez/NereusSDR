// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/RevealFile.cpp  (NereusSDR)
// =================================================================
//
// See RevealFile.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 6.
// =================================================================

#include "gui/recordings/RevealFile.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QUrl>

#include "core/LogCategories.h"

namespace NereusSDR::RevealFile {

QString actionText()
{
#if defined(Q_OS_MAC)
    return QStringLiteral("Show in Finder");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Show in File Explorer");
#else
    return QStringLiteral("Show in folder");
#endif
}

bool reveal(const QString& path)
{
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists()) {
        qCWarning(lcRecordings) << "cannot show" << path << "in the file manager: it does not exist";
        return false;
    }
    if (info.isDir()) {
        const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
        if (!opened) {
            qCWarning(lcRecordings) << "the file manager did not open" << info.absoluteFilePath();
        }
        return opened;
    }
    bool started = false;
#if defined(Q_OS_MAC)
    started = QProcess::startDetached(QStringLiteral("/usr/bin/open"),
                                      {QStringLiteral("-R"), info.absoluteFilePath()});
#elif defined(Q_OS_WIN)
    // Explorer reads /select, with the path quoted after the comma, not the
    // whole argument quoted as QProcess would quote it.
    QProcess explorer;
    explorer.setProgram(QStringLiteral("explorer.exe"));
    explorer.setNativeArguments(QStringLiteral("/select,\"%1\"")
                                    .arg(QDir::toNativeSeparators(info.absoluteFilePath())));
    started = explorer.startDetached();
#else
    started = QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
#endif
    if (!started) {
        qCWarning(lcRecordings) << "the file manager did not open" << info.absoluteFilePath();
    }
    return started;
}

}  // namespace NereusSDR::RevealFile
