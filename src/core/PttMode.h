// =================================================================
// src/core/PttMode.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. The PTTMode enum values and their integer
// assignments are derived from Thetis:
//   enums.cs:346-359 [v2.10.3.13] — PTTMode enum (FIRST=-1, NONE=0,
//   MANUAL=1, MIC=2, CW=3, X2=4, CAT=5, VOX=6, SPACE=7, TCI=8, LAST=9)
//
// Upstream file has no per-member inline attribution tags in this region.
//
// Disambiguation: PttSource (src/core/PttSource.h) is a separate,
// NereusSDR-native enum that tracks which UI surface triggered the most
// recent PTT/MOX event (Diagnostics page). PttMode is the radio-level
// mode used by MoxController and mirrors the Thetis PTTMode state machine.
// Both enums coexist; neither is a superset of the other.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-25 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis-derived enum values
// cited inline below.

#pragma once

#include <QString>

namespace NereusSDR {

// From Thetis enums.cs:346-359 [v2.10.3.13] — PTTMode enum; integer values
// preserved verbatim so wire serialisation and persistence round-trip safely.
enum class PttMode : int {
    First  = -1, // sentinel: below valid range
    None   =  0,
    Manual =  1,
    Mic    =  2,
    Cw     =  3,
    X2     =  4,
    Cat    =  5,
    Vox    =  6,
    Space  =  7,
    Tci    =  8,
    Last   =  9, // sentinel: one past valid range
};

// Returns a plain-English user-visible label for the given PttMode.
// Labels follow common ham-radio terminology; no Thetis source names here.
inline QString pttModeLabel(PttMode m)
{
    switch (m) {
        case PttMode::None:   return QStringLiteral("None");
        case PttMode::Manual: return QStringLiteral("MOX/Manual");
        case PttMode::Mic:    return QStringLiteral("Mic PTT");
        case PttMode::Cw:     return QStringLiteral("CW");
        case PttMode::X2:     return QStringLiteral("X2");
        case PttMode::Cat:    return QStringLiteral("CAT");
        case PttMode::Vox:    return QStringLiteral("VOX");
        case PttMode::Space:  return QStringLiteral("Space PTT");
        case PttMode::Tci:    return QStringLiteral("TCI");
        default:              return QStringLiteral("?");
    }
}

// Returns the Thetis-source identifier string for the given PttMode.
// Useful for diagnostics, logging, and AppSettings persistence round-trips.
// From Thetis enums.cs:346-359 [v2.10.3.13] — member names preserved exactly.
inline QString pttModeThetisName(PttMode m)
{
    switch (m) {
        case PttMode::None:   return QStringLiteral("NONE");
        case PttMode::Manual: return QStringLiteral("MANUAL");
        case PttMode::Mic:    return QStringLiteral("MIC");
        case PttMode::Cw:     return QStringLiteral("CW");
        case PttMode::X2:     return QStringLiteral("X2");
        case PttMode::Cat:    return QStringLiteral("CAT");
        case PttMode::Vox:    return QStringLiteral("VOX");
        case PttMode::Space:  return QStringLiteral("SPACE");
        case PttMode::Tci:    return QStringLiteral("TCI");
        default:              return QStringLiteral("NONE");
    }
}

// Converts a string to a PttMode.  Accepts both:
//   - Thetis-source identifiers (case-insensitive): "MANUAL", "manual", …
//   - User-visible labels (case-insensitive): "MOX/Manual", "Mic PTT", …
// Returns PttMode::None for any unrecognised string.
inline PttMode pttModeFromString(const QString& s)
{
    const QString lower = s.toLower();

    // Thetis-source names (case-insensitive).
    if (lower == QLatin1String("none"))   { return PttMode::None; }
    if (lower == QLatin1String("manual")) { return PttMode::Manual; }
    if (lower == QLatin1String("mic"))    { return PttMode::Mic; }
    if (lower == QLatin1String("cw"))     { return PttMode::Cw; }
    if (lower == QLatin1String("x2"))     { return PttMode::X2; }
    if (lower == QLatin1String("cat"))    { return PttMode::Cat; }
    if (lower == QLatin1String("vox"))    { return PttMode::Vox; }
    if (lower == QLatin1String("space"))  { return PttMode::Space; }
    if (lower == QLatin1String("tci"))    { return PttMode::Tci; }

    // User-visible labels (case-insensitive).
    if (lower == QLatin1String("mox/manual")) { return PttMode::Manual; }
    if (lower == QLatin1String("mic ptt"))    { return PttMode::Mic; }
    if (lower == QLatin1String("space ptt"))  { return PttMode::Space; }

    return PttMode::None;
}

} // namespace NereusSDR

// Qt metatype registration — required so PttMode can be carried by
// QVariant / QSignalSpy::value<PttMode>() without silently returning
// a zero-initialised value on Qt6 builds that haven't called
// qRegisterMetaType<>().  Matches the pattern in WdspTypes.h:298-305.
#include <QMetaType>
Q_DECLARE_METATYPE(NereusSDR::PttMode)
