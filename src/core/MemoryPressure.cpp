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

    // Host-level compression activity via HOST_VM_INFO64.  The
    // "compressions" + "decompressions" counters monotonically tick
    // whenever the kernel pages something through the compressor.  A
    // non-zero delta over our sample window means the OS is under
    // memory pressure and is paging *something* somewhere on the
    // system (not necessarily our process).  Even other processes
    // paging hurts us because it contends for memory bandwidth.
    static natural_t s_prevCompressions   = 0;
    static natural_t s_prevDecompressions = 0;
    static bool      s_primed             = false;

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t vmCount = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(),
                          HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vmStats),
                          &vmCount) == KERN_SUCCESS) {
        if (!s_primed) {
            s_primed = true;
            s_prevCompressions   = vmStats.compressions;
            s_prevDecompressions = vmStats.decompressions;
            // First call has no baseline; report not-compressing.
            sample.compressing = false;
        } else {
            const natural_t dCompress =
                vmStats.compressions   - s_prevCompressions;
            const natural_t dDecompress =
                vmStats.decompressions - s_prevDecompressions;
            sample.compressing = (dCompress + dDecompress) > 0;
            s_prevCompressions   = vmStats.compressions;
            s_prevDecompressions = vmStats.decompressions;
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
