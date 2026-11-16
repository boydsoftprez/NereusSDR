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
// Each reader of the list (a flag, the Recordings panel, the Recordings
// window) takes its own token from newListReader() and asks with it: the
// answer comes back as listChanged(token) and stays in listed(token), so
// readers never see each other's pages.
//
// The voice keyer (recordingsVersion 3) is the document's `voiceKeyer`.
// sendVoice() is the one call that keys the radio, as this window's device;
// the rest never key. An import or export ends with voiceImportFinished() or
// voiceExportFinished(), whose error is also its only report.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: a token per list
//                 reader and the list's slice and kind filters. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11: the voice
//                 keyer's methods (recordingsVersion 3). AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 21: kDsrVersion,
//                 the Digital Spectrum Recorder's version gate. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 22: rewind, live
//                 and seekPast (rewind sound, recordingsVersion 4).
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 23: the past
//                 picture (requestPastRows, requestPastSpectrum,
//                 requestHours). AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: saving a
//                 stretch, sound only and opening a saved stretch.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

#include "core/NereusCoreExport.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingPlayer.h"
#include "core/recordings/dsr/DsrReplay.h"

namespace NereusSDR {

/// One request for the list: `count` recordings from `offset`, newest first,
/// of only one slice and only one kind when those are set.
struct RecordingsListQuery {
    int offset = 0;
    int count = 0;
    std::optional<int> sliceId;
    std::optional<RecordingKind> kind;
};

/// One answer: the page from `offset`, and how many recordings match.
struct RecordingsListPage {
    int offset = 0;
    int total = 0;
    QList<RecordingEntry> entries;
};

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
    /// A new reader's token (never 0): its answers are its own.
    int newListReader() { return ++m_lastListReader; }
    /// Asks for a page for `reader`; listed(reader) holds it when
    /// listChanged(reader) fires. A later answer replaces an earlier one.
    virtual void requestList(int reader, const RecordingsListQuery& query) = 0;
    /// The last answer to `reader`; empty until one comes.
    virtual RecordingsListPage listed(int reader) const = 0;
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

