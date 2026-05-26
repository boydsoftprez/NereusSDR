// =================================================================
// src/core/MemoryLock.cpp  (NereusSDR-native)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// See MemoryLock.h for design rationale.
// =================================================================
#include "MemoryLock.h"

#include <QLoggingCategory>

#include <atomic>
#include <cstdint>
#include <cstring>

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
    const AlignedRange r = alignRange(addr, bytes);

#ifdef Q_OS_WIN
    if (VirtualUnlock(reinterpret_cast<LPVOID>(r.startPage), r.bytesAligned)
            == 0) {
        // VirtualUnlock returns 0 also when the region wasn't locked;
        // not actionable.  Don't log to avoid noise on benign double-unlock.
        return;
    }
#else
    if (munlock(reinterpret_cast<void*>(r.startPage), r.bytesAligned) != 0) {
        // munlock errors are benign (region already unlocked / process
        // teardown); not actionable for the caller.
        return;
    }
#endif

    s_bytesLocked.fetch_sub(r.bytesAligned, std::memory_order_relaxed);
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
