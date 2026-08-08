// =================================================================
// tests/tst_dss_mesh_geometry.cpp  (NereusSDR)
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
//                 Golden test for the GPU mesh vertex generation ported
//                 from AetherSDR `src/gui/SpectrumWidget.cpp`, verifying
//                 angle-sized column counts, mesh byte size, the density
//                 invariant, and vertex/edge-tag encoding.
// =================================================================

#include <QTest>
#include <QVector>
#include <cmath>

#include "gui/DssMeshGeometry.h"

using namespace NereusSDR;

class TestDssMeshGeometry : public QObject {
    Q_OBJECT

private slots:
    // At the default angle the mesh must cost exactly what upstream costs.
    // Upstream states 33.7 MiB at SpectrumWidget.cpp:150-157 [@1872028c].
    void defaultAngle_matchesUpstreamMeshSize() {
        const int cols = dssMeshColsFor(dssShapeForAngle(50));
        QCOMPARE(cols, 1281);
        const qint64 bytes = dssMeshBytesFor(cols);
        const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
        QVERIFY2(mib > 33.0 && mib < 34.5,
                 qPrintable(QStringLiteral("expected ~33.8 MiB, got %1")
                                .arg(mib)));
    }

    // The whole reason the mesh is sized dynamically: the worst-case angle
    // is materially more expensive and must not be paid unconditionally.
    void widestAngle_isMoreExpensive() {
        const int wide = dssMeshColsFor(dssShapeForAngle(0));
        const int mid  = dssMeshColsFor(dssShapeForAngle(50));
        QCOMPARE(wide, 2195);
        QVERIFY(dssMeshBytesFor(wide) > dssMeshBytesFor(mid));
    }

    // The invariant dss_mesh.vert's Nearest height sampler depends on: at
    // the widest span the on-screen columns must still be at least kDssCols,
    // or bins fall between samples and a narrow carrier vanishes at a fixed
    // screen position. Must hold at EVERY angle, after resizing for it.
    void densityInvariant_holdsAtEveryAngle() {
        for (int a = 0; a <= 100; ++a) {
            const DssShape s = dssShapeForAngle(a);
            const int cols = dssMeshColsFor(s);
            QVERIFY2(dssMeshDensityHolds(cols, s),
                     qPrintable(QStringLiteral("density fails at angle %1"
                                               " (cols=%2)").arg(a).arg(cols)));
        }
    }

    // Guards the dynamic-resize bug the design calls out: computing a new
    // column count but reusing the old mesh.
    void densityInvariant_failsIfMeshNotResized() {
        const DssShape topDown = dssShapeForAngle(100);
        const DssShape edgeOn  = dssShapeForAngle(0);
        const int narrowMesh = dssMeshColsFor(topDown);
        QVERIFY(dssMeshDensityHolds(narrowMesh, topDown));
        // Same mesh, dramatic angle: must be detected as too sparse.
        QVERIFY(!dssMeshDensityHolds(narrowMesh, edgeOn));
    }

    void vertexCounts_matchUpstreamFormula() {
        QCOMPARE(dssFillVerticesPerRow(1281), (1281 - 1) * 6 * 2);
        QCOMPARE(dssLineVerticesPerRow(1281), (1281 - 1) * 6 * 2);
    }

    void buildMeshVertices_producesExpectedSizeAndRange() {
        constexpr int kCols = 64;   // small mesh keeps the test fast
        QVector<float> fill;
        QVector<float> line;
        dssBuildMeshVertices(kCols, fill, line);
        QCOMPARE(fill.size(),
                 kDssVisibleRows * dssFillVerticesPerRow(kCols) * 3);
        QCOMPARE(line.size(),
                 kDssVisibleRows * dssLineVerticesPerRow(kCols) * 3);
        // u in [0,1], v in [0,1). The shader multiplies v by visibleRows.
        for (int i = 0; i < fill.size(); i += 3) {
            QVERIFY(fill[i]     >= 0.0f && fill[i]     <= 1.0f);
            QVERIFY(fill[i + 1] >= 0.0f && fill[i + 1] <  1.0f);
        }
    }

    // Guards the guard clause itself: below the 2-column minimum there is
    // no quad to emit, and the previous contents must be cleared rather
    // than left stale.
    void buildMeshVertices_tooFewColumns_yieldsEmptyOutput() {
        QVector<float> fill{1.0f, 2.0f, 3.0f};
        QVector<float> line{4.0f, 5.0f, 6.0f};
        dssBuildMeshVertices(1, fill, line);
        QVERIFY(fill.isEmpty());
        QVERIFY(line.isEmpty());
    }

    // Rows are emitted back to front so the painter's-algorithm draw order
    // lets nearer curtains occlude farther ones.
    void buildMeshVertices_emitsBackToFront() {
        constexpr int kCols = 8;
        QVector<float> fill;
        QVector<float> line;
        dssBuildMeshVertices(kCols, fill, line);
        const float firstV = fill[1];
        const float lastV  = fill[fill.size() - 2];
        QVERIFY2(firstV > lastV,
                 "first emitted row must be the deepest (largest v)");
    }

    // Edge tags must land in the ranges dss_mesh.vert:180-195 decodes.
    void buildMeshVertices_usesDecodableEdgeTags() {
        constexpr int kCols = 8;
        QVector<float> fill;
        QVector<float> line;
        dssBuildMeshVertices(kCols, fill, line);
        for (int i = 2; i < fill.size(); i += 3) {
            const float e = fill[i];
            const bool baseLayer    = (e == 0.0f || e == 1.0f);
            const bool overlayLayer = (e == 2.0f || e == 3.0f);
            QVERIFY2(baseLayer || overlayLayer,
                     qPrintable(QStringLiteral("bad fill edge tag %1").arg(e)));
        }
        for (int i = 2; i < line.size(); i += 3) {
            const float e = line[i];
            // Ribbon outlines encode as -(10 + side) or -(20 + side).
            QVERIFY2(e <= -10.0f,
                     qPrintable(QStringLiteral("bad line edge tag %1").arg(e)));
            const float code = -e;
            const bool baseRibbon    = (code == 10.0f || code == 11.0f);
            const bool overlayRibbon = (code == 20.0f || code == 21.0f);
            QVERIFY(baseRibbon || overlayRibbon);
        }
    }
};

QTEST_APPLESS_MAIN(TestDssMeshGeometry)
#include "tst_dss_mesh_geometry.moc"
