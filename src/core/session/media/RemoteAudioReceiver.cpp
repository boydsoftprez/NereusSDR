// no-port-check: NereusSDR-original remote audio lifecycle and worker wiring.
#include "core/session/media/RemoteAudioReceiver.h"
#include "core/AudioEngine.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/session/media/AudioJitterBuffer.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteAudioRateMatcher.h"
#include "core/session/media/RtpReceptionStats.h"
#include <algorithm>
#include <cmath>
#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <numeric>
#include <thread>

namespace NereusSDR {
namespace {
// The default receiver clock: steady time since this process first read it,
// so values stay small enough to cross the control channel exactly.
qint64 defaultClockNs()
{
    static const std::chrono::steady_clock::time_point base = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - base).count();
}

// The packet shape each profile's context carries (R-R3-23).
constexpr qint64 kLosslessPacketDurationNs = qint64(PcmAudioCodecConfig::kPacketFrames)
    * 1'000'000'000 / PcmAudioCodecConfig::kSampleRate; // 4 ms
static_assert(kLosslessPacketDurationNs == 4'000'000, "a lossless packet is 4 ms");
static_assert(AudioJitterBuffer::kDefaultPacketFrames == OpusAudioCodecConfig::kFrameSamples,
              "the jitter queue's default shape is the Opus packet");
int packetFramesFor(RemoteAudioProfile profile)
{
    return profile == RemoteAudioProfile::Lossless ? PcmAudioCodecConfig::kPacketFrames
                                                   : AudioJitterBuffer::kDefaultPacketFrames;
}
qint64 packetDurationNsFor(RemoteAudioProfile profile)
{
    return profile == RemoteAudioProfile::Lossless ? kLosslessPacketDurationNs
                                                   : AudioJitterBuffer::kDefaultPacketDurationNs;
}

// The RTP boundary for one packet of the context's profile: what submit()
// needs before the packet may be queued.
struct AudioHeader {
    bool accepted = false;
    quint16 sequence = 0;
    quint32 timestamp = 0;
    qsizetype payloadBytes = 0;
};
AudioHeader inspectAudioRtp(const QByteArray& packet, quint32 ssrc, RemoteAudioProfile profile)
{
    AudioHeader header;
    if (profile == RemoteAudioProfile::Opus) {
        const auto opus = inspectOpusRtp(packet, ssrc);
        header.accepted = opus.status == OpusAudioCodecStatus::Accepted;
        header.sequence = opus.sequence;
        header.timestamp = opus.timestamp;
        header.payloadBytes = opus.payloadBytes;
        return header;
    }
    // Lossless: the payload type first, then the one L16 packet shape. The
    // payload's size is read from the header, not from a copy of it.
    if (audioRtpPayloadType(packet) != PcmAudioCodecConfig::kPayloadType) { return header; }
    AudioRtpView view;
    if (inspectAudioRtpHeader(packet, PcmAudioCodecConfig::kPayloadType, view)
            != OpusAudioCodecStatus::Accepted
        || view.ssrc != ssrc || view.payloadBytes != PcmAudioCodecConfig::kPayloadBytes) {
        return header;
    }
    header.accepted = true;
    header.sequence = view.sequence;
    header.timestamp = view.timestamp;
    header.payloadBytes = view.payloadBytes;
    return header;
}
}
namespace {
// One packet as submit() queued it for the worker.
struct ReceivedAudioPacket { QByteArray bytes; quint32 timestamp; qint64 arrival; quint16 sequence; };
}
// The receive worker's state: what its loop carries from one wake to the
// next, its setup and one wake of the loop (pass()). It is built, run and
// destroyed by whichever thread runs the passes: the worker thread, or in
// the test-only step mode the test's own. Nothing else touches it.
struct RemoteAudioReceiver::WorkerState {
    WorkerState(RemoteAudioReceiver* receiver, quint32 ssrc, quint32 firstTimestamp,
                quint64 generation, RemoteAudioProfile profile, AudioFormat speakerFormat);
    // The setup before the first wake. False: the worker ends (a fault was
    // reported).
    bool setup();
    // One wake of the worker's loop. waitForWork: first wait up to 2 ms for
    // work, as the worker thread does (the step mode does not wait). False:
    // the worker ends (stopped, or a fault was reported).
    bool pass(bool waitForWork);

    // The speaker queue the worker keeps: two blocks, or the device's
    // callback and one block when that is more.
    int speakerTargetFrames(int callbackFrames) const
    {
        return std::max(2 * blockFrames, callbackFrames + blockFrames);
    }
    // One packet to PCM: the profile's decoder for a present packet, the
    // Opus concealment or lossless silence for a missing one. Empty on
    // a decode failure.
    QVector<float> decodePacket(const QByteArray& packet);
    void publishSpeakerQueue(const std::optional<IAudioBus::OutputPacing>& pacing);
    RemoteAudioRateMatcherStats publishMatcherStats();
    void notify(const QString& reason, Fault fault, bool fatal = false);
    void trimStartBacklog(std::deque<ReceivedAudioPacket>& batch, bool backlog);
    void noteReleased(quint32 timestamp, bool concealed, qint64 atNs);
    void publishHold();

