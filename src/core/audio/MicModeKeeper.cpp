// =================================================================
// src/core/audio/MicModeKeeper.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See MicModeKeeper.h.
//
// Modification history (NereusSDR):
//   2026-10-16: new. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#include "core/audio/MicModeKeeper.h"

namespace NereusSDR {

namespace {

// The helper's exit codes.  Small numbers a crash or a kill never gives.
constexpr int kExitAlreadyStandard = 0;
constexpr int kExitMadeStandard = 1;
constexpr int kExitRefused = 2;
constexpr int kExitUnavailable = 3;
constexpr int kExitNoBundle = 4;

} // namespace

MicModeOutcome keepMicModeStandard(const QString& bundleId, const MicModeCalls& calls)
{
    if (bundleId.isEmpty()) {
        return MicModeOutcome::NoBundle;
    }
    if (!calls.read || !calls.write) {
        return MicModeOutcome::Unavailable;
    }
    if (calls.read(bundleId) == kMicModeStandard) {
        return MicModeOutcome::AlreadyStandard;
    }
    // Whatever mode it was, NereusSDR wants none of the system's voice
    // processing on its mic.  Read back: a yes that did not stick is no.
    if (!calls.write(kMicModeStandard, bundleId) || calls.read(bundleId) != kMicModeStandard) {
        return MicModeOutcome::Refused;
    }
    return MicModeOutcome::MadeStandard;
}

QString micModeOutcomeText(MicModeOutcome outcome)
{
    switch (outcome) {
    case MicModeOutcome::AlreadyStandard: return QStringLiteral("Mic Mode is Standard");
    case MicModeOutcome::MadeStandard:    return QStringLiteral("Mic Mode set to Standard");
    case MicModeOutcome::Refused:         return QStringLiteral("Mic Mode could not be set to Standard");
    case MicModeOutcome::Unavailable:     return QStringLiteral("Mic Mode is not available on this system");
    case MicModeOutcome::NoBundle:        return QStringLiteral("no bundle id, so no Mic Mode");
    }
    return QStringLiteral("Mic Mode outcome unknown");
}

int micModeExitCode(MicModeOutcome outcome)
{
    switch (outcome) {
    case MicModeOutcome::AlreadyStandard: return kExitAlreadyStandard;
    case MicModeOutcome::MadeStandard:    return kExitMadeStandard;
    case MicModeOutcome::Refused:         return kExitRefused;
    case MicModeOutcome::Unavailable:     return kExitUnavailable;
    case MicModeOutcome::NoBundle:        return kExitNoBundle;
    }
    return kExitRefused;
}

bool micModeOutcomeFromExitCode(int code, MicModeOutcome* outcome)
{
    MicModeOutcome found = MicModeOutcome::Refused;
    switch (code) {
    case kExitAlreadyStandard: found = MicModeOutcome::AlreadyStandard; break;
    case kExitMadeStandard:    found = MicModeOutcome::MadeStandard; break;
    case kExitRefused:         found = MicModeOutcome::Refused; break;
    case kExitUnavailable:     found = MicModeOutcome::Unavailable; break;
    case kExitNoBundle:        found = MicModeOutcome::NoBundle; break;
    default:
        return false;
    }
    if (outcome != nullptr) {
        *outcome = found;
    }
    return true;
}

} // namespace NereusSDR
