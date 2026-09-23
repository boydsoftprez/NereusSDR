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
    // Zero once: everything past kCaptureSamples stays zero for the life of
    // the engine, since computeFft only ever writes the first
    // kCaptureSamples entries.
    std::fill(m_input, m_input + kFftSize, 0.0f);
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

// Spacing of the bins we hand out, which after zero-padding is FINER than
// the resolution they represent. Callers mapping bin index to frequency want
// this; callers reasoning about resolvability want adcRateHz /
// kCaptureSamples, which is 4x larger.
double WidebandFftEngine::binWidthHz() const
{
    return m_adcRateHz / double(kFftSize);
}

// Hann window.
//
// From Thetis wdsp/analyzer.c:84-99 [v2.10.3.15], `case 2: // hann window`,
// coefficient form verbatim: 0.5 * (1 - cos(i * arg0)).
//
// NereusSDR applied NO window here before 2026-08-08 -- rectangular, -13 dB
// sidelobes -- which on a survey with the preselector bypassed let every
// strong broadcast carrier smear across the display.
//
// Blackman-Harris 7-term (Thetis's own choice for its wideband analyzer,
// wbDisplay.cs:4533 window_type=6) was tried first and REJECTED on the bench
// the same day: BH-7's equivalent noise bandwidth is 2.63 bins against Hann's
// 1.50, so it widened the effective resolution from 7.5 kHz to 19.7 kHz and
// the wings came back visibly blurrier than before the window existed.
//
// Upstream can afford that trade because its wideband display is a
// standalone survey window where dynamic range beats resolution. Here the
// wings sit directly beside a DDC island resolving 11.7 Hz, and the wideband
// path is already resolution-starved by the FPGA's fixed 16k capture, so
// spending 2.6x of the little resolution there is on sidelobes is the wrong
// way round. Hann costs 1.5x for -31 dB sidelobes and 18 dB/octave rolloff,
// which is the compromise this display actually wants.
//
// Upstream normalises by inv_coherent_gain (analyzer.c:96-98) so the window's
// mean is 1. Deliberately NOT done here: this engine hands out raw magnitudes
// and the amplitude reference is applied on the display side, so the
// un-normalised sum and ENB are published instead.
//
// Built once into a function-local static: the window is a pure function of
// kCaptureSamples, so every engine and every caller shares one table and
// there is a single definition of it in the build.
namespace {
struct WidebandWindow {
    std::vector<float> coeffs;
    double sum {0.0};
    double sumSq {0.0};
};

const WidebandWindow& widebandWindow()
{
    static const WidebandWindow w = []() {
        WidebandWindow built;
        const int n = WidebandFftEngine::kCaptureSamples;
        built.coeffs.resize(n);
        const double arg0 = 2.0 * M_PI / (double(n) - 1.0);
        for (int i = 0; i < n; ++i) {
            const double c = 0.5 * (1.0 - std::cos(double(i) * arg0));
            built.coeffs[i] = float(c);
            built.sum   += c;
            built.sumSq += c * c;
        }
        return built;
    }();
    return w;
}
}  // namespace

const std::vector<float>& WidebandFftEngine::window()
{
    return widebandWindow().coeffs;
}

double WidebandFftEngine::windowSum()
{
    return widebandWindow().sum;
}

// N * sum(w^2) / (sum w)^2, the standard equivalent-noise-bandwidth in bins.
// Mirrors the same quantity WDSP derives at analyzer.c:174-175
// [v2.10.3.15] (inherent_power_gain / inv_coherent_gain pair feeding
// inv_enb).
double WidebandFftEngine::windowEnbBins()
{
    const WidebandWindow& w = widebandWindow();
    if (w.sum == 0.0) { return 1.0; }
    return double(kCaptureSamples) * w.sumSq / (w.sum * w.sum);
}

void WidebandFftEngine::computeFft(const QVector<float>& realSamples,
                                   QVector<float>& dbmBins)
{
    if (realSamples.size() != kCaptureSamples) {
        return;
    }

    // Window the capture, then leave the rest of the transform zero-padded.
    // The tail was zeroed at construction and nothing writes to it.
    const std::vector<float>& win = window();
    for (int i = 0; i < kCaptureSamples; ++i) {
        m_input[i] = realSamples[i] * win[i];
    }
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
