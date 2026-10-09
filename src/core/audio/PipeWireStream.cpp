// =================================================================
// src/core/audio/PipeWireStream.cpp  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Claude Code.
//   2026-09-23: R-R3-44: output counters and isStreaming(). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-44 fix wave: an output cycle fills and counts the
//                 frames the graph asked for (pw_buffer::requested), not
//                 the whole buffer. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09: native audio plan Task 10 (R-AUD-07, R-AUD-15): the
//                 matcher output and input sink modes, audio.position and
//                 stream.dont-remix from StreamConfig, the graph's quantum
//                 from the position io and the device delay from
//                 pw_stream_get_time_n. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE
#include "core/audio/PipeWireStream.h"

#include <QLoggingCategory>
#include <pipewire/keys.h>

#include <spa/param/audio/type-info.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#include "core/audio/AudioDelayProbe.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/PipeWireOutputFrames.h"
#include "core/audio/PipeWireThreadLoop.h"

Q_DECLARE_LOGGING_CATEGORY(lcPw)

namespace NereusSDR {

namespace {

// The SPA channel for a position's short name ("FL", "AUX3"), from SPA's
// own table; SPA_AUDIO_CHANNEL_UNKNOWN for a name it does not hold.
uint32_t spaChannelForName(const QString& name)
{
    const QByteArray wanted = name.trimmed().toUtf8();
    for (const spa_type_info* info = spa_type_audio_channel; info->name != nullptr; ++info) {
        const char* shortName = std::strrchr(info->name, ':');
        shortName = shortName ? shortName + 1 : info->name;
        if (wanted == shortName) {
            return info->type;
        }
    }
    return SPA_AUDIO_CHANNEL_UNKNOWN;
}

} // namespace

void pipeWireFillFromMatcher(MatcherReader& reader, float* scratch, int scratchFrames,
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

// ---------------------------------------------------------------------------
// configToProperties — pure, unit-testable (no daemon required).
// ---------------------------------------------------------------------------
pw_properties* configToProperties(const StreamConfig& cfg)
{
    // For OUTPUT-direction Audio/Source streams (our VAX virtual sources
    // that consumer apps capture FROM), media.category must be "Capture"
    // — the value is from the consumer's perspective, not ours. Setting
    // "Playback" produces a contradictory node (Audio/Source class +
    // Playback category) that the ALSA-compat bridge refuses to expose
    // as an arecord-listable device, which is what Qt's audio backend
    // (and therefore WSJT-X / fldigi / VARA) enumerates.
    const bool isVirtualSource =
        (cfg.direction == StreamConfig::Output) &&
        (cfg.mediaClass == QStringLiteral("Audio/Source"));
    const char* mediaCategory =
        isVirtualSource                              ? "Capture" :
        (cfg.direction == StreamConfig::Output)      ? "Playback" :
                                                       "Capture";

    pw_properties* p = pw_properties_new(
        PW_KEY_NODE_NAME,        cfg.nodeName.toUtf8().constData(),
        PW_KEY_NODE_DESCRIPTION, cfg.nodeDescription.toUtf8().constData(),
        PW_KEY_MEDIA_TYPE,       "Audio",
        PW_KEY_MEDIA_CATEGORY,   mediaCategory,
        PW_KEY_MEDIA_ROLE,       cfg.mediaRole.toUtf8().constData(),
        PW_KEY_MEDIA_CLASS,      cfg.mediaClass.toUtf8().constData(),
        nullptr);

    if (!cfg.targetNodeName.isEmpty()) {
        pw_properties_set(p, PW_KEY_TARGET_OBJECT,
                          cfg.targetNodeName.toUtf8().constData());
    }

    pw_properties_setf(p, PW_KEY_NODE_RATE, "1/%u", cfg.rate);
    pw_properties_setf(p, PW_KEY_NODE_LATENCY, "%u/%u",
                       cfg.quantum, cfg.rate);

    // Audio format hints — the system mic exposes these (channels=2,
    // position=FL,FR) and they're required for WirePlumber's ALSA-compat
    // bridge to expose us as an arecord-listable capture device. Without
    // them WSJT-X / fldigi can see us in `wpctl status` but we don't
    // appear in their device dropdowns (which enumerate ALSA PCMs).
    pw_properties_setf(p, PW_KEY_AUDIO_CHANNELS, "%u", cfg.channels);
    if (!cfg.audioPosition.isEmpty()) {
        // Native audio plan Task 10 (R-AUD-07): a device stream on an
        // interface takes the node's own positions.
        pw_properties_set(p, "audio.position",
                          cfg.audioPosition.join(QLatin1Char(',')).toUtf8().constData());
    } else if (cfg.channels == 2) {
        pw_properties_set(p, "audio.position", "FL,FR");
    } else if (cfg.channels == 1) {
        pw_properties_set(p, "audio.position", "MONO");
    }

    // node.virtual = true is the canonical hint for "I am a virtual node,
    // not backed by hardware — please expose me as a first-class source
    // through the session manager / ALSA bridge."
    if (isVirtualSource) {
        pw_properties_set(p, "node.virtual", "true");
    }

    // Native audio plan Task 10 (R-AUD-07): the pair stays on its channels.
    if (cfg.dontRemix) {
        pw_properties_set(p, "stream.dont-remix", "true");
    }

    return p;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
PipeWireStream::PipeWireStream(PipeWireThreadLoop* loop,
                               StreamConfig cfg, QObject* parent)
    : QObject(parent), m_loop(loop), m_cfg(std::move(cfg)) {}

PipeWireStream::~PipeWireStream() { close(); }

// ---------------------------------------------------------------------------
// Device modes (native audio plan Task 10)
// ---------------------------------------------------------------------------
void PipeWireStream::setMatcherReader(MatcherReader reader)
{
    if (m_stream) {
        qCWarning(lcPw) << "setMatcherReader() after open():" << m_cfg.nodeName;
        return;
    }
    m_matcherReader = std::move(reader);
    m_matcherMode = m_matcherReader.valid();
    m_stereoScratch.assign(size_t(2 * kPipeWireDeviceChunkFrames), 0.0f);
}

void PipeWireStream::setInputSink(IAudioInputSink* sink, MicChannelPick pick)
{
    if (m_stream) {
        qCWarning(lcPw) << "setInputSink() after open():" << m_cfg.nodeName;
        return;
    }
    m_inputSink = sink;
    m_micPick = pick;
    m_stereoScratch.assign(size_t(2 * kPipeWireDeviceChunkFrames), 0.0f);
}

int PipeWireStream::graphQuantumFrames() const
{
    return m_graphQuantum.load(std::memory_order_relaxed);
}

std::int64_t PipeWireStream::deviceDelayNs() const
{
    return m_deviceDelayNs.load(std::memory_order_relaxed);
}

void PipeWireStream::requestFadeOut()
{
    if (m_matcherReader.valid()) {
        m_matcherReader.requestFadeOut();
    }
}

bool PipeWireStream::fadedOut() const
{
    if (!m_stream || !m_matcherReader.valid()) {
        return true;
    }
    return m_matcherReader.fadedOut();
}

// ---------------------------------------------------------------------------
// open() — Step 1
// ---------------------------------------------------------------------------
bool PipeWireStream::open()
{
    // Placed inside the member function so that private static callbacks are
    // accessible. libpipewire stores only a pointer to this table, so the
    // static storage duration ensures it outlives every stream.
    static const pw_stream_events k_streamEvents = {
        .version       = PW_VERSION_STREAM_EVENTS,
        .destroy       = nullptr,
        .state_changed = &PipeWireStream::onStateChangedCb,
        .control_info  = nullptr,
        .io_changed    = &PipeWireStream::onIoChangedCb,
        .param_changed = &PipeWireStream::onParamChangedCb,
        .add_buffer    = nullptr,
        .remove_buffer = nullptr,
        .process       = &PipeWireStream::onProcessCb,
        .drained       = nullptr,
        .command       = nullptr,
        .trigger_done  = nullptr,
    };

    if (m_stream) {
        qCWarning(lcPw) << "open() called on already-open stream:" << m_cfg.nodeName;
        return false;
    }
    if (!m_loop || !m_loop->core()) {
        qCWarning(lcPw) << "open() with no thread loop or core";
        return false;
    }

    m_loop->lock();
    m_stream = pw_stream_new(m_loop->core(),
                             m_cfg.nodeName.toUtf8().constData(),
                             configToProperties(m_cfg));
    if (!m_stream) {
        m_loop->unlock();
        qCWarning(lcPw) << "pw_stream_new failed for" << m_cfg.nodeName;
        return false;
    }
    pw_stream_add_listener(m_stream, &m_listener, &k_streamEvents, this);

    // Format param: S_F32_LE, channels, rate.
    uint8_t buffer[1024];
    spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    spa_audio_info_raw info{};
    info.format     = SPA_AUDIO_FORMAT_F32_LE;
    info.channels   = m_cfg.channels;
    info.rate       = m_cfg.rate;
    if (!m_cfg.audioPosition.isEmpty()) {
        // Native audio plan Task 10 (R-AUD-07): the node's own positions.
        const int n = std::min<int>(int(m_cfg.channels), SPA_AUDIO_MAX_CHANNELS);
        for (int i = 0; i < n; ++i) {
            info.position[i] = i < m_cfg.audioPosition.size()
                ? spaChannelForName(m_cfg.audioPosition.at(i))
                : uint32_t(SPA_AUDIO_CHANNEL_UNKNOWN);
        }
    } else {
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
    }
    const spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    const auto flags = static_cast<pw_stream_flags>(
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS);

    const auto dir = (m_cfg.direction == StreamConfig::Output)
                       ? PW_DIRECTION_OUTPUT : PW_DIRECTION_INPUT;

    const int r = pw_stream_connect(m_stream, dir, PW_ID_ANY, flags,
                                    params, 1);
    m_loop->unlock();
    if (r < 0) {
        qCWarning(lcPw) << "pw_stream_connect failed:" << r;
        return false;
    }

    qCInfo(lcPw) << "stream opened:" << m_cfg.nodeName
                 << "direction:" << (dir == PW_DIRECTION_OUTPUT ? "OUT" : "IN");
    return true;
}

// ---------------------------------------------------------------------------
// close() — Step 2
// ---------------------------------------------------------------------------
void PipeWireStream::close()
{
    // loop.lock() blocks until any in-flight onProcessCb() (and the
    // maybeEmitTelemetry call inside it) completes — so m_stream is
    // safe to destroy without racing the pw data thread.
    if (!m_stream) { return; }
    m_loop->lock();
    pw_stream_disconnect(m_stream);
    pw_stream_destroy(m_stream);
    m_stream = nullptr;
    spa_hook_remove(&m_listener);
    m_loop->unlock();
    m_position.store(nullptr, std::memory_order_release);
    m_streamState.store(int(PW_STREAM_STATE_UNCONNECTED), std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// push() — Step 5
// Forward contract #2: writes must be sizeof(float)*channels-aligned.
// Q_ASSERT fires in debug builds; compiles to no-op in release.
//
// Uses tryPushCopy (non-blocking, drop-on-full + xrun) instead of pushCopy
// (yield-until-space). When a VAX OUTPUT stream is in PipeWire's PAUSED
// state (no consumer connected — e.g. WSJT-X not running yet), the
// consumer-side onProcessOutput callback never fires, so the ring never
// drains. The original pushCopy yield-loop would block the DSP thread
// indefinitely on the first VAX bus tap, cascading into "no audio out the
// speakers" because rxBlockReady never reaches the speakers push at its
// tail. Drop-on-full is the right semantic for real-time audio anyway —
// stale samples are useless.
// ---------------------------------------------------------------------------
qint64 PipeWireStream::push(const char* data, qint64 bytes)
{
    Q_ASSERT(bytes % (sizeof(float) * m_cfg.channels) == 0);

    // RMS of the block, for the meter UI. Computed on the producer
    // thread (DSP thread for OUTPUT direction). Cheap — single pass,
    // no allocations.
    if (bytes > 0) {
        const float* samples = reinterpret_cast<const float*>(data);
        const qint64 floatCount = bytes / qint64(sizeof(float));
        double sumSq = 0.0;
        for (qint64 i = 0; i < floatCount; ++i) {
            const float s = samples[i];
            sumSq += double(s) * double(s);
        }
        const float rms = floatCount > 0
            ? float(std::sqrt(sumSq / double(floatCount)))
            : 0.0f;
        m_rxLevel.store(rms, std::memory_order_relaxed);
    }

    const qint64 written = m_ring.tryPushCopy(
        reinterpret_cast<const uint8_t*>(data), bytes);
    if (written < bytes) {
        m_xruns.fetch_add(1, std::memory_order_relaxed);
    }
    return written;
}

qint64 PipeWireStream::pull(char* data, qint64 maxBytes)
{
    return m_ring.popInto(reinterpret_cast<uint8_t*>(data), maxBytes);
}

// ---------------------------------------------------------------------------
// outputCounters() / isStreaming(), R-R3-44
// ---------------------------------------------------------------------------
PipeWireStream::OutputCounters PipeWireStream::outputCounters() const
{
    const qint64 frameBytes = qint64(sizeof(float) * m_cfg.channels);
    OutputCounters counters;
    counters.consumedFrames = m_outputConsumedFrames.load(std::memory_order_relaxed);
    counters.queuedFrames = frameBytes > 0 ? int(qint64(m_ring.usedBytes()) / frameBytes) : 0;
    counters.capacityFrames = frameBytes > 0 ? int(qint64(m_ring.capacity() - 1) / frameBytes) : 0;
    counters.callbackFrames = m_outputCallbackFrames.load(std::memory_order_relaxed);
    return counters;
}

bool PipeWireStream::isStreaming() const
{
    return m_streamState.load(std::memory_order_relaxed) == int(PW_STREAM_STATE_STREAMING);
}

// ---------------------------------------------------------------------------
// telemetry()
// ---------------------------------------------------------------------------
PipeWireStream::Telemetry PipeWireStream::telemetry() const {
    Telemetry t;
    t.streamStateName   = streamStateName(m_streamState.load(std::memory_order_relaxed));
    t.xrunCount         = m_xruns.load();
    t.processCbCpuPct   = m_cpuPct.load();
    t.measuredLatencyMs = m_latencyMs.load();
    t.deviceLatencyMs   = m_deviceLatencyMs.load();
    t.ringDepthMs       = m_cfg.rate
        ? double(m_ring.usedBytes()) / (m_cfg.rate * m_cfg.channels * sizeof(float)) * 1000.0
        : 0.0;
    t.pwQuantumMs       = m_cfg.rate ? double(m_cfg.quantum) / m_cfg.rate * 1000.0 : 0.0;
    t.schedPolicy       = m_schedPolicy.load(std::memory_order_relaxed);
    t.schedPriority     = m_schedPriority.load(std::memory_order_relaxed);
    return t;
}

// ---------------------------------------------------------------------------
// Static callbacks — Steps 3 & 4
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// streamStateName() — static helper: pw_stream_state int → display string.
// Safe to call from any thread; no shared state.
// ---------------------------------------------------------------------------
QString PipeWireStream::streamStateName(int s)
{
    switch (s) {
        case int(PW_STREAM_STATE_ERROR):       return QStringLiteral("error");
        case int(PW_STREAM_STATE_UNCONNECTED): return QStringLiteral("unconnected");
        case int(PW_STREAM_STATE_CONNECTING):  return QStringLiteral("connecting");
        case int(PW_STREAM_STATE_PAUSED):      return QStringLiteral("paused");
        case int(PW_STREAM_STATE_STREAMING):   return QStringLiteral("streaming");
        default:                               return QStringLiteral("unknown");
    }
}

void PipeWireStream::onStateChangedCb(void* userData,
                                      pw_stream_state /*old_*/,
                                      pw_stream_state new_,
                                      const char* error)
{
    auto* self = static_cast<PipeWireStream*>(userData);
    self->m_streamState.store(int(new_), std::memory_order_relaxed);
    // Compute the name once here; capture by value into the queued lambda so
    // the GUI-thread signal carries the correct string without touching
    // m_streamState again (avoids a second atomic load across the queue).
    const QString name = streamStateName(int(new_));
    // FIXME(task 14): self may dangle if ~PipeWireStream runs before queued emit drains.
    QMetaObject::invokeMethod(self, [self, name]() {
        emit self->streamStateChanged(name);
    }, Qt::QueuedConnection);
    if (error) {
        QString reason = QString::fromUtf8(error);
        QMetaObject::invokeMethod(self, [self, reason]() {
            emit self->errorOccurred(reason);
        }, Qt::QueuedConnection);
    }
}

void PipeWireStream::onParamChangedCb(void* /*userData*/, uint32_t /*id*/,
                                      const spa_pod* /*param*/)
{
    // TODO(task 10): extract quantum when SPA_PARAM_Latency arrives.
    // Native audio plan Task 10: the device modes read the graph's quantum
    // from the position io's duration instead (onIoChangedCb,
    // publishGraphTiming), which is the cycle the graph really runs.
}

// The graph's position io (SPA_IO_Position), read by the device modes'
// process callbacks for the cycle's duration.  Null when it is removed.
void PipeWireStream::onIoChangedCb(void* userData, uint32_t id, void* area,
                                   uint32_t size)
{
    auto* self = static_cast<PipeWireStream*>(userData);
    if (id != SPA_IO_Position) {
        return;
    }
    auto* position = (area != nullptr && size >= sizeof(spa_io_position))
        ? static_cast<spa_io_position*>(area) : nullptr;
    self->m_position.store(position, std::memory_order_release);
}

void PipeWireStream::onProcessCb(void* userData)
{
    auto* self = static_cast<PipeWireStream*>(userData);
    if (self->m_cfg.direction == StreamConfig::Output) {
        if (self->m_matcherMode) {
            self->onProcessMatcherOutput();
            return;
        }
        self->onProcessOutput();
    } else {
        if (self->m_inputSink != nullptr) {
            self->onProcessSinkInput();
            return;
        }
        self->onProcessInput();   // Task 11
    }
}

// ---------------------------------------------------------------------------
// Device modes' process callbacks (native audio plan Task 10, R-AUD-15).
// PipeWire's data thread, with PipeWire's own loop lock held around the
// call.  They take no lock of ours, allocate nothing (the scratch was sized
// before open()), log nothing and make no call but PipeWire's own buffer
// and time calls; unlike the VAX paths they post no telemetry.
// ---------------------------------------------------------------------------

// The graph's quantum (the position's duration, else the frames this cycle
// asked for) and the device delay, for the delay readout.
void PipeWireStream::publishGraphTiming(const pw_buffer* b)
{
    const spa_io_position* position = m_position.load(std::memory_order_acquire);
    uint64_t quantum = position ? position->clock.duration : 0;
    if (quantum == 0 && b != nullptr) {
        quantum = b->requested;
    }
    if (quantum > 0) {
        m_graphQuantum.store(int(std::min<uint64_t>(quantum, 1u << 20)),
                             std::memory_order_relaxed);
    }
    pw_time t{};
    if (pw_stream_get_time_n(m_stream, &t, sizeof(t)) == 0 && t.rate.denom > 0) {
        // pw_time::delay is in units of t.rate (as maybeEmitTelemetry reads it).
        const double delayNs = double(t.delay) * 1e9 * double(t.rate.num)
                             / double(t.rate.denom);
        m_deviceDelayNs.store(std::int64_t(std::max(0.0, delayNs)),
                              std::memory_order_relaxed);
    }
}

void PipeWireStream::onProcessMatcherOutput()
{
    pw_buffer* b = pw_stream_dequeue_buffer(m_stream);
    if (!b) { m_xruns.fetch_add(1, std::memory_order_relaxed); return; }

    spa_buffer* sb = b->buffer;
    if (!sb || !sb->datas[0].data || !sb->datas[0].chunk) {
        // The same negotiation-phase guard as onProcessOutput.
        pw_stream_queue_buffer(m_stream, b);
        return;
    }
    const uint32_t frameBytes = uint32_t(sizeof(float) * m_cfg.channels);
    const uint32_t frames = pipeWireOutputFrames(b->requested, sb->datas[0].maxsize, frameBytes);
    if (frames == 0) {
        pw_stream_queue_buffer(m_stream, b);
        return;
    }

    pipeWireFillFromMatcher(m_matcherReader, m_stereoScratch.data(),
                            kPipeWireDeviceChunkFrames,
                            static_cast<float*>(sb->datas[0].data), int(frames),
                            int(m_cfg.channels), m_cfg.pair);
    m_outputConsumedFrames.fetch_add(frames, std::memory_order_relaxed);
    m_outputCallbackFrames.store(int(frames), std::memory_order_relaxed);

    sb->datas[0].chunk->offset = 0;
    sb->datas[0].chunk->stride = int32_t(frameBytes);
    sb->datas[0].chunk->size   = frames * frameBytes;
    pw_stream_queue_buffer(m_stream, b);

    publishGraphTiming(b);
}

void PipeWireStream::onProcessSinkInput()
{
    pw_buffer* b = pw_stream_dequeue_buffer(m_stream);
    if (!b) { m_xruns.fetch_add(1, std::memory_order_relaxed); return; }

    const spa_buffer* sb = b->buffer;
    if (!sb || !sb->datas[0].data || !sb->datas[0].chunk) {
        pw_stream_queue_buffer(m_stream, b);
        return;
    }
    const uint32_t frameBytes = uint32_t(sizeof(float) * m_cfg.channels);
    const uint32_t maxsize = sb->datas[0].maxsize;
    const uint32_t offset = std::min(sb->datas[0].chunk->offset, maxsize);
    const uint32_t size = std::min(sb->datas[0].chunk->size, maxsize - offset);
    const int frames = frameBytes > 0 ? int(size / frameBytes) : 0;
    if (frames <= 0 || m_cfg.rate == 0) {
        pw_stream_queue_buffer(m_stream, b);
        return;
    }

    publishGraphTiming(b);
    const auto* src = static_cast<const uint8_t*>(sb->datas[0].data) + offset;
    const double nsPerFrame = 1e9 / double(m_cfg.rate);
    const std::int64_t delayNs = std::max<std::int64_t>(
        0, m_deviceDelayNs.load(std::memory_order_relaxed));
    // Frame 0 was captured the device delay plus this buffer's length ago.
    const std::int64_t frame0Ns = audioProbeNowNs() - delayNs
                                - std::int64_t(double(frames) * nsPerFrame);
    float* const stereo = m_stereoScratch.data();
    int done = 0;
    while (done < frames) {
        const int n = std::min(frames - done, kPipeWireDeviceChunkFrames);
        readDeviceToStereo(src + size_t(done) * frameBytes, nullptr, true,
                           DeviceSampleFormat::Float32, int(m_cfg.channels),
                           m_cfg.pair, m_micPick, n, stereo);
        m_inputSink->onInput(stereo, n, int(m_cfg.rate),
                             frame0Ns + std::int64_t(double(done) * nsPerFrame));
        done += n;
    }
    pw_stream_queue_buffer(m_stream, b);
}

// One-shot RT-scheduling probe on the pw data thread — this is the
// actual thread where rtkit's RT grant lands (not Qt main). See the
// forward contract from f35cc7b which removed the probe from
// PipeWireThreadLoop::connect(). Called from both onProcessOutput
// and onProcessInput so INPUT-only streams (e.g. the TX input path)
// also get probed.
//
// IMPORTANT: do NOT log from this function. Qt's logger is not
// RT-safe — it allocates QStrings, locks the global logger mutex,
// and walks the message handler chain. On the pw data thread, the
// resulting brief stall against the main thread (Qt log mutex) is
// long enough for Mutter's xdg_wm_base.ping watchdog to flag the
// client as unresponsive and SIGKILL the process. The schedPolicy /
// schedPriority values are stored in atomics and exposed via the
// Telemetry struct — the Setup → Audio Backend Strip / Output page
// reads them from the GUI thread at its own cadence, where Qt
// logging is safe if needed.
void PipeWireStream::probeSchedOnce()
{
    if (m_schedProbed.load(std::memory_order_relaxed)) { return; }
    int policy = SCHED_OTHER;
    sched_param param{};
    if (pthread_getschedparam(pthread_self(), &policy, &param) == 0) {
        m_schedPolicy.store(policy, std::memory_order_relaxed);
        m_schedPriority.store(param.sched_priority, std::memory_order_relaxed);
    }
    m_schedProbed.store(true, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// onProcessOutput() — Step 4
// Called on the PipeWire RT data thread (PW_STREAM_FLAG_RT_PROCESS).
// No allocations, no locks, no blocking calls.
// ---------------------------------------------------------------------------
void PipeWireStream::onProcessOutput()
{
    timespec t0{}, t1{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);

    probeSchedOnce();

    pw_buffer* b = pw_stream_dequeue_buffer(m_stream);
    if (!b) { m_xruns.fetch_add(1, std::memory_order_relaxed); return; }

    spa_buffer* sb = b->buffer;
    if (!sb || !sb->datas[0].data || !sb->datas[0].chunk) {
        // PipeWire fires on_process during PAUSED state to drive graph
        // negotiation. The first such callback can deliver a buffer that is
        // allocated but not yet wired (chunk or data pointer is null).
        // Requeue and return cleanly — no data to consume, no crash.
        pw_stream_queue_buffer(m_stream, b);
        return;
    }

    auto* dst = static_cast<uint8_t*>(sb->datas[0].data);
    const uint32_t dstCapacity = sb->datas[0].maxsize;
    if (dstCapacity == 0) {
        pw_stream_queue_buffer(m_stream, b);
        return;
    }

    // R-R3-44 fix wave: fill what the graph asked for this cycle
    // (pw_buffer::requested, PipeWire >= 0.3.49; the build requires 0.3.50),
    // clamped to the buffer. Filling and counting the whole buffer
    // (maxsize) ran this clock several times fast whenever the quantum was
    // smaller than the buffer, and a VAX feeder paces against it.
    const uint32_t frameBytes = uint32_t(sizeof(float) * m_cfg.channels);
    const uint32_t frames = pipeWireOutputFrames(b->requested, dstCapacity, frameBytes);
    const uint32_t fillBytes = frames * frameBytes;
    const qint64 popped = m_ring.popInto(dst, qint64(fillBytes));
    if (popped < qint64(fillBytes)) {
        std::memset(dst + popped, 0, fillBytes - size_t(popped));
    }
    // R-R3-44: the graph takes these frames each cycle, audio or silence,
    // so this is the output clock a VAX feeder paces against.
    m_outputConsumedFrames.fetch_add(frames, std::memory_order_relaxed);
    m_outputCallbackFrames.store(int(frames), std::memory_order_relaxed);

    sb->datas[0].chunk->offset = 0;
    sb->datas[0].chunk->stride = int32_t(frameBytes);
    sb->datas[0].chunk->size   = fillBytes;
    pw_stream_queue_buffer(m_stream, b);

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
    const double cbNs = double((t1.tv_sec - t0.tv_sec) * 1'000'000'000LL
                               + (t1.tv_nsec - t0.tv_nsec));
    const double quantumNs = double(m_cfg.quantum) / m_cfg.rate * 1e9;
    m_cpuPct.store(quantumNs > 0.0 ? cbNs / quantumNs * 100.0 : 0.0,
                   std::memory_order_relaxed);

    maybeEmitTelemetry();
}

void PipeWireStream::onProcessInput()
{
    timespec t0{}, t1{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);

    probeSchedOnce();

    pw_buffer* b = pw_stream_dequeue_buffer(m_stream);
    if (!b) { m_xruns.fetch_add(1, std::memory_order_relaxed); return; }

    const spa_buffer* sb = b->buffer;
    if (!sb || !sb->datas[0].data || !sb->datas[0].chunk) {
        // Same negotiation-phase guard as onProcessOutput: PAUSED-state
        // callbacks can deliver a buffer with null data/chunk pointers.
        // Requeue and return cleanly.
        pw_stream_queue_buffer(m_stream, b);
        return;
    }

    const auto* src = static_cast<const uint8_t*>(sb->datas[0].data);
    const uint32_t size = sb->datas[0].chunk->size;
    if (size == 0) {
        pw_stream_queue_buffer(m_stream, b);
        return;
    }

    // Non-blocking write: this runs on PipeWire's data thread, which
    // the daemon SIGKILLs if it doesn't return within the per-quantum
    // budget. Yielding (the original pushCopy contract) is fatal here.
    // tryPushCopy returns whatever fits in the current free space; any
    // shortfall is counted as an xrun (data dropped because the
    // consumer — the TX DSP, Phase 3M — isn't keeping up or doesn't
    // exist yet).
    const qint64 written = m_ring.tryPushCopy(src, qint64(size));
    if (written < qint64(size)) {
        m_xruns.fetch_add(1, std::memory_order_relaxed);
    }

    pw_stream_queue_buffer(m_stream, b);

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
    const double cbNs = double((t1.tv_sec - t0.tv_sec) * 1'000'000'000LL
                               + (t1.tv_nsec - t0.tv_nsec));
    const double quantumNs = double(m_cfg.quantum) / m_cfg.rate * 1e9;
    m_cpuPct.store(quantumNs > 0.0 ? cbNs / quantumNs * 100.0 : 0.0,
                   std::memory_order_relaxed);

    maybeEmitTelemetry();
}

// ---------------------------------------------------------------------------
// 1 Hz coalesced telemetry update from the pw data thread.
// pw_stream_get_time_n() is RT-safe (per pipewire/stream.h:591). The
// QueuedConnection emit is NOT strictly RT-safe — it allocates a
// QMetaCallEvent and briefly locks the receiver's event-queue mutex —
// but the 1 Hz gate makes the cost ~1×/sec, far below the per-quantum
// budget. Re-evaluate if you ever drop the 1 Hz coalesce.
// ---------------------------------------------------------------------------
void PipeWireStream::maybeEmitTelemetry()
{
    const qint64 nowNs = qint64(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    const qint64 last = m_lastTelemetryNs.load(std::memory_order_relaxed);
    if (nowNs - last < 1'000'000'000) { return; }
    m_lastTelemetryNs.store(nowNs, std::memory_order_relaxed);

    pw_time t{};
    pw_stream_get_time_n(m_stream, &t, sizeof(t));

    // pw_time::buffered is frames (per pipewire/stream.h:395), NOT nanoseconds.
    // Plan had /1e6 which would under-report by ~21000× at 48 kHz. Corrected:
    const double bufferedMs = m_cfg.rate
        ? double(t.buffered) * 1000.0 / m_cfg.rate
        : 0.0;
    // pw_time::delay is in units of t.rate (samples × rate fraction → ms):
    const double delayMs = (t.rate.denom > 0)
        ? double(t.delay) * 1000.0 * t.rate.num / double(t.rate.denom)
        : 0.0;
    const double ringMs = double(m_ring.usedBytes())
                        / (m_cfg.rate * m_cfg.channels * sizeof(float))
                        * 1000.0;
    m_latencyMs.store(bufferedMs + delayMs + ringMs, std::memory_order_relaxed);
    m_deviceLatencyMs.store(delayMs, std::memory_order_relaxed);

    QMetaObject::invokeMethod(this, &PipeWireStream::telemetryUpdated,
                              Qt::QueuedConnection);
}

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
