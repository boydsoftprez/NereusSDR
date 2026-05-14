// no-port-check: NereusSDR utility wrapping Thetis volume math from
// TCIServer.cs:4110-4132 [v2.10.3.13]. Pure inline functions; no state.

// src/core/TciVolume.h  (NereusSDR)
// NereusSDR-original utility — TCI AF/MON volume dB <-> linear conversion.
//
// Ports the private linearToDbVolume / dbToLinearVolume pair from:
//   Thetis TCIServer.cs:4110-4132 [v2.10.3.13]
//
// These are pure free functions with no state. Placed in a separate header
// so they can be unit-tested without pulling in TciProtocol or Qt::WebSockets.
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 10 (audio stream family) by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#pragma once

namespace NereusSDR {

// From Thetis TCIServer.cs:4110-4120 [v2.10.3.13] — linearToDbVolume.
// Maps linear int [0..100] to dB double [-60..0] via linear interpolation.
// Saturates at endpoints: values outside [0,100] clamp to [-60, 0] dB.
inline double tciLinearToDbVolume(int linearVolume)
{
    constexpr double dbMin = -60.0;
    constexpr double dbMax = 0.0;
    constexpr double linearMin = 0.0;
    constexpr double linearMax = 100.0;
    double dbValue = ((static_cast<double>(linearVolume) - linearMin)
                      / (linearMax - linearMin)) * (dbMax - dbMin) + dbMin;
    if (dbValue < dbMin) { dbValue = dbMin; }
    if (dbValue > dbMax) { dbValue = dbMax; }
    return dbValue;
}

// From Thetis TCIServer.cs:4121-4132 [v2.10.3.13] — dbToLinearVolume.
// Inverse of tciLinearToDbVolume. Returns int in [0..100].
// Saturates at endpoints: values outside [-60, 0] dB clamp to [0, 100].
inline int tciDbToLinearVolume(double dbLevel)
{
    constexpr double dbMin = -60.0;
    constexpr double dbMax = 0.0;
    constexpr double linearMin = 0.0;
    constexpr double linearMax = 100.0;
    double linearValue = ((dbLevel - dbMin) / (dbMax - dbMin))
                         * (linearMax - linearMin) + linearMin;
    if (linearValue < linearMin) { linearValue = linearMin; }
    if (linearValue > linearMax) { linearValue = linearMax; }
    return static_cast<int>(linearValue);
}

} // namespace NereusSDR
