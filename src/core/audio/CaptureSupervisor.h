// =================================================================
// src/core/audio/CaptureSupervisor.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Parent-side supervision of the
// nereus-audio-capture helper process: demand leases, generations,
// deadlines and status; no Thetis logic.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// (Ownership and interfaces; Process and PCM contract).  Requirement R-R3-36.
//
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): a remote window's
//               microphone uplink is a demand too (Demand::RemoteWindow).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/AudioDeviceConfig.h"

#include <QHash>
#include <QMetaType>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QThread>

#include <atomic>
#include <memory>

namespace NereusSDR {

class CaptureAudioBus;
class CaptureSupervisorWorker;

// CaptureSupervisor::Options.  Declared at namespace scope only so that it
// can be the constructor's default argument.
struct CaptureSupervisorOptions {
    QString program;            // empty: locateCaptureHelper(); still empty means HelperMissing
    QStringList arguments;
    int helloTimeoutMs = 3000;
    int openTimeoutMs = 10000;
    int stopTimeoutMs = 2000;
};

// Owns the helper process on a private I/O thread and publishes its status.
//
// Demand: the first active Lease starts the helper and opens a new
// generation (Configure then Open); releasing the last Lease stops it (Stop,
// at most stopTimeoutMs, then kill) and returns to Closed.  No demand means
// no helper process.
//
// Ready requires the helper's Ready status for the current generation and
// at least one valid PCM record of that generation.  Records of any other
// generation are dropped.
//
// Failed persists; the supervisor never retries by itself.  retry(),
// configure() with a different config, or a new demand after demand had
// dropped to zero start a new generation.  With no demand, retry() and a
// configuration change clear Failed back to Closed.  A failure that is
// current when demand drops to zero stays published until then.
//
// Threading: construct, call and destroy on one owner thread (the GUI
// thread in the application).  The QProcess lives on the supervisor's own
// QThread; PCM goes straight from that thread into reader(); statusChanged
// is emitted on the owner thread.  No public call waits on the helper,
// except shutdown(), which is bounded by stopTimeoutMs + 500 ms.
class CaptureSupervisor final : public QObject {
    Q_OBJECT

public:
    /// RemoteWindow (iPhone app plan Task 36): a remote window sending the
    /// microphone chosen in Audio > Devices to its Core, while it transmits.
    enum class Demand { LocalSession, TestMic, RemoteWindow };

    struct Status {
        enum class State { Closed, PreparingPermission, Opening, Ready, Failed, Stopping };
        enum class Reason { None, PermissionDenied, DeviceNotFound, OpenFailed, StartFailed,
                            InputLost, Timeout, HelperMissing, HelperDidNotStart,
                            HelperExited, ProtocolError };
        State state = State::Closed;
        QString configuredDevice;   // empty = system default
        QString actualDevice;
        Reason reason = Reason::None;
        quint32 generation = 0;
        friend bool operator==(const Status&, const Status&) = default;
    };

    // One unit of capture demand.  Move-only.  Releasing twice, or after the
    // supervisor was shut down or destroyed, does nothing.
    class Lease {
    public:
        Lease();
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        ~Lease();

        void release();
        bool isActive() const;

    private:
        friend class CaptureSupervisor;
        Lease(CaptureSupervisor* owner, quint64 id);

        QPointer<CaptureSupervisor> m_owner;
        quint64 m_id = 0;
    };

    using Options = CaptureSupervisorOptions;

    explicit CaptureSupervisor(Options options = {}, QObject* parent = nullptr);
    ~CaptureSupervisor() override;          // calls shutdown()

    Lease acquire(Demand demand);           // owner thread only; inactive Lease after shutdown()
    void configure(const AudioDeviceConfig& config);
    void retry();
    Status status() const;
    CaptureAudioBus* reader() const;        // same pointer for the supervisor's lifetime
    void shutdown();                        // idempotent; returns within stopTimeoutMs + 500 ms

    // Diagnostics: the running helper's process id, 0 when none.
    qint64 helperProcessId() const;

    // True while at least one Lease is active.  Owner thread only.
    bool hasDemand() const { return !m_leases.isEmpty(); }

signals:
    void statusChanged(const NereusSDR::CaptureSupervisor::Status& status);

private:
    void releaseLease(quint64 id);
    bool hasLease(quint64 id) const;
    void onWorkerStatus(const Status& status);
    // Marks the owner's status copy non-Ready before a restart is queued,
    // so no caller sees the old Ready while the capture thread retires it.
    void markRestarting(const QString* configuredDevice);

    Options m_options;
    std::shared_ptr<CaptureAudioBus> m_reader;
    std::shared_ptr<std::atomic<qint64>> m_helperPid;
    QThread m_thread;
    std::unique_ptr<CaptureSupervisorWorker> m_worker;   // lives on m_thread
    QHash<quint64, Demand> m_leases;
    quint64 m_nextLeaseId = 1;
    AudioDeviceConfig m_config;
    bool m_shutDown = false;

    mutable QMutex m_statusMutex;
    Status m_status;
    // A Ready for a generation below this was queued before a restart and
    // is stale; it is dropped. Owner thread only.
    quint32 m_readyGenerationFloor = 0;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::CaptureSupervisor::Status)
