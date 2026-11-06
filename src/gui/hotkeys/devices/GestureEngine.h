// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original. The one upstream fact is cited at kHoldMs.
// Tells a press from a hold on a control that has both. The caller gives it only
// controls with a Hold binding, and the time: it has no timer of its own.
#pragma once

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QtGlobal>

namespace NereusSDR {

class NEREUS_GUI_EXPORT GestureEngine {
public:
    static constexpr int kHoldMs = 600;  // From AetherSDR src/gui/MainWindow_Controllers.cpp:2476-2495 [@1e0718ad]

    // A down says nothing at once. An up before kHoldMs gives a Press token, and
    // an up after a Hold gives nothing. Anything but a button passes through.
    QList<DeviceEvent> onEvent(const DeviceEvent& raw, qint64 nowMs);
    QList<DeviceEvent> onTick(qint64 nowMs);    // a Hold token, once, at kHoldMs

    void reset();                               // the device went away
    bool waiting() const;                       // a control still down with no Hold yet

private:
    struct Down {
        qint64 atMs;
        bool holdFired;
    };
    QMap<QString, Down> m_down;
};

} // namespace NereusSDR
