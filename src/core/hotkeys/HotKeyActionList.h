// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// The one list of hot key actions. No GUI headers: the phone can use it later.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/hotkeys/HotKeyAction.h"

#include <QList>

namespace NereusSDR {

class NEREUS_CORE_EXPORT HotKeyActionList {
public:
    // In the order of the plan's Action ids table.
    static const QList<HotKeyAction>& all();
    // Null when no action has this id.
    static const HotKeyAction* find(const QString& id);
};

} // namespace NereusSDR
