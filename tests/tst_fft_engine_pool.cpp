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

    // Fix round 1 finding 1 (coordinator spec review): MainWindow's
    // ensureStreamWired() now re-reads AppSettings and calls setConfig()
    // again immediately before building any stream that does not exist
    // yet ("refresh-before-create"), because setConfig() has no other call
    // site and is otherwise a one-time snapshot taken at buildUI() time --
    // a stream built later (e.g. RX2 activated after the user changes
    // Setup -> Display) would silently run on launch-time settings
    // instead of the current ones.
    //
    // This pins the pool-side half of that fix: unlike
    // configAppliesToExistingAndFutureEngines above, which calls
    // setConfig() exactly ONCE, this calls it TWICE with a stream created
    // between each call, and checks that the SECOND stream picks up the
    // SECOND config, not the first one ever set (i.e. setConfig() is not
    // "sticky from the first call" -- each call's values are what the
    // next newly-created engine gets). FftEnginePool itself was never
    // broken -- the regression was MainWindow only calling setConfig()
    // once -- so this test does not exercise MainWindow's wiring and
    // would pass against the pool code from before this fix round too;
    // it documents the exact contract MainWindow::ensureStreamWired's
    // refresh-before-create call now depends on.
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
