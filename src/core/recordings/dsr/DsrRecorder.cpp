// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original Digital Spectrum Recorder.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; the Digital Spectrum Recorder (spec 3,
//             6.1, 6.7, A10, A17). AI tooling: Anthropic Claude Code.
// 2026-11-17  J.J. Boyd / KG4VCF; drainLane checks a lane's time against
//             the clock as signal arrives and starts it again from the
//             clock when they part by more than kClockToleranceNs (a
//             receiver closed and reopened kept the time it closed at).
//             AI tooling: Anthropic Claude Code.

#include "DsrRecorder.h"

#include "core/LogCategories.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <thread>

namespace NereusSDR {

namespace {

// The worker takes at most this many frames from a ring at once.
constexpr int kScratchFrames = 65536;
constexpr qint64 kNsPerMs = 1000000;

qint16 toSample(float v)
{
    const float scaled = std::clamp(v * DsrRecorder::kIqScale, -DsrRecorder::kIqScale, DsrRecorder::kIqScale);
    return static_cast<qint16>(std::lrint(scaled));
}

}  // namespace

DsrRecorder::DsrRecorder() = default;

DsrRecorder::~DsrRecorder()
{
    setAccepting(false);
}

void DsrRecorder::setStream(int receiver, qint64 centreHz, int sampleRateHz)
{
    if (receiver < 0 || receiver >= kMaxReceivers) {
        return;
    }
    Lane& lane = m_lanes[static_cast<size_t>(receiver)];
    lane.centreHz.store(centreHz, std::memory_order_release);
    lane.rateHz.store(std::max(0, sampleRateHz), std::memory_order_release);
    if (sampleRateHz > 0 && !lane.owned) {
        // Made once and kept until the recorder goes, so a tap never sees
        // its ring freed.
        lane.owned = std::make_unique<SpscFloatRing>(kIqRingFloats);
        lane.ring.store(lane.owned.get(), std::memory_order_release);
    }
}

void DsrRecorder::setTransmitting(bool on)
{
    m_transmitting.store(on, std::memory_order_release);
    if (on) {
        m_transmitSeen.store(true, std::memory_order_release);
    }
}

void DsrRecorder::setAccepting(bool on)
{
    m_accepting.store(on, std::memory_order_seq_cst);
    if (!on) {
        while (m_tapsInFlight.load(std::memory_order_seq_cst) != 0) {
            std::this_thread::yield();
        }
    }
}

quint64 DsrRecorder::overflows() const
{
    quint64 total = m_txOverflows.load(std::memory_order_relaxed);
    for (const Lane& lane : m_lanes) {
        total += lane.overflows.load(std::memory_order_relaxed);
    }
    return total;
}

quint64 DsrRecorder::overflows(int receiver) const
{
    if (receiver < 0 || receiver >= kMaxReceivers) {
        return 0;
    }
    return m_lanes[static_cast<size_t>(receiver)].overflows.load(std::memory_order_relaxed);
}

// ---- Worker thread.

void DsrRecorder::resetLanes()
{
    for (Lane& lane : m_lanes) {
        lane.held = 0;
        lane.transmit = false;
        lane.anchored = false;
        lane.seenOverflows = lane.overflows.load(std::memory_order_relaxed);
    }
    m_txHeld.clear();
    m_txHeldRateHz = 0;
}

bool DsrRecorder::begin(const QString& path, qint64 sizeLimitBytes, QString* error)
{
    resetLanes();
    m_blocksWritten = 0;
    if (m_scratch.size() < static_cast<size_t>(kScratchFrames) * 2) {
        m_scratch.assign(static_cast<size_t>(kScratchFrames) * 2, 0.0f);
    }
    return m_ring.open(path, sizeLimitBytes, error);
}

bool DsrRecorder::drain(qint64 nowUtcNs)
{
    if (!m_ring.isOpen()) {
        return true;
    }
    drainTransmitAudio();
    const bool transmitting = m_transmitSeen.exchange(false, std::memory_order_acq_rel)
                              || m_transmitting.load(std::memory_order_acquire);
    for (int r = 0; r < kMaxReceivers; ++r) {
        if (!drainLane(r, m_lanes[static_cast<size_t>(r)], nowUtcNs, transmitting)) {
            abandon();
            return false;
        }
    }
    return true;
}

bool DsrRecorder::finish(qint64 nowUtcNs)
{
    if (!m_ring.isOpen()) {
        return true;
    }
    if (!drain(nowUtcNs)) {
        return false;
    }
    for (int r = 0; r < kMaxReceivers; ++r) {
        if (!writeHeld(r, m_lanes[static_cast<size_t>(r)])) {
            abandon();
            return false;
        }
    }
    m_lastFigures = liveFigures();
    m_ring.close();
    resetLanes();
    return true;
}

void DsrRecorder::abandon()
{
    if (m_ring.isOpen()) {
        m_lastFigures = liveFigures();
    }
    m_ring.close();
    resetLanes();
}

void DsrRecorder::drainTransmitAudio()
{
    const int rate = m_txRateHz.load(std::memory_order_acquire);
    if (rate != m_txHeldRateHz) {
        m_txHeld.clear();
        m_txHeldRateHz = rate;
    }
    const size_t cap = static_cast<size_t>(std::max(0, rate)) * kTxAudioHeldSeconds;
    for (;;) {
        const int got = m_txRing.pop(m_scratch.data(), static_cast<int>(m_scratch.size()));
        if (got <= 0) {
            break;
        }
        for (int i = 0; i < got; ++i) {
            m_txHeld.push_back(toSample(m_scratch[static_cast<size_t>(i)]));
        }
    }
    if (m_txHeld.size() > cap) {
        // Nothing to keep it beside for that long: the oldest goes.
        m_txHeld.erase(m_txHeld.begin(), m_txHeld.begin() + static_cast<std::ptrdiff_t>(m_txHeld.size() - cap));
    }
}

bool DsrRecorder::drainLane(int receiver, Lane& lane, qint64 nowUtcNs, bool transmitting)
{
    SpscFloatRing* ring = lane.ring.load(std::memory_order_acquire);
    if (ring == nullptr) {
        return true;
    }
    const int rate = lane.rateHz.load(std::memory_order_acquire);
    const qint64 centre = lane.centreHz.load(std::memory_order_acquire);

    // The tap dropped samples: what is held ends there, and the time
    // starts again from the clock.
    const quint64 overflowed = lane.overflows.load(std::memory_order_relaxed);
    if (overflowed != lane.seenOverflows) {
        if (!writeHeld(receiver, lane)) {
            return false;
        }
        lane.anchored = false;
        lane.seenOverflows = overflowed;
    }
    if (rate <= 0) {
        // Not streaming: anything left over is not kept.
        if (!writeHeld(receiver, lane)) {
            return false;
        }
        while (ring->pop(m_scratch.data(), static_cast<int>(m_scratch.size())) > 0) {
        }
        lane.anchored = false;
        return true;
    }
    // A new rate or centre ends the block; the time runs on.
    if (rate != lane.rate || centre != lane.centre) {
        if (!writeHeld(receiver, lane)) {
            return false;
        }
        lane.rate = rate;
        lane.centre = centre;
        if (lane.block.size() < static_cast<size_t>(rate) * 2) {
            lane.block.assign(static_cast<size_t>(rate) * 2, 0);
        }
    }

    qint64 waiting = ring->available() / 2;
    if (waiting <= 0) {
        return true;
    }
    if (lane.anchored) {
        // Where the samples counted so far say the waiting frames start,
        // and where the clock says they do. A receiver closed and reopened
        // at the same rate and centre sends nothing in between, so the
        // count carries on from when it closed: what is held ends there,
        // and the time starts again from the clock. Either direction.
        const qint64 byCount = lane.startNs + DsrRingFile::durationNs(lane.held, rate);
        const qint64 byClock = nowUtcNs - DsrRingFile::durationNs(waiting, rate);
        if (std::llabs(byCount - byClock) > kClockToleranceNs) {
            if (!writeHeld(receiver, lane)) {
                return false;
            }
            lane.anchored = false;
        }
    }
    if (!lane.anchored) {
        // The first frame waiting came that long before now.
        lane.startNs = nowUtcNs - DsrRingFile::durationNs(waiting, rate);
        lane.held = 0;
        lane.transmit = false;
        lane.anchored = true;
    }
    while (waiting > 0) {
        const int want = static_cast<int>(std::min<qint64>({waiting, qint64(rate - lane.held), kScratchFrames}));
        const int floats = ring->pop(m_scratch.data(), want * 2);
        const int frames = floats / 2;
        if (frames <= 0) {
            break;
        }
        qint16* out = lane.block.data() + static_cast<size_t>(lane.held) * 2;
        for (int i = 0; i < frames * 2; ++i) {
            out[i] = toSample(m_scratch[static_cast<size_t>(i)]);
        }
        lane.held += frames;
        waiting -= frames;
        // Only the block these frames went into is marked.
        if (transmitting) {
            lane.transmit = true;
        }
        if (lane.held >= rate) {
            if (!writeHeld(receiver, lane)) {
                return false;
            }
        }
    }
    return true;
}

bool DsrRecorder::writeHeld(int receiver, Lane& lane)
{
    if (lane.held <= 0 || lane.rate <= 0) {
        lane.held = 0;
        return true;
    }
    DsrBlockHeader h;
    h.receiver = static_cast<quint16>(receiver);
    h.utcNs = lane.startNs;
    h.sampleRateHz = lane.rate;
    h.centreHz = lane.centre;
    h.transmit = lane.transmit ? 1 : 0;
    h.frames = static_cast<quint32>(lane.held);
    // Your transmit audio goes beside the first block written after it.
    const bool withAudio = !m_txHeld.empty() && m_txHeldRateHz > 0;
    const bool ok = m_ring.append(h, lane.block.data(), lane.held, withAudio ? m_txHeld.data() : nullptr,
                                  withAudio ? static_cast<int>(m_txHeld.size()) : 0,
                                  withAudio ? m_txHeldRateHz : 0);
    if (!ok) {
        qCWarning(lcRecordings) << "the spectrum recorder cannot write receiver" << receiver;
        return false;
    }
    if (withAudio) {
        m_txHeld.clear();
    }
    ++m_blocksWritten;
    lane.startNs += DsrRingFile::durationNs(lane.held, lane.rate);
    lane.held = 0;
    lane.transmit = false;
    return true;
}

DsrStatus DsrRecorder::figures() const
{
    DsrStatus st = m_ring.isOpen() ? liveFigures() : m_lastFigures;
    st.overflows = overflows();
    return st;
}

DsrStatus DsrRecorder::liveFigures() const
{
    DsrStatus st;
    st.sizeLimitBytes = m_ring.sizeLimitBytes();
    st.usedBytes = m_ring.usedBytes();
    st.writePosition = m_ring.writePosition();
    st.oldestUtcMs = m_ring.oldestUtcNs() / kNsPerMs;
    st.newestUtcMs = m_ring.newestUtcNs() / kNsPerMs;
    return st;
}

}  // namespace NereusSDR
