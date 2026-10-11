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
//   2026-10-08: native audio plan Task 1 (V-HW-8): setProbeEnabled() sends
//               ProbeEnable once the helper is Ready; probeHit() forwards
//               the helper's ProbeHit records. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 (R-AUD-02): config() returns the
//               config the last configure() applied. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17, R-AUD-18): the audio
//               comes through a shared clock matcher ring; Ready needs the
//               helper's Ready and the ring's first wake; Status carries
//               the device's rate, latency and buffer, the measured wake
//               hop, the device-in-use reason and the request it answers.
//               Exported for the tests.  J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 15 (R-AUD-19, R-AUD-20, R-AUD-21):
//               Demand::AsioDevice keeps the helper running for the ASIO
//               outputs with no microphone open; describeAsio(),
//               openAsio() and openAsioControlPanel() go to the helper,
//               asioCaps() and asioState() come back.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-R3-36):
//               lastHelperProcessId(), for a helper that exits at once.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-R3-36):
//               fireDeadlinesNowForTest(), to fire a deadline early.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/AudioDeviceConfig.h"
#include "core/NereusCoreExport.h"
#include "core/audio/CaptureProtocol.h"

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
#include <optional>

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
// Each generation gets its own shared-memory region and wake (CaptureShm),
// handed to the helper with AttachRing; the helper builds the clock
// matcher in it and posts the wake after every write.  Ready requires the
// helper's Ready status for the current generation and the region's first
// wake with a valid ring.  Records of any other generation are dropped; a
// Pcm record is a protocol error.
//
// Failed persists; the supervisor never retries by itself.  retry(),
// configure() with a different config, or a new demand after demand had
// dropped to zero start a new generation.  With no demand, retry() and a
// configuration change clear Failed back to Closed.  A failure that is
// current when demand drops to zero stays published until then.
//
// Threading: construct, call and destroy on one owner thread (the GUI
// thread in the application).  The QProcess lives on the supervisor's own
// QThread; a wake thread per generation follows the ring; statusChanged
// is emitted on the owner thread.  No public call waits on the helper,
// except shutdown(), which is bounded by stopTimeoutMs + 500 ms.
class NEREUS_CORE_EXPORT CaptureSupervisor final : public QObject {
    Q_OBJECT

public:
    /// RemoteWindow (iPhone app plan Task 36): a remote window sending the
    /// microphone chosen in Audio > Devices to its Core, while it transmits.
    /// AsioDevice (native audio plan Task 15): an output or input on ASIO,
    /// which runs in the helper.  It keeps the helper running and opens no
    /// microphone; the other demands are the microphone's.
    enum class Demand { LocalSession, TestMic, RemoteWindow, AsioDevice };

    struct Status {
        enum class State { Closed, PreparingPermission, Opening, Ready, Failed, Stopping };
        enum class Reason { None, PermissionDenied, DeviceNotFound, OpenFailed, StartFailed,
                            InputLost, Timeout, HelperMissing, HelperDidNotStart,
                            HelperExited, ProtocolError, DeviceInUse };
        State state = State::Closed;
        QString configuredDevice;   // empty = system default
        QString actualDevice;
        Reason reason = Reason::None;
        quint32 generation = 0;
        // From the helper's Ready (0 until then): the device's own rate,
        // the latency its engine reports and its buffer, in ms.
        int nativeRate = 0;
        double deviceLatencyMs = 0.0;
        double deviceBufferMs = 0.0;
        // The median time from the helper's ring write to the window's
        // wake over the last delay window; a measurement, so it is not
        // part of ==.  nullopt until the first window closes.
        std::optional<double> hopMs;
        // The requestSerial() this status answers (configure, retry or the
        // first demand): a status of an earlier request is stale.
        quint64 request = 0;
        friend bool operator==(const Status& a, const Status& b)
        {
            return a.state == b.state && a.configuredDevice == b.configuredDevice
                && a.actualDevice == b.actualDevice && a.reason == b.reason
                && a.generation == b.generation && a.nativeRate == b.nativeRate
                && a.deviceLatencyMs == b.deviceLatencyMs
                && a.deviceBufferMs == b.deviceBufferMs && a.request == b.request;
        }
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
    // The config the last configure() applied (native audio plan Task 7).
    AudioDeviceConfig config() const { return m_config; }
    void retry();
    // Counts the requests that start a generation: a configure() that
    // changes the config, retry(), and the first demand.  Each status
    // carries the serial of the request it answers (Status::request).
    quint64 requestSerial() const { return m_requestSerial; }
    Status status() const;
    CaptureAudioBus* reader() const;        // same pointer for the supervisor's lifetime
    void shutdown();                        // idempotent; returns within stopTimeoutMs + 500 ms

