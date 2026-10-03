// no-port-check: NereusSDR-original. See PgxlStatusGauges.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/PgxlStatusGauges.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created from MainWindow's Power Genius
//                                    gauge handler (R-R3-47, R-R3-22).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/PgxlStatusGauges.h"

#include <cmath>

namespace NereusSDR {

float pgxlDbmToWatts(float dbm)
{
    // 2026-05-20 bench fix: peakfwd is dBm, not watts.
    return std::pow(10.0f, dbm / 10.0f) / 1000.0f;
}

float pgxlReturnLossToSwr(float returnLossDb)
{
    // 2026-05-20 bench fix: swr is signed dB return loss (negative on a
    // good match; e.g. -24.5 -> |G| 0.0596 -> SWR 1.13), not a ratio.
    if (returnLossDb >= 0.0f) {
        return 99.0f;  // RL >= 0 -> open/short, cap display
    }
    const float gamma = std::pow(10.0f, returnLossDb / 20.0f);
    return (gamma >= 0.999f) ? 99.0f : (1.0f + gamma) / (1.0f - gamma);
}

bool pgxlStateIsOperate(const QString& deviceState)
{
    return deviceState == QStringLiteral("IDLE")
        || deviceState == QStringLiteral("OPERATE")
        || deviceState == QStringLiteral("TRANSMIT_A")
        || deviceState == QStringLiteral("TRANSMIT_B");
}

bool applyPgxlStatus(const QMap<QString, QString>& kvs, PgxlGauges& gauges)
{
    PgxlGauges next = gauges;
    next.present = true;

    if (kvs.contains(QStringLiteral("temp"))) {
        next.temperatureC = static_cast<double>(kvs.value(QStringLiteral("temp")).toFloat());
    }
    if (kvs.contains(QStringLiteral("id"))) {
        next.drainCurrentA = static_cast<double>(kvs.value(QStringLiteral("id")).toFloat());
    }
    if (kvs.contains(QStringLiteral("vac"))) {
        next.mainsVoltageV = static_cast<double>(kvs.value(QStringLiteral("vac")).toFloat());
    }
    if (kvs.contains(QStringLiteral("meffa"))) {
        next.efficiencyText = kvs.value(QStringLiteral("meffa"));
    }

    // 2026-05-22 bench fix: peakfwd and swr latch the last transmit peak on
    // the amp and do not decay when it leaves TRANSMIT_A/B. Take them only
    // while transmitting, inferred from this frame's state or, without one,
    // from the last state seen; outside transmit the gauges read 0 W / 1.0.
    if (kvs.contains(QStringLiteral("state"))) {
        next.deviceState = kvs.value(QStringLiteral("state"));
        next.transmitting = next.deviceState == QStringLiteral("TRANSMIT_A")
                         || next.deviceState == QStringLiteral("TRANSMIT_B");
        if (!next.transmitting) {
            next.forwardPowerW = 0.0;
            next.swr = 1.0;
        }
    }
    if (next.transmitting && kvs.contains(QStringLiteral("peakfwd"))) {
        next.forwardPowerW = static_cast<double>(
            pgxlDbmToWatts(kvs.value(QStringLiteral("peakfwd")).toFloat()));
    }
    if (next.transmitting && kvs.contains(QStringLiteral("swr"))) {
        next.swr = static_cast<double>(
            pgxlReturnLossToSwr(kvs.value(QStringLiteral("swr")).toFloat()));
    }

    const bool changed = !(next == gauges);
    gauges = next;
    return changed;
}

} // namespace NereusSDR
