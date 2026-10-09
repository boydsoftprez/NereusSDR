// =================================================================
// src/core/audio/PipeWireDeviceSystem.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See PipeWireDeviceSystem.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 10 (R-AUD-01, R-AUD-03, R-AUD-07,
//               R-AUD-14). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/audio/PipeWireDeviceSystem.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/DeviceRateMatcher.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <thread>
#include <utility>
#include <vector>

#ifdef NEREUS_HAVE_PIPEWIRE
#include "core/audio/PipeWireThreadLoop.h"

#include <QObject>

#include <pipewire/extensions/metadata.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>

#include <cerrno>
#include <cstring>
#endif

namespace NereusSDR {

namespace {

const QString kSinkClass = QStringLiteral("Audio/Sink");
const QString kSourceClass = QStringLiteral("Audio/Source");
const QString kDuplexClass = QStringLiteral("Audio/Duplex");
const QString kDefaultSinkKey = QStringLiteral("default.audio.sink");
const QString kDefaultSourceKey = QStringLiteral("default.audio.source");

int intProperty(const QHash<QString, QString>& props, const QString& key)
{
    bool ok = false;
    const int value = props.value(key).trimmed().toInt(&ok);
    return ok ? value : -1;
}

QStringList positionsFromChannels(int channels)
{
    if (channels == 1) {
        return {QStringLiteral("MONO")};
    }
    if (channels == 2) {
        return {QStringLiteral("FL"), QStringLiteral("FR")};
    }
    QStringList positions;
    for (int i = 0; i < channels; ++i) {
        positions.append(QStringLiteral("AUX%1").arg(i));
    }
    return positions;
}

} // namespace

bool pipeWireNodeIsListed(const PipeWireNodeRecord& node)
{
    if (node.nodeName.isEmpty() || node.nodeName.endsWith(QStringLiteral(".monitor"))) {
        return false;
    }
    return node.mediaClass == kSinkClass || node.mediaClass == kSourceClass
           || node.mediaClass == kDuplexClass;
}

std::optional<PipeWireNodeRecord> pipeWireNodeRecordFromProperties(
    std::uint32_t id, const QHash<QString, QString>& props)
{
    PipeWireNodeRecord record;
    record.id = id;
    record.nodeName = props.value(QStringLiteral("node.name"));
    if (record.nodeName.isEmpty()) {
        return std::nullopt;
    }
    record.description = props.value(QStringLiteral("node.description"));
    if (record.description.isEmpty()) {
        record.description = props.value(QStringLiteral("node.nick"), record.nodeName);
    }
    record.mediaClass = props.value(QStringLiteral("media.class"));
    record.deviceApi = props.value(QStringLiteral("device.api"));
    const QString position = props.value(QStringLiteral("audio.position")).trimmed();
    if (!position.isEmpty()) {
        // "FL,FR" or "[ FL, FR ]" (the JSON-ish form PipeWire also accepts).
        QString list = position;
        list.remove(QLatin1Char('[')).remove(QLatin1Char(']'));
        for (const QString& part : list.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            const QString name = part.trimmed();
            if (!name.isEmpty()) {
                record.positions.append(name);
            }
        }
    }
    if (record.positions.isEmpty()) {
        const int channels = intProperty(props, QStringLiteral("audio.channels"));
        if (channels > 0) {
            record.positions = positionsFromChannels(channels);
        }
    }
    record.alsaCard = intProperty(props, QStringLiteral("api.alsa.pcm.card"));
    record.alsaDevice = intProperty(props, QStringLiteral("api.alsa.pcm.device"));
    return record;
}

QString pipeWireMetadataDefaultName(const QString& value)
{
    const QJsonDocument doc = QJsonDocument::fromJson(value.toUtf8());
    if (!doc.isObject()) {
        return {};
    }
    return doc.object().value(QStringLiteral("name")).toString();
}

// ---------------------------------------------------------------------------
// PipeWireNodeDirectory
// ---------------------------------------------------------------------------
void PipeWireNodeDirectory::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    m_sink = std::move(sink);
}

void PipeWireNodeDirectory::post(AudioNotice notice)
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

void PipeWireNodeDirectory::nodeProperties(std::uint32_t id, const QHash<QString, QString>& props)
{
    const std::optional<PipeWireNodeRecord> record = pipeWireNodeRecordFromProperties(id, props);
    const bool listed = record && pipeWireNodeIsListed(*record);
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_nodes.find(id);
        if (!listed) {
            if (it != m_nodes.end()) {
                m_nodes.erase(it);
                changed = true;
            }
        } else if (it == m_nodes.end()) {
            m_nodes.insert(id, *record);
            changed = true;
        } else if (!(it.value() == *record)) {
            it.value() = *record;
            changed = true;
        }
    }
    if (changed) {
        post(AudioNotice::DevicesChanged);
    }
}

void PipeWireNodeDirectory::nodeRemoved(std::uint32_t id)
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        changed = m_nodes.remove(id) > 0;
    }
    if (changed) {
        post(AudioNotice::DevicesChanged);
    }
}

