// no-port-check: NereusSDR-original GUI history regression.
// Modification history (NereusSDR):
//   2026-10-26 — Skip the GPU frame cases where QRhi is unavailable by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: a remote
//                 row handed over early and put in before its trace is
//                 accepted takes the event on the frame where the source
//                 turns eligible (remoteEarlyRowTakesTheEdgeAsEligibilityTurnsOn).
#include "gui/ImdWaterfallAnnotations.h"
#include "gui/SpectrumWidget.h"
#include "gui/PureSignalViewStyle.h"
#include "core/AppSettings.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "support/ImdSpectrumTestAccess.h"
#include "NativeWindowFrames.h"
#include <QDir>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <cmath>
#include <memory>
#include <QtTest>

using namespace NereusSDR;

namespace NereusSDR {
class ImdWaterfallTestAccess {
public:
    static void rewind(SpectrumWidget& widget, int rows)
    {
        widget.m_wfHistoryOffsetRows = rows;
        widget.m_wfLive = false;
        widget.rebuildWaterfallViewport();
    }
    static QImage cpuFrame(SpectrumWidget& widget)
    {
        const qreal dpr = widget.devicePixelRatioF();
        QImage image(qRound(widget.width() * dpr), qRound(widget.height() * dpr),
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::black);
        QPainter painter(&image);
        widget.m_imdWaterfallGlideRows = 0.0;
        widget.drawWaterfall(painter, widget.waterfallRectForTest());
        return image;
    }
};
}

static int readableTagPixels(const QImage& image, const SpectrumWidget& widget)
{
    const QRect wf = widget.waterfallRectForTest();
    const qreal dpr = image.width() / qreal(widget.width());
    int pixels = 0;
    for (int y = qRound(wf.top() * dpr);
         y < qMin(image.height(), qRound((wf.top() + 24) * dpr)); ++y) {
        for (int x = qRound((wf.left() + 5) * dpr);
             x < qMin(image.width(), qRound((wf.left() + 190) * dpr)); ++x) {
            const QColor colour = image.pixelColor(x, y);
            if (colour.red() > 185 && colour.green() > 185 && colour.blue() > 185) {
                ++pixels;
            }
        }
    }
    return pixels;
}

static int markerInkPixels(const QImage& image, const QColor& ink)
{
    int pixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor colour = image.pixelColor(x, y);
            if (colour.alpha() > 200
                && std::abs(colour.red() - ink.red()) <= 3
                && std::abs(colour.green() - ink.green()) <= 3
                && std::abs(colour.blue() - ink.blue()) <= 3) {
                ++pixels;
            }
        }
    }
    return pixels;
}