    RemoteAudioReceiver* q;
    Private* d;
    const quint32 ssrc;
    const quint32 firstTimestamp;
    const quint64 generation;
    const RemoteAudioProfile profile;
    const AudioFormat speakerFormat;
    // The run is created and destroyed on this worker, including every
    // decoder-error return. Its WDSP handles never cross to the GUI.
    std::unique_ptr<IRemotePcmWorkerStage::Run> stageRun;
    const bool lossless;
    const bool sinkMode;
    const int packetFrames;
    // R-R3-23: the speaker side runs at the device's rate. One worker
    // block is 10 ms of it (480 frames at 48 kHz, 441 at 44.1 kHz, 960
    // at 96 kHz), and every speaker-side size below is counted in such
    // blocks, so the queue and reserve keep their times at any rate.
    const int deviceRate;
    const int deviceChannels;
    const int blockFrames;
    // A 48 kHz packet once the matcher has made it at the device's rate,
    // rounded up.
    const int packetDeviceFrames;
    AudioJitterBuffer jitter;
    quint64 publishedTrimmed = 0;
    quint64 publishedSkipped = 0;
    quint64 publishedRewound = 0;
    // Only an Opus context needs the Opus decoder; a lossless packet is
    // read directly, and a lost one is silence.
    std::optional<OpusAudioDecoder> decoder;
    const QVector<float> silence;
    RemoteAudioRateMatcher matcher;
    // Named apart from publishMatcherStats()'s local `stats` below.
    RtpReceptionStats receptionStats;
    qint64 startedAt = 0;
    qint64 lastPacket = 0;
    qint64 previousArrival = 0;
    qint64 maxArrivalGap = 0;
    qint64 previousWake = 0;
    qint64 maxWakeGap = 0;
    quint64 accepted = 0, late = 0, invalid = 0, duplicate = 0;
    quint64 publishedUnderflows = 0, publishedOverflows = 0;
    std::optional<IAudioBus::OutputPacing> initialPacing;
    bool playing = false;
    // Connect-time start: nothing has been released for playback yet.
    // The receiver has no starting-fill constant of its own (the jitter
    // hold, AudioJitterBuffer::kHoldNs, is the start buffer), so a backlog
    // keeps only its newest packet. A burst within the arrival bound and
    // the jitter window is kept whole, as before.
    bool released = false;
    bool admittedAny = false;
    // R-R3-35: the end RTP time of the newest packet in the rate
    // matcher, for the playout point, and the newest release not yet
    // published. Both are published together once per wake, so the
    // points lock is never taken per packet (250 a second lossless).
    std::optional<quint32> pushedEnd;
    std::optional<RemoteAudioReleasePoint> unpublishedRelease;
    // Fixed delays a sample passes through after the matcher fill and
    // the speaker queue say it is heard: the codec's own delay (Opus
    // only) and the rate matcher's filter.
    const int codecDelayFrames;
    const int pipelineDelayFrames;
    quint64 lastDeviceFrames = 0;
    quint64 deviceConsumedBase = 0;
    // R-R3-43, PCM sink: frames handed to the sink, and whether the
    // stream has gone quiet (no admitted packet for 500 ms), so nothing
    // more is released until a packet arrives and restarts it.
    quint64 sinkFrames = 0;
    bool sinkIdle = false;
    quint64 telemetryDeviceFrames = 0;
    qint64 lastDeviceProgress = 0;
    // R-R3-21: when the speaker first took audio in this context. Until
    // it has, a stream that is playing is waiting on a device that is
    // still starting, not on the network, so the no-packet rule waits.
    std::optional<qint64> speakerStartedAt;
    // R-R3-21: the hold and the arrival bound it sets, as published.
    qint64 publishedHoldNs = -1;
};
struct RemoteAudioReceiver::Private {
    using Packet = ReceivedAudioPacket;
    AudioEngine* engine = nullptr; // owner stops/joins before engine destruction
    // R-R3-45: the output this receiver plays on.
    RemotePlaybackOutput output = RemotePlaybackOutput::Speakers;
    // R-R3-43: set only in the PCM-sink mode, which has no engine.
    RemoteAudioReceiver::PcmSink sink;
    std::shared_ptr<IRemotePcmWorkerStage> stage;
    // std::thread and a flag, not std::jthread: the libc++ in Xcode 16
    // (LLVM 19, the macos-15 CI runner) keeps jthread and stop_token
    // behind -fexperimental-library. stop() sets the flag and joins;
    // start() clears it before each new worker.
    std::thread worker;
    std::atomic<bool> stopWorker{false};
    // The worker's state, touched only by the thread that runs its passes
    // (see WorkerState): the worker thread, or the owner in step mode.
    std::optional<RemoteAudioReceiver::WorkerState> workerState;
    // Test-only step mode (owner thread): no worker thread; the owner runs
    // the passes through runWorkerPassForTest().
    bool stepMode = false;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Packet> incoming;
    std::atomic<bool> running{false};
    std::atomic<quint64> decoded{0};
    std::atomic<quint64> concealed{0};
    std::atomic<int> underflows{0};
    std::atomic<int> overflows{0};
    std::atomic<quint64> rejectedHeaders{0};
    std::atomic<quint64> lifetimeUnderflows{0};
    std::atomic<quint64> lifetimeOverflows{0};
    std::atomic<quint64> telemetryGeneration{0};
    std::atomic<quint64> telemetrySequence{0}; // owner lifecycle mutations
    std::atomic<quint64> accepted{0};
    std::atomic<quint64> late{0};
    std::atomic<quint64> invalid{0};
    std::atomic<quint64> duplicate{0};
    std::atomic<quint64> startDiscarded{0};
    std::atomic<quint64> receivedAudioPayloadBytes{0};
    std::atomic<quint64> deviceConsumedFrames{0};
    // -1 is deliberately distinct from a measured empty speaker queue.
    std::atomic<int> speakerQueuedFrames{-1};
    std::atomic<qint64> lastAdmittedPacketNs{0};
    std::atomic<qint64> lastDeviceProgressNs{0};
    std::atomic<bool> hasLastAdmittedPacket{false};
    std::atomic<bool> hasLastDeviceProgress{false};
    // RtpReceptionStats publication. reorderQueuedPackets uses the same -1
    // sentinel/live-gauge pattern as speakerQueuedFrames above.
    std::atomic<quint64> expectedPackets{0};
    std::atomic<quint64> missingPackets{0};
    std::atomic<bool> hasArrivalJitterMs{false};
    std::atomic<double> arrivalJitterMs{0.0};
    std::atomic<int> reorderQueuedPackets{-1};
    // R-R3-21: the jitter queue's adaptive hold, a live gauge like
    // reorderQueuedPackets (-1: not measured), and the counters of what
    // this context rode through on this computer (see the telemetry).
    std::atomic<qint64> jitterHoldNs{-1};
    std::atomic<quint64> burstDropped{0};
    std::atomic<quint64> streamGapReanchors{0};
    std::atomic<quint64> trimmed{0};
    std::atomic<quint64> skipped{0};
    std::atomic<quint64> rewound{0};
    std::atomic<qint64> maxWakeGapNs{0};
    std::atomic<quint64> linkInterruptions{0};
    // Rate matcher ratio, published by the playback loop (R-R3-07). Like
    // reorderQueuedPackets it is reported only for a running context.
    std::atomic<bool> hasDriftRatio{false};
    std::atomic<double> driftRatio{1.0};
    // R-R3-23: the speaker's rate for this context, which speakerQueuedFrames
    // counts. Set by start() before the worker runs.
    std::atomic<int> deviceRateHz{PcmAudioCodecConfig::kSampleRate};
    bool overflow = false; // under mutex
    // Until the first packet is released for playback, a full arrival queue
    // drops its oldest packet instead of raising overflow. startBacklog
    // records such a drop for the worker's next batch. Both under mutex.
    bool startPhase = false;
    bool startBacklog = false;
    // The context's profile and its arrival bound: the jitter window in
    // packets (8 Opus, 80 lossless), so the bound is the same 320 ms for
    // both. Set by start() before the worker runs; read by submit() only
    // while running. R-R3-21: the worker moves the bound with the jitter
    // window as the hold adapts.
    std::atomic<RemoteAudioProfile> profile{RemoteAudioProfile::Opus};
    std::atomic<int> arrivalBoundPackets{
        AudioJitterBuffer::windowPackets(AudioJitterBuffer::kDefaultPacketDurationNs)};
    std::atomic<qint64> packetDurationNs{AudioJitterBuffer::kDefaultPacketDurationNs};
    quint32 ssrc = 0;
    quint64 generation = 0; // owner thread only
    // R-R3-35. The clock is set once at construction. The playout and
    // release points are published by the worker and read by telemetry();
    // the worker is never the device callback, so a short lock is fine.
    RemoteAudioReceiver::Clock clock;
    qint64 now() const { return clock(); }
    std::mutex pointsMutex;
    std::optional<RemoteAudioPlayoutPoint> playout;
    std::optional<RemoteAudioReleasePoint> release;
    void clearPoints()
    {
        std::lock_guard<std::mutex> lock(pointsMutex);
        playout.reset();
        release.reset();
    }
};

namespace {
qint64 playoutDeviceRate(int deviceRateHz)
{
    return deviceRateHz > 0 ? qint64(deviceRateHz) : qint64(PcmAudioCodecConfig::kSampleRate);
}
}

