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
// the backend returns; nothing it does reaches the catalogue again, and it
// calls no backend after that call.  Until it returns, a later start() or
// rescan lists that backend as not running and never calls it, so no
// backend is ever called from two threads at once.  That holds for the
// notice sinks too: a sink is set or cleared only by the thread that has
// claimed its backend, so stop() leaves the sink of a backend still in a
// call to that thread, which clears it once the call returns, and a later
// run's sink is set as that thread lets the backend go.  A backend
// skipped that way keeps the defaults it last listed.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: Windows test fix (R-AUD-03): exported from the Core DLL,
//               so a signal of it is found from outside the DLL on
//               Windows. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: final review fix (R-AUD-03, R-AUD-06): notice sinks are
//               set and cleared under the backend's claim; a stopped run
//               rescans nothing. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-10: load fix (R-AUD-03): a test reads how often and how long
//               the debounce window was started, and ends a window itself,
//               so no debounce test rests on the wall clock. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"
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

class NEREUS_CORE_EXPORT AudioDeviceCatalog final : public IAudioDeviceCatalog {
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
    // The windows this run has started, and the length the last one was
    // started with (-1 before the first).  Zero and -1 while stopped.
    int debounceWindowsStartedForTest() const;
    int lastDebounceWindowMsForTest() const;
    // Ends the open window now, as its timer would: one re-list.  Nothing
    // when no window is open.  Runs on the catalogue's thread, after every
    // notice posted before this call.
    void endDebounceWindowForTest();

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
        // Skipped while an earlier call into it has not returned: the
        // defaults last listed stand.
        bool held = false;
    };
    struct Listing {
        std::uint64_t generation = 0;
        std::vector<BackendSnapshot> backends;
    };
    class Worker;
    struct State;
    struct BackendClaim;

    void adoptListing(Listing listing);
    void adoptDefault(int index, AudioDeviceDirection direction, std::optional<QString> id);
    const BackendSnapshot* snapshotFor(AudioBackendId id) const;
    QString busyBackendName() const;

    std::vector<std::shared_ptr<IAudioEngineBackend>> m_backends;
    // One per backend, kept across runs: a backend a thread left behind by
    // stop() is still inside is skipped (listed as not running) until that
    // call returns.
    std::vector<std::shared_ptr<BackendClaim>> m_claims;
    std::shared_ptr<std::atomic<std::uint64_t>> m_generations =
        std::make_shared<std::atomic<std::uint64_t>>(0);

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
