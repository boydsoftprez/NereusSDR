#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/recordings/RemoteRecordingsAccess.h  (NereusSDR)
// =================================================================
//
// Recordings in a remote window: the document comes from the mirror and each
// call goes out as a recordings.* verb through the window's station link.
// Against a Core that does not offer recordingsVersion every call is refused
// here, without sending, with "This Core needs an update for recordings."
// (V9). StationClient hands every recordings.* answer, and the Core's
// recordings.playback for this window, to takeResult().
//
// A copy ("Save a copy here") asks for the file a chunk at a time
// (recordings.export) and writes it through a QSaveFile, so a copy that
// fails leaves nothing behind.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: each list
//                 reader's own answer. AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QSaveFile>

#include <memory>
#include <optional>

#include "core/NereusCoreExport.h"
#include "core/session/SessionMessages.h"
#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

class RadioModel;

class NEREUS_CORE_EXPORT RemoteRecordingsAccess final : public RecordingsAccess {
    Q_OBJECT

public:
    explicit RemoteRecordingsAccess(RadioModel* model);
    ~RemoteRecordingsAccess() override;

    State state() const override;
    int coreVersion() const override;
    QString unavailableReason(int needsVersion = 1) const override;
    QJsonObject document() const override { return m_document; }
    void start(int sliceId) override;
    void stop(int sliceId) override;
    void requestList(int reader, const RecordingsListQuery& query) override;
    RecordingsListPage listed(int reader) const override { return m_pages.value(reader); }
    void play(const QString& id) override;
    void stopPlay() override;
    void seek(qint64 ms) override;
    std::optional<PlaybackState> playback() const override { return m_playback; }
    void rename(const QString& id, const QString& name) override;
    void remove(const QStringList& ids) override;
    void download(const QString& id, const QString& toPath) override;
    void setSettings(const QJsonObject& changed) override;
    bool coreIsOnThisComputer() const override;

    /// StationClient: a recordings.* answer, or the Core's recordings.playback.
    void takeResult(const QByteArray& verb, bool accepted, const QString& reason,
                    const QList<MirrorUpdate>& values);

private:
    struct Download {
        QString id;
        std::unique_ptr<QSaveFile> file;
        qint64 done = 0;
        qint64 total = -1;
    };

    /// Sends the verb, or says why not (refused()). True when it went.
    bool send(const QString& verb, const QJsonObject& arguments);
    bool requestChunk(Download& download);
    void finishDownload(const QString& id, bool ok, const QString& error);
    void takeExport(bool accepted, const QString& reason, const QList<MirrorUpdate>& values);
    void onFacadeChanged();
    void refreshState();

    QPointer<RadioModel> m_model;
    QString m_json;
    QJsonObject m_document;
    std::optional<qint64> m_seenKept;
    QHash<int, RecordingsListPage> m_pages;   // by reader
    std::optional<PlaybackState> m_playback;
    QList<std::shared_ptr<Download>> m_downloads;
    // The recording each recordings.export in flight asks for, in the order
    // sent; the Core answers them in that order.
    QList<QString> m_exportsInFlight;
    State m_lastState{State::NotConnected};
};

} // namespace NereusSDR
