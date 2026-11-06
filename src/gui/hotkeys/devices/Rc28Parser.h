#pragma once

// =================================================================
// src/gui/hotkeys/devices/Rc28Parser.h  (NereusSDR)
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
//   2026-11-06 - Icom RC-28 parser built on AetherSDR
//                 src/core/HidDeviceParser.cpp:43-90 [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 13. Reads the button bits one by one where upstream compares
//                 the whole byte, so a hold sees its release with another
//                 button down, and reports rotation with a button held.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QByteArray>
#include <QtGlobal>

namespace NereusSDR {

class NEREUS_GUI_EXPORT Rc28Parser : public DeviceParser {
public:
    QList<DeviceEvent> parse(QByteArrayView report) override;
    void reset() override;

private:
    quint8 m_buttons{0x07};     // active-low, so all released
};

// The output report that sets the four lights. true is lit.
NEREUS_GUI_EXPORT QByteArray rc28LightReport(bool tx, bool f1, bool f2, bool link);

} // namespace NereusSDR
