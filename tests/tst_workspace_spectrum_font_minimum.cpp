// no-port-check: NereusSDR-original actual paint-device typography checks.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex, 2026-10-12.
// 2026-10-12: J.J. Boyd (KG4VCF), OpenAI Codex: guard GPU-only cache checks.
// 2026-11-17: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code: the
// 16 px frequency row; its numbers are plain weight after a bold painter.
#include <QtTest>
#include <QDir>
#include <QFontMetricsF>
#include <QImage>
#include <QPaintEngine>
#include <QPainter>
#include "gui/SpectrumWidget.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/BandPlanManager.h"

using namespace NereusSDR;

namespace {
class TextDevice final : public QPaintDevice {
public:
    struct Run { QString text; QFont font; };
    class Engine final : public QPaintEngine {
    public:
        Engine() : QPaintEngine(AllFeatures) {}
        bool begin(QPaintDevice* device) override { setPaintDevice(device); setActive(true); return true; }
        bool end() override { setActive(false); return true; }
        Type type() const override { return User; }
        void updateState(const QPaintEngineState&) override {}
        void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) override {}
        void drawPolygon(const QPointF*, int, PolygonDrawMode) override {}
        void drawTextItem(const QPointF&, const QTextItem& item) override { runs.append({item.text(), item.font()}); }
        QVector<Run> runs;
    };
    explicit TextDevice(int dpi) : m_dpi(dpi) {}
    QPaintEngine* paintEngine() const override { return &m_engine; }
    const QVector<Run>& runs() const { return m_engine.runs; }
protected:
    int metric(PaintDeviceMetric metric) const override {
        switch (metric) {
        case PdmWidth: return 820;
        case PdmHeight: return 300;
        case PdmDpiX: case PdmDpiY: case PdmPhysicalDpiX: case PdmPhysicalDpiY: return m_dpi;
        case PdmDepth: return 32;
        case PdmDevicePixelRatio: return 1;
        case PdmDevicePixelRatioScaled: return devicePixelRatioFScale();
        default: return QPaintDevice::metric(metric);
        }
    }
private:
    int m_dpi;
    mutable Engine m_engine;
};
}

