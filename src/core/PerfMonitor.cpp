// =================================================================
// src/core/PerfMonitor.cpp  (NereusSDR-native)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// See PerfMonitor.h for design rationale.
// =================================================================
#include "PerfMonitor.h"

namespace NereusSDR {

PerfMonitor& PerfMonitor::instance()
{
    static PerfMonitor s;
    return s;
}

void PerfMonitor::recordPaintFrame(double ms)
{
    m_paintRing.push(ms);
}

void PerfMonitor::recordInterFrameGap(double ms)
{
    m_gapRing.push(ms);
}

void PerfMonitor::recordFftCompute(double ms)
{
    m_fftRing.push(ms);
}

void PerfMonitor::recordOverlayRebuild(double ms)
{
    m_ovlyRing.push(ms);
}

void PerfMonitor::recordAudioFillMs(double ms)
{
    m_audioFillRing.push(ms);
}

void PerfMonitor::incAudioUnderrun()
{
    m_audioUnderrunsTotal.fetch_add(1, std::memory_order_relaxed);
    m_audioUnderrunsDelta.fetch_add(1, std::memory_order_relaxed);
}

void PerfMonitor::incUdpDrop()
{
    m_udpDropsTotal.fetch_add(1, std::memory_order_relaxed);
    m_udpDropsDelta.fetch_add(1, std::memory_order_relaxed);
}

void PerfMonitor::setMemoryStats(bool compressing, double footprintMb)
{
    QMutexLocker lock(&m_memMtx);
    m_memCompressing = compressing;
    m_memFootprintMb = footprintMb;
}

PerfMonitor::Snapshot PerfMonitor::snapshotAndClearDeltas()
{
    Snapshot s;
    double dummyMin = 0.0;
    m_paintRing.stats(s.paintMsAvg, s.paintMsMax, dummyMin, s.paintSamples);
    m_gapRing.stats(s.gapMsAvg, s.gapMsMax, dummyMin, s.gapSamples);
    m_fftRing.stats(s.fftMsAvg, s.fftMsMax, dummyMin, s.fftSamples);
    m_ovlyRing.stats(s.ovlyMsAvg, s.ovlyMsMax, dummyMin, s.ovlySamples);
    m_audioFillRing.stats(s.audioFillAvgMs, s.audioFillMaxMs,
                          s.audioFillMinMs, s.audioFillSamples);

    s.audioUnderrunsTotal = m_audioUnderrunsTotal.load(std::memory_order_relaxed);
    s.audioUnderrunsDelta = m_audioUnderrunsDelta.exchange(0, std::memory_order_relaxed);
    s.udpDropsTotal = m_udpDropsTotal.load(std::memory_order_relaxed);
    s.udpDropsDelta = m_udpDropsDelta.exchange(0, std::memory_order_relaxed);

    {
        QMutexLocker lock(&m_memMtx);
        s.memCompressing = m_memCompressing;
        s.memFootprintMb = m_memFootprintMb;
    }
    {
        QMutexLocker lock(&m_lastSnapshotMtx);
        m_lastSnapshot = s;
    }
    return s;
}

PerfMonitor::Snapshot PerfMonitor::lastSnapshot() const
{
    QMutexLocker lock(&m_lastSnapshotMtx);
    return m_lastSnapshot;
}

void PerfMonitor::resetAll()
{
    m_paintRing.clear();
    m_gapRing.clear();
    m_fftRing.clear();
    m_ovlyRing.clear();
    m_audioFillRing.clear();
    m_audioUnderrunsTotal.store(0, std::memory_order_relaxed);
    m_audioUnderrunsDelta.store(0, std::memory_order_relaxed);
    m_udpDropsTotal.store(0, std::memory_order_relaxed);
    m_udpDropsDelta.store(0, std::memory_order_relaxed);
    QMutexLocker lock(&m_memMtx);
    m_memCompressing = false;
    m_memFootprintMb = 0.0;
}

} // namespace NereusSDR
