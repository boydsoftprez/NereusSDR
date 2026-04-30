// =================================================================
// src/models/Band.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include "Band.h"

#include <QStringLiteral>

namespace NereusSDR {

namespace {

// Band edges in Hz. IARU Region 2 covers the widest allocations; we use
// the union so European/Australian frequencies also round-trip correctly.
// Source: ARRL band plan / IARU Region 2, cross-checked against Thetis
// BandStackManager HF definitions.
struct HamBandRange {
    Band band;
    double lowHz;
    double highHz;
};

constexpr HamBandRange kHamBandRanges[] = {
    { Band::Band160m,  1800000.0,   2000000.0 },
    { Band::Band80m,   3500000.0,   4000000.0 },
    { Band::Band60m,   5330000.0,   5410000.0 },  // US channelized block
    { Band::Band40m,   7000000.0,   7300000.0 },
    { Band::Band30m,  10100000.0,  10150000.0 },
    { Band::Band20m,  14000000.0,  14350000.0 },
    { Band::Band17m,  18068000.0,  18168000.0 },
    { Band::Band15m,  21000000.0,  21450000.0 },
    { Band::Band12m,  24890000.0,  24990000.0 },
    { Band::Band10m,  28000000.0,  29700000.0 },
    { Band::Band6m,   50000000.0,  54000000.0 },
};

// WWV time-signal transmitters (NIST Fort Collins, CO) and WWVH (Hawaii).
// Detect with a ±5 kHz window so a VFO parked near the carrier still
// auto-selects WWV.
constexpr double kWwvCenters[] = {
    2500000.0, 5000000.0, 10000000.0, 15000000.0, 20000000.0, 25000000.0
};
constexpr double kWwvHalfWindow = 5000.0;

} // namespace

QString bandLabel(Band b)
{
    switch (b) {
        case Band::Band160m: return QStringLiteral("160m");
        case Band::Band80m:  return QStringLiteral("80m");
        case Band::Band60m:  return QStringLiteral("60m");
        case Band::Band40m:  return QStringLiteral("40m");
        case Band::Band30m:  return QStringLiteral("30m");
        case Band::Band20m:  return QStringLiteral("20m");
        case Band::Band17m:  return QStringLiteral("17m");
        case Band::Band15m:  return QStringLiteral("15m");
        case Band::Band12m:  return QStringLiteral("12m");
        case Band::Band10m:  return QStringLiteral("10m");
        case Band::Band6m:   return QStringLiteral("6m");
        case Band::GEN:      return QStringLiteral("GEN");
        case Band::WWV:      return QStringLiteral("WWV");
        case Band::XVTR:     return QStringLiteral("XVTR");
        // SWL bands — labels mirror mi0bot enums.cs:310-322 [v2.10.3.13-beta2]
        case Band::Band120m: return QStringLiteral("120m");
        case Band::Band90m:  return QStringLiteral("90m");
        case Band::Band61m:  return QStringLiteral("61m");
        case Band::Band49m:  return QStringLiteral("49m");
        case Band::Band41m:  return QStringLiteral("41m");
        case Band::Band31m:  return QStringLiteral("31m");
        case Band::Band25m:  return QStringLiteral("25m");
        case Band::Band22m:  return QStringLiteral("22m");
        case Band::Band19m:  return QStringLiteral("19m");
        case Band::Band16m:  return QStringLiteral("16m");
        case Band::Band14m:  return QStringLiteral("14m");
        case Band::Band13m:  return QStringLiteral("13m");
        case Band::Band11m:  return QStringLiteral("11m");
        case Band::Count:    break;
    }
    return QStringLiteral("GEN");
}

QString bandKeyName(Band b)
{
    // Keep as a separate function from bandLabel so UI label text can
    // change without breaking persisted AppSettings keys.
    return bandLabel(b);
}

Band bandFromFrequency(double hz)
{
    for (const double center : kWwvCenters) {
        if (hz >= center - kWwvHalfWindow && hz <= center + kWwvHalfWindow) {
            return Band::WWV;
        }
    }
    for (const auto& range : kHamBandRanges) {
        if (hz >= range.lowHz && hz <= range.highHz) {
            return range.band;
        }
    }
    return Band::GEN;
}

Band bandFromUiIndex(int idx)
{
    // UI band buttons are HF amateur + GEN/WWV/XVTR only (14).  SWL bands
    // (Phase 3L extension, indices >= Band::SwlFirst) have no buttons —
    // they're set programmatically via the OcMatrix path on HL2.
    if (idx < 0 || idx >= static_cast<int>(Band::SwlFirst)) {
        return Band::GEN;
    }
    return static_cast<Band>(idx);
}

int uiIndexFromBand(Band b)
{
    return static_cast<int>(b);
}

Band bandFromName(const QString& name)
{
    // Short-name form (as emitted by SpectrumOverlayPanel::kBands).
    if (name == QLatin1String("160")) { return Band::Band160m; }
    if (name == QLatin1String("80"))  { return Band::Band80m; }
    if (name == QLatin1String("60"))  { return Band::Band60m; }
    if (name == QLatin1String("40"))  { return Band::Band40m; }
    if (name == QLatin1String("30"))  { return Band::Band30m; }
    if (name == QLatin1String("20"))  { return Band::Band20m; }
    if (name == QLatin1String("17"))  { return Band::Band17m; }
    if (name == QLatin1String("15"))  { return Band::Band15m; }
    if (name == QLatin1String("12"))  { return Band::Band12m; }
    if (name == QLatin1String("10"))  { return Band::Band10m; }
    if (name == QLatin1String("6"))   { return Band::Band6m; }

    // Label form (matches bandKeyName() output).
    if (name == QLatin1String("160m")) { return Band::Band160m; }
    if (name == QLatin1String("80m"))  { return Band::Band80m; }
    if (name == QLatin1String("60m"))  { return Band::Band60m; }
    if (name == QLatin1String("40m"))  { return Band::Band40m; }
    if (name == QLatin1String("30m"))  { return Band::Band30m; }
    if (name == QLatin1String("20m"))  { return Band::Band20m; }
    if (name == QLatin1String("17m"))  { return Band::Band17m; }
    if (name == QLatin1String("15m"))  { return Band::Band15m; }
    if (name == QLatin1String("12m"))  { return Band::Band12m; }
    if (name == QLatin1String("10m"))  { return Band::Band10m; }
    if (name == QLatin1String("6m"))   { return Band::Band6m; }

    // Special bands.
    if (name == QLatin1String("GEN"))  { return Band::GEN; }
    if (name == QLatin1String("WWV"))  { return Band::WWV; }
    if (name == QLatin1String("XVTR")) { return Band::XVTR; }

    return Band::GEN;
}

} // namespace NereusSDR
