// =================================================================
// src/core/HermesLiteBandwidthMonitor.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis sources:
//   Project Files/Source/ChannelMaster/bandwidth_monitor.h
//   Project Files/Source/ChannelMaster/bandwidth_monitor.c
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Closes Phase 3I-T12 deferred work
//                (P1RadioConnection.cpp — hl2CheckBandwidthMonitor stub).
//                Detects HL2 LAN PHY throttling via ep6 ingress byte-rate
//                analysis, faithfully porting upstream compute_bps() using
//                std::atomic<int64_t> in place of Windows InterlockedAdd64.
// =================================================================

// --- From bandwidth_monitor.h ---
/*  bandwidth_monitor.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2025 Richard Samphire, MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk

*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From bandwidth_monitor.c ---
/*  bandwidth_monitor.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2025 Richard Samphire, MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk

*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "HermesLiteBandwidthMonitor.h"

#include <chrono>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// nowMs — portable replacement for upstream GetTickCount64().
// Source: mi0bot bandwidth_monitor.c:54-57 now_ms() [@c26a8a4]
// Original:
//   static int64_t now_ms(void) { return (int64_t)GetTickCount64(); }
// ---------------------------------------------------------------------------
// static
int64_t HermesLiteBandwidthMonitor::nowMs()
{
    using namespace std::chrono;
    return static_cast<int64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// ---------------------------------------------------------------------------
HermesLiteBandwidthMonitor::HermesLiteBandwidthMonitor(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// recordEp6Bytes — mirror of bandwidth_monitor_in(int bytes).
// Source: mi0bot bandwidth_monitor.c:74-78 [@c26a8a4]
// Original:
//   void bandwidth_monitor_in(int bytes) {
//       if (bytes > 0)
//           InterlockedAdd64(&in_total_bytes, (LONG64)bytes);
//   }
// ---------------------------------------------------------------------------
void HermesLiteBandwidthMonitor::recordEp6Bytes(int bytes)
{
    if (bytes > 0) {
        m_inTotalBytes.fetch_add(static_cast<int64_t>(bytes), std::memory_order_relaxed);
    }
}

// ---------------------------------------------------------------------------
// recordEp2Bytes — mirror of bandwidth_monitor_out(int bytes).
// Source: mi0bot bandwidth_monitor.c:80-84 [@c26a8a4]
// Original:
//   void bandwidth_monitor_out(int bytes) {
//       if (bytes > 0)
//           InterlockedAdd64(&out_total_bytes, (LONG64)bytes);
//   }
// ---------------------------------------------------------------------------
void HermesLiteBandwidthMonitor::recordEp2Bytes(int bytes)
{
    if (bytes > 0) {
        m_outTotalBytes.fetch_add(static_cast<int64_t>(bytes), std::memory_order_relaxed);
    }
}

// ---------------------------------------------------------------------------
// computeBps — faithful port of upstream compute_bps() static function.
// Source: mi0bot bandwidth_monitor.c:86-113 [@c26a8a4]
// Original (Windows):
//   static double compute_bps(volatile LONG64* total_bytes,
//                             volatile LONG64* last_bytes,
//                             volatile LONG64* last_ms,
//                             volatile double* last_bps)
//   {
//       int64_t t = now_ms();
//       int64_t total = InterlockedAdd64(total_bytes, 0);
//       int64_t prev_bytes = InterlockedAdd64(last_bytes, 0);
//       int64_t prev_ms    = InterlockedAdd64(last_ms, 0);
//       if (prev_ms == 0) {
//           InterlockedExchange64(last_bytes, total);
//           InterlockedExchange64(last_ms, t);
//           *last_bps = 0.0;
//           return 0.0;
//       }
//       int64_t dt = t - prev_ms;
//       if (dt <= 0) return *last_bps;
//       int64_t db = total - prev_bytes;
//       double bps = (double)db * 1000.0 / (double)dt;
//       InterlockedExchange64(last_bytes, total);
//       InterlockedExchange64(last_ms, t);
//       *last_bps = bps;
//       return bps;
//   }
// NereusSDR: InterlockedAdd64(x, 0) → x.load(); InterlockedExchange64 → x.store().
// ---------------------------------------------------------------------------
double HermesLiteBandwidthMonitor::computeBps(std::atomic<int64_t>& totalBytes,
                                              std::atomic<int64_t>& lastBytes,
                                              std::atomic<int64_t>& lastMs,
                                              double&               lastBps) const
{
    const int64_t t          = nowMs();
    const int64_t total      = totalBytes.load(std::memory_order_relaxed);
    const int64_t prevBytes  = lastBytes.load(std::memory_order_relaxed);
    const int64_t prevMs     = lastMs.load(std::memory_order_relaxed);

    if (prevMs == 0) {
        lastBytes.store(total,  std::memory_order_relaxed);
        lastMs.store(t,         std::memory_order_relaxed);
        lastBps = 0.0;
        return 0.0;
    }

    const int64_t dt = t - prevMs;
    if (dt <= 0) { return lastBps; }

    const int64_t db = total - prevBytes;
    // Source: bandwidth_monitor.c:106 — bps = (double)db * 1000.0 / (double)dt
    const double bps = static_cast<double>(db) * 1000.0 / static_cast<double>(dt);

    lastBytes.store(total, std::memory_order_relaxed);
    lastMs.store(t,        std::memory_order_relaxed);
    lastBps = bps;
    return bps;
}

// ---------------------------------------------------------------------------
// tick — called from P1RadioConnection::onWatchdogTick() on every watchdog fire.
//
// 1. Compute ingress/egress rates via the upstream compute_bps() algorithm.
//    (Mirrors what the upstream caller would do: call GetInboundBps() +
//    GetOutboundBps() on a poll timer and compare against thresholds.)
//
// 2. Run NereusSDR's throttle-detection state machine:
//    - ep6 ingress rate drops to ~0 while ep2 egress is active → silent tick
//    - kThrottleTickThreshold consecutive silent ticks → throttle asserted
//    - any tick where ep6 ingress is active → clear throttle + silent counter
//
// Source: mi0bot bandwidth_monitor.c:115-123 GetInboundBps/GetOutboundBps [@c26a8a4]
// Throttle state machine: NereusSDR design — no upstream equivalent.
// ---------------------------------------------------------------------------
void HermesLiteBandwidthMonitor::tick()
{
    // Compute byte rates — matches what upstream GetInboundBps/GetOutboundBps
    // do when polled from a periodic timer.
    // Source: mi0bot bandwidth_monitor.c:115-123 [@c26a8a4]
    const double ep6Bps = computeBps(m_inTotalBytes,  m_inLastBytes,  m_inLastMs,  m_inLastBps);
    const double ep2Bps = computeBps(m_outTotalBytes, m_outLastBytes, m_outLastMs, m_outLastBps);

    // Throttle detection — NereusSDR layer (no upstream equivalent).
    // ep6 silent: ingress rate below 1 byte/sec (0 frames delivered this tick).
    // ep2 active: egress rate above 1 byte/sec (host is still sending commands).
    const bool ep6Silent = (ep6Bps < 1.0);
    const bool ep2Active = (ep2Bps > 1.0);

    if (ep6Silent && ep2Active) {
        ++m_silentTicks;
    } else {
        m_silentTicks = 0;
    }

    const bool nowThrottled = (m_silentTicks >= kThrottleTickThreshold);
    if (nowThrottled != m_throttled) {
        m_throttled = nowThrottled;
        if (m_throttled) { ++m_throttleEventCount; }
        emit throttledChanged(m_throttled);
    }

    emit liveStatsUpdated(ep6Bps, ep2Bps, m_throttleEventCount);
}

// ---------------------------------------------------------------------------
// reset — mirror of bandwidth_monitor_reset().
// Source: mi0bot bandwidth_monitor.c:59-72 [@c26a8a4]
// Original:
//   InterlockedExchange64(&in_total_bytes, 0);  ... (all six fields)
//   in_last_bps = 0.0; out_last_bps = 0.0;
// ---------------------------------------------------------------------------
void HermesLiteBandwidthMonitor::reset()
{
    // Source: mi0bot bandwidth_monitor.c:61-70 — InterlockedExchange64 all fields [@c26a8a4]
    m_inTotalBytes.store(0,  std::memory_order_relaxed);
    m_outTotalBytes.store(0, std::memory_order_relaxed);
    m_inLastBytes.store(0,   std::memory_order_relaxed);
    m_outLastBytes.store(0,  std::memory_order_relaxed);
    m_inLastMs.store(0,      std::memory_order_relaxed);
    m_outLastMs.store(0,     std::memory_order_relaxed);
    // Source: mi0bot bandwidth_monitor.c:71-72 [@c26a8a4]
    m_inLastBps  = 0.0;
    m_outLastBps = 0.0;

    // NereusSDR throttle state (no upstream equivalent).
    m_silentTicks       = 0;
    m_throttleEventCount = 0;
    if (m_throttled) {
        m_throttled = false;
        emit throttledChanged(false);
    }
}

// ---------------------------------------------------------------------------
// Accessors — map to GetInboundBps / GetOutboundBps.
// Source: mi0bot bandwidth_monitor.c:115-123 [@c26a8a4]
// In the upstream these recompute on every call; here we cache the last
// tick value so callers pay no extra computation cost between ticks.
// ---------------------------------------------------------------------------
double HermesLiteBandwidthMonitor::ep6IngressBytesPerSec() const { return m_inLastBps; }
double HermesLiteBandwidthMonitor::ep2EgressBytesPerSec()  const { return m_outLastBps; }

bool   HermesLiteBandwidthMonitor::isThrottled()       const { return m_throttled; }
int    HermesLiteBandwidthMonitor::throttleEventCount() const { return m_throttleEventCount; }

} // namespace NereusSDR
