// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - FreeDVRadeReporterBridge implementation. Drives the
// Path B (RADE sync-only, empty-callsign) rx_report upload. Path A
// (callsign-decoded via EOO) is already driven by
// RadioModel::onRadeTextDecoded.
//
// Ported from freedv-gui src/main.cpp:1971-1996 [@77e793a]
//   (the FREEDV_MODE_RADE && syncState else-if inside
//   MainFrame::OnTimer) plus:
//   - main.cpp:1884 (`(int)(g_snr + 0.5)` SNR rounding),
//   - main.cpp:1982 (`m_reportCounter = (counter+1)%10` throttle),
//   - main.cpp:1737 (NaN/Inf SNR gate),
//   - main.h:101 + defines.h:41 (10 Hz `_REFRESH_TIMER_PERIOD`),
//   - freedv_interface.cpp:62-63 ("RADEV1" mode string),
//   - integrations/common/ReportingController.cpp:50
//     (`#define MODE_STRING "RADEV1"`).
//
// Structural pattern from AetherSDR src/core/FreeDvClient.cpp
// [@0cd4559] for Qt6 lifecycle.
//
// License (upstream):
//   - freedv-gui main.cpp carries the GPL v2.1 header reproduced
//     verbatim below per the upstream redistribution clause.
//   - AetherSDR is GPL-3.0-or-later
//     (https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//     Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//
// --- From freedv-gui/src/main.cpp [@77e793a] (verbatim header) ---
//
// ==========================================================================
//  Name:            main.cpp
//
//  Purpose:         FreeDV main()
//  Created:         Apr. 9, 2012
//  Authors:         David Rowe, David Witten
//
//  License:
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2.1,
//   as published by the Free Software Foundation.  This program is
//   distributed in the hope that it will be useful, but WITHOUT ANY
//   WARRANTY; without even the implied warranty of MERCHANTABILITY or
//   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//   License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, see <http://www.gnu.org/licenses/>.
//
// ==========================================================================
//
// Modification history (NereusSDR):
//   2026-05-15  J.J. Boyd / KG4VCF  Phase 3R-bridge. New port -
//                 Path B (RADE sync-only) rx_report upload. AI
//                 tooling assistance for the Qt6 plumbing; DSP/
//                 protocol semantics from freedv-gui main.cpp:
//                 1971-1996 verbatim.
//

#include "FreeDVRadeReporterBridge.h"
#include "FreeDVReporterClient.h"
#include "PskReporterClient.h"

#include <QLoggingCategory>
#include <QString>
#include <QTimer>
#include <cmath>

Q_LOGGING_CATEGORY(lcRadeReporterBridge, "nereus.freedv.rade.bridge")

