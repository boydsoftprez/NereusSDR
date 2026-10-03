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
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 17 follow-up: setDecimation
//                                    applies Rendering > Decimation to every
//                                    engine. AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/FFTEngine.h"

#include <QMap>
#include <QObject>
#include <QSet>
#include <QVector>

#include <optional>

class QThread;

namespace NereusSDR {

/// A stream can have the normal full-span engine and an independently sized
/// deep-resolution engine at the same time.  This is a production identity,
/// not a pan identity: several panes may consume the same source.
enum class FftTier : quint8 {
    Wide,
    Fine,
};

struct FftSourceKey {
    int streamIndex{-1};
    FftTier tier{FftTier::Wide};

    friend bool operator==(const FftSourceKey& lhs, const FftSourceKey& rhs)
    {
        return lhs.streamIndex == rhs.streamIndex && lhs.tier == rhs.tier;
    }

    friend bool operator<(const FftSourceKey& lhs, const FftSourceKey& rhs)
    {
        if (lhs.streamIndex != rhs.streamIndex) {
            return lhs.streamIndex < rhs.streamIndex;
        }
        return static_cast<quint8>(lhs.tier) < static_cast<quint8>(rhs.tier);
    }
};

using MediaSourceKey = FftSourceKey;

/// The four global display knobs MainWindow::createFftEngineForStream used
/// to read from AppSettings, plus the thread-count policy the old code
/// never exposed (it always parked every engine on one shared thread).
struct FftPoolConfig {
    int    fps            {30};
    int    fftSize        {4096};
    // 1 == WindowFunction::BlackmanHarris4, which is FFTEngine's own
    // constructor default and what MainWindow::refreshFftPoolConfig()
    // falls back to when DisplayFftWindow is unset. In other words, the
    // window the app actually ships with.
    //
    // This defaulted to 4 (Hamming) for one release, and the header
    // comment at the time described that as a trap for whoever wrote the
    // daemon config. It then caught one: the Task 13 Pi bench diagnostic
    // built an FftPoolConfig, never called a setter, and produced its
    // frames on a window the app never ships. Matching the production
    // value removes the trap rather than documenting it.
    int    windowType     {1};
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
/// Extracted from MainWindow::createFftEngineForStream: per-stream engine
/// creation, reuse, removal, and thread parking all live here instead of
/// in a QWidget, so the headless daemon can produce spectrum with no
/// widget toolkit in the process.
///
/// MainWindow fills an FftPoolConfig from the four display AppSettings
/// keys in refreshFftPoolConfig(), which ensureStreamWired() calls
/// immediately before building a stream that does not exist yet, and
/// which ends in setConfigForNewStreams() -- NOT setConfig(). See both
/// methods below for why the distinction is load-bearing rather than
/// incidental. (An earlier version of this paragraph said MainWindow
/// "calls setConfig()"; that was true only before the fix round 2 split,
/// and setConfig has had no production caller since.)
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
    /// Local GUI callers retain the historical full-frame relay by default.
    /// A daemon producer that owns its own bounded latest-frame handoff can
    /// disable it before creating engines, avoiding an unused queued copy of
    /// each full FFT frame onto the pool's owner thread.
    explicit FftEnginePool(QObject* parent = nullptr,
                           bool forwardFrameReady = true);
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

    /// Parity Task 17 follow-up (R-R3-01): Setup > Display > Rendering >
    /// Decimation applies to every pan, so to every engine in the pool now
    /// and to every engine created after this call (FFTEngine::setDecimation;
    /// 1 to 16, other values ignored). Until it is called the pool leaves
    /// each engine's decimation alone, so the Core's spectrum source, which
    /// sets decimation per engine, never meets a pool-wide value.
    void setDecimation(int factor);
    /// The value setDecimation() last took; 1 before any call.
    int decimation() const { return m_decimation.value_or(1); }

    /// Returns the engine for streamIndex, creating and configuring one
    /// from the current config on first use. A negative streamIndex
    /// returns nullptr and creates nothing (mirrors the old
    /// createFftEngineForStream guard).
    FFTEngine* engineForStream(int streamIndex);

    /// Returns a source-specific engine, creating it with `cfg` on first
    /// use. Wide and Fine keys for one stream deliberately have separate
    /// engines, accumulators, plans and resolution settings.  `cfg` applies
    /// only to this source; it cannot resize its sibling tier.
    ///
    /// The worker-thread count remains the pool's established policy from
    /// setConfig()/setConfigForNewStreams(); source-specific requests only
    /// select FFT behaviour, not a new thread topology.
    FFTEngine* engineForSource(const FftSourceKey& key, const FftPoolConfig& cfg);

    /// Drops and deletes the engine for streamIndex, if one exists. Safe
    /// to call for a stream with no engine (no-op). The engine's own
    /// worker thread keeps running for whatever other streams still use
    /// it; only this one engine is torn down.
    void removeStream(int streamIndex);

    /// Drops one source tier without affecting the other tier on the same
    /// stream.  Safe for an unknown key.
    void removeSource(const FftSourceKey& key);

    /// Stream indices with a live engine, ascending (QMap keeps its keys
    /// sorted).
    QList<int> streams() const;

    /// Every live production source, ordered by stream then tier.
    QList<FftSourceKey> sources() const;

    /// Number of live engines.
    int engineCount() const;

    bool frameForwardingEnabled() const { return m_forwardFrameReady; }

signals:
    /// Re-emission of every pooled engine's fftReadyLinear, with the
    /// emitting engine's own stream index as the first argument. That
    /// index is always the engine's receiverId -- every engine is
    /// constructed with receiverId == streamIndex -- so this is a plain
    /// signal-to-signal forward, not a per-engine lambda.
    void fftFrameReady(int streamIndex, const QVector<float>& binsLinear,
                       double windowEnb, double dbmOffset);

private:
    FFTEngine* createEngine(const FftSourceKey& key, const FftPoolConfig& cfg);
    static void applyConfigTo(FFTEngine* engine, const FftPoolConfig& cfg);

    FftPoolConfig          m_config;
    bool                   m_forwardFrameReady{true};
    std::optional<int>     m_decimation;
    QMap<FftSourceKey, FFTEngine*> m_engines;
    QMap<int, QThread*>    m_threadsByBucket;  // keyed by streamIndex % threadCount
};

} // namespace NereusSDR
