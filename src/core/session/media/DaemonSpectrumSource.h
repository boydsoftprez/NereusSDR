#pragma once
// =================================================================
// src/core/session/media/DaemonSpectrumSource.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  A daemon-owned, explicit spectrum
// producer.  It intentionally contains no endpoint, reducer, codec or
// transport policy.
//
// Modification history (NereusSDR):
//   2026-10-15: Display send thread: setFrameWake(), a published frame
//               wakes the display thread instead of queueing frameAvailable
//               to the main thread; currentGeneration(), so a frame held
//               for the main thread is dropped after a retune. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-21: producedAtNs is the time of the frame's samples
//               (SpectrumSampleClock), not the FFT's publish time; input
//               queue overflows, the engine's 5 ms skips and the clock's
//               re-anchors are counted, and the first two logged. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22: an I/Q sequence gap from the radio is an input
//               discontinuity, counted in inputHistoryResets (after
//               Longpath PR 62). J.J. Boyd (KG4VCF), with AI-assisted
//               transformation via Claude Code.
//   2026-10-23: the gap takes the engine's bounded window reset, as the
//               desktop app's engines do; engineWindowResets and
//               engineWindowResetsSkipped. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Claude Code.
//   2026-10-24: engineWindowResetsSkipped's doc names the engine's rule as
//               it now stands (JJ's ruling, option i). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Claude Code.
//   2026-10-26: setEngineFrameGapClockForTest, the engine's 5 ms minimum
//               frame gap on a test's clock. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Claude Code.
// =================================================================

#include "core/spectrum/FftEnginePool.h"
#include "core/NereusCoreExport.h"

#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

class FFTEngine;

namespace NereusSDR {

class RadioModel;

struct DaemonSpectrumSourceConfig {
    FftPoolConfig fft;
    double centreHz{0.0};
    double sampleRateHz{0.0};
    /// Explicit queue capacity in interleaved floats.  There is deliberately
    /// no guessed default: the service accepts a bounded ingress budget.
    int maxPendingIqFloats{0};
    /// R-R3-08/40: the Core is busy, so each transform advances a whole
    /// frame period (FFTEngine::setTransformsFollowFrameRate). Like the
    /// frame rate it changes how far the window moves, not what a bin
    /// represents.
    bool transformsFollowFrameRate{false};
    /// Parity Task 17 (R-R3-01): the engine's input decimation, 1 to 16
    /// (FFTEngine::setDecimation, Setup > Display > Rendering > Decimation):
    /// only every Nth I/Q pair reaches the FFT. A change renews the input
    /// history, as the FFT size does.
    int decimation{1};
};

/// What DaemonSpectrumSource::update() does with a change of the center alone.
/// R-DISP-RETUNE: `Restart` is the default and what every caller that
/// measures gets (the auto-AGC noise floor, level calibration): the source
/// restarts and its input history is emptied, so no frame under the new
/// center holds a sample from before the move. `Relabel` is the display's
/// choice: the running source takes the new center as a label and carries
/// on, and for up to one FFT length its frames mix samples from both sides
/// of the move.
enum class DaemonSpectrumCenterMove {
    Restart,
    Relabel,
};

/// What DaemonSpectrumSource::update() did to a source it accepted.
/// R-DISP-RETUNE: `relabeled` is true when only the center changed and the
/// running source took it as a new label, without a restart; what it
/// published under `fromGeneration` is then continued by `toGeneration`.
/// Otherwise the source restarted and `toGeneration` continues nothing.
struct DaemonSpectrumSourceUpdate {
    bool relabeled{false};
    quint64 fromGeneration{0};
    quint64 toGeneration{0};
};

/// A complete, unreduced FFT frame.  `generation` changes on every accepted
/// activate/update so consumers can tell a previous frequency context.
/// `producedAtNs` is a monotonic producer timestamp, not wall-clock time:
/// the time of the frame's samples on monotonicNowNs()'s clock
/// (SpectrumSampleClock), so frames of one source are spaced as their
/// samples are, free of the FFT's publish jitter.
struct DaemonSpectrumFrame {
    MediaSourceKey source;
    quint64 generation{0};
    double centreHz{0.0};
    double sampleRateHz{0.0};
    qint64 producedAtNs{0};
    /// producedAtNs less that of the frame the source published before this
    /// one, whether or not anyone took it: the source's frame spacing, so an
    /// endpoint slower than its source can take the frame nearest each of
    /// its own slots (SpectrumEndpoint::consume). 0 for a source's first
    /// frame and the first after its samples were interrupted.
    qint64 sourceFrameGapNs{0};
    QVector<float> binsLinear;
    double windowEnb{0.0};
    double dbmOffset{0.0};
    /// The bins in dBm, filled on the engine thread only by a source whose
    /// setComputesBinsDbm(true) is set (DaemonAgcSource), so its consumer
    /// does no per-bin log10 on the main thread. binsLinearToDbm() gives the
    /// values; binsDbmValid is false when it rejected the frame.
    QVector<float> binsDbm;
    bool binsDbmValid{false};
    /// Set by DaemonSpectrumSource::takeLatest(): how many frames before
    /// this one the engine put in the source's one-frame slot and replaced
    /// there with nobody having taken them. Frames the source discarded
    /// itself (a retune, a restart) are not among them.
    quint32 replacedBefore{0};
};

class NEREUS_CORE_EXPORT DaemonSpectrumSource final : public QObject {
    Q_OBJECT
public:
    explicit DaemonSpectrumSource(QObject* parent = nullptr);
    ~DaemonSpectrumSource() override;

