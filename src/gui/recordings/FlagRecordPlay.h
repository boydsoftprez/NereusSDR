#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/FlagRecordPlay.h  (NereusSDR)
// =================================================================
//
// Record and Play on one slice flag (the side buttons between the lock and
// the more button), fed by RecordingsAccess, so it works the same whether
// the Core is in this process or across the link. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// sections 3 (flag order, desktop flag look, older recordings, recording
// together), 6.2, 6.3, V1 and V9.
//
//   Record   idle: a round button with a dot, like the lock. Recording: it
//            blinks red with the running time beside it ("+ your voice"
//            while the slice hears your own transmit). Armed (start by
//            itself when you transmit): a hollow red ring and "Waits for you
//            to transmit". A click starts the slice's recording on the Core,
//            armed or not; a click while recording stops it.
//   Play     plays the slice's latest contact recording on this computer's
//            speaker; while it plays, a blue stop square with a label
//            (frequency, mode, call when known, position and length). A
//            click stops it. Right-click: the slice's last eight, All
//            recordings..., Show in Finder (the Core on this computer only),
//            Recording settings....
//   Kept     each recording this slice keeps puts a notice under the flag for
//            kKeptNoticeMs: its name and length, why it stopped, All
//            recordings... and Play.
//
// Not usable (not connected, or a Core without recordings): both buttons
// stay in place, disabled, with the access's reason as their tooltip and
// accessible description. Nothing here keys the radio.
//
// The labels and the notice are children of the flag's pan, beside the
// buttons (which are the pan's children too), and follow them.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 6.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: the flag asks
//                 for its own slice's last eight contact recordings, with a
//                 list reader of its own. AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

#include "core/recordings/RecordingNames.h"
#include "gui/NereusGuiExport.h"

class QFrame;
class QLabel;
class QMenu;
class QPushButton;
class QTimer;

namespace NereusSDR {

class RecordingsAccess;
class VfoWidget;

class NEREUS_GUI_EXPORT FlagRecordPlay : public QObject {
    Q_OBJECT

public:
    enum class RecordLook { Idle, Armed, Recording };

    /// How long the Kept notice stays under the flag.
    static constexpr int kKeptNoticeMs = 6000;
    /// Recordings in the right-click list.
    static constexpr int kMenuRecordings = 8;
    /// Half a blink of Record while it records.
    static constexpr int kBlinkMs = 550;
    /// How often the playing label's position is read while it plays.
    static constexpr int kPositionRefreshMs = 250;

    FlagRecordPlay(VfoWidget* flag, RecordingsAccess* access, int sliceId, QObject* parent = nullptr);
    ~FlagRecordPlay() override;

    /// Follow another access (null: not connected).
    void setAccess(RecordingsAccess* access);
    RecordingsAccess* access() const;
    int sliceId() const { return m_sliceId; }

    RecordLook recordLook() const { return m_recordLook; }
    /// This window plays a recording now.
    bool isPlaying() const;
    /// The slice's latest contact recording, empty when it has none.
    QString latestRecordingId() const;
    /// The slice's contact recordings in the right-click list, newest first.
    QList<RecordingEntry> recentRecordings() const;
    /// The words beside Record and beside Play; empty when none shows.
    QString recordLabelText() const;
    QString playLabelText() const;

    /// Fills `menu` with the right-click list of Play.
    void populatePlayMenu(QMenu* menu);

    /// The Kept notice's reason line for the document's `kept` object.
    /// `quietMinutes` is the stop-by-itself setting; `coreHere` is true when
    /// the Core runs on this computer.
    static QString keptReasonText(const QJsonObject& kept, int quietMinutes, bool coreHere);
    /// "1:23", or "1:02:03" past an hour.
    static QString lengthText(qint64 ms);

    // ---- Test seams.
    QLabel* recordLabelForTest() const { return m_recordLabel.data(); }
    QLabel* playLabelForTest() const { return m_playLabel.data(); }
    QFrame* keptNoticeForTest() const { return m_kept.data(); }
    QLabel* keptTextForTest() const { return m_keptText.data(); }
    QPushButton* keptPlayButtonForTest() const { return m_keptPlay.data(); }
    QPushButton* keptAllButtonForTest() const { return m_keptAll.data(); }
    QTimer* keptTimerForTest() const { return m_keptTimer; }
    /// Replaces RevealFile::reveal (a test must not open Finder).
    void setRevealForTest(std::function<bool(const QString&)> reveal) { m_reveal = std::move(reveal); }
    /// Replaces the clock the right-click list's "today" and "yesterday" use.
    void setClockForTest(std::function<QDateTime()> now) { m_now = std::move(now); }

signals:
    /// All recordings... was picked, with the recording to select (empty for
    /// none). Enabled only while something listens.
    void allRecordingsRequested(const QString& recordingId);
    /// Recording settings... was picked. Enabled only while something
    /// listens.
    void recordingSettingsRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void attachButtons();
    void onDocumentChanged();
    void onListChanged(int reader);
    void onKept(const QJsonObject& event);
    void onRecordClicked();
    void onPlayClicked();
    void requestListIfStale();
    void refresh();
    void applyRecordLook();
    void applyPlayLook();
    void placeLabels();
    void placeKeptNotice();
    void raiseOwn();
    void hideKeptNotice();
    void showPlayMenu(const QPoint& globalPos);
    QString unavailableReason() const;
    QString sliceName() const;
    QJsonObject sliceDocument() const;
    bool hasListener(const char* signal) const;
    bool playLabelParts(QString* name, QString* times) const;

    QPointer<VfoWidget> m_flag;
    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    int m_sliceId;

    QPointer<QPushButton> m_recordButton;
    QPointer<QPushButton> m_playButton;
    QPointer<QLabel> m_recordLabel;
    QPointer<QLabel> m_playLabel;
    QPointer<QFrame> m_kept;
    QPointer<QLabel> m_keptText;
    QPointer<QPushButton> m_keptAll;
    QPointer<QPushButton> m_keptPlay;
    QString m_keptId;

    QTimer* m_keptTimer = nullptr;
    QTimer* m_blinkTimer = nullptr;
    QTimer* m_positionTimer = nullptr;
    bool m_blinkOn = true;

    RecordLook m_recordLook = RecordLook::Idle;
    qint64 m_elapsedMs = 0;
    bool m_myVoice = false;
    QList<RecordingEntry> m_entries;   // the slice's last contact recordings
    qint64 m_listRevision = -1;
    int m_listReader = 0;              // this flag's list reader on m_access

    std::function<bool(const QString&)> m_reveal;
    std::function<QDateTime()> m_now;
};

}  // namespace NereusSDR
