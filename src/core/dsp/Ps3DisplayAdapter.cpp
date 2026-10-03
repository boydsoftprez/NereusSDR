/*  calcc.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014, 2016, 2019, 2023, 2026 Warren Pratt, NR0V

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
// Ported from TAPR OpenHPSDR-wdsp calcc.c at
// b02d5bac675dd2f33ec2bab2b339f79a597c47dd (WDSP 2.10).
//
// Modification history (NereusSDR):
//   2026-09-21 — Added bounded owning PS3 display adapter and source-derived
//                 plot transforms by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via OpenAI Codex.

#include "core/dsp/Ps3DisplayAdapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace NereusSDR {

namespace {

template <typename Container>
bool allFinite(const Container& values, int count)
{
    return std::all_of(values.begin(), values.begin() + count,
                       [](double value) { return std::isfinite(value); });
}

template <typename Container>
std::vector<double> copyPrefix(const Container& values, int count)
{
    return std::vector<double>(values.begin(), values.begin() + count);
}

} // namespace

Ps3DisplayAdapter::Ps3DisplayAdapter(Reader reader) noexcept
    : m_reader(reader)
{
}

std::optional<Ps3Snapshot> Ps3DisplayAdapter::capture(
    int channelId,
    std::uint64_t sessionGeneration,
    std::uint64_t sequence,
    std::int64_t capturedAtUnixMilliseconds)
{
    if (!m_reader || channelId < 0) {
        return std::nullopt;
    }

    int sampleCount = -1;
    int correctionCount = -1;
    double phaseReferenceDegrees = std::numeric_limits<double>::quiet_NaN();

    m_reader(channelId,
             m_x.data(), m_ym.data(), m_yc.data(), m_ys.data(),
             m_xmCorrection.data(), m_ymCorrection.data(),
             m_xaCorrection.data(), m_yaCorrection.data(),
             &sampleCount, &correctionCount, &phaseReferenceDegrees);

    if (sampleCount <= 0 || sampleCount > Ps3Snapshot::kMaxSampleCount
        || correctionCount <= 0
        || correctionCount > Ps3Snapshot::kMaxCorrectionCount
        || !std::isfinite(phaseReferenceDegrees)) {
        return std::nullopt;
    }

    if (!allFinite(m_x, sampleCount) || !allFinite(m_ym, sampleCount)
        || !allFinite(m_yc, sampleCount) || !allFinite(m_ys, sampleCount)
        || !allFinite(m_xmCorrection, correctionCount)
        || !allFinite(m_ymCorrection, correctionCount)
        || !allFinite(m_xaCorrection, correctionCount)
        || !allFinite(m_yaCorrection, correctionCount)) {
        return std::nullopt;
    }

    Ps3Snapshot snapshot;
    snapshot.channelId = channelId;
    snapshot.sessionGeneration = sessionGeneration;
    snapshot.sequence = sequence;
    snapshot.capturedAtUnixMilliseconds = capturedAtUnixMilliseconds;
    snapshot.sampleCount = sampleCount;
    snapshot.correctionCount = correctionCount;
    snapshot.x = copyPrefix(m_x, sampleCount);
    snapshot.ym = copyPrefix(m_ym, sampleCount);
    snapshot.yc = copyPrefix(m_yc, sampleCount);
    snapshot.ys = copyPrefix(m_ys, sampleCount);
    snapshot.xmCorrection = copyPrefix(m_xmCorrection, correctionCount);
    snapshot.ymCorrection = copyPrefix(m_ymCorrection, correctionCount);
    snapshot.xaCorrection = copyPrefix(m_xaCorrection, correctionCount);
    snapshot.yaCorrection = copyPrefix(m_yaCorrection, correctionCount);
    snapshot.phaseReferenceDegrees = phaseReferenceDegrees;
    return snapshot;
}

Ps3PlotData Ps3DisplayAdapter::transform(const Ps3Snapshot& snapshot)
{
    Ps3PlotData plot;
    if (snapshot.sampleCount <= 0
        || snapshot.sampleCount > Ps3Snapshot::kMaxSampleCount
        || snapshot.correctionCount <= 0
        || snapshot.correctionCount > Ps3Snapshot::kMaxCorrectionCount) {
        return plot;
    }
    const std::size_t sampleCount = static_cast<std::size_t>(snapshot.sampleCount);
    const std::size_t correctionCount =
        static_cast<std::size_t>(snapshot.correctionCount);
    if (snapshot.x.size() < sampleCount || snapshot.ym.size() < sampleCount
        || snapshot.yc.size() < sampleCount || snapshot.ys.size() < sampleCount
        || snapshot.xmCorrection.size() < correctionCount
        || snapshot.ymCorrection.size() < correctionCount
        || snapshot.xaCorrection.size() < correctionCount
        || snapshot.yaCorrection.size() < correctionCount) {
        return plot;
    }

    plot.measuredMagnitude.reserve(sampleCount);
    plot.measuredGain.reserve(sampleCount);
    plot.measuredPhase.reserve(sampleCount);
    plot.correctionMagnitude.reserve(correctionCount);
    plot.correctionGain.reserve(correctionCount);
    plot.correctionPhase.reserve(correctionCount);

    constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const double horizontal = snapshot.ym[i] * snapshot.x[i];
        if (!std::isfinite(horizontal) || !std::isfinite(snapshot.x[i])
            || !std::isfinite(snapshot.yc[i]) || !std::isfinite(snapshot.ys[i])) {
            continue;
        }

        plot.measuredMagnitude.push_back({horizontal, snapshot.x[i]});

        if (snapshot.ym[i] != 0.0 && std::isfinite(snapshot.ym[i])) {
            const double gain = 1.0 / snapshot.ym[i];
            if (std::isfinite(gain)) {
                plot.measuredGain.push_back({horizontal, gain});
            }
        }

        const double phase = wrap180(
            std::atan2(snapshot.ys[i], snapshot.yc[i]) * kRadiansToDegrees
            - snapshot.phaseReferenceDegrees);
        if (std::isfinite(phase)) {
            plot.measuredPhase.push_back({horizontal, phase});
        }
    }

    for (std::size_t i = 0; i < correctionCount; ++i) {
        const double x = snapshot.xmCorrection[i];
        const double magnitude = snapshot.ymCorrection[i] * x;
        if (std::isfinite(x) && std::isfinite(magnitude)) {
            plot.correctionMagnitude.push_back({x, magnitude});
        }
        if (std::isfinite(x) && std::isfinite(snapshot.ymCorrection[i])) {
            plot.correctionGain.push_back({x, snapshot.ymCorrection[i]});
        }
        if (std::isfinite(snapshot.xaCorrection[i])
            && std::isfinite(snapshot.yaCorrection[i])) {
            // calcc.c generates ya_cor as unwrapped degrees and then applies
            // the phase-reference offset before GetPSDisp publishes it.
            plot.correctionPhase.push_back(
                {snapshot.xaCorrection[i], snapshot.yaCorrection[i]});
        }
    }

    return plot;
}

double Ps3DisplayAdapter::wrap180(double degrees) noexcept
{
    if (!std::isfinite(degrees)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    double wrapped = std::fmod(degrees + 180.0, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return wrapped - 180.0;
}

} // namespace NereusSDR
