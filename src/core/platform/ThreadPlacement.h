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
//   2026-10-10: radio connection and spectrum roles, from an evening of
//               measuring on the Rock 5C with wide receivers (the numbers
//               are in ThreadPlacement.cpp). The connection thread shares
//               the DSP thread's core, both raised: neither runs longer
//               than a millisecond at a stretch. Spectrum threads may use
//               every fast core at normal priority and own none: held to
//               one core they sat behind a receive worker's burst while
//               holding a lock the connection thread needs. A receive
//               worker left without a core shares the later receivers'
//               fast cores instead of the slow ones. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-10-11: TransmitUse and setTransmitUse(): a transmit channel
//               that is up only so VOX can hear the microphone comes
//               after every receive worker. Seated as if keyed, its
//               worker and pump held two of the Rock's four fast cores
//               for as long as VOX was on. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-14: TxIqSender's comment names the P1 EP2 sender as well,
//               which takes the same role. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-15: the connection thread shares the DSP thread's core only
//               where slower cores run housekeeping; on equal cores it
//               stays on the housekeeping cores. While transmitting,
//               spectrum threads keep off the transmit roles' cores. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-27: CW transmit, the CWX sender role (R-CW-31): it shares the
//               DSP thread's core where the connection thread does, and
//               runs with everything else elsewhere. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
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
    TxIqSender,      ///< the protocol's transmit send thread (P2 TX I/Q sender or P1 EP2 sender)
    RadeDecoder,     ///< a RADE slice's decoder thread (RadeRx<id>), per channel
    ConnectionThread, ///< the radio connection thread (reads the radio's UDP)
    SpectrumThread,  ///< a spectrum FFT thread, told apart by a number FftEnginePool gives each
    CwxSender,       ///< the CWX sender's element timing thread (R-CW-31)
};

/// Which signal processing threads are busy.
struct PlacementDemand {
    QList<int> rxChannels;        ///< active receive channels
    bool dspThread{false};
    bool txWorker{false};         ///< transmit channel active
    /// The transmit channel runs only so VOX can hear the microphone, and
    /// nothing is on the air. Read only with txWorker.
    bool txListening{false};
    bool txWorkerThread{false};   ///< transmit pump running while transmitting
    bool txIqSender{false};       ///< transmit I/Q sender running while transmitting
    QList<int> radeDecoders;      ///< running RADE decoder threads, by channel, placed in this order
    bool connectionThread{false}; ///< the radio connection thread is running
    QList<int> spectrumThreads;   ///< running spectrum threads, by FftEnginePool's number
    bool cwxSender{false};        ///< the CWX sender's thread is running
};

struct RoleAssignment {
    ThreadRole role{ThreadRole::RxWorker};
    int channel{-1};
    int cpu{-1};                  ///< its core; -1: it has none
    /// With no core of its own: the signal processing cores it may use,
    /// ascending. The kernel moves it between them. Empty with cpu -1: it
    /// runs with everything else.
    QList<int> shared{};
    /// Runs at kDspNice where the system permits it.
    bool raised{false};
};

/// The result of planThreadPlacement().
struct PlacementPlan {
    bool active{false};           ///< false: leave every thread where it is
    QString reason;               ///< plain words: why inactive, or the method
    QList<int> signalPool;        ///< cores a thread may own, in the order given
    QList<int> housekeeping;      ///< cores for every other thread
    QList<RoleAssignment> assignments;