qint64 RemoteAudioPlayoutPoint::playoutNs() const
{
    // Doubled frame counts keep half a callback exact. R-R3-23: device-side
    // frames run at deviceRateHz and the pipeline delay at 48 kHz, so both
    // are put over their rates' common multiple before the one division
    // (at 48 kHz the sum and the rounding are exactly the single-rate ones).
    const qint64 deviceRate = playoutDeviceRate(deviceRateHz);
    const qint64 streamRate = PcmAudioCodecConfig::kSampleRate;
    const qint64 common = std::gcd(deviceRate, streamRate);
    const qint64 deviceWeight = streamRate / common;
    const qint64 streamWeight = deviceRate / common;
    const qint64 deviceHalfFrames = 2 * (qint64(matcherFillFrames) + qint64(speakerQueuedFrames))
        + qint64(callbackFrames);
    const qint64 weightedHalfFrames = deviceHalfFrames * deviceWeight
        + 2 * qint64(pipelineDelayFrames) * streamWeight;
    return measuredNs + weightedHalfFrames * 1'000'000'000 / (2 * deviceRate * deviceWeight)
        + deviceLatencyNs.value_or(0);
}

qint64 RemoteAudioPlayoutPoint::matcherStretchNs() const
{
    if (!std::isfinite(matcherRatio) || matcherRatio <= 0.0 || matcherRatio == 1.0) {
        return 0;
    }
    // The play time ahead of measuredNs that the rate matcher made: all of
    // it but the codec's delay, which lies before the matcher.
    const qint64 madeNs = playoutNs() - measuredNs
        - qint64(codecDelayFrames) * 1'000'000'000 / PcmAudioCodecConfig::kSampleRate;
    return std::llround(double(std::max<qint64>(0, madeNs)) * (1.0 - 1.0 / matcherRatio));
}

qint64 RemoteAudioPlayoutPoint::accuracyNs() const
{
    const qint64 deviceRate = playoutDeviceRate(deviceRateHz);
    return (qint64(callbackFrames) * 1'000'000'000 + 2 * deviceRate - 1) / (2 * deviceRate)
        + (readWindowNs + 1) / 2;
}

RemoteAudioReceiver::RemoteAudioReceiver(AudioEngine* engine, QObject* parent, Clock clock)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->engine = engine;
    d->clock = clock ? std::move(clock) : Clock(defaultClockNs);
}
RemoteAudioReceiver::RemoteAudioReceiver(AudioEngine* engine, RemotePlaybackOutput output,
                                         QObject* parent, Clock clock)
    : RemoteAudioReceiver(engine, parent, std::move(clock))
{
    d->output = output;
}
RemotePlaybackOutput RemoteAudioReceiver::output() const { return d->output; }
RemoteAudioReceiver::RemoteAudioReceiver(PcmSinkMode mode, QObject* parent, Clock clock)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->sink = std::move(mode.sink);
    d->stage = std::move(mode.stage);
    d->clock = clock ? std::move(clock) : Clock(defaultClockNs);
}
bool RemoteAudioReceiver::isPcmSink() const { return bool(d->sink); }
qint64 RemoteAudioReceiver::nowNs() const { return d->now(); }
RemoteAudioReceiver::~RemoteAudioReceiver() { stop(); }
bool RemoteAudioReceiver::isRunning() const { return d->running.load(); }
RemoteAudioProfile RemoteAudioReceiver::profile() const { return d->profile.load(); }
quint64 RemoteAudioReceiver::decodedPackets() const { return d->decoded.load(); }
quint64 RemoteAudioReceiver::concealedPackets() const { return d->concealed.load(); }
int RemoteAudioReceiver::rateMatcherUnderflows() const { return d->underflows.load(); }
int RemoteAudioReceiver::rateMatcherOverflows() const { return d->overflows.load(); }

RemoteAudioReceiverTelemetry RemoteAudioReceiver::telemetry() const
{
    const auto ageMs = [this](qint64 eventNs) {
        return std::max<qint64>(0, (d->now() - eventNs) / 1'000'000);
    };
    // A lifecycle change can race a reader. The owner brackets stop and a
    // successful-start reset with an odd sequence. Bounded retries avoid
    // attaching a retired context's counters to the next context without
    // making packet submission, the worker, or the device callback wait.
    for (int attempt = 0; attempt < 3; ++attempt) {
        const quint64 sequence = d->telemetrySequence.load();
        if (sequence & 1) { continue; }
        const quint64 generation = d->telemetryGeneration.load();
        RemoteAudioReceiverTelemetry snapshot;
        snapshot.generation = generation;
        snapshot.running = d->running.load();
        snapshot.acceptedPackets = d->accepted.load();
        snapshot.decodedPackets = d->decoded.load();
        snapshot.concealedPackets = d->concealed.load();
        snapshot.latePackets = d->late.load();
        snapshot.invalidPackets = d->invalid.load();
        snapshot.duplicatePackets = d->duplicate.load();
        snapshot.rejectedHeaders = d->rejectedHeaders.load();
        snapshot.startDiscardedPackets = d->startDiscarded.load();
        snapshot.burstDroppedPackets = d->burstDropped.load();
        snapshot.streamGapReanchors = d->streamGapReanchors.load();
        snapshot.trimmedPackets = d->trimmed.load();
        snapshot.skippedIntervals = d->skipped.load();
        snapshot.rewoundIntervals = d->rewound.load();
        snapshot.burstDroppedAudioMs = double(snapshot.burstDroppedPackets)
            * (double(d->packetDurationNs.load()) / 1'000'000.0);
        snapshot.skippedAudioMs = double(snapshot.skippedIntervals)
            * (double(d->packetDurationNs.load()) / 1'000'000.0);
        snapshot.maxWorkerWakeGapMs = double(d->maxWakeGapNs.load()) / 1'000'000.0;
        snapshot.linkInterruptions = d->linkInterruptions.load();
        snapshot.receivedAudioPayloadBytes = d->receivedAudioPayloadBytes.load();
        snapshot.deviceConsumedFrames = d->deviceConsumedFrames.load();
        snapshot.underflows = d->underflows.load();
        snapshot.overflows = d->overflows.load();
        snapshot.lifetimeUnderflows = d->lifetimeUnderflows.load();
        snapshot.lifetimeOverflows = d->lifetimeOverflows.load();
        if (d->hasLastAdmittedPacket.load()) {
            snapshot.lastAdmittedPacketAgeMs = ageMs(d->lastAdmittedPacketNs.load());
        }
        if (d->hasLastDeviceProgress.load()) {
            snapshot.lastDeviceProgressAgeMs = ageMs(d->lastDeviceProgressNs.load());
        }
        snapshot.expectedPackets = d->expectedPackets.load();
        snapshot.missingPackets = d->missingPackets.load();
        if (d->hasArrivalJitterMs.load()) {
            snapshot.arrivalJitterMs = d->arrivalJitterMs.load();
        }
        if (snapshot.running) {
            const int queuedFrames = d->speakerQueuedFrames.load();
            if (queuedFrames >= 0) {
                snapshot.speakerQueuedMs = double(queuedFrames)
                    / (double(d->deviceRateHz.load()) / 1000.0);
            }
            const int reorderQueued = d->reorderQueuedPackets.load();
            if (reorderQueued >= 0) {
                snapshot.reorderQueuedMs = double(reorderQueued)
                    * (double(d->packetDurationNs.load()) / 1'000'000.0);
            }
            const qint64 holdNs = d->jitterHoldNs.load();
            if (holdNs >= 0) {
                snapshot.jitterHoldMs = double(holdNs) / 1'000'000.0;
            }
            if (d->hasDriftRatio.load()) {
                snapshot.driftRatio = d->driftRatio.load();
            }
            std::lock_guard<std::mutex> lock(d->pointsMutex);
            snapshot.playout = d->playout;
            snapshot.release = d->release;
        }
        if (d->telemetrySequence.load() == sequence) {
            return snapshot;
        }
    }
    // A snapshot sampled in the middle of an owner lifecycle transition is
    // intentionally unavailable instead of combining observations from two
    // contexts. The next 1 Hz collection tick obtains the settled snapshot.
    RemoteAudioReceiverTelemetry unavailable;
    unavailable.generation = d->telemetryGeneration.load();
    return unavailable;
}

