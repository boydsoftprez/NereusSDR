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
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-31): the
//               answer comes from the Linux engine selection
//               (chooseLinuxEngines); Audio/LinuxBackendPreferred still
//               forces it, and "pulse" forces Pactl as "pactl" does.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
#include "core/audio/LinuxAudioBackend.h"
#include "core/audio/LinuxEngineSelection.h"

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
    QString forced = probes.forcedBackendOverride
                       ? probes.forcedBackendOverride() : QString();
#ifndef NEREUS_HAVE_PIPEWIRE
    if (forced == QLatin1String("pipewire")) {
        qCWarning(lcAudio) << "Audio/LinuxBackendPreferred=pipewire but build lacks"
                              " libpipewire — falling through to Pactl probe";
        forced.clear();   // fall through
    }
#endif
    // Any other value (empty or garbage) falls through to probes, which
    // run only when the override does not decide on its own.
    LinuxSoundServerProbe probe;
    probe.forced = forced;
    if (!linuxEngineForced(forced)) {
#ifdef NEREUS_HAVE_PIPEWIRE
        probe.pipewireAnswers = probes.pipewireSocketReachable
                                && probes.pipewireSocketReachable(500);
#endif
        // Build lacks PipeWire support — socket found but we can't use it.
        // Fall through silently to the Pactl probe.
        if (!probe.pipewireAnswers && probes.pactlBinaryRunnable
            && probes.pactlBinaryRunnable(500)) {
            // pactl answers: a PulseAudio-compatible server is there.
            probe.pulseServerName = QStringLiteral("pulseaudio");
        }
    }
    const LinuxEngineChoice choice = chooseLinuxEngines(probe);
    if (choice.pipewireRunning) {
        return LinuxAudioBackend::PipeWire;
    }
    if (choice.pulseRunning) {
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
