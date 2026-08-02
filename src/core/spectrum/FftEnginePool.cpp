// =================================================================
// src/core/spectrum/FftEnginePool.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See FftEnginePool.h for what this
// replaces (MainWindow::createFftEngineForStream + m_fftEngines +
// m_fftThread) and why.
//
// Config application (applyConfigTo) calls the engine's setters directly,
// never marshalled through QMetaObject::invokeMethod onto the engine's
// own thread. That is deliberate, not an oversight: every setter touched
// here (setOutputFps / setFftSizeBaseline / setFftSize /
// setWindowFunction / setHzPerBinTarget) is a plain std::atomic store
// inside FFTEngine (see FFTEngine.cpp), so a direct cross-thread call is
// safe, and it is also the exact pattern MainWindow's own auto-zoom
// lambda already used on primaryFftEngine()->setFftSize()
// (MainWindow.cpp:3862, pre-extraction) -- this is precedent, not a new
// risk. It is also required for setConfig() to be observable
// synchronously: tests/tst_fft_engine_pool.cpp's
// configAppliesToExistingAndFutureEngines reads an existing engine's
// fftSize()/outputFps() immediately after setConfig() returns, with no
// event-loop wait, which only holds if the store already happened.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 4 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/spectrum/FftEnginePool.h"

#include "core/audio/RealtimeAudioPriority.h"

#include <QThread>
#include <QtGlobal>

#include <utility>

namespace NereusSDR {

FftEnginePool::FftEnginePool(QObject* parent)
    : QObject(parent)
{
}

FftEnginePool::~FftEnginePool()
{
    // Coordinator decision (Task 6 brief): every worker thread must be
    // fully stopped BEFORE any engine parked on it is deleted. A thread
    // that is still running could be mid-feedIQ() (or any other queued
    // slot call) on one of these engines at the exact moment this
    // destructor runs; deleting the engine out from under that call is a
    // use-after-free racing the pool's own teardown.
    //
    // wait() with no timeout blocks until the thread's run() has actually
    // returned (FFTEngine's slots are bounded, non-blocking compute --
    // no external I/O to get stuck in -- so quit() always drains
    // promptly), so by the time this loop completes, nothing can execute
    // on any pooled engine again and a direct delete is safe. This is
    // deliberately NOT deleteLater() connected to QThread::finished():
    // that idiom is correct for a thread that keeps running afterwards
    // (removeStream() below uses it for exactly that reason), but here
    // the thread is going away too, and a plain delete after a completed
    // wait() removes any dependency on event-loop delivery timing during
    // shutdown.
    for (QThread* thread : std::as_const(m_threadsByBucket)) {
        if (thread && thread->isRunning()) {
            thread->quit();
            thread->wait();
        }
    }
    qDeleteAll(m_engines);
    m_engines.clear();
    qDeleteAll(m_threadsByBucket);
    m_threadsByBucket.clear();
}

void FftEnginePool::applyConfigTo(FFTEngine* engine) const
{
    engine->setOutputFps(m_config.fps);
    engine->setFftSizeBaseline(m_config.fftSize);
    engine->setFftSize(m_config.fftSize);
    engine->setWindowFunction(static_cast<WindowFunction>(m_config.windowType));
    engine->setHzPerBinTarget(m_config.hzPerBinTarget);
}

void FftEnginePool::setConfig(const FftPoolConfig& cfg)
{
    m_config = cfg;
    for (FFTEngine* engine : std::as_const(m_engines)) {
        if (engine) {
            applyConfigTo(engine);
        }
    }
}

FFTEngine* FftEnginePool::engineForStream(int streamIndex)
{
    if (streamIndex < 0) { return nullptr; }
    if (FFTEngine* existing = m_engines.value(streamIndex, nullptr)) {
        return existing;
    }
    return createEngine(streamIndex);
}

FFTEngine* FftEnginePool::createEngine(int streamIndex)
{
    // No QObject parent: this pool owns the engine's lifetime directly
    // (removeStream()'s deleteLater(), or the destructor's quit-wait-then-
    // delete above), matching the ownership model
    // createFftEngineForStream used before this extraction.
    auto* engine = new FFTEngine(streamIndex);

    // Configured from the pool's current config, on the calling thread --
    // this runs before moveToThread() below, so it is a same-thread call
    // exactly like the old createFftEngineForStream's construction-time
    // setters were (all of them ran before that function's own
    // moveToThread call).
    applyConfigTo(engine);

    const int threadCount = qMax(1, m_config.threadCount);
    const int bucket = streamIndex % threadCount;
    QThread* thread = m_threadsByBucket.value(bucket, nullptr);
    const bool threadIsNew = (thread == nullptr);
    if (threadIsNew) {
        thread = new QThread(this);
        thread->setObjectName(QStringLiteral("SpectrumThread%1").arg(bucket));
        m_threadsByBucket.insert(bucket, thread);
    }

    engine->moveToThread(thread);

    if (threadIsNew) {
        // 2026-05-25/26 KG4VCF bench fix, carried over from MainWindow
        // (MainWindow.cpp:3105-3119 pre-extraction): elevate the FFT
        // thread's scheduling priority as soon as it starts, so compile
        // workers and other DEFAULT-QoS work consistently lose the
        // time-slice race against live spectrum production. The context
        // object must be `engine`, not `thread` -- a QThread instance's
        // own affinity is the thread that CREATED it, not the thread it
        // manages, so connecting with `thread` as context would deliver
        // this lambda on the wrong thread. `engine` was just moved onto
        // `thread` above, so Auto-connection resolves to Direct: the
        // lambda runs ON the worker thread once started() fires. One
        // connection per thread is enough for every engine parked on it
        // afterwards, same as the pre-extraction code's single
        // connection using stream 0's engine as context for the whole
        // (then-implicitly-one) shared thread.
        connect(thread, &QThread::started, engine, []() {
            elevateLatencyCriticalThreadPriority();
        });
        thread->start();
    }

    // Re-emit with the stream index attached, matching the shape
    // MainWindow::dispatchFftFrameToPans expects. fftReadyLinear and
    // fftFrameReady share the same parameter list (both carry the
    // engine's receiverId first), so this is a plain signal-to-signal
    // forward, no lambda needed.
    connect(engine, &FFTEngine::fftReadyLinear, this, &FftEnginePool::fftFrameReady);

    m_engines.insert(streamIndex, engine);
    return engine;
}

void FftEnginePool::removeStream(int streamIndex)
{
    FFTEngine* engine = m_engines.take(streamIndex);
    if (!engine) { return; }
    // Disconnect first so a frame already in flight toward fftFrameReady
    // does not fire for a stream that is being removed right now (mirrors
    // MainWindow::disconnectPanadapter's "disconnect first, then remove"
    // pattern). deleteLater() is thread-safe to call from any thread --
    // it posts to the engine's OWN thread's queue regardless of which
    // thread calls it -- and that thread keeps running for whatever
    // other streams still share it (or, in the threadCount == 1 default,
    // for every other stream), so the deferred delete drains normally.
    engine->disconnect();
    engine->deleteLater();
}

QList<int> FftEnginePool::streams() const
{
    return m_engines.keys();
}

int FftEnginePool::engineCount() const
{
    return m_engines.size();
}

} // namespace NereusSDR
