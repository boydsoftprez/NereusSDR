// =================================================================
// src/core/spectrum/SpectrumAvenger.cpp  (NereusSDR)
// =================================================================
//
// Ported from WDSP source:
//   third_party/wdsp/src/analyzer.c:464-554 [v2.10.3.13] —
//   verbatim port of the avenger() function.
//
// All inline comments preserved per CLAUDE.md "Inline comment
// preservation" rule.  Algorithm, switch ordering, and 10·log₁₀
// placement are byte-for-byte equivalent — translation is C++
// idiom (std::log10, namespace) and class-owned state replacing
// the upstream pointer-array parameter pattern.
//
// Original WDSP source license preserved verbatim in
// SpectrumAvenger.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-05 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-08-02  J.J. Boyd / KG4VCF  Relocated from src/gui/spectrum/ to
//                                    src/core/spectrum/ for the headless
//                                    daemon build split (R1).  No logic
//                                    change.  AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

//=================================================================
// analyzer.c
//=================================================================
//  This file is part of a program that implements a Spectrum Analyzer
//  used in conjunction with software-defined-radio hardware.
//
//  Copyright (C) 2012, 2013, 2014, 2016, 2023, 2025 Warren Pratt, NR0V
//  Copyright (C) 2012 David McQuate, WA8YWQ - Kaiser window & Bessel function added.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//  The author can be reached by email at
//
//  warren@wpratt.com
//=================================================================

#include "SpectrumAvenger.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// Tiny epsilon added inside log10 to avoid log(0).  Matches Thetis
// avenger() at analyzer.c:491 / 499 / 508 / 521 / 531 / 545
// [v2.10.3.13] — the upstream uses 1.0e-60 for log-domain floors.
constexpr double kLogEpsilon = 1.0e-60;

// Helper: 10·log₁₀(x + eps).  Mirrors mlog10 wrapper in WDSP comm.h.
inline double tenLog10(double x)
{
    return 10.0 * std::log10(x + kLogEpsilon);
}

// Helper: returns correction[i] if the array is non-empty, else 1.0.
// Matches Thetis cd[i] argument when no per-pixel compensation is
// desired (NereusSDR call shape — we don't implement Thetis
// sub-band-segment gain compensation).
inline double cd(const QVector<double>& correction, int i)
{
    return correction.isEmpty() ? 1.0 : correction[i];
}

}  // namespace

void SpectrumAvenger::resize(int numPixels)
{
    if (numPixels <= 0) {
        m_avSum.clear();
        m_avBuff.clear();
        m_availFrames = 0;
        m_avInIdx = 0;
        m_avOutIdx = 0;
        return;
    }
    // resize() + fill() instead of assign() — QVector::assign was added in
    // Qt 6.6 but Linux CI runs on Qt 6.4 (deb12 default).  Same effect,
    // wider compatibility.
    m_avSum.resize(numPixels);
    m_avSum.fill(0.0);
    m_avBuff.resize(kMaxAverage);
    for (int j = 0; j < kMaxAverage; ++j) {
        m_avBuff[j].resize(numPixels);
        m_avBuff[j].fill(0.0);
    }
    m_availFrames = 0;
    m_avInIdx = 0;
    m_avOutIdx = 0;
}

void SpectrumAvenger::setNumAverage(int numFrames)
{
    m_numAverage = std::max(1, std::min(numFrames, kMaxAverage));
    m_availFrames = 0;
    m_avInIdx = 0;
    m_avOutIdx = 0;
}

void SpectrumAvenger::clear()
{
    // From Thetis SetDisplayAverageMode at analyzer.c:1854-1872 [v2.10.3.13]:
    // re-initialise av_sum based on the (new) av_mode.  Without knowing the
    // mode here we conservatively zero everything; SpectrumWidget calls
    // clear() right before flipping av_mode, then the next apply() seeds
    // the appropriate initial values per-mode (recursive linear starts at
    // 1.0e-12, recursive log at -160.0 dB, window mode resets indices).
    std::fill(m_avSum.begin(), m_avSum.end(), 0.0);
    m_availFrames = 0;
    m_avInIdx = 0;
    m_avOutIdx = 0;
}

// From Thetis WDSP analyzer.c:464-554 [v2.10.3.13] — avenger() verbatim port.
// Class wraps state pointers (av_sum, av_buff, in/out indices) that the
// upstream function received as parameters.
void SpectrumAvenger::apply(const QVector<float>& tPixels,
                            int avMode,
                            double avBackmult,
                            double scale,
                            const QVector<double>& correction,
                            bool normalize,
                            double normOneHzDb,
                            QVector<float>& pixelsOut)
{
    const int num_pixels = m_avSum.size();
    if (num_pixels == 0 || tPixels.size() != num_pixels) {
        pixelsOut.clear();
        return;
    }
    if (pixelsOut.size() != num_pixels) {
        pixelsOut.resize(num_pixels);
    }

    int i;
    double factor;
    switch (avMode)
    {
    case -1:    // peak-hold
        {
            for (i = 0; i < num_pixels; i++)
            {
                if (static_cast<double>(tPixels[i]) > m_avSum[i])
                    m_avSum[i] = tPixels[i];
                pixelsOut[i] = static_cast<float>(tenLog10(scale * cd(correction, i) * m_avSum[i]));
            }
            break;
        }
    case 0:     // no averaging
    default:
        {
            for (i = 0; i < num_pixels; i++)
                pixelsOut[i] = static_cast<float>(tenLog10(scale * cd(correction, i) * tPixels[i]));
            break;
        }
    case 1:     // weighted averaging of linear data
        {
            double onem_avb = 1.0 - avBackmult;
            for (i = 0; i < num_pixels; i++)
            {
                m_avSum[i] = avBackmult * m_avSum[i] + onem_avb * tPixels[i];
                pixelsOut[i] = static_cast<float>(tenLog10(scale * cd(correction, i) * m_avSum[i]));
            }
            break;
        }
    case 2:     // window averaging of linear data
        {
            if (m_availFrames < m_numAverage)
            {
                factor = scale / static_cast<double>(++m_availFrames);
                for (i = 0; i < num_pixels; i++)
                {
                    m_avSum[i] += tPixels[i];
                    m_avBuff[m_avInIdx][i] = tPixels[i];
                    pixelsOut[i] = static_cast<float>(tenLog10(cd(correction, i) * m_avSum[i] * factor));
                }
            }
            else
            {
                factor = scale / static_cast<double>(m_availFrames);
                for (i = 0; i < num_pixels; i++)
                {
                    m_avSum[i] += tPixels[i] - m_avBuff[m_avOutIdx][i];
                    m_avBuff[m_avInIdx][i] = tPixels[i];
                    pixelsOut[i] = static_cast<float>(tenLog10(cd(correction, i) * m_avSum[i] * factor));
                }
                if (++m_avOutIdx == kMaxAverage)
                    m_avOutIdx = 0;
            }
            if (++m_avInIdx == kMaxAverage)
                m_avInIdx = 0;
            break;
        }
    case 3:     // weighted averaging of log data - looks nice, not accurate for time-varying signals
        {
            double onem_avb = 1.0 - avBackmult;
            for (i = 0; i < num_pixels; i++)
            {
                m_avSum[i] = avBackmult * m_avSum[i]
                           + onem_avb * tenLog10(scale * cd(correction, i) * tPixels[i]);
                pixelsOut[i] = static_cast<float>(m_avSum[i]);
            }
            break;
        }
    }
    if (normalize) {
        for (i = 0; i < num_pixels; i++)
            pixelsOut[i] += static_cast<float>(normOneHzDb);
    }
}

}  // namespace NereusSDR