void PipeWireNodeDirectory::metadataProperty(std::uint32_t subject, const QString& key,
                                             bool keyIsNull, const QString& value)
{
    if (subject != 0) {
        return;
    }
    bool sinkChanged = false;
    bool sourceChanged = false;
    {
        QMutexLocker lock(&m_mutex);
        if (keyIsNull) {
            sinkChanged = !m_defaultSink.isEmpty();
            sourceChanged = !m_defaultSource.isEmpty();
            m_defaultSink.clear();
            m_defaultSource.clear();
        } else if (key == kDefaultSinkKey) {
            const QString name = pipeWireMetadataDefaultName(value);
            sinkChanged = name != m_defaultSink;
            m_defaultSink = name;
        } else if (key == kDefaultSourceKey) {
            const QString name = pipeWireMetadataDefaultName(value);
            sourceChanged = name != m_defaultSource;
            m_defaultSource = name;
        }
    }
    if (sinkChanged) {
        post(AudioNotice::DefaultOutputChanged);
    }
    if (sourceChanged) {
        post(AudioNotice::DefaultInputChanged);
    }
}

void PipeWireNodeDirectory::clear()
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        changed = !m_nodes.isEmpty();
        m_nodes.clear();
        m_defaultSink.clear();
        m_defaultSource.clear();
    }
    if (changed) {
        post(AudioNotice::DevicesChanged);
    }
}

QList<PipeWireNodeRecord> PipeWireNodeDirectory::nodes() const
{
    QMutexLocker lock(&m_mutex);
    return m_nodes.values();
}

QString PipeWireNodeDirectory::defaultNodeName(AudioDeviceDirection direction) const
{
    QMutexLocker lock(&m_mutex);
    return direction == AudioDeviceDirection::Output ? m_defaultSink : m_defaultSource;
}

// ---------------------------------------------------------------------------
// ReconnectingPipeWireDeviceSystem
// ---------------------------------------------------------------------------

// The engine's notice sink.  Each current connection's directory posts
// through it; it outlives the system while a connection still holds it.
struct ReconnectingPipeWireDeviceSystem::Forward {
    std::mutex mutex;
    std::function<void(AudioNotice)> sink;

    void post(AudioNotice notice)
    {
        std::function<void(AudioNotice)> s;
        {
            std::lock_guard<std::mutex> lock(mutex);
            s = sink;
        }
        if (s) {
            s(notice);
        }
    }
};

// A lost handler's way back to the system's thread, and the guard on every
// timer and queued call into the system.  stop() clears the target under
// `mutex`, so no handler or try queues anything after it; it sets `stopped`
// under `runMutex`, which a running timer, report or loss holds, so nothing
// runs in the system once stop() returns, even when the system is
// destroyed on another thread while its own thread is in one of them.
struct ReconnectingPipeWireDeviceSystem::Relay {
    std::mutex mutex;
    QObject* target = nullptr;

    std::mutex runMutex;
    bool stopped = false;
};

ReconnectingPipeWireDeviceSystem::ReconnectingPipeWireDeviceSystem(PipeWireConnector connector,
                                                                   int retryIntervalMs)
    : m_connector(std::move(connector))
    , m_retryIntervalMs(retryIntervalMs)
    , m_forward(std::make_shared<Forward>())
    , m_relay(std::make_shared<Relay>())
    , m_retryTimer(std::make_unique<QTimer>())
{
    m_retryTimer->setSingleShot(true);
    std::shared_ptr<Relay> relay = m_relay;
    QObject::connect(m_retryTimer.get(), &QTimer::timeout, m_retryTimer.get(), [this, relay] {
        std::lock_guard<std::mutex> run(relay->runMutex);
        if (!relay->stopped) {
            retryNow();
        }
    });
    {
        std::lock_guard<std::mutex> lock(m_relay->mutex);
        m_relay->target = m_retryTimer.get();
    }
    std::shared_ptr<IPipeWireConnection> first = m_connector ? m_connector() : nullptr;
    if (first) {
        adopt(first);
    }
    if (!first || !first->running()) {
        if (m_retryIntervalMs > 0) {
            qCInfo(lcAudio) << "PipeWire is not running; its engine is offered as not running"
                            << "and tries again every" << m_retryIntervalMs << "ms";
        }
        scheduleRetry();
    }
}

ReconnectingPipeWireDeviceSystem::~ReconnectingPipeWireDeviceSystem()
{
    stop();
}

void ReconnectingPipeWireDeviceSystem::stop()
{
    if (m_stopped) {
        return;
    }
    m_stopped = true;
    {
        std::lock_guard<std::mutex> lock(m_relay->mutex);
        m_relay->target = nullptr;
    }
    {
        // Waits only for a call already running on the system's thread
        // (adopting a finished try, or a loss), which never waits on the
        // daemon; a pending try, or one still on its worker, is never
        // waited for.
        std::lock_guard<std::mutex> run(m_relay->runMutex);
        m_relay->stopped = true;
    }
    if (m_retryTimer) {
        QObject::disconnect(m_retryTimer.get(), nullptr, nullptr, nullptr);
        if (m_retryTimer->thread() == QThread::currentThread()) {
            m_retryTimer.reset();   // stops a pending try; queued lost events go with it
        } else {
            // Destroyed off its thread (a catalogue thread that outlived
            // its stop()): the timer is stopped and deleted on its own.
            m_retryTimer.release()->deleteLater();
        }
    }
    if (std::shared_ptr<IPipeWireConnection> connection = current()) {
        connection->setLostHandler({});
        connection->directory().setNoticeSink({});
    }
}

bool ReconnectingPipeWireDeviceSystem::retrying() const
{
    return !m_stopped && (m_tryRunning || (m_retryTimer && m_retryTimer->isActive()));
}

std::shared_ptr<IPipeWireConnection> ReconnectingPipeWireDeviceSystem::current() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current;
}

