#pragma once

// =================================================================
// src/gui/DssMeshGeometry.h  (NereusSDR)
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
//                 GPU mesh vertex generation ported from AetherSDR
//                 `src/gui/SpectrumWidget.cpp`.
// =================================================================

#include <QVector>

#include <algorithm>
#include <cmath>

#include "gui/DssGeometry.h"

namespace NereusSDR {

// ─── 3DSS GPU mesh geometry ─────────────────────────────────────────────────
//
// Vertex generation for the height-map mesh sampled by dss_mesh.vert. The
// mesh carries only grid position and an edge tag; height comes from the
// ring texture, so pan and zoom never rebuild a vertex.
//
// Structure from AetherSDR src/gui/SpectrumWidget.cpp:148-172 and
// :13136-13200, and src/gui/DssRenderer.h:66-82 (kMeshCols + its
// static_assert) [@1872028c]. NereusSDR-original: the column count is a
// function of the live perspective shape rather than a compile-time
// constant, so the 3D Angle slider does not force every panadapter to pay
// the worst-case mesh unconditionally (design doc §5.5).

// Mesh columns per row for the GPU path. The mesh spans rowSpanFactor x the
// viewport, while the height texture holds kDssCols texels ACROSS THE
// VIEWPORT — so a kDssCols-wide mesh would leave (span-1)/span of those
// texels unread. That is not shimmer: the mesh-column-to-frequency mapping
// is static, so the same texels are missed every frame and a narrow carrier
// landing on one is permanently invisible at a fixed screen position. Size
// the mesh for the widest span instead, so the on-screen region is never
// sparser than the texture it samples. Below the widest span it merely
// oversamples, which Nearest filtering absorbs by repeating texels.
inline int dssMeshColsFor(const DssShape& shape)
{
    return static_cast<int>(kDssCols * dssMaxRowSpanFactor(shape)) + 1;
}

// The invariant dss_mesh.vert's Nearest height sampler depends on: at the
// widest span the on-screen columns (meshCols / maxRowSpanFactor) must
// still be at least kDssCols, or bins fall between samples and vanish.
// Upstream enforces this with a static_assert; a runtime shape needs a
// runtime check, and it must be re-evaluated after every angle change
// rather than only at construction.
inline bool dssMeshDensityHolds(int meshCols, const DssShape& shape)
{
    return static_cast<float>(meshCols)
        >= kDssCols * dssMaxRowSpanFactor(shape);
}

// Two exact-row crossfade layers, each with two triangles per column.
// Together with the matching ribbon VBO this is about 33.7 MiB of static
// vertex storage at meshCols (1280) columns x 96 visible rows. It is built
// once, never rebuilt while scrolling. The column count is meshCols rather
// than kDssCols so the on-screen part of a row widened by rowSpanFactor
// still samples the height texture at least once per texel — see
// dssMeshColsFor.
inline int dssFillVerticesPerRow(int meshCols)
{
    return (meshCols - 1) * 6 * 2;
}

inline int dssLineVerticesPerRow(int meshCols)
{
    return (meshCols - 1) * 6 * 2;
}

// Total static vertex storage for both VBOs, in bytes. Three floats per
// vertex (u, v, edge).
inline qint64 dssMeshBytesFor(int meshCols)
{
    const qint64 verts =
        static_cast<qint64>(kDssVisibleRows)
        * (dssFillVerticesPerRow(meshCols) + dssLineVerticesPerRow(meshCols));
    return verts * 3 * static_cast<qint64>(sizeof(float));
}

namespace detail {

inline void appendDssVertex(QVector<float>& vertices,
                            float u, float v, float edge)
{
    vertices << u << v << edge;
}

}  // namespace detail

// 3DSS mesh: build the static perspective grid once (geometry never changes
// — height comes from the ring-buffered texture sampled per-vertex). Rows
// are emitted back to front so the painter's-algorithm draw order lets
// nearer curtains occlude farther ones. Base first, overlay second.
// Curtains use both only at the fixed front/rear boundaries; ridge outlines
// use both at every fixed depth for exact-row crossfades.
inline void dssBuildMeshVertices(int meshCols,
                                 QVector<float>& fillOut,
                                 QVector<float>& lineOut)
{
    fillOut.clear();
    lineOut.clear();
    if (meshCols < 2) {
        return;
    }
    const int rows = kDssVisibleRows;
    fillOut.reserve(rows * dssFillVerticesPerRow(meshCols) * 3);
    lineOut.reserve(rows * dssLineVerticesPerRow(meshCols) * 3);

    const float du = 1.0f / static_cast<float>(meshCols - 1);
    for (int rr = rows - 1; rr >= 0; --rr) {
        const float v = static_cast<float>(rr) / rows;  // 0 front .. ~1 back
        for (int layer = 0; layer < 2; ++layer) {
            const float edgeBias = (layer == 1) ? 2.0f : 0.0f;
            const float ribbonBase = (layer == 1) ? 20.0f : 10.0f;
            for (int c = 0; c + 1 < meshCols; ++c) {
                const float u0 = c * du;
                const float u1 = (c + 1) * du;
                // Curtain quad: ridge (edge 0) down to plot floor (edge 1).
                detail::appendDssVertex(fillOut, u0, v, 0.0f + edgeBias);
                detail::appendDssVertex(fillOut, u1, v, 0.0f + edgeBias);
                detail::appendDssVertex(fillOut, u0, v, 1.0f + edgeBias);
                detail::appendDssVertex(fillOut, u1, v, 0.0f + edgeBias);
                detail::appendDssVertex(fillOut, u1, v, 1.0f + edgeBias);
                detail::appendDssVertex(fillOut, u0, v, 1.0f + edgeBias);
                // Ridge outline expanded into a two-pixel screen-space
                // ribbon; the vertex shader offsets by the encoded side.
                const float sideLo = -(ribbonBase + 0.0f);
                const float sideHi = -(ribbonBase + 1.0f);
                detail::appendDssVertex(lineOut, u0, v, sideLo);
                detail::appendDssVertex(lineOut, u1, v, sideLo);
                detail::appendDssVertex(lineOut, u0, v, sideHi);
                detail::appendDssVertex(lineOut, u1, v, sideLo);
                detail::appendDssVertex(lineOut, u1, v, sideHi);
                detail::appendDssVertex(lineOut, u0, v, sideHi);
            }
        }
    }
}

}  // namespace NereusSDR
