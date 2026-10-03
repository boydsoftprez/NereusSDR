#pragma once
// no-port-check: NereusSDR-original helper for Setup > Hardware Config.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/setup/hardware/HardwareTransmitGate.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. R-R3-46: Hardware Config's transmit fields (TX
// antennas, relays, external PA, User Dig Out, PA calibration) follow the
// transmit permission with its reason, while the rest of each tab stays
// live. A local window is always permitted, so nothing changes there.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QString>
#include <QVariant>
#include <QWidget>

namespace NereusSDR::HardwareTransmitGate {

/// Enable `widget` when transmit is permitted; otherwise disable it and show
/// `reason` as its tooltip. The widget's own tooltip comes back when it is
/// permitted again.
inline void apply(QWidget* widget, bool permitted, const QString& reason)
{
    if (widget == nullptr) {
        return;
    }
    static const char* const kOwnTip = "nereusHardwareOwnToolTip";
    if (!widget->property(kOwnTip).isValid()) {
        widget->setProperty(kOwnTip, widget->toolTip());
    }
    widget->setEnabled(permitted);
    widget->setToolTip(permitted ? widget->property(kOwnTip).toString() : reason);
}

} // namespace NereusSDR::HardwareTransmitGate
