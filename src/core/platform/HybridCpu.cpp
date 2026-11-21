// src/core/platform/HybridCpu.cpp  (NereusSDR)
// =================================================================
// Ported from Thetis: Console/HybridCpu.cs (v1.3.2).
//
// Windows' GetSystemCpuSetInformation() returns one record per logical CPU
// with an EfficiencyClass. Higher classes are the more performant cores, so
// the highest class is the P-core set. A CPU without a P/E split has one
// class and needs no restriction.
//
// The structure layout is read directly from the returned buffer, as Thetis
// does, so this compiles against Windows SDKs that do not declare
// SYSTEM_CPU_SET_INFORMATION. The API is resolved at runtime from kernel32.
// =================================================================
#include "core/platform/HybridCpu.h"

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace NereusSDR {

namespace {

#ifdef Q_OS_WIN
using GetSystemCpuSetInformationFn =
    BOOL (WINAPI*)(PVOID, ULONG, PULONG, HANDLE, ULONG);

// Kept so "uncheck" restores the mask the process had before the option was
// applied (which may already have been narrowed by a job object or Task
// Manager). Zero means nothing has been applied this run.
DWORD_PTR g_originalProcessMask = 0;

GetSystemCpuSetInformationFn cpuSetQuery()
{
    static const auto fn = reinterpret_cast<GetSystemCpuSetInformationFn>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                       "GetSystemCpuSetInformation"));
    return fn;
}
#endif

} // namespace

HybridCpuTopology detectHybridCpu()
{
    HybridCpuTopology info;

#ifdef Q_OS_WIN
    const auto query = cpuSetQuery();
    if (query == nullptr) {
        info.description = QStringLiteral(
            "CPU set API is unavailable on this Windows version.");
        return info;
    }

    ULONG length = 0;
    query(nullptr, 0, &length, nullptr, 0);
    if (length == 0) {
        info.description = QStringLiteral(
            "Windows did not return CPU set information.");
        return info;
    }

    std::vector<unsigned char> buffer(length);
    ULONG returned = 0;
    if (!query(buffer.data(), length, &returned, nullptr, 0)) {
        info.description = QStringLiteral("CPU set query failed (error %1).")
                               .arg(GetLastError());
        return info;
    }

    struct CpuSet {
        USHORT group;
        UCHAR logical;
        UCHAR efficiency;
    };
    std::vector<CpuSet> sets;

    ULONG offset = 0;
    while (offset + 32 <= returned && offset + 32 <= length) {
        ULONG size = 0;
        std::memcpy(&size, buffer.data() + offset, sizeof(size));
        if (size < 32 || offset + size > returned) {
            break;
        }
        ULONG type = 0;
        std::memcpy(&type, buffer.data() + offset + 4, sizeof(type));
        if (type == 0) {  // CpuSet
            CpuSet set{};
            std::memcpy(&set.group, buffer.data() + offset + 12,
                        sizeof(set.group));
            set.logical = buffer[offset + 14];
            set.efficiency = buffer[offset + 18];
            sets.push_back(set);
        }
        offset += size;
    }

    if (sets.empty()) {
        info.description = QStringLiteral("Windows reported no CPU sets.");
        return info;
    }

    info.totalCount = static_cast<int>(sets.size());

    int minClass = 255;
    int maxClass = 0;
    for (const CpuSet& set : sets) {
        minClass = std::min(minClass, static_cast<int>(set.efficiency));
        maxClass = std::max(maxClass, static_cast<int>(set.efficiency));
    }

    int performanceCount = 0;
    for (const CpuSet& set : sets) {
        if (set.efficiency == maxClass) {
            ++performanceCount;
        }
    }

    info.performanceCount = performanceCount;
    info.efficiencyCount = info.totalCount - performanceCount;
    info.isHybrid = maxClass != minClass && performanceCount > 0
        && performanceCount < info.totalCount;

    if (!info.isHybrid) {
        info.description = QStringLiteral(
            "This CPU has no P/E core split; all cores are equal.");
        info.supported = true;  // no restriction needed
        return info;
    }

    // A process affinity mask addresses one processor group and at most 64
    // logical CPUs. Bail out safely on larger systems, as Thetis does.
    const USHORT group = sets.front().group;
    bool oneGroup = true;
    for (const CpuSet& set : sets) {
        if (set.group != group) {
            oneGroup = false;
            break;
        }
    }
    if (!oneGroup || sets.size() > 64) {
        info.description = QStringLiteral(
            "CPU topology spans multiple processor groups; affinity "
            "restriction is not applied.");
        return info;
    }

    quint64 all = 0;
    quint64 performance = 0;
    for (const CpuSet& set : sets) {
        const quint64 bit = quint64(1) << set.logical;
        all |= bit;
        if (set.efficiency == maxClass) {
            performance |= bit;
        }
    }

    info.allMask = all;
    info.performanceMask = performance;
    info.supported = true;
    info.description = QStringLiteral(
        "Detected %1 performance cores and %2 efficiency cores.")
        .arg(performanceCount)
        .arg(info.efficiencyCount);
#endif

    return info;
}

bool applyPerformanceCoresOnly(QString* message)
{
    const HybridCpuTopology info = detectHybridCpu();
    if (message != nullptr) {
        *message = info.description;
    }
    if (!info.supported || !info.isHybrid || info.performanceMask == 0) {
        return false;
    }

#ifdef Q_OS_WIN
    DWORD_PTR processMask = 0;
    DWORD_PTR systemMask = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &processMask,
                                &systemMask)) {
        if (message != nullptr) {
            *message = QStringLiteral(
                "Could not read the current process affinity (error %1).")
                .arg(GetLastError());
        }
        return false;
    }

    const quint64 allowed = static_cast<quint64>(processMask);
    const quint64 restricted = info.performanceMask & allowed;
    if (restricted == 0) {
        if (message != nullptr) {
            *message = QStringLiteral(
                "This process is already restricted away from the P-cores.");
        }
        return false;
    }
    if ((allowed & ~info.performanceMask) == 0) {
        // Already inside the performance cores.
        if (message != nullptr) {
            *message = info.description + QStringLiteral(
                " Nereus is already restricted to the performance cores.");
        }
        return true;
    }

    if (g_originalProcessMask == 0) {
        g_originalProcessMask = processMask;
    }
    if (!SetProcessAffinityMask(GetCurrentProcess(),
                                static_cast<DWORD_PTR>(restricted))) {
        if (message != nullptr) {
            *message = QStringLiteral(
                "Could not set the process affinity (error %1).")
                .arg(GetLastError());
        }
        return false;
    }

    if (message != nullptr) {
        *message = info.description + QStringLiteral(
            " Nereus is restricted to the performance cores.");
    }
    return true;
#else
    return false;
#endif
}

void restoreAllCores()
{
#ifdef Q_OS_WIN
    if (g_originalProcessMask != 0) {
        SetProcessAffinityMask(GetCurrentProcess(), g_originalProcessMask);
        g_originalProcessMask = 0;
        return;
    }
    // No recorded original: fall back to every CPU the topology reports.
    const HybridCpuTopology info = detectHybridCpu();
    if (info.supported && info.allMask != 0) {
        SetProcessAffinityMask(GetCurrentProcess(),
                               static_cast<DWORD_PTR>(info.allMask));
    }
#endif
}

} // namespace NereusSDR
