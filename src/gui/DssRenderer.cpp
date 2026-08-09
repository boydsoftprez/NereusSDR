// =================================================================
// src/gui/DssRenderer.cpp  (NereusSDR)
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
//                 `src/gui/DssRenderer.cpp`.
// =================================================================

#include "gui/DssRenderer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QPainter>
#include <QPolygonF>

namespace NereusSDR {

namespace {

// CPU-only tunables (image()/rebuild()). The perspective geometry (back-width
// / depth-span / front-ridge / haze) lives in DssGeometry.h as shared
// constants so the GPU mesh uses the same values -- upstream's equivalent
// comment says "DssRenderer.h" because upstream keeps them as members of
// this class; NereusSDR split the shared geometry into its own file in
// Task 1. These four are the extra CPU-render-only touches the GPU frag
// doesn't replicate (depth dimming floor, slope shading); kTemporalAlpha
// below is the fifth (temporal smoothing), ported separately by Task 4.
// From AetherSDR src/gui/DssRenderer.cpp:12-16 [@1872028c].
constexpr double kMinDim = 0.50;  // depth dimming never falls below this

// From AetherSDR src/gui/DssRenderer.cpp:17 [@1872028c].
constexpr float kTemporalAlpha = 0.60f;  // temporal IIR: fraction of the new row

// From AetherSDR src/gui/DssRenderer.cpp:18-20 [@1872028c].
constexpr double kSlopeGain = 0.55;  // slope shading strength
constexpr double kShadeLo   = 0.68;
constexpr double kShadeHi   = 1.32;

// From AetherSDR src/gui/DssRenderer.cpp:28 [@1872028c].
inline int chan(double v) { return static_cast<int>(std::clamp(v, 0.0, 255.0)); }

// From AetherSDR src/gui/DssRenderer.cpp:30-33 [@1872028c].
inline float median3(float a, float b, float c)
{
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

// From AetherSDR src/gui/DssRenderer.cpp:35-39 [@1872028c].
inline QColor scaled(const QColor& c, double f)
{
    f = std::max(0.0, f);
    return QColor(chan(c.red() * f), chan(c.green() * f), chan(c.blue() * f));
}

// Linear blend c -> t by f in [0,1].
// From AetherSDR src/gui/DssRenderer.cpp:41-48 [@1872028c].
inline QColor lerpColor(const QColor& c, const QColor& t, double f)
{
    f = std::clamp(f, 0.0, 1.0);
    return QColor(chan(c.red()   + (t.red()   - c.red())   * f),
                  chan(c.green() + (t.green() - c.green()) * f),
                  chan(c.blue()  + (t.blue()  - c.blue())  * f));
}

// From AetherSDR src/gui/DssRenderer.cpp:50-60 [@1872028c].
bool frequencyFramesMatch(double firstCenterMhz, double firstBandwidthMhz,
                          double secondCenterMhz, double secondBandwidthMhz)
{
    const auto nearlyEqual = [](double first, double second) {
        const double scale =
            std::max({1.0, std::abs(first), std::abs(second)});
        return std::abs(first - second) <= scale * 1.0e-9;
    };
    return nearlyEqual(firstCenterMhz, secondCenterMhz)
        && nearlyEqual(firstBandwidthMhz, secondBandwidthMhz);
}

// From AetherSDR src/gui/DssRenderer.cpp:183-216 [@1872028c].
// Peak-preserving: a one-bin carrier must survive the reduction to kDssCols.
std::array<float, kDssCols> resampledRawRow(const QVector<float>& binsDbm,
                                            float fallback)
{
    std::array<float, kDssCols> row;
    const int n = binsDbm.size();
    if (n <= 0) {
        row.fill(fallback);
        return row;
    }

    if (n == kDssCols) {
        for (int c = 0; c < kDssCols; ++c) {
            row[c] = std::isfinite(binsDbm[c]) ? binsDbm[c] : fallback;
        }
    } else {
        const double step = static_cast<double>(n) / kDssCols;
        for (int c = 0; c < kDssCols; ++c) {
            int i0 = static_cast<int>(std::floor(c * step));
            int i1 = static_cast<int>(std::ceil((c + 1) * step));
            i0 = std::clamp(i0, 0, n - 1);
            i1 = std::clamp(i1, i0 + 1, n);
            float mx = std::isfinite(binsDbm[i0]) ? binsDbm[i0] : fallback;
            for (int i = i0 + 1; i < i1; ++i) {
                if (std::isfinite(binsDbm[i])) {
                    mx = std::max(mx, binsDbm[i]);
                }
            }
            row[c] = mx;
        }
    }

    return row;
}

// From AetherSDR src/gui/DssRenderer.cpp:218-250 [@1872028c].
std::array<float, kDssCols> smoothDssRow(
    const std::array<float, kDssCols>& raw,
    std::array<float, kDssCols>& rawPrev1,
    std::array<float, kDssCols>& rawPrev2,
    int& rawHistCount,
    const std::array<float, kDssCols>* previousSmoothed)
{
    std::array<float, kDssCols> row = raw;
    if (rawHistCount >= 2) {
        for (int c = 0; c < kDssCols; ++c) {
            row[c] = median3(raw[c], rawPrev1[c], rawPrev2[c]);
        }
    }

    rawPrev2 = rawPrev1;
    rawPrev1 = raw;
    rawHistCount = std::min(rawHistCount + 1, 2);

    std::array<float, kDssCols> smoothed = row;
    for (int c = 0; c < kDssCols; ++c) {
        const float a = row[std::max(0, c - 1)];
        const float b = row[c];
        const float d = row[std::min(kDssCols - 1, c + 1)];
        smoothed[c] = 0.25f * a + 0.5f * b + 0.25f * d;
    }
    if (previousSmoothed != nullptr) {
        for (int c = 0; c < kDssCols; ++c) {
            smoothed[c] = kTemporalAlpha * smoothed[c]
                + (1.0f - kTemporalAlpha) * (*previousSmoothed)[c];
        }
    }
    return smoothed;
}

}  // namespace

int DssRenderer::ringAtAge(int age) const
{
    return (m_head + std::clamp(age, 0, kDssRows - 1)) % kDssRows;
}

double DssRenderer::rowCenterMhzAtAge(int age) const
{
    return m_rowCenterMhz[ringAtAge(age)];
}

double DssRenderer::rowBandwidthMhzAtAge(int age) const
{
    return m_rowBandwidthMhz[ringAtAge(age)];
}

double DssRenderer::rowWideCenterMhzAtAge(int age) const
{
    return m_rowWideCenterMhz[ringAtAge(age)];
}

double DssRenderer::rowWideBandwidthMhzAtAge(int age) const
{
    return m_rowWideBandwidthMhz[ringAtAge(age)];
}

double DssRenderer::newestWideBandwidthMhz(double targetBandwidthMhz) const
{
    if (!std::isfinite(targetBandwidthMhz) || targetBandwidthMhz <= 0.0) {
        return 0.0;
    }
    const int visible = visibleRowCount();
    for (int age = 0; age < visible; ++age) {
        const double bw = m_rowWideBandwidthMhz[ringAtAge(age)];
        if (std::isfinite(bw) && bw > targetBandwidthMhz) {
            return bw;
        }
    }
    return 0.0;
}

void DssRenderer::resetInputSmoothing()
{
    m_rawHistCount = 0;
    m_wideRawHistCount = 0;
    // Also break the temporal IIR blend for the next row of each path. Zeroing
    // the median-of-3 counters alone does not forget the previous *smoothed*
    // row that pushRow blends the new row against — that row was decoded
    // under the old scale, so without this the first post-reset row is
    // contaminated by it.
    m_skipLiveTemporalBlendOnce = true;
    m_skipWideTemporalBlendOnce = true;
}

void DssRenderer::clear()
{
    m_head = 0;
    m_count = 0;
    m_rowCenterMhz.fill(0.0);
    m_rowBandwidthMhz.fill(0.0);
    m_rowWideCenterMhz.fill(0.0);
    m_rowWideBandwidthMhz.fill(0.0);
    for (std::array<quint8, kDssCols>& coverage : m_rowWideCoverage) {
        coverage.fill(0);
    }
    m_dirty = true;
    m_rawHistCount = 0;
    m_wideRawHistCount = 0;
    m_skipLiveTemporalBlendOnce = false;
    m_skipWideTemporalBlendOnce = false;
    ++m_rowGeneration;
}

void DssRenderer::pushRow(const QVector<float>& binsDbm,
                          double centerMhz,
                          double bandwidthMhz)
{
    pushRowWithWide(binsDbm, centerMhz, bandwidthMhz,
                    QVector<float>{}, 0.0, 0.0);
}

void DssRenderer::pushRowWithWide(const QVector<float>& binsDbm,
                                  double centerMhz,
                                  double bandwidthMhz,
                                  const QVector<float>& wideBinsDbm,
                                  double wideCenterMhz,
                                  double wideBandwidthMhz)
{
    const std::array<float, kDssCols> raw = resampledRawRow(binsDbm, -200.0f);
    const bool sameFrameAsPrevious =
        m_count > 0
        && frequencyFramesMatch(
            m_rowCenterMhz[m_head], m_rowBandwidthMhz[m_head],
            centerMhz, bandwidthMhz);
    if (m_count > 0 && !sameFrameAsPrevious) {
        // Adjacent bin indices no longer represent the same frequencies.
        // Blending them would smear a signal in the direction of the pan.
        m_rawHistCount = 0;
    }
    const std::array<float, kDssCols>* previous =
        (sameFrameAsPrevious && !m_skipLiveTemporalBlendOnce)
            ? &m_rows[m_head]
            : nullptr;
    m_skipLiveTemporalBlendOnce = false;
    const std::array<float, kDssCols> nr =
        smoothDssRow(raw, m_rawPrev1, m_rawPrev2, m_rawHistCount, previous);

    m_head = (m_head - 1 + kDssRows) % kDssRows;
    m_rows[m_head] = nr;
    m_rowCoverage[m_head].fill(1);
    m_rowCenterMhz[m_head] = centerMhz;
    m_rowBandwidthMhz[m_head] = bandwidthMhz;

    const bool wideValid =
        !wideBinsDbm.isEmpty()
        && std::isfinite(wideCenterMhz)
        && std::isfinite(wideBandwidthMhz)
        && wideCenterMhz > 0.0
        && wideBandwidthMhz > 0.0;
    if (wideValid) {
        const std::array<float, kDssCols> wideRaw =
            resampledRawRow(wideBinsDbm, -200.0f);
        const int previousRing = (m_head + 1) % kDssRows;
        const bool sameWideFrameAsPrevious =
            m_count > 0
            && frequencyFramesMatch(
                m_rowWideCenterMhz[previousRing],
                m_rowWideBandwidthMhz[previousRing],
                wideCenterMhz, wideBandwidthMhz);
        if (m_count > 0 && !sameWideFrameAsPrevious) {
            m_wideRawHistCount = 0;
        }
        const std::array<float, kDssCols>* widePrevious =
            (sameWideFrameAsPrevious && !m_skipWideTemporalBlendOnce)
                ? &m_rowWide[previousRing]
                : nullptr;
        m_skipWideTemporalBlendOnce = false;
        m_rowWide[m_head] = smoothDssRow(
            wideRaw, m_wideRawPrev1, m_wideRawPrev2,
            m_wideRawHistCount, widePrevious);
        m_rowWideCoverage[m_head].fill(1);
        m_rowWideCenterMhz[m_head] = wideCenterMhz;
        m_rowWideBandwidthMhz[m_head] = wideBandwidthMhz;
    } else {
        m_rowWide[m_head].fill(-200.0f);
        m_rowWideCoverage[m_head].fill(0);
        m_rowWideCenterMhz[m_head] = 0.0;
        m_rowWideBandwidthMhz[m_head] = 0.0;
        m_wideRawHistCount = 0;
    }

    m_count = std::min(m_count + 1, kDssRows);
    m_dirty = true;
    ++m_rowGeneration;
}

// From AetherSDR src/gui/DssRenderer.cpp:753-779 [@1872028c].
const QImage& DssRenderer::image(const QSize& px, int scaleStripPx,
                                 float floorDbm, float rangeDb, float zCurve,
                                 const PaletteFn& palette,
                                 quint64 paletteToken,
                                 const QColor& bgFill, const DssShape& shape)
{
    const bool changed = m_dirty
        || px != m_cacheSize
        || scaleStripPx != m_cacheScaleStrip
        || floorDbm != m_cacheFloor
        || rangeDb != m_cacheRange
        || zCurve != m_cacheZCurve
        || paletteToken != m_cachePaletteToken
        // -KG4VCF [v0.5.3] Not present upstream: its shape is a compile-time
        // constant, so it cannot change and needs no cache-key entry. Ours
        // moves with the runtime 3D Angle control -- without this, the
        // fallback would keep drawing the previous perspective after the
        // operator moves the slider.
        || shape.backWidthFrac != m_cacheShape.backWidthFrac
        || shape.depthSpanFrac != m_cacheShape.depthSpanFrac
        || shape.frontMaxRidgeFrac != m_cacheShape.frontMaxRidgeFrac;

    if (changed) {
        rebuild(px, scaleStripPx, floorDbm, rangeDb, zCurve, palette, bgFill,
               shape);
        ++m_generation;
        m_cacheSize         = px;
        m_cacheScaleStrip   = scaleStripPx;
        m_cacheFloor        = floorDbm;
        m_cacheRange        = rangeDb;
        m_cacheZCurve       = zCurve;
        m_cachePaletteToken = paletteToken;
        m_cacheShape        = shape;
        m_dirty             = false;
    }
    return m_cache;
}

// From AetherSDR src/gui/DssRenderer.cpp:781-888 [@1872028c].
void DssRenderer::rebuild(const QSize& px, int scaleStripPx, float floorDbm,
                          float rangeDb, float zCurve, const PaletteFn& palette,
                          const QColor& bgFill, const DssShape& shape)
{
    const int W = px.width();
    const int Htot = px.height();
    if (W <= 0 || Htot <= 0) {
        m_cache = QImage();
        return;
    }

    if (m_cache.size() != px || m_cache.format() != QImage::Format_RGBA8888_Premultiplied) {
        m_cache = QImage(px, QImage::Format_RGBA8888_Premultiplied);
    }
    m_cache.fill(Qt::transparent);

    // Plot region is everything above the (transparent) scale strip.
    const double H = std::max(1, Htot - std::max(0, scaleStripPx));

    QPainter p(&m_cache);
    p.fillRect(QRectF(0, 0, W, H), bgFill);

    if (m_count <= 0 || !palette || rangeDb <= 0.0f) {
        return;
    }

    const double zc            = std::max(0.05, static_cast<double>(zCurve));
    const double bottomY       = H;                       // plot floor
    const double depthSpan     = H * shape.depthSpanFrac;
    const double frontMaxRidge = H * shape.frontMaxRidgeFrac;
    // Match dss_mesh.vert's depth parametrization exactly (v = rr / rows), so
    // the CPU fallback and the GPU mesh place rows at the same depth.
    const double denom         = kDssVisibleRows;

    std::array<QPointF, kDssCols> pts;
    std::array<QColor, kDssCols>  cols;   // depth/slope-shaded fill colour per column

    QPolygonF poly;                    // reused (clear keeps capacity → no realloc)
    poly.reserve(4);
    QPen ridgePen;
    ridgePen.setCosmetic(true);
    ridgePen.setCapStyle(Qt::RoundCap);
    ridgePen.setJoinStyle(Qt::RoundJoin);

    // Back (oldest) → front (newest): painter's algorithm. Nearer traces are
    // wider, sit lower, and fill to the floor, so they occlude farther ones.
    for (int age = visibleRowCount() - 1; age >= 0; --age) {
        const double depthFrac    = age / denom;
        const double rowWidthFrac = 1.0 - depthFrac * (1.0 - shape.backWidthFrac);
        const double inset        = W * (1.0 - rowWidthFrac) * 0.5;
        const double rowW         = W - 2.0 * inset;
        const double baselineY    = bottomY - depthFrac * depthSpan;
        const double maxRidge     = frontMaxRidge * rowWidthFrac;
        const double dim          = kMinDim + (1.0 - kMinDim) * (1.0 - depthFrac);

        const int ring = ringAtAge(age);
        const auto& row = m_rows[ring];
        const auto& coverage = m_rowCoverage[ring];
        // Pass 1: geometry — noise-floor-anchored ridge heights, with the same
        // pow(s, zCurve) floor-lift the GPU shader applies.
        for (int c = 0; c < kDssCols; ++c) {
            const double x = inset + (kDssCols > 1 ? double(c) / (kDssCols - 1) : 0.0) * rowW;
            const float dbm = coverage[c] != 0 ? row[c] : floorDbm;
            double strength = std::clamp(
                (dbm - floorDbm) / rangeDb, 0.0f, 1.0f);
            strength = std::pow(strength, zc);
            pts[c] = QPointF(x, baselineY - strength * maxRidge);
        }
        // Pass 2: colour — palette by amplitude, hazed by depth, lit by slope.
        const double slopeScale = (maxRidge > 1.0) ? maxRidge : 1.0;
        for (int c = 0; c < kDssCols; ++c) {
            const int cl = std::max(0, c - 1);
            const int cr = std::min(kDssCols - 1, c + 1);
            const double slope = (pts[cl].y() - pts[cr].y()) / slopeScale; // +: rises to right
            const double shade = std::clamp(1.0 + kSlopeGain * slope, kShadeLo, kShadeHi);
            const float dbm = coverage[c] != 0 ? row[c] : floorDbm;
            QColor base = QColor(palette(dbm));
            base = lerpColor(base, bgFill, depthFrac * kDssHaze);
            cols[c] = scaled(base, dim * shade);
        }

        // Fill — flat per-column trapezoid to the floor. AA off so adjacent
        // columns tile without seams; the AA ridge line on top hides the
        // jagged upper edge. No per-column gradient → no per-column allocs.
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        for (int c = 0; c < kDssCols - 1; ++c) {
            poly.clear();
            poly << pts[c] << pts[c + 1]
                 << QPointF(pts[c + 1].x(), bottomY)
                 << QPointF(pts[c].x(), bottomY);
            p.setBrush(cols[c]);
            p.drawPolygon(poly);
        }

        // Ridge line — bright per-amplitude rim, AA on for a crisp crest.
        p.setRenderHint(QPainter::Antialiasing, true);
        ridgePen.setWidthF(age == 0 ? 1.6 : 1.0);
        for (int c = 0; c < kDssCols - 1; ++c) {
            const float dbm = coverage[c] != 0 ? row[c] : floorDbm;
            QColor rc = QColor(palette(dbm)).lighter(165);
            rc = lerpColor(rc, bgFill, depthFrac * kDssHaze);
            ridgePen.setColor(scaled(rc, dim));
            p.setPen(ridgePen);
            p.drawLine(pts[c], pts[c + 1]);
        }
    }
}

// From AetherSDR src/gui/DssRenderer.cpp:890-903 [@1872028c].
QVector<bool> dssDepthVisibleSegments(const QVector<qreal>& yFrontToBack)
{
    if (yFrontToBack.size() < 2) {
        return {};
    }
    QVector<bool> visible(yFrontToBack.size() - 1, true);
    qreal silhouetteY = std::numeric_limits<qreal>::max();
    for (qsizetype i = 1; i < yFrontToBack.size(); ++i) {
        visible[i - 1] = yFrontToBack.at(i - 1) <= silhouetteY + 0.5
            || yFrontToBack.at(i) <= silhouetteY + 0.5;
        silhouetteY = std::min(silhouetteY, yFrontToBack.at(i - 1));
    }
    return visible;
}

}  // namespace NereusSDR
