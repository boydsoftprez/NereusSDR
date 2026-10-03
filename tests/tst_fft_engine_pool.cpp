// =================================================================
// tests/tst_fft_engine_pool.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R1 Task 6: pins the FftEnginePool contract (src/core/spectrum/
// FftEnginePool.h) extracted from MainWindow::createFftEngineForStream /
// m_fftEngines / m_fftThread: one engine per stream, reused on lookup,
// dropped on removeStream, and a single FftPoolConfig that reaches every
// engine that exists right now AND every engine created afterwards.
//
// This filename previously belonged to a test of FFTRouter's pan/receiver
// mapping (Phase 3F Sub-Epic I Tasks 8-9); that content is unchanged and
// now lives in tst_fft_router_topology.cpp, since FftEnginePool did not
// exist as a class until this task.
// =================================================================
#include <QtTest>
#include "core/spectrum/FftEnginePool.h"
#include "core/FFTEngine.h"

#include <QLoggingCategory>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QThread>

#include <algorithm>

using namespace NereusSDR;

class TstFftEnginePool : public QObject {
    Q_OBJECT
private slots:
    void createsOneEnginePerStreamAndReuses()
    {
        FftEnginePool pool;
        FFTEngine* a = pool.engineForStream(0);
        FFTEngine* b = pool.engineForStream(1);
        QVERIFY(a != nullptr);
        QVERIFY(b != nullptr);
        QVERIFY(a != b);
        QCOMPARE(pool.engineForStream(0), a);   // reuse, not recreate
        QCOMPARE(pool.engineCount(), 2);
    }

    void removeStreamDropsTheEngine()
    {
        FftEnginePool pool;
        pool.engineForStream(0);
        pool.engineForStream(1);
        pool.removeStream(0);
        QCOMPARE(pool.engineCount(), 1);
        QCOMPARE(pool.streams(), QList<int>{1});
    }

    void twoTiersOnOneStreamKeepIndependentFftSizes()
    {
        FftEnginePool pool;
        FftPoolConfig wideConfig;
        wideConfig.fftSize = 4096;
        FftPoolConfig fineConfig = wideConfig;
        fineConfig.fftSize = 16384;

        const FftSourceKey wide{0, FftTier::Wide};
        const FftSourceKey fine{0, FftTier::Fine};
        FFTEngine* wideEngine = pool.engineForSource(wide, wideConfig);
        FFTEngine* fineEngine = pool.engineForSource(fine, fineConfig);

        QVERIFY(wideEngine != nullptr);
        QVERIFY(fineEngine != nullptr);
        QVERIFY(wideEngine != fineEngine);
        QCOMPARE(wideEngine->fftSize(), 4096);
        QCOMPARE(fineEngine->fftSize(), 16384);
        const QList<FftSourceKey> expectedSources{wide, fine};
        QCOMPARE(pool.sources(), expectedSources);
        QCOMPARE(pool.streams(), QList<int>{0});

        pool.removeSource(fine);
        QCOMPARE(pool.engineCount(), 1);
        QCOMPARE(pool.engineForStream(0), wideEngine);
        QCOMPARE(wideEngine->fftSize(), 4096);

        pool.removeStream(0);
        QCOMPARE(pool.engineCount(), 0);
    }

    void disabledForwardingStillLetsSourceEngineProduce()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#else
        FftEnginePool pool(nullptr, false);
        QVERIFY(!pool.frameForwardingEnabled());

        FftPoolConfig config;
        config.fftSize = 1024;
        config.fps = 60;
        config.windowType = static_cast<int>(WindowFunction::Hann);
        FFTEngine* engine = pool.engineForSource({0, FftTier::Wide}, config);
        QVERIFY(engine != nullptr);

        QSignalSpy directFrames(engine, &FFTEngine::fftReadyLinear);
        QSignalSpy forwardedFrames(&pool, &FftEnginePool::fftFrameReady);
        const QVector<float> iq(2052, 0.0f); // 1026 complex samples
        QMetaObject::invokeMethod(engine, [engine, iq]() {
            engine->feedIQ(iq);
        }, Qt::QueuedConnection);

