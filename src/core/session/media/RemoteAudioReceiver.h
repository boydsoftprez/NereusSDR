#pragma once
// no-port-check: NereusSDR-original remote audio lifecycle and worker wiring.
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/IRemotePcmWorkerStage.h"
#include <QObject>
#include <QByteArray>
#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {
class AudioEngine;
// R-R3-45: defined in AudioEngine.h (Speakers, Headphones).
enum class RemotePlaybackOutput : int;

// R-R3-35: when a known RTP time will be heard. rtpTimestamp is the end of
// the newest packet handed to the rate matcher; behind it, when the worker
// read them at measuredNs, sat matcherFillFrames in the rate matcher and
// speakerQueuedFrames in the speaker queue, and then the device's own
// latency when the backend reports it. All times are the receiver clock's.
// R-R3-23: the rate matcher makes audio at the speaker device's rate, so the
// matcher fill, the speaker queue and the callback count frames at
// deviceRateHz; the pipeline and codec delays count 48 kHz stream frames.
struct RemoteAudioPlayoutPoint {
    quint32 rtpTimestamp = 0;
    qint64 measuredNs = 0;
    int matcherFillFrames = 0;
    int speakerQueuedFrames = 0;
    std::optional<qint64> deviceLatencyNs;
    /// The speaker device's rate, which the three device-side frame counts
    /// use.
    int deviceRateHz = 48'000;
    /// Fixed delays inside the pipeline, in frames at 48 kHz: the codec's
    /// algorithmic delay (Opus lookahead; none for lossless) and the rate
    /// matcher's filter delay. A sample at rtpTimestamp comes out of them
    /// this much later than the fill and queue alone say.
    int pipelineDelayFrames = 0;
    /// The device callback's quantum. The speaker queue drains a callback at
    /// a time, so the queue read at measuredNs is heard up to one callback
    /// later than a steady drain would say, depending on where in its cycle
    /// the callback was: half a callback is counted, and half is accuracy.
    int callbackFrames = 0;
    /// The clock readings taken just before and just after the queue read
    /// were this far apart; measuredNs is their midpoint and half of it is
    /// accuracy.
    qint64 readWindowNs = 0;
    /// Of pipelineDelayFrames, the part before the rate matcher: the codec's
    /// delay (Opus lookahead; none for lossless). Everything else ahead of
    /// the newest sample is audio the rate matcher has already made.
    int codecDelayFrames = 0;
    /// Output frames the rate matcher makes per input frame when read (WDSP
    /// rmatch's var; 1 before its control starts). While it corrects, the
    /// delay itself changes as the audio plays.
    double matcherRatio = 1.0;
    /// How much longer the newest sample's delay is than that of the sample
    /// heard at measuredNs: the audio between them was made at matcherRatio,
    /// so it spans 1 / matcherRatio as much capture time as play time.
    /// Zero at a ratio of 1 (or one that is not a positive number).
    qint64 matcherStretchNs() const;
    /// measuredNs plus the matcher fill, the speaker queue and half a
    /// callback at deviceRateHz, the pipeline delay at 48 kHz, plus the
    /// device latency when known.
    qint64 playoutNs() const;
    /// How far the true playout time may lie from playoutNs(): half a
    /// callback plus half the read window.
    qint64 accuracyNs() const;
    bool operator==(const RemoteAudioPlayoutPoint&) const = default;
};

// R-R3-35: the end RTP time of the newest received (not concealed) packet
// released from the reorder buffer, and when it was released.
struct RemoteAudioReleasePoint {
    quint32 rtpTimestamp = 0;
    qint64 releasedNs = 0;
    bool operator==(const RemoteAudioReleasePoint&) const = default;
};

