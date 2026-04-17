// =================================================================
// src/core/NoiseFloorEstimator.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

// src/core/NoiseFloorEstimator.h
//
// Percentile-based noise-floor estimator for the Clarity adaptive display
// tuning feature (Phase 3G-9c). Given a frame of FFT bin magnitudes in dB,
// returns the dB value at a configured percentile (default 30th).
//
// The percentile-based approach is inherently robust against strong local
// carriers — a carrier sits above the 30th percentile cutoff without moving
// the estimate. Contrast with Thetis's processNoiseFloor() in display.cs:5866
// which uses a caller-filtered linear average; Clarity uses a percentile so
// no upstream filter is needed.
//
// Algorithm: std::nth_element partial sort on a reusable work buffer.
// O(n) average case, reused across calls so no per-frame allocation.
//
// Pure math — no Qt signals/slots, not a QObject.

#pragma once

#include <QVector>

namespace NereusSDR {

class NoiseFloorEstimator {
public:
    // percentile in [0.0, 1.0]. 0.30 = 30th percentile, the Clarity default
    // locked in 2026-04-15-display-refactor-design.md §6.2.2.
    explicit NoiseFloorEstimator(float percentile = 0.30f);

    void  setPercentile(float p);
    float percentile() const noexcept { return m_percentile; }

    // Returns the dB value at the configured percentile of `bins`.
    // `bins` is read-only — internally copied to a reusable work buffer
    // so callers can reuse their own vectors without surprise mutations.
    // Returns qQNaN() if `bins` is empty.
    float estimate(const QVector<float>& bins);

private:
    float          m_percentile;
    QVector<float> m_workBuf;
};

}  // namespace NereusSDR
