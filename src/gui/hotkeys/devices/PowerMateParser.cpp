// =================================================================
// src/gui/hotkeys/devices/PowerMateParser.cpp  (NereusSDR)
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
//   2026-11-06 - Griffin PowerMate parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:92-113 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 13. A press and a turn in one report give both events.
// =================================================================

#include "gui/hotkeys/devices/PowerMateParser.h"

#include <QtGlobal>

namespace NereusSDR {

namespace {

// From AetherSDR src/core/HidDeviceParser.cpp:93 [@1e0718ad]
// 6-byte reports. Byte 0 = button (0/1). Byte 1 = signed rotation delta.
constexpr int kMinReport = 2;

} // namespace

QList<DeviceEvent> PowerMateParser::parse(QByteArrayView report)
{
    QList<DeviceEvent> events;
    if (report.size() < kMinReport) {
        return events;
    }

    const bool down = report[0] != 0;
    if (down != m_down) {
        m_down = down;
        DeviceEvent event;
        event.kind = down ? DeviceEvent::Kind::ButtonDown : DeviceEvent::Kind::ButtonUp;
        event.control = QStringLiteral("press");
        events.append(event);
    }

    const auto rotation = static_cast<qint8>(report[1]);
    if (rotation != 0) {
        DeviceEvent event;
        event.kind = DeviceEvent::Kind::Rotate;
        event.control = QStringLiteral("wheel");
        event.value = rotation;
        events.append(event);
    }
    return events;
}

void PowerMateParser::reset()
{
    m_down = false;
}

} // namespace NereusSDR