        QTRY_VERIFY(directFrames.count() >= 1);
        QCOMPARE(forwardedFrames.count(), 0);
#endif
    }

    // Config must reach engines created BEFORE and AFTER the call, otherwise
    // stream 0 and stream 4 silently run different FFT sizes.
    void configAppliesToExistingAndFutureEngines()
    {
        FftEnginePool pool;
        FFTEngine* early = pool.engineForStream(0);
        FftPoolConfig cfg;
        cfg.fftSize = 16384;
        cfg.fps     = 15;
        pool.setConfig(cfg);
        FFTEngine* late = pool.engineForStream(1);
        QCOMPARE(early->fftSize(), 16384);
        QCOMPARE(late->fftSize(),  16384);
        QCOMPARE(early->outputFps(), 15);
        QCOMPARE(late->outputFps(),  15);
    }

    // Fix round 1 finding 1 (coordinator spec review): pins that
    // setConfig() is not "sticky from the first call" -- called twice,
    // with a stream created between each call, the SECOND stream picks up
    // the SECOND config, not the first one ever set, and (per setConfig()'s
    // own documented contract) the FIRST stream follows the second call
    // too. Written to model MainWindow's refresh-before-create pattern
    // (re-read AppSettings, push to the pool, immediately before building
    // a stream that does not exist yet) as it stood at the end of fix
    // round 1.
    //
    // Superseded as a MainWindow model by fix round 2:
    // MainWindow::refreshFftPoolConfig() now calls
    // setConfigForNewStreams(), not setConfig(), specifically BECAUSE
    // setConfig()'s retroactive-touch-existing-engines behaviour pinned
    // here is wrong for that call site (see
    // secondSetConfigForNewStreamsCallLeavesExistingEnginesAlone below,
    // and both methods' doc comments in FftEnginePool.h). This test still
    // stands on its own as a pin of setConfig()'s own contract -- unchanged
    // and still brief-mandated -- just no longer as a stand-in for what
    // MainWindow itself calls.
    void secondSetConfigCallReachesTheStreamCreatedAfterIt()
    {
        FftEnginePool pool;

        FftPoolConfig first;
        first.fftSize = 8192;
        pool.setConfig(first);
        FFTEngine* streamA = pool.engineForStream(0);
        QCOMPARE(streamA->fftSize(), 8192);

        // A live Setup -> Display change happens here; MainWindow's fix
        // re-reads AppSettings and calls setConfig() again before
        // building the next new stream.
        FftPoolConfig second;
        second.fftSize = 32768;
        pool.setConfig(second);
        FFTEngine* streamB = pool.engineForStream(1);

        QCOMPARE(streamB->fftSize(), 32768);
        // setConfig() reaches existing engines too (its documented
        // contract), so streamA follows the second call as well.
        QCOMPARE(streamA->fftSize(), 32768);
    }

    // Fix round 2 (coordinator spec review): my round-1 fix made
    // ensureStreamWired() call setConfig() every time a previously-unseen
    // stream appears, and setConfig() retroactively touches every
    // existing engine -- brief-mandated, unchanged, and correct for a
    // caller with no other source of per-engine truth. It is the WRONG
    // call for MainWindow specifically, because the auto-zoom lambda
    // calls engine->setFftSize() directly on one stream's engine, a
    // transient override that is deliberately never written back to
    // AppSettings. Concrete, reachable failure: RX1 zoomed to FFT 32768,
    // user enables RX2 -> ensureStreamWired(1) sees a new stream ->
    // refreshFftPoolConfig() re-reads AppSettings (still "4096") and
    // calls setConfig() -> RX1's engine gets setFftSize(4096) too,
    // silently undoing the zoom with a visible replan pause, on a pan the
    // user did not touch.
    //
    // setConfigForNewStreams() is the fix: it stores cfg for the NEXT
    // engineForStream() call only, and touches nothing that already
    // exists. This pins that directly: streamA's live-only override
    // survives a setConfigForNewStreams() call that creates streamB.
    void setConfigForNewStreamsLeavesExistingEnginesAlone()
    {
        FftEnginePool pool;
        FFTEngine* streamA = pool.engineForStream(0);

        // Simulate the auto-zoom lambda's direct, deliberately-never-
        // persisted override on stream 0 (MainWindow.cpp calls
        // engine->setFftSize() straight on the engine, no AppSettings
        // write).
        streamA->setFftSize(32768);

        FftPoolConfig cfg;
        cfg.fftSize = 4096;
        pool.setConfigForNewStreams(cfg);

        FFTEngine* streamB = pool.engineForStream(1);

        QCOMPARE(streamB->fftSize(), 4096);   // new stream: current baseline
        QCOMPARE(streamA->fftSize(), 32768);  // existing stream: untouched
    }

    // Contrast pinned in the same file as the test above: the SAME
    // sequence through setConfig() instead DOES retroactively overwrite
    // streamA's live zoom -- this is setConfig()'s correct, unchanged,
    // brief-mandated behaviour (see configAppliesToExistingAndFutureEngines
    // and secondSetConfigCallReachesTheStreamCreatedAfterIt above), not a
    // bug. Pinning both here, with opposite outcomes on stream 0, makes
    // the difference between the two methods explicit rather than
    // something a future reader has to take on faith from a comment.
    void setConfigInContrastDoesOverwriteExistingEngines()
    {
        FftEnginePool pool;
        FFTEngine* streamA = pool.engineForStream(0);
        streamA->setFftSize(32768);

        FftPoolConfig cfg;
        cfg.fftSize = 4096;
        pool.setConfig(cfg);

        FFTEngine* streamB = pool.engineForStream(1);

        QCOMPARE(streamB->fftSize(), 4096);
        QCOMPARE(streamA->fftSize(), 4096);   // retroactively overwritten
    }

    // R-R3-08/R-R3-40: while the Core is busy, a transform advances a whole
    // frame period, so transforms per second follow the frame rate and a
    // lower rate saves FFT work. Off (the default, and always in the desktop
    // app) nothing changes: a 4096-point FFT at 192 kHz runs a transform
    // every 4096 samples whatever the frame rate. Counted over one second
    // of samples.
    void transformsFollowTheFrameRateOnlyWhenAsked_data()
    {
        QTest::addColumn<int>("fps");
        QTest::addColumn<bool>("follow");
        QTest::addColumn<int>("transforms");
        // 1 + floor((192000 - 4096) / 4096) = 46 transforms, clamped
        // advance, as before.
        QTest::newRow("10 fps, default") << 10 << false << 46;
        QTest::newRow("30 fps, default") << 30 << false << 46;
        // Advance 192000 / fps: 1 + floor((192000 - 4096) / 19200) = 10,
        // 1 + floor((192000 - 4096) / 6400) = 30.
        QTest::newRow("10 fps, Core busy") << 10 << true << 10;
        QTest::newRow("30 fps, Core busy") << 30 << true << 30;
        // 192000 / 60 = 3200 is under the FFT size: overlapping windows,
        // the same either way.
        QTest::newRow("60 fps, default") << 60 << false << 59;
        QTest::newRow("60 fps, Core busy") << 60 << true << 59;
    }

    void transformsFollowTheFrameRateOnlyWhenAsked()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#else
        QFETCH(int, fps);
        QFETCH(bool, follow);
        QFETCH(int, transforms);
        // The engine's replan and memory-lock notes are not this test's.
        QLoggingCategory::setFilterRules(
            QStringLiteral("nereus.dsp.info=false\nnereus.memlock.info=false"));
        const auto restoreRules = qScopeGuard([] { QLoggingCategory::setFilterRules({}); });
        FFTEngine engine(0);
        QVERIFY(!engine.transformsFollowFrameRate()); // Off unless asked.
        engine.setSampleRate(192000.0);
        engine.setFftSize(4096);
        engine.setOutputFps(fps);
        engine.setTransformsFollowFrameRate(follow);
        QSignalSpy frames(&engine, &FFTEngine::fftReady);
        // One second of samples in 64-pair batches. Each batch holds at
        // most one transform; after one, wait out the engine's 5 ms
        // back-to-back guard so no transform is deferred by it.
        const int batchPairs = 64;
        const QVector<float> batch(batchPairs * 2, 0.25f);
        for (int fed = 0; fed < 192000; fed += batchPairs) {
            const qsizetype before = frames.count();
            engine.feedIQ(batch);
            if (frames.count() != before) {
                QThread::msleep(6);
            }
        }
        QCOMPARE(frames.count(), transforms);
