// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original. See RemoteRecordingsAccess.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: recordings.list
//                 carries the reader, its slice and kind; the Core's answer
//                 names the reader, so each reader gets its own page.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "models/recordings/RemoteRecordingsAccess.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

#include "core/LogCategories.h"
#include "core/session/IStationLink.h"
#include "core/session/RecordingsFacade.h"
#include "models/RadioModel.h"

namespace NereusSDR {

namespace {

QVariant valueOf(const QList<MirrorUpdate>& values, const char* name)
{
    for (const MirrorUpdate& value : values) {
        if (value.name == name) {
            return value.value;
        }
    }
    return {};
}

const QString kCopyWentWrong = QStringLiteral("The copy from the Core went wrong. Try again.");

}  // namespace

RemoteRecordingsAccess::RemoteRecordingsAccess(RadioModel* model)
    : RecordingsAccess(model)
    , m_model(model)
{
    connect(model->recordingsFacade(), &RecordingsFacade::changed, this,
            &RemoteRecordingsAccess::onFacadeChanged);
    connect(model, &RadioModel::stationLinkStateChanged, this, &RemoteRecordingsAccess::refreshState);
}

RemoteRecordingsAccess::~RemoteRecordingsAccess()
{
    for (const std::shared_ptr<Download>& download : m_downloads) {
        if (download->file) {
            download->file->cancelWriting();
        }
    }
}

RecordingsAccess::State RemoteRecordingsAccess::state() const
{
    IStationLink* const link = m_model ? m_model->stationLink() : nullptr;
    if (link == nullptr || !link->stationLinkReady()) {
        return State::NotConnected;
    }
    if (link->recordingsVersion() < 1) {
        return State::NeedsNewerCore;
    }
    // Offered, and the document not here yet.
    return m_document.isEmpty() ? State::NotConnected : State::Ready;
}

int RemoteRecordingsAccess::coreVersion() const
{
    IStationLink* const link = m_model ? m_model->stationLink() : nullptr;
    return link != nullptr ? link->recordingsVersion() : 0;
}

QString RemoteRecordingsAccess::unavailableReason(int needsVersion) const
{
    const State now = state();
    if (now == State::NotConnected) {
        return notConnectedReason();
    }
    return now == State::NeedsNewerCore || coreVersion() < needsVersion ? needsNewerCoreReason()
                                                                        : QString();
}

bool RemoteRecordingsAccess::coreIsOnThisComputer() const
{
    IStationLink* const link = m_model ? m_model->stationLink() : nullptr;
    return link != nullptr && link->coreIsOnThisComputer();
}

bool RemoteRecordingsAccess::send(const QString& verb, const QJsonObject& arguments)
{
    if (state() != State::Ready) {
        emit refused(unavailableReason());
        return false;
    }
    const IStationLink::CommandOutcome outcome =
        m_model->stationLink()->sendRecordingsCommand(verb, arguments);
    // What the Core refuses comes back through takeResult.
    if (!outcome.sent) {
        emit refused(outcome.reason);
    }
    return outcome.sent;
}

void RemoteRecordingsAccess::start(int sliceId)
{
    send(QStringLiteral("recordings.start"), QJsonObject{{QStringLiteral("slice"), sliceId}});
}

void RemoteRecordingsAccess::stop(int sliceId)
{
    send(QStringLiteral("recordings.stop"), QJsonObject{{QStringLiteral("slice"), sliceId}});
}

void RemoteRecordingsAccess::requestList(int reader, const RecordingsListQuery& query)
{
    QJsonObject arguments{{QStringLiteral("offset"), std::max(0, query.offset)},
                          {QStringLiteral("count"), std::max(1, query.count)},
                          {QStringLiteral("reader"), reader}};
    if (query.sliceId.has_value()) {
        arguments.insert(QStringLiteral("slice"), *query.sliceId);
    }
    if (query.kind.has_value()) {
        arguments.insert(QStringLiteral("kind"), recordingKindKey(*query.kind));
    }
    send(QStringLiteral("recordings.list"), arguments);
}

void RemoteRecordingsAccess::play(const QString& id)
{
    send(QStringLiteral("recordings.play"), QJsonObject{{QStringLiteral("id"), id}});
}

void RemoteRecordingsAccess::stopPlay()
{
    send(QStringLiteral("recordings.stopPlay"), QJsonObject{});
}

void RemoteRecordingsAccess::seek(qint64 ms)
{
    send(QStringLiteral("recordings.seek"),
         QJsonObject{{QStringLiteral("positionMs"), std::max<qint64>(0, ms)}});
}

void RemoteRecordingsAccess::rename(const QString& id, const QString& name)
{
    send(QStringLiteral("recordings.rename"),
         QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}});
}

