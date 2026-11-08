// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// One action that a keyboard chord, MIDI control, knob or deck plugin can run.
#pragma once

#include <QString>
#include <QStringList>

namespace NereusSDR {

enum class HotKeyGesture { Press, Hold, Turn };
enum class HotKeyScope { Slice, TransmitSlice, Pan, Window };

struct HotKeyAction {
    QString id;      // stable, goes on the wire to plugins
    QString name;    // shown to the user
    QString group;
    HotKeyGesture gesture;
    bool transmits;
    HotKeyScope scope;
    QStringList defaultKeys;  // canonical chord text
};

} // namespace NereusSDR
