#pragma once

// =================================================================
// src/gui/meters/NereusButtonFace.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file. The default colors of the
// Nereus button face (the PureSignal raised button), in one place for
// the Nereus band buttons and the RX Filter copy's preset buttons. It
// contains no Thetis code and derives from no Thetis source.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24 - One source for the Nereus button face defaults.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

namespace NereusSDR::NereusButtonFace {

inline constexpr auto kText = "#dcdce1";
// PureSignal raised button: #222227 over #141417.
inline constexpr auto kRestingTop = "#222227";
inline constexpr auto kRestingBottom = "#141417";
// PureSignal checked green: raised ends of shiftL(#1c3a2a, +14).
inline constexpr auto kSelectedTop = "#306247";
inline constexpr auto kSelectedBottom = "#1d3d2c";

} // namespace NereusSDR::NereusButtonFace
