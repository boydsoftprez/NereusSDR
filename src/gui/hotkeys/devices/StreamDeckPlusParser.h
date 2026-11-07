#pragma once

// =================================================================
// src/gui/hotkeys/devices/StreamDeckPlusParser.h  (NereusSDR)
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
//   2026-11-06 - Stream Deck+ parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:196-268 and
//                 src/core/HidDeviceParser.h:82-104 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 14. Reports every dial turned and every key changed in one
//                 report, where upstream reports only the first.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QtGlobal>

namespace NereusSDR {

class NEREUS_GUI_EXPORT StreamDeckPlusParser : public DeviceParser {
public:
    QList<DeviceEvent> parse(QByteArrayView report) override;
    void reset() override;

private:
    bool m_keysSeeded{false};
    bool m_dialsSeeded{false};
    quint8 m_keys{0};       // bitmask of previous LCD key states (bits 0-7)
    quint8 m_dialPresses{0};    // bitmask of previous encoder button states (bits 0-3)
};

} // namespace NereusSDR
