// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording disk list.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the disks a recording or the Digital
//             Spectrum Recorder can use (spec 6.5, 6.7). AI tooling:
//             Anthropic Claude Code.

#include "RecordingDisks.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>

#include <algorithm>
#include <array>

namespace NereusSDR {

namespace {

constexpr std::array<const char*, 26> kPseudoFileSystems = {
    "autofs", "binfmt_misc", "bpf", "cgroup", "cgroup2", "configfs", "debugfs",
    "devfs", "devpts", "devtmpfs", "efivarfs", "fusectl", "hugetlbfs", "mqueue",
    "nsfs", "nullfs", "overlay", "proc", "pstore", "ramfs", "rpc_pipefs",
    "securityfs", "squashfs", "sysfs", "tmpfs", "tracefs",
};

}  // namespace

bool RecordingDisks::isSdCardDevice(const QByteArray& device)
{
    return device.startsWith("/dev/mmcblk");
}

bool RecordingDisks::isPseudoFileSystem(const QByteArray& fileSystemType)
{
    const QByteArray type = fileSystemType.toLower();
    return std::any_of(kPseudoFileSystems.begin(), kPseudoFileSystems.end(),
                       [&type](const char* pseudo) { return type == pseudo; });
}

namespace {

// The volume that holds the Core's own data folder. The folder may not
// exist yet; its nearest existing parent is on the same disk.
QStorageInfo coreOwnStorage()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    while (!path.isEmpty() && !QFileInfo::exists(path)) {
        const QString parent = QFileInfo(path).absolutePath();
        if (parent == path) {
            break;
        }
        path = parent;
    }
    return QStorageInfo(path.isEmpty() ? QDir::homePath() : path);
}

RecordingVolume volumeFrom(const QStorageInfo& storage, bool own)
{
    RecordingVolume v;
    v.rootPath = storage.rootPath();
    v.displayName = storage.displayName();
    v.freeBytes = storage.bytesAvailable();
    v.totalBytes = storage.bytesTotal();
    v.isSdCard = RecordingDisks::isSdCardDevice(storage.device());
    v.isCoreOwnDisk = own;
    return v;
}

}  // namespace

QString RecordingDisks::coreOwnRootPath()
{
    const QStorageInfo storage = coreOwnStorage();
    return storage.isValid() ? storage.rootPath() : QString();
}

QList<RecordingVolume> RecordingDisks::volumes()
{
    QList<RecordingVolume> out;
    // The Core's own disk always comes first, whether or not the system
    // lists it: on macOS the data volume is a hidden one, which
    // mountedVolumes() leaves out.
    const QStorageInfo own = coreOwnStorage();
    const QString ownRoot = own.isValid() ? own.rootPath() : QString();
    if (own.isValid() && own.isReady()) {
        out.append(volumeFrom(own, true));
    }
    for (const QStorageInfo& storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady() || storage.isReadOnly()
            || isPseudoFileSystem(storage.fileSystemType())) {
            continue;
        }
        const QString root = storage.rootPath();
        if (root == ownRoot) {
            continue;
        }
#if defined(Q_OS_MAC)
        // The system's own APFS volumes (Preboot, VM, Update, ...) are not
        // places to keep recordings.
        if (root.startsWith(QStringLiteral("/System/Volumes/"))) {
            continue;
        }
#endif
        if (!QFileInfo(root).isWritable()) {
            continue;
        }
        out.append(volumeFrom(storage, false));
    }
    return out;
}

}  // namespace NereusSDR
