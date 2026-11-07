#pragma once

// =================================================================
// src/gui/hotkeys/devices/FlexControlParser.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - FlexControl token parser built on AetherSDR
//                 src/core/FlexControlManager.cpp:249-344 and
//                 src/core/FlexControlManager.h:14-27 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 15. Returns events where upstream emits signals, and holds
//                 no port, so a test can feed it bytes.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QList>

namespace NereusSDR {

// The FlexControl sends semicolon-delimited ASCII, so a token can arrive in two reads.
// Tap, double tap and hold are decided in the device's firmware: no timing here.
class NEREUS_GUI_EXPORT FlexControlParser {
public:
    QList<DeviceEvent> feed(QByteArrayView bytes);
    void reset();

    // True once for each device reset token seen since the last call. The device has
    // just cleared its lights, so they want sending again.
    bool takeDeviceReset();

    // The report that lights aux 1 to 3, one at a time. Anything else lights none.
    static QByteArray auxLights(int lit);

private:
    void processCommand(const QByteArray& cmd, QList<DeviceEvent>& events);

    QByteArray m_buffer;
    bool m_deviceReset{false};
};

} // namespace NereusSDR
