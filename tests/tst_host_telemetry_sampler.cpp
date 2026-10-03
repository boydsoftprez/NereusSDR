// =================================================================
// tests/tst_host_telemetry_sampler.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Fixture-driven tests for the Core's
// procfs/sysfs host sampler (R-R3-32, R-R3-33). Every file lives under a
// temporary root; nothing reads the build machine's /proc or /sys.
// =================================================================

#include <QtTest>

#include "core/daemon/DisplayLoadInputs.h"
#include "core/daemon/HostTelemetrySampler.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace NereusSDR;

namespace {

// A /proc/self/stat line whose command holds spaces and parentheses, so
// field counting must start after the last ')'. utime and stime are the
// 14th and 15th fields.
QByteArray processStat(quint64 utime, quint64 stime)
{
    return QByteArray("4242 (nereusd (core) x) S 1 4242 4242 0 -1 4194560 900 0 0 0 ")
        + QByteArray::number(utime) + ' ' + QByteArray::number(stime)
        + " 0 0 20 0 9 0 12345 104857600 5000 18446744073709551615\n";
}

QByteArray meminfo(bool withAvailable = true)
{
    QByteArray text("MemTotal:        8000000 kB\nMemFree:         1000000 kB\n");
    if (withAvailable) {
        text += "MemAvailable:    6500000 kB\n";
    }
    text += "Buffers:          100000 kB\nCached:          2000000 kB\n";
    return text;
}

QByteArray status()
{
    return "Name:\tnereusd\nState:\tS (sleeping)\nVmPeak:\t  300000 kB\n"
           "VmRSS:\t   51234 kB\nThreads:\t12\n";
}

class Fixture {
public:
    Fixture()
    {
        Q_ASSERT(m_dir.isValid());
        QDir(m_dir.path()).mkpath(QStringLiteral("proc/self"));
        QDir(m_dir.path()).mkpath(QStringLiteral("sys/class/thermal"));
    }

    QString root() const { return m_dir.path(); }

    void write(const QString& relative, const QByteArray& contents)
    {
        const QString path = m_dir.filePath(relative);
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(contents), contents.size());
    }

    void remove(const QString& relative) { QFile::remove(m_dir.filePath(relative)); }

    void zone(int number, const QByteArray& type, const QByteArray& temp)
    {
        const QString dir = QStringLiteral("sys/class/thermal/thermal_zone%1/").arg(number);
        if (!type.isNull()) { write(dir + QStringLiteral("type"), type); }
        if (!temp.isNull()) { write(dir + QStringLiteral("temp"), temp); }
    }

    // Real sysfs zones are symlinks from /sys/class/thermal into
    // /sys/devices/virtual/thermal; this lays one out the same way.
    void linkedZone(int number, const QByteArray& type, const QByteArray& temp)
    {
#ifdef Q_OS_UNIX
        const QString target = QStringLiteral("sys/devices/virtual/thermal/thermal_zone%1").arg(number);
        QVERIFY(QDir(m_dir.path()).mkpath(target));
        if (!type.isNull()) { write(target + QStringLiteral("/type"), type); }
        if (!temp.isNull()) { write(target + QStringLiteral("/temp"), temp); }
        const QString link = m_dir.filePath(
            QStringLiteral("sys/class/thermal/thermal_zone%1").arg(number));
        QVERIFY(QFile::link(m_dir.filePath(target), link));
        QVERIFY(QFileInfo(link).isSymLink());
        QVERIFY(QFileInfo(link).isDir());
#else
        zone(number, type, temp);
#endif
    }

    void standardProc()
    {
        write(QStringLiteral("proc/stat"),
              "cpu  100 0 100 700 100 0 0 0 50 0\ncpu0 50 0 50 350 50 0 0 0 25 0\n");
        write(QStringLiteral("proc/self/stat"), processStat(40, 10));
        write(QStringLiteral("proc/meminfo"), meminfo());
        write(QStringLiteral("proc/self/status"), status());
    }

private:
    QTemporaryDir m_dir;
};

} // namespace

