#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/setup/RecordingsSetupPage.h  (NereusSDR)
// =================================================================
//
// Setup > Cores > Recordings: the Core's recording settings, which every
// device shares. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// sections 3 (Recording settings) and 6.5; mockup recording-settings/.
//
//   What the Record button keeps   what you hear (always), your own voice
//                                  when you transmit, the two sides
//                                  together or apart
//   Starting and stopping          start by itself when I transmit, stop by
//                                  itself when it goes quiet, after 1 to 60
//                                  minutes
//   Files                          MP3 or WAV with its size an hour, and a
//                                  name example
//   Where they are kept            the Core's disks, how many recordings
//                                  and their size, Recordings ›
//   Playing back                   keying while a recording plays sends it
//                                  on the air; EQ, compressor, leveler,
//                                  CFC and phase rotator while it goes out
//
// The page reads the `recordings` document's `settings` and `index` through
// RecordingsAccess and changes them only with setSettings(), one changed
// field at a time; it stores nothing itself. A change from another device
// arrives as documentChanged() and is shown under m_updatingFromModel, so
// it is never sent back. Not usable (not connected, or a Core without
// recordings): every group stays, dimmed, with the reason at the top. The
// five on-air switches need recordingsVersion 2 and are disabled with the
// access's reason until the Core offers it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 8.
// =================================================================

#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QString>

#include "gui/NereusGuiExport.h"
#include "gui/SetupPage.h"
#include "models/recordings/RecordingsAccess.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;

namespace NereusSDR {

class NEREUS_GUI_EXPORT RecordingsSetupPage : public SetupPage {
    Q_OBJECT

public:
    /// The on-air switches arrive with this recordingsVersion (Task 10).
    static constexpr int kOnAirVersion = 2;

    explicit RecordingsSetupPage(RecordingsAccess* access, QWidget* parent = nullptr);

    void setAccess(RecordingsAccess* access);
    RecordingsAccess* access() const { return m_access; }

    /// "About 29 MB an hour." for MP3 together, from the Core's file rates.
    static int megabytesPerHour(bool wav, bool sidesApart);

    // Test seams (tst_recordings_setup_page).
    QLabel* availabilityLabel() const { return m_availability; }
    QList<QGroupBox*> groups() const { return m_groups; }
    QCheckBox* hearBox() const { return m_hear; }
    QCheckBox* myVoiceBox() const { return m_myVoice; }
    QComboBox* sidesCombo() const { return m_sides; }
    QCheckBox* autoStartBox() const { return m_autoStart; }
    QCheckBox* autoStopBox() const { return m_autoStop; }
    QSpinBox* quietSpin() const { return m_quiet; }
    QLabel* startStopSummary() const { return m_startStopSummary; }
    QComboBox* formatCombo() const { return m_format; }
    QLabel* formatSizeLabel() const { return m_formatSize; }
    QLabel* nameExample() const { return m_nameExample; }
    QComboBox* diskCombo() const { return m_disk; }
    QLabel* sdCardNote() const { return m_sdNote; }
    QLabel* usageLabel() const { return m_usage; }
    QPushButton* recordingsButton() const { return m_recordings; }
    QLabel* playingBackSentence() const { return m_onAirSentence; }
    QList<QCheckBox*> onAirBoxes() const { return m_onAir; }

signals:
    /// Recordings › was clicked: open the Recordings window.
    void recordingsWindowRequested();

private:
    void buildKeeps();
    void buildStartStop();
    void buildFiles();
    void buildWhere();
    void buildPlayingBack();
    void showDocument();
    void showAvailability();
    void showDependentText();
    void send(const QString& field, const QJsonValue& value);

    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    bool m_updatingFromModel = false;

    QLabel* m_availability = nullptr;
    QList<QGroupBox*> m_groups;
    QCheckBox* m_hear = nullptr;
    QCheckBox* m_myVoice = nullptr;
    QComboBox* m_sides = nullptr;
    QCheckBox* m_autoStart = nullptr;
    QCheckBox* m_autoStop = nullptr;
    QSpinBox* m_quiet = nullptr;
    QLabel* m_startStopSummary = nullptr;
    QComboBox* m_format = nullptr;
    QLabel* m_formatSize = nullptr;
    QLabel* m_nameExample = nullptr;
    QComboBox* m_disk = nullptr;
    QLabel* m_sdNote = nullptr;
    QProgressBar* m_usageBar = nullptr;
    QLabel* m_usage = nullptr;
    QPushButton* m_recordings = nullptr;
    QLabel* m_onAirSentence = nullptr;
    QLabel* m_onAirNote = nullptr;
    QList<QCheckBox*> m_onAir;
};

}  // namespace NereusSDR