void ReconnectingPipeWireDeviceSystem::adopt(const std::shared_ptr<IPipeWireConnection>& connection)
{
    std::shared_ptr<IPipeWireConnection> old;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        old = std::exchange(m_current, connection);
    }
    if (old) {
        // Its streams keep it alive until they close; it posts nothing more.
        old->setLostHandler({});
        old->directory().setNoticeSink({});
    }
    std::weak_ptr<IPipeWireConnection> weak = connection;
    std::shared_ptr<Relay> relay = m_relay;
    connection->setLostHandler([this, relay, weak] {
        std::lock_guard<std::mutex> lock(relay->mutex);
        if (relay->target != nullptr) {
            // Runs only while the target (owned by this system) lives.
            QMetaObject::invokeMethod(
                relay->target,
                [this, relay, weak] {
                    std::lock_guard<std::mutex> run(relay->runMutex);
                    if (!relay->stopped) {
                        onLost(weak);
                    }
                },
                Qt::QueuedConnection);
        }
    });
    std::shared_ptr<Forward> forward = m_forward;
    connection->directory().setNoticeSink([forward](AudioNotice notice) { forward->post(notice); });
}

void ReconnectingPipeWireDeviceSystem::onLost(const std::weak_ptr<IPipeWireConnection>& lost)
{
    if (m_stopped || lost.lock() != current()) {
        return;   // an old connection, already replaced
    }
    if (m_retryIntervalMs > 0) {
        qCWarning(lcAudio) << "PipeWire went away; trying again every" << m_retryIntervalMs << "ms";
    }
    scheduleRetry();
}

void ReconnectingPipeWireDeviceSystem::scheduleRetry()
{
    if (m_stopped || m_tryRunning || m_retryIntervalMs <= 0 || !m_connector || !m_retryTimer
        || m_retryTimer->isActive()) {
        return;
    }
    m_retryTimer->start(m_retryIntervalMs);
}

void ReconnectingPipeWireDeviceSystem::retryNow()
{
    if (m_stopped || m_tryRunning) {
        return;
    }
    std::shared_ptr<IPipeWireConnection> active = current();
    if (active && active->running()) {
        return;
    }
    // The try runs on a worker of its own, so a daemon that is up but slow
    // never holds this thread.  The worker holds no pointer to the system:
    // it reports back through the relay, and a report that arrives after
    // stop() is never run (its connection is dropped with it).
    m_tryRunning = true;
    std::thread([this, connector = m_connector, relay = m_relay] {
        std::shared_ptr<IPipeWireConnection> next = connector();
        std::lock_guard<std::mutex> lock(relay->mutex);
        if (relay->target == nullptr) {
            return;   // stopped: discarded here, never adopted
        }
        QMetaObject::invokeMethod(
            relay->target,
            [this, relay, next] {
                std::lock_guard<std::mutex> run(relay->runMutex);
                if (!relay->stopped) {
                    finishTry(next);
                }
            },
            Qt::QueuedConnection);
    }).detach();
}

void ReconnectingPipeWireDeviceSystem::finishTry(const std::shared_ptr<IPipeWireConnection>& next)
{
    m_tryRunning = false;
    if (m_stopped) {
        return;
    }
    if (!next || !next->running()) {
        scheduleRetry();
        return;
    }
    std::shared_ptr<IPipeWireConnection> active = current();
    if (active && active->running()) {
        return;   // nothing to replace
    }
    adopt(next);
    qCInfo(lcAudio) << "PipeWire answers again; its devices are listed";
    // The supervisor reopens the chosen devices on the new list.
    m_forward->post(AudioNotice::DevicesChanged);
    m_forward->post(AudioNotice::DefaultOutputChanged);
    m_forward->post(AudioNotice::DefaultInputChanged);
    if (!next->running()) {
        scheduleRetry();   // lost again before its handler was set
    }
}

bool ReconnectingPipeWireDeviceSystem::running()
{
    std::shared_ptr<IPipeWireConnection> connection = current();
    return connection && connection->running();
}

QList<PipeWireNodeRecord> ReconnectingPipeWireDeviceSystem::nodes()
{
    std::shared_ptr<IPipeWireConnection> connection = current();
    return connection ? connection->directory().nodes() : QList<PipeWireNodeRecord>{};
}

QString ReconnectingPipeWireDeviceSystem::defaultNodeName(AudioDeviceDirection direction)
{
    std::shared_ptr<IPipeWireConnection> connection = current();
    return connection ? connection->directory().defaultNodeName(direction) : QString();
}

void ReconnectingPipeWireDeviceSystem::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    std::lock_guard<std::mutex> lock(m_forward->mutex);
    m_forward->sink = std::move(sink);
}

std::unique_ptr<IAudioBus> ReconnectingPipeWireDeviceSystem::createOutput(
    const PipeWireNodeRecord& node, const AudioStreamRequest& request)
{
    std::shared_ptr<IPipeWireConnection> connection = current();
    return connection ? connection->createOutput(node, request) : nullptr;
}

std::unique_ptr<IAudioInputStream> ReconnectingPipeWireDeviceSystem::createInput(
    const PipeWireNodeRecord& node, const AudioStreamRequest& request, MicChannelPick pick,
    IAudioInputSink* sink)
{
    std::shared_ptr<IPipeWireConnection> connection = current();
    return connection ? connection->createInput(node, request, pick, sink) : nullptr;
}

