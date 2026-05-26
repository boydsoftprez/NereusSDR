// =================================================================
// src/core/MemoryPressure.cpp  (NereusSDR-native)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// See MemoryPressure.h for design rationale.
// =================================================================
#include "MemoryPressure.h"

#include <QtGlobal>

#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <sys/sysctl.h>
#endif

#ifdef Q_OS_LINUX
#include <fstream>
#include <sstream>
#include <string>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace NereusSDR {

#ifdef Q_OS_MAC

MemoryPressureSample pollMemoryPressure()
{
    MemoryPressureSample sample;

    // Process RSS / phys_footprint via task_info(MACH_TASK_BASIC_INFO).
    // phys_footprint is the post-compression-aware "memory footprint"
    // that matches Activity Monitor's "Memory" column.
    mach_task_basic_info_data_t taskInfo;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&taskInfo), &count)
            == KERN_SUCCESS) {
        sample.footprintMb = static_cast<double>(taskInfo.resident_size)
                              / (1024.0 * 1024.0);
    }

    // 2026-05-26 KG4VCF: Primary signal is the OS-classified memory
    // pressure level (kern.memorystatus_vm_pressure_level sysctl).
    // Values per <kern_memorystatus.h>:
    //   1 = NORMAL  (steady-state, no app should change behaviour)
    //   2 = WARN    (kernel signalling apps to free memory; back off
    //                optional work here)
    //   4 = CRITICAL (kernel killing low-priority processes; user
    //                is in active pain)
    //
    // Earlier revisions of this function used a delta-of-
    // (compressions + decompressions) heuristic that flagged
    // "compressing" on any non-zero activity in the last sample
    // window.  Bench finding 2026-05-26: that heuristic fires
    // continuously even at idle because modern macOS treats
    // compressed memory as a *tier*, not an emergency response.
    // The compressor runs constantly to manage the memory hierarchy
    // regardless of whether the system has any actual pressure.
    // Using the kernel's own classification gives a much cleaner
    // signal: only flag when the OS itself is signalling distress.
    int pressureLevel = 1;  // normal default if sysctl unavailable
    size_t plSize = sizeof(pressureLevel);
    if (sysctlbyname("kern.memorystatus_vm_pressure_level",
                     &pressureLevel, &plSize, nullptr, 0) == 0) {
        sample.compressing = (pressureLevel >= 2);
    } else {
        // Sysctl missing (e.g. some sandbox configurations).  Fall
        // back to a conservative compression-delta heuristic with a
        // high threshold so background tiering doesn't trip it.
        static natural_t s_prevCompressions = 0;
        static bool      s_primed           = false;
        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t vmCount = HOST_VM_INFO64_COUNT;
        if (host_statistics64(mach_host_self(),
                              HOST_VM_INFO64,
                              reinterpret_cast<host_info64_t>(&vmStats),
                              &vmCount) == KERN_SUCCESS) {
            if (!s_primed) {
                s_primed = true;
                s_prevCompressions = vmStats.compressions;
                sample.compressing = false;
            } else {
                const natural_t dCompress =
                    vmStats.compressions - s_prevCompressions;
                // > 500 compressions/sec = real pressure, not idle
                // tiering (idle macOS typically shows < 50/sec).
                sample.compressing = dCompress > 500;
                s_prevCompressions = vmStats.compressions;
            }
        }
    }

    return sample;
}

#elif defined(Q_OS_LINUX)

MemoryPressureSample pollMemoryPressure()
{
    MemoryPressureSample sample;

    // RSS from /proc/self/statm (second field, in pages).
    {
        std::ifstream f("/proc/self/statm");
        long sizePages = 0;
        long rssPages = 0;
        f >> sizePages >> rssPages;
        const long pageSize = sysconf(_SC_PAGE_SIZE);
        sample.footprintMb = static_cast<double>(rssPages)
                              * static_cast<double>(pageSize)
                              / (1024.0 * 1024.0);
    }

    // Swap-out delta from /proc/vmstat ("pswpout").  Linux's
    // closest analogue to macOS's compressor activity.
    static long s_prevSwapOut = 0;
    static bool s_primed      = false;

    long swapOut = 0;
    {
        std::ifstream f("/proc/vmstat");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("pswpout ", 0) == 0) {
                std::istringstream iss(line.substr(8));
                iss >> swapOut;
                break;
            }
        }
    }
    if (!s_primed) {
        s_primed = true;
        s_prevSwapOut = swapOut;
        sample.compressing = false;
    } else {
        sample.compressing = (swapOut - s_prevSwapOut) > 0;
        s_prevSwapOut = swapOut;
    }

    return sample;
}

#elif defined(Q_OS_WIN)

MemoryPressureSample pollMemoryPressure()
{
    MemoryPressureSample sample;

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        sample.footprintMb = static_cast<double>(pmc.WorkingSetSize)
                              / (1024.0 * 1024.0);
    }

    // Pagefile use delta as a coarse pressure proxy.  Windows has
    // QueryMemoryResourceNotification for proper notifications, but
    // a pagefile delta poll is good enough for the perf overlay's
    // 1 Hz cadence.
    static SIZE_T s_prevPagefile = 0;
    static bool   s_primed       = false;
    if (!s_primed) {
        s_primed = true;
        s_prevPagefile = pmc.PagefileUsage;
        sample.compressing = false;
    } else {
        sample.compressing = pmc.PagefileUsage > s_prevPagefile;
        s_prevPagefile = pmc.PagefileUsage;
    }

    return sample;
}

#else

MemoryPressureSample pollMemoryPressure()
{
    return {};
}

#endif

} // namespace NereusSDR
