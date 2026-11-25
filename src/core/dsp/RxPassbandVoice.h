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
// The filter's outer edges are left out: kEdgeShare of its width at each
// edge away from the carrier (the top in USB, the bottom in LSB, both in
// AM). A neighbor overlapping the filter puts its strongest audio, its low
// end, there; counting those bins kept "station present" up for the
// neighbor while the wanted station paused. The edge next to the carrier
// is kept: the wanted voice's own strongest harmonics sit there, and a
// neighbor reaches it only with its weak treble.
//
// take() returns the share of the passband's bins (the whole filter, so
// the station check's bars keep their meaning) that stood out,
// averaged over the frames since the previous take().
//
// Modification history (NereusSDR):
//   2026-11-15 — New. J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-11-16 — Leave the filter's outer edges out (kEdgeShare): a
//                neighbor overlapping one edge made RXL pump (JJ on the
//                air). J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-11-22 - outerEdges follows the mode: a USB or LSB filter crossing
//                the carrier is trimmed on its far edge only. J.J. Boyd
//                (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "core/WdspTypes.h"

#include <vector>

namespace NereusSDR {

class RxPassbandVoice {
public:
    // Share of the filter's width left out at each edge away from the
    // carrier, here and in the leveler's loudness reading
    // (RxLeveler::measureBand): 0.15 of a 100-2900 Hz filter is its top
    // 420 Hz, 2480 to 2900 Hz.
    static constexpr double kEdgeShare = 0.15;
    // A passband bin counts when it is this far above the band's noise.
    static constexpr double kBinMarginDb = 10.0;
    // A bin above the margin this long is steady, not voice.
    static constexpr double kSteadySec = 2.0;
    // Fewer counted bins than this: no reading (too coarse to tell).
    static constexpr int kMinBins = 6;
    // The noise reference spans this many passband widths on each side.
    static constexpr int kReferenceWidths = 4;
    // Bins at or below this are empty (FFTEngine's -200 dBm floor).
    static constexpr float kEmptyBinDbm = -199.0f;

    // Forget the steady bins and the frames taken so far.
    void reset();

    // One frame: `binCount` dBm bins in frequency order, the passband
    // bins [loBin, hiBin), `dtSec` after the previous frame. `trimLow` and
    // `trimHigh` leave kEdgeShare of the passband out at that edge: the
    // edges away from the carrier (outerEdges).
    void addFrame(const float* binsDbm, int binCount, int loBin, int hiBin, double dtSec,
                  bool trimLow = false, bool trimHigh = false);

    // Which edges of a filter filterLowHz..filterHighHz about the carrier
    // are away from it, for a receiver in `mode`: the low one in LSB and the
    // other lower modes, the high one in USB and the other upper modes, even
    // when the filter crosses the carrier. In AM, SAM and DSB, both sides:
    // the low one when it is below the carrier, the high one when it is
    // above.
    static void outerEdges(DSPMode mode, int filterLowHz, int filterHighHz, bool& trimLow,
                           bool& trimHigh);

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
