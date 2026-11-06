// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Which USB devices are plugged in, read without opening any of them. On macOS this
// reads the IOKit registry and never touches IOHIDManager, which is what lets a
// window with no knobs avoid the Input Monitoring prompt.
#pragma once

#include "gui/NereusGuiExport.h"

#include <QSet>
#include <QtGlobal>

class QMutex;

namespace NereusSDR {

class NEREUS_GUI_EXPORT UsbPresence {
public:
    static quint32 key(quint16 vendorId, quint16 productId)
    {
        return (static_cast<quint32>(vendorId) << 16) | productId;
    }

    static QSet<quint32> connectedVidPids();    // no HID open; no permission prompt

    // hidapi's global calls (hid_enumerate, hid_open, hid_close, hid_exit) are not
    // thread safe, and this is called from more than one thread.
    static QMutex& hidApiLock();
};

} // namespace NereusSDR