#ifdef NEREUS_HAVE_PIPEWIRE

// ---------------------------------------------------------------------------
// pipeWireDeviceStreamConfig
// ---------------------------------------------------------------------------
StreamConfig pipeWireDeviceStreamConfig(const PipeWireNodeRecord& node,
                                        const AudioStreamRequest& request,
                                        AudioDeviceDirection direction)
{
    StreamConfig cfg;
    const bool output = direction == AudioDeviceDirection::Output;
    cfg.direction = output ? StreamConfig::Output : StreamConfig::Input;
    cfg.nodeName = output ? QStringLiteral("nereussdr.device-output")
                          : QStringLiteral("nereussdr.device-input");
    cfg.nodeDescription = output ? QStringLiteral("NereusSDR output")
                                 : QStringLiteral("NereusSDR input");
    cfg.mediaClass = output ? QStringLiteral("Stream/Output/Audio")
                            : QStringLiteral("Stream/Input/Audio");
    cfg.mediaRole = output ? QStringLiteral("Music") : QStringLiteral("Phone");
    cfg.rate = request.sampleRate > 0 ? uint32_t(request.sampleRate) : 48000u;
    cfg.quantum = request.bufferFrames > 0 ? uint32_t(request.bufferFrames)
                                           : uint32_t(kPipeWireDefaultQuantumFrames);
    cfg.targetNodeName = node.nodeName;
    cfg.pair = request.pair;
    const int positions = int(node.positions.size());
    if (positions > 2) {
        // R-AUD-07: every channel of the interface, in its own positions,
        // with no remix, so the pair is the interface's own channels.
        cfg.channels = uint32_t(positions);
        cfg.audioPosition = node.positions;
        cfg.dontRemix = true;
    } else {
        cfg.channels = positions == 1 ? 1u : 2u;
    }
    // A pair past the stream's channels plays on the first pair.
    if (cfg.pair.firstChannel < 1
        || cfg.pair.firstChannel + std::max(1, cfg.pair.channelCount) - 1 > int(cfg.channels)) {
        cfg.pair = AudioChannelPair{1, cfg.channels == 1 ? 1 : 2};
    }
    return cfg;
}

namespace {

QHash<QString, QString> dictToHash(const spa_dict* dict)
{
    QHash<QString, QString> props;
    if (dict == nullptr) {
        return props;
    }
    const spa_dict_item* item = nullptr;
    spa_dict_for_each(item, dict) {
        if (item->key != nullptr) {
            props.insert(QString::fromUtf8(item->key),
                         item->value != nullptr ? QString::fromUtf8(item->value) : QString());
        }
    }
    return props;
}

bool isDeviceClass(const char* mediaClass)
{
    return mediaClass != nullptr
           && (std::strcmp(mediaClass, "Audio/Sink") == 0 || std::strcmp(mediaClass, "Audio/Source") == 0
               || std::strcmp(mediaClass, "Audio/Duplex") == 0);
}

// A stream's link to the node it plays on: the registry listener posts
// DeviceLost through it when the node goes away (R-AUD-08's "on the
// stream's error").
struct PipeWireLossWatch {
    std::uint32_t nodeId = 0;   // 0: follows the system default, never lost
    std::mutex mutex;
    std::function<void(const AudioStreamEvent&)> sink;

