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

// A bright carrier is above the floor-only curtain, so its horizontal
// position can be read from the actual CPU surface without inspecting bins.
int carrierX(const QImage& image)
{
    int left = image.width(), right = -1;
    for (int y = 0; y < 195; ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qRed(image.pixel(x, y)) > 100) {
                left = std::min(left, x);
                right = std::max(right, x);
            }
        }
    }
    return right >= left ? (left + right) / 2 : -1;
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

    // Captured bins keep their frequency identity when the current frame
    // changes. A carrier initially at column 512 must move to column 256
    // after a center shift of 256 bins, even while that old row is retained.
    void retainedCarrier_movesWithCurrentFrequencyFrame() {
        DssRenderer r;
        QVector<float> carrier(kDssCols, -140.0f);
        carrier[512] = -60.0f;
        r.pushRow(carrier, 14.0, 0.768);
        r.pushRow(QVector<float>(kDssCols, -140.0f),
                  14.0 + 0.768 * 256.0 / (kDssCols - 1), 0.768);
        const QImage img = r.image(QSize(768, 200), 0, -140.0f, 80.0f, 1.0f,
                                  greyPalette, 1, QColor(0, 0, 0),
                                  kDssUpstreamShape);
        const int peakX = carrierX(img);
        QVERIFY2(peakX >= 0, "retained carrier must remain visible");
        QVERIFY2(peakX >= 245 && peakX <= 267,
                 qPrintable(QStringLiteral("carrier drawn at x=%1 instead of 256").arg(peakX)));
    }

    void viewportPreview_reprojectsWithoutNewRowsOrLoss() {
        DssRenderer r;
        QVector<float> carrier(kDssCols, -140.0f);
        carrier[512] = -60.0f;
        r.pushRow(carrier, 14.0, 0.768);
        const auto draw = [&](double center, double bandwidth) {
            return carrierX(r.image(QSize(768, 200), 0, -140.0f, 80.0f, 1.0f,
                                    greyPalette, 1, QColor(0, 0, 0),
                                    kDssUpstreamShape, center, bandwidth));
        };
        QCOMPARE(draw(14.0, 0.768), 512);
        const quint64 generation = r.generation();
        const int shifted = draw(14.0 + 0.768 * 256.0 / (kDssCols - 1), 0.768);
        QVERIFY(shifted >= 254 && shifted <= 258);
        QVERIFY(r.generation() > generation);
        const int zoomed = draw(14.0, 0.384);
        QVERIFY(zoomed >= 638 && zoomed <= 643);
        QCOMPARE(draw(15.0, 0.384), -1);  // no captured coverage
        QCOMPARE(draw(14.0, 0.768), 512); // capture survived repeated previews
        QCOMPARE(r.rowCount(), 1);
        QCOMPARE(r.rowCenterMhzAtAge(0), 14.0);
    }

    void viewportOutsideExactFrame_usesWideCapture() {
        DssRenderer r;
        QVector<float> wide(kDssCols, -140.0f);
        wide[512] = -60.0f;
        r.pushRowWithWide(QVector<float>(kDssCols, -140.0f), 14.0, 0.2,
                          wide, 14.0, 0.768);
        const int x = carrierX(r.image(QSize(768, 200), 0, -140.0f, 80.0f, 1.0f,
                                      greyPalette, 1, QColor(0, 0, 0),
                                      kDssUpstreamShape,
                                      14.0 + 0.768 * 256.0 / (kDssCols - 1), 0.768));
        QVERIFY2(x >= 254 && x <= 258, "off-screen FFT capture must stay frequency aligned");
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
