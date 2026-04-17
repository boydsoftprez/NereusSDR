#pragma once

// =================================================================
// src/models/PanadapterModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   Copyright (C) 2017-2019  Chris Codella (W2PA) — console.cs / setup.cs / NetworkIO.cs inline mods
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2020-2025  Joe (WD5Y) — console.cs UI tweaks / mute / ForeColor
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — pattern-level contributions (https://github.com/ten9876/AetherSDR)
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
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
//                 Claude Code. Per-panadapter display-state template from AetherSDR
//                 `src/models/PanadapterModel.{h,cpp}`; per-band grid
//                 storage added in Phase 3G-8 ports Thetis
//                 `console.cs` / `display.cs`.
// =================================================================

#include "Band.h"

#include <QHash>
#include <QObject>

#include <limits>

namespace NereusSDR {

// Per-band grid scale storage. Added in Phase 3G-8 (commit 2).
// dbStep is NOT per-band — Thetis keeps it as a single global value
// (verified console.cs:14242-14436).
// clarityFloor added in Phase 3G-9c: Clarity adaptive tuning stores the
// smoothed noise floor per band so band-switch can snap instantly.
struct BandGridSettings {
    int   dbMax;
    int   dbMin;
    float clarityFloor = std::numeric_limits<float>::quiet_NaN();
};

// Represents a single panadapter display.
// In NereusSDR, panadapters are entirely client-side — the radio sends
// raw I/Q, and the client computes FFT data for display. This model
// holds display state (center frequency, bandwidth, dBm range, per-band
// grid slots, current band).
class PanadapterModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(double centerFrequency READ centerFrequency WRITE setCenterFrequency NOTIFY centerFrequencyChanged)
    Q_PROPERTY(double bandwidth       READ bandwidth       WRITE setBandwidth       NOTIFY bandwidthChanged)
    Q_PROPERTY(int    dBmFloor        READ dBmFloor        WRITE setdBmFloor        NOTIFY levelChanged)
    Q_PROPERTY(int    dBmCeiling      READ dBmCeiling      WRITE setdBmCeiling      NOTIFY levelChanged)

public:
    explicit PanadapterModel(QObject* parent = nullptr);
    ~PanadapterModel() override;

    double centerFrequency() const { return m_centerFrequency; }
    // Setting the center frequency auto-derives the band via
    // bandFromFrequency() and calls setBand() on boundary crossing.
    // Direct setBand() is also callable when that's the wrong choice
    // (e.g. XVTR mode, or band button click that shouldn't auto-switch
    // on every subsequent VFO tweak).
    void setCenterFrequency(double freq);

    double bandwidth() const { return m_bandwidth; }
    void setBandwidth(double bw);

    int dBmFloor() const { return m_dBmFloor; }
    void setdBmFloor(int floor);

    int dBmCeiling() const { return m_dBmCeiling; }
    void setdBmCeiling(int ceiling);

    int fftSize() const { return m_fftSize; }
    void setFftSize(int size);

    int averageCount() const { return m_averageCount; }
    void setAverageCount(int count);

    // ---- Per-band grid (Phase 3G-8 commit 2) ----

    // Currently-active band. Changing the band updates dBmFloor/dBmCeiling
    // to the stored slot for the new band and emits levelChanged()
    // automatically so existing SpectrumWidget wiring reacts.
    Band band() const { return m_band; }
    void setBand(Band b);

    // Per-band grid slot accessors. dbMax/dbMin are persisted to
    // AppSettings on every write. Reading an unset band returns the
    // Thetis uniform default (-40 / -140) that was installed at
    // construction time.
    BandGridSettings perBandGrid(Band b) const;
    void setPerBandDbMax(Band b, int dbMax);
    void setPerBandDbMin(Band b, int dbMin);

    // Clarity per-band floor memory (Phase 3G-9c). NaN = no data yet.
    float clarityFloor(Band b) const;
    void setClarityFloor(Band b, float floor);

    // Grid step (single global value, matches Thetis). Persisted under
    // the "DisplayGridStep" key.
    int gridStep() const { return m_gridStep; }
    void setGridStep(int step);

signals:
    void centerFrequencyChanged(double freq);
    void bandwidthChanged(double bw);
    void levelChanged();
    void fftSizeChanged(int size);
    void bandChanged(NereusSDR::Band band);
    void gridStepChanged(int step);

private:
    // Reads m_perBandGrid[b] and pushes it into dBmFloor/dBmCeiling.
    // Called by setBand(); does NOT emit bandChanged itself.
    void applyBandGrid(Band b);

    // AppSettings persistence helpers.
    void loadPerBandGridFromSettings();
    void saveBandGridToSettings(Band b) const;

    double m_centerFrequency{14225000.0};
    double m_bandwidth{200000.0};  // 200 kHz default
    int m_dBmFloor{-140};          // Matches Thetis default (was -130).
    int m_dBmCeiling{-40};         // Matches Thetis default (was -40).
    int m_fftSize{4096};
    int m_averageCount{3};

    Band m_band{Band::Band20m};    // Matches default center freq 14.225 MHz.
    QHash<Band, BandGridSettings> m_perBandGrid;
    int m_gridStep{10};            // NereusSDR divergence from Thetis 2.
};

} // namespace NereusSDR