    void post(AudioStreamEvent::Kind kind, const QString& detail)
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
};

// One connection to the daemon: everything the adapter and its streams
// share.  A stream holds it, so the thread loop outlives every stream on it.
class PipeWireDeviceCore final : public IPipeWireConnection,
                                 public std::enable_shared_from_this<PipeWireDeviceCore> {
public:
    PipeWireDeviceCore() = default;
    ~PipeWireDeviceCore() override { disconnect(); }

    PipeWireDeviceCore(const PipeWireDeviceCore&) = delete;
    PipeWireDeviceCore& operator=(const PipeWireDeviceCore&) = delete;

    bool connect(bool reportUnreachable);
    void disconnect();

    PipeWireThreadLoop* loop() const { return m_loop.get(); }
    bool running() const override { return m_running.load(std::memory_order_acquire); }
    PipeWireNodeDirectory& directory() override { return m_directory; }

    void setLostHandler(std::function<void()> handler) override
    {
        std::lock_guard<std::mutex> lock(m_lostMutex);
        m_lostHandler = std::move(handler);
    }

    std::unique_ptr<IAudioBus> createOutput(const PipeWireNodeRecord& node,
                                            const AudioStreamRequest& request) override;
    std::unique_ptr<IAudioInputStream> createInput(const PipeWireNodeRecord& node,
                                                   const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;

    void watch(const std::shared_ptr<PipeWireLossWatch>& watch)
    {
        std::lock_guard<std::mutex> lock(m_watchMutex);
        m_watches.erase(std::remove_if(m_watches.begin(), m_watches.end(),
                                       [](const std::weak_ptr<PipeWireLossWatch>& w) {
                                           return w.expired();
                                       }),
                        m_watches.end());
        for (const std::weak_ptr<PipeWireLossWatch>& existing : m_watches) {
            if (existing.lock() == watch) {
                return;   // reopened: already watched
            }
        }
        m_watches.push_back(watch);
    }

private:
    struct BoundNode {
        PipeWireDeviceCore* core = nullptr;
        std::uint32_t id = 0;
        pw_proxy* proxy = nullptr;
        spa_hook listener{};
    };

    static void onCoreDone(void* data, uint32_t id, int seq);
    static void onCoreError(void* data, uint32_t id, int seq, int res, const char* message);
    static void onGlobal(void* data, uint32_t id, uint32_t permissions, const char* type,
                         uint32_t version, const spa_dict* props);
    static void onGlobalRemove(void* data, uint32_t id);
    static void onNodeInfo(void* data, const pw_node_info* info);
    static int onMetadataProperty(void* data, uint32_t subject, const char* key,
                                  const char* type, const char* value);

    void unbindNode(std::map<std::uint32_t, std::unique_ptr<BoundNode>>::iterator it);
    void unbindMetadata();
    void postLost(std::uint32_t id);
    void postAllLost();

    std::unique_ptr<PipeWireThreadLoop> m_loop;
    pw_registry* m_registry = nullptr;
    spa_hook m_coreListener{};
    spa_hook m_registryListener{};
    bool m_coreListening = false;
    std::map<std::uint32_t, std::unique_ptr<BoundNode>> m_nodes;
    pw_proxy* m_metadata = nullptr;
    std::uint32_t m_metadataId = 0;
    spa_hook m_metadataListener{};
    int m_pendingSeq = -1;
    int m_syncStage = 0;
    bool m_synced = false;
    std::atomic<bool> m_running{false};
    PipeWireNodeDirectory m_directory;

    std::mutex m_watchMutex;
    std::vector<std::weak_ptr<PipeWireLossWatch>> m_watches;

    std::mutex m_lostMutex;
    std::function<void()> m_lostHandler;
};

bool PipeWireDeviceCore::connect(bool reportUnreachable)
{
    // Only the members every supported libpipewire has (0.3.50 up); the
    // rest stay null.
    static const pw_core_events kCoreEvents = [] {
        pw_core_events events{};
        events.version = PW_VERSION_CORE_EVENTS;
        events.done = &PipeWireDeviceCore::onCoreDone;
        events.error = &PipeWireDeviceCore::onCoreError;
        return events;
    }();
    static const pw_registry_events kRegistryEvents = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = &PipeWireDeviceCore::onGlobal,
        .global_remove = &PipeWireDeviceCore::onGlobalRemove,
    };

    m_loop = std::make_unique<PipeWireThreadLoop>();
    if (!m_loop->connect(reportUnreachable)) {
        m_loop.reset();
        return false;
    }
    m_loop->lock();
    pw_core_add_listener(m_loop->core(), &m_coreListener, &kCoreEvents, this);
    m_coreListening = true;
    m_registry = pw_core_get_registry(m_loop->core(), PW_VERSION_REGISTRY, 0);
    if (m_registry == nullptr) {
        m_loop->unlock();
        qCWarning(lcAudio) << "PipeWire registry not available";
        disconnect();
        return false;
    }
    pw_registry_add_listener(m_registry, &m_registryListener, &kRegistryEvents, this);
    m_running.store(true, std::memory_order_release);
    // Two round trips: the first brings every global (and binds the device
    // nodes and the metadata), the second their info and properties.
    m_syncStage = 1;
    m_pendingSeq = pw_core_sync(m_loop->core(), PW_ID_CORE, 0);
    while (!m_synced && m_running.load(std::memory_order_acquire)) {
        if (pw_thread_loop_timed_wait(m_loop->loop(), kPipeWireConnectWaitSeconds) != 0) {
            qCWarning(lcAudio) << "PipeWire did not finish its device list in"
                               << kPipeWireConnectWaitSeconds << "s; it fills in as it arrives";
            break;
        }
    }
    const bool running = m_running.load(std::memory_order_acquire);
    m_loop->unlock();
    return running;
}

void PipeWireDeviceCore::disconnect()
{
    if (!m_loop) {
        return;
    }
    m_loop->lock();
    while (!m_nodes.empty()) {
        unbindNode(m_nodes.begin());
    }
    unbindMetadata();
    if (m_registry != nullptr) {
        spa_hook_remove(&m_registryListener);
        pw_proxy_destroy(reinterpret_cast<pw_proxy*>(m_registry));
        m_registry = nullptr;
    }
    if (m_coreListening) {
        spa_hook_remove(&m_coreListener);
        m_coreListening = false;
    }
    m_running.store(false, std::memory_order_release);
    m_loop->unlock();
    m_loop.reset();
}

void PipeWireDeviceCore::unbindNode(std::map<std::uint32_t, std::unique_ptr<BoundNode>>::iterator it)
{
    BoundNode& node = *it->second;
    spa_hook_remove(&node.listener);
    if (node.proxy != nullptr) {
        pw_proxy_destroy(node.proxy);
    }
    m_nodes.erase(it);
}

void PipeWireDeviceCore::unbindMetadata()
{
    if (m_metadata != nullptr) {
        spa_hook_remove(&m_metadataListener);
        pw_proxy_destroy(m_metadata);
        m_metadata = nullptr;
        m_metadataId = 0;
    }
}

void PipeWireDeviceCore::postLost(std::uint32_t id)
{
    std::vector<std::shared_ptr<PipeWireLossWatch>> lost;
    {
        std::lock_guard<std::mutex> lock(m_watchMutex);
        for (const std::weak_ptr<PipeWireLossWatch>& weak : m_watches) {
            if (std::shared_ptr<PipeWireLossWatch> watch = weak.lock()) {
                if (watch->nodeId != 0 && watch->nodeId == id) {
                    lost.push_back(std::move(watch));
                }
            }
        }
    }
    for (const std::shared_ptr<PipeWireLossWatch>& watch : lost) {
        watch->post(AudioStreamEvent::Kind::DeviceLost, QStringLiteral("The device went away"));
    }
}

void PipeWireDeviceCore::postAllLost()
{
    std::vector<std::shared_ptr<PipeWireLossWatch>> lost;
    {
        std::lock_guard<std::mutex> lock(m_watchMutex);
        for (const std::weak_ptr<PipeWireLossWatch>& weak : m_watches) {
            if (std::shared_ptr<PipeWireLossWatch> watch = weak.lock()) {
                lost.push_back(std::move(watch));
            }
        }
    }
    for (const std::shared_ptr<PipeWireLossWatch>& watch : lost) {
        watch->post(AudioStreamEvent::Kind::DeviceLost, QStringLiteral("PipeWire went away"));
    }
}

// The listener's callbacks: the thread loop's thread, its lock held.
void PipeWireDeviceCore::onCoreDone(void* data, uint32_t id, int seq)
{
    auto* self = static_cast<PipeWireDeviceCore*>(data);
    if (id != PW_ID_CORE || seq != self->m_pendingSeq) {
        return;
    }
    if (self->m_syncStage == 1) {
        self->m_syncStage = 2;
        self->m_pendingSeq = pw_core_sync(self->m_loop->core(), PW_ID_CORE, 0);
        return;
    }
    self->m_synced = true;
    pw_thread_loop_signal(self->m_loop->loop(), false);
}

void PipeWireDeviceCore::onCoreError(void* data, uint32_t id, int /*seq*/, int res,
                                     const char* message)
{
    auto* self = static_cast<PipeWireDeviceCore*>(data);
    if (id != PW_ID_CORE || res != -EPIPE) {
        return;
    }
    // The daemon went away: nothing is listed and the engine is not running.
    // (The thread loop's thread, not a device callback: logging is safe.)
    qCWarning(lcAudio) << "PipeWire connection lost:" << (message ? message : "");
    const bool wasRunning = self->m_running.exchange(false, std::memory_order_acq_rel);
    self->m_directory.clear();
    pw_thread_loop_signal(self->m_loop->loop(), false);
    if (!wasRunning) {
        return;
    }
    // Every open stream is on a dead connection; the system's handler
    // queues a reconnect to its own thread and returns.
    self->postAllLost();
    std::function<void()> handler;
    {
        std::lock_guard<std::mutex> lock(self->m_lostMutex);
        handler = self->m_lostHandler;
    }
    if (handler) {
        handler();
    }
}

void PipeWireDeviceCore::onGlobal(void* data, uint32_t id, uint32_t /*permissions*/,
                                  const char* type, uint32_t /*version*/, const spa_dict* props)
{
    static const pw_node_events kNodeEvents = [] {
        pw_node_events events{};
        events.version = PW_VERSION_NODE_EVENTS;
        events.info = &PipeWireDeviceCore::onNodeInfo;
        return events;
    }();
    static const pw_metadata_events kMetadataEvents = {
        .version = PW_VERSION_METADATA_EVENTS,
        .property = &PipeWireDeviceCore::onMetadataProperty,
    };

    auto* self = static_cast<PipeWireDeviceCore*>(data);
    if (type == nullptr || self->m_registry == nullptr) {
        return;
    }
    if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        const char* mediaClass = props ? spa_dict_lookup(props, PW_KEY_MEDIA_CLASS) : nullptr;
        if (!isDeviceClass(mediaClass) || self->m_nodes.count(id) > 0) {
            return;
        }
        auto node = std::make_unique<BoundNode>();
        node->core = self;
        node->id = id;
        node->proxy = static_cast<pw_proxy*>(
            pw_registry_bind(self->m_registry, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
        if (node->proxy == nullptr) {
            return;
        }
        pw_proxy_add_object_listener(node->proxy, &node->listener, &kNodeEvents, node.get());
        self->m_nodes.emplace(id, std::move(node));
        return;
    }
    if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0 && self->m_metadata == nullptr) {
        const char* name = props ? spa_dict_lookup(props, "metadata.name") : nullptr;
        if (name == nullptr || std::strcmp(name, "default") != 0) {
            return;
        }
        self->m_metadata = static_cast<pw_proxy*>(
            pw_registry_bind(self->m_registry, id, PW_TYPE_INTERFACE_Metadata,
                             PW_VERSION_METADATA, 0));
        if (self->m_metadata == nullptr) {
            return;
        }
        self->m_metadataId = id;
        pw_proxy_add_object_listener(self->m_metadata, &self->m_metadataListener,
                                     &kMetadataEvents, self);
    }
}

void PipeWireDeviceCore::onGlobalRemove(void* data, uint32_t id)
{
    auto* self = static_cast<PipeWireDeviceCore*>(data);
    const auto it = self->m_nodes.find(id);
    if (it != self->m_nodes.end()) {
        self->unbindNode(it);
        self->m_directory.nodeRemoved(id);
        self->postLost(id);
        return;
    }
    if (self->m_metadata != nullptr && id == self->m_metadataId) {
        self->unbindMetadata();
        self->m_directory.metadataProperty(0, QString(), true, QString());
    }
}

void PipeWireDeviceCore::onNodeInfo(void* data, const pw_node_info* info)
{
    auto* node = static_cast<BoundNode*>(data);
    if (info == nullptr || (info->change_mask & PW_NODE_CHANGE_MASK_PROPS) == 0
        || info->props == nullptr) {
        return;
    }
    node->core->m_directory.nodeProperties(node->id, dictToHash(info->props));
}

int PipeWireDeviceCore::onMetadataProperty(void* data, uint32_t subject, const char* key,
                                           const char* /*type*/, const char* value)
{
    auto* self = static_cast<PipeWireDeviceCore*>(data);
    self->m_directory.metadataProperty(subject,
                                       key != nullptr ? QString::fromUtf8(key) : QString(),
                                       key == nullptr,
                                       value != nullptr ? QString::fromUtf8(value) : QString());
    return 0;
}

const QString kBarredError = QStringLiteral("Audio devices are not opened in a test run");
const QString kNotRunningError = QStringLiteral("PipeWire is not running");

// ---------------------------------------------------------------------------
// The output: a DeviceRateMatcher the stream's process callback reads.
// ---------------------------------------------------------------------------
class PipeWireDeviceOutputBus final : public IAudioBus {
public:
    PipeWireDeviceOutputBus(std::shared_ptr<PipeWireDeviceCore> core, StreamConfig cfg,
                            int delayMs, std::uint32_t nodeId)
        : m_core(std::move(core))
        , m_cfg(std::move(cfg))
        , m_delayMs(delayMs)
        , m_watch(std::make_shared<PipeWireLossWatch>())
    {
        m_watch->nodeId = nodeId;
    }
    ~PipeWireDeviceOutputBus() override { close(); }

