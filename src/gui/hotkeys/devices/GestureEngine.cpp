// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original. The one upstream fact is cited at kHoldMs.
#include "gui/hotkeys/devices/GestureEngine.h"

#include <algorithm>

namespace NereusSDR {

namespace {

DeviceEvent token(const QString& control, ControlGesture gesture)
{
    return DeviceEvent{DeviceEvent::Kind::TokenGesture, control, 0, gesture};
}

} // namespace

QList<DeviceEvent> GestureEngine::onEvent(const DeviceEvent& raw, qint64 nowMs)
{
    if (raw.kind == DeviceEvent::Kind::ButtonDown) {
        if (!m_down.contains(raw.control)) {
            m_down.insert(raw.control, Down{nowMs, false});
        }
        return {};
    }
    if (raw.kind != DeviceEvent::Kind::ButtonUp) {
        return {raw};
    }
    const auto down = m_down.constFind(raw.control);
    if (down == m_down.constEnd()) {
        return {};
    }
    const Down was = *down;
    m_down.erase(down);
    if (was.holdFired) {
        return {};
    }
    // Let go past the time with no tick in between: it was still a hold.
    const bool held = nowMs - was.atMs >= kHoldMs;
    return {token(raw.control, held ? ControlGesture::Hold : ControlGesture::Press)};
}

QList<DeviceEvent> GestureEngine::onTick(qint64 nowMs)
{
    QList<DeviceEvent> events;
    for (auto it = m_down.begin(); it != m_down.end(); ++it) {
        if (!it->holdFired && nowMs - it->atMs >= kHoldMs) {
            it->holdFired = true;
            events.append(token(it.key(), ControlGesture::Hold));
        }
    }
    return events;
}

void GestureEngine::reset()
{
    m_down.clear();
}

bool GestureEngine::waiting() const
{
    return std::any_of(m_down.cbegin(), m_down.cend(), [](const Down& down) { return !down.holdFired; });
}

} // namespace NereusSDR
