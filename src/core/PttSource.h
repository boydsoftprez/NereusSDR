// =================================================================
// src/core/PttSource.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Tracks which source asserted the most recent
// PTT/MOX state change. Phase 3P-H Diagnostics → Radio Status page
// surfaces this for users.
//
// No Thetis port at this layer.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#pragma once

#include <QString>

namespace NereusSDR {

enum class PttSource {
    None      = 0,
    Mox       = 1,   // MOX button on the front console
    Vox       = 2,   // Voice-operated
    Cat       = 3,   // CAT command (rigctld / hamlib / etc.)
    MicPtt    = 4,   // Mic-element PTT ring
    Cw        = 5,   // Firmware keyer
    Tune      = 6,   // Tune button (reduced power carrier)
    TwoTone   = 7,   // Two-tone test generator
    Count
};

inline QString pttSourceLabel(PttSource s) {
    switch (s) {
        case PttSource::None:    return QStringLiteral("none");
        case PttSource::Mox:     return QStringLiteral("MOX");
        case PttSource::Vox:     return QStringLiteral("VOX");
        case PttSource::Cat:     return QStringLiteral("CAT");
        case PttSource::MicPtt:  return QStringLiteral("Mic PTT");
        case PttSource::Cw:      return QStringLiteral("CW");
        case PttSource::Tune:    return QStringLiteral("Tune");
        case PttSource::TwoTone: return QStringLiteral("2-Tone");
        default:                 return QStringLiteral("?");
    }
}

} // namespace NereusSDR
