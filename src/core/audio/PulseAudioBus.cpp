// =================================================================
// src/core/audio/PulseAudioBus.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See PulseAudioBus.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-07, R-AUD-15, R-AUD-32).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-10: setClockMatchWritePacket(): a writer of whole packets
//               (remote playback) tells the bus's clock matcher its packet
//               (R-AUD-15, bench regression). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/PulseAudioBus.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/DeviceSampleFormat.h"

#include <pulse/pulseaudio.h>
#include <pulse/rtclock.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace NereusSDR {

namespace {

// libpulse's largest channel count (PA_CHANNELS_MAX, pulse/sample.h).
constexpr int kPulseMaxChannels = int(PA_CHANNELS_MAX);

constexpr std::uint32_t kPulseAttrDefault = static_cast<std::uint32_t>(-1);

const QString kBarredError = QStringLiteral("Audio devices are not opened in a test run");
const QString kNotRunningError = QStringLiteral("PulseAudio is not running");

std::uint32_t frameBytesOf(int channels)
{
    return std::uint32_t(sizeof(float)) * std::uint32_t(std::max(1, channels));
}

bool toPaChannelMap(const PulseStreamConfig& config, pa_channel_map* map)
{
    if (!config.channelMap.isEmpty() && int(config.channelMap.size()) == config.channels) {
        pa_channel_map_init(map);
        map->channels = std::uint8_t(config.channels);
        for (int i = 0; i < config.channels; ++i) {
            const QByteArray name = config.channelMap.at(i).toUtf8();
            const pa_channel_position_t position = pa_channel_position_from_string(name.constData());
            if (position == PA_CHANNEL_POSITION_INVALID) {
                return false;
            }
            map->map[i] = position;
        }
        return pa_channel_map_valid(map) != 0;
    }
    return pa_channel_map_init_auto(map, unsigned(config.channels), PA_CHANNEL_MAP_DEFAULT) != nullptr;
}

pa_stream_flags_t streamFlags(const PulseStreamConfig& config)
{
    unsigned flags = PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE
                     | PA_STREAM_INTERPOLATE_TIMING;
    if (config.noRemix) {
        flags |= PA_STREAM_NO_REMIX_CHANNELS;
    }
    if (config.dontMove) {
        flags |= PA_STREAM_DONT_MOVE;
    }
    return static_cast<pa_stream_flags_t>(flags);
}

std::int64_t latencyNsOf(pa_stream* stream)
{
    pa_usec_t usec = 0;
    int negative = 0;
    if (pa_stream_get_latency(stream, &usec, &negative) != 0) {
        return -1;
    }
    return negative != 0 ? 0 : std::int64_t(usec) * 1000;
}

// What a stream's callbacks read: owned by the stream, handed to libpulse
// as userdata, destroyed only after the stream is disconnected under the
// mainloop lock.
struct PulseStreamContext {
    pa_threaded_mainloop* mainloop = nullptr;
    PulseStreamConfig config;
    std::uint32_t frameBytes = 0;
    std::vector<float> scratch;            // 2 * kPulseChunkFrames, allocated before open
    MatcherReader reader;                  // an output's
    MicChannelPick pick = MicChannelPick::Left;   // an input's
    IAudioInputSink* sink = nullptr;       // an input's
    std::shared_ptr<PulseLossWatch> watch;
    bool ready = false;                    // under the mainloop lock
    std::atomic<std::int64_t> latencyNs{-1};
    std::atomic<std::uint64_t> consumedFrames{0};
};

void onStreamState(pa_stream* stream, void* userdata)
{
    auto* context = static_cast<PulseStreamContext*>(userdata);
    const pa_stream_state_t state = pa_stream_get_state(stream);
    if ((state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED) && context->ready) {
        // The mainloop's thread, not a device callback: the watch only posts.
        context->ready = false;
        context->watch->post(AudioStreamEvent::Kind::DeviceLost,
                             QStringLiteral("The device went away"));
    }
    pa_threaded_mainloop_signal(context->mainloop, 0);
}

// The device callback: libpulse holds its mainloop lock; we take none and
// allocate nothing.
void onStreamWrite(pa_stream* stream, size_t nbytes, void* userdata)
{
    auto* context = static_cast<PulseStreamContext*>(userdata);
    const std::uint32_t frameBytes = context->frameBytes;
    while (nbytes >= frameBytes) {
        void* data = nullptr;
        size_t size = nbytes;
        if (pa_stream_begin_write(stream, &data, &size) < 0 || data == nullptr) {
            break;
        }
        const size_t frames = size / frameBytes;
        if (frames == 0) {
            pa_stream_cancel_write(stream);
            break;
        }
        pulseFillFromMatcher(context->reader, context->scratch.data(), kPulseChunkFrames,
                             static_cast<float*>(data), int(frames), context->config.channels,
                             context->config.pair);
        const size_t bytes = frames * frameBytes;
        if (pa_stream_write(stream, data, bytes, nullptr, 0, PA_SEEK_RELATIVE) < 0) {
            break;
        }
        context->consumedFrames.fetch_add(frames, std::memory_order_relaxed);
        nbytes -= std::min(nbytes, bytes);
    }
    context->latencyNs.store(latencyNsOf(stream), std::memory_order_relaxed);
}

// The input's device callback: the pair, as the pick picks it, to the sink.
void onStreamRead(pa_stream* stream, size_t /*nbytes*/, void* userdata)
{
    auto* context = static_cast<PulseStreamContext*>(userdata);
    const std::uint32_t frameBytes = context->frameBytes;
    const int rate = context->config.rate;
    const std::int64_t delayNs = std::max<std::int64_t>(0, latencyNsOf(stream));
    context->latencyNs.store(delayNs, std::memory_order_relaxed);
    while (pa_stream_readable_size(stream) > 0) {
        const void* data = nullptr;
        size_t size = 0;
        if (pa_stream_peek(stream, &data, &size) < 0 || size == 0) {
            return;
        }
        if (data != nullptr && rate > 0 && context->sink != nullptr) {
            const int frames = int(size / frameBytes);
            const double nsPerFrame = 1e9 / double(rate);
            // Frame 0 was captured the device delay plus this block's length ago.
            const std::int64_t frame0Ns = audioProbeNowNs() - delayNs
                                        - std::int64_t(double(frames) * nsPerFrame);
            const auto* src = static_cast<const std::uint8_t*>(data);
            float* const stereo = context->scratch.data();
            int done = 0;
            while (done < frames) {
                const int n = std::min(frames - done, kPulseChunkFrames);
                readDeviceToStereo(src + size_t(done) * frameBytes, nullptr, true,
                                   DeviceSampleFormat::Float32, context->config.channels,
                                   context->config.pair, context->pick, n, stereo);
                context->sink->onInput(stereo, n, rate,
                                       frame0Ns + std::int64_t(double(done) * nsPerFrame));
                done += n;
            }
        }
        pa_stream_drop(stream);   // a hole (data null) is dropped as well
    }
}

struct WaitTimer {
    pa_threaded_mainloop* mainloop = nullptr;
    bool fired = false;
};

void onWaitTimer(pa_mainloop_api* /*api*/, pa_time_event* /*event*/, const struct timeval* /*tv*/,
                 void* userdata)
{
    auto* timer = static_cast<WaitTimer*>(userdata);
    timer->fired = true;
    pa_threaded_mainloop_signal(timer->mainloop, 0);
}

// Opens one stream of either direction; the caller holds no lock.
class PulseStreamCore {
public:
    PulseStreamCore(std::shared_ptr<PulseStreamHost> host, PulseStreamConfig config)
        : m_host(std::move(host))
        , m_config(std::move(config))
        , m_watch(std::make_shared<PulseLossWatch>())
    {
    }
    ~PulseStreamCore() { close(); }

