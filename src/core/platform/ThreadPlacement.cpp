// =================================================================
// src/core/platform/ThreadPlacement.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See ThreadPlacement.h. Chooses cores
// from the kernel's CPU capacity data; no upstream logic is involved.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-41).
//   2026-09-23: fix wave after the final review (see ThreadPlacement.h):
//               priority-only mode, raised priority only with a core of
//               its own while placing, worker exit, refusals logged once,
//               startup priority probe. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code
//               (R-R3-41).
//   2026-09-23: planRevision() moves on every plan change (R-R3-40).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-23: appliedPlan(): only the assignments whose move succeeded
//               (R-R3-40, R-R3-41). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: the transmit I/Q sender role (R-IOS-13, R-R3-42). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: RADE decoder role, placed on the least busy fast core by
//               the plan (JJ's ruling of 2026-09-30), each choice logged.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-10: radio connection and spectrum roles, and a receive
//               worker without a core shares the later receivers' cores
//               (see ThreadPlacement.h; measured on the Rock 5C, numbers
//               at planThreadPlacement). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-11: a transmit channel listening for VOX is seated after the
//               receive workers (setTransmitUse); as if keyed it held two
//               fast cores for as long as VOX was on. The comment at the
//               spectrum threads records what a raise does not buy,
//               measured on the installed build. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-10-11: that comment corrected: the wait is not held to two
//               ticks. A thread that has just woken keeps the core for up
//               to half the scheduling latency (12.1 ms measured). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-11: that comment corrected again: the Rock 5C does not refuse
//               real-time priority to a service outright, only while a
//               root login has the CPU controller switched on. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-15: fix round after the review: the connection thread moves
//               to the DSP thread's core only where slower cores run
//               housekeeping (on equal cores it was not measured and stays
//               on the housekeeping cores), and while transmitting the
//               spectrum threads keep off the transmit roles' cores. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-27: CW transmit, the CWX sender role (R-CW-31), on the DSP
//               thread's core where the connection thread shares it. J.J.
//               Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-26: CW transmit, the serial key reader role (R-CW-36), placed
//               as the CWX sender is. J.J. Boyd (KG4VCF), AI-assisted via
//               Claude Code.
// =================================================================

#include "core/platform/ThreadPlacement.h"

#include "core/LogCategories.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/wdsp_api.h"

#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QRegularExpression>

#include <algorithm>
#include <functional>
#include <set>
#include <tuple>
#include <utility>

#ifdef Q_OS_LINUX
#include <cerrno>
#include <cstring>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace NereusSDR {