void RemoteAudioReceiver::stop()
{
    d->telemetrySequence.fetch_add(1);
    d->running.store(false);
    d->speakerQueuedFrames.store(-1);
    d->reorderQueuedPackets.store(-1);
    d->jitterHoldNs.store(-1);
    ++d->generation;
    if (d->worker.joinable()) {
        d->stopWorker.store(true);
        d->wake.notify_one();
        d->worker.join();
    }
    // Step mode: the state ends here, on the owner thread that ran it (the
    // worker thread has already ended its own).
    d->workerState.reset();
    // After the join, so a worker's last publication cannot outlive stop().
    d->hasDriftRatio.store(false);
    d->clearPoints();
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->incoming.clear();
        d->overflow = false;
        d->startPhase = false;
        d->startBacklog = false;
    }
    if (d->engine) { d->engine->endRemotePlayback(d->output); }
    d->telemetrySequence.fetch_add(1);
}

bool RemoteAudioReceiver::start(quint32 ssrc, quint32 firstTimestamp, RemoteAudioProfile profile)
{
    stop();
    QString error;
    // R-R3-43: a PCM sink needs no speaker.
    if (!d->sink && (!d->engine || !d->engine->beginRemotePlayback(&error, d->output))) {
        emit errorOccurred(error.isEmpty()
                               ? (d->output == RemotePlaybackOutput::Headphones
                                      ? QStringLiteral("Headphones playback is unavailable")
                                      : QStringLiteral("Speaker playback is unavailable"))
                               : error,
                            Fault::SpeakerOpenFailed);
        return false;
    }
    // R-R3-23: the speaker's rate and channel count, as begin accepted them.
    // A PCM sink takes the stream's own 48 kHz stereo.
    AudioFormat speakerFormat;
    if (!d->sink) {
        if (const auto accepted = d->engine->remotePlaybackFormat(d->output)) {
            speakerFormat = *accepted;
        }
    }
    d->telemetrySequence.fetch_add(1);
    d->ssrc = ssrc;
    d->profile.store(profile);
    d->packetDurationNs.store(packetDurationNsFor(profile));
    d->arrivalBoundPackets.store(AudioJitterBuffer::windowPackets(packetDurationNsFor(profile)));
    d->decoded.store(0);
    d->concealed.store(0);
    d->underflows.store(0);
    d->overflows.store(0);
    d->rejectedHeaders.store(0);
    d->accepted.store(0);
    d->late.store(0);
    d->invalid.store(0);
    d->duplicate.store(0);
    d->startDiscarded.store(0);
    d->receivedAudioPayloadBytes.store(0);
    d->deviceConsumedFrames.store(0);
    d->speakerQueuedFrames.store(-1);
    d->hasLastAdmittedPacket.store(false);
    d->hasLastDeviceProgress.store(false);
    d->lastAdmittedPacketNs.store(0);
    d->lastDeviceProgressNs.store(0);
    d->expectedPackets.store(0);
    d->missingPackets.store(0);
    d->hasArrivalJitterMs.store(false);
    d->arrivalJitterMs.store(0.0);
    d->reorderQueuedPackets.store(-1);
    d->jitterHoldNs.store(-1);
    d->burstDropped.store(0);
    d->streamGapReanchors.store(0);
    d->trimmed.store(0);
    d->skipped.store(0);
    d->rewound.store(0);
    d->maxWakeGapNs.store(0);
    d->linkInterruptions.store(0);
    d->hasDriftRatio.store(false);
    d->driftRatio.store(1.0);
    d->deviceRateHz.store(speakerFormat.sampleRate);
    d->clearPoints();
    d->telemetryGeneration.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->startPhase = true;
        d->startBacklog = false;
    }
    d->running.store(true);
    d->telemetrySequence.fetch_add(1);
    const quint64 generation = d->generation;
    d->stopWorker.store(false);
    if (d->stepMode) {
        // Test-only step mode: no worker thread. The setup runs here, and
        // each runWorkerPassForTest() runs one pass, on the caller's thread.
        d->workerState.emplace(this, ssrc, firstTimestamp, generation, profile, speakerFormat);
        if (!d->workerState->setup()) { d->workerState.reset(); }
        return true;
    }
    d->worker = std::thread([this, ssrc, firstTimestamp, generation, profile,
                             speakerFormat] {
        // R-R3-21: this thread feeds the speaker's 20 ms queue, so it runs
        // in the latency-critical class, as the local DSP feeders do
        // (RxDspWorker, TxWorkerThread). At the default class a busy Mac
        // left it unscheduled for over 20 ms mid-stream and the speaker
        // ran dry (tst_remote_audio_receiver playsOnEverySpeakerFormat).
        // A per-thread attribute: it ends with the thread. Said in the log
        // once per process, not once per context.
        static std::atomic<bool> priorityLogged{false};
        elevateLatencyCriticalThreadPriority(!priorityLogged.exchange(true));
        // The worker's state lives, runs and ends on this thread.
        d->workerState.emplace(this, ssrc, firstTimestamp, generation, profile, speakerFormat);
        if (d->workerState->setup()) {
            while (d->workerState->pass(true)) {
            }
        }
        d->workerState.reset();
    });
    return true;
}

RemoteAudioReceiver::WorkerState::WorkerState(RemoteAudioReceiver* receiver, quint32 ssrcIn,
                                              quint32 firstTimestampIn, quint64 generationIn,
                                              RemoteAudioProfile profileIn,
                                              AudioFormat speakerFormatIn)
    : q(receiver), d(receiver->d.get()), ssrc(ssrcIn), firstTimestamp(firstTimestampIn),
      generation(generationIn), profile(profileIn), speakerFormat(speakerFormatIn),
      lossless(profile == RemoteAudioProfile::Lossless), sinkMode(bool(d->sink)),
      packetFrames(packetFramesFor(profile)), deviceRate(speakerFormat.sampleRate),
      deviceChannels(speakerFormat.channels), blockFrames(std::max(1, deviceRate / 100)),
      packetDeviceFrames(int((qint64(packetFrames) * deviceRate
                              + PcmAudioCodecConfig::kSampleRate - 1)
                             / PcmAudioCodecConfig::kSampleRate)),
      jitter(packetFrames, packetDurationNsFor(profile)),
      silence(packetFrames * PcmAudioCodecConfig::kChannels, 0.0f),
      codecDelayFrames(lossless ? 0 : opusCodecDelayFrames()),
      pipelineDelayFrames(RemoteAudioRateMatcher::filterDelayFrames(deviceRate)
                          + codecDelayFrames)
{
}

