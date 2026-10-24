// no-port-check: NereusSDR-original regressions for the GPU static overlay
// cache (m_overlayStatic). The cache is rebuilt and uploaded only when
// m_overlayStaticDirty is set, so steady frames must leave it clean, and
// every input that changes what the overlay draws must set it.
//
// Offscreen there is no QRhi, so renderGpuFrame() never runs: the dirty
// flag itself is the counter. A frame that leaves the flag set is a frame
// that would rebuild the overlay and upload the full-window texture.

#include <QtTest>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>

#include "gui/SpectrumWidget.h"
#include "gui/widgets/VfoWidget.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "support/ImdSpectrumTestAccess.h"

#include <array>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 20;
constexpr std::array<int, 6> kImdPositions{100, 220, 340, 460, 580, 700};
constexpr std::array<float, 6> kImdLevels{-88.0f, -65.0f, -22.0f, -22.0f, -65.0f, -88.0f};

void configure(SpectrumWidget& w)
{
    w.resize(800, 600);
    w.setSampleRate(192000.0);
    w.setDdcCenterFrequency(14200000.0);
    w.setDisplayWindowPreservingHistory(14200000.0, 48000.0);
    w.setSpectrumAveraging(SpectrumAveraging::None);
    w.setDbmRange(-140.0f, 0.0f);
}

void configureImd(SpectrumWidget& w)
{
    configure(w);
    w.setDisplayDuplex(true);
    w.setMoxOverlay(true);
    w.setTestingIMD(true);
    w.setShowIMDMeasurements(true);
}

QVector<float> linearBins(int size, float level = 1.0e-12f)
{
    return QVector<float>(size, level);
}

QVector<float> imdFrame()
{
    QVector<float> frame(800, -120.0f);
    for (size_t i = 0; i < kImdPositions.size(); ++i) {
        frame[kImdPositions[i]] = kImdLevels[i];
    }
    return frame;
}

SpectrumEndpointContext remoteContext()
{
    SpectrumEndpointContext context;
    context.codec = {43, 1, -180.0f, 0.0f, 800, 800, 0};
    context.exactCentreHz = 14200000.0;
    context.exactSpanHz = 48000.0;
    context.targetFps = 30;
    context.framesPerLine = 1;
    return context;
}

DisplayCodecFrame remoteFrame(const SpectrumEndpointContext& context, quint32 sequence)
{
    DisplayCodecFrame frame;
    frame.context = context.codec;
    frame.encoderSequence = sequence;
    frame.traceDbm = QVector<float>(800, -120.0f);
    frame.waterfallDbm = QVector<float>(800, -120.0f);
    frame.waterfallAdvance = true;
    return frame;
}

// Runs one input change against a clean cache and answers whether it
// dirtied the cache.
template <typename Change>
bool dirties(SpectrumWidget& w, Change&& change)
{
    w.clearOverlayStaticDirtyForTest();
    change();
    return w.overlayStaticDirtyForTest();
}

} // namespace

class TestSpectrumOverlayDirty : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
#ifndef NEREUS_GPU_SPECTRUM
        QSKIP("CPU-only build: there is no cached overlay texture");
