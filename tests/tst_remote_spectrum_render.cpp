// no-port-check: NereusSDR-original. Remote display rendering contract.
// Modification history (NereusSDR):
//   2026-10-01  J.J. Boyd / KG4VCF. Opt-in history trace regression.
//                 AI-assisted via OpenAI Codex.
#include <QTest>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <cmath>
#include "core/AppSettings.h"
#include "core/FFTEngine.h"
#include "core/spectrum/FftEnginePool.h"
#include "models/RadioModel.h"
#define private public
#include "gui/SpectrumWidget.h"
#include "gui/setup/DisplaySetupPages.h"
#undef private
#include "core/session/media/SpectrumEndpoint.h"

using namespace NereusSDR;

class TestRemoteSpectrumRender : public QObject {
    Q_OBJECT
private slots:
    void localStreamIdentity_restartsPartialAtTheActualFftInput()
    {
        SpectrumWidget widget;
        widget.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        widget.setDssRowDivider(2);
        widget.m_bandwidthHz = 24000;
        const QVector<float> bins(4096, 1e-12f);
        widget.updateSpectrumLinear(0, bins, 2.0, -10.0);
        widget.pushWaterfallRow(QVector<float>(8, -40));
        QCOMPARE(widget.m_dssFoldCount, 1);
        widget.updateSpectrumLinear(1, bins, 2.0, -10.0);
        widget.pushWaterfallRow(QVector<float>(8, -130));
        QCOMPARE(widget.m_dssFoldCount, 1);
        QCOMPARE(widget.dssRowsPushedForTest(), 0);
        widget.pushWaterfallRow(QVector<float>(8, -130));
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        const int ring = widget.m_dss.headRing();
        QCOMPARE(widget.m_dss.rowDataRing(ring)[widget.m_dss.cols() / 2], -130.0f);
    }

    void moxEdges_retirePartialReceiveFoldEvenWhenNoTxRowAdvances()
    {
        SpectrumWidget widget;
        widget.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        widget.setDssRowDivider(2);
        widget.setDisplayDuplex(true); // The visible axes do not change on either edge.
        widget.setWaterfallStopOnTx(true);
        widget.pushWaterfallRow(QVector<float>(8, -40));
        QCOMPARE(widget.m_dssFoldCount, 1);
        widget.setMoxOverlay(true);
        QCOMPARE(widget.m_dssFoldCount, 0);
        widget.pushWaterfallRow(QVector<float>(8, -30));
        QCOMPARE(widget.dssRowsPushedForTest(), 0);
        widget.setMoxOverlay(false);
        widget.pushWaterfallRow(QVector<float>(8, -130));
        QCOMPARE(widget.m_dssFoldCount, 1);
        QCOMPARE(widget.dssRowsPushedForTest(), 0);
        widget.pushWaterfallRow(QVector<float>(8, -130));
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        const int ring = widget.m_dss.headRing();
        QCOMPARE(widget.m_dss.rowDataRing(ring)[widget.m_dss.cols() / 2], -130.0f);
    }

    void queuedCapture_keepsWideAxesAndConsumesSuppressedWide()
    {
        SpectrumWidget widget;
        widget.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
        widget.m_waterfallTickerPausedForTest = true;
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        widget.setDssRowDivider(1);
        SpectrumEndpointContext context;
        context.codec = {43, 1, -180, 0, 8, 8, 8};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 24000;
        context.wideCentreHz = 14225000;
        context.wideSpanHz = 96000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        const RemoteSpectrumCapture accepted = widget.m_remoteCapture;
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(8, -100);
        frame.waterfallDbm = QVector<float>(8, -110);
        frame.wideDbm = QVector<float>(8, -120);
        frame.waterfallAdvance = true;
        QVERIFY(widget.updateRemoteSpectrum(frame, accepted));
        ++context.codec.contextGeneration;
        context.wideCentreHz += 1000;
        context.wideSpanHz *= 2;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        QVERIFY(widget.m_remoteRowQueue.front().capture == accepted);
        widget.drainRemoteWaterfallRows();
        QCOMPARE(widget.m_dss.rowWideCenterMhzAtAge(0), 14.225);
        QCOMPARE(widget.m_dss.rowWideBandwidthMhzAtAge(0), 0.096);
        QVERIFY(widget.m_pendingRemoteWide.isEmpty());
        QVERIFY(!widget.updateRemoteSpectrum(frame, accepted));
        widget.setWaterfallStopOnTx(true);
        widget.setMoxOverlay(true);
        QVERIFY(widget.enqueueRemoteWaterfallRow(frame.waterfallDbm, frame.wideDbm, accepted));
        widget.drainRemoteWaterfallRows();
        QVERIFY(widget.m_pendingRemoteWide.isEmpty());
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        widget.setMoxOverlay(false);
        QVERIFY(widget.enqueueRemoteWaterfallRow(frame.waterfallDbm, {}, widget.m_remoteCapture));
        widget.drainRemoteWaterfallRows();
        QCOMPARE(widget.dssRowsPushedForTest(), 2);
        QCOMPARE(widget.m_dss.rowWideBandwidthMhzAtAge(0), 0.0);
        widget.clearRemoteSpectrum();
        QVERIFY(widget.m_remoteRowQueue.isEmpty());
        QCOMPARE(widget.m_dssFoldCount, 0);
    }

