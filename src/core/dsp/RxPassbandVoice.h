// no-port-check: NereusSDR-original receive leveler station check; no upstream source.
#pragma once

// =================================================================
// src/core/dsp/RxPassbandVoice.h  (NereusSDR)
// =================================================================
//
// Reads a weak voice off the panadapter for the receive leveler's
// station check (RxStationGate). A weak SSB voice hardly moves the
// passband's average S-meter reading: it is bursty, and its energy sits
// in the few bins its pitch harmonics land in. In those bins it stands
// well above the band's noise even when the passband as a whole barely
// rises.
//
// Fed one spectrum frame at a time (the receiver's stream, FFTEngine's
// dBm bins in frequency order) with the receiver's filter passband as a
// bin range. For each frame it takes the band's noise per bin as the
// median of the bins around the passband (kReferenceWidths passband
// widths each side), and counts the passband bins more than
// kBinMarginDb above it. A noise crash raises the whole band, the
// reference with it, so it does not count. A bin that stays above the
// margin for kSteadySec (a carrier, a birdie, a steady data signal) stops
// counting until it drops: voice comes and goes.
//
// take() returns the share of the passband's bins that stood out,
// averaged over the frames since the previous take().
//
// Modification history (NereusSDR):
//   2026-11-15 — New. J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
// =================================================================

#include <vector>

namespace NereusSDR {

class RxPassbandVoice {
public:
    // A passband bin counts when it is this far above the band's noise.
    static constexpr double kBinMarginDb = 10.0;
    // A bin above the margin this long is steady, not voice.
    static constexpr double kSteadySec = 2.0;
    // Fewer passband bins than this: no reading (too coarse to tell).
    static constexpr int kMinBins = 6;
    // The noise reference spans this many passband widths on each side.
    static constexpr int kReferenceWidths = 4;
    // Bins at or below this are empty (FFTEngine's -200 dBm floor).
    static constexpr float kEmptyBinDbm = -199.0f;

    // Forget the steady bins and the frames taken so far.
    void reset();

    // One frame: `binCount` dBm bins in frequency order, the passband
    // bins [loBin, hiBin), `dtSec` after the previous frame.
    void addFrame(const float* binsDbm, int binCount, int loBin, int hiBin, double dtSec);

    // The share (0 to 1) of the passband's bins above the band's noise,
    // averaged over the frames since the last call, or -1 with none.
    double take();

private:
    std::vector<float> m_reference;    // scratch for the median
    std::vector<double> m_aboveSec;    // per passband bin
    int m_lo{-1};
    int m_hi{-1};
    double m_shareSum{0.0};
    int m_frames{0};
};

}  // namespace NereusSDR
