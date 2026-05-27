// =================================================================
// src/core/WidebandFftEngine.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. FFTW3 real-to-complex 16384-pt
// FFT engine for the P2 wideband ADC stream (Phase 3F Sub-Epic F).
//
// One instance per ADC. Consumes 16384-sample float frames emitted
// by WidebandFrameAccumulator and produces 8192 dBm-style bins
// covering 0..(adcRateHz / 2). Bin width = adcRateHz / 16384
// (7500 Hz at 122.88 MHz, 9375 Hz at 153.6 MHz).
//
// Why a separate engine rather than reusing FFTEngine: FFTEngine
// is complex-input (I/Q) and rebuilds its plan around per-DDC
// bandwidth (typically 48..384 kHz). Wideband is real-input at the
// full ADC rate (122.88 / 153.6 / 245.76 MHz) with a fixed 16384-pt
// plan. Separate class keeps both code paths small and avoids
// branching the existing complex FFT pipeline.
//
// See:
//   docs/architecture/2026-05-26-phase3f-sub-epic-f-wideband-plan.md
//   Task 4 for design context.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic F Task 4.
//                                    NereusSDR-original 16384-pt
//                                    real-input FFTW3 r2c engine
//                                    for the P2 wideband ADC stream.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================
#pragma once

#include <QObject>
#include <QVector>

#include <fftw3.h>

namespace NereusSDR {

/// Real-to-complex 16384-pt FFT engine for the P2 wideband ADC stream.
///
/// Input: 16384 normalized real samples (typically from
/// WidebandFrameAccumulator::frameReady).
/// Output: 8192 dBm-style bins (10 * log10(|c|^2)), DC bin dropped.
class WidebandFftEngine : public QObject {
    Q_OBJECT
public:
    explicit WidebandFftEngine(QObject* parent = nullptr);
    ~WidebandFftEngine() override;

    /// Configure the ADC sample rate. Used only by `binWidthHz()`;
    /// the FFT plan itself is rate-agnostic.
    void setAdcSampleRateHz(double rateHz) { m_adcRateHz = rateHz; }

    /// Per-bin frequency width (Hz). Equals adcRateHz / kFftSize.
    /// Examples: 7500 Hz at 122.88 MHz, 9375 Hz at 153.6 MHz.
    double binWidthHz() const;

    /// Compute FFT: real samples in, dBm-style bins out (size 8192
    /// for the canonical 16384-sample input). Output is resized.
    /// If `realSamples.size() != kFftSize`, the call is a no-op
    /// (callers must always pass a full frame).
    /// Note: dBm calibration constant (gain offset) deferred to the
    /// Sub-Epic F Task 5+ wiring into SpectrumWidget.
    void computeFft(const QVector<float>& realSamples,
                    QVector<float>& dbmBins);

private:
    static constexpr int kFftSize    = 16384;
    static constexpr int kOutputBins = 8192;

    double         m_adcRateHz {122880000.0};
    fftwf_plan     m_plan      {nullptr};
    float*         m_input     {nullptr};
    fftwf_complex* m_output    {nullptr};
};

} // namespace NereusSDR
