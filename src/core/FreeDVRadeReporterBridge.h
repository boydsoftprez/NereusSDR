// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - FreeDVRadeReporterBridge: drives the RADE "sync-only"
// rx_report upload path (Path B) into FreeDVReporterClient, mirroring
// freedv-gui's MainFrame::OnTimer behavior. Path A (callsign-decoded
// via EOO) is already driven by RadioModel::onRadeTextDecoded, which
// calls FreeDVReporterClient::sendRxReport directly. This bridge fills
// the gap for the long stretches where RADE has sync but the remote
// station has not yet sent an EOO frame.
//
// Ported from freedv-gui src/main.cpp:1869-1996 [@77e793a]
//   (MainFrame::OnTimer reporter-upload block, in particular the
//   `else if (... FREEDV_MODE_RADE && syncState)` branch at lines
//   1971-1996 and the `pendingSnr = (int)(g_snr + 0.5)` rounding at
//   line 1884) plus
// freedv-gui src/integrations/common/ReportingController.cpp:50,
//   196-209 [@77e793a]
//   (MODE_STRING "RADEV1" constant and reportCallsign throttle wrapper).
//
// Structural pattern from AetherSDR src/core/FreeDvClient.{h,cpp}
// [@0cd4559] for Qt6 lifecycle (QObject + slots + QTimer-driven tick).
//
// License (upstream):
//   - freedv-gui carries an LGPLv2.1+ root license (freedv-gui/COPYING).
//     The specific main.cpp file carries the GPL v2.1 header reproduced
//     below verbatim per the upstream redistribution clause. The
//     FreeDVReporter.cpp file carries a BSD-2-Clause-style file header
//     (Copyright Mooneer Salem) and is reproduced verbatim in
//     FreeDVReporterClient.h.
//   - AetherSDR is GPL-3.0-or-later
//     (https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// LGPL is upgrade-compatible to GPL-3 (LGPL §3 conversion clause);
// the GPL v2.1 carve-out on individual freedv-gui files is GPL-3
// compatible.
//
// --- From freedv-gui src/main.cpp [@77e793a] (verbatim header) ---
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
// --- Modification history (NereusSDR) ---
//
// 2026-05-15  J.J. Boyd (KG4VCF), with AI tooling assistance
//   New port. Path B (RADE sync-only) rx_report upload bridge.
//
#ifndef NEREUSSDR_CORE_FREEDV_RADE_REPORTER_BRIDGE_H
#define NEREUSSDR_CORE_FREEDV_RADE_REPORTER_BRIDGE_H

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <limits>

class QTimer;

namespace NereusSDR {

class FreeDVReporterClient;
class PskReporterClient;

class FreeDVRadeReporterBridge : public QObject
{
    Q_OBJECT
public:
    // From freedv-gui src/main.h:101 [@77e793a]:
    //   #define _REFRESH_TIMER_PERIOD   (DT*1000)
    // with `#define DT 0.10` (freedv-gui src/defines.h:41 [@77e793a]).
    // 100 ms = 10 Hz timer; modulo-10 yields 1 Hz effective upload
    // cadence for Path B.
    static constexpr int kTickIntervalMs = 100;
    static constexpr int kTicksPerReport = 10;

    explicit FreeDVRadeReporterBridge(FreeDVReporterClient* freedv,
                                      PskReporterClient*    psk,
                                      QObject*              parent = nullptr);
    ~FreeDVRadeReporterBridge() override;

    // Reporting-enabled gate. Mirrors freedv-gui main.cpp:1874
    // `appConfiguration.reportingConfiguration.reportingEnabled`.
    // Defaults to false until something (RadioModel wiring or an
    // explicit Setup toggle) flips it on.
    void setReportingEnabled(bool enabled);
    bool isReportingEnabled() const { return m_reportingEnabled; }

    // Test seam: drive a single tick deterministically without
    // waiting on the live 100 ms QTimer. Mirrors the test-seam
    // pattern already used by FreeDVReporterClient
    // (handleSocketIOForTest, lastSentMessageForTest).
    void tickForTest();

public slots:
    // Mirror of RadioModel::radeSyncChanged + radeSnrChanged. The
    // bridge collapses per-slice state into a single station-wide
    // view (matches freedv-gui's single-`g_snr` design at
    // main.cpp:121). Multi-slice RADE is a 3F future.
    void onRadeSyncChanged(int sliceId, bool synced);
    void onRadeSnrChanged(int sliceId, float snrDb);

    // MOX gate. Mirrors freedv-gui main.cpp:1756 `!txState` -- while
    // TXing we suppress Path B so we are not reporting our own
    // carrier as a decode.
    void onMoxStateChanged(bool tx);

signals:
    // Diagnostic: emitted on every Path B upload so RadioModel /
    // tests can observe activity without scraping the wire.
    void rxReportEmitted(int snrDb);

private slots:
    void onTick();

private:
    bool shouldEmitPathB_() const;
    int  roundedSnrDb_() const;  // (int)(snrDb + 0.5), matches upstream

    FreeDVReporterClient* m_freedv {nullptr};
    PskReporterClient*    m_psk    {nullptr};
    QTimer*               m_tickTimer {nullptr};

    // Path B state. The 10 Hz tick increments m_reportCounter; emit
    // when (counter + 1) % 10 == 0, matching freedv-gui main.cpp:1982
    //   `m_reportCounter = (m_reportCounter + 1) % 10;`
    //   `if (freq > 0 && m_reportCounter == 0)`.
    int m_reportCounter {0};

    // Single-station collapsed RADE state (see slot doc above).
    bool   m_synced    {false};
    double m_snrDb     {std::numeric_limits<double>::quiet_NaN()};

    bool m_txActive         {false};
    bool m_reportingEnabled {false};
};

} // namespace NereusSDR

#endif // NEREUSSDR_CORE_FREEDV_RADE_REPORTER_BRIDGE_H