    /// The RadioModel remains externally owned.  Supplying it wires its
    /// tagged I/Q signal straight to each active engine's worker thread;
    /// no I/Q packet is bounced through this object's thread.
    void setRadioModel(RadioModel* radioModel);

    /// Creates a source only after an explicit subscription activation.
    /// Returns false without side effects when config is invalid or the key
    /// already exists.  The first accepted generation is one.
    bool activate(const MediaSourceKey& key,
                  const DaemonSpectrumSourceConfig& config);

    /// Applies a validated source context and increments its generation.
    ///
    /// R-DISP-RETUNE: when the caller passes DaemonSpectrumCenterMove::Relabel,
    /// the center is the only field that differs and the source is running,
    /// the source is relabeled, not restarted. It stays
    /// active, queued I/Q and the engine's overlap history are kept, nothing
    /// is counted as dropped, and the next frame is published on the normal
    /// cadence with the new center and generation. A frame published before
    /// the call and not yet taken is discarded, so no consumer is handed the
    /// old center after the move.
    ///
    /// Any other change (sample rate, FFT size, window, decimation, bin
    /// target, queue size, frame rate), and every change under the default
    /// DaemonSpectrumCenterMove::Restart, restarts the source: it accepts no
    /// I/Q until its engine has taken the configuration, and a changed
    /// center, sample rate, FFT size, window or decimation drops queued
    /// input and the engine's overlap history first.
    ///
    /// `result`, when given, says which of the two happened.
    bool update(const MediaSourceKey& key,
                const DaemonSpectrumSourceConfig& config,
                DaemonSpectrumCenterMove centerMove = DaemonSpectrumCenterMove::Restart,
                DaemonSpectrumSourceUpdate* result = nullptr);

    /// Changes only the engine's output frame rate and whether transforms
    /// follow it. Both set how far the FFT window advances between frames,
    /// not what a bin represents, so the generation, queued input and
    /// overlap history all stand and no consumer's context is renewed.
    /// Returns false for an unknown key or a rate outside 1..60.
    bool updateFrameRate(const MediaSourceKey& key, int fps,
                         bool transformsFollowFrameRate = false);

    /// A MOX edge: every active source's queued I/Q is discarded and the
    /// source is marked for an input reset through the discontinuity path a
    /// dropped packet uses, so each engine clears its own input history on
    /// its own worker thread before it takes the first samples after the
    /// edge. No generation changes and no consumer's context is renewed.
    /// Owner thread only.
    void resetInputHistoryAll();
    /// Input resets the source's engine has made on its worker thread for a
    /// discontinuity (a dropped packet, an I/Q sequence gap from the radio
    /// that dropped the window, or resetInputHistoryAll). 0 for an unknown
    /// source.
    quint64 inputHistoryResets(const MediaSourceKey& key) const;
    /// The engine's windows dropped for an I/Q sequence gap
    /// (FFTEngine::windowResets), and the gaps it let through
    /// (FFTEngine::windowResetsSkipped): with no frame for
    /// FFTEngine::kGapResetStallFrameIntervals frame intervals, and then
    /// every gap until a whole window passes with none
    /// (FFTEngine::requestWindowReset). 0 for an unknown source.
    quint64 engineWindowResets(const MediaSourceKey& key) const;
    quint64 engineWindowResetsSkipped(const MediaSourceKey& key) const;

