/*  calcc.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2016, 2023, 2026 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@pratt.one

*/
//
// Ported from TAPR OpenHPSDR-wdsp calcc.c/calcc.h at
// b02d5bac675dd2f33ec2bab2b339f79a597c47dd (WDSP 2.10).
//
// Modification history (NereusSDR):
//   2026-09-21 — Added bounded owning PS3 display adapter and source-derived
//                 plot transforms by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via OpenAI Codex.

#pragma once

#include "core/dsp/Ps3Snapshot.h"

#include <array>
#include <cstdint>
#include <optional>

namespace NereusSDR {

class Ps3DisplayAdapter {
public:
    using Reader = void (*)(int channel,
                            double* x,
                            double* ym,
                            double* yc,
                            double* ys,
                            double* xmCorrection,
                            double* ymCorrection,
                            double* xaCorrection,
                            double* yaCorrection,
                            int* sampleCount,
                            int* correctionCount,
                            double* phaseReferenceDegrees);

    explicit Ps3DisplayAdapter(Reader reader) noexcept;

    std::optional<Ps3Snapshot> capture(
        int channelId,
        std::uint64_t sessionGeneration,
        std::uint64_t sequence,
        std::int64_t capturedAtUnixMilliseconds);

    static std::optional<Ps3CorrectionSummary> correctionSummary(const Ps3Snapshot& snapshot);

    static Ps3PlotData transform(const Ps3Snapshot& snapshot);

    // Normalizes to [-180, 180).  Both input endpoints therefore map to
    // -180, avoiding two serialized representations for the same direction.
    static double wrap180(double degrees) noexcept;

private:
    Reader m_reader{nullptr};

    // GetPSDisp has no capacity arguments, so storage is allocated at the
    // pinned source maxima before every vendor call can occur.
    std::array<double, Ps3Snapshot::kMaxSampleCount> m_x{};
    std::array<double, Ps3Snapshot::kMaxSampleCount> m_ym{};
    std::array<double, Ps3Snapshot::kMaxSampleCount> m_yc{};
    std::array<double, Ps3Snapshot::kMaxSampleCount> m_ys{};
    std::array<double, Ps3Snapshot::kMaxCorrectionCount> m_xmCorrection{};
    std::array<double, Ps3Snapshot::kMaxCorrectionCount> m_ymCorrection{};
    std::array<double, Ps3Snapshot::kMaxCorrectionCount> m_xaCorrection{};
    std::array<double, Ps3Snapshot::kMaxCorrectionCount> m_yaCorrection{};
};

} // namespace NereusSDR