#endif
    }

    // R-R3-08/40: the sample that completes a window's buffer triggers
    // the transform and is the first sample after that window. When
    // transforms follow the frame rate it belongs to the gap, never to
    // the next window.
    void noSampleBeforeTheGapEntersTheNextWindow()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#else
        QLoggingCategory::setFilterRules(
            QStringLiteral("nereus.dsp.info=false\nnereus.memlock.info=false"));
        const auto restoreRules = qScopeGuard([] { QLoggingCategory::setFilterRules({}); });
        constexpr int kFftSize = 4096;
        FFTEngine engine(0);
        engine.setSampleRate(192000.0);
        engine.setFftSize(kFftSize);
        engine.setOutputFps(10); // advance 19200: a gap of 15104 samples
        engine.setWindowFunction(WindowFunction::Rectangular);
        engine.setTransformsFollowFrameRate(true);
        QList<QVector<float>> frames;
        connect(&engine, &FFTEngine::fftReadyLinear, this,
                [&frames](int, const QVector<float>& bins, double, double) {
                    frames.append(bins);
                });
        // One silent window, then the sample that triggers its transform:
        // a loud one. Everything after it is silent.
        QVector<float> first((kFftSize + 1) * 2, 0.0f);
        first[kFftSize * 2] = 1.0f;
        first[kFftSize * 2 + 1] = 1.0f;
        engine.feedIQ(first);
        QCOMPARE(frames.size(), 1);
        // Wait out the engine's 5 ms back-to-back guard, then feed the gap
        // and one more window, plus the sample that triggers it.
        QThread::msleep(6);
        const QVector<float> silent((192000 / 10) * 2, 0.0f);
        engine.feedIQ(silent);
        QCOMPARE(frames.size(), 2);
        // The second window is all silence: the loud sample was not in it.
        const QVector<float>& second = frames.at(1);
        QCOMPARE(second.size(), kFftSize);
        const float loudest = *std::max_element(second.cbegin(), second.cend());
        QCOMPARE(loudest, 0.0f);
