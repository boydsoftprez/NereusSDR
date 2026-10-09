// =================================================================
// src/core/audio/LinuxEngineSelection.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Which Linux engine runs (R-AUD-01,
// R-AUD-02, R-AUD-31; native audio plan design choice 6, settled calls
// 21 and 23); no upstream logic.
//
// One Linux build carries both the PipeWire and the PulseAudio engines.
// Which one is offered as running depends on the sound server that
// answers at run time: PipeWire when its daemon answers, PulseAudio when a
// PulseAudio server answers that is not PipeWire's own PulseAudio service,
// neither when no server answers (the older drivers remain and the saved
// device keys wait for a later start, settled call 23).
// Audio/LinuxBackendPreferred still forces the answer, as it does for
// detectLinuxBackend: "pipewire" gives PipeWire, "pactl" (and "pulse")
// gives PulseAudio, "none" gives neither; any other value is ignored.
//
// Pure: no sound server, no settings, no libpulse or libpipewire.  It
// builds and is tested on every platform.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-02, R-AUD-31).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QString>

#include <optional>

namespace NereusSDR {

struct LinuxSoundServerProbe {
    bool pipewireAnswers = false;      // IPipeWireDeviceSystem::running()
    std::optional<QString> pulseServerName;   // pa_server_info.server_name when a context connected within 1 s
    QString forced;                    // Audio/LinuxBackendPreferred, as today
};

struct LinuxEngineChoice { bool pipewireRunning; bool pulseRunning; };

// A PulseAudio server whose name contains "PipeWire" counts as PipeWire (R-AUD-01).
LinuxEngineChoice chooseLinuxEngines(const LinuxSoundServerProbe&);

// True when `forced` decides the answer on its own ("pipewire", "pactl",
// "pulse" or "none"), so the servers need not be probed.
bool linuxEngineForced(const QString& forced);

} // namespace NereusSDR