    void partialFoldIdentity_resetsWithoutBlankingPaintedHistory_data()
    {
        QTest::addColumn<int>("change");
        QTest::newRow("codec-generation") << 0;
        QTest::newRow("source-generation") << 1;
        QTest::newRow("source-stream") << 2;
        QTest::newRow("wide-axis") << 3;
        QTest::newRow("source-axis") << 4;
        QTest::newRow("adc-generation") << 5;
    }
    void partialFoldIdentity_resetsWithoutBlankingPaintedHistory()
    {
        QFETCH(int, change);
        SpectrumWidget widget;
        widget.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
        widget.m_waterfallTickerPausedForTest = true;
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        widget.setDssRowDivider(2);
        SpectrumEndpointContext context;
        context.codec = {42, 1, -180, 0, 8, 8, 8};
        context.source.streamIndex = 0;
        context.sourceGeneration = 10;
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 24000;
        context.wideCentreHz = context.exactCentreHz;
        context.wideSpanHz = 96000;
        double sourceCentre = context.exactCentreHz;
        widget.setRemoteSpectrumContext(context, sourceCentre, 192000);
        const auto row = [&](float level) {
            DisplayCodecFrame frame;
            frame.context = context.codec;
            frame.traceDbm = QVector<float>(8, level);
            frame.waterfallDbm = QVector<float>(8, level);
            frame.wideDbm = QVector<float>(8, level);
            frame.waterfallAdvance = true;
            QVERIFY(widget.updateRemoteSpectrum(frame));
            widget.drainRemoteWaterfallRows();
        };
        row(-120); row(-120);
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        row(-40);
        switch (change) {
        case 0: ++context.codec.contextGeneration; break;
        case 1: ++context.sourceGeneration; break;
        case 2: ++context.source.streamIndex; break;
        case 3: context.wideCentreHz += 1000; break;
        case 4: sourceCentre += 1000; break;
        case 5: ++context.wideband.sourceGeneration; break;
        }
        widget.setRemoteSpectrumContext(context, sourceCentre, 192000);
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        row(-130);
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        QCOMPARE(widget.m_dssFoldCount, 1);
        row(-130);
        QCOMPARE(widget.dssRowsPushedForTest(), 2);
        const int ring = widget.m_dss.headRing();
        QCOMPARE(widget.m_dss.rowDataRing(ring)[widget.m_dss.cols() / 2], -130.0f);
        row(-30);
        widget.clearRemoteSpectrum();
        QCOMPARE(widget.m_dssFoldCount, 0);
    }

    void localPeakFold_calibratesEachActualPlaneBeforeMaximum()
    {
        SpectrumWidget widget;
        widget.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        widget.setDssRowDivider(2);
        widget.m_bandwidthHz = 24000;
        widget.setDbmCalOffset(18);
        widget.m_lastFullBinsDbm = QVector<float>(4096, -100);
        widget.pushWaterfallRow(QVector<float>(8, -100));
        widget.setDbmCalOffset(0);
        widget.m_lastFullBinsDbm.fill(-90);
        widget.pushWaterfallRow(QVector<float>(8, -90));
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        const int ring = widget.m_dss.headRing();
        QCOMPARE(widget.m_dss.rowDataRing(ring)[widget.m_dss.cols() / 2], -82.0f);
        bool sawWide = false;
        for (int c = 0; c < widget.m_dss.cols(); ++c) {
            if (widget.m_dss.rowWideCoverageRing(ring)[c]) {
                QCOMPARE(widget.m_dss.rowWideDataRing(ring)[c], -82.0f);
                sawWide = true;
            }
        }
        QVERIFY(sawWide);
    }

