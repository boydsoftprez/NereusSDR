// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording store.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the Core's recording store: one folder
//             and its recordings.json index (spec 6.4). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; a whole-pan stretch keeps its pan and
//             slice A's filter in the index (recording plan Task 25). AI
//             tooling: Anthropic Claude Code.

#include "RecordingStore.h"

#include "core/LogCategories.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStorageInfo>
#include <QUuid>

#include <algorithm>

namespace NereusSDR {

namespace {

QJsonObject toJson(const RecordingEntry& e)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), e.id);
    o.insert(QStringLiteral("kind"), recordingKindKey(e.kind));
    o.insert(QStringLiteral("sliceId"), e.sliceId);
    o.insert(QStringLiteral("startUtc"), e.startUtc.toUTC().toString(Qt::ISODateWithMs));
    o.insert(QStringLiteral("lengthMs"), double(e.lengthMs));
    o.insert(QStringLiteral("frequencyHz"), double(e.frequencyHz));
    o.insert(QStringLiteral("mode"), e.mode);
    o.insert(QStringLiteral("call"), e.call);
    o.insert(QStringLiteral("name"), e.name);
    o.insert(QStringLiteral("fileName"), e.fileName);
    o.insert(QStringLiteral("sizeBytes"), double(e.sizeBytes));
    // Recording plan Task 25: a whole-pan stretch's pan and slice A's filter.
    if (e.kind == RecordingKind::WholePan) {
        o.insert(QStringLiteral("centreHz"), double(e.centreHz));
        o.insert(QStringLiteral("spanHz"), double(e.spanHz));
        o.insert(QStringLiteral("filterLowHz"), e.filterLowHz);
        o.insert(QStringLiteral("filterHighHz"), e.filterHighHz);
    }
    return o;
}

bool fromJson(const QJsonObject& o, RecordingEntry* e)
{
    e->id = o.value(QStringLiteral("id")).toString();
    e->fileName = o.value(QStringLiteral("fileName")).toString();
    if (e->id.isEmpty() || e->fileName.isEmpty()
        || !recordingKindFromKey(o.value(QStringLiteral("kind")).toString(), &e->kind)) {
        return false;
    }
    e->sliceId = o.value(QStringLiteral("sliceId")).toInt(-1);
    e->startUtc = QDateTime::fromString(o.value(QStringLiteral("startUtc")).toString(), Qt::ISODateWithMs)
                      .toUTC();
    e->lengthMs = qint64(o.value(QStringLiteral("lengthMs")).toDouble());
    e->frequencyHz = qint64(o.value(QStringLiteral("frequencyHz")).toDouble());
    e->mode = o.value(QStringLiteral("mode")).toString();
    e->call = o.value(QStringLiteral("call")).toString();
    e->name = o.value(QStringLiteral("name")).toString();
    if (e->name.isEmpty()) {
        e->name = QFileInfo(e->fileName).completeBaseName();
    }
    e->sizeBytes = qint64(o.value(QStringLiteral("sizeBytes")).toDouble());
    e->centreHz = qint64(o.value(QStringLiteral("centreHz")).toDouble());
    e->spanHz = qint64(o.value(QStringLiteral("spanHz")).toDouble());
    e->filterLowHz = o.value(QStringLiteral("filterLowHz")).toInt();
    e->filterHighHz = o.value(QStringLiteral("filterHighHz")).toInt();
    return true;
}

QString normalizedSuffix(const QString& suffix)
{
    QString s = suffix;
    while (s.startsWith(QLatin1Char('.'))) {
        s.remove(0, 1);
    }
    return s;
}

}  // namespace

RecordingStore::RecordingStore(QObject* parent)
    : QObject(parent)
{
}

