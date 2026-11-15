#pragma once

// =================================================================
// src/gui/widgets/MasterOutputWidget.h  (Longpath)
// =================================================================
//
// Longpath-original widget. Visual styling draws on AetherSDR
// `src/gui/TitleBar.cpp:172-215` (the master-volume section of
// AetherSDR's title bar): speaker emoji button with mute glyph flip,
// horizontal volume slider with the shared #00b4d8 handle/sub-page
// palette, and an inset percent readout to the right of the slider.
// The structure here is Longpath-original because this widget
// isolates JUST the master-output triad; AetherSDR's TitleBar
// combines master + headphones + PC-audio + minimal-mode in one
// monolithic bar. TitleBar-strip host wiring lands in Task 10c.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-20 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code. Phase 3O
//                Sub-Phase 10 Task 10b. Widget layout + mute-glyph
//                flip + inset readout styling take cues from
//                AetherSDR TitleBar.cpp:172-215. Two-way bind to
//                AudioEngine setVolume / setMasterMuted with the
//                m_updatingFromModel + QSignalBlocker echo guard
//                pattern documented in CLAUDE.md "GUI↔Model Sync".
//                Persists audio/Master/Volume and audio/Master/Muted
//                per design spec
//                docs/architecture/2026-04-19-vax-design.md §5.4.
// =================================================================

#include "core/AudioDeviceConfig.h"

#include <QString>
#include <QWidget>

class QLabel;
class QPoint;
class QPushButton;
class QSlider;
class QTimer;

namespace Longpath {

class AudioEngine;

// MasterOutputWidget — menu-bar master-output composite.
//
// Layout (matches design spec §7.3, ~222 px wide × 22 px tall):
//
//   [speaker 20] [slider 100] [label 22]
//
// - Speaker button: left-click toggles mute (🔊 ↔ 🔇). Right-click
//   opens an output-device picker populated from
//   PortAudioBus::hostApis() + PortAudioBus::outputDevicesFor. The
//   picker emits outputDeviceChanged(name); the host (Task 10c
//   TitleBar inside MainWindow) is responsible for calling
//   AudioEngine::setSpeakersConfig to actually rebuild the speakers
//   bus. See design spec §6.3.
// - Slider: 0–100 range mapped linearly to AudioEngine volume [0,1].
// - Label: inset percent readout (0–100, shown as the raw integer
//   slider value — Option A per task brief, matches AetherSDR).
//
// volumeChanged / mutedChanged / outputDeviceChanged signals fire
// ONLY on user action. The m_updatingFromModel guard plus a
// QSignalBlocker on the speaker button prevents the engine→widget
// echo from re-emitting into the engine.
class MasterOutputWidget : public QWidget {
    Q_OBJECT
public:
    explicit MasterOutputWidget(AudioEngine* audio, QWidget* parent = nullptr);

    // Called by Setup → Audio → Devices when the user picks a
    // speakers device elsewhere in the app, so the context-menu
    // check state stays in sync with the engine's current device.
    // Does NOT emit outputDeviceChanged — this is a sync-from-
    // elsewhere path, not a user action.
    void setCurrentOutputDevice(const QString& name);

signals:
    // User moved the slider. Value is the 0.0–1.0 linear volume.
    void volumeChanged(float v);
    // User clicked the speaker button.
    void mutedChanged(bool muted);
    // User picked an output device from the right-click context menu.
    void outputDeviceChanged(QString deviceName);

    // Der Ausgang des BETRIEBSSYSTEMS wurde stumm, waehrend Longpath
    // laeuft -- in Longpath selbst ist alles offen. Am Mac ist das fast
    // immer F10: die Stummtaste, wenn Longpath gerade nicht vorne ist
    // (der F-Tasten-Vorrang gilt nur fuer das vordere Programm).
    // Nur die Flanke wird gemeldet, nicht jeder Takt.
    void systemOutputMutedDetected();

private slots:
    // Populate and pop the right-click device picker at `pos`
    // (widget-local coordinates, as delivered by
    // QWidget::customContextMenuRequested).
    void onSpeakerContextMenu(const QPoint& pos);

    // AudioEngine → widget echo handlers. Both use the
    // m_updatingFromModel / QSignalBlocker guard so a setValue /
    // setChecked driven by these slots does not reenter the
    // widget→engine path.
    void onAudioEngineVolumeChanged(float v);
    void onAudioEngineMasterMutedChanged(bool m);

    // Sub-Phase 12 Task 12.2: live sync with Setup → Audio → Devices edits.
    // Receives the negotiated AudioDeviceConfig from the engine after any
    // speakers bus reconfig and updates m_currentDeviceName so the right-
    // click picker's checkmark stays consistent.
    void onSpeakersConfigChanged(const Longpath::AudioDeviceConfig& cfg);

    // Takt des Waechters ueber den Stummschalter des Betriebssystems.
    void pruefeSystemStumm();

private:
    AudioEngine* m_audio{nullptr};
    QPushButton* m_speakerBtn{nullptr};
    QSlider*     m_slider{nullptr};
    QLabel*      m_dbLabel{nullptr};
    bool         m_updatingFromModel{false};
    QString      m_currentDeviceName;
    QTimer*      m_systemMuteWatch{nullptr};
    bool         m_systemMuteSeen{false};   // letzter gesehener Zustand
    QString      m_speakerTipBase;          // Tooltip ohne den Zusatz
};

} // namespace Longpath
