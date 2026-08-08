#pragma once

// =================================================================
// src/gui/DssRenderer.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Row ring store ported from AetherSDR
//                 `src/gui/DssRenderer.h`.
// =================================================================

#include <QColor>
#include <QImage>
#include <QSize>
#include <QVector>

#include <array>
#include <functional>

#include "gui/DssGeometry.h"

namespace NereusSDR {

// ─── Stacked-trace spectrum stream surface ──────────────────────────────────
//
// From AetherSDR src/gui/DssRenderer.h [@1872028c].
//
// Renders a perspective stacked-trace spectrum stream: a rolling history of
// FFT rows drawn back-to-front (painter's algorithm) as a receding trapezoid.
// The newest trace spans the full width across the front; older traces recede
// into a narrower, higher trapezoid. Each ridge is filled down to the plot
// floor so nearer traces occlude farther ones.
//
// The renderer is standalone and knows nothing about SpectrumWidget or QRhi.
//
// NereusSDR divergence: upstream's "supplemental" channel carries native FLEX
// waterfall tiles, a separate measurement on an arbitrary display scale that
// must be quantile-calibrated before use. Ours carries off-screen DDC bins
// from the SAME FFT at the SAME calibration, so the arbitration collapses to
// a bin-range test and upstream's DssSupplementalCoverage.h is not ported.
// Renamed "wide" throughout to keep the distinction visible at every callsite.
class DssRenderer {
public:
    using PaletteFn = std::function<QRgb(float dbm)>;

    // Push one freshly-decoded FFT row (any bin count, dBm). Peak-preserving
    // downsample to kDssCols and store it as the newest (front) trace.
    void pushRow(const QVector<float>& binsDbm,
                 double centerMhz = 0.0,
                 double bandwidthMhz = 0.0);
    void pushRowWithWide(const QVector<float>& binsDbm,
                         double centerMhz,
                         double bandwidthMhz,
                         const QVector<float>& wideBinsDbm,
                         double wideCenterMhz,
                         double wideBandwidthMhz);

    void invalidate() { m_dirty = true; }
    bool hasData() const { return m_count > 0; }
    // Forget temporal filter inputs without discarding already-decoded dBm
    // rows. Used when the upstream raw-pixel scale changes.
    void resetInputSmoothing();
    void clear();

    // ── Data-model accessors for the GPU mesh path ──────────────────────────
    // The renderer doubles as the smoothed dBm ring store that the GPU
    // height-map mesh uploads from (one new row per frame). Indices are RING
    // indices.
    int cols() const { return kDssCols; }
    int rows() const { return kDssRows; }
    int rowCount() const { return m_count; }     // valid rows (0..kDssRows)
    int visibleRowCount() const
    {
        return std::min(m_count, kDssVisibleRows);
    }
    int headRing() const { return m_head; }       // ring index of the newest row
    const float* rowDataRing(int ringIndex) const
    {
        return m_rows[ringIndex].data();
    }
    const quint8* rowCoverageRing(int ringIndex) const
    {
        return m_rowCoverage[ringIndex].data();
    }
    const float* rowWideDataRing(int ringIndex) const
    {
        return m_rowWide[ringIndex].data();
    }
    const quint8* rowWideCoverageRing(int ringIndex) const
    {
        return m_rowWideCoverage[ringIndex].data();
    }
    double rowCenterMhzAtAge(int age) const;
    double rowBandwidthMhzAtAge(int age) const;
    double rowWideCenterMhzAtAge(int age) const;
    double rowWideBandwidthMhzAtAge(int age) const;

    // Bandwidth of the newest VISIBLE row carrying an overhang wider than
    // targetBandwidthMhz, or 0 when no such row is on screen.
    //
    // The front row is deliberately not authoritative: a producer that
    // appends with no wide slice would otherwise drop the overhang to nothing
    // while 90-odd rows of it are still on screen. Once the last covered row
    // scrolls out, returning 0 is the correct answer.
    double newestWideBandwidthMhz(double targetBandwidthMhz) const;

    quint64 rowGeneration() const { return m_rowGeneration; }
    // Increments on every cache rebuild — lets the GPU path upload the
    // texture only when the surface actually changed.
    quint64 generation() const { return m_generation; }

private:
    int ringAtAge(int age) const;

    // Circular store: m_head indexes the newest row.
    std::array<std::array<float, kDssCols>, kDssRows> m_rows{};
    // One byte per bin records whether that frequency existed in the captured
    // row. Reprojection-created gaps remain drawable floor lines, but the
    // marker lets both renderers keep their height/colour independent of
    // later zoom and dBm-range changes.
    std::array<std::array<quint8, kDssCols>, kDssRows> m_rowCoverage{};
    // Off-screen DDC bins, kept in separate channels so they fill only
    // frequencies the exact FFT row never captured. Must never resample or
    // replace the working FFT trace.
    std::array<std::array<float, kDssCols>, kDssRows> m_rowWide{};
    std::array<std::array<quint8, kDssCols>, kDssRows> m_rowWideCoverage{};
    // Each live row keeps the frequency frame in which its bins were
    // captured. The GPU maps rows independently during pan previews,
    // allowing retained off-screen data and newly received radio coverage
    // to coexist.
    std::array<double, kDssRows> m_rowCenterMhz{};
    std::array<double, kDssRows> m_rowBandwidthMhz{};
    std::array<double, kDssRows> m_rowWideCenterMhz{};
    std::array<double, kDssRows> m_rowWideBandwidthMhz{};

    int     m_head  = 0;         // index of the newest row
    int     m_count = 0;         // number of valid rows (0..kDssRows)
    bool    m_dirty = true;
    quint64 m_generation = 0;    // bumped on each rebuild
    quint64 m_rowGeneration = 0; // bumped whenever stored row contents change

    // Last two RAW (pre-smoothing) resampled rows, for temporal median-of-3
    // impulse rejection of broadband interference bursts.
    std::array<float, kDssCols> m_rawPrev1{};
    std::array<float, kDssCols> m_rawPrev2{};
    int m_rawHistCount = 0;
    // The wide channel needs an independent copy of the same smoothing
    // chain. Sharing FFT history would blend different frequency frames.
    std::array<float, kDssCols> m_wideRawPrev1{};
    std::array<float, kDssCols> m_wideRawPrev2{};
    int m_wideRawHistCount = 0;

    // One-shot flags: after resetInputSmoothing() the next pushed row must not
    // blend against the retained row that preceded the reset (it was decoded
    // under the old scale). Cleared once each path consumes it. See
    // resetInputSmoothing() — the median-of-3 raw history is not enough on its
    // own; the temporal IIR term against the previous smoothed row also carries
    // pre-reset data.
    bool m_skipLiveTemporalBlendOnce = false;
    bool m_skipWideTemporalBlendOnce = false;
};

}  // namespace NereusSDR
