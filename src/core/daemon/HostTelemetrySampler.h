// =================================================================
// src/core/daemon/HostTelemetrySampler.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Reads the Core computer's CPU, memory
// and thermal counters from Linux procfs/sysfs for observational telemetry
// (R-R3-32, R-R3-33); no upstream logic is involved.
// =================================================================

#pragma once

#include "core/session/StationTelemetry.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

/// Samples host load for the Core's 1 Hz telemetry.
///
/// Every file is read relative to a root directory, which is "/" on Linux
/// and empty (sampling disabled) everywhere else. Tests inject a fixture
/// directory laid out like the real one (proc/stat, proc/meminfo,
/// proc/self/stat, proc/self/status, sys/class/thermal/thermal_zone*/).
///
/// sample() reads a handful of small files into a stack buffer and does not
/// allocate beyond the returned value; it never logs. It is meant for the
/// Core's event-loop thread, never a real-time audio or DSP thread. A missing
/// or unreadable file makes only the values it carries absent.
class HostTelemetrySampler final {
public:
    /// "/" on Linux; an empty string on macOS and Windows, where the host
    /// section is sent absent rather than faked.
    static QString defaultRootDirectory();

    explicit HostTelemetrySampler(const QString& rootDirectory = defaultRootDirectory());

    bool isEnabled() const noexcept { return m_enabled; }

    /// Forgets both CPU baselines, so the next sample reports no CPU
    /// percentages, and rediscovers the thermal zones.
    void reset();

    /// One observation. CPU percentages are the difference from the previous
    /// sample and are absent on the first sample, after any reading error
    /// and when a counter went backwards.
    StationHostTelemetry sample();

    /// Cores whose combined busy share sample() also measures, from the
    /// per-core lines of proc/stat (R-R3-40: the display load governor's
    /// housekeeping cores). A different set restarts only this measurement;
    /// the telemetry values and their baselines are untouched. Empty: none.
    void setWatchedCpus(const QList<int>& cpus);
    QList<int> watchedCpus() const { return m_watchedCpus; }
    /// Busy share of the watched cores between the last two samples, 0..100.
    /// Absent under the same rules as systemCpuPercent, and when a watched
    /// core has no line (offline) in either sample.
    std::optional<double> watchedCpuPercent() const { return m_watchedCpuPercent; }

private:
    struct ThermalZone {
        QByteArray tempPath;
        QString type;
    };

    void discoverThermalZones();

    bool m_enabled = false;
    QString m_root;
    QByteArray m_procStatPath;
    QByteArray m_procSelfStatPath;
    QByteArray m_procMeminfoPath;
    QByteArray m_procSelfStatusPath;
    QVector<ThermalZone> m_zones;

    struct SystemBaseline {
        quint64 total = 0;
        quint64 idle = 0;
    };
    struct ProcessBaseline {
        quint64 processTicks = 0;
        quint64 systemTotal = 0;
    };
    std::optional<SystemBaseline> m_systemBaseline;
    std::optional<ProcessBaseline> m_processBaseline;
    QList<int> m_watchedCpus;
    std::optional<SystemBaseline> m_watchedBaseline;
    std::optional<double> m_watchedCpuPercent;
};

/// The Core's one host sampler, shared by every reader on its event loop:
/// the 1 Hz telemetry and the display load governor (R-R3-40).
///
/// reading() returns the cached observation while it is younger than
/// kMinimumIntervalMs and otherwise takes a new one, so readers at
/// different cadences see the same reading and none of them restarts the
/// CPU interval another reader relies on. Nothing here ever resets the
/// sampler's baselines; a reader that must not report an interval started
/// before it began (telemetry at a new session) drops the percentages from
/// its own copy instead.
class SharedHostSampler final {
public:
    using MonotonicClock = std::function<qint64()>;

    /// Below the 1 Hz telemetry period, so each telemetry tick still gets a
    /// fresh reading when it is the only reader, with room for timer jitter.
    static constexpr qint64 kMinimumIntervalMs = 900;

    /// A null sampler means the platform default; a null clock means this
    /// object's own monotonic clock (milliseconds).
    explicit SharedHostSampler(std::unique_ptr<HostTelemetrySampler> sampler = {},
                               MonotonicClock clock = {});

    bool isEnabled() const noexcept { return m_sampler->isEnabled(); }

    /// The latest reading, sampled now when the cached one is too old.
    StationHostTelemetry reading();

    /// The display load governor's cores (the housekeeping cores while
    /// thread placement is active; empty otherwise). Changing them restarts
    /// only governorCpuPercent()'s interval.
    void setGovernorCpus(const QList<int>& cpus);
    /// Busy share of the governor's cores from the same sample as reading(),
    /// sampled now when the cached one is too old. Absent without cores.
    std::optional<double> governorCpuPercent();
    /// How long ago, in milliseconds, the interval behind the current CPU
    /// percentages began (the sample before the cached one), without
    /// taking a new sample. Absent before two samples exist.
    std::optional<qint64> cpuSampleBeganMsAgo() const;

private:
    std::unique_ptr<HostTelemetrySampler> m_sampler;
    MonotonicClock m_clock;
    QElapsedTimer m_ownClock;
    std::optional<qint64> m_sampledAtMs;
    std::optional<qint64> m_previousSampledAtMs;
    StationHostTelemetry m_cached;
};

} // namespace NereusSDR
