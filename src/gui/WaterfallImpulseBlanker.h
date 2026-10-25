// no-port-check: NereusSDR-original. NB Blank's row decision, without a widget.

#pragma once

// =================================================================
// src/gui/WaterfallImpulseBlanker.h  (NereusSDR)
// =================================================================
//
// The waterfall's NB Blank decision: is this row an impulse (a wideband
// burst such as an electric fence charger's click, which draws one thin
// line across every bin), and so to be replaced by the last accepted row?
//
// Based on AetherSDR's waterfall blanker:
//   src/gui/SpectrumWidget.{h,cpp} @ 1e0718ad, and its later
//   src/gui/WaterfallImpulseBlanker.h @ 52b6f7ad (#6117)
//   https://github.com/ten9876/AetherSDR
// AetherSDR project lead: Jeremy Fielder (KK7GWY, ten9876).
// The upstream files have no top-of-file licence header; AetherSDR's
// project-level GNU GPLv3 LICENSE applies. No upstream code is copied
// here; the decision below is NereusSDR's own.
//
// NereusSDR design. The feature and its workflow come from AetherSDR's
// blanker (#277: a ring of 32 row levels as the rolling baseline, 8 rows of
// history before anything is called a burst, the burst replaced by the last
// good row, the ring frozen while transmitting), which SpectrumWidget keeps.
// The test for a burst is NereusSDR's and differs from AetherSDR's in three
// ways (both AetherSDR versions read: src/gui/SpectrumWidget.cpp:8426-8468
// [@1e0718ad], the whole-row mean of tile intensity; and
// src/gui/WaterfallImpulseBlanker.h [@52b6f7ad] (#6117), the whole-row mean
// of a dB row against a dB margin, with a rejected row adding at most 5 dB
// to the ring):
//
//   1. A row's level is the MEDIAN of its finite bins, in dB, and the
//      baseline is the median of the last kRingRows accepted rows' levels.
//      A wideband impulse lifts every bin, so the median jumps with it; a
//      loud station on a minority of the bins barely moves it. A mean
//      cannot tell the two apart: a station 40 dB over the floor on 5 %
//      of the bins lifts the mean of linear power by about 27 dB, and the
//      waterfall held on every SSB syllable (seen on 75 m, 2026-10-22).
//   2. At most kMaxConsecutiveRows rows in a row are blanked. From the
//      next burst row of the same run on, rows are accepted as real (a
//      band opening, a preamp or attenuator change) and enter the ring,
//      so the baseline follows within a few rows instead of freezing.
//   3. Only accepted rows enter the ring. AetherSDR lets a rejected row in,
//      capped (1.05 x the baseline, or 5 dB over it); that cap is what let
//      a baseline creep slowly under a long burst while the waterfall held.
//
// The threshold is the operator's, in dB over the baseline (1 to 20,
// SpectrumWidget's setting); its meaning, range and default are unchanged.
//
// No Qt: the scratch copy for the selection is a member, so a row costs no
// allocation once the scratch has grown to the row width.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Claude Opus 5.5): new. NB Blank judges a row by its
//                 median against the median of accepted rows, blanks at
//                 most two rows in a row and lets only accepted rows into
//                 the baseline, so a loud SSB station no longer freezes
//                 the waterfall while a fence charger's line is still
//                 hidden. Fix wave: AetherSDR block-level attribution.
// =================================================================

#include <array>
#include <cstddef>
#include <vector>

#include "gui/NereusGuiExport.h"

namespace NereusSDR {

class NEREUS_GUI_EXPORT WaterfallImpulseBlanker {
public:
    // From AetherSDR src/gui/SpectrumWidget.h:1775 [@1e0718ad]
    // (WF_BLANKER_N): accepted rows' levels the baseline is taken over.
    static constexpr int kRingRows = 32;
    // From AetherSDR src/gui/SpectrumWidget.cpp:8449 [@1e0718ad]: rows of
    // history needed before a row can be called a burst.
    static constexpr int kMinRows = 8;
    // NereusSDR: the most rows in a row that are blanked. A fence click is
    // one row; a third burst row in a row is a real change of level.
    static constexpr int kMaxConsecutiveRows = 2;

    enum class Verdict {
        // Not a burst: draw it, it enters the baseline and is the new last
        // accepted row.
        Accepted,
        // A burst: replace it with the last accepted row. Not in the ring.
        Blanked,
        // A burst, but already kMaxConsecutiveRows rows were blanked in a
        // row: drawn as is, enters the ring and is the new last accepted
        // row, so the baseline follows a real rise.
        AcceptedOverHold,
        // No finite bin: drawn as is, never a burst, never in the ring, and
        // not a last accepted row. A burst run it falls in goes on counting.
        NoFiniteBins,
    };

    struct Decision {
        Verdict verdict = Verdict::NoFiniteBins;
        // The row's median, dB (NaN with no finite bin).
        float rowDb = 0.0f;
        // The baseline the row was judged against, dB (NaN before
        // kMinRows rows of history).
        float baselineDb = 0.0f;
    };

    // Judges one row of `count` bins in dB (as drawn; dBm here) against
    // `thresholdDb` dB over the baseline, and updates the ring.
    Decision judge(const float* rowDb, std::size_t count, float thresholdDb);

    // Forgets the baseline and the burst run.
    void reset();

    int historyRows() const { return m_ringCount; }
    int consecutiveBurstRows() const { return m_burstRun; }

private:
    void accept(float rowDb);

    std::array<float, kRingRows> m_ring{};
    int m_ringIdx = 0;
    int m_ringCount = 0;
    int m_burstRun = 0;
    // Scratch for the selection: the row's finite bins.
    std::vector<float> m_scratch;
};

} // namespace NereusSDR
