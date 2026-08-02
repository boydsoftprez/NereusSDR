#pragma once
// =================================================================
// src/core/spectrum/FftEnginePool.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Extracted from
// MainWindow::createFftEngineForStream (MainWindow.cpp:1430-1530) and the
// m_fftEngines / m_fftThread members it filled in (MainWindow.h:597-626).
// That function created one FFTEngine per DDC stream, read four global
// display AppSettings keys (DisplaySpectrumFps, DisplayFftSize,
// DisplayFftWindow, DisplayHzPerBinTarget), and parked every engine on one
// shared worker thread -- core work sitting inside a QWidget, so a
// headless daemon had no way to produce spectrum at all.
//
// threadCount is exposed as policy rather than hard-coded, per design
// section 4.5a's measured Pi 4B floor: four threads delivered only 1.35x
// the aggregate FFT throughput of one (572 fps against 425 at FFT 65536),
// because a 65536-point complex transform moves 512 kB each way against a
// 1 MB shared L2 -- memory-bandwidth bound, not core bound. Default stays
// 1; see FftPoolConfig::threadCount.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 4 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/FFTEngine.h"

#include <QMap>
#include <QObject>
#include <QVector>

class QThread;

namespace NereusSDR {

/// The four global display knobs MainWindow::createFftEngineForStream used
/// to read from AppSettings, plus the thread-count policy the old code
/// never exposed (it always parked every engine on one shared thread).
struct FftPoolConfig {
    int    fps            {30};
    int    fftSize        {4096};
    // Trap for a Task 9/10 daemon-config author (coordinator spec review,
    // fix round 1, finding 2): this struct literal defaults to 4 (Hamming),
    // but every production caller configures 1 (WindowFunction::
    // BlackmanHarris4) to match FFTEngine's own constructor default -- see
    // MainWindow::refreshFftPoolConfig()'s DisplayFftWindow fallback. A
    // caller that builds an FftPoolConfig and never calls setConfig() (as
    // two of this class's own brief-specified unit tests do) gets Hamming,
    // not the window the app actually ships with.
    int    windowType     {4};
    double hzPerBinTarget {0.0};
    // 1 = today's shared thread. Design section 4.5a measured only a
    // 1.35x aggregate-throughput gain at 4 threads on the Pi 4B floor
    // hardware -- the workload is memory-bandwidth bound, not core
    // bound -- so this must not default above 1.
    int    threadCount    {1};
};

/// Owns one FFTEngine per DDC stream, parked on threadCount worker
/// threads (round-robin by stream index, bucket = streamIndex %
/// threadCount), configured from a single FftPoolConfig that reaches
/// every engine that exists right now AND every engine created later.
///
/// Extracted from MainWindow::createFftEngineForStream: MainWindow now
/// reads the four display AppSettings keys once, fills an FftPoolConfig,
/// and calls setConfig(); per-stream engine creation, reuse, removal, and
/// thread parking all live here instead of in a QWidget, so the headless
/// daemon can produce spectrum with no widget toolkit in the process.
///
/// Not thread-safe for concurrent callers: every public method is meant
/// to be driven from one thread (the thread that owns the pool), exactly
/// as m_fftEngines / m_fftThread were plain MainWindow members accessed
/// only from the main thread before this extraction. The engines this
/// class vends are the cross-thread-safe part (moveToThread'd onto the
/// worker threads; their own setters are std::atomic stores).
class FftEnginePool : public QObject {
    Q_OBJECT
public:
    explicit FftEnginePool(QObject* parent = nullptr);
    ~FftEnginePool() override;