    void suppliedWidePeakFold_keepsFirstTickAndResetsOnRenewal()
    {
        SpectrumWidget widget;
        widget.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
        widget.m_waterfallTickerPausedForTest = true;
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        widget.setDssRowDivider(3);
        widget.setDbmCalOffset(18.0f); // Supplied remote planes are calibrated already.
        SpectrumEndpointContext context;
        context.codec = {41, 1, -180, 0, 8, 8, 8};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 24000;
        context.wideCentreHz = context.exactCentreHz;
        context.wideSpanHz = 96000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        const auto row = [&](float exact, float wide) {
            DisplayCodecFrame frame;
            frame.context = context.codec;
            frame.traceDbm = QVector<float>(8, -80);
            frame.waterfallDbm = QVector<float>(8, exact);
            frame.wideDbm = QVector<float>(8, wide);
            frame.waterfallAdvance = true;
            QVERIFY(widget.updateRemoteSpectrum(frame));
            widget.drainRemoteWaterfallRows();
        };
        row(-60, -50);
        row(-130, -120);
        row(-130, -120);
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        const int ring = widget.m_dss.headRing();
        bool sawWide = false;
        for (int c = 0; c < widget.m_dss.cols(); ++c) {
            if (widget.m_dss.rowWideCoverageRing(ring)[c]) {
                QCOMPARE(widget.m_dss.rowWideDataRing(ring)[c], -50.0f);
                sawWide = true;
            }
        }
        QVERIFY(sawWide);
        QCOMPARE(widget.m_dss.rowDataRing(ring)[widget.m_dss.cols() / 2], -60.0f);
        row(-40, -30); // A partial fold must not cross a generation renewal.
        ++context.codec.contextGeneration;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        row(-130, -120);
        QCOMPARE(widget.m_dssFoldCount, 1);
        row(-130, -120);
        row(-130, -120);
        QCOMPARE(widget.dssRowsPushedForTest(), 2);
        const int renewed = widget.m_dss.headRing();
        QCOMPARE(widget.m_dss.rowDataRing(renewed)[widget.m_dss.cols() / 2], -130.0f);
        widget.clearRemoteSpectrum();
        QCOMPARE(widget.m_dssFoldCount, 0);
    }

