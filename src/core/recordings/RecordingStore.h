// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording store.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the Core's recording store: one folder
//             and its recordings.json index (spec 6.4). AI tooling:
//             Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/recordings/RecordingNames.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace NereusSDR {

// The recordings kept in one folder, listed in that folder's
// recordings.json. The store deletes a file only when remove() is asked
// to; a file with no row is left alone, and a row whose file is gone is
// dropped when the folder is opened. Main thread (the Core's model thread).
class NEREUS_CORE_EXPORT RecordingStore : public QObject {
    Q_OBJECT
public:
    static constexpr const char* kIndexFileName = "recordings.json";
    static constexpr int kIndexFormat = 1;

    explicit RecordingStore(QObject* parent = nullptr);

    bool openFolder(const QString& folder, QString* error);   // loads recordings.json, drops missing files
    QString folder() const;
    QList<RecordingEntry> entries() const;                     // newest first
    std::optional<RecordingEntry> find(const QString& id) const;
    QString pathForNew(const QString& baseName, const QString& suffix) const;  // adds " (2)" on collision
    QString add(const RecordingEntry& e);                      // returns id
    bool rename(const QString& id, const QString& newName, QString* error);
    bool remove(const QStringList& ids, QString* error);
    qint64 usedBytes() const;
    qint64 freeBytes() const;

signals:
    void changed();

private:
    bool saveIndex(QString* error) const;
    int indexOf(const QString& id) const;
    bool nameTaken(const QString& name, const QString& exceptId) const;
    QString pathOf(const QString& fileName) const;

    QString m_folder;
    QList<RecordingEntry> m_entries;
};

}  // namespace NereusSDR
