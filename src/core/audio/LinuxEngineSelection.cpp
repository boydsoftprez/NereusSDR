// =================================================================
// src/core/audio/LinuxEngineSelection.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See LinuxEngineSelection.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-02, R-AUD-31).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: final review fix (R-AUD-31): PipeWire's PulseAudio service
//               runs the PulseAudio engine while PipeWire does not answer.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/LinuxEngineSelection.h"

namespace NereusSDR {

namespace {

// The Audio/LinuxBackendPreferred values detectLinuxBackend has always
// read, plus "pulse" for the PulseAudio engine by its own name.
const QString kForcePipeWire = QStringLiteral("pipewire");
const QString kForcePactl = QStringLiteral("pactl");
const QString kForcePulse = QStringLiteral("pulse");
const QString kForceNone = QStringLiteral("none");

} // namespace

bool linuxEngineForced(const QString& forced)
{
    return forced == kForcePipeWire || forced == kForcePactl || forced == kForcePulse
           || forced == kForceNone;
}

LinuxEngineChoice chooseLinuxEngines(const LinuxSoundServerProbe& probe)
{
    if (probe.forced == kForcePipeWire) {
        return {true, false};
    }
    if (probe.forced == kForcePactl || probe.forced == kForcePulse) {
        return {false, true};
    }
    if (probe.forced == kForceNone) {
        return {false, false};
    }
    if (probe.pipewireAnswers) {
        return {true, false};
    }
    // PipeWire does not answer (or this build has no PipeWire engine): a
    // server that answers runs the PulseAudio engine, PipeWire's own
    // PulseAudio service too, so one engine always plays while a server
    // answers.
    if (probe.pulseServerName.has_value()) {
        return {false, true};
    }
    return {false, false};
}

} // namespace NereusSDR