    void remoteHistoryTraceIsOptInAndKeepsClearSemantics()
    {
        const QByteArray previous = qgetenv("NEREUS_TRACE_RX_HISTORY");
        const bool wasSet = qEnvironmentVariableIsSet("NEREUS_TRACE_RX_HISTORY");
        const auto restore = qScopeGuard([previous, wasSet]() {
            if (wasSet) { qputenv("NEREUS_TRACE_RX_HISTORY", previous); }
            else { qunsetenv("NEREUS_TRACE_RX_HISTORY"); }
        });
        SpectrumWidget widget;
        qputenv("NEREUS_TRACE_RX_HISTORY", "1");
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("^RX_HISTORY reason=1 pan=")));
        widget.clearRemoteSpectrum();
        QCOMPARE(widget.dssRowsPushedForTest(), 0);
        QCOMPARE(widget.waterfallHistoryRowsForTest(), 0);
        QCOMPARE(widget.peakDbmInPassband(9996000, 9998000), -400.0);
    }

    void remoteFrequencyScaleDragStopsAtAvailableSourceBandwidth()
    {
        SpectrumWidget widget;
        widget.resize(1000, 700);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setConnectionState(ConnectionState::Connected);
        widget.setExtendedViewAllowed(true); // Keep the operator's saved preference.
        widget.setVfoFrequency(14225000);
        SpectrumEndpointContext context;
        context.codec = {31, 1, -180, 0, 128, 128, 0};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 180000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        QSignalSpy spans(&widget, &SpectrumWidget::bandwidthChangeRequested);
        const int y = widget.notchSpecRect().height() + widget.kDividerH
            + widget.kFreqScaleH / 2;
        const auto mouse = [&](QEvent::Type type, int x, Qt::MouseButton button,
                               Qt::MouseButtons buttons) {
            const QPointF pos(x, y);
            QMouseEvent event(type, pos, widget.mapToGlobal(pos), button, buttons,
                              Qt::NoModifier);
            QCoreApplication::sendEvent(&widget, &event);
        };
        mouse(QEvent::MouseButtonPress, 800, Qt::LeftButton, Qt::LeftButton);
        mouse(QEvent::MouseMove, 300, Qt::NoButton, Qt::LeftButton);
        QCOMPARE(widget.bandwidth(), 192000.0); // Clamp during drag, before any ACK.
        mouse(QEvent::MouseButtonRelease, 300, Qt::LeftButton, Qt::NoButton);
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.first().first().toDouble(), 192000.0);
        context.codec.contextGeneration++;
        context.exactSpanHz = 192000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        QCOMPARE(widget.bandwidth(), 192000.0); // Accepted source cannot snap it narrower.
        QVERIFY(widget.extendedViewAllowed());
        QVERIFY(!widget.extendedMode());

        SpectrumWidget local;
        local.setSampleRate(192000);
        local.setExtendedViewAllowed(true);
        QVERIFY(local.maxZoomOutBandwidthHz() > 192000.0);
    }

    void remoteMaxBinUsesEachRequestedPassbandAndClearsWithContext()
    {
        SpectrumWidget widget;
        SpectrumEndpointContext context;
        context.codec = {29, 1, -180, 0, 11, 11, 0};
        context.exactCentreHz = 10000000;
        context.exactSpanHz = 10000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(11, -120);
        frame.traceDbm[2] = -62;
        frame.traceDbm[8] = -43;
        frame.waterfallDbm = QVector<float>(11, -110);
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QCOMPARE(widget.peakDbmInPassband(9996000, 9998000), -62.0);
        QCOMPARE(widget.peakDbmInPassband(10002000, 10004000), -43.0);
        QCOMPARE(widget.peakDbmInPassband(11000000, 11002000), -400.0);
        widget.clearRemoteSpectrum();
        QCOMPARE(widget.peakDbmInPassband(9996000, 9998000), -400.0);
    }

    void independentPlanesAndSuppliedWideCoverage()
    {
        SpectrumWidget widget;
        widget.resize(500, 300);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        widget.setDssRowDivider(1);
        widget.setDbmCalOffset(18.0f); // A remote client-local value is ignored.
        widget.setWfUpdatePeriodMs(20);
        widget.setActivePeakHoldEnabled(true);
        SpectrumEndpointContext context;
        context.codec = {1, 1, -180, 0, 128, 128, 96};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 24000;
        context.wideCentreHz = context.exactCentreHz;
        context.wideSpanHz = 96000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(128, -80);
        frame.waterfallDbm = QVector<float>(128, -130);
        frame.wideDbm = QVector<float>(96, -110);
        frame.waterfallAdvance = true;
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QCOMPARE(widget.renderedPixels(), frame.traceDbm);
        QCOMPARE(widget.wfRenderedPixels(), frame.waterfallDbm);
        QCOMPARE(widget.activePeakHoldPeaksForTest().size(), frame.traceDbm.size());
        // The first frame's reset starts the 500 ms display delay (Thetis
        // display.cs:859-877 [v2.10.3.15]): the trace is held back.
        QVERIFY(!widget.activePeakHoldActive());
        QTRY_COMPARE(widget.dssRowsPushedForTest(), 1);
        QCOMPARE(widget.dssNewestRowWideBandwidthForTest(), 0.096);
        // A display pause must not replay the last received waterfall row.
        QTest::qWait(100);
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        frame.waterfallAdvance = false;
        frame.traceDbm.fill(-70);
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QTest::qWait(70);
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        QCOMPARE(widget.renderedPixels(), frame.traceDbm);
    }

    void remoteFramesIgnoreClientCalibrationWhileLocalRenderingKeepsIt()
    {
        SpectrumWidget local;
        SpectrumWidget remote;
        SpectrumWidget uncalibrated;
        uncalibrated.setDbmCalOffset(0.0f);
        local.setDbmCalOffset(18.0f);
        remote.setDbmCalOffset(18.0f); // Deliberately wrong for the station.

        SpectrumEndpointContext context;
        context.codec = {19, 1, -180, 0, 64, 64, 0};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 24000;
        remote.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);

        const QRect plot(0, 0, 640, 240);
        constexpr float stationCalibratedDbm = -120.0f;
        QCOMPARE(remote.dbmToY(stationCalibratedDbm, plot),
                 uncalibrated.dbmToY(stationCalibratedDbm, plot));
        QCOMPARE(remote.dbmToYf(stationCalibratedDbm, plot),
                 uncalibrated.dbmToYf(stationCalibratedDbm, plot));
        QVERIFY(local.dbmToY(stationCalibratedDbm, plot)
                 < remote.dbmToY(stationCalibratedDbm, plot));
    }

    void receiveWaterfallCalibratesLocalRowsOnly()
    {
        const QVector<float> rawRow(8, -100.0f);
        const auto renderedColor = [&](float rowDbm, float offset, bool remote) {
            SpectrumWidget widget;
            widget.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
            widget.m_waterfall.fill(Qt::black);
            widget.setWfLowThreshold(-130.0f);
            widget.setWfHighThreshold(-60.0f);
            widget.setWfAgcEnabled(false);
            widget.setWaterfallNFAGCEnabled(false);
            widget.setDbmCalOffset(offset);
            widget.m_remoteSpectrum = remote;
            widget.pushWaterfallRow(QVector<float>(8, rowDbm));
            return widget.m_waterfall.pixel(0, widget.m_wfWriteRow);
        };

        QCOMPARE(renderedColor(-100.0f, 18.0f, false),
                 renderedColor(-82.0f, 0.0f, false));
        QCOMPARE(renderedColor(-100.0f, -18.0f, false),
                 renderedColor(-118.0f, 0.0f, false));
        // The Core has already calibrated a remote row. The client setting
        // belongs to its own radio and must not shift that row a second time.
        QCOMPARE(renderedColor(-100.0f, 18.0f, true),
                 renderedColor(-100.0f, 0.0f, true));

        for (float offset : {18.0f, -18.0f}) {
            SpectrumWidget local;
            SpectrumWidget alreadyCalibrated;
            local.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
            alreadyCalibrated.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
            local.setDbmCalOffset(offset);
            local.setWaterfallNFAGCEnabled(true);
            alreadyCalibrated.setWaterfallNFAGCEnabled(true);
            local.pushWaterfallRow(rawRow);
            alreadyCalibrated.pushWaterfallRow(QVector<float>(8, -100.0f + offset));
            QCOMPARE(local.wfActiveLowThreshold(), alreadyCalibrated.wfActiveLowThreshold());
            QCOMPARE(local.wfActiveHighThreshold(), alreadyCalibrated.wfActiveHighThreshold());
        }

        const auto txColor = [](float rowDbm, float offset) {
            SpectrumWidget widget;
            widget.m_waterfall = QImage(8, 2, QImage::Format_RGB32);
            widget.setTxDisplayCalOffsetDb(offset);
            widget.setMoxOverlay(true);
            widget.pushTxWaterfallRow(0, QVector<float>(8, rowDbm));
            return widget.m_waterfall.pixel(0, widget.m_wfWriteRow);
        };
        QCOMPARE(txColor(-40.0f, 18.0f), txColor(-22.0f, 0.0f));

        SpectrumWidget local3d;
        SpectrumWidget reference3d;
        for (SpectrumWidget* widget : {&local3d, &reference3d}) {
            widget->m_waterfall = QImage(8, 2, QImage::Format_RGB32);
            widget->setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
            widget->m_bandwidthHz = 24000;
        }
        local3d.setDbmCalOffset(18.0f);
        local3d.m_lastFullBinsDbm = QVector<float>(4096, -100.0f);
        reference3d.m_lastFullBinsDbm = QVector<float>(4096, -82.0f);
        local3d.pushWaterfallRow(rawRow);
        reference3d.pushWaterfallRow(QVector<float>(8, -82.0f));
        QVERIFY(local3d.m_dss.rowWideBandwidthMhzAtAge(0) > 0.0);
        const int localRing = local3d.m_dss.headRing();
        const int referenceRing = reference3d.m_dss.headRing();
        bool comparedWideBin = false;
        for (int c = 0; c < local3d.m_dss.cols(); ++c) {
            if (!local3d.m_dss.rowWideCoverageRing(localRing)[c]) {
                continue;
            }
            QCOMPARE(local3d.m_dss.rowWideDataRing(localRing)[c],
                     reference3d.m_dss.rowWideDataRing(referenceRing)[c]);
            comparedWideBin = true;
        }
        QVERIFY(comparedWideBin);
    }

    void acceptedGeometryReprojectsPaintedHistory()
    {
        SpectrumWidget widget;
        SpectrumEndpointContext context;
        context.codec = {7, 1, -180, 0, 128, 128, 0};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 24000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        // A previously painted RF marker at 14.225 MHz, in both 2D rings.
        // At 50 Hz/pixel a 1 kHz view correction moves it 20 pixels left.
        QImage painted(480, 2, QImage::Format_RGB32);
        painted.fill(Qt::black);
        painted.setPixel(240, 0, qRgb(255, 0, 0));
        widget.m_waterfall = painted;
        // Record through the production history append path so the ring has
        // its real capacity, write position, timestamp and captured RF window.
        widget.appendHistoryRow(
            reinterpret_cast<const QRgb*>(painted.constScanLine(0)), 1234);
        const QImage originalHistory = widget.m_waterfallHistory;
        const QVector<qint64> originalTimestamps = widget.m_wfHistoryTimestamps;
        const int capturedRow = widget.m_wfHistoryWriteRow;
        QCOMPARE(widget.m_wfHistoryWindows[capturedRow].centerHz, context.exactCentreHz);
        QCOMPARE(widget.m_wfHistoryWindows[capturedRow].bandwidthHz, context.exactSpanHz);
        ++context.codec.contextGeneration;
        context.exactCentreHz += 1000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        QCOMPARE(widget.m_waterfall.pixel(220, 0), qRgb(255, 0, 0));
        QCOMPARE(widget.m_waterfall.pixel(240, 0), qRgb(0, 0, 0));
        QCOMPARE(widget.m_waterfallHistory, originalHistory);
        QCOMPARE(widget.m_wfHistoryRowCount, 1);
        QCOMPARE(widget.m_wfHistoryTimestamps, originalTimestamps);
        QCOMPARE(widget.m_wfHistoryTimestamps[capturedRow], qint64(1234));
        // Returning to the original view must recover the original RF marker
        // without reprojecting or cropping its retained source pixels.
        ++context.codec.contextGeneration;
        context.exactCentreHz -= 1000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        QCOMPARE(widget.m_waterfall.pixel(240, 0), qRgb(255, 0, 0));
        QCOMPARE(widget.m_waterfall.pixel(220, 0), qRgb(0, 0, 0));
        QCOMPARE(widget.m_waterfallHistory, originalHistory);
        QCOMPARE(widget.m_wfHistoryTimestamps, originalTimestamps);
        QCOMPARE(widget.m_wfHistoryRowCount, 1);
    }

    void contextRenewalPreservesHistoryWhileRejectingOldData_data()
    {
        QTest::addColumn<bool>("moveView");
        QTest::newRow("fixed-view-source-retune") << false;
        QTest::newRow("small-view-tune") << true;
    }

    void contextRenewalPreservesHistoryWhileRejectingOldData()
    {
        QFETCH(bool, moveView);
        SpectrumWidget widget;
        widget.resize(500, 300);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        widget.setDssRowDivider(1);
        widget.setWfUpdatePeriodMs(20);
        SpectrumEndpointContext context;
        context.codec = {7, 1, -180, 0, 128, 128, 0};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 24000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(128, -90);
        frame.waterfallDbm = QVector<float>(128, -120);
        frame.waterfallAdvance = true;
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QTRY_COMPARE(widget.dssRowsPushedForTest(), 1);
        QCOMPARE(widget.m_wfHistoryRowCount, 1);
        if (moveView) {
            context.exactCentreHz += 500;
            widget.setCenterFrequency(context.exactCentreHz);
        }
        const QImage paintedHistory = widget.m_waterfallHistory;
        const QImage paintedViewport = widget.m_waterfall;
        const auto timestamps = widget.m_wfHistoryTimestamps;
        ++context.codec.contextGeneration;
        widget.setRemoteSpectrumContext(context, 14225500, 192000);
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        QCOMPARE(widget.m_dss.rowCenterMhzAtAge(0), 14.225);
        QCOMPARE(widget.m_wfHistoryRowCount, 1);
        QCOMPARE(widget.m_waterfallHistory, paintedHistory);
        QCOMPARE(widget.m_waterfall, paintedViewport);
        QCOMPARE(widget.m_wfHistoryTimestamps, timestamps);
        // R-R3-21: the old trace stays, drawn at the frequency it was
        // captured at, until the new context's first frame; old data is
        // still refused.
        QCOMPARE(widget.renderedPixels().size(), 128);
        QCOMPARE(widget.renderedPixels().at(64), -90.0f);
        QVERIFY(!widget.updateRemoteSpectrum(frame));
        QTest::qWait(80);
        QCOMPARE(widget.dssRowsPushedForTest(), 1); // No replay of the old pending row.
        frame.context = context.codec;
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QTRY_COMPARE(widget.dssRowsPushedForTest(), 2);
        QCOMPARE(widget.m_dss.rowCenterMhzAtAge(1), 14.225);
        QCOMPARE(widget.m_wfHistoryRowCount, 2);
        widget.setCenterFrequency(context.exactCentreHz + 200000);
        // Nothing captured lies in the new window: no trace.
        QVERIFY(widget.renderedPixels().isEmpty());
        QVERIFY(!widget.updateRemoteSpectrum(frame));
        widget.clearRemoteSpectrum();
        QVERIFY(!widget.updateRemoteSpectrum(frame));
        QTest::qWait(80);
        QCOMPARE(widget.dssRowsPushedForTest(), 0);
        QCOMPARE(widget.m_wfHistoryRowCount, 0);
    }

    // Parity Task 17 (B3.2): a remote pan's bin width, Hz/bin readout and
    // normalise shift follow the FFT size the Core granted, not this
    // window's idle engine (4096), and follow a new grant after a zoom.
    void remoteBinWidthFollowsTheCoresGrantedFftSize()
    {
        SpectrumWidget widget;
        widget.setDispNormalize(true);
        // Normalise applies with the Average, Sample and RMS detectors only.
        widget.setSpectrumDetector(SpectrumDetector::Average);
        SpectrumEndpointContext context;
        context.codec = {41, 1, -180, 0, 128, 128, 0};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 96000;
        context.targetFps = 30;
        QSignalSpy grants(&widget, &SpectrumWidget::remoteSpectrumGrantChanged);
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000, 16384);
        QCOMPARE(widget.remoteGrantedFftSize(), 16384);
        QCOMPARE(widget.binWidthHz(), 192000.0 / 16384.0);
        // No 6 dB error from a 4096-point guess: -10 log10(11.71875 Hz).
        QVERIFY(std::abs(widget.normalizeShiftDb()
                         - float(-10.0 * std::log10(192000.0 / 16384.0))) < 1.0e-4f);
        QCOMPARE(grants.size(), 1);

        // A zoom the Core answers with a finer engine.
        context.codec.contextGeneration++;
        context.exactSpanHz = 12000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000, 65536);
        QCOMPARE(widget.binWidthHz(), 192000.0 / 65536.0);
        QCOMPARE(grants.size(), 2);
        // The same grant again moves nothing.
        context.codec.contextGeneration++;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000, 65536);
        QCOMPARE(grants.size(), 2);

        // Without a grant (0) the old guess stands until one arrives, and a
        // retired binding forgets the grant.
        widget.clearRemoteSpectrum();
        QCOMPARE(widget.remoteGrantedFftSize(), 0);
        QCOMPARE(widget.binWidthHz(), 192000.0 / 4096.0);
    }

    // Parity Task 17 (B3.10): the active peak hold falls at its rate per
    // second at the frame rate the Core sends (the context's, after the
    // display budget), not at this window's display timer.
    void remotePeakHoldDecaysAtTheCoresFrameRate()
    {
        SpectrumWidget widget;
        widget.setDisplayFps(30);
        widget.setActivePeakHoldEnabled(true);
        widget.setActivePeakHoldDurationMs(100);
        widget.setActivePeakHoldDropDbPerSec(60.0);
        SpectrumEndpointContext context;
        context.codec = {43, 1, -180, 0, 4, 4, 0};
        context.exactCentreHz = 10000000;
        context.exactSpanHz = 10000;
        context.targetFps = 10; // the budget lowered this pan to 10 frames a second
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000, 4096);
        QCOMPARE(widget.remoteFrameRate(), 10);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(4, -50.0f);
        frame.waterfallDbm = QVector<float>(4, -110.0f);
        // Frames 100 ms apart at 10 a second. The first frame's reset holds
        // the trace back 500 ms (Thetis display.cs:859-877 [v2.10.3.15]):
        // the frame starting at 600 ms ends the delay, so the one at 700 ms
        // is the first to raise the peak. (At the window's 30 a frame the
        // delay would last 16 frames, not 7.)
        for (int i = 0; i < 7; ++i) {
            QVERIFY(widget.updateRemoteSpectrum(frame));
            QVERIFY(!widget.activePeakHoldActive());
        }
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QVERIFY(widget.activePeakHoldActive());
        QCOMPARE(widget.activePeakHoldPeaksForTest().first(), -50.0f);
        frame.traceDbm = QVector<float>(4, -100.0f);
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QVERIFY(widget.updateRemoteSpectrum(frame));
        // The peak raised at 700 ms holds through 800 ms (not more than the
        // 100 ms hold), then the frame at 900 ms drops it 60 dB/s / 10 = 6 dB.
        QCOMPARE(widget.activePeakHoldPeaksForTest().size(), 4);
        QVERIFY(std::abs(widget.activePeakHoldPeaksForTest().first() - -56.0f) < 1.0e-3f);
    }

    // Thetis display.cs:5011 [v2.10.3.15]: a transmitting pan's peak hold is
    // off unless "Update during TX" is on. The pan's MOX overlay is the
    // transmitting state, for a remote pan as for a local one.
    void peakHoldStopsWhileThePanTransmitsUnlessUpdateDuringTx()
    {
        SpectrumWidget widget;
        widget.setActivePeakHoldEnabled(true);
        widget.setActivePeakHoldDurationMs(100);
        widget.setActivePeakHoldDropDbPerSec(60.0);
        SpectrumEndpointContext context;
        context.codec = {44, 1, -180, 0, 4, 4, 0};
        context.exactCentreHz = 10000000;
        context.exactSpanHz = 10000;
        context.targetFps = 10;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000, 4096);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(4, -50.0f);
        frame.waterfallDbm = QVector<float>(4, -110.0f);
        // Past the 500 ms display delay after the first frame's reset.
        for (int i = 0; i < 8; ++i) {
            QVERIFY(widget.updateRemoteSpectrum(frame));
        }
        QVERIFY(widget.activePeakHoldActive());
        QCOMPARE(widget.activePeakHoldPeaksForTest().first(), -50.0f);

        widget.setDisplayDuplex(true);   // keep the receive axis and trace
        widget.setMoxOverlay(true);
        QVERIFY(!widget.activePeakHoldActive());
        frame.traceDbm = QVector<float>(4, -20.0f);
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QCOMPARE(widget.activePeakHoldPeaksForTest().first(), -50.0f);

        widget.setActivePeakHoldOnTx(true);
        QVERIFY(widget.activePeakHoldActive());
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QCOMPARE(widget.activePeakHoldPeaksForTest().first(), -20.0f);

        widget.setActivePeakHoldOnTx(false);
        widget.setMoxOverlay(false);
        QVERIFY(widget.activePeakHoldActive());
    }

    // Parity Task 17 (B3.9, C10, C11): in a remote window Spectrum Defaults
    // opens on the Core's stored FFT size, window, Hz/bin target and FPS
    // (the station keys this window holds from the Core), its readouts show
    // the Core's grant for the pan, and Cal Offset and Display Thread
    // Priority are disabled with their reasons.
    void spectrumDefaultsInARemoteWindowShowTheCoresSpectrum()
    {
        auto& settings = AppSettings::instance();
        const QStringList keys{QStringLiteral("DisplayFftSize"), QStringLiteral("DisplayFftWindow"),
                               QStringLiteral("DisplayHzPerBinTarget"),
                               QStringLiteral("DisplaySpectrumFps")};
        QHash<QString, QVariant> saved;
        for (const QString& key : keys) {
            if (settings.contains(key)) { saved.insert(key, settings.value(key)); }
        }
        const auto restore = qScopeGuard([&] {
            for (const QString& key : keys) {
                if (saved.contains(key)) { settings.setValue(key, saved.value(key)); }
                else { settings.remove(key); }
            }
        });
        settings.setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("16384"));
        settings.setValue(QStringLiteral("DisplayFftWindow"), QStringLiteral("2"));
        settings.setValue(QStringLiteral("DisplayHzPerBinTarget"), QStringLiteral("3.5"));
        settings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("25"));

        RadioModel remote(RadioModel::Role::Remote);
        SpectrumWidget widget;
        NereusSDR::FFTEngine idle(-1); // A remote window's own engine does not run.
        remote.setSpectrumWidget(&widget);
        remote.setFftEngine(&idle);
        SpectrumDefaultsPage page(&remote);
        page.setStationSettingsAvailable(true, QString());

        QCOMPARE(page.m_fftSizeSlider->value(), 2);        // 4096 << 2
        QCOMPARE(page.m_windowCombo->currentIndex(), 2);   // Hann
        QCOMPARE(page.m_hzPerBinTargetSpin->value(), 3.5);
        QCOMPARE(page.m_fpSlider->value(), 25);
        QCOMPARE(page.m_fpSpin->value(), 25);
        // No grant yet: the stored size, no bin width.
        QCOMPARE(page.m_fftSizeReadout->text(), QStringLiteral("16384"));
        QCOMPARE(page.m_binWidthLabel->text(), QStringLiteral("0.000"));

        // The Core grants this pan 65536 points on a 192 kHz receiver.
        SpectrumEndpointContext context;
        context.codec = {51, 1, -180, 0, 128, 128, 0};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 12000;
        context.targetFps = 25;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000, 65536);
        QCOMPARE(page.m_fftSizeReadout->text(), QStringLiteral("65536"));
        QCOMPARE(page.m_binWidthLabel->text(), QString::number(192000.0 / 65536.0, 'f', 3));
        QCOMPARE(page.m_binWidthReadout->text(),
                 QStringLiteral("%1 Hz/bin").arg(192000.0 / 65536.0, 0, 'f', 3));

        QVERIFY(!page.m_calOffsetSpin->isEnabled());
        QCOMPARE(page.m_calOffsetSpin->toolTip(),
                 QStringLiteral("The Core calibrates the display for its radio."));
        QVERIFY(!page.m_threadPriorityCombo->isEnabled());
        QCOMPARE(page.m_threadPriorityCombo->toolTip(),
                 QStringLiteral("The Core sets its own display thread priority."));
        // The Core's settings going away and coming back keeps them disabled.
        page.setStationSettingsAvailable(false, QStringLiteral("Waiting for the Core."));
        page.setStationSettingsAvailable(true, QString());
        QVERIFY(!page.m_calOffsetSpin->isEnabled());
        QVERIFY(!page.m_threadPriorityCombo->isEnabled());

        // A local window keeps both.
        RadioModel local;
        SpectrumWidget localWidget;
        NereusSDR::FFTEngine localEngine(-1);
        local.setSpectrumWidget(&localWidget);
        local.setFftEngine(&localEngine);
        SpectrumDefaultsPage localPage(&local);
        QVERIFY(localPage.m_calOffsetSpin->isEnabled());
        QVERIFY(localPage.m_threadPriorityCombo->isEnabled());
        remote.setSpectrumWidget(nullptr);
        remote.setFftEngine(nullptr);
        local.setSpectrumWidget(nullptr);
        local.setFftEngine(nullptr);
    }

    // Parity Task 17 follow-up (R-R3-01): Rendering > Decimation applies to
    // every pan in a local window, not only stream 0's engine: every engine
    // the pool has, and every engine it makes afterwards, as a remote window
    // sends it for every pan.
    void localDecimationReachesEveryPansEngine()
    {
        RadioModel local;
        SpectrumWidget widget;
        NereusSDR::FftEnginePool pool;
        NereusSDR::FFTEngine* stream0 = pool.engineForStream(0);
        NereusSDR::FFTEngine* stream1 = pool.engineForStream(1);
        QVERIFY(stream0 && stream1);
        local.setSpectrumWidget(&widget);
        local.setFftEngine(stream0);
        local.setFftEnginePool(&pool);
        SpectrumDefaultsPage page(&local);
        page.m_decimationSpin->setValue(6);
        QCOMPARE(stream0->decimation(), 6);
        QCOMPARE(stream1->decimation(), 6);
        NereusSDR::FFTEngine* stream2 = pool.engineForStream(2);
        QVERIFY(stream2);
        QCOMPARE(stream2->decimation(), 6);
        local.setSpectrumWidget(nullptr);
        local.setFftEngine(nullptr);
        local.setFftEnginePool(nullptr);
    }
};
QTEST_MAIN(TestRemoteSpectrumRender)
#include "tst_remote_spectrum_render.moc"
