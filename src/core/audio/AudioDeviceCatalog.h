// =================================================================
// src/core/audio/AudioDeviceCatalog.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The live device catalogue
// (R-AUD-03, spec "The device catalogue"); no upstream logic.
//
// The catalogue owns a thread of its own.  Every backend's devices are
// listed there, never on the main thread and never in a device callback.
// A DevicesChanged notice, from whatever thread the system posts it on,
// starts a kDebounceMs window on the catalogue's thread; notices inside
// the window join it without restarting it, and one re-list follows the
// window.  A default-device notice is answered at once with the backend's
// new default, without a re-list.  The results are handed to the main
// thread, where the getters read them and the signals are emitted.  The
// catalogue never touches a stream.
//
// A backend that hangs never hangs the caller: start() waits at most
// kStartWaitMs for the first list and stop() at most kStopWaitMs for the
// thread.  A thread that outlives stop() owns everything it still
// touches (the shared State, the backends, its worker) and frees it when
// the backend returns; nothing it does reaches the catalogue again.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QList>
#include <QString>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QThread;

namespace NereusSDR {

class AudioDeviceCatalog final : public IAudioDeviceCatalog {
    Q_OBJECT
public:
    static constexpr int kDebounceMs = 500;
    static constexpr int kStartWaitMs = 3000;   // start() waits at most this long for the first list
    static constexpr int kStopWaitMs = 3000;    // stop() waits at most this long for the thread

    // backends in R-AUD-01 order; backends() returns them in this order.
    explicit AudioDeviceCatalog(std::vector<std::shared_ptr<IAudioEngineBackend>> backends,
                                QObject* parent = nullptr);
    ~AudioDeviceCatalog() override;      // stops its thread

    void start();                        // lists once; waits at most 3 s for the first list
    void stop();
    void setDebounceIntervalForTest(int ms);

    QList<AudioBackendId> backends() const override;
    bool backendRunning(AudioBackendId id) const override;
    QList<AudioDeviceInfo> devices(AudioBackendId id, AudioDeviceDirection direction) const override;
    std::optional<AudioDeviceInfo> defaultDevice(AudioBackendId id,
                                                 AudioDeviceDirection direction) const override;
    void rescanOlderDrivers() override;

private:
    struct BackendSnapshot {
        AudioBackendId id = AudioBackendId::PortAudio;
        bool running = false;
        QList<AudioDeviceInfo> outputs;
        QList<AudioDeviceInfo> inputs;
        std::optional<QString> defaultOutput;
        std::optional<QString> defaultInput;
    };
    struct Listing {
        std::uint64_t generation = 0;
        std::vector<BackendSnapshot> backends;
    };
    class Worker;
    struct State;

    void adoptListing(Listing listing);
    void adoptDefault(int index, AudioDeviceDirection direction, std::optional<QString> id);
    const BackendSnapshot* snapshotFor(AudioBackendId id) const;
    QString busyBackendName() const;

    std::vector<std::shared_ptr<IAudioEngineBackend>> m_backends;

    // Main thread only.
    std::vector<BackendSnapshot> m_snapshot;
    std::uint64_t m_adoptedGeneration = 0;

    // One run's thread, worker and the state they share.  The worker
    // deletes itself on its own thread when the thread finishes; the
    // thread is deleted here, or by itself when it outlived stop().
    std::unique_ptr<QThread> m_thread;
    Worker* m_worker = nullptr;
    std::shared_ptr<State> m_state;
    std::atomic<int> m_debounceMs{kDebounceMs};
};

} // namespace NereusSDR