class TstHostTelemetrySampler final : public QObject {
    Q_OBJECT
private slots:
    void cpuPercentagesComeFromTwoSamples()
    {
        Fixture f;
        f.standardProc();
        HostTelemetrySampler sampler(f.root());
        QVERIFY(sampler.isEnabled());

        StationHostTelemetry host = sampler.sample();
        QVERIFY(!host.systemCpuPercent); // no baseline yet
        QVERIFY(!host.processCpuPercent);
        QCOMPARE(host.memoryTotalKiB, std::optional<qint64>(8000000));

        // Total over the first eight fields: 1000 then 2000 (guest 50 and
        // 80 are already inside user). Idle plus iowait: 800 then 1500.
        // Busy 300 of 1000 ticks is 30 %; the process used 100 of them.
        f.write(QStringLiteral("proc/stat"), "cpu  200 0 200 1250 250 50 50 0 80 0\n");
        f.write(QStringLiteral("proc/self/stat"), processStat(100, 50));
        host = sampler.sample();
        QVERIFY(host.systemCpuPercent);
        QVERIFY(host.processCpuPercent);
        QCOMPARE(*host.systemCpuPercent, 30.0);
        QCOMPARE(*host.processCpuPercent, 10.0);

        // An idle interval is a measured zero, not an absence.
        f.write(QStringLiteral("proc/stat"), "cpu  200 0 200 2250 250 50 50 0 80 0\n");
        host = sampler.sample();
        QCOMPARE(host.systemCpuPercent, std::optional<double>(0.0));
        QCOMPARE(host.processCpuPercent, std::optional<double>(0.0));

        // reset() starts a new baseline, as a new telemetry session does.
        sampler.reset();
        QVERIFY(!sampler.sample().systemCpuPercent);
    }

    void counterWrapOrResetIsAbsentNotNegative()
    {
        Fixture f;
        f.standardProc();
        HostTelemetrySampler sampler(f.root());
        sampler.sample();

        // System counters went backwards (reset); the process counter too.
        f.write(QStringLiteral("proc/stat"), "cpu  10 0 10 70 10 0 0 0 0 0\n");
        f.write(QStringLiteral("proc/self/stat"), processStat(1, 1));
        StationHostTelemetry host = sampler.sample();
        QVERIFY(!host.systemCpuPercent);
        QVERIFY(!host.processCpuPercent);

        // The reset value is the new baseline: the next interval reports.
        f.write(QStringLiteral("proc/stat"), "cpu  60 0 10 120 10 0 0 0 0 0\n");
        f.write(QStringLiteral("proc/self/stat"), processStat(11, 1));
        host = sampler.sample();
        QCOMPARE(host.systemCpuPercent, std::optional<double>(50.0));
        QCOMPARE(host.processCpuPercent, std::optional<double>(10.0));

        // Only the process counter regresses: the system value survives.
        f.write(QStringLiteral("proc/stat"), "cpu  110 0 10 170 10 0 0 0 0 0\n");
        f.write(QStringLiteral("proc/self/stat"), processStat(0, 0));
        host = sampler.sample();
        QCOMPARE(host.systemCpuPercent, std::optional<double>(50.0));
        QVERIFY(!host.processCpuPercent);

        // Idle alone going backwards (a known iowait quirk) is absent too.
        f.write(QStringLiteral("proc/stat"), "cpu  210 0 10 160 10 0 0 0 0 0\n");
        host = sampler.sample();
        QVERIFY(!host.systemCpuPercent);
    }

