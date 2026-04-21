// =================================================================
// src/core/HermesLiteBandwidthMonitor.h  (NereusSDR)
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

#pragma once

#include <QObject>
#include <atomic>

namespace NereusSDR {

// HL2 LAN PHY byte-rate monitor + throttle detector.
//
// Ports mi0bot bandwidth_monitor.{c,h} (MW0LGE) from Windows-specific
// InterlockedAdd64 / GetTickCount64 to std::atomic<int64_t> /
// std::chrono::steady_clock, preserving the compute_bps() algorithm
// byte-for-byte.
//
// The upstream does NOT implement throttle detection — it is a byte-rate
// telemetry helper that callers compare against an expected rate.
// NereusSDR adds a throttle-detection layer (ep6-ingress goes silent while
// ep2-egress continues) on top of the faithful upstream port.
//
// Caller responsibilities:
// - Call recordEp6Bytes(n) on every ep6 datagram received (ingress).
// - Call recordEp2Bytes(n) on every ep2 datagram sent (egress).
// - Call tick() at regular intervals (from P1RadioConnection::onWatchdogTick).
//   tick() drives both rate computation (via the upstream compute_bps logic)
//   and the throttle state machine.
//
// Both recordEp6Bytes and recordEp2Bytes are called from the connection
// thread; tick() is also called from the connection thread (watchdog).
// All methods are therefore single-threaded in the current design — the
// std::atomic members are a faithful port of the upstream LONG64 volatiles
// and provide the same forward-progress guarantee without Windows-specific
// primitives.
//
// Source: mi0bot bandwidth_monitor.{h,c} [@c26a8a4]
class HermesLiteBandwidthMonitor : public QObject {
    Q_OBJECT

public:
    // Number of consecutive silent ticks (ep6 rate ≈ 0 while ep2 active)
    // required to assert the throttle flag.
    // NereusSDR heuristic — no upstream equivalent (upstream has no throttle).
    static constexpr int kThrottleTickThreshold = 3;

    explicit HermesLiteBandwidthMonitor(QObject* parent = nullptr);

    // Per-frame byte recording.
    // Source: mi0bot bandwidth_monitor.c:74-84 bandwidth_monitor_in/out() [@c26a8a4]
    // Original: InterlockedAdd64(&in_total_bytes, (LONG64)bytes)
    void recordEp6Bytes(int bytes);   // mirrors bandwidth_monitor_in()
    void recordEp2Bytes(int bytes);   // mirrors bandwidth_monitor_out()

    // Per-tick evaluation. Computes ingress/egress byte rates using the upstream
    // compute_bps() algorithm, then updates throttle state and emits signals on
    // change. Called from P1RadioConnection::onWatchdogTick().
    // Source: mi0bot bandwidth_monitor.c:86-113 compute_bps() [@c26a8a4]
    void tick();

    // Live readouts — mirrors GetInboundBps / GetOutboundBps.
    // Source: mi0bot bandwidth_monitor.c:115-123 [@c26a8a4]
    double ep6IngressBytesPerSec() const;
    double ep2EgressBytesPerSec() const;

    // NereusSDR throttle state (no upstream equivalent).
    bool isThrottled() const;
    int  throttleEventCount() const;

    // Hard reset — clears all counters and throttle state.
    // Source: mi0bot bandwidth_monitor.c:59-72 bandwidth_monitor_reset() [@c26a8a4]
    // Original: InterlockedExchange64() across all six volatile fields.
    void reset();

signals:
    // Emitted when the throttle state changes.
    // NereusSDR addition — no upstream equivalent.
    void throttledChanged(bool throttled);

    // Emitted by tick() with the latest rates for live UI feeds.
    // Connects to BandwidthMonitorTab::updateLiveStats().
    void liveStatsUpdated(double ep6Bps, double ep2Bps, int throttleEvents);

private:
    // Upstream compute_bps() state — faithfully ported from bandwidth_monitor.c.
    // Original: static volatile LONG64 in_total_bytes / in_last_bytes / in_last_ms
    // NereusSDR: std::atomic<int64_t> (same semantics, cross-platform).
    // Source: mi0bot bandwidth_monitor.c:42-56 [@c26a8a4]
    std::atomic<int64_t> m_inTotalBytes{0};
    std::atomic<int64_t> m_outTotalBytes{0};
    std::atomic<int64_t> m_inLastBytes{0};
    std::atomic<int64_t> m_outLastBytes{0};
    std::atomic<int64_t> m_inLastMs{0};
    std::atomic<int64_t> m_outLastMs{0};

    // Cached last-computed rates (updated by tick() / compute_bps()).
    // Source: mi0bot bandwidth_monitor.c:51-53 — static volatile double in/out_last_bps [@c26a8a4]
    double m_inLastBps{0.0};
    double m_outLastBps{0.0};

    // NereusSDR throttle state (no upstream equivalent).
    bool m_throttled{false};
    int  m_silentTicks{0};
    int  m_throttleEventCount{0};

    // Inner compute_bps() — ported from bandwidth_monitor.c:86-113 [@c26a8a4]
    double computeBps(std::atomic<int64_t>& totalBytes,
                      std::atomic<int64_t>& lastBytes,
                      std::atomic<int64_t>& lastMs,
                      double&               lastBps) const;

    // Return current time in milliseconds since epoch (portable replacement for
    // upstream GetTickCount64).
    // Source: mi0bot bandwidth_monitor.c:54-57 now_ms() [@c26a8a4]
    static int64_t nowMs();
};

} // namespace NereusSDR
