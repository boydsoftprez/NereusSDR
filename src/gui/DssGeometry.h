#pragma once

// =================================================================
// src/gui/DssGeometry.h  (NereusSDR)
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
//                 Perspective geometry ported from AetherSDR
//                 `src/gui/DssRenderer.h`.
// =================================================================

#include <QPointF>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ─── Stacked-trace spectrum surface geometry ────────────────────────────────
//
// From AetherSDR src/gui/DssRenderer.h:33-64, 84-187 [@1872028c].
//
// Perspective geometry of the surface, shared by this CPU renderer and the
// GPU mesh UBO (SpectrumWidget::renderGpuFrame). dss_mesh.vert applies the
// SAME formulas with these values passed as uniforms — single source of
// truth so the CPU fallback and the GPU mesh can't drift apart.
//
//-KG4VCF [v0.5.3] Upstream holds backWidthFrac / depthSpanFrac /
// frontMaxRidgeFrac as static constexpr. NereusSDR promotes them to a
// runtime DssShape so the 3D Angle slider can move them; upstream's values
// are preserved exactly as kDssUpstreamShape and are reproduced by
// dssShapeForAngle(50). Permitted tier 1 deviation 1 of 2, design doc §2.3.
struct DssShape {
    float backWidthFrac{0.60f};      // back row width / front
    float depthSpanFrac{0.58f};      // baseline rise to the back
    float frontMaxRidgeFrac{0.46f};  // front ridge height / plot H
};

inline constexpr DssShape kDssUpstreamShape{0.60f, 0.58f, 0.46f};

inline constexpr float kDssHaze = 0.16f;   // fade toward bg with depth
// Colour uses a stable signal aperture independent of the Ref-level height
// span. Otherwise a high Ref level compresses every real signal into blue.
inline constexpr float kDssColorSpanDb = 45.0f;

inline constexpr int kDssVisibleRows = 96;    // front → back display depth
// Outgoing rows kept during scroll. Must cover the largest row distance
// passed to SpectrumWidget::startWaterfallScrollAnimation(); the current
// Flex, Kiwi, and fallback producers append at most one row per update.
//-KG4VCF [v0.5.3] NereusSDR's producer appends exactly one row per
// waterfall tick, so only one is ever needed; the reserve is kept at
// upstream's 8 so every ring-index formula stays line-comparable with
// upstream. Permitted tier 1 deviation 2 of 2, design doc §2.3.
inline constexpr int kDssTransitionRows = 8;
inline constexpr int kDssRows = kDssVisibleRows + kDssTransitionRows;
inline constexpr int kDssCols = 768;  // resampled columns per row

// ── 3D Angle mapping (NereusSDR-original, design doc §5.3) ──────────────
// Both curves pass through upstream's constants at t = 0.5, so slider 50
// reproduces AetherSDR's fixed look exactly.
inline constexpr float kDssBackWidthAtZero = 0.35f;
inline constexpr float kDssBackWidthAtOne  = 0.85f;
inline constexpr float kDssDepthSpanAtZero = 0.36f;
inline constexpr float kDssDepthSpanAtOne  = 0.80f;
inline constexpr float kDssMaxRidgeFrac    = 0.75f;

// Fraction of the on-screen ridge ceiling upstream occupies. Defined from
// the upstream constants rather than a rounded literal so dssShapeForAngle(50)
// returns kDssUpstreamShape.frontMaxRidgeFrac exactly.
inline constexpr float kDssRidgeHeadroom =
    kDssUpstreamShape.frontMaxRidgeFrac * kDssUpstreamShape.backWidthFrac
    / (1.0f - kDssUpstreamShape.depthSpanFrac);

// Slider 0..100 -> perspective shape. Ridge height is derived rather than
// exposed, so a rising angle cannot push back-row peaks off the top of the
// plot: the back ridge top sits at (1 - depthSpanFrac) - ridge * backWidthFrac
// and must stay >= 0.
inline DssShape dssShapeForAngle(int anglePercent)
{
    const float t =
        std::clamp(static_cast<float>(anglePercent), 0.0f, 100.0f) / 100.0f;
    DssShape s;
    s.backWidthFrac =
        kDssBackWidthAtZero + t * (kDssBackWidthAtOne - kDssBackWidthAtZero);
    s.depthSpanFrac =
        kDssDepthSpanAtZero + t * (kDssDepthSpanAtOne - kDssDepthSpanAtZero);
    s.frontMaxRidgeFrac = std::min(
        kDssMaxRidgeFrac,
        kDssRidgeHeadroom * (1.0f - s.depthSpanFrac) / s.backWidthFrac);
    return s;
}

// Perspective narrowing with depth — the only thing that places a frequency.
inline float dssDepthScale(float depth, const DssShape& shape)
{
    return 1.0f
         - std::clamp(depth, 0.0f, 1.0f) * (1.0f - shape.backWidthFrac);
}

// Mesh column (0..1 across the drawn row) -> frequency in viewport units,
// where 0..1 spans the on-screen bandwidth. Depth-independent by design.
inline float dssRowFrequencyUnit(float meshUnit, float rowSpanFactor)
{
    return 0.5f + (meshUnit - 0.5f) * rowSpanFactor;
}