    PipeWireDeviceOutputBus(const PipeWireDeviceOutputBus&) = delete;
    PipeWireDeviceOutputBus& operator=(const PipeWireDeviceOutputBus&) = delete;

    bool open(const AudioFormat& format) override
    {
        if (audioDevicesBarredForTestRun()) {
            m_err = kBarredError;
            return false;
        }
        if (m_stream) {
            close();
        }
        if (!m_core || !m_core->running() || m_core->loop() == nullptr) {
            m_err = kNotRunningError;
            return false;
        }
        DeviceRateMatcher::Config config;
        config.inRate = 48000;
        config.outRate = int(m_cfg.rate);
        config.callbackFrames = int(m_cfg.quantum);
        config.delayMs = m_delayMs;
        auto matcher = std::make_unique<DeviceRateMatcher>(config);
        if (!matcher->valid()) {
            m_err = QStringLiteral("Clock matcher cannot run at %1 Hz").arg(m_cfg.rate);
            return false;
        }
        auto stream = std::make_unique<PipeWireStream>(m_core->loop(), m_cfg);
        stream->setMatcherReader(matcher->makeReader());
        std::weak_ptr<PipeWireLossWatch> weakWatch = m_watch;
        QObject::connect(stream.get(), &PipeWireStream::errorOccurred, stream.get(),
                         [weakWatch](const QString& reason) {
                             if (std::shared_ptr<PipeWireLossWatch> watch = weakWatch.lock()) {
                                 watch->post(AudioStreamEvent::Kind::DeviceLost, reason);
                             }
                         });
        m_matcher = std::move(matcher);
        if (!stream->open()) {
            stream.reset();
            m_matcher.reset();
            m_err = QStringLiteral("PipeWire did not open the stream");
            return false;
        }
        m_stream = std::move(stream);
        m_core->watch(m_watch);
        m_open.store(true, std::memory_order_release);
        m_format = format;
        m_format.sampleRate = int(m_cfg.rate);
        m_format.channels = 2;
        m_format.sample = AudioFormat::Sample::Float32;
        m_err.clear();
        return true;
    }

