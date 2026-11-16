// no-port-check: NereusSDR-original receive leveler station check; no upstream source.
// =================================================================
// src/core/dsp/RxPassbandVoice.cpp  (NereusSDR)
// =================================================================
//
// See RxPassbandVoice.h.
// =================================================================

#include "RxPassbandVoice.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

void RxPassbandVoice::reset()
{
    m_aboveSec.clear();
    m_lo = -1;
    m_hi = -1;
    m_shareSum = 0.0;
    m_frames = 0;
}

void RxPassbandVoice::addFrame(const float* binsDbm, int binCount, int loBin, int hiBin,
                               double dtSec)
{
    if (binsDbm == nullptr || binCount <= 0) {
        return;
    }
    const int lo = std::clamp(loBin, 0, binCount);
    const int hi = std::clamp(hiBin, 0, binCount);
    const int width = hi - lo;
    if (width < kMinBins) {
        return;
    }
    if (lo != m_lo || hi != m_hi) {
        // The passband moved: what was steady there is no longer known.
        m_lo = lo;
        m_hi = hi;
        m_aboveSec.assign(static_cast<size_t>(width), 0.0);
    }
    const double dt = std::isfinite(dtSec) ? std::clamp(dtSec, 0.0, 1.0) : 0.0;

    // The band's noise per bin: the median of the bins around the passband.
    const int refLo = std::max(0, lo - kReferenceWidths * width);
    const int refHi = std::min(binCount, hi + kReferenceWidths * width);
    m_reference.clear();
    for (int i = refLo; i < refHi; ++i) {
        if (binsDbm[i] > kEmptyBinDbm && std::isfinite(binsDbm[i])) {
            m_reference.push_back(binsDbm[i]);
        }
    }
    if (static_cast<int>(m_reference.size()) < 2 * kMinBins) {
        return;
    }
    const auto mid = m_reference.begin() + static_cast<std::ptrdiff_t>(m_reference.size() / 2);
    std::nth_element(m_reference.begin(), mid, m_reference.end());
    const double threshold = static_cast<double>(*mid) + kBinMarginDb;

    int counted = 0;
    for (int i = lo; i < hi; ++i) {
        double& above = m_aboveSec[static_cast<size_t>(i - lo)];
        if (static_cast<double>(binsDbm[i]) >= threshold) {
            above += dt;
            if (above < kSteadySec) {
                ++counted;
            }
        } else {
            above = 0.0;
        }
    }
    m_shareSum += static_cast<double>(counted) / static_cast<double>(width);
    ++m_frames;
}

double RxPassbandVoice::take()
{
    if (m_frames == 0) {
        return -1.0;
    }
    const double share = m_shareSum / static_cast<double>(m_frames);
    m_shareSum = 0.0;
    m_frames = 0;
    return share;
}

}  // namespace NereusSDR