// Fraction of the plot width a row at `depth` covers. >= 1 means that row
// reaches both edges and leaves no wedge; the front row is the widest.
inline float dssRowScreenCoverage(float depth, float rowSpanFactor,
                                  const DssShape& shape)
{
    return dssDepthScale(depth, shape) * rowSpanFactor;
}

// ── Wedge-closing row span ──────────────────────────────────────────────
// Because every row covers the SAME frequency span while the far rows
// narrow to kBackWidthFrac, the surface leaves two empty triangles beside
// it. Widen the span each row covers instead: the near rows then run off
// both edges of the plot and the existing perspective narrowing walks them
// back in, closing the wedge from the front. The projection below is
// untouched, so the converging slant, the frequency ruler and every marker
// stay put. dss_mesh.vert applies the SAME formulas.

// Span at which the DEEPEST row lands exactly on the plot edge. Beyond this
// the surface only overhangs further without revealing more of the plot.
inline float dssMaxRowSpanFactor(const DssShape& shape)
{
    return 1.0f / shape.backWidthFrac;
}

// Shallowest depth still leaving a wedge, or 1 when the surface is closed
// all the way to the back. Lets the host report how much a given overhang
// actually bought.
inline float dssWedgeFreeDepth(float rowSpanFactor, const DssShape& shape)
{
    if (rowSpanFactor <= 1.0f) {
        return 0.0f;
    }
    if (rowSpanFactor >= dssMaxRowSpanFactor(shape)) {
        return 1.0f;
    }
    return (1.0f - 1.0f / rowSpanFactor) / (1.0f - shape.backWidthFrac);
}

// Usable span for an overhang of `spanFactor` x the viewport bandwidth.
// Clamped at kMaxRowSpanFactor: past it the extra data is off-plot anyway.
inline float dssRowSpanFactorForOverhang(float spanFactor,
                                         const DssShape& shape)
{
    if (!(spanFactor > 1.0f) || !std::isfinite(spanFactor)) {
        return 1.0f;
    }
    return std::min(spanFactor, dssMaxRowSpanFactor(shape));
}

// Row span for a source whose calibrated overhang spans
// supplementalBandwidthMhz against a targetBandwidthMhz viewport, scaled by
// a 0-100 operator setting. Anything that cannot be trusted -- a
// non-positive or non-finite bandwidth, or an overhang no wider than the
// viewport -- yields 1.0, the clipped trapezoid, rather than widening into
// spectrum that was never captured.
//
// The percentage scales the AVAILABLE span, not the absolute maximum:
// against a ~1.15x tile an absolute reading would clamp everything above
// ~22% to the same picture, leaving most of the control's travel dead.
inline float dssRowSpanFactorFor(double supplementalBandwidthMhz,
                                 double targetBandwidthMhz,
                                 int spanPercent,
                                 const DssShape& shape)
{
    if (!std::isfinite(targetBandwidthMhz) || targetBandwidthMhz <= 0.0
        || !std::isfinite(supplementalBandwidthMhz)
        || supplementalBandwidthMhz <= targetBandwidthMhz) {
        return 1.0f;
    }
    const float available = dssRowSpanFactorForOverhang(
        static_cast<float>(supplementalBandwidthMhz / targetBandwidthMhz),
        shape);
    const float fraction =
        static_cast<float>(std::clamp(spanPercent, 0, 100)) / 100.0f;
    return 1.0f + fraction * (available - 1.0f);
}

// Project a normalized frequency coordinate onto the same perspective
// plane used by both DSS renderers. depth=0 is the full-width front edge;
// depth=1 is the narrowed back edge. Slice overlays use this helper so
// their apparent angle cannot drift from the FFT surface.
//
// Frequency-in / screen-out, and independent of rowSpanFactor: widening a
// row extends the frequency range it covers, never where a frequency lands.
inline QPointF dssProjectPerspective(float frequencyUnit, float depth,
                                     const DssShape& shape)
{
    const float d = std::clamp(depth, 0.0f, 1.0f);
    const float width = dssDepthScale(d, shape);
    return QPointF(0.5f + (frequencyUnit - 0.5f) * width,
                   1.0f - d * shape.depthSpanFrac);
}

// Project a point onto the visible top of a DSS ridge. This is the CPU
// equivalent of dss_mesh.vert's height mapping and is used by overlays
// that must stay attached to the surface when the 3D floor moves.
inline QPointF dssProjectSurface(float frequencyUnit, float depth, float dbm,
                                 float floorDbm, float rangeDb, float zCurve,
                                 const DssShape& shape)
{
    const float d = std::clamp(depth, 0.0f, 1.0f);
    const float width = dssDepthScale(d, shape);
    const float finiteDbm = std::isfinite(dbm) ? dbm : floorDbm;
    const float strengthLinear = std::clamp(
        (finiteDbm - floorDbm) / std::max(rangeDb, 1.0f),
        0.0f, 1.0f);
    const float strengthHeight = std::pow(
        strengthLinear, std::max(zCurve, 0.05f));
    QPointF point = dssProjectPerspective(frequencyUnit, d, shape);
    point.ry() -=
        static_cast<double>(strengthHeight) * shape.frontMaxRidgeFrac * width;
    return point;
}

}  // namespace NereusSDR
