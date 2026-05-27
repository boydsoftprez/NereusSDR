// =================================================================
// src/core/WidebandFftEngine.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See WidebandFftEngine.h for
// design context.
//
// =================================================================
#include "core/WidebandFftEngine.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

WidebandFftEngine::WidebandFftEngine(QObject* parent)
    : QObject(parent)
{
    m_input  = fftwf_alloc_real(kFftSize);
    m_output = fftwf_alloc_complex(kFftSize / 2 + 1);
    // FFTW_ESTIMATE matches the codebase convention used by FFTEngine
    // (FFTEngine.cpp:333). It avoids the global FFTW measurement
    // mutex which can otherwise contend with the WDSP audio thread
    // during plan creation. The 16384-pt r2c plan is small enough
    // that ESTIMATE is fine without measured wisdom.
    m_plan = fftwf_plan_dft_r2c_1d(kFftSize, m_input, m_output,
                                   FFTW_ESTIMATE);
}

WidebandFftEngine::~WidebandFftEngine()
{
    if (m_plan != nullptr) {
        fftwf_destroy_plan(m_plan);
    }
    if (m_input != nullptr) {
        fftwf_free(m_input);
    }
    if (m_output != nullptr) {
        fftwf_free(m_output);
    }
}

double WidebandFftEngine::binWidthHz() const
{
    return m_adcRateHz / double(kFftSize);
}

void WidebandFftEngine::computeFft(const QVector<float>& realSamples,
                                   QVector<float>& dbmBins)
{
    if (realSamples.size() != kFftSize) {
        return;
    }

    std::copy(realSamples.cbegin(), realSamples.cend(), m_input);
    fftwf_execute(m_plan);

    dbmBins.resize(kOutputBins);
    for (int i = 0; i < kOutputBins; ++i) {
        // Skip the DC bin (m_output[0]); first output bin is m_output[1].
        const float re = m_output[i + 1][0];
        const float im = m_output[i + 1][1];
        const float mag2 = re * re + im * im;
        // 10 * log10(|c|^2). Floor at -200 dB to avoid -inf when a
        // bin is exactly zero (silence). Calibration offset (to map
        // raw FFT magnitude into true dBm) is added downstream when
        // SpectrumWidget hooks the engine up to a calibration source
        // (Sub-Epic F Task 5+).
        dbmBins[i] = (mag2 > 0.0f) ? (10.0f * std::log10(mag2))
                                   : -200.0f;
    }
}

} // namespace NereusSDR
