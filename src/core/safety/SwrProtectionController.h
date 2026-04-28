// =================================================================
// src/core/safety/SwrProtectionController.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.13 @501e3f5]:
//   Project Files/Source/Console/console.cs
//
// Original licence from the Thetis source file is included below,
// verbatim, with // --- From [filename] --- marker per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — Ported to C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
//                Task: Phase 3M-0 Task 3 — SwrProtectionController
//                Ports PollPAPWR (console.cs:25933-26120 [v2.10.3.13])
//                and UIMOXChangedFalse reset (console.cs:29191-29195
//                [v2.10.3.13]).
// =================================================================

// --- From console.cs ---
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
//
// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include <QObject>

namespace NereusSDR::safety {

/// Two-stage SWR foldback controller ported from Thetis PollPAPWR.
///
/// Stage 1 — per-sample foldback: if SWR > limit on any ingest() call,
///   protectFactor = limit / (swr + 1). Cite: console.cs:26073-26075 [v2.10.3.13].
///
/// Stage 2 — windback latch: after 4 consecutive trips with windBackEnabled,
///   protectFactor is latched at 0.01 until onMoxOff() clears it.
///   Cite: console.cs:26070 [v2.10.3.13] (high_swr_count >= 4),
///         console.cs:26083-26090 [v2.10.3.13] (catch-all windback block).
///
/// Open-antenna heuristic: fwd > 10W and (fwd-rev) < 1W → swr=50, factor=0.01,
///   highSwr=true, openAntennaDetected=true.
///   Cite: console.cs:25989-26009 [v2.10.3.6]MW0LGE.
///
/// Tune-time bypass: disableOnTune=true + tuneActive=true + fwd in [1, tunePowerSwrIgnore]
///   + tunePowerSliderValue ≤ 70 → factor=1.0, highSwr cleared.
///   Cite: console.cs:26020-26057 [v2.10.3.13].
///
/// alex_fwd_limit floor: fwd must exceed alexFwdLimit (default 5W; 2× power-slider for
///   ANAN-8000D) for a trip to fire. Suppresses false trips during TX ramp-up.
///   Cite: console.cs:26067 [v2.10.3.13].
///
/// onMoxOff() resets all latches and emits changes.
///   Cite: console.cs:29191-29195 [v2.10.3.13] (UIMOXChangedFalse).
///
/// The controller is inert (signals only, no radio I/O) until 3M-1a wires it.
class SwrProtectionController : public QObject
{
    Q_OBJECT
public:
    explicit SwrProtectionController(QObject* parent = nullptr);

    /// Enable or disable the entire SWR protection subsystem.
    /// When disabled, ingest() always sets factor=1.0 and clears all flags.
    void setEnabled(bool on) noexcept;
    bool isEnabled() const noexcept;

    /// Set the SWR trip threshold. Thetis default is 3.0.
    /// Cite: console.cs:26067 [v2.10.3.13] (_swrProtectionLimit).
    void setLimit(float limit) noexcept;

    /// Enable the stage-2 windback latch (4 consecutive trips → lock at 0.01).
    /// Cite: console.cs:26083 [v2.10.3.13] (_swr_wind_back_power).
    void setWindBackEnabled(bool on) noexcept;

    /// Maximum fwd power (watts) that is considered "tune power" for bypass.
    /// Cite: console.cs:26033-26045 [v2.10.3.13] (_tunePowerSwrIgnore).
    void setTunePowerSwrIgnore(float watts) noexcept;

    /// When true and tuneActive is true in ingest(), bypass SWR protection
    /// if fwd power is within the tune-power range.
    /// Cite: console.cs:26020-26057 [v2.10.3.13] (disable_swr_on_tune).
    void setDisableOnTune(bool on) noexcept;

    /// Minimum forward power (watts) that can trigger a trip.
    /// Suppresses false trips during TX ramp-up. Default 5.0W.
    /// For ANAN-8000D set to 2.0 × ptbPWR.Value before each ingest call.
    /// The caller computes the watts value (ptbPWR.Value × 2.0); this setter
    /// stores it directly — do NOT pass a raw slider int.
    /// Cite: console.cs:26067 [v2.10.3.13] (alex_fwd_limit).
    void setAlexFwdLimit(float watts) noexcept;

