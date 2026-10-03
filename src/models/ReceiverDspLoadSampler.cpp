// =================================================================
// src/models/ReceiverDspLoadSampler.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Measurement bookkeeping for the R3 DSP
// overload work; no Thetis counterpart. See ReceiverDspLoadSampler.h.
//
// Modification history (NereusSDR):
//   2026-09-23 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code (R-R3-40).
//                 Later the same day: first-reading baseline and forget().
//                 Later the same day: busy time over wall time between two
//                 reads replaces mean block / period and the "time so far
//                 over one period" rule for a late block in progress
//                 (R-R3-40, R-R3-37).
//                 Later the same day: a read that may be torn keeps the
//                 slice's baseline and snapshot (R-R3-40).
// =================================================================

#include "models/ReceiverDspLoadSampler.h"

#include <algorithm>

namespace NereusSDR {

ReceiverDspLoad ReceiverDspLoadSampler::compute(const Reading& now,
                                                const Baseline& previous)
{
    ReceiverDspLoad out;
    const qint64 blocks = now.blocks - previous.blocks;
    const qint64 currentBlockUs = now.currentBlockNs / 1000;

    out.lateBlocks = now.lateBlocks - previous.lateBlocks;
    out.maxBlockUs = std::max(now.intervalMaxBlockUs, currentBlockUs);
    out.lifetimeMaxBlockUs = std::max(now.lifetimeMaxBlockUs, currentBlockUs);
    out.inputDelayMs = now.inputDelayMs;
    out.droppedInputMs = now.droppedInputMs;

    if (blocks <= 0 && now.currentBlockNs <= 0) {
        out.idle = true;
        return out;
    }
    // Busy time over wall time. Each read's busyNs + currentBlockNs is the
    // worker's time inside blocks up to its readNs (dsplock.c reads them as
    // one instant's pair), so the difference is the time it spent inside
    // blocks during the interval, counting a block still running only for
    // the part of it that fell in the interval.
    const qint64 wallNs = now.readNs - previous.readNs;
    if (wallNs <= 0) {
        return out;
    }
    const qint64 busyNs = (now.busyNs + now.currentBlockNs) - previous.busyToReadNs;
    out.load = static_cast<double>(std::max<qint64>(0, busyNs)) / static_cast<double>(wallNs);
    return out;
}

void ReceiverDspLoadSampler::update(const QHash<int, Reading>& readings)
{
    QHash<int, Baseline> baselines;
    QHash<int, ReceiverDspLoad> snapshots;
    baselines.reserve(readings.size());
    snapshots.reserve(readings.size());

    for (auto it = readings.constBegin(); it != readings.constEnd(); ++it) {
        const Reading& now = it.value();
        // A read that may be torn measures nothing: keep what the slice had.
        if (!now.consistent) {
            const auto kept = m_baselines.constFind(it.key());
            if (kept != m_baselines.constEnd()) {
                baselines.insert(it.key(), *kept);
                const auto shown = m_snapshots.constFind(it.key());
                if (shown != m_snapshots.constEnd()) {
                    snapshots.insert(it.key(), *shown);
                }
            }
            continue;
        }
        const Baseline current{now.blocks, now.busyNs, now.lateBlocks,
                               now.busyNs + now.currentBlockNs, now.readNs};
        baselines.insert(it.key(), current);
        const auto previous = m_baselines.constFind(it.key());
        // The first reading for a slice seeds its baseline and publishes
        // nothing. dsplock.c keeps each channel id's counters for the whole
        // process, so they may already hold an earlier slice's history.
        if (previous == m_baselines.constEnd()) {
            continue;
        }
        // The counters never go backwards in production, since dsplock.c
        // never resets them. Should one ever do so, the interval is unknown:
        // start again from this reading rather than report a guess.
        if (now.blocks < previous->blocks || now.busyNs < previous->busyNs
            || now.lateBlocks < previous->lateBlocks || now.readNs < previous->readNs) {
            continue;
        }
        snapshots.insert(it.key(), compute(now, *previous));
    }

    m_baselines = std::move(baselines);
    m_snapshots = std::move(snapshots);
}

std::optional<ReceiverDspLoad> ReceiverDspLoadSampler::snapshot(int sliceId) const
{
    const auto it = m_snapshots.constFind(sliceId);
    if (it == m_snapshots.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

void ReceiverDspLoadSampler::forget(int sliceId)
{
    m_baselines.remove(sliceId);
    m_snapshots.remove(sliceId);
}

void ReceiverDspLoadSampler::clear()
{
    m_baselines.clear();
    m_snapshots.clear();
}

} // namespace NereusSDR
