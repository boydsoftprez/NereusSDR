// =================================================================
// src/core/safety/SwrProtectionController.cpp  (NereusSDR)
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
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-10-22 - PR 604 fix wave: ingest() follows PollPAPWR
//                (console.cs:25991-26159 [v2.10.3.15]) for the ANAN-8000D
//                open-antenna exclusion, the alexpresent/apollopresent gate
//                and swrprotection off (no early return, so a latched
//                wind-back holds until MOX off); onMoxOff() keeps
//                high_swr_count and alex_swr as UIMOXChangedFalse does, by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                Claude Code.
//   2026-10-22 - PR 604 second pass: a clean sample zeroes high_swr_count
//                while the wind-back latch holds too (console.cs:26126-26131
//                [v2.10.3.15]), by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Claude Code.
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

#include "core/safety/SwrProtectionController.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR::safety {

SwrProtectionController::SwrProtectionController(QObject* parent)
    : QObject(parent)
{
}

void SwrProtectionController::setEnabled(bool on) noexcept
{
    m_enabled = on;
}

bool SwrProtectionController::isEnabled() const noexcept
{
    return m_enabled;
}

void SwrProtectionController::setLimit(float limit) noexcept
{
    m_limit = limit;
}

void SwrProtectionController::setWindBackEnabled(bool on) noexcept
{
    m_windBackEnabled = on;
}

void SwrProtectionController::setTunePowerSwrIgnore(float watts) noexcept
{
    m_tunePowerSwrIgnore = watts;
}

void SwrProtectionController::setDisableOnTune(bool on) noexcept
{
    m_disableOnTune = on;
}

void SwrProtectionController::setHfPaDisabled(bool on) noexcept
{
    m_hfPaDisabled = on;
}

void SwrProtectionController::setAlexFwdLimit(float watts) noexcept
{
    // console.cs:26067 [v2.10.3.13]: alex_fwd_limit = 5.0f (or 2×ptbPWR.Value for ANAN-8000D)
    m_alexFwdLimit = watts;
}

void SwrProtectionController::setTunePowerSliderValue(int value) noexcept
{
    // console.cs:26020-26057 [v2.10.3.13]: tunePowerSliderValue — bypass only fires when <= 70
    // Clamp to [0, 100] to match Thetis ptbTune.Value / ptbPWR.Value range and
    // prevent a stale -1 sentinel from any caller silently arming the tune-bypass.
    m_tunePowerSliderValue = std::clamp(value, 0, 100);
}

void SwrProtectionController::setHpsdrModel(HPSDRModel model) noexcept
{
    // console.cs:26039 [v2.10.3.15]: HardwareSpecific.Model
    m_model = model;
}

