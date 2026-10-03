#pragma once
// no-port-check: NereusSDR-original deterministic speaker sink for remote audio tests.
#include "core/IAudioBus.h"
#include <QVector>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

// R-R3-23: the bus plays in the format it was opened with (48 kHz stereo
// float when never opened): its queue, clock, capacity and `heard` count
// frames of that rate and channel count. Its capacity is the ring a
// PortAudio output stream of that format has (PortAudioBus::
// outputRingSamples): 4800 frames at 48 kHz stereo.
class PacedAudioBus final : public NereusSDR::IAudioBus {
public:
    bool open(const NereusSDR::AudioFormat& f) override { format = f; active = true; return true; }
    void close() override { active = false; }
    bool isOpen() const override { return active; }
    qint64 push(const char* data, qint64 bytes) override {
        std::lock_guard<std::mutex> lock(mutex);
        const auto* samples = reinterpret_cast<const float*>(data);
        const int count = int(bytes / sizeof(float));
        const int channels = channelsLocked();
        if (int(queue.size()) - channels * playedAheadFramesLocked() + count
            > capacityFramesLocked() * channels) { return -1; }
        if (playClock) {
            // The device has taken past everything queued: it played silence
            // meanwhile, so these samples start at its next callback, not in
            // the past.
            const qint64 dry = takenFramesLocked() - playedFrames - qint64(queue.size()) / channels;
            if (dry > 0) {
                queue.insert(queue.end(), std::size_t(dry * channels), 0.0f);
                playedDryFrames += dry;
                dryEvents.push_back({dueFramesLocked(), dry});
            }
        }
        if (playedDryFramesAtFirstPush < 0) {
            playedDryFramesAtFirstPush = playedDryFrames;
            firstPushDueFrame = dueFramesLocked();
        }
        queue.insert(queue.end(), samples, samples + count);
        peakQueued.store(std::max(peakQueued.load(), int(queue.size()) / channels));
        return bytes;
    }
    qint64 pull(char*, qint64) override { return 0; }
    void flush() override { std::lock_guard<std::mutex> lock(mutex); queue.clear(); ++flushes; }
    std::optional<OutputPacing> outputPacing() const override {
        std::unique_lock<std::mutex> lock(mutex);
        ++outputPacingCalls;
        if (blockOutputPacingAfterCalls >= 0
            && outputPacingCalls > blockOutputPacingAfterCalls) {
            pacingGateEntered = true;
            pacingGateChanged.notify_all();
            pacingGateChanged.wait(lock, [this] { return releaseOutputPacingGate; });
        }
        if (!outputPacingAvailable) { return std::nullopt; }
        const int ahead = playedAheadFramesLocked();
        return OutputPacing{consumed + quint64(ahead),
                            std::max(0, int(queue.size()) / channelsLocked() - ahead),
                            capacityFramesLocked(), callbackFrames, deviceLatencyNs};
    }
    // R-R3-35 test device clock. From this call the device plays one frame
    // every 1/rate s of `clockNs` continuously (its opened rate, R-R3-23),
    // as hardware does, and it
    // takes frames from the queue the way the callback device it reports
    // does: `callbackFrames` at a time, each callback at the instant its
    // first frame starts to play (no device latency). So the queue and
    // consumed count it reports drop a callback at a time, following the
    // clock rather than the render() calls, and renderDue() moves exactly
    // the frames played so far into `heard`. Heard frame k (counted from
    // this call) played at playClockOriginNs() + k / rate, however late
    // the caller's timer runs. Set before any render.
    void setPlayClockForTesting(std::function<qint64()> clockNs)
    {
        std::lock_guard<std::mutex> lock(mutex);
        playClock = std::move(clockNs);
        playOriginNs = playClock();
        playedFrames = 0;
    }
    qint64 playClockOriginNs() const { std::lock_guard<std::mutex> lock(mutex); return playOriginNs; }
    // Frames the play clock found nothing to play for (the queue ran dry).
    qint64 playedDryFramesForTesting() const { std::lock_guard<std::mutex> lock(mutex); return playedDryFrames; }
    // R-R3-21: the dry frames counted by the first push that queued audio,
    // that is, all the silence played before playback started (-1 before
    // any push). Recorded under the same lock as the count, so no later
    // shortfall can fold into it.
    qint64 playedDryFramesAtFirstPushForTesting() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return playedDryFramesAtFirstPush;
    }
    // Each push that found the device had run dry: the play-clock frame of
    // that push and the frames it had played silent. For failure messages.
    struct DryEvent { qint64 atFrame; qint64 frames; };
    std::vector<DryEvent> dryEventsForTesting() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return dryEvents;
    }
    qint64 firstPushDueFrameForTesting() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return firstPushDueFrame;
    }
    // Renders every frame the play clock has reached; returns how many.
    int renderDue()
    {
        int frames = 0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!playClock) { return 0; }
            frames = int(dueFramesLocked() - playedFrames);
        }
        if (frames > 0) { render(frames); }
        return frames;
    }
    // Test-only worker gate. Configure it before beginRemotePlayback(); the
    // first pacing read belongs to that synchronous setup and the receiver
    // worker blocks on the following read. releaseOutputPacingGateForTesting
    // must run before a receiver stop joins that worker.
    void blockOutputPacingAfterCallsForTesting(int calls)
    {
        std::lock_guard<std::mutex> lock(mutex);
        blockOutputPacingAfterCalls = calls;
        releaseOutputPacingGate = false;
        pacingGateEntered = false;
    }
    // Re-arms the gate so the next pacing read, from whichever thread, blocks.
    void blockNextOutputPacingForTesting()
    {
        std::lock_guard<std::mutex> lock(mutex);
        blockOutputPacingAfterCalls = outputPacingCalls;
        releaseOutputPacingGate = false;
        pacingGateEntered = false;
    }
    bool waitForOutputPacingGateForTesting(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return pacingGateChanged.wait_for(lock, timeout, [this] { return pacingGateEntered; });
    }
    void releaseOutputPacingGateForTesting()
    {
        std::lock_guard<std::mutex> lock(mutex);
        releaseOutputPacingGate = true;
        pacingGateChanged.notify_all();
    }
    // Test-only device loss. A speaker that goes away stops reporting its
    // playback timing: while unavailable, outputPacing() answers nullopt, so
    // remote playback cannot begin and a playing receiver loses its device
    // clock through the real AudioEngine path. Callable from any thread.
    void setOutputPacingAvailableForTesting(bool available)
    {
        std::lock_guard<std::mutex> lock(mutex);
        outputPacingAvailable = available;
    }
    void render(int frames) {
        std::lock_guard<std::mutex> lock(mutex);
        for (int i = 0; i < frames * channelsLocked(); ++i) {
            heard.append(queue.empty() ? 0.0f : queue.front());
            if (!queue.empty()) { queue.pop_front(); }
        }
        consumed += frames;
        playedFrames += frames;
    }
    float rxLevel() const override { return 0; }
    float txLevel() const override { return 0; }
    QString backendName() const override { return QStringLiteral("PacedTest"); }
    NereusSDR::AudioFormat negotiatedFormat() const override { return format; }
    QVector<float> heard;
    // R-R3-49: written by the playback worker's push()/flush() under the
    // bus lock and read unlocked by the test's own thread, so atomic.
    std::atomic<int> flushes{0};
    std::atomic<int> peakQueued{0};
    int callbackFrames = 480;
    // R-R3-35: what this bus reports as its device latency (none: unknown).
    std::optional<qint64> deviceLatencyNs;
