#pragma once
// no-port-check: NereusSDR-original. What a Power Genius XL status frame
// means for its gauges, in the units the Core's `amplifier` object carries.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/PgxlStatusGauges.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-47 / R-R3-22.
//
// One conversion for the Power Genius XL gauges, used by the Core (which
// mirrors the result as the `amplifier` object) and by a local window in
// the same process. It was MainWindow's PgxlConnection::statusUpdated
// handler; the arithmetic and the transmit gate are unchanged:
//
//   peakfwd  dBm              -> W      10^(dBm/10) / 1000
//   swr      signed dB return  -> ratio  |G| = 10^(rl/20), (1+|G|)/(1-|G|),
//            loss (negative              99 for rl >= 0 dB or |G| >= 0.999
//            on a good match)
//   temp     degrees C, id  A (drain current), vac  V (mains),
//   meffa    the amp's own efficiency label, kept as sent
//
// peakfwd and swr are hold values on the amp: they latch the last
// transmit peak. So they are taken only while the state is TRANSMIT_A or
// TRANSMIT_B, and a state frame outside those sets forward power to 0 W
// and SWR to 1.0 (2026-05-22 bench fix, formerly in MainWindow).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created from MainWindow's Power Genius
//                                    gauge handler (R-R3-47, R-R3-22).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QMap>
#include <QString>

namespace NereusSDR {

/// The Power Genius XL gauge readings after the status frames seen so far.
struct PgxlGauges {
    /// A status frame has arrived.
    bool present = false;
    /// The amp's own state word, as sent (IDLE, OPERATE, STANDBY, POWERUP,
    /// FAULT..., TRANSMIT_A, TRANSMIT_B). Empty until a frame carries one.
    QString deviceState;
    /// deviceState is TRANSMIT_A or TRANSMIT_B.
    bool transmitting = false;
    /// Peak forward power while transmitting, in watts; 0 otherwise.
    double forwardPowerW = 0.0;
    /// SWR as a ratio (1.0 is a perfect match, capped at 99) while
    /// transmitting; 1.0 otherwise.
    double swr = 1.0;
    /// Heat-sink temperature, degrees C.
    double temperatureC = 0.0;
    /// Mains voltage, V.
    double mainsVoltageV = 0.0;
    /// Drain current, A.
    double drainCurrentA = 0.0;
    /// The amp's efficiency label (MEffA), as sent.
    QString efficiencyText;

    bool operator==(const PgxlGauges&) const = default;
};

/// Peak forward power: dBm to watts.
float pgxlDbmToWatts(float dbm);

/// The amp's signed return loss in dB to an SWR ratio, capped at 99.
float pgxlReturnLossToSwr(float returnLossDb);

/// True for the states in which the amp is operating (on air ready or
/// keyed): IDLE, OPERATE, TRANSMIT_A, TRANSMIT_B.
bool pgxlStateIsOperate(const QString& deviceState);

/// Applies one status frame (a PgxlConnection::statusUpdated map). A key
/// the frame does not carry keeps its value. Returns true when anything
/// in `gauges` changed.
bool applyPgxlStatus(const QMap<QString, QString>& kvs, PgxlGauges& gauges);

} // namespace NereusSDR
