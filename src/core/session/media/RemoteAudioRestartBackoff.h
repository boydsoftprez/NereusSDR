#pragma once
// no-port-check: NereusSDR-original retry pacing for remote audio contexts.
#include <QtGlobal>
#include <algorithm>

namespace NereusSDR {

/// R-R3-21: how long a window waits before it asks the Core for a fresh
/// audio context after a receiver asked for one. The first restart waits
/// 1 s from the previous request (as before), each further one twice as
/// long, capped at 4 s: 1, 2, 4, 4 s. A context that ran healthy for 10 s
/// (its restart came 10 s or more after it was requested) starts the
/// count over.
class RemoteAudioRestartBackoff {
public:
    static constexpr qint64 kFirstDelayMs = 1000;
    static constexpr qint64 kMaxDelayMs = 4000;
    static constexpr qint64 kHealthyResetMs = 10'000;

    /// A restart at nowMs of the context requested at lastRequestMs: the
    /// wait from nowMs before the next request (never negative).
    qint64 nextDelayMs(qint64 nowMs, qint64 lastRequestMs)
    {
        const qint64 ranMs = nowMs - lastRequestMs;
        if (ranMs >= kHealthyResetMs) { m_streak = 0; }
        const qint64 stepMs = std::min(kMaxDelayMs, kFirstDelayMs << std::min(m_streak, 2));
        m_streak = std::min(m_streak + 1, 3);
        return std::max<qint64>(0, stepMs - ranMs);
    }
    /// Start over. The window calls it for a new media connection, for the
    /// operator's Retry (speakers) and for a headphones device change.
    void reset() { m_streak = 0; }
    /// Restarts counted since the last reset (at most 3).
    int streak() const { return m_streak; }

private:
    int m_streak = 0;
};

} // namespace NereusSDR
