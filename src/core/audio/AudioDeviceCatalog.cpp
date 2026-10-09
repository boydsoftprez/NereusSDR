// =================================================================
// src/core/audio/AudioDeviceCatalog.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AudioDeviceCatalog.h
// (R-AUD-03).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AudioDeviceCatalog.h"

#include "core/LogCategories.h"

#include <QMetaObject>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <utility>

namespace NereusSDR {

// Lets a backend's notice sink reach the worker from any thread, and stop
// reaching it once the catalogue stops.
struct AudioDeviceCatalog::NoticeGate {
    std::mutex mutex;
    QObject* worker = nullptr;
};

// Lives on the catalogue's thread: the debounce timer and every call to a
// backend's enumerate() and defaultDeviceId().
class AudioDeviceCatalog::Worker final : public QObject {
public:
    explicit Worker(AudioDeviceCatalog* owner)
        : m_owner(owner)
        , m_timer(this)
    {
        m_timer.setSingleShot(true);
        connect(&m_timer, &QTimer::timeout, this, [this] { relist(); });
    }

    void onNotice(int index, AudioNotice notice)
    {
        if (notice == AudioNotice::DevicesChanged) {
            // A notice inside the window joins it; it never restarts it.
            if (!m_timer.isActive()) {
                m_timer.start(m_owner->m_debounceMs.load());
            }
            return;
        }
        const AudioDeviceDirection direction = notice == AudioNotice::DefaultOutputChanged
            ? AudioDeviceDirection::Output
            : AudioDeviceDirection::Input;
        IAudioEngineBackend& backend = *m_owner->m_backends[std::size_t(index)];
        std::optional<QString> id;
        if (backend.running()) {
            id = backend.defaultDeviceId(direction);
        }
        AudioDeviceCatalog* owner = m_owner;
        QMetaObject::invokeMethod(owner, [owner, index, direction, id] {
            owner->adoptDefault(index, direction, id);
        }, Qt::QueuedConnection);
    }

    void relist()
    {
        m_timer.stop();
        Listing listing;
        listing.generation = ++m_generation;
        listing.backends.reserve(m_owner->m_backends.size());
        for (const std::shared_ptr<IAudioEngineBackend>& backend : m_owner->m_backends) {
            BackendSnapshot snap;
            snap.id = backend->id();
            snap.running = backend->running();
            if (snap.running) {
                const QList<AudioDeviceInfo> all = backend->enumerate();
                for (const AudioDeviceInfo& info : all) {
                    if (info.direction == AudioDeviceDirection::Output) {
                        snap.outputs.append(info);
                    } else {
                        snap.inputs.append(info);
                    }
                }
                snap.defaultOutput = backend->defaultDeviceId(AudioDeviceDirection::Output);
                snap.defaultInput = backend->defaultDeviceId(AudioDeviceDirection::Input);
            }
            listing.backends.push_back(std::move(snap));
        }

        {
            std::lock_guard<std::mutex> lock(m_owner->m_firstMutex);
            if (m_owner->m_waitingForFirst) {
                m_owner->m_firstListing = listing;
                m_owner->m_waitingForFirst = false;
                m_owner->m_firstCv.notify_all();
            }
        }
        AudioDeviceCatalog* owner = m_owner;
        QMetaObject::invokeMethod(owner, [owner, listing = std::move(listing)]() mutable {
            owner->adoptListing(std::move(listing));
        }, Qt::QueuedConnection);
    }

    void rescanOlderDrivers()
    {
        for (const std::shared_ptr<IAudioEngineBackend>& backend : m_owner->m_backends) {
            if (backend->id() == AudioBackendId::PortAudio) {
                backend->rescan();
            }
        }
        relist();
    }

