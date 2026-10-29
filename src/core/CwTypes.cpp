// no-port-check: NereusSDR-original. The station CW pitch accessor; the
// range it clamps to is cited in CwTypes.h.
// =================================================================
// src/core/CwTypes.cpp  (NereusSDR)
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, clampCwPitchHz and stationCwPitchHz
//               (R-CW-02). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "core/CwTypes.h"

#include "core/AppSettings.h"

#include <QString>
#include <algorithm>

namespace NereusSDR {

int clampCwPitchHz(int hz)
{
    const int clamped = std::clamp(hz, kCwPitchMinHz, kCwPitchMaxHz);
    // Both ends are whole steps, so the rounded value stays in range.
    return ((clamped + kCwPitchStepHz / 2) / kCwPitchStepHz) * kCwPitchStepHz;
}

int stationCwPitchHz()
{
    bool ok = false;
    const int stored = AppSettings::instance()
                           .value(QStringLiteral("CWPitch"), kCwPitchDefaultHz)
                           .toInt(&ok);
    return clampCwPitchHz(ok ? stored : kCwPitchDefaultHz);
}

} // namespace NereusSDR
