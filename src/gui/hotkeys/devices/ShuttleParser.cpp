// =================================================================
// src/gui/hotkeys/devices/ShuttleParser.cpp  (NereusSDR)
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
//   2026-11-06 - Contour Shuttle parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:115-195 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 13. Reads the buttons from bytes 3 and 4, not upstream's
//                 bytes, and decodes the ring, which upstream does not. See the
//                 comment in the .cpp for why.
// =================================================================

#include "gui/hotkeys/devices/ShuttleParser.h"

namespace NereusSDR {

namespace {

// The buttons: where upstream and the other sources disagree.
//
// AetherSDR src/core/HidDeviceParser.cpp:117, 124, 157, 164 [@1e0718ad] says bytes
// 2-3. ShuttleXpress reads bit 0-4 of [3] alone and ShuttlePro v2 reads [2]|[3]<<8.
// It names no capture of either device.
//
// Three other sources agree with each other that the buttons are [3]|[4]<<8. Each
// is tied to these devices by USB id. Facts read from them, no code taken:
//   contour-shuttle packages/core/src/Shuttle.ts:78, products.ts:44-59 [@96136488]:
//     buttons = uint16 little-endian at offset 3. ShuttleXpress (0B33:0020) is bits
//     4-8, ShuttlePro v2 (0B33:0030) is bits 0-14.
//   Ardour libs/surfaces/contourdesign/contourdesign.cc:47-50, 452 [@704731d2]:
//     (_buf[4] << 8) + _buf[3] for 0B33:0010, 0020 and 0030.
//     contourdesign_gui.cc:184: the Xpress has button indexes 4-8 of those.
//   kino 0.75-7 shuttlepro.c, its "Contour ShuttlePro packet" comment: byte 2 blank,
//     byte 3 and byte 4 are buttons, on the older 05F3:0240.
// We follow those. Upstream's bytes would give the Xpress one button out of five
// (bit 4 of [3]) and the Pro none in the right place.
//
// None of this has been seen on a real device here.
constexpr int kMinReport = 5;
constexpr int kXpressFirstBit = 4;
constexpr int kXpressButtons = 5;
constexpr int kPro2FirstBit = 0;
constexpr int kPro2Buttons = 15;

// The ring: AetherSDR's comment gives its range and no decoder.
// From AetherSDR src/core/HidDeviceParser.cpp:116 [@1e0718ad]
// 5-byte reports. Byte 0 = shuttle position (signed, -7..+7).
// The same in contour-shuttle Shuttle.ts:61 [@96136488] (getInt8(0)) and in kino
// 0.75-7 shuttlepro.c's packet comment (ff=-1 down to f9=-7, 1 to 7).
constexpr int kRingLimit = 7;

} // namespace

ShuttleParser::ShuttleParser(Model model)
    : m_model(model)
{
}

QList<DeviceEvent> ShuttleParser::parse(QByteArrayView report)
{
    QList<DeviceEvent> events;
    if (report.size() < kMinReport) {
        return events;
    }

    const int firstBit = m_model == Model::Xpress ? kXpressFirstBit : kPro2FirstBit;
    const int buttonCount = m_model == Model::Xpress ? kXpressButtons : kPro2Buttons;
    const auto buttons = static_cast<quint16>(static_cast<quint8>(report[3]) | (static_cast<quint8>(report[4]) << 8));

    // Buttons
    // [original inline comment from src/core/HidDeviceParser.cpp:126]
    for (int b = 0; b < buttonCount; ++b) {
        const auto mask = static_cast<quint16>(1u << (firstBit + b));
        if ((buttons & mask) != (m_buttons & mask)) {
            DeviceEvent event;
            event.kind = (buttons & mask) != 0 ? DeviceEvent::Kind::ButtonDown : DeviceEvent::Kind::ButtonUp;
            event.control = QStringLiteral("button%1").arg(b + 1);
            events.append(event);
        }
    }
    m_buttons = buttons;

    // Jog wheel (relative, wrapping)
    // [original inline comment from src/core/HidDeviceParser.cpp:138]
    const auto jog = static_cast<quint8>(report[1]);
    if (m_firstReport) {
        m_firstReport = false;
    } else if (jog != m_jog) {
        int delta = static_cast<int>(jog) - static_cast<int>(m_jog);
        // From AetherSDR src/core/HidDeviceParser.cpp:147-148 [@1e0718ad]
        if (delta > 128) {
            delta -= 256;
        }
        if (delta < -128) {
            delta += 256;
        }
        DeviceEvent event;
        event.kind = DeviceEvent::Kind::Rotate;
        event.control = QStringLiteral("jog");
        event.value = delta;
        events.append(event);
    }
    m_jog = jog;

    const int ring = static_cast<qint8>(report[0]);
    if (ring != m_ring && ring >= -kRingLimit && ring <= kRingLimit) {
        m_ring = ring;
        DeviceEvent event;
        event.kind = DeviceEvent::Kind::RingPosition;
        event.control = QStringLiteral("ring");
        event.value = ring;
        events.append(event);
    }
    return events;
}

void ShuttleParser::reset()
{
    m_jog = 0;
    m_firstReport = true;
    m_buttons = 0;
    m_ring = 0;
}

} // namespace NereusSDR
