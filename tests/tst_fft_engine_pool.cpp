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

    // Coordinator decision beyond the brief's baseline three: the
    // destructor must quit() and wait() on every worker thread BEFORE
    // deleting the engines parked on it, or a still-running thread can be
    // mid-feedIQ() on an engine the main thread is simultaneously freeing
    // -- a use-after-free racing the pool's own teardown. Queue real work
    // onto the engine's thread right before destroying the pool, so the
    // destructor races an actual in-flight call rather than an idle one.
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
