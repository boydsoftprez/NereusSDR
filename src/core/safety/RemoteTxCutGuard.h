// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/RemoteTxCutGuard.h  (NereusSDR)
// =================================================================
//
// A keyed remote device's transmit is cut off the main thread when the
// Core's main thread stalls. RemoteTxWatchdog lives on the main thread, so
// it cannot run while the event loop is held; this guard runs on a thread
// of its own and judges the watchdog's cut deadline
// (RemoteTxWatchdog::kLinkCutMs) from keepalives it hears off the
// main thread (the "tx" data channel's messages, at their receipt).
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
// - While judging, once now >= last-heard + kDeadlineMs + 1 ms (more than
//   kDeadlineMs of silence, the watchdog's own rule), CutFn runs once on
//   the guard's thread with the armed epoch, the arm serial and how long
//   the link was silent; then nothing more until the next arm(). A heard
//   after the cut does not undo it.
// - disarm() stops judging. The destructor stops and joins the thread; no
//   cut runs after it returns.
//
// ---- The thread ----
// One std::thread ("NereusTxCutGuard" where the platform names threads)
// waits on a condition variable until the deadline, or indefinitely while
// not judging. A heard does not wake it: at the deadline it rechecks
// last-heard and waits again if it moved. CutFn runs with the guard's mutex
// held, so arm() and disarm() never return while a cut for an older arm is
// still running; CutFn must not block, wait on another thread, or call
// arm(), disarm(), heardOffMain() or heardOnMain(). isJudging() and
// armSerial() read atomics and may be called from it.
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

    static constexpr qint64 kDeadlineMs = RemoteTxWatchdog::kLinkCutMs;

    explicit RemoteTxCutGuard(CutFn cut);   // starts its thread
    ~RemoteTxCutGuard();                    // stops and joins it; no cut after it returns
    RemoteTxCutGuard(const RemoteTxCutGuard&) = delete;
    RemoteTxCutGuard& operator=(const RemoteTxCutGuard&) = delete;

    quint64 arm(quint64 mediaEpoch);        // main: a remote device keyed; 0 = no media session. Returns the new arm serial.
    void disarm();                          // main: no remote key
    void heardOffMain(quint64 mediaEpoch, Clock::time_point at);   // any thread: a "tx" channel message
    void heardOnMain(Clock::time_point at); // main: a keepalive for the armed device on any path

    bool isJudging() const;                 // tests and logs
    quint64 armSerial() const;

private:
    void run();

    const CutFn m_cut;

    std::mutex m_mutex;
    std::condition_variable m_wake;
    // Guarded by m_mutex.
    bool m_armed{false};
    bool m_stop{false};
    quint64 m_epoch{0};
    Clock::time_point m_lastHeard{};
    // Written under m_mutex; read without it by isJudging() and armSerial().
    std::atomic<bool> m_judging{false};
    std::atomic<quint64> m_armSerial{0};

    // Last, so every member above exists before the thread starts.
    std::thread m_thread;
};

} // namespace NereusSDR
