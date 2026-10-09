// =================================================================
// tests/tst_device_rate_matcher_threads.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original real-time test for the
// already-attributed DeviceRateMatcher (R-AUD-15, V-SW-5).  A writer thread
// and a reader thread run the matcher on the wall clock at a +-200 ppm
// offset.  The reader thread plays the device callback: the test counts
// every global operator new and delete it makes (replaced below, for this
// binary only) and, on Linux, every malloc, calloc, realloc, free,
// pthread_mutex_lock and pthread_mutex_trylock (interposed with
// dlsym(RTLD_NEXT, ...)), and checks the frames it reads are whole.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 2 (R-AUD-15, V-SW-5). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: early-review fix wave (R-AUD-15): the reads check, which
//               counted due times and could not fail, checks the ring's
//               read counters instead.  J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "RealtimeTestLoad.h"

#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

#if defined(Q_OS_LINUX)
#include <dlfcn.h>
#include <pthread.h>
#endif

namespace {

// The watched thread: set by that thread, then g_watching published.
std::atomic<bool> g_watching{false};
std::thread::id g_watchedId;
std::atomic<std::uint64_t> g_newCalls{0};
std::atomic<std::uint64_t> g_deleteCalls{0};

bool onWatchedThread()
{
    return g_watching.load(std::memory_order_acquire) && std::this_thread::get_id() == g_watchedId;
}

void* countedAllocate(std::size_t size)
{
    if (onWatchedThread()) {
        g_newCalls.fetch_add(1, std::memory_order_relaxed);
    }
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr) {
        std::abort();   // a test binary out of memory: stop here
    }
    return p;
}

void* countedAllocateAligned(std::size_t size, std::align_val_t alignment)
{
    if (onWatchedThread()) {
        g_newCalls.fetch_add(1, std::memory_order_relaxed);
    }
    const std::size_t align = std::max(static_cast<std::size_t>(alignment), sizeof(void*));
    void* p = nullptr;
#if defined(Q_OS_WIN)
    p = _aligned_malloc(size == 0 ? 1 : size, align);
#else
    if (posix_memalign(&p, align, size == 0 ? 1 : size) != 0) {
        p = nullptr;
    }
#endif
    if (p == nullptr) {
        std::abort();
    }
    return p;
}

void countedFree(void* p)
{
    if (p != nullptr && onWatchedThread()) {
        g_deleteCalls.fetch_add(1, std::memory_order_relaxed);
    }
    std::free(p);
}

void countedFreeAligned(void* p)
{
    if (p != nullptr && onWatchedThread()) {
        g_deleteCalls.fetch_add(1, std::memory_order_relaxed);
    }
#if defined(Q_OS_WIN)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

} // namespace

