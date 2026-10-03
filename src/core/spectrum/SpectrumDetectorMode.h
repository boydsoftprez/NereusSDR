#pragma once
// =================================================================
// src/core/spectrum/SpectrumDetectorMode.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Enumeration only, extracted from
// gui/SpectrumWidget.h so that SpectrumDetector and SpectrumAvenger can
// live in src/core/ without dragging in QWidget and the QRhi stack.
// The values are WDSP analyzer.c detector modes and are pinned by
// tests/tst_spectrum_detector_mode.cpp.
//
// Spectrum detector type. Ported from Thetis comboDispPanDetector /
// comboDispWFDetector (setup.designer.cs:34876 + setup.designer.cs:34461
// [v2.10.3.13]).  Thetis items: Peak / Rosenfell / Average / Sample / RMS
// (Pan only has RMS; WF has 4 items).
// Applied during bin reduction: when N FFT bins are mapped to M display
// pixels, this policy decides which value is chosen.
// From Thetis specHPSDR.cs:302-321 [v2.10.3.13] DetTypePan / DetTypeWF
// -> SetDisplayDetectorMode(disp, pixout, mode).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 2 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

namespace NereusSDR {

/// Bin-to-pixel reduction mode. Mirrors the WDSP analyzer detector modes.
/// Count is a bounds sentinel (not itself a detector mode), used to clamp
/// UI combo-box indices; it is not one of the WDSP analyzer.c modes above.
enum class SpectrumDetectorMode : int {
    Peak      = 0, // take max bin in window (Thetis "Peak")
    Rosenfell = 1, // Rosenfell: alternate max/min per pixel (Thetis "Rosenfell")
    Average   = 2, // arithmetic mean of bins in window (Thetis "Average")
    Sample    = 3, // take first bin in window (Thetis "Sample")
    RMS       = 4, // root-mean-square of bins in window, Pan only; Thetis "RMS"
    Count
};

} // namespace NereusSDR