#endif
    }

    // ---- Steady frames: no rebuild after the first ----

    void steadyLocalFramesLeaveTheOverlayClean()
    {
        SpectrumWidget w;
        configure(w);
        w.updateSpectrumLinear(0, linearBins(4096), 1.0, 0.0);
        int rebuilds = 0;
        for (int i = 0; i < kFrames; ++i) {
            if (dirties(w, [&] { w.updateSpectrumLinear(0, linearBins(4096), 1.0, 0.0); })) {
                ++rebuilds;
            }
        }
        QCOMPARE(rebuilds, 0);
    }

    void steadyRemoteFramesLeaveTheOverlayClean()
    {
        SpectrumWidget w;
        configure(w);
        const SpectrumEndpointContext context = remoteContext();
        w.setRemoteSpectrumContext(context, 14200000.0, 192000.0, 4096);
        QVERIFY(w.updateRemoteSpectrum(remoteFrame(context, 1)));
        int rebuilds = 0;
        for (int i = 0; i < kFrames; ++i) {
            const DisplayCodecFrame frame = remoteFrame(context, quint32(i + 2));
            if (dirties(w, [&] { QVERIFY(w.updateRemoteSpectrum(frame)); })) {
                ++rebuilds;
            }
        }
        QCOMPARE(rebuilds, 0);
    }

    void steadyTransmitFramesLeaveTheOverlayClean()
    {
        SpectrumWidget w;
        configure(w);
        w.setMoxOverlay(true);
        const QVector<float> txPixels(800, -60.0f);
        w.updateSpectrumFromTxPixels(-1, txPixels);
        int rebuilds = 0;
        for (int i = 0; i < kFrames; ++i) {
            if (dirties(w, [&] { w.updateSpectrumFromTxPixels(-1, txPixels); })) {
                ++rebuilds;
            }
        }
        QCOMPARE(rebuilds, 0);
    }

    void steadyRemoteHeldTracesLeaveTheOverlayClean()
    {
        SpectrumWidget w;
        configure(w);
        const SpectrumEndpointContext context = remoteContext();
        w.setRemoteSpectrumContext(context, 14200000.0, 192000.0, 4096);
        const QVector<float> trace(800, -120.0f);
        QVERIFY(w.presentRemoteTraceCaptured(trace, 14200000.0, 48000.0));
        int rebuilds = 0;
        for (int i = 0; i < kFrames; ++i) {
            if (dirties(w, [&] {
                    QVERIFY(w.presentRemoteTraceCaptured(trace, 14200000.0, 48000.0));
                })) {
                ++rebuilds;
            }
        }
        QCOMPARE(rebuilds, 0);
    }

    // ---- The IMD overlay ----

    // While the IMD display is shown the GPU path rebuilds the overlay on
    // every frame (its markers follow each accepted frame and its
    // annotations glide with the waterfall), so changed markers, and
    // markers that stop being drawn, need no mark of their own.
    void shownImdOverlayRebuildsEveryFrame()
    {
        SpectrumWidget w;
        configure(w);
        QVERIFY(!w.overlayRebuildsEveryFrameForTest());
        configureImd(w);
        QVERIFY(w.overlayRebuildsEveryFrameForTest());
        ImdSpectrumTestAccess::seedVisiblePixels(w, imdFrame());
        QVERIFY(w.imdMeasurementFrame().context.sourceEligible);
        // A frame with no receiver (receiverId -1) is not eligible: its
        // markers go, and the show condition still rebuilds the overlay.
        w.updateSpectrumLinear(-1, linearBins(4096), 1.0, 0.0);
        QVERIFY(!w.imdMeasurementFrame().context.sourceEligible);
        QVERIFY(w.overlayRebuildsEveryFrameForTest());
        w.setDisplayDuplex(false);
        QVERIFY(!w.overlayRebuildsEveryFrameForTest());
    }

    void hiddenImdOverlayDoesNotRebuildPerFrame()
    {
        SpectrumWidget w;
        configure(w);   // MOX, two-tone and IMD display all off
        QVERIFY(!w.overlayRebuildsEveryFrameForTest());
        ImdSpectrumTestAccess::seedVisiblePixels(w, imdFrame());
        int rebuilds = 0;
        for (int i = 0; i < kFrames; ++i) {
            QVector<float> frame = imdFrame();
            frame[340] = -22.0f - float(i);
            if (dirties(w, [&] { ImdSpectrumTestAccess::seedVisiblePixels(w, frame); })) {
                ++rebuilds;
            }
        }
        QCOMPARE(rebuilds, 0);
    }

    void imdToggleOffRebuilds()
    {
        SpectrumWidget w;
        configureImd(w);
        ImdSpectrumTestAccess::seedVisiblePixels(w, imdFrame());
        QVERIFY(dirties(w, [&] { w.setShowIMDMeasurements(false); }));
        QVERIFY(dirties(w, [&] { w.setShowIMDMeasurements(true); }));
        QVERIFY(dirties(w, [&] { w.setTestingIMD(false); }));
    }

    // ---- Bin width: the 1 Hz normalization and the Hz/bin readout ----

    void remoteFftSizeChangeUnderNormalizationRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        w.setSpectrumDetector(SpectrumDetector::Average);
        w.setDispNormalize(true);
        QVERIFY(w.normalizeActive());
        SpectrumEndpointContext context = remoteContext();
        w.setRemoteSpectrumContext(context, 14200000.0, 192000.0, 4096);
        QVERIFY(w.updateRemoteSpectrum(remoteFrame(context, 1)));
        ++context.codec.contextGeneration;
        QVERIFY(dirties(w, [&] {
            w.setRemoteSpectrumContext(context, 14200000.0, 192000.0, 8192);
        }));
        QCOMPARE(w.remoteGrantedFftSize(), 8192);
    }

    void localFftSizeChangeUnderNormalizationRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        w.setSpectrumDetector(SpectrumDetector::Average);
        w.setDispNormalize(true);
        w.updateSpectrumLinear(0, linearBins(4096), 1.0, 0.0);
        QVERIFY(dirties(w, [&] { w.updateSpectrumLinear(0, linearBins(8192), 1.0, 0.0); }));
        QVERIFY(!dirties(w, [&] { w.updateSpectrumLinear(0, linearBins(8192), 1.0, 0.0); }));
    }

    void localFftSizeChangeWithBinWidthReadoutRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        w.setShowBinWidth(true);
        w.updateSpectrumLinear(0, linearBins(4096), 1.0, 0.0);
        QVERIFY(dirties(w, [&] { w.updateSpectrumLinear(0, linearBins(8192), 1.0, 0.0); }));
    }

    void sampleRateChangeRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        QVERIFY(dirties(w, [&] { w.setSampleRate(384000.0); }));
    }

    void ddcCenterMoveInExtendedViewRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        w.setExtendedViewAllowed(true);
        w.setDisplayWindowPreservingHistory(14200000.0, 400000.0);
        w.setLocalWidebandAvailable(true);
        QVERIFY(w.extendedMode());
        QVERIFY(dirties(w, [&] { w.setDdcCenterFrequency(14210000.0); }));
    }

    // ---- Calibration: a local pan that becomes remote ----

    void becomingRemoteDropsTheLocalCalibrationFromTheScale()
    {
        SpectrumWidget w;
        configure(w);
        w.setDbmCalOffset(5.0f);
        // The local row period (50 ms) equals the remote one (1 frame a
        // line at 20 fps), so the time scale does not move either.
        w.setWfUpdatePeriodMs(50);
        SpectrumEndpointContext context = remoteContext();
        context.targetFps = 20;
        // Same window, rate and DDC center, no FFT grant: the one change is
        // that a remote pan adds no local calibration to its grid.
        QVERIFY(dirties(w, [&] {
            w.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        }));
        QCOMPARE(w.waterfallRowPeriodMsForTest(), 50.0);
    }

    // ---- The waterfall time scale ----

    // The clock is in the static overlay; only its ticker refreshes it, so
    // a timestamp loaded from settings has to start it.
    void loadedTimestampStartsItsTicker()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("DisplayWfTimestampPos"),
                                         QStringLiteral("1"));
        SpectrumWidget w;
        w.loadSettings();
        QVERIFY(w.wfTimestampPosition() == SpectrumWidget::TimestampPosition::Left);
        QVERIFY(w.wfTimestampTickerActiveForTest());
        AppSettings::instance().setValue(QStringLiteral("DisplayWfTimestampPos"),
                                         QStringLiteral("0"));
        w.loadSettings();
        QVERIFY(!w.wfTimestampTickerActiveForTest());
        AppSettings::instance().clear();
    }

    void waterfallUpdatePeriodChangeRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        w.setWfUpdatePeriodMs(50);
        QVERIFY(dirties(w, [&] { w.setWfUpdatePeriodMs(100); }));
    }

    void remoteRowPeriodChangeRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        SpectrumEndpointContext context = remoteContext();
        w.setRemoteSpectrumContext(context, 14200000.0, 192000.0, 4096);
        QVERIFY(w.updateRemoteSpectrum(remoteFrame(context, 1)));
        // The same picture at another frame rate: the time scale's seconds
        // follow the Core's row period.
        context.targetFps = 15;
        QVERIFY(dirties(w, [&] {
            w.setRemoteSpectrumContext(context, 14200000.0, 192000.0, 4096);
        }));
        QVERIFY(w.remoteContextKeptPicture());
        QVERIFY(!dirties(w, [&] {
            w.setRemoteSpectrumContext(context, 14200000.0, 192000.0, 4096);
        }));
    }

    void pausedWaterfallWithFrozenOffsetRebuildsPerRow()
    {
        SpectrumWidget w;
        configure(w);
        // Hidden, the widget has had no resize event, so no waterfall image.
        QResizeEvent resize(w.size(), QSize());
        QCoreApplication::sendEvent(&w, &resize);
        const QVector<float> row(800, -120.0f);
        w.pushWaterfallRowForTest(row);
        w.setWaterfallLiveForTest(false);
        // With fewer history rows than the waterfall is tall there is no
        // older row to hold, so each row moves what the paused time scale
        // labels.
        QVERIFY(dirties(w, [&] { w.pushWaterfallRowForTest(row); }));
    }

    void replacedHistoryGoesLiveAndRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        QResizeEvent resize(w.size(), QSize());
        QCoreApplication::sendEvent(&w, &resize);
        w.pushWaterfallRowForTest(QVector<float>(800, -120.0f));
        w.setWaterfallLiveForTest(false);
        // A new history length replaces the ring after the debounce; the
        // paused time scale and its LIVE button go back to live.
        w.setWaterfallHistoryMs(120000);
        QVERIFY(w.waterfallHistoryResizePendingForTest());
        w.clearOverlayStaticDirtyForTest();
        QTRY_VERIFY(!w.waterfallHistoryResizePendingForTest());
        QVERIFY(w.overlayStaticDirtyForTest());
    }

    // ---- Cursor readout, slices and the dBm strip ----

    // Over the waterfall time scale the hover returns early (its cursor),
    // as over a spot, a notch or the LIVE button; the readout still moves.
    void pointerMoveOverTheTimeScaleMovesTheCursorReadout()
    {
        SpectrumWidget w;
        configure(w);
        QResizeEvent resize(w.size(), QSize());
        QCoreApplication::sendEvent(&w, &resize);
        w.setCursorFreqVisible(true);
        const QRect wf = w.waterfallRectForTest();
        const QPointF first(w.width() - 4, wf.center().y());
        const QPointF second(w.width() - 4, wf.center().y() + 20);
        QMouseEvent move(QEvent::MouseMove, first, first,
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QVERIFY(dirties(w, [&] { QCoreApplication::sendEvent(&w, &move); }));
        QCOMPARE(w.cursor().shape(), Qt::SizeVerCursor);
        QMouseEvent next(QEvent::MouseMove, second, second,
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QVERIFY(dirties(w, [&] { QCoreApplication::sendEvent(&w, &next); }));
    }

    void pointerLeavingRemovesTheCursorReadout()
    {
        SpectrumWidget w;
        configure(w);
        w.setCursorFreqVisible(true);
        QMouseEvent move(QEvent::MouseMove, QPointF(200, 100), QPointF(200, 100),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &move);
        QEvent leave(QEvent::Leave);
        QVERIFY(dirties(w, [&] { QCoreApplication::sendEvent(&w, &leave); }));
    }

    void frontSliceChangeRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        w.addVfoWidget(0);
        w.addVfoWidget(1);
        w.setFrontSliceIndex(0);
        QVERIFY(dirties(w, [&] { w.setFrontSliceIndex(1); }));
        QVERIFY(!dirties(w, [&] { w.setFrontSliceIndex(1); }));
    }

    void addingASliceFlagRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        QVERIFY(dirties(w, [&] { w.addVfoWidget(0); }));
    }

    // The marker's triangle hangs off the flag's bottom edge: a flag tab
    // opened or closed resizes the flag and moves the triangle.
    void flagTabOpenedRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        VfoWidget* flag = w.addVfoWidget(0);
        flag->setFrequency(14200000.0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.updateVfoPositions();
        QVERIFY(flag->isVisible());
        auto* tab = flag->findChild<QPushButton*>(QStringLiteral("audioTabButton"));
        QVERIFY(tab);
        const int closedHeight = flag->height();
        QVERIFY(dirties(w, [&] { tab->click(); }));
        QVERIFY(flag->height() != closedHeight);
        QVERIFY(dirties(w, [&] { tab->click(); }));
        QCOMPARE(flag->height(), closedHeight);
    }

    void removingASliceFlagRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        w.addVfoWidget(0);
        w.addVfoWidget(1);
        QVERIFY(dirties(w, [&] { w.removeVfoWidget(1); }));
    }

    void dbmStripArrowClickRebuilds()
    {
        SpectrumWidget w;
        configure(w);
        w.setDbmScaleVisible(true);
        w.setConnectionState(ConnectionState::Connected);
        const QPoint upArrow(w.width() - 30, 5);
        QVERIFY(dirties(w, [&] {
            QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, upArrow);
        }));
    }
};

QTEST_MAIN(TestSpectrumOverlayDirty)
#include "tst_spectrum_overlay_dirty.moc"