QVector<float> RemoteAudioReceiver::WorkerState::decodePacket(const QByteArray& packet)
{
    if (lossless) {
        if (packet.isEmpty()) { return silence; }
        const PcmRtpDecodeResult audio = decodeL16Rtp(packet, ssrc);
        return audio.status == OpusAudioCodecStatus::Accepted
            ? audio.pcmInterleaved : QVector<float>{};
    }
    const auto audio = packet.isEmpty() ? decoder->decodeMissing()
                                        : decoder->decodeRtp(packet, ssrc);
    return audio.status == OpusAudioCodecStatus::Accepted
            || audio.status == OpusAudioCodecStatus::Concealed
        ? audio.pcmInterleaved : QVector<float>{};
}

void RemoteAudioReceiver::WorkerState::publishSpeakerQueue(
    const std::optional<IAudioBus::OutputPacing>& pacing)
{
    d->speakerQueuedFrames.store(pacing ? pacing->queuedFrames : -1);
}

RemoteAudioRateMatcherStats RemoteAudioReceiver::WorkerState::publishMatcherStats()
{
    const auto stats = matcher.stats();
    const quint64 underflows = std::max(0, stats.underflows);
    const quint64 overflows = std::max(0, stats.overflows);
    d->underflows.store(stats.underflows);
    d->overflows.store(stats.overflows);
    if (underflows > publishedUnderflows) {
        d->lifetimeUnderflows.fetch_add(underflows - publishedUnderflows);
        publishedUnderflows = underflows;
    }
    if (overflows > publishedOverflows) {
        d->lifetimeOverflows.fetch_add(overflows - publishedOverflows);
        publishedOverflows = overflows;
    }
    return stats;
}

void RemoteAudioReceiver::WorkerState::notify(const QString& reason, Fault fault, bool fatal)
{
    d->speakerQueuedFrames.store(-1);
    d->reorderQueuedPackets.store(-1);
    d->jitterHoldNs.store(-1);
    d->running.store(false);
    const auto stats = publishMatcherStats();
    const auto pacing = d->engine ? d->engine->remotePlaybackPacing(d->output)
                                  : std::optional<IAudioBus::OutputPacing>{};
    const qint64 now = d->now();
    // Bounded, restart-only diagnostics distinguish capture/network
    // loss from a stalled consumer without logging media or secrets.
    const QString detail = reason + QStringLiteral(
        " [ageMs=%1 accepted=%2 decoded=%3 plc=%4 late=%5 invalid=%6 duplicate=%7"
        " rejectedHeaders=%8 lastPacketMs=%9 maxArrivalGapMs=%10 maxWakeGapMs=%11"
        " jitterPackets=%12 ratio=%13 fill=%14 callback=%15 deviceQueued=%16"
        " startDiscarded=%17 holdMs=%18 interruptions=%19]")
        .arg((now - startedAt) / 1'000'000).arg(accepted)
        .arg(d->decoded.load()).arg(d->concealed.load()).arg(late).arg(invalid)
        .arg(duplicate).arg(d->rejectedHeaders.load())
        .arg((now - lastPacket) / 1'000'000).arg(maxArrivalGap / 1'000'000)
        .arg(maxWakeGap / 1'000'000).arg(jitter.queuedPackets())
        .arg(stats.currentRatio, 0, 'f', 7).arg(stats.ringFillFrames)
        .arg(pacing ? pacing->callbackFrames : 0)
        .arg(pacing ? pacing->queuedFrames : 0)
        .arg(d->startDiscarded.load())
        .arg(jitter.holdNs() / 1'000'000).arg(d->linkInterruptions.load());
    QMetaObject::invokeMethod(q, [q = q, generation = generation, detail, fault, fatal] {
        if (q->d->generation != generation) { return; }
        if (fatal) { emit q->errorOccurred(detail, fault); }
        else { emit q->restartRequested(detail, fault); }
    }, Qt::QueuedConnection);
}

void RemoteAudioReceiver::WorkerState::trimStartBacklog(std::deque<ReceivedAudioPacket>& batch,
                                                        bool backlog)
{
    // Packets behind the anchor or off the packet grid keep their
    // normal Late/Invalid classification in insert().
    const auto aheadOf = [&](const ReceivedAudioPacket& packet) -> std::optional<qint32> {
        const qint32 delta = std::bit_cast<qint32>(
            quint32(packet.timestamp - jitter.nextTimestamp()));
        if (delta < 0 || delta % jitter.packetFrames() != 0) {
            return std::nullopt;
        }
        return delta / jitter.packetFrames();
    };
    std::optional<std::size_t> newest, oldest;
    qint32 newestAhead = -1, oldestAhead = 0;
    std::size_t playable = 0;
    for (std::size_t i = 0; i < batch.size(); ++i) {
        const auto ahead = aheadOf(batch[i]);
        if (!ahead) { continue; }
        ++playable;
        if (*ahead > newestAhead) { newest = i; newestAhead = *ahead; }
        if (!oldest || *ahead < oldestAhead) { oldest = i; oldestAhead = *ahead; }
    }
    if (!newest) { return; }
    if (backlog || newestAhead >= jitter.maxPackets()) {
        // Keep only the newest packet and restart the reorder window
        // and its loss accounting from it: the dropped packets were
        // never heard, so they are neither a gap nor a stream break.
        // Packets already in the jitter queue were counted as
        // admitted; they are discarded here instead, so move them to
        // the start-discard count and "admitted" keeps only packets
        // that can still play (fix wave M1).
        const auto queued = quint64(jitter.queuedPackets());
        accepted -= std::min(accepted, queued);
        d->accepted.fetch_sub(std::min(d->accepted.load(), queued));
        d->startDiscarded.fetch_add(queued + (playable - 1));
        const quint32 anchor = batch[*newest].timestamp;
        std::deque<ReceivedAudioPacket> kept;
        for (std::size_t i = 0; i < batch.size(); ++i) {
            if (i == *newest || !aheadOf(batch[i])) { kept.push_back(std::move(batch[i])); }
        }
        batch.swap(kept);
        jitter.reset(anchor);
        receptionStats.reset();
    } else if (!admittedAny) {
        jitter.reset(batch[*oldest].timestamp);
    }
}

void RemoteAudioReceiver::WorkerState::noteReleased(quint32 timestamp, bool concealed, qint64 atNs)
{
    pushedEnd = timestamp + quint32(packetFrames);
    if (!concealed) {
        unpublishedRelease = RemoteAudioReleasePoint{*pushedEnd, atNs};
    }
}

