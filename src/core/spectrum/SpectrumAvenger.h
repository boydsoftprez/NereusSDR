#pragma once

// =================================================================
// src/core/spectrum/SpectrumAvenger.h  (NereusSDR)
// =================================================================
//
// Ported from WDSP source:
//   third_party/wdsp/src/analyzer.c:464-554 [v2.10.3.13] —
//   verbatim port of the avenger() function (mirrors Thetis
//   Project Files/Source/wdsp/analyzer.c at v2.10.3.13).
//
//   The WDSP avenger applies the chosen averaging policy to the
//   linear-power pixel array produced by the detector (see
//   SpectrumDetector.h) and converts to dB at the appropriate
//   point per av_mode.  Wraps the upstream function in a class
//   that owns the per-channel state (av_sum running accumulator,
//   av_buff frame ring, indices) so callers don't manage them.
//
//   Modes:
//     -1  peak hold      — av_sum tracks max linear power seen
//      0  none           — pass-through (10·log₁₀ each frame)
//      1  recursive lin  — exponential smooth in linear, then dB
//      2  window lin     — N-frame moving average in linear, dB
//      3  recursive log  — convert to dB, then exponential smooth
//
//   Original WDSP source license preserved in SpectrumAvenger.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-05 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//                 Class wrapper around WDSP avenger() function;
//                 owns the per-channel state arrays (av_sum,
//                 av_buff, indices) so SpectrumWidget can hold
//                 separate spectrum + waterfall instances.
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

#include <QVector>

namespace NereusSDR {

/// Frame-averaging policy + linear→dB conversion stage of the WDSP
/// analyzer pipeline.  Owns per-channel state (av_sum running
/// accumulator, av_buff frame ring) so spectrum and waterfall keep
/// independent state.
///
/// Apply this to the linear-power pixels produced by
/// applySpectrumDetector(); output is in dB (10·log₁₀).
///
/// All av_mode integer values match Thetis's wire format from
/// analyzer.c:464 [v2.10.3.13]:
///
///   -1 = peak-hold           (analyzer.c:485-494)
///    0 = no averaging        (analyzer.c:495-501)
///    1 = recursive linear    (analyzer.c:502-511)
///    2 = window linear       (analyzer.c:512-539)
///    3 = recursive log       (analyzer.c:540-549)
class SpectrumAvenger {
public:
    /// Maximum frames retained in the window-averaging ring.  From
    /// WDSP comm.h:120 [v2.10.3.13]: dMAX_AVERAGE = 60.
    static constexpr int kMaxAverage = 60;

    /// (Re-)allocate state arrays for a given pixel count.  Called
    /// when display width / FFT size change.  Resets all running
    /// accumulators.
    void resize(int numPixels);

    /// Set the window-averaging frame depth (av_mode == 2).
    /// Clamped to [1, kMaxAverage].  Reduces dMAX_AVERAGE per
    /// analyzer.c:535-537 ring-wrap logic.  Resets indices.
    void setNumAverage(int numFrames);

    /// Wipe all accumulators.  Use when av_mode changes, FFT size
    /// changes, or sample rate changes — anything that invalidates
    /// the running average.  Mirrors analyzer.c:SetDisplayAverageMode
    /// re-init at line 1854 [v2.10.3.13].
    void clear();

    /// Apply averaging + log conversion.  Mirrors WDSP
    /// avenger() at analyzer.c:464-554 [v2.10.3.13].
    ///
    ///   tPixels      — input (linear power, length numPixels — must
    ///                  match the resize() argument)
    ///   avMode       — -1 / 0 / 1 / 2 / 3 (see class header)
    ///   avBackmult   — recursive averaging multiplier (alpha-style;
    ///                  used by modes 1 + 3)
    ///   scale        — overall power-domain scale factor (Thetis
    ///                  applies window-power-gain compensation
    ///                  here; NereusSDR can pass 1.0 if FFTEngine
    ///                  already normalised)
    ///   correction   — per-pixel correction factor (Thetis sub-band
    ///                  gain compensation; pass empty for no
    ///                  correction — equivalent to all 1.0)
    ///   normalize    — if true, add normOneHzDb to each output
    ///                  pixel (1-Hz BW normalisation)
    ///   normOneHzDb  — additive offset applied when normalize=true
    ///   pixelsOut    — output (length numPixels, in dB)
    ///
    /// Pre-condition: resize(numPixels) was called and tPixels is
    /// the same length.  pixelsOut is resized if needed.
    void apply(const QVector<float>& tPixels,
               int avMode,
               double avBackmult,
               double scale,
               const QVector<double>& correction,
               bool normalize,
               double normOneHzDb,
               QVector<float>& pixelsOut);

    int  numPixels()   const { return m_avSum.size(); }
    int  numAverage()  const { return m_numAverage; }
    int  availFrames() const { return m_availFrames; }

private:
    // av_sum at analyzer.h:81 [v2.10.3.13] — per-pixel accumulator
    // whose semantic depends on av_mode (peak max, linear sum,
    // log running EMA, etc).
    QVector<double> m_avSum;

    // av_buff at analyzer.h:83 [v2.10.3.13] — ring of past frames
    // used by window-averaging mode (av_mode == 2).  Sized
    // numPixels × kMaxAverage; only the first numAverage rows are
    // walked at runtime.
    QVector<QVector<double>> m_avBuff;

    int m_numAverage{2};   // av_mode==2 frame count, [1, kMaxAverage]
    int m_avInIdx{0};
    int m_avOutIdx{0};
    int m_availFrames{0};
};

}  // namespace NereusSDR
