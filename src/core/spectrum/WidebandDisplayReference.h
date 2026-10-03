#pragma once
// =================================================================
// src/core/spectrum/WidebandDisplayReference.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See the implementation for provenance.
//
// Modification history (NereusSDR):
//   2026-09-22 J.J. Boyd / KG4VCF — Core/GUI wideband reference extraction.
//              AI-assisted transformation via OpenAI Codex.
// =================================================================

#include "core/spectrum/SpectrumDetectorMode.h"

namespace NereusSDR {

/// Refer raw positive-frequency real FFT power to the capture window's gain.
float widebandFftNormalisationDb();

/// Match a raw ADC bin's NOISE bandwidth to this DDC plane's detector.
/// Inputs are the source's configured ADC rate, DDC FFT bin spacing and
/// window ENB. Zero-padding changes spacing, not capture noise bandwidth.
/// Preserves the local display's zero correction for nonpositive bandwidths.
float widebandBandwidthNormalisationDb(double adcRateHz,
                                       double ddcBinWidthHz,
                                       double ddcWindowEnb,
                                       SpectrumDetectorMode detector);

/// Sum of the two relative references above. This is NOT absolute antenna
/// calibration. The caller owns the existing station RX display scalar and
/// applies it once to the composed DDC/ADC row; the GUI must not add it again
/// to a Core-produced row. Unresolved ADC carriers retain the local survey's
/// density-reference limitation.
float widebandRelativeReferenceDb(double adcRateHz,
                                 double ddcBinWidthHz,
                                 double ddcWindowEnb,
                                 SpectrumDetectorMode detector);

} // namespace NereusSDR
