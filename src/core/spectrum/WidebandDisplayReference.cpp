// =================================================================
// src/core/spectrum/WidebandDisplayReference.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original relative display normalization moved
// from SpectrumWidget.cpp. This is the existing local wing-to-DDC reference,
// not a new DSP formula or an absolute per-ADC antenna calibration.
// Source explanations and citations are retained below; references to widget
// members describe the original caller. Rate, bin width and ENB are now inputs.
//
// Modification history (NereusSDR):
//   2026-09-22 J.J. Boyd / KG4VCF — Core/GUI wideband reference extraction.
//              AI-assisted transformation via OpenAI Codex.
// =================================================================

#include "core/spectrum/WidebandDisplayReference.h"
#include "core/WidebandFftEngine.h"

#include <QtGlobal>
#include <cmath>

namespace NereusSDR {
namespace {

// Divisor for a REAL transform's peak: a real sinusoid splits its
// energy between +f and -f, and an r2c transform returns only the
// positive half, so the peak sits at (sum w)/2 rather than (sum w).
// This is the 6.02 dB between this path and the complex I/Q path's
// convention at FFTEngine.cpp:484-486.
constexpr float kWidebandRealFftPeakDivisor = 2.0f;

} // namespace

// -20*log10((sum w)/2): the peak bin of a real transform of a full-scale
// sinusoid, once the analysis window is accounted for. Same convention the
// I/Q path uses (FFTEngine.cpp:484-486, -20*log10(sum w)), adjusted by the
// real-vs-complex split.
//
// Reads the window sum from the engine rather than assuming an unwindowed
// block: WidebandFftEngine gained a Hann window on 2026-08-08, which halves
// the coherent gain (about 6 dB). Hardcoding N here would have left the wings
// that far out the moment the window landed, and would have to be revisited
// again on any future window change. Blackman-Harris 7-term was tried the
// same day and rejected on the bench for its 2.63-bin ENB; had it stayed, the
// figure would have been about 24 dB rather than 6, which is the point of
// reading it rather than writing it down.
float widebandFftNormalisationDb()
{
    const double coherentPeak =
        WidebandFftEngine::windowSum() / kWidebandRealFftPeakDivisor;
    if (coherentPeak <= 0.0) { return 0.0f; }
    return -20.0f * std::log10(static_cast<float>(coherentPeak));
}

// Refer a wideband bin to the DDC's bin width.
//
// Bench 2026-08-08: the wings saturated the panel even with the FFT
// normalisation right. Both halves of the trace are showing noise, and noise
// power in a bin scales with that bin's width -- so putting a 7500 Hz
// wideband bin (122.88 MHz / 16384) next to a 47 Hz DDC bin (192 kHz / 4096)
// without referring them to a common bandwidth overstates the wideband side
// by 10*log10(7500/47) = 22 dB. That is the whole discrepancy: -78.27 dB
// alone saturated, -124.39 dB fell through the floor, and the two brackets
// straddle this value.
//
// Computed per frame rather than baked into a constant because BOTH widths
// move: the DDC bin width follows the pan's FFT size and the radio's sample
// rate, and the wideband bin width follows the ADC clock. A fixed number
// would be correct for exactly one combination and silently wrong for the
// rest.
//
// Same idea as the existing 1-Hz normalisation the DDC path already offers
// (normalizeShiftDb, -10*log10(binWidthHz), Thetis SetDisplayNormOneHz at
// specHPSDR.cs:325) — but applied only to the wings, and referred to the
// island's bin rather than to 1 Hz, since the goal is for the two halves to
// agree with each other.
//
// Note this is a power-DENSITY match, so it is the NOISE FLOORS either side
// of the boundary that line up. A narrow carrier in a wing reads low by the
// same factor; the wideband bin is 160x too coarse to resolve one anyway.
float widebandBandwidthNormalisationDb(double adcRateHz,
                                       double ddcBinWidthHz,
                                       double ddcWindowEnb,
                                       SpectrumDetectorMode detector)
{
    // NOISE bandwidth on both sides, not bin spacing. The wideband transform
    // is zero-padded, so its bins sit 4x closer together than the bandwidth
    // each one integrates -- using the spacing here would under-correct by
    // exactly that padding factor and put the wings 6 dB hot.
    //
    // The wing side is always a peak: fillWidebandWings takes a max over the
    // bins under a pixel and the engine applies no detector of its own, so
    // the window ENB stays in.
    const double wbNoiseBwHz =
        (adcRateHz
         / static_cast<double>(WidebandFftEngine::kCaptureSamples))
        * WidebandFftEngine::windowEnbBins();

    // The island side depends on which detector produced it, which is what
    // this argument is for.
    //
    // applySpectrumDetector is handed invEnb = 1 / m_fftWindowEnb, and only
    // Average, Sample and RMS actually apply it (SpectrumDetector.cpp cases
    // 2, 3 and 4 multiply by invEnb; the Peak and Rosenfell cases at 0 and 1
    // take a max and never touch it). So under those three the island pixels
    // have already had the window ENB divided out and their effective noise
    // reference is the bare bin width; under Peak and Rosenfell the ENB is
    // still in and the reference is binWidth * ENB.
    //
    // Using one reference for both left the wings high by exactly the DDC
    // window ENB whenever a normalising detector was selected: about 1.8 dB
    // on Hann, and near 5.8 dB on Flat-Top. Found by Codex on PR #318.
    const bool detectorDividesOutEnb = (detector == SpectrumDetectorMode::Average
                                     || detector == SpectrumDetectorMode::Sample
                                     || detector == SpectrumDetectorMode::RMS);
    const double ddcNoiseBwHz = detectorDividesOutEnb
        ? ddcBinWidthHz
        : ddcBinWidthHz * qMax(ddcWindowEnb, 1e-9);

    if (wbNoiseBwHz <= 0.0 || ddcNoiseBwHz <= 0.0) {
        return 0.0f;
    }
    return -10.0f * std::log10(static_cast<float>(wbNoiseBwHz / ddcNoiseBwHz));
}

float widebandRelativeReferenceDb(double adcRateHz,
                                 double ddcBinWidthHz,
                                 double ddcWindowEnb,
                                 SpectrumDetectorMode detector)
{
    return widebandFftNormalisationDb()
         + widebandBandwidthNormalisationDb(adcRateHz, ddcBinWidthHz,
                                            ddcWindowEnb, detector);
}

} // namespace NereusSDR
