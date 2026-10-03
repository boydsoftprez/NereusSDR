// =================================================================
// src/core/platform/ThreadPlacement.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Chooses which processor cores the
// Core's signal processing threads run on, from the kernel's own CPU
// capacity data, and keeps every other thread off those cores (R-R3-41).
// Thetis has no equivalent; no upstream logic is involved.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-41).
//   2026-09-23: fix wave after the final review: with placement off or
//               inactive, nereusd still owns thread priority and gives
//               signal processing its raised priority without moving
//               threads; raised priority only for a role with a core of
//               its own while placing; a finished WDSP worker is
//               forgotten; the first refusal of each kind is logged once
//               at warning; raised priority is tried once at startup
//               rather than inferred from the limit. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code
//               (R-R3-41).
//   2026-09-23: planRevision(), so the display load governor can keep a
//               copy of the plan without taking the mutex every tick.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code (R-R3-40).
//   2026-09-27: the Protocol 2 transmit I/Q sender is a role, placed and
//               raised while transmitting like the transmit pump. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-IOS-13, R-R3-42).
//   2026-09-23: appliedPlan(), the assignments whose move succeeded, so
//               the governor judges by where threads actually run. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code (R-R3-40, R-R3-41).
//   2026-09-30: RADE decoder role: each RADE decoder thread shares the
//               least busy fast core, judged from the plan (JJ's ruling
//               of 2026-09-30). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QList>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>

namespace NereusSDR {

/// Nice level for signal processing threads when the system permits it.
/// No real-time scheduling policy is used: a systemd service on a kernel
/// with real-time group scheduling is refused one, and a never-sleeping
/// real-time thread starves its core's kernel threads.
constexpr int kDspNice = -10;

/// What the kernel says about the computer's processor cores. Everything is
/// read once, before any thread is moved.
struct CpuTopology {
    QList<int> online;               ///< cpu/online, ascending; empty = unreadable
    QList<int> allowed;              ///< the process's cores at startup; empty = all online
    QMap<int, int> capacity;         ///< cpuN/cpu_capacity, where present and valid
    QList<QList<int>> clockDomains;  ///< cpufreq/policy*/related_cpus
    QList<QList<int>> coreSiblings;  ///< SMT groups (more than one CPU per core)
    QStringList problems;            ///< plain-words notes on missing or bad files
};

/// Reads the topology under `cpuRoot` (normally /sys/devices/system/cpu).
/// `allowed` is the process's affinity mask, read before any pinning.
CpuTopology readCpuTopology(const QString& cpuRoot, const QList<int>& allowed);

/// Parses a kernel CPU list ("0-3,6-7"). Sets *ok to false on anything else.
QList<int> parseCpuList(const QString& text, bool* ok);

/// Formats a CPU list for a log line ("0-3, 6-7"). The kernel's own lists
/// separate ranges with a comma and no space; this adds a space to read.
QString formatCpuList(const QList<int>& cpus);

/// The threads that get a core of their own, in the order they get one.
enum class ThreadRole {
    RxWorker,        ///< a receive channel's WDSP worker (wdspmain)
    DspThread,       ///< RxDspWorker's thread (fexchange2 for every slice)
    TxWorker,        ///< the transmit channel's WDSP worker
    TxWorkerThread,  ///< TxWorkerThread, the transmit audio pump
    TxIqSender,      ///< the Protocol 2 transmit I/Q send thread
    RadeDecoder,     ///< a RADE slice's decoder thread (RadeRx<id>), per channel
};

/// Which signal processing threads are busy.
struct PlacementDemand {
    QList<int> rxChannels;        ///< active receive channels
    bool dspThread{false};
    bool txWorker{false};         ///< transmit channel active
    bool txWorkerThread{false};   ///< transmit pump running while transmitting
    bool txIqSender{false};       ///< transmit I/Q sender running while transmitting
    QList<int> radeDecoders;      ///< running RADE decoder threads, by channel, placed in this order
};

struct RoleAssignment {
    ThreadRole role{ThreadRole::RxWorker};
    int channel{-1};
    int cpu{-1};                  ///< -1: runs with everything else
};

/// The result of planThreadPlacement().
struct PlacementPlan {
    bool active{false};           ///< false: leave every thread where it is
    QString reason;               ///< plain words: why inactive, or the method
    QList<int> signalPool;        ///< cores a thread may own, in the order given
    QList<int> housekeeping;      ///< cores for every other thread
    QList<RoleAssignment> assignments;

