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
#include <condition_variable>
#include <mutex>
#include <utility>

namespace NereusSDR {

struct AudioDeviceCatalog::BackendClaim {
    std::atomic<bool> inUse{false};
};

// What one run's thread shares with the catalogue.  Held by the
// catalogue, the worker and every notice sink, so a thread that outlives
// stop() never reaches freed memory.
struct AudioDeviceCatalog::State {
    std::vector<std::shared_ptr<IAudioEngineBackend>> backends;
    // One per backend, shared with every run: a call into a backend first
    // claims it, so a thread left behind by stop() and a later run never
    // call one backend at once.
    std::vector<std::shared_ptr<BackendClaim>> claims;
    std::atomic<bool> stopped{false};
    // Shared with every run, so a later run's lists always number above
    // an earlier run's lists still in the main thread's queue.
    std::shared_ptr<std::atomic<std::uint64_t>> generations;   // set by stop(); the thread then calls no backend again
    std::atomic<int> debounceMs{kDebounceMs};
    std::atomic<int> busyBackend{-1};   // the backend a call is in, or -1

    // Guards both pointers.  The catalogue clears them in stop(); a post
    // holds the mutex, so neither can be deleted under it.
    std::mutex gateMutex;
    AudioDeviceCatalog* owner = nullptr;
    QObject* worker = nullptr;

