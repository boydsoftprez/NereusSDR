// =================================================================
// src/core/audio/PipeWireStream.cpp  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Claude Code.
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE
#include "core/audio/PipeWireStream.h"

#include <QLoggingCategory>
#include <pipewire/keys.h>

#include <cstring>

#include "core/audio/PipeWireThreadLoop.h"

Q_DECLARE_LOGGING_CATEGORY(lcPw)

namespace NereusSDR {

// ---------------------------------------------------------------------------
// configToProperties — pure, unit-testable (no daemon required).
// ---------------------------------------------------------------------------
pw_properties* configToProperties(const StreamConfig& cfg)
{
    pw_properties* p = pw_properties_new(
        PW_KEY_NODE_NAME,        cfg.nodeName.toUtf8().constData(),
        PW_KEY_NODE_DESCRIPTION, cfg.nodeDescription.toUtf8().constData(),
        PW_KEY_MEDIA_TYPE,       "Audio",
        PW_KEY_MEDIA_CATEGORY,
            cfg.direction == StreamConfig::Output ? "Playback" : "Capture",
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
        .io_changed    = nullptr,
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
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;
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
    if (!m_stream) { return; }
    m_loop->lock();
    pw_stream_disconnect(m_stream);
    pw_stream_destroy(m_stream);
    m_stream = nullptr;
    spa_hook_remove(&m_listener);
    m_loop->unlock();
    m_stateName = QStringLiteral("closed");
}

// ---------------------------------------------------------------------------
// push() — Step 5
// Forward contract #2: writes must be sizeof(float)*channels-aligned.
// Q_ASSERT fires in debug builds; compiles to no-op in release.
// ---------------------------------------------------------------------------
qint64 PipeWireStream::push(const char* data, qint64 bytes)
{
    Q_ASSERT(bytes % (sizeof(float) * m_cfg.channels) == 0);
    return m_ring.pushCopy(reinterpret_cast<const uint8_t*>(data), bytes);
}

qint64 PipeWireStream::pull(char*, qint64)       { return 0; }    // Task 11

// ---------------------------------------------------------------------------
// telemetry()
// ---------------------------------------------------------------------------
PipeWireStream::Telemetry PipeWireStream::telemetry() const {
    Telemetry t;
    t.streamStateName = m_stateName;
    t.xrunCount = m_xruns.load();
    t.processCbCpuPct = m_cpuPct.load();
    t.measuredLatencyMs = m_latencyMs.load();
    t.deviceLatencyMs = m_deviceLatencyMs.load();
    return t;
}

// ---------------------------------------------------------------------------
// Static callbacks — Steps 3 & 4
// ---------------------------------------------------------------------------

void PipeWireStream::onStateChangedCb(void* userData,
                                      pw_stream_state /*old_*/,
                                      pw_stream_state new_,
                                      const char* error)
{
    auto* self = static_cast<PipeWireStream*>(userData);
    QString name;
    switch (new_) {
        case PW_STREAM_STATE_ERROR:       name = QStringLiteral("error");       break;
        case PW_STREAM_STATE_UNCONNECTED: name = QStringLiteral("unconnected"); break;
        case PW_STREAM_STATE_CONNECTING:  name = QStringLiteral("connecting");  break;
        case PW_STREAM_STATE_PAUSED:      name = QStringLiteral("paused");      break;
        case PW_STREAM_STATE_STREAMING:   name = QStringLiteral("streaming");   break;
        default:                          name = QStringLiteral("unknown");     break;
    }
    self->m_stateName = name;
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
}

void PipeWireStream::onProcessCb(void* userData)
{
    auto* self = static_cast<PipeWireStream*>(userData);
    if (self->m_cfg.direction == StreamConfig::Output) {
        self->onProcessOutput();
    } else {
        self->onProcessInput();   // Task 11
    }
}

// ---------------------------------------------------------------------------
// onProcessOutput() — Step 4
// Called on the PipeWire RT data thread (PW_STREAM_FLAG_RT_PROCESS).
// No allocations, no locks, no blocking calls.
// ---------------------------------------------------------------------------
void PipeWireStream::onProcessOutput()
{
    pw_buffer* b = pw_stream_dequeue_buffer(m_stream);
    if (!b) { m_xruns.fetch_add(1, std::memory_order_relaxed); return; }

    spa_buffer* sb = b->buffer;
    auto* dst = static_cast<uint8_t*>(sb->datas[0].data);
    const uint32_t dstCapacity = sb->datas[0].maxsize;

    const qint64 popped = m_ring.popInto(dst, qint64(dstCapacity));
    if (popped < qint64(dstCapacity)) {
        std::memset(dst + popped, 0, dstCapacity - size_t(popped));
    }

    sb->datas[0].chunk->offset = 0;
    sb->datas[0].chunk->stride = sizeof(float) * m_cfg.channels;
    sb->datas[0].chunk->size   = dstCapacity;
    pw_stream_queue_buffer(m_stream, b);
}

void PipeWireStream::onProcessInput()
{
    // Task 11
}

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