class TestImdWaterfallAnnotations : public QObject {
    Q_OBJECT
    QTemporaryDir m_directory;
    std::unique_ptr<AppSettings> m_settings;
private slots:
    void initTestCase()
    {
        m_settings = std::make_unique<AppSettings>(
            m_directory.filePath(QStringLiteral("annotations.settings")));
    }
    void eventUsesAcceptedRowNotPaintTime()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.show();
        widget.setWaterfallTickerPausedForTest(true);
        widget.setDisplayDuplex(true);
        widget.setMoxOverlay(true);
        widget.setTestingIMD(true);
        widget.setShowIMDMeasurements(true);
        ImdSpectrumTestAccess::seedVisiblePixels(widget, QVector<float>(400, -100.0f));
        const QVector<float> row(400, -100.0f);
        widget.queueImdCorrectionEdge(true);
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
        widget.pushWaterfallRowForTest(row); // a retained/synthetic row
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
        widget.pushImdReceiverRowForTest(row);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 1);
        const auto event = widget.imdWaterfallAnnotationsForTest().events().first();
        QCOMPARE(event.rowSequence, widget.waterfallRowSequenceForTest());
        QVERIFY(event.correctionsApplied);
        widget.queueImdCorrectionEdge(false);
        widget.queueImdCorrectionEdge(true); // coalesced before next receiver row
        widget.pushWaterfallRowForTest(row);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 1);
        widget.pushImdReceiverRowForTest(row);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 2);
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().last().correctionsApplied);
        widget.queueImdCorrectionEdge(false);
        widget.pushImdReceiverRowForTest(row);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 3);
        QVERIFY(!widget.imdWaterfallAnnotationsForTest().events().last().correctionsApplied);
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().size()
                <= widget.waterfallHistoryRowsForTest());
    }
    void pausedRowsDoNotCreateTimePositions()
    {
        QCOMPARE(ImdWaterfallAnnotations::visibleRowY(6, 2, 20, 0.0).value(), qreal(4.5));
        QCOMPARE(ImdWaterfallAnnotations::visibleRowY(9, 5, 20, 0.0).value(), qreal(4.5));
    }
    void wrapExpiresEvents()
    {
        ImdWaterfallAnnotations annotations;
        annotations.record(2, 1, true);
        annotations.record(5, 1, false);
        annotations.expireBefore(3);
        QCOMPARE(annotations.events().size(), 1);
        QCOMPARE(annotations.events().first().rowSequence, quint64(5));
        for (quint64 sequence = 6; sequence <= 10006; ++sequence) {
            annotations.record(sequence, 1, true);
        }
        annotations.expireBefore(10000);
        QCOMPARE(annotations.events().size(), 7);
        QCOMPARE(annotations.events().first().rowSequence, quint64(10000));
    }
    void rewindMapsSameSequence()
    {
        QCOMPARE(ImdWaterfallAnnotations::visibleRowY(7, 3, 20, 0.5).value(), qreal(4.0));
        QVERIFY(!ImdWaterfallAnnotations::visibleRowY(7, 9, 20, 0.0));
    }
    void retuneAndReconnectClearContext()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.show();
        widget.setWaterfallTickerPausedForTest(true);
        widget.setDisplayDuplex(true);
        widget.setMoxOverlay(true);
        widget.setTestingIMD(true);
        widget.setShowIMDMeasurements(true);
        ImdSpectrumTestAccess::seedVisiblePixels(widget, QVector<float>(400, -100.0f));
        const QVector<float> row(400, -100.0f);
        widget.queueImdCorrectionEdge(true);
        widget.setDisplayWindowPreservingHistory(14200000.0, 48000.0);
        widget.pushImdReceiverRowForTest(row);
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
        widget.queueImdCorrectionEdge(true);
        widget.pushImdReceiverRowForTest(row);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 1);
        widget.clearWaterfallHistory(); // reconnect/clear path
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
    }
    void preExistingHistoryGetsNoInventedEvents()
    {
        ImdWaterfallAnnotations annotations;
        annotations.expireBefore(30);
        QVERIFY(annotations.events().isEmpty());
    }
    void remoteSyntheticRowCannotConsumePendingEdge()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.show();
        widget.setWaterfallTickerPausedForTest(true);
        widget.setDisplayDuplex(true);
        widget.setMoxOverlay(true);
        widget.setTestingIMD(true);
        widget.setShowIMDMeasurements(true);
        SpectrumEndpointContext context;
        context.codec = {43, 1, -180.0f, 0.0f, 400, 400, 0};
        context.exactCentreHz = 14200000.0;
        context.exactSpanHz = 48000.0;
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        const RemoteSpectrumCapture capture = ImdSpectrumTestAccess::remoteCapture(widget);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.encoderSequence = 1;
        frame.traceDbm = QVector<float>(400, -100.0f);
        frame.waterfallDbm = QVector<float>(400, -100.0f);
        frame.waterfallAdvance = false;
        QVERIFY(widget.updateRemoteSpectrum(frame, capture));
        QVERIFY(widget.imdMeasurementFrame().context.sourceEligible);
        QVERIFY(widget.enqueueRemoteWaterfallRow(frame.waterfallDbm, {}, capture));
        widget.queueImdCorrectionEdge(true);
        widget.tickWaterfallForTest();
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
        frame.encoderSequence = 2;
        frame.waterfallAdvance = true;
        QVERIFY(widget.updateRemoteSpectrum(frame, capture));
        widget.tickWaterfallForTest();
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 1);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().first().rowSequence,
                 widget.waterfallRowSequenceForTest());
    }
    void localCachedReceiverRowBeforeEdgeCannotConsumeIt()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.show();
        widget.setWaterfallTickerPausedForTest(true);
        widget.setDisplayDuplex(true);
        widget.setMoxOverlay(true);
        widget.setTestingIMD(true);
        widget.setShowIMDMeasurements(true);
        const QVector<float> row(400, -100.0f);
        ImdSpectrumTestAccess::seedVisiblePixels(widget, row);
        ImdSpectrumTestAccess::queueAcceptedLocalWaterfallRow(widget, row);
        widget.queueImdCorrectionEdge(true);
        widget.tickWaterfallForTest(); // accepted before the predicate edge
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
        ImdSpectrumTestAccess::seedVisiblePixels(widget, row);
        ImdSpectrumTestAccess::queueAcceptedLocalWaterfallRow(widget, row);
        widget.tickWaterfallForTest();
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 1);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().first().rowSequence,
                 widget.waterfallRowSequenceForTest());
    }
    void remoteBacklogBeforeEdgeCannotConsumeIt()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.show();
        widget.setWaterfallTickerPausedForTest(true);
        widget.setDisplayDuplex(true);
        widget.setMoxOverlay(true);
        widget.setTestingIMD(true);
        widget.setShowIMDMeasurements(true);
        SpectrumEndpointContext context;
        context.codec = {43, 1, -180.0f, 0.0f, 400, 400, 0};
        context.exactCentreHz = 14200000.0;
        context.exactSpanHz = 48000.0;
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        const RemoteSpectrumCapture capture = ImdSpectrumTestAccess::remoteCapture(widget);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.encoderSequence = 1;
        frame.traceDbm = QVector<float>(400, -100.0f);
        frame.waterfallDbm = frame.traceDbm;
        frame.waterfallAdvance = true;
        QVERIFY(widget.updateRemoteSpectrum(frame, capture)); // queues row 1
        widget.queueImdCorrectionEdge(true);
        widget.tickWaterfallForTest();
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
        frame.encoderSequence = 2;
        QVERIFY(widget.updateRemoteSpectrum(frame, capture));
        widget.tickWaterfallForTest();
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 1);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().first().rowSequence,
                 widget.waterfallRowSequenceForTest());
    }
    void remoteEarlyRowTakesTheEdgeAsEligibilityTurnsOn()
    {
        // A remote frame's row is handed over early and goes in about a row
        // period before its trace is accepted. On the frame where the
        // source turns eligible again (after a keyframe wait), that row
        // went in while it was not yet eligible; the acceptance must still
        // tag it, not the row after.
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.show();
        widget.setWaterfallTickerPausedForTest(true);
        widget.setDisplayDuplex(true);
        widget.setMoxOverlay(true);
        widget.setTestingIMD(true);
        widget.setShowIMDMeasurements(true);
        SpectrumEndpointContext context;
        context.codec = {43, 1, -180.0f, 0.0f, 400, 400, 0};
        context.exactCentreHz = 14200000.0;
        context.exactSpanHz = 48000.0;
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        const RemoteSpectrumCapture capture = ImdSpectrumTestAccess::remoteCapture(widget);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.encoderSequence = 1;
        frame.traceDbm = QVector<float>(400, -100.0f);
        frame.waterfallDbm = frame.traceDbm;
        frame.waterfallAdvance = false;
        QVERIFY(widget.updateRemoteSpectrum(frame, capture));
        QVERIFY(widget.imdMeasurementFrame().context.sourceEligible);
        // A keyframe wait: the source is not eligible until a frame is
        // accepted again.
        widget.noteImdReceiverInputLost();
        QVERIFY(!widget.imdMeasurementFrame().context.sourceEligible);
        widget.queueImdCorrectionEdge(true);
        frame.encoderSequence = 2;
        frame.waterfallAdvance = true;
        // Handed over early (due now), and in before its trace.
        QVERIFY(widget.enqueueRemoteWaterfallRow(frame.waterfallDbm, {}, capture, 0,
                                                 frame.encoderSequence));
        widget.tickWaterfallForTest();
        const quint64 earlyRow = widget.waterfallRowSequenceForTest();
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
        // Its trace, accepted: the source turns eligible on this frame.
        QVERIFY(widget.updateRemoteSpectrum(frame, capture, true, false));
        QVERIFY(widget.imdMeasurementFrame().context.sourceEligible);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 1);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().first().rowSequence, earlyRow);
    }
    void nativeGpuWrapPauseRewindGlideCapture()
    {
        NEREUS_SKIP_WITHOUT_QRHI();
#ifdef NEREUS_GPU_SPECTRUM
        SpectrumWidget widget;
        widget.resize(640, 240);
        widget.setWaterfallTickerPausedForTest(true);
        widget.setWfUpdatePeriodMs(250);
        widget.setWaterfallHistoryMs(60000);
        widget.setWfAgcEnabled(false);
        widget.setWfLowThreshold(-130.0f);
        widget.setWfHighThreshold(-50.0f);
        widget.setWfOpacity(100);
        widget.setDbmScaleVisible(false);
        widget.setSampleRate(192000.0);
        widget.setDdcCenterFrequency(14200000.0);
        widget.setDisplayWindowPreservingHistory(14200000.0, 48000.0);
        widget.setDisplayDuplex(true);
        widget.setMoxOverlay(true);
        widget.setTestingIMD(true);
        widget.setShowIMDMeasurements(true);
        QVector<float> trace(640, -120.0f);
        const int indices[] = {80, 176, 272, 368, 464, 560};
        const float levels[] = {-88, -65, -22, -22, -65, -88};
        for (int i = 0; i < 6; ++i) { trace[indices[i]] = levels[i]; }
        ImdSpectrumTestAccess::seedVisiblePixels(widget, trace);
        QSignalSpy submitted(&widget, &QRhiWidget::frameSubmitted);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        NEREUS_VERIFY_FRAME(NativeWindowFrames::requestFrame(widget, submitted, 2000));
        QTest::qWait(300); // history capacity debounce
        ImdSpectrumTestAccess::seedVisiblePixels(widget, trace);
        QVERIFY(widget.imdMeasurementFrame().context.sourceEligible);
        QVERIFY(widget.imdMeasurementFrame().expectedFrequencyHz[2].has_value());
        const QVector<float> row(640, -45.0f);
        const int visibleRows = widget.waterfallHeightForTest();
        QVERIFY(visibleRows > 40);
        for (int i = 0; i < visibleRows + 8; ++i) { widget.pushWaterfallRowForTest(row); }
        widget.queueImdCorrectionEdge(true);
        widget.pushImdReceiverRowForTest(row);
        const quint64 marked = widget.imdWaterfallAnnotationsForTest().events().last().rowSequence;
        for (int i = 0; i < 36; ++i) { widget.pushWaterfallRowForTest(row); }
        widget.queueImdCorrectionEdge(false);
        widget.pushImdReceiverRowForTest(row);
        QCOMPARE(widget.imdWaterfallAnnotationsForTest().events().size(), 2);
        QVERIFY(!widget.imdWaterfallAnnotationsForTest().events().last().correctionsApplied);
        for (int i = 0; i < 3; ++i) { widget.pushWaterfallRowForTest(row); }
        const auto y0 = widget.imdEventYForTest(marked, 0.0);
        const auto yHalf = widget.imdEventYForTest(marked, 0.5);
        QVERIFY(y0 && yHalf);
        QCOMPARE(*y0 - *yHalf, qreal(0.5));
        const QString path = QString::fromUtf8(qgetenv("NEREUS_IMD_EVIDENCE"));
        if (!path.isEmpty()) { QVERIFY(QDir().mkpath(path)); }
        const auto capture = [&](const QString& stem) {
            NEREUS_VERIFY_FRAME(NativeWindowFrames::requestFrame(widget, submitted, 2000));
            const QImage frame = widget.grabFramebuffer();
            QVERIFY(!frame.isNull());
            QCOMPARE(frame.width(), qRound(widget.width() * widget.devicePixelRatioF()));
            if (stem == QStringLiteral("glide0") || stem == QStringLiteral("glidehalf")
                || stem == QStringLiteral("guidesoff")) {
                QVERIFY(readableTagPixels(frame, widget) > 8);
            }
            if (!path.isEmpty()) { QVERIFY(frame.save(path + QStringLiteral("/task6-native-") + stem + QStringLiteral(".png"))); }
        };
        widget.setWaterfallGlideForTest(0.0f);
        capture(QStringLiteral("glide0"));
        const QImage cpu = ImdWaterfallTestAccess::cpuFrame(widget);
        QVERIFY(!cpu.isNull());
        QVERIFY(readableTagPixels(cpu, widget) > 8);
        if (!path.isEmpty()) { QVERIFY(cpu.save(path + QStringLiteral("/task6-cpu-integral.png"))); }
        widget.setWaterfallGlideForTest(0.5f);
        capture(QStringLiteral("glidehalf"));
        const QImage cpuGuidesOn = ImdWaterfallTestAccess::cpuFrame(widget);
        widget.setImdGuidesVisible(false);
        const QImage cpuGuidesOff = ImdWaterfallTestAccess::cpuFrame(widget);
        QVERIFY(cpuGuidesOn != cpuGuidesOff); // vertical frequency guides obey the toggle
        QVERIFY(readableTagPixels(cpuGuidesOff, widget) > 8); // history remains legible
        if (!path.isEmpty()) {
            QVERIFY(cpuGuidesOff.save(path + QStringLiteral("/task6-cpu-guidesoff.png")));
        }
        capture(QStringLiteral("guidesoff"));
        widget.setImdGuidesVisible(true);
        widget.setWaterfallGlideForTest(-1.0f);
        widget.setWaterfallLiveForTest(false);
        const auto pausedY = widget.imdEventYForTest(marked);
        for (int i = 0; i < 2; ++i) { widget.pushWaterfallRowForTest(row); }
        QCOMPARE(widget.imdEventYForTest(marked), pausedY);
        capture(QStringLiteral("paused"));
        ImdWaterfallTestAccess::rewind(widget, widget.waterfallHistoryOffsetRowsForTest() + 2);
        QCOMPARE(widget.imdEventYForTest(marked).value(), pausedY.value() - 2.0);
        capture(QStringLiteral("rewound"));
        widget.setWaterfallLiveForTest(true);
        for (int i = 0; i < widget.waterfallHistoryForTest().height(); ++i) {
            widget.pushWaterfallRowForTest(row);
        }
        QVERIFY(widget.imdWaterfallAnnotationsForTest().events().isEmpty());
        capture(QStringLiteral("wrapped"));
        ImdSpectrumTestAccess::seedVisiblePixels(widget, trace);
        QVERIFY(widget.imdMeasurementFrame().summary.has_value());
        NEREUS_VERIFY_FRAME(NativeWindowFrames::requestFrame(widget, submitted, 2000));
        const QImage current = widget.grabFramebuffer();
        const QColor productInk = PureSignalViewStyle::product(2);
        QVERIFY(markerInkPixels(current, productInk) > 8);
        const qint64 acceptedAtMs = widget.imdMeasurementFrame().context.acceptedAtMs;
        ImdSpectrumTestAccess::checkFreshnessAt(widget,
            acceptedAtMs + ImdSpectrumTestAccess::freshnessTimeoutMs(widget) + 1);
        QCOMPARE(widget.imdMeasurementFrame().status, ImdStatus::WaitingForSpectrum);
        QVERIFY(!widget.imdMeasurementFrame().summary.has_value());
        NEREUS_VERIFY_FRAME(NativeWindowFrames::requestFrame(widget, submitted, 2000));
        const QImage stale = widget.grabFramebuffer();
        QCOMPARE(markerInkPixels(stale, productInk), 0);
        if (!path.isEmpty()) {
            QVERIFY(current.save(path + QStringLiteral("/task6-native-freshness-current.png")));
            QVERIFY(stale.save(path + QStringLiteral("/task6-native-freshness-stale.png")));
        }
#endif
    }
};
QTEST_MAIN(TestImdWaterfallAnnotations)
#include "tst_imd_waterfall_annotations.moc"
