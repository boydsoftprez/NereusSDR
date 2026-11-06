// =================================================================
// src/gui/hotkeys/devices/Rc28Parser.cpp  (NereusSDR)
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
//   2026-11-06 - Icom RC-28 parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:43-90 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 13. Reads the button bits one by one where upstream compares
//                 the whole byte, so a hold sees its release with another
//                 button down, and reports rotation with a button held.
// =================================================================

#include "gui/hotkeys/devices/Rc28Parser.h"

namespace NereusSDR {

namespace {

// From AetherSDR src/core/HidDeviceParser.cpp:46-54 [@1e0718ad]
// Verified layout (cross-referenced against FlexRC-28 driver and wfview source):
//   [0] = 0x01 constant — guard byte, discard report if != 0x01
//   [1] = rotation speed (encoder pulse count since last report; higher = faster turn)
//         NOT a monotonic counter — stays at 1 during slow continuous rotation
//   [2] = 0x00 (unused)
//   [3] = direction: 0x01 = CW, 0x02 = CCW; stays set while rotating, 0x00 when idle
//   [4] = 0x00 (unused)
//   [5] = button state bitmask (active-low): 0x07 = all idle,
//         bit0=TX/PTT, bit1=F1, bit2=F2
constexpr int kMinReport = 6;
constexpr quint8 kGuardByte = 0x01;
constexpr quint8 kClockwise = 0x01;
constexpr quint8 kCounterClockwise = 0x02;
constexpr quint8 kAllReleased = 0x07;

struct ButtonBit {
    quint8 mask;
    const char* control;
};
// F1 alone reads 0x05, F2 alone 0x03, TX alone 0x06
// (AetherSDR src/core/HidDeviceParser.cpp:71-77 [@1e0718ad]).
constexpr ButtonBit kButtons[] = {{0x01, "tx"}, {0x02, "f1"}, {0x04, "f2"}};

// From AetherSDR src/core/HidEncoderManager.cpp:924-936 [@1e0718ad]
// Output report: [0x00=reportID, 0x01=cmd, ledByte, zeros...], 33 bytes total.
// Format verified against FlexRC-28 Node.js driver (_sendLED) and
// wfview src/usbcontroller.cpp (RC28 featureLEDControl path).
// Active-low: bit0=TX, bit1=F1, bit2=F2, bit3=LINK; 0x0F = all off.
constexpr int kLightReportSize = 33;
constexpr quint8 kLightCommand = 0x01;
constexpr quint8 kAllLightsOff = 0x0F;
constexpr quint8 kLightTx = 0x01;
constexpr quint8 kLightF1 = 0x02;
constexpr quint8 kLightF2 = 0x04;
constexpr quint8 kLightLink = 0x08;

} // namespace

QList<DeviceEvent> Rc28Parser::parse(QByteArrayView report)
{
    QList<DeviceEvent> events;
    if (report.size() < kMinReport || static_cast<quint8>(report[0]) != kGuardByte) {
        return events;
    }

    const quint8 speed = static_cast<quint8>(report[1]);
    const quint8 direction = static_cast<quint8>(report[3]);
    const quint8 buttons = static_cast<quint8>(report[5]) & kAllReleased;

    // Upstream compares the whole byte, so with a second button down it never sees
    // the first let go. A hold has to see its release, so each bit stands alone.
    const quint8 changed = buttons ^ m_buttons;
    for (const ButtonBit& button : kButtons) {
        if ((changed & button.mask) != 0) {
            const bool down = (buttons & button.mask) == 0;
            DeviceEvent event;
            event.kind = down ? DeviceEvent::Kind::ButtonDown : DeviceEvent::Kind::ButtonUp;
            event.control = QString::fromLatin1(button.control);
            events.append(event);
        }
    }
    m_buttons = buttons;

    // Rotation: one event per report, magnitude proportional to speed (buf[1]).
    // [original inline comment from src/core/HidDeviceParser.cpp:81]
    // Upstream skips rotation with a button held. Here a turn is reported either
    // way, and what a held button means for it is up to the bindings.
    if (speed > 0 && (direction == kClockwise || direction == kCounterClockwise)) {
        DeviceEvent event;
        event.kind = DeviceEvent::Kind::Rotate;
        event.control = QStringLiteral("wheel");
        event.value = direction == kClockwise ? speed : -static_cast<int>(speed);
        events.append(event);
    }
    return events;
}

void Rc28Parser::reset()
{
    m_buttons = kAllReleased;
}

QByteArray rc28LightReport(bool tx, bool f1, bool f2, bool link)
{
    quint8 lights = kAllLightsOff;
    if (tx) {
        lights &= static_cast<quint8>(~kLightTx);
    }
    if (f1) {
        lights &= static_cast<quint8>(~kLightF1);
    }
    if (f2) {
        lights &= static_cast<quint8>(~kLightF2);
    }
    if (link) {
        lights &= static_cast<quint8>(~kLightLink);
    }
    QByteArray report(kLightReportSize, '\0');
    report[1] = static_cast<char>(kLightCommand);
    report[2] = static_cast<char>(lights);
    return report;
}

} // namespace NereusSDR
