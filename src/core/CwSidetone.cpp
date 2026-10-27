// =================================================================
// src/core/CwSidetone.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis ChannelMaster sources:
//   Project Files/Source/ChannelMaster/sidetone.c
//   Project Files/Source/ChannelMaster/sidetone.h
// original licence from Thetis source is included below. sidetone.h
// carries one Windows-1252 byte (0x85) that tools read as a NEL line
// terminator; it sits in the file's closing SYMBOL GENERATION comment,
// not in the header copied here, which is unchanged.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the computer sidetone (R-CW-39, R-CW-51):
//               reimplemented in C++20 for NereusSDR from sidetone.c and
//               sidetone.h, lock-free for the audio drain. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

// --- From sidetone.c ---
/*  sidetone.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2024 Warren Pratt, NR0V

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

warren@pratt.one

*/

// --- From sidetone.h ---
/*  sidetone.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2024 Warren Pratt, NR0V

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

warren@pratt.one

*/

#include "core/CwSidetone.h"

#include <cmath>
#include <numbers>

namespace NereusSDR {

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

} // namespace

// ---------------------------------------------------------------------------
// Control thread
// ---------------------------------------------------------------------------

// From Thetis sidetone.c:98-106 [v2.10.3.15] (calc_sidetone) and
// sidetone.c:290-298 (setSidetoneRate): the tone and both edges for the
// rate, the machine back to LOW. calc_wpm_times is not ported (no dot or
// dash bits here).
void CwSidetone::configure(int sampleRate, int blockSize)
{
    m_rate = sampleRate > 0 ? sampleRate : 0;
    m_size = blockSize > 0 ? blockSize : 0;
    m_edgetype = m_edgeTypeWanted.load(std::memory_order_acquire);
    m_edgelength = m_edgeSecondsWanted.load(std::memory_order_acquire);
    calcRisingEdge();
    calcFallingEdge();
    // The tone is recomputed at the next render for the new rate.
    m_pitch = 0.0;
    m_state = LOW;
    m_ntimer = 0;
}

// From Thetis sidetone.c:402-409 [v2.10.3.15] (SetSidetonePitch); the
// audio thread applies it (calc_tone1) at its next render.
void CwSidetone::setPitchHz(double hz)
{
    if (!(hz > 0.0)) {
        return;
    }
    m_pitchHz.store(hz, std::memory_order_release);
}

// From Thetis sidetone.c:355-361 [v2.10.3.15] (SetSidetoneVolume).
void CwSidetone::setVolume(double v)
{
    m_volume.store(v, std::memory_order_release);
}

// From Thetis sidetone.c:384-390 [v2.10.3.15] (SetSidetoneRun).
void CwSidetone::setRun(bool on)
{
    m_run.store(on, std::memory_order_release);
}

// From Thetis sidetone.c:411-435 [v2.10.3.15] (SetSidetoneEdgetype,
// SetSidetoneEdgelength), applied by the next configure() so the tables
// are never rebuilt under the audio thread.
void CwSidetone::setEdge(int edgeType, double edgeSeconds)
{
    m_edgeTypeWanted.store(edgeType, std::memory_order_release);
    if (edgeSeconds > 0.0) {
        m_edgeSecondsWanted.store(edgeSeconds, std::memory_order_release);
    }
}

// From Thetis sidetone.c:317-325 [v2.10.3.15] (keySidetone).
void CwSidetone::setKey(bool down)	// 0 = KEY-UP; 1 = KEY-DOWN
{
    m_key.store(down, std::memory_order_release);
}

// From Thetis sidetone.c:31-39 [v2.10.3.15] (calc_tone1).
void CwSidetone::calcTone1()
{
    m_tonePhs = 0.0;
    m_toneCosPhs = std::cos(m_tonePhs);
    m_toneSinPhs = std::sin(m_tonePhs);
    m_toneDelta = kTwoPi * m_pitch / static_cast<double>(m_rate);
    m_toneCosdelta = std::cos(m_toneDelta);
    m_toneSindelta = std::sin(m_toneDelta);
}

// From Thetis sidetone.c:41-58 [v2.10.3.15] (calc_rising_edge).
void CwSidetone::calcRisingEdge()
{
    m_nrise = static_cast<int>(m_edgelength * m_rate);
    if (m_nrise < 1) {
        m_nrise = 1;
    }
    m_riseSamps.assign(static_cast<size_t>(m_nrise) + 1, 0.0);
    switch (m_edgetype)
    {
    case 0:		// raised-cosine
    default:
        m_riseDelta = kPi / static_cast<double>(m_nrise);
        m_riseTheta = 0.0;
        for (int i = 0; i <= m_nrise; i++)
        {
            m_riseSamps[static_cast<size_t>(i)] = 0.5 * (1.0 - std::cos(m_riseTheta));
            m_riseTheta += m_riseDelta;
        }
        break;
    }
}

