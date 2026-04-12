// src/models/Band.h
#pragma once

#include <QString>

namespace NereusSDR {

/// Ham band identity used by the per-band display grid (Phase 3G-8) and
/// future per-band state (Alex filter selection, band stacks, etc.).
///
/// Enum order matches Thetis Band enum convention and the `BandButtonItem`
/// UI button order: 11 HF bands + 6m + GEN + WWV + XVTR. WWV and XVTR
/// were added to NereusSDR in Phase 3G-8 to match Thetis's 14-band set.
///
/// XVTR is never returned by `bandFromFrequency` — it represents
/// transverter mode and is set explicitly by UI when a transverter is
/// active. GEN is the fallback for any frequency outside the ham bands.
enum class Band : int {
    Band160m = 0,
    Band80m,
    Band60m,
    Band40m,
    Band30m,
    Band20m,
    Band17m,
    Band15m,
    Band12m,
    Band10m,
    Band6m,
    GEN,
    WWV,
    XVTR,
    Count = 14
};

/// Human-readable label used in UI (e.g. "160m", "WWV").
QString bandLabel(Band b);

/// Stable persistence key suffix used for AppSettings keys
/// (e.g. "160m", "80m", "GEN", "WWV", "XVTR"). Matches `bandLabel` for
/// now but is kept as a separate function so label text can change
/// without breaking persistence compatibility.
QString bandKeyName(Band b);

/// Maps a frequency in Hz to the enclosing ham band, or GEN if outside
/// all ham bands. Never returns XVTR. WWV is detected at the six
/// standard time-signal center frequencies (2.5/5/10/15/20/25 MHz)
/// within a ±5 kHz window.
///
/// Simplified port of Thetis BandByFreq (console.cs:6443) which delegates
/// to BandStackManager with region-aware ranges. NereusSDR uses
/// IARU Region 2 ham band edges without region variation — sufficient
/// for auto-selecting the per-band grid slot. User can always override
/// via direct setBand() if the auto-derived band is wrong.
Band bandFromFrequency(double hz);

/// Maps a 0-based `BandButtonItem` UI index to the corresponding Band.
/// Button order: 160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m,
/// GEN, WWV, XVTR. Returns Band::GEN for out-of-range indices.
Band bandFromUiIndex(int idx);

/// Inverse of bandFromUiIndex.
int uiIndexFromBand(Band b);

} // namespace NereusSDR