    void shutdown()
    {
        m_timer.stop();
        QThread::currentThread()->quit();
    }

private:
    AudioDeviceCatalog* m_owner;
    QTimer m_timer;
    std::uint64_t m_generation = 0;
};

namespace {

void applyDefault(QList<AudioDeviceInfo>& list, const std::optional<QString>& id)
{
    if (!id.has_value()) {
        return;   // the backend's own flags stand
    }
    for (AudioDeviceInfo& info : list) {
        info.isDefault = info.id == *id;
    }
}

} // namespace

AudioDeviceCatalog::AudioDeviceCatalog(std::vector<std::shared_ptr<IAudioEngineBackend>> backends,
                                       QObject* parent)
    : IAudioDeviceCatalog(parent)
    , m_backends(std::move(backends))
{
    qRegisterMetaType<NereusSDR::AudioDeviceDirection>("NereusSDR::AudioDeviceDirection");
    m_snapshot.reserve(m_backends.size());
    for (const std::shared_ptr<IAudioEngineBackend>& backend : m_backends) {
        BackendSnapshot snap;
        snap.id = backend->id();
        m_snapshot.push_back(std::move(snap));
    }
}

AudioDeviceCatalog::~AudioDeviceCatalog()
{
    stop();
}

void AudioDeviceCatalog::start()
{
    if (m_thread) {
        return;
    }
    m_thread = std::make_unique<QThread>();
    m_thread->setObjectName(QStringLiteral("AudioDeviceCatalog"));
    m_worker = std::make_unique<Worker>(this);
    m_worker->moveToThread(m_thread.get());

    m_gate = std::make_shared<NoticeGate>();
    m_gate->worker = m_worker.get();
    for (std::size_t i = 0; i < m_backends.size(); ++i) {
        const int index = int(i);
        std::shared_ptr<NoticeGate> gate = m_gate;
        m_backends[i]->setNoticeSink([gate, index](AudioNotice notice) {
            std::lock_guard<std::mutex> lock(gate->mutex);
            if (gate->worker == nullptr) {
                return;
            }
            auto* worker = static_cast<Worker*>(gate->worker);
            QMetaObject::invokeMethod(worker, [worker, index, notice] {
                worker->onNotice(index, notice);
            }, Qt::QueuedConnection);
        });
    }

    {
        std::lock_guard<std::mutex> lock(m_firstMutex);
        m_firstListing.reset();
        m_waitingForFirst = true;
    }
    m_thread->start();
    Worker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker] { worker->relist(); }, Qt::QueuedConnection);

    std::optional<Listing> first;
    {
        std::unique_lock<std::mutex> lock(m_firstMutex);
        const bool listed = m_firstCv.wait_for(lock, std::chrono::milliseconds(kStartWaitMs),
                                               [this] { return m_firstListing.has_value(); });
        if (listed) {
            first = std::move(m_firstListing);
            m_firstListing.reset();
        } else {
            // The list still arrives through the main thread's queue.
            m_waitingForFirst = false;
            qCWarning(lcAudio) << "Audio device list took longer than"
                               << kStartWaitMs << "ms; continuing without it";
        }
    }
    if (first.has_value()) {
        adoptListing(std::move(*first));
    }
}

void AudioDeviceCatalog::stop()
{
    if (!m_thread) {
        return;
    }
    for (const std::shared_ptr<IAudioEngineBackend>& backend : m_backends) {
        backend->setNoticeSink({});
    }
    {
        std::lock_guard<std::mutex> lock(m_gate->mutex);
        m_gate->worker = nullptr;
    }
    Worker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker] { worker->shutdown(); }, Qt::QueuedConnection);
    m_thread->wait();
    m_worker.reset();
    m_thread.reset();
    m_gate.reset();
    {
        std::lock_guard<std::mutex> lock(m_firstMutex);
        m_waitingForFirst = false;
        m_firstListing.reset();
    }
}

void AudioDeviceCatalog::setDebounceIntervalForTest(int ms)
{
    m_debounceMs.store(ms);
}

QList<AudioBackendId> AudioDeviceCatalog::backends() const
{
    QList<AudioBackendId> ids;
    for (const BackendSnapshot& snap : m_snapshot) {
        ids.append(snap.id);
    }
    return ids;
}

