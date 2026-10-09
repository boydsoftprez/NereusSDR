// =================================================================
// src/core/audio/PulseAudioSystem.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See PulseAudioSystem.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-07, R-AUD-31,
//               R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/audio/PulseAudioSystem.h"

#include "core/LogCategories.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/PulseAudioBus.h"

#include <QElapsedTimer>
#include <QMutexLocker>

#include <pulse/pulseaudio.h>

#include <algorithm>
#include <atomic>
#include <utility>

namespace NereusSDR {

namespace {

const QString kBusKey = QStringLiteral("device.bus");   // PA_PROP_DEVICE_BUS
const QString kAlsaCardKey = QStringLiteral("alsa.card");
const QString kAlsaDeviceKey = QStringLiteral("alsa.device");

int intProperty(const QHash<QString, QString>& props, const QString& key)
{
    bool ok = false;
    const int value = props.value(key).trimmed().toInt(&ok);
    return ok ? value : -1;
}

} // namespace

bool pulseDeviceIsListed(const PulseDeviceRecord& record)
{
    return !record.name.isEmpty() && !record.isMonitor;
}

PulseDeviceRecord pulseDeviceRecordFrom(const QString& name, const QString& description,
                                        bool isSink, bool isMonitor,
                                        const QStringList& channelMap,
                                        const QHash<QString, QString>& props)
{
    PulseDeviceRecord record;
    record.name = name;
    record.description = description;
    record.isSink = isSink;
    record.isMonitor = isMonitor;
    record.channelMap = channelMap;
    record.busProperty = props.value(kBusKey);
    record.alsaCard = intProperty(props, kAlsaCardKey);
    record.alsaDevice = intProperty(props, kAlsaDeviceKey);
    return record;
}

// ---------------------------------------------------------------------------
// PulseServerState
// ---------------------------------------------------------------------------
void PulseServerState::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    m_sink = std::move(sink);
}

void PulseServerState::post(AudioNotice notice)
{
    std::function<void(AudioNotice)> sink;
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        sink = m_sink;
    }
    if (sink) {
        sink(notice);
    }
}

bool PulseServerState::subscriptionEvent(PulseFacility facility, PulseEventType type)
{
    switch (facility) {
    case PulseFacility::Sink:
    case PulseFacility::Source:
        // A volume or port change is a Change; a device that comes or goes
        // (a profile change remakes its sinks) is New or Remove.
        if (type != PulseEventType::Change) {
            post(AudioNotice::DevicesChanged);
        }
        return false;
    case PulseFacility::Server:
        return true;
    case PulseFacility::Other:
        return false;
    }
    return false;
}

void PulseServerState::serverInfo(const QString& serverName, const QString& defaultSink,
                                  const QString& defaultSource)
{
    bool sinkChanged = false;
    bool sourceChanged = false;
    {
        QMutexLocker lock(&m_mutex);
        const bool first = !m_serverName.has_value();
        sinkChanged = !first && defaultSink != m_defaultSink;
        sourceChanged = !first && defaultSource != m_defaultSource;
        m_serverName = serverName;
        m_defaultSink = defaultSink;
        m_defaultSource = defaultSource;
    }
    if (sinkChanged) {
        post(AudioNotice::DefaultOutputChanged);
    }
    if (sourceChanged) {
        post(AudioNotice::DefaultInputChanged);
    }
}

void PulseServerState::clear()
{
    {
        QMutexLocker lock(&m_mutex);
        m_serverName.reset();
        m_defaultSink.clear();
        m_defaultSource.clear();
    }
    post(AudioNotice::DevicesChanged);
}

std::optional<QString> PulseServerState::serverName() const
{
    QMutexLocker lock(&m_mutex);
    return m_serverName;
}

QString PulseServerState::defaultName(AudioDeviceDirection direction) const
{
    QMutexLocker lock(&m_mutex);
    return direction == AudioDeviceDirection::Output ? m_defaultSink : m_defaultSource;
}

