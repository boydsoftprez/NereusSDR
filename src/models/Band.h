// =================================================================
// src/models/Band.h  (NereusSDR)
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
//   2026-09-28 - 2 m as its own band (Band2m, number 27), per-band state
//                 slots (R-IOS-26, R-R3-49). J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
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

#include <QMetaType>
#include <QString>

#include <array>

namespace NereusSDR {

/// Ham + SWL band identity used by the per-band display grid (Phase 3G-8),
/// per-band state (Alex filter selection, band stacks, etc.) and N2ADR
/// Filter pin assignments on HL2 (Phase 3L).
///
/// Enum order: 11 HF bands + 6m + GEN + WWV + XVTR + 13 SWL bands.  HF
/// + GEN + WWV + XVTR (values 0-13) match the original Phase 3G-8
/// `BandButtonItem` UI button order; SWL bands (values 14-26) were
/// appended for Phase 3L HL2 N2ADR Filter visibility — the mi0bot
/// chkHERCULES handler writes pin-7 entries for all 13 SWL bands when
/// N2ADR is enabled (mi0bot setup.cs:14355-14367 [v2.10.3.13-beta2]).
///
/// SWL band names mirror mi0bot enums.cs:310-322 [v2.10.3.13-beta2]
/// (B120M, B90M, B61M, B49M, B41M, B31M, B25M, B22M, B19M, B16M, B14M,
/// B13M, B11M).  Order within the SWL block matches mi0bot exactly so
/// per-band iteration produces results consistent with upstream.
///
/// NereusSDR-specific deviation: SWL bands are appended after XVTR
/// rather than placed in mi0bot's BLMF-then-SWL slot (which would
/// require renumbering values 14+ and breaking persisted AppSettings
/// keys + StepAttenuator per-band arrays).  Existing values 0-13
/// preserved verbatim.
///
/// 2 m (Band2m) is Thetis's B2M (enums.cs:292 [v2.10.3.15]), 144.0 to
/// 148.0 MHz in every region (clsBandStackManager.cs:1330-1729), labelled
/// "2m" (console.cs:17364). JJ ruled it its own band on 2026-09-28; before
/// that 2 m frequencies fell into GEN. It is appended after the SWL block
/// as number 27 so that no existing band's number moves: per-band arrays,
/// numbered settings keys and the station link's band numbers keep their
/// meaning.
///
/// XVTR is never returned by `bandFromFrequency` — it represents
/// transverter mode and is set explicitly by UI when a transverter is
/// active.  SWL bands are likewise never auto-detected from frequency
/// (HL2 N2ADR pin selection is OcMatrix-driven, not freq-driven).  GEN
/// is the fallback for any frequency outside the ham bands.
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
    // SWL bands — mi0bot enums.cs:310-322 [v2.10.3.13-beta2]
    Band120m,
    Band90m,
    Band61m,
    Band49m,
    Band41m,
    Band31m,
    Band25m,
    Band22m,
    Band19m,
    Band16m,
    Band14m,
    Band13m,
    Band11m,
    // Thetis enums.cs:292 [v2.10.3.15] B2M; appended, never renumber.
    Band2m,
    Count = 28,

    // Iteration aliases for SWL-only loops (e.g. OcOutputsSwlTab matrix
    // build, HERCULES SWL pin-7 auto-fill).
    SwlFirst = Band120m,
    SwlLast  = Band11m,
};

/// Number of SWL bands (13).  Convenience for SWL-only iteration.
constexpr int kSwlBandCount =
    static_cast<int>(Band::SwlLast) - static_cast<int>(Band::SwlFirst) + 1;

/// The bands that keep per-band state of their own (TX power and tune
/// power, step attenuator, Alex antennas, the band button grid, the
/// display grid): 160m .. XVTR and 2 m. Thetis sizes these stores by
/// (int)Band.LAST, which includes B2M (console.cs:1793-1827 [v2.10.3.15]).
/// The SWL bands have none (they inherit ham-band values).
///
/// A store keeps them in slot order: slot 0-13 is the band's own number
/// (160m .. XVTR), slot 14 is 2 m. The band button grid's index is the
/// slot too.
inline constexpr int kPerBandStateCount = 15;

inline constexpr std::array<Band, kPerBandStateCount> kPerBandStateBands{
    Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m, Band::Band30m,
    Band::Band20m,  Band::Band17m, Band::Band15m, Band::Band12m, Band::Band10m,
    Band::Band6m,   Band::GEN,     Band::WWV,     Band::XVTR,    Band::Band2m,
};

/// `b`'s slot in a per-band store, or -1 for a band without state of its
/// own (the SWL bands, Count).
constexpr int perBandStateSlot(Band b) noexcept
{
    const int n = static_cast<int>(b);
    if (n >= 0 && n < static_cast<int>(Band::SwlFirst)) {
        return n;
    }
    return b == Band::Band2m ? kPerBandStateCount - 1 : -1;
}

/// The band in per-band store slot `slot` (0 .. kPerBandStateCount - 1);
/// GEN for any other slot.
constexpr Band bandFromPerBandStateSlot(int slot) noexcept
{
    return (slot >= 0 && slot < kPerBandStateCount)
        ? kPerBandStateBands[static_cast<std::size_t>(slot)]
        : Band::GEN;
}

/// True when `b` keeps per-band state of its own.
constexpr bool hasPerBandState(Band b) noexcept { return perBandStateSlot(b) >= 0; }

/// Human-readable label used in UI (e.g. "160m", "WWV").
QString bandLabel(Band b);

/// Stable persistence key suffix used for AppSettings keys
/// (e.g. "160m", "80m", "GEN", "WWV", "XVTR"). Matches `bandLabel` for
/// now but is kept as a separate function so label text can change
/// without breaking persistence compatibility.
QString bandKeyName(Band b);

/// Maps a frequency in Hz to the enclosing ham band (2 m included), or GEN
/// if outside all ham bands. Never returns XVTR. WWV is detected at the six
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
/// GEN, WWV, XVTR, 2m (the per-band state slot). Returns Band::GEN for
/// out-of-range indices.
Band bandFromUiIndex(int idx);

/// Inverse of bandFromUiIndex; -1 for a band with no button (SWL).
int uiIndexFromBand(Band b);

/// Maps a string band name to the corresponding Band enum. Accepts both
/// short-name form ("160", "80", "WWV") used by SpectrumOverlayPanel and
/// label form ("160m", "80m") used by bandKeyName(); "2" and "2m" are 2 m.
/// Returns Band::GEN
/// for unknown strings. Case-sensitive for special names ("GEN", "WWV",
/// "XVTR" — uppercase).
///
/// Added for issue #118 to route SpectrumOverlayPanel::bandSelected and
/// ContainerWidget::bandClicked through a single RadioModel handler.
Band bandFromName(const QString& name);

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::Band)