const AudioDeviceCatalog::BackendSnapshot* AudioDeviceCatalog::snapshotFor(AudioBackendId id) const
{
    for (const BackendSnapshot& snap : m_snapshot) {
        if (snap.id == id) {
            return &snap;
        }
    }
    return nullptr;
}

bool AudioDeviceCatalog::backendRunning(AudioBackendId id) const
{
    const BackendSnapshot* snap = snapshotFor(id);
    return snap != nullptr && snap->running;
}

QList<AudioDeviceInfo> AudioDeviceCatalog::devices(AudioBackendId id,
                                                   AudioDeviceDirection direction) const
{
    const BackendSnapshot* snap = snapshotFor(id);
    if (snap == nullptr) {
        return {};
    }
    return direction == AudioDeviceDirection::Output ? snap->outputs : snap->inputs;
}

std::optional<AudioDeviceInfo> AudioDeviceCatalog::defaultDevice(AudioBackendId id,
                                                                 AudioDeviceDirection direction) const
{
    const BackendSnapshot* snap = snapshotFor(id);
    if (snap == nullptr) {
        return std::nullopt;
    }
    const bool output = direction == AudioDeviceDirection::Output;
    const QList<AudioDeviceInfo>& list = output ? snap->outputs : snap->inputs;
    const std::optional<QString>& defaultId = output ? snap->defaultOutput : snap->defaultInput;
    for (const AudioDeviceInfo& info : list) {
        if (defaultId.has_value() ? info.id == *defaultId : info.isDefault) {
            return info;
        }
    }
    return std::nullopt;
}

void AudioDeviceCatalog::rescanOlderDrivers()
{
    if (!m_worker) {
        return;
    }
    Worker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker] { worker->rescanOlderDrivers(); },
                              Qt::QueuedConnection);
}

void AudioDeviceCatalog::adoptListing(Listing listing)
{
    if (listing.generation <= m_adoptedGeneration
        || listing.backends.size() != m_snapshot.size()) {
        return;
    }
    m_adoptedGeneration = listing.generation;

    bool devicesDiffer = false;
    bool outputDefaultDiffers = false;
    bool inputDefaultDiffers = false;
    for (std::size_t i = 0; i < listing.backends.size(); ++i) {
        BackendSnapshot& next = listing.backends[i];
        const BackendSnapshot& prev = m_snapshot[i];
        applyDefault(next.outputs, next.defaultOutput);
        applyDefault(next.inputs, next.defaultInput);
        if (next.running != prev.running || next.outputs != prev.outputs
            || next.inputs != prev.inputs) {
            devicesDiffer = true;
        }
        if (next.defaultOutput != prev.defaultOutput) {
            outputDefaultDiffers = true;
        }
        if (next.defaultInput != prev.defaultInput) {
            inputDefaultDiffers = true;
        }
    }
    m_snapshot = std::move(listing.backends);
    if (devicesDiffer) {
        emit devicesChanged();
    }
    if (outputDefaultDiffers) {
        emit defaultChanged(AudioDeviceDirection::Output);
    }
    if (inputDefaultDiffers) {
        emit defaultChanged(AudioDeviceDirection::Input);
    }
}

void AudioDeviceCatalog::adoptDefault(int index, AudioDeviceDirection direction,
                                      std::optional<QString> id)
{
    if (index < 0 || std::size_t(index) >= m_snapshot.size()) {
        return;
    }
    BackendSnapshot& snap = m_snapshot[std::size_t(index)];
    const bool output = direction == AudioDeviceDirection::Output;
    std::optional<QString>& current = output ? snap.defaultOutput : snap.defaultInput;
    if (current == id) {
        return;
    }
    current = std::move(id);
    applyDefault(output ? snap.outputs : snap.inputs, current);
    emit defaultChanged(direction);
}

} // namespace NereusSDR