    void close() override
    {
        m_open.store(false, std::memory_order_release);
        if (m_stream) {
            m_stream->close();   // the loop lock: no process callback runs after it
            m_stream.reset();
        }
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
        pacing.callbackFrames = bufferFrames();
        const std::int64_t delayNs = m_stream ? m_stream->deviceDelayNs() : -1;
        if (delayNs > 0) {
            pacing.deviceLatencyNs = qint64(delayNs);
        }
        return pacing;
    }

    float rxLevel() const override { return m_rxLevel.load(std::memory_order_acquire); }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("PipeWire"); }
    AudioFormat negotiatedFormat() const override { return m_format; }
    QString errorString() const override { return m_err; }

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_watch->mutex);
        m_watch->sink = std::move(sink);
    }

    // Design choice 11: the buffer is the quantum the graph really runs,
    // the latency the device delay pw_stream_get_time_n reports.
    AudioDelayParts delayParts() const override
    {
        if (!m_matcher || m_cfg.rate == 0) {
            return {};
        }
        const double bufferMs = 1000.0 * double(bufferFrames()) / double(m_cfg.rate);
        const std::int64_t delayNs = m_stream ? m_stream->deviceDelayNs() : -1;
        const double latencyMs = delayNs > 0 ? double(delayNs) / 1e6 : 0.0;
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

    void requestFadeOut() override
    {
        if (m_stream) {
            m_stream->requestFadeOut();
        }
    }

    bool fadedOut() const override { return m_stream ? m_stream->fadedOut() : true; }

private:
    int bufferFrames() const
    {
        const int graph = m_stream ? m_stream->graphQuantumFrames() : 0;
        return graph > 0 ? graph : int(m_cfg.quantum);
    }

    std::shared_ptr<PipeWireDeviceCore> m_core;
    StreamConfig m_cfg;
    int m_delayMs = 0;
    std::shared_ptr<PipeWireLossWatch> m_watch;
    std::unique_ptr<DeviceRateMatcher> m_matcher;
    std::unique_ptr<PipeWireStream> m_stream;   // after the matcher: destroyed first
    AudioFormat m_format;
    QString m_err;
    std::atomic<bool> m_open{false};
    std::atomic<float> m_rxLevel{0.0f};
};

