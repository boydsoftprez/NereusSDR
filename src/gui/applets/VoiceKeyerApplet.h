#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original. Shaped like the CWX applet's memory
// panel (CwxApplet.cpp); no logic is ported.
// =================================================================
// src/gui/applets/VoiceKeyerApplet.h  (NereusSDR)
// =================================================================
//
// The voice keyer panel: a right-column applet (Containers > Applets, id
// "VoiceKeyer", "Voice keyer"), shape A of the approved mockup, fed by
// RecordingsAccess. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// sections 3 (Voice keyer panel, Voice keyer sending) and 6.6 (Desktop
// panel); mockup voice-keyer-panel/ (shape A).
//
//   Record, Stop   Record lights; then a click on a message records this
//                  computer's microphone into it (before any processing,
//                  up to 2 minutes); a second click on it, Record or Stop
//                  ends it, and the clip goes to the Core. Stop also stops
//                  a message going out, or Listen.
//   M1 to M8       a 2 by 4 grid with their names. A click sends the
//                  message: the radio keys by itself and returns to receive
//                  at the end. While it goes out its button shows progress.
//                  Right-click: Listen (this computer's speaker only),
//                  Record into this message, Rename..., Load WAV...,
//                  Save as WAV..., Clear. A message on the air keeps Clear,
//                  Load WAV and Record disabled with "M<n> is on the air.";
//                  Rename stays.
//   Mic level      while recording.
//   Repeat         Rpt and the wait between repeats.
//   Import WAV...  fills the first empty message ("All eight messages are
//                  full." when none is).
//   Status         what is happening, or the Core's refusal.
//
// Another station holds transmit: the messages dim with the reason and
// Take transmit shows (recording and the rest stay). An older Core: the
// panel stays, dimmed, with "This Core needs an update for recordings."
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 12.
// =================================================================

#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>

#include <array>
#include <functional>
#include <memory>
#include <optional>

#include "gui/NereusGuiExport.h"
#include "gui/applets/AppletWidget.h"
#include "gui/recordings/VoiceButton.h"

class QFrame;
class QLabel;
class QMenu;
class QProgressBar;
class QPushButton;
class QSlider;
class QTimer;

namespace NereusSDR {

class LocalMicClipRecorder;
class MicClipSource;
class RecordingsAccess;

class NEREUS_GUI_EXPORT VoiceKeyerApplet : public AppletWidget {
    Q_OBJECT

public:
    static constexpr int kSlots = 8;
    static constexpr int kNeedsVersion = 3;
    static constexpr int kRefreshMs = 100;
    static constexpr int kBlinkMs = 500;
    static constexpr int kNameMaxChars = 32;   // the Core's (VoiceKeyer::kNameMaxChars)

    VoiceKeyerApplet(RadioModel* model, RecordingsAccess* access, QWidget* parent = nullptr);
    ~VoiceKeyerApplet() override;

    QString appletId() const override { return QStringLiteral("voice_keyer"); }
    QString appletTitle() const override { return QStringLiteral("Voice keyer"); }
    void syncFromModel() override;

    void setAccess(RecordingsAccess* access);
    RecordingsAccess* access() const;
    /// Where a recording's sound comes from (this computer's microphone).
    void setMicSourceFactory(std::function<std::unique_ptr<MicClipSource>()> factory);

    /// As TUNE and MOX are gated: while not permitted, the messages dim
    /// with `reason` and a click says why instead of sending.
    void setTransmitPermitted(bool permitted, const QString& reason);
    void setTakeTransmitOffered(bool offered);

    /// Lights Record for message `n` (from VOICE's empty message): the
    /// next click on it records.
    void armRecord(int n = 0);

    /// Fills `menu` with message n's right-click menu.
    void populateMessageMenu(QMenu* menu, int n);
    /// The status line's words.
    QString statusText() const;

