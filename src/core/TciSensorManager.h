// =================================================================
// src/core/TciSensorManager.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/TCIServer.cs,
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 19.1 by J.J. Boyd (KG4VCF),
//                AI-assisted transformation via Anthropic Claude Code.
// =================================================================

/*  TCIServer.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

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

// src/core/TciSensorManager.h  (NereusSDR)
// NereusSDR-original — TCI sensor wire-format helpers + interval aggregation.
//
// Ports the four private sendXxx sensor methods and the per-listener
// MinimumRequiredRxSensorInterval / MinimumRequiredTxSensorInterval helpers
// from Thetis TCIServer.cs:2314-2332, 915-922 [v2.10.3.13] as static
// inline helpers.
//
// Header-only: all methods are inline — no .cpp required for pure logic.
//
// Ported from Thetis v2.10.3.13 (commit 501e3f51) — TCIServer.cs

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

namespace NereusSDR {

// ── TciSensorManager ──────────────────────────────────────────────────────────
//
// Static helpers for encoding TCI sensor broadcast frames and aggregating
// per-client sensor polling intervals.
//
// All format helpers produce F1 (1 decimal place) output using Qt's C-locale
// QString::number(value, 'f', 1) — equivalent to C# "F1" with
// CultureInfo.InvariantCulture.  Verified: Qt uses '.' as decimal separator
// regardless of process locale.

class TciSensorManager {
public:
    // From Thetis TCIServer.cs:2314-2316 [v2.10.3.13] — sendRxSensors.
    //
    // Encodes:  rx_sensors:<rx>,<levelDbm>;
    // Example:  rx_sensors:0,-95.3;
    static inline QString formatRxSensors(int rx, double levelDbm)
    {
        return QStringLiteral("rx_sensors:%1,%2;")
            .arg(rx)
            .arg(QString::number(levelDbm, 'f', 1));
    }

    // From Thetis TCIServer.cs:2318-2320 [v2.10.3.13] — sendRxChannelSensors
    // basic form (first sendTextFrame in the dual-emit).
    //
    // Encodes:  rx_channel_sensors:<rx>,<channel>,<levelDbm>;
    // Example:  rx_channel_sensors:0,0,-95.3;
    static inline QString formatRxChannelSensors(int rx, int channel, double levelDbm)
    {
        return QStringLiteral("rx_channel_sensors:%1,%2,%3;")
            .arg(rx)
            .arg(channel)
            .arg(QString::number(levelDbm, 'f', 1));
    }

    // From Thetis TCIServer.cs:2321 [v2.10.3.13] — sendRxChannelSensors
    // extended form (second sendTextFrame in the dual-emit).
    //
    // Encodes:  rx_channel_sensors_ex:<rx>,<channel>,<levelDbm>,<avgDbm>,<peakBinDbm>;
    // Example:  rx_channel_sensors_ex:0,0,-95.3,-97.1,-93.2;
    static inline QString formatRxChannelSensorsEx(int rx, int channel,
                                                    double levelDbm,
                                                    double avgDbm,
                                                    double peakBinDbm)
    {
        return QStringLiteral("rx_channel_sensors_ex:%1,%2,%3,%4,%5;")
            .arg(rx)
            .arg(channel)
            .arg(QString::number(levelDbm, 'f', 1))
            .arg(QString::number(avgDbm, 'f', 1))
            .arg(QString::number(peakBinDbm, 'f', 1));
    }

    // From Thetis TCIServer.cs:2323-2332 [v2.10.3.13] — sendTxSensors.
    //
    // Encodes:  tx_sensors:<rx>,<micLevelDbm>,<rmsPowerWatts>,<peakPowerWatts>,<swr>;
    // Example:  tx_sensors:0,-30.0,12.5,12.5,1.1;
    //
    // Note: ALL five fields are F1 (1 decimal).  The plan example showed SWR as
    // 1.10 (F2) — this was incorrect.  Thetis source uses "{4:F1}" for SWR, so
    // 1.10 -> 1.1.  This is the corrected implementation.
    static inline QString formatTxSensors(int rx, double micLevelDbm,
                                          double rmsPowerWatts,
                                          double peakPowerWatts,
                                          double swr)
    {
        return QStringLiteral("tx_sensors:%1,%2,%3,%4,%5;")
            .arg(rx)
            .arg(QString::number(micLevelDbm, 'f', 1))
            .arg(QString::number(rmsPowerWatts, 'f', 1))
            .arg(QString::number(peakPowerWatts, 'f', 1))
            .arg(QString::number(swr, 'f', 1));
    }

    // Aggregate the minimum requested interval across all subscribed clients.
    //
    // From Thetis TCIServer.cs:7571-7603 [v2.10.3.13] —
    // MinimumRequiredRxSensorInterval / MinimumRequiredTxSensorInterval
    // on the server class iterates all listeners and picks the minimum.
    // The per-listener clampIntervalMs helper is at TCIServer.cs:501-506
    // [v2.10.3.13] (clamps 30..1000 ms).
    //
    // NereusSDR flattens the two levels (server + per-listener) into one
    // static helper: pass in the per-client interval list directly.
    //
    // Behaviour:
    //   - Empty list      -> returns kDefault (200 ms)
    //   - Intervals < 30  -> clamped up to 30 ms
    //   - Intervals > 1000 -> clamped down to 1000 ms
    //   - Returns the minimum of all clamped values
    static inline int minimumRequiredInterval(const QList<int>& intervals)
    {
        // From Thetis clampIntervalMs (TCIServer.cs ~501-506 [v2.10.3.13])
        constexpr int kDefault = 200;
        constexpr int kMin     = 30;
        constexpr int kMax     = 1000;

        if (intervals.isEmpty()) {
            return kDefault;
        }

        int minVal = kMax;
        for (int v : intervals) {
            // clampIntervalMs inlined — matches Thetis exactly
            const int clamped = (v < kMin) ? kMin : (v > kMax) ? kMax : v;
            if (clamped < minVal) {
                minVal = clamped;
            }
        }
        return minVal;
    }
};

} // namespace NereusSDR