    /// The dedicated core of a role, or -1 when it runs on housekeeping.
    int cpuFor(ThreadRole role, int channel = -1) const;
    /// Every core given to a thread, ascending, each once (RADE decoders
    /// share cores).
    QList<int> signalCores() const;
};

/// Pure placement rule.
///
/// When the cores differ in capacity, the fast tier is every core with at
/// least 90% of the largest capacity; fast cores are ordered by capacity,
/// filling one clock domain before the next. With at least two slower
/// cores, those run everything else.
///
/// Otherwise (equal or unknown capacity, or fewer than two slower cores)
/// the highest-numbered physical cores are reserved, never CPU0 or its
/// core, never two SMT siblings of one core, and at most N-2 of N physical
/// cores (N-1 with two or three, none with one); everything else runs on
/// the rest.
///
/// Dedicated cores go, in order, to the first active receive worker (lowest
/// channel), the DSP thread, the transmit worker, the transmit pump, the
/// transmit I/Q sender, then the other active receive workers (G-06). A
/// role left without a core runs on the housekeeping cores.
///
/// RADE decoders come after every other role and share a core rather
/// than own one, on the least busy signal processing core: first the core
/// with the fewest decoders (they spread before any core takes a second),
/// then the fewest roles of any kind (transmit roles included), then a
/// core without a transmit role, then a core without the DSP thread, then
/// the earlier core in signalPool
/// (fastest first). Decoders are placed in the order given, so a later one
/// never moves an earlier one. Judged from the plan only: nothing moves on
/// live load.
PlacementPlan planThreadPlacement(const CpuTopology& topology,
                                  const PlacementDemand& demand);

/// The operating-system calls placement makes, behind an interface so tests
/// can record them.
class ThreadSchedulingApi {
public:
    virtual ~ThreadSchedulingApi() = default;
    virtual qint64 currentThreadId() = 0;
    virtual bool setAffinity(qint64 threadId, const QList<int>& cpus) = 0;
    virtual bool setNice(qint64 threadId, int nice) = 0;
    /// Plain words for the last refused call (empty when unknown).
    virtual QString lastError() const { return {}; }
};

/// The real calls on Linux (sched_setaffinity, setpriority per thread);
/// elsewhere every call does nothing and reports failure.
std::unique_ptr<ThreadSchedulingApi> makeSystemThreadSchedulingApi();

/// Tries `nice` on the calling thread and puts its own level back, so the
/// startup line says what the system actually allows. True on Linux when
/// the system accepted it; false elsewhere.
bool canRaiseCurrentThreadPriority(int nice);

/// Keeps track of the Core's signal processing threads and moves them as
/// channels start and stop. Off (every call a no-op) until start() or
/// startPriorityOnly(); the GUI never starts it.
///
/// Placing: each busy role gets a core of its own, and raised priority
/// while it has one. Priority only (placement off in nereusd.conf, or not
/// possible on this computer): no thread is moved, and each busy role gets
/// raised priority, so signal processing never sits below spectrum and
/// networking, which skip their own priority calls whenever this is on.
///
/// Calls happen when a thread starts or stops, or a channel starts or stops,
/// never per block. A mutex guards the registry.
class ThreadPlacement final {
public:
    ThreadPlacement();
    ~ThreadPlacement();
    ThreadPlacement(const ThreadPlacement&) = delete;
    ThreadPlacement& operator=(const ThreadPlacement&) = delete;

    /// The process-wide registry nereusd starts and WDSP reports to.
    static ThreadPlacement& instance();

    /// True on Linux once instance() is started, placing threads or
    /// managing priority only. Housekeeping threads then skip their own
    /// nice calls, and the DSP thread and the transmit pump register here
    /// instead of asking for real-time policy.
    static bool managesThreadPriority();

    /// WDSP thread hook (WDSPSetThreadStartHook, which also reports a
    /// worker about to end); forwards to instance().onWdspThreadStarted().
    static void wdspThreadStartHook(int kind, int channel);