void RemoteAudioReceiver::WorkerState::publishHold()
{
    // Shedding happens as packets are released; published here,
    // at most a wake later.
    if (jitter.trimmedPackets() != publishedTrimmed) {
        d->trimmed.fetch_add(jitter.trimmedPackets() - publishedTrimmed);
        publishedTrimmed = jitter.trimmedPackets();
    }
    if (jitter.rewoundIntervals() != publishedRewound) {
        d->rewound.fetch_add(jitter.rewoundIntervals() - publishedRewound);
        publishedRewound = jitter.rewoundIntervals();
    }
    if (jitter.skippedIntervals() != publishedSkipped) {
        d->skipped.fetch_add(jitter.skippedIntervals() - publishedSkipped);
        publishedSkipped = jitter.skippedIntervals();
    }
    if (jitter.holdNs() == publishedHoldNs) { return; }
    publishedHoldNs = jitter.holdNs();
    d->jitterHoldNs.store(publishedHoldNs);
    d->arrivalBoundPackets.store(jitter.maxPackets());
}

bool RemoteAudioReceiver::WorkerState::setup()
{
    stageRun = d->stage ? d->stage->createRun() : nullptr;
    if (stageRun) { stageRun->reconcile(); }
    jitter.reset(firstTimestamp);
    // R-R3-21: the PCM sink is paced by the hold alone, so a late packet
    // there only stays late: no rewind and no deeper hold, which would
    // hand an app a silent pause and then a burst mid-transmission.
    jitter.setAdaptive(!sinkMode);
    if (!lossless) { decoder.emplace(); }
    startedAt = d->now();
    lastPacket = startedAt;
    previousWake = startedAt;
    // R-R3-43: a PCM sink has no speaker to pace it and no rate matcher.
    initialPacing = sinkMode ? std::optional<IAudioBus::OutputPacing>{}
                                        : d->engine->remotePlaybackPacing(d->output);
    if (sinkMode) {
        if (decoder && !decoder->isReady()) {
            notify(QStringLiteral("Could not initialize the remote audio decoder"),
                   Fault::DecoderUnavailable, true);
            return false;
        }
    } else {
        if (!initialPacing) {
            notify(QStringLiteral("Speaker device timing is unavailable"),
                   Fault::SpeakerTimingUnavailable, true);
            return false;
        }
        publishSpeakerQueue(initialPacing);
        const int initialTarget = speakerTargetFrames(initialPacing->callbackFrames);
        if (initialTarget + blockFrames > initialPacing->capacityFrames) {
            notify(QStringLiteral("Speaker callback exceeds remote playback capacity"),
                   Fault::SpeakerCallbackTooLarge, true);
            return false;
        }
        // WDSP starts half full. Preserve the default 90 ms reserve when a
        // larger selected device quantum transfers extra frames into the
        // speaker ring at startup. This is queue sizing, not a change to the
        // resampler feedback or its continuous interpolation. R-R3-23:
        // in device-rate blocks, so 18 blocks is the same 180 ms ring at
        // any rate (8640 frames at 48 kHz).
        const int deviceHighWater = ((initialTarget + blockFrames - 1) / blockFrames)
            * blockFrames;
        const int matchRingFrames = 18 * blockFrames
            + 2 * std::max(0, deviceHighWater - 2 * blockFrames);
        if ((decoder && !decoder->isReady())
            || !matcher.configure(packetFrames, blockFrames, matchRingFrames, deviceRate)) {
            notify(QStringLiteral("Could not initialize the remote audio decoder or rate matcher"),
                   Fault::DecoderUnavailable, true);
            return false;
        }
    }
    deviceConsumedBase = initialPacing ? initialPacing->consumedFrames : 0;
    telemetryDeviceFrames = deviceConsumedBase;
    lastDeviceProgress = lastPacket;
    publishHold();
    return true;
}

