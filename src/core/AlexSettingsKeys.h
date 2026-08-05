#pragma once

// no-port-check: NereusSDR-original AppSettings key vocabulary.  Nothing here
// is derived from upstream code: Thetis persists these band edges in its own
// SQL database under WinForms control names (udAlex1_5HPFStart and friends),
// not under path-shaped setting keys, so there is no upstream spelling to
// preserve.  The Thetis control names appear below purely as cross-references
// so a reader can find the matching row in the Setup designer.

// =================================================================
// src/core/AlexSettingsKeys.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.  The canonical spelling of the Alex filter-bank
// AppSettings keys, shared by the Setup tab that writes them and the
// core-side code that has to find them again.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-25: Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.  Extracted from the slug table
//                in AntennaAlexAlex1Tab so SettingsHygiene (core) can
//                use the same strings without depending on src/gui/.
// =================================================================

#include <array>

namespace NereusSDR::alexKeys {

// ── Preselector row slugs ────────────────────────────────────────────────────
//
// Six rows, one slug each.  The same six slugs name three different key
// families, because all three are the same physical relay ladder viewed
// through different boards:
//
//   alex/hpf/<slug>/…    Alex-1 high-pass bank   (ANAN-100 / 200 class)
//   alex/bpf1/<slug>/…   Alex-1 band-pass bank   (Orion MkII / Saturn class)
//   alex2/hpf/<slug>/…   Alex-2 high-pass bank   (dual-Alex boards)
//
// The slugs are named after the Alex-1 high-pass crossovers because that is
// the bank NereusSDR shipped first.  On a band-pass board the same slug means
// a different filter ("1_5MHz" is the 160m band-pass at 1.5-2.1 MHz, not a
// 1.5 MHz high-pass), but the storage slot is deliberately shared so edges
// persisted before the band-pass table existed keep loading.
//
// These strings are an on-disk contract with every settings file in the
// field.  Renaming one orphans the corresponding stored value: AppSettings
// keeps a flat key->value map with no migration on read.
inline constexpr const char* kPreselector1_5MHz = "1_5MHz";  // ud*1_5HPF* / ud1_5BPF1*
inline constexpr const char* kPreselector6_5MHz = "6_5MHz";  // ud*6_5HPF* / ud6_5BPF1*
inline constexpr const char* kPreselector9_5MHz = "9_5MHz";  // ud*9_5HPF* / ud9_5BPF1*
inline constexpr const char* kPreselector13MHz  = "13MHz";   // ud*13HPF*  / ud13BPF1*
inline constexpr const char* kPreselector20MHz  = "20MHz";   // ud*20HPF*  / ud20BPF1*
inline constexpr const char* kPreselector6mBP   = "6mBP";    // ud*6BPF*   / ud6BPF1*

// Row order matches the Setup tab's top-to-bottom layout.
inline constexpr std::array<const char*, 6> kPreselectorSlugs = {
    kPreselector1_5MHz,
    kPreselector6_5MHz,
    kPreselector9_5MHz,
    kPreselector13MHz,
    kPreselector20MHz,
    kPreselector6mBP,
};

// ── Leaf keys under a preselector row ────────────────────────────────────────
//
// Every preselector row persists all three.  Code that sweeps or removes a
// row must cover "enabled" as well as the two edges, or a stale per-band
// bypass flag outlives the band edges it belonged to.
inline constexpr const char* kLeafEnabled = "enabled";
inline constexpr const char* kLeafStart   = "start";
inline constexpr const char* kLeafEnd     = "end";

inline constexpr std::array<const char*, 3> kPreselectorLeaves = {
    kLeafEnabled,
    kLeafStart,
    kLeafEnd,
};

// ── Key prefixes ─────────────────────────────────────────────────────────────
//
// Relative to a radio's per-MAC namespace, i.e. AppSettings stores these under
// hardware/<mac>/<prefix>/<slug>/<leaf>.  Use AppSettings::setHardwareValue /
// hardwareValue with the relative form; the flat-map calls (contains / remove)
// need the hardware/<mac>/ prefix spelled out.
inline constexpr const char* kAlex1HpfPrefix  = "alex/hpf";
inline constexpr const char* kAlex1Bpf1Prefix = "alex/bpf1";

// The Alex transmit low-pass rows (alex/lpf/…) use ham-band slugs
// (160m, 80m, 40m, 20m, 15m, 10m, 6m) rather than crossover slugs, and they
// have no core-side consumer: low-pass edges are meaningful on every Alex
// board, so there is no capability gate that would make them stray data.
// They are intentionally absent here rather than forgotten.

} // namespace NereusSDR::alexKeys
