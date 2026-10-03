#include "SettingsBackupTransfer.h"

#include <QCryptographicHash>

#include <algorithm>
#include <utility>

namespace NereusSDR {
namespace {

bool fail(QString* error, const QString& reason)
{
    if (error) { *error = reason; }
    return false;
}

void succeed(QString* error)
{
    if (error) { error->clear(); }
}

} // namespace

bool SettingsBackupTransferSource::create(const QByteArray& snapshot,
                                          SettingsBackupTransferSource* output, QString* error)
{
    if (!output) { return fail(error, QStringLiteral("Missing snapshot output")); }
    if (snapshot.isEmpty() || snapshot.size() > kMaxBytes) {
        return fail(error, QStringLiteral("Snapshot must contain 1 to 16 MiB of XML bytes"));
    }
    SettingsBackupTransferSource next;
    // A QByteArray may wrap caller-owned storage via fromRawData(). Take an
    // owning copy so later caller mutation or release cannot change the bytes
    // promised by the manifest digest.
    next.m_snapshot = QByteArray(snapshot.constData(), snapshot.size());
    next.m_manifest.byteLength = snapshot.size();
    next.m_manifest.sha256 = QCryptographicHash::hash(next.m_snapshot, QCryptographicHash::Sha256);
    *output = std::move(next);
    succeed(error);
    return true;
}

bool SettingsBackupTransferSource::readChunk(qint64 offset, qint64 requestedLength,
                                             QByteArray* output, QString* error) const
{
    if (!output) { return fail(error, QStringLiteral("Missing chunk output")); }
    if (m_snapshot.isEmpty() || offset < 0 || offset >= m_manifest.byteLength
        || requestedLength < 1 || requestedLength > kMaxChunkBytes) {
        return fail(error, QStringLiteral("Invalid snapshot chunk range"));
    }
    const qint64 length = std::min(requestedLength, m_manifest.byteLength - offset);
    *output = m_snapshot.mid(qsizetype(offset), qsizetype(length));
    succeed(error);
    return true;
}

bool SettingsBackupTransferAssembler::begin(const SettingsBackupTransferManifest& manifest,
                                            QString* error)
{
    if (manifest.byteLength < 1 || manifest.byteLength > kMaxBytes
        || manifest.sha256.size() != 32) {
        return fail(error, QStringLiteral("Invalid snapshot manifest size or SHA-256 digest"));
    }
    // Replace the old transaction only after the new manifest passes bounds.
    cancel();
    m_manifest = manifest;
    m_active = true;
    succeed(error);
    return true;
}

bool SettingsBackupTransferAssembler::acceptChunk(qint64 offset, const QByteArray& chunk,
                                                  QString* error)
{
    if (!m_active || m_finished) {
        return fail(error, QStringLiteral("No snapshot transfer is awaiting chunks"));
    }
    const qint64 expected = m_bytes.size();
    if (offset < 0 || offset != expected || chunk.isEmpty()
        || chunk.size() > kMaxChunkBytes
        || qint64(chunk.size()) > m_manifest.byteLength - expected) {
        return fail(error, QStringLiteral("Snapshot chunk is out of order or exceeds its bounds"));
    }
    m_bytes.append(chunk);
    if (m_bytes.size() == m_manifest.byteLength) {
        if (QCryptographicHash::hash(m_bytes, QCryptographicHash::Sha256)
            != m_manifest.sha256) {
            cancel();
            return fail(error, QStringLiteral("Snapshot SHA-256 digest does not match"));
        }
        m_completed = std::move(m_bytes);
        m_bytes.clear();
        m_finished = true;
    }
    succeed(error);
    return true;
}

bool SettingsBackupTransferAssembler::completedPayload(QByteArray* output, QString* error) const
{
    if (!output) { return fail(error, QStringLiteral("Missing completed snapshot output")); }
    if (!m_finished) {
        return fail(error, QStringLiteral("Snapshot transfer is not complete"));
    }
    *output = m_completed;
    succeed(error);
    return true;
}

void SettingsBackupTransferAssembler::cancel()
{
    m_bytes.clear();
    m_bytes.squeeze();
    m_completed.clear();
    m_completed.squeeze();
    m_manifest = {};
    m_active = false;
    m_finished = false;
}

} // namespace NereusSDR
