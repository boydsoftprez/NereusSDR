// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original. See LocalRecordingsAccess.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: each list
//                 reader's own answer, filtered by slice and kind.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11: the voice
//                 keyer, as the station. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11 review fixes:
//                 the import finishes once the Core's worker has converted
//                 it; Stop is refused while another device's message goes
//                 out (ruling 8.5). AI-assisted via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11 fix round 2:
//                 Repeat off is the station's as a device, refused under
//                 another device's message (ruling 8.5). AI-assisted via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 22: rewind, live
//                 and seekPast (rewind sound, recordingsVersion 4).
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 23: the past
//                 picture (past rows in chunks, a past spectrum frame, the
//                 hours map). AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: saving a
//                 stretch, sound only and opening a saved stretch.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 26: eraseDsr
//                 (dsr.eraseAll). AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF): rename and delete go through the
//                 recording service, which lets go of the file first.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), JJ's ruling on PR 797: importVoice
//                 says whether the file was imported (fixed level on the
//                 air) or recorded on the mic. AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

#include "models/recordings/LocalRecordingsAccess.h"

#include <QDir>
#include <QJsonDocument>
#include <QTimer>

#include <algorithm>

#include "core/LogCategories.h"
#include "core/recordings/RecordingDisks.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingStore.h"
#include "core/recordings/VoiceKeyer.h"
#include "core/session/RecordingsFacade.h"
#include "models/RadioModel.h"