void SwrProtectionController::ingest(float fwdW, float revW, bool tuneActive) noexcept
{
    // From Thetis console.cs:25933-26120 [v2.10.3.13] (PollPAPWR loop)
    // Upstream tags preserved: //N1GP (from cited console.cs:26006) [v2.10.3.15]
    // Verbatim inline tags from the cited range preserved per CLAUDE.md:
    //   console.cs:25961  case HPSDRModel.REDPITAYA: //DH1KLM
    //   console.cs:25980  //[2.10.3.6]MW0LGE modifications to use setup config for swr
    //   console.cs:25985  // in following 'if', K2UE recommends not checking open antenna
    //   console.cs:25987  //-W2PA Changed to allow 35w - some amplifier tuners need about 30w
    //   console.cs:25989  //[2.10.3.6]MW0LGE ignored if tuning, and returned the 10.0f
    //   console.cs:26064  // K2UE idea:  try to determine if Hi-Z or Lo-Z load

    // From Thetis console.cs:25991-26159 [v2.10.3.15] (PollPAPWR, keyed).
    // Every value is worked out first and the changes are emitted once at
    // the end, so a sample never emits a transient value.
    const float prevFactor   = m_protectFactor;
    const bool  prevHighSwr  = m_highSwr;
    const bool  prevWindBack = m_windBackLatched;
    const bool  prevOpen     = m_openAntennaDetected;

    // ── SWR computation ───────────────────────────────────────────────────
    // From Thetis console.cs:26021-26027 [v2.10.3.15]
    // Upstream tags preserved: //MW0LGE (from cited console.cs:26029) [v2.10.3.15]
    float swr = 1.0f;
    float rho = std::sqrt(revW / fwdW);
    if (std::isnan(rho) || std::isinf(rho)) {
        swr = 1.0f;
    } else {
        swr = (1.0f + rho) / (1.0f - rho);
    }

    // Both fwd and rev ≤ 2W floor → clamp SWR to 1.0
    if ((fwdW <= kLowPowerFloor && revW <= kLowPowerFloor) || swr < 1.0f) {
        swr = 1.0f;
    }

    // ── Open-antenna check, behind the Alex / Apollo gate ─────────────────
    // From Thetis console.cs:26029-26065 [v2.10.3.15]:
    //   //[2.10.3.6]MW0LGE modifications to use setup config for swr and tune ignore power. Implements #221 (https://github.com/ramdor/Thetis/issues/221)
    //   if (alexpresent || apollopresent)
    //   {
    //       // open antenna connection dectection, ignored if tuning or 8000 model
    //
    //       // in following 'if', K2UE recommends not checking open antenna for the 8000 model
    //       // if (swrprotection && alex_fwd > 10.0f && (alex_fwd - alex_rev) < 1.0f)
    //       //-W2PA Changed to allow 35w - some amplifier tuners need about 30w to reliably start working
    //       //if (swrprotection && alex_fwd > 35.0f && (alex_fwd - alex_rev) < 1.0f
    //       if (!chkTUN.Checked && (swrprotection && alex_fwd > 10.0f && (alex_fwd - alex_rev) < 1.0f //[2.10.3.6]MW0LGE ignored if tuning, and returned the 10.0f
    //           && HardwareSpecific.Model != HPSDRModel.ANAN8000D)) // open ant condition
    //       {
    //           swr = 50.0f;
    //           NetworkIO.SWRProtect = 0.01f;
    //           chkMOX.Checked = false;
    //           ... checkAntennaWarning(); ...
    //           goto end;
    //       }
    //   }
    //   else
    //   {
    //       swr = 1.0f;
    //       alex_fwd = 0;
    //       alex_rev = 0;
    //   }
    // chkMOX.Checked = false and the warning are the caller's
    // (RadioModel::handlePaTelemetry), which reads openAntennaDetected().
    // NereusSDR also raises highSwr here so the overlay and the TX applet's
    // SWR LED light (Thetis leaves HighSWR as it was).
    bool openAntenna = false;
    if (kAlexPresent || kApolloPresent) {
        // open antenna connection dectection, ignored if tuning or 8000 model
        // [original inline comment from console.cs:26032]
        if (!tuneActive && (m_enabled && fwdW > kOpenAntennaFwdMin
                            && (fwdW - revW) < kOpenAntennaDeltaMax //[2.10.3.6]MW0LGE ignored if tuning, and returned the 10.0f
                            && m_model != HPSDRModel::ANAN8000D)) { // open ant condition
            swr = kOpenAntennaSwr;
            m_protectFactor = kWindBackFactor;
            m_highSwr = true;
            openAntenna = true;
        }
    } else {
        swr = 1.0f;
        fwdW = 0.0f;
        revW = 0.0f;
    }

    if (!openAntenna) {
        // ── Tune-time bypass ──────────────────────────────────────────────
        // From Thetis console.cs:26067-26107 [v2.10.3.15]:
        //   if (chkTUN.Checked && disable_swr_on_tune && (alexpresent || apollopresent))
        //   ...
        //   if (alex_fwd >= 1.0f && alex_fwd <= _tunePowerSwrIgnore && tunePowerSliderValue <= 70)
        //   {
        //       swr_pass = true;
        //   }
        //   else swr_pass = false;
        bool swrPass = false;
        if (tuneActive && m_disableOnTune && (kAlexPresent || kApolloPresent)) {
            if (fwdW >= 1.0f && fwdW <= m_tunePowerSwrIgnore && m_tunePowerSliderValue <= 70) {
                swrPass = true;
            } else {
                swrPass = false;
            }
        }

        // "Disable HF PA": no trip with the HF PA off. NereusSDR has no
        // transverter bands, so the tx_xvtr_index arm never applies.
        // console.cs:26109-26110 [v2.10.3.15]
        //   if (tx_xvtr_index >= 0 || hf_tr_relay)
        //       swr_pass = true;
        if (m_hfPaDisabled) {
            swrPass = true;
        }

        // ── Trip detection + foldback ─────────────────────────────────────
        // From Thetis console.cs:26112-26131 [v2.10.3.15]:
        //   float alex_fwd_limit = 5.0f;
        //   if (HardwareSpecific.Model == HPSDRModel.ANAN8000D)        // K2UE idea:  try to determine if Hi-Z or Lo-Z load
        //       alex_fwd_limit = 2.0f * (float)ptbPWR.Value;        //    by comparing alex_fwd with power setting
        //
        //   if (swr > _swrProtectionLimit && alex_fwd > alex_fwd_limit && swrprotection && !swr_pass)
        //   {
        //       high_swr_count++;
        //       if (high_swr_count >= 4)
        //       {
        //           high_swr_count = 0;
        //           NetworkIO.SWRProtect = (float)(_swrProtectionLimit / (swr + 1.0f));
        //           HighSWR = true;
        //       }
        //   }
        //   else
        //   {
        //       high_swr_count = 0;
        //       NetworkIO.SWRProtect = 1.0f;
        //       HighSWR = false;
        //   }
        // The caller sets alex_fwd_limit (setAlexFwdLimit) before each sample.
        if (swr > m_limit && fwdW > m_alexFwdLimit && m_enabled && !swrPass) {
            m_tripCount++;
            if (m_tripCount >= kTripDebounceCount) {
                m_tripCount = 0;
                m_protectFactor = m_limit / (swr + 1.0f);
                m_highSwr = true;
            }
        } else {
            m_tripCount = 0;
            m_protectFactor = 1.0f;
            // NereusSDR keeps HighSWR up while the wind-back latch holds and
            // protection is on, so the overlay stays lit with the drive
            // folded back (Thetis shows Display.PowerFoldedBack for that);
            // with protection off HighSWR clears as upstream. The trip count
            // and the factor follow Thetis either way; the factor is 0.01
            // once latched (below).
            if (!m_enabled || !m_windBackLatched) {
                m_highSwr = false;
            }
        }

        // ── Catch-all: windback latch ─────────────────────────────────────
        // From Thetis console.cs:26133-26142 [v2.10.3.15]:
        //   //catch all
        //   if (swrprotection && _swr_wind_back_power & !_wind_back_engaged && HighSWR) _wind_back_engaged = true; // this and NetworkIO.SWRProtect reset in UIMOXChangedFalse
        //   if (_wind_back_engaged)
        //   {
        //       NetworkIO.SWRProtect = 0.01f;
        //       Display.PowerFoldedBack = true;
        //   }
        // The latch is not tied to swrprotection once engaged: turning
        // protection off keeps it (and 0.01) until MOX off.
        if (m_enabled && m_windBackEnabled && !m_windBackLatched && m_highSwr) {
            m_windBackLatched = true;
        }
        if (m_windBackLatched) {
            m_protectFactor = kWindBackFactor;
        }
    }

    // ── end: store measured SWR ───────────────────────────────────────────
    // From Thetis console.cs:26143-26149 [v2.10.3.15]:
    //   end:
    //       swr_pass = false;
    //       if (float.IsNaN(swr) || float.IsInfinity(swr) || swr < 1.0f)
    //           alex_swr = 1.0f;
    //       else
    //           alex_swr = swr;
    if (std::isnan(swr) || std::isinf(swr) || swr < 1.0f) {
        m_measuredSwr = 1.0f;
    } else {
        m_measuredSwr = swr;
    }
    m_openAntennaDetected = openAntenna;

    if (prevFactor != m_protectFactor) {
        emit protectFactorChanged(m_protectFactor);
    }
    if (prevHighSwr != m_highSwr) {
        emit highSwrChanged(m_highSwr);
    }
    if (prevWindBack != m_windBackLatched) {
        emit windBackLatchedChanged(m_windBackLatched);
    }
    if (prevOpen != m_openAntennaDetected) {
        emit openAntennaDetectedChanged(m_openAntennaDetected);
    }
}