// Read-only diagnostics for one remote playback context. generation advances
// only after a successful start. Packet counters stay available after stop for
// diagnostics; a later successful start resets them. deviceConsumedFrames and
// device progress are published only after the worker observes the speaker
// pacing source.
struct RemoteAudioReceiverTelemetry {
    quint64 generation = 0;
    bool running = false;
    // Admitted to the jitter queue and not later discarded at start: a packet
    // trimmed with a connect-time backlog moves to startDiscardedPackets.
    quint64 acceptedPackets = 0;
    quint64 decodedPackets = 0;
    quint64 concealedPackets = 0;
    quint64 latePackets = 0;
    quint64 invalidPackets = 0;
    quint64 duplicatePackets = 0;
    quint64 rejectedHeaders = 0;
    // Packets dropped before playback began because a connect-time backlog
    // overran the arrival bound or the jitter window. Only the newest packet
    // of such a backlog is kept; nothing counted here was ever heard.
    quint64 startDiscardedPackets = 0;
    // Valid RTP/profile payload bytes received in this context, Opus or
    // lossless (whichever the context runs). This counts duplicates and
    // packets later dropped by the bounded local queue.
    quint64 receivedAudioPayloadBytes = 0;
    // Frames the speaker device played in this context, at its own rate.
    quint64 deviceConsumedFrames = 0;
    int underflows = 0;
    int overflows = 0;
    // Lifetime interruption totals remain available across receiver contexts.
    // They are absent only in the bounded transition-unavailable snapshot.
    std::optional<quint64> lifetimeUnderflows;
    std::optional<quint64> lifetimeOverflows;
    std::optional<qint64> lastAdmittedPacketAgeMs;
    std::optional<qint64> lastDeviceProgressAgeMs;
    // Worker-observed speaker queue duration. It is unavailable before a
    // pacing sample, and for stopped or failed contexts.
    std::optional<double> speakerQueuedMs;
    // RFC 3550 interarrival jitter and extended-sequence expected/missing
    // accounting for this computer's admitted RTP in this context, from
    // RtpReceptionStats. Like the packet counters above, the last measured
    // value stays available after stop(); a later successful start() resets
    // it. arrivalJitterMs is absent until a second packet has been observed.
    std::optional<double> arrivalJitterMs;
    quint64 expectedPackets = 0;
    quint64 missingPackets = 0;
    // Packets currently held in the jitter buffer for reordering, in ms
    // (queued count x the context's packet duration: 40 ms Opus, 4 ms
    // lossless). Unlike the
    // fields above, this is a live gauge: unavailable before a measurement
    // and for stopped or failed contexts, like speakerQueuedMs.
    std::optional<double> reorderQueuedMs;
    // R-R3-21: how long the jitter queue holds each packet now, in ms. It
    // starts at 80 ms, deepens (up to 500 ms) when packets arrive after
    // their interval was concealed, and eases back while the link is
    // steady. A live gauge like reorderQueuedMs.
    std::optional<double> jitterHoldMs;
    // R-R3-21: what this context rode through on this computer instead of
    // asking the Core for a fresh context. Like the packet counters, reset
    // by a successful start() and kept after stop().
    // Packets dropped at the arrival bound during playback (a burst larger
    // than the bound), one per packet; each interval is then concealed.
    quint64 burstDroppedPackets = 0;
    // Stream gaps: a packet beyond the jitter window moved the head to it.
    quint64 streamGapReanchors = 0;
    // Queued packets dropped unheard to bound the delay: those a stream
    // gap moved past, and those shed once the link was quiet again after a
    // stall, so the delay it added comes back down as the hold eases.
    // This computer's latency policy, not the network's loss.
    quint64 trimmedPackets = 0;
    // Intervals skipped unheard for the same reasons, present or missing,
    // and that as audio time (intervals x the context's packet duration),
    // which is what the operator heard skipped.
    quint64 skippedIntervals = 0;
    double skippedAudioMs = 0.0;
    // Intervals a rewind replayed: each was heard once concealed and again
    // as its late audio, so decoded + concealed + skipped - rewound is the
    // stream's own timeline.
    quint64 rewoundIntervals = 0;
    // burstDroppedPackets as audio time.
    double burstDroppedAudioMs = 0.0;
    // The longest the receive worker went between wakes in this context:
    // a starved worker, not the network, when a speaker runs dry.
    double maxWorkerWakeGapMs = 0.0;
    // Network interruptions, at most one a worker pass: an arrival burst,
    // a stream gap, or packets that came after their intervals were
    // concealed (a stall). The lossless link trial counts them as it
    // counted the restarts they used to cause.
    quint64 linkInterruptions = 0;
    // The continuous clock correction's current resample ratio (WDSP rmatch
    // `var`, read through RemoteAudioRateMatcherStats::currentRatio), the
    // same value restart fault text reports as `ratio=`. It is a ratio near
    // 1.0; (ratio - 1) x 1e6 is the correction in parts per million. WDSP
    // holds the initial 1.0 until its 3.0 s startup delay of audio has passed
    // (third_party/wdsp/src/rmatch.c create_rmatchV), then adjusts it. A live
    // gauge like speakerQueuedMs: absent until rmatch has measured it (its
    // startup delay has passed during playback), and for stopped or failed
    // contexts.
    std::optional<double> driftRatio;
    // R-R3-35 live gauges, like speakerQueuedMs: absent before the worker
    // has played or released a packet in this context, and for stopped or
    // failed contexts.
    std::optional<RemoteAudioPlayoutPoint> playout;
    std::optional<RemoteAudioReleasePoint> release;
};

