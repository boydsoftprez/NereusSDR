// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording disk list.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the disks a recording or the Digital
//             Spectrum Recorder can use (spec 6.5, 6.7). AI tooling:
//             Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

namespace NereusSDR {

struct RecordingVolume {
    QString rootPath;
    QString displayName;
    qint64 freeBytes = 0;
    qint64 totalBytes = 0;
    bool isSdCard = false;
    bool isCoreOwnDisk = false;
};

class NEREUS_CORE_EXPORT RecordingDisks {
public:
    // The Core's own disk first, always; then the other mounted volumes
    // that are valid, ready, writable, not read-only and not a pseudo file
    // system.
    static QList<RecordingVolume> volumes();
    // A Linux MMC block device (/dev/mmcblk*, major 179 in the kernel's
    // Documentation/admin-guide/devices.txt): an SD card or eMMC.
    static bool isSdCardDevice(const QByteArray& device);
    // True for a file system type that holds no real disk (tmpfs, proc, ...).
    static bool isPseudoFileSystem(const QByteArray& fileSystemType);
    // The root of the volume that holds the Core's own data folder.
    static QString coreOwnRootPath();
};

}  // namespace NereusSDR