    /// Stops a single source tier and releases its engine. Safe for an
    /// unknown key.  The other tier for the same stream remains active.
    void deactivate(const MediaSourceKey& key);
    void deactivateStream(int streamIndex);

    bool isActive(const MediaSourceKey& key) const;
    QList<MediaSourceKey> activeSources() const;
    quint64 droppedInputFrames(const MediaSourceKey& key) const;
    /// Times a full input queue was emptied to take a new packet (a slow
    /// engine thread): each also counts in droppedInputFrames and resets
    /// the engine's input history. Logged at most once per 10 s.
    quint64 inputQueueOverflows(const MediaSourceKey& key) const;
    /// The engine's 5 ms minimum frame gap skips (FFTEngine::
    /// minimumGapSkips), 0 for an unknown source.
    quint64 engineMinimumGapSkips(const MediaSourceKey& key) const;
    /// Times the source's sample clock took a new anchor for a large error
    /// (SpectrumSampleClock::largeErrorReanchors): samples lost without a
    /// reset, or a stall.
    quint64 sampleClockReanchors(const MediaSourceKey& key) const;
    /// Number of completed engine I/Q handoffs since source activation.
    quint64 completedInputHandoffs(const MediaSourceKey& key) const;
    /// Frames the engine published into this source's latest-frame slot,
    /// each replacing the one before (so it can exceed frameAvailable).
    quint64 publishedFrames(const MediaSourceKey& key) const;
    /// Read-only source queue state for diagnosing a slow first context.
    struct InputQueueDiagnostics {
        quint64 generation{0};
        bool active{false};
        bool configurationPending{false};
        bool drainQueued{false};
        int pendingIqFloats{0};
        int maxPendingIqFloats{0};
    };
    InputQueueDiagnostics inputQueueDiagnostics(const MediaSourceKey& key) const;
    /// The generation the source's frames carry now, 0 for an unknown key.
    /// Display send thread: a frame taken before an update() is no longer
    /// of it.
    quint64 currentGeneration(const MediaSourceKey& key) const;
    /// The decimation the source's engine runs at (FFTEngine::decimation),
    /// 0 for an unknown source.
    int engineDecimation(const MediaSourceKey& key) const;

    /// Returns and clears the one latest frame slot for key.  A slow consumer
    /// can therefore miss frames but cannot cause an output-frame backlog.
    std::optional<DaemonSpectrumFrame> takeLatest(const MediaSourceKey& key);

    /// Display send thread: who is told that a frame waits, in place of the
    /// queued frameAvailable. While one is set, every frame an engine
    /// publishes calls `wake` on that engine's thread, outside the frame
    /// lock, and queues no frameAvailable; the woken side takes the frames
    /// with takeLatest(). Empty (the default), frameAvailable is queued as
    /// before; a frame published while a wake was set is not announced
    /// then, so whoever clears the wake takes the waiting frames itself.
    /// The owner's thread. When it returns, no engine thread is still
    /// inside the wake it replaced.
    void setFrameWake(std::function<void()> wake);

    /// Test and non-RadioModel ingress.  Real daemon production uses
    /// RadioModel::rawIqDataForStream.  This is queued to the engine thread
    /// and intentionally has the same input-backlog characteristic as that
    /// production route.
    void submitIq(int streamIndex, const QVector<float>& interleavedIq);

    /// Test seam: publishes one frame for an active source as its engine
    /// would, stamped `producedAtNs` instead of the clock, with every bin at
    /// `binLinear`, and emits frameAvailable now rather than queued (or,
    /// with a frame wake set, calls the wake). Lets a test drive the
    /// source's cadence itself. False for an unknown source
    /// or one whose engine has not yet taken its configuration.
    bool publishFrameForTest(const MediaSourceKey& key, qint64 producedAtNs,
                             float binLinear = 1.0e-6f);
    /// As above with the engine's bins given: `binsLinear` holds one power
    /// per bin, exactly the source's FFT size (false otherwise), so a test
    /// can drive a shaped spectrum on a clock of its own.
    bool publishFrameForTest(const MediaSourceKey& key, qint64 producedAtNs,
                             const QVector<float>& binsLinear);

