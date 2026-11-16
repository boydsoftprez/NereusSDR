// =================================================================
// src/gui/hotkeys/devices/TMate2Parser.cpp  (NereusSDR)
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
//   2026-11-06 - TMate 2 parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:270-329 and
//                 src/core/HidDeviceParser.h:106-132 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 14. Gives every change in a report, where upstream gives the
//                 first and leaves the rest for the next report.
// =================================================================

#include "gui/hotkeys/devices/TMate2Parser.h"

#include "core/hotkeys/HotKeyDeviceBindings.h"

namespace NereusSDR {

namespace {

// From AetherSDR src/core/HidDeviceParser.h:108-116 [@1e0718ad]
// Input layout (bytes 0-8 mapped; 9-63 not yet fully decoded):
//   [0]    report ID = 0x01
//   [1..2] encoder 1, little-endian uint16 (absolute wrapping counter)
//   [3..4] encoder 2, little-endian uint16
//   [5..6] encoder 3 (volume), little-endian uint16
//   [7..8] key bitmask, little-endian uint16, active-low (bit clear = pressed):
//          bit0=F1, bit1=F2, bit2=F3, bit3=F4, bit4=F5, bit5=F6,
//          bit6=enc1/main-encoder push, bit7=enc2 push, bit8=enc3 push.
//          Idle state: 0x01FF (all bits set).
constexpr int kMinReport = 9;
constexpr quint8 kReportId = 0x01;
constexpr int kEncoderCount = 3;
constexpr quint16 kKeysIdle = 0x01FF;

// Buttons 7-8 are intentionally unused (no hardware key on those numbers).
// [original inline comment from src/core/HidDeviceParser.cpp:311]
constexpr int kButtonMap[9] = {1, 2, 3, 4, 5, 6, 9, 10, 11};

quint16 littleEndian(QByteArrayView report, int at)
{
    return static_cast<quint16>(static_cast<quint8>(report[at]) | (static_cast<quint8>(report[at + 1]) << 8));
}

} // namespace

QList<DeviceEvent> TMate2Parser::parse(QByteArrayView report)
{
    QList<DeviceEvent> events;
    if (report.size() < kMinReport || static_cast<quint8>(report[0]) != kReportId) {
        return events;
    }

    std::array<quint16, kEncoderCount> encoders{};
    for (int i = 0; i < kEncoderCount; ++i) {
        encoders[static_cast<size_t>(i)] = littleEndian(report, 1 + 2 * i);
    }
    const quint16 keys = littleEndian(report, 7) & kKeysIdle;

    if (!m_seeded) {
        m_seeded = true;
        m_encoders = encoders;
        m_keys = keys;
        return events;
    }

    // Keys (active-low) — one bit at a time so simultaneous presses are not lost.
    // [original inline comment from src/core/HidDeviceParser.cpp:307]
    // Upstream gives one change per report and leaves the rest for the next report,
    // which a device that stops sending would never bring. Here all of them come now.
    const quint16 changed = keys ^ m_keys;
    for (int bit = 0; bit < 9; ++bit) {
        const auto mask = static_cast<quint16>(1U << bit);
        if ((changed & mask) != 0) {
            // bit set = idle/released (action 1); bit clear = pressed (action 0)
            // [original inline comment from src/core/HidDeviceParser.cpp:319]
            DeviceEvent event;
            event.kind = (keys & mask) != 0 ? DeviceEvent::Kind::ButtonUp : DeviceEvent::Kind::ButtonDown;
            event.control = QStringLiteral("button%1").arg(kButtonMap[bit]);
            events.append(event);
        }
    }
    m_keys = keys;

    for (int i = 0; i < kEncoderCount; ++i) {
        const auto index = static_cast<size_t>(i);
        int diff = static_cast<int>(encoders[index]) - static_cast<int>(m_encoders[index]);
        if (diff > 32767) {
            diff -= 65536;
        }
        if (diff < -32768) {
            diff += 65536;
        }
        m_encoders[index] = encoders[index];
        if (diff == 0) {
            continue;
        }
        if (i == 0) {
            diff = -diff;
        }
        DeviceEvent event;
        event.kind = DeviceEvent::Kind::Rotate;
        event.control = HotKeyDeviceBindings::tmate2EncoderControl(i);
        event.value = diff;
        events.append(event);
    }
    return events;
}

void TMate2Parser::reset()
{
    m_seeded = false;
    m_keys = kKeysIdle;
    m_encoders = {};
}

} // namespace NereusSDR
