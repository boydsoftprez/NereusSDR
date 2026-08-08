// =================================================================
// src/gui/DssRenderer.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Row ring store ported from AetherSDR
//                 `src/gui/DssRenderer.cpp`.
// =================================================================

#include "gui/DssRenderer.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// From AetherSDR src/gui/DssRenderer.cpp:17 [@1872028c].
constexpr float kTemporalAlpha = 0.60f;  // temporal IIR: fraction of the new row

// From AetherSDR src/gui/DssRenderer.cpp:30-33 [@1872028c].
inline float median3(float a, float b, float c)
{
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

// From AetherSDR src/gui/DssRenderer.cpp:50-60 [@1872028c].
bool frequencyFramesMatch(double firstCenterMhz, double firstBandwidthMhz,
                          double secondCenterMhz, double secondBandwidthMhz)
{
    const auto nearlyEqual = [](double first, double second) {
        const double scale =
            std::max({1.0, std::abs(first), std::abs(second)});
        return std::abs(first - second) <= scale * 1.0e-9;
    };
    return nearlyEqual(firstCenterMhz, secondCenterMhz)
        && nearlyEqual(firstBandwidthMhz, secondBandwidthMhz);
}

// From AetherSDR src/gui/DssRenderer.cpp:183-216 [@1872028c].
// Peak-preserving: a one-bin carrier must survive the reduction to kDssCols.
std::array<float, kDssCols> resampledRawRow(const QVector<float>& binsDbm,
                                            float fallback)
{
    std::array<float, kDssCols> row;
    const int n = binsDbm.size();
    if (n <= 0) {
        row.fill(fallback);
        return row;
    }

    if (n == kDssCols) {
        for (int c = 0; c < kDssCols; ++c) {
            row[c] = std::isfinite(binsDbm[c]) ? binsDbm[c] : fallback;
        }
    } else {
        const double step = static_cast<double>(n) / kDssCols;
        for (int c = 0; c < kDssCols; ++c) {
            int i0 = static_cast<int>(std::floor(c * step));
            int i1 = static_cast<int>(std::ceil((c + 1) * step));
            i0 = std::clamp(i0, 0, n - 1);
            i1 = std::clamp(i1, i0 + 1, n);
            float mx = std::isfinite(binsDbm[i0]) ? binsDbm[i0] : fallback;
            for (int i = i0 + 1; i < i1; ++i) {
                if (std::isfinite(binsDbm[i])) {
                    mx = std::max(mx, binsDbm[i]);
                }
            }
            row[c] = mx;
        }
    }

    return row;
}

// From AetherSDR src/gui/DssRenderer.cpp:218-250 [@1872028c].
std::array<float, kDssCols> smoothDssRow(
    const std::array<float, kDssCols>& raw,
    std::array<float, kDssCols>& rawPrev1,
    std::array<float, kDssCols>& rawPrev2,
    int& rawHistCount,
    const std::array<float, kDssCols>* previousSmoothed)
{
    std::array<float, kDssCols> row = raw;
    if (rawHistCount >= 2) {
        for (int c = 0; c < kDssCols; ++c) {
            row[c] = median3(raw[c], rawPrev1[c], rawPrev2[c]);
        }
    }

    rawPrev2 = rawPrev1;
    rawPrev1 = raw;
    rawHistCount = std::min(rawHistCount + 1, 2);

    std::array<float, kDssCols> smoothed = row;
    for (int c = 0; c < kDssCols; ++c) {
        const float a = row[std::max(0, c - 1)];
        const float b = row[c];
        const float d = row[std::min(kDssCols - 1, c + 1)];
        smoothed[c] = 0.25f * a + 0.5f * b + 0.25f * d;
    }
    if (previousSmoothed != nullptr) {
        for (int c = 0; c < kDssCols; ++c) {
            smoothed[c] = kTemporalAlpha * smoothed[c]
                + (1.0f - kTemporalAlpha) * (*previousSmoothed)[c];
        }
    }
    return smoothed;
}

}  // namespace

int DssRenderer::ringAtAge(int age) const
{
    return (m_head + std::clamp(age, 0, kDssRows - 1)) % kDssRows;
}

double DssRenderer::rowCenterMhzAtAge(int age) const
{
    return m_rowCenterMhz[ringAtAge(age)];
}

double DssRenderer::rowBandwidthMhzAtAge(int age) const
{
    return m_rowBandwidthMhz[ringAtAge(age)];
}

double DssRenderer::rowWideCenterMhzAtAge(int age) const
{
    return m_rowWideCenterMhz[ringAtAge(age)];
}

double DssRenderer::rowWideBandwidthMhzAtAge(int age) const
{
    return m_rowWideBandwidthMhz[ringAtAge(age)];
}

double DssRenderer::newestWideBandwidthMhz(double targetBandwidthMhz) const
{
    if (!std::isfinite(targetBandwidthMhz) || targetBandwidthMhz <= 0.0) {
        return 0.0;
    }
    const int visible = visibleRowCount();
    for (int age = 0; age < visible; ++age) {
        const double bw = m_rowWideBandwidthMhz[ringAtAge(age)];
        if (std::isfinite(bw) && bw > targetBandwidthMhz) {
            return bw;
        }
    }
    return 0.0;
}

