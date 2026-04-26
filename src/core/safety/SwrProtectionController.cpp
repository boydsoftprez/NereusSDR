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

void SwrProtectionController::ingest(float fwdW, float revW, bool tuneActive) noexcept
{
    // From Thetis console.cs:25933-26120 [v2.10.3.13] (PollPAPWR loop)
    // Verbatim inline tags from the cited range preserved per CLAUDE.md:
    //   console.cs:25961  case HPSDRModel.REDPITAYA: //DH1KLM
    //   console.cs:25980  //[2.10.3.6]MW0LGE modifications to use setup config for swr
    //   console.cs:25985  // in following 'if', K2UE recommends not checking open antenna
    //   console.cs:25987  //-W2PA Changed to allow 35w - some amplifier tuners need about 30w
    //   console.cs:25989  //[2.10.3.6]MW0LGE ignored if tuning, and returned the 10.0f
    //   console.cs:26064  // K2UE idea:  try to determine if Hi-Z or Lo-Z load

    if (!m_enabled) {
        // When protection is disabled, always pass-through at full power
        // and clear all latched/observable state so re-enable starts clean.
        // (Codex P2 follow-up to PR #139: previously left m_windBackLatched,
        //  m_openAntennaDetected, and m_tripCount stale; phantom fault
        //  state would survive into the re-enabled path.)
        if (m_protectFactor != 1.0f) {
            m_protectFactor = 1.0f;
            emit protectFactorChanged(m_protectFactor);
        }
        if (m_highSwr) {
            m_highSwr = false;
            emit highSwrChanged(m_highSwr);
        }
        if (m_windBackLatched) {
            m_windBackLatched = false;
            emit windBackLatchedChanged(false);
        }
        if (m_openAntennaDetected) {
            m_openAntennaDetected = false;
            emit openAntennaDetectedChanged(false);
        }
        m_tripCount = 0;
        return;
    }

    // ── SWR computation ───────────────────────────────────────────────────
    // From Thetis console.cs:25972-25978 [v2.10.3.13]
    // Tag preserved: //[2.10.3.6]MW0LGE (console.cs:25980 — SWR/tune config)

    float swr = 1.0f;
    float rho = std::sqrt(revW / fwdW);
    if (std::isnan(rho) || std::isinf(rho)) {
        swr = 1.0f;
    } else {
        swr = (1.0f + rho) / (1.0f - rho);
    }

    // Both fwd and rev ≤ 2W floor → clamp SWR to 1.0
    // console.cs:25978 [v2.10.3.13]
    if ((fwdW <= kLowPowerFloor && revW <= kLowPowerFloor) || swr < 1.0f) {
        swr = 1.0f;
    }

    // ── Open-antenna heuristic ────────────────────────────────────────────
    // console.cs:25989-26009 [v2.10.3.6]MW0LGE
    // in following 'if', K2UE recommends not checking open antenna for the 8000 model
    // if (swrprotection && alex_fwd > 10.0f && (alex_fwd - alex_rev) < 1.0f)
    //-W2PA Changed to allow 35w - some amplifier tuners need about 30w to reliably start working
    //if (swrprotection && alex_fwd > 35.0f && (alex_fwd - alex_rev) < 1.0f
    // [2.10.3.6]MW0LGE ignored if tuning, and returned the 10.0f
    if (!tuneActive && fwdW > kOpenAntennaFwdMin && (fwdW - revW) < kOpenAntennaDeltaMax) {
        // open ant condition
        swr = kOpenAntennaSwr;

        m_measuredSwr = swr;

        bool prevOpen = m_openAntennaDetected;
        m_openAntennaDetected = true;

        float prevFactor = m_protectFactor;
        m_protectFactor = kWindBackFactor;

        bool prevHighSwr = m_highSwr;
        m_highSwr = true;

        if (prevFactor != m_protectFactor) {
            emit protectFactorChanged(m_protectFactor);
        }
        if (prevHighSwr != m_highSwr) {
            emit highSwrChanged(m_highSwr);
        }
        if (prevOpen != m_openAntennaDetected) {
            emit openAntennaDetectedChanged(m_openAntennaDetected);
        }
        return;
    }

    // Clear open-antenna flag when condition no longer holds
    if (m_openAntennaDetected) {
        m_openAntennaDetected = false;
        emit openAntennaDetectedChanged(false);
    }

    // ── Tune-time bypass ──────────────────────────────────────────────────
    // console.cs:26020-26057 [v2.10.3.13]
    bool swrPass = false;
    if (tuneActive && m_disableOnTune) {
        // alex_fwd >= 1.0f && alex_fwd <= _tunePowerSwrIgnore
        if (fwdW >= 1.0f && fwdW <= m_tunePowerSwrIgnore) {
            swrPass = true;
        }
    }

    // ── Trip detection + foldback / windback ──────────────────────────────
    // console.cs:26067-26091 [v2.10.3.13]

    // TODO [3M-1a]: add `alex_fwd > alex_fwd_limit` floor (5W default,
    //   2× power-slider for ANAN-8000D) to suppress false trips during TX
    //   ramp-up. Mirrors console.cs:26067 [v2.10.3.13].
    // TODO [3M-1a]: add the `tunePowerSliderValue <= 70` override to the
    //   tune-bypass block when the power slider value is exposed by the
    //   integration in Task 17. Mirrors console.cs:26020-26057 [v2.10.3.13].
    //
    // Both conditions are intentionally deferred — SwrProtectionController
    // is inert until 3M-1a wires it into the MOX path.
    if (swr > m_limit && !swrPass) {
        m_tripCount++;
        if (m_tripCount >= kTripDebounceCount) {
            // console.cs:26070-26075 [v2.10.3.13]
            m_tripCount = 0;

            float newFactor = m_limit / (swr + 1.0f);
            bool prevHighSwr = m_highSwr;
            m_highSwr = true;

            float prevFactor = m_protectFactor;
            m_protectFactor = newFactor;

            if (prevFactor != m_protectFactor) {
                emit protectFactorChanged(m_protectFactor);
            }
            if (prevHighSwr != m_highSwr) {
                emit highSwrChanged(m_highSwr);
            }
        }
        // (below 4 trips: factor unchanged until trip debounce fires)
    } else if (!m_windBackLatched) {
        // Below limit and not latched: full power, clear trip counter
        // console.cs:26078-26082 [v2.10.3.13]
        m_tripCount = 0;

        float prevFactor = m_protectFactor;
        bool prevHighSwr = m_highSwr;

        m_protectFactor = 1.0f;
        m_highSwr = false;

        if (prevFactor != m_protectFactor) {
            emit protectFactorChanged(m_protectFactor);
        }
        if (prevHighSwr != m_highSwr) {
            emit highSwrChanged(m_highSwr);
        }
    }

    // ── Catch-all: windback latch ─────────────────────────────────────────
    // console.cs:26083-26090 [v2.10.3.13]
    // if (swrprotection && _swr_wind_back_power & !_wind_back_engaged && HighSWR)
    //     _wind_back_engaged = true; // this and NetworkIO.SWRProtect reset in UIMOXChangedFalse
    if (m_windBackEnabled && !m_windBackLatched && m_highSwr) {
        m_windBackLatched = true;
        emit windBackLatchedChanged(true);
    }

    if (m_windBackLatched) {
        // console.cs:26087-26088 [v2.10.3.13]
        float prevFactor = m_protectFactor;
        m_protectFactor = kWindBackFactor;
        if (prevFactor != m_protectFactor) {
            emit protectFactorChanged(m_protectFactor);
        }
    }

    // ── Store measured SWR ────────────────────────────────────────────────
    // console.cs:26096-26099 [v2.10.3.13] (alex_swr assignment after end: label)
    if (std::isnan(swr) || std::isinf(swr) || swr < 1.0f) {
        m_measuredSwr = 1.0f;
    } else {
        m_measuredSwr = swr;
    }
}

void SwrProtectionController::onMoxOff() noexcept
{
    // From Thetis console.cs:29191-29195 [v2.10.3.13] (UIMOXChangedFalse)
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
    m_tripCount           = 0;
    m_measuredSwr         = 1.0f;

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
