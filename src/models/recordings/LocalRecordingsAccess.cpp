// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original. See LocalRecordingsAccess.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
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
#include "core/session/RecordingsFacade.h"
#include "models/RadioModel.h"

namespace NereusSDR {

namespace {

const QString kLocal = QString::fromLatin1(RecordingService::kLocalListener);

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

void LocalRecordingsAccess::stop(int sliceId)
{
    if (RecordingService* recordings = begin()) {
        recordings->stopRecording(sliceId, QString());
    }
}

void LocalRecordingsAccess::requestList(int offset, int count)
{
    RecordingService* recordings = begin();
    if (recordings == nullptr) {
        return;
    }
    const QList<RecordingEntry> entries = recordings->store()->entries();
    const qsizetype from = std::clamp<qsizetype>(offset, 0, entries.size());
    m_listed = entries.mid(from, std::max(0, count));
    emit listChanged();
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
    if (!recordings->store()->rename(id, name, &refusal)) {
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
    if (!store->remove(ids, &refusal)) {
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
