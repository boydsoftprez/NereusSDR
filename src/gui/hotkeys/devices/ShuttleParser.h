#pragma once

// =================================================================
// src/gui/hotkeys/devices/ShuttleParser.h  (NereusSDR)
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
//   2026-11-06 - Contour Shuttle parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:115-195 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 13. Reads the buttons from bytes 3 and 4, not upstream's
//                 bytes, and decodes the ring, which upstream does not. See the
//                 comment in the .cpp for why.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QtGlobal>

namespace NereusSDR {

class NEREUS_GUI_EXPORT ShuttleParser : public DeviceParser {
public:
    enum class Model { Xpress, Pro2 };

    explicit ShuttleParser(Model model);

    QList<DeviceEvent> parse(QByteArrayView report) override;
    void reset() override;

private:
    Model m_model;
    quint8 m_jog{0};
    bool m_firstReport{true};
    quint16 m_buttons{0};
    int m_ring{0};
};

} // namespace NereusSDR