    /// Current value of the active tune-power slider (0–100).
    /// Tune-bypass only fires when this value is ≤ 70.
    /// Cite: console.cs:26020-26057 [v2.10.3.13] (tunePowerSliderValue).
    void setTunePowerSliderValue(int value) noexcept;

    /// Feed a new forward/reflected power pair.
    /// @param fwdW   forward power in watts (alex_fwd in Thetis)
    /// @param revW   reflected power in watts (alex_rev in Thetis)
    /// @param tuneActive  true when ATU tuner is running (chkTUN.Checked)
    /// Cadence: caller drives at 1 ms during MOX, 10 ms otherwise.
    /// Cite: console.cs:25933-26120 [v2.10.3.13] (PollPAPWR loop).
    void ingest(float fwdW, float revW, bool tuneActive) noexcept;

    /// Called when MOX goes false. Resets all latches and emits changes.
    /// Cite: console.cs:29191-29195 [v2.10.3.13] (UIMOXChangedFalse).
    void onMoxOff() noexcept;

    // ── Accessors ──────────────────────────────────────────────────────────

    /// Power output scale factor [0.01..1.0]. 1.0 = full power, 0.01 = foldback.
    float protectFactor() const noexcept;

    /// True when SWR exceeded the limit on the most recent ingest().
    bool  highSwr() const noexcept;

    /// True when the stage-2 windback latch has fired (only cleared by onMoxOff()).
    bool  windBackLatched() const noexcept;

    /// True when the open-antenna heuristic triggered on the most recent ingest().
    bool  openAntennaDetected() const noexcept;

    /// Most recently computed SWR value (after floor clamp). 1.0 when clean.
    float measuredSwr() const noexcept;

signals:
    void protectFactorChanged(float factor);
    void highSwrChanged(bool isHigh);
    void windBackLatchedChanged(bool latched);
    void openAntennaDetectedChanged(bool detected);

private:
    // ── Constants (all from Thetis console.cs PollPAPWR [v2.10.3.13]) ─────

    // console.cs:25978 — both fwd and rev ≤ this → clamp SWR to 1.0
    static constexpr float kLowPowerFloor = 2.0f;

    // console.cs:25989 — open-antenna heuristic: fwd must exceed this
    static constexpr float kOpenAntennaFwdMin = 10.0f;

    // console.cs:25989 — open-antenna heuristic: (fwd-rev) must be below this
    static constexpr float kOpenAntennaDeltaMax = 1.0f;

    // console.cs:25992 — SWR value reported on open-antenna condition //[2.10.3.6]MW0LGE
    static constexpr float kOpenAntennaSwr = 50.0f;

    // console.cs:25993; 26088 — protect factor used for open-antenna and windback
    static constexpr float kWindBackFactor = 0.01f;

    // console.cs:26070 — number of consecutive trips before windback latches
    static constexpr int kTripDebounceCount = 4;

    // ── State ──────────────────────────────────────────────────────────────

    bool  m_enabled             = true;
    float m_limit               = 3.0f;  // _swrProtectionLimit
    bool  m_windBackEnabled     = false; // _swr_wind_back_power
    float m_tunePowerSwrIgnore  = 0.0f;  // _tunePowerSwrIgnore
    bool  m_disableOnTune       = false; // disable_swr_on_tune
    // console.cs:26067 [v2.10.3.13]: alex_fwd_limit (default 5W; 2×slider for ANAN-8000D)
    float m_alexFwdLimit        = 5.0f;
    // console.cs:26020-26057 [v2.10.3.13]: tunePowerSliderValue (bypass only when ≤ 70)
    int   m_tunePowerSliderValue = 0;

    float m_protectFactor       = 1.0f;  // NetworkIO.SWRProtect
    bool  m_highSwr             = false; // HighSWR
    bool  m_windBackLatched     = false; // _wind_back_engaged
    bool  m_openAntennaDetected = false;
    float m_measuredSwr         = 1.0f;  // alex_swr
    int   m_tripCount           = 0;     // high_swr_count
};

} // namespace NereusSDR::safety