bool RecordingStore::openFolder(const QString& folder, QString* error)
{
    if (folder.isEmpty() || !QDir().mkpath(folder)) {
        if (error) {
            *error = QStringLiteral("The recordings folder cannot be made: %1").arg(folder);
        }
        qCWarning(lcRecordings) << "cannot make the recordings folder" << folder;
        return false;
    }

    QList<RecordingEntry> loaded;
    const QString dir = QDir(folder).absolutePath();
    QFile index(QDir(dir).filePath(QString::fromLatin1(kIndexFileName)));
    if (index.exists()) {
        if (!index.open(QIODevice::ReadOnly)) {
            if (error) {
                *error = QStringLiteral("The recordings list cannot be read: %1").arg(index.errorString());
            }
            qCWarning(lcRecordings) << "cannot read" << index.fileName() << index.errorString();
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(index.readAll(), &parseError);
        index.close();
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            // Keep the unreadable list beside the folder rather than writing
            // over it; the recordings themselves stay where they are.
            const QString aside = index.fileName() + QStringLiteral(".unreadable-")
                + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"));
            QFile::rename(index.fileName(), aside);
            qCWarning(lcRecordings) << "recordings list unreadable, kept as" << aside
                                    << parseError.errorString();
        } else {
            const QJsonArray rows = doc.object().value(QStringLiteral("recordings")).toArray();
            for (const QJsonValue& v : rows) {
                RecordingEntry e;
                if (!fromJson(v.toObject(), &e)) {
                    qCWarning(lcRecordings) << "recordings list row skipped, it is incomplete";
                    continue;
                }
                const QFileInfo file(QDir(dir).filePath(e.fileName));
                if (!file.isFile()) {
                    qCWarning(lcRecordings) << "recording" << e.name << "dropped, its file is gone:"
                                            << file.filePath();
                    continue;
                }
                e.sizeBytes = file.size();
                loaded.append(e);
            }
        }
    }

    m_folder = dir;
    m_entries = loaded;
    emit changed();
    return true;
}

QString RecordingStore::folder() const
{
    return m_folder;
}

QList<RecordingEntry> RecordingStore::entries() const
{
    QList<RecordingEntry> out = m_entries;
    std::stable_sort(out.begin(), out.end(), [](const RecordingEntry& a, const RecordingEntry& b) {
        return a.startUtc > b.startUtc;
    });
    return out;
}

std::optional<RecordingEntry> RecordingStore::find(const QString& id) const
{
    const int i = indexOf(id);
    if (i < 0) {
        return std::nullopt;
    }
    return m_entries.at(i);
}

QString RecordingStore::pathForNew(const QString& baseName, const QString& suffix) const
{
    const QString ext = normalizedSuffix(suffix);
    const QString dotExt = ext.isEmpty() ? QString() : QLatin1Char('.') + ext;
    QString name = baseName;
    for (int n = 2;; ++n) {
        const QString path = pathOf(name + dotExt);
        if (!QFileInfo::exists(path) && !nameTaken(name, QString())) {
            return path;
        }
        name = QStringLiteral("%1 (%2)").arg(baseName).arg(n);
    }
}