// ---------------------------------------------------------------------------
// The real adapter
// ---------------------------------------------------------------------------
namespace {

QString fromUtf8(const char* text)
{
    return text != nullptr ? QString::fromUtf8(text) : QString();
}

QStringList channelNames(const pa_channel_map& map)
{
    QStringList names;
    for (unsigned i = 0; i < map.channels && i < PA_CHANNELS_MAX; ++i) {
        names.append(fromUtf8(pa_channel_position_to_string(map.map[i])));
    }
    return names;
}

QHash<QString, QString> wantedProperties(const pa_proplist* props)
{
    QHash<QString, QString> hash;
    if (props == nullptr) {
        return hash;
    }
    for (const QString& key : {kBusKey, kAlsaCardKey, kAlsaDeviceKey}) {
        const QByteArray k = key.toUtf8();
        const char* value = pa_proplist_gets(props, k.constData());
        if (value != nullptr) {
            hash.insert(key, QString::fromUtf8(value));
        }
    }
    return hash;
}

PulseFacility facilityOf(pa_subscription_event_type_t type)
{
    switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SINK:
        return PulseFacility::Sink;
    case PA_SUBSCRIPTION_EVENT_SOURCE:
        return PulseFacility::Source;
    case PA_SUBSCRIPTION_EVENT_SERVER:
        return PulseFacility::Server;
    default:
        return PulseFacility::Other;
    }
}

PulseEventType eventTypeOf(pa_subscription_event_type_t type)
{
    switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
    case PA_SUBSCRIPTION_EVENT_NEW:
        return PulseEventType::New;
    case PA_SUBSCRIPTION_EVENT_REMOVE:
        return PulseEventType::Remove;
    default:
        return PulseEventType::Change;
    }
}

struct DeviceQuery {
    pa_threaded_mainloop* mainloop = nullptr;
    QList<PulseDeviceRecord> records;
    int pending = 0;
};

void onSinkInfo(pa_context* /*context*/, const pa_sink_info* info, int eol, void* userdata)
{
    auto* query = static_cast<DeviceQuery*>(userdata);
    if (eol != 0 || info == nullptr) {
        --query->pending;
        pa_threaded_mainloop_signal(query->mainloop, 0);
        return;
    }
    query->records.append(pulseDeviceRecordFrom(fromUtf8(info->name), fromUtf8(info->description),
                                                true, false, channelNames(info->channel_map),
                                                wantedProperties(info->proplist)));
}

void onSourceInfo(pa_context* /*context*/, const pa_source_info* info, int eol, void* userdata)
{
    auto* query = static_cast<DeviceQuery*>(userdata);
    if (eol != 0 || info == nullptr) {
        --query->pending;
        pa_threaded_mainloop_signal(query->mainloop, 0);
        return;
    }
    const bool monitor = info->monitor_of_sink != PA_INVALID_INDEX;
    query->records.append(pulseDeviceRecordFrom(fromUtf8(info->name), fromUtf8(info->description),
                                                false, monitor, channelNames(info->channel_map),
                                                wantedProperties(info->proplist)));
}

// One context on one threaded mainloop: everything the adapter and its
// streams share.
class PulseConnection final : public PulseStreamHost {
public:
    PulseConnection() = default;
    ~PulseConnection() override { disconnect(); }

    PulseConnection(const PulseConnection&) = delete;
    PulseConnection& operator=(const PulseConnection&) = delete;

    bool connect();
    void disconnect();

    pa_threaded_mainloop* mainloop() const override { return m_mainloop; }
    pa_context* context() const override { return m_context; }
    bool running() const override { return m_running.load(std::memory_order_acquire); }

    PulseServerState& state() { return m_state; }
    QList<PulseDeviceRecord> devices();

private:
    static void onContextState(pa_context* context, void* userdata);
    static void onServerInfo(pa_context* context, const pa_server_info* info, void* userdata);
    static void onSubscribe(pa_context* context, pa_subscription_event_type_t type,
                            uint32_t index, void* userdata);

    pa_threaded_mainloop* m_mainloop = nullptr;
    pa_context* m_context = nullptr;
    std::atomic<bool> m_running{false};
    bool m_infoArrived = false;   // under the mainloop lock
    PulseServerState m_state;
};