void RemoteRecordingsAccess::remove(const QStringList& ids)
{
    send(QStringLiteral("recordings.delete"),
         QJsonObject{{QStringLiteral("ids"), QJsonArray::fromStringList(ids)}});
}

void RemoteRecordingsAccess::setSettings(const QJsonObject& changed)
{
    send(QStringLiteral("recordings.setSettings"), changed);
}

void RemoteRecordingsAccess::download(const QString& id, const QString& toPath)
{
    if (state() != State::Ready) {
        emit downloadFinished(id, false, unavailableReason());
        return;
    }
    const bool busy = std::any_of(m_downloads.cbegin(), m_downloads.cend(),
                                  [&id](const std::shared_ptr<Download>& d) { return d->id == id; });
    if (busy) {
        emit downloadFinished(id, false, QStringLiteral("That recording is already being copied."));
        return;
    }
    auto download = std::make_shared<Download>();
    download->id = id;
    download->file = std::make_unique<QSaveFile>(toPath);
    if (!download->file->open(QIODevice::WriteOnly)) {
        qCWarning(lcRecordings) << "cannot write a copy to" << toPath << download->file->errorString();
        emit downloadFinished(id, false, QStringLiteral("The copy cannot be saved there."));
        return;
    }
    m_downloads.append(download);
    if (!requestChunk(*download)) {
        finishDownload(id, false, QStringLiteral("The copy could not be asked for. Try again."));
    }
}

bool RemoteRecordingsAccess::requestChunk(Download& download)
{
    IStationLink* const link = m_model ? m_model->stationLink() : nullptr;
    if (link == nullptr) {
        return false;
    }
    const IStationLink::CommandOutcome outcome = link->sendRecordingsCommand(
        QStringLiteral("recordings.export"),
        QJsonObject{{QStringLiteral("id"), download.id}, {QStringLiteral("offset"), download.done}});
    if (outcome.sent) {
        m_exportsInFlight.append(download.id);
    }
    return outcome.sent;
}

void RemoteRecordingsAccess::finishDownload(const QString& id, bool ok, const QString& error)
{
    const auto it = std::find_if(m_downloads.begin(), m_downloads.end(),
                                 [&id](const std::shared_ptr<Download>& d) { return d->id == id; });
    if (it == m_downloads.end()) {
        return;
    }
    const std::shared_ptr<Download> download = *it;
    m_downloads.erase(it);
    bool done = ok;
    QString why = error;
    if (done && !download->file->commit()) {
        qCWarning(lcRecordings) << "cannot finish the copy" << download->file->fileName()
                                << download->file->errorString();
        done = false;
        why = QStringLiteral("The copy cannot be saved there.");
    } else if (!done) {
        download->file->cancelWriting();
    }
    emit downloadFinished(id, done, why);
}

