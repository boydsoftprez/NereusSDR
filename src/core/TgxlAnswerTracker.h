// src/core/TgxlAnswerTracker.h (NereusSDR)
//
// What this computer has sent the Tuner Genius XL that the tuner may answer
// with its own `transmit tune on` / `transmit tune off` on the SmartSDR API
// port, so that an answer is never taken for the tuner's front-panel TUNE
// (ruling 8.9b, design doc 2026-09-24-several-devices-on-one-core-design.md).
//
// no-port-check: NereusSDR-original file. The tuner behaviour it models is
// read from captures, not ported from any upstream source:
//   - captures/flex-tgxl-direct-CONTROL.pcapng: `C27|autotune` at T+172.199,
//     `S0|state ... tuning=1` at T+172.201, then `C7|transmit tune on` and
//     `C6|interlock ready 3` together at T+172.702 (503 ms after the send).
//     Each `autotune` in the captures is followed by its own tuning rise:
//     T+172.199 -> 172.201, T+541.692 -> 541.716, T+546.988 -> 547.030.
//   - commit 01ca5b824 item (8), bench 2026-05-20: the tuner answers our
//     `transmit tune=1` with its own `transmit tune on`. How often (once per
//     tune, or once per frame) and how late has never been measured.
//
// Modification history (NereusSDR):
//   2026-10-01 - Created for the TGXL tune lane (Job B round 2, I-A and
//                m-A). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-10-01 - Round 3: every tune=1 frame is counted; answers carry
//                their kind and latency; `autotune` entries are keyed by
//                link epoch and sequence; a tuning rise starts only the
//                oldest unstarted `autotune`. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-01 - Round 4: the guarantees below restated as the invariant
//                that holds. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//
// One entry per thing sent, counted, never a single flag:
//   - an `autotune` (by link epoch and sequence) expects a tune on;
//   - every frame carrying `transmit ... tune=1` expects a tune on (the
//     echo), counted per frame because the echo cadence is unknown;
//   - a tune=0 the tuner can see as a change expects a tune off (the echo).
// A tune on from the tuner first answers the oldest entry that expects one;
// only a tune on that answers nothing is a press. An entry leaves when:
//   - its answer arrives;
//   - the tuner refuses that `autotune` (its own epoch and sequence only);
//   - the sweep it started ends (the first tuning rise after the send
//     belongs to it; that sweep's fall, or a tune off that is not an echo,
//     ends it);
//   - it is older than kAnswerWindowMs.
// A link drop and reconnect does not clear it: the answer comes on the
// SmartSDR API port, not the dropped :9010 link.
//
// The invariant: takes never outnumber the tuner's real presses, provided
// every send the tuner answers is counted, each answer comes within
// kAnswerWindowMs, and an `autotune`'s tune on comes before its sweep's
// fall (the capture: tune on 503 ms after the send, inside the sweep).
// It is not that every answer is classed as an answer. An answer is
// classed as the press in two cases:
//   1. A real press arrives first and uses up the entry its answer was
//      waiting for (press, then echo). The answer is then the take; one
//      press, one take.
//   2. A sweep's fall (or a tune off that is not an echo) removes an
//      `autotune` entry before that sweep's tune on arrives. That answer
//      is then a take with no press. The capture never shows this order.
// The other side fails closed: a press inside the window after a tune,
// while an entry still waits (an echo the tuner never sent, a frame to a
// client that is not the tuner or to a dropped socket), uses up that entry
// and is dropped: it keys nothing and takes nothing, and the operator
// presses again once the window has passed. If the entry is the band-change
// recall's `autotune`, RadioModel keys the press as a station key, which
// never takes.
// An answer later than kAnswerWindowMs, or one to a send the Core did not
// count, is a take with no press.
// RadioModel logs each answer's kind and latency, and the echoes per tune,
// so a bench tune can size the window and confirm the cadence.

#pragma once

#include <QtGlobal>

#include <vector>

namespace NereusSDR {

class TgxlAnswerTracker
{
public:
    /// How long an entry waits for its answer. The capture shows the
    /// tuner answering an `autotune` in 503 ms; the Core already gives the
    /// tuner 3 s to start a sweep once the carrier is up
    /// (RadioModel::kTgxlDeviceCycleStartMs, TunerApplet's short watchdog).
    /// The echo's latency has never been measured; RadioModel logs it.
    static constexpr qint64 kAnswerWindowMs = 3000;

