#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/recordings/LocalRecordingsAccess.h  (NereusSDR)
// =================================================================
//
// Recordings in a window that runs its own radio: every call goes straight
// to the model's RecordingService and its store, as the listener "local"
// (RecordingService::kLocalListener). A recording started or stopped here is
// the Core's own ("stopped here"). The first use publishes the `recordings`
// document (RecordingsFacade::publish), which opens the recordings folder;
// building the model does not.
//
// A copy ("Save a copy here") is read and written a step at a time on this
// thread's event loop, so a large recording does not hold the window up.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: each list
//                 reader's own answer. AI-assisted implementation via
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
// =================================================================

#include <QFile>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QSaveFile>

#include <memory>
#include <optional>

#include "core/NereusCoreExport.h"
#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

class RadioModel;
class RecordingService;
class VoiceKeyer;

class NEREUS_CORE_EXPORT LocalRecordingsAccess final : public RecordingsAccess {
    Q_OBJECT

public:
    /// A copy moves this many bytes per step.
    static constexpr qint64 kCopyStepBytes = 4 * 1024 * 1024;

    explicit LocalRecordingsAccess(RadioModel* model);
    ~LocalRecordingsAccess() override;

    State state() const override;
    int coreVersion() const override;
    QString unavailableReason(int needsVersion = 1) const override;
    QJsonObject document() const override;
    void start(int sliceId) override;
    void stop(int sliceId) override;
    void requestList(int reader, const RecordingsListQuery& query) override;
    RecordingsListPage listed(int reader) const override { return m_pages.value(reader); }
    void play(const QString& id) override;
    void stopPlay() override;
    void seek(qint64 ms) override;
    std::optional<PlaybackState> playback() const override;
    void rename(const QString& id, const QString& name) override;
    void remove(const QStringList& ids) override;
    void download(const QString& id, const QString& toPath) override;
    void setSettings(const QJsonObject& changed) override;
    bool coreIsOnThisComputer() const override { return true; }
    void sendVoice(int n) override;
    void stopVoice() override;
    void importVoice(int n, const QString& wavPath, const QString& name = QString()) override;
    void exportVoice(int n, const QString& toPath) override;
    void renameVoice(int n, const QString& name) override;
    void clearVoice(int n) override;
    void setVoiceRepeat(bool on, int delaySec) override;
    void listenVoice(int n) override;
    void rewind(int slice, qint64 utcNs, qint64 hz) override;
    void live(int slice) override;
    void seekPast(int slice, qint64 utcNs) override;
    void requestPastRows(int slice, const DsrPanView& view, qint64 fromUtcNs, qint64 toUtcNs,
                         int rowCount) override;
    void requestPastSpectrum(int slice, const DsrPanView& view, qint64 utcNs) override;
    void requestHours() override;
    void saveStretch(qint64 fromNs, qint64 toNs, bool wholePan) override;
    void saveSoundOnly(const QString& id) override;
    void openStretch(const QString& id) override;

private:
    struct Copy {
        QString id;
        std::unique_ptr<QFile> from;
        std::unique_ptr<QSaveFile> to;
        qint64 done = 0;
        qint64 total = 0;
    };

    RecordingService* service() const;
    /// The service when a call may run now; otherwise refused() and null.
    RecordingService* begin();
    /// The voice keyer when a call may run now; otherwise refused() and null.
    VoiceKeyer* beginVoice();
    void ensurePublished() const;
    void onFacadeChanged();
    void refreshState();
    void copyStep();
    void finishCopy(bool ok, const QString& error);
    /// One chunk of request `seq`'s rows; the next is asked for when it
    /// comes, while `seq` is still the newest request.
    void askPastRows(quint64 seq, int slice, const DsrPanView& view, qint64 fromUtcNs, qint64 toUtcNs,
                     int rowCount);

    QPointer<RadioModel> m_model;
    QString m_json;
    QJsonObject m_document;
    std::optional<qint64> m_seenKept;
    QHash<int, RecordingsListPage> m_pages;   // by reader
    QList<std::shared_ptr<Copy>> m_copies;
    bool m_copyScheduled = false;
    quint64 m_pastRowsSeq = 0;   // the newest requestPastRows
    State m_lastState{State::NotConnected};
};

} // namespace NereusSDR
