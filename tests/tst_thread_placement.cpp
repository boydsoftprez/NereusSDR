// =================================================================
// tests/tst_thread_placement.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Fixture-driven tests for nereusd's
// signal processing thread placement (R-R3-41): the topology reader and the
// pure plan against sysfs trees laid out under a temporary root, the
// registry against a recording fake of the system calls, and a Linux-only
// smoke test of the real calls. Nothing reads the build machine's /sys.
//
// Modification history (NereusSDR):
//   2026-09-27: the transmit I/Q sender role (R-IOS-13, R-R3-42). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: the RADE decoder role (JJ's ruling of 2026-09-30: the
//               least busy fast core, judged from the plan). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/daemon/DisplayLoadInputs.h"
#include "core/platform/ThreadPlacement.h"
#include "core/wdsp_api.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>
#include <thread>

#ifdef Q_OS_LINUX
#include <cerrno>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

using namespace NereusSDR;

namespace {

class SysfsFixture {
public:
    SysfsFixture() { Q_ASSERT(m_dir.isValid()); }

    QString root() const { return m_dir.path(); }

    void write(const QString& relative, const QByteArray& contents)
    {
        const QString path = m_dir.filePath(relative);
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(contents), contents.size());
    }

    void online(const QByteArray& list) { write(QStringLiteral("online"), list + '\n'); }

    void capacity(int cpu, int value)
    {
        write(QStringLiteral("cpu%1/cpu_capacity").arg(cpu), QByteArray::number(value) + '\n');
    }

    void policy(int number, const QByteArray& cpus)
    {
        write(QStringLiteral("cpufreq/policy%1/related_cpus").arg(number), cpus + '\n');
    }

    void siblings(int cpu, const QByteArray& cpus)
    {
        write(QStringLiteral("cpu%1/topology/core_cpus_list").arg(cpu), cpus + '\n');
    }

    CpuTopology read(const QList<int>& allowed = {}) const
    {
        return readCpuTopology(root(), allowed);
    }

private:
    QTemporaryDir m_dir;
};

// RK3588S (Rock 5C): four 405 cores (0-3), two 1024 (4-5), two 982 (6-7),
// clock domains 0-3, 4-5 and 6-7, as read on the Rock's 6.1 kernel.
void layOutRk3588s(SysfsFixture& f)
{
    f.online("0-7");
    for (int cpu = 0; cpu < 4; ++cpu) {
        f.capacity(cpu, 405);
    }
    f.capacity(4, 1024);
    f.capacity(5, 1024);
    f.capacity(6, 982);
    f.capacity(7, 982);
    // The kernel writes related_cpus space-separated.
    f.policy(0, "0 1 2 3");
    f.policy(4, "4 5");
    f.policy(6, "6 7");
}

// RK3588S as read on the Rock on 2026-09-30: all four fast cores report
// 1024 (the 982 fixture above is the older kernel reading).
void layOutRk3588sAsRead(SysfsFixture& f)
{
    layOutRk3588s(f);
    f.capacity(6, 1024);
    f.capacity(7, 1024);
}

PlacementDemand demand(QList<int> rx, bool dsp, bool tx = false, bool txThread = false)
{
    PlacementDemand d;
    d.rxChannels = std::move(rx);
    d.dspThread = dsp;
    d.txWorker = tx;
    d.txWorkerThread = txThread;
    return d;
}

struct Call {
    enum Kind { Affinity, Nice } kind;
    qint64 threadId;
    QList<int> cpus;
    int nice;
};

class RecordingApi final : public ThreadSchedulingApi {
public:
    explicit RecordingApi(QList<Call>* calls, qint64* current)
        : m_calls(calls), m_current(current) {}
    qint64 currentThreadId() override { return *m_current; }
    bool setAffinity(qint64 threadId, const QList<int>& cpus) override
    {
        m_calls->append({Call::Affinity, threadId, cpus, 0});
        for (int cpu : cpus) {
            if (refuseCpus.contains(cpu)) {
                return false;
            }
        }
        return !refuseAffinity;
    }
    bool setNice(qint64 threadId, int nice) override
    {
        m_calls->append({Call::Nice, threadId, {}, nice});
        return !refuseNice;
    }

    bool refuseAffinity{false};   ///< every move is refused (recorded anyway)
    bool refuseNice{false};       ///< every nice change is refused
    QList<int> refuseCpus;        ///< a move onto any of these is refused
    QString lastError() const override { return QStringLiteral("refused by the test"); }

private:
    QList<Call>* m_calls;
    qint64* m_current;
};

// The last affinity and nice each thread was given.
QList<int> lastCpus(const QList<Call>& calls, qint64 tid)
{
    for (auto it = calls.crbegin(); it != calls.crend(); ++it) {
        if (it->kind == Call::Affinity && it->threadId == tid) {
            return it->cpus;
        }
    }
    return {};
}

int lastNice(const QList<Call>& calls, qint64 tid)
{
    for (auto it = calls.crbegin(); it != calls.crend(); ++it) {
        if (it->kind == Call::Nice && it->threadId == tid) {
            return it->nice;
        }
    }
    return 999;
}

// A thread's nice level: the last one it was given, else the process's 0.
int niceOf(const QList<Call>& calls, qint64 tid)
{
    const int nice = lastNice(calls, tid);
    return nice == 999 ? 0 : nice;
}

// What a housekeeping thread (spectrum, networking) ends at in nereusd on
// Linux: it asks for nice -5 itself (elevateLatencyCriticalThreadPriority,
// FftEnginePool.cpp and RadioModel.cpp's connection thread) unless placement
// owns thread priority, and LimitNICE=-10 lets that call succeed.
constexpr int kHousekeepingOwnNice = -5;

int housekeepingNice(const ThreadPlacement& placement)
{
    return placement.isActive() ? 0 : kHousekeepingOwnNice;
}

// Starts every signal processing role the way nereusd does (receive worker
// active, DSP thread, transmitting) and returns their thread IDs.
QList<qint64> runEveryDspRole(ThreadPlacement& placement, qint64* current)
{
    *current = 101;
    placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
    placement.setChannelActive(ThreadRole::RxWorker, 0, true);
    *current = 200;
    placement.registerCurrentThread(ThreadRole::DspThread);
    *current = 105;
    placement.onWdspThreadStarted(kWdspThreadTxMain, 5);
    *current = 300;
    placement.registerCurrentThread(ThreadRole::TxWorkerThread);
    placement.setChannelActive(ThreadRole::TxWorker, 5, true);
    return {101, 200, 105, 300};
}

int affinityCalls(const QList<Call>& calls)
{
    int n = 0;
    for (const Call& c : calls) {
        n += c.kind == Call::Affinity ? 1 : 0;
    }
    return n;
}

// Collects the warnings placement logs while one is alive.
class WarningCatcher {
public:
    WarningCatcher() { s_warnings.clear(); m_previous = qInstallMessageHandler(&handler); }
    ~WarningCatcher() { qInstallMessageHandler(m_previous); }
    QStringList warnings() const { return s_warnings; }

private:
    static void handler(QtMsgType type, const QMessageLogContext&, const QString& text)
    {
        if (type == QtWarningMsg && text.startsWith(QLatin1String("Thread placement"))) {
            s_warnings.append(text);
        }
    }
    static inline QStringList s_warnings;
    QtMessageHandler m_previous{nullptr};
};

// Collects the RADE decoder placement lines.
class DecoderLineCatcher {
public:
    DecoderLineCatcher() { s_lines.clear(); m_previous = qInstallMessageHandler(&handler); }
    ~DecoderLineCatcher() { qInstallMessageHandler(m_previous); }
    QStringList lines() const { return s_lines; }

private:
    static void handler(QtMsgType, const QMessageLogContext&, const QString& text)
    {
        if (text.startsWith(QLatin1String("Thread placement: the RADE decoder"))) {
            s_lines.append(text);
        }
    }
    static inline QStringList s_lines;
    QtMessageHandler m_previous{nullptr};
};

