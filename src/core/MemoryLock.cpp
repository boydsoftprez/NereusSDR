// =================================================================
// src/core/MemoryLock.cpp  (NereusSDR-native)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// See MemoryLock.h for design rationale.
// =================================================================
#include "MemoryLock.h"

#include <QLoggingCategory>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <memoryapi.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

Q_LOGGING_CATEGORY(lcMemLock, "nereus.memlock")

namespace NereusSDR {

namespace {

std::atomic<std::size_t> s_bytesLocked{0};
std::atomic<int>         s_regionsLocked{0};
std::atomic<int>         s_lockFailuresTotal{0};

std::size_t pageSize()
{
#ifdef Q_OS_WIN
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return static_cast<std::size_t>(si.dwPageSize);
#else
    static const std::size_t kPageSize =
        static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    return kPageSize;
#endif
}

// Round addr down + length up to page boundaries.  Returns the
// aligned start address (as uintptr_t) and the aligned byte count.
struct AlignedRange {
    std::uintptr_t startPage;
    std::size_t    bytesAligned;
};

AlignedRange alignRange(const void* addr, std::size_t bytes)
{
    const std::size_t pg = pageSize();
    const std::uintptr_t a = reinterpret_cast<std::uintptr_t>(addr);
    const std::uintptr_t start = a & ~(static_cast<std::uintptr_t>(pg - 1));
    const std::uintptr_t end =
        (a + bytes + pg - 1) & ~(static_cast<std::uintptr_t>(pg - 1));
    return { start, static_cast<std::size_t>(end - start) };
}

// Registry of ranges we actually hold a kernel lock on.
//
// Callers unlock unconditionally -- FFTEngine and SpectrumWidget both
// pair a discarded-return lockMemory() with an unconditional
// unlockMemory() in their teardown / realloc paths, and PortAudioBus
// does the same in its destructor.  When the lock never succeeded
// (routine on Linux once RLIMIT_MEMLOCK is exhausted, and on any
// unprivileged host) POSIX munlock() still returns 0 for a mapped
// range, so subtracting on every unlock underflowed s_bytesLocked and
// drove s_regionsLocked negative -- the perf overlay then rendered
// nonsense lock totals.  Codex review, PR #291.
//
// Only successfully-locked ranges land here, so the counters can only
// move in matched pairs.  Guarded by a plain mutex: every call site is
// a setup / teardown / realloc path on the GUI, FFT or connection
// thread.  Nothing calls this from the audio callback, and the vector
// holds a handful of entries (FFT scratch x3, waterfall, two overlay
// textures, audio ring).
std::mutex               s_registryMutex;
std::vector<AlignedRange> s_lockedRanges;

} // namespace

bool lockMemory(const void* addr, std::size_t bytes, const char* tag)
{
    if (addr == nullptr || bytes == 0) {
        return false;
    }
    const AlignedRange r = alignRange(addr, bytes);

#ifdef Q_OS_WIN
    if (VirtualLock(reinterpret_cast<LPVOID>(r.startPage), r.bytesAligned)
            == 0) {
        const DWORD err = GetLastError();
        qCWarning(lcMemLock) << "VirtualLock failed for tag" << (tag ? tag : "")
                             << "bytes" << r.bytesAligned
                             << "GetLastError" << err
                             << "-- region remains pageable.";
        s_lockFailuresTotal.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
#else
    if (mlock(reinterpret_cast<void*>(r.startPage), r.bytesAligned) != 0) {
        const int e = errno;
        qCWarning(lcMemLock) << "mlock failed for tag" << (tag ? tag : "")
                             << "bytes" << r.bytesAligned
                             << "errno" << e << strerror(e)
                             << "-- region remains pageable.";
        s_lockFailuresTotal.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
#endif

    {
        const std::lock_guard<std::mutex> guard(s_registryMutex);
        s_lockedRanges.push_back(r);
    }
    s_bytesLocked.fetch_add(r.bytesAligned, std::memory_order_relaxed);
    s_regionsLocked.fetch_add(1, std::memory_order_relaxed);
    qCInfo(lcMemLock) << "locked" << r.bytesAligned << "bytes for tag"
                      << (tag ? tag : "(unnamed)");
    return true;
}

void unlockMemory(const void* addr, std::size_t bytes)
{
    if (addr == nullptr || bytes == 0) {
        return;
    }
    const AlignedRange asked = alignRange(addr, bytes);

    // Claim the matching registry entry first.  If we are not holding a
    // lock on this range there is nothing to release and nothing to
    // subtract -- returning here is what keeps the counters from
    // underflowing on the unconditional-unlock call sites.
    AlignedRange held{};
    {
        const std::lock_guard<std::mutex> guard(s_registryMutex);
        auto it = std::find_if(
            s_lockedRanges.begin(), s_lockedRanges.end(),
            [&asked](const AlignedRange& e) {
                return e.startPage == asked.startPage
                       && e.bytesAligned == asked.bytesAligned;
            });
        if (it == s_lockedRanges.end()) {
            // Same start page, different length: the caller resized the
            // buffer and is unlocking with the NEW byte count.  Release
            // what we actually locked so the kernel lock does not leak.
            it = std::find_if(s_lockedRanges.begin(), s_lockedRanges.end(),
                              [&asked](const AlignedRange& e) {
                                  return e.startPage == asked.startPage;
                              });
        }
        if (it == s_lockedRanges.end()) {
            return;
        }
        held = *it;
        s_lockedRanges.erase(it);
    }

#ifdef Q_OS_WIN
    // VirtualUnlock's return value is not actionable here -- we already
    // know from the registry that we held this range, and a failure at
    // process teardown is benign.  Don't log; it would be noise.
    VirtualUnlock(reinterpret_cast<LPVOID>(held.startPage),
                  held.bytesAligned);
#else
    // munlock errors are benign (region already unlocked by the kernel
    // during teardown); not actionable for the caller.
    munlock(reinterpret_cast<void*>(held.startPage), held.bytesAligned);
#endif

    s_bytesLocked.fetch_sub(held.bytesAligned, std::memory_order_relaxed);
    s_regionsLocked.fetch_sub(1, std::memory_order_relaxed);
}

MemoryLockStats memoryLockStats()
{
    MemoryLockStats s;
    s.bytesLocked       = s_bytesLocked.load(std::memory_order_relaxed);
    s.regionsLocked     = s_regionsLocked.load(std::memory_order_relaxed);
    s.lockFailuresTotal = s_lockFailuresTotal.load(std::memory_order_relaxed);
    return s;
}

} // namespace NereusSDR