namespace NereusSDR {

namespace {

const QString kLocal = QString::fromLatin1(RecordingService::kLocalListener);

// This window keys the radio as the station's own operator.
KeyerIdentity stationKeyer()
{
    return KeyerIdentity::station(PttMode::None);
}

}  // namespace

LocalRecordingsAccess::LocalRecordingsAccess(RadioModel* model)
    : RecordingsAccess(model)
    , m_model(model)
{
    connect(model->recordingsFacade(), &RecordingsFacade::changed, this,
            &LocalRecordingsAccess::onFacadeChanged);
    connect(model, &RadioModel::connectionStateChanged, this, &LocalRecordingsAccess::refreshState);
    if (RecordingService* recordings = model->recordingService()) {
        connect(recordings, &RecordingService::playbackChanged, this, [this](const ListenerId& who) {
            if (who == kLocal) {
                emit playbackChanged();
            }
        });
        connect(recordings, &RecordingService::playbackRefused, this,
                [this](const ListenerId& who, const QString& reason) {
                    if (who == kLocal) {
                        emit refused(reason);
                    }
                });
        if (VoiceKeyer* voice = recordings->voiceKeyer()) {
            connect(voice, &VoiceKeyer::sendRefused, this,
                    [this](const QByteArray& deviceId, const QString& reason) {
                        if (deviceId == stationKeyer().deviceId) {
                            emit refused(reason);
                        }
                    });
        }
    }
}

LocalRecordingsAccess::~LocalRecordingsAccess()
{
    for (const std::shared_ptr<Copy>& copy : m_copies) {
        if (copy->to) {
            copy->to->cancelWriting();
        }
    }
}

RecordingService* LocalRecordingsAccess::service() const
{
    return m_model ? m_model->recordingService() : nullptr;
}

void LocalRecordingsAccess::ensurePublished() const
{
    if (m_model && service() != nullptr) {
        m_model->recordingsFacade()->publish();
    }
}

RecordingsAccess::State LocalRecordingsAccess::state() const
{
    return service() != nullptr && m_model->isConnected() ? State::Ready : State::NotConnected;
}

int LocalRecordingsAccess::coreVersion() const
{
    return service() != nullptr ? RecordingsFacade::kVersion : 0;
}

QString LocalRecordingsAccess::unavailableReason(int needsVersion) const
{
    if (state() != State::Ready) {
        return notConnectedReason();
    }
    return coreVersion() < needsVersion ? needsNewerCoreReason() : QString();
}

QJsonObject LocalRecordingsAccess::document() const
{
    ensurePublished();
    return m_document;
}

RecordingService* LocalRecordingsAccess::begin()
{
    if (state() != State::Ready) {
        emit refused(unavailableReason());
        return nullptr;
    }
    ensurePublished();
    return service();
}

void LocalRecordingsAccess::start(int sliceId)
{
    RecordingService* recordings = begin();
    if (recordings == nullptr) {
        return;
    }
    if (m_model->sliceById(sliceId) == nullptr) {
        emit refused(QStringLiteral("That slice is not open."));
        return;
    }
    recordings->startRecording(sliceId, QString());
    for (const RecordingSliceState& st : recordings->sliceStates()) {
        if (st.sliceId == sliceId && st.state == RecordState::Recording) {
            return;
        }
    }
    emit refused(QStringLiteral("Slice %1 could not start recording. Check where recordings are kept "
                                "in Setup.")
                     .arg(QChar(QLatin1Char('A').unicode() + sliceId)));
}

void LocalRecordingsAccess::rewind(int slice, qint64 utcNs, qint64 hz)
{
    if (RecordingService* recordings = begin()) {
        QString refusal;
        if (!recordings->rewind(slice, utcNs, hz, &refusal)) {
            emit refused(refusal);
        }
    }
}

void LocalRecordingsAccess::live(int slice)
{
    if (RecordingService* recordings = begin()) {
        QString refusal;
        if (!recordings->goLive(slice, &refusal)) {
            emit refused(refusal);
        }
    }
}

void LocalRecordingsAccess::seekPast(int slice, qint64 utcNs)
{
    if (RecordingService* recordings = begin()) {
        QString refusal;
        if (!recordings->seekRewind(slice, utcNs, &refusal)) {
            emit refused(refusal);
        }
    }
}

void LocalRecordingsAccess::requestPastRows(int slice, const DsrPanView& view, qint64 fromUtcNs,
                                            qint64 toUtcNs, int rowCount)
{
    if (begin() == nullptr) {
        return;
    }
    askPastRows(++m_pastRowsSeq, slice, view, fromUtcNs, toUtcNs, rowCount);
}

void LocalRecordingsAccess::askPastRows(quint64 seq, int slice, const DsrPanView& view, qint64 fromUtcNs,
                                        qint64 toUtcNs, int rowCount)
{
    RecordingService* recordings = service();
    if (recordings == nullptr) {
        return;
    }
    const QPointer<LocalRecordingsAccess> self(this);
    recordings->pastRows(slice, view, fromUtcNs, toUtcNs, rowCount,
                         [self, seq, slice, view, fromUtcNs](bool ok, const QString& refusal,
                                                             const DsrPastFrames& rows) {
                             if (self.isNull() || seq != self->m_pastRowsSeq) {
                                 return;
                             }
                             if (!ok) {
                                 emit self->refused(refusal);
                                 return;
                             }
                             emit self->pastRowsReady(rows);
                             if (rows.rowsLeft > 0) {
                                 self->askPastRows(seq, slice, view, fromUtcNs, rows.nextToUtcNs, rows.rowsLeft);
                             }
                         });
}

void LocalRecordingsAccess::requestPastSpectrum(int slice, const DsrPanView& view, qint64 utcNs)
{
    if (RecordingService* recordings = begin()) {
        const QPointer<LocalRecordingsAccess> self(this);
        recordings->pastSpectrum(slice, view, utcNs,
                                 [self](bool ok, const QString& refusal, const DsrPastFrames& frame) {
                                     if (self.isNull()) {
                                         return;
                                     }
                                     if (!ok) {
                                         emit self->refused(refusal);
                                         return;
                                     }
                                     emit self->pastSpectrumReady(frame);
                                 });
    }
}

void LocalRecordingsAccess::saveStretch(qint64 fromNs, qint64 toNs, bool wholePan)
{
    if (RecordingService* recordings = begin()) {
        QString note;
        QString refusal;
        if (!recordings->saveStretch(fromNs, toNs, wholePan, &note, &refusal)) {
            emit refused(refusal);
            return;
        }
        emit stretchSaving(note);
    }
}

void LocalRecordingsAccess::saveSoundOnly(const QString& id)
{
    if (RecordingService* recordings = begin()) {
        QString note;
        QString refusal;
        if (!recordings->saveSoundOnly(id, &note, &refusal)) {
            emit refused(refusal);
            return;
        }
        emit stretchSaving(note);
    }
}

void LocalRecordingsAccess::openStretch(const QString& id)
{
    if (RecordingService* recordings = begin()) {
        QString refusal;
        if (!recordings->openStretch(id, &refusal)) {
            emit refused(refusal);
        }
    }
}

void LocalRecordingsAccess::eraseDsr()
{
    if (RecordingService* recordings = begin()) {
        recordings->eraseDsr();
    }
}

void LocalRecordingsAccess::requestHours()
{
    if (RecordingService* recordings = begin()) {
        const QPointer<LocalRecordingsAccess> self(this);
        recordings->pastHours([self](const QList<DsrHour>& hours) {
            if (!self.isNull()) {
                emit self->hoursReady(hours);
            }
        });
    }
}

void LocalRecordingsAccess::stop(int sliceId)
{
    if (RecordingService* recordings = begin()) {
        recordings->stopRecording(sliceId, QString());
    }
}

void LocalRecordingsAccess::requestList(int reader, const RecordingsListQuery& query)
{
    RecordingService* recordings = begin();
    if (recordings == nullptr) {
        return;
    }
    const QList<RecordingEntry> entries =
        filterRecordings(recordings->store()->entries(), query.sliceId, query.kind);
    const qsizetype from = std::clamp<qsizetype>(query.offset, 0, entries.size());
    RecordingsListPage page;
    page.offset = static_cast<int>(from);
    page.total = static_cast<int>(entries.size());
    page.entries = entries.mid(from, std::max(0, query.count));
    m_pages.insert(reader, page);
    emit listChanged(reader);
}

void LocalRecordingsAccess::play(const QString& id)
{
    if (RecordingService* recordings = begin()) {
        recordings->play(kLocal, id);
    }
}

void LocalRecordingsAccess::stopPlay()
{
    if (RecordingService* recordings = begin()) {
        recordings->stopPlayback(kLocal);
    }
}

void LocalRecordingsAccess::seek(qint64 ms)
{
    if (RecordingService* recordings = begin()) {
        recordings->seek(kLocal, std::max<qint64>(0, ms));
    }
}

std::optional<PlaybackState> LocalRecordingsAccess::playback() const
{
    RecordingService* recordings = service();
    return recordings != nullptr ? recordings->playback(kLocal) : std::nullopt;
}

void LocalRecordingsAccess::rename(const QString& id, const QString& name)
{
    RecordingService* recordings = begin();
    if (recordings == nullptr) {
        return;
    }
    QString refusal;
    if (!recordings->renameRecording(id, name, &refusal)) {
        emit refused(refusal);
    }
}

void LocalRecordingsAccess::remove(const QStringList& ids)
{
    RecordingService* recordings = begin();
    if (recordings == nullptr) {
        return;
    }
    RecordingStore* store = recordings->store();
    const bool anyKnown = std::any_of(ids.cbegin(), ids.cend(), [store](const QString& id) {
        return store->find(id).has_value();
    });
    if (!anyKnown) {
        emit refused(QString::fromLatin1(RecordingService::kPlayGoneReason));
        return;
    }
    QString refusal;
    if (!recordings->removeRecordings(ids, &refusal)) {
        emit refused(refusal);
    }
}

void LocalRecordingsAccess::download(const QString& id, const QString& toPath)
{
    if (state() != State::Ready) {
        emit downloadFinished(id, false, unavailableReason());
        return;
    }
    ensurePublished();
    RecordingStore* store = service()->store();
    const std::optional<RecordingEntry> entry = store->find(id);
    if (!entry.has_value()) {
        emit downloadFinished(id, false, QString::fromLatin1(RecordingService::kPlayGoneReason));
        return;
    }
    auto copy = std::make_shared<Copy>();
    copy->id = id;
    copy->from = std::make_unique<QFile>(QDir(store->folder()).filePath(entry->fileName));
    copy->to = std::make_unique<QSaveFile>(toPath);
    if (!copy->from->open(QIODevice::ReadOnly)) {
        qCWarning(lcRecordings) << "cannot read" << copy->from->fileName() << "for a copy:"
                                << copy->from->errorString();
        emit downloadFinished(id, false, QStringLiteral("That recording cannot be read on the Core."));
        return;
    }
    if (!copy->to->open(QIODevice::WriteOnly)) {
        qCWarning(lcRecordings) << "cannot write a copy to" << toPath << copy->to->errorString();
        emit downloadFinished(id, false, QStringLiteral("The copy cannot be saved there."));
        return;
    }
    copy->total = copy->from->size();
    m_copies.append(copy);
    if (!m_copyScheduled) {
        m_copyScheduled = true;
        QTimer::singleShot(0, this, &LocalRecordingsAccess::copyStep);
    }
}

void LocalRecordingsAccess::copyStep()
{
    m_copyScheduled = false;
    if (m_copies.isEmpty()) {
        return;
    }
    Copy& copy = *m_copies.first();
    const QByteArray chunk = copy.from->read(kCopyStepBytes);
    if (chunk.isEmpty() && copy.done < copy.total) {
        qCWarning(lcRecordings) << "short read of" << copy.from->fileName() << "at" << copy.done;
        finishCopy(false, QStringLiteral("That recording cannot be read on the Core."));
    } else if (!chunk.isEmpty() && copy.to->write(chunk) != chunk.size()) {
        qCWarning(lcRecordings) << "cannot write a copy to" << copy.to->fileName()
                                << copy.to->errorString();
        finishCopy(false, QStringLiteral("The copy cannot be saved there."));
    } else {
        copy.done += chunk.size();
        emit downloadProgress(copy.id, copy.done, copy.total);
        if (copy.done >= copy.total) {
            const bool committed = copy.to->commit();
            if (!committed) {
                qCWarning(lcRecordings) << "cannot finish the copy" << copy.to->fileName()
                                        << copy.to->errorString();
            }
            finishCopy(committed, committed ? QString()
                                            : QStringLiteral("The copy cannot be saved there."));
        }
    }
    if (!m_copies.isEmpty() && !m_copyScheduled) {
        m_copyScheduled = true;
        QTimer::singleShot(0, this, &LocalRecordingsAccess::copyStep);
    }
}

void LocalRecordingsAccess::finishCopy(bool ok, const QString& error)
{
    const std::shared_ptr<Copy> copy = m_copies.takeFirst();
    if (!ok && copy->to) {
        copy->to->cancelWriting();
    }
    emit downloadFinished(copy->id, ok, error);
}

void LocalRecordingsAccess::setSettings(const QJsonObject& changed)
{
    RecordingService* recordings = begin();
    if (recordings == nullptr) {
        return;
    }
    RecordingSettings settings = recordings->settings();
    QString refusal;
    if (!RecordingsFacade::applySettingsChange(changed, RecordingDisks::volumes(), &settings,
                                               &refusal)) {
        emit refused(refusal);
        return;
    }
    recordings->setSettings(settings);
    m_model->recordingsFacade()->refresh();
}

VoiceKeyer* LocalRecordingsAccess::beginVoice()
{
    RecordingService* recordings = begin();
    if (recordings == nullptr) {
        return nullptr;
    }
    if (recordings->voiceKeyer() == nullptr) {
        emit refused(needsNewerCoreReason());
    }
    return recordings->voiceKeyer();
}

void LocalRecordingsAccess::sendVoice(int n)
{
    VoiceKeyer* voice = beginVoice();
    if (voice == nullptr) {
        return;
    }
    QString refusal;
    const VoiceKeyer::Requester who{stationKeyer(), QString::fromLatin1(VoiceKeyer::kStationName),
                                    QString::fromLatin1(stationKeyer().deviceId)};
    if (!voice->send(n, who, &refusal)) {
        emit refused(refusal);
    }
}

void LocalRecordingsAccess::stopVoice()
{
    VoiceKeyer* voice = beginVoice();
    TxRefusal refusal;
    if (voice != nullptr && !voice->stopFrom(stationKeyer(), &refusal)) {
        emit refused(refusal.text);
    }
}

void LocalRecordingsAccess::importVoice(int n, const QString& wavPath, const QString& name, bool imported)
{
    if (state() != State::Ready || service() == nullptr || service()->voiceKeyer() == nullptr) {
        emit voiceImportFinished(n, false, unavailableReason(kVoiceKeyerVersion));
        return;
    }
    ensurePublished();
    // Converted on the Core's recordings worker; the answer comes later.
    const QPointer<LocalRecordingsAccess> self(this);
    service()->voiceKeyer()->importFile(n, wavPath, name, [self, n](bool ok, const QString& refusal) {
        if (self) {
            emit self->voiceImportFinished(n, ok, ok ? QString() : refusal);
        }
    }, imported);
}

void LocalRecordingsAccess::exportVoice(int n, const QString& toPath)
{
    if (state() != State::Ready || service() == nullptr || service()->voiceKeyer() == nullptr) {
        emit voiceExportFinished(n, false, unavailableReason(kVoiceKeyerVersion));
        return;
    }
    ensurePublished();
    VoiceKeyer* voice = service()->voiceKeyer();
    // Mn is at most two minutes of 48 kHz 16-bit mono (about 11.5 MB).
    QByteArray bytes;
    qint64 size = 0;
    qint64 offset = 0;
    QString refusal;
    do {
        QByteArray chunk;
        if (!voice->exportChunk(n, offset, &chunk, &size, &refusal)) {
            emit voiceExportFinished(n, false, refusal);
            return;
        }
        if (chunk.isEmpty() && offset < size) {
            emit voiceExportFinished(n, false,
                                     QStringLiteral("That voice message cannot be read on the Core."));
            return;
        }
        bytes.append(chunk);
        offset += chunk.size();
    } while (offset < size);
    QSaveFile to(toPath);
    if (!to.open(QIODevice::WriteOnly) || to.write(bytes) != bytes.size() || !to.commit()) {
        qCWarning(lcRecordings) << "cannot save a voice message to" << toPath << to.errorString();
        emit voiceExportFinished(n, false, QStringLiteral("The copy cannot be saved there."));
        return;
    }
    emit voiceExportFinished(n, true, QString());
}

void LocalRecordingsAccess::renameVoice(int n, const QString& name)
{
    VoiceKeyer* voice = beginVoice();
    QString refusal;
    if (voice != nullptr && !voice->rename(n, name, &refusal)) {
        emit refused(refusal);
    }
}

void LocalRecordingsAccess::clearVoice(int n)
{
    VoiceKeyer* voice = beginVoice();
    QString refusal;
    if (voice != nullptr && !voice->clear(n, &refusal)) {
        emit refused(refusal);
    }
}

void LocalRecordingsAccess::setVoiceRepeat(bool on, int delaySec)
{
    VoiceKeyer* voice = beginVoice();
    QString refusal;
    if (voice != nullptr && !voice->setRepeatFrom(stationKeyer(), on, delaySec, &refusal)) {
        emit refused(refusal);
    }
}

void LocalRecordingsAccess::listenVoice(int n)
{
    VoiceKeyer* voice = beginVoice();
    QString refusal;
    if (voice != nullptr && !voice->listen(n, kLocal, &refusal)) {
        emit refused(refusal);
    }
}

void LocalRecordingsAccess::onFacadeChanged()
{
    const QString json = m_model ? m_model->recordingsFacade()->json() : QString();
    if (json != m_json) {
        m_json = json;
        m_document = QJsonDocument::fromJson(json.toUtf8()).object();
        emit documentChanged();
        // The first document reports nothing: that recording is old.
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

void LocalRecordingsAccess::refreshState()
{
    const State now = state();
    if (now != m_lastState) {
        m_lastState = now;
        emit stateChanged();
    }
}

} // namespace NereusSDR
