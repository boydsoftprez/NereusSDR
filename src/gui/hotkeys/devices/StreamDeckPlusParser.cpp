// =================================================================
// src/gui/hotkeys/devices/StreamDeckPlusParser.cpp  (NereusSDR)
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
//   2026-11-06 - Stream Deck+ parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:196-268 and
//                 src/core/HidDeviceParser.h:82-104 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 14. Reports every dial turned and every key changed in one
//                 report, where upstream reports only the first.
//   2026-11-06 - The first report after an open only seeds the buttons, so connecting
//                 never starts a press. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code (Claude Opus 5.5).
// =================================================================

#include "gui/hotkeys/devices/StreamDeckPlusParser.h"

namespace NereusSDR {

namespace {

// From AetherSDR src/core/HidDeviceParser.cpp:196-207 [@1e0718ad]
// 14-byte reports. hidapi always includes the report ID as buf[0] = 0x01.
// Protocol verified against python-elgato-streamdeck v0.9.8 source:
//   buf[0]  = 0x01 (report ID — strip it)
//   buf[1]  = event type: 0x00=key, 0x02=touchscreen, 0x03=dial
//   buf[2..3] = reserved
//   Dial event (buf[1]==0x03):
//     buf[4] = sub-type: 0x01=turn, 0x00=push
//     buf[5..8] = 4 encoder values (signed int8 delta for turn, bool for push)
//   Key event (buf[1]==0x00):
//     buf[4..11] = 8 LCD key states (0=up, 1=down)
constexpr quint8 kReportId = 0x01;
constexpr quint8 kTypeKey = 0x00;
constexpr quint8 kTypeDial = 0x03;
constexpr quint8 kDialPush = 0x00;
constexpr quint8 kDialTurn = 0x01;
constexpr int kFirstKey = 4;
constexpr int kKeyCount = 8;
constexpr int kFirstDial = 5;
constexpr int kDialCount = 4;
constexpr int kMinDialReport = 9;
constexpr int kMinKeyReport = 12;

// Every change between two states, in bit order.
void appendChanges(QList<DeviceEvent>& events, quint8 was, quint8 now, int count, const char* name)
{
    const quint8 changed = was ^ now;
    for (int i = 0; i < count; ++i) {
        const auto mask = static_cast<quint8>(1U << i);
        if ((changed & mask) != 0) {
            DeviceEvent event;
            event.kind = (now & mask) != 0 ? DeviceEvent::Kind::ButtonDown : DeviceEvent::Kind::ButtonUp;
            event.control = QStringLiteral("%1%2").arg(QLatin1String(name)).arg(i + 1);
            events.append(event);
        }
    }
}

quint8 stateBits(QByteArrayView report, int first, int count)
{
    quint8 state = 0;
    for (int i = 0; i < count; ++i) {
        if (report[first + i] != 0) {
            state |= static_cast<quint8>(1U << i);
        }
    }
    return state;
}

} // namespace

QList<DeviceEvent> StreamDeckPlusParser::parse(QByteArrayView report)
{
    QList<DeviceEvent> events;
    if (report.size() < kMinDialReport || static_cast<quint8>(report[0]) != kReportId) {
        return events;
    }

    const auto type = static_cast<quint8>(report[1]);
    if (type == kTypeKey) {
        if (report.size() < kMinKeyReport) {
            return events;
        }
        // Upstream gives one change per report, which is lost if no report follows.
        const quint8 keys = stateBits(report, kFirstKey, kKeyCount);
        // Connecting never keys: one down in the first report starts no press.
        if (!m_keysSeeded) {
            m_keysSeeded = true;
            m_keys = keys;
        }
        appendChanges(events, m_keys, keys, kKeyCount, "button");
        m_keys = keys;
    } else if (type == kTypeDial) {
        const auto subtype = static_cast<quint8>(report[4]);
        if (subtype == kDialPush) {
            const quint8 presses = stateBits(report, kFirstDial, kDialCount);
            if (!m_dialsSeeded) {
                m_dialsSeeded = true;
                m_dialPresses = presses;
            }
            appendChanges(events, m_dialPresses, presses, kDialCount, "dial");
            m_dialPresses = presses;
        } else if (subtype == kDialTurn) {
            // Upstream returns the first non-zero delta and drops the others.
            for (int i = 0; i < kDialCount; ++i) {
                const int delta = static_cast<qint8>(report[kFirstDial + i]);
                if (delta != 0) {
                    DeviceEvent event;
                    event.kind = DeviceEvent::Kind::Rotate;
                    event.control = QStringLiteral("dial%1").arg(i + 1);
                    event.value = delta;
                    events.append(event);
                }
            }
        }
    }
    // 0x02 is the touchscreen, which no action uses.
    return events;
}

void StreamDeckPlusParser::reset()
{
    m_keysSeeded = false;
    m_dialsSeeded = false;
    m_keys = 0;
    m_dialPresses = 0;
}

} // namespace NereusSDR