PlacementDemand radeDemand(QList<int> rx, QList<int> decoders)
{
    PlacementDemand d = demand(std::move(rx), true);
    d.radeDecoders = std::move(decoders);
    return d;
}

// Starts placement on `f` as the Rock runs while receiving with two pans:
// rx0 (thread 101) on 4, the DSP thread (200) on 5, rx1 (102) on 6.
void startRockReceivingTwoPans(ThreadPlacement& placement, const SysfsFixture& f,
                               QList<Call>& calls, qint64& current)
{
    placement.start(f.read(), demand({0, 1}, true),
                    std::make_unique<RecordingApi>(&calls, &current), true);
    current = 101;
    placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
    placement.setChannelActive(ThreadRole::RxWorker, 0, true);
    current = 102;
    placement.onWdspThreadStarted(kWdspThreadRxMain, 1);
    placement.setChannelActive(ThreadRole::RxWorker, 1, true);
    current = 200;
    placement.registerCurrentThread(ThreadRole::DspThread);
    QCOMPARE(lastCpus(calls, 101), QList<int>{4});
    QCOMPARE(lastCpus(calls, 200), QList<int>{5});
    QCOMPARE(lastCpus(calls, 102), QList<int>{6});
}

int niceCalls(const QList<Call>& calls)
{
    int n = 0;
    for (const Call& c : calls) {
        n += c.kind == Call::Nice ? 1 : 0;
    }
    return n;
}

} // namespace

class TestThreadPlacement : public QObject {
    Q_OBJECT

private slots:
    // ---------------------------------------------------------- CPU lists

    void cpuListsParseAndFormat()
    {
        bool ok = false;
        QCOMPARE(parseCpuList(QStringLiteral("0-3,6-7"), &ok), (QList<int>{0, 1, 2, 3, 6, 7}));
        QVERIFY(ok);
        QCOMPARE(parseCpuList(QStringLiteral("5"), &ok), QList<int>{5});
        QVERIFY(ok);
        QCOMPARE(parseCpuList(QStringLiteral("4 5"), &ok), (QList<int>{4, 5}));
        QVERIFY(ok);
        for (const char* bad : {"", "abc", "3-1", "0-", "-2", "0,,1", "0-99999"}) {
            QVERIFY2(parseCpuList(QString::fromLatin1(bad), &ok).isEmpty(), bad);
            QVERIFY2(!ok, bad);
        }
        QCOMPARE(formatCpuList({4, 5, 6, 7}), QStringLiteral("4-7"));
        QCOMPARE(formatCpuList({7, 0, 2, 3}), QStringLiteral("0, 2-3, 7"));
    }

    // ---------------------------------------------------------- fixtures

