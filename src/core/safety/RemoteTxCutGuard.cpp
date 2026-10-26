// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/RemoteTxCutGuard.cpp  (NereusSDR)
// =================================================================
//
// See RemoteTxCutGuard.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-15: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), remote transmit cut off the main thread, with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-16: cuts after more than kDeadlineMs of silence, not at it,
//               as the watchdog does. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: run() holds RF after more
//               than kHoldMs of silence, releases the hold on a heard,
//               and cuts after more than kCutMs; arm(), disarm() and the
//               destructor release a hold. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/safety/RemoteTxCutGuard.h"

#include <utility>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <pthread.h>
#endif

namespace NereusSDR {

namespace {

void nameThisThread()
{
#if defined(Q_OS_MAC)
    pthread_setname_np("NereusTxCutGuard");
#elif defined(Q_OS_LINUX)
    // Linux allows 15 characters plus the terminator; this name is 16.
    pthread_setname_np(pthread_self(), "NereusTxCutGrd");
#endif
}

} // namespace

RemoteTxCutGuard::RemoteTxCutGuard(CutFn cut, HoldFn hold)
    : m_cut(std::move(cut))
    , m_hold(std::move(hold))
{
    m_thread = std::thread([this] { run(); });
}

RemoteTxCutGuard::~RemoteTxCutGuard()
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
        m_judging.store(false);
        releaseHoldLocked();
    }
    m_wake.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

quint64 RemoteTxCutGuard::arm(quint64 mediaEpoch)
{
    quint64 serial = 0;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        // The previous arm's hold goes before this arm starts.
        releaseHoldLocked();
        m_armed = true;
        m_epoch = mediaEpoch;
        m_lastHeard = Clock::now();
        m_judging.store(false);
        serial = m_armSerial.load() + 1;
        m_armSerial.store(serial);
    }
    m_wake.notify_all();
    return serial;
}

void RemoteTxCutGuard::disarm()
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_armed = false;
        m_judging.store(false);
        releaseHoldLocked();
    }
    m_wake.notify_all();
}

void RemoteTxCutGuard::heardOffMain(quint64 mediaEpoch, Clock::time_point at)
{
    bool wake = false;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_armed || mediaEpoch == 0 || mediaEpoch != m_epoch) {
            return;
        }
        if (at > m_lastHeard) {
            m_lastHeard = at;
        }
        if (!m_judging.load()) {
            m_judging.store(true);
            wake = true;
        }
        // A heard while holding wakes the thread to release the hold now,
        // not at the cut deadline it is waiting for.
        if (m_holding.load()) {
            wake = true;
        }
    }
    // Otherwise a later heard is picked up when the thread rechecks at the
    // deadline.
    if (wake) {
        m_wake.notify_all();
    }
}

void RemoteTxCutGuard::heardOnMain(Clock::time_point at)
{
    bool wake = false;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_armed) {
            return;
        }
        if (at > m_lastHeard) {
            m_lastHeard = at;
        }
        wake = m_holding.load();
    }
    if (wake) {
        m_wake.notify_all();
    }
}

bool RemoteTxCutGuard::isJudging() const
{
    return m_judging.load();
}

bool RemoteTxCutGuard::isHolding() const
{
    return m_holding.load();
}

void RemoteTxCutGuard::releaseHoldLocked()
{
    if (!m_holding.load()) {
        return;
    }
    m_holding.store(false);
    if (m_hold) {
        m_hold(false);
    }
}

quint64 RemoteTxCutGuard::armSerial() const
{
    return m_armSerial.load();
}

void RemoteTxCutGuard::run()
{
    nameThisThread();
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_stop) {
        if (!m_armed || !m_judging.load()) {
            m_wake.wait(lock);
            continue;
        }
        // "More than 400 ms", as RemoteTxWatchdog judges it: the first
        // millisecond past the deadline. Cutting at the deadline itself cut
        // at exactly 400 ms of silence, and the watchdog's stop then
        // reported 400 (CI, 2026-10-16). The hold and the cut follow the
        // same rule (2026-10-26).
        const Clock::time_point holdAt =
            m_lastHeard + std::chrono::milliseconds(kHoldMs + 1);
        const Clock::time_point cutAt =
            m_lastHeard + std::chrono::milliseconds(kCutMs + 1);
        const Clock::time_point now = Clock::now();
        if (now < holdAt) {
            if (m_holding.load()) {
                // Heard again within the hold: RF back, judging goes on.
                releaseHoldLocked();
                continue;
            }
            // Woken early or spuriously, or last-heard moved: recheck then.
            m_wake.wait_until(lock, holdAt);
            continue;
        }
        if (!m_holding.load()) {
            m_holding.store(true);
            if (m_hold) {
                m_hold(true);
            }
            continue;
        }
        if (now < cutAt) {
            // Holding: a heard wakes this wait, so the hold is released at
            // once rather than at the cut deadline.
            m_wake.wait_until(lock, cutAt);
            continue;
        }
        const qint64 silentMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastHeard).count();
        // One cut per arm: stop judging and disarm before it runs, so a heard
        // after the cut cannot start judging again until the next arm().
        m_judging.store(false);
        m_armed = false;
        if (m_cut) {
            m_cut(m_epoch, m_armSerial.load(), silentMs);
        }
        // The cut has shut RF; the hold is no longer needed.
        releaseHoldLocked();
    }
}

} // namespace NereusSDR
