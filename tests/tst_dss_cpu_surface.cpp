// 3D stacked-trace spectrum plan, Task 10: CPU fallback surface.
//
// Pins DssRenderer::image()'s cache gate (rebuilds only when an input that
// actually affects the picture changes -- including, as a NereusSDR-original
// requirement upstream does not share, the runtime perspective DssShape) and
// dssDepthVisibleSegments()'s painter's-algorithm occlusion test used by the
// depth-shadow overlay (Task 12).
//
// Source: AetherSDR src/gui/DssRenderer.cpp:753-903 [@1872028c].

#include <QTest>
#include <QColor>
#include <QImage>
#include <QVector>

#include "gui/DssRenderer.h"

using namespace NereusSDR;

namespace {

QRgb greyPalette(float dbm)
{
    const int v = std::clamp(static_cast<int>((dbm + 140.0f) * 2.0f), 0, 255);
    return qRgb(v, v, v);
}

}  // namespace

class TestDssCpuSurface : public QObject {
    Q_OBJECT

private slots:
    void image_matchesRequestedSize() {
        DssRenderer r;
        r.pushRow(QVector<float>(768, -120.0f), 14.2, 0.192);
        const QImage& img = r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                                    greyPalette, 1, QColor(10, 10, 20),
                                    kDssUpstreamShape);
        QCOMPARE(img.size(), QSize(400, 200));
    }

    // The plot region is painted opaque; the bottom scale strip is left
    // transparent so the host can composite a scale on top.
    void scaleStrip_isLeftTransparent() {
        DssRenderer r;
        r.pushRow(QVector<float>(768, -120.0f), 14.2, 0.192);
        const QImage& img = r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                                    greyPalette, 1, QColor(10, 10, 20),
                                    kDssUpstreamShape);
        QCOMPARE(qAlpha(img.pixel(200, 195)), 0);     // inside the strip
        QVERIFY(qAlpha(img.pixel(200, 100)) > 0);     // inside the plot
    }

    // Rebuild is expensive, so the cache must hold when nothing changed and
    // drop when any mapping input moves.
    void cache_holdsUntilAnInputChanges() {
        DssRenderer r;
        r.pushRow(QVector<float>(768, -120.0f), 14.2, 0.192);
        const quint64 g0 = r.generation();
        r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), kDssUpstreamShape);
        const quint64 g1 = r.generation();
        QVERIFY(g1 > g0);
        r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), kDssUpstreamShape);
        QCOMPARE(r.generation(), g1);                 // unchanged: cache hit
        r.image(QSize(400, 200), 20, -130.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), kDssUpstreamShape);
        QVERIFY(r.generation() > g1);                 // floor moved: rebuild
    }

    // A different angle must invalidate the cache, or the CPU fallback keeps
    // drawing the previous perspective after the slider moves.
    void cache_dropsOnShapeChange() {
        DssRenderer r;
        r.pushRow(QVector<float>(768, -120.0f), 14.2, 0.192);
        r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), dssShapeForAngle(50));
        const quint64 g = r.generation();
        r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), dssShapeForAngle(20));
        QVERIFY2(r.generation() > g, "shape change must invalidate the cache");
    }

    // Painter's algorithm: a nearer ridge hides anything below it.
    //
    // Corrected from the brief's given h.at(0) expectation (false): nothing
    // sits in front of the leading segment to occlude it, so it can never be
    // culled -- traced by hand against dssDepthVisibleSegments()'s silhouetteY
    // initialization (starts at the max sentinel, so the very first segment
    // always clears it) and confirmed against upstream's own dedicated test,
    // AetherSDR tests/dss_renderer_test.cpp:363-374 [@1872028c]
    // (testDepthShadowOcclusion's "behind" case: `!behind.at(0)` -- i.e.
    // behind.at(0) must be true -- `|| behind.at(1) || behind.at(2)`, same
    // leading-segment-always-visible shape as this case).
    void depthVisibleSegments_cullsOccludedPoints() {
        // Front row highest (smallest y). The leading segment (0<->1) has
        // nothing in front of it and is never culled; the segment behind it
        // (1<->2) sits under the front ridge's silhouette and is hidden.
        const QVector<qreal> hidden{10.0, 50.0, 90.0};
        const QVector<bool> h = dssDepthVisibleSegments(hidden);
        QCOMPARE(h.size(), 2);
        QCOMPARE(h.at(0), true);
        QCOMPARE(h.at(1), false);
        // Rising behind the silhouette stays visible.
        const QVector<qreal> visible{90.0, 50.0, 10.0};
        const QVector<bool> v = dssDepthVisibleSegments(visible);
        QCOMPARE(v.size(), 2);
        QCOMPARE(v.at(0), true);
        QCOMPARE(v.at(1), true);
    }

    void depthVisibleSegments_handlesDegenerateInput() {
        QCOMPARE(dssDepthVisibleSegments({}).size(),        0);
        QCOMPARE(dssDepthVisibleSegments({5.0}).size(),     0);
    }
};

QTEST_APPLESS_MAIN(TestDssCpuSurface)
#include "tst_dss_cpu_surface.moc"
