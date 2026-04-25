// =================================================================
// src/core/audio/LinuxAudioBackend.cpp  (NereusSDR)
// =================================================================
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Created for the Linux PipeWire-native bridge.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================
#include "core/audio/LinuxAudioBackend.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QtCore/qglobal.h>
#include "core/AppSettings.h"
#include "core/LogCategories.h"

namespace {

bool pipewireSocketReachableImpl(int /*timeoutMs*/)
{
    const QByteArray xdg = qgetenv("XDG_RUNTIME_DIR");
    if (xdg.isEmpty()) { return false; }
    return QFile::exists(QString::fromUtf8(xdg)
                         + QStringLiteral("/pipewire-0"));
}

bool pactlBinaryRunnableImpl(int timeoutMs)
{
    const QString path = QStandardPaths::findExecutable(
                           QStringLiteral("pactl"));
    if (path.isEmpty()) { return false; }
    QProcess p;
    p.start(path, {QStringLiteral("--version")});
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        return false;
    }
    return p.exitCode() == 0;
}

QString forcedOverrideImpl()
{
    return NereusSDR::AppSettings::instance()
        .value(QStringLiteral("Audio/LinuxBackendPreferred"),
               QStringLiteral(""))
        .toString();
}

}  // anonymous

namespace NereusSDR {

LinuxAudioBackend detectLinuxBackend(const LinuxAudioBackendProbes& probes)
{
    // Forced override (AppSettings debug key).
    const QString forced = probes.forcedBackendOverride
                             ? probes.forcedBackendOverride() : QString();
    if (forced == QLatin1String("pipewire")) {
#ifdef NEREUS_HAVE_PIPEWIRE
        return LinuxAudioBackend::PipeWire;
#else
        qCWarning(lcAudio) << "Audio/LinuxBackendPreferred=pipewire but build lacks"
                              " libpipewire — falling through to Pactl probe";
        // fall through
#endif
    }
    if (forced == QLatin1String("pactl"))    { return LinuxAudioBackend::Pactl; }
    if (forced == QLatin1String("none"))     { return LinuxAudioBackend::None; }
    // Any other value (empty or garbage) falls through to probes.

    if (probes.pipewireSocketReachable && probes.pipewireSocketReachable(500)) {
#ifdef NEREUS_HAVE_PIPEWIRE
        return LinuxAudioBackend::PipeWire;
#endif
        // Build lacks PipeWire support — socket found but we can't use it.
        // Fall through silently to the Pactl probe.
    }
    if (probes.pactlBinaryRunnable && probes.pactlBinaryRunnable(500)) {
        return LinuxAudioBackend::Pactl;
    }
    return LinuxAudioBackend::None;
}

LinuxAudioBackend detectLinuxBackend()
{
    return detectLinuxBackend(defaultProbes());
}

QString toString(LinuxAudioBackend b)
{
    switch (b) {
        case LinuxAudioBackend::PipeWire: return QStringLiteral("PipeWire");
        case LinuxAudioBackend::Pactl:    return QStringLiteral("Pactl");
        case LinuxAudioBackend::None:     return QStringLiteral("None");
    }
    return QStringLiteral("None");
}

// defaultProbes() — real probes used by production code.
// Tests use their own LinuxAudioBackendProbes built with mocked callbacks.
LinuxAudioBackendProbes defaultProbes()
{
    LinuxAudioBackendProbes p;
    p.pipewireSocketReachable = &pipewireSocketReachableImpl;
    p.pactlBinaryRunnable     = &pactlBinaryRunnableImpl;
    p.forcedBackendOverride   = &forcedOverrideImpl;
    return p;
}

}  // namespace NereusSDR
