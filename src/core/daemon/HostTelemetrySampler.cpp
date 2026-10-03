// =================================================================
// src/core/daemon/HostTelemetrySampler.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See HostTelemetrySampler.h.
// =================================================================

#include "core/daemon/HostTelemetrySampler.h"

#include <QDir>
#include <QFile>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace NereusSDR {
namespace {

// Large enough for /proc/meminfo and /proc/self/status on the Rock (each
// well under 2 KiB). A value past the end of the buffer reads as absent.
// /proc/stat is read through it in chunks (readProcStat), so its per-core
// lines fit whatever the number of cores.
constexpr qsizetype kReadBufferBytes = 8192;
using ReadBuffer = std::array<char, kReadBufferBytes>;

// Below absolute zero is a driver error, not a measurement.
constexpr qint64 kMinimumMilliCelsius = -273150;

// The largest integer JSON carries exactly; a larger KiB count is not real.
constexpr quint64 kMaxExactJsonInteger = 9007199254740991ULL;

// Reads up to the buffer size from a small procfs/sysfs file. procfs reports
// a zero size, so read until end of file. Returns the byte count, or -1.
qsizetype readSmallFile(const QByteArray& path, ReadBuffer& buffer)
{
    if (path.isEmpty()) { return -1; }
#ifdef Q_OS_UNIX
    int fd = -1;
    do {
        fd = ::open(path.constData(), O_RDONLY | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) { return -1; }
    qsizetype length = 0;
    bool failed = false;
    while (length < kReadBufferBytes) {
        const ssize_t got = ::read(fd, buffer.data() + length,
                                   static_cast<size_t>(kReadBufferBytes - length));
        if (got < 0) {
            if (errno == EINTR) { continue; }
            failed = true;
            break;
        }
        if (got == 0) { break; }
        length += got;
    }
    ::close(fd);
    return failed ? -1 : length;
#else
    QFile file(QFile::decodeName(path));
    if (!file.open(QIODevice::ReadOnly)) { return -1; }
    const qint64 got = file.read(buffer.data(), kReadBufferBytes);
    return got < 0 ? -1 : static_cast<qsizetype>(got);
#endif
}

bool isSpace(char c) { return c == ' ' || c == '\t'; }

// Parses one unsigned decimal after optional blanks; advances p.
bool parseUnsigned(const char*& p, const char* end, quint64* value)
{
    while (p < end && isSpace(*p)) { ++p; }
    if (p == end || *p < '0' || *p > '9') { return false; }
    quint64 result = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        const quint64 digit = static_cast<quint64>(*p - '0');
        if (result > (std::numeric_limits<quint64>::max() - digit) / 10) { return false; }
        result = result * 10 + digit;
        ++p;
    }
    *value = result;
    return true;
}

bool parseSigned(const char*& p, const char* end, qint64* value)
{
    while (p < end && (isSpace(*p) || *p == '\n')) { ++p; }
    bool negative = false;
    if (p < end && *p == '-') { negative = true; ++p; }
    quint64 magnitude = 0;
    if (!parseUnsigned(p, end, &magnitude)
        || magnitude > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        return false;
    }
    if (p < end && *p != '\n' && !isSpace(*p)) { return false; }
    *value = negative ? -static_cast<qint64>(magnitude) : static_cast<qint64>(magnitude);
    return true;
}

// Finds "<key>" at the start of a line and parses the number after it.
bool keyedValue(const char* data, qsizetype length, const char* key, quint64* value)
{
    const qsizetype keyLength = static_cast<qsizetype>(std::strlen(key));
    const char* const end = data + length;
    const char* line = data;
    while (line < end) {
        const char* newline = static_cast<const char*>(
            std::memchr(line, '\n', static_cast<size_t>(end - line)));
        const char* const lineEnd = newline ? newline : end;
        if (lineEnd - line >= keyLength && std::memcmp(line, key, static_cast<size_t>(keyLength)) == 0) {
            const char* p = line + keyLength;
            return parseUnsigned(p, lineEnd, value);
        }
        line = newline ? newline + 1 : end;
    }
    return false;
}

struct SystemTimes {
    quint64 total = 0;
    quint64 idle = 0;
};

// The counters after a "cpu" or "cpuN" label: user nice system idle iowait
// irq softirq steal guest guest_nice, in USER_HZ ticks. guest and
// guest_nice are already counted in user and nice, so only the first eight
// make up the total. Idle time is idle plus iowait.
bool parseCpuTimes(const char* p, const char* end, SystemTimes* times)
{
    std::array<quint64, 8> fields{};
    int count = 0;
    while (count < static_cast<int>(fields.size()) && parseUnsigned(p, end, &fields[count])) {
        ++count;
    }
    if (count < 4) { return false; }
    quint64 total = 0;
    for (int i = 0; i < count; ++i) {
        if (total > std::numeric_limits<quint64>::max() - fields[i]) { return false; }
        total += fields[i];
    }
    times->total = total;
    times->idle = fields[3] + (count > 4 ? fields[4] : 0);
    return true;
}

// What readProcStat() found: the aggregate "cpu" line and the watched
// cores' "cpuN" lines summed.
struct ProcStatTimes {
    bool systemRead = false;
    SystemTimes system;
    bool watchedRead = false;
    SystemTimes watched;
};

// Reads proc/stat's cpu lines through `buffer` a chunk at a time, keeping
// any part line for the next chunk, so a computer with many cores (one
// line each) is read whole; nothing is allocated. The aggregate line must
// be the first line. The watched cores' lines are summed; the set reads as
// absent when any of them has no line (an offline core) or is unreadable.
// Reading stops at the first line after the cpu lines (the next, "intr",
// can itself be longer than the buffer). A line cut off at the end of the
// file, or one longer than the buffer, is not trusted.
ProcStatTimes readProcStat(const QByteArray& path, ReadBuffer& buffer, const QList<int>& cpus)
{
    ProcStatTimes result;
    if (path.isEmpty()) { return result; }
#ifdef Q_OS_UNIX
    int fd = -1;
    do {
        fd = ::open(path.constData(), O_RDONLY | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) { return result; }
    const auto readSome = [fd](char* into, qsizetype room) -> qsizetype {
        for (;;) {
            const ssize_t got = ::read(fd, into, static_cast<size_t>(room));
            if (got < 0 && errno == EINTR) { continue; }
            return got < 0 ? -1 : static_cast<qsizetype>(got);
        }
    };
#else
    QFile file(QFile::decodeName(path));
    if (!file.open(QIODevice::ReadOnly)) { return result; }
    const auto readSome = [&file](char* into, qsizetype room) -> qsizetype {
        const qint64 got = file.read(into, room);
        return got < 0 ? -1 : static_cast<qsizetype>(got);
    };
#endif
    bool failed = false;
    bool eof = false;
    bool firstLine = true;
    bool pastCpuLines = false;
    bool watchedBad = false;
    int found = 0;
    SystemTimes sum;
    qsizetype filled = 0;
    while (!failed && !pastCpuLines) {
        while (!eof && filled < kReadBufferBytes) {
            const qsizetype got = readSome(buffer.data() + filled, kReadBufferBytes - filled);
            if (got < 0) { failed = true; break; }
            if (got == 0) { eof = true; break; }
            filled += got;
        }
        if (failed) { break; }
        const char* const begin = buffer.data();
        const char* const end = begin + filled;
        const char* line = begin;
        while (line < end) {
            const char* newline = static_cast<const char*>(
                std::memchr(line, '\n', static_cast<size_t>(end - line)));
            if (!newline) {
                // A part line: once it plainly is not a cpu line, the cpu
                // lines are over, however long it runs.
                pastCpuLines = end - line >= 4
                    && !(std::memcmp(line, "cpu", 3) == 0
                         && (line[3] == ' ' || (line[3] >= '0' && line[3] <= '9')));
                break;
            }
            if (firstLine) {
                firstLine = false;
                result.systemRead = newline - line >= 4 && std::memcmp(line, "cpu ", 4) == 0
                    && parseCpuTimes(line + 4, newline, &result.system);
            } else if (newline - line > 3 && std::memcmp(line, "cpu", 3) == 0
                       && line[3] >= '0' && line[3] <= '9') {
                const char* p = line + 3;
                quint64 number = 0;
                if (parseUnsigned(p, newline, &number)
                    && number <= quint64(std::numeric_limits<int>::max())
                    && cpus.contains(static_cast<int>(number))) {
                    SystemTimes core;
                    if (!parseCpuTimes(p, newline, &core)
                        || sum.total > std::numeric_limits<quint64>::max() - core.total) {
                        watchedBad = true;
                    } else {
                        sum.total += core.total;
                        sum.idle += core.idle;
                        ++found;
                    }
                }
            } else {
                pastCpuLines = true;
                break;
            }
            line = newline + 1;
        }
        if (pastCpuLines) { break; }
        const qsizetype consumed = line - begin;
        if (consumed == 0) {
            // No whole line: the end of the file after a cut-off line, or a
            // cpu line longer than the buffer.
            failed = !eof;
            break;
        }
        std::memmove(buffer.data(), line, static_cast<size_t>(filled - consumed));
        filled -= consumed;
    }
#ifdef Q_OS_UNIX
    ::close(fd);
#endif
    if (failed) {
        return ProcStatTimes{};
    }
    result.watchedRead = !cpus.isEmpty() && !watchedBad && found == cpus.size();
    if (result.watchedRead) {
        result.watched = sum;
    }
    return result;
}

// /proc/self/stat: the command name sits in parentheses and may itself hold
// spaces or parentheses, so fields are counted after the LAST ')'. The
// first field after it is field 3 (state); utime and stime are fields 14
// and 15.
bool readProcessTicks(const QByteArray& path, ReadBuffer& buffer, quint64* ticks)
{
    const qsizetype length = readSmallFile(path, buffer);
    if (length <= 0) { return false; }
    const char* const begin = buffer.data();
    const char* close = nullptr;
    for (const char* q = begin + length; q > begin; --q) {
        if (*(q - 1) == ')') { close = q - 1; break; }
    }
    if (!close) { return false; }
    const char* p = close + 1;
    const char* const end = begin + length;
    for (int field = 3; field < 14; ++field) {
        while (p < end && isSpace(*p)) { ++p; }
        if (p == end) { return false; }
        while (p < end && !isSpace(*p) && *p != '\n') { ++p; }
    }
    quint64 utime = 0;
    quint64 stime = 0;
    if (!parseUnsigned(p, end, &utime) || !parseUnsigned(p, end, &stime)
        || utime > std::numeric_limits<quint64>::max() - stime) {
        return false;
    }
    *ticks = utime + stime;
    return true;
}

std::optional<qint64> kibValue(const char* data, qsizetype length, const char* key)
{
    quint64 value = 0;
    if (!keyedValue(data, length, key, &value)
        || value > kMaxExactJsonInteger) {
        return std::nullopt;
    }
    return static_cast<qint64>(value);
}

double percent(quint64 part, quint64 whole)
{
    return std::clamp(static_cast<double>(part) * 100.0 / static_cast<double>(whole),
                      0.0, 100.0);
}

int zoneNumber(const QString& name)
{
    bool ok = false;
    const int number = name.mid(qsizetype(std::strlen("thermal_zone"))).toInt(&ok);
    return ok ? number : std::numeric_limits<int>::max();
}

} // namespace

QString HostTelemetrySampler::defaultRootDirectory()
{
#ifdef Q_OS_LINUX
    return QStringLiteral("/");
#else
    return {};
#endif
}

HostTelemetrySampler::HostTelemetrySampler(const QString& rootDirectory)
    : m_enabled(!rootDirectory.isEmpty())
    , m_root(rootDirectory)
{
    if (!m_enabled) { return; }
    const QDir root(m_root);
    m_procStatPath = QFile::encodeName(root.filePath(QStringLiteral("proc/stat")));
    m_procSelfStatPath = QFile::encodeName(root.filePath(QStringLiteral("proc/self/stat")));
    m_procMeminfoPath = QFile::encodeName(root.filePath(QStringLiteral("proc/meminfo")));
    m_procSelfStatusPath = QFile::encodeName(root.filePath(QStringLiteral("proc/self/status")));
    reset();
}

void HostTelemetrySampler::reset()
{
    m_systemBaseline.reset();
    m_processBaseline.reset();
    if (m_enabled) {
        discoverThermalZones();
    }
}

void HostTelemetrySampler::setWatchedCpus(const QList<int>& cpus)
{
    QList<int> sorted = cpus;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    if (sorted == m_watchedCpus) { return; }
    m_watchedCpus = sorted;
    m_watchedBaseline.reset();
    m_watchedCpuPercent.reset();
}

void HostTelemetrySampler::discoverThermalZones()
{
    m_zones.clear();
    const QDir thermal(QDir(m_root).filePath(QStringLiteral("sys/class/thermal")));
    QStringList names = thermal.entryList({QStringLiteral("thermal_zone*")},
                                          QDir::Dirs | QDir::NoDotAndDotDot);
    // Numeric order, so a tie between zones names the lowest-numbered one.
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) {
        return zoneNumber(a) < zoneNumber(b);
    });
    ReadBuffer buffer;
    for (const QString& name : std::as_const(names)) {
        const QDir zone(thermal.filePath(name));
        ThermalZone entry;
        entry.tempPath = QFile::encodeName(zone.filePath(QStringLiteral("temp")));
        const qsizetype length = readSmallFile(
            QFile::encodeName(zone.filePath(QStringLiteral("type"))), buffer);
        if (length > 0) {
            entry.type = QString::fromUtf8(buffer.data(), length).trimmed()
                             .left(kMaxHostZoneNameLength);
        }
        m_zones.append(entry);
    }
}

