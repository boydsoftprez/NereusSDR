// =================================================================
// src/core/WidebandFftEngine.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. FFTW3 real-to-complex FFT engine
// for the P2 wideband ADC stream (Phase 3F Sub-Epic F).
//
// One instance per ADC. Consumes 16384-sample float frames emitted
// by WidebandFrameAccumulator, zero-pads to 65536, and produces 32768 raw
// dB bins covering the positive frequencies through adcRateHz / 2 (DC
// dropped). Bin spacing = adcRateHz / 65536 (1875 Hz at 122.88 MHz).
// Noise bandwidth still comes from the 16384 captured samples and Hann ENB.
//
// Why a separate engine rather than reusing FFTEngine: FFTEngine
// is complex-input (I/Q) and rebuilds its plan around per-DDC
// bandwidth (typically 48..384 kHz). Wideband is real-input at the
// full configured ADC rate with a fixed 65536-point plan. The current
// production wiring uses the existing 122.88 MHz display reference, not
// a rate negotiated over P2. Separate class keeps both code paths small and avoids
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

#include <vector>
#include <atomic>
#include <memory>

#include <fftw3.h>

namespace NereusSDR {

/// Zero-padded real-to-complex FFT engine for the P2 wideband ADC stream.
///
/// Input: 16384 normalized real samples (typically from
/// WidebandFrameAccumulator::frameReady).
/// Output: 32768 raw dB bins (10 * log10(|c|^2)), DC bin dropped.
class WidebandFftEngine : public QObject {
    Q_OBJECT
public:
    explicit WidebandFftEngine(QObject* parent = nullptr);
    ~WidebandFftEngine() override;

    /// Configure the ADC geometry rate for bin spacing and noise bandwidth;
    /// the FFT plan itself is rate-agnostic. This is not P2 negotiation.
    void setAdcSampleRateHz(double rateHz);

    struct Geometry {
        double adcRateHz{122880000.0};
        quint64 generation{1};
        qint64 configuredAtNs{0};
    };
    /// One immutable pair, safe to capture before the FFT thread hop.
    std::shared_ptr<const Geometry> geometry() const
    {
        return std::atomic_load_explicit(&m_geometry, std::memory_order_acquire);
    }
    double adcSampleRateHz() const { return geometry()->adcRateHz; }

    /// Analysis-window figures, shared by every instance because the window
    /// is a pure function of kCaptureSamples. Static so the display side can
    /// reference amplitudes without plumbing an engine pointer through, and
    /// so there is exactly one definition of the window in the build.
    ///
    /// windowSum   — sum w[i]. The amplitude reference for converting these
    ///               bins to dB: an unwindowed block would give
    ///               kCaptureSamples, and windowing lowers it.
    /// windowEnbBins — equivalent noise bandwidth in BINS,
    ///               N * sum(w^2) / (sum w)^2. What a bin actually integrates
    ///               for noise purposes, which is NOT the zero-padded bin
    ///               spacing.
    static double windowSum();
    static double windowEnbBins();

    /// Spacing between the bins handed out (adcRate / kFftSize) versus the
    /// bandwidth each one truly integrates (adcRate / kCaptureSamples times
    /// the window ENB). Zero-padding drives these apart on purpose: the
    /// first governs where a bin sits, the second governs how much noise is
    /// in it.
    double binSpacingHz() const { return adcSampleRateHz() / double(kFftSize); }
    double noiseBandwidthHz() const
    {
        return (adcSampleRateHz() / double(kCaptureSamples)) * windowEnbBins();
    }

    /// Per-bin frequency width (Hz). Equals adcRateHz / kFftSize.
    /// Example: 1875 Hz at the configured 122.88 MHz reference.
    double binWidthHz() const;

    /// Compute FFT: real samples in, raw dB bins out (size kOutputBins
    /// for the canonical kCaptureSamples input). Output is resized.
    /// If `realSamples.size() != kCaptureSamples`, the call is a no-op
    /// (callers must always pass a full frame).
    /// Note: dBm calibration constant (gain offset) deferred to the
    /// Sub-Epic F Task 5+ wiring into SpectrumWidget.
    void computeFft(const QVector<float>& realSamples,
                    QVector<float>& dbmBins);

    /// Samples the radio actually captures per burst. Fixed in the FPGA:
    /// the capture FIFO is 16k and is not host-selectable, per the gateware's
    /// own note at n1gp-Anvelina_PROIII Orion.v:1503 [@8e86a61] ("TODO:
    /// change number of samples in FIFO (presently 16k) based on user
    /// selection"). 16384 samples at 122.88 MHz is 133 us of contiguous
    /// signal, so 1/133us = 7.5 kHz is the true resolution and no amount of
    /// client-side processing can better it.
    static constexpr int kCaptureSamples = 16384;

    /// Transform length. Larger than the capture on purpose: the extra span
    /// is zero-padding, which sinc-interpolates the SAME 7.5 kHz resolution
    /// onto a finer bin grid.
    ///
    /// This buys smoothness, NOT resolution. Bench 2026-08-08: at a 424 kHz
    /// pan window only ~56 independent wideband values covered ~620 pixels,
    /// so each one smeared across 11 px and the wings looked like stair
    /// steps beside the DDC's island. 4x padding puts a bin every 1875 Hz
    /// and the trace reads as a curve. Anyone reading 1875 Hz as a
    /// resolution figure is reading it wrong -- the main lobe is still
    /// 7.5 kHz wide.
    static constexpr int kFftSize    = 65536;
    static constexpr int kOutputBins = kFftSize / 2;

signals:
    void geometryChanged();

private:
    /// The Hann window, built once and shared.
    static const std::vector<float>& window();

    // Use shared_ptr's atomic free functions for the supported libc++ builds
    // that do not yet provide atomic<shared_ptr<T>>.
    std::shared_ptr<const Geometry> m_geometry{std::make_shared<const Geometry>()};
    fftwf_plan     m_plan      {nullptr};
    float*         m_input     {nullptr};
    fftwf_complex* m_output    {nullptr};
};

} // namespace NereusSDR