    void rk3588sGivesTheBigCoresToSignalProcessing()
    {
        SysfsFixture f;
        layOutRk3588s(f);
        const CpuTopology topology = f.read();
        QCOMPARE(topology.online, (QList<int>{0, 1, 2, 3, 4, 5, 6, 7}));
        QCOMPARE(topology.clockDomains.size(), 3);
        QVERIFY(topology.problems.isEmpty());

        const PlacementPlan plan =
            planThreadPlacement(topology, demand({0, 1}, true, true));
        QVERIFY(plan.active);
        // G-06: RX1's worker, the DSP thread, then the transmit roles,
        // then the further receivers.
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 0), 4);
        QCOMPARE(plan.cpuFor(ThreadRole::DspThread), 5);
        QCOMPARE(plan.cpuFor(ThreadRole::TxWorker, 5), 6);
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 1), 7);
        QCOMPARE(plan.housekeeping, (QList<int>{0, 1, 2, 3}));
        QCOMPARE(plan.signalPool, (QList<int>{4, 5, 6, 7}));

        // A fifth busy thread: the transmit pump comes before the second
        // receiver, which has no fast core left and runs on 0-3.
        const PlacementPlan full =
            planThreadPlacement(topology, demand({0, 1}, true, true, true));
        QCOMPARE(full.cpuFor(ThreadRole::TxWorkerThread), 7);
        QCOMPARE(full.cpuFor(ThreadRole::RxWorker, 1), -1);
        QCOMPARE(full.housekeeping, (QList<int>{0, 1, 2, 3}));
    }

    // G-06: the three transmit roles, in order, come before every receive
    // worker after the first. Eight equal cores reserve six (N-2).
    void transmitRolesComeBeforeFurtherReceivers()
    {
        SysfsFixture f;
        f.online("0-7");
        PlacementDemand d = demand({0, 1, 2}, true, true, true);
        d.txIqSender = true;
        const PlacementPlan plan = planThreadPlacement(f.read(), d);
        QVERIFY(plan.active);
        QCOMPARE(plan.signalPool, (QList<int>{7, 6, 5, 4, 3, 2}));
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 0), 7);
        QCOMPARE(plan.cpuFor(ThreadRole::DspThread), 6);
        QCOMPARE(plan.cpuFor(ThreadRole::TxWorker, 5), 5);
        QCOMPARE(plan.cpuFor(ThreadRole::TxWorkerThread), 4);
        QCOMPARE(plan.cpuFor(ThreadRole::TxIqSender), 3);
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 1), 2);
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 2), -1);
    }

    void raspberryPiReservesTheTopTwoCores()
    {
        // Pi 4 and Pi 5: four equal cores, one clock domain.
        SysfsFixture f;
        f.online("0-3");
        for (int cpu = 0; cpu < 4; ++cpu) {
            f.capacity(cpu, 1024);
        }
        f.policy(0, "0 1 2 3");
        const PlacementPlan plan =
            planThreadPlacement(f.read(), demand({0}, true, true, true));
        QVERIFY(plan.active);
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 0), 3);
        QCOMPARE(plan.cpuFor(ThreadRole::DspThread), 2);
        QCOMPARE(plan.cpuFor(ThreadRole::TxWorker, 5), -1);
        QCOMPARE(plan.housekeeping, (QList<int>{0, 1}));
    }

    void x86WithoutCapacityUsesTheHighestCores()
    {
        SysfsFixture f;
        f.online("0-7");
        for (int cpu = 0; cpu < 8; ++cpu) {
            f.policy(cpu, QByteArray::number(cpu));
        }
        const CpuTopology topology = f.read();
        QVERIFY(topology.capacity.isEmpty());
        // Missing on every core is normal on x86, not a problem to report.
        QVERIFY(topology.problems.isEmpty());
        const PlacementPlan plan = planThreadPlacement(topology, demand({0}, true));
        QVERIFY(plan.active);
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 0), 7);
        QCOMPARE(plan.cpuFor(ThreadRole::DspThread), 6);
        QCOMPARE(plan.housekeeping, (QList<int>{0, 1, 2, 3, 4, 5}));
        QVERIFY(plan.reason.contains(QStringLiteral("unknown")));
    }

    void smtSiblingsAreNeverBothReserved()
    {
        // Eight logical CPUs on four cores: n and n+4 share a core.
        SysfsFixture f;
        f.online("0-7");
        for (int cpu = 0; cpu < 8; ++cpu) {
            f.siblings(cpu, QStringLiteral("%1,%2").arg(cpu % 4).arg(cpu % 4 + 4).toLatin1());
        }
        const CpuTopology topology = f.read();
        QCOMPARE(topology.coreSiblings.size(), 4);
        const PlacementPlan plan =
            planThreadPlacement(topology, demand({0, 1, 2}, true, true, true));
        QVERIFY(plan.active);
        const QList<int> reserved = plan.signalCores();
        QVERIFY(!reserved.isEmpty());
        QVERIFY(!reserved.contains(0));
        QVERIFY(!reserved.contains(4));  // CPU0's sibling
        for (int cpu : reserved) {
            QVERIFY2(!reserved.contains(cpu % 4 == cpu ? cpu + 4 : cpu - 4),
                     qPrintable(formatCpuList(reserved)));
        }
        // Four physical cores: at most two reserved.
        QCOMPARE(reserved, (QList<int>{6, 7}));
    }

    void twoCoresReserveOne()
    {
        SysfsFixture f;
        f.online("0-1");
        const PlacementPlan plan =
            planThreadPlacement(f.read(), demand({0}, true, true, true));
        QVERIFY(plan.active);
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 0), 1);
        QCOMPARE(plan.cpuFor(ThreadRole::DspThread), -1);
        QCOMPARE(plan.cpuFor(ThreadRole::TxWorker, 5), -1);
        QCOMPARE(plan.housekeeping, QList<int>{0});
    }

    void oneCoreDoesNothing()
    {
        SysfsFixture f;
        f.online("0");
        const PlacementPlan plan = planThreadPlacement(f.read(), demand({0}, true));
        QVERIFY(!plan.active);
        QVERIFY(plan.assignments.isEmpty());

        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        const QString line = placement.start(f.read(), demand({0}, true),
                                             std::make_unique<RecordingApi>(&calls, &current),
                                             true);
        // No thread moves, but nereusd still owns priority (Important 1).
        QCOMPARE(line, QStringLiteral("Thread placement: every thread may run on any core,"
                                      " because only one processor core is available;"
                                      " signal processing priority raised."));
        QVERIFY(placement.isActive());
        QVERIFY(!placement.isPlacing());
        QVERIFY(calls.isEmpty());
        QVERIFY(!placement.currentPlan().active);
    }

    void gapsInTheOnlineListAreRespected()
    {
        SysfsFixture f;
        f.online("0-3,6-7");
        const CpuTopology topology = f.read();
        QCOMPARE(topology.online, (QList<int>{0, 1, 2, 3, 6, 7}));
        const PlacementPlan plan = planThreadPlacement(topology, demand({0}, true));
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 0), 7);
        QCOMPARE(plan.cpuFor(ThreadRole::DspThread), 6);
        QCOMPARE(plan.housekeeping, (QList<int>{0, 1, 2, 3}));
    }

    void restrictedAllowedMaskIsRespected()
    {
        // systemd CPUAffinity=2-5 on the RK3588S: only 4-5 are fast.
        SysfsFixture f;
        layOutRk3588s(f);
        const PlacementPlan plan =
            planThreadPlacement(f.read({2, 3, 4, 5}), demand({0, 1}, true));
        QVERIFY(plan.active);
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 0), 4);
        QCOMPARE(plan.cpuFor(ThreadRole::DspThread), 5);
        QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 1), -1);
        QCOMPARE(plan.housekeeping, (QList<int>{2, 3}));

        // Uniform machine, mask of two cores: one reserved, never outside it.
        SysfsFixture g;
        g.online("0-7");
        const PlacementPlan narrow = planThreadPlacement(g.read({2, 3}), demand({0}, true));
        QCOMPARE(narrow.cpuFor(ThreadRole::RxWorker, 0), 3);
        QCOMPARE(narrow.housekeeping, QList<int>{2});
    }

    void missingOrGarbageFiles()
    {
        {
            SysfsFixture f;  // no online file at all
            const CpuTopology t = f.read();
            QVERIFY(t.online.isEmpty());
            QCOMPARE(t.problems.size(), 1);
            QVERIFY(!planThreadPlacement(t, demand({0}, true)).active);
        }
        {
            SysfsFixture f;
            f.online("zero to seven");
            const PlacementPlan plan = planThreadPlacement(f.read(), demand({0}, true));
            QVERIFY(!plan.active);
            QCOMPARE(plan.reason, QStringLiteral("the list of processor cores could not be read"));
        }
        {
            // Capacity on only some cores (one unreadable): uniform rules,
            // and the gap is reported.
            SysfsFixture f;
            layOutRk3588s(f);
            f.write(QStringLiteral("cpu6/cpu_capacity"), "garbage\n");
            const CpuTopology t = f.read();
            QVERIFY(!t.capacity.contains(6));
            QVERIFY(!t.problems.isEmpty());
            const PlacementPlan plan = planThreadPlacement(t, demand({0}, true));
            QVERIFY(plan.active);
            QCOMPARE(plan.cpuFor(ThreadRole::RxWorker, 0), 7);
            QCOMPARE(plan.cpuFor(ThreadRole::DspThread), 6);
        }
        {
            // Empty files read as absent.
            SysfsFixture f;
            f.online("0-3");
            f.write(QStringLiteral("cpu1/cpu_capacity"), "");
            f.write(QStringLiteral("cpufreq/policy0/related_cpus"), "");
            const CpuTopology t = f.read();
            QVERIFY(t.capacity.isEmpty());
            QVERIFY(t.clockDomains.isEmpty());
            QCOMPARE(planThreadPlacement(t, demand({0}, true)).cpuFor(ThreadRole::RxWorker, 0), 3);
        }
    }

    // ---------------------------------------------------------- startup line

    void rk3588sStartupLine()
    {
        SysfsFixture f;
        layOutRk3588s(f);
        // nereusd's startup demand with one slice (startDaemonThreadPlacement).
        const PlacementDemand startup = demand({0}, true, true, true);
        {
            QList<Call> calls;
            qint64 current = 1000;
            ThreadPlacement placement;
            const QString line = placement.start(
                f.read(), startup, std::make_unique<RecordingApi>(&calls, &current), true);
            QCOMPARE(line, QStringLiteral(
                "Thread placement: signal processing runs on cores 4-7, everything"
                " else on cores 0-3 (the fastest cores, from the kernel's core capacity"
                " data); signal processing priority raised."));
            QVERIFY(placement.isActive());
            // The starting (main) thread moves to the housekeeping cores.
            QCOMPARE(calls.size(), 1);
            QCOMPARE(calls.first().threadId, qint64(1000));
            QCOMPARE(calls.first().cpus, (QList<int>{0, 1, 2, 3}));
        }
        {
            QList<Call> calls;
            qint64 current = 1000;
            ThreadPlacement placement;
            const QString line = placement.start(
                f.read(), startup, std::make_unique<RecordingApi>(&calls, &current), false);
            QCOMPARE(line, QStringLiteral(
                "Thread placement: signal processing runs on cores 4-7, everything"
                " else on cores 0-3 (the fastest cores, from the kernel's core capacity"
                " data); raising signal processing priority is not permitted, so it"
                " runs at normal priority."));
        }
    }

    // ---------------------------------------------------------- registry

    void registryPromotesOnActivationAndReturnsOnStop()
    {
        SysfsFixture f;
        layOutRk3588s(f);
        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        placement.start(f.read(), demand({0}, true, true, true),
                        std::make_unique<RecordingApi>(&calls, &current), true);
        const QList<int> hk{0, 1, 2, 3};

        // RX0's worker starts idle: housekeeping, normal priority.
        current = 101;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        QCOMPARE(lastCpus(calls, 101), hk);
        QCOMPARE(lastNice(calls, 101), 0);

        // Order-independent: RX1 is activated before its worker registers.
        placement.setChannelActive(ThreadRole::RxWorker, 1, true);
        current = 102;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 1);
        QCOMPARE(lastCpus(calls, 102), QList<int>{4});
        QCOMPARE(lastNice(calls, 102), kDspNice);

        // RX0 activates: it is the first receive worker, so it takes 4.
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        QCOMPARE(lastCpus(calls, 101), QList<int>{4});
        QCOMPARE(lastNice(calls, 101), kDspNice);
        QCOMPARE(lastCpus(calls, 102), QList<int>{5});

        // The DSP thread takes the second slot; RX1 moves to 6.
        current = 200;
        placement.registerCurrentThread(ThreadRole::DspThread);
        QCOMPARE(lastCpus(calls, 200), QList<int>{5});
        QCOMPARE(lastNice(calls, 200), kDspNice);
        QCOMPARE(lastCpus(calls, 102), QList<int>{6});

        // Transmit worker and pump (G-06): while transmitting they come
        // before the second receiver, so the worker takes 6 and the pump 7,
        // and RX1, with no fast core left, runs on housekeeping at normal
        // priority, level with spectrum and networking there.
        current = 105;
        placement.onWdspThreadStarted(kWdspThreadTxMain, 5);
        QCOMPARE(lastCpus(calls, 105), hk);
        QCOMPARE(lastNice(calls, 105), 0);
        current = 300;
        placement.registerCurrentThread(ThreadRole::TxWorkerThread);
        QCOMPARE(lastNice(calls, 300), 0);
        placement.setChannelActive(ThreadRole::TxWorker, 5, true);
        QCOMPARE(lastCpus(calls, 105), QList<int>{6});
        QCOMPARE(lastNice(calls, 105), kDspNice);
        QCOMPARE(lastCpus(calls, 300), QList<int>{7});
        QCOMPARE(lastNice(calls, 300), kDspNice);
        QCOMPARE(lastCpus(calls, 102), hk);
        QCOMPARE(lastNice(calls, 102), 0);

        // RX0 stops: back to housekeeping at normal priority. RX1 is now
        // the first receive worker, so it takes 4 and the rest follow.
        placement.setChannelActive(ThreadRole::RxWorker, 0, false);
        QCOMPARE(lastCpus(calls, 101), hk);
        QCOMPARE(lastNice(calls, 101), 0);
        QCOMPARE(lastCpus(calls, 102), QList<int>{4});
        QCOMPARE(lastNice(calls, 102), kDspNice);
        QCOMPARE(lastCpus(calls, 200), QList<int>{5});
        QCOMPARE(lastCpus(calls, 105), QList<int>{6});
        QCOMPARE(lastCpus(calls, 300), QList<int>{7});
        QCOMPARE(lastNice(calls, 300), kDspNice);

        // Transmit ends.
        placement.setChannelActive(ThreadRole::TxWorker, 5, false);
        QCOMPARE(lastCpus(calls, 105), hk);
        QCOMPARE(lastNice(calls, 105), 0);
        QCOMPARE(lastCpus(calls, 300), hk);
        QCOMPARE(lastNice(calls, 300), 0);
        QCOMPARE(lastCpus(calls, 102), QList<int>{4});

        // Channel 1 closes: its thread is forgotten, never touched again.
        placement.forgetChannel(1);
        const int before = calls.size();
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        for (int i = before; i < calls.size(); ++i) {
            QVERIFY(calls.at(i).threadId != 102);
        }
        QCOMPARE(lastCpus(calls, 101), QList<int>{4});

        // A flush thread runs with everything else at normal priority.
        current = 400;
        placement.onWdspThreadStarted(kWdspThreadFlush, 0);
        QCOMPARE(lastCpus(calls, 400), hk);
        QCOMPARE(lastNice(calls, 400), 0);

        // FFTW planning runs on the first signal processing core.
        current = 500;
        placement.placeCurrentThreadOnFastCore();
        QCOMPARE(lastCpus(calls, 500), QList<int>{4});

        // The DSP thread ends.
        current = 200;
        placement.deregisterCurrentThread();
        QCOMPARE(placement.currentPlan().cpuFor(ThreadRole::DspThread), -1);
    }

    void aReplacedWorkerDropsTheOldThread()
    {
        // WDSP's own rebuilds (a rate change) start a new worker for the
        // same channel; the old thread's ID may be reused, so it is dropped.
        SysfsFixture f;
        layOutRk3588s(f);
        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        placement.start(f.read(), demand({0}, true),
                        std::make_unique<RecordingApi>(&calls, &current), true);
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        current = 101;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        current = 111;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        QCOMPARE(lastCpus(calls, 111), QList<int>{4});
        const int before = calls.size();
        placement.setChannelActive(ThreadRole::RxWorker, 0, false);
        for (int i = before; i < calls.size(); ++i) {
            QVERIFY(calls.at(i).threadId != 101);
        }
    }

    // R-R3-40: the display load governor keeps a copy of the plan and asks
    // for it again, under the registry's mutex, only when planRevision()
    // has moved; so every change of the plan must move it.
    void planRevisionMovesWithEveryPlanChange()
    {
        SysfsFixture f;
        layOutRk3588s(f);
        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        const quint64 unstarted = placement.planRevision();
        placement.start(f.read(), demand({0}, true),
                        std::make_unique<RecordingApi>(&calls, &current), true);
        const quint64 started = placement.planRevision();
        QVERIFY(started != unstarted);
        QCOMPARE(placement.currentPlan().cpuFor(ThreadRole::RxWorker, 0), -1);

        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        const quint64 active = placement.planRevision();
        QVERIFY(active != started);
        QCOMPARE(placement.currentPlan().cpuFor(ThreadRole::RxWorker, 0), 4);
        // Nothing changed: the revision stands.
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        QCOMPARE(placement.planRevision(), active);

        current = 201;
        placement.registerCurrentThread(ThreadRole::DspThread);
        const quint64 dsp = placement.planRevision();
        QVERIFY(dsp != active);
        QVERIFY(placement.currentPlan().cpuFor(ThreadRole::DspThread) >= 0);
        placement.forgetChannel(0);
        QVERIFY(placement.planRevision() != dsp);
        QCOMPARE(placement.currentPlan().cpuFor(ThreadRole::RxWorker, 0), -1);
    }

    void priorityNotPermittedMakesNoNiceCalls()
    {
        SysfsFixture f;
        layOutRk3588s(f);
        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        placement.start(f.read(), demand({0}, true),
                        std::make_unique<RecordingApi>(&calls, &current), false);
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        current = 101;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        current = 200;
        placement.registerCurrentThread(ThreadRole::DspThread);
        current = 400;
        placement.onWdspThreadStarted(kWdspThreadFlush, 0);
        QCOMPARE(lastCpus(calls, 101), QList<int>{4});
        QCOMPARE(lastCpus(calls, 200), QList<int>{5});
        QCOMPARE(niceCalls(calls), 0);
    }

    void dspNeverBelowHousekeepingWithoutPlacement_data()
    {
        QTest::addColumn<QString>("how");
        QTest::addColumn<bool>("permitted");
        for (const bool permitted : {true, false}) {
            const char* p = permitted ? " (raise permitted)" : " (raise not permitted)";
            QTest::newRow(qPrintable(QStringLiteral("one core") + QLatin1String(p)))
                << QStringLiteral("one core") << permitted;
            QTest::newRow(qPrintable(QStringLiteral("moves refused") + QLatin1String(p)))
                << QStringLiteral("moves refused") << permitted;
            QTest::newRow(qPrintable(QStringLiteral("off in nereusd.conf") + QLatin1String(p)))
                << QStringLiteral("off") << permitted;
        }
    }

    void dspNeverBelowHousekeepingWithoutPlacement()
    {
        // Important 1: when placement is inactive, no signal processing
        // role may end at a lower priority (higher nice) than spectrum or
        // networking.
        QFETCH(QString, how);
        QFETCH(bool, permitted);
        SysfsFixture f;
        if (how == QLatin1String("one core")) {
            f.online("0");
        } else {
            layOutRk3588s(f);
        }
        QList<Call> calls;
        qint64 current = 1;
        auto api = std::make_unique<RecordingApi>(&calls, &current);
        api->refuseAffinity = how == QLatin1String("moves refused");
        ThreadPlacement placement;
        if (how == QLatin1String("off")) {
            placement.startPriorityOnly(std::move(api), permitted,
                                        QStringLiteral("thread_placement is off in nereusd.conf"));
        } else {
            placement.start(f.read(), demand({0}, true, true, true), std::move(api), permitted);
        }
        const int movesAtStart = affinityCalls(calls);

        const QList<qint64> dsp = runEveryDspRole(placement, &current);
        for (qint64 tid : dsp) {
            QVERIFY2(niceOf(calls, tid) <= housekeepingNice(placement),
                     qPrintable(QStringLiteral("thread %1 at nice %2, housekeeping at %3")
                                    .arg(tid).arg(niceOf(calls, tid))
                                    .arg(housekeepingNice(placement))));
            // Priority only: every busy role is raised when permitted.
            QCOMPARE(niceOf(calls, tid), permitted ? kDspNice : 0);
        }
        QVERIFY(!placement.isPlacing());
        // No thread is moved after startup, flush threads and FFTW included.
        current = 400;
        placement.onWdspThreadStarted(kWdspThreadFlush, 0);
        current = 500;
        placement.placeCurrentThreadOnFastCore();
        QCOMPARE(affinityCalls(calls), movesAtStart);
        if (permitted) {
            QCOMPARE(lastNice(calls, 400), 0);
        }

        // A receive worker that stops goes back to normal priority.
        placement.setChannelActive(ThreadRole::RxWorker, 0, false);
        QCOMPARE(niceOf(calls, 101), 0);
    }

    void priorityOnlyStartupLines()
    {
        {
            QList<Call> calls;
            qint64 current = 1;
            ThreadPlacement placement;
            QCOMPARE(placement.startPriorityOnly(
                         std::make_unique<RecordingApi>(&calls, &current), true,
                         QStringLiteral("thread_placement is off in nereusd.conf")),
                     QStringLiteral("Thread placement: every thread may run on any core,"
                                    " because thread_placement is off in nereusd.conf;"
                                    " signal processing priority raised."));
        }
        {
            QList<Call> calls;
            qint64 current = 1;
            ThreadPlacement placement;
            QCOMPARE(placement.startPriorityOnly(
                         std::make_unique<RecordingApi>(&calls, &current), false,
                         QStringLiteral("thread_placement is off in nereusd.conf")),
                     QStringLiteral("Thread placement: every thread may run on any core,"
                                    " because thread_placement is off in nereusd.conf;"
                                    " raising signal processing priority is not permitted,"
                                    " so it runs at normal priority."));
        }
        {
            SysfsFixture f;
            layOutRk3588s(f);
            QList<Call> calls;
            qint64 current = 1;
            auto api = std::make_unique<RecordingApi>(&calls, &current);
            api->refuseAffinity = true;
            ThreadPlacement placement;
            QCOMPARE(placement.start(f.read(), demand({0}, true, true, true), std::move(api),
                                     true),
                     QStringLiteral("Thread placement: every thread may run on any core,"
                                    " because the system refused to move threads;"
                                    " signal processing priority raised."));
            QVERIFY(placement.isActive());
            QVERIFY(!placement.isPlacing());
        }
    }

    void aRoleWithoutItsOwnCoreIsNotRaised()
    {
        // Minor 2: while placing, raised priority goes only with a core of
        // its own. Two cores: RX0 takes 1; the DSP thread runs on 0 with
        // everything else, at normal priority.
        SysfsFixture f;
        f.online("0-1");
        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        placement.start(f.read(), demand({0}, true),
                        std::make_unique<RecordingApi>(&calls, &current), true);
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        current = 101;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        current = 200;
        placement.registerCurrentThread(ThreadRole::DspThread);
        QCOMPARE(lastCpus(calls, 101), QList<int>{1});
        QCOMPARE(lastNice(calls, 101), kDspNice);
        QCOMPARE(lastCpus(calls, 200), QList<int>{0});
        QCOMPARE(lastNice(calls, 200), 0);
        // RX0 stops: the DSP thread takes core 1 and is raised there.
        placement.setChannelActive(ThreadRole::RxWorker, 0, false);
        QCOMPARE(lastCpus(calls, 200), QList<int>{1});
        QCOMPARE(lastNice(calls, 200), kDspNice);
        QCOMPARE(lastNice(calls, 101), 0);
    }

    void aRefusedMoveIsNotRaised()
    {
        // Minor 2: a role whose move to its core is refused stays on the
        // housekeeping cores, so it is not raised either.
        SysfsFixture f;
        layOutRk3588s(f);
        QList<Call> calls;
        qint64 current = 1;
        auto api = std::make_unique<RecordingApi>(&calls, &current);
        api->refuseCpus = {4};
        ThreadPlacement placement;
        WarningCatcher catcher;   // the one expected refusal warning
        placement.start(f.read(), demand({0}, true), std::move(api), true);
        current = 200;
        placement.registerCurrentThread(ThreadRole::DspThread);
        QCOMPARE(lastCpus(calls, 200), QList<int>{4});   // asked, refused
        QCOMPARE(lastNice(calls, 200), 0);
        // RX0 activates and takes 4 (refused); the DSP thread moves to 5.
        current = 101;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        QCOMPARE(lastNice(calls, 101), 0);
        QCOMPARE(lastCpus(calls, 200), QList<int>{5});
        QCOMPARE(lastNice(calls, 200), kDspNice);
        QCOMPARE(catcher.warnings().size(), 1);
    }

    // R-R3-40/41: the display load governor leaves a receiver's load out
    // only when its worker and the DSP thread really run on their own
    // cores. A plan that gives them cores is not enough: a refused move
    // leaves the worker on the housekeeping cores, beside the display.
    void aRefusedMoveIsLeftOutOfTheAppliedPlan()
    {
        SysfsFixture f;
        layOutRk3588s(f);
        QList<Call> calls;
        qint64 current = 1;
        auto api = std::make_unique<RecordingApi>(&calls, &current);
        RecordingApi* const recording = api.get();
        ThreadPlacement placement;
        WarningCatcher catcher;   // the one expected refusal warning
        placement.start(f.read(), demand({0}, true), std::move(api), true);
        current = 200;
        placement.registerCurrentThread(ThreadRole::DspThread);
        // Not registered yet: the plan gives RX0 a core, nothing runs there.
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        const PlacementPlan planned = placement.currentPlan();
        QVERIFY(planned.cpuFor(ThreadRole::RxWorker, 0) >= 0);
        QVERIFY(planned.cpuFor(ThreadRole::DspThread) >= 0);
        QCOMPARE(placement.appliedPlan().cpuFor(ThreadRole::RxWorker, 0), -1);
        QCOMPARE(placement.appliedPlan().cpuFor(ThreadRole::DspThread),
                 planned.cpuFor(ThreadRole::DspThread));

        // RX0's worker starts and its move is refused.
        recording->refuseCpus = {planned.cpuFor(ThreadRole::RxWorker, 0)};
        current = 101;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        QCOMPARE(lastCpus(calls, 101), QList<int>{planned.cpuFor(ThreadRole::RxWorker, 0)});
        const PlacementPlan applied = placement.appliedPlan();
        QVERIFY(applied.active);
        QCOMPARE(applied.housekeeping, planned.housekeeping);
        QCOMPARE(applied.cpuFor(ThreadRole::RxWorker, 0), -1);
        QVERIFY(applied.cpuFor(ThreadRole::DspThread) >= 0);

        // What the governor sees: RX0's load counts by the applied plan,
        // and would have been left out by the plan alone.
        DisplayLoadInputs inputs;
        ReceiverDspLoad load;
        load.load = 0.85;
        inputs.receivers.append({0, load});
        const DisplayBudgetCharge charge = spectrumDisplayCost(1024, 30, false)->charge;
        inputs.placement = planned;
        QVERIFY(!displayLoadReadingFrom(inputs, 0, charge).highestReceiverLoad.has_value());
        inputs.placement = applied;
        QCOMPARE(displayLoadReadingFrom(inputs, 0, charge).highestReceiverLoad,
                 std::optional<double>(0.85));

        // Once a move succeeds, the worker is on its own core and left out.
        recording->refuseCpus.clear();
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        inputs.placement = placement.appliedPlan();
        QCOMPARE(inputs.placement.cpuFor(ThreadRole::RxWorker, 0),
                 planned.cpuFor(ThreadRole::RxWorker, 0));
        QVERIFY(!displayLoadReadingFrom(inputs, 0, charge).highestReceiverLoad.has_value());
        QCOMPARE(catcher.warnings().size(), 1);
    }

    void transmitKeyMovesOnTheRk3588s()
    {
        // Minor 3 (recorded, not changed): the order of dedicated cores is
        // recomputed on every change, so keying and unkeying one slice on
        // the Rock moves these threads each time. The Rock check expects
        // exactly these moves (se.nr_migrations grows by them per key and
        // unkey), and no others.
        SysfsFixture f;
        layOutRk3588s(f);
        const CpuTopology rk = f.read();
        const PlacementPlan receiving = planThreadPlacement(rk, demand({0}, true));
        QCOMPARE(receiving.cpuFor(ThreadRole::RxWorker, 0), 4);
        QCOMPARE(receiving.cpuFor(ThreadRole::DspThread), 5);
        // Keyed: RX0 stops (RadioModel stops the transmitting slice's
        // receiver), the transmit channel and its pump start.
        const PlacementPlan keyed = planThreadPlacement(rk, demand({}, true, true, true));
        QCOMPARE(keyed.cpuFor(ThreadRole::RxWorker, 0), -1);  // 4 -> 0-3
        QCOMPARE(keyed.cpuFor(ThreadRole::DspThread), 4);     // 5 -> 4
        QCOMPARE(keyed.cpuFor(ThreadRole::TxWorker), 5);      // 0-3 -> 5
        QCOMPARE(keyed.cpuFor(ThreadRole::TxWorkerThread), 6);  // 0-3 -> 6
        // Unkeyed: the reverse.
        const PlacementPlan back = planThreadPlacement(rk, demand({0}, true));
        QCOMPARE(back.cpuFor(ThreadRole::RxWorker, 0), 4);
        QCOMPARE(back.cpuFor(ThreadRole::DspThread), 5);
    }

    void transmitIqSenderTakesTheCoreAfterThePump()
    {
        // R-IOS-13, R-R3-42: the P2 transmit I/Q send thread is a role of
        // its own, after the transmit pump. Keyed on the Rock with one
        // slice it gets the last fast core; with more receive channels
        // busy there is none left and it shares cores 0-3.
        SysfsFixture f;
        layOutRk3588s(f);
        const CpuTopology rk = f.read();
        PlacementDemand keyed = demand({}, true, true, true);
        keyed.txIqSender = true;
        const PlacementPlan one = planThreadPlacement(rk, keyed);
        QCOMPARE(one.cpuFor(ThreadRole::TxWorkerThread), 6);
        QCOMPARE(one.cpuFor(ThreadRole::TxIqSender), 7);
        PlacementDemand busy = demand({1, 2}, true, true, true);
        busy.txIqSender = true;
        const PlacementPlan three = planThreadPlacement(rk, busy);
        QCOMPARE(three.cpuFor(ThreadRole::TxIqSender), -1);
        QCOMPARE(three.housekeeping, (QList<int>{0, 1, 2, 3}));
    }

    // ---------------------------------------------------------- RADE decoders
    // JJ's ruling of 2026-09-30: each RADE decoder shares the least busy
    // fast core, judged from the plan, never the housekeeping cores while
    // there is a fast core.

    void rk3588sRadeDecoders()
    {
        for (bool asRead : {true, false}) {
            SysfsFixture f;
            if (asRead) {
                layOutRk3588sAsRead(f);
            } else {
                layOutRk3588s(f);
            }
            const CpuTopology rk = f.read();
            // Receiving with two pans: rx0 on 4, DSP on 5, rx1 on 6, 7 free.
            const PlacementPlan none = planThreadPlacement(rk, radeDemand({0, 1}, {}));
            QCOMPARE(none.cpuFor(ThreadRole::RxWorker, 0), 4);
            QCOMPARE(none.cpuFor(ThreadRole::DspThread), 5);
            QCOMPARE(none.cpuFor(ThreadRole::RxWorker, 1), 6);
            QCOMPARE(none.signalCores(), (QList<int>{4, 5, 6}));

            // One decoder: the free core.
            const PlacementPlan one = planThreadPlacement(rk, radeDemand({0, 1}, {1}));
            QCOMPARE(one.cpuFor(ThreadRole::RxWorker, 0), 4);
            QCOMPARE(one.cpuFor(ThreadRole::DspThread), 5);
            QCOMPARE(one.cpuFor(ThreadRole::RxWorker, 1), 6);
            QCOMPARE(one.cpuFor(ThreadRole::RadeDecoder, 1), 7);
            QCOMPARE(one.housekeeping, (QList<int>{0, 1, 2, 3}));
            QCOMPARE(one.signalCores(), (QList<int>{4, 5, 6, 7}));

            // Two: the second avoids the first decoder's core; 4, 5 and 6
            // are equally busy, the DSP core goes last, 4 is earlier.
            const PlacementPlan two = planThreadPlacement(rk, radeDemand({0, 1}, {0, 1}));
            QCOMPARE(two.cpuFor(ThreadRole::RadeDecoder, 0), 7);
            QCOMPARE(two.cpuFor(ThreadRole::RadeDecoder, 1), 4);
            QCOMPARE(two.cpuFor(ThreadRole::DspThread), 5);
            QCOMPARE(two.housekeeping, (QList<int>{0, 1, 2, 3}));
            // Each core once in the startup line's list.
            QCOMPARE(two.signalCores(), (QList<int>{4, 5, 6, 7}));

            // Four: one per fast core, the DSP core last.
            const PlacementPlan four =
                planThreadPlacement(rk, radeDemand({0, 1}, {0, 1, 2, 3}));
            QCOMPARE(four.cpuFor(ThreadRole::RadeDecoder, 0), 7);
            QCOMPARE(four.cpuFor(ThreadRole::RadeDecoder, 1), 4);
            QCOMPARE(four.cpuFor(ThreadRole::RadeDecoder, 2), 6);
            QCOMPARE(four.cpuFor(ThreadRole::RadeDecoder, 3), 5);
            QCOMPARE(four.housekeeping, (QList<int>{0, 1, 2, 3}));
        }
    }

    void rk3588sKeyedRadeDecoders()
    {
        SysfsFixture f;
        layOutRk3588sAsRead(f);
        const CpuTopology rk = f.read();
        // Keyed on pan 0 (its receiver stops; the transmit worker and pump
        // take 6 and 7, the I/Q sender has no fast core left).
        PlacementDemand keyed = demand({1}, true, true, true);
        keyed.txIqSender = true;
        const PlacementPlan base = planThreadPlacement(rk, keyed);
        QCOMPARE(base.cpuFor(ThreadRole::RxWorker, 1), 4);
        QCOMPARE(base.cpuFor(ThreadRole::DspThread), 5);
        QCOMPARE(base.cpuFor(ThreadRole::TxWorker), 6);
        QCOMPARE(base.cpuFor(ThreadRole::TxWorkerThread), 7);
        QCOMPARE(base.cpuFor(ThreadRole::TxIqSender), -1);
        const QList<int> transmitCores{6, 7};

        // Pan 1 in RADE: every fast core has no decoder and one role, so
        // the transmit cores are not taken: 4 (the DSP core goes last).
        keyed.radeDecoders = {1};
        const PlacementPlan one = planThreadPlacement(rk, keyed);
        QCOMPARE(one.cpuFor(ThreadRole::RadeDecoder, 1), 4);
        QVERIFY(!transmitCores.contains(one.cpuFor(ThreadRole::RadeDecoder, 1)));
        // The transmit roles keep their cores (G-06).
        QCOMPARE(one.cpuFor(ThreadRole::TxWorker), 6);
        QCOMPARE(one.cpuFor(ThreadRole::TxWorkerThread), 7);

        // Both pans in RADE (pan 0's decoder is gated, not stopped, so it
        // is still placed). The first takes 4. For the second, 5, 6 and 7
        // have no decoder and one role each; the transmit cores go last,
        // then the DSP core, so it takes 5 and 6 and 7 stay with the
        // transmit path.
        keyed.radeDecoders = {0, 1};
        const PlacementPlan two = planThreadPlacement(rk, keyed);
        QCOMPARE(two.cpuFor(ThreadRole::RadeDecoder, 0), 4);
        QCOMPARE(two.cpuFor(ThreadRole::RadeDecoder, 1), 5);
        QVERIFY(!transmitCores.contains(two.cpuFor(ThreadRole::RadeDecoder, 0)));
        QVERIFY(!transmitCores.contains(two.cpuFor(ThreadRole::RadeDecoder, 1)));
        QCOMPARE(two.cpuFor(ThreadRole::TxWorker), 6);
        QCOMPARE(two.cpuFor(ThreadRole::TxWorkerThread), 7);
    }

    void moreRadeDecodersThanFastCores()
    {
        SysfsFixture f;
        layOutRk3588sAsRead(f);
        const PlacementPlan plan =
            planThreadPlacement(f.read(), radeDemand({0, 1}, {0, 1, 2, 3, 4}));
        QVERIFY(plan.active);
        // One per fast core first (7, 4, 6, then the DSP core 5), then a
        // second on 7, the least busy once every core has a decoder.
        QCOMPARE(plan.cpuFor(ThreadRole::RadeDecoder, 0), 7);
        QCOMPARE(plan.cpuFor(ThreadRole::RadeDecoder, 1), 4);
        QCOMPARE(plan.cpuFor(ThreadRole::RadeDecoder, 2), 6);
        QCOMPARE(plan.cpuFor(ThreadRole::RadeDecoder, 3), 5);
        QCOMPARE(plan.cpuFor(ThreadRole::RadeDecoder, 4), 7);
        // Never onto the housekeeping cores while a fast core exists.
        QCOMPARE(plan.housekeeping, (QList<int>{0, 1, 2, 3}));
        QCOMPARE(plan.signalCores(), (QList<int>{4, 5, 6, 7}));
    }

    void radeDecodersOnEqualCores()
    {
        // Pi 4 and Pi 5: two reserved cores (rx0 on 3, DSP on 2).
        SysfsFixture f;
        f.online("0-3");
        for (int cpu = 0; cpu < 4; ++cpu) {
            f.capacity(cpu, 1024);
        }
        f.policy(0, "0 1 2 3");
        const CpuTopology pi = f.read();
        const PlacementPlan one = planThreadPlacement(pi, radeDemand({0}, {0}));
        QCOMPARE(one.cpuFor(ThreadRole::RxWorker, 0), 3);
        QCOMPARE(one.cpuFor(ThreadRole::DspThread), 2);
        QCOMPARE(one.cpuFor(ThreadRole::RadeDecoder, 0), 3);
        QCOMPARE(one.housekeeping, (QList<int>{0, 1}));

        // Two pans receiving: rx1 has no reserved core left; the decoders
        // spread, the DSP core second.
        const PlacementPlan two = planThreadPlacement(pi, radeDemand({0, 1}, {0, 1}));
        QCOMPARE(two.cpuFor(ThreadRole::RxWorker, 0), 3);
        QCOMPARE(two.cpuFor(ThreadRole::DspThread), 2);
        QCOMPARE(two.cpuFor(ThreadRole::RxWorker, 1), -1);
        QCOMPARE(two.cpuFor(ThreadRole::RadeDecoder, 0), 3);
        QCOMPARE(two.cpuFor(ThreadRole::RadeDecoder, 1), 2);
        QCOMPARE(two.housekeeping, (QList<int>{0, 1}));

        // Two pans, keyed on pan 0: the transmit roles have no reserved
        // core, rx1 takes 3; the decoders are where they were.
        PlacementDemand keyed = demand({1}, true, true, true);
        keyed.txIqSender = true;
        keyed.radeDecoders = {0, 1};
        const PlacementPlan tx = planThreadPlacement(pi, keyed);
        QCOMPARE(tx.cpuFor(ThreadRole::RxWorker, 1), 3);
        QCOMPARE(tx.cpuFor(ThreadRole::DspThread), 2);
        QCOMPARE(tx.cpuFor(ThreadRole::TxWorker), -1);
        QCOMPARE(tx.cpuFor(ThreadRole::RadeDecoder, 0), 3);
        QCOMPARE(tx.cpuFor(ThreadRole::RadeDecoder, 1), 2);
    }

    void radeDecodersWithUnknownCoreSpeeds()
    {
        // x86 without capacity data: six reserved cores, two in use, so
        // each decoder takes a free reserved core, highest first, and
        // that core leaves housekeeping.
        SysfsFixture f;
        f.online("0-7");
        const CpuTopology x86 = f.read();
        QVERIFY(x86.capacity.isEmpty());
        const PlacementPlan one = planThreadPlacement(x86, radeDemand({0}, {0}));
        QCOMPARE(one.cpuFor(ThreadRole::RxWorker, 0), 7);
        QCOMPARE(one.cpuFor(ThreadRole::DspThread), 6);
        QCOMPARE(one.cpuFor(ThreadRole::RadeDecoder, 0), 5);
        QCOMPARE(one.housekeeping, (QList<int>{0, 1, 2, 3, 4}));
        const PlacementPlan two = planThreadPlacement(x86, radeDemand({0}, {0, 1}));
        QCOMPARE(two.cpuFor(ThreadRole::RadeDecoder, 0), 5);
        QCOMPARE(two.cpuFor(ThreadRole::RadeDecoder, 1), 4);
        QCOMPARE(two.housekeeping, (QList<int>{0, 1, 2, 3}));
    }

    void radeDecoderWithoutAFastCoreIsNotPlaced()
    {
        SysfsFixture f;
        f.online("0");
        const PlacementPlan plan = planThreadPlacement(f.read(), radeDemand({0}, {0}));
        QVERIFY(!plan.active);
        QCOMPARE(plan.cpuFor(ThreadRole::RadeDecoder, 0), -1);
    }

    void radeDecoderThreadsArePlacedAndLoggedOnce()
    {
        SysfsFixture f;
        layOutRk3588sAsRead(f);
        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        startRockReceivingTwoPans(placement, f, calls, current);

        DecoderLineCatcher lines;
        const quint64 before = placement.planRevision();
        current = 400;
        placement.registerCurrentThread(ThreadRole::RadeDecoder, 1);
        QVERIFY(placement.planRevision() != before);
        QCOMPARE(lastCpus(calls, 400), QList<int>{7});
        QCOMPARE(lastNice(calls, 400), kDspNice);
        QCOMPARE(placement.appliedPlan().cpuFor(ThreadRole::RadeDecoder, 1), 7);

        // A second decoder, on a lower channel, does not move the first.
        current = 401;
        placement.registerCurrentThread(ThreadRole::RadeDecoder, 0);
        QCOMPARE(lastCpus(calls, 401), QList<int>{4});
        QCOMPARE(lastCpus(calls, 400), QList<int>{7});
        // The receive workers and the DSP thread keep their cores.
        QCOMPARE(lastCpus(calls, 101), QList<int>{4});
        QCOMPARE(lastCpus(calls, 200), QList<int>{5});
        QCOMPARE(lastCpus(calls, 102), QList<int>{6});

        // The later decoder removed: the plan is recomputed, and in this
        // layout it still gives the earlier decoder 7, so it is not moved.
        placement.deregisterCurrentThread();
        QCOMPARE(placement.currentPlan().cpuFor(ThreadRole::RadeDecoder, 0), -1);
        QCOMPARE(placement.currentPlan().cpuFor(ThreadRole::RadeDecoder, 1), 7);
        QCOMPARE(lastCpus(calls, 400), QList<int>{7});

        // One line per choice, none for the recomputes that moved nothing.
        QCOMPARE(lines.lines(),
                 (QStringList{
                     QStringLiteral("Thread placement: the RADE decoder for channel 1"
                                    " runs on core 7."),
                     QStringLiteral("Thread placement: the RADE decoder for channel 0"
                                    " runs on core 4.")}));
    }

    void removingTheEarlierDecoderMovesTheLaterOne()
    {
        SysfsFixture f;
        layOutRk3588sAsRead(f);
        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        startRockReceivingTwoPans(placement, f, calls, current);
        current = 400;
        placement.registerCurrentThread(ThreadRole::RadeDecoder, 1);
        current = 401;
        placement.registerCurrentThread(ThreadRole::RadeDecoder, 0);
        QCOMPARE(lastCpus(calls, 400), QList<int>{7});
        QCOMPARE(lastCpus(calls, 401), QList<int>{4});

        // The earlier-started decoder (channel 1, on 7) ends: the remaining
        // one is now the first decoder and moves to the free core, 7. No
        // other thread moves.
        DecoderLineCatcher lines;
        const int movesBefore = affinityCalls(calls);
        current = 400;
        placement.deregisterCurrentThread();
        QCOMPARE(placement.currentPlan().cpuFor(ThreadRole::RadeDecoder, 1), -1);
        QCOMPARE(placement.currentPlan().cpuFor(ThreadRole::RadeDecoder, 0), 7);
        QCOMPARE(lastCpus(calls, 401), QList<int>{7});
        QCOMPARE(affinityCalls(calls), movesBefore + 1);
        QCOMPARE(lastCpus(calls, 101), QList<int>{4});
        QCOMPARE(lastCpus(calls, 200), QList<int>{5});
        QCOMPARE(lastCpus(calls, 102), QList<int>{6});
        QCOMPARE(lines.lines(),
                 QStringList{QStringLiteral("Thread placement: the RADE decoder for"
                                            " channel 0 runs on core 7.")});
    }

    void refusalsAreWarnedOnce()
    {
        // Minor 5: the first refusal of each kind is one warning; repeats
        // are not, however often the plan is applied.
        SysfsFixture f;
        layOutRk3588s(f);
        QList<Call> calls;
        qint64 current = 1;
        auto api = std::make_unique<RecordingApi>(&calls, &current);
        api->refuseCpus = {4};   // an offline or forbidden core
        api->refuseNice = true;
        ThreadPlacement placement;
        WarningCatcher catcher;
        placement.start(f.read(), demand({0}, true), std::move(api), true);
        QVERIFY(placement.isPlacing());
        current = 101;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        current = 200;
        placement.registerCurrentThread(ThreadRole::DspThread);
        for (int i = 0; i < 5; ++i) {
            placement.setChannelActive(ThreadRole::RxWorker, 0, true);
            placement.setChannelActive(ThreadRole::RxWorker, 0, false);
        }
        const QStringList warnings = catcher.warnings();
        QCOMPARE(warnings.size(), 2);
        QVERIFY(warnings.at(0).contains(QLatin1String("refused to change")));
        QVERIFY(warnings.at(1).contains(QLatin1String("refused to move")));
        QVERIFY(warnings.at(1).contains(QLatin1String("cores 4")));
    }

    void aFinishedWorkerIsForgotten()
    {
        // Minor 4: a worker reports that it is about to end; if its
        // channel's rebuild never starts a new one, the old thread ID is
        // not touched again.
        SysfsFixture f;
        layOutRk3588s(f);
        QList<Call> calls;
        qint64 current = 1;
        ThreadPlacement placement;
        placement.start(f.read(), demand({0}, true),
                        std::make_unique<RecordingApi>(&calls, &current), true);
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        current = 101;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        QCOMPARE(lastCpus(calls, 101), QList<int>{4});
        placement.onWdspThreadStarted(kWdspThreadWorkerExit, 0);
        const int before = calls.size();
        current = 200;
        placement.registerCurrentThread(ThreadRole::DspThread);
        placement.setChannelActive(ThreadRole::RxWorker, 0, false);
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        for (int i = before; i < calls.size(); ++i) {
            QVERIFY(calls.at(i).threadId != 101);
        }
        // The channel's state is kept for the worker that replaces it.
        current = 111;
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        QCOMPARE(lastCpus(calls, 111), QList<int>{4});
    }

    void anInactiveRegistryDoesNothing()
    {
        // The GUI never starts placement: every entry point is a no-op.
        ThreadPlacement placement;
        QVERIFY(!placement.isActive());
        placement.registerCurrentThread(ThreadRole::DspThread);
        placement.setChannelActive(ThreadRole::RxWorker, 0, true);
        placement.onWdspThreadStarted(kWdspThreadRxMain, 0);
        placement.placeCurrentThreadOnFastCore();
        placement.forgetChannel(0);
        placement.deregisterCurrentThread();
        QVERIFY(!placement.isActive());
        QVERIFY(!ThreadPlacement::managesThreadPriority());
    }

    // ---------------------------------------------------------- Linux smoke

    void linuxSystemCallsMoveAndRenice()
    {
#ifndef Q_OS_LINUX
        QSKIP("Thread placement makes real system calls only on Linux.");
#else
        cpu_set_t original;
        CPU_ZERO(&original);
        QCOMPARE(sched_getaffinity(0, sizeof(original), &original), 0);
        QList<int> allowed;
        for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (CPU_ISSET(cpu, &original)) {
                allowed.append(cpu);
            }
        }
        QVERIFY(!allowed.isEmpty());

        // A helper thread, so the test thread keeps its mask and nice.
        const std::unique_ptr<ThreadSchedulingApi> api = makeSystemThreadSchedulingApi();
        bool moved = false;
        bool readBack = false;
        bool reniced = false;
        int niceRead = 0;
        int niceRequested = 0;
        bool initialNiceRead = false;
        std::thread helper([&]() {
            const qint64 tid = api->currentThreadId();
            moved = api->setAffinity(tid, {allowed.last()});
            cpu_set_t now;
            CPU_ZERO(&now);
            readBack = sched_getaffinity(0, sizeof(now), &now) == 0
                && CPU_COUNT(&now) == 1 && CPU_ISSET(allowed.last(), &now);
            // Inherit the runner's nice level; never require permission to
            // raise priority when the suite itself runs at nice 10.
            errno = 0;
            const int inheritedNice = getpriority(PRIO_PROCESS, static_cast<id_t>(tid));
            initialNiceRead = errno == 0;
            niceRequested = std::min(19, inheritedNice + 1);
            reniced = initialNiceRead && api->setNice(tid, niceRequested);
            errno = 0;
            niceRead = getpriority(PRIO_PROCESS, static_cast<id_t>(tid));
        });
        helper.join();
        QVERIFY(moved);
        QVERIFY(readBack);
        QVERIFY(initialNiceRead);
        QVERIFY(reniced);
        QCOMPARE(niceRead, niceRequested);

        // The startup probe puts the thread's own level back either way.
        int niceBefore = 99;
        int niceAfter = 98;
        bool permitted = false;
        bool probeReadOk = false;
        std::thread prober([&]() {
            const auto tid = static_cast<id_t>(::syscall(SYS_gettid));
            errno = 0;
            niceBefore = getpriority(PRIO_PROCESS, tid);
            probeReadOk = errno == 0;
            permitted = canRaiseCurrentThreadPriority(kDspNice);
            errno = 0;
            niceAfter = getpriority(PRIO_PROCESS, tid);
            probeReadOk = probeReadOk && errno == 0;
        });
        prober.join();
        QVERIFY(probeReadOk);
        QCOMPARE(niceAfter, niceBefore);
        Q_UNUSED(permitted);
#endif
    }
};

QTEST_GUILESS_MAIN(TestThreadPlacement)
#include "tst_thread_placement.moc"
