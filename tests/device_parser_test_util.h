// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 13.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test helper.
//
// Parser events as short text, so a failed comparison reads plainly.
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "gui/hotkeys/devices/DeviceParser.h"

namespace NereusSDR::parsertest {

inline QStringList text(const QList<DeviceEvent>& events)
{
    QStringList out;
    for (const DeviceEvent& event : events) {
        switch (event.kind) {
        case DeviceEvent::Kind::ButtonDown:
            out << QStringLiteral("down %1").arg(event.control);
            break;
        case DeviceEvent::Kind::ButtonUp:
            out << QStringLiteral("up %1").arg(event.control);
            break;
        case DeviceEvent::Kind::Rotate:
            out << QStringLiteral("turn %1 %2").arg(event.control).arg(event.value);
            break;
        case DeviceEvent::Kind::RingPosition:
            out << QStringLiteral("at %1 %2").arg(event.control).arg(event.value);
            break;
        case DeviceEvent::Kind::TokenGesture:
            out << QStringLiteral("token %1").arg(event.control);
            break;
        }
    }
    return out;
}

} // namespace NereusSDR::parsertest
