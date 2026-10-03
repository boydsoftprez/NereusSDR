// =================================================================
// src/core/audio/RemoteVaxFeeder.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-44 (R3 receiver audio plan,
// Task 5): VAX in a remote window. The rate matching it drives is the
// existing RemoteAudioRateMatcher (WDSP rmatch, attributed there); this
// file adds no ported logic.
//
// One feeder per VAX channel. It is a receiver-stream sink
// (IReceiverPcmSink): the remote window asks the Core for the audio of
// every slice assigned to the channel and the streams' blocks land here on
// the receive worker thread. receiverAudioBlock() only copies the block
// into that slice's bounded hand-off ring and returns (no allocation, no
// lock, no device or network work), because it runs under the stream's
// lock (R3 receiver audio plan, Task 3's finding).
//
// Slices sharing a channel are mixed, as the local VAX tee mixes them
// (AudioEngine, VaxChannelMixer): the pump sums the slices' rings in
// 192-frame chunks before the rate matcher. A chunk waits until every
// slice still sending has one queued; a slice whose stream stopped, or
// that has sent nothing for kQuietNs, counts as silence until its audio
// flows again. A slice that falls behind the others (a stall, then a
// burst) keeps at most kMaxLagFrames more queued than the least-queued
// slice; the oldest beyond that is dropped, so it cannot play late from
// then on.
//
// A worker of its own (pump(), every 5 ms) moves the audio from the rings
// through a rate matcher into the VAX output, topping the output's queue
// up to a small target as the output reports consuming it. The output's
// own clock (the app reading the VAX device) therefore paces the audio,
// not the Core's: a difference between the two clocks is absorbed by the
// rate matcher instead of growing or starving the delay.
//
// Nothing reading the output is not a fault. When the output reports no
// progress with audio queued for longer than an app's read step (at least
// 60 ms), the feeder stops feeding the rate matcher and drops what
// arrives; after 500 ms it reports that nothing reads the output (a VAX
// device no app has open). It plays again as soon as the output moves.
// When the Core goes quiet for 250 ms, it restarts the rate matcher and
// waits for audio.
//
// An output that reports no playback timing (the Linux pactl pipe) is
// written as the audio arrives, paced by the Core's stream alone.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: R-R3-44 fix wave: a set of slices per channel, one hand-off
//                 ring each, summed before the rate matcher. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-44 fix wave follow-up: stop reasons kept per slice;
//                 a slice's lag behind the others capped at 85 ms; a slice
//                 leaving no longer trims the others. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AudioRingSpsc.h"
#include "core/session/media/IReceiverPcmSink.h"
#include "core/session/media/RemoteAudioRateMatcher.h"