    // ---- The voice keyer (recordingsVersion 3), M1 to M8.
    static constexpr int kVoiceKeyerSlots = 8;
    static constexpr int kVoiceClipMaxMs = 120000;
    static constexpr int kVoiceKeyerVersion = 3;
    // The Digital Spectrum Recorder (the document's `dsr`, its settings in
    // setSettings): a Core below this leaves its controls visible and
    // disabled with needsNewerCoreReason() (unavailableReason(kDsrVersion)).
    static constexpr int kDsrVersion = 4;
    // Rewind sound (recordingsVersion 4): a slice plays the past kept by
    // the Digital Spectrum Recorder. Its frequency moves to `hz` and its
    // sound comes from `utcNs` (ns since 1970 UTC), in real time; the
    // document's `dsr.rewound` lists it. None keys the radio. A moment the
    // recorder no longer keeps is refused (refused()).
    virtual void rewind(int slice, qint64 utcNs, qint64 hz) = 0;
    /// Back to the radio's sound, at the frequency before rewinding.
    virtual void live(int slice) = 0;
    /// A rewound slice plays on from `utcNs`.
    virtual void seekPast(int slice, qint64 utcNs) = 0;
    // The past picture (recordingsVersion 4): the Core draws past rows and
    // a past spectrum frame from the Digital Spectrum Recorder with this
    // pan's own FFT settings (`view`), for the receiver `slice` is on.
    /// `rowCount` rows from `toUtcNs` back to `fromUtcNs`, newest first.
    /// They come in chunks through pastRowsReady (its rowsLeft says more
    /// follow); a newer request replaces the rest of an older one.
    virtual void requestPastRows(int slice, const DsrPanView& view, qint64 fromUtcNs, qint64 toUtcNs,
                                 int rowCount) = 0;
    /// One past spectrum frame at `utcNs`, through pastSpectrumReady.
    virtual void requestPastSpectrum(int slice, const DsrPanView& view, qint64 utcNs) = 0;
    /// Every kept hour, newest first, through hoursReady.
    virtual void requestHours() = 0;
    // Saving a stretch (recordingsVersion 4; spec 3 "Saving a stretch",
    // 6.3, 6.4, A13). The Core copies or demodulates it on its worker; the
    // list shows the recording when it is done. stretchSaving() says what
    // is being saved (and if only part of it is still kept); a refusal
    // comes through refused(). None keys the radio.
    /// `fromNs` to `toNs` (ns since 1970 UTC): the whole pan, or slice A's
    /// sound in the Files format.
    virtual void saveStretch(qint64 fromNs, qint64 toNs, bool wholePan) = 0;
    /// A Sound recording of a saved whole-pan stretch.
    virtual void saveSoundOnly(const QString& id) = 0;
    /// A saved whole-pan stretch on the waterfall, like rewind: slice A
    /// plays it and the document's `dsr.stretch` names it.
    virtual void openStretch(const QString& id) = 0;
    /// Keys the radio and sends Mn; it unkeys at the end (or repeats).
    virtual void sendVoice(int n) = 0;
    /// Stops what goes out, or the wait to repeat it; the radio unkeys.
    /// Another device's message is not this window's to stop (ruling 8.5):
    /// refused() says to take transmit, and it goes on.
    virtual void stopVoice() = 0;
    /// Puts the sound file at `wavPath` (WAV or MP3, any rate) into Mn,
    /// converted on the Core to 48 kHz mono. An empty `name` keeps Mn's.
    virtual void importVoice(int n, const QString& wavPath, const QString& name = QString()) = 0;
    /// Saves Mn as a WAV file at `toPath` on this computer.
    virtual void exportVoice(int n, const QString& toPath) = 0;
    virtual void renameVoice(int n, const QString& name) = 0;
    virtual void clearVoice(int n) = 0;
    virtual void setVoiceRepeat(bool on, int delaySec) = 0;
    /// Plays Mn on this window's own speaker only.
    virtual void listenVoice(int n) = 0;
    /// The first message with nothing in it, 1 to 8, or 0 when all eight
    /// hold one (from the document).
    int firstEmptyVoiceSlot() const
    {
        const QJsonArray slotList = document().value(QStringLiteral("voiceKeyer")).toObject()
                                        .value(QStringLiteral("slots")).toArray();
        for (const QJsonValue& value : slotList) {
            const QJsonObject slot = value.toObject();
            const int n = slot.value(QStringLiteral("n")).toInt(0);
            if (n >= 1 && n <= kVoiceKeyerSlots && slot.value(QStringLiteral("empty")).toBool(false)) {
                return n;
            }
        }
        return 0;
    }

    static QString needsNewerCoreReason() { return QStringLiteral("This Core needs an update for recordings."); }
    static QString notConnectedReason() { return QStringLiteral("Connect to a radio to use recordings."); }

signals:
    void documentChanged();
    void stateChanged();
    void listChanged(int reader);
    void playbackChanged();
    void kept(const QJsonObject& event);
    void downloadProgress(const QString& id, qint64 done, qint64 total);
    void downloadFinished(const QString& id, bool ok, const QString& error);
    void refused(const QString& reason);
    void voiceImportFinished(int n, bool ok, const QString& error);
    void voiceExportFinished(int n, bool ok, const QString& error);
    void pastRowsReady(const NereusSDR::DsrPastFrames& rows);
    void pastSpectrumReady(const NereusSDR::DsrPastFrames& frame);
    void hoursReady(const QList<NereusSDR::DsrHour>& hours);
    /// What a save is keeping, in words ("Saving 13:05 to 13:15 UTC...").
    void stretchSaving(const QString& note);

private:
    int m_lastListReader = 0;
};

} // namespace NereusSDR
