// =================================================================
// src/gui/hotkeys/devices/FlexControlParser.cpp  (NereusSDR)
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

#include "gui/hotkeys/devices/FlexControlParser.h"

#include <QString>

#include <algorithm>

namespace NereusSDR {

namespace {

// From AetherSDR src/core/FlexControlManager.cpp:263 [@1e0718ad]
constexpr qsizetype kMaxBufferBytes = 256;
// From AetherSDR src/core/FlexControlManager.cpp:288 [@1e0718ad]
constexpr int kKnobButton = 4;

QString buttonControl(int button)
{
    return button == kKnobButton ? QStringLiteral("knob") : QStringLiteral("button%1").arg(button);
}

ControlGesture gestureFor(char action)
{
    // From AetherSDR src/core/FlexControlManager.cpp:290 [@1e0718ad]
    if (action == 'S') {
        return ControlGesture::Tap;
    }
    return action == 'C' ? ControlGesture::DoubleTap : ControlGesture::Hold;
}

DeviceEvent tokenEvent(int button, char action)
{
    DeviceEvent event;
    event.kind = DeviceEvent::Kind::TokenGesture;
    event.control = buttonControl(button);
    event.gesture = gestureFor(action);
    return event;
}

DeviceEvent wheelEvent(int steps)
{
    DeviceEvent event;
    event.kind = DeviceEvent::Kind::Rotate;
    event.control = QStringLiteral("wheel");
    event.value = steps;
    return event;
}

int accelOf(const QByteArray& cmd)
{
    return cmd.size() > 1 ? std::max(1, cmd.mid(1).toInt()) : 1;
}

} // namespace

QList<DeviceEvent> FlexControlParser::feed(QByteArrayView bytes)
{
    QList<DeviceEvent> events;
    m_buffer.append(bytes);

    // Process all complete commands (semicolon-delimited)
    // [original inline comment from src/core/FlexControlManager.cpp:253]
    qsizetype idx = 0;
    while ((idx = m_buffer.indexOf(';')) >= 0) {
        const QByteArray cmd = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);
        if (!cmd.isEmpty()) {
            processCommand(cmd, events);
        }
    }

    // Cap buffer to prevent runaway growth from garbage data
    // [original inline comment from src/core/FlexControlManager.cpp:262]
    if (m_buffer.size() > kMaxBufferBytes) {
        m_buffer.clear();
    }
    return events;
}

void FlexControlParser::processCommand(const QByteArray& cmd, QList<DeviceEvent>& events)
{
    // Direction: upstream's header says D tunes up (FlexControlManager.h:18), its code
    // emits -accel for D (FlexControlManager.cpp:274), and MainWindow_Controllers.cpp:366
    // passes that through. This follows the code, since it is what users have run. The
    // Knobs page's direction setting covers anyone it's wrong for.
    if (cmd.startsWith('D')) {
        // Clockwise rotation: D (1 step), D02–D06 (accelerated)
        // [original inline comment from src/core/FlexControlManager.cpp:270]
        events.append(wheelEvent(-accelOf(cmd)));

    } else if (cmd.startsWith('U')) {
        // Counter-clockwise rotation: U (1 step), U02–U06 (accelerated)
        // [original inline comment from src/core/FlexControlManager.cpp:277]
        events.append(wheelEvent(accelOf(cmd)));

    } else if (cmd.startsWith('X') && cmd.size() >= 3) {
        // Side-button press: X<button><action>
        //   button: 1, 2, 3 (side buttons)
        //   action: S=tap(0), C=double-tap(1), L=hold(2)
        // [original inline comment from src/core/FlexControlManager.cpp:284-286]
        const int button = cmd.at(1) - '0';
        if (button < 1 || button > kKnobButton) {
            return;
        }
        events.append(tokenEvent(button, cmd.at(2)));

    } else if (cmd.size() == 1 && (cmd == "S" || cmd == "C" || cmd == "L")) {
        // Knob press: bare S/C/L token (no X-prefix). Confirmed via FlexControl
        // hardware capture (#2263). The knob is button 4 in our action-dropdown
        // layout, matching the pre-existing X4S/X4C/X4L slot.
        // [original inline comment from src/core/FlexControlManager.cpp:294-296]
        events.append(tokenEvent(kKnobButton, cmd.at(0)));

    } else if (cmd.startsWith('F')) {
        // Init/reset (e.g. F0304;) — device just cleared its hardware state,
        // including the Aux LEDs. Re-issue our cached LED state so the
        // hardware matches the application's active wheel-mode button. This
        // covers race-windows where our open()'s writeLedState() arrives at
        // the device before its own power-on reset completes (#2908).
        // [original inline comment from src/core/FlexControlManager.cpp:301-305]
        m_deviceReset = true;
    }
}

void FlexControlParser::reset()
{
    m_buffer.clear();
    m_deviceReset = false;
}

bool FlexControlParser::takeDeviceReset()
{
    const bool seen = m_deviceReset;
    m_deviceReset = false;
    return seen;
}

QByteArray FlexControlParser::auxLights(int lit)
{
    // From AetherSDR src/core/FlexControlManager.cpp:323-326 [@1e0718ad]
    QByteArray cmd("I000;");
    if (lit >= 1 && lit <= 3) {
        cmd[lit] = '1';
    }
    return cmd;
}

} // namespace NereusSDR
