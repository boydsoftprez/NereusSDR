#pragma once

// =================================================================
// src/gui/setup/AudioOutputsPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup > Audio > Outputs page: where the sound goes
// (R-SPK-21, D13). No Thetis port.
//
// Design spec: docs/architecture/2026-10-05-radio-speaker-and-audio-setup-design.md
// (R-SPK-21 Outputs, R-SPK-22, R-SPK-24, D13 to D15); mockup audio-setup.html.
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 9
//                (R-SPK-21, R-SPK-22, R-SPK-24). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-10-09 - Native audio plan Task 16 (R-AUD-01, R-AUD-03, R-AUD-06):
//                the cards follow the engine's device catalogue; Rescan
//                devices rescans the older drivers. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/SetupPage.h"

class QButtonGroup;
class QCheckBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class QWidget;

namespace NereusSDR {

class AudioEngine;
class DeviceCard;
class SoundSystemLine;

// ---------------------------------------------------------------------------
// Audio > Outputs (SetupScope::Mixed)
//
//   Sound system line                     (soundSystemLine)
//   This computer  (cyan DeviceCard, audio/Speakers, thisComputerGroup)
//     Volume [pc icon] [slider pcVolume] [readout] [x] Mute (pcMute)
//     Device, Device details (folded)
//   Headphones     (DeviceCard, audio/Headphones, headphonesGroup)
//     Enabled; Device and Device details greyed until Enabled
//   Radio speaker  (amber group, radioSpeakerGroup)
//     status line (radioSpeakerStatusLine)
//     Volume [radio icon] [slider radioSpeakerVolume] [readout]
//            [x] Mute radio speaker (radioSpeakerMute)
//     Speaker amplifier (speakerAmplifierChoice: ampNormal, ampOffOnTx,
//            ampAlwaysOff), its explanation, reason and live status
//            (speakerAmplifierStatus)
//   [Rescan devices] (rescanDevices)
//
// This computer's Volume and Mute are the header's PC control: they drive
// AudioEngine::setVolume / setMasterMuted and save audio/Master/Volume and
// audio/Master/Muted as the header does, and follow the engine's changes.
// The Radio speaker controls write RadioModel and follow its signals; the
// page never writes the model while it builds (realizing a page in a remote
// window sends nothing to the Core). While the Core's settings are
// unavailable in a remote window, the Radio speaker controls are disabled
// with the station reason; this computer's controls stay live.
// ---------------------------------------------------------------------------
class AudioOutputsPage : public SetupPage {
    Q_OBJECT
public:
    explicit AudioOutputsPage(RadioModel* model, QWidget* parent = nullptr);

    void setStationSettingsAvailable(bool available, const QString& reason) override;

    // The status line text under the Radio speaker title.
    QString radioSpeakerStatusText() const;

    // The amplifier choice's tooltip while it can be switched: the
    // explanation and when it is greyed (R-SPK-10, D9). The same words as
    // audio.outputs.speakerAmplifierMode's described tooltip.
    static QString speakerAmplifierToolTip();

private:
    void buildThisComputer();
    void buildHeadphones();
    void buildRadioSpeaker();
    void buildRescan();
    void wireEngine();
    void wireModel();

    void syncPcFromEngine();
    void syncRadioSpeaker();
    void rescan();
    void updateRescanState();

    AudioEngine*     m_engine{nullptr};
    QTimer*          m_cataloguePickup{nullptr};
    SoundSystemLine* m_soundSystem{nullptr};

    // This computer
    DeviceCard*  m_speakersCard{nullptr};
    QPushButton* m_pcButton{nullptr};
    QSlider*     m_pcSlider{nullptr};
    QLabel*      m_pcReadout{nullptr};
    QCheckBox*   m_pcMute{nullptr};

    // Headphones
    DeviceCard* m_headphonesCard{nullptr};

    // Radio speaker
    QWidget*      m_radioGroup{nullptr};
    QLabel*       m_radioStatus{nullptr};
    QPushButton*  m_radioButton{nullptr};
    QSlider*      m_radioSlider{nullptr};
    QLabel*       m_radioReadout{nullptr};
    QCheckBox*    m_radioMute{nullptr};
    QLabel*       m_radioNote{nullptr};
    QWidget*      m_ampChoice{nullptr};
    QButtonGroup* m_ampButtons{nullptr};
    QLabel*       m_ampReason{nullptr};
    QLabel*       m_ampStatus{nullptr};

    // Rescan
    QPushButton* m_rescanButton{nullptr};
    QLabel*      m_rescanResult{nullptr};

    bool    m_stationAvailable{true};
    QString m_stationReason;

    bool m_updatingFromEngine{false};
    bool m_updatingFromModel{false};
};

} // namespace NereusSDR