    // Diagnostics: the running helper's process id, 0 when none.
    qint64 helperProcessId() const;
    // Diagnostics: the latest helper's process id, kept after it ends (a
    // helper that answers and exits at once is still seen); 0 before the
    // first.
    qint64 lastHelperProcessId() const;

    // For tests: runs the handler of each named deadline that is armed at
    // once, as a timer firing before its interval would.  Returns the
    // deadlines it ran.  Owner thread only; returns after the capture
    // thread has run them.
    enum DeadlineForTest { HelloDeadline = 1, OpenDeadline = 2, StopDeadline = 4 };
    int fireDeadlinesNowForTest(int deadlines);

    // True while at least one microphone Lease is active (any demand but
    // AsioDevice).  Owner thread only.
    bool hasDemand() const { return micLeaseCount() > 0; }

    // ASIO (native audio plan Task 15).  Owner thread only.  Each starts
    // the helper when none runs.  describeAsio("") asks for the installed
    // drivers; a name asks for that driver's caps too (asioCaps()).  A
    // helper started only to describe stops after the answer when nothing
    // else holds it.  openAsio() replaces the uses the helper plays; an
    // open with no uses closes the session.  The AsioState answers come
    // through asioState().
    void describeAsio(const QString& driver);
    void openAsio(const CaptureProtocol::AsioOpen& open);
    void openAsioControlPanel();

    // Audio delay probe (V-HW-8).  Enabled: ProbeEnable {"enabled":true}
    // goes to the helper once it is Ready for the current generation (and
    // again on every later Ready); disabled: ProbeEnable {"enabled":false}
    // goes to a running helper at once.  Holds no demand by itself.
    void setProbeEnabled(bool enabled);

signals:
    void statusChanged(const NereusSDR::CaptureSupervisor::Status& status);
    // A ProbeHit from the helper while the probe is enabled, on the owner
    // thread: the steady-clock time the click reached the input converter.
    void probeHit(qint64 captureNs);
    // ASIO answers from the helper, on the owner thread (Task 15).  A
    // helper that ends while an ASIO open is held reports Failed.
    void asioCaps(const NereusSDR::CaptureProtocol::AsioCapsRecord& caps);
    void asioState(const NereusSDR::CaptureProtocol::AsioState& state);

private:
    void releaseLease(quint64 id);
    int micLeaseCount() const;
    int asioLeaseCount() const;
    bool hasLease(quint64 id) const;
    void onWorkerStatus(const Status& status);
    void onWorkerHop(quint32 generation, double hopMs);
    // Marks the owner's status copy non-Ready before a restart is queued,
    // so no caller sees the old Ready while the capture thread retires it.
    void markRestarting(const QString* configuredDevice);

    Options m_options;
    std::shared_ptr<CaptureAudioBus> m_reader;
    std::shared_ptr<std::atomic<qint64>> m_helperPid;
    std::shared_ptr<std::atomic<qint64>> m_lastHelperPid;
    QThread m_thread;
    std::unique_ptr<CaptureSupervisorWorker> m_worker;   // lives on m_thread
    QHash<quint64, Demand> m_leases;
    quint64 m_nextLeaseId = 1;
    AudioDeviceConfig m_config;
    bool m_shutDown = false;
    bool m_probeEnabled = false;
    quint64 m_requestSerial = 0;

    mutable QMutex m_statusMutex;
    Status m_status;
    // A Ready for a generation below this was queued before a restart and
    // is stale; it is dropped. Owner thread only.
    quint32 m_readyGenerationFloor = 0;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::CaptureSupervisor::Status)
Q_DECLARE_METATYPE(NereusSDR::CaptureProtocol::AsioCapsRecord)
Q_DECLARE_METATYPE(NereusSDR::CaptureProtocol::AsioState)