// ---------------------------------------------------------------------------
// The input: the stream's process callback hands the pair to the sink.
// ---------------------------------------------------------------------------
class PipeWireDeviceInputStream final : public IAudioInputStream {
public:
    PipeWireDeviceInputStream(std::shared_ptr<PipeWireDeviceCore> core, StreamConfig cfg,
                              MicChannelPick pick, IAudioInputSink* sink, std::uint32_t nodeId)
        : m_core(std::move(core))
        , m_cfg(std::move(cfg))
        , m_pick(pick)
        , m_sink(sink)
        , m_watch(std::make_shared<PipeWireLossWatch>())
    {
        m_watch->nodeId = nodeId;
    }
    ~PipeWireDeviceInputStream() override { close(); }

    PipeWireDeviceInputStream(const PipeWireDeviceInputStream&) = delete;
    PipeWireDeviceInputStream& operator=(const PipeWireDeviceInputStream&) = delete;

    bool open() override
    {
        if (audioDevicesBarredForTestRun()) {
            m_err = kBarredError;
            return false;
        }
        if (m_stream) {
            close();
        }
        if (!m_core || !m_core->running() || m_core->loop() == nullptr) {
            m_err = kNotRunningError;
            return false;
        }
        if (m_sink == nullptr) {
            m_err = QStringLiteral("No input sink");
            return false;
        }
        auto stream = std::make_unique<PipeWireStream>(m_core->loop(), m_cfg);
        stream->setInputSink(m_sink, m_pick);
        std::weak_ptr<PipeWireLossWatch> weakWatch = m_watch;
        QObject::connect(stream.get(), &PipeWireStream::errorOccurred, stream.get(),
                         [weakWatch](const QString& reason) {
                             if (std::shared_ptr<PipeWireLossWatch> watch = weakWatch.lock()) {
                                 watch->post(AudioStreamEvent::Kind::DeviceLost, reason);
                             }
                         });
        if (!stream->open()) {
            m_err = QStringLiteral("PipeWire did not open the stream");
            return false;
        }
        m_stream = std::move(stream);
        m_core->watch(m_watch);
        m_open.store(true, std::memory_order_release);
        m_err.clear();
        return true;
    }

    void close() override
    {
        m_open.store(false, std::memory_order_release);
        if (m_stream) {
            m_stream->close();
            m_stream.reset();
        }
    }

    bool isOpen() const override { return m_open.load(std::memory_order_acquire); }
    QString errorString() const override { return m_err; }
    int sampleRate() const override { return int(m_cfg.rate); }

    std::optional<std::int64_t> inputLatencyNs() const override
    {
        const std::int64_t delayNs = m_stream ? m_stream->deviceDelayNs() : -1;
        if (delayNs < 0) {
            return std::nullopt;
        }
        return delayNs;
    }

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_watch->mutex);
        m_watch->sink = std::move(sink);
    }

private:
    std::shared_ptr<PipeWireDeviceCore> m_core;
    StreamConfig m_cfg;
    MicChannelPick m_pick;
    IAudioInputSink* m_sink;
    std::shared_ptr<PipeWireLossWatch> m_watch;
    std::unique_ptr<PipeWireStream> m_stream;
    QString m_err;
    std::atomic<bool> m_open{false};
};

// ---------------------------------------------------------------------------
// The connection's streams, and the adapter.
// ---------------------------------------------------------------------------
std::unique_ptr<IAudioBus> PipeWireDeviceCore::createOutput(const PipeWireNodeRecord& node,
                                                            const AudioStreamRequest& request)
{
    return std::make_unique<PipeWireDeviceOutputBus>(
        shared_from_this(), pipeWireDeviceStreamConfig(node, request, AudioDeviceDirection::Output),
        request.delayMs, node.id);
}

std::unique_ptr<IAudioInputStream> PipeWireDeviceCore::createInput(const PipeWireNodeRecord& node,
                                                                   const AudioStreamRequest& request,
                                                                   MicChannelPick pick,
                                                                   IAudioInputSink* sink)
{
    return std::make_unique<PipeWireDeviceInputStream>(
        shared_from_this(), pipeWireDeviceStreamConfig(node, request, AudioDeviceDirection::Input),
        pick, sink, node.id);
}

} // namespace

std::unique_ptr<IPipeWireDeviceSystem> makePipeWireDeviceSystem()
{
    // R-AUD-32: a test run never reaches a sound server, and never retries.
    const bool barred = audioDevicesBarredForTestRun();
    auto reported = std::make_shared<std::atomic<bool>>(false);
    PipeWireConnector connector = [barred, reported]() -> std::shared_ptr<IPipeWireConnection> {
        auto core = std::make_shared<PipeWireDeviceCore>();
        if (!barred) {
            // The first try that finds no daemon says so; the retries stay quiet.
            core->connect(!reported->exchange(true));
        }
        return core;
    };
    return std::make_unique<ReconnectingPipeWireDeviceSystem>(
        std::move(connector), barred ? 0 : kPipeWireReconnectIntervalMs);
}

#endif // NEREUS_HAVE_PIPEWIRE

} // namespace NereusSDR
