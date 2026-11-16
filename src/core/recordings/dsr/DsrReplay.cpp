// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original Digital Spectrum Recorder rewind.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; rewind sound (spec 3, 6.7, recording plan
//             Task 22). AI tooling: Anthropic Claude Code.

#include "DsrReplay.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace NereusSDR {

DsrReplay::DsrReplay(int slice, int receiver, int sampleRateHz, qint64 fromUtcNs, qint64 liveCentreHz)
    : m_slice(slice)
    , m_receiver(receiver)
    , m_rate(sampleRateHz)
    , m_fromUtcNs(fromUtcNs)
    , m_liveCentreHz(liveCentreHz)
    , m_readEndUtcNs(fromUtcNs)
    , m_nextUtcNs(fromUtcNs)
    , m_read(static_cast<size_t>(kReadFrames) * 2)
    , m_push(static_cast<size_t>(kReadFrames) * 2)
    , m_pull(static_cast<size_t>(kMaxPullFrames) * 2)
{
}

qint64 DsrReplay::positionUtcNs() const
{
    const qint64 waitingFrames = m_ring.available() / 2;
    return m_readEndUtcNs.load(std::memory_order_acquire) - DsrRingFile::durationNs(waitingFrames, m_rate);
}

void DsrReplay::fill(const DsrRingFile& ring)
{
    if (m_rate <= 0 || m_finished.load(std::memory_order_acquire)) {
        return;
    }
    const int capacityFrames = m_ring.capacity() / 2 - kReadFrames;
    const int target = std::min(capacityFrames, int(qint64(m_rate) * kReadAheadMs / 1000));
    bool atNewest = false;
    // Every pass either pushes frames or moves to the next block, so this
    // bound only guards against a ring file that changes under the reads.
    for (int pass = 0; pass < 256; ++pass) {
        const int waiting = m_ring.available() / 2;
        if (waiting >= target) {
            break;
        }
        if (!m_haveBlock) {
            DsrBlockHeader h;
            if (!ring.blockAt(m_receiver, m_nextUtcNs, &h) && !ring.nextBlock(m_receiver, m_nextUtcNs, &h)) {
                // Past the newest second kept now; it may come on the next
                // round.
                atNewest = true;
                break;
            }
            if (h.sampleRateHz != m_rate) {
                // The receiver's rate changed here: the slice's demodulator
                // runs at the old one, so the past ends here.
                atNewest = true;
                break;
            }
            m_block = h;
            m_haveBlock = true;
            const qint64 from = std::max(m_nextUtcNs, h.utcNs);
            m_frameInBlock = std::clamp<qint64>((from - h.utcNs) * m_rate / DsrRingFile::kNsPerSecond, 0,
                                                qint64(h.frames));
        }
        const qint64 left = qint64(m_block.frames) - m_frameInBlock;
        const int want = int(std::min<qint64>({qint64(kReadFrames), qint64(target - waiting), left}));
        DsrBlockHeader got;
        const int n = want > 0 ? ring.read(m_receiver, DsrRingFile::frameUtcNs(m_block.utcNs, m_frameInBlock, m_rate),
                                           m_read.data(), want, &got)
                               : 0;
        if (n <= 0) {
            // The block went (the loop wrote over it) or is done.
            m_haveBlock = false;
            m_nextUtcNs = m_block.utcNs + DsrRingFile::durationNs(m_block.frames, m_rate);
            continue;
        }
        push(m_read.data(), n, m_block.centreHz);
        m_frameInBlock += n;
        m_nextUtcNs = DsrRingFile::frameUtcNs(m_block.utcNs, m_frameInBlock, m_rate);
        if (m_frameInBlock >= qint64(m_block.frames)) {
            m_haveBlock = false;
            m_nextUtcNs = m_block.utcNs + DsrRingFile::durationNs(m_block.frames, m_rate);
        }
        m_readEndUtcNs.store(m_nextUtcNs, std::memory_order_release);
    }
    m_atNewest.store(atNewest, std::memory_order_release);
    m_primed.store(true, std::memory_order_release);
}

void DsrReplay::push(const qint16* iq, int frames, qint64 keptCentreHz)
{
    // A signal at RF f was kept at baseband f - keptCentre; the slice's
    // shift looks for it at f - liveCentre. Multiplying by
    // e^(j 2 pi (keptCentre - liveCentre) n / rate) moves it there.
    const double offsetHz = double(keptCentreHz - m_liveCentreHz.load(std::memory_order_acquire));
    float* out = m_push.data();
    if (offsetHz == 0.0) {
        for (int k = 0; k < frames * 2; ++k) {
            out[k] = float(iq[k]) / kIqScale;
        }
    } else {
        const double step = 2.0 * std::numbers::pi * offsetHz / double(m_rate);
        const double stepRe = std::cos(step);
        const double stepIm = std::sin(step);
        double pr = m_phaseRe;
        double pIm = m_phaseIm;
        for (int k = 0; k < frames; ++k) {
            const double i = double(iq[2 * k]) / kIqScale;
            const double q = double(iq[2 * k + 1]) / kIqScale;
            out[2 * k] = float(i * pr - q * pIm);
            out[2 * k + 1] = float(i * pIm + q * pr);
            const double nr = pr * stepRe - pIm * stepIm;
            pIm = pr * stepIm + pIm * stepRe;
            pr = nr;
        }
        // Back to unit length, so rounding never grows the signal.
        const double mag = std::hypot(pr, pIm);
        m_phaseRe = mag > 0.0 ? pr / mag : 1.0;
        m_phaseIm = mag > 0.0 ? pIm / mag : 0.0;
    }
    // fill() keeps the ring below its capacity, so this fits.
    m_ring.tryPush(out, frames * 2);
}

bool DsrReplay::pull(float* i, float* q, int frames) noexcept
{
    if (m_finished.load(std::memory_order_acquire) || frames <= 0 || frames > kMaxPullFrames) {
        return false;
    }
    const int waiting = m_ring.available() / 2;
    if (waiting < frames && m_atNewest.load(std::memory_order_acquire)) {
        // Played up to the newest second kept: the radio's I/Q from here.
        m_finished.store(true, std::memory_order_release);
        return false;
    }
    const int take = std::min(waiting, frames);
    const int got = m_ring.pop(m_pull.data(), take * 2) / 2;
    for (int k = 0; k < got; ++k) {
        i[k] = m_pull[static_cast<size_t>(2 * k)];
        q[k] = m_pull[static_cast<size_t>(2 * k + 1)];
    }
    if (got < frames) {
        // The worker is late: silence for the rest, never the radio's I/Q.
        std::fill(i + got, i + frames, 0.0f);
        std::fill(q + got, q + frames, 0.0f);
        m_underruns.fetch_add(1, std::memory_order_relaxed);
    }
    m_framesPlayed.fetch_add(got, std::memory_order_relaxed);
    return true;
}

}  // namespace NereusSDR
