// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 16.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/devices/UlanziDialInput.h"

#include "gui/hotkeys/devices/UlanziLinuxInput.h"
#include "gui/hotkeys/devices/UlanziMacInput.h"
#include "gui/hotkeys/devices/UlanziWindowsInput.h"

namespace NereusSDR {

bool UlanziDialInput::staysOnMainThread()
{
#if defined(Q_OS_MAC)
    return true;
#else
    return false;
#endif
}

UlanziDialInput* UlanziDialInput::create(QObject* parent)
{
#if defined(Q_OS_MAC)
    return new UlanziMacInput(parent);
#elif defined(Q_OS_WIN)
    return new UlanziWindowsInput(parent);
#else
    return new UlanziLinuxInput(parent);
#endif
}

const QString& UlanziDialInput::notPluggedInReason()
{
    static const QString kReason = QStringLiteral("Not plugged in");
    return kReason;
}

const QString& UlanziDialInput::claimedReason()
{
    // As DeviceInput says it for the other devices a plugin can take.
    static const QString kReason = QStringLiteral("A deck plugin is using it.");
    return kReason;
}

void UlanziDialInput::setClaimedElsewhere(bool claimed)
{
    if (claimed == m_claimedElsewhere) {
        return;
    }
    m_claimedElsewhere = claimed;
    if (claimed) {
        stop();     // lets go of the device, and of any key it had down
        notify(false, claimedReason());
    } else {
        start();
    }
}

void UlanziDialInput::feedKey(int linuxKeyCode, int value)
{
    const QList<DeviceEvent> events = m_decoder.feed(linuxKeyCode, value);
    for (const DeviceEvent& event : events) {
        emit deviceEvent(DeviceId::UlanziDial, event);
    }
}

void UlanziDialInput::notify(bool connected, const QString& reason)
{
    if (m_stateKnown && m_connected == connected && m_reason == reason) {
        return;
    }
    m_stateKnown = true;
    m_connected = connected;
    m_reason = reason;
    emit deviceStateChanged(DeviceId::UlanziDial, connected, reason);
}

void UlanziDialInput::keysGone(const QString& reason)
{
    const QList<DeviceEvent> released = m_decoder.reset();
    notify(false, reason);
    for (const DeviceEvent& event : released) {
        emit deviceEvent(DeviceId::UlanziDial, event);
    }
}

} // namespace NereusSDR