    enum class Kind {
        None,             ///< answers nothing: the tuner's own TUNE
        Autotune,         ///< an `autotune` sent outside a tune cycle (the recall)
        CycleAutotune,    ///< an `autotune` sent for a running tune cycle
        TuneOnEcho,       ///< the echo of a tune=1 frame
        TuneOffEcho,      ///< the echo of a tune=0 change
    };

    struct Answer {
        Kind kind{Kind::None};
        qint64 latencyMs{0};
    };

    void autotuneSent(quint64 epoch, quint32 seq, bool forCycle, qint64 nowMs)
    {
        expire(nowMs);
        m_entries.push_back(
            {forCycle ? Kind::CycleAutotune : Kind::Autotune, epoch, seq, nowMs, false});
    }

    void autotuneRejected(quint64 epoch, quint32 seq)
    {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (isAutotune(it->kind) && it->epoch == epoch && it->seq == seq) {
                m_entries.erase(it);
                return;
            }
        }
    }

    /// The Core sent `transmit tune=<on>`: every tune=1 frame, and each
    /// tune=0 the tuner can see as a change.
    void tuneSent(bool on, qint64 nowMs)
    {
        expire(nowMs);
        m_entries.push_back({on ? Kind::TuneOnEcho : Kind::TuneOffEcho, 0, 0, nowMs, false});
    }

    void tuningChanged(bool tuning, qint64 nowMs)
    {
        expire(nowMs);
        if (tuning) {
            // One rise per `autotune` (the captures): the oldest one not
            // yet started is the one this rise belongs to.
            for (Entry& e : m_entries) {
                if (isAutotune(e.kind) && !e.sweepStarted) {
                    e.sweepStarted = true;
                    return;
                }
            }
            return;
        }
        endStartedSweeps();
    }

    /// The tuner sent `transmit tune on`. Uses up the oldest entry that
    /// expects one and says which; Kind::None is the tuner's own TUNE.
    Answer tuneOnAnswer(qint64 nowMs)
    {
        expire(nowMs);
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->kind != Kind::TuneOffEcho) {
                const Answer answer{it->kind, nowMs - it->sentMs};
                m_entries.erase(it);
                return answer;
            }
        }
        return {};
    }

    /// The tuner sent `transmit tune off`: the echo of a tune=0 if one is
    /// waiting, else the tuner letting go of a sweep it started.
    Answer tuneOff(qint64 nowMs)
    {
        expire(nowMs);
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->kind == Kind::TuneOffEcho) {
                const Answer answer{it->kind, nowMs - it->sentMs};
                m_entries.erase(it);
                return answer;
            }
        }
        endStartedSweeps();
        return {};
    }

    /// Entries still waiting for a tune on.
    int awaitingTuneOn(qint64 nowMs)
    {
        expire(nowMs);
        int n = 0;
        for (const Entry& e : m_entries) {
            if (e.kind != Kind::TuneOffEcho) {
                ++n;
            }
        }
        return n;
    }

    static const char* kindName(Kind kind)
    {
        switch (kind) {
        case Kind::None:          return "none";
        case Kind::Autotune:      return "autotune";
        case Kind::CycleAutotune: return "cycle autotune";
        case Kind::TuneOnEcho:    return "tune=1 echo";
        case Kind::TuneOffEcho:   return "tune=0 echo";
        }
        return "none";
    }

private:
    struct Entry {
        Kind kind;
        quint64 epoch;
        quint32 seq;
        qint64 sentMs;
        bool sweepStarted;
    };

    static bool isAutotune(Kind kind)
    {
        return kind == Kind::Autotune || kind == Kind::CycleAutotune;
    }

    void expire(qint64 nowMs)
    {
        for (auto it = m_entries.begin(); it != m_entries.end();) {
            if (nowMs - it->sentMs > kAnswerWindowMs) {
                it = m_entries.erase(it);
            } else {
                ++it;
            }
        }
    }

    void endStartedSweeps()
    {
        for (auto it = m_entries.begin(); it != m_entries.end();) {
            if (isAutotune(it->kind) && it->sweepStarted) {
                it = m_entries.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::vector<Entry> m_entries;
};

} // namespace NereusSDR