// One generation of bounded RTP receive, decoding and WDSP rate matching.
// All codec/resampler work runs off the GUI and device callback threads.
// R-R3-23: a context runs one profile, named at start(): Opus (40 ms
// packets, payload type 111) or lossless (L16, 4 ms packets, payload type
// 96). submit() reads each packet's payload type and hands only the
// context's own type to its decoder; the other type is a rejected header.
// A lost lossless packet plays as 4 ms of silence. The jitter window and
// the arrival bound are the same 320 ms for both profiles, and both grow
// with the adaptive hold (R-R3-21). Only a true outage (nothing arrives,
// late packets included, for 500 ms once the speaker has started) or a
// real fault asks for a fresh context; a stream gap or an arrival burst
// re-anchors on this computer.
// R-R3-23: the speaker plays at whatever rate and channel count it opened
// at (AudioEngine::remotePlaybackFormat()): the rate matcher matches the
// 48 kHz stream to the device's rate and clock, and a mono device hears
// the two channels mixed as (left + right) / 2.
class RemoteAudioReceiver final : public QObject {
    Q_OBJECT
public:
    // The fault a worker notify() site observed. Q_ENUM registers it as a
    // real meta-type so QSignalSpy and the cross-thread Qt::QueuedConnection
    // delivery in notify() carry it, not an opaque int.
    enum class Fault {
        SpeakerOpenFailed, SpeakerTimingUnavailable, SpeakerCallbackTooLarge,
        SpeakerStalled, SpeakerWriteFailed, DecoderUnavailable,
        // R-R3-21: an arrival burst and a stream gap no longer end a
        // context (they re-anchor locally and count in linkInterruptions),
        // so they are no longer faults.
        NoPackets, DecodeFailed, ClockBuffer,
    };
    Q_ENUM(Fault)

    /// Monotonic nanoseconds, never negative, callable from any thread.
    using Clock = std::function<qint64()>;

    /// R-R3-43: where a receiver without a speaker hands its audio: frames
    /// of interleaved stereo 48 kHz float, in stream order. Called on the
    /// receive worker thread, never the GUI or a device callback thread;
    /// it must return quickly.
    using PcmSink = std::function<void(const float* interleavedStereo, int frames)>;

