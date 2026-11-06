#pragma once

// =================================================================
// src/gui/hotkeys/devices/PowerMateParser.h  (NereusSDR)
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
//   2026-11-06 - Griffin PowerMate parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:92-113 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 13. A press and a turn in one report give both events.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

namespace NereusSDR {

class NEREUS_GUI_EXPORT PowerMateParser : public DeviceParser {
public:
    QList<DeviceEvent> parse(QByteArrayView report) override;
    void reset() override;

private:
    bool m_down{false};
};

} // namespace NereusSDR