private:
    bool active = true;
    NereusSDR::AudioFormat format;
    mutable std::mutex mutex;
    mutable std::condition_variable pacingGateChanged;
    std::deque<float> queue;
    quint64 consumed = 0;
    mutable int outputPacingCalls = 0;
    int blockOutputPacingAfterCalls = -1;
    mutable bool pacingGateEntered = false;
    bool releaseOutputPacingGate = false;
    bool outputPacingAvailable = true;
    std::function<qint64()> playClock;
    qint64 playOriginNs = 0;
    qint64 playedFrames = 0;
    qint64 playedDryFrames = 0;
    qint64 playedDryFramesAtFirstPush = -1;
    qint64 firstPushDueFrame = -1;
    std::vector<DryEvent> dryEvents;
    qint64 dueFramesLocked() const
    {
        return playClock ? (playClock() - playOriginNs) * qint64(format.sampleRate)
                / 1'000'000'000
                         : 0;
    }
    int channelsLocked() const { return std::max(1, format.channels); }
    int capacityFramesLocked() const
    {
        const int ringSamples = std::max(4800 * 2, (format.sampleRate / 10) * channelsLocked());
        return ringSamples / channelsLocked();
    }
    // Frames the device has taken from the queue by now: every callback up
    // to and including the one that holds the frame now playing.
    qint64 takenFramesLocked() const
    {
        const qint64 perCallback = std::max(1, callbackFrames);
        return (dueFramesLocked() / perCallback + 1) * perCallback;
    }
    // Frames the device has taken that render() has not yet moved.
    int playedAheadFramesLocked() const
    {
        return playClock ? int(std::max<qint64>(0, takenFramesLocked() - playedFrames)) : 0;
    }
};