QString RecordingStore::add(const RecordingEntry& e)
{
    RecordingEntry row = e;
    row.fileName = QFileInfo(e.fileName).fileName();
    if (row.id.isEmpty()) {
        row.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (row.name.isEmpty()) {
        row.name = QFileInfo(row.fileName).completeBaseName();
    }
    const QFileInfo file(pathOf(row.fileName));
    if (file.isFile()) {
        row.sizeBytes = file.size();
    }
    const int existing = indexOf(row.id);
    if (existing >= 0) {
        m_entries[existing] = row;
    } else {
        m_entries.append(row);
    }
    QString error;
    if (!saveIndex(&error)) {
        qCWarning(lcRecordings) << "recording" << row.name << "kept, but the list is not saved:" << error;
    }
    emit changed();
    return row.id;
}

bool RecordingStore::rename(const QString& id, const QString& newName, QString* error)
{
    auto refuse = [error](const QString& why) {
        if (error) {
            *error = why;
        }
        return false;
    };
    const int i = indexOf(id);
    if (i < 0) {
        return refuse(QStringLiteral("That recording is no longer on the Core."));
    }
    QString reason;
    if (!isValidRecordingName(newName, &reason)) {
        return refuse(reason);
    }
    RecordingEntry& row = m_entries[i];
    if (newName == row.name) {
        return true;
    }
    if (nameTaken(newName, id)) {
        return refuse(QStringLiteral("Another recording already has that name."));
    }
    const QString suffix = QFileInfo(row.fileName).suffix();
    const QString newFileName = suffix.isEmpty() ? newName : newName + QLatin1Char('.') + suffix;
    const QString from = pathOf(row.fileName);
    const QString to = pathOf(newFileName);
    const bool caseOnly = newFileName.compare(row.fileName, Qt::CaseInsensitive) == 0;
    if (!caseOnly && QFileInfo::exists(to)) {
        return refuse(QStringLiteral("A file with that name is already in the recordings folder."));
    }
    QFile file(from);
    if (!file.rename(to)) {
        qCWarning(lcRecordings) << "cannot rename" << from << "to" << to << file.errorString();
        return refuse(QStringLiteral("The file cannot be renamed: %1").arg(file.errorString()));
    }
    row.name = newName;
    row.fileName = newFileName;
    QString saveError;
    if (!saveIndex(&saveError)) {
        qCWarning(lcRecordings) << "recording renamed, but the list is not saved:" << saveError;
    }
    emit changed();
    return true;
}

bool RecordingStore::remove(const QStringList& ids, QString* error)
{
    QStringList failed;
    bool any = false;
    for (const QString& id : ids) {
        const int i = indexOf(id);
        if (i < 0) {
            continue;
        }
        const QString path = pathOf(m_entries.at(i).fileName);
        if (QFileInfo::exists(path)) {
            QFile file(path);
            if (!file.remove()) {
                qCWarning(lcRecordings) << "cannot delete" << path << file.errorString();
                failed << m_entries.at(i).name;
                continue;
            }
        }
        m_entries.removeAt(i);
        any = true;
    }
    if (any) {
        QString saveError;
        if (!saveIndex(&saveError)) {
            qCWarning(lcRecordings) << "recordings deleted, but the list is not saved:" << saveError;
        }
        emit changed();
    }
    if (!failed.isEmpty()) {
        if (error) {
            *error = QStringLiteral("These recordings could not be deleted: %1")
                         .arg(failed.join(QStringLiteral(", ")));
        }
        return false;
    }
    return true;
}

qint64 RecordingStore::usedBytes() const
{
    qint64 total = 0;
    for (const RecordingEntry& e : m_entries) {
        total += e.sizeBytes;
    }
    return total;
}

qint64 RecordingStore::freeBytes() const
{
    if (m_folder.isEmpty()) {
        return -1;
    }
    const QStorageInfo storage(m_folder);
    return storage.isValid() ? storage.bytesAvailable() : -1;
}

bool RecordingStore::saveIndex(QString* error) const
{
    if (m_folder.isEmpty()) {
        if (error) {
            *error = QStringLiteral("no recordings folder is open");
        }
        return false;
    }
    QJsonArray rows;
    for (const RecordingEntry& e : m_entries) {
        rows.append(toJson(e));
    }
    QJsonObject root;
    root.insert(QStringLiteral("format"), kIndexFormat);
    root.insert(QStringLiteral("recordings"), rows);

    QSaveFile file(QDir(m_folder).filePath(QString::fromLatin1(kIndexFileName)));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

int RecordingStore::indexOf(const QString& id) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

bool RecordingStore::nameTaken(const QString& name, const QString& exceptId) const
{
    for (const RecordingEntry& e : m_entries) {
        if (e.id != exceptId && e.name.compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString RecordingStore::pathOf(const QString& fileName) const
{
    return QDir(m_folder).filePath(fileName);
}

}  // namespace NereusSDR
