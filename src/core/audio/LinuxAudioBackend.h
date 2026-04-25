// =================================================================
// src/core/audio/LinuxAudioBackend.h  (NereusSDR)
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Created for the Linux PipeWire-native bridge
//                (design: docs/architecture/2026-04-23-linux-
//                audio-pipewire-design.md §4). J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
// =================================================================
#pragma once

#include <QString>
#include <functional>

namespace NereusSDR {

enum class LinuxAudioBackend {
    PipeWire,   // libpipewire-0.3 native path
    Pactl,      // pactl shell-out to PulseAudio or pipewire-pulse
    None        // no working backend detected
};

// Probe callbacks — exposed for unit-test injection.
// Return true iff the respective backend is available.
struct LinuxAudioBackendProbes {
    std::function<bool(int timeoutMs)> pipewireSocketReachable;
    std::function<bool(int timeoutMs)> pactlBinaryRunnable;
    std::function<QString()>           forcedBackendOverride;
};

LinuxAudioBackendProbes defaultProbes();

// Pure function: given probes + overrides, return the detected backend.
LinuxAudioBackend detectLinuxBackend(const LinuxAudioBackendProbes& probes);

// Convenience: call with defaultProbes().
LinuxAudioBackend detectLinuxBackend();

QString toString(LinuxAudioBackend b);

}  // namespace NereusSDR
