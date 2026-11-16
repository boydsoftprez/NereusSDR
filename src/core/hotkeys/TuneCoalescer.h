// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 2.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Adds up steps from held keys and knob bursts and lets them out at most once
// per interval, with none lost. Pure logic: the caller owns the clock and timer.
#pragma once

#include <QtGlobal>

namespace NereusSDR {

class TuneCoalescer {
public:
    static constexpr qint64 kMinIntervalMs = 20;  // Spec V5. AetherSDR MainWindow_Controllers.cpp:2295 [@1e0718ad]

    // Adds steps; returns the steps to send now, or 0 if the interval has not passed.
    int add(int steps, qint64 nowMs)
    {
        m_pending += steps;
        return takeDue(nowMs);
    }

    // Called by the timer.
    int takeDue(qint64 nowMs)
    {
        if (m_pending == 0 || (m_hasSent && nowMs - m_lastSendMs < kMinIntervalMs)) {
            return 0;
        }
        const int due = m_pending;
        m_pending = 0;
        m_lastSendMs = nowMs;
        m_hasSent = true;
        return due;
    }

    // Lock or refusal. The last send still counts toward the interval.
    void drop() { m_pending = 0; }

    int pending() const { return m_pending; }

private:
    int m_pending{0};
    qint64 m_lastSendMs{0};
    bool m_hasSent{false};
};

} // namespace NereusSDR
