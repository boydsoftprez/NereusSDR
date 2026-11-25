#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/setup/DsrSetupPage.h  (NereusSDR)
// =================================================================
//
// Setup > Cores > Digital Spectrum Recorder: the Core's recorder settings,
// which every device shares. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// sections 3 (Recorder Setup) and 6.7, A14; mockup dsr-setup/.
//
//   Recording          on or off, and what it is doing now
//   Where it is kept   the disk (an SD card is never one: the red
//                      warning), Scan again, the size limit and how long it
//                      holds at the receivers and rates streaming now, the
//                      "It is always erasing" note, the in-use bar with its
//                      moving write head and the red "erased next" zone
//   Saved stretches    kept apart from the limit and never overwritten;
//                      how many, Show recordings, and the sound-only format
//   Erase what is kept now...   asks first, and says it cannot be undone
//
// The page reads the `recordings` document's `dsr`, `settings` (its disks
// and format) and `index` through RecordingsAccess, and changes them only
// with setSettings() (dsrEnabled, dsrDisk, dsrSizeLimitGb, format) and
// eraseDsr(); it stores nothing itself. A change from another device
// arrives as documentChanged() and is shown under m_updatingFromModel, so
// it is never sent back. Every control needs recordingsVersion 4
// (RecordingsAccess::kDsrVersion): until the Core offers it, or while not
// connected, each group stays, dimmed, with the reason at the top (V9).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 26.
//   2026-11-17 - J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code: m_diskFiller (PR 808 crash fix).
// =================================================================

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>

#include "gui/NereusGuiExport.h"
#include "gui/SetupPage.h"
#include "models/recordings/RecordingsAccess.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

namespace NereusSDR {

class DiskComboFiller;

/// The recorder's loop as a bar: what is kept (blue), where it writes now
/// (a white head) and, once full, the red stretch it erases next. Between
/// the Core's figures (every few seconds) the head moves on at the
/// receivers' rate, so it is seen to move.
class NEREUS_GUI_EXPORT DsrUsageBar : public QWidget {
    Q_OBJECT

public:
    /// How often the head moves on between the Core's figures.
    static constexpr int kStepMs = 250;

    explicit DsrUsageBar(QWidget* parent = nullptr);

    /// The Core's figures. `recording` moves the head; `full` draws the red
    /// zone after it.
    void setFigures(qint64 limitBytes, qint64 usedBytes, qint64 writePosition, qint64 bytesPerHour,
                    bool recording, bool full);
    /// Moves the head on by `ms` of writing (the timer's step; tests).
    void advance(int ms);

    bool full() const { return m_full; }
    bool moving() const;
    /// Where it writes now, 0 to 1 of the limit.
    double headFraction() const;
    /// What is kept, 0 to 1 of the limit.
    double keptFraction() const;
    /// The red zone's start and width, 0 to 1 of the limit (0 width unless
    /// full).
    double eraseFraction() const;
    double eraseWidthFraction() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void updateTimer();

    qint64 m_limit = 0;
    qint64 m_used = 0;
    double m_head = 0.0;   // bytes into the loop
    qint64 m_bytesPerHour = 0;
    bool m_recording = false;
    bool m_full = false;
    QTimer* m_timer = nullptr;
};

class NEREUS_GUI_EXPORT DsrSetupPage : public SetupPage {
    Q_OBJECT

public:
    /// Asks before erasing: the title and the words; true to erase. The
    /// page's own shows a dialog; tests answer it.
    using Confirm = std::function<bool(const QString& title, const QString& text)>;

    /// The slider and the box never offer more than this share of the
    /// disk's room (what is free, plus what the loop holds now).
    static constexpr double kRoomShare = 0.9;
    /// How long "Erased." stays beside the button.
    static constexpr int kErasedNoteMs = 6000;

    explicit DsrSetupPage(RecordingsAccess* access, QWidget* parent = nullptr);

    void setAccess(RecordingsAccess* access);
    RecordingsAccess* access() const { return m_access; }
    void setConfirm(Confirm confirm) { m_confirm = std::move(confirm); }

    /// "1 h 49 min", "35 min", "7.5 days".
    static QString durationText(double hours);
    /// "2 receivers at 192 kHz", "2 receivers at 192 and 384 kHz"; empty
    /// for none.
    static QString receiversText(const QJsonArray& receivers);

    /// The erase confirm's title and words (A: it says it cannot be undone).
    static QString eraseTitle();
    QString eraseText() const;

    // Test seams (tst_dsr_setup_page).
    QLabel* availabilityLabel() const { return m_availability; }
    QList<QGroupBox*> groups() const { return m_groups; }
    QCheckBox* enabledBox() const { return m_enabled; }
    QLabel* statusLabel() const { return m_status; }
    QLabel* sdWarning() const { return m_sdWarning; }
    QComboBox* diskCombo() const { return m_disk; }
    QPushButton* scanButton() const { return m_scan; }
    QSlider* sizeSlider() const { return m_sizeSlider; }
    QSpinBox* sizeSpin() const { return m_sizeSpin; }
    QLabel* holdsLabel() const { return m_holds; }
    QLabel* loopNote() const { return m_loopNote; }
    DsrUsageBar* usageBar() const { return m_usageBar; }
    QLabel* usageKey() const { return m_usageKey; }
    QLabel* usageLabel() const { return m_usage; }
    QLabel* unusableNote() const { return m_unusableNote; }
    QLabel* savedLabel() const { return m_saved; }
    QPushButton* recordingsButton() const { return m_recordings; }
    QComboBox* formatCombo() const { return m_format; }
    QPushButton* eraseButton() const { return m_erase; }
    QLabel* eraseNote() const { return m_eraseNote; }

signals:
    /// Show recordings was clicked: open the Recordings window.
    void recordingsWindowRequested();

private:
    void buildRecording();
    void buildWhere();
    void buildSaved();
    void buildErase();
    void showDocument();
    void showSizeDependent(int gb);
    void requestSaved();
    void showSaved();
    void send(const QJsonObject& changed);
    void commitSize(int gb);
    void askToErase();

    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    bool m_updatingFromModel = false;
    Confirm m_confirm;

    // What the last document said, for the controls' handlers.
    QJsonObject m_dsr;
    bool m_ready = false;
    bool m_sdBlocked = false;
    qint64 m_indexRevision = -1;
    int m_wholePanReader = 0;
    int m_soundReader = 0;

    QLabel* m_availability = nullptr;
    QList<QGroupBox*> m_groups;
    QCheckBox* m_enabled = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_sdWarning = nullptr;
    QComboBox* m_disk = nullptr;
    DiskComboFiller* m_diskFiller = nullptr;
    QPushButton* m_scan = nullptr;
    QSlider* m_sizeSlider = nullptr;
    QSpinBox* m_sizeSpin = nullptr;
    QLabel* m_holds = nullptr;
    QLabel* m_loopNote = nullptr;
    DsrUsageBar* m_usageBar = nullptr;
    QLabel* m_usageKey = nullptr;
    QLabel* m_usage = nullptr;
    QLabel* m_unusableNote = nullptr;
    QLabel* m_saved = nullptr;
    QPushButton* m_recordings = nullptr;
    QComboBox* m_format = nullptr;
    QPushButton* m_erase = nullptr;
    QLabel* m_eraseNote = nullptr;
    QTimer* m_eraseNoteTimer = nullptr;
};

}  // namespace NereusSDR