void DssRenderer::resetInputSmoothing()
{
    m_rawHistCount = 0;
    m_wideRawHistCount = 0;
    // Also break the temporal IIR blend for the next row of each path. Zeroing
    // the median-of-3 counters alone does not forget the previous *smoothed*
    // row that pushRow blends the new row against — that row was decoded
    // under the old scale, so without this the first post-reset row is
    // contaminated by it.
    m_skipLiveTemporalBlendOnce = true;
    m_skipWideTemporalBlendOnce = true;
}

void DssRenderer::clear()
{
    m_head = 0;
    m_count = 0;
    m_rowCenterMhz.fill(0.0);
    m_rowBandwidthMhz.fill(0.0);
    m_rowWideCenterMhz.fill(0.0);
    m_rowWideBandwidthMhz.fill(0.0);
    for (std::array<quint8, kDssCols>& coverage : m_rowWideCoverage) {
        coverage.fill(0);
    }
    m_dirty = true;
    m_rawHistCount = 0;
    m_wideRawHistCount = 0;
    m_skipLiveTemporalBlendOnce = false;
    m_skipWideTemporalBlendOnce = false;
    ++m_rowGeneration;
}

void DssRenderer::pushRow(const QVector<float>& binsDbm,
                          double centerMhz,
                          double bandwidthMhz)
{
    pushRowWithWide(binsDbm, centerMhz, bandwidthMhz,
                    QVector<float>{}, 0.0, 0.0);
}

void DssRenderer::pushRowWithWide(const QVector<float>& binsDbm,
                                  double centerMhz,
                                  double bandwidthMhz,
                                  const QVector<float>& wideBinsDbm,
                                  double wideCenterMhz,
                                  double wideBandwidthMhz)
{
    const std::array<float, kDssCols> raw = resampledRawRow(binsDbm, -200.0f);
    const bool sameFrameAsPrevious =
        m_count > 0
        && frequencyFramesMatch(
            m_rowCenterMhz[m_head], m_rowBandwidthMhz[m_head],
            centerMhz, bandwidthMhz);
    if (m_count > 0 && !sameFrameAsPrevious) {
        // Adjacent bin indices no longer represent the same frequencies.
        // Blending them would smear a signal in the direction of the pan.
        m_rawHistCount = 0;
    }
    const std::array<float, kDssCols>* previous =
        (sameFrameAsPrevious && !m_skipLiveTemporalBlendOnce)
            ? &m_rows[m_head]
            : nullptr;
    m_skipLiveTemporalBlendOnce = false;
    const std::array<float, kDssCols> nr =
        smoothDssRow(raw, m_rawPrev1, m_rawPrev2, m_rawHistCount, previous);

    m_head = (m_head - 1 + kDssRows) % kDssRows;
    m_rows[m_head] = nr;
    m_rowCoverage[m_head].fill(1);
    m_rowCenterMhz[m_head] = centerMhz;
    m_rowBandwidthMhz[m_head] = bandwidthMhz;

    const bool wideValid =
        !wideBinsDbm.isEmpty()
        && std::isfinite(wideCenterMhz)
        && std::isfinite(wideBandwidthMhz)
        && wideCenterMhz > 0.0
        && wideBandwidthMhz > 0.0;
    if (wideValid) {
        const std::array<float, kDssCols> wideRaw =
            resampledRawRow(wideBinsDbm, -200.0f);
        const int previousRing = (m_head + 1) % kDssRows;
        const bool sameWideFrameAsPrevious =
            m_count > 0
            && frequencyFramesMatch(
                m_rowWideCenterMhz[previousRing],
                m_rowWideBandwidthMhz[previousRing],
                wideCenterMhz, wideBandwidthMhz);
        if (m_count > 0 && !sameWideFrameAsPrevious) {
            m_wideRawHistCount = 0;
        }
        const std::array<float, kDssCols>* widePrevious =
            (sameWideFrameAsPrevious && !m_skipWideTemporalBlendOnce)
                ? &m_rowWide[previousRing]
                : nullptr;
        m_skipWideTemporalBlendOnce = false;
        m_rowWide[m_head] = smoothDssRow(
            wideRaw, m_wideRawPrev1, m_wideRawPrev2,
            m_wideRawHistCount, widePrevious);
        m_rowWideCoverage[m_head].fill(1);
        m_rowWideCenterMhz[m_head] = wideCenterMhz;
        m_rowWideBandwidthMhz[m_head] = wideBandwidthMhz;
    } else {
        m_rowWide[m_head].fill(-200.0f);
        m_rowWideCoverage[m_head].fill(0);
        m_rowWideCenterMhz[m_head] = 0.0;
        m_rowWideBandwidthMhz[m_head] = 0.0;
        m_wideRawHistCount = 0;
    }

    m_count = std::min(m_count + 1, kDssRows);
    m_dirty = true;
    ++m_rowGeneration;
}

}  // namespace NereusSDR
