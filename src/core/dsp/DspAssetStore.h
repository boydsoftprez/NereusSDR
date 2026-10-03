#pragma once
// =================================================================
// src/core/dsp/DspAssetStore.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original station asset storage.
//
// Modification history (NereusSDR):
//   2026-09-21 — Created for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via OpenAI Codex.
// =================================================================

#include "DspAssetValidation.h"

#include <QHash>
#include <QList>
#include <QString>

class QIODevice;

namespace NereusSDR {

struct DspAssetRecord {
    QString id;
    DspAssetKind kind{DspAssetKind::NnrModel};
    QString hashHex;
    qint64 size{0};
    QString format;
    int version{0};
    QString compatibility;
    QString numericEncoding;
    QString label;
    QString radioIdentity;
    bool valid{false};
    QString validationError;
};

struct DspAssetImportResult {
    bool accepted{false};
    QString error;
    DspAssetRecord record;
};

// Stores validated model and correction bytes beneath an explicitly supplied
// station-profile directory. Asset IDs are SHA-256 identities; callers never
// choose or resolve a client-provided filename inside the store.
class DspAssetStore final
{
public:
    static constexpr qsizetype kTransferChunkBytes =
        DspAssetValidation::kTransferChunkBytes;

    explicit DspAssetStore(const QString& stationProfileRoot);

    bool isValid() const { return m_valid; }
    QString lastError() const { return m_lastError; }
    QString rootDirectory() const { return m_rootDirectory; }

    QList<DspAssetRecord> assets() const;
    // Manifest label for an id, without re-reading or re-validating the file.
    QString label(const QString& id) const;

    DspAssetImportResult importBytes(DspAssetKind kind, const QString& label,
                                     const QByteArray& bytes,
                                     const QString& radioIdentity = {});

    // Staged imports bound each decoded transfer chunk to 64 KiB. The token is
    // opaque and is looked up in process memory; it is never treated as a path.
    QString beginImport(DspAssetKind kind, const QString& label,
                        const QString& radioIdentity = {}, QString* error = nullptr);
    bool appendImport(const QString& token, const QByteArray& chunk, QString* error = nullptr);
    DspAssetImportResult finishImport(const QString& token);
    void cancelImport(const QString& token);

    // Resolve verifies the stored bytes, hash, format metadata, requested kind
    // and optional correction radio identity before returning an internal path.
    QString resolvePath(const QString& id, DspAssetKind expectedKind,
                        const QString& radioIdentity = {}, QString* error = nullptr) const;
    bool exportAsset(const QString& id, QIODevice* destination, QString* error = nullptr) const;

private:
    struct PendingImport {
        DspAssetKind kind{DspAssetKind::NnrModel};
        QString label;
        QString radioIdentity;
        QString path;
        qint64 size{0};
    };

    bool loadManifest();
    bool saveManifest(QString* error = nullptr) const;
    bool verifyRecord(const DspAssetRecord& record, QByteArray* bytes,
                      QString* error = nullptr) const;
    QString assetPath(const DspAssetRecord& record) const;
    QString assetPath(DspAssetKind kind, const QString& hashHex) const;
    int recordIndex(const QString& id) const;
    qint64 sizeLimit(DspAssetKind kind) const;
    void cleanInterruptedImports();

    QString m_rootDirectory;
    QString m_assetsDirectory;
    QString m_stagingDirectory;
    QString m_manifestPath;
    bool m_valid{false};
    QString m_lastError;
    QList<DspAssetRecord> m_records;
    QHash<QString, PendingImport> m_pending;
};

} // namespace NereusSDR