    void missingFilesMakeOnlyTheirValuesAbsent()
    {
        Fixture f;
        f.standardProc();
        HostTelemetrySampler sampler(f.root());
        sampler.sample();

        // No /proc/stat: both CPU values need it; memory is unaffected.
        f.remove(QStringLiteral("proc/stat"));
        f.write(QStringLiteral("proc/self/stat"), processStat(50, 10));
        StationHostTelemetry host = sampler.sample();
        QVERIFY(!host.systemCpuPercent);
        QVERIFY(!host.processCpuPercent);
        QCOMPARE(host.memoryTotalKiB, std::optional<qint64>(8000000));
        QCOMPARE(host.memoryAvailableKiB, std::optional<qint64>(6500000));
        QCOMPARE(host.processResidentKiB, std::optional<qint64>(51234));

        // The first sample after a reading error has no baseline either.
        f.write(QStringLiteral("proc/stat"), "cpu  100 0 100 700 100 0 0 0 0 0\n");
        QVERIFY(!sampler.sample().systemCpuPercent);
        f.write(QStringLiteral("proc/stat"), "cpu  200 0 100 800 100 0 0 0 0 0\n");
        f.write(QStringLiteral("proc/self/stat"), processStat(70, 10));
        host = sampler.sample();
        QCOMPARE(host.systemCpuPercent, std::optional<double>(50.0));
        QCOMPARE(host.processCpuPercent, std::optional<double>(10.0));

        // No /proc/self/stat: the system value is still measured.
        f.remove(QStringLiteral("proc/self/stat"));
        f.write(QStringLiteral("proc/stat"), "cpu  300 0 100 900 100 0 0 0 0 0\n");
        host = sampler.sample();
        QCOMPARE(host.systemCpuPercent, std::optional<double>(50.0));
        QVERIFY(!host.processCpuPercent);

        // A malformed process line is a reading error, not a zero.
        f.write(QStringLiteral("proc/self/stat"), "4242 (nereusd) S 1 2\n");
        QVERIFY(!sampler.sample().processCpuPercent);

        // An older kernel without MemAvailable: only that value is absent.
        f.write(QStringLiteral("proc/meminfo"), meminfo(false));
        host = sampler.sample();
        QCOMPARE(host.memoryTotalKiB, std::optional<qint64>(8000000));
        QVERIFY(!host.memoryAvailableKiB);

        f.remove(QStringLiteral("proc/meminfo"));
        f.remove(QStringLiteral("proc/self/status"));
        f.write(QStringLiteral("proc/stat"), "cpu  400 0 100 1000 100 0 0 0 0 0\n");
        host = sampler.sample();
        QVERIFY(!host.memoryTotalKiB);
        QVERIFY(!host.memoryAvailableKiB);
        QVERIFY(!host.processResidentKiB);
        QVERIFY(!host.hottestZoneCelsius); // no zones in this fixture
        QVERIFY(host.hottestZoneName.isEmpty());
        QVERIFY(host.systemCpuPercent); // /proc/stat is still there
    }

    void hottestThermalZoneIsPickedWithItsType()
    {
        Fixture f;
        f.standardProc();
        // The RK3588S names its zones by block; values are millidegrees.
        f.zone(0, "soc-thermal\n", "45000\n");
        f.linkedZone(1, "bigcore0-thermal\n", "52500\n"); // a symlink, as in sysfs
        f.zone(2, "gpu-thermal\n", QByteArray());   // temp missing
        f.zone(3, "littlecore-thermal\n", "-5000\n"); // below zero is real
        f.zone(4, "center-thermal\n", "garbage\n");  // unreadable value
        f.zone(10, "npu-thermal\n", "52500\n");      // tie: lower number wins
        HostTelemetrySampler sampler(f.root());
        StationHostTelemetry host = sampler.sample();
        QCOMPARE(host.hottestZoneCelsius, std::optional<double>(52.5));
        QCOMPARE(host.hottestZoneName, QStringLiteral("bigcore0-thermal"));

        // Values are read each sample; the names were read at discovery.
        f.zone(10, QByteArray(), "71250\n");
        host = sampler.sample();
        QCOMPARE(host.hottestZoneCelsius, std::optional<double>(71.25));
        QCOMPARE(host.hottestZoneName, QStringLiteral("npu-thermal"));

        // Only sub-zero zones left: a negative reading is still a reading.
        Fixture cold;
        cold.zone(0, "soc-thermal\n", "-12500\n");
        cold.zone(1, "bigcore0-thermal\n", "-300000\n"); // below absolute zero
        HostTelemetrySampler coldSampler(cold.root());
        host = coldSampler.sample();
        QCOMPARE(host.hottestZoneCelsius, std::optional<double>(-12.5));
        QCOMPARE(host.hottestZoneName, QStringLiteral("soc-thermal"));
        QVERIFY(!host.memoryTotalKiB); // this fixture has no proc files
    }