namespace {

// The fast tier is every core with at least 90% of the largest capacity,
// so a second cluster a few percent slower (982 against 1024 on the
// RK3588S) still counts as fast.
constexpr int kFastTierPercent = 90;

// Kernel CPU lists are short ("0-3,6-7"); anything longer is not one.
constexpr qint64 kMaxSysfsBytes = 4096;

QString readSmallFile(const QString& path, bool* exists)
{
    QFile file(path);
    *exists = file.exists();
    if (!*exists || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromLatin1(file.read(kMaxSysfsBytes)).trimmed();
}

int physicalCoreOf(const CpuTopology& topology, int cpu)
{
    for (const QList<int>& group : topology.coreSiblings) {
        if (group.contains(cpu)) {
            return *std::min_element(group.cbegin(), group.cend());
        }
    }
    return cpu;
}

int clockDomainOf(const CpuTopology& topology, int cpu)
{
    for (int i = 0; i < topology.clockDomains.size(); ++i) {
        if (topology.clockDomains.at(i).contains(cpu)) {
            return i;
        }
    }
    // A core with no policy is its own domain, after every listed one.
    return topology.clockDomains.size() + cpu;
}

QString priorityClause(bool raisePriority)
{
    return raisePriority
        ? QStringLiteral("signal processing priority raised.")
        : QStringLiteral("raising signal processing priority is not"
                         " permitted, so it runs at normal priority.");
}

QString activeLine(const PlacementPlan& plan, bool raisePriority)
{
    return QStringLiteral("Thread placement: signal processing runs on cores %1,"
                          " everything else on cores %2 (%3); %4")
        .arg(formatCpuList(plan.signalCores()), formatCpuList(plan.housekeeping),
             plan.reason, priorityClause(raisePriority));
}

QString inactiveLine(const QString& reason)
{
    return QStringLiteral("Thread placement: every thread may run on any core,"
                          " because %1.").arg(reason);
}

// Priority only: no thread is moved, but signal processing still gets its
// priority, above spectrum and networking or level with them.
QString priorityOnlyLine(const QString& reason, bool raisePriority)
{
    return QStringLiteral("Thread placement: every thread may run on any core,"
                          " because %1; %2").arg(reason, priorityClause(raisePriority));
}

bool isChannelRole(ThreadRole role)
{
    return role == ThreadRole::RxWorker || role == ThreadRole::TxWorker;
}

// Roles with one thread per channel in a plan. One transmit channel: the
// transmit worker is not told apart by channel.
bool assignedPerChannel(ThreadRole role)
{
    return role == ThreadRole::RxWorker || role == ThreadRole::RadeDecoder
        || role == ThreadRole::SpectrumThread;
}

bool isTransmitRole(ThreadRole role)
{
    return role == ThreadRole::TxWorker || role == ThreadRole::TxWorkerThread
        || role == ThreadRole::TxIqSender;
}

// Roles that are housekeeping threads like any other while no thread is
// moved (priority only): raised there, they would sit level with signal
// processing on whatever core the kernel chose.
bool housekeepingWithoutPlacement(ThreadRole role)
{
    return role == ThreadRole::ConnectionThread || role == ThreadRole::SpectrumThread;
}

const RoleAssignment* findAssignment(const QList<RoleAssignment>& assignments,
                                     ThreadRole role, int channel)
{
    for (const RoleAssignment& a : assignments) {
        // One transmit channel: only receive workers, RADE decoders and
        // spectrum threads are told apart by channel.
        if (a.role == role && (!assignedPerChannel(role) || a.channel == channel)) {
            return &a;
        }
    }
    return nullptr;
}

QList<int> coresOf(const RoleAssignment& a)
{
    return a.cpu >= 0 ? QList<int>{a.cpu} : a.shared;
}

} // namespace

// ---------------------------------------------------------------- CPU lists

QList<int> parseCpuList(const QString& text, bool* ok)
{
    QList<int> cpus;
    bool good = !text.trimmed().isEmpty();
    // Most files use commas ("0-3,6-7"); cpufreq's related_cpus uses spaces.
    static const QRegularExpression separators(QStringLiteral("\\s*,\\s*|\\s+"));
    const QStringList parts = text.trimmed().split(separators);
    for (const QString& rawPart : parts) {
        if (!good) {
            break;
        }
        const QString part = rawPart.trimmed();
        const int dash = part.indexOf(QLatin1Char('-'));
        bool okFirst = false;
        bool okLast = false;
        int first = 0;
        int last = 0;
        if (dash < 0) {
            first = part.toInt(&okFirst);
            last = first;
            okLast = okFirst;
        } else {
            first = part.left(dash).toInt(&okFirst);
            last = part.mid(dash + 1).toInt(&okLast);
        }
        // CPU numbers are small; a huge range is garbage, not a machine.
        if (!okFirst || !okLast || first < 0 || last < first || last > 4095) {
            good = false;
            break;
        }
        for (int cpu = first; cpu <= last; ++cpu) {
            cpus.append(cpu);
        }
    }
    if (ok) {
        *ok = good;
    }
    if (!good) {
        return {};
    }
    std::sort(cpus.begin(), cpus.end());
    cpus.erase(std::unique(cpus.begin(), cpus.end()), cpus.end());
    return cpus;
}

QString formatCpuList(const QList<int>& input)
{
    QList<int> cpus = input;
    std::sort(cpus.begin(), cpus.end());
    QStringList ranges;
    for (int i = 0; i < cpus.size();) {
        int j = i;
        while (j + 1 < cpus.size() && cpus.at(j + 1) == cpus.at(j) + 1) {
            ++j;
        }
        ranges.append(i == j ? QString::number(cpus.at(i))
                             : QStringLiteral("%1-%2").arg(cpus.at(i)).arg(cpus.at(j)));
        i = j + 1;
    }
    return ranges.join(QStringLiteral(", "));
}

// ---------------------------------------------------------------- topology

CpuTopology readCpuTopology(const QString& cpuRoot, const QList<int>& allowed)
{
    CpuTopology topology;
    topology.allowed = allowed;
    const QDir root(cpuRoot);

    bool exists = false;
    bool ok = false;
    const QString onlineText = readSmallFile(root.filePath(QStringLiteral("online")), &exists);
    topology.online = parseCpuList(onlineText, &ok);
    if (!ok) {
        topology.online.clear();
        topology.problems.append(QStringLiteral("the list of online cores could not be read"));
        return topology;
    }

    int missingCapacity = 0;
    for (int cpu : std::as_const(topology.online)) {
        const QString cpuDir = QStringLiteral("cpu%1/").arg(cpu);
        const QString capText = readSmallFile(
            root.filePath(cpuDir + QStringLiteral("cpu_capacity")), &exists);
        bool capOk = false;
        const int capacity = capText.toInt(&capOk);
        if (capOk && capacity > 0) {
            topology.capacity.insert(cpu, capacity);
        } else {
            ++missingCapacity;
            if (exists) {
                topology.problems.append(
                    QStringLiteral("core %1 capacity is unreadable").arg(cpu));
            }
        }

        QString siblingsText = readSmallFile(
            root.filePath(cpuDir + QStringLiteral("topology/core_cpus_list")), &exists);
        if (!exists) {
            siblingsText = readSmallFile(
                root.filePath(cpuDir + QStringLiteral("topology/thread_siblings_list")),
                &exists);
        }
        bool sibOk = false;
        const QList<int> siblings = parseCpuList(siblingsText, &sibOk);
        if (sibOk && siblings.size() > 1 && !topology.coreSiblings.contains(siblings)) {
            topology.coreSiblings.append(siblings);
        }
    }
    if (missingCapacity > 0 && missingCapacity < topology.online.size()) {
        topology.problems.append(QStringLiteral("some cores have no capacity data"));
    }

    const QDir cpufreq(root.filePath(QStringLiteral("cpufreq")));
    const QStringList policies = cpufreq.entryList({QStringLiteral("policy*")},
                                                   QDir::Dirs | QDir::NoDotAndDotDot,
                                                   QDir::Name);
    for (const QString& policy : policies) {
        const QString text = readSmallFile(
            cpufreq.filePath(policy + QStringLiteral("/related_cpus")), &exists);
        bool domOk = false;
        const QList<int> cpus = parseCpuList(text, &domOk);
        if (domOk && !topology.clockDomains.contains(cpus)) {
            topology.clockDomains.append(cpus);
        } else if (exists && !domOk) {
            topology.problems.append(
                QStringLiteral("clock domain %1 is unreadable").arg(policy));
        }
    }
    return topology;
}

// ---------------------------------------------------------------- plan

int PlacementPlan::cpuFor(ThreadRole role, int channel) const
{
    const RoleAssignment* a = findAssignment(assignments, role, channel);
    return a != nullptr ? a->cpu : -1;
}

QList<int> PlacementPlan::coresFor(ThreadRole role, int channel) const
{
    const RoleAssignment* a = findAssignment(assignments, role, channel);
    return a != nullptr ? coresOf(*a) : QList<int>{};
}

bool PlacementPlan::raisedFor(ThreadRole role, int channel) const
{
    const RoleAssignment* a = findAssignment(assignments, role, channel);
    return a != nullptr && a->raised;
}

QList<int> PlacementPlan::signalCores() const
{
    QList<int> cores;
    for (const RoleAssignment& a : assignments) {
        if (a.cpu >= 0) {
            cores.append(a.cpu);
        }
    }
    std::sort(cores.begin(), cores.end());
    cores.erase(std::unique(cores.begin(), cores.end()), cores.end());
    return cores;
}

bool PlacementPlan::carriesSpectrum(int cpu) const
{
    return cpu >= 0
        && std::any_of(assignments.cbegin(), assignments.cend(), [cpu](const RoleAssignment& a) {
               return a.role == ThreadRole::SpectrumThread && coresOf(a).contains(cpu);
           });
}

PlacementPlan planThreadPlacement(const CpuTopology& topology,
                                  const PlacementDemand& demand)
{
    PlacementPlan plan;
    if (topology.online.isEmpty()) {
        plan.reason = QStringLiteral("the list of processor cores could not be read");
        return plan;
    }

    QList<int> usable;
    for (int cpu : topology.online) {
        if (topology.allowed.isEmpty() || topology.allowed.contains(cpu)) {
            usable.append(cpu);
        }
    }
    std::set<int> physical;
    for (int cpu : std::as_const(usable)) {
        physical.insert(physicalCoreOf(topology, cpu));
    }
    const int physicalCount = static_cast<int>(physical.size());
    if (physicalCount < 2) {
        plan.reason = QStringLiteral("only one processor core is available");
        return plan;
    }
    // At most N-2 of N physical cores, N-1 with two or three.
    const int cap = physicalCount >= 4 ? physicalCount - 2 : physicalCount - 1;

    bool allHaveCapacity = true;
    int maxCapacity = 0;
    int minCapacity = 0;
    for (int cpu : std::as_const(usable)) {
        if (!topology.capacity.contains(cpu)) {
            allHaveCapacity = false;
            break;
        }
        const int c = topology.capacity.value(cpu);
        maxCapacity = std::max(maxCapacity, c);
        minCapacity = (minCapacity == 0) ? c : std::min(minCapacity, c);
    }
    const bool tiered = allHaveCapacity && maxCapacity > minCapacity;

    QList<int> ordered;
    QList<int> slow;
    if (tiered) {
        QList<int> fast;
        for (int cpu : std::as_const(usable)) {
            const qint64 c = topology.capacity.value(cpu);
            if (c * 100 >= qint64(maxCapacity) * kFastTierPercent) {
                fast.append(cpu);
            } else {
                slow.append(cpu);
            }
        }
        // Group fast cores by clock domain; the domain with the fastest
        // core first (then the lowest core number), filling it before the
        // next.
        QMap<int, QList<int>> byDomain;
        for (int cpu : std::as_const(fast)) {
            byDomain[clockDomainOf(topology, cpu)].append(cpu);
        }
        QList<QList<int>> domains = byDomain.values();
        auto domainCapacity = [&](const QList<int>& cpus) {
            int best = 0;
            for (int cpu : cpus) {
                best = std::max(best, topology.capacity.value(cpu));
            }
            return best;
        };
        for (QList<int>& cpus : domains) {
            std::sort(cpus.begin(), cpus.end(), [&](int a, int b) {
                const int ca = topology.capacity.value(a);
                const int cb = topology.capacity.value(b);
                return ca != cb ? ca > cb : a < b;
            });
        }
        std::sort(domains.begin(), domains.end(),
                  [&](const QList<int>& a, const QList<int>& b) {
                      const int ca = domainCapacity(a);
                      const int cb = domainCapacity(b);
                      if (ca != cb) {
                          return ca > cb;
                      }
                      return *std::min_element(a.cbegin(), a.cend())
                             < *std::min_element(b.cbegin(), b.cend());
                  });
        for (const QList<int>& cpus : std::as_const(domains)) {
            ordered.append(cpus);
        }
        plan.reason = QStringLiteral("the fastest cores, from the kernel's core capacity data");
    } else {
        ordered = usable;
        std::sort(ordered.begin(), ordered.end(), std::greater<int>());
        plan.reason = allHaveCapacity
            ? QStringLiteral("all cores are equally fast, so the highest-numbered are used")
            : QStringLiteral("core speeds are unknown, so the highest-numbered are used");
    }
    const bool slowRunsHousekeeping = tiered && slow.size() >= 2;

    // Never CPU0 or its core, never two siblings of one core.
    const int cpu0Core = physicalCoreOf(topology, 0);
    std::set<int> takenCores;
    for (int cpu : std::as_const(ordered)) {
        const int core = physicalCoreOf(topology, cpu);
        if (cpu == 0 || core == cpu0Core || takenCores.count(core) != 0) {
            continue;
        }
        takenCores.insert(core);
        plan.signalPool.append(cpu);
    }
    if (!slowRunsHousekeeping && plan.signalPool.size() > cap) {
        plan.signalPool = plan.signalPool.mid(0, cap);
    }
    if (plan.signalPool.isEmpty()) {
        plan.reason = QStringLiteral("no core can be set aside for signal processing");
        return plan;
    }

    QList<int> rx = demand.rxChannels;
    std::sort(rx.begin(), rx.end());
    rx.erase(std::unique(rx.begin(), rx.end()), rx.end());
    // G-06: the first receive worker, the DSP thread, the three transmit
    // roles, then the further receive workers. While transmitting, the
    // transmit path (worker, pump, I/Q sender) must not lose its core to a
    // second or third receiver; those demand cores only for as long as
    // transmit does not.
    //
    // Where slower cores run everything else, the radio connection thread
    // shares the DSP thread's core, both raised. Measured on the Rock 5C on 2026-10-10 with three receivers,
    // two at 1,536,000 and one at 768,000 (17,000 packets a second, a
    // socket buffer worth 9 ms of them), each receive worker processing
    // 85 ms of signal at a stretch (DSP blocks of 4096) except where
    // noted. Packets the kernel dropped per second:
    //   - on a slow core with everything else (four receivers then,
    //     26,600 packets a second): 12,000;
    //   - a fast core to itself at normal priority, the DSP thread on the
    //     first receive worker's core, a spectrum thread held to each
    //     later receiver's core: 37 to 59;
    //   - the same with the spectrum threads free on every fast core
    //     (below): 0.8, and it waited up to 14 ms for its own core behind
    //     a spectrum thread at its own priority;
    //   - that with the connection thread raised: none in three minutes
    //     while the first receive worker ran a fifth of a millisecond at
    //     a stretch (DSP blocks of 64). Once it ran 10 ms at a stretch
    //     (blocks of 4096) the DSP thread, level with it on its core,
    //     waited the burst out 803 times in 94 s, the radio's audio ran
    //     dry at every block and 6 packets a second were dropped;
    //   - on the DSP thread's core, both raised: none in 90 s, then 0.8
    //     over four minutes, all of it one wait of 13.9 ms for a lock a
    //     spectrum thread held (below). The DSP thread never waited
    //     0.2 ms for the core. With a fourth receiver (20,200 packets a
    //     second, a buffer worth 7.6 ms): 1.3 over 150 s, with one 9 ms
    //     wait for that lock.
    // The two belong together because neither runs for long: at a stretch
    // the DSP thread ran 0.07 ms (0.9 ms at most) and the connection thread
    // 0.18 ms (3.9 ms at most), so neither can keep the other off the core
    // the way a receive worker's burst does, and together they filled 46%
    // of it (56% with the fourth receiver). With no DSP thread running the
    // connection thread has the core to itself; with no core for the DSP
    // thread both run on the housekeeping cores at normal priority.
    // Where every core is equally fast (a Raspberry Pi 4 or 5, x86 Linux)
    // the connection thread stays on the housekeeping cores at normal
    // priority, as it did before it had a role: none of this was measured
    // there, and the Pi 4, the Hermes Lite 2 bench, has one core for the
    // DSP thread to fill with several receivers.
    QList<RoleAssignment> order;
    if (!rx.isEmpty()) {
        order.append({ThreadRole::RxWorker, rx.first(), -1});
    }
    if (demand.dspThread) {
        order.append({ThreadRole::DspThread, -1, -1});
    } else if (demand.connectionThread && slowRunsHousekeeping) {
        order.append({ThreadRole::ConnectionThread, -1, -1});
    }
    // G-06 is for transmitting. With VOX on the transmit channel runs
    // through every receive period so VOX can hear the microphone, with
    // nothing on the air, and its roles come after every receive worker.
    // Seated as if keyed, they kept two fast cores for as long as VOX was
    // on: on the Rock on 2026-10-11, VOX on and one receiver, the transmit
    // worker held core 6 and the pump core 7 between transmissions, and
    // each receiver after the first would have run on the slow cores,
    // where a wide one does not fit. Listening is not light work for the
    // worker: 456 ms of a fast core each second in 1,170 runs, against
    // 483 ms keyed. The pump took 33 ms.
    const bool listening = demand.txWorker && demand.txListening;
    const auto appendTransmitRoles = [&order, &demand]() {
        if (demand.txWorker) {
            order.append({ThreadRole::TxWorker, -1, -1});
        }
        if (demand.txWorkerThread) {
            order.append({ThreadRole::TxWorkerThread, -1, -1});
        }
        if (demand.txIqSender) {
            order.append({ThreadRole::TxIqSender, -1, -1});
        }
    };
    if (!listening) {
        appendTransmitRoles();
    }
    for (int i = 1; i < rx.size(); ++i) {
        order.append({ThreadRole::RxWorker, rx.at(i), -1});
    }
    if (listening) {
        appendTransmitRoles();
    }
    int next = 0;
    for (RoleAssignment& a : order) {
        if (next < plan.signalPool.size()) {
            a.cpu = plan.signalPool.at(next++);
        }
    }

    // Where slower cores run everything else, a receive worker left without
    // a core shares the cores of the later receive workers that have one,
    // raised like them. On a slow core it runs at normal priority and takes
    // far longer: the Rock's third wide receiver took 386 ms of a slow core
    // each second against 178 ms of a fast one, and the heaviest receiver
    // that evening (600 ms of a fast core) would not fit a slow core at
    // all. Sharing one later receiver's core, both raised, it dropped no
    // packets and the later receiver waited 178 ms a second for the core.
    // Never the first receiver's core or the one the DSP and connection
    // threads share. Where every core is equally fast the housekeeping
    // cores cost it nothing but the priority, so it stays there.
    //
    // A transmit worker listening for VOX and left without a core shares
    // those cores the same way, for the same reason: at twice the time a
    // slow core would not carry it, and VOX would hear the microphone
    // late. Not measured with a receiver on those cores. The pump and the
    // I/Q sender are light and run with everything else, as they do while
    // the transmit channel is off.
    const int firstRx = rx.isEmpty() ? -1 : rx.first();
    QList<int> laterReceiverCores;
    if (slowRunsHousekeeping) {
        for (const RoleAssignment& a : std::as_const(order)) {
            if (a.role == ThreadRole::RxWorker && a.cpu >= 0 && a.channel != firstRx) {
                laterReceiverCores.append(a.cpu);
            }
        }
        std::sort(laterReceiverCores.begin(), laterReceiverCores.end());
    }
    for (RoleAssignment& a : order) {
        const bool sharesWithReceivers = a.role == ThreadRole::RxWorker
            || (listening && a.role == ThreadRole::TxWorker);
        if (sharesWithReceivers && a.cpu < 0) {
            a.shared = laterReceiverCores;
        }
        // Minor 2: raised only on signal processing cores. On the
        // housekeeping cores a raised thread would crowd spectrum and
        // networking.
        a.raised = a.cpu >= 0 || !a.shared.isEmpty();
    }

    if (demand.dspThread && demand.connectionThread) {
        int dspCpu = -1;
        if (slowRunsHousekeeping) {
            for (const RoleAssignment& a : std::as_const(order)) {
                if (a.role == ThreadRole::DspThread) {
                    dspCpu = a.cpu;
                }
            }
        }
        order.append({ThreadRole::ConnectionThread, -1, dspCpu, {}, dspCpu >= 0});
    }

    // Spectrum threads own no core. Where slower cores run everything else
    // they may use every signal processing core (while transmitting, every
    // one the transmit roles leave), at normal priority; held to fewer they
    // stall the connection thread. A spectrum thread takes
    // its event queue's lock for each block of samples, and so does the
    // connection thread to hand it one. Held to one later receiver's core,
    // a spectrum thread that lost the core to that receiver's burst (50 ms
    // of processing for each 85 ms block at 1,536,000) kept the lock until
    // the burst ended, and the connection thread waited 47 to 55 ms for it
    // about four times a minute, against a socket buffer of 9 ms. Measured
    // as above with the connection thread raised, packets dropped per
    // second and the connection thread's longest wait for the lock, by the
    // cores the spectrum threads could use:
    //   - one later receiver's core each (connection thread not raised):
    //     37 to 59, 55 ms;
    //   - the three fast cores without the first receiver's: 3.2, 14.5 ms;
    //   - all four: none in three minutes, 1.1 ms; over a later four
    //     minutes 0.8, 13.9 ms once.
    // That last wait is not the placement's to remove: a spectrum thread
    // that loses its core to a receive worker's burst while it holds the
    // lock keeps it until another fast core takes the thread over. A
    // receive worker running shorter DSP blocks, a socket buffer granted
    // in full (the kernel gave 426 KB of the 4 MB asked for) or a handoff
    // that takes no lock would each end it.
    // At normal priority they cost the raised threads little: a spectrum
    // thread on its core kept the connection thread waiting 2 ms or more
    // 14 times in four minutes, 5.4 ms at most, and the DSP thread never
    // 0.3 ms. Every figure here was measured receiving. Where the system
    // refuses the raise (no LimitNICE), spectrum, the receive workers and
    // the connection thread share the fast cores at nice 0; that case is
    // not measured either.
    //
    // While transmitting (G-06, not listening for VOX) they keep off the
    // cores the transmit roles hold, as the RADE decoders do (below): a
    // raised thread can wait out a normal-priority thread's run (next
    // paragraph), and the pump feeds the radio every 1.33 ms. With no fast
    // core left they run on the housekeeping cores, as on equal cores.
    // Keyed behavior is not measured. While listening for VOX nothing
    // changes.
    //
    // What a raise buys against a thread at normal priority is less than
    // it looks: it does not take the core from one that has just woken.
    // Linux places a thread that wakes from sleep up to half its
    // scheduling latency ahead of the threads already queued
    // (place_entity, kernel/sched/fair.c, v6.1; 12 ms on this board:
    // latency 24 ms with GENTLE_FAIR_SLEEPERS on). A normal-priority
    // thread uses that lead up one for one as it runs, and until it has,
    // the tick hands the core straight back to it (check_preempt_tick,
    // pick_next_entity). So a raised thread can wait out a normal-priority
    // thread's whole run, up to about 12 ms. Recorded on 2026-10-11 with
    // one receiver at 768,000. Every switch on the connection thread's
    // core for four minutes: nine waits of 2 ms or more, eight of them
    // (4.4 to 5.4 ms) behind a program that is not the Core's (htop in a
    // terminal, allowed on every core) and one (2.0 ms) behind a spectrum
    // thread. On a later build, two minutes of polling: one wait, 12.1 ms,
    // while htop ran 12.7 ms on that core. No packet was dropped: one
    // receiver's packets fill the socket buffer in about 45 ms. Four wide
    // receivers fill it in 6 to 9 ms, which is where such a wait costs
    // packets. Real-time priority would end it. This board does not
    // refuse it to a service outright, as this comment first said: its
    // kernel groups real-time scheduling by cgroup, and a request is
    // refused only while the CPU controller is switched on, which root's
    // login manager (user@0.service, started by any root login) asks for.
    // Read on the Rock on 2026-10-11 by the native audio work: of six
    // requests by audio threads, the four made while that login manager
    // ran were refused and the two made with it stopped were granted
    // (SCHED_FIFO 98). The threads placed here do not ask for it; they
    // take the raise above. A socket buffer granted in full makes the
    // wait harmless.
    //
    // Where every core is equally fast they stay on the housekeeping cores,
    // which no raised thread runs on.
    QList<int> spectrum = demand.spectrumThreads;
    std::sort(spectrum.begin(), spectrum.end());
    spectrum.erase(std::unique(spectrum.begin(), spectrum.end()), spectrum.end());
    QList<int> spectrumCores;
    if (slowRunsHousekeeping) {
        spectrumCores = plan.signalPool;
        if (!listening) {
            for (const RoleAssignment& a : std::as_const(order)) {
                if (isTransmitRole(a.role) && a.cpu >= 0) {
                    spectrumCores.removeAll(a.cpu);
                }
            }
        }
        std::sort(spectrumCores.begin(), spectrumCores.end());
    }
    for (int bucket : std::as_const(spectrum)) {
        order.append({ThreadRole::SpectrumThread, bucket, -1, spectrumCores});
    }

    // RADE decoders (JJ's ruling of 2026-09-30: the least busy fast core),
    // after every other role, each sharing a signal processing core, judged
    // from the plan. The key, smallest first:
    //  1. decoders already on the core, so decoders spread across cores
    //     before any core takes a second;
    //  1a. the connection thread's core after every other core without a
    //     decoder: it shares only with the DSP thread for a reason (above).
    //     Where the connection thread has no core of its own (equal
    //     cores) this counts nothing;
    //  2. busy: every role the plan gives the core as its own. Transmit
    //     roles count in full: they are in the plan only while
    //     transmitting, and then they are working (G-06 keeps their cores
    //     for them). Roles that move between cores are not counted;
    //  3. a core carrying a transmit role last among equals, so that while
    //     keyed a receive decoder leaves the transmit cores to the transmit
    //     path (G-06) whenever an equal core without one exists;
    //  4. the DSP thread's core next to last: it runs fexchange2 for every
    //     slice, so it is the one receive-time core whose single thread is
    //     not like the others. It stays a candidate;
    //  5. signalPool order (fastest first).
    // In the order given (the registry's: the order the decoders started),
    // so a decoder added later never moves one already placed.
    QList<int> rade;
    for (int channel : demand.radeDecoders) {
        if (!rade.contains(channel)) {
            rade.append(channel);
        }
    }
    if (!rade.isEmpty()) {
        QMap<int, int> busy;
        QMap<int, int> decoders;
        std::set<int> dspCores;
        std::set<int> transmitCores;
        std::set<int> connectionCores;
        for (const RoleAssignment& a : std::as_const(order)) {
            if (a.cpu < 0) {
                continue;
            }
            ++busy[a.cpu];
            if (a.role == ThreadRole::ConnectionThread) {
                connectionCores.insert(a.cpu);
            }
            if (isTransmitRole(a.role)) {
                transmitCores.insert(a.cpu);
            }
            if (a.role == ThreadRole::DspThread) {
                dspCores.insert(a.cpu);
            }
        }
        for (int channel : std::as_const(rade)) {
            int best = -1;
            for (int cpu : std::as_const(plan.signalPool)) {
                if (best < 0) {
                    best = cpu;
                    continue;
                }
                const auto key = [&](int c) {
                    return std::make_tuple(decoders.value(c), connectionCores.count(c),
                                           busy.value(c), transmitCores.count(c),
                                           dspCores.count(c));
                };
                // Strictly less: an equal core keeps the earlier one.
                if (key(cpu) < key(best)) {
                    best = cpu;
                }
            }
            order.append({ThreadRole::RadeDecoder, channel, best, {}, true});
            ++busy[best];
            ++decoders[best];
        }
    }

    // CW transmit (R-CW-31): the CWX sender's thread wakes once per element
    // and runs for microseconds, but a late wake puts a key edge off its
    // element boundary. It shares the DSP thread's core, raised, wherever
    // the connection thread does, for the same reason (neither keeps the
    // other off the core). Appended after the RADE decoders, so it changes
    // no decoder's choice. Not measured; NereusSDR's own rule.
    if (demand.cwxSender) {
        int dspCpu = -1;
        if (slowRunsHousekeeping && demand.dspThread) {
            for (const RoleAssignment& a : std::as_const(order)) {
                if (a.role == ThreadRole::DspThread) {
                    dspCpu = a.cpu;
                }
            }
        }
        order.append({ThreadRole::CwxSender, -1, dspCpu, {}, dspCpu >= 0});
    }
    // CW transmit (R-CW-36): the serial key reader sleeps in the kernel
    // until a paddle or key line moves, then writes the key line within
    // microseconds; a late wake delays the edge. Placed as the CWX sender
    // is, for the same reason. Not measured; NereusSDR's own rule.
    if (demand.cwSerialKey) {
        int dspCpu = -1;
        if (slowRunsHousekeeping && demand.dspThread) {
            for (const RoleAssignment& a : std::as_const(order)) {
                if (a.role == ThreadRole::DspThread) {
                    dspCpu = a.cpu;
                }
            }
        }
        order.append({ThreadRole::CwSerialKey, -1, dspCpu, {}, dspCpu >= 0});
    }
    plan.assignments = order;

    if (slowRunsHousekeeping) {
        plan.housekeeping = slow;
    } else {
        const QList<int> taken = plan.signalCores();
        for (int cpu : std::as_const(usable)) {
            if (!taken.contains(cpu)) {
                plan.housekeeping.append(cpu);
            }
        }
    }
    std::sort(plan.housekeeping.begin(), plan.housekeeping.end());
    plan.active = true;
    return plan;
}

// ---------------------------------------------------------------- system calls

namespace {

#ifndef Q_OS_LINUX
class NullSchedulingApi final : public ThreadSchedulingApi {
public:
    qint64 currentThreadId() override { return 0; }
    bool setAffinity(qint64, const QList<int>&) override { return false; }
    bool setNice(qint64, int) override { return false; }
};
#endif

#ifdef Q_OS_LINUX
class LinuxSchedulingApi final : public ThreadSchedulingApi {
public:
    qint64 currentThreadId() override
    {
        return static_cast<qint64>(::syscall(SYS_gettid));
    }