namespace NereusSDR {

FreeDVRadeReporterBridge::FreeDVRadeReporterBridge(
    FreeDVReporterClient* freedv,
    PskReporterClient*    psk,
    QObject*              parent)
    : QObject(parent)
    , m_freedv(freedv)
    , m_psk(psk)
    , m_tickTimer(new QTimer(this))
{
    // 100 ms tick matches freedv-gui src/main.h:101 [@77e793a]
    // `_REFRESH_TIMER_PERIOD = DT*1000 = 100 ms`.
    m_tickTimer->setInterval(kTickIntervalMs);
    m_tickTimer->setTimerType(Qt::CoarseTimer);
    connect(m_tickTimer, &QTimer::timeout,
            this, &FreeDVRadeReporterBridge::onTick);
    // Timer stays stopped until we have a reason to tick (sync + SNR
    // arrived). Avoids burning a 10 Hz wakeup when no RADE slice is
    // active.
}

FreeDVRadeReporterBridge::~FreeDVRadeReporterBridge() = default;

void FreeDVRadeReporterBridge::setReportingEnabled(bool enabled)
{
    if (m_reportingEnabled == enabled) { return; }
    m_reportingEnabled = enabled;
    if (!enabled && m_tickTimer->isActive()) {
        m_tickTimer->stop();
        m_reportCounter = 0;
    }
    qCDebug(lcRadeReporterBridge)
        << "Reporting" << (enabled ? "enabled" : "disabled");
}

void FreeDVRadeReporterBridge::onRadeSyncChanged(int /*sliceId*/, bool synced)
{
    if (m_synced == synced) { return; }
    m_synced = synced;
    qCDebug(lcRadeReporterBridge) << "RADE sync ->" << synced;

    if (synced && m_reportingEnabled && !m_tickTimer->isActive()) {
        m_tickTimer->start();
    } else if (!synced) {
        m_tickTimer->stop();
        m_reportCounter = 0;
    }
}

void FreeDVRadeReporterBridge::onRadeSnrChanged(int /*sliceId*/, float snrDb)
{
    m_snrDb = static_cast<double>(snrDb);
}

void FreeDVRadeReporterBridge::onMoxStateChanged(bool tx)
{
    m_txActive = tx;
}

void FreeDVRadeReporterBridge::tickForTest()
{
    onTick();
}

// From freedv-gui src/main.cpp:1971-1996 [@77e793a]. The 10 Hz timer
// increments a counter and emits on every 10th tick (so 1 Hz). Gates
// match upstream:
//   reporter object exists  (FreeDVReporter.cpp:191 isFullyConnected_)
//   FREEDV_MODE_RADE active (here: m_synced + caller sets synced only
//                            while in RADE mode)
//   syncState               (m_synced)
//   freq > 0                (NereusSDR-side: caller wires freq via
//                            FreeDVReporterClient::setFrequency which
//                            already gates on > 0; bridge does not
//                            re-check)
//   !txState                (m_txActive)
//   !g_playFileFromRadio    (NereusSDR has no equivalent recording
//                            playback feature; gate omitted)
void FreeDVRadeReporterBridge::onTick()
{
    if (!shouldEmitPathB_()) {
        // Keep ticker alive so a transient un-sync/TX bounce does not
        // require a fresh sync transition to resume. Reset the
        // counter so we do not "catch up" with a burst when gating
        // clears.
        m_reportCounter = 0;
        return;
    }

    // From freedv-gui src/main.cpp:1982 [@77e793a]:
    //   m_reportCounter = (m_reportCounter + 1) % 10;
    //   if (freq > 0 && m_reportCounter == 0) { ... emit ... }
    m_reportCounter = (m_reportCounter + 1) % kTicksPerReport;
    if (m_reportCounter != 0) {
        return;
    }

    // From freedv-gui src/main.cpp:1988-1993 [@77e793a]:
    //   m_sharedReporterObject->addReceiveRecord(
    //       "", freedvInterface.getCurrentModeStr(), freq, pendingSnr);
    // RADE-mode currentModeStr == "RADEV1"
    // (freedv-gui src/freedv_interface.cpp:62-63 [@77e793a]).
    const int snrDb = roundedSnrDb_();
    m_freedv->sendRxReport(QString(),                       // empty callsign
                           QStringLiteral("RADEV1"),
                           snrDb);
    // Path B is FreeDV-reporter-only (upstream uses
    // m_sharedReporterObject, not the m_reporters[] fan-out). PSK
    // Reporter is intentionally NOT fed here because an empty
    // callsign is not a meaningful PSK Reporter record (the spec
    // requires a non-empty receiver-reported-callsign).
    emit rxReportEmitted(snrDb);
}

bool FreeDVRadeReporterBridge::shouldEmitPathB_() const
{
    if (!m_reportingEnabled) { return false; }
    if (!m_synced)           { return false; }
    if (m_txActive)          { return false; }
    if (!m_freedv)           { return false; }
    if (!m_freedv->isConnected()) { return false; }
    if (std::isnan(m_snrDb)) { return false; }
    return true;
}

int FreeDVRadeReporterBridge::roundedSnrDb_() const
{
    // From freedv-gui src/main.cpp:1884 [@77e793a] verbatim:
    //   auto pendingSnr = (int)(g_snr + 0.5);
    // C-style cast on a positive value rounds half-up; on a negative
    // value the +0.5 first nudges toward zero, then the cast truncates
    // toward zero. We preserve upstream behavior exactly for both
    // signs of SNR so other operators' dashboards see the same value
    // a freedv-gui user on the same signal would report.
    return static_cast<int>(m_snrDb + 0.5);
}

} // namespace NereusSDR