class TestWorkspaceSpectrumFontMinimum : public QObject {
    Q_OBJECT
private:
    void pose(SpectrumWidget& spectrum, BandPlanManager& plans) {
        spectrum.resize(820, 300);
        spectrum.setBandPlanManager(&plans);
        spectrum.m_centerHz = 14225000.0;
        spectrum.m_bandwidthHz = 1000000.0;
        spectrum.m_vfoHz = spectrum.m_centerHz;
    }
    void layers(SpectrumWidget& spectrum, QPainter& painter) {
        spectrum.drawFreqScale(painter, QRect(36, 0, 784, 16));
        spectrum.drawDbmScale(painter, QRect(0, 30, 36, 230));
        spectrum.drawBandPlan(painter, QRect(36, 30, 784, 230));
        spectrum.drawTxFilterOverlay(painter, QRect(36, 30, 784, 230));
    }
private slots:
    void paintFontsRespectActualDeviceAndKeepBandPlanSetting_data() {
        QTest::addColumn<int>("dpi");
        QTest::addColumn<int>("points");
        QTest::newRow("72dpi-small") << 72 << 6;
        QTest::newRow("96dpi-small") << 96 << 6;
        QTest::newRow("72dpi-medium") << 72 << 10;
        QTest::newRow("96dpi-medium") << 96 << 10;
    }
    void paintFontsRespectActualDeviceAndKeepBandPlanSetting() {
        QFETCH(int, dpi);
        QFETCH(int, points);
        BandPlanManager plans;
        plans.loadPlans();
        SpectrumWidget spectrum;
        pose(spectrum, plans);
        spectrum.setBandPlanFontSize(points);
        WorkspacePreview::setEnabledForTest(true);
        TextDevice device(dpi);
        QPainter painter(&device);
        layers(spectrum, painter);
        painter.end();
        QVERIFY(!device.runs().isEmpty());
        for (const TextDevice::Run& run : device.runs()) {
            const qreal pixels = run.font.pixelSize() > 0 ? run.font.pixelSize()
                : run.font.pointSizeF() * QFontMetricsF(run.font, &device).fontDpi() / 72.0;
            QVERIFY2(pixels >= 11.0, qPrintable(run.text));
        }
        QCOMPARE(spectrum.bandPlanFontSize(), points);
        // A larger point-based band label on the 96 DPI paint device stays
        // point-based and unchanged; it must not be shrunk using widget DPI.
        if (dpi == 96 && points == 10) {
            bool foundBandLabel = false;
            for (const TextDevice::Run& run : device.runs()) {
                if (run.font.pointSizeF() == 10.0 && run.font.bold()) { foundBandLabel = true; }
            }
            QVERIFY(foundBandLabel);
        }
    }
    // The band plan leaves the painter bold; the frequency row's numbers
    // are drawn plain all the same, at the readable minimum or more.
    void freqScaleNumbersArePlainAfterABoldPainter() {
        BandPlanManager plans;
        plans.loadPlans();
        SpectrumWidget spectrum;
        pose(spectrum, plans);
        WorkspacePreview::setEnabledForTest(true);
        TextDevice device(96);
        QPainter painter(&device);
        QFont bold = painter.font();
        bold.setBold(true);
        painter.setFont(bold);
        spectrum.drawFreqScale(painter, QRect(36, 0, 784, 16));
        painter.end();
        QVERIFY(!device.runs().isEmpty());
        for (const TextDevice::Run& run : device.runs()) {
            QVERIFY2(!run.font.bold(), qPrintable(run.text));
            QVERIFY2(run.font.pixelSize() >= 11, qPrintable(run.text));
        }
    }
    void cachedGpuGeometryAndCpuRasterUseMatchingFonts_data() {
        QTest::addColumn<int>("dpi");
        QTest::addColumn<int>("dpr");
        QTest::newRow("72dpi-1x") << 72 << 1;
        QTest::newRow("96dpi-1x") << 96 << 1;
        QTest::newRow("72dpi-2x") << 72 << 2;
        QTest::newRow("96dpi-2x") << 96 << 2;
    }
    void cachedGpuGeometryAndCpuRasterUseMatchingFonts() {
        QFETCH(int, dpi);
        QFETCH(int, dpr);
        BandPlanManager plans;
        plans.loadPlans();
        SpectrumWidget spectrum;
        pose(spectrum, plans);
        spectrum.setBandPlanFontSize(6);
        WorkspacePreview::setEnabledForTest(true);
        QImage image(QSize(820 * dpr, 300 * dpr), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.setDotsPerMeterX(qRound(dpi / 0.0254));
        image.setDotsPerMeterY(qRound(dpi / 0.0254));
        image.fill(Qt::black);
#ifdef NEREUS_GPU_SPECTRUM
        spectrum.m_overlayStatic = image;
        QCOMPARE(spectrum.bandPlanStripHeight(), spectrum.bandPlanStripHeight(&image));
#endif
        QPainter painter(&image);
        layers(spectrum, painter);
        painter.end();
        const QString output = qEnvironmentVariable("NEREUS_FONT_CAPTURE_DIR");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            QVERIFY(image.save(output + QStringLiteral("/spectrum-%1dpi-%2x.png").arg(dpi).arg(dpr)));
        }
        WorkspacePreview::setEnabledForTest(false);
        QCOMPARE(spectrum.bandPlanStripHeight(&image), 10);
    }
    void cleanup() { WorkspacePreview::setEnabledForTest(false); }
};

QTEST_MAIN(TestWorkspaceSpectrumFontMinimum)
#include "tst_workspace_spectrum_font_minimum.moc"