    /// The dedicated core of a role, or -1 when it has none.
    int cpuFor(ThreadRole role, int channel = -1) const;
    /// The signal processing cores a role may run on: its own core, or the
    /// cores it shares. Empty when it runs on housekeeping.
    QList<int> coresFor(ThreadRole role, int channel = -1) const;
    /// True when the role runs at raised priority on those cores.
    bool raisedFor(ThreadRole role, int channel = -1) const;
    /// Every core given to a thread as its own, ascending, each once (RADE
    /// decoders share cores).
    QList<int> signalCores() const;
    /// True when a spectrum thread may run on `cpu`.
    bool carriesSpectrum(int cpu) const;
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
/// transmit I/Q sender, then the other active receive workers (G-06). Each
/// runs at raised priority on its core.
///
/// That order is for transmitting. A transmit channel that runs only so
/// VOX can hear the microphone (txListening) puts nothing on the air, and
/// with VOX on it runs through every receive period: its three roles come
/// after every receive worker and take the cores those leave.
///
/// Where slower cores run everything else, the radio connection thread
/// shares the DSP thread's core, raised like it. It has the one hard
/// deadline in the Core: the radio's packets wait
/// in a socket buffer that holds a few milliseconds of wide receivers, and
/// whatever does not fit is lost. Neither thread runs longer than a
/// millisecond at a stretch, so neither keeps the other waiting, where a
/// receive worker runs for tens of milliseconds and must share with
/// neither. With no DSP thread the connection thread takes that core
/// alone; with no core for the DSP thread both run on the housekeeping
/// cores at normal priority. Where every core is equally fast it stays on
/// the housekeeping cores at normal priority: it was measured only on the
/// Rock 5C, and the Pi 4 (the Hermes Lite 2 bench) has fewer cores to
/// share.
///
/// Where slower cores run everything else, a receive worker left without a
/// core shares the cores of the later receive workers that have one, raised
/// like them: a slower core costs a wide receiver more than twice the
/// time. With no such core (transmit holding them, or too few), or where
/// every core is equally fast, it runs on the housekeeping cores at normal
/// priority, like any other role without a core. A transmit worker
/// listening for VOX and left without a core shares the same cores the
/// same way: it does as much work listening as transmitting.
///
/// Spectrum threads own no core. Where slower cores run everything else,
/// they may use every signal processing core, at normal priority (while
/// transmitting, every one the transmit roles leave), so they
/// take the time signal processing leaves and the kernel moves them off a
/// core a receive worker wants. They must not be held to one core: a
/// receive worker's burst there (50 ms at 1,536,000) would keep a spectrum
/// thread off the processor while it holds its event queue's lock, and the
/// connection thread, which takes that lock to hand it samples, would wait
/// the burst out and lose the radio's packets. Where all cores are equally
/// fast they run on the housekeeping cores with everything else.
///
/// The CWX sender's thread shares the DSP thread's core, raised, wherever
/// the connection thread does (slower cores run everything else and the
/// DSP thread has a core). It wakes once per CW element and runs for
/// microseconds, but each key edge it writes must land on its element's
/// boundary. Elsewhere it runs with everything else at normal priority.
/// It is not counted when RADE decoders choose their cores.
///
/// RADE decoders come after every role with a core and share one rather
/// than own one, on the least busy signal processing core: first the core
/// with the fewest decoders (they spread before any core takes a second),
/// then a core without the connection thread, then the fewest roles of any
/// kind (transmit roles included), then a core without a transmit role,
/// then a core without the DSP thread, then the earlier core in signalPool
/// (fastest first). Decoders are placed in the order given, so a later one
/// never moves an earlier one. Judged from the plan only: nothing moves on
/// live load.
PlacementPlan planThreadPlacement(const CpuTopology& topology,
                                  const PlacementDemand& demand);

/// What a transmit channel is doing, for ThreadPlacement::setTransmitUse().
enum class TransmitUse {
    Stopped,       ///< the channel is off
    Listening,     ///< running only so VOX can hear the microphone
    Transmitting,  ///< keyed
};

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
/// Placing: each busy role gets the cores the plan gives it, and raised
/// priority where the plan says so. Priority only (placement off in nereusd.conf, or not
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
    /// not its worker has registered yet. A transmit channel reported here
    /// as started is taken to be transmitting.
    void setChannelActive(ThreadRole role, int channel, bool active);

    /// A transmit channel stopped, started to listen for VOX, or keyed.
    /// Kept whether or not its worker has registered yet.
    void setTransmitUse(int channel, TransmitUse use);

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
    QMap<int, bool> m_listeningTx;  // channel -> active only to listen for VOX
};

/// nereusd's startup call, on the main thread before any other thread
/// exists. `enabled` is nereusd.conf's thread_placement (auto = true).
/// On Linux reads /sys/devices/system/cpu and the process's affinity mask,
/// checks RLIMIT_NICE, starts instance() and logs the one startup line.
void startDaemonThreadPlacement(bool enabled, int sliceCount);

} // namespace NereusSDR