    /// Applies to every engine that exists right now AND every engine
    /// created after this call. Callers must not assume the config only
    /// reaches engines built later -- otherwise stream 0 (configured
    /// before setConfig) and stream 4 (configured after) would silently
    /// run different FFT sizes. Brief-mandated contract; pinned by
    /// tests/tst_fft_engine_pool.cpp's configAppliesToExistingAndFutureEngines.
    /// Do not change this method's behaviour to stop touching existing
    /// engines -- that is setConfigForNewStreams()'s job, not this one's.
    ///
    /// Two config-setters exist on this class on purpose (fix round 2,
    /// coordinator spec review). This one is for a config source that is
    /// always authoritative for every engine, existing or not -- true for
    /// a caller with nothing else that can diverge an engine from it. It
    /// is the wrong one for MainWindow: MainWindow's auto-zoom lambda
    /// calls engine->setFftSize() directly on a single stream's engine, a
    /// deliberately transient, never-persisted-to-AppSettings override
    /// (that is the entire point of auto-zoom). If MainWindow's
    /// AppSettings-driven refresh used THIS method, creating any new
    /// stream would silently snap every other, already-zoomed stream back
    /// to the persisted baseline FFT size -- see setConfigForNewStreams()
    /// below, which is what MainWindow actually calls.
    void setConfig(const FftPoolConfig& cfg);

    /// Stores cfg for future engineForStream() calls ONLY -- does not
    /// touch any engine that already exists, unlike setConfig() above.
    ///
    /// This is the method MainWindow::refreshFftPoolConfig() actually
    /// calls (fix round 2, coordinator spec review). MainWindow re-reads
    /// AppSettings and calls this immediately before building a stream
    /// that does not exist yet, so that new stream picks up the current
    /// persisted baseline -- but AppSettings is not the only thing that
    /// can set an existing engine's fftSize: the auto-zoom lambda
    /// (MainWindow.cpp, wired to SpectrumWidget::bandwidthChangeRequested)
    /// calls engine->setFftSize() directly and deliberately never writes
    /// AppSettings, because the zoom override is transient by design. Had
    /// MainWindow instead called setConfig() from ensureStreamWired(), the
    /// AppSettings-sourced baseline would retroactively overwrite that
    /// live zoom on every OTHER stream's engine the moment any new stream
    /// appeared -- a real, reachable bug (RX1 zoomed to FFT 32768, user
    /// enables RX2, RX1's panadapter silently snaps back to the 4096
    /// baseline with a visible replan pause). Do not merge this back into
    /// setConfig(): the two methods differ in this one respect
    /// deliberately, not by oversight.
    void setConfigForNewStreams(const FftPoolConfig& cfg);

    const FftPoolConfig& config() const { return m_config; }

    /// Returns the engine for streamIndex, creating and configuring one
    /// from the current config on first use. A negative streamIndex
    /// returns nullptr and creates nothing (mirrors the old
    /// createFftEngineForStream guard).
    FFTEngine* engineForStream(int streamIndex);

    /// Drops and deletes the engine for streamIndex, if one exists. Safe
    /// to call for a stream with no engine (no-op). The engine's own
    /// worker thread keeps running for whatever other streams still use
    /// it; only this one engine is torn down.
    void removeStream(int streamIndex);

    /// Stream indices with a live engine, ascending (QMap keeps its keys
    /// sorted).
    QList<int> streams() const;

    /// Number of live engines.
    int engineCount() const;

signals:
    /// Re-emission of every pooled engine's fftReadyLinear, with the
    /// emitting engine's own stream index as the first argument. That
    /// index is always the engine's receiverId -- every engine is
    /// constructed with receiverId == streamIndex -- so this is a plain
    /// signal-to-signal forward, not a per-engine lambda.
    void fftFrameReady(int streamIndex, const QVector<float>& binsLinear,
                       double windowEnb, double dbmOffset);

private:
    FFTEngine* createEngine(int streamIndex);
    void applyConfigTo(FFTEngine* engine) const;

    FftPoolConfig          m_config;
    QMap<int, FFTEngine*>  m_engines;       // keyed by stream index
    QMap<int, QThread*>    m_threadsByBucket;  // keyed by streamIndex % threadCount
};

} // namespace NereusSDR
