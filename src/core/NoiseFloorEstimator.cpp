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
