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
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11: the voice
//                 keyer's verbs, a chunked import and export. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 22: rewind, live
//                 and seekPast (rewind sound, recordingsVersion 4).
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 23: the past
//                 picture (past rows in chunks, a past spectrum frame, the
//                 hours map). AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 24: the pan's
//                 pixels go with dsr.rows and dsr.spectrum, and each row's
//                 transmit mark comes back. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: saving a
//                 stretch, sound only and opening a saved stretch.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 26: eraseDsr
//                 (dsr.eraseAll). AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), JJ's ruling on PR 797: importVoice
//                 says whether the file was imported (fixed level on the
//                 air) or recorded on the mic. AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

#include "models/recordings/RemoteRecordingsAccess.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

#include "core/LogCategories.h"
#include "core/recordings/VoiceKeyer.h"
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
const QString kLinkLost = QStringLiteral("The connection to the Core was lost.");

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
    for (const std::shared_ptr<VoiceExport>& voiceExport : m_voiceExports) {
        if (voiceExport->file) {
            voiceExport->file->cancelWriting();
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

bool RemoteRecordingsAccess::sendVoiceVerb(const QString& verb, const QJsonObject& arguments)
{
    if (state() == State::Ready && coreVersion() < kVoiceKeyerVersion) {
        emit refused(needsNewerCoreReason());
        return false;
    }
    return send(verb, arguments);
}

bool RemoteRecordingsAccess::sendDsrVerb(const QString& verb, const QJsonObject& arguments)
{
    if (state() == State::Ready && coreVersion() < kDsrVersion) {
        emit refused(needsNewerCoreReason());
        return false;
    }
    return send(verb, arguments);
}

void RemoteRecordingsAccess::rewind(int slice, qint64 utcNs, qint64 hz)
{
    sendDsrVerb(QStringLiteral("dsr.rewind"), QJsonObject{{QStringLiteral("slice"), slice},
                                                          {QStringLiteral("utcNs"), utcNs},
                                                          {QStringLiteral("frequencyHz"), hz}});
}

void RemoteRecordingsAccess::live(int slice)
{
    sendDsrVerb(QStringLiteral("dsr.live"), QJsonObject{{QStringLiteral("slice"), slice}});
}

void RemoteRecordingsAccess::seekPast(int slice, qint64 utcNs)
{
    sendDsrVerb(QStringLiteral("dsr.seek"),
                QJsonObject{{QStringLiteral("slice"), slice}, {QStringLiteral("utcNs"), utcNs}});
}

void RemoteRecordingsAccess::requestPastRows(int slice, const DsrPanView& view, qint64 fromUtcNs,
                                             qint64 toUtcNs, int rowCount)
{
    m_pastRows.seq += 1;
    m_pastRows.slice = slice;
    m_pastRows.view = view;
    m_pastRows.fromUtcNs = fromUtcNs;
    sendPastRows(fromUtcNs, toUtcNs, rowCount);
}

void RemoteRecordingsAccess::sendPastRows(qint64 fromUtcNs, qint64 toUtcNs, int rowCount)
{
    const DsrPanView& view = m_pastRows.view;
    if (sendDsrVerb(QStringLiteral("dsr.rows"),
                    QJsonObject{{QStringLiteral("slice"), m_pastRows.slice},
                                {QStringLiteral("fromUtcNs"), fromUtcNs},
                                {QStringLiteral("toUtcNs"), toUtcNs},
                                {QStringLiteral("rowCount"), rowCount},
                                {QStringLiteral("fftSize"), view.fftSize},
                                {QStringLiteral("windowType"), view.windowType},
                                {QStringLiteral("centreHz"), view.centreHz},
                                {QStringLiteral("spanHz"), view.spanHz},
                                {QStringLiteral("pixels"), view.pixels}})) {
        m_pastRowsInFlight.append(m_pastRows.seq);
    }
}

void RemoteRecordingsAccess::requestPastSpectrum(int slice, const DsrPanView& view, qint64 utcNs)
{
    sendDsrVerb(QStringLiteral("dsr.spectrum"), QJsonObject{{QStringLiteral("slice"), slice},
                                                            {QStringLiteral("utcNs"), utcNs},
                                                            {QStringLiteral("fftSize"), view.fftSize},
                                                            {QStringLiteral("windowType"), view.windowType},
                                                            {QStringLiteral("centreHz"), view.centreHz},
                                                            {QStringLiteral("spanHz"), view.spanHz},
                                                            {QStringLiteral("pixels"), view.pixels}});
}

void RemoteRecordingsAccess::requestHours()
{
    sendDsrVerb(QStringLiteral("dsr.hours"), QJsonObject{});
}

void RemoteRecordingsAccess::saveStretch(qint64 fromNs, qint64 toNs, bool wholePan)
{
    sendDsrVerb(QStringLiteral("dsr.saveStretch"),
                QJsonObject{{QStringLiteral("fromUtcNs"), fromNs},
                            {QStringLiteral("toUtcNs"), toNs},
                            {QStringLiteral("kind"), recordingKindKey(wholePan ? RecordingKind::WholePan
                                                                               : RecordingKind::Sound)}});
}

void RemoteRecordingsAccess::saveSoundOnly(const QString& id)
{
    sendDsrVerb(QStringLiteral("recordings.saveSoundOnly"), QJsonObject{{QStringLiteral("id"), id}});
}

void RemoteRecordingsAccess::openStretch(const QString& id)
{
    sendDsrVerb(QStringLiteral("recordings.openStretch"), QJsonObject{{QStringLiteral("id"), id}});
}

void RemoteRecordingsAccess::eraseDsr()
{
    sendDsrVerb(QStringLiteral("dsr.eraseAll"), QJsonObject{});
}

bool RemoteRecordingsAccess::sendLink(const QString& verb, const QJsonObject& arguments)
{
    IStationLink* const link = m_model ? m_model->stationLink() : nullptr;
    return link != nullptr && link->sendRecordingsCommand(verb, arguments).sent;
}

void RemoteRecordingsAccess::sendVoice(int n)
{
    sendVoiceVerb(QStringLiteral("voiceKeyer.send"), QJsonObject{{QStringLiteral("n"), n}});
}

void RemoteRecordingsAccess::stopVoice()
{
    sendVoiceVerb(QStringLiteral("voiceKeyer.stop"), QJsonObject{});
}

void RemoteRecordingsAccess::renameVoice(int n, const QString& name)
{
    sendVoiceVerb(QStringLiteral("voiceKeyer.rename"),
                  QJsonObject{{QStringLiteral("n"), n}, {QStringLiteral("name"), name}});
}

void RemoteRecordingsAccess::clearVoice(int n)
{
    sendVoiceVerb(QStringLiteral("voiceKeyer.clear"), QJsonObject{{QStringLiteral("n"), n}});
}

void RemoteRecordingsAccess::setVoiceRepeat(bool on, int delaySec)
{
    sendVoiceVerb(QStringLiteral("voiceKeyer.setRepeat"),
                  QJsonObject{{QStringLiteral("on"), on}, {QStringLiteral("delaySec"), delaySec}});
}

void RemoteRecordingsAccess::listenVoice(int n)
{
    sendVoiceVerb(QStringLiteral("voiceKeyer.listen"), QJsonObject{{QStringLiteral("n"), n}});
}

void RemoteRecordingsAccess::importVoice(int n, const QString& wavPath, const QString& name, bool imported)
{
    if (state() != State::Ready || coreVersion() < kVoiceKeyerVersion) {
        emit voiceImportFinished(n, false, unavailableReason(kVoiceKeyerVersion));
        return;
    }
    if (m_voiceImport.has_value()) {
        emit voiceImportFinished(n, false, QStringLiteral("Another voice message is being loaded."));
        return;
    }
    VoiceImport voiceImport;
    voiceImport.n = n;
    voiceImport.file = std::make_unique<QFile>(wavPath);
    if (!voiceImport.file->open(QIODevice::ReadOnly)) {
        qCWarning(lcRecordings) << "cannot read" << wavPath << "for a voice message:"
                                << voiceImport.file->errorString();
        emit voiceImportFinished(n, false, QString::fromLatin1(VoiceKeyer::kUnreadableReason));
        return;
    }
    voiceImport.size = voiceImport.file->size();
    if (voiceImport.size > VoiceKeyer::kImportMaxBytes) {
        emit voiceImportFinished(n, false, QString::fromLatin1(VoiceKeyer::kTooLargeReason));
        return;
    }
    m_voiceImport = std::move(voiceImport);
    QJsonObject begin{{QStringLiteral("n"), n},
                      {QStringLiteral("name"), name},
                      {QStringLiteral("size"), m_voiceImport->size}};
    // Sent only for a chosen file: a mic recording's begin is as it always
    // was, and the Core takes no `imported` as false.
    if (imported) {
        begin.insert(QStringLiteral("imported"), true);
    }
    if (!sendLink(QStringLiteral("voiceKeyer.beginImport"), begin)) {
        finishVoiceImport(false, kLinkLost);
    }
}

void RemoteRecordingsAccess::sendNextImportPiece()
{
    VoiceImport& voiceImport = *m_voiceImport;
    if (voiceImport.sent >= voiceImport.size) {
        voiceImport.finishing = true;
        if (!sendLink(QStringLiteral("voiceKeyer.finishImport"), QJsonObject{})) {
            finishVoiceImport(false, kLinkLost);
        }
        return;
    }
    const QByteArray chunk = voiceImport.file->read(VoiceKeyer::kChunkBytes);
    if (chunk.isEmpty()) {
        qCWarning(lcRecordings) << "short read of" << voiceImport.file->fileName() << "at"
                                << voiceImport.sent;
        // The Core drops the part it has when the next import begins.
        finishVoiceImport(false, QString::fromLatin1(VoiceKeyer::kUnreadableReason));
        return;
    }
    voiceImport.sent += chunk.size();
    if (!sendLink(QStringLiteral("voiceKeyer.chunk"),
                  QJsonObject{{QStringLiteral("data"), QString::fromLatin1(chunk.toBase64())}})) {
        finishVoiceImport(false, kLinkLost);
    }
}

void RemoteRecordingsAccess::takeVoiceImport(const QByteArray& verb, bool accepted, const QString& reason)
{
    if (!m_voiceImport.has_value()) {
        return;
    }
    if (!accepted) {
        finishVoiceImport(false, reason);
        return;
    }
    if (verb == "voiceKeyer.finishImport") {
        finishVoiceImport(true, QString());
        return;
    }
    sendNextImportPiece();
}

void RemoteRecordingsAccess::finishVoiceImport(bool ok, const QString& error)
{
    if (!m_voiceImport.has_value()) {
        return;
    }
    const int n = m_voiceImport->n;
    m_voiceImport.reset();
    emit voiceImportFinished(n, ok, error);
}

void RemoteRecordingsAccess::exportVoice(int n, const QString& toPath)
{
    if (state() != State::Ready || coreVersion() < kVoiceKeyerVersion) {
        emit voiceExportFinished(n, false, unavailableReason(kVoiceKeyerVersion));
        return;
    }
    const bool busy = std::any_of(m_voiceExports.cbegin(), m_voiceExports.cend(),
                                  [n](const std::shared_ptr<VoiceExport>& e) { return e->n == n; });
    if (busy) {
        emit voiceExportFinished(n, false, QStringLiteral("That voice message is already being saved."));
        return;
    }
    auto voiceExport = std::make_shared<VoiceExport>();
    voiceExport->n = n;
    voiceExport->file = std::make_unique<QSaveFile>(toPath);
    if (!voiceExport->file->open(QIODevice::WriteOnly)) {
        qCWarning(lcRecordings) << "cannot save a voice message to" << toPath
                                << voiceExport->file->errorString();
        emit voiceExportFinished(n, false, QStringLiteral("The copy cannot be saved there."));
        return;
    }
    m_voiceExports.append(voiceExport);
    if (!requestVoiceChunk(*voiceExport)) {
        finishVoiceExport(n, false, kLinkLost);
    }
}

bool RemoteRecordingsAccess::requestVoiceChunk(VoiceExport& voiceExport)
{
    if (!sendLink(QStringLiteral("voiceKeyer.export"),
                  QJsonObject{{QStringLiteral("n"), voiceExport.n},
                              {QStringLiteral("offset"), voiceExport.done}})) {
        return false;
    }
    m_voiceExportsInFlight.append(voiceExport.n);
    return true;
}

void RemoteRecordingsAccess::finishVoiceExport(int n, bool ok, const QString& error)
{
    const auto it = std::find_if(m_voiceExports.begin(), m_voiceExports.end(),
                                 [n](const std::shared_ptr<VoiceExport>& e) { return e->n == n; });
    if (it == m_voiceExports.end()) {
        return;
    }
    const std::shared_ptr<VoiceExport> voiceExport = *it;
    m_voiceExports.erase(it);
    bool done = ok;
    QString why = error;
    if (done && !voiceExport->file->commit()) {
        qCWarning(lcRecordings) << "cannot finish saving" << voiceExport->file->fileName()
                                << voiceExport->file->errorString();
        done = false;
        why = QStringLiteral("The copy cannot be saved there.");
    } else if (!done) {
        voiceExport->file->cancelWriting();
    }
    emit voiceExportFinished(n, done, why);
}

void RemoteRecordingsAccess::takeVoiceExport(bool accepted, const QString& reason,
                                             const QList<MirrorUpdate>& values)
{
    const int asked = m_voiceExportsInFlight.isEmpty() ? 0 : m_voiceExportsInFlight.takeFirst();
    const int n = accepted ? valueOf(values, "n").toInt() : asked;
    const auto it = std::find_if(m_voiceExports.begin(), m_voiceExports.end(),
                                 [n](const std::shared_ptr<VoiceExport>& e) { return e->n == n; });
    if (it == m_voiceExports.end()) {
        return;
    }
    VoiceExport& voiceExport = **it;
    if (!accepted) {
        finishVoiceExport(n, false, reason);
        return;
    }
    const qint64 offset = valueOf(values, "offset").toLongLong();
    const qint64 size = valueOf(values, "size").toLongLong();
    const QByteArray::FromBase64Result chunk = QByteArray::fromBase64Encoding(
        valueOf(values, "data").toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!chunk || offset != voiceExport.done || size < 0 || offset + chunk.decoded.size() > size
        || (chunk.decoded.isEmpty() && offset < size)) {
        qCWarning(lcRecordings) << "a chunk of voice message" << n << "did not fit: offset" << offset
                                << "size" << size << "expected" << voiceExport.done;
        finishVoiceExport(n, false, kCopyWentWrong);
        return;
    }
    if (voiceExport.file->write(chunk.decoded) != chunk.decoded.size()) {
        qCWarning(lcRecordings) << "cannot save a voice message to" << voiceExport.file->fileName()
                                << voiceExport.file->errorString();
        finishVoiceExport(n, false, QStringLiteral("The copy cannot be saved there."));
        return;
    }
    voiceExport.done += chunk.decoded.size();
    if (voiceExport.done >= size) {
        finishVoiceExport(n, true, QString());
    } else if (!requestVoiceChunk(voiceExport)) {
        finishVoiceExport(n, false, kLinkLost);
    }
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
    if (verb == "voiceKeyer.export") {
        takeVoiceExport(accepted, reason, values);
        return;
    }
    if (verb == "voiceKeyer.beginImport" || verb == "voiceKeyer.chunk"
        || verb == "voiceKeyer.finishImport") {
        takeVoiceImport(verb, accepted, reason);
        return;
    }
    if (verb == "dsr.rows" || verb == "dsr.spectrum" || verb == "dsr.hours") {
        takePast(verb, accepted, reason, values);
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
    if (verb == "dsr.saveStretch" || verb == "recordings.saveSoundOnly") {
        emit stretchSaving(valueOf(values, "note").toString());
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
        m_voiceExportsInFlight.clear();
        while (!m_voiceExports.isEmpty()) {
            finishVoiceExport(m_voiceExports.first()->n, false, kLinkLost);
        }
        finishVoiceImport(false, kLinkLost);
        // No past rows still asked for will come.
        m_pastRowsInFlight.clear();
        if (m_playback.has_value()) {
            m_playback.reset();
            emit playbackChanged();
        }
    }
    emit stateChanged();
}

// The past picture's answers (recording plan Task 23): the bin layout, the
// rows' times and bins in DsrPastRenderer's wire form, or the hours.
void RemoteRecordingsAccess::takePast(const QByteArray& verb, bool accepted, const QString& reason,
                                      const QList<MirrorUpdate>& values)
{
    if (verb == "dsr.rows") {
        // An answer to an older request than the newest is dropped.
        const quint64 seq = m_pastRowsInFlight.isEmpty() ? 0 : m_pastRowsInFlight.takeFirst();
        if (seq != m_pastRows.seq) {
            return;
        }
    }
    if (!accepted) {
        emit refused(reason);
        return;
    }
    if (verb == "dsr.hours") {
        const QList<qint64> starts = DsrPastRenderer::unpackTimes(
            QByteArray::fromBase64(valueOf(values, "starts").toString().toLatin1()));
        const QJsonArray activity =
            QJsonDocument::fromJson(valueOf(values, "activity").toString().toUtf8()).array();
        QList<DsrHour> hours;
        for (qsizetype i = 0; i < starts.size() && i < activity.size(); ++i) {
            hours.append(DsrHour{starts.at(i), activity.at(i).toDouble()});
        }
        emit hoursReady(hours);
        return;
    }
    DsrPastFrames frames;
    frames.slice = valueOf(values, "slice").toInt();
    frames.firstBinHz = valueOf(values, "firstBinHz").toDouble();
    frames.hzPerBin = valueOf(values, "hzPerBin").toDouble();
    frames.binCount = valueOf(values, "binCount").toInt();
    frames.utcNs = DsrPastRenderer::unpackTimes(QByteArray::fromBase64(valueOf(values, "times").toString().toLatin1()));
    frames.rows = DsrPastRenderer::unpackRows(QByteArray::fromBase64(valueOf(values, "bins").toString().toLatin1()),
                                              frames.binCount);
    frames.transmit = DsrPastRenderer::unpackTransmit(
        QByteArray::fromBase64(valueOf(values, "transmit").toString().toLatin1()));
    frames.rowsLeft = valueOf(values, "rowsLeft").toInt();
    frames.nextToUtcNs = valueOf(values, "nextToUtcNs").toLongLong();
    if (frames.rows.size() != frames.utcNs.size()) {
        qCWarning(lcRecordings) << "past rows from the Core do not match their times";
        return;
    }
    if (verb == "dsr.spectrum") {
        emit pastSpectrumReady(frames);
        return;
    }
    emit pastRowsReady(frames);
    if (frames.rowsLeft > 0) {
        sendPastRows(m_pastRows.fromUtcNs, frames.nextToUtcNs, frames.rowsLeft);
    }
}

} // namespace NereusSDR
