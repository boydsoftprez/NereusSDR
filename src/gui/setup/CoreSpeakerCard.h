#pragma once

// =================================================================
// src/gui/setup/CoreSpeakerCard.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Setup > Audio > Outputs' "Core
// speaker" card in a window connected to a Core; no upstream logic.
//
// Design spec: docs/architecture/2026-10-08-native-audio-engines-design.md
// (R-AUD-27, R-AUD-30, D24); mockup
// docs/architecture/2026-10-08-native-audio-engines-design/core-speaker-mockup.html.
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
//   2026-10-09: native audio plan Task 22 (R-AUD-27, R-AUD-30, D24).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QGroupBox>
#include <QMetaObject>
#include <QPointer>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;
class QToolButton;
class QWidget;

namespace NereusSDR {

class RadioModel;
class RemoteDevicesState;

// ---------------------------------------------------------------------------
// The Core speaker (objectName coreSpeakerGroup), as a remote window shows it:
//
//   A speaker or sound card plugged into <Core>.   (coreSpeakerWhere)
//   Volume [slider coreSpeakerVolume] [readout coreSpeakerVolumeReadout]
//          [x] Mute Core speaker (coreSpeakerMute)
//   Device [coreSpeakerDevice]: "(none)" on a desktop box, "(the Core's
//          default)", then the Core's cards
//   state note (coreSpeakerStateNote, amber), desktop note
//          (coreSpeakerDesktopNote), the note (coreSpeakerNote), the reason
//          while greyed (coreSpeakerReason)
//   Device details (coreSpeakerDetailsToggle, folded; coreSpeakerDetails):
//          Driver "ALSA, direct" greyed with its note, Sample rate, Channels,
//          Buffer size, Delay with its "Now" line, Negotiated
//
// The controls write RadioModel's coreSpeaker* properties and follow their
// signals without echo.  Every control is greyed with the model's reason
// while the Core does not offer the speaker (unreachable, or an older Core),
// and with the page's station reason while the Core's settings are
// unavailable.
// ---------------------------------------------------------------------------
class CoreSpeakerCard : public QGroupBox {
    Q_OBJECT
public:
    CoreSpeakerCard(RadioModel* model, QWidget* parent);

    // The page's gate (R-R3-21): the Core's settings unavailable greys the
    // card with `reason`.
    void setStationSettingsAvailable(bool available, const QString& reason);

    bool detailsExpanded() const;
    void setDetailsExpanded(bool expanded);

private:
    void buildLayout();
    void wireModel();
    void followStationDevices();
    void sync();
    void syncWhere();
    void fillDevices();
    void syncDetails();
    void writeDetails();
    QString reason() const;

    QPointer<RadioModel> m_model;
    QPointer<RemoteDevicesState> m_devices;
    QMetaObject::Connection m_devicesConnection;

    QLabel*      m_where{nullptr};
    QSlider*     m_volume{nullptr};
    QLabel*      m_readout{nullptr};
    QCheckBox*   m_mute{nullptr};
    QComboBox*   m_device{nullptr};
    QLabel*      m_stateNote{nullptr};
    QLabel*      m_desktopNote{nullptr};
    QLabel*      m_note{nullptr};
    QLabel*      m_reason{nullptr};
    QToolButton* m_detailsToggle{nullptr};
    QWidget*     m_details{nullptr};
    QComboBox*   m_driver{nullptr};
    QComboBox*   m_sampleRate{nullptr};
    QComboBox*   m_channels{nullptr};
    QComboBox*   m_bufferSize{nullptr};
    QLabel*      m_bufferMs{nullptr};
    QComboBox*   m_delay{nullptr};
    QLabel*      m_delayNow{nullptr};
    QLabel*      m_negotiated{nullptr};

    bool    m_stationAvailable{true};
    QString m_stationReason;
    bool    m_updatingFromModel{false};
};

} // namespace NereusSDR
