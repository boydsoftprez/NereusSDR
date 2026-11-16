#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/recordings/RecordingsAccess.h  (NereusSDR)
// =================================================================
//
// What a window talks to for recordings, so a panel does not care whether
// the Core is in this process. RadioModel::recordings() returns the right
// one: LocalRecordingsAccess where the model runs its own radio and
// RemoteRecordingsAccess in a remote window. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md, sections
// 6.9 and V9.
//
// document() is the `recordings` object's document (RecordingsFacade.h).
// Every refusal comes through refused() exactly once; a copy's failure comes
// through downloadFinished() instead. kept() fires once for each recording
// kept after this window first saw the document, with the document's `kept`
// object.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
// =================================================================

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

#include "core/NereusCoreExport.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingPlayer.h"

namespace NereusSDR {

class NEREUS_CORE_EXPORT RecordingsAccess : public QObject {
    Q_OBJECT

public:
    enum class State { Ready, NeedsNewerCore, NotConnected };

    explicit RecordingsAccess(QObject* parent = nullptr) : QObject(parent) {}

    virtual State state() const = 0;
    virtual int coreVersion() const = 0;        // the Core's recordingsVersion, 0 if none
    /// Why a control that needs recordingsVersion `needsVersion` cannot be
    /// used now. Empty when it can.
    virtual QString unavailableReason(int needsVersion = 1) const = 0;
    virtual QJsonObject document() const = 0;
    virtual void start(int sliceId) = 0;
    virtual void stop(int sliceId) = 0;
    /// Asks for `count` recordings from `offset`, newest first; listed()
    /// holds them when listChanged() fires.
    virtual void requestList(int offset, int count) = 0;
    virtual QList<RecordingEntry> listed() const = 0;
    virtual void play(const QString& id) = 0;
    virtual void stopPlay() = 0;
    virtual void seek(qint64 ms) = 0;
    /// This window's own playback; empty when it plays nothing.
    virtual std::optional<PlaybackState> playback() const = 0;
    virtual void rename(const QString& id, const QString& name) = 0;
    virtual void remove(const QStringList& ids) = 0;
    /// Saves a copy of the recording at `toPath` on this computer.
    virtual void download(const QString& id, const QString& toPath) = 0;   // progress and done signals
    /// Any of the document's settings names (RecordingsFacade::applySettingsChange).
    virtual void setSettings(const QJsonObject& changed) = 0;
    /// The Core runs on this computer (Show in Finder works).
    virtual bool coreIsOnThisComputer() const = 0;

    static QString needsNewerCoreReason() { return QStringLiteral("This Core needs an update for recordings."); }
    static QString notConnectedReason() { return QStringLiteral("Connect to a radio to use recordings."); }

signals:
    void documentChanged();
    void stateChanged();
    void listChanged();
    void playbackChanged();
    void kept(const QJsonObject& event);
    void downloadProgress(const QString& id, qint64 done, qint64 total);
    void downloadFinished(const QString& id, bool ok, const QString& error);
    void refused(const QString& reason);
};

} // namespace NereusSDR