    PulseStreamCore(const PulseStreamCore&) = delete;
    PulseStreamCore& operator=(const PulseStreamCore&) = delete;

    const PulseStreamConfig& config() const { return m_config; }
    const std::shared_ptr<PulseLossWatch>& watch() const { return m_watch; }
    bool isOpen() const { return m_stream != nullptr; }
    PulseStreamContext* context() const { return m_context.get(); }

    // `prepare` fills the callbacks' context (the reader, the sink) before
    // the stream exists.  An empty string on success, else the reason.
    QString open(const std::function<void(PulseStreamContext&)>& prepare)
    {
        close();
        if (audioDevicesBarredForTestRun()) {
            return kBarredError;
        }
        if (!m_host || !m_host->running() || m_host->mainloop() == nullptr
            || m_host->context() == nullptr) {
            return kNotRunningError;
        }
        auto context = std::make_unique<PulseStreamContext>();
        context->mainloop = m_host->mainloop();
        context->config = m_config;
        context->frameBytes = frameBytesOf(m_config.channels);
        context->scratch.assign(size_t(2 * kPulseChunkFrames), 0.0f);
        context->watch = m_watch;
        if (prepare) {
            prepare(*context);
        }

        pa_sample_spec spec{};
        spec.format = PA_SAMPLE_FLOAT32NE;
        spec.rate = std::uint32_t(m_config.rate);
        spec.channels = std::uint8_t(m_config.channels);
        pa_channel_map map{};
        if (pa_sample_spec_valid(&spec) == 0 || !toPaChannelMap(m_config, &map)) {
            return QStringLiteral("PulseAudio cannot open %1 channels at %2 Hz")
                .arg(m_config.channels)
                .arg(m_config.rate);
        }
        const bool output = m_config.direction == AudioDeviceDirection::Output;
        const QByteArray device = m_config.deviceName.toUtf8();
        const char* deviceName = m_config.deviceName.isEmpty() ? nullptr : device.constData();

        pa_threaded_mainloop* mainloop = m_host->mainloop();
        pa_threaded_mainloop_lock(mainloop);
        pa_stream* stream = pa_stream_new(m_host->context(),
                                          output ? "NereusSDR output" : "NereusSDR input",
                                          &spec, &map);
        if (stream == nullptr) {
            pa_threaded_mainloop_unlock(mainloop);
            return QStringLiteral("PulseAudio did not make the stream");
        }
        PulseStreamContext* raw = context.get();
        pa_stream_set_state_callback(stream, &onStreamState, raw);
        pa_buffer_attr attr{};
        attr.maxlength = kPulseAttrDefault;
        attr.prebuf = kPulseAttrDefault;
        attr.tlength = kPulseAttrDefault;
        attr.minreq = kPulseAttrDefault;
        attr.fragsize = kPulseAttrDefault;
        int rc = 0;
        if (output) {
            attr.tlength = m_config.tlengthBytes;
            attr.minreq = m_config.minreqBytes;
            pa_stream_set_write_callback(stream, &onStreamWrite, raw);
            rc = pa_stream_connect_playback(stream, deviceName, &attr, streamFlags(m_config),
                                            nullptr, nullptr);
        } else {
            attr.fragsize = m_config.fragsizeBytes;
            pa_stream_set_read_callback(stream, &onStreamRead, raw);
            rc = pa_stream_connect_record(stream, deviceName, &attr, streamFlags(m_config));
        }
        bool ready = false;
        if (rc == 0) {
            pulseWait(mainloop, m_host->context(), kPulseConnectWaitMs, [stream] {
                const pa_stream_state_t state = pa_stream_get_state(stream);
                return state != PA_STREAM_CREATING && state != PA_STREAM_UNCONNECTED;
            });
            ready = pa_stream_get_state(stream) == PA_STREAM_READY;
        }
        if (!ready) {
            detach(stream);
            pa_threaded_mainloop_unlock(mainloop);
            return QStringLiteral("PulseAudio did not open the stream");
        }
        raw->ready = true;
        raw->latencyNs.store(latencyNsOf(stream), std::memory_order_relaxed);
        m_stream = stream;
        m_context = std::move(context);
        pa_threaded_mainloop_unlock(mainloop);
        return {};
    }