bool PulseConnection::connect()
{
    QElapsedTimer clock;
    clock.start();
    m_mainloop = pa_threaded_mainloop_new();
    if (m_mainloop == nullptr) {
        return false;
    }
    if (pa_threaded_mainloop_start(m_mainloop) < 0) {
        pa_threaded_mainloop_free(m_mainloop);
        m_mainloop = nullptr;
        return false;
    }
    pa_threaded_mainloop_lock(m_mainloop);
    m_context = pa_context_new(pa_threaded_mainloop_get_api(m_mainloop), "NereusSDR");
    if (m_context == nullptr) {
        pa_threaded_mainloop_unlock(m_mainloop);
        disconnect();
        return false;
    }
    pa_context_set_state_callback(m_context, &PulseConnection::onContextState, this);
    // Never start a server that is not running.
    if (pa_context_connect(m_context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0) {
        pa_threaded_mainloop_unlock(m_mainloop);
        disconnect();
        return false;
    }
    pa_context* context = m_context;
    pulseWait(m_mainloop, m_context, kPulseConnectWaitMs, [context] {
        const pa_context_state_t state = pa_context_get_state(context);
        return state == PA_CONTEXT_READY || !PA_CONTEXT_IS_GOOD(state);
    });
    bool answered = pa_context_get_state(m_context) == PA_CONTEXT_READY;
    if (answered) {
        pa_operation* op = pa_context_get_server_info(m_context, &PulseConnection::onServerInfo, this);
        if (op != nullptr) {
            const int left = std::max(0, kPulseConnectWaitMs - int(clock.elapsed()));
            pulseWait(m_mainloop, m_context, left, [this] { return m_infoArrived; });
            if (!m_infoArrived) {
                pa_operation_cancel(op);
            }
            pa_operation_unref(op);
        }
        answered = m_infoArrived && pa_context_get_state(m_context) == PA_CONTEXT_READY;
    }
    if (!answered) {
        pa_threaded_mainloop_unlock(m_mainloop);
        disconnect();
        qCInfo(lcAudio) << "PulseAudio is not running; its engine is offered as not running";
        return false;
    }
    pa_context_set_subscribe_callback(m_context, &PulseConnection::onSubscribe, this);
    const auto mask = static_cast<pa_subscription_mask_t>(
        PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE | PA_SUBSCRIPTION_MASK_SERVER);
    if (pa_operation* op = pa_context_subscribe(m_context, mask, nullptr, nullptr)) {
        pa_operation_unref(op);
    }
    m_running.store(true, std::memory_order_release);
    pa_threaded_mainloop_unlock(m_mainloop);
    return true;
}

void PulseConnection::disconnect()
{
    if (m_mainloop == nullptr) {
        return;
    }
    pa_threaded_mainloop_lock(m_mainloop);
    m_running.store(false, std::memory_order_release);
    if (m_context != nullptr) {
        pa_context_set_state_callback(m_context, nullptr, nullptr);
        pa_context_set_subscribe_callback(m_context, nullptr, nullptr);
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        m_context = nullptr;
    }
    pa_threaded_mainloop_unlock(m_mainloop);
    pa_threaded_mainloop_stop(m_mainloop);
    pa_threaded_mainloop_free(m_mainloop);
    m_mainloop = nullptr;
}

QList<PulseDeviceRecord> PulseConnection::devices()
{
    if (!running() || m_mainloop == nullptr) {
        return {};
    }
    DeviceQuery query;
    query.mainloop = m_mainloop;
    pa_threaded_mainloop_lock(m_mainloop);
    if (!running()) {
        pa_threaded_mainloop_unlock(m_mainloop);
        return {};
    }
    pa_operation* sinks = pa_context_get_sink_info_list(m_context, &onSinkInfo, &query);
    pa_operation* sources = pa_context_get_source_info_list(m_context, &onSourceInfo, &query);
    query.pending = (sinks != nullptr ? 1 : 0) + (sources != nullptr ? 1 : 0);
    const bool complete = pulseWait(m_mainloop, m_context, kPulseConnectWaitMs, [this, &query] {
        return query.pending <= 0 || !running();
    }) && query.pending <= 0;
    for (pa_operation* op : {sinks, sources}) {
        if (op != nullptr) {
            if (!complete) {
                pa_operation_cancel(op);   // no callback after this
            }
            pa_operation_unref(op);
        }
    }
    pa_threaded_mainloop_unlock(m_mainloop);
    if (!complete) {
        qCWarning(lcAudio) << "PulseAudio did not finish its device list in"
                           << kPulseConnectWaitMs << "ms";
    }
    return query.records;
}

// The mainloop's thread, its lock held.
void PulseConnection::onContextState(pa_context* context, void* userdata)
{
    auto* self = static_cast<PulseConnection*>(userdata);
    if (!PA_CONTEXT_IS_GOOD(pa_context_get_state(context))
        && self->m_running.exchange(false, std::memory_order_acq_rel)) {
        // The server went away; every stream on this context fails with it
        // and posts its own DeviceLost.
        qCWarning(lcAudio) << "PulseAudio connection lost";
        self->m_state.clear();
    }
    pa_threaded_mainloop_signal(self->m_mainloop, 0);
}

void PulseConnection::onServerInfo(pa_context* /*context*/, const pa_server_info* info, void* userdata)
{
    auto* self = static_cast<PulseConnection*>(userdata);
    if (info != nullptr) {
        self->m_state.serverInfo(fromUtf8(info->server_name), fromUtf8(info->default_sink_name),
                                 fromUtf8(info->default_source_name));
        self->m_infoArrived = true;
    }
    pa_threaded_mainloop_signal(self->m_mainloop, 0);
}

// Only posts; a server event asks for the server's info, whose answer posts.
void PulseConnection::onSubscribe(pa_context* context, pa_subscription_event_type_t type,
                                  uint32_t /*index*/, void* userdata)
{
    auto* self = static_cast<PulseConnection*>(userdata);
    if (self->m_state.subscriptionEvent(facilityOf(type), eventTypeOf(type))) {
        if (pa_operation* op = pa_context_get_server_info(context, &PulseConnection::onServerInfo, self)) {
            pa_operation_unref(op);
        }
    }
}

class PulseAudioSystemImpl final : public IPulseAudioSystem {
public:
    explicit PulseAudioSystemImpl(std::shared_ptr<PulseConnection> connection)
        : m_connection(std::move(connection))
    {
    }

    bool running() override { return m_connection->running(); }
    std::optional<QString> serverName() override
    {
        if (!m_connection->running()) {
            return std::nullopt;
        }
        return m_connection->state().serverName();
    }
    QList<PulseDeviceRecord> devices() override { return m_connection->devices(); }
    QString defaultName(AudioDeviceDirection direction) override
    {
        return m_connection->state().defaultName(direction);
    }
    void setNoticeSink(std::function<void(AudioNotice)> sink) override
    {
        m_connection->state().setNoticeSink(std::move(sink));
    }
    std::unique_ptr<IAudioBus> createOutput(const PulseDeviceRecord& device,
                                            const AudioStreamRequest& request) override
    {
        AudioStreamRequest output = request;
        output.direction = AudioDeviceDirection::Output;
        return makePulseOutputBus(m_connection, pulseStreamConfig(device, output), request.delayMs);
    }
    std::unique_ptr<IAudioInputStream> createInput(const PulseDeviceRecord& device,
                                                   const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override
    {
        AudioStreamRequest input = request;
        input.direction = AudioDeviceDirection::Input;
        return makePulseInputStream(m_connection, pulseStreamConfig(device, input), pick, sink);
    }

private:
    std::shared_ptr<PulseConnection> m_connection;
};

} // namespace

std::unique_ptr<IPulseAudioSystem> makePulseAudioSystem()
{
    auto connection = std::make_shared<PulseConnection>();
    // R-AUD-32: a test run never reaches a sound server.
    if (!audioDevicesBarredForTestRun()) {
        connection->connect();
    }
    return std::make_unique<PulseAudioSystemImpl>(std::move(connection));
}

} // namespace NereusSDR