bool RemoteAudioReceiver::WorkerState::pass(bool waitForWork)
{
    std::deque<ReceivedAudioPacket> incoming;
    bool overflow = false;
    bool startBacklog = false;
    {
        std::unique_lock<std::mutex> lock(d->mutex);
        if (waitForWork && (!stageRun || !stageRun->hasRunnableWork())) {
            d->wake.wait_for(lock, std::chrono::milliseconds(2), [this] {
                return d->stopWorker.load() || !d->incoming.empty() || d->overflow;
            });
        }
        incoming.swap(d->incoming);
        overflow = d->overflow;
        d->overflow = false;
        startBacklog = d->startBacklog;
        d->startBacklog = false;
    }
    if (d->stopWorker.load()) { return false; }
    publishHold();
    const qint64 wakeAt = d->now();
    maxWakeGap = std::max(maxWakeGap, wakeAt - previousWake);
    d->maxWakeGapNs.store(maxWakeGap);
    previousWake = wakeAt;
    // R-R3-21: a network interruption this wake (an arrival burst,
    // a stream gap, or packets that came after their intervals were
    // concealed), counted once for the lossless link trial.
    bool interrupted = false;
    if (overflow) {
        // R-R3-21: more packets arrived at once than the arrival
        // bound holds, and the newest were dropped at the door
        // (counted in submit()). The link is alive, so this is
        // concealed loss, not a reason to ask the Core for a fresh
        // context: the batch plays on and the jitter queue conceals
        // what was dropped. Only a true outage (no packets at all)
        // or a real fault restarts.
        interrupted = true;
    }
    if (sinkIdle && !incoming.empty()) {
        // R-R3-43: the quiet stream starts again on the earliest
        // packet of this batch (serial order), as a fresh context
        // would start on its first timestamp. The quiet time was
        // heard as nothing, so it is neither a gap nor a restart.
        quint32 anchor = incoming.front().timestamp;
        for (const auto& packet : incoming) {
            if (std::bit_cast<qint32>(quint32(packet.timestamp - anchor)) < 0) {
                anchor = packet.timestamp;
            }
        }
        jitter.reset(anchor);
        sinkIdle = false;
    }
    if (!released && !incoming.empty()) {
        trimStartBacklog(incoming, startBacklog);
    }
    for (const auto& packet : incoming) {
        if (previousArrival != 0) {
            maxArrivalGap = std::max(maxArrivalGap, packet.arrival - previousArrival);
        }
        previousArrival = packet.arrival;
        auto admitted = jitter.insert(packet.bytes, packet.timestamp, packet.arrival);
        if (admitted == AudioJitterBuffer::Admission::OutsideWindow) {
            // R-R3-21: a stream gap re-anchors here: the head moves
            // forward just far enough for this packet to fit the
            // window, dropping only the oldest queued audio. The
            // Core is not asked for a fresh context; its stream is
            // alive.
            ++d->streamGapReanchors;
            interrupted = true;
            jitter.advanceToFit(packet.timestamp);
            admitted = jitter.insert(packet.bytes, packet.timestamp, packet.arrival);
        }
        switch (admitted) {
        case AudioJitterBuffer::Admission::Accepted:
            admittedAny = true;
            ++accepted;
            ++d->accepted;
            lastPacket = packet.arrival;
            d->lastAdmittedPacketNs.store(packet.arrival);
            d->hasLastAdmittedPacket.store(true);
            receptionStats.observe(packet.sequence, packet.timestamp, packet.arrival);
            break;
        case AudioJitterBuffer::Admission::Rewound:
            // R-R3-21: late, after its interval was concealed; the
            // queue rewound to play it, so it is admitted as well
            // as counted late.
            interrupted = true;
            admittedAny = true;
            ++accepted;
            ++d->accepted;
            ++late;
            ++d->late;
            lastPacket = packet.arrival;
            d->lastAdmittedPacketNs.store(packet.arrival);
            d->hasLastAdmittedPacket.store(true);
            receptionStats.observe(packet.sequence, packet.timestamp, packet.arrival);
            break;
        case AudioJitterBuffer::Admission::LateConcealed:
            // R-R3-21: its interval was concealed: the link stalled.
            interrupted = true;
            [[fallthrough]];
        case AudioJitterBuffer::Admission::Late:
            ++late;
            ++d->late;
            // R-R3-21: a late packet proves the link is alive. Too
            // late to rewind for, it still deepened the hold (a
            // late copy of a packet that played changes nothing).
            lastPacket = packet.arrival;
            receptionStats.observe(packet.sequence, packet.timestamp, packet.arrival);
            break;
        case AudioJitterBuffer::Admission::Invalid:
            ++invalid;
            ++d->invalid;
            break;
        case AudioJitterBuffer::Admission::Duplicate:
            ++duplicate;
            ++d->duplicate;
            break;
        case AudioJitterBuffer::Admission::OutsideWindow: break;
        }
    }
    publishHold();
    if (interrupted) { ++d->linkInterruptions; }
    d->expectedPackets.store(receptionStats.expectedPackets());
    d->missingPackets.store(receptionStats.missingPackets());
    if (const auto measuredJitterMs = receptionStats.jitterMs()) {
        d->arrivalJitterMs.store(*measuredJitterMs);
        d->hasArrivalJitterMs.store(true);
    }
    const qint64 now = d->now();
    if (sinkMode) {
        // R-R3-43: released by the jitter hold alone, so the Core's
        // RTP clock paces the sink; at most the window per wake.
        if (!sinkIdle && now - lastPacket > 500'000'000) {
            sinkIdle = true;
        }
        for (int i = 0; !sinkIdle && i < jitter.maxPackets(); ++i) {
            const auto frame = jitter.takeReady(now);
            if (!frame) { break; }
            const QVector<float> audio = decodePacket(frame->packet);
            if (audio.isEmpty()) {
                notify(QStringLiteral("Remote audio decode failed"), Fault::DecodeFailed);
                return false;
            }
            const int frames = int(audio.size() / PcmAudioCodecConfig::kChannels);
            d->sink(audio.constData(), frames);
            if (stageRun) { stageRun->appendPcm(audio.constData(), frames); }
            if (frame->concealed()) { ++d->concealed; }
            else { ++d->decoded; }
            noteReleased(frame->timestamp, frame->concealed(), now);
            sinkFrames += quint64(frames);
            d->deviceConsumedFrames.store(sinkFrames);
            d->lastDeviceProgressNs.store(now);
            d->hasLastDeviceProgress.store(true);
            if (!released) {
                released = true;
                std::lock_guard<std::mutex> lock(d->mutex);
                d->startPhase = false;
            }
        }
        d->reorderQueuedPackets.store(jitter.queuedPackets());
        if (unpublishedRelease) {
            std::lock_guard<std::mutex> lock(d->pointsMutex);
            d->release = *unpublishedRelease;
            unpublishedRelease.reset();
        }
        if (stageRun) {
            stageRun->reconcile();
            stageRun->serviceUntil(
                std::chrono::steady_clock::now() + std::chrono::microseconds(1800),
                32);
        }
        return true;
    }
    // A true outage: nothing, late packets included, for 500 ms.
    // R-R3-21: once audio is playing the clock starts only when the
    // speaker has taken its first frames, so a device that is slow
    // to start cannot look like a dead link. Before anything plays
    // it runs from the start, so a context that never receives a
    // packet still asks for a fresh one.
    if ((!playing || speakerStartedAt)
        && now - std::max(lastPacket, speakerStartedAt.value_or(lastPacket))
            > 500'000'000) {
        notify(QStringLiteral("Remote audio had no admitted/playable packets for 500 ms"),
               Fault::NoPackets);
        return false;
    }
    // R-R3-21: the rate matcher's fill above its working level
    // (half its ring, where WDSP rmatch starts and steers) is delay
    // too; shedding as the hold eases counts it.
    {
        const auto fill = matcher.stats();
        jitter.setDownstreamExcessNs(
            qint64(fill.ringFillFrames - fill.ringCapacityFrames / 2) * 1'000'000'000
            / deviceRate);
    }
    // Every wake, even one whose release the full matcher holds
    // back: that is when a backlog stands.
    jitter.tick(now);
    // R-R3-21: a shed Opus interval is decoded (a missing one
    // concealed) and its audio thrown away, so the decoder's state
    // (its prediction across frames) runs on as if nothing had been
    // skipped. That keeps the decoder continuous; it does not smooth
    // the splice itself. Lossless has no decoder state.
    for (const QByteArray& shed : jitter.takeShedPackets()) {
        if (!decoder) { continue; }
        if (shed.isEmpty()) { decoder->decodeMissing(); }
        else { decoder->decodeRtp(shed, ssrc); }
    }
    // At most the bounded jitter window per wake, never an
    // unbounded catch-up burst.
    for (int i = 0; i < jitter.maxPackets(); ++i) {
        // A network burst stays in the bounded jitter queue until
        // the device has drained room. Reserve two input packets, a
        // conservative bound above WDSP's maximum resampled block;
        // never let its drop-oldest overflow repair handle a burst.
        const auto room = matcher.stats();
        if (room.ringCapacityFrames - room.ringFillFrames < 2 * packetDeviceFrames) { break; }
        const auto frame = jitter.takeReady(now);
        if (!frame) { break; }
        const QVector<float> audio = decodePacket(frame->packet);
        if (audio.isEmpty() || !matcher.push(audio)) {
            notify(QStringLiteral("Remote audio decode failed"), Fault::DecodeFailed);
            return false;
        }
        if (frame->concealed()) { ++d->concealed; }
        else { ++d->decoded; }
        noteReleased(frame->timestamp, frame->concealed(), now);
        playing = true;
        if (!released) {
            // Playback has begun; the normal overflow rule applies.
            released = true;
            std::lock_guard<std::mutex> lock(d->mutex);
            d->startPhase = false;
        }
    }
    d->reorderQueuedPackets.store(jitter.queuedPackets());
    if (!playing) {
        publishSpeakerQueue(d->engine->remotePlaybackPacing(d->output));
        return true;
    }
    auto pacing = d->engine->remotePlaybackPacing(d->output);
    if (!pacing) {
        notify(QStringLiteral("Speaker device timing became unavailable"),
               Fault::SpeakerTimingUnavailable, true);
        return false;
    }
    if (!speakerStartedAt && pacing->consumedFrames > deviceConsumedBase) {
        speakerStartedAt = now;
    }
    if (pacing->consumedFrames != lastDeviceFrames) {
        lastDeviceFrames = pacing->consumedFrames;
        lastDeviceProgress = now;
        if (pacing->consumedFrames != telemetryDeviceFrames) {
            telemetryDeviceFrames = pacing->consumedFrames;
            d->deviceConsumedFrames.store(telemetryDeviceFrames - deviceConsumedBase);
            d->lastDeviceProgressNs.store(now);
            d->hasLastDeviceProgress.store(true);
        }
    } else if (now - lastDeviceProgress > 500'000'000) {
        notify(QStringLiteral("Speaker device stopped consuming audio"),
               Fault::SpeakerStalled, true);
        return false;
    }
    // Cover the selected callback quantum plus one worker block.
    // Default 128-frame callbacks need 20-30 ms queued; a user-selected
    // 2048-frame callback needs more so one callback cannot exhaust it.
    const int targetFrames = speakerTargetFrames(pacing->callbackFrames);
    if (targetFrames + blockFrames > pacing->capacityFrames) {
        notify(QStringLiteral("Speaker callback exceeds remote playback capacity"),
               Fault::SpeakerCallbackTooLarge, true);
        return false;
    }
    // The timer only wakes us. Actual device queue consumption is the
    // output clock; at most one bounded ring can be replenished here.
    for (int i = 0; i < pacing->capacityFrames / blockFrames
         && pacing->queuedFrames < targetFrames; ++i) {
        // A packet may already be admitted yet remain behind its
        // per-arrival reorder hold while the independently clocked
        // speaker consumes the final usable matcher block. There is
        // no ordering benefit in retaining the exact expected packet
        // at that point. One 40 ms Opus packet always covers an
        // output block; 4 ms lossless packets may take several, each
        // the exact expected packet, bounded by the jitter window.
        // R-R3-21: with the expected packet not here but a later one
        // queued (a hole), conceal it now. Waiting for its deadline
        // (which a deepened hold pushes later) would underflow the
        // rate matcher, and that costs a fresh context. An empty
        // queue keeps its normal loss deadline.
        for (int early = 0; early < jitter.maxPackets()
             && !matcher.canTakeWithoutUnderflow(); ++early) {
            const qint64 concealAt = d->now();
            auto next = jitter.takeExpectedPresentEarly(concealAt);
            if (!next) { next = jitter.concealExpectedNow(concealAt); }
            if (!next) { break; }
            const QVector<float> audio = decodePacket(next->packet);
            if (audio.isEmpty() || !matcher.push(audio)) {
                notify(QStringLiteral("Remote audio decode failed"), Fault::DecodeFailed);
                return false;
            }
            if (next->concealed()) { ++d->concealed; }
            else { ++d->decoded; }
            noteReleased(next->timestamp, next->concealed(), concealAt);
        }
        QVector<float> pcm = matcher.take();
        if (pcm.size() != 2 * blockFrames) { pcm.clear(); }
        if (deviceChannels == 1 && !pcm.isEmpty()) {
            // R-R3-23: a mono speaker hears both channels, mixed
            // as (left + right) / 2 like the VAX microphone's mix.
            QVector<float> mono(blockFrames);
            for (int frame = 0; frame < blockFrames; ++frame) {
                mono[frame] = 0.5f * (pcm[2 * frame] + pcm[2 * frame + 1]);
            }
            pcm = std::move(mono);
        }
        if (pcm.isEmpty() || !d->engine->writeRemotePlayback(pcm, d->output)) {
            notify(QStringLiteral("Could not write remote audio to the speaker device"),
                   Fault::SpeakerWriteFailed, true);
            return false;
        }
        pacing = d->engine->remotePlaybackPacing(d->output);
        if (!pacing) { break; }
    }
    const auto stats = publishMatcherStats();
    // The ratio the fault text reports, published every playback
    // iteration so a soak can record it without a fault (R-R3-07).
    // Only once rmatch measures it: before its 3.0 s startup delay
    // the ratio is the initial 1.0, which would read as a measured
    // zero drift, so drift stays absent until then (review minor 3).
    if (stats.controlActive) {
        d->driftRatio.store(stats.currentRatio);
        d->hasDriftRatio.store(true);
    }
    // The final pacing read in every playback iteration observes the
    // queue after bounded replenishment. It remains worker-only and
    // does not participate in device callback scheduling.
    // R-R3-35: the clock is read on both sides of the queue read, so
    // a worker preempted between them widens the accuracy instead of
    // moving the figure.
    const qint64 readStart = d->now();
    const auto finalPacing = d->engine->remotePlaybackPacing(d->output);
    const qint64 readEnd = d->now();
    publishSpeakerQueue(finalPacing);
    // R-R3-35: the newest matched sample is heard after the matcher
    // fill and the speaker queue have played, then the device.
    if ((finalPacing && pushedEnd) || unpublishedRelease) {
        std::lock_guard<std::mutex> lock(d->pointsMutex);
        if (finalPacing && pushedEnd) {
            RemoteAudioPlayoutPoint point;
            point.rtpTimestamp = *pushedEnd;
            point.measuredNs = readStart + (readEnd - readStart) / 2;
            point.matcherFillFrames = std::max(0, stats.ringFillFrames);
            point.speakerQueuedFrames = std::max(0, finalPacing->queuedFrames);
            point.deviceLatencyNs = finalPacing->deviceLatencyNs;
            point.deviceRateHz = deviceRate;
            point.pipelineDelayFrames = pipelineDelayFrames;
            point.callbackFrames = std::max(0, finalPacing->callbackFrames);
            point.readWindowNs = readEnd - readStart;
            // Follow-up item 5: while rmatch corrects, its ratio
            // is part of the delay (see matcherStretchNs).
            point.codecDelayFrames = codecDelayFrames;
            point.matcherRatio = stats.currentRatio;
            d->playout = point;
        }
        if (unpublishedRelease) { d->release = *unpublishedRelease; }
    }
    unpublishedRelease.reset();
    if (stats.underflows || stats.overflows) {
        notify(QStringLiteral("Remote audio exceeded its continuous clock buffer (%1 underflows, %2 overflows)")
            .arg(stats.underflows).arg(stats.overflows), Fault::ClockBuffer);
        return false;
    }
    return true;
}


