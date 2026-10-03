#pragma once

#include "core/settings/SettingsBackup.h"

#include <QByteArray>
#include <QString>

namespace NereusSDR {

// Binary pieces of one owner's local XML snapshot. These types know nothing
// about peers or transport. Callers MUST enforce authentication, authorization,
// transfer IDs, session epochs, expiry, and a global memory budget. They MUST
// validate the assembled XML semantically before passing it to an owner.
struct SettingsBackupTransferManifest {
    qint64 byteLength = 0;
    QByteArray sha256;
};

class SettingsBackupTransferSource {
public:
    static constexpr qsizetype kMaxBytes = SettingsBackup::kMaxXmlBytes;
    static constexpr qsizetype kMaxChunkBytes = 256 * 1024;

    static bool create(const QByteArray& snapshot, SettingsBackupTransferSource* output,
                       QString* error = nullptr);
    SettingsBackupTransferManifest manifest() const { return m_manifest; }
    bool readChunk(qint64 offset, qint64 requestedLength, QByteArray* output,
                   QString* error = nullptr) const;

private:
    QByteArray m_snapshot;
    SettingsBackupTransferManifest m_manifest;
};

class SettingsBackupTransferAssembler {
public:
    static constexpr qsizetype kMaxBytes = SettingsBackup::kMaxXmlBytes;
    static constexpr qsizetype kMaxChunkBytes = SettingsBackupTransferSource::kMaxChunkBytes;

    bool begin(const SettingsBackupTransferManifest& manifest, QString* error = nullptr);
    bool acceptChunk(qint64 offset, const QByteArray& chunk, QString* error = nullptr);
    bool completedPayload(QByteArray* output, QString* error = nullptr) const;
    void cancel();
    qint64 expectedOffset() const { return m_finished ? m_manifest.byteLength : m_bytes.size(); }

private:
    QByteArray m_bytes;
    QByteArray m_completed;
    SettingsBackupTransferManifest m_manifest;
    bool m_active = false;
    bool m_finished = false;
};

} // namespace NereusSDR
