// CFC paired-profile codec (NereusSDR).
// Ported from Thetis frmCFCConfig.cs:333-392,492-557 and
// ucParametricEq.cs:1353-1452 [v2.10.3.15].
// Modification history (NereusSDR): 2026-09-27 J.J. Boyd (KG4VCF),
// AI-assisted via OpenAI Codex: bounded Core codec for the existing
// CFCParaEQData format.
// 2026-09-29 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
// the published band editor (transmit.cfcProfile) and the cfc.setProfile
// verb's reader (transmitSettingsVersion 15).
/*  frmCFCConfig.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#pragma once

#include <QString>

#include <array>
#include <vector>

namespace NereusSDR::CfcProfile {

struct Profile {
    std::vector<double> f;
    std::vector<double> postF;
    std::vector<double> g;
    std::vector<double> e;
    std::vector<double> qg;
    std::vector<double> qe;
    double minHz = 0.0;
    double maxHz = 4000.0;
    double postMinHz = 0.0;
    double postMaxHz = 4000.0;
    double precompDb = 0.0;
    double postEqGainDb = 0.0;
    bool compParametric = true;
    bool eqParametric = true;

    bool usesQ() const noexcept { return compParametric && eqParametric; }
};

// CFCParaEQData is gzip+base64url of two widget JSON objects separated by
// literal <SEP>. Thetis frmCFCConfig.cs:492-557 [v2.10.3.15].
// Decode is transactional: out is unchanged on failure. Empty/opaque saved
// values are handled by their caller's legacy ten-band fallback.
bool decode(const QString& blob, Profile& out);
QString encode(const Profile& profile);

// ── The band editor an app edits (cfc.setProfile, transmitSettingsVersion 15) ──
//
// What the CFC dialog lets an operator choose, from Thetis
// frmCFCConfig.Designer.cs [v2.10.3.15]: 5, 10 or 18 bands (radCFC_5/10/18,
// applied by radCFC_bands_CheckedChanged, frmCFCConfig.cs:108-118); Low and
// High from 0 to 20000 Hz (udCFC_low/high, Designer.cs:440-458, 625-643)
// kept at least 1000 Hz apart (frmCFCConfig.cs:120-140); a band's frequency
// from 0 to 20000 Hz (nudCFC_f, Designer.cs:261-274); its compression from
// 0 to 16 dB (nudCFC_c, Designer.cs:210-224); its post-EQ gain from -24 to
// 24 dB (nudCFC_gain, Designer.cs:557-571); each Q from 0.2 to 20
// (nudCFC_cq / nudCFC_q, Designer.cs:112-130, 595-613); the pre-compression
// from 0 to 16 dB (nudCFC_precomp, Designer.cs:401-415) and the post-EQ gain
// from -24 to 24 dB (nudCFC_posteqgain, Designer.cs:330-344). One frequency
// per band drives both curves (frmCFCConfig.cs:217-230 SetPointHz; the
// runtime profile uses the compression curve's frequencies, cs:333-392).
inline constexpr int    kBandCounts[]       = {5, 10, 18};
inline constexpr double kFrequencyMinHz     = 0.0;
inline constexpr double kFrequencyMaxHz     = 20000.0;
inline constexpr double kMinRangeSpreadHz   = 1000.0;
inline constexpr double kCompressionMinDb   = 0.0;
inline constexpr double kCompressionMaxDb   = 16.0;
inline constexpr double kPostEqGainMinDb    = -24.0;
inline constexpr double kPostEqGainMaxDb    = 24.0;
inline constexpr double kQMin               = 0.2;
inline constexpr double kQMax               = 20.0;

/// transmit.cfcProfile: the band editor as compact JSON with sorted keys
/// (NereusSDR-owned; the station link document's "The CFC band editor"):
///   {"bands":[{"compressionDb","compressionQ","frequencyHz","postEqGainDb",
///              "postEqQ"}], "maxHz", "minHz", "parametric", "postEqGainDb",
///    "precompDb", "revision":"<16 hex>", "state":"saved"|"legacy"}
/// "saved" is a CFCParaEQData the Core reads; "legacy" is the ten-band
/// values an older profile keeps (legacyProfile). "parametric" is the Use Q
/// check box: both curves' flags (frmCFCConfig.cs:378).
QString publishedJson(const Profile& profile, const QString& state);

/// The revision publishedJson carries: the first 16 hex digits of the
/// SHA-256 of the published JSON without "revision" and "state", so equal
/// values give an equal revision on every host.
QString revision(const Profile& profile);

/// A band editor an app sent in the cfcProfile shape (bands, minHz, maxHz,
/// parametric, precompDb, postEqGainDb; any other key ignored), checked
/// against the dialog's choices above, each value rounded as the Core keeps
/// it (frequency to 0.001 Hz, dB to 0.1, Q to 0.01), the first band at the
/// low end and the last at the high end, one frequency per band for both
/// curves and the Use Q choice on both. False, with the reason in plain
/// words and `out` left alone, for anything the dialog could not hold.
bool fromPublishedJson(const QString& json, Profile& out, QString* refusal);

/// The profile the CFC dialog shows for the ten-band values an older
/// profile keeps (TxCfcDialog::seedWidgetsFromTransmitModel): Q 4, the
/// range 0..4000 Hz widened to cover every band, the first band at the low
/// end and the last at the high end.
Profile legacyProfile(const std::array<int, 10>& frequencyHz,
                      const std::array<int, 10>& compressionDb,
                      const std::array<int, 10>& postEqGainDb,
                      int precompDb, int postEqGainDbGlobal);

} // namespace NereusSDR::CfcProfile