    /// R-R3-43: the PCM-sink mode. No speaker, no rate matcher and no
    /// AudioEngine: each packet is released by the jitter hold, so the
    /// Core's RTP clock paces the sink. A lost Opus packet is concealed and
    /// a lost lossless packet is silence, as for the speaker. The sink mode
    /// never asks for a restart because packets stopped: while the Core is
    /// quiet the sink hears missing-packet audio (silence) for 500 ms and
    /// then nothing, and the next packet to arrive starts the stream again
    /// on its own timestamp. R-R3-21: a stream gap or an arrival burst
    /// re-anchors locally, as for the speaker; a decode failure still asks
    /// for a restart, and a decoder that cannot start is still an error.
    /// Its hold stays fixed: a late packet is only late (no rewind and no
    /// deeper hold), so an app hears concealment for a stall and then the
    /// stream in place, never a pause and a burst.
    struct PcmSinkMode {
        PcmSink sink;
        std::shared_ptr<IRemotePcmWorkerStage> stage;
    };

    /// `clock` (R-R3-35) stamps arrivals, playout and release times. Empty:
    /// std::chrono::steady_clock, counted from this process's first use.
    explicit RemoteAudioReceiver(AudioEngine* engine, QObject* parent = nullptr,
                                 Clock clock = {});
    /// R-R3-45: a receiver that plays on `output` of `engine`: the
    /// headphones output for the Core's headphones mix, with its own rate
    /// matching against that device's clock. Everything else is as the
    /// speakers' receiver; the faults keep their names (SpeakerOpenFailed
    /// and so on) and mean the headphones device.
    RemoteAudioReceiver(AudioEngine* engine, RemotePlaybackOutput output,
                        QObject* parent = nullptr, Clock clock = {});
    /// The output this receiver plays on (Speakers unless built for another).
    RemotePlaybackOutput output() const;
    /// A receiver in the PCM-sink mode (see PcmSinkMode). Its telemetry
    /// counts frames handed to the sink as deviceConsumedFrames, with
    /// lastDeviceProgressAgeMs from the newest hand-off; it reports no
    /// speaker queue, drift ratio or playout point.
    RemoteAudioReceiver(PcmSinkMode mode, QObject* parent = nullptr, Clock clock = {});
    /// True for a receiver built in the PCM-sink mode.
    bool isPcmSink() const;
    ~RemoteAudioReceiver() override;
    /// The receiver clock now; the playout and release points use it.
    qint64 nowNs() const;
    bool start(quint32 ssrc, quint32 firstTimestamp,
               RemoteAudioProfile profile = RemoteAudioProfile::Opus);
    /// The profile of the current (or last) context.
    RemoteAudioProfile profile() const;
    void stop();
    void submit(const QByteArray& packet);
    bool isRunning() const;
    quint64 decodedPackets() const;
    quint64 concealedPackets() const;
    int rateMatcherUnderflows() const;
    int rateMatcherOverflows() const;
    // Thread-safe observational snapshot.  It neither reads worker-owned
    // jitter/resampler state nor synchronizes with the audio callback.
    RemoteAudioReceiverTelemetry telemetry() const;
#ifdef NEREUS_BUILD_TESTS
    /// Test-only step mode, set before start(): start() runs the receive
    /// worker's setup on the calling thread and starts no worker thread,
    /// and each runWorkerPassForTest() then runs exactly one pass of its
    /// loop on the calling thread, at the receiver clock's time and with no
    /// wait. With an injected clock and a paced test bus on the same clock,
    /// the whole receive path runs on virtual time. Off: nothing changes.
    void setStepModeForTest(bool on);
    /// One worker pass (step mode). False once the context has ended: not
    /// started, stopped, or ended by a fault (its signal is queued).
    bool runWorkerPassForTest();
#endif
signals:
    void restartRequested(const QString& reason, NereusSDR::RemoteAudioReceiver::Fault fault);
    void errorOccurred(const QString& reason, NereusSDR::RemoteAudioReceiver::Fault fault);
private:
    struct Private;
    // The receive worker's loop state and its one pass (in the .cpp).
    struct WorkerState;
    std::unique_ptr<Private> d;
};
} // namespace NereusSDR
