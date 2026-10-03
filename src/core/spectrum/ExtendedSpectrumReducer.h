#pragma once
// =================================================================
// src/core/spectrum/ExtendedSpectrumReducer.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/HPSDR/specHPSDR.cs:529-535
//   [v2.10.3.15 @3759d09] -- CLIP_FRACTION and its floor convention.
//
// The remaining composition is an extraction of the existing local
// SpectrumWidget extended-view path.  Its DDC detector and avenger are the
// existing WDSP ports, and its ADC reference remains in
// WidebandDisplayReference.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-22  J.J. Boyd / KG4VCF  Remote daemon R3, Core extraction.
//                                    AI-assisted transformation via
//                                    OpenAI Codex.
// =================================================================

/*
*
* Copyright (C) 2010-2018  Doug Wigley 
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "core/spectrum/SpectrumAvenger.h"
#include "core/spectrum/SpectrumReducer.h"

#include <QVector>

#include <utility>

namespace NereusSDR {

/// Compose the clipped DDC island and physical-ADC wideband wings into one
/// display row, then apply one per-plane WDSP avenger.  This is used only by
/// an extended view; ordinary DDC rows remain SpectrumReducer's responsibility.
class ExtendedSpectrumReducer
{
public:
    /// Fraction of the DDC spectrum discarded at EACH edge. From Thetis
    /// specHPSDR.cs:529 [v2.10.3.15] -- `const double CLIP_FRACTION = 0.04;`
    /// with its `clip = floor(CLIP_FRACTION * fft_size)` at :535. The local
    /// renderer uses this same value before laying out the island.
    static constexpr double kDdcClipFraction = 0.04;

    void setConfig(const ReducerConfig& cfg) { m_cfg = cfg; }
    const ReducerConfig& config() const { return m_cfg; }
    void clearAveraging() { m_avenger.clear(); }

    /// Reduce a local DDC linear-power row plus an optional raw-dB physical
    /// ADC row. ddcRawDbmOffset calibrates the DDC FFT domain; stationOffsetDb
    /// is the existing shared RX display scalar and is applied exactly once to
    /// both valid regions. floorDbm is already final display dBm.
    ///
    /// Returns false for malformed or numerically unsafe input and leaves out
    /// untouched. An empty ADC row is valid: every wing is floorDbm.
    bool reduce(const QVector<float>& ddcBinsLinear,
                double ddcWindowEnb,
                double ddcRawDbmOffset,
                const QVector<float>& adcRawDb,
                double adcRateHz,
                double stationOffsetDb,
                double floorDbm,
                QVector<float>& out);

    /// Pixel span [first, last] occupied by the clipped listenable DDC island
    /// in an extended window, or {0, -1} when it does not overlap the window.
    static std::pair<int, int> listenableIslandPixels(const ReducerConfig& cfg);

private:
    ReducerConfig m_cfg;
    QVector<float> m_linearPixels;
    SpectrumAvenger m_avenger;
};

} // namespace NereusSDR