    // ---- Test seams.
    QPushButton* recordButtonForTest() const { return m_record; }
    QPushButton* stopButtonForTest() const { return m_stop; }
    QPushButton* messageButtonForTest(int n) const;
    QFrame* messageProgressForTest(int n) const;
    QPushButton* repeatButtonForTest() const { return m_repeat; }
    QSlider* repeatSliderForTest() const { return m_repeatSlider; }
    QPushButton* importButtonForTest() const { return m_import; }
    QPushButton* takeTransmitForTest() const { return m_take; }
    QProgressBar* levelForTest() const { return m_level; }
    QLabel* noteForTest() const { return m_note; }
    QLabel* statusForTest() const { return m_status; }
    LocalMicClipRecorder* recorderForTest() const { return m_recorder.get(); }
    bool armedForTest() const { return m_armed; }
    int recordingSlotForTest() const { return m_recordingSlot; }
    /// Replaces the open-file dialog: the file to load, empty cancels.
    void setOpenChooserForTest(std::function<QString()> chooser) { m_openChooser = std::move(chooser); }
    /// Replaces the save-file dialog: where to save, empty cancels.
    void setSaveChooserForTest(std::function<QString(const QString&)> chooser) { m_saveChooser = std::move(chooser); }
    /// Replaces the rename dialog: the new name, nothing cancels.
    void setNameAskerForTest(std::function<std::optional<QString>(int, const QString&)> asker)
    {
        m_nameAsker = std::move(asker);
    }
    void setClockForTest(std::function<qint64()> now) { m_now = std::move(now); }
    /// Shows a recording in progress at `elapsedMs` (a still image; no
    /// microphone is read).
    void showRecordingForTest(int n, qint64 elapsedMs, float level);

signals:
    void takeTransmitRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void onDocumentChanged();
    void refresh();
    void refreshMessages();
    void onMessageClicked(int n);
    void onRecordClicked();
    void onStopClicked();
    void startRecording(int n);
    void stopRecording();
    void onRecordingFinished(const QString& path, qint64 ms);
    void importInto(int n);
    void importFirstEmpty();
    void saveAs(int n);
    void rename(int n);
    void showMessageMenu(int n, const QPoint& globalPos);
    void sendRepeat();
    void say(const QString& words);
    QString unavailableReason() const;
    QString nameForImport(int n, const QString& path) const;
    qint64 now() const;
    void restoreListenState();   // Listen: unmute slices, drop the boost
    void hideEvent(QHideEvent* event) override;

    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    VoiceKeyerSnapshot m_snapshot;
    qint64 m_snapshotAtMs = 0;
    bool m_permitted = true;
    QString m_permittedReason;
    bool m_takeOffered = false;
    bool m_awaitingAnswer = false;

    // Listen: the receivers are muted while the message plays, then restored.
    QHash<int, bool> m_listenMuteSaved;
    bool m_listenMuting = false;
    qint64 m_listenStartedAtMs = 0;

    bool m_armed = false;
    int m_armedFor = 0;
    int m_recordingSlot = 0;
    bool m_blinkOn = true;
    std::optional<qint64> m_recordingShownMs;   // showRecordingForTest
    std::unique_ptr<LocalMicClipRecorder> m_recorder;
    std::function<std::unique_ptr<MicClipSource>()> m_sourceFactory;
    QHash<int, QString> m_clipImports;   // message -> this computer's clip file
    QHash<int, QString> m_exports;       // message -> where it is being saved
    QString m_message;                   // the last thing said

    QLabel* m_note = nullptr;
    QWidget* m_body = nullptr;
    QPushButton* m_record = nullptr;
    QPushButton* m_stop = nullptr;
    std::array<QPushButton*, kSlots> m_messages{};
    std::array<QFrame*, kSlots> m_progress{};
    QProgressBar* m_level = nullptr;
    QPushButton* m_repeat = nullptr;
    QSlider* m_repeatSlider = nullptr;
    QLabel* m_repeatValue = nullptr;
    QPushButton* m_import = nullptr;
    QLabel* m_heldNote = nullptr;
    QPushButton* m_take = nullptr;
    QLabel* m_status = nullptr;
    QTimer* m_refreshTimer = nullptr;
    QTimer* m_blinkTimer = nullptr;

    std::function<QString()> m_openChooser;
    std::function<QString(const QString&)> m_saveChooser;
    std::function<std::optional<QString>(int, const QString&)> m_nameAsker;
    std::function<qint64()> m_now;
};

}  // namespace NereusSDR
