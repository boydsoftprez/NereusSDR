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