    // The first list, handed to start() while it waits.
    std::mutex firstMutex;
    std::condition_variable firstCv;
    std::optional<Listing> firstListing;
    bool waitingForFirst = false;
};

namespace {

QString backendName(AudioBackendId id)
{
    switch (id) {
    case AudioBackendId::PortAudio:
        return QStringLiteral("PortAudio");
    case AudioBackendId::CoreAudio:
        return QStringLiteral("CoreAudio");
    case AudioBackendId::Wasapi:
        return QStringLiteral("Wasapi");
    case AudioBackendId::Asio:
        return QStringLiteral("ASIO");
    case AudioBackendId::PipeWire:
        return QStringLiteral("PipeWire");
    case AudioBackendId::PulseAudio:
        return QStringLiteral("PulseAudio");
    case AudioBackendId::AlsaDirect:
        return QStringLiteral("AlsaDirect");
    }
    return QStringLiteral("unknown");
}

} // namespace

// Lives on the catalogue's thread: the debounce timer and every call to a
// backend's enumerate() and defaultDeviceId().  Deleted on that thread
// when it finishes.
class AudioDeviceCatalog::Worker final : public QObject {
public:
    explicit Worker(std::shared_ptr<State> state)
        : m_state(std::move(state))
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
                m_timer.start(m_state->debounceMs.load());
            }
            return;
        }
        const AudioDeviceDirection direction = notice == AudioNotice::DefaultOutputChanged
            ? AudioDeviceDirection::Output
            : AudioDeviceDirection::Input;
        IAudioEngineBackend& backend = *m_state->backends[std::size_t(index)];
        if (!claim(std::size_t(index))) {
            return;   // still held by a thread stop() left behind
        }
        std::optional<QString> id;
        m_state->busyBackend.store(index);
        if (backend.running()) {
            id = backend.defaultDeviceId(direction);
        }
        m_state->busyBackend.store(-1);
        unclaim(std::size_t(index));
        std::lock_guard<std::mutex> lock(m_state->gateMutex);
        if (AudioDeviceCatalog* owner = m_state->owner) {
            QMetaObject::invokeMethod(owner, [owner, index, direction, id] {
                owner->adoptDefault(index, direction, id);
            }, Qt::QueuedConnection);
        }
    }

    void relist()
    {
        m_timer.stop();
        Listing listing;
        listing.generation = m_state->generations->fetch_add(1) + 1;
        listing.backends.reserve(m_state->backends.size());
        for (std::size_t i = 0; i < m_state->backends.size(); ++i) {
            if (m_state->stopped.load()) {
                return;   // left behind by stop(): call nothing more
            }
            IAudioEngineBackend& backend = *m_state->backends[i];
            BackendSnapshot snap;
            snap.id = backend.id();
            if (!claim(i)) {
                // A thread stop() left behind is still inside this backend.
                // It is listed as not running, with no devices, until the
                // call returns and a later list claims it.
                if (!m_warnedHeld[i]) {
                    m_warnedHeld[i] = true;
                    qCWarning(lcAudio) << "Audio device list skips"
                                       << qPrintable(backendName(snap.id))
                                       << "while an earlier call into it has not returned";
                }
                listing.backends.push_back(std::move(snap));
                continue;
            }
            m_warnedHeld[i] = false;
            m_state->busyBackend.store(int(i));
            snap.running = backend.running();
            if (snap.running) {
                const QList<AudioDeviceInfo> all = backend.enumerate();
                for (const AudioDeviceInfo& info : all) {
                    if (info.direction == AudioDeviceDirection::Output) {
                        snap.outputs.append(info);
                    } else {
                        snap.inputs.append(info);
                    }
                }
                if (!m_state->stopped.load()) {
                    snap.defaultOutput = backend.defaultDeviceId(AudioDeviceDirection::Output);
                    snap.defaultInput = backend.defaultDeviceId(AudioDeviceDirection::Input);
                }
            }
            m_state->busyBackend.store(-1);
            unclaim(i);
            listing.backends.push_back(std::move(snap));
        }

        {
            std::lock_guard<std::mutex> lock(m_state->firstMutex);
            if (m_state->waitingForFirst) {
                m_state->firstListing = listing;
                m_state->waitingForFirst = false;
                m_state->firstCv.notify_all();
            }
        }
        std::lock_guard<std::mutex> lock(m_state->gateMutex);
        if (AudioDeviceCatalog* owner = m_state->owner) {
            QMetaObject::invokeMethod(owner, [owner, listing = std::move(listing)]() mutable {
                owner->adoptListing(std::move(listing));
            }, Qt::QueuedConnection);
        }
    }

    void rescanOlderDrivers()
    {
        for (std::size_t i = 0; i < m_state->backends.size(); ++i) {
            IAudioEngineBackend& backend = *m_state->backends[i];
            if (backend.id() == AudioBackendId::PortAudio && claim(i)) {
                m_state->busyBackend.store(int(i));
                backend.rescan();
                m_state->busyBackend.store(-1);
                unclaim(i);
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
    bool claim(std::size_t index)
    {
        bool expected = false;
        return m_state->claims[index]->inUse.compare_exchange_strong(expected, true);
    }
    void unclaim(std::size_t index) { m_state->claims[index]->inUse.store(false); }

    std::shared_ptr<State> m_state;
    QTimer m_timer;
    std::vector<bool> m_warnedHeld = std::vector<bool>(m_state->backends.size(), false);
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
        m_claims.push_back(std::make_shared<BackendClaim>());
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
    m_state = std::make_shared<State>();
    m_state->backends = m_backends;
    m_state->claims = m_claims;
    m_state->generations = m_generations;
    m_state->debounceMs.store(m_debounceMs.load());
    m_state->owner = this;

    m_thread = std::make_unique<QThread>();
    m_thread->setObjectName(QStringLiteral("AudioDeviceCatalog"));
    auto worker = std::make_unique<Worker>(m_state);
    worker->moveToThread(m_thread.get());
    m_worker = worker.release();   // deletes itself on its thread when the thread finishes
    connect(m_thread.get(), &QThread::finished, m_worker, &QObject::deleteLater);
    m_state->worker = m_worker;

    for (std::size_t i = 0; i < m_backends.size(); ++i) {
        const int index = int(i);
        std::shared_ptr<State> state = m_state;
        m_backends[i]->setNoticeSink([state, index](AudioNotice notice) {
            std::lock_guard<std::mutex> lock(state->gateMutex);
            if (state->worker == nullptr) {
                return;
            }
            auto* target = static_cast<Worker*>(state->worker);
            QMetaObject::invokeMethod(target, [target, index, notice] {
                target->onNotice(index, notice);
            }, Qt::QueuedConnection);
        });
    }

    {
        std::lock_guard<std::mutex> lock(m_state->firstMutex);
        m_state->waitingForFirst = true;
    }
    m_thread->start();
    Worker* target = m_worker;
    QMetaObject::invokeMethod(target, [target] { target->relist(); }, Qt::QueuedConnection);

    std::optional<Listing> first;
    {
        State& state = *m_state;
        std::unique_lock<std::mutex> lock(state.firstMutex);
        const bool listed = state.firstCv.wait_for(lock, std::chrono::milliseconds(kStartWaitMs),
                                                   [&state] { return state.firstListing.has_value(); });
        if (listed) {
            first = std::move(state.firstListing);
            state.firstListing.reset();
        } else {
            // The list still arrives through the main thread's queue.
            state.waitingForFirst = false;
            qCWarning(lcAudio) << "Audio device list took longer than"
                               << kStartWaitMs << "ms; continuing without it; waiting on"
                               << qPrintable(busyBackendName());
        }
    }
    if (first.has_value()) {
        adoptListing(std::move(*first));
    }
}

QString AudioDeviceCatalog::busyBackendName() const
{
    if (!m_state) {
        return QStringLiteral("none");
    }
    const int busy = m_state->busyBackend.load();
    if (busy < 0 || std::size_t(busy) >= m_state->backends.size()) {
        return QStringLiteral("none");
    }
    return backendName(m_state->backends[std::size_t(busy)]->id());
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
        // After this the thread posts nothing to the catalogue, and no
        // notice reaches the worker.
        std::lock_guard<std::mutex> lock(m_state->gateMutex);
        m_state->owner = nullptr;
        m_state->worker = nullptr;
    }
    m_state->stopped.store(true);
    Worker* target = m_worker;
    QMetaObject::invokeMethod(target, [target] { target->shutdown(); }, Qt::QueuedConnection);
    if (m_thread->wait(kStopWaitMs)) {
        m_thread.reset();
    } else {
        // A backend has not returned.  The thread finishes on its own when
        // it does: the worker deletes itself then, the State and backends
        // stay alive through the worker's share, and the thread object is
        // deleted from the main thread's queue.
        qCWarning(lcAudio) << "Audio device list did not stop within" << kStopWaitMs
                           << "ms; leaving it to finish; waiting on" << qPrintable(busyBackendName());
        QThread* orphan = m_thread.release();
        connect(orphan, &QThread::finished, orphan, &QObject::deleteLater);
        if (orphan->isFinished()) {
            // It finished between the wait and the connection.
            orphan->deleteLater();
        }
    }
    m_worker = nullptr;
    m_state.reset();
}

void AudioDeviceCatalog::setDebounceIntervalForTest(int ms)
{
    m_debounceMs.store(ms);
    if (m_state) {
        m_state->debounceMs.store(ms);
    }
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
    if (m_worker == nullptr) {
        return;
    }
    Worker* worker = m_worker;
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