    /// Test seam: publishes one frame as the engine would, stamped by the
    /// source's sample clock for samples ending at `inputPosition` (I/Q
    /// pairs) and published at `publishedAtNs`, so a test can check the
    /// stamps the source gives a jittered publish. Returns the stamp, or
    /// nothing for an unknown or inactive source.
    std::optional<qint64> publishSampledFrameForTest(const MediaSourceKey& key,
                                                     qint64 inputPosition,
                                                     qint64 publishedAtNs);

    /// Test seam: key's engine reads its 5 ms minimum frame gap from
    /// `clockMs` (FFTEngine::setFrameGapClockForTest). Call it after
    /// activate() and before any I/Q is fed. False for an unknown source,
    /// or once the engine has taken input.
    bool setEngineFrameGapClockForTest(const MediaSourceKey& key,
                                       std::function<qint64()> clockMs);

    /// Test seam: whether a published frame waits in key's latest-frame
    /// slot, not yet taken.
    bool frameWaitingForTest(const MediaSourceKey& key) const;

    /// The producer clock: steady_clock nanoseconds since its epoch, the
    /// clock every DaemonSpectrumFrame::producedAtNs (and so each display
    /// frame's producerTimestamp) is read from. R-R3-21: the Core's media
    /// clock (DaemonMediaController::displayNowNs) is this same clock, so a
    /// window can present display frames on its audio's clock.
    static qint64 monotonicNowNs();

    /// When true, every frame this source publishes also carries binsDbm,
    /// computed on the engine thread that produced it. Any thread.
    void setComputesBinsDbm(bool computes);
    bool computesBinsDbm() const;

    /// Exactly FFTEngine::fftReady's conversion, as DaemonAgcSource did it
    /// per frame on the main thread: a bin below 1e-20 is -200 dBm, any
    /// other is 10*log10(power) + dbmOffset. Returns false (and leaves `out`
    /// empty) when a power or a result is not finite, which rejects the
    /// whole frame.
    static bool binsLinearToDbm(const QVector<float>& binsLinear, double dbmOffset,
                                QVector<float>& out);

signals:
    /// At most one queued notification per active source exists at a time.
    /// Consumers call takeLatest() to acquire the current frame.
    void frameAvailable(NereusSDR::MediaSourceKey key);

private:
    struct FrameState;
    struct SourceEntry;

    static bool isValidConfig(const MediaSourceKey& key,
                              const DaemonSpectrumSourceConfig& config);
    static bool inputHistoryIsCompatible(const DaemonSpectrumSourceConfig& before,
                                         const DaemonSpectrumSourceConfig& after);
    static bool onlyCenterDiffers(const DaemonSpectrumSourceConfig& before,
                                  const DaemonSpectrumSourceConfig& after);
    static void applyEngineConfig(FFTEngine* engine,
                                  const DaemonSpectrumSourceConfig& config);
    static void enqueueIq(FFTEngine* engine,
                          const QSharedPointer<FrameState>& state,
                          const QVector<float>& interleavedIq);

    static void drainInput(FFTEngine* engine,
                           const QSharedPointer<FrameState>& state);
    void attachRadioIq(SourceEntry& entry);
    void queueConfiguredActivation(SourceEntry& entry,
                                   const DaemonSpectrumSourceConfig& config,
                                   quint64 generation,
                                   bool resetInputHistory);
    void publishFrame(const MediaSourceKey& key,
                      const QSharedPointer<FrameState>& state,
                      const QVector<float>& binsLinear,
                      double windowEnb,
                      double dbmOffset,
                      qint64 inputPosition,
                      qint64 publishedAtNs);

    /// Sets state.latest.sourceFrameGapNs from its producedAtNs and the
    /// frame published before it. Under the state's lock.
    static void noteSourceFrameGap(FrameState& state);

    /// Calls the wake, when one is set, for a frame just published, or
    /// says there is none. Engine threads and the owner's.
    bool callFrameWake();

    QPointer<RadioModel> m_radioModel;
    // Display send thread: setFrameWake()'s wake. The mutex is held while
    // the wake is called, so replacing it waits for a call under way.
    std::mutex m_frameWakeMutex;
    std::function<void()> m_frameWake;
    std::unique_ptr<FftEnginePool> m_pool;
    QMap<MediaSourceKey, SourceEntry> m_sources;
    quint64 m_nextGeneration{0};
    std::atomic<bool> m_computesBinsDbm{false};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::FftSourceKey)
Q_DECLARE_METATYPE(NereusSDR::DaemonSpectrumFrame)