// Replaced for this test binary; every image in the process calls these.
void* operator new(std::size_t size) { return countedAllocate(size); }
void* operator new[](std::size_t size) { return countedAllocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return countedAllocate(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return countedAllocate(size); }
void* operator new(std::size_t size, std::align_val_t a) { return countedAllocateAligned(size, a); }
void* operator new[](std::size_t size, std::align_val_t a) { return countedAllocateAligned(size, a); }
void* operator new(std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept
{
    return countedAllocateAligned(size, a);
}
void* operator new[](std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept
{
    return countedAllocateAligned(size, a);
}
void operator delete(void* p) noexcept { countedFree(p); }
void operator delete[](void* p) noexcept { countedFree(p); }
void operator delete(void* p, std::size_t) noexcept { countedFree(p); }
void operator delete[](void* p, std::size_t) noexcept { countedFree(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { countedFree(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { countedFree(p); }
void operator delete(void* p, std::align_val_t) noexcept { countedFreeAligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { countedFreeAligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { countedFreeAligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { countedFreeAligned(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { countedFreeAligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { countedFreeAligned(p); }

#if defined(Q_OS_LINUX)
// Interposed C allocator and mutex entry points.  dlsym may itself
// allocate while the real ones are being found; those few bytes come from
// a static buffer and are never freed.
namespace {

using MallocFn = void* (*)(std::size_t);
using CallocFn = void* (*)(std::size_t, std::size_t);
using ReallocFn = void* (*)(void*, std::size_t);
using FreeFn = void (*)(void*);
using MutexFn = int (*)(pthread_mutex_t*);

MallocFn g_realMalloc = nullptr;
CallocFn g_realCalloc = nullptr;
ReallocFn g_realRealloc = nullptr;
FreeFn g_realFree = nullptr;
MutexFn g_realLock = nullptr;
MutexFn g_realTrylock = nullptr;
std::atomic<bool> g_resolving{false};

alignas(16) unsigned char g_bootstrap[16384];
std::atomic<std::size_t> g_bootstrapUsed{0};

std::atomic<pthread_t> g_watchedPthread{};
std::atomic<std::uint64_t> g_cAllocCalls{0};
std::atomic<std::uint64_t> g_lockCalls{0};

bool onWatchedPthread()
{
    return g_watching.load(std::memory_order_acquire)
        && pthread_equal(pthread_self(), g_watchedPthread.load(std::memory_order_relaxed)) != 0;
}

bool fromBootstrap(const void* p)
{
    const auto* c = static_cast<const unsigned char*>(p);
    return c >= g_bootstrap && c < g_bootstrap + sizeof(g_bootstrap);
}

void* bootstrapAllocate(std::size_t size)
{
    const std::size_t rounded = (size + 15) & ~static_cast<std::size_t>(15);
    const std::size_t at = g_bootstrapUsed.fetch_add(rounded);
    if (at + rounded > sizeof(g_bootstrap)) {
        std::abort();
    }
    return g_bootstrap + at;   // static storage: already zero
}

void resolveReal()
{
    if (g_realFree != nullptr) {
        return;
    }
    g_resolving.store(true);
    g_realMalloc = reinterpret_cast<MallocFn>(dlsym(RTLD_NEXT, "malloc"));
    g_realCalloc = reinterpret_cast<CallocFn>(dlsym(RTLD_NEXT, "calloc"));
    g_realRealloc = reinterpret_cast<ReallocFn>(dlsym(RTLD_NEXT, "realloc"));
    g_realLock = reinterpret_cast<MutexFn>(dlsym(RTLD_NEXT, "pthread_mutex_lock"));
    g_realTrylock = reinterpret_cast<MutexFn>(dlsym(RTLD_NEXT, "pthread_mutex_trylock"));
    g_realFree = reinterpret_cast<FreeFn>(dlsym(RTLD_NEXT, "free"));
    g_resolving.store(false);
}

void countCAlloc()
{
    if (onWatchedPthread()) {
        g_cAllocCalls.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace

extern "C" void* malloc(std::size_t size) noexcept
{
    if (g_realMalloc == nullptr) {
        if (g_resolving.load()) {
            return bootstrapAllocate(size);
        }
        resolveReal();
    }
    countCAlloc();
    return g_realMalloc(size);
}

extern "C" void* calloc(std::size_t count, std::size_t size) noexcept
{
    if (g_realCalloc == nullptr) {
        if (g_resolving.load()) {
            return bootstrapAllocate(count * size);
        }
        resolveReal();
    }
    countCAlloc();
    return g_realCalloc(count, size);
}

extern "C" void* realloc(void* p, std::size_t size) noexcept
{
    if (g_realRealloc == nullptr) {
        if (g_resolving.load()) {
            void* fresh = bootstrapAllocate(size);
            if (p != nullptr) {
                std::memcpy(fresh, p, std::min<std::size_t>(size,
                    static_cast<std::size_t>(g_bootstrap + sizeof(g_bootstrap)
                                             - static_cast<unsigned char*>(p))));
            }
            return fresh;
        }
        resolveReal();
    }
    countCAlloc();
    if (p != nullptr && fromBootstrap(p)) {
        void* fresh = g_realMalloc(size);
        if (fresh != nullptr) {
            std::memcpy(fresh, p, std::min<std::size_t>(size,
                static_cast<std::size_t>(g_bootstrap + sizeof(g_bootstrap)
                                         - static_cast<unsigned char*>(p))));
        }
        return fresh;
    }
    return g_realRealloc(p, size);
}

extern "C" void free(void* p) noexcept
{
    if (p == nullptr || fromBootstrap(p)) {
        return;
    }
    if (g_realFree == nullptr) {
        resolveReal();
    }
    countCAlloc();
    g_realFree(p);
}

extern "C" int pthread_mutex_lock(pthread_mutex_t* mutex) noexcept
{
    if (g_realLock == nullptr) {
        resolveReal();
    }
    if (onWatchedPthread()) {
        g_lockCalls.fetch_add(1, std::memory_order_relaxed);
    }
    return g_realLock(mutex);
}

extern "C" int pthread_mutex_trylock(pthread_mutex_t* mutex) noexcept
{
    if (g_realTrylock == nullptr) {
        resolveReal();
    }
    if (onWatchedPthread()) {
        g_lockCalls.fetch_add(1, std::memory_order_relaxed);
    }
    return g_realTrylock(mutex);
}
#endif // Q_OS_LINUX

using namespace NereusSDR;

namespace {

constexpr int kRate = 48000;
constexpr int kWriteFrames = 64;
constexpr int kReadFrames = 128;
constexpr int kTrianglePeriod = 9600;
constexpr double kTriangleSlope = 1.0 / (kTrianglePeriod / 2);   // -0.5 .. 0.5
// The resampler's filter rounds the corners and its ratio moves within
// 0.96 .. 1.04, so a whole frame steps by at most this much.
constexpr double kMaxRampStep = 1.5 * kTriangleSlope;
// After a dry run or an overrun the slew, blend and fade-in are not a
// ramp; frames within this many of one are not checked for continuity.
constexpr std::uint64_t kExemptFrames = 8192;

double triangle(std::uint64_t frame)
{
    const auto phase = static_cast<double>(frame % kTrianglePeriod) / kTrianglePeriod;   // 0 .. 1
    if (phase < 0.25) {
        return 2.0 * phase;
    }
    if (phase < 0.75) {
        return 1.0 - 2.0 * phase;
    }
    return 2.0 * phase - 2.0;
}

void markWatchedThread()
{
    g_watchedId = std::this_thread::get_id();
#if defined(Q_OS_LINUX)
    g_watchedPthread.store(pthread_self(), std::memory_order_relaxed);
#endif
}

void resetCounts()
{
    g_newCalls.store(0);
    g_deleteCalls.store(0);
#if defined(Q_OS_LINUX)
    g_cAllocCalls.store(0);
    g_lockCalls.store(0);
#endif
}

struct ReaderResult {
    std::uint64_t reads = 0;
    std::uint64_t tornFrames = 0;
    std::uint64_t breaks = 0;
    std::uint64_t checkedFrames = 0;
    double largestCheckedStep = 0.0;
};

} // namespace

class TestDeviceRateMatcherThreads : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { RealtimeTestLoad::printLoadAverageIfFailed(); }
    void hooksSeeTheWatchedThread();
    void writerAndReaderThreads_data();
    void writerAndReaderThreads();
};

// The hooks are live: an allocation inside NereusSDRLib on a watched
// thread is counted (and, on Linux, a pthread mutex lock), and the same on
// an unwatched thread is not.
void TestDeviceRateMatcherThreads::hooksSeeTheWatchedThread()
{
    DeviceRateMatcher matcher(DeviceRateMatcher::Config{});
    QVERIFY(matcher.valid());
    resetCounts();
    std::thread watched([&matcher] {
        markWatchedThread();
        g_watching.store(true, std::memory_order_release);
        {
            MatcherReader reader = matcher.makeReader();   // allocates its slew buffers
            std::mutex mutex;
            const std::lock_guard<std::mutex> lock(mutex);
        }
        g_watching.store(false, std::memory_order_release);
    });
    watched.join();
    QVERIFY(g_newCalls.load() > 0);
    QVERIFY(g_deleteCalls.load() > 0);
#if defined(Q_OS_LINUX)
    QVERIFY(g_cAllocCalls.load() > 0);
    QVERIFY(g_lockCalls.load() > 0);
#endif
    resetCounts();
    MatcherReader unwatched = matcher.makeReader();
    QCOMPARE(g_newCalls.load(), std::uint64_t{0});
}

void TestDeviceRateMatcherThreads::writerAndReaderThreads_data()
{
    QTest::addColumn<int>("ppm");
    QTest::newRow("reader 200 ppm fast") << 200;
    QTest::newRow("reader 200 ppm slow") << -200;
}

// Thirty seconds a row, 60 s in all: no torn frames (left and right are
// written as x and -x, so a frame read while it was being written shows as
// a mismatch), the ramp continuous outside dry runs and overruns, and the
// reader thread free of allocation and (Linux) mutex calls.
void TestDeviceRateMatcherThreads::writerAndReaderThreads()
{
    QFETCH(int, ppm);
    using Clock = std::chrono::steady_clock;
    constexpr auto kRunTime = std::chrono::seconds(30);

    DeviceRateMatcher matcher(DeviceRateMatcher::Config{});
    QVERIFY(matcher.valid());
    MatcherReader reader = matcher.makeReader();
    QVERIFY(reader.valid());
    MatcherRingHeader* ring = matcher.ring();

    resetCounts();
    std::atomic<bool> stop{false};
    const Clock::time_point start = Clock::now() + std::chrono::milliseconds(20);

    std::thread writer([&] {
        std::vector<float> block(static_cast<std::size_t>(kWriteFrames) * 2);
        std::uint64_t frame = 0;
        std::int64_t n = 0;
        while (!stop.load(std::memory_order_acquire)) {
            const auto due = start + std::chrono::nanoseconds(n * kWriteFrames * 1'000'000'000LL / kRate);
            std::this_thread::sleep_until(due);
            for (int f = 0; f < kWriteFrames; ++f) {
                const auto v = static_cast<float>(triangle(frame + static_cast<std::uint64_t>(f)));
                block[static_cast<std::size_t>(2 * f + 0)] = v;
                block[static_cast<std::size_t>(2 * f + 1)] = -v;
            }
            frame += kWriteFrames;
            const std::int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now().time_since_epoch()).count();
            matcher.write(block.data(), kWriteFrames, nowNs);
            ++n;
        }
    });

    ReaderResult result;
    std::thread readerThread([&] {
        std::vector<float> out(static_cast<std::size_t>(kReadFrames) * 2);
        // Warm the paths (lazy symbol binding) before the watch starts.
        std::this_thread::sleep_until(Clock::now());
        markWatchedThread();
        g_watching.store(true, std::memory_order_release);

        const double periodNs = kReadFrames * 1.0e9 / (kRate * (1.0 + ppm * 1.0e-6));
        std::uint64_t dryRuns = ring->dryRuns.load(std::memory_order_relaxed);
        std::uint64_t overruns = ring->overruns.load(std::memory_order_relaxed);
        std::uint64_t exemptUntil = kExemptFrames;   // the start: silence, then the filter
        std::uint64_t played = 0;
        double last = 0.0;
        const Clock::time_point end = start + kRunTime;
        for (std::uint64_t m = 0;; ++m) {
            const auto due = start + std::chrono::nanoseconds(static_cast<std::int64_t>(m * periodNs));
            if (due >= end) {
                break;
            }
            std::this_thread::sleep_until(due);
            reader.read(out.data(), kReadFrames);
            ++result.reads;
            const std::uint64_t nowDry = ring->dryRuns.load(std::memory_order_relaxed);
            const std::uint64_t nowOver = ring->overruns.load(std::memory_order_relaxed);
            if (nowDry != dryRuns || nowOver != overruns) {
                dryRuns = nowDry;
                overruns = nowOver;
                exemptUntil = played + kExemptFrames;
            }
            for (int f = 0; f < kReadFrames; ++f) {
                const float left = out[static_cast<std::size_t>(2 * f + 0)];
                const float right = out[static_cast<std::size_t>(2 * f + 1)];
                if (left != -right) {
                    ++result.tornFrames;
                }
                const double step = std::abs(static_cast<double>(left) - last);
                if (played >= exemptUntil) {
                    ++result.checkedFrames;
                    result.largestCheckedStep = std::max(result.largestCheckedStep, step);
                    if (step > kMaxRampStep) {
                        ++result.breaks;
                    }
                }
                last = left;
                ++played;
            }
        }
        g_watching.store(false, std::memory_order_release);
    });

    readerThread.join();
    stop.store(true, std::memory_order_release);
    writer.join();

    const DeviceRateMatcherStats stats = matcher.stats();
    qInfo("ppm %d: %llu reads, %llu frames checked, largest step %.6f (limit %.6f), %llu torn, "
          "%llu breaks; ratio %.6f, step %d ms, dry runs %llu, overruns %llu; reader new %llu "
          "delete %llu",
          ppm, static_cast<unsigned long long>(result.reads),
          static_cast<unsigned long long>(result.checkedFrames), result.largestCheckedStep,
          kMaxRampStep, static_cast<unsigned long long>(result.tornFrames),
          static_cast<unsigned long long>(result.breaks), stats.ratio, stats.delayStepMs,
          static_cast<unsigned long long>(stats.dryRuns),
          static_cast<unsigned long long>(stats.overruns),
          static_cast<unsigned long long>(g_newCalls.load()),
          static_cast<unsigned long long>(g_deleteCalls.load()));
#if defined(Q_OS_LINUX)
    qInfo("ppm %d: reader malloc family %llu, pthread mutex %llu", ppm,
          static_cast<unsigned long long>(g_cAllocCalls.load()),
          static_cast<unsigned long long>(g_lockCalls.load()));
#endif

    // The reader made one read for every due time in the 30 s, late or
    // not, so its count says nothing about timing; what it can check is
    // that the matcher counted every read and every frame the reader asked
    // for.
    QVERIFY(result.reads > 0);
    QCOMPARE(ring->readCalls.load(std::memory_order_acquire), result.reads);
    QCOMPARE(ring->requested.load(std::memory_order_acquire),
             result.reads * static_cast<std::uint64_t>(kReadFrames));
    QVERIFY(result.checkedFrames > 0);
    QCOMPARE(result.tornFrames, std::uint64_t{0});
    QCOMPARE(result.breaks, std::uint64_t{0});
    QCOMPARE(g_newCalls.load(), std::uint64_t{0});
    QCOMPARE(g_deleteCalls.load(), std::uint64_t{0});
#if defined(Q_OS_LINUX)
    QCOMPARE(g_cAllocCalls.load(), std::uint64_t{0});
    QCOMPARE(g_lockCalls.load(), std::uint64_t{0});
#endif
}

QTEST_GUILESS_MAIN(TestDeviceRateMatcherThreads)
#include "tst_device_rate_matcher_threads.moc"
