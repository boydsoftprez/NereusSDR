#pragma once

// =================================================================
// src/gui/hotkeys/devices/TMate2Parser.h  (NereusSDR)
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
//   2026-11-06 - TMate 2 parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:270-329 and
//                 src/core/HidDeviceParser.h:106-132 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 14. Gives every change in a report, where upstream gives the
//                 first and leaves the rest for the next report.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QtGlobal>

#include <array>

namespace NereusSDR {

class NEREUS_GUI_EXPORT TMate2Parser : public DeviceParser {
public:
    QList<DeviceEvent> parse(QByteArrayView report) override;
    void reset() override;

private:
    std::array<quint16, 3> m_encoders{};
    quint16 m_keys{0x01FF};     // active-low, so all released
    bool m_seeded{false};       // the counters are absolute, so the first report only sets them
};

} // namespace NereusSDR