    /// Starts placing threads. Moves the calling thread to the housekeeping
    /// cores, so threads it creates later start there. Returns the one
    /// startup line for the log. When the plan for `startupDemand` is
    /// inactive, or the system refuses the first move, it manages priority
    /// only (startPriorityOnly) and the line says why.
    QString start(const CpuTopology& topology, const PlacementDemand& startupDemand,
                  std::unique_ptr<ThreadSchedulingApi> api, bool raisePriorityPermitted);

    /// Starts managing priority only: no thread is moved. `reason` says, in
    /// plain words, why threads are not placed. Returns the startup line.
    QString startPriorityOnly(std::unique_ptr<ThreadSchedulingApi> api,
                              bool raisePriorityPermitted, const QString& reason);

    /// Started (placing or priority only).
    bool isActive() const noexcept { return m_active.load(std::memory_order_acquire); }
    /// Started and moving threads between cores.
    bool isPlacing() const;

    /// Called on the thread itself as it starts.
    void registerCurrentThread(ThreadRole role, int channel = -1);
    /// Called on the thread itself before it ends.
    void deregisterCurrentThread();

    /// A channel started or stopped (RxWorker or TxWorker). Kept whether or
    /// not its worker has registered yet.
    void setChannelActive(ThreadRole role, int channel, bool active);

    /// A WDSP channel closed: forget its threads (their IDs can be reused)
    /// and its active state.
    void forgetChannel(int channel);

    /// A WDSP thread started or a worker is about to end (kinds as in
    /// wdsp_api.h). Workers register, and a worker about to end is
    /// forgotten (its ID can be reused); flush threads run with everything
    /// else.
    void onWdspThreadStarted(int kind, int channel);

    /// Moves the calling thread to the first signal processing core, for
    /// FFTW planning, which times its plans on the core it runs on.
    void placeCurrentThreadOnFastCore();

    /// The plan for the current demand (tests and diagnostics).
    PlacementPlan currentPlan() const;
    /// currentPlan() with only the assignments now in force: the role's
    /// thread has registered and its move to that core succeeded. A role
    /// whose move was refused, or whose thread has not registered yet,
    /// is left out, so it counts as sharing the housekeeping cores
    /// (R-R3-40: the display load governor's view). Takes the registry's
    /// mutex; changes only when planRevision() moves.
    PlacementPlan appliedPlan() const;

    /// Changes whenever the plan currentPlan() returns may have changed. A
    /// reader that polls (the display load governor, every 500 ms) keeps
    /// its copy of the plan and calls currentPlan(), which takes the
    /// registry's mutex, only when this has moved. Lock-free.
    quint64 planRevision() const noexcept
    {
        return m_planRevision.load(std::memory_order_acquire);
    }

private:
    struct Registered {
        qint64 threadId{0};
        ThreadRole role{ThreadRole::RxWorker};
        int channel{-1};
        QList<int> appliedCpus;
        int appliedNice{0};
        bool niceApplied{false};
    };

    PlacementDemand demandLocked() const;
    bool roleActiveLocked(ThreadRole role, int channel) const;
    void applyLocked();
    void applyOneLocked(Registered& thread, const PlacementPlan& plan);
    QString startPriorityOnlyLocked(std::unique_ptr<ThreadSchedulingApi> api,
                                    bool raisePriorityPermitted, const QString& reason);
    bool setAffinityLocked(qint64 threadId, const QList<int>& cpus);
    bool setNiceLocked(qint64 threadId, int nice);

    mutable QMutex m_mutex;
    std::atomic<bool> m_active{false};
    std::atomic<quint64> m_planRevision{0};
    CpuTopology m_topology;
    PlacementPlan m_startupPlan;
    std::unique_ptr<ThreadSchedulingApi> m_api;
    bool m_raisePriority{false};
    bool m_placing{false};             // false: priority only
    bool m_affinityRefusalLogged{false};
    bool m_niceRefusalLogged{false};
    QList<Registered> m_threads;
    QMap<int, bool> m_activeRx;   // channel -> active
    QMap<int, bool> m_activeTx;   // channel -> active
};

/// nereusd's startup call, on the main thread before any other thread
/// exists. `enabled` is nereusd.conf's thread_placement (auto = true).
/// On Linux reads /sys/devices/system/cpu and the process's affinity mask,
/// checks RLIMIT_NICE, starts instance() and logs the one startup line.
void startDaemonThreadPlacement(bool enabled, int sliceCount);

} // namespace NereusSDR