StationHostTelemetry HostTelemetrySampler::sample()
{
    StationHostTelemetry host;
    if (!m_enabled) { return host; }
    ReadBuffer buffer;

    const ProcStatTimes stat = readProcStat(m_procStatPath, buffer, m_watchedCpus);
    const SystemTimes& system = stat.system;
    const SystemTimes& watched = stat.watched;
    const bool systemRead = stat.systemRead;
    const bool watchedRead = stat.watchedRead;
    m_watchedCpuPercent.reset();
    if (watchedRead && m_watchedBaseline && watched.total > m_watchedBaseline->total
        && watched.idle >= m_watchedBaseline->idle
        && watched.idle - m_watchedBaseline->idle <= watched.total - m_watchedBaseline->total) {
        m_watchedCpuPercent = percent(
            (watched.total - m_watchedBaseline->total) - (watched.idle - m_watchedBaseline->idle),
            watched.total - m_watchedBaseline->total);
    }
    if (watchedRead) {
        m_watchedBaseline = SystemBaseline{watched.total, watched.idle};
    } else {
        m_watchedBaseline.reset();
    }
    quint64 processTicks = 0;
    const bool processRead = readProcessTicks(m_procSelfStatPath, buffer, &processTicks);

    if (systemRead && m_systemBaseline && system.total > m_systemBaseline->total
        && system.idle >= m_systemBaseline->idle) {
        const quint64 total = system.total - m_systemBaseline->total;
        const quint64 idle = system.idle - m_systemBaseline->idle;
        if (idle <= total) {
            host.systemCpuPercent = percent(total - idle, total);
        }
    }
    if (systemRead && processRead && m_processBaseline
        && system.total > m_processBaseline->systemTotal
        && processTicks >= m_processBaseline->processTicks) {
        host.processCpuPercent = percent(processTicks - m_processBaseline->processTicks,
                                         system.total - m_processBaseline->systemTotal);
    }
    // A reading error forgets the baseline; a counter that went backwards
    // restarts it from the new value. Either way nothing negative is sent.
    if (systemRead) {
        m_systemBaseline = SystemBaseline{system.total, system.idle};
    } else {
        m_systemBaseline.reset();
    }
    if (systemRead && processRead) {
        m_processBaseline = ProcessBaseline{processTicks, system.total};
    } else {
        m_processBaseline.reset();
    }

    qsizetype length = readSmallFile(m_procMeminfoPath, buffer);
    if (length > 0) {
        host.memoryTotalKiB = kibValue(buffer.data(), length, "MemTotal:");
        host.memoryAvailableKiB = kibValue(buffer.data(), length, "MemAvailable:");
    }
    length = readSmallFile(m_procSelfStatusPath, buffer);
    if (length > 0) {
        host.processResidentKiB = kibValue(buffer.data(), length, "VmRSS:");
    }

    // Values are millidegrees Celsius. Some zones refuse reads (a sensor
    // that is powered down returns an error); those are skipped.
    std::optional<qint64> hottest;
    const ThermalZone* hottestZone = nullptr;
    for (const ThermalZone& zone : std::as_const(m_zones)) {
        length = readSmallFile(zone.tempPath, buffer);
        if (length <= 0) { continue; }
        const char* p = buffer.data();
        qint64 milli = 0;
        if (!parseSigned(p, buffer.data() + length, &milli) || milli < kMinimumMilliCelsius) {
            continue;
        }
        if (!hottest || milli > *hottest) {
            hottest = milli;
            hottestZone = &zone;
        }
    }
    if (hottest) {
        host.hottestZoneCelsius = static_cast<double>(*hottest) / 1000.0;
        host.hottestZoneName = hottestZone->type;
    }
    return host;
}