    void close()
    {
        if (m_stream == nullptr) {
            m_context.reset();
            return;
        }
        pa_threaded_mainloop* mainloop = m_host->mainloop();
        const bool inLoop = pa_threaded_mainloop_in_thread(mainloop) != 0;
        if (!inLoop) {
            pa_threaded_mainloop_lock(mainloop);
        }
        detach(m_stream);
        m_stream = nullptr;
        if (!inLoop) {
            pa_threaded_mainloop_unlock(mainloop);
        }
        // No callback runs after the stream is disconnected under the lock.
        m_context.reset();
    }

    std::int64_t latencyNs() const
    {
        return m_context ? m_context->latencyNs.load(std::memory_order_relaxed) : -1;
    }

private:
    // The mainloop lock is held.
    static void detach(pa_stream* stream)
    {
        pa_stream_set_state_callback(stream, nullptr, nullptr);
        pa_stream_set_write_callback(stream, nullptr, nullptr);
        pa_stream_set_read_callback(stream, nullptr, nullptr);
        const pa_stream_state_t state = pa_stream_get_state(stream);
        if (state == PA_STREAM_READY || state == PA_STREAM_CREATING) {
            pa_stream_disconnect(stream);
        }
        pa_stream_unref(stream);
    }

    std::shared_ptr<PulseStreamHost> m_host;
    PulseStreamConfig m_config;
    std::shared_ptr<PulseLossWatch> m_watch;
    std::unique_ptr<PulseStreamContext> m_context;
    pa_stream* m_stream = nullptr;
};

// ---------------------------------------------------------------------------
// The output: a DeviceRateMatcher the stream's write callback reads.
// ---------------------------------------------------------------------------
class PulseAudioOutputBus final : public IAudioBus {
public:
    PulseAudioOutputBus(std::shared_ptr<PulseStreamHost> host, PulseStreamConfig config, int delayMs)
        : m_core(std::move(host), std::move(config))
        , m_delayMs(delayMs)
    {
    }
    ~PulseAudioOutputBus() override { close(); }