void SwrProtectionController::onMoxOff() noexcept
{
    // From Thetis console.cs:29241-29246 [v2.10.3.15] (UIMOXChangedFalse)
    //   //[2.10.3.7]MW0LGE reset
    //   NetworkIO.SWRProtect = 1.0f;
    //   _wind_back_engaged = false;
    //   Display.PowerFoldedBack = false;
    //   Audio.HighSWRScale = 1.0;
    //   HighSWR = false;

    float prevFactor = m_protectFactor;
    bool prevHighSwr = m_highSwr;
    bool prevWindBack = m_windBackLatched;
    bool prevOpen = m_openAntennaDetected;

    m_protectFactor       = 1.0f;
    m_highSwr             = false;
    m_windBackLatched     = false;
    m_openAntennaDetected = false;
    // high_swr_count and alex_swr are not reset there: the trip count and
    // the measured SWR carry over to the next key, as in Thetis.

    if (prevFactor != m_protectFactor) {
        emit protectFactorChanged(m_protectFactor);
    }
    if (prevHighSwr != m_highSwr) {
        emit highSwrChanged(m_highSwr);
    }
    if (prevWindBack != m_windBackLatched) {
        emit windBackLatchedChanged(m_windBackLatched);
    }
    if (prevOpen != m_openAntennaDetected) {
        emit openAntennaDetectedChanged(m_openAntennaDetected);
    }
}

float SwrProtectionController::protectFactor() const noexcept
{
    return m_protectFactor;
}

bool SwrProtectionController::highSwr() const noexcept
{
    return m_highSwr;
}

bool SwrProtectionController::windBackLatched() const noexcept
{
    return m_windBackLatched;
}

bool SwrProtectionController::openAntennaDetected() const noexcept
{
    return m_openAntennaDetected;
}

float SwrProtectionController::measuredSwr() const noexcept
{
    return m_measuredSwr;
}

} // namespace NereusSDR::safety
