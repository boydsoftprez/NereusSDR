// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original "stop by itself when it goes quiet" rule.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the quiet rule of spec V4 as pure logic
//             with an injected clock. AI tooling: Anthropic Claude Code.
#pragma once

#include <QtGlobal>

namespace NereusSDR {

// Spec V4: a recording is quiet when you have not transmitted and the slice
// has heard nothing above its squelch (or, with squelch off, nothing more
// than 6 dB above the noise floor) for the set number of minutes. Times are
// milliseconds on whatever clock the caller uses; nothing here reads a
// clock. One rule per recording, on the Core's model thread.
class QuietRule {
public:
    // With squelch off, a level this far above the floor (strictly more)
    // counts as heard (spec V4).
    static constexpr double kAboveFloorDb = 6.0;
    static constexpr int kDefaultMinutes = 2;
    static constexpr qint64 kMsPerMinute = 60000;

    void setMinutes(int m) { m_windowMs = static_cast<qint64>(m > 0 ? m : 1) * kMsPerMinute; }

    // The recording starts, which counts as the last thing heard.
    void start(qint64 nowMs) { m_lastActivityMs = nowMs; }

    void onTransmit(qint64 nowMs) { noteActivity(nowMs); }

    // One reading of the slice. With squelch on, `aboveSquelch` says whether
    // the squelch let anything through; with it off, the level and the noise
    // floor (both in dB, on the same scale) decide.
    void onLevel(qint64 nowMs, bool squelchOn, bool aboveSquelch, double levelDb, double noiseFloorDb)
    {
        if (squelchOn) {
            if (aboveSquelch) {
                noteActivity(nowMs);
            }
            return;
        }
        if (levelDb > noiseFloorDb + kAboveFloorDb) {
            noteActivity(nowMs);
        }
    }

    bool quiet(qint64 nowMs) const { return nowMs - m_lastActivityMs >= m_windowMs; }

    qint64 lastActivityMs() const { return m_lastActivityMs; }

private:
    void noteActivity(qint64 nowMs)
    {
        if (nowMs > m_lastActivityMs) {
            m_lastActivityMs = nowMs;
        }
    }

    qint64 m_windowMs = static_cast<qint64>(kDefaultMinutes) * kMsPerMinute;
    qint64 m_lastActivityMs = 0;
};

}  // namespace NereusSDR
