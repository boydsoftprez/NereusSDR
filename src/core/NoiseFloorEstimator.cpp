// =================================================================
// src/core/NoiseFloorEstimator.cpp  (NereusSDR)
// =================================================================
//
// Independently implemented from NoiseFloorEstimator.h interface.
// This .cpp implements NereusSDR's percentile-based noise-floor
// estimator, which deliberately replaces (does not port) Thetis's
// processNoiseFloor algorithm in display.cs:5866. The .h carries the
// Thetis citation for contrast; this implementation is original
// NereusSDR work licensed under GPLv3.
// =================================================================

#include "NoiseFloorEstimator.h"

#include <QtNumeric>

#include <algorithm>

namespace NereusSDR {

NoiseFloorEstimator::NoiseFloorEstimator(float percentile)
    : m_percentile(percentile)
{
}

void NoiseFloorEstimator::setPercentile(float p)
{
    m_percentile = p;
}

float NoiseFloorEstimator::estimate(const QVector<float>& bins)
{
    if (bins.isEmpty()) {
        return qQNaN();
    }
    m_workBuf = bins;
    const int   n = m_workBuf.size();
    const float p = std::clamp(m_percentile, 0.0f, 1.0f);
    const int   k = static_cast<int>(p * (n - 1));
    std::nth_element(m_workBuf.begin(),
                     m_workBuf.begin() + k,
                     m_workBuf.end());
    return m_workBuf[k];
}

}  // namespace NereusSDR