#endif
    }

    // Coordinator decision beyond the brief's baseline three: the
    // destructor must quit() and wait() on every worker thread BEFORE
    // deleting the engines parked on it, or a still-running thread can be
    // mid-feedIQ() on an engine the main thread is simultaneously freeing
    // -- a use-after-free racing the pool's own teardown. Queue real work
    // onto the engine's thread right before destroying the pool, so the
    // destructor races an actual in-flight call rather than an idle one.
    // Parity Task 17 follow-up (R-R3-01): Rendering > Decimation reaches
    // every engine the pool has and every one it makes afterwards; before
    // any call the pool leaves each engine's own decimation alone (the
    // Core's spectrum source sets it per engine).
    void decimationAppliesToExistingAndFutureEngines()
    {
        FftEnginePool pool;
        FFTEngine* first = pool.engineForStream(0);
        QVERIFY(first);
        first->setDecimation(3);
        pool.engineForSource({0, FftTier::Fine}, pool.config());
        QCOMPARE(pool.engineForStream(1)->decimation(), 1);
        QCOMPARE(first->decimation(), 3);

        pool.setDecimation(5);
        QCOMPARE(pool.decimation(), 5);
        for (const FftSourceKey& key : pool.sources()) {
            QCOMPARE(pool.engineForSource(key, pool.config())->decimation(), 5);
        }
        QCOMPARE(pool.engineForStream(2)->decimation(), 5);

        // Outside 1 to 16 is ignored, as FFTEngine::setDecimation ignores it.
        pool.setDecimation(0);
        pool.setDecimation(17);
        QCOMPARE(pool.decimation(), 5);
        QCOMPARE(first->decimation(), 5);
    }

    void destroysSafelyWithPendingWorkQueued()
    {
        auto* pool = new FftEnginePool;
        FFTEngine* engine = pool->engineForStream(0);
        QVERIFY(engine != nullptr);

        const QVector<float> iq(8192, 0.0f);
        QMetaObject::invokeMethod(engine, [engine, iq]() {
            engine->feedIQ(iq);
        }, Qt::QueuedConnection);

        delete pool;    // must not crash or hang
        QVERIFY(true);  // reaching this line is the actual assertion
    }
};

QTEST_MAIN(TstFftEnginePool)
#include "tst_fft_engine_pool.moc"
