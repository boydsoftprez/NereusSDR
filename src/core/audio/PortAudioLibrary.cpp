// =================================================================
// src/core/audio/PortAudioLibrary.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See PortAudioLibrary.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 fix (R-AUD-06). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: final review fix (R-AUD-03, R-AUD-06): bounded waits
//               (lockBounded, LongHold); the counts are atomics read
//               without the lock. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/audio/PortAudioLibrary.h"

#include "core/LogCategories.h"

#include <portaudio.h>

#include <QThread>

#include <atomic>
#include <chrono>
#include <cstdint>

namespace NereusSDR::PortAudioLibrary {

namespace {

// Changed under mutex(); read without it.
std::atomic<int> g_references{0};
std::atomic<int> g_openStreams{0};

// steady_clock nanoseconds since a LongHold took the lock; 0 while none
// holds it.
std::atomic<std::int64_t> g_longHoldSinceNs{0};
// The LongHold a bounded caller last said it gave up on, so a stuck
// driver is reported once, not on every query.
std::atomic<std::int64_t> g_reportedHoldNs{0};

constexpr std::int64_t kBoundedWaitNs = std::int64_t(kBoundedWaitMs) * 1000000;
constexpr int kPollMs = 1;

std::int64_t nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

std::recursive_mutex& mutex()
{
    static std::recursive_mutex lock;
    return lock;
}

std::unique_lock<std::recursive_mutex> lockBounded()
{
    std::unique_lock<std::recursive_mutex> lock(mutex(), std::try_to_lock);
    if (lock.owns_lock()) {
        return lock;
    }
    const std::int64_t startNs = nowNs();
    for (;;) {
        const std::int64_t now = nowNs();
        const std::int64_t heldSince = g_longHoldSinceNs.load(std::memory_order_acquire);
        if (now - startNs >= kBoundedWaitNs
            || (heldSince != 0 && now - heldSince >= kBoundedWaitNs)) {
            break;
        }
        QThread::msleep(kPollMs);
        if (lock.try_lock()) {
            return lock;
        }
    }
    const std::int64_t heldSince = g_longHoldSinceNs.load(std::memory_order_acquire);
    if (g_reportedHoldNs.exchange(heldSince, std::memory_order_acq_rel) != heldSince
        || heldSince == 0) {
        qCWarning(lcAudio) << "Older drivers: PortAudio is busy in a driver call that has"
                           << "not returned; not waiting for it";
    }
    return lock;
}

LongHold::LongHold()
    : m_lock(mutex())
{
    std::int64_t expected = 0;
    m_marked = g_longHoldSinceNs.compare_exchange_strong(expected, nowNs(),
                                                         std::memory_order_acq_rel);
}

LongHold::~LongHold()
{
    if (m_marked) {
        g_longHoldSinceNs.store(0, std::memory_order_release);
    }
}

bool acquire()
{
    std::unique_lock<std::recursive_mutex> lock = lockBounded();
    if (!lock.owns_lock()) {
        return false;
    }
    const PaError err = Pa_Initialize();
    if (err != paNoError) {
        qCWarning(lcAudio) << "Pa_Initialize failed:" << Pa_GetErrorText(err);
        return false;
    }
    g_references.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

void release()
{
    std::unique_lock<std::recursive_mutex> lock = lockBounded();
    if (!lock.owns_lock()) {
        // A driver call has not returned: Pa_Terminate now would pull
        // PortAudio away under it.  The reference stays; the process's
        // exit reclaims PortAudio.
        qCWarning(lcAudio) << "Older drivers: PortAudio left running at shutdown;"
                           << "a driver call has not returned";
        return;
    }
    if (g_references.load(std::memory_order_acquire) <= 0) {
        return;
    }
    g_references.fetch_sub(1, std::memory_order_acq_rel);
    Pa_Terminate();
}

int references()
{
    return g_references.load(std::memory_order_acquire);
}

bool reinitialize()
{
    const LongHold hold;
    const int openStreams = g_openStreams.load(std::memory_order_acquire);
    if (openStreams > 0) {
        qCWarning(lcAudio) << "Older drivers: PortAudio not listed again;" << openStreams
                           << "of its streams are still open, so the list is unchanged";
        return false;
    }
    const int references = g_references.load(std::memory_order_acquire);
    if (references <= 0) {
        return false;   // not running: the next start lists the devices present then
    }
    for (int i = 0; i < references; ++i) {
        Pa_Terminate();
    }
    g_references.store(0, std::memory_order_release);
    for (int i = 0; i < references; ++i) {
        const PaError err = Pa_Initialize();
        if (err != paNoError) {
            qCWarning(lcAudio) << "Older drivers: PortAudio did not start again:"
                               << Pa_GetErrorText(err);
            break;
        }
        g_references.fetch_add(1, std::memory_order_acq_rel);
    }
    if (g_references.load(std::memory_order_acquire) != references) {
        return false;
    }
    qCInfo(lcAudio) << "Older drivers: PortAudio started again to list the devices present now";
    return true;
}

void streamOpened()
{
    g_openStreams.fetch_add(1, std::memory_order_acq_rel);
}

void streamClosed()
{
    int open = g_openStreams.load(std::memory_order_acquire);
    while (open > 0
           && !g_openStreams.compare_exchange_weak(open, open - 1, std::memory_order_acq_rel)) {
    }
}

int openStreams()
{
    return g_openStreams.load(std::memory_order_acquire);
}

} // namespace NereusSDR::PortAudioLibrary