SharedHostSampler::SharedHostSampler(std::unique_ptr<HostTelemetrySampler> sampler,
                                     MonotonicClock clock)
    : m_sampler(std::move(sampler))
    , m_clock(std::move(clock))
{
    if (!m_sampler) {
        m_sampler = std::make_unique<HostTelemetrySampler>();
    }
    m_ownClock.start();
    if (!m_clock) {
        m_clock = [this] { return m_ownClock.elapsed(); };
    }
}

void SharedHostSampler::setGovernorCpus(const QList<int>& cpus)
{
    m_sampler->setWatchedCpus(cpus);
}

std::optional<double> SharedHostSampler::governorCpuPercent()
{
    reading();
    return m_sampler->watchedCpuPercent();
}

std::optional<qint64> SharedHostSampler::cpuSampleBeganMsAgo() const
{
    if (!m_previousSampledAtMs) {
        return std::nullopt;
    }
    return std::max<qint64>(0, m_clock() - *m_previousSampledAtMs);
}

StationHostTelemetry SharedHostSampler::reading()
{
    const qint64 nowMs = m_clock();
    if (!m_sampledAtMs || nowMs < *m_sampledAtMs
        || nowMs - *m_sampledAtMs >= kMinimumIntervalMs) {
        m_cached = m_sampler->sample();
        m_previousSampledAtMs = m_sampledAtMs;
        m_sampledAtMs = nowMs;
    }
    return m_cached;
}

} // namespace NereusSDR