    PulseAudioOutputBus(const PulseAudioOutputBus&) = delete;
    PulseAudioOutputBus& operator=(const PulseAudioOutputBus&) = delete;

    bool open(const AudioFormat& format) override
    {
        close();
        const PulseStreamConfig& config = m_core.config();
        DeviceRateMatcher::Config matcherConfig;
        matcherConfig.inRate = 48000;
        matcherConfig.outRate = config.rate;
        matcherConfig.callbackFrames = config.bufferFrames;
        matcherConfig.delayMs = m_delayMs;
        std::unique_ptr<DeviceRateMatcher> matcher;
        if (!audioDevicesBarredForTestRun()) {
            matcher = std::make_unique<DeviceRateMatcher>(matcherConfig);
            if (!matcher->valid()) {
                m_err = QStringLiteral("Clock matcher cannot run at %1 Hz").arg(config.rate);
                return false;
            }
        }
        DeviceRateMatcher* raw = matcher.get();
        const QString error = m_core.open([raw](PulseStreamContext& context) {
            if (raw != nullptr) {
                context.reader = raw->makeReader();
            }
        });
        if (!error.isEmpty()) {
            m_err = error;
            return false;
        }
        m_matcher = std::move(matcher);
        m_open.store(true, std::memory_order_release);
        m_format = format;
        m_format.sampleRate = config.rate;
        m_format.channels = 2;
        m_format.sample = AudioFormat::Sample::Float32;
        m_err.clear();
        return true;
    }

    void close() override
    {
        m_open.store(false, std::memory_order_release);
        m_core.close();   // the reader goes with the stream's context
        m_matcher.reset();
    }

    bool isOpen() const override { return m_open.load(std::memory_order_acquire); }

    qint64 push(const char* data, qint64 bytes) override
    {
        if (!m_matcher || data == nullptr || bytes <= 0) {
            return 0;
        }
        const int frames = int(bytes / qint64(2 * sizeof(float)));
        if (frames <= 0) {
            return 0;
        }
        const auto* in = reinterpret_cast<const float*>(data);
        float peak = 0.0f;
        for (int i = 0; i < frames * 2; ++i) {
            peak = std::max(peak, std::abs(in[i]));
        }
        m_matcher->write(in, frames, audioProbeNowNs());
        m_rxLevel.store(peak, std::memory_order_release);
        return bytes;
    }

    qint64 pull(char*, qint64) override { return 0; }

    void flush() override
    {
        if (m_matcher) {
            m_matcher->requestFlush();
        }
    }

    std::optional<OutputPacing> outputPacing() const override
    {
        if (!m_matcher) {
            return std::nullopt;
        }
        const MatcherRingHeader* ring = m_matcher->ring();
        OutputPacing pacing;
        pacing.consumedFrames = ring->requested.load(std::memory_order_acquire);
        pacing.queuedFrames = std::max(0, int(std::lround(m_matcher->fillFrames())));
        pacing.capacityFrames = int(ring->rsizeFrames.load(std::memory_order_acquire));
        pacing.callbackFrames = m_core.config().bufferFrames;
        const std::int64_t latencyNs = m_core.latencyNs();
        if (latencyNs > 0) {
            pacing.deviceLatencyNs = qint64(latencyNs);
        }
        return pacing;
    }