    void zoneWithoutTypeKeepsItsTemperature()
    {
        Fixture f;
        f.zone(0, QByteArray(), "48000\n");
        HostTelemetrySampler sampler(f.root());
        const StationHostTelemetry host = sampler.sample();
        QCOMPARE(host.hottestZoneCelsius, std::optional<double>(48.0));
        QVERIFY(host.hottestZoneName.isEmpty());
    }

    void unreadableFileIsAbsent()
    {
#ifdef Q_OS_UNIX
        if (::geteuid() == 0) {
            QSKIP("root reads any file regardless of mode");
        }
        Fixture f;
        f.standardProc();
        f.zone(0, "soc-thermal\n", "45000\n");
        f.zone(1, "gpu-thermal\n", "90000\n");
        const QString hot = f.root() + QStringLiteral("/sys/class/thermal/thermal_zone1/temp");
        QVERIFY(QFile::setPermissions(hot, QFileDevice::Permissions{}));
        const QString meminfoPath = f.root() + QStringLiteral("/proc/meminfo");
        QVERIFY(QFile::setPermissions(meminfoPath, QFileDevice::Permissions{}));
        HostTelemetrySampler sampler(f.root());
        const StationHostTelemetry host = sampler.sample();
        QCOMPARE(host.hottestZoneCelsius, std::optional<double>(45.0));
        QCOMPARE(host.hottestZoneName, QStringLiteral("soc-thermal"));
        QVERIFY(!host.memoryTotalKiB);
        QCOMPARE(host.processResidentKiB, std::optional<qint64>(51234));
        QFile::setPermissions(hot, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QFile::setPermissions(meminfoPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#else
        QSKIP("file modes are a POSIX fixture");
#endif
    }

    // R-R3-40/41 (final review I2): the display load governor's CPU input
    // while thread placement is active is the housekeeping cores' share, from
    // the per-core lines. A Rock-shaped load: four housekeeping cores at
    // 100 % and four signal processing cores at 60 % average 80 %, under the
    // 85 % busy threshold, while spectrum and sending are starved.
    void watchedCoresAreMeasuredApartFromTheAggregate()
    {
        Fixture f;
        f.standardProc();
        const auto statWith = [](quint64 step) {
            // Ticks after `step` intervals of 100 per core.
            QByteArray text("cpu  ");
            const quint64 busyAll = step * (4 * 100 + 4 * 60);
            const quint64 idleAll = step * (4 * 40);
            text += QByteArray::number(busyAll) + " 0 0 " + QByteArray::number(idleAll)
                + " 0 0 0 0 0 0\n";
            for (int cpu = 0; cpu < 8; ++cpu) {
                const quint64 busy = step * (cpu < 4 ? 100 : 60);
                const quint64 idle = step * (cpu < 4 ? 0 : 40);
                text += "cpu" + QByteArray::number(cpu) + ' ' + QByteArray::number(busy)
                    + " 0 0 " + QByteArray::number(idle) + " 0 0 0 0 0 0\n";
            }
            text += "intr 12345 0 0 0\nctxt 999\n";
            return text;
        };
        f.write(QStringLiteral("proc/stat"), statWith(1));
        auto sampler = std::make_unique<HostTelemetrySampler>(f.root());
        sampler->setWatchedCpus({3, 1, 0, 2, 2}); // any order, duplicates
        QCOMPARE(sampler->watchedCpus(), (QList<int>{0, 1, 2, 3}));
        qint64 now = 0;
        SharedHostSampler shared(std::move(sampler), [&now] { return now; });
        QVERIFY(!shared.reading().systemCpuPercent.has_value());
        QVERIFY(!shared.governorCpuPercent().has_value());

        f.write(QStringLiteral("proc/stat"), statWith(2));
        now = SharedHostSampler::kMinimumIntervalMs;
        const StationHostTelemetry host = shared.reading();
        QVERIFY(host.systemCpuPercent.has_value());
        QCOMPARE(*host.systemCpuPercent, 80.0); // telemetry: every core
        QCOMPARE(shared.governorCpuPercent(), std::optional<double>(100.0));

        // What the governor does with each: the aggregate never steps down,
        // the housekeeping share does after the busy hold.
        PlacementPlan placed;
        placed.active = true;
        placed.housekeeping = {0, 1, 2, 3};
        DisplayLoadInputs in;
        in.systemCpuPercent = host.systemCpuPercent;
        in.housekeepingCpuPercent = shared.governorCpuPercent();
        const DisplayBudgetCharge pan = spectrumDisplayCost(1024, 30, false)->charge;
        const auto run = [&](const PlacementPlan& plan) {
            in.placement = plan;
            DisplayLoadGovernor governor(DisplayLoadGovernor::computedCeiling());
            int decisions = 0;
            for (qint64 t = 0; t <= 5'000; t += 500) {
                if (const auto d = governor.update(displayLoadReadingFrom(in, t, pan))) {
                    governor.accept(*d);
                    ++decisions;
                }
            }
            return decisions;
        };
        QCOMPARE(run(placed), 1);
        QCOMPARE(run(PlacementPlan{}), 0);

        // A different set restarts only the watched measurement.
        shared.setGovernorCpus({0, 1});
        QVERIFY(!shared.governorCpuPercent().has_value());
        QCOMPARE(shared.reading().systemCpuPercent, std::optional<double>(80.0));
    }

    // R-R3-40: the governor judges a settling step only on a host sample
    // begun after the step took effect, so it needs to know when the
    // sample behind the CPU values began. Reading it takes no sample.
    void theCpuSampleStartIsKnown()
    {
        Fixture f;
        f.standardProc();
        qint64 now = 0;
        SharedHostSampler shared(std::make_unique<HostTelemetrySampler>(f.root()),
                                 [&now] { return now; });
        shared.reading();
        QVERIFY(!shared.cpuSampleBeganMsAgo().has_value()); // One sample: no interval.
        now = 900;
        shared.reading();
        QCOMPARE(shared.cpuSampleBeganMsAgo(), std::optional<qint64>(900));
        now = 1'300; // The cached reading stands; its interval began at 0.
        shared.reading();
        QCOMPARE(shared.cpuSampleBeganMsAgo(), std::optional<qint64>(1'300));
        now = 1'800;
        shared.reading();
        QCOMPARE(shared.cpuSampleBeganMsAgo(), std::optional<qint64>(900));

        // On the governor's clock: the reading's start is its time less the
        // sample's age, and only where there is a CPU value.
        DisplayLoadInputs in;
        in.systemCpuPercent = 50.0;
        in.cpuSampleBeganMsAgo = 900;
        const DisplayBudgetCharge pan = spectrumDisplayCost(1024, 30, false)->charge;
        QCOMPARE(displayLoadReadingFrom(in, 5'000, pan).systemCpuSampleStartMs,
                 std::optional<qint64>(4'100));
        in.systemCpuPercent.reset();
        QVERIFY(!displayLoadReadingFrom(in, 5'000, pan).systemCpuSampleStartMs.has_value());
    }

    // R-R3-40: /proc/stat has one line per core, so a large computer's file
    // is far longer than the read buffer, and its "intr" line alone can be
    // longer still. The last cores' lines are read like the first.
    void aLargeCpuCountIsReadWhole()
    {
        constexpr int kCpus = 128;
        const auto statWith = [](quint64 step) {
            // Large counters, as on a long-running machine, make each line
            // about 90 bytes: the 128 core lines alone pass the 8 KiB read
            // buffer.
            constexpr quint64 kBase = 10'000'000'000'000'000ULL;
            const QByteArray rest = " " + QByteArray::number(kBase) + " "
                + QByteArray::number(kBase);
            QByteArray text;
            quint64 busyAll = 0;
            quint64 idleAll = 0;
            QByteArray cores;
            for (int cpu = 0; cpu < kCpus; ++cpu) {
                // The last eight cores are fully busy, the rest half busy.
                const bool top = cpu >= kCpus - 8;
                const quint64 busy = kBase + step * (top ? 100 : 50);
                const quint64 idle = kBase + step * (top ? 0 : 50);
                busyAll += busy;
                idleAll += idle;
                // user nice system idle iowait irq softirq steal guest
                // guest_nice: nice and system are constant, the rest zero.
                cores += "cpu" + QByteArray::number(cpu) + ' ' + QByteArray::number(busy)
                    + rest + ' ' + QByteArray::number(idle) + " 0 0 0 0 0 0\n";
            }
            text += "cpu  " + QByteArray::number(busyAll) + ' '
                + QByteArray::number(kBase * kCpus) + ' ' + QByteArray::number(kBase * kCpus)
                + ' ' + QByteArray::number(idleAll) + " 0 0 0 0 0 0\n";
            text += cores;
            text += "intr 123456789";
            for (int irq = 0; irq < 4'000; ++irq) {
                text += " 12345";
            }
            text += "\nctxt 999\nbtime 1700000000\n";
            return text;
        };
        Fixture f;
        f.standardProc();
        f.write(QStringLiteral("proc/stat"), statWith(1));
        QVERIFY(statWith(1).indexOf("cpu127 ") > 8192);
        QVERIFY(statWith(1).size() - statWith(1).indexOf("intr") > 8192);
        HostTelemetrySampler sampler(f.root());
        QList<int> top;
        for (int cpu = kCpus - 8; cpu < kCpus; ++cpu) {
            top.append(cpu);
        }
        sampler.setWatchedCpus(top);
        sampler.sample();
        f.write(QStringLiteral("proc/stat"), statWith(2));
        const StationHostTelemetry host = sampler.sample();
        // 120 cores at 50 % and 8 at 100 %.
        QVERIFY(host.systemCpuPercent.has_value());
        QCOMPARE(*host.systemCpuPercent, (120.0 * 50.0 + 8.0 * 100.0) / 128.0);
        QCOMPARE(sampler.watchedCpuPercent(), std::optional<double>(100.0));

        // A core past the last line (offline, or never there) is absent.
        sampler.setWatchedCpus({kCpus});
        sampler.sample();
        f.write(QStringLiteral("proc/stat"), statWith(3));
        sampler.sample();
        QVERIFY(!sampler.watchedCpuPercent().has_value());
    }

    void anOfflineWatchedCoreIsAbsent()
    {
        Fixture f;
        f.write(QStringLiteral("proc/stat"),
                "cpu  200 0 0 200 0 0 0 0 0 0\ncpu0 100 0 0 100 0 0 0 0 0 0\n"
                "cpu2 100 0 0 100 0 0 0 0 0 0\n");
        HostTelemetrySampler sampler(f.root());
        sampler.setWatchedCpus({0, 1}); // cpu1 has no line
        sampler.sample();
        f.write(QStringLiteral("proc/stat"),
                "cpu  400 0 0 200 0 0 0 0 0 0\ncpu0 200 0 0 100 0 0 0 0 0 0\n"
                "cpu2 200 0 0 100 0 0 0 0 0 0\n");
        const StationHostTelemetry host = sampler.sample();
        QVERIFY(host.systemCpuPercent.has_value());
        QVERIFY(!sampler.watchedCpuPercent().has_value());
        sampler.setWatchedCpus({0});
        sampler.sample();
        f.write(QStringLiteral("proc/stat"),
                "cpu  500 0 0 300 0 0 0 0 0 0\ncpu0 250 0 0 150 0 0 0 0 0 0\n"
                "cpu2 250 0 0 150 0 0 0 0 0 0\n");
        sampler.sample();
        QCOMPARE(sampler.watchedCpuPercent(), std::optional<double>(50.0));
    }

    void disabledSamplerMeasuresNothing()
    {
        HostTelemetrySampler disabled{QString()};
        QVERIFY(!disabled.isEnabled());
        QVERIFY(disabled.sample().isEmpty());
        QVERIFY(disabled.sample().isEmpty());
#ifdef Q_OS_LINUX
        QCOMPARE(HostTelemetrySampler::defaultRootDirectory(), QStringLiteral("/"));
#else
        // macOS and Windows Cores send the host section absent.
        QVERIFY(HostTelemetrySampler::defaultRootDirectory().isEmpty());
        QVERIFY(!HostTelemetrySampler().isEnabled());
#endif
    }
};

QTEST_GUILESS_MAIN(TstHostTelemetrySampler)
#include "tst_host_telemetry_sampler.moc"