#ifdef NEREUS_BUILD_TESTS
void RemoteAudioReceiver::setStepModeForTest(bool on) { d->stepMode = on; }
bool RemoteAudioReceiver::runWorkerPassForTest()
{
    if (!d->workerState) { return false; }
    if (!d->workerState->pass(false)) {
        d->workerState.reset();
        return false;
    }
    return true;
}
#endif

void RemoteAudioReceiver::submit(const QByteArray& packet)
{
    if (!isRunning()) { return; }
    const AudioHeader header = inspectAudioRtp(packet, d->ssrc, d->profile.load());
    if (!header.accepted) {
        ++d->rejectedHeaders;
        return;
    }
    // Traffic is measured at the valid RTP/profile boundary, before the
    // bounded process-local arrival queue can discard a burst. A duplicate
    // is still received traffic even if jitter ordering later rejects it.
    d->receivedAudioPayloadBytes.fetch_add(quint64(header.payloadBytes));
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        // Bounded in time: the jitter window's 320 ms of packets.
        const bool full = d->incoming.size()
            >= static_cast<std::size_t>(d->arrivalBoundPackets.load());
        if (full && !d->startPhase) {
            // R-R3-21: dropped at the door, counted per packet.
            d->overflow = true;
            ++d->burstDropped;
        }
        else {
            if (full) {
                // A connect-time backlog: nothing has been heard yet, so the
                // oldest queued packet is the least useful one to keep.
                d->incoming.pop_front();
                ++d->startDiscarded;
                d->startBacklog = true;
            }
            d->incoming.push_back({packet, header.timestamp, d->now(), header.sequence});
        }
    }
    d->wake.notify_one();
}
} // namespace NereusSDR