    float rxLevel() const override { return m_rxLevel.load(std::memory_order_acquire); }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("PulseAudio"); }
    AudioFormat negotiatedFormat() const override { return m_format; }
    QString errorString() const override { return m_err; }

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_core.watch()->mutex);
        m_core.watch()->sink = std::move(sink);
    }

    // Design choice 11: the buffer is the tlength asked for, the latency
    // the one pa_stream_get_latency reports.
    AudioDelayParts delayParts() const override
    {
        const int rate = m_core.config().rate;
        if (!m_matcher || rate <= 0) {
            return {};
        }
        const double bufferMs = 1000.0 * double(m_core.config().bufferFrames) / double(rate);
        const std::int64_t latencyNs = m_core.latencyNs();
        const double latencyMs = latencyNs > 0 ? double(latencyNs) / 1e6 : 0.0;
        return m_matcher->delayParts(bufferMs, latencyMs);
    }

    bool takesStereoMix() const override { return true; }

    std::optional<DeviceRateMatcherStats> matcherStats() const override
    {
        if (!m_matcher) {
            return std::nullopt;
        }
        return m_matcher->stats();
    }

    void restartClockMatch() override
    {
        if (m_matcher) {
            m_matcher->requestRestart();
        }
    }

    void setClockMatchWritePacket(int frames, bool waited) override
    {
        if (m_matcher) {
            m_matcher->setWritePacketFrames(frames, waited);
        }
    }

    void requestFadeOut() override
    {
        if (PulseStreamContext* context = m_core.context()) {
            context->reader.requestFadeOut();
        }
    }

    bool fadedOut() const override
    {
        PulseStreamContext* context = m_core.context();
        return context ? context->reader.fadedOut() : true;
    }

private:
    std::unique_ptr<DeviceRateMatcher> m_matcher;
    PulseStreamCore m_core;   // after the matcher: destroyed first
    int m_delayMs = 0;
    AudioFormat m_format;
    QString m_err;
    std::atomic<bool> m_open{false};
    std::atomic<float> m_rxLevel{0.0f};
};

// ---------------------------------------------------------------------------
// The input: the stream's read callback hands the pair to the sink.
// ---------------------------------------------------------------------------
class PulseAudioInputStream final : public IAudioInputStream {
public:
    PulseAudioInputStream(std::shared_ptr<PulseStreamHost> host, PulseStreamConfig config,
                          MicChannelPick pick, IAudioInputSink* sink)
        : m_core(std::move(host), std::move(config))
        , m_pick(pick)
        , m_sink(sink)
    {
    }
    ~PulseAudioInputStream() override { close(); }

    PulseAudioInputStream(const PulseAudioInputStream&) = delete;
    PulseAudioInputStream& operator=(const PulseAudioInputStream&) = delete;

    bool open() override
    {
        close();
        if (m_sink == nullptr && !audioDevicesBarredForTestRun()) {
            m_err = QStringLiteral("No input sink");
            return false;
        }
        const MicChannelPick pick = m_pick;
        IAudioInputSink* sink = m_sink;
        const QString error = m_core.open([pick, sink](PulseStreamContext& context) {
            context.pick = pick;
            context.sink = sink;
        });
        if (!error.isEmpty()) {
            m_err = error;
            return false;
        }
        m_open.store(true, std::memory_order_release);
        m_err.clear();
        return true;
    }

    void close() override
    {
        m_open.store(false, std::memory_order_release);
        m_core.close();
    }

    bool isOpen() const override { return m_open.load(std::memory_order_acquire); }
    QString errorString() const override { return m_err; }
    int sampleRate() const override { return m_core.config().rate; }

    std::optional<std::int64_t> inputLatencyNs() const override
    {
        const std::int64_t latencyNs = m_core.latencyNs();
        if (latencyNs < 0) {
            return std::nullopt;
        }
        return latencyNs;
    }

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_core.watch()->mutex);
        m_core.watch()->sink = std::move(sink);
    }

private:
    PulseStreamCore m_core;
    MicChannelPick m_pick;
    IAudioInputSink* m_sink;
    QString m_err;
    std::atomic<bool> m_open{false};
};

} // namespace

