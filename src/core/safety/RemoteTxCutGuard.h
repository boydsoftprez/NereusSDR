// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/RemoteTxCutGuard.h  (NereusSDR)
// =================================================================
//
// A keyed remote device's RF is held silent, then its transmit cut, off the
// main thread when the Core's main thread stalls. RemoteTxWatchdog lives on
// the main thread, so it cannot run while the event loop is held; this
// guard runs on a thread of its own and judges the watchdog's hold and cut
// deadlines (RemoteTxWatchdog::kLinkHoldMs, kLinkCutMs) from keepalives it
// hears off the main thread (the "tx" data channel's messages, at their
// receipt).
//
// ---- The rules ----
// - arm(epoch) when a remote device keys: a new arm serial (1, 2, 3, ...),
//   last-heard set to the arm time, not judging, nothing cut. Epoch 0 means
//   the device has no media session, so nothing off the main thread can
//   start judging.
// - heardOffMain(epoch, at) with the armed epoch (not 0) moves last-heard to
//   the later of last-heard and at, and starts judging. Any other epoch, or
//   a heard while disarmed, is ignored.
// - heardOnMain(at) while armed moves last-heard the same way (a keepalive
//   for the armed device on any path, seen on the main thread) but does not
//   start judging: a main thread that runs needs no guard.
// - While judging, once now >= last-heard + kHoldMs + 1 ms (more than
//   kHoldMs of silence, the watchdog's own rule), HoldFn(true) runs once on
//   the guard's thread and the guard is holding. A heard while holding (off
//   main or on main) wakes the thread, which runs HoldFn(false) and goes on
//   judging; a later silence holds again.
// - Once now >= last-heard + kCutMs + 1 ms while holding, CutFn runs once on
//   the guard's thread with the armed epoch, the arm serial and how long
//   the link was silent, then HoldFn(false); then nothing more until the
//   next arm(). A heard after the cut does not undo it.
// - disarm() stops judging, and a new arm() resets the guard; either one
//   runs HoldFn(false) on its caller's thread, under the mutex, before it
//   returns when the guard was holding. The destructor does the same, then
//   stops and joins the thread; no cut or hold runs after it returns.
//
// ---- The thread ----
// One std::thread ("NereusTxCutGuard" where the platform names threads)
// waits on a condition variable until the next deadline, or indefinitely
// while not judging. A heard does not wake it unless the guard is holding:
// at the deadline it rechecks last-heard and waits again if it moved. CutFn
// and HoldFn run with the guard's mutex held, so arm() and disarm() never
// return while a cut or hold for an older arm is still running; neither
// may block, wait on another thread (apart from the TX channel's short
// sender spin), or call arm(), disarm(), heardOffMain() or heardOnMain().
// isJudging(), isHolding() and armSerial() read atomics and may be called
// from them.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-15: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), remote transmit cut off the main thread, with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-16: cuts after more than kDeadlineMs of silence, not at it,
//               as the watchdog does. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: kDeadlineMs is now
//               RemoteTxWatchdog::kLinkCutMs (renamed from
//               kLinkLossDeadlineMs); its hold comes later. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: holds RF (HoldFn) after
//               more than kHoldMs of silence and cuts after more than
//               kCutMs; kDeadlineMs is gone. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "core/NereusCoreExport.h"
#include "core/safety/RemoteTxWatchdog.h"

namespace NereusSDR {

class NEREUS_CORE_EXPORT RemoteTxCutGuard {
public:
    using Clock = std::chrono::steady_clock;
    // Called on the guard's own thread, at most once per arm, with the guard's
    // mutex held, so arm() and disarm() never return while a cut for an older
    // arm is still running. It must not block or wait on any other thread.
    using CutFn = std::function<void(quint64 mediaEpoch, quint64 armSerial, qint64 silentMs)>;

    // Called on the guard's thread with its mutex held, like CutFn; must not block.
    // Also called from arm(), disarm() and the destructor, on their caller's
    // thread under the same mutex, to release a hold.
    using HoldFn = std::function<void(bool held)>;

    static constexpr qint64 kHoldMs = RemoteTxWatchdog::kLinkHoldMs;
    static constexpr qint64 kCutMs = RemoteTxWatchdog::kLinkCutMs;

    RemoteTxCutGuard(CutFn cut, HoldFn hold);   // starts its thread
    ~RemoteTxCutGuard();                    // stops and joins it; no cut after it returns
    RemoteTxCutGuard(const RemoteTxCutGuard&) = delete;
    RemoteTxCutGuard& operator=(const RemoteTxCutGuard&) = delete;

    quint64 arm(quint64 mediaEpoch);        // main: a remote device keyed; 0 = no media session. Returns the new arm serial.
    void disarm();                          // main: no remote key
    void heardOffMain(quint64 mediaEpoch, Clock::time_point at);   // any thread: a "tx" channel message
    void heardOnMain(Clock::time_point at); // main: a keepalive for the armed device on any path

    bool isJudging() const;                 // tests and logs
    bool isHolding() const;                 // tests and logs
    quint64 armSerial() const;

private:
    void run();
    // With m_mutex held: HoldFn(false) if holding.
    void releaseHoldLocked();

    const CutFn m_cut;
    const HoldFn m_hold;

    std::mutex m_mutex;
    std::condition_variable m_wake;
    // Guarded by m_mutex.
    bool m_armed{false};
    bool m_stop{false};
    quint64 m_epoch{0};
    Clock::time_point m_lastHeard{};
    // Written under m_mutex; read without it by isJudging(), isHolding()
    // and armSerial().
    std::atomic<bool> m_judging{false};
    std::atomic<bool> m_holding{false};
    std::atomic<quint64> m_armSerial{0};

    // Last, so every member above exists before the thread starts.
    std::thread m_thread;
};

} // namespace NereusSDR