void RemoteRecordingsAccess::takeExport(bool accepted, const QString& reason,
                                        const QList<MirrorUpdate>& values)
{
    const QString asked = m_exportsInFlight.isEmpty() ? QString() : m_exportsInFlight.takeFirst();
    const QString id = accepted ? valueOf(values, "id").toString() : asked;
    const auto it = std::find_if(m_downloads.begin(), m_downloads.end(),
                                 [&id](const std::shared_ptr<Download>& d) { return d->id == id; });
    if (it == m_downloads.end()) {
        return;
    }
    Download& download = **it;
    if (!accepted) {
        finishDownload(id, false, reason);
        return;
    }
    const qint64 offset = valueOf(values, "offset").toLongLong();
    const qint64 size = valueOf(values, "size").toLongLong();
    const QByteArray::FromBase64Result chunk = QByteArray::fromBase64Encoding(
        valueOf(values, "data").toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!chunk || offset != download.done || size < 0 || offset + chunk.decoded.size() > size
        || (chunk.decoded.isEmpty() && offset < size)) {
        qCWarning(lcRecordings) << "a chunk of recording" << id << "did not fit: offset" << offset
                                << "size" << size << "expected" << download.done;
        finishDownload(id, false, kCopyWentWrong);
        return;
    }
    if (download.file->write(chunk.decoded) != chunk.decoded.size()) {
        qCWarning(lcRecordings) << "cannot write a copy to" << download.file->fileName()
                                << download.file->errorString();
        finishDownload(id, false, QStringLiteral("The copy cannot be saved there."));
        return;
    }
    download.done += chunk.decoded.size();
    download.total = size;
    emit downloadProgress(id, download.done, download.total);
    if (download.done >= download.total) {
        finishDownload(id, true, QString());
    } else if (!requestChunk(download)) {
        finishDownload(id, false, QStringLiteral("The connection to the Core was lost."));
    }
}

void RemoteRecordingsAccess::takeResult(const QByteArray& verb, bool accepted, const QString& reason,
                                        const QList<MirrorUpdate>& values)
{
    if (verb == "recordings.export") {
        takeExport(accepted, reason, values);
        return;
    }
    if (verb == "recordings.playback") {
        const QString id = valueOf(values, "id").toString();
        if (id.isEmpty()) {
            m_playback.reset();
        } else {
            m_playback = PlaybackState{QString(), id, valueOf(values, "positionMs").toLongLong(),
                                       valueOf(values, "lengthMs").toLongLong(),
                                       valueOf(values, "playing").toBool(),
                                       valueOf(values, "onAir").toBool()};
        }
        emit playbackChanged();
        return;
    }
    if (!accepted) {
        emit refused(reason);
        return;
    }
    if (verb == "recordings.list") {
        RecordingsListPage page;
        const QJsonArray rows =
            QJsonDocument::fromJson(valueOf(values, "entries").toString().toUtf8()).array();
        for (const QJsonValue& value : rows) {
            if (const std::optional<RecordingEntry> entry = RecordingsFacade::entryFromJson(value.toObject())) {
                page.entries.append(*entry);
            }
        }
        page.offset = valueOf(values, "offset").toInt();
        page.total = valueOf(values, "total").toInt();
        const int reader = valueOf(values, "reader").toInt();
        m_pages.insert(reader, page);
        emit listChanged(reader);
    }
}

void RemoteRecordingsAccess::onFacadeChanged()
{
    // The mirror sets `json` and `revision` one at a time.
    const QString json = m_model ? m_model->recordingsFacade()->json() : QString();
    if (json != m_json) {
        m_json = json;
        m_document = QJsonDocument::fromJson(json.toUtf8()).object();
        emit documentChanged();
        // The first document of a session reports nothing: that recording is old.
        const QJsonObject keptNow = m_document.value(QStringLiteral("kept")).toObject();
        const qint64 seq = keptNow.value(QStringLiteral("seq")).toInteger();
        if (m_document.isEmpty()) {
            m_seenKept.reset();
        } else if (!m_seenKept.has_value()) {
            m_seenKept = seq;
        } else if (*m_seenKept != seq) {
            m_seenKept = seq;
            emit kept(keptNow);
        }
    }
    refreshState();
}

void RemoteRecordingsAccess::refreshState()
{
    const State now = state();
    if (now == m_lastState) {
        return;
    }
    m_lastState = now;
    if (now != State::Ready) {
        // The session went: no copy can finish, and this window's playback
        // is no longer known.
        m_exportsInFlight.clear();
        while (!m_downloads.isEmpty()) {
            finishDownload(m_downloads.first()->id, false,
                           QStringLiteral("The connection to the Core was lost."));
        }
        if (m_playback.has_value()) {
            m_playback.reset();
            emit playbackChanged();
        }
    }
    emit stateChanged();
}

} // namespace NereusSDR