    bool setAffinity(qint64 threadId, const QList<int>& cpus) override
    {
        cpu_set_t set;
        CPU_ZERO(&set);
        for (int cpu : cpus) {
            if (cpu >= 0 && cpu < CPU_SETSIZE) {
                CPU_SET(cpu, &set);
            }
        }
        if (::sched_setaffinity(static_cast<pid_t>(threadId), sizeof(set), &set) != 0) {
            recordError(errno);
            return false;
        }
        return true;
    }

    bool setNice(qint64 threadId, int nice) override
    {
        if (::setpriority(PRIO_PROCESS, static_cast<id_t>(threadId), nice) != 0) {
            recordError(errno);
            return false;
        }
        return true;
    }

    // The caller logs, once per kind of refusal (ThreadPlacement).
    QString lastError() const override { return m_lastError; }

private:
    void recordError(int err)
    {
        m_lastError = QStringLiteral("errno %1, %2")
                          .arg(err)
                          .arg(QString::fromLocal8Bit(std::strerror(err)));
    }

    QString m_lastError;
};
#endif

} // namespace

std::unique_ptr<ThreadSchedulingApi> makeSystemThreadSchedulingApi()
{
#ifdef Q_OS_LINUX
    return std::make_unique<LinuxSchedulingApi>();
#else
    return std::make_unique<NullSchedulingApi>();
#endif
}

bool canRaiseCurrentThreadPriority(int nice)
{
#ifdef Q_OS_LINUX
    const auto tid = static_cast<id_t>(::syscall(SYS_gettid));
    // getpriority can return -1 as a real level, so errno decides.
    errno = 0;
    const int before = ::getpriority(PRIO_PROCESS, tid);
    if (before == -1 && errno != 0) {
        return false;
    }
    if (::setpriority(PRIO_PROCESS, tid, nice) != 0) {
        return false;
    }
    // Going back to a level at or above the one just set is always allowed.
    ::setpriority(PRIO_PROCESS, tid, before);
    return true;
#else
    Q_UNUSED(nice);
    return false;
#endif
}

// ---------------------------------------------------------------- registry

ThreadPlacement::ThreadPlacement() = default;
ThreadPlacement::~ThreadPlacement() = default;

ThreadPlacement& ThreadPlacement::instance()
{
    static ThreadPlacement placement;
    return placement;
}

bool ThreadPlacement::isPlacing() const
{
    QMutexLocker lock(&m_mutex);
    return m_active.load(std::memory_order_acquire) && m_placing;
}

bool ThreadPlacement::managesThreadPriority()
{
#ifdef Q_OS_LINUX
    return instance().isActive();
#else
    return false;
#endif
}

void ThreadPlacement::wdspThreadStartHook(int kind, int channel)
{
    instance().onWdspThreadStarted(kind, channel);
}

QString ThreadPlacement::start(const CpuTopology& topology,
                               const PlacementDemand& startupDemand,
                               std::unique_ptr<ThreadSchedulingApi> api,
                               bool raisePriorityPermitted)
{
    QMutexLocker lock(&m_mutex);
    if (m_active.load(std::memory_order_acquire)) {
        return QStringLiteral("Thread placement: already started.");
    }
    const PlacementPlan plan = planThreadPlacement(topology, startupDemand);
    if (!api) {
        return inactiveLine(plan.active ? QStringLiteral("the system refused to move threads")
                                        : plan.reason);
    }
    if (!plan.active) {
        return startPriorityOnlyLocked(std::move(api), raisePriorityPermitted, plan.reason);
    }
    // Every thread created after this one starts on the housekeeping cores.
    if (!api->setAffinity(api->currentThreadId(), plan.housekeeping)) {
        return startPriorityOnlyLocked(std::move(api), raisePriorityPermitted,
                                       QStringLiteral("the system refused to move threads"));
    }
    m_topology = topology;
    m_startupPlan = plan;
    m_api = std::move(api);
    m_raisePriority = raisePriorityPermitted;
    m_placing = true;
    m_active.store(true, std::memory_order_release);
    m_planRevision.fetch_add(1, std::memory_order_acq_rel);
    return activeLine(plan, raisePriorityPermitted);
}

QString ThreadPlacement::startPriorityOnly(std::unique_ptr<ThreadSchedulingApi> api,
                                           bool raisePriorityPermitted,
                                           const QString& reason)
{
    QMutexLocker lock(&m_mutex);
    if (m_active.load(std::memory_order_acquire)) {
        return QStringLiteral("Thread placement: already started.");
    }
    if (!api) {
        return inactiveLine(reason);
    }
    return startPriorityOnlyLocked(std::move(api), raisePriorityPermitted, reason);
}

QString ThreadPlacement::startPriorityOnlyLocked(std::unique_ptr<ThreadSchedulingApi> api,
                                                 bool raisePriorityPermitted,
                                                 const QString& reason)
{
    // Important 1 (final review): spectrum and networking skip their own
    // nice calls whenever this registry is started, so it must start here
    // too, or signal processing would sit below them.
    m_topology = CpuTopology{};
    m_startupPlan = PlacementPlan{};
    m_api = std::move(api);
    m_raisePriority = raisePriorityPermitted;
    m_placing = false;
    m_active.store(true, std::memory_order_release);
    m_planRevision.fetch_add(1, std::memory_order_acq_rel);
    return priorityOnlyLine(reason, raisePriorityPermitted);
}

bool ThreadPlacement::setAffinityLocked(qint64 threadId, const QList<int>& cpus)
{
    if (m_api->setAffinity(threadId, cpus)) {
        return true;
    }
    // A refusal repeats on every later change (an offline core, a
    // restricted mask), so only the first is worth a warning.
    if (!m_affinityRefusalLogged) {
        m_affinityRefusalLogged = true;
        qCWarning(lcApp).noquote()
            << QStringLiteral("Thread placement: the system refused to move a signal"
                              " processing thread to cores %1 (%2); later refusals"
                              " are not logged.")
                   .arg(formatCpuList(cpus), m_api->lastError());
    } else {
        qCDebug(lcApp) << "Thread placement: move of thread" << threadId << "to cores"
                       << formatCpuList(cpus) << "refused:" << m_api->lastError();
    }
    return false;
}

bool ThreadPlacement::setNiceLocked(qint64 threadId, int nice)
{
    if (m_api->setNice(threadId, nice)) {
        return true;
    }
    if (!m_niceRefusalLogged) {
        m_niceRefusalLogged = true;
        qCWarning(lcApp).noquote()
            << QStringLiteral("Thread placement: the system refused to change a signal"
                              " processing thread's priority (%1); later refusals"
                              " are not logged.")
                   .arg(m_api->lastError());
    } else {
        qCDebug(lcApp) << "Thread placement: priority change of thread" << threadId
                       << "to" << nice << "refused:" << m_api->lastError();
    }
    return false;
}

PlacementDemand ThreadPlacement::demandLocked() const
{
    PlacementDemand demand;
    for (auto it = m_activeRx.cbegin(); it != m_activeRx.cend(); ++it) {
        if (it.value()) {
            demand.rxChannels.append(it.key());
        }
    }
    bool keyed = false;
    for (auto it = m_activeTx.cbegin(); it != m_activeTx.cend(); ++it) {
        demand.txWorker = demand.txWorker || it.value();
        keyed = keyed || (it.value() && !m_listeningTx.value(it.key(), false));
    }
    demand.txListening = demand.txWorker && !keyed;
    for (const Registered& t : m_threads) {
        demand.dspThread = demand.dspThread || t.role == ThreadRole::DspThread;
        demand.txWorkerThread = demand.txWorkerThread
            || (t.role == ThreadRole::TxWorkerThread && demand.txWorker);
        demand.txIqSender = demand.txIqSender
            || (t.role == ThreadRole::TxIqSender && demand.txWorker);
        if (t.role == ThreadRole::RadeDecoder) {
            demand.radeDecoders.append(t.channel);
        }
        demand.connectionThread = demand.connectionThread
            || t.role == ThreadRole::ConnectionThread;
        if (t.role == ThreadRole::SpectrumThread) {
            demand.spectrumThreads.append(t.channel);
        }
        demand.cwxSender = demand.cwxSender || t.role == ThreadRole::CwxSender;
        demand.cwSerialKey = demand.cwSerialKey || t.role == ThreadRole::CwSerialKey;
    }
    return demand;
}

bool ThreadPlacement::roleActiveLocked(ThreadRole role, int channel) const
{
    switch (role) {
    case ThreadRole::RxWorker:
        return m_activeRx.value(channel, false);
    case ThreadRole::TxWorker:
        return m_activeTx.value(channel, false);
    case ThreadRole::DspThread:
    case ThreadRole::RadeDecoder:
    case ThreadRole::ConnectionThread:
    case ThreadRole::SpectrumThread:
    case ThreadRole::CwxSender:
    case ThreadRole::CwSerialKey:
        return true;
    case ThreadRole::TxWorkerThread:
    case ThreadRole::TxIqSender:
        for (bool on : m_activeTx) {
            if (on) {
                return true;
            }
        }
        return false;
    }
    return false;
}

void ThreadPlacement::applyOneLocked(Registered& thread, const PlacementPlan& plan)
{
    bool raised = roleActiveLocked(thread.role, thread.channel);
    if (m_placing) {
        const int cpu = plan.active ? plan.cpuFor(thread.role, thread.channel) : -1;
        const QList<int> own =
            plan.active ? plan.coresFor(thread.role, thread.channel) : QList<int>{};
        const QList<int> cpus = own.isEmpty() ? m_startupPlan.housekeeping : own;
        if (cpus != thread.appliedCpus && setAffinityLocked(thread.threadId, cpus)) {
            thread.appliedCpus = cpus;
            // Each RADE decoder's core is logged when it is chosen (a
            // decoder added or removed, a transmit edge), never per block.
            if (thread.role == ThreadRole::RadeDecoder) {
                qCInfo(lcApp).noquote()
                    << (cpu >= 0
                            ? QStringLiteral("Thread placement: the RADE decoder for"
                                             " channel %1 runs on core %2.")
                                  .arg(thread.channel).arg(cpu)
                            : QStringLiteral("Thread placement: the RADE decoder for"
                                             " channel %1 runs on cores %2 with"
                                             " everything else.")
                                  .arg(thread.channel)
                                  .arg(formatCpuList(cpus)));
            }
        }
        // A role left without signal processing cores (none free, or the
        // move was refused) shares the housekeeping cores; raised there,
        // it would crowd spectrum and networking. The plan says which of
        // the others are raised: never a spectrum thread.
        raised = !own.isEmpty() && thread.appliedCpus == cpus
            && plan.raisedFor(thread.role, thread.channel);
    } else if (housekeepingWithoutPlacement(thread.role)) {
        raised = false;
    }
    if (m_raisePriority) {
        const int nice = raised ? kDspNice : 0;
        if ((!thread.niceApplied || nice != thread.appliedNice)
            && setNiceLocked(thread.threadId, nice)) {
            thread.appliedNice = nice;
            thread.niceApplied = true;
        }
    }
}

void ThreadPlacement::applyLocked()
{
    const PlacementPlan plan = m_placing ? planThreadPlacement(m_topology, demandLocked())
                                         : PlacementPlan{};
    for (Registered& thread : m_threads) {
        applyOneLocked(thread, plan);
    }
    m_planRevision.fetch_add(1, std::memory_order_acq_rel);
}

void ThreadPlacement::registerCurrentThread(ThreadRole role, int channel)
{
    if (!isActive()) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    const qint64 threadId = m_api->currentThreadId();
    const bool perChannel = isChannelRole(role) || assignedPerChannel(role);
    const int ownChannel = perChannel ? channel : -1;
    // A channel has one worker: WDSP's own rebuilds (a rate or size change)
    // replace it with a new thread, so the old entry goes too. A RADE
    // channel likewise has one decoder thread.
    m_threads.erase(std::remove_if(m_threads.begin(), m_threads.end(),
                                   [&](const Registered& t) {
                                       return t.threadId == threadId
                                           || (perChannel && t.role == role
                                               && t.channel == ownChannel);
                                   }),
                    m_threads.end());
    Registered thread;
    thread.threadId = threadId;
    thread.role = role;
    thread.channel = ownChannel;
    m_threads.append(thread);
    applyLocked();
}

void ThreadPlacement::deregisterCurrentThread()
{
    if (!isActive()) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    const qint64 threadId = m_api->currentThreadId();
    m_threads.erase(std::remove_if(m_threads.begin(), m_threads.end(),
                                   [threadId](const Registered& t) {
                                       return t.threadId == threadId;
                                   }),
                    m_threads.end());
    applyLocked();
}

void ThreadPlacement::setChannelActive(ThreadRole role, int channel, bool active)
{
    if (!isActive() || !isChannelRole(role)) {
        return;
    }
    if (role == ThreadRole::TxWorker) {
        setTransmitUse(channel, active ? TransmitUse::Transmitting : TransmitUse::Stopped);
        return;
    }
    QMutexLocker lock(&m_mutex);
    QMap<int, bool>& states = m_activeRx;
    if (states.value(channel, false) == active && states.contains(channel)) {
        return;
    }
    states.insert(channel, active);
    applyLocked();
}

void ThreadPlacement::setTransmitUse(int channel, TransmitUse use)
{
    if (!isActive()) {
        return;
    }
    const bool active = use != TransmitUse::Stopped;
    const bool listening = use == TransmitUse::Listening;
    QMutexLocker lock(&m_mutex);
    if (m_activeTx.contains(channel) && m_activeTx.value(channel) == active
        && m_listeningTx.value(channel, false) == listening) {
        return;
    }
    m_activeTx.insert(channel, active);
    m_listeningTx.insert(channel, listening);
    applyLocked();
}

void ThreadPlacement::forgetChannel(int channel)
{
    if (!isActive()) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    m_threads.erase(std::remove_if(m_threads.begin(), m_threads.end(),
                                   [channel](const Registered& t) {
                                       return isChannelRole(t.role) && t.channel == channel;
                                   }),
                    m_threads.end());
    m_activeRx.remove(channel);
    m_activeTx.remove(channel);
    m_listeningTx.remove(channel);
    applyLocked();
}

void ThreadPlacement::onWdspThreadStarted(int kind, int channel)
{
    if (!isActive()) {
        return;
    }
    if (kind == kWdspThreadRxMain) {
        registerCurrentThread(ThreadRole::RxWorker, channel);
        return;
    }
    if (kind == kWdspThreadTxMain) {
        registerCurrentThread(ThreadRole::TxWorker, channel);
        return;
    }
    if (kind == kWdspThreadWorkerExit) {
        // A worker about to end: forget it now, before its ID can be
        // reused. A rebuild whose new worker never starts would otherwise
        // leave this entry behind.
        deregisterCurrentThread();
        return;
    }
    if (kind != kWdspThreadFlush) {
        return;
    }
    // A flush thread runs with everything else, whichever thread created it.
    QMutexLocker lock(&m_mutex);
    const qint64 threadId = m_api->currentThreadId();
    if (m_placing) {
        setAffinityLocked(threadId, m_startupPlan.housekeeping);
    }
    if (m_raisePriority) {
        setNiceLocked(threadId, 0);
    }
}

void ThreadPlacement::placeCurrentThreadOnFastCore()
{
    if (!isActive()) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    if (m_placing && !m_startupPlan.signalPool.isEmpty()) {
        setAffinityLocked(m_api->currentThreadId(), {m_startupPlan.signalPool.first()});
    }
}

PlacementPlan ThreadPlacement::currentPlan() const
{
    QMutexLocker lock(&m_mutex);
    return m_placing ? planThreadPlacement(m_topology, demandLocked()) : PlacementPlan{};
}

PlacementPlan ThreadPlacement::appliedPlan() const
{
    QMutexLocker lock(&m_mutex);
    if (!m_placing) {
        return PlacementPlan{};
    }
    PlacementPlan plan = planThreadPlacement(m_topology, demandLocked());
    QList<RoleAssignment> inForce;
    for (const RoleAssignment& assignment : std::as_const(plan.assignments)) {
        const QList<int> cores = coresOf(assignment);
        if (cores.isEmpty()) {
            continue;
        }
        for (const Registered& thread : m_threads) {
            // As in cpuFor(): only receive workers, RADE decoders and
            // spectrum threads are told apart by channel.
            if (thread.role != assignment.role
                || (assignedPerChannel(assignment.role)
                    && thread.channel != assignment.channel)
                || thread.appliedCpus != cores) {
                continue;
            }
            RoleAssignment applied = assignment;
            // Raised only where the system took the priority too.
            applied.raised = assignment.raised && thread.niceApplied
                && thread.appliedNice == kDspNice;
            inForce.append(applied);
            break;
        }
    }
    plan.assignments = inForce;
    return plan;
}

// ---------------------------------------------------------------- nereusd

void startDaemonThreadPlacement(bool enabled, int sliceCount)
{
#ifdef Q_OS_LINUX
    // Raised priority is tried once on this (main) thread and put back, so
    // the startup line says what the system actually allows (LimitNICE=-10
    // in the unit, or root). No other thread exists yet to inherit it.
    const bool raisePermitted = canRaiseCurrentThreadPriority(kDspNice);
    ThreadPlacement& placement = ThreadPlacement::instance();
    const auto logStartupLine = [&](const QString& line) {
        if (placement.isActive() && !raisePermitted) {
            // This line is the refusal notice; the generic one is not repeated.
            claimThreadPriorityRefusedWarning();
            qCWarning(lcApp).noquote() << line;
        } else {
            qCInfo(lcApp).noquote() << line;
        }
    };

    if (!enabled) {
        logStartupLine(placement.startPriorityOnly(
            makeSystemThreadSchedulingApi(), raisePermitted,
            QStringLiteral("thread_placement is off in nereusd.conf")));
        return;
    }

    // The mask systemd (CPUAffinity=, AllowedCPUs=) gave the process, read
    // once, before anything is moved.
    QList<int> allowed;
    cpu_set_t set;
    CPU_ZERO(&set);
    if (::sched_getaffinity(0, sizeof(set), &set) == 0) {
        for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (CPU_ISSET(cpu, &set)) {
                allowed.append(cpu);
            }
        }
    }
    const CpuTopology topology =
        readCpuTopology(QStringLiteral("/sys/devices/system/cpu"), allowed);
    for (const QString& problem : topology.problems) {
        qCDebug(lcApp) << "Thread placement:" << problem;
    }

    PlacementDemand demand;
    for (int channel = 0; channel < std::max(1, sliceCount); ++channel) {
        demand.rxChannels.append(channel);
    }
    demand.dspThread = true;
    demand.txWorker = true;
    demand.txWorkerThread = true;
    demand.txIqSender = true;

    logStartupLine(placement.start(topology, demand, makeSystemThreadSchedulingApi(),
                                   raisePermitted));
#else
    Q_UNUSED(enabled);
    Q_UNUSED(sliceCount);
    qCDebug(lcApp).noquote()
        << inactiveLine(QStringLiteral("thread placement is available only on Linux"));
#endif
}

} // namespace NereusSDR