#include <QList>
#include <QString>
#include <QVector>

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace NereusSDR {

class AudioEngine;

/// Where a feeder writes: one VAX output's playback timing and its write.
/// Both are called on the feeder's worker only.
struct VaxOutputPort {
    std::function<std::optional<IAudioBus::OutputPacing>()> pacing;
    /// Interleaved 48 kHz stereo, `frames` frames. False when nothing was
    /// written (the output is closed).
    std::function<bool(const float* stereo, int frames)> write;

    /// The engine's VAX output for `channel` (1..4), with the channel's
    /// VAX gain and mute applied (AudioEngine::writeVaxOutput).
    static VaxOutputPort forEngine(AudioEngine* engine, int channel);
};

struct RemoteVaxFeederStats {
    enum class State {
        Idle,            // no slice assigned
        WaitingForAudio, // assigned; nothing arriving from the Core
        Playing,
        NoReader,        // the output stopped taking audio; nothing is reading it
    };
    State state = State::Idle;
    // The lowest assigned slice (-1 for none), and all of them.
    int sourceSliceId = -1;
    QList<int> sourceSliceIds;
    // Frames the streams delivered for the assigned slices (blocks for any
    // other slice are ignored), and frames dropped because a hand-off ring
    // was full.
    quint64 receivedFrames = 0;
    quint64 droppedFrames = 0;
    // Frames dropped from a slice's ring to keep the slices in step (a
    // slice lagging the others, or the backlog behind one that went quiet).
    quint64 trimmedFrames = 0;
    quint64 writtenFrames = 0;
    // The rate matcher ran dry or over while the output was reading, and
    // was restarted: a gap the app heard (the Core's audio ran late or
    // stopped). An output that stops reading never counts here.
    int restarts = 0;
    // The delay ahead of the output, in frames at 48 kHz: its own queue,
    // the rate matcher's fill and the fullest hand-off ring.
    int queuedFrames = 0;
    int matcherFillFrames = 0;
    int handoffFrames = 0;
    // The rate matcher's output-per-input ratio, once it has measured one.
    std::optional<double> ratio;
    // The output reports playback timing, so the feeder paces by it.
    bool paced = false;
    int delayFrames() const { return queuedFrames + matcherFillFrames + handoffFrames; }
};

class RemoteVaxFeeder final : public IReceiverPcmSink {
public:
    /// Monotonic nanoseconds. Empty: std::chrono::steady_clock.
    using Clock = std::function<qint64()>;

    // One lossless packet (4 ms); an Opus packet is ten of them.
    static constexpr int kInputChunkFrames = 192;
    static constexpr int kOutputBlockFrames = 480;
    // The rate matcher's ring: 180 ms, the remote speaker's default, so it
    // starts with 90 ms of reserve (WDSP rmatch starts half full).
    static constexpr int kMatcherRingFrames = 8640;
    // Hand-off ring from the receive worker, one per slice: 32768 stereo
    // frames, 683 ms.
    static constexpr std::size_t kHandoffBytes = std::size_t(1) << 18;
    // Slices one channel can carry: a Core's slice limit
    // (WdspEngine::kMaxSliceChannels).
    static constexpr int kMaxSources = 5;
    static constexpr qint64 kPumpIntervalNs = 5'000'000;
    // The output took nothing for this long with audio queued: it paused
    // (at least; twice its largest read step when that is longer).
    static constexpr qint64 kPausedNs = 60'000'000;
    // ... and for this long: no reader.
    static constexpr qint64 kNoReaderNs = 500'000'000;
    // No audio from the Core for this long: wait for it afresh.
    static constexpr qint64 kQuietNs = 250'000'000;
    // How far one slice may run behind the others: 4096 frames, 85 ms at
    // 48 kHz, the ring the local VAX mix keeps per slice
    // (VaxChannelMixer::kRingFrames).
    static constexpr int kMaxLagFrames = 4096;

    RemoteVaxFeeder(int channel, VaxOutputPort port, Clock clock = {});
    ~RemoteVaxFeeder() override;

    RemoteVaxFeeder(const RemoteVaxFeeder&) = delete;
    RemoteVaxFeeder& operator=(const RemoteVaxFeeder&) = delete;

    int channel() const { return m_channel; }

    /// The slices whose streams feed this channel (at most kMaxSources;
    /// empty for none). Anything queued for a slice no longer in the set is
    /// dropped; a slice that stays keeps playing. GUI thread, after the
    /// streams of the slices leaving are released and before those of the
    /// slices joining are requested.
    void setSourceSlices(const QList<int>& sliceIds);
    /// One slice, or none for -1.
    void setSourceSlice(int sliceId);
    QList<int> sourceSlices() const;
    /// The lowest assigned slice, -1 for none.
    int sourceSlice() const;

    // IReceiverPcmSink
    /// Receive worker thread: copies the block into its slice's hand-off
    /// ring and returns. A block for a slice not assigned here, or one that
    /// does not fit, is dropped.
    void receiverAudioBlock(int sliceId, const float* interleavedStereo, int frames) override;
    /// GUI thread: records the reason; that slice counts as silence until
    /// its audio flows again.
    void receiverAudioStopped(int sliceId, const QString& reason) override;
    /// Why an assigned slice's stream stopped, per slice, oldest first:
    /// each stays until that slice's audio flows again or it leaves the
    /// channel. GUI thread.
    struct Stop {
        int sliceId = -1;
        QString reason;   // a wire reason or a sentence
    };
    QList<Stop> stops() const;
    /// The most recent of stops(), empty when there is none. GUI thread.
    QString lastStopReason() const;

    /// Runs pump() every kPumpIntervalNs on a thread of its own until
    /// stopWorker() or destruction. Tests drive pump() themselves instead.
    void startWorker();
    void stopWorker();
    bool workerRunning() const;

    /// One step: move what arrived into the output as far as its queue
    /// target. Called by the worker, or by a test with its own clock; never
    /// from two threads at once.
    void pump();

    RemoteVaxFeederStats stats() const;

private:
    using State = RemoteVaxFeederStats::State;

    struct Source {
        // Written by the GUI thread, read by the receive worker and pump.
        std::atomic<int> sliceId{-1};
        std::atomic<quint32> generation{0};
        // Bytes the receive worker has pushed, and how many of them
        // belonged to the previous slice in this slot when it last changed
        // (the change follows that slice's release, so none of its blocks
        // follow).
        std::atomic<quint64> pushedBytes{0};
        std::atomic<quint64> dropUntilBytes{0};
        // The Core stopped this slice's stream; silence until audio flows.
        std::atomic<bool> stopped{false};
        // Frames this slot's slice delivered since it was assigned.
        std::atomic<quint64> receivedFrames{0};
        std::unique_ptr<AudioRingSpsc<kHandoffBytes>> handoff;

        // GUI thread: why the slice's stream stopped, the frames it had
        // delivered then (the stop is news until it delivers more), and
        // when (for the order of stops()).
        QString stopReason;
        quint64 stopAtFrames = 0;
        quint64 stopOrder = 0;

        // Pump only.
        quint32 seenGeneration = 0;
        quint64 poppedBytes = 0;
        quint64 seenPushedBytes = 0;
        qint64 lastArrivalNs = 0;
        bool heard = false;
        bool wasLive = false;
    };

    Source* sourceFor(int sliceId);
    void dropHandoff();
    void dropHandoff(Source& source);
    void popHandoff(Source& source, void* into, std::size_t bytes);
    // A slice counted in the mix: sending, and heard within kQuietNs.
    bool isLive(const Source& source, qint64 now) const;
    // Drops the oldest `frames` of a slice's ring and counts them.
    void trimHandoff(Source& source, std::size_t frames);
    qint64 pausedNs() const;
    void restartMatcher();
    // Sums whole chunks from the live slices' rings into the matcher (or,
    // unpaced, straight to the output). Returns the frames moved.
    int drainHandoff(bool paced, qint64 now);
    void publish(State state, int queuedFrames);

    const int m_channel;
    VaxOutputPort m_port;
    Clock m_clock;

    std::array<Source, kMaxSources> m_sources;
    // Written by the GUI thread, read by the pump.
    std::atomic<quint32> m_generation{0};
    std::atomic<quint64> m_receivedFrames{0};
    std::atomic<quint64> m_droppedFrames{0};

    // Pump only.
    RemoteAudioRateMatcher m_matcher;
    bool m_matcherReady = false;
    bool m_matcherTried = false;
    QVector<float> m_chunk;
    QVector<float> m_part;
    QVector<float> m_matcherChunk;
    quint32 m_seenGeneration = 0;
    State m_state = State::Idle;
    bool m_haveConsumed = false;
    quint64 m_lastConsumed = 0;
    qint64 m_lastProgressNs = 0;
    qint64 m_lastInputNs = 0;
    int m_largestStep = 0;
    int m_restarts = 0;
    quint64 m_writtenFrames = 0;

    mutable std::mutex m_statsMutex;
    RemoteVaxFeederStats m_stats;
    quint64 m_stopCount = 0;  // GUI thread: orders the stops
    quint64 m_trimmedFrames = 0;  // pump only

    std::mutex m_workerMutex;
    std::condition_variable m_workerWake;
    bool m_workerStop = false;
    std::thread m_worker;
};

} // namespace NereusSDR
