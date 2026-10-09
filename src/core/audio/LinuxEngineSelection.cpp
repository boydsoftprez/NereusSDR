// =================================================================
// src/core/audio/LinuxEngineSelection.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See LinuxEngineSelection.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-02, R-AUD-31).
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

// pipewire-pulse names itself "PulseAudio (on PipeWire 1.0.5)".
const QString kPipeWireInServerName = QStringLiteral("PipeWire");

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
    if (probe.pulseServerName.has_value()) {
        if (probe.pulseServerName->contains(kPipeWireInServerName, Qt::CaseInsensitive)) {
            return {true, false};
        }
        return {false, true};
    }
    return {false, false};
}

} // namespace NereusSDR