// From Thetis sidetone.c:65-82 [v2.10.3.15] (calc_falling_edge).
void CwSidetone::calcFallingEdge()
{
    m_nfall = static_cast<int>(m_edgelength * m_rate);
    if (m_nfall < 1) {
        m_nfall = 1;
    }
    m_fallSamps.assign(static_cast<size_t>(m_nfall) + 1, 0.0);
    switch (m_edgetype)
    {
    case 0:		// raised-cosine
    default:
        m_fallDelta = kPi / static_cast<double>(m_nfall);
        m_fallTheta = kPi;
        for (int i = 0; i <= m_nfall; i++)
        {
            m_fallSamps[static_cast<size_t>(i)] = 0.5 * (1.0 - std::cos(m_fallTheta));
            m_fallTheta -= m_fallDelta;
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Audio thread
// ---------------------------------------------------------------------------

// From Thetis sidetone.c:172-176 [v2.10.3.15] (osc_init).
void CwSidetone::oscInit()
{
    m_toneCosPhs = std::cos(m_tonePhs);
    m_toneSinPhs = std::sin(m_tonePhs);
}

// From Thetis sidetone.c:178-191 [v2.10.3.15] (osc).
void CwSidetone::osc()
{
    m_toneOut0 = +(m_toneCosPhs);
    m_toneOut1 = -(m_toneSinPhs);
    const double t1 = m_toneCosPhs;
    const double t2 = m_toneSinPhs;
    m_toneCosPhs = t1 * m_toneCosdelta - t2 * m_toneSindelta;
    m_toneSinPhs = t1 * m_toneSindelta + t2 * m_toneCosdelta;
    m_tonePhs += m_toneDelta;
    if (m_tonePhs >= kTwoPi) {
        m_tonePhs -= kTwoPi;
    }
    if (m_tonePhs < 0.0) {
        m_tonePhs += kTwoPi;
    }
}

// From Thetis sidetone.c:193-288 [v2.10.3.15] (xsidetone), the sidetone
// output only: out_st[2 * i + 0] = outI * volume_st, out_st[2 * i + 1] =
// outQ * volume_st, so left and right are the tone's I and Q. The dot and
// dash terms of each key test are gone with the bits (the paddle never
// keys it); with only the key, RISE leaves ntimer at nrise + 1, which is
// HIGH's shortest hold, as upstream.
bool CwSidetone::render(float* out, int frames)
{
    if (!m_run.load(std::memory_order_acquire) || out == nullptr || frames <= 0
        || m_rate <= 0) {
        return false;
    }
    const double pitch = m_pitchHz.load(std::memory_order_acquire);
    if (pitch != m_pitch) {
        m_pitch = pitch;
        calcTone1();
    }
    const double volume = m_volume.load(std::memory_order_acquire);

    oscInit();
    for (int i = 0; i < frames; i++)
    {
        const bool key = m_key.load(std::memory_order_acquire);
        osc();
        switch (m_state)
        {
        case LOW:
            m_outI = 0.0;
            m_outQ = 0.0;
            if (key) {
                m_state = RISE;
            }
            m_ntimer = 0;
            break;
        case RISE:
            m_outI = m_riseSamps[static_cast<size_t>(m_ntimer)] * m_toneOut0;
            m_outQ = m_riseSamps[static_cast<size_t>(m_ntimer)] * m_toneOut1;
            ++m_ntimer;
            if (m_ntimer > m_nrise)
            {
                if (key)
                {
                    m_state = HIGH;
                }
                else
                {
                    m_state = FALL;
                    m_ntimer = 0;
                }
            }
            break;
        case HIGH:
            m_outI = m_toneOut0;
            m_outQ = m_toneOut1;
            if (m_ntimer > 0) {
                --m_ntimer;
            }
            if (m_ntimer <= 0 && !key)
            {
                m_state = FALL;
                m_ntimer = 0;
            }
            break;
        case FALL:
            m_outI = m_fallSamps[static_cast<size_t>(m_ntimer)] * m_toneOut0;
            m_outQ = m_fallSamps[static_cast<size_t>(m_ntimer)] * m_toneOut1;
            ++m_ntimer;
            if (m_ntimer > m_nfall)
            {
                if (key)
                {
                    m_state = RISE;
                    m_ntimer = 0;
                }
                else
                {
                    m_state = LOW;
                    m_ntimer = 0;
                }
            }
            break;
        default:
            m_state = LOW;
            m_ntimer = 0;
            break;
        }
        out[2 * i + 0] = static_cast<float>(m_outI * volume);
        out[2 * i + 1] = static_cast<float>(m_outQ * volume);
    }
    return true;
}

} // namespace NereusSDR
