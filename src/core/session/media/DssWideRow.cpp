// =================================================================
// src/core/session/media/DssWideRow.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//   https://github.com/ten9876/AetherSDR
//   AetherSDR is licensed under the GNU General Public License v3.
//   The upstream source has no per-file header; its project LICENSE applies.
//
// Extracted from NereusSDR SpectrumWidget::buildDssWideRow() and the
// peak-preserving reduction port in gui/DssRenderer.cpp. The reduction
// originates in AetherSDR src/gui/DssRenderer.cpp:183-216 [@1872028c].
// Modification history (NereusSDR):
//   2026-09-20  J.J. Boyd / KG4VCF. Shared Core/UI extraction and bounded
//               remote wide-row reduction, assisted by OpenAI Codex.
//
// =================================================================

#include "core/session/media/DssWideRow.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {
namespace {

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

DssWideRow cropDssWideRow(const QVector<float>& fullBinsDbm,
                          const DssWideRowRequest& request)
{
    DssWideRow result;
    if (fullBinsDbm.isEmpty() || !finitePositive(request.sourceSampleRateHz)
        || !finitePositive(request.viewSpanHz)
        || !std::isfinite(request.viewCentreHz)
        || !std::isfinite(request.sourceCentreHz)
        || !std::isfinite(request.requestedSpanFactor)
        || request.requestedSpanFactor <= 1.0
        || request.viewSpanHz >= request.sourceSampleRateHz) {
        return result;
    }

    const double wantHz = std::min(request.requestedSpanFactor * request.viewSpanHz,
                                   request.sourceSampleRateHz);
    if (!finitePositive(wantHz)) {
        return result;
    }
    const double sourceLowHz = request.sourceCentreHz - request.sourceSampleRateHz * 0.5;
    const double binHz = request.sourceSampleRateHz / static_cast<double>(fullBinsDbm.size());
    const double wideLowHz = std::clamp(request.viewCentreHz - wantHz * 0.5,
                                        sourceLowHz,
                                        sourceLowHz + request.sourceSampleRateHz - wantHz);
    const int binCount = static_cast<int>(fullBinsDbm.size());
    const int first = std::clamp(static_cast<int>((wideLowHz - sourceLowHz) / binHz),
                                 0, binCount - 1);
    const int last = std::clamp(static_cast<int>((wideLowHz + wantHz - sourceLowHz) / binHz),
                                first + 1, binCount);
    result.centreHz = wideLowHz + wantHz * 0.5;
    result.spanHz = wantHz;
    result.binsDbm = QVector<float>(fullBinsDbm.constBegin() + first,
                                    fullBinsDbm.constBegin() + last);
    return result;
}

QVector<float> peakReduceDssWideRow(const QVector<float>& binsDbm,
                                    int maximumSamples)
{
    // From AetherSDR src/gui/DssRenderer.cpp:183-216 [@1872028c].
    // Peak-preserving: a one-bin carrier must survive the row reduction.
    if (maximumSamples <= 0 || binsDbm.isEmpty()) {
        return {};
    }
    if (binsDbm.size() <= maximumSamples) {
        return binsDbm;
    }
    QVector<float> reduced;
    reduced.resize(maximumSamples);
    const double step = static_cast<double>(binsDbm.size()) / maximumSamples;
    for (int output = 0; output < maximumSamples; ++output) {
        int first = static_cast<int>(std::floor(output * step));
        int last = static_cast<int>(std::ceil((output + 1) * step));
        first = std::clamp(first, 0, static_cast<int>(binsDbm.size()) - 1);
        last = std::clamp(last, first + 1, static_cast<int>(binsDbm.size()));
        float maximum = std::isfinite(binsDbm.at(first)) ? binsDbm.at(first) : -200.0f;
        for (int index = first + 1; index < last; ++index) {
            if (std::isfinite(binsDbm.at(index))) {
                maximum = std::max(maximum, binsDbm.at(index));
            }
        }
        reduced[output] = maximum;
    }
    return reduced;
}

} // namespace NereusSDR
