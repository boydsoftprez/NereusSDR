#pragma once

// =================================================================
// src/gui/hotkeys/devices/DeviceOutput.h  (NereusSDR)
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
//   2026-11-06 - TMate 2 frame and Stream Deck+ packets built on AetherSDR
//                 src/core/HidEncoderManager.cpp:13-38,105-136,282-294,
//                 299-314,535-546,644-761 and
//                 src/gui/MainWindow_Controllers.cpp:1111-1197 [@1e0718ad] for
//                 NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code (Claude Opus 5.5).
//                 Hot keys plan Task 14. Builds bytes for the USB worker to
//                 send, where upstream writes to the device as it builds them.
//                 The main digits and three fixed segments only. Key and strip
//                 images are drawn for this design.
//   2026-11-06 - The off state for hot keys plan Task 17, J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code
//                 (Claude Opus 5.5).
// =================================================================

// What goes out to a display or a lit key, as the bytes to write. Everything here runs
// on the main thread. The USB worker is only ever handed the result, and never sees an
// image (hot keys plan Task 14).

#include "gui/NereusGuiExport.h"

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QList>
#include <QString>

namespace NereusSDR::DeviceOutput {

inline constexpr int kStreamDeckKeyCount = 8;
inline constexpr int kStreamDeckPacketSize = 1024;

// frequency is the state feed's text, "14.250.000". With no digits the display is blank.
NEREUS_GUI_EXPORT QByteArray tmate2Frame(const QString& frequency, bool connected, bool locked);
// The display blank, its lights off and its backlight dark.
NEREUS_GUI_EXPORT QByteArray tmate2OffFrame();

// key counts from 0. An image of another size is scaled to fit.
NEREUS_GUI_EXPORT QList<QByteArray> streamDeckKeyImage(int key, const QImage& image120);
NEREUS_GUI_EXPORT QList<QByteArray> streamDeckStrip(const QImage& image800x100);

// The JPEG split into packets, without the encoding above.
NEREUS_GUI_EXPORT QList<QByteArray> streamDeckKeyPackets(int key, const QByteArray& jpeg);
NEREUS_GUI_EXPORT QList<QByteArray> streamDeckStripPackets(const QByteArray& jpeg);

// What the keys and the strip show.
NEREUS_GUI_EXPORT QImage renderStreamDeckKey(const QString& label, const QColor& background);
NEREUS_GUI_EXPORT QImage renderStreamDeckStrip(const QString& slice, const QString& frequency, const QString& mode,
                                               const QString& filter);
NEREUS_GUI_EXPORT QImage renderStreamDeckBlankStrip();

} // namespace NereusSDR::DeviceOutput
