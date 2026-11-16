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
// The voice keyer needs recordingsVersion 3; an older Core refuses here,
// without sending. An import sends voiceKeyer.beginImport, then the file a
// chunk at a time, each once the one before it is taken, then
// voiceKeyer.finishImport; one import at a time. An export asks for Mn a
// chunk at a time (voiceKeyer.export), as a copy does.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: each list
//                 reader's own answer. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11: the voice
//                 keyer's voiceKeyer.* verbs (recordingsVersion 3), with a
//                 chunked import and export. AI-assisted implementation via
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

#include <QByteArray>
#include <QFile>
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

    struct VoiceImport {
        int n = 0;
        std::unique_ptr<QFile> file;
        qint64 size = 0;
        qint64 sent = 0;
        bool finishing = false;
    };
    struct VoiceExport {
        int n = 0;
        std::unique_ptr<QSaveFile> file;
        qint64 done = 0;
    };

    /// Sends the verb, or says why not (refused()). True when it went.
    bool send(const QString& verb, const QJsonObject& arguments);
    /// A voiceKeyer.* verb: as send(), and refused against a Core older
    /// than recordingsVersion 3.
    bool sendVoiceVerb(const QString& verb, const QJsonObject& arguments);
    /// A dsr.* verb: as send(), and refused against a Core older than
    /// recordingsVersion 4.
    bool sendDsrVerb(const QString& verb, const QJsonObject& arguments);
    bool sendLink(const QString& verb, const QJsonObject& arguments);
    void takeVoiceImport(const QByteArray& verb, bool accepted, const QString& reason);
    void sendNextImportPiece();
    void finishVoiceImport(bool ok, const QString& error);
    bool requestVoiceChunk(VoiceExport& voiceExport);
    void takeVoiceExport(bool accepted, const QString& reason, const QList<MirrorUpdate>& values);
    void finishVoiceExport(int n, bool ok, const QString& error);
    bool requestChunk(Download& download);
    void finishDownload(const QString& id, bool ok, const QString& error);
    void takeExport(bool accepted, const QString& reason, const QList<MirrorUpdate>& values);
    void sendPastRows(qint64 fromUtcNs, qint64 toUtcNs, int rowCount);
    void takePast(const QByteArray& verb, bool accepted, const QString& reason, const QList<MirrorUpdate>& values);
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
    std::optional<VoiceImport> m_voiceImport;
    QList<std::shared_ptr<VoiceExport>> m_voiceExports;
    // The message each voiceKeyer.export in flight asks for, in order.
    QList<int> m_voiceExportsInFlight;
    // The past picture: the newest requestPastRows, and the request each
    // dsr.rows in flight belongs to, in the order sent.
    struct PastRowsRequest {
        quint64 seq = 0;
        int slice = -1;
        DsrPanView view;
        qint64 fromUtcNs = 0;
    };
    PastRowsRequest m_pastRows;
    QList<quint64> m_pastRowsInFlight;
    State m_lastState{State::NotConnected};
};

} // namespace NereusSDR