// ---------------------------------------------------------------------------
// The portable parts.
// ---------------------------------------------------------------------------
PulseStreamConfig pulseStreamConfig(const PulseDeviceRecord& device, const AudioStreamRequest& request)
{
    PulseStreamConfig config;
    config.direction = request.direction;
    config.deviceName = device.name;
    config.dontMove = !device.name.isEmpty();
    config.rate = request.sampleRate > 0 ? request.sampleRate : 48000;
    config.bufferFrames = request.bufferFrames > 0 ? request.bufferFrames : kPulseDefaultBufferFrames;
    const int mapped = int(device.channelMap.size());
    if (mapped > 2 && mapped <= kPulseMaxChannels) {
        // R-AUD-07: every channel of the interface, in its own map, with no
        // remix, so the pair is the interface's own channels.
        config.channels = mapped;
        config.channelMap = device.channelMap;
        config.noRemix = true;
    } else {
        config.channels = mapped == 1 ? 1 : 2;
    }
    const std::uint32_t bytes = std::uint32_t(config.bufferFrames) * frameBytesOf(config.channels);
    if (config.direction == AudioDeviceDirection::Output) {
        config.tlengthBytes = bytes;
        config.minreqBytes = bytes;
    } else {
        config.fragsizeBytes = bytes;
    }
    config.pair = request.pair;
    // A pair past the stream's channels plays on the first pair.
    if (config.pair.firstChannel < 1
        || config.pair.firstChannel + std::max(1, config.pair.channelCount) - 1 > config.channels) {
        config.pair = AudioChannelPair{1, config.channels == 1 ? 1 : 2};
    }
    return config;
}

void pulseFillFromMatcher(MatcherReader& reader, float* scratch, int scratchFrames,
                          float* dst, int frames, int channels, AudioChannelPair pair)
{
    if (dst == nullptr || frames <= 0 || channels <= 0) {
        return;
    }
    if (!reader.valid() || scratch == nullptr || scratchFrames <= 0) {
        std::memset(dst, 0, sizeof(float) * size_t(frames) * size_t(channels));
        return;
    }
    int done = 0;
    while (done < frames) {
        const int n = std::min(frames - done, scratchFrames);
        reader.read(scratch, n);
        writeStereoToDevice(scratch, n, dst + std::ptrdiff_t(done) * channels,
                            DeviceSampleFormat::Float32, channels, pair, true, nullptr);
        done += n;
    }
}

void PulseLossWatch::post(AudioStreamEvent::Kind kind, const QString& detail)
{
    std::function<void(const AudioStreamEvent&)> s;
    {
        std::lock_guard<std::mutex> lock(mutex);
        s = sink;
    }
    if (s) {
        AudioStreamEvent event;
        event.kind = kind;
        event.detail = detail;
        s(event);
    }
}

bool pulseWait(pa_threaded_mainloop* mainloop, pa_context* context, int timeoutMs,
               const std::function<bool()>& done)
{
    if (done()) {
        return true;
    }
    if (mainloop == nullptr || context == nullptr) {
        return false;
    }
    WaitTimer timer;
    timer.mainloop = mainloop;
    pa_time_event* event = pa_context_rttime_new(
        context, pa_rtclock_now() + pa_usec_t(std::max(0, timeoutMs)) * PA_USEC_PER_MSEC,
        &onWaitTimer, &timer);
    if (event == nullptr) {
        return done();   // no timer: never wait unbounded
    }
    while (!done() && !timer.fired) {
        pa_threaded_mainloop_wait(mainloop);
    }
    pa_threaded_mainloop_get_api(mainloop)->time_free(event);
    return done();
}

std::unique_ptr<IAudioBus> makePulseOutputBus(std::shared_ptr<PulseStreamHost> host,
                                              PulseStreamConfig config, int delayMs)
{
    return std::make_unique<PulseAudioOutputBus>(std::move(host), std::move(config), delayMs);
}

std::unique_ptr<IAudioInputStream> makePulseInputStream(std::shared_ptr<PulseStreamHost> host,
                                                        PulseStreamConfig config,
                                                        MicChannelPick pick,
                                                        IAudioInputSink* sink)
{
    return std::make_unique<PulseAudioInputStream>(std::move(host), std::move(config), pick, sink);
}

} // namespace NereusSDR
